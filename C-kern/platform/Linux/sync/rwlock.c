/* title: ReadWriteLock Linuximpl

   Implements <ReadWriteLock>.

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

   file: C-kern/api/platform/sync/rwlock.h
    Header file <ReadWriteLock>.

   file: C-kern/platform/Linux/sync/rwlock.c
    Implementation file <ReadWriteLock Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/sync/rwlock.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/atomic.h"
#include "C-kern/api/platform/task/thread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/task/process.h"
#endif


// section: rwlock_t

// group: lifetime

int init_rwlock(/*out*/rwlock_t * rwlock)
{
   int err ;

   err = init_mutex(&rwlock->entrylock) ;
   if (err) goto ONABORT ;
   err = init_mutex(&rwlock->exitlock) ;
   if (err) goto ONABORT_FREE ;

   rwlock->nrofreader = 0 ;
   rwlock->writer     = 0 ;

   return 0 ;
ONABORT_FREE:
   free_mutex(&rwlock->entrylock) ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_rwlock(rwlock_t * rwlock)
{
   int err ;
   int err2 ;

   err = free_mutex(&rwlock->entrylock) ;
   err2 = free_mutex(&rwlock->exitlock) ;
   if (err2) err = err2 ;

   rwlock->nrofreader = 0 ;
   rwlock->writer     = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

uint32_t nrofreader_rwlock(rwlock_t * rwlock)
{
   return atomicread_int(&rwlock->nrofreader) ;
}

bool iswriter_rwlock(rwlock_t * rwlock)
{
   return 0 != atomicread_int((uintptr_t*)&rwlock->writer) ;
}

// group: update

int lockreader_rwlock(rwlock_t * rwlock)
{
   int err ;

   err = lock_mutex(&rwlock->entrylock) ;
   if (err) goto ONABORT ;
   ++ rwlock->nrofreader ;
   bool isOverflow = (! rwlock->nrofreader) ;
   if (isOverflow) {
      -- rwlock->nrofreader ;
   }
   err = unlock_mutex(&rwlock->entrylock) ;
   if (err) goto ONABORT ;

   if (isOverflow) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int lockwriter_rwlock(rwlock_t * rwlock)
{
   int err ;

   err = lock_mutex(&rwlock->entrylock) ;
   if (err) goto ONABORT ;

   for (;;) {
      err = lock_mutex(&rwlock->exitlock) ;
      if (err) goto ONABORT_UNLOCK ;

      bool isSuspend = (0 != rwlock->nrofreader) ;
      rwlock->writer = self_thread() ;

      err = unlock_mutex(&rwlock->exitlock) ;
      if (err) goto ONABORT_UNLOCK ;

      if (!isSuspend) break ;
      suspend_thread() ;
   }

   return 0 ;
ONABORT_UNLOCK:
   rwlock->writer = 0 ;
   unlock_mutex(&rwlock->entrylock) ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int unlockreader_rwlock(rwlock_t * rwlock)
{
   int err ;

   err = lock_mutex(&rwlock->exitlock) ;
   if (err) goto ONABORT ;

   -- rwlock->nrofreader ;
   bool isResume = ( 0 == rwlock->nrofreader
                     && rwlock->writer) ;

   if (isResume) {
      resume_thread(rwlock->writer) ;
   }

   err = unlock_mutex(&rwlock->exitlock) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int unlockwriter_rwlock(rwlock_t * rwlock)
{
   int err ;

   err = unlock_mutex(&rwlock->entrylock) ;
   if (err) goto ONABORT ;

   rwlock->writer = 0 ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   rwlock_t rwlock = rwlock_INIT_FREEABLE ;

   // TEST rwlock_INIT_FREEABLE
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST init_rwlock
   TEST(0 == init_rwlock(&rwlock));
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST free_rwlock
   rwlock.nrofreader = 1 ;                // not checked
   rwlock.writer     = self_thread() ;    // not checked
   TEST(0 == free_rwlock(&rwlock));
   TEST(EINVAL == lock_mutex(&rwlock.entrylock)) ;
   TEST(EINVAL == lock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int thread_lockunlockwrite(rwlock_t * rwlock)
{
   int err = lockwriter_rwlock(rwlock) ;
   if (!err) {
      err = unlockwriter_rwlock(rwlock) ;
   }
   if (err) CLEARBUFFER_LOG() ;
   return err ;
}

static int test_lock(void)
{
   rwlock_t    rwlock     = rwlock_INIT_FREEABLE ;
   thread_t *  threads[2] = { 0 } ;

   // prepare
   TEST(0 == init_rwlock(&rwlock)) ;

   // TEST lockreader_rwlock
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;    // will not be acquired
   TEST(0 == lockreader_rwlock(&rwlock)) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;   // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST lockreader_rwlock: entrylock is acquired
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;   // will be acquired and released
   TEST(EDEADLK == lockreader_rwlock(&rwlock)) ;
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;    // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST lockreader_rwlock: EOVERFLOW
   rwlock.nrofreader = UINT32_MAX ;
   TEST(EOVERFLOW == lockreader_rwlock(&rwlock)) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;   // released
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;    // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == (uint32_t)(1 + rwlock.nrofreader)) ;
   TEST(0 == rwlock.writer) ;

   // TEST unlockreader_rwlock
   rwlock.nrofreader = 3 ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;   // will not be acquired
   TEST(0 == unlockreader_rwlock(&rwlock)) ;
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;    // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(2 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST unlockreader_rwlock: exitlock is acquired
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;    // will be acquired and released
   TEST(EDEADLK == unlockreader_rwlock(&rwlock)) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;   // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(2 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST unlockreader_rwlock: resume waiter
   while (0 == trysuspend_thread()) {
   }
   rwlock.writer     = self_thread() ;
   rwlock.nrofreader = 2 ;
   // not resumed
   TEST(0 == unlockreader_rwlock(&rwlock)) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(self_thread() == rwlock.writer) ;
   TEST(EAGAIN == trysuspend_thread()) ;
   // resumed
   TEST(0 == unlockreader_rwlock(&rwlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(self_thread() == rwlock.writer) ;
   TEST(0 == trysuspend_thread()) ;
   rwlock.writer = 0 ;

   // TEST lockwriter_rwlock
   TEST(0 == lockwriter_rwlock(&rwlock)) ;
   TEST(EDEADLK == lock_mutex(&rwlock.entrylock)) ;   // acquired
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;          // released
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(self_thread() == rwlock.writer) ;

   // TEST lockwriter_rwlock: entrylock error
   rwlock.writer = 0 ;
   TEST(EDEADLK == lockwriter_rwlock(&rwlock)) ;       // entrylock already acquired
   TEST(EDEADLK == lock_mutex(&rwlock.entrylock)) ;   // not released
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;          // released
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST lockwriter_rwlock: exitlock is acquired
   rwlock.writer = 0 ;
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;    // will be acquired and released
   TEST(EDEADLK == lockwriter_rwlock(&rwlock)) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;   // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST lockwriter_rwlock: suspend, wakeup acquires exitlock
   TEST(0 == lockreader_rwlock(&rwlock)) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_lockunlockwrite, &rwlock)) ;
   while (0 == atomicread_int((uintptr_t*)&rwlock.writer)) {
      yield_thread() ;
   }
   yield_thread() ;
   TEST(threads[0] == (void*)atomicread_int((uintptr_t*)&rwlock.writer)) ;
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;          // released
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(EPERM == unlock_mutex(&rwlock.entrylock)) ;   // hold
   TEST(0 == free_mutex(&rwlock.exitlock)) ;          // generates error
   rwlock.nrofreader = 0 ;
   resume_thread(threads[0]) ;
   for (unsigned i = 0; i < 1000 && 0 != atomicread_int((uintptr_t*)&rwlock.writer); ++i) {
      yield_thread() ;
   }
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == (void*)atomicread_int((uintptr_t*)&rwlock.writer)) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;      // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(EINVAL == returncode_thread(threads[0]))
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == init_mutex(&rwlock.exitlock)) ;

   // TEST unlockwriter_rwlock
   TEST(0 == lockwriter_rwlock(&rwlock)) ;
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;       // will not be acquired
   TEST(0 == unlockwriter_rwlock(&rwlock)) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;      // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST unlockwriter_rwlock: ERROR
   TEST(0 == lockwriter_rwlock(&rwlock)) ;
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;    // released
   TEST(EPERM == unlockwriter_rwlock(&rwlock)) ;   // ==> error
   TEST(self_thread() == rwlock.writer) ;          // writer keeps set !
   rwlock.writer = 0 ;

   // TEST lockwriter_rwlock: suspend, resume
   TEST(0 == lockreader_rwlock(&rwlock)) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_lockunlockwrite, &rwlock)) ;
   while (0 == atomicread_int((uintptr_t*)&rwlock.writer)) {
      yield_thread() ;
   }
   yield_thread() ;
   TEST(threads[0] == (void*)atomicread_int((uintptr_t*)&rwlock.writer)) ;
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;          // released
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(EPERM == unlock_mutex(&rwlock.entrylock)) ;   // hold
   for (unsigned i = 0; i < 10; ++i) {
      resume_thread(threads[0]) ;
      yield_thread() ;
   }
   // not woken up
   TEST(threads[0] == (void*)atomicread_int((uintptr_t*)&rwlock.writer)) ;
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;          // released
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(EPERM == unlock_mutex(&rwlock.entrylock)) ;   // hold
   TEST(0 == unlockreader_rwlock(&rwlock)) ;
   for (unsigned i = 0; i < 1000 && 0 != atomicread_int((uintptr_t*)&rwlock.writer); ++i) {
      yield_thread() ;
   }
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == (void*)atomicread_int((uintptr_t*)&rwlock.writer)) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;      // released
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;       // released
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlockreader_rwlock(&rwlock)) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0]))
   TEST(0 == delete_thread(&threads[0])) ;

   // unprepare
   TEST(0 == free_rwlock(&rwlock)) ;

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      delete_thread(&threads[i]) ;
   }
   free_rwlock(&rwlock) ;
   return EINVAL ;
}

