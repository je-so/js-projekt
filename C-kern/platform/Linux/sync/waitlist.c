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
#include "C-kern/api/platform/sync/waitlist.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/math/int/atomic.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/sync/mutex.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: waitlist_t

// group: helper

/* define: slist_IMPLEMENT_wlist
 * Uses macro <slist_IMPLEMENT> to generate an adapted interface of <slist_t>. */
slist_IMPLEMENT(_wlist, thread_t, nextwait)

// group: lifetime

int init_waitlist(/*out*/waitlist_t * wlist)
{
   init_wlist(genericcast_slist(wlist)) ;

   wlist->nr_waiting = 0 ;
   wlist->lockflag   = 0 ; ;

   return 0 ;
}

int free_waitlist(waitlist_t * wlist)
{
   while (nrwaiting_waitlist(wlist)) {
      trywakeup_waitlist(wlist, 0, 0) ;
   }

   return 0 ;
}

// group: query

bool isempty_waitlist(waitlist_t * wlist)
{
   bool isempty = (0 == atomicread_int(&wlist->nr_waiting)) ;
   return isempty ;
}

size_t nrwaiting_waitlist(waitlist_t * wlist)
{
   size_t nr_waiting = atomicread_int(&wlist->nr_waiting) ;
   return nr_waiting ;
}

// group: synchronize

/* function: lockflag_waitlist
 * Wait until wlist->lockflag is cleared and set it atomically.
 * Includes an acuire memory barrier. Calling thread can see what
 * was written by other threads before the flag was set. */
static inline void lockflag_waitlist(waitlist_t * wlist)
{
   while (0 != atomicset_int(&wlist->lockflag)) {
      yield_thread() ;
   }
}

/* function: unlockflag_waitlist
 * Clear wlist->lockflag.
 * Include a release memory barrier. All other threads can see what was written before
 * flag was cleared. */
static inline void unlockflag_waitlist(waitlist_t * wlist)
{
   atomicclear_int(&wlist->lockflag) ;
}

int wait_waitlist(waitlist_t * wlist)
{
   thread_t *  self = self_thread() ;

   lockflag_waitlist(wlist) ;
   insertlast_wlist(genericcast_slist(wlist), self) ;
   ++ wlist->nr_waiting ;
   unlockflag_waitlist(wlist) ;

   // waiting loop
   for (;;) {
      suspend_thread() ;

      lockflag_thread(self_thread()) ;
      bool isWakeup = (0 == self->nextwait) ;
      unlockflag_thread(self_thread()) ;

      if (isWakeup) break ;
      // spurious resume
   }

   return 0 ;
}

int trywakeup_waitlist(waitlist_t * wlist, int (*main_task)(void * main_arg), void * main_arg)
{
   lockflag_waitlist(wlist) ;

   thread_t * thread = first_wlist(genericcast_slist(wlist)) ;

   if (thread) {
      lockflag_thread(thread) ;
      (void) removefirst_wlist(genericcast_slist(wlist), &thread) ;
      -- wlist->nr_waiting ;
      unlockflag_waitlist(wlist) ;

      settask_thread(thread, main_task, main_arg) ;
      resume_thread(thread) ;
      unlockflag_thread(thread) ;

   } else {
      unlockflag_waitlist(wlist) ;
   }

   return thread ? 0 : EAGAIN ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   waitlist_t  wlist = waitlist_INIT_FREEABLE ;

   // TEST waitlist_INIT_FREEABLE
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == wlist.lockflag) ;

   // TEST init_waitlist
   memset(&wlist, 255 , sizeof(wlist)) ;
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == wlist.lockflag) ;

   // TEST free_waitlist
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == wlist.lockflag) ;
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == wlist.lockflag) ;

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

static unsigned s_thread_runcount = 0 ;

static int thread_waitonwlist(waitlist_t * wlist)
{
   atomicadd_int(&s_thread_runcount, 1) ;
   int err = wait_waitlist(wlist) ;
   atomicsub_int(&s_thread_runcount, 1) ;
   if (err) CLEARBUFFER_LOG() ;
   return err ;
}

