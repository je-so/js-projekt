/* title: ReadWriteLock Linuximpl

   Implements <ReadWriteLock>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/task/thread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/process.h"
#endif


// section: rwlock_t

// group: helper

/* define: INTERFACE_rwlocklist
 * Use macro <slist_IMPLEMENT> to generate an adapted interface of <slist_t> to <thread_t>. */
slist_IMPLEMENT(_rwlocklist, thread_t, nextwait)

// group: lifetime

int free_rwlock(rwlock_t * rwlock)
{
   int err ;

   if (  read_atomicint((uintptr_t*)&rwlock->readers.last)
         // race beetwen query single data members
         // but after calling free lockXXX works without error
         // solution: introduce init_or_free flag which prevents calling lockXXX on rwlock
         || rwlock->writers.last
         || rwlock->writer
         || rwlock->nrofreader
         || rwlock->lockflag) {
      err = EBUSY ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

// group: query

uint32_t nrofreader_rwlock(rwlock_t * rwlock)
{
   return read_atomicint(&rwlock->nrofreader) ;
}

bool iswriter_rwlock(rwlock_t * rwlock)
{
   return 0 != read_atomicint((uintptr_t*)&rwlock->writer) ;
}

// group: synchronize

/* function: lockflag_rwlock
 * Wait until rwlock->lockflag is cleared and set it atomically.
 * Includes an acuire memory barrier. Calling thread can see what
 * was written by other threads before the flag was set. */
static inline void lockflag_rwlock(rwlock_t * rwlock)
{
   while (0 != set_atomicflag(&rwlock->lockflag)) {
      yield_thread() ;
   }
}

/* function: unlockflag_rwlock
 * Clear rwlock->lockflag.
 * Include a release memory barrier. All other threads can see what was written before
 * flag was cleared. */
static inline void unlockflag_rwlock(rwlock_t * rwlock)
{
   clear_atomicflag(&rwlock->lockflag) ;
}

/* function: wakeupreader_rwlock
 * Wakes up all readers waiting in rwlock->readers.
 *
 * (unchecked) Precondition:
 * o rwlock->lockflag is set.
 * o rwlock->readers not empty */
 static inline void wakeupreader_rwlock(rwlock_t * rwlock)
{
   thread_t * lastthread = last_rwlocklist(cast_slist(&rwlock->readers)) ;
   thread_t * nextthread = lastthread ;
   thread_t * firstthread ;
   do {
      firstthread = nextthread ;
      nextthread  = next_rwlocklist(nextthread) ;
      lockflag_thread(firstthread) ;
      firstthread->nextwait = 0 ;
      ++ rwlock->nrofreader ;
      resume_thread(firstthread) ;
      unlockflag_thread(firstthread) ;
   } while (nextthread != lastthread) ;

   init_rwlocklist(cast_slist(&rwlock->readers)) ;
}

/* function: wakeupwriter_rwlock
 * Wakes up the first writer stored in list rwlock->writers.
 *
 * (unchecked) Precondition:
 * o rwlock->lockflag is set.
 * o rwlock->writers not empty */
static inline void wakeupwriter_rwlock(rwlock_t * rwlock)
{
   thread_t * firstthread = first_rwlocklist(cast_slist(&rwlock->writers));
   lockflag_thread(firstthread);
   rwlock->writer = firstthread;
   firstthread = removefirst_rwlocklist(cast_slist(&rwlock->writers));
   resume_thread(firstthread);
   unlockflag_thread(firstthread);
}

/* function: insertandwait_rwlock
 * Adds thread to list of waiting threads in rwlock.
 * rwlock->lockflag is cleared and thread is suspended.
 * If the thread is resumed lockflag of thread is set and nextwait of thread is checked against 0
 * and then lockflag of thread is cleared. If nextwait is 0 the function returns else the thread suspends itself again.
 *
 * (unchecked) Precondition:
 * o rwlock->lockflag is set. */
static inline void insertandwait_rwlock(rwlock_t * rwlock, slist_t * waitlist, thread_t * self)
{
   insertlast_rwlocklist(waitlist, self) ;
   unlockflag_rwlock(rwlock) ;

   // waiting loop
   for (;;) {
      suspend_thread() ;
      lockflag_thread(self) ;
      bool isWakeup = (0 == self->nextwait) ;
      unlockflag_thread(self) ;
      if (isWakeup) break ;
      // spurious resume
   }
}

int lockreader_rwlock(rwlock_t * rwlock)
{
   int err ;
   thread_t * const self = self_thread() ;

   lockflag_rwlock(rwlock) ;

   if (  rwlock->writers.last
         || rwlock->writer) {
      if (self == rwlock->writer) {
         err = EDEADLK ;
         goto ONERR;
      }
      insertandwait_rwlock(rwlock, cast_slist(&rwlock->readers), self) ;

   } else {
      ++ rwlock->nrofreader ;
      bool isOverflow = (! rwlock->nrofreader) ;
      if (isOverflow) {
         -- rwlock->nrofreader ;
         err = EOVERFLOW ;
         goto ONERR;
      }
      unlockflag_rwlock(rwlock) ;
   }

   return 0 ;
ONERR:
   unlockflag_rwlock(rwlock) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int lockwriter_rwlock(rwlock_t * rwlock)
{
   int err ;
   thread_t * const self = self_thread() ;

   lockflag_rwlock(rwlock) ;

   if (  0 != rwlock->writer
         || 0 != rwlock->nrofreader) {
      if (self == rwlock->writer) {
         err = EDEADLK ;
         goto ONERR;
      }
      insertandwait_rwlock(rwlock, cast_slist(&rwlock->writers), self) ;

   } else {
      rwlock->writer = self ;
      unlockflag_rwlock(rwlock) ;
   }

   return 0 ;
ONERR:
   unlockflag_rwlock(rwlock) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int unlockreader_rwlock(rwlock_t * rwlock)
{
   int err ;

   lockflag_rwlock(rwlock) ;

   if (! rwlock->nrofreader) {
      err = EPERM ;
      goto ONERR;
   }

   -- rwlock->nrofreader ;

   if (0 == rwlock->nrofreader) {
      if (rwlock->writers.last) {
         wakeupwriter_rwlock(rwlock) ;
      } else if (rwlock->readers.last) {
         // should never enter this branch
         // readers are only inserted into readers if rwlock->writers.last != 0
         wakeupreader_rwlock(rwlock) ;
      }
   }

   unlockflag_rwlock(rwlock) ;

   return 0 ;
ONERR:
   unlockflag_rwlock(rwlock) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int unlockwriter_rwlock(rwlock_t * rwlock)
{
   int err ;
   thread_t * const self = self_thread() ;

   lockflag_rwlock(rwlock) ;

   if (rwlock->writer != self) {
      err = EPERM ;
      goto ONERR;
   }

   rwlock->writer = 0 ;

   if (rwlock->readers.last) {
      wakeupreader_rwlock(rwlock) ;
   } else if (rwlock->writers.last) {
      wakeupwriter_rwlock(rwlock) ;
   }

   unlockflag_rwlock(rwlock) ;

   return 0 ;
ONERR:
   unlockflag_rwlock(rwlock) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   rwlock_t rwlock = rwlock_FREE ;

   // TEST rwlock_FREE
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST rwlock_INIT
   rwlock = (rwlock_t) rwlock_INIT ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST init_rwlock
   memset(&rwlock, 255, sizeof(rwlock)) ;
   init_rwlock(&rwlock) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST free_rwlock
   TEST(0 == free_rwlock(&rwlock));
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST free_rwlock: EBUSY
   rwlock.readers.last = (void*)1 ;
   TEST(free_rwlock(&rwlock) == EBUSY);
   rwlock.readers.last = 0 ;
   TEST(free_rwlock(&rwlock) == 0);
   rwlock.writers.last = (void*)1 ;
   TEST(free_rwlock(&rwlock) == EBUSY);
   rwlock.writers.last = 0 ;
   TEST(free_rwlock(&rwlock) == 0);
   rwlock.writer = self_thread() ;
   TEST(free_rwlock(&rwlock) == EBUSY);
   rwlock.writer = 0 ;
   TEST(free_rwlock(&rwlock) == 0);
   rwlock.nrofreader = 1 ;
   TEST(free_rwlock(&rwlock) == EBUSY);
   rwlock.nrofreader = 0 ;
   TEST(free_rwlock(&rwlock) == 0);
   rwlock.lockflag = 1 ;
   TEST(free_rwlock(&rwlock) == EBUSY);
   rwlock.lockflag = 0 ;
   TEST(free_rwlock(&rwlock) == 0);

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_query(void)
{
   rwlock_t rwlock = rwlock_FREE ;

   // TEST nrofreader_rwlock
   for (uint32_t i = 1; i; i <<= 1) {
      rwlock.nrofreader = i ;
      TEST(i == nrofreader_rwlock(&rwlock)) ;
   }
   rwlock.nrofreader = 0 ;
   TEST(0 == nrofreader_rwlock(&rwlock)) ;

   // TEST iswriter_rwlock
   for (uintptr_t i = 1; i; i <<= 1) {
      rwlock.writer = (void*) i ;
      TEST(1 == iswriter_rwlock(&rwlock)) ;
   }
   rwlock.writer = 0 ;
   TEST(0 == iswriter_rwlock(&rwlock)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static unsigned s_thread_runcount = 0 ;

static int thread_lockreader(rwlock_t * rwlock)
{
   add_atomicint(&s_thread_runcount, 1) ;
   int err = lockreader_rwlock(rwlock) ;
   sub_atomicint(&s_thread_runcount, 1) ;
   if (err) CLEARBUFFER_ERRLOG() ;
   return err ;
}

static int thread_unlockreader(rwlock_t * rwlock)
{
   add_atomicint(&s_thread_runcount, 1) ;
   int err = unlockreader_rwlock(rwlock) ;
   sub_atomicint(&s_thread_runcount, 1) ;
   if (err) CLEARBUFFER_ERRLOG() ;
   return err ;
}

static int thread_lockwriter(rwlock_t * rwlock)
{
   add_atomicint(&s_thread_runcount, 1) ;
   int err = lockwriter_rwlock(rwlock) ;
   sub_atomicint(&s_thread_runcount, 1) ;
   if (err) CLEARBUFFER_ERRLOG() ;
   return err ;
}

static int thread_unlockwriter(rwlock_t * rwlock)
{
   add_atomicint(&s_thread_runcount, 1) ;
   int err = unlockwriter_rwlock(rwlock) ;
   sub_atomicint(&s_thread_runcount, 1) ;
   if (err) CLEARBUFFER_ERRLOG() ;
   return err ;
}

static int test_synchronize(void)
{
   rwlock_t    rwlock     = rwlock_FREE ;
   thread_t *  threads[5] = { 0 } ;

   // prepare
   init_rwlock(&rwlock) ;

   // TEST lockreader_rwlock
   rwlock.nrofreader = 1 ; //
   TEST(0 == lockreader_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(2 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.nrofreader = 0 ;

   // TEST lockreader_rwlock: EDEADLK
   rwlock.writer = self_thread() ;
   TEST(EDEADLK == lockreader_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(self_thread() == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.writer = 0 ;

   // TEST lockreader_rwlock: EOVERFLOW
   rwlock.nrofreader = UINT32_MAX ;
   TEST(EOVERFLOW == lockreader_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   ++ rwlock.nrofreader ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST lockreader_rwlock: active waiting on lockflag
   lockflag_rwlock(&rwlock) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_lockreader, &rwlock)) ;
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 3; ++i) {
      yield_thread() ;
      TEST(1 == read_atomicint(&s_thread_runcount)) ;
   }
   unlockflag_rwlock(&rwlock) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   TEST(0 == delete_thread(&threads[0])) ;
   rwlock.nrofreader = 0 ;

   for (int waitreason = 0; waitreason < 2; ++waitreason) {

      // TEST lockreader_rwlock: insert into wait list
      rwlock.writer       = waitreason == 0 ? self_thread() : 0 ;
      rwlock.writers.last = waitreason == 1 ? (void*)1      : 0 ;
      for (unsigned i = 0; i < lengthof(threads); ++i) {
         uintptr_t oldlast = (uintptr_t) rwlock.readers.last ;
         TEST(0 == newgeneric_thread(&threads[i], &thread_lockreader, &rwlock)) ;
         while (oldlast == read_atomicint((uintptr_t*)&rwlock.readers.last)
                || 0 != read_atomicint(&rwlock.lockflag)) {
            yield_thread() ;
         }
         TEST(i+1                 == read_atomicint(&s_thread_runcount)) ;
         TEST(rwlock.readers.last == cast2node_rwlocklist(threads[i])) ;
         TEST(rwlock.writers.last == (waitreason == 1 ? (void*)1      : 0)) ;
         TEST(rwlock.writer       == (waitreason == 0 ? self_thread() : 0)) ;
         TEST(rwlock.nrofreader   == 0) ;
         TEST(rwlock.lockflag     == 0) ;
         TEST(threads[i]->nextwait       == cast2node_rwlocklist(threads[0])) ;
         TEST(threads[i-(i>0)]->nextwait == cast2node_rwlocklist(threads[i])) ;
      }
      rwlock.writer       = 0 ;
      rwlock.writers.last = 0 ;

      // TEST lockreader_rwlock: wait until resume_thread && nextwait == 0
      for (unsigned i = 0; i < lengthof(threads); ++i) {
         resume_thread(threads[i]) ;   // spurious wakeup
         for (int i2 = 0; i2 < 10; ++i2) {
            yield_thread() ;
            TEST(lengthof(threads)-i == read_atomicint(&s_thread_runcount)) ;
         }
         lockflag_thread(threads[i]) ; // flag is acquired in wakeup
         TEST( !isempty_rwlocklist(cast_slist(&rwlock.readers)));
         thread_t* firstthread = removefirst_rwlocklist(cast_slist(&rwlock.readers));
         TEST(threads[i]           == firstthread);
         TEST(threads[i]->nextwait == 0);
         add_atomicint(&rwlock.nrofreader, 1) ; // wakeup increments nrofreader
         resume_thread(threads[i]) ;            // real wakeup
         for (int i2 = 0; i2 < 10; ++i2) {
            yield_thread() ;
            TEST(lengthof(threads)-i == read_atomicint(&s_thread_runcount)) ;
         }
         unlockflag_thread(threads[i]) ;
         TEST(0 == join_thread(threads[i])) ;
         TEST(0 == returncode_thread(threads[i])) ;
         TEST(0 == delete_thread(&threads[i])) ;
         TEST(lengthof(threads)-1-i == read_atomicint(&s_thread_runcount)) ;
         TEST(rwlock.readers.last   == (i+1 < lengthof(threads) ? cast2node_rwlocklist(threads[lengthof(threads)-1]) : 0)) ;
         TEST(rwlock.writers.last   == 0) ;
         TEST(rwlock.writer         == 0) ;
         TEST(rwlock.nrofreader     == 1 + i) ;
         TEST(rwlock.lockflag       == 0) ;
      }
      rwlock.nrofreader = 0 ;

   }  // for waitreason

   // TEST lockwriter_rwlock
   TEST(0 == lockwriter_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(self_thread() == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST lockwriter_rwlock: EDEADLK
   TEST(EDEADLK == lockwriter_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(self_thread() == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.writer = 0 ;

   // TEST lockwriter_rwlock: active waiting on lockflag
   lockflag_rwlock(&rwlock) ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_lockwriter, &rwlock)) ;
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 3; ++i) {
      yield_thread() ;
      TEST(1 == read_atomicint(&s_thread_runcount)) ;
   }
   unlockflag_rwlock(&rwlock) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(threads[0] == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   TEST(0 == delete_thread(&threads[0])) ;
   rwlock.writer = 0 ;

   for (int waitreason = 0; waitreason < 2; ++waitreason) {

      // TEST lockwriter_rwlock: insert into wait list
      rwlock.writer     = waitreason == 0 ? self_thread() : 0 ;
      rwlock.nrofreader = waitreason == 1 ? 1             : 0 ;
      for (unsigned i = 0; i < lengthof(threads); ++i) {
         uintptr_t oldlast = (uintptr_t) rwlock.writers.last ;
         TEST(0 == newgeneric_thread(&threads[i], &thread_lockwriter, &rwlock)) ;
         while (oldlast == read_atomicint((uintptr_t*)&rwlock.writers.last)
                || 0 != read_atomicint(&rwlock.lockflag)) {
            yield_thread() ;
         }
         TEST(i+1                 == read_atomicint(&s_thread_runcount)) ;
         TEST(rwlock.readers.last == 0) ;
         TEST(rwlock.writers.last == cast2node_rwlocklist(threads[i])) ;
         TEST(rwlock.writer       == (waitreason == 0 ? self_thread() : 0)) ;
         TEST(rwlock.nrofreader   == (waitreason == 1 ? 1             : 0)) ;
         TEST(rwlock.lockflag     == 0) ;
         TEST(threads[i]->nextwait       == cast2node_rwlocklist(threads[0])) ;
         TEST(threads[i-(i>0)]->nextwait == cast2node_rwlocklist(threads[i])) ;
      }
      rwlock.writer     = 0 ;
      rwlock.nrofreader = 0 ;

      // TEST lockwriter_rwlock: wait until resume_thread && nextwait == 0
      for (unsigned i = 0; i < lengthof(threads); ++i) {
         resume_thread(threads[i]) ;   // spurious wakeup
         for (int i2 = 0; i2 < 10; ++i2) {
            yield_thread() ;
            TEST(lengthof(threads)-i == read_atomicint(&s_thread_runcount)) ;
         }
         lockflag_thread(threads[i]) ; // flag is acquired in wakeup
         TEST( !isempty_rwlocklist(cast_slist(&rwlock.writers)));
         thread_t* firstthread = removefirst_rwlocklist(cast_slist(&rwlock.writers));
         TEST(threads[i]           == firstthread) ;
         TEST(threads[i]->nextwait == 0) ;
         resume_thread(threads[i]) ;   // real wakeup
         for (int i2 = 0; i2 < 10; ++i2) {
            yield_thread() ;
            TEST(lengthof(threads)-i == read_atomicint(&s_thread_runcount)) ;
         }
         unlockflag_thread(threads[i]) ;
         TEST(0 == join_thread(threads[i])) ;
         TEST(0 == returncode_thread(threads[i])) ;
         TEST(lengthof(threads)-1-i == read_atomicint(&s_thread_runcount)) ;
         TEST(rwlock.readers.last   == 0) ;
         TEST(rwlock.writers.last   == (i+1 < lengthof(threads) ? cast2node_rwlocklist(threads[lengthof(threads)-1]) : 0)) ;
         TEST(rwlock.writer         == 0) ;  // set in unlockwriter during resume
         TEST(rwlock.nrofreader     == 0) ;
         TEST(rwlock.lockflag       == 0) ;
         TEST(0 == delete_thread(&threads[i])) ;
      }

   }  // for waitreason

   // TEST unlockreader_rwlock
   rwlock.nrofreader = 3 ;
   for (unsigned i = 3; (i--) > 0; ) {
      TEST(0 == unlockreader_rwlock(&rwlock)) ;
      TEST(0 == rwlock.readers.last) ;
      TEST(0 == rwlock.writers.last) ;
      TEST(0 == rwlock.writer) ;
      TEST(i == rwlock.nrofreader) ;
      TEST(0 == rwlock.lockflag) ;
   }

   // TEST unlockreader_rwlock: EPERM
   TEST(EPERM == unlockreader_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST unlockreader_rwlock: active waiting on lockflag of rwlock
   lockflag_rwlock(&rwlock) ;    // flag is acquired in unlockreader
   rwlock.nrofreader = 1 ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_unlockreader, &rwlock)) ;
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 10; ++i) {
      yield_thread() ;
      TEST(1 == read_atomicint(&s_thread_runcount)) ;
   }
   unlockflag_rwlock(&rwlock) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST unlockreader_rwlock: active waiting on lockflag of thread
   insertlast_rwlocklist(cast_slist(&rwlock.readers), self_thread()) ;
   rwlock.nrofreader = 1 ;
   lockflag_thread(self_thread()) ; // flag is acquired in unlockreader
   trysuspend_thread() ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_unlockreader, &rwlock)) ;
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 10; ++i) {
      yield_thread() ;
      TEST(1 == read_atomicint(&s_thread_runcount)) ;
   }
   unlockflag_thread(self_thread()) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == trysuspend_thread()) ;
   TEST(0 == self_thread()->nextwait) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.nrofreader = 0 ;

   // TEST unlockreader_rwlock: sends resume to waiting writer before reader
   TEST(0 == lockwriter_rwlock(&rwlock)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      uintptr_t oldlast = (uintptr_t) rwlock.readers.last ;
      TEST(0 == newgeneric_thread(&threads[i], i == 0 ? &thread_lockwriter : &thread_lockreader, &rwlock)) ;
      if (i == 0) {
         while (0 == read_atomicint((uintptr_t*)&rwlock.writers.last)) {
            yield_thread() ;
         }
      } else {
         while (oldlast == read_atomicint((uintptr_t*)&rwlock.readers.last)) {
            yield_thread() ;
         }
     }
   }
   TEST(0 == nrofreader_rwlock(&rwlock)) ;
   rwlock.writer     = 0 ;
   rwlock.nrofreader = 1 ;
   TEST(0 == unlockreader_rwlock(&rwlock)) ;
   TEST(0 != rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(threads[0] == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   rwlock.writer = 0 ;

   // TEST unlockreader_rwlock: sends resume to waiting reader
   rwlock.nrofreader = 1 ;
   TEST(0 == unlockreader_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(lengthof(threads)-1 == rwlock.nrofreader) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      TEST(0 == join_thread(threads[i])) ;
      TEST(0 == returncode_thread(threads[i])) ;
      TEST(0 == delete_thread(&threads[i])) ;
   }
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(lengthof(threads)-1 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.nrofreader = 0 ;

   // TEST unlockwriter_rwlock
   TEST(0 == lockwriter_rwlock(&rwlock)) ;
   TEST(0 == unlockwriter_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST unlockwriter_rwlock: EPERM
   TEST(EPERM == unlockwriter_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.writer = (void*) 1 ;
   TEST(EPERM == unlockwriter_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(1 == (uintptr_t)rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.writer = 0 ;

   // TEST unlockwriter_rwlock: active waiting on lockflag of rwlock
   lockflag_rwlock(&rwlock) ;    // flag is acquired in unlockwriter
   TEST(0 == newgeneric_thread(&threads[0], &thread_unlockwriter, &rwlock)) ;
   write_atomicint((uintptr_t*)&rwlock.writer, (uintptr_t)threads[0]) ;
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 10; ++i) {
      yield_thread() ;
      TEST(1 == read_atomicint(&s_thread_runcount)) ;
   }
   unlockflag_rwlock(&rwlock) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST unlockwriter_rwlock: active waiting on lockflag of thread
   lockflag_thread(self_thread()) ; // flag is acquired in unlockreader
   insertlast_rwlocklist(cast_slist(&rwlock.writers), self_thread()) ;
   trysuspend_thread() ;
   lockflag_rwlock(&rwlock) ;       // flag is acquired in unlockwriter
   TEST(0 == newgeneric_thread(&threads[0], &thread_unlockwriter, &rwlock)) ;
   write_atomicint((uintptr_t*)&rwlock.writer, (uintptr_t)threads[0]) ;
   unlockflag_rwlock(&rwlock) ;     // now let unlockwriter run
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread() ;
   }
   for (int i = 0; i < 10; ++i) {
      yield_thread() ;
      TEST(1 == read_atomicint(&s_thread_runcount)) ;
   }
   unlockflag_thread(self_thread()) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == read_atomicint(&s_thread_runcount)) ;
   TEST(0 == trysuspend_thread()) ;
   TEST(0 == self_thread()->nextwait) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(self_thread() == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.writer = 0 ;

   // TEST unlockwriter_rwlock: sends resume to waiting reader before writer
   TEST(0 == lockwriter_rwlock(&rwlock)) ;
   for (unsigned i = 0; i < lengthof(threads); ++i) {
      uintptr_t oldlast = (uintptr_t) rwlock.writers.last ;
      TEST(0 == newgeneric_thread(&threads[i], i == 0 ? &thread_lockreader : &thread_lockwriter, &rwlock)) ;
      if (i == 0) {
         while (0 == read_atomicint((uintptr_t*)&rwlock.readers.last)) {
            yield_thread() ;
         }
      } else {
         while (oldlast == read_atomicint((uintptr_t*)&rwlock.writers.last)) {
            yield_thread() ;
         }
     }
   }
   TEST(0 == nrofreader_rwlock(&rwlock)) ;
   TEST(0 == unlockwriter_rwlock(&rwlock)) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 != rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == join_thread(threads[0])) ;
   TEST(0 == returncode_thread(threads[0])) ;
   TEST(0 == delete_thread(&threads[0])) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 != rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;
   rwlock.nrofreader = 0 ;

   // TEST unlockwriter_rwlock: sends resume to waiting reader
   for (unsigned i = 1; i < lengthof(threads); ++i) {
      rwlock.writer = self_thread() ;
      TEST(0 == unlockwriter_rwlock(&rwlock)) ;
      TEST(rwlock.readers.last   == 0) ;
      TEST(rwlock.writers.last   == (i+1 < lengthof(threads) ? cast2node_rwlocklist(threads[lengthof(threads)-1]) : 0)) ;
      TEST(rwlock.writer         == threads[i]) ;
      TEST(rwlock.nrofreader     == 0) ;
      TEST(0 == join_thread(threads[i])) ;
      TEST(0 == returncode_thread(threads[i])) ;
      TEST(lengthof(threads)-1-i == read_atomicint(&s_thread_runcount)) ;
      TEST(rwlock.readers.last   == 0) ;
      TEST(rwlock.writers.last   == (i+1 < lengthof(threads) ? cast2node_rwlocklist(threads[lengthof(threads)-1]) : 0)) ;
      TEST(rwlock.writer         == threads[i]) ;
      TEST(rwlock.nrofreader     == 0) ;
      TEST(rwlock.lockflag       == 0) ;
      TEST(0 == delete_thread(&threads[i])) ;
   }
   rwlock.writer = 0 ;

   // unprepare
   TEST(0 == free_rwlock(&rwlock)) ;

   return 0 ;
ONERR:
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
   rwlock->nrofreader = 0 ;            // forces EPERM
   sunlockreader_rwlock(rwlock) ;
   return 0 ;
}

static int process_slockwriter(rwlock_t * rwlock)
{
   rwlock->writer = self_thread() ;    // forces EDEADLK
   slockwriter_rwlock(rwlock) ;
   return 0 ;
}

static int process_sunlockwriter(rwlock_t * rwlock)
{
   rwlock->writer = 0 ;                // forces EPERM
   sunlockwriter_rwlock(rwlock) ;
   return 0 ;
}

static int test_safesync(void)
{
   rwlock_t          rwlock = rwlock_INIT ;
   process_t         child  = process_FREE ;
   process_result_t  result ;

   // TEST slockreader_rwlock
   slockreader_rwlock(&rwlock) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(1 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST sunlockreader_rwlock
   sunlockreader_rwlock(&rwlock) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST slockwriter_rwlock
   slockwriter_rwlock(&rwlock) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(self_thread() == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

   // TEST sunlockwriter_rwlock
   sunlockwriter_rwlock(&rwlock) ;
   TEST(0 == rwlock.readers.last) ;
   TEST(0 == rwlock.writers.last) ;
   TEST(0 == rwlock.writer) ;
   TEST(0 == rwlock.nrofreader) ;
   TEST(0 == rwlock.lockflag) ;

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
ONERR:
   free_process(&child) ;
   free_rwlock(&rwlock) ;
   return EINVAL ;
}

int unittest_platform_sync_rwlock()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_synchronize()) goto ONERR;
   if (test_safesync())    goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