static int process_slockreader(rwlock_t * rwlock)
{
   rwlock->nrofreader = UINT32_MAX ;   // forces EOVERFLOW
   slockreader_rwlock(rwlock) ;
   return 0 ;
}

static int process_sunlockreader(rwlock_t * rwlock)
{
   lock_mutex(&rwlock->exitlock) ;  // forces EDEADLK
   sunlockreader_rwlock(rwlock) ;
   return 0 ;
}

static int process_slockwriter(rwlock_t * rwlock)
{
   lock_mutex(&rwlock->entrylock) ; // forces EDEADLK
   slockwriter_rwlock(rwlock) ;
   return 0 ;
}

static int process_sunlockwriter(rwlock_t * rwlock)
{
   unlock_mutex(&rwlock->entrylock) ;  // forces EPERM
   sunlockwriter_rwlock(rwlock) ;
   return 0 ;
}

static int test_safelock(void)
{
   rwlock_t          rwlock = rwlock_INIT_FREEABLE ;
   process_t         child  = process_INIT_FREEABLE ;
   process_result_t  result ;

   // prepare
   TEST(0 == init_rwlock(&rwlock)) ;

   // TEST slockreader_rwlock
   slockreader_rwlock(&rwlock) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;   // no locks hold
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;    // no locks hold
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST sunlockreader_rwlock
   sunlockreader_rwlock(&rwlock) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;   // no locks hold
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;    // no locks hold
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST slockwriter_rwlock
   slockwriter_rwlock(&rwlock) ;
   TEST(EDEADLK == lock_mutex(&rwlock.entrylock)) ;   // hold
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;          // released
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(self_thread() == rwlock.writer) ;

   // TEST sunlockwriter_rwlock
   sunlockwriter_rwlock(&rwlock) ;
   TEST(0 == lock_mutex(&rwlock.entrylock)) ;      // released
   TEST(0 == lock_mutex(&rwlock.exitlock)) ;       // released
   TEST(0 == unlock_mutex(&rwlock.entrylock)) ;
   TEST(0 == unlock_mutex(&rwlock.exitlock)) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.writer) ;

   // TEST slockreader_rwlock: ERROR
   TEST(0 == initgeneric_process(&child, process_slockreader, &rwlock, 0)) ;
   TEST(0 == wait_process(&child, &result)) ;
   TEST(result.state      == process_state_ABORTED) ;
   TEST(result.returncode == SIGABRT)
   TEST(0 == free_process(&child)) ;

   // TEST sunlockreader_rwlock: ERROR
   TEST(0 == initgeneric_process(&child, process_sunlockreader, &rwlock, 0)) ;
   TEST(0 == wait_process(&child, &result)) ;
   TEST(result.state      == process_state_ABORTED) ;
   TEST(result.returncode == SIGABRT)
   TEST(0 == free_process(&child)) ;

   // TEST slockwriter_rwlock: ERROR
   TEST(0 == initgeneric_process(&child, process_slockwriter, &rwlock, 0)) ;
   TEST(0 == wait_process(&child, &result)) ;
   TEST(result.state      == process_state_ABORTED) ;
   TEST(result.returncode == SIGABRT)
   TEST(0 == free_process(&child)) ;

   // TEST sunlockwriter_rwlock: ERROR
   TEST(0 == initgeneric_process(&child, process_sunlockwriter, &rwlock, 0)) ;
   TEST(0 == wait_process(&child, &result)) ;
   TEST(result.state      == process_state_ABORTED) ;
   TEST(result.returncode == SIGABRT)
   TEST(0 == free_process(&child)) ;

   // unprepare
   TEST(0 == free_rwlock(&rwlock)) ;

   return 0 ;
ONABORT:
   free_process(&child) ;
   free_rwlock(&rwlock) ;
   return EINVAL ;
}

int unittest_platform_sync_rwlock()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_lock())        goto ONABORT ;
   if (test_safelock())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