static int thread_callwakeup(waitlist_t * wlist)
{
   atomicadd_int(&s_thread_runcount, 1) ;
   int err = trywakeup_waitlist(wlist, (thread_f)3, (void*)4) ;
   atomicsub_int(&s_thread_runcount , 1) ;
   if (err) CLEARBUFFER_LOG() ;
   return err ;
}

static int test_synchronize(void)
{
   waitlist_t  wlist       = waitlist_INIT_FREEABLE ;
   thread_t *  threads[20] = { 0 } ;

   // TEST lockflag_waitlist
   lockflag_waitlist(&wlist) ;
   TEST(0 != wlist.lockflag) ;

   // TEST unlockflag_waitlist
   unlockflag_waitlist(&wlist) ;
   TEST(0 == wlist.lockflag) ;
   unlockflag_waitlist(&wlist) ;
   TEST(0 == wlist.lockflag) ;

   // TEST wait_waitlist
   TEST(0 == init_waitlist(&wlist)) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_waitonwlist, &wlist)) ;
   while (  0 == nrwaiting_waitlist(&wlist)
            || 0 != atomicread_int(&wlist.lockflag)) {
      yield_thread() ;
   }
   TEST(1                == atomicread_int(&s_thread_runcount)) ;
   TEST(wlist.last       == asnode_wlist(threads[0])) ;
   TEST(wlist.nr_waiting == 1) ;
   TEST(wlist.lockflag   == 0) ;
   TEST(threads[0]       == next_wlist(threads[0])) ;

   // TEST wait_waitlist: resume
   lockflag_waitlist(&wlist) ;
   threads[0]->nextwait = 0 ;
   wlist.last = 0 ;
   unlockflag_waitlist(&wlist) ;
   resume_thread(threads[0]) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == atomicread_int(&s_thread_runcount)) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == atomicread_int((uintptr_t*)&wlist.last)) ;
   TEST(1 == wlist.nr_waiting) ; // not changed (changed in trywakeup)
   TEST(0 == wlist.lockflag) ;
   wlist.nr_waiting = 0 ;

   // TEST wait_waitlist: active waiting on lockflag
   lockflag_waitlist(&wlist) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_waitonwlist, &wlist)) ;
   while (0 == atomicread_int(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 3; ++i) {
      yield_thread() ;
      TEST(0 == nrwaiting_waitlist(&wlist)) ;
      TEST(1 == atomicread_int(&s_thread_runcount)) ;
   }
   unlockflag_waitlist(&wlist) ;
   while (  0 == nrwaiting_waitlist(&wlist)
            || 0 != atomicread_int(&wlist.lockflag)) {
      yield_thread() ;
   }
   TEST(wlist.last       == asnode_wlist(threads[0])) ;
   TEST(wlist.nr_waiting == 1) ;
   TEST(wlist.lockflag   == 0) ;
   TEST(threads[0]       == next_wlist(threads[0])) ;
   // wait_waitlist locks lockflag of thread
   lockflag_thread(threads[0]) ;
   threads[0]->nextwait = 0 ;
   atomicwrite_int((uintptr_t*)&wlist.last, 0) ;
   resume_thread(threads[0]) ;
   for (int i = 0; i < 5; ++i) {
      yield_thread() ;
      TEST(1 == atomicread_int(&s_thread_runcount)) ;
   }
   unlockflag_thread(threads[0]) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == atomicread_int(&s_thread_runcount)) ;
   TEST(0 == wlist.last) ;
   TEST(1 == wlist.nr_waiting) ;
   TEST(0 == wlist.lockflag) ;
   wlist.nr_waiting = 0 ;

   // TEST wait_waitlist: insert into list
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == newgeneric_thread(&threads[i], &thread_waitonwlist, &wlist)) ;
      while (  i == nrwaiting_waitlist(&wlist)
               || 0 != atomicread_int(&wlist.lockflag)) {
         yield_thread() ;
      }
      TEST(i+1              == atomicread_int(&s_thread_runcount)) ;
      TEST(wlist.last       == asnode_wlist(threads[i])) ;
      TEST(wlist.nr_waiting == i+1) ;
      TEST(wlist.lockflag   == 0) ;
   }

   // TEST wait_waitlist: wait until resume_thread && nextwait == 0
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      resume_thread(threads[i]) ;   // spurious wakeup
      for (int i2 = 0; i2 < 5; ++i2) {
         yield_thread() ;
         TEST(lengthof(threads)-i == atomicread_int(&s_thread_runcount)) ;
      }
      lockflag_thread(threads[i]) ;
      thread_t * firstthread = 0 ;
      TEST(0 == removefirst_wlist(genericcast_slist(&wlist), &firstthread)) ;
      TEST(threads[i]           == firstthread) ;
      TEST(threads[i]->nextwait == 0) ;
      unlockflag_thread(threads[i]) ;
      resume_thread(threads[i]) ;   // real wakeup
      TEST(0 == join_thread(threads[i])) ;
      TEST(0 == returncode_thread(threads[i])) ;
      TEST(0 == delete_thread(&threads[i])) ;
      TEST(lengthof(threads)-1-i == atomicread_int(&s_thread_runcount)) ;
      TEST(wlist.last        == (i+1 < lengthof(threads) ? asnode_wlist(threads[lengthof(threads)-1]) : 0)) ;
      TEST(wlist.nr_waiting  == lengthof(threads)) ;
      TEST(wlist.lockflag    == 0) ;
   }

   // TEST trywakeup_waitlist
   TEST(0 == self_thread()->nextwait) ;
   insertlast_wlist(genericcast_slist(&wlist), self_thread()) ;
   TEST(0 != self_thread()->nextwait) ;
   wlist.nr_waiting = 1 ;
   settask_thread(self_thread(), 0, 0) ;
   trysuspend_thread() ;   // consume any previous resume
   TEST(EAGAIN == trysuspend_thread()) ;
   TEST(0 == trywakeup_waitlist(&wlist, (thread_f)1, (void*)2)) ;
   TEST(0 == trysuspend_thread()) ;                   // resumed
   TEST(0 == self_thread()->nextwait) ;               // removed from list
   TEST(1 == (uintptr_t)self_thread()->main_task) ;   // settask called
   TEST(2 == (uintptr_t)self_thread()->main_arg) ;    // settask called
   TEST(0 == wlist.last) ;       // removed from list
   TEST(0 == wlist.nr_waiting) ; // nr_waiting changed
   TEST(0 == wlist.lockflag) ;

   // TEST trywakeup_waitlist: active waiting on lockflag
   TEST(0 == self_thread()->nextwait) ;
   insertlast_wlist(genericcast_slist(&wlist), self_thread()) ;
   TEST(0 != self_thread()->nextwait) ;
   wlist.nr_waiting = 1 ;
   settask_thread(self_thread(), 0, 0) ;
   lockflag_waitlist(&wlist) ;
   trysuspend_thread() ;   // consume any previous resume
   TEST(0 == atomicread_int(&s_thread_runcount)) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_callwakeup, &wlist)) ;
   while (0 == atomicread_int(&s_thread_runcount)) {
      yield_thread() ;
   }
   TEST(1 == atomicread_int(&s_thread_runcount)) ;
   for (int i = 0; i < 3; ++i) {
      yield_thread() ;
      TEST(EAGAIN == trysuspend_thread()) ;  // no resume cause of waiting
   }
   unlockflag_waitlist(&wlist) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == atomicread_int(&s_thread_runcount)) ;
   TEST(0 == trysuspend_thread()) ;                   // resumed
   TEST(0 == self_thread()->nextwait) ;               // resumed
   TEST(3 == (uintptr_t)self_thread()->main_task) ;   // set
   TEST(4 == (uintptr_t)self_thread()->main_arg) ;    // set
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == wlist.lockflag) ;

   // TEST trywakeup_waitlist: active waiting lockflag of woken up thread
   TEST(0 == self_thread()->nextwait) ;
   insertlast_wlist(genericcast_slist(&wlist), self_thread()) ;
   TEST(0 != self_thread()->nextwait) ;
   wlist.nr_waiting = 1 ;
   settask_thread(self_thread(), 0, 0) ;
   lockflag_thread(self_thread()) ;
   trysuspend_thread() ;   // consume any previous resume
   TEST(0 == atomicread_int(&s_thread_runcount)) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_callwakeup, &wlist)) ;
   while (0 == atomicread_int(&s_thread_runcount)) {
      yield_thread() ;
   }
   TEST(1 == atomicread_int(&s_thread_runcount)) ;
   for (int i = 0; i < 3; ++i) {
      yield_thread() ;
      TEST(EAGAIN == trysuspend_thread()) ;  // no resume cause of waiting
   }
   unlockflag_thread(self_thread()) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == atomicread_int(&s_thread_runcount)) ;
   TEST(0 == trysuspend_thread()) ;                   // resumed
   TEST(0 == self_thread()->nextwait) ;               // resumed
   TEST(3 == (uintptr_t)self_thread()->main_task) ;   // set
   TEST(4 == (uintptr_t)self_thread()->main_arg) ;    // set
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.nr_waiting) ;
   TEST(0 == wlist.lockflag) ;

   // TEST trywakeup_waitlist: EAGAIN
   TEST(EAGAIN == trywakeup_waitlist(&wlist, 0, 0)) ;

   // TEST trywakeup_waitlist: sends resume and removes them from internal list
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == newgeneric_thread(&threads[i], &thread_waitonwlist, &wlist)) ;
      while (i+1 != nrwaiting_waitlist(&wlist)) {
         yield_thread() ;
      }
   }
   TEST(lengthof(threads) == atomicread_int(&s_thread_runcount)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == trywakeup_waitlist(&wlist, (thread_f)i, (void*)i)) ;
      TEST(i == (uintptr_t)threads[i]->main_task) ;
      TEST(i == (uintptr_t)threads[i]->main_arg) ;
      TEST(0 == join_thread(threads[i])) ;
      TEST(0 == returncode_thread(threads[i])) ;
      TEST(0 == delete_thread(&threads[i])) ;
      TEST(lengthof(threads)-1-i == nrwaiting_waitlist(&wlist)) ;
      TEST(wlist.last            == (i+1 < lengthof(threads) ? asnode_wlist(threads[lengthof(threads)-1]) : 0)) ;
      TEST(wlist.lockflag        == 0) ;
   }

   // TEST free_waitlist: calls trywakeup_waitlist
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == newgeneric_thread(&threads[i], &thread_waitonwlist, &wlist)) ;
      while (i+1 != nrwaiting_waitlist(&wlist)) {
         yield_thread() ;
      }
      settask_thread(threads[i], (thread_f)1, (void*)1) ;
   }
   TEST(0 == free_waitlist(&wlist)) ;
   TEST(0 == nrwaiting_waitlist(&wlist)) ;
   TEST(0 == wlist.last) ;
   TEST(0 == wlist.lockflag) ;
   // check that all threads have been resumed
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == threads[i]->main_task) ;     // cleared in free_waitlist
      TEST(0 == threads[i]->main_arg) ;      // cleared in free_waitlist
      TEST(0 == threads[i]->nextwait) ;
      TEST(0 == delete_thread(&threads[i])) ;
   }

   return 0 ;
ONABORT:
   unlockflag_waitlist(&wlist) ;
   (void) free_waitlist(&wlist) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      (void) delete_thread(&threads[i]) ;
   }
   return EINVAL ;
}

int unittest_platform_sync_waitlist()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_initfree())       goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_synchronize())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
