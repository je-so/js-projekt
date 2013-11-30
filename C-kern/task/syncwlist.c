/* title: SyncWaitlist impl

   Implements <SyncWaitlist>.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/task/syncwlist.h
    Header file <SyncWaitlist>.

   file: C-kern/task/syncwlist.c
    Implementation file <SyncWaitlist impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncwlist.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/dlist.h"
#include "C-kern/api/ds/inmem/queue.h"
#include "C-kern/api/task/syncthread.h"
#include "C-kern/api/task/syncqueue.h"
#include "C-kern/api/task/syncwait.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/ds/foreach.h"
#endif


/* typedef: struct wlistentry_t
 * Export <wlistentry_t> into global namespace. */
typedef struct wlistentry_t               wlistentry_t ;


/* struct: wlistentry_t
 * Type of entry stored in <syncwlist_t>.
 * It embeds a <dlist_node_t> and a <syncevent_t>. */
struct wlistentry_t {
   dlist_node_t * next ;
   dlist_node_t * prev ;
   syncevent_t    event ;
} ;

/* define: wlistentry_INIT_FREEABLE
 * Static initializer. */
#define wlistentry_INIT_FREEABLE          { 0, 0, syncevent_INIT_FREEABLE }

/* function: initmove_wlistentry
 * Moves src to dest. Consider src uninitialized after this operation. */
void initmove_wlistentry(/*out*/wlistentry_t * dest, wlistentry_t * src)
{
   dlist_t dummy = dlist_INIT ;
   replacenode_dlist(&dummy, genericcast_dlistnode(dest), genericcast_dlistnode(src)) ;
   initmovesafe_syncevent(&dest->event, &src->event) ;
}


// section: syncwlist_t

// group: variable
#ifdef KONFIG_UNITTEST
static test_errortimer_t      s_syncwlist_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: helper

/* define: dlist_IMPLEMENT _wlist
 * Implements interface to manage <wlistentry_t> in a double linked list of type <dlist_t>.
 * The generated functions have name YYY_wlist (init_wlist...) instead of YYY_dlist (init_dlist...). */
dlist_IMPLEMENT(_wlist, wlistentry_t,)

/* function: compiletime_assert
 * Ensures that <syncwlist_t> and <wlistentry_t> can be cast into <dlist_node_t>
 * without changing the address of the pointer.
 * This also ensures that <syncwlist_t> can be cast into <wlistentry_t>. */
static inline void compiletime_assert(void)
{
   syncwlist_t    wlist ;
   wlistentry_t   entry ;
   static_assert((dlist_node_t*)&wlist == genericcast_dlistnode(&wlist), "cast without adding offset possible") ;
   static_assert((dlist_node_t*)&entry == genericcast_dlistnode(&entry), "cast without adding offset possible") ;
}

// group: lifetime

/* function: init_syncwlist
 *
 * Implementation notes:
 *
 * The double linked list dlist_t will not be stored.
 * Instead wlist is considered as the last node in it
 * and therefore has a prev and next pointer instead of a single last pointer.
 * This allows to move objects of type wlistentry_t in their queue
 * and to fill any gaps created during removal of nodes.
 * If a node is removed from the queue function <initmove_wlistentry> is called
 * during execution of <remove_syncqueue> which allows to compact memory.
 *
 * Reason:
 * If the moved node would be the last node of a dlist_t the last pointer
 * would become invalid. But making wlist part of the list
 * (as dummy node) function <initmove_wlistentry> will adapt the pointers
 * prev or next in case a node will be moved before or after node wlist. */
void init_syncwlist(/*out*/syncwlist_t * wlist)
{
   dlist_t dlist = dlist_INIT ;

   insertlast_wlist(&dlist, (wlistentry_t*)wlist) ;
   wlist->nrnodes = 0 ;
}

void initmove_syncwlist(/*out*/syncwlist_t * destwlist, syncwlist_t * srcwlist)
{
   dlist_t dlist = dlist_INIT_LAST(genericcast_dlistnode(destwlist)) ;

   replacenode_wlist(&dlist, (wlistentry_t*)destwlist, (wlistentry_t*)srcwlist) ;
   destwlist->nrnodes = srcwlist->nrnodes ;

   init_syncwlist(srcwlist) ;
}

