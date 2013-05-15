/* title: Waitlist Linuximpl

   Implements <Waitlist>.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/sync/waitlist.h
    Header file of <Waitlist>.

   file: C-kern/platform/Linux/sync/waitlist.c
    Linux specific implementation file <Waitlist Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/math/int/atomic.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/sync/waitlist.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/sync/signal.h"
#endif


// section: waitlist_t

/* define: slist_IMPLEMENT_wlist
 * Uses macro <slist_IMPLEMENT> to generate an adapted interface to <slist_t>. */
slist_IMPLEMENT(_wlist, thread_t, wlistnext)

// group: lifetime

int init_waitlist(/*out*/waitlist_t * wlist)
{
   int err ;

   static_assert(offsetof(waitlist_t,last) == offsetof(slist_t,last), "waitlist_t == slist_t") ;
   static_assert(sizeof(waitlist_t) == sizeof(uint32_t) + sizeof(mutex_t) + sizeof(slist_t), "waitlist_t == slist_t") ;

   err = init_mutex(&wlist->lock) ;
   if (err) goto ONABORT ;

   init_wlist(genericcast_slist(wlist)) ;

   wlist->nr_waiting = 0 ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_waitlist(waitlist_t * wlist)
{
   int err ;

   if (atomicread_int(&wlist->nr_waiting)) {
      do {
         trywakeup_waitlist(wlist, 0, 0) ;
      } while (nrwaiting_waitlist(wlist)) ;
   }

   err = free_mutex(&wlist->lock) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

bool isempty_waitlist(waitlist_t * wlist)
{
   slock_mutex(&wlist->lock) ;
   bool isempty = (wlist->nr_waiting == 0) ;
   sunlock_mutex(&wlist->lock) ;
   return isempty ;
}

size_t nrwaiting_waitlist(waitlist_t * wlist)
{
   slock_mutex(&wlist->lock) ;
   size_t nr_waiting = wlist->nr_waiting ;
   sunlock_mutex(&wlist->lock) ;
   return nr_waiting ;
}

// group: synchronize

int wait_waitlist(waitlist_t * wlist)
{
   thread_t * self = self_thread() ;

   slock_mutex(&wlist->lock) ;
   insertlast_wlist(genericcast_slist(wlist), self) ;
   ++ wlist->nr_waiting ;
   sunlock_mutex(&wlist->lock) ;

   bool isWakeup ;
   do {
      suspend_thread() ;
      slock_mutex(&wlist->lock) ;
      isWakeup = (0 == self->wlistnext) ;
      wlist->nr_waiting -= isWakeup ;
      sunlock_mutex(&wlist->lock) ;
   } while (! isWakeup) ;

   return 0 ;
}

int trywakeup_waitlist(waitlist_t * wlist, int (*task_main)(void * main_arg), void * main_arg)
{
   slock_mutex(&wlist->lock) ;

   thread_t * thread = first_wlist(genericcast_slist(wlist)) ;
   if (thread) {

      settask_thread(thread, task_main, main_arg) ;

      int err = removefirst_wlist(genericcast_slist(wlist), &thread) ;
      assert(!err) ;
      assert(!thread->wlistnext /*indicates that thread->task is valid*/) ;

      resume_thread(thread) ;
   }

   sunlock_mutex(&wlist->lock) ;

   return thread ? 0 : EAGAIN ;
}



// section: test

#ifdef KONFIG_UNITTEST

static int thread_waitonwlist(waitlist_t * wlist)
{
   assert(0 == send_rtsignal(0)) ;
   assert(0 == wait_waitlist(wlist)) ;
   assert(0 == send_rtsignal(1)) ;
   return 0 ;
}

static int test_initfree(void)
{
   waitlist_t  wlist       = waitlist_INIT_FREEABLE ;

   // TEST waitlist_INIT_FREEABLE
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;

   // TEST init_waitlist
   wlist.last = (slist_node_t*)1 ;
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;

   // TEST free_waitlist
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;

   return 0 ;
ONABORT:
   free_waitlist(&wlist) ;
   return EINVAL ;
}

static int test_query(void)
{
   waitlist_t  wlist = waitlist_INIT_FREEABLE ;

   // prepare
   TEST(0 == init_waitlist(&wlist)) ;

   // TEST isempty_waitlist
   TEST(1 == isempty_waitlist(&wlist)) ;
   wlist.nr_waiting = 1 ;
   TEST(0 == isempty_waitlist(&wlist)) ;
   wlist.nr_waiting = SIZE_MAX ;
   TEST(0 == isempty_waitlist(&wlist)) ;
   wlist.nr_waiting = 0 ;
   TEST(1 == isempty_waitlist(&wlist)) ;

   // TEST nrwaiting_waitlist
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   for (size_t i = 1; i; i <<= 1) {
      wlist.nr_waiting = i ;
      TEST(i == nrwaiting_waitlist(&wlist)) ;
   }

   // unprepare
   wlist.nr_waiting = 0 ;
   TEST(0 == free_waitlist(&wlist)) ;

   return 0 ;
ONABORT:
   free_waitlist(&wlist) ;
   return EINVAL ;
}

static int test_wait(void)
{
   waitlist_t  wlist       = waitlist_INIT_FREEABLE ;
   thread_t *  threads[20] = { 0 } ;
   thread_t *  next ;

   // TEST waitlist_INIT_FREEABLE
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;

   // TEST init_waitlist
   wlist.last = (slist_node_t*)1 ;
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;

   // TEST free_waitlist
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;

   // TEST wait_waitlist: 1 thread
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(0 == newgeneric_thread(&threads[0], thread_waitonwlist, &wlist)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;
   for (int i = 0; i < 1000000; ++i) {
      pthread_yield() ;
      if (threads[0] == last_wlist(genericcast_slist(&wlist))) break ;
   }
   TEST(threads[0] == last_wlist(genericcast_slist(&wlist))) ;
   TEST(threads[0] == next_wlist(threads[0])) ;
   settask_thread(threads[0], 0, 0) ;
   TEST(1 == wlist.nr_waiting) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;

   // TEST trywakeup_waitlist: 1 thread
   TEST(0 == trywakeup_waitlist(&wlist, (thread_f)1, (void*)2)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == threads[0]->wlistnext) ;
   TEST(1 == (uintptr_t)threads[0]->main_task) ;
   TEST(2 == (uintptr_t)threads[0]->main_arg) ;
   TEST(0 == wait_rtsignal(1, 1)) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == free_waitlist(&wlist)) ;

   // TEST wait_waitlist: waiting group of threads (FIFO)
   TEST(0 == init_waitlist(&wlist)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == newgeneric_thread(&threads[i], thread_waitonwlist, &wlist)) ;
   }
   TEST(0 == wait_rtsignal(0, lengthof(threads))) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      for (int i2 = 0; i2 < 1000000; ++i2) {
         if (threads[i]->wlistnext) break ;
         pthread_yield() ;
      }
      TEST(threads[i]->wlistnext) ;
   }
   TEST(lengthof(threads) == wlist.nr_waiting) ;
   next = last_wlist(genericcast_slist(&wlist)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      next = next_wlist(next) ;
      TEST(next) ;
      next->main_arg = 0 ;
      if (i != lengthof(threads)-1) {
         TEST(next != last_wlist(genericcast_slist(&wlist))) ;
      } else {
         TEST(next == last_wlist(genericcast_slist(&wlist))) ;
      }
   }

   // TEST trywakeup_waitlist: wakeup group of threads
   TEST(EAGAIN == trywait_rtsignal(1)) ;  // no one woken up
   next = first_wlist(genericcast_slist(&wlist)) ;
   for (unsigned i = lengthof(threads); i > 0; --i) {
      thread_t * first = next ;
      next = next_wlist(next) ;
      TEST(first) ;
      // test that first is woken up
      TEST(0 == first->main_arg) ;
      TEST(EAGAIN == trywait_rtsignal(1)) ;
      TEST(i == wlist.nr_waiting) ;
      TEST(0 == trywakeup_waitlist(&wlist, (thread_f)0, (void*)i)) ;
      TEST(0 == first->wlistnext) ;
      TEST(i == (unsigned)first->main_arg) ;
      TEST(0 == wait_rtsignal(1, 1)) ;
      TEST(i == wlist.nr_waiting+1u) ;
      if (i != 1) {
         TEST(next != first) ;
      } else {
         TEST(next == first) ;
      }
      // test that others have not changed
      thread_t * next2 = next ;
      for (unsigned i2 = 0; i2 < i-1u; ++i2) {
         TEST(0 == next2->main_arg) ;
         TEST(0 != next2->wlistnext) ;
         next2 = next_wlist(next2) ;
         if (i2 != i-2) {
            TEST(next2 != next) ;
         } else {
            TEST(next2 == next) ;
         }
      }
      for (unsigned i2 = 0; i2 < lengthof(threads); ++i2) {
         if (threads[i2] == first) {
            TEST(0 == delete_thread(&threads[i2])) ;
         }
      }
   }
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(threads[i] == 0) ;
   }
   TEST(0 == wlist.last) ;
   TEST(0 == free_waitlist(&wlist)) ;

   // TEST free_waitlist: free wakes up all waiters
   TEST(0 == init_waitlist(&wlist)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == newgeneric_thread(&threads[i], thread_waitonwlist, &wlist)) ;
   }
   TEST(0 == wait_rtsignal(0, 20)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      threads[i]->main_task = (thread_f)1 ;
      threads[i]->main_arg  = (void*)13 ;
   }
   for (int i = 0; i < 20; ++i) {
      for (int i2 = 0; i2 < 1000000; ++i2) {
         if (threads[i]->wlistnext) break ;
         pthread_yield() ;
      }
      TEST(threads[i]->wlistnext) ;
   }
   TEST(lengthof(threads) == wlist.nr_waiting) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == wlist.last) ;
   // all woken up
   TEST(0 == wait_rtsignal(1, 20)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == threads[i]->main_task) ;     // cleared in free_waitlist
      TEST(0 == threads[i]->main_arg) ;      // cleared in free_waitlist
      TEST(0 == threads[i]->wlistnext) ;
      TEST(0 == delete_thread(&threads[i])) ;
   }

   // TEST trywakeup_waitlist: EAGAIN
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(EAGAIN == trywakeup_waitlist(&wlist, 0, 0)) ;
   TEST(0 == free_waitlist(&wlist)) ;

   return 0 ;
ONABORT:
   (void) free_waitlist(&wlist) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      (void) delete_thread(&threads[i]) ;
   }
   while (0 == trywait_rtsignal(0)) {
   }
   while (0 == trywait_rtsignal(1)) {
   }
   return EINVAL ;
}

int unittest_platform_sync_waitlist()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_initfree())       goto ONABORT ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_wait())        goto ONABORT ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
