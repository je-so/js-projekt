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

   file: C-kern/api/os/sync/waitlist.h
    Header file of <Waitlist>.

   file: C-kern/os/Linux/waitlist.c
    Linux specific implementation file <Waitlist Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/sync/waitlist.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/os/sync/signal.h"
#endif

// section: waitlist_t

/* typedef: wlist_t
 * Another name for <waitlist_t>.
 * Used in <slist_IMPLEMENT> to offer an adapted interface to <slist_t>. */
typedef waitlist_t   wlist_t ;

slist_IMPLEMENT(wlist, wlistnext, void)

// group: helper

static int wakupfirst_nolock_waitlist(waitlist_t * wlist, task_callback_f task_main, callback_param_t * start_arg)
{
   int err ;

   osthread_t * thread = first_wlist(wlist) ;

   lock_osthread(thread) ;

   thread->task.fct = task_main ;
   thread->task.arg = start_arg ;

   err = removefirst_wlist(wlist, &thread) ;
   assert(!err) ;
   assert(!thread->wlistnext /*indicates that command is valid*/) ;

   -- wlist->nr_waiting ;

   unlock_osthread(thread) ;

   resume_osthread(thread) ;

   return 0 ;
}

// group: implementation

int init_waitlist(/*out*/waitlist_t * wlist)
{
   int err ;

   static_assert(offsetof(waitlist_t,last) == offsetof(slist_t,last), "waitlist_t == slist_t") ;
   static_assert(sizeof(waitlist_t) == sizeof(uint32_t) + sizeof(mutex_t) + sizeof(slist_t), "waitlist_t == slist_t") ;

   err = init_mutex(&wlist->lock) ;
   if (err) goto ABBRUCH ;

   wlist->nr_waiting = 0 ;

   init_wlist(wlist) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int free_waitlist(waitlist_t * wlist)
{
   int err ;

   err = free_mutex(&wlist->lock) ;

   while( !isempty_wlist(wlist) ) {
      int err2 = wakupfirst_nolock_waitlist( wlist, (task_callback_f)0, (callback_param_t*)0 ) ;
      if (err2) err = err2 ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

bool isempty_waitlist(waitlist_t * wlist)
{
   slock_mutex(&wlist->lock) ;
   bool isempty = isempty_wlist(wlist) ;
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
   osthread_t * self = self_osthread() ;

   slock_mutex(&wlist->lock) ;
   err = insertlast_wlist(wlist, self) ;
   if (!err) {
      ++ wlist->nr_waiting ;
   }
   sunlock_mutex(&wlist->lock) ;
   if (err) goto ABBRUCH ;

   bool isSpuriousWakeup ;
   do {
      suspend_osthread() ;
      lock_osthread(self) ;
      isSpuriousWakeup = (0 != self->wlistnext) ;
      unlock_osthread(self) ;
   } while( isSpuriousWakeup ) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int trywakeup_waitlist(waitlist_t * wlist, task_callback_f task_main, callback_param_t * start_arg)
{
   int err ;
   bool isEmpty ;

   slock_mutex(&wlist->lock) ;

   isEmpty = isempty_wlist(wlist) ;
   if (isEmpty) {
      err = EAGAIN ;
   } else {
      err = wakupfirst_nolock_waitlist( wlist, task_main, start_arg ) ;
   }

   sunlock_mutex(&wlist->lock) ;

   if (isEmpty) {
      return err ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_sync_waitlist,ABBRUCH)

static int thread_waitonwlist(waitlist_t * wlist)
{
   assert(0 == send_rtsignal(0)) ;
   assert(0 == wait_waitlist(wlist)) ;
   assert(0 == send_rtsignal(1)) ;
   return 0 ;
}

static int test_initfree(void)
{
   waitlist_t     wlist  = waitlist_INIT_FREEABLE ;
   osthread_t   * thread = 0 ;
   osthread_t   * next ;

   // TEST static init
   TEST(0 == wlist.last) ;

   // TEST init, double free
   wlist.last = (osthread_t*)1 ;
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
   TEST(0 == new_osthread(&thread, thread_waitonwlist, &wlist)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;
   for(int i = 0; i < 1000000; ++i) {
      pthread_yield() ;
      if (thread == wlist.last) break ;
   }
   TEST(thread == wlist.last) ;
   TEST(thread == thread->wlistnext) ;
   thread->task = (task_callback_t) task_callback_INIT_FREEABLE ;
   TEST(0 == isempty_waitlist(&wlist)) ;
   TEST(1 == nrwaiting_waitlist(&wlist)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == trywakeup_waitlist(&wlist, (task_callback_f)1, (callback_param_t*)2 )) ;
   TEST(0 == wlist.last) ;
   TEST(0 == thread->wlistnext) ;
   TEST(thread->task.fct = (task_callback_f)1) ;
   TEST(thread->task.arg = (void*)2) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   TEST(0 == wait_rtsignal(1, 1)) ;
   TEST(0 == delete_osthread(&thread)) ;
   TEST(0 == free_waitlist(&wlist)) ;

   // TEST waiting group of threads (FIFO)
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   TEST(0 == newgroup_osthread(&thread, thread_waitonwlist, &wlist, 20)) ;
   TEST(0 == wait_rtsignal(0, 20)) ;
   TEST(0 != wlist.last) ;
   TEST(0 == isempty_waitlist(&wlist)) ;
   next = thread ;
   for(int i = 0; i < 20; ++i) {
      for(int i2 = 0; i2 < 1000000; ++i2) {
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
   next = wlist.last ;
   for(int i = 0; i < 20; ++i) {
      osthread_t * prev = next ;
      next = next_wlist(next) ;
      TEST(next) ;
      TEST(prev->wlistnext == next) ;
      next->task.arg = 0 ;
      if (i != 19) {
         TEST(next != wlist.last) ;
      } else {
         TEST(next == wlist.last) ;
      }
   }
      // wakeup all members
   next = first_wlist(&wlist) ;
   for(int i = 0; i < 20; ++i) {
      osthread_t * first = next ;
      next = next_wlist(next) ;
      TEST(first) ;
      // test that first is woken up
      TEST(0 == first->task.arg) ;
      TEST(EAGAIN == trywait_rtsignal(1)) ;
      TEST(20-i == (int)nrwaiting_waitlist(&wlist)) ;
      TEST(0 == trywakeup_waitlist(&wlist, (task_callback_f)0, (callback_param_t*)(i+1) )) ;
      TEST(19-i == (int)nrwaiting_waitlist(&wlist)) ;
      TEST(0 == first->wlistnext) ;
      TEST(i+1 == (int)first->task.arg) ;
      TEST(0 == wait_rtsignal(1, 1)) ;
      if (i != 19) {
         TEST(next != first) ;
      } else {
         TEST(next == first) ;
      }
      // test that others are not changed
      osthread_t * next2 = next ;
      for(int i2 = i; i2 < 19; ++i2) {
         TEST(0 == next2->task.arg) ;
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
   TEST(0 == join_osthread(thread)) ;
   TEST(0 == delete_osthread(&thread)) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;

   // TEST free wakes up all waiters
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(0 == newgroup_osthread(&thread, thread_waitonwlist, &wlist, 20)) ;
   TEST(0 == wait_rtsignal(0, 20)) ;
   TEST(0 != wlist.last) ;
   TEST(0 == isempty_waitlist(&wlist)) ;
   next = thread ;
   for(int i = 0; i < 20; ++i) {
      next->task.arg = (callback_param_t*)13 ;
      next = next->groupnext ;
      TEST(next) ;
   }
   next = thread ;
   for(int i = 0; i < 20; ++i) {
      for(int i2 = 0; i2 < 1000000; ++i2) {
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
   for(int i = 0; i < 20; ++i) {
      // free_waitlist sets command to 0
      TEST(0 == (int)next->task.arg)
      TEST(0 == next->wlistnext) ;
      next = next->groupnext ;
      TEST(next) ;
   }
   TEST(next == thread) ;
   TEST(0 == delete_osthread(&thread)) ;

   // TEST EAGAIN
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(true == isempty_waitlist(&wlist)) ;
   TEST(EAGAIN == trywakeup_waitlist(&wlist, 0, 0)) ;
   TEST(0 == free_waitlist(&wlist)) ;

   return 0 ;
ABBRUCH:
   (void) free_waitlist(&wlist) ;
   (void) delete_osthread(&thread) ;
   while( 0 == trywait_rtsignal(0) ) ;
   while( 0 == trywait_rtsignal(1) ) ;
   return EINVAL ;
}

int unittest_os_sync_waitlist()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_initfree())       goto ABBRUCH ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
