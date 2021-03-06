/* title: InterThreadMutex impl

   Implements <InterThreadMutex>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/sync/thrmutex.h
    Header file <InterThreadMutex>.

   file: C-kern/platform/Linux/sync/thrmutex.c
    Implementation file <InterThreadMutex impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/sync/thrmutex.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/dlist.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/platform/task/thread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/process.h"
#endif


// section: thrmutex_t

// group: helper

/* define: INTERFACE_thrmutexlist
 * Use macro <dlist_IMPLEMENT> to generate an adapted interface of <dlist_t> to <thread_t>. */
dlist_IMPLEMENT(_thrmutexlist, thread_t, wait.)

// group: lifetime

int free_thrmutex(thrmutex_t * mutex)
{
   int err ;
   bool isFree ;

   isFree = (0 == set_atomicflag(&mutex->lockflag)) ;

   if (isFree) {
      isFree = (0 == mutex->last && 0 == mutex->lockholder) ;
      clear_atomicflag(&mutex->lockflag) ;
   }

   if (! isFree) {
      err = EBUSY ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

// group: query

bool isfree_thrmutex(thrmutex_t * mutex)
{
   return   0 == read_atomicint((uintptr_t*)&mutex->last)
            && 0 == mutex->lockholder
            && 0 == mutex->lockflag ;
}

bool islocked_thrmutex(thrmutex_t * mutex)
{
   return 0 != read_atomicint((uintptr_t*)&mutex->lockholder) ;
}

bool iswaiting_thrmutex(thrmutex_t * mutex)
{
   return 0 != read_atomicint((uintptr_t*)&mutex->last) ;
}

thread_t * lockholder_thrmutex(thrmutex_t * mutex)
{
   return (thread_t*) read_atomicint((uintptr_t*)&mutex->lockholder) ;
}

// group: synchronize

/* function: lockflag_thrmutex
 * Waits until mutex->lockflag is cleared and sets it atomically.
 * Includes an acuire memory barrier. Calling thread can see what
 * was written by other threads before the flag was set. */
static inline void lockflag_thrmutex(thrmutex_t * mutex)
{
   while (0 != set_atomicflag(&mutex->lockflag)) {
      yield_thread() ;
   }
}

/* function: unlockflag_thrmutex
 * clears mutex->lockflag.
 * Includes a release memory barrier. All other threads can see what was written before
 * flag is cleared. */
static inline void unlockflag_thrmutex(thrmutex_t * mutex)
{
   clear_atomicflag(&mutex->lockflag) ;
}

int lock_thrmutex(thrmutex_t * mutex)
{
   int err ;
   thread_t * const self = self_thread() ;

   lockflag_thrmutex(mutex) ;

   if (!mutex->lockholder) {
      mutex->lockholder = self ;
      unlockflag_thrmutex(mutex) ;

   } else {
      // add to wait list
      if (self == mutex->lockholder) {
         err = EDEADLK ;
         goto ONERR;
      }
      insertlast_thrmutexlist(cast_dlist(mutex), self) ;
      unlockflag_thrmutex(mutex) ;

      // suspend
      for (;;) {
         suspend_thread();
         lockflag_thrmutex(mutex) ;
         bool isWakeup = (0 == self->wait.next);
         unlockflag_thrmutex(mutex) ;
         if (isWakeup) break ;
         // spurious resume
      }
   }

   return 0 ;
ONERR:
   unlockflag_thrmutex(mutex) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int unlock_thrmutex(thrmutex_t * mutex)
{
   int err ;
   thread_t * const self = self_thread() ;

   lockflag_thrmutex(mutex) ;

   if (self != mutex->lockholder) {
      err = EPERM ;
      goto ONERR;
   }

   thread_t * nextwait = 0 ;

   if (! isempty_thrmutexlist(cast_dlist(mutex))) {
      // resume waiting thread
      nextwait = removefirst_thrmutexlist(cast_dlist(mutex));
      resume_thread(nextwait) ;
   }

   mutex->lockholder = nextwait ;

   unlockflag_thrmutex(mutex) ;

   return 0 ;
ONERR:
   unlockflag_thrmutex(mutex) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   thrmutex_t mutex = thrmutex_FREE ;

   // TEST thrmutex_FREE
   TEST(0 == mutex.last) ;
   TEST(0 == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;

   // TEST thrmutex_INIT
   memset(&mutex, 255, sizeof(mutex)) ;
   mutex = (thrmutex_t) thrmutex_INIT ;
   TEST(0 == mutex.last) ;
   TEST(0 == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;

   // TEST init_thrmutex
   memset(&mutex, 255, sizeof(mutex)) ;
   init_thrmutex(&mutex) ;
   TEST(0 == mutex.last) ;
   TEST(0 == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;

   // TEST free_thrmutex
   TEST(0 == mutex.last) ;
   TEST(0 == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;
   TEST(0 == free_thrmutex(&mutex)) ;

   // TEST free_thrmutex: EBUSY
   TEST(0 == set_atomicflag(&mutex.lockflag)) ;
   TEST(EBUSY == free_thrmutex(&mutex)) ;
   TEST(0 != set_atomicflag(&mutex.lockflag)) ;
   clear_atomicflag(&mutex.lockflag) ;
   mutex.last = (void*) 1 ;
   TEST(EBUSY == free_thrmutex(&mutex)) ;
   mutex.last = 0 ;
   TEST(0 == free_thrmutex(&mutex)) ;
   mutex.lockholder = (void*) 1 ;
   TEST(EBUSY == free_thrmutex(&mutex)) ;
   mutex.lockholder = 0 ;
   TEST(0 == free_thrmutex(&mutex)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_query(void)
{
   thrmutex_t mutex = thrmutex_INIT ;

   // TEST isfree_thrmutex
   TEST(1 == isfree_thrmutex(&mutex)) ;
   mutex.last = (void*) 1 ;
   TEST(0 == isfree_thrmutex(&mutex)) ;
   mutex.last = 0 ;
   TEST(1 == isfree_thrmutex(&mutex)) ;
   mutex.lockholder = (void*) 1 ;
   TEST(0 == isfree_thrmutex(&mutex)) ;
   mutex.lockholder = 0 ;
   TEST(1 == isfree_thrmutex(&mutex)) ;
   set_atomicflag(&mutex.lockflag) ;
   TEST(0 == isfree_thrmutex(&mutex)) ;
   clear_atomicflag(&mutex.lockflag) ;
   TEST(1 == isfree_thrmutex(&mutex)) ;

   // TEST islocked_thrmutex
   TEST(0 == islocked_thrmutex(&mutex)) ;
   mutex.lockholder = self_thread() ;
   TEST(1 == islocked_thrmutex(&mutex)) ;
   mutex.lockholder = 0 ;
   TEST(0 == islocked_thrmutex(&mutex)) ;

   // TEST iswaiting_thrmutex
   TEST(0 == iswaiting_thrmutex(&mutex)) ;
   mutex.last = (void*) 1 ;
   TEST(1 == iswaiting_thrmutex(&mutex)) ;
   mutex.last = 0 ;
   TEST(0 == iswaiting_thrmutex(&mutex)) ;

   // TEST lockholder_thrmutex
   TEST(0 == lockholder_thrmutex(&mutex)) ;
   mutex.lockholder = (void*) 1 ;
   TEST(1 == (uintptr_t)lockholder_thrmutex(&mutex)) ;
   mutex.lockholder = self_thread() ;
   TEST(self_thread() == lockholder_thrmutex(&mutex)) ;
   mutex.lockholder = 0 ;
   TEST(0 == lockholder_thrmutex(&mutex)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static unsigned s_thread_runcount = 0 ;

static int thread_calllock(thrmutex_t * mutex)
{
   add_atomicint(&s_thread_runcount, 1) ;
   int err = lock_thrmutex(mutex) ;
   sub_atomicint(&s_thread_runcount, 1) ;
   if (err) CLEARBUFFER_ERRLOG() ;
   return err ;
}

static int thread_callunlock(thrmutex_t * mutex)
{
   add_atomicint(&s_thread_runcount, 1) ;
   int err = unlock_thrmutex(mutex) ;
   sub_atomicint(&s_thread_runcount, 1) ;
   if (err) CLEARBUFFER_ERRLOG() ;
   return err ;
}

static int test_synchronize(void)
{
   thrmutex_t  mutex       = thrmutex_INIT ;
   thread_t *  threads[10] = { 0 } ;

   // TEST lockflag_thrmutex
   TEST(0 == mutex.lockflag) ;
   lockflag_thrmutex(&mutex) ;
   TEST(0 != mutex.lockflag) ;

   // TEST unlockflag_thrmutex
   TEST(0 != mutex.lockflag) ;
   unlockflag_thrmutex(&mutex) ;
   TEST(0 == mutex.lockflag) ;

   // TEST lock_thrmutex
   TEST(0 == lock_thrmutex(&mutex)) ;
   TEST(0 == mutex.last) ;
   TEST(self_thread() == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;

   // TEST lock_thrmutex: EDEADLOCK
   TEST(EDEADLK == lock_thrmutex(&mutex)) ;
   TEST(0 == mutex.last) ;
   TEST(self_thread() == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;
   mutex.lockholder = 0 ;

   // TEST lock_thrmutex: active waiting on lockflag
   lockflag_thrmutex(&mutex) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_calllock, &mutex)) ;
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 3; ++i) {
      yield_thread() ;
      TEST(1 == read_atomicint(&s_thread_runcount)) ;
   }
   unlockflag_thrmutex(&mutex) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == mutex.last) ;
   TEST(threads[0] == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;
   TEST(0 == delete_thread(&threads[0])) ;
   mutex.lockholder = 0 ;

   // TEST lock_thrmutex: insert into internal wait list
   TEST(0 == lock_thrmutex(&mutex)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      uintptr_t oldlast = (uintptr_t) mutex.last ;
      TEST(0 == newgeneric_thread(&threads[i], &thread_calllock, &mutex)) ;
      while (oldlast == read_atomicint((uintptr_t*)&mutex.last)
             || 0 != read_atomicint(&mutex.lockflag)) {
         yield_thread() ;
      }
      TEST(i+1              == read_atomicint(&s_thread_runcount)) ;
      TEST(mutex.last       == cast2node_thrmutexlist(threads[i])) ;
      TEST(mutex.lockholder == self_thread()) ;
      TEST(mutex.lockflag   == 0) ;
      TEST(threads[i]->wait.next       == cast2node_thrmutexlist(threads[0]));
      TEST(threads[i-(i>0)]->wait.next == cast2node_thrmutexlist(threads[i]));
   }
   mutex.lockholder = 0 ;

   // TEST lock_thrmutex: wait until resume_thread && nextwait == 0
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      resume_thread(threads[i]) ;   // spurious wakeup
      for (int i2 = 0; i2 < 5; ++i2) {
         yield_thread() ;
         TEST(lengthof(threads)-i == read_atomicint(&s_thread_runcount)) ;
      }
      lockflag_thrmutex(&mutex) ;   // acquired during wakeup
      thread_t * firstthread = 0 ;
      TEST( !isempty_thrmutexlist(cast_dlist(&mutex)));
      firstthread = removefirst_thrmutexlist(cast_dlist(&mutex));
      TEST(threads[i]            == firstthread);
      TEST(threads[i]->wait.next == 0);
      TEST(threads[i]->wait.prev == 0);
      resume_thread(threads[i]) ;   // real wakeup
      for (int i2 = 0; i2 < 10; ++i2) {
         yield_thread() ;
         TEST(lengthof(threads)-i == read_atomicint(&s_thread_runcount)) ;
      }
      unlockflag_thrmutex(&mutex) ;
      TEST(0 == join_thread(threads[i])) ;
      TEST(0 == returncode_thread(threads[i])) ;
      TEST(0 == delete_thread(&threads[i])) ;
      TEST(lengthof(threads)-1-i == read_atomicint(&s_thread_runcount)) ;
      TEST(mutex.last            == (i+1 < lengthof(threads) ? cast2node_thrmutexlist(threads[lengthof(threads)-1]) : 0)) ;
      TEST(mutex.lockholder      == 0) ; // not set in lock_thrmutex
      TEST(mutex.lockflag        == 0) ;
   }

   // TEST unlock_thrmutex
   TEST(0 == lock_thrmutex(&mutex)) ;
   TEST(0 == unlock_thrmutex(&mutex)) ;
   TEST(0 == mutex.last) ;
   TEST(0 == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;

   // TEST unlock_thrmutex: active waiting on lockflag
   lockflag_thrmutex(&mutex) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_callunlock, &mutex)) ;
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 3; ++i) {
      yield_thread() ;
      TEST(1 == read_atomicint(&s_thread_runcount)) ;
   }
   write_atomicint((uintptr_t*)&mutex.lockholder, (uintptr_t)threads[0]) ;
   unlockflag_thrmutex(&mutex) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == mutex.last) ;
   TEST(0 == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;
   TEST(0 == delete_thread(&threads[0])) ;

   // TEST unlock_thrmutex: EPERM
   TEST(EPERM == unlock_thrmutex(&mutex)) ;
   TEST(0 == mutex.last) ;
   TEST(0 == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_calllock, &mutex)) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(threads[0] == (thread_t*)read_atomicint((uintptr_t*)&mutex.lockholder)) ;
   TEST(EPERM == unlock_thrmutex(&mutex)) ;
   TEST(0 == mutex.last) ;
   TEST(threads[0] == mutex.lockholder) ;
   TEST(0 == mutex.lockflag) ;
   TEST(0 == delete_thread(&threads[0])) ;
   mutex.lockholder = 0 ;

   // TEST unlock_thrmutex: sends resume and sets lockholder
   TEST(0 == lock_thrmutex(&mutex)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      uintptr_t oldlast = (uintptr_t) mutex.last ;
      TEST(0 == newgeneric_thread(&threads[i], &thread_calllock, &mutex)) ;
      while (oldlast == read_atomicint((uintptr_t*)&mutex.last)) {
         yield_thread() ;
      }
   }
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      write_atomicint((uintptr_t*)&mutex.lockholder, (uintptr_t)self_thread()) ;
      TEST(0 == unlock_thrmutex(&mutex)) ;
      TEST(mutex.last       == (i+1 < lengthof(threads) ? cast2node_thrmutexlist(threads[lengthof(threads)-1]) : 0)) ;
      TEST(mutex.lockholder == threads[i]) ;  // set in unlock_thrmutex
      TEST(0 == join_thread(threads[i])) ;
      TEST(0 == returncode_thread(threads[i])) ;
      TEST(lengthof(threads)-1-i == read_atomicint(&s_thread_runcount)) ;
      TEST(mutex.last       == (i+1 < lengthof(threads) ? cast2node_thrmutexlist(threads[lengthof(threads)-1]) : 0)) ;
      TEST(mutex.lockholder == threads[i]) ;  // set in unlock_thrmutex
      TEST(mutex.lockflag   == 0) ;
      TEST(0 == delete_thread(&threads[i])) ;
   }

   return 0 ;
ONERR:
   unlockflag_thrmutex(&mutex) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      write_atomicint((uintptr_t*)&mutex.lockholder, (uintptr_t)self_thread()) ;
      unlock_thrmutex(&mutex) ;
      delete_thread(&threads[i]) ;
   }
   return EINVAL ;
}

static int process_callslock(thrmutex_t * mutex)
{
   slock_thrmutex(mutex) ;
   return 0 ;
}

static int process_callsunlock(thrmutex_t * mutex)
{
   sunlock_thrmutex(mutex) ;
   return 0 ;
}

static int test_safesync(void)
{
   thrmutex_t        mutex   = thrmutex_INIT ;
   process_t         process = process_FREE ;
   process_result_t  result ;

   // TEST slock_thrmutex


   // TEST slock_thrmutex: ERROR
   init_thrmutex(&mutex) ;
   mutex.lockholder = self_thread() ;
   TEST(0 == initgeneric_process(&process, &process_callslock, &mutex, 0)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(result.returncode == SIGABRT)
   TEST(result.state      == process_state_ABORTED) ;
   TEST(0 == free_process(&process)) ;

   // TEST sunlock_thrmutex: ERROR
   init_thrmutex(&mutex) ;
   TEST(0 == initgeneric_process(&process, &process_callsunlock, &mutex, 0)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(result.returncode == SIGABRT)
   TEST(result.state      == process_state_ABORTED) ;
   TEST(0 == free_process(&process)) ;

   return 0 ;
ONERR:
   free_process(&process) ;
   return EINVAL ;
}

int unittest_platform_sync_thrmutex()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_synchronize())    goto ONERR;
   if (test_safesync())       goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
