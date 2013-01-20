/* title: Waitlist Linux
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

   file: C-kern/platform/Linux/waitlist.c
    Linux specific implementation file <Waitlist Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/platform/thread.h"
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

// group: helper

static int wakupfirst_nolock_waitlist(waitlist_t * wlist, int (*task_main)(void * start_arg), void * start_arg)
{
   int err ;

   thread_t * thread = first_wlist(genericcast_slist(wlist)) ;

   lock_thread(thread) ;

   thread->task_f   = task_main ;
   thread->task_arg = start_arg ;

   err = removefirst_wlist(genericcast_slist(wlist), &thread) ;
   assert(!err) ;
   assert(!thread->wlistnext /*indicates that thread->task is valid*/) ;

   -- wlist->nr_waiting ;

   unlock_thread(thread) ;

   resume_thread(thread) ;

   return 0 ;
}

// group: implementation

int init_waitlist(/*out*/waitlist_t * wlist)
{
   int err ;

   static_assert(offsetof(waitlist_t,last) == offsetof(slist_t,last), "waitlist_t == slist_t") ;
   static_assert(sizeof(waitlist_t) == sizeof(uint32_t) + sizeof(mutex_t) + sizeof(slist_t), "waitlist_t == slist_t") ;

   err = init_mutex(&wlist->lock) ;
   if (err) goto ONABORT ;

   wlist->nr_waiting = 0 ;

   init_wlist(genericcast_slist(wlist)) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_waitlist(waitlist_t * wlist)
{
   int err ;

   err = free_mutex(&wlist->lock) ;

   while (!isempty_wlist(genericcast_slist(wlist))) {
      int err2 = wakupfirst_nolock_waitlist( wlist, 0, 0 ) ;
      if (err2) err = err2 ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

bool isempty_waitlist(waitlist_t * wlist)
{
   slock_mutex(&wlist->lock) ;
   bool isempty = isempty_wlist(genericcast_slist(wlist)) ;
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

int wait_waitlist(waitlist_t * wlist)
{
   int err ;
   thread_t * self = self_thread() ;

   slock_mutex(&wlist->lock) ;
   err = insertlast_wlist(genericcast_slist(wlist), self) ;
   if (!err) {
      ++ wlist->nr_waiting ;
   }
   sunlock_mutex(&wlist->lock) ;
   if (err) goto ONABORT ;

   bool isSpuriousWakeup ;
   do {
      suspend_thread() ;
      lock_thread(self) ;
      isSpuriousWakeup = (0 != self->wlistnext) ;
      unlock_thread(self) ;
   } while( isSpuriousWakeup ) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int trywakeup_waitlist(waitlist_t * wlist, int (*task_main)(void * start_arg), void * start_arg)
{
   int err ;
   bool isEmpty ;

   slock_mutex(&wlist->lock) ;

   isEmpty = isempty_wlist(genericcast_slist(wlist)) ;
   if (isEmpty) {
      err = EAGAIN ;
   } else {
      err = wakupfirst_nolock_waitlist(wlist, task_main, start_arg ) ;
   }

   sunlock_mutex(&wlist->lock) ;

   if (isEmpty) {
      return err ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
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
   waitlist_t  wlist    = waitlist_INIT_FREEABLE ;
   thread_t    * thread = 0 ;
   thread_t    * next ;

   // TEST static init
   TEST(0 == wlist.last) ;

   // TEST init, double free
   wlist.last = (slist_node_t*)1 ;
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;

   // TEST waiting 1 thread
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(0 == new_thread(&thread, thread_waitonwlist, &wlist)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;
   for (int i = 0; i < 1000000; ++i) {
      pthread_yield() ;
      if (thread == last_wlist(genericcast_slist(&wlist))) break ;
   }
   TEST(thread == last_wlist(genericcast_slist(&wlist))) ;
   TEST(thread == next_wlist(thread)) ;
   thread->task_arg = 0 ;
   thread->task_f   = 0 ;
   TEST(0 == isempty_waitlist(&wlist)) ;
   TEST(1 == nrwaiting_waitlist(&wlist)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == trywakeup_waitlist(&wlist, (thread_task_f)1, (void*)2 )) ;
   TEST(0 == wlist.last) ;
   TEST(0 == thread->wlistnext) ;
   TEST((thread_task_f)1 == thread->task_f) ;
   TEST((void*)2         == thread->task_arg) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   TEST(0 == wait_rtsignal(1, 1)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == free_waitlist(&wlist)) ;

   // TEST waiting group of threads (FIFO)
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   TEST(0 == newgeneric_thread(&thread, thread_waitonwlist, &wlist, 20)) ;
   TEST(0 == wait_rtsignal(0, 20)) ;
   TEST(0 != wlist.last) ;
   TEST(0 == isempty_waitlist(&wlist)) ;
   next = thread ;
   for (int i = 0; i < 20; ++i) {
      for (int i2 = 0; i2 < 1000000; ++i2) {
         TEST(EAGAIN == trywait_rtsignal(1)) ;
         if (next->wlistnext) break ;
         pthread_yield() ;
      }
      TEST(next->wlistnext) ;
      next = next->groupnext ;
      TEST(next) ;
   }
   TEST(20 == nrwaiting_waitlist(&wlist)) ;
      // list has 20 members !
   next = last_wlist(genericcast_slist(&wlist)) ;
   for (int i = 0; i < 20; ++i) {
      next = next_wlist(next) ;
      TEST(next) ;
      next->task_arg = 0 ;
      if (i != 19) {
         TEST(next != last_wlist(genericcast_slist(&wlist))) ;
      } else {
         TEST(next == last_wlist(genericcast_slist(&wlist))) ;
      }
   }
      // wakeup all members
   next = first_wlist(genericcast_slist(&wlist)) ;
   for (int i = 0; i < 20; ++i) {
      thread_t * first = next ;
      next = next_wlist(next) ;
      TEST(first) ;
      // test that first is woken up
      TEST(0 == first->task_arg) ;
      TEST(EAGAIN == trywait_rtsignal(1)) ;
      TEST(20-i == (int)nrwaiting_waitlist(&wlist)) ;
      TEST(0 == trywakeup_waitlist(&wlist, (thread_task_f)0, (void*)(i+1) )) ;
      TEST(19-i == (int)nrwaiting_waitlist(&wlist)) ;
      TEST(0 == first->wlistnext) ;
      TEST(i+1 == (int)first->task_arg) ;
      TEST(0 == wait_rtsignal(1, 1)) ;
      if (i != 19) {
         TEST(next != first) ;
      } else {
         TEST(next == first) ;
      }
      // test that others are not changed
      thread_t * next2 = next ;
      for (int i2 = i; i2 < 19; ++i2) {
         TEST(0 == next2->task_arg) ;
         TEST(0 != next2->wlistnext) ;
         next2 = next_wlist(next2) ;
         if (i2 != 18) {
            TEST(next2 != next) ;
         } else {
            TEST(next2 == next) ;
         }
      }
   }
   TEST(0 == wlist.last) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;

   // TEST free wakes up all waiters
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == newgeneric_thread(&thread, thread_waitonwlist, &wlist, 20)) ;
   TEST(0 == wait_rtsignal(0, 20)) ;
   TEST(0 != wlist.last) ;
   TEST(0 == isempty_waitlist(&wlist)) ;
   next = thread ;
   for (int i = 0; i < 20; ++i) {
      next->task_arg = (void*)13 ;
      next = next->groupnext ;
      TEST(next) ;
   }
   next = thread ;
   for (int i = 0; i < 20; ++i) {
      for (int i2 = 0; i2 < 1000000; ++i2) {
         TEST(EAGAIN == trywait_rtsignal(1)) ;
         if (next->wlistnext) break ;
         pthread_yield() ;
      }
      TEST(next->wlistnext) ;
      next = next->groupnext ;
      TEST(next) ;
   }
   TEST(20 == nrwaiting_waitlist(&wlist)) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wait_rtsignal(1, 20)) ;
   next = thread ;
   for (int i = 0; i < 20; ++i) {
      // free_waitlist sets command to 0
      TEST(0 == next->task_arg)
      TEST(0 == next->wlistnext) ;
      next = next->groupnext ;
      TEST(next) ;
   }
   TEST(next == thread) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST EAGAIN
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(EAGAIN == trywakeup_waitlist(&wlist, 0, 0)) ;
   TEST(0 == free_waitlist(&wlist)) ;

   return 0 ;
ONABORT:
   (void) free_waitlist(&wlist) ;
   (void) delete_thread(&thread) ;
   while( 0 == trywait_rtsignal(0) ) ;
   while( 0 == trywait_rtsignal(1) ) ;
   return EINVAL ;
}

int unittest_platform_sync_waitlist()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_initfree())       goto ONABORT ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