int free_syncwlist(syncwlist_t * wlist, struct syncqueue_t * queue)
{
   int err ;

   err = 0 ;

   if (wlist->next) {
      // clear all wlistentry_t->event
      // the reason is that initmove_wlistentry should not access waiting threads called from remove_syncqueue
      wlistentry_t * next = (wlistentry_t*)wlist->next ;
      while (next != (wlistentry_t*)wlist) {
         next->event = (syncevent_t) syncevent_INIT_FREEABLE ;
         next = next_wlist(next) ;
      }

      dlist_t dlist = dlist_INIT_LAST(genericcast_dlistnode(wlist)) ;
      while (wlist != (syncwlist_t*)wlist->next) {
         wlistentry_t * entry ;
         int err2 = removefirst_wlist(&dlist, &entry) ;
         if (err2) err = err2 ;
         err2 = remove_syncqueue(queue, entry, &initmove_wlistentry) ;
         if (err2) err = err2 ;
         SETONERROR_testerrortimer(&s_syncwlist_errtimer, &err) ;
      }
   }

   wlist->next  = 0 ;
   wlist->prev  = 0 ;
   wlist->nrnodes = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

bool isfree_syncwlist(const syncwlist_t * wlist)
{
   return 0 == wlist->next && 0 == wlist->prev && 0 == wlist->nrnodes ;
}

struct syncevent_t * last_syncwlist(const syncwlist_t * wlist)
{
   if (isempty_syncwlist(wlist)) return 0 ;

   return &prev_wlist((wlistentry_t*)CONST_CAST(syncwlist_t,wlist))->event ;
}

// group: update

int insert_syncwlist(syncwlist_t * wlist, syncqueue_t * queue, /*out*/syncevent_t ** newevent)
{
   int err ;
   wlistentry_t * entry ;

   ONERROR_testerrortimer(&s_syncwlist_errtimer, ONABORT) ;
   err = insert_syncqueue(queue, &entry) ;
   if (err) goto ONABORT ;

   insertbefore_wlist((wlistentry_t*)wlist, entry) ;
   ++ wlist->nrnodes ;

   entry->event = (syncevent_t) syncevent_INIT_FREEABLE ;

   *newevent = &entry->event ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int remove_syncwlist(syncwlist_t * wlist, syncqueue_t * queue, /*out*/syncevent_t * removedevent)
{
   int err ;

   if (isempty_syncwlist(wlist)) return ENODATA ;

   dlist_t        dlist = dlist_INIT_LAST(genericcast_dlistnode(wlist)) ;
   wlistentry_t * entry ;

   ONERROR_testerrortimer(&s_syncwlist_errtimer, ONABORT) ;
   err = removefirst_wlist(&dlist, &entry) ;
   if (err) goto ONABORT ;

   -- wlist->nrnodes ;

   syncevent_t event = entry->event ;

   ONERROR_testerrortimer(&s_syncwlist_errtimer, ONABORT) ;
   err = remove_syncqueue(queue, entry, &initmove_wlistentry) ;
   if (err) goto ONABORT ;

   // do not call initmove_syncevent
   // this function should work in any case
   *removedevent = event ;

   return 0 ;
ONABORT:
   // list could have been changed
   // but if remove_syncqueue fails there is an internal error
   // TODO: introduce error classes (if remove_syncqueue fails ==> change error class to unexpected
   // unexpected may be an internal error (a bug == fatal) or a hardware error (fatal)
   // it may be possible to introduce concepts which allow to overcome certain classes of internal errors
   // at least it is possible to restart the process and if that fails to reboot the hardware
   // but there might be something better !
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int removeempty_syncwlist(syncwlist_t * wlist, struct syncqueue_t * queue)
{
   int err ;

   if (!isempty_syncwlist(wlist)) {
      dlist_t        dlist = dlist_INIT_LAST(genericcast_dlistnode(wlist)) ;
      wlistentry_t * entry = prev_wlist((wlistentry_t*)wlist) ;

      if (!iswaiting_syncevent(&entry->event)) {
         ONERROR_testerrortimer(&s_syncwlist_errtimer, ONABORT) ;
         err = remove_wlist(&dlist, entry) ;
         if (err) goto ONABORT ;

         -- wlist->nrnodes ;

         ONERROR_testerrortimer(&s_syncwlist_errtimer, ONABORT) ;
         err = remove_syncqueue(queue, entry, &initmove_wlistentry) ;
         if (err) goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int transferfirst_syncwlist(syncwlist_t * towlist, syncwlist_t * fromwlist)
{
   int err ;

   if (!isempty_syncwlist(fromwlist)) {
      dlist_t        dlist = dlist_INIT_LAST(genericcast_dlistnode(fromwlist)) ;
      wlistentry_t * entry ;

      err = removefirst_wlist(&dlist, &entry) ;
      if (err) goto ONABORT ;

      -- fromwlist->nrnodes ;
      ++ towlist->nrnodes ;

      insertbefore_wlist((wlistentry_t*)towlist, entry) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int transferall_syncwlist(syncwlist_t * towlist, syncwlist_t * fromwlist)
{
   int err ;

   if (!isempty_syncwlist(fromwlist)) {
      dlist_t        fromdlist = dlist_INIT_LAST(genericcast_dlistnode(fromwlist)) ;
      dlist_t        todlist   = dlist_INIT_LAST(genericcast_dlistnode(towlist)) ;

      err = remove_wlist(&todlist, (wlistentry_t*)towlist) ;
      if (err) goto ONABORT ;

      transfer_wlist(&todlist, &fromdlist) ;
      replacenode_wlist(&todlist, (wlistentry_t*)towlist, (wlistentry_t*)fromwlist) ;
      towlist->nrnodes += fromwlist->nrnodes ;

      init_syncwlist(fromwlist) ;

   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// section: syncwlist_iterator_t

int initfirst_syncwlistiterator(/*out*/syncwlist_iterator_t * iter, syncwlist_t * wlist)
{
   if (isempty_syncwlist(wlist)) {
      iter->next = 0 ;
   } else {
      iter->next = next_dlist(genericcast_dlistnode(wlist)) ;
   }

   iter->wlist = wlist ;
   return 0 ;
}

bool next_syncwlistiterator(syncwlist_iterator_t * iter, /*out*/struct syncevent_t ** event)
{
   if (!iter->next) return false ;

   *event = &((wlistentry_t*)iter->next)->event ;

   iter->next = next_dlist(iter->next) ;

   if (iter->next == (const dlist_node_t*)iter->wlist) {
      iter->next = 0 ;
   }

   return true ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_wlistentry(void)
{
   wlistentry_t   entry = wlistentry_INIT_FREEABLE ;
   wlistentry_t   entries[2] ;
   syncwait_t     syncwait ;

   // TEST wlistentry_INIT_FREEABLE
   TEST(0 == entry.next) ;
   TEST(0 == entry.prev) ;
   TEST(1 == isfree_syncevent(&entry.event)) ;

   // TEST initmove_wlistentry
   entries[0].next = (dlist_node_t*)&entries[1] ;
   entries[0].prev = (dlist_node_t*)&entries[1] ;
   entries[0].event = (syncevent_t) syncevent_INIT_FREEABLE ;
   entries[1].next = (dlist_node_t*)&entries[0] ;
   entries[1].prev = (dlist_node_t*)&entries[0] ;
   entries[1].event = (syncevent_t) syncevent_INIT_FREEABLE ;
   init_syncwait(&syncwait, &(syncthread_t)syncthread_INIT_FREEABLE, &entries[1].event, 0) ;
   initmove_wlistentry(&entry, &entries[1]) ;
   TEST(entries[1].next == 0) ;
   TEST(entries[1].prev == 0) ;
   TEST(entries[1].event.waiting == &syncwait) ;
   TEST(entries[0].next == (dlist_node_t*)&entry) ;
   TEST(entries[0].prev == (dlist_node_t*)&entry) ;
   TEST(entries[0].event.waiting == 0) ;
   TEST(entry.next      == (dlist_node_t*)&entries[0]) ;
   TEST(entry.prev      == (dlist_node_t*)&entries[0]) ;
   TEST(&syncwait       == entry.event.waiting) ;
   TEST(syncwait.event  == &entry.event) ;
   initmove_wlistentry(&entries[1], &entry) ;
   TEST(entries[1].next == (dlist_node_t*)&entries[0]) ;
   TEST(entries[1].prev == (dlist_node_t*)&entries[0]) ;
   TEST(entries[1].event.waiting == &syncwait) ;
   TEST(entries[0].next == (dlist_node_t*)&entries[1]) ;
   TEST(entries[0].prev == (dlist_node_t*)&entries[1]) ;
   TEST(entries[0].event.waiting == 0) ;
   TEST(entry.next      == 0) ;
   TEST(entry.prev      == 0) ;
   TEST(&syncwait       == entry.event.waiting) ;
   TEST(syncwait.event  == &entries[1].event) ;

   // TEST initmove_wlistentry: empty src
   entry.event.waiting = &syncwait ;
   initmove_wlistentry(&entry, &entries[0]) ;
   TEST(entries[1].next == (dlist_node_t*)&entry) ;
   TEST(entries[1].prev == (dlist_node_t*)&entry) ;
   TEST(entries[1].event.waiting == &syncwait) ;
   TEST(entries[0].next == 0) ;
   TEST(entries[0].prev == 0) ;
   TEST(entries[0].event.waiting == 0) ;
   TEST(entry.next      == (dlist_node_t*)&entries[1]) ;
   TEST(entry.prev      == (dlist_node_t*)&entries[1]) ;
   TEST(0               == entry.event.waiting/*cleared*/) ;
   TEST(syncwait.event  == &entries[1].event/*not changed*/) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   syncwlist_t    wlist = syncwlist_INIT_FREEABLE ;
   syncqueue_t    queue = syncqueue_INIT ;   // stores waitentry_t

   // TEST syncwlist_INIT_FREEABLE
   TEST(1 == isfree_syncwlist(&wlist)) ;

   // TEST init_syncwlist
   memset(&wlist, 255, sizeof(wlist)) ;
   init_syncwlist(&wlist) ;
   TEST(wlist.next == genericcast_dlistnode(&wlist)) ;
   TEST(wlist.prev == genericcast_dlistnode(&wlist)) ;
   TEST(wlist.nrnodes == 0) ;

   // TEST free_syncwlist: empty
   wlist.nrnodes = 1 ;
   TEST(0 == free_syncwlist(&wlist, &queue)) ;
   TEST(1 == isfree_syncwlist(&wlist)) ;

   // TEST free_syncwlist: not empty
   init_syncwlist(&wlist) ;
   wlistentry_t * events[100] = { 0 } ;
   dlist_t        dlist       = dlist_INIT_LAST(genericcast_dlistnode(&wlist)) ;
   for (unsigned i = 0; i < lengthof(events); ++i) {
      TEST(i == len_syncqueue(&queue)) ;
      TEST(0 == insert_syncqueue(&queue, &events[i])) ;
      events[i]->event.waiting = (void*) 1 ; // bad value, accessing it would cause abort of program
      insertlast_wlist(&dlist, events[i]) ;
      TEST(events[i]->next == (dlist_node_t*)&wlist) ;
      TEST(events[i]->prev == (dlist_node_t*)(i ? (void*)events[i-1] : (void*)&wlist)) ;
   }
   TEST(0 == free_syncwlist(&wlist, &queue)) ;
   TEST(1 == isfree_syncwlist(&wlist)) ;
   TEST(0 == len_syncqueue(&queue)) ;

   // TEST free_syncwlist: EINVAL
   for (unsigned errcount = 1; errcount <= 5; ++errcount) {
      init_syncwlist(&wlist) ;
      dlist = (dlist_t) dlist_INIT_LAST(genericcast_dlistnode(&wlist)) ;
      for (unsigned i = 0; i < lengthof(events); ++i) {
         TEST(0 == insert_syncqueue(&queue, &events[i])) ;
         events[i]->event.waiting = (void*) 1 ;
         insertlast_wlist(&dlist, events[i]) ;
      }
      init_testerrortimer(&s_syncwlist_errtimer, 3*errcount, EINVAL) ;
      TEST(EINVAL == free_syncwlist(&wlist, &queue)) ;
      TEST(1 == isfree_syncwlist(&wlist)) ;
      TEST(0 == len_syncqueue(&queue)) ;
   }

   // TEST initmove_syncwlist
   syncwlist_t wlistcopy ;
   init_syncwlist(&wlist) ;
   dlist = (dlist_t) dlist_INIT_LAST(genericcast_dlistnode(&wlist)) ;
   for (unsigned i = 0; i < lengthof(events); ++i) {
      TEST(0 == insert_syncqueue(&queue, &events[i])) ;
      events[i]->event.waiting = 0 ;
      insertlast_wlist(&dlist, events[i]) ;
      ++ wlist.nrnodes ;
   }
   initmove_syncwlist(&wlistcopy, &wlist) ;
   TEST(wlistcopy.next    == (dlist_node_t*)events[0]) ;
   TEST(wlistcopy.prev    == (dlist_node_t*)events[lengthof(events)-1]) ;
   TEST(&wlistcopy        == (void*)events[0]->prev) ;
   TEST(&wlistcopy        == (void*)events[lengthof(events)-1]->next) ;
   TEST(wlistcopy.nrnodes == lengthof(events)) ;
   TEST(wlist.next        == (dlist_node_t*)&wlist) ;
   TEST(wlist.prev        == (dlist_node_t*)&wlist) ;
   TEST(wlist.nrnodes     == 0) ;
   TEST(0 == free_syncwlist(&wlistcopy, &queue)) ;
   TEST(1 == isfree_syncwlist(&wlistcopy)) ;
   TEST(0 == len_syncqueue(&queue)) ;

   // TEST initmove_syncwlist: empty list
   wlistcopy = (syncwlist_t) syncwlist_INIT_FREEABLE ;
   init_syncwlist(&wlist) ;
   initmove_syncwlist(&wlistcopy, &wlist) ;
   TEST(wlistcopy.next    == (dlist_node_t*)&wlistcopy) ;
   TEST(wlistcopy.prev    == (dlist_node_t*)&wlistcopy) ;
   TEST(wlistcopy.nrnodes == 0) ;
   TEST(wlist.next        == (dlist_node_t*)&wlist) ;
   TEST(wlist.prev        == (dlist_node_t*)&wlist) ;
   TEST(wlist.nrnodes     == 0) ;

   // unprepare
   TEST(0 == free_syncqueue(&queue)) ;

   return 0 ;
ONABORT:
   free_syncqueue(&queue) ;
   return EINVAL ;
}

static int test_query(void)
{
   syncwlist_t    wlist = syncwlist_INIT_FREEABLE ;
   syncqueue_t    queue = syncqueue_INIT ;

   // TEST isempty_syncwlist
   TEST(1 == isempty_syncwlist(&wlist)) ;
   wlist.prev = (void*) &wlist ;
   wlist.next = (void*) &wlist ;
   TEST(1 == isempty_syncwlist(&wlist)) ;
   wlist.prev = 0 ;
   wlist.next = 0 ;
   wlist.nrnodes = 1 ;
   TEST(0 == isempty_syncwlist(&wlist)) ;
   wlist.nrnodes = 0 ;
   TEST(1 == isempty_syncwlist(&wlist)) ;

   // TEST isfree_syncwlist
   TEST(1 == isfree_syncwlist(&wlist)) ;
   wlist.next = (void*) 1 ;
   TEST(0 == isfree_syncwlist(&wlist)) ;
   wlist.next = (void*) 0 ;
   TEST(1 == isfree_syncwlist(&wlist)) ;
   wlist.prev = (void*) 1 ;
   TEST(0 == isfree_syncwlist(&wlist)) ;
   wlist.prev = (void*) 0 ;
   TEST(1 == isfree_syncwlist(&wlist)) ;
   wlist.nrnodes = 1 ;
   TEST(0 == isfree_syncwlist(&wlist)) ;
   wlist.nrnodes = 0 ;
   TEST(1 == isfree_syncwlist(&wlist)) ;

   // TEST len_syncwlist
   TEST(0 == len_syncwlist(&wlist)) ;
   for (size_t i = 1; i; i <<= 1) {
      wlist.nrnodes = i ;
      TEST(i == len_syncwlist(&wlist)) ;
   }

   // TEST queue_syncwlist
   for (unsigned i = 0; i < pagesizeinbytes_queue(); ++i) {
      wlistentry_t * entry ;
      TEST(0 == insert_syncqueue(&queue, &entry)) ;
      wlist.prev = 0 ; /* not used */
      wlist.next = (dlist_node_t*)entry ;
      // empty queue
      wlist.nrnodes = 0 ;
      TEST(0 == queue_syncwlist(&wlist)) ;
      // non empty queue
      wlist.nrnodes = 1 ;
      TEST(&queue == queue_syncwlist(&wlist)) ;
   }
   TEST(0 == free_syncqueue(&queue)) ;

   // TEST last_syncwlist
   init_syncwlist(&wlist) ;
   init_syncqueue(&queue) ;
   wlistentry_t * entry[129] ;
   for (unsigned i = 0; i < lengthof(entry); ++i) {
      TEST(0 == insert_syncqueue(&queue, &entry[i])) ;
      wlist.prev = genericcast_dlistnode(entry[i]) ;
      wlist.next = genericcast_dlistnode(entry[0]) ;
      entry[i]->next = genericcast_dlistnode(&wlist) ;
      entry[i]->prev = genericcast_dlistnode(&wlist) ;
      if (i) {
         entry[i-1]->next = genericcast_dlistnode(entry[i]) ;
         entry[i]->prev   = genericcast_dlistnode(entry[i-1]) ;
      }
      wlist.nrnodes = 0 ;
      TEST(last_syncwlist(&wlist) == 0/*empty*/) ;
      wlist.nrnodes = 1+i ;
      TEST(last_syncwlist(&wlist) == &entry[i]->event) ;
   }
   TEST(0 == free_syncwlist(&wlist, &queue)) ;
   TEST(0 == len_syncqueue(&queue)) ;

   // unprepare
   TEST(0 == free_syncqueue(&queue)) ;

   return 0 ;
ONABORT:
   free_syncqueue(&queue) ;
   return EINVAL ;
}

static int test_update(void)
{
   syncwlist_t    wlist     = syncwlist_INIT_FREEABLE ;
   syncwlist_t    fromwlist = syncwlist_INIT_FREEABLE ;
   syncqueue_t    queue     = syncqueue_INIT ;  // stores waitentry_t
   syncwait_t     syncwait[100] ;
   syncevent_t *  event = 0 ;
   syncevent_t    removedevent ;

   // prepare
   init_syncwlist(&wlist) ;
   init_syncwlist(&fromwlist) ;
   for (size_t i = 0; i < lengthof(syncwait); ++i) {
      syncwait[i].event         = 0 ;
      syncwait[i].continuelabel = (void*)(i+1) ;
   }

   // TEST insert_syncwlist
   for (size_t i = 1, size = 0; i <= lengthof(syncwait); ++i) {
      event = 0 ;
      TEST(0 == insert_syncwlist(&wlist, &queue, &event)) ;
      TEST(i == wlist.nrnodes) ;
      TEST(0 != event) ;
      TEST(0 == event->waiting) ;
      event->waiting = &syncwait[i-1] ;
      size += sizeof(wlistentry_t) ;
      TEST(size  == sizelast_queue(genericcast_queue(&queue))) ;
      TEST(event == &((wlistentry_t*)last_queue(genericcast_queue(&queue), sizeof(wlistentry_t)))->event) ;
   }

   // TEST insert_syncwlist: ENOMEM
   init_testerrortimer(&s_syncwlist_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == insert_syncwlist(&wlist, &queue, &event)) ;
   TEST(sizelast_queue(genericcast_queue(&queue)) == lengthof(syncwait)*sizeof(wlistentry_t)) ;
   TEST(wlist.nrnodes == lengthof(syncwait)) ;

   // TEST remove_syncwlist
   for (size_t i = 1, size = sizelast_queue(genericcast_queue(&queue)); i <= lengthof(syncwait); ++i) {
      syncwait_t *      s     = &syncwait[i-1] ;
      wlistentry_t *    entry = last_queue(genericcast_queue(&queue), sizeof(wlistentry_t)) ;
      removedevent.waiting = 0 ;
      TEST(0 == remove_syncwlist(&wlist, &queue, &removedevent)) ;
      TEST(i == lengthof(syncwait) - wlist.nrnodes) ;
      TEST(s == removedevent.waiting) ;
      TEST(i == (uintptr_t)removedevent.waiting->continuelabel) ;
      if (i > lengthof(syncwait)/2) {
         TEST(removedevent.waiting->event == &entry->event) ;
      } else {
         TEST(removedevent.waiting->event == 0) ;
      }
      size -= sizeof(wlistentry_t) ;
      TEST(size == sizelast_queue(genericcast_queue(&queue))) ;
   }
   TEST(1 == isempty_queue(genericcast_queue(&queue))) ;

   // TEST remove_syncwlist: ENODATA
   TEST(0 == insert_syncwlist(&wlist, &queue, &event)) ;
   event->waiting = &syncwait[0] ;
   TEST(1 == wlist.nrnodes) ;
   wlist.nrnodes = 0 ;
   TEST(ENODATA == remove_syncwlist(&wlist, &queue, &removedevent)) ;
   TEST(sizeof(wlistentry_t) == sizelast_queue(genericcast_queue(&queue))) ;
   TEST(0 == wlist.nrnodes) ;
   wlist.nrnodes = 1 ;
   TEST(0 == remove_syncwlist(&wlist, &queue, &removedevent)) ;
   TEST(0 == wlist.nrnodes) ;
   TEST(0 == sizelast_queue(genericcast_queue(&queue))) ;
   TEST(&syncwait[0] == removedevent.waiting) ;
   TEST(0 == free_syncwlist(&wlist, &queue)) ;
   TEST(0 == free_syncqueue(&queue)) ;

   // TEST remove_syncwlist: EINVAL
   removedevent.waiting = 0 ;
   for (int i = 2; i >= 0; --i) {
      init_syncwlist(&wlist) ;
      init_syncqueue(&queue) ;
      TEST(0 == insert_syncwlist(&wlist, &queue, &event)) ;
      event->waiting = &syncwait[i] ;
      init_testerrortimer(&s_syncwlist_errtimer, 3-(unsigned)i, EINVAL) ;
      if (!i) {
         TEST(0 == remove_syncwlist(&wlist, &queue, &removedevent)) ;
         TEST(1 == isempty_queue(genericcast_queue(&queue))) ;
      } else {
         TEST(EINVAL == remove_syncwlist(&wlist, &queue, &removedevent)) ;
         TEST(0 == isempty_queue(genericcast_queue(&queue))) ;
      }
      TEST(removedevent.waiting == (i==0 ? &syncwait[i] :0)) ;
      TEST(wlist.nrnodes        == (i==2)) ;
      TEST(0 == free_syncwlist(&wlist, &queue)) ;
      TEST(0 == free_syncqueue(&queue)) ;
   }
   init_testerrortimer(&s_syncwlist_errtimer, 0, 0) ;

   // TEST removeempty_syncwlist
   syncevent_t * events2[100] ;
   init_syncwlist(&wlist) ;
   init_syncqueue(&queue) ;
   for (unsigned i = 0; i < lengthof(events2); ++i) {
      TEST(0 == insert_syncwlist(&wlist, &queue, &events2[i])) ;
   }
   for (unsigned i = lengthof(events2); (i--) > 0; ) {
      // set node not removed
      TEST(events2[i] == last_syncwlist(&wlist)) ;
      events2[i]->waiting = &syncwait[0] ;               // last node not empty
      TEST(0 == removeempty_syncwlist(&wlist, &queue)) ; // nothing removed
      TEST(i+1 == len_syncqueue(&queue)) ;
      TEST(i+1 == len_syncwlist(&wlist)) ;
      // empty node removed
      events2[i]->waiting = 0 ;  // last node empty
      TEST(0 == removeempty_syncwlist(&wlist, &queue)) ; // nothing removed
      TEST(i == len_syncqueue(&queue)) ;
      TEST(i == len_syncwlist(&wlist)) ;
   }

   // TEST removeempty_syncwlist: EINVAL
   for (unsigned i = 1; i <= 3; ++i) {
      init_syncwlist(&wlist) ;
      init_syncqueue(&queue) ;
      TEST(0 == insert_syncwlist(&wlist, &queue, &event)) ;
      init_testerrortimer(&s_syncwlist_errtimer, i, EINVAL) ;
      TEST((i!=3 ? EINVAL:0) == removeempty_syncwlist(&wlist, &queue)) ;
      TEST((i<=2) == len_syncqueue(&queue)) ;
      TEST((i<=1) == len_syncwlist(&wlist)) ;
      TEST(0 == free_syncwlist(&wlist, &queue)) ;
      TEST(0 == free_syncqueue(&queue)) ;
   }
   init_testerrortimer(&s_syncwlist_errtimer, 0, 0) ;

   // TEST transferfirst_syncwlist
   init_syncwlist(&wlist) ;
   init_syncqueue(&queue) ;
   for (size_t i = 1; i <= lengthof(syncwait); ++i) {
      TEST(0 == insert_syncwlist(&fromwlist, &queue, &event)) ;
      event->waiting = &syncwait[lengthof(syncwait)-i] ;
   }
   TEST(fromwlist.nrnodes == lengthof(syncwait)) ;
   TEST(queue.nrelements  == lengthof(syncwait)) ;
   for (size_t i = 1; i <= lengthof(syncwait); ++i) {
      unsigned       j = lengthof(syncwait)-i ;
      syncwait_t *   s = &syncwait[lengthof(syncwait)-i] ;
      transferfirst_syncwlist(&wlist, &fromwlist) ;
      TEST(i == wlist.nrnodes) ;
      TEST(j == fromwlist.nrnodes) ;
      TEST(s == prev_wlist((wlistentry_t*)&wlist)->event.waiting) ;
      TEST(queue.nrelements  == lengthof(syncwait)) ;
   }
   for (size_t i = 1; i <= lengthof(syncwait); ++i) {
      unsigned       j = lengthof(syncwait)-i ;
      syncwait_t *   s = &syncwait[lengthof(syncwait)-i] ;
      remove_syncwlist(&wlist, &queue, &removedevent) ;
      TEST(j == wlist.nrnodes) ;
      TEST(j == queue.nrelements) ;
      TEST(s == removedevent.waiting) ;
   }

   // TEST transferfirst_syncwlist: empty from list does nothing
   TEST(0 == transferfirst_syncwlist(&wlist, &fromwlist)) ;
   TEST(0 == fromwlist.nrnodes) ;
   TEST(0 == wlist.nrnodes) ;
   TEST(wlist.next == (void*)&wlist) ;
   TEST(wlist.prev == (void*)&wlist) ;
   TEST(fromwlist.next == (void*)&fromwlist) ;
   TEST(fromwlist.prev == (void*)&fromwlist) ;

   // TEST transferall_syncwlist: empty target wlist
   for (size_t i = 0; i < lengthof(syncwait); ++i) {
      TEST(0 == insert_syncwlist(&fromwlist, &queue, &event)) ;
      event->waiting = &syncwait[i] ;
   }
   TEST(fromwlist.nrnodes == lengthof(syncwait)) ;
   TEST(queue.nrelements  == lengthof(syncwait)) ;
   TEST(wlist.nrnodes     == 0) ;
   TEST(0 == transferall_syncwlist(&wlist, &fromwlist)) ;
   TEST(fromwlist.nrnodes == 0) ;
   TEST(queue.nrelements  == lengthof(syncwait)) ;
   TEST(wlist.nrnodes     == lengthof(syncwait)) ;
   TEST(fromwlist.next == (void*)&fromwlist) ;
   TEST(fromwlist.prev == (void*)&fromwlist) ;
   // check content
   for (size_t i = 0; i < lengthof(syncwait); ++i) {
      unsigned       j = lengthof(syncwait)-1-i ;
      syncwait_t *   s = &syncwait[i] ;
      remove_syncwlist(&wlist, &queue, &removedevent) ;
      TEST(j == wlist.nrnodes) ;
      TEST(j == queue.nrelements) ;
      TEST(s == removedevent.waiting) ;
   }

   // TEST transferall_syncwlist: non empty wlist
   for (size_t i = 1; i <= lengthof(syncwait)/2; ++i) {
      TEST(0 == insert_syncwlist(&wlist, &queue, &event)) ;
      event->waiting = &syncwait[i-1] ;
      TEST(0 == insert_syncwlist(&fromwlist, &queue, &event)) ;
      event->waiting = &syncwait[i-1+lengthof(syncwait)/2] ;
   }
   TEST(fromwlist.nrnodes == lengthof(syncwait)/2) ;
   TEST(queue.nrelements  == lengthof(syncwait)) ;
   TEST(wlist.nrnodes     == lengthof(syncwait)/2) ;
   TEST(0 == transferall_syncwlist(&wlist, &fromwlist)) ;
   TEST(fromwlist.nrnodes == 0) ;
   TEST(queue.nrelements  == lengthof(syncwait)) ;
   TEST(wlist.nrnodes     == lengthof(syncwait)) ;
   TEST(fromwlist.next == (void*)&fromwlist) ;
   TEST(fromwlist.prev == (void*)&fromwlist) ;
   // check content
   for (size_t i = 0; i < lengthof(syncwait); ++i) {
      unsigned       j = lengthof(syncwait)-1-i ;
      syncwait_t *   s = &syncwait[i] ;
      remove_syncwlist(&wlist, &queue, &removedevent) ;
      TEST(j == wlist.nrnodes) ;
      TEST(j == queue.nrelements) ;
      TEST(s == removedevent.waiting) ;
   }

   // TEST transferall_syncwlist: empty from list does nothing
   TEST(0 == transferall_syncwlist(&wlist, &fromwlist)) ;
   TEST(0 == fromwlist.nrnodes) ;
   TEST(0 == wlist.nrnodes) ;
   TEST(wlist.next == (void*)&wlist) ;
   TEST(wlist.prev == (void*)&wlist) ;
   TEST(fromwlist.next == (void*)&fromwlist) ;
   TEST(fromwlist.prev == (void*)&fromwlist) ;

   // TEST insert_syncwlist, remove_syncwlist, transferall_syncwlist: random
   for (size_t i = 0; i < lengthof(syncwait); ++i) {
      syncwait[i].event = 0 ;
   }
   unsigned seed = 123456 ;
   for (unsigned i = 0; i < 10000; ++i) {
      unsigned i1 = (unsigned)rand_r(&seed) % lengthof(syncwait)/2 ;
      if (syncwait[i1].event) {
         wlistentry_t * entry = (wlistentry_t *) wlist.next ;
         remove_syncwlist(&wlist, &queue, &removedevent) ;
         TEST(removedevent.waiting->event == &entry->event) ;
         removedevent.waiting->event = 0 ;
      } else {
         insert_syncwlist(&wlist, &queue, &event) ;
         event->waiting     = &syncwait[i1] ;
         syncwait[i1].event = event ;
      }
      unsigned i2 = lengthof(syncwait)/2 + (unsigned)rand_r(&seed) % lengthof(syncwait)/2 ;
      if (syncwait[i2].event) {
         wlistentry_t * entry = (wlistentry_t *) fromwlist.next ;
         remove_syncwlist(&fromwlist, &queue, &removedevent) ;
         TEST(removedevent.waiting->event == &entry->event) ;
         removedevent.waiting->event = 0 ;
      } else {
         insert_syncwlist(&fromwlist, &queue, &event) ;
         event->waiting     = &syncwait[i2] ;
         syncwait[i2].event = event ;
      }
   }
   transferall_syncwlist(&wlist, &fromwlist) ;
   for (size_t size = 0; 0 == size; size = 1) {
      for (size_t i = 0; i < lengthof(syncwait); ++i) {
         size += (0 != syncwait[i].event) ;
      }
      TEST(sizelast_queue(genericcast_queue(&queue)) == size*sizeof(wlistentry_t)) ;
      TEST(wlist.nrnodes                             == size) ;
      TEST(fromwlist.nrnodes                         == 0) ;
      TEST(size > 0) ;
      for (size_t i = 0; i < size; ++i) {
         wlistentry_t * entry = (wlistentry_t *) wlist.next ;
         TEST(0 == remove_syncwlist(&wlist, &queue, &removedevent)) ;
         TEST(removedevent.waiting->event == &entry->event) ;
      }
      TEST(0 == sizelast_queue(genericcast_queue(&queue))) ;
      TEST(0 == wlist.nrnodes) ;
   }

   // unprepare
   TEST(0 == free_syncwlist(&wlist, &queue)) ;
   TEST(0 == free_syncwlist(&fromwlist, &queue)) ;
   TEST(0 == free_syncqueue(&queue)) ;

   return 0 ;
ONABORT:
   free_syncwlist(&wlist, &queue) ;
   free_syncwlist(&fromwlist, &queue) ;
   free_syncqueue(&queue) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   syncwlist_t          wlist = syncwlist_INIT_FREEABLE ;
   syncqueue_t          queue = syncqueue_INIT ;
   syncwlist_iterator_t iter  = syncwlist_iterator_INIT_FREEABLE ;
   syncevent_t *        events[129] ;
   syncevent_t *        nextevent ;

   // prepare
   init_syncqueue(&queue) ;
   init_syncwlist(&wlist) ;
   for (unsigned i = 0; i < lengthof(events); ++i) {
      TEST(0 == insert_syncwlist(&wlist, &queue, &events[i])) ;
   }

   // TEST syncwlist_iterator_INIT_FREEABLE
   TEST(iter.next  == 0) ;
   TEST(iter.wlist == 0) ;

   // TEST initfirst_syncwlistiterator: empty list
   syncwlist_t emptywlist ;
   init_syncwlist(&emptywlist) ;
   TEST(0 == initfirst_syncwlistiterator(&iter, &emptywlist)) ;
   TEST(iter.next  == 0) ;
   TEST(iter.wlist == &emptywlist) ;

   // TEST next_syncwlistiterator: empty list
   nextevent = 0 ;
   TEST(0 == next_syncwlistiterator(&iter, &nextevent)) ;
   TEST(iter.next  == 0) ;
   TEST(iter.wlist == &emptywlist) ;
   TEST(nextevent  == 0) ;

   // TEST free_syncwlistiterator
   iter.next  = (void*)1 ;
   iter.wlist = &wlist ;
   TEST(0 == free_syncwlistiterator(&iter)) ;
   TEST(iter.next  == 0) ;
   TEST(iter.wlist == 0) ;

   // TEST initfirst_syncwlistiterator
   TEST(0 == initfirst_syncwlistiterator(&iter, &wlist)) ;
   TEST(iter.next  == (dlist_node_t*)structof(wlistentry_t, event, events[0])) ;
   TEST(iter.wlist == &wlist) ;

   // TEST next_syncwlistiterator
   for (unsigned i = 0; i < lengthof(events); ++i) {
      TEST(iter.next  == (dlist_node_t*)structof(wlistentry_t, event, events[i])) ;
      TEST(1 == next_syncwlistiterator(&iter, &nextevent)) ;
      TEST(iter.wlist == &wlist) ;
      TEST(nextevent  == events[i]) ;
   }
   TEST(iter.next == 0) ;

   // TEST foreach
   for (unsigned i = 0; i == 0; ) {
      foreach (_syncwlist, next, &wlist) {
         TEST(next == events[i]) ;
         ++ i ;
      }
      TEST(i == lengthof(events)) ;
   }

   // unprepare
   TEST(0 == free_syncqueue(&queue)) ;

   return 0 ;
ONABORT:
   free_syncqueue(&queue) ;
   return EINVAL ;
}

int unittest_task_syncwlist()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_wlistentry())     goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_update())         goto ONABORT ;
   if (test_iterator())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
