/* title: EventCounter impl

   Implements <EventCounter>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2017 Jörg Seebohn

   file: C-kern/api/platform/sync/eventcount.h
    Header file <EventCounter>.

   file: C-kern/platform/Linux/sync/eventcount.c
    Implementation file <EventCounter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/sync/eventcount.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/process.h"
#endif


// section: eventcount_t

// group: helper

/* define: INTERFACE_threadlist
 * Use macro <slist_IMPLEMENT> to generate an adapted interface of <slist_t> to <thread_t>. */
slist_IMPLEMENT(_threadlist, thread_t, nextwait)

// group: synchronize

/* function: lock_counter
 * Wait until counter->lockflag is cleared and set it atomically.
 * Includes an acquire memory barrier. Calling thread can see what
 * was written by other threads before the flag was set. */
static inline void lock_counter(eventcount_t* counter)
{
   while (0 != set_atomicflag(&counter->lockflag)) {
      yield_thread();
   }
}

/* function: unlock_counter
 * Clear counter->lockflag.
 * Include a release memory barrier. All other threads can see what was written before
 * flag was cleared. */
static inline void unlock_counter(eventcount_t* counter)
{
   clear_atomicflag(&counter->lockflag);
}

/* function: wakeup_thread
 * Removes first thread in list from list of waiting threads and wakes it up.
 * */
static inline void wakeup_thread(eventcount_t* counter)
{
   thread_t* thread = first_threadlist(cast_slist(counter));
   lockflag_thread(thread);
   removefirst_threadlist(cast_slist(counter));
   unlockflag_thread(thread);
   resume_thread(thread);
}

// group: lifetime

int free_eventcount(eventcount_t* counter)
{
   lock_counter(counter);
   counter->nrevents = 0;
   while (!isempty_slist(cast_slist(counter))) {
      wakeup_thread(counter);
   }
   unlock_counter(counter);
   return 0;
}

// group: query

uint32_t nrwaiting_eventcount(eventcount_t* counter)
{
   int32_t oldval = read_atomicint(&counter->nrevents);
   return oldval < 0 ? -(uint32_t)oldval : 0;
}

uint32_t nrevents_eventcount(eventcount_t* counter)
{
   int32_t oldval = read_atomicint(&counter->nrevents);
   return oldval >= 0 ? (uint32_t)oldval : 0;
}

// group: update

void count_eventcount(eventcount_t* counter)
{
   int32_t oldval = add_atomicint(&counter->nrevents, 1);
   assert(oldval != INT32_MAX);

   if (oldval < 0) {
      lock_counter(counter);
      wakeup_thread(counter);
      unlock_counter(counter);
   }
}

int trywait_eventcount(eventcount_t* counter)
{
   int32_t oldval = read_atomicint(&counter->nrevents);

   while (oldval > 0) {
      const int32_t oldval2 = cmpxchg_atomicint(&counter->nrevents, oldval, oldval-1);
      int isDone = (oldval2 == oldval);
      if (isDone) return 0;
      oldval = oldval2;
   }

   return EAGAIN;
}

static void wait2_eventcount(eventcount_t* counter)
{
   thread_t *self = self_thread();
   lock_counter(counter);
   const int32_t oldval = sub_atomicint(&counter->nrevents, 1);
   assert(oldval != INT32_MIN);
   const int isEvent = (oldval > 0);
   if (isEvent) {
      unlock_counter(counter);
   } else {
      insertlast_threadlist(cast_slist(counter), self);
      unlock_counter(counter);

      // waiting loop
      bool isNoWakeup;
      do {
         suspend_thread();

         // spurious resume ?
         lockflag_thread(self);
         isNoWakeup = (0 != self->nextwait);
         unlockflag_thread(self);

      } while (isNoWakeup);
   }
}

void wait_eventcount(eventcount_t* counter)
{
   if (trywait_eventcount(counter) /*is_error?*/) {
      wait2_eventcount(counter);
   }
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int check_isfree(eventcount_t* counter)
{
   TEST( 0 == counter->nrevents);
   TEST( 0 == counter->last);
   TEST( 0 == counter->lockflag);
   return 0;
ONERR:
   return EINVAL;
}

static size_t s_nrthreads_started;
static size_t s_nrthreads_stopped;

static int testthread_wait(void* _counter)
{
   eventcount_t* counter = _counter;
   add_atomicint(&s_nrthreads_started, 1);
   wait_eventcount(counter);
   add_atomicint(&s_nrthreads_stopped, 1);
   return 0;
}

static int testthread_wait2(void* _counter)
{
   eventcount_t* counter = _counter;
   add_atomicint(&s_nrthreads_started, 1);
   wait2_eventcount(counter);
   add_atomicint(&s_nrthreads_stopped, 1);
   return 0;
}

static int testthread_count(void* _counter)
{
   eventcount_t* counter = _counter;
   add_atomicint(&s_nrthreads_started, 1);
   count_eventcount(counter);
   add_atomicint(&s_nrthreads_stopped, 1);
   return 0;
}

static int testthread_lock(void* _counter)
{
   eventcount_t* counter = _counter;
   add_atomicint(&s_nrthreads_started, 1);
   lock_counter(counter);
   add_atomicint(&s_nrthreads_stopped, 1);
   return 0;
}

static int testthread_wakeup(void* _counter)
{
   eventcount_t* counter = _counter;
   add_atomicint(&s_nrthreads_started, 1);
   wakeup_thread(counter);
   add_atomicint(&s_nrthreads_stopped, 1);
   return 0;
}

static int check_nrthreads(size_t size, size_t* nrthreads)
{
   for (size_t i=0; i<size+1 && read_atomicint(nrthreads) != size; ++i) {
      sleepms_thread(5);
   }
   TEST( size == read_atomicint(nrthreads));

   return 0;
ONERR:
   return EINVAL;
}

static int check_nrstarted(size_t size)
{
   return check_nrthreads(size, &s_nrthreads_started);
}

static int check_nrstopped(size_t size)
{
   return check_nrthreads(size, &s_nrthreads_stopped);
}

static int start_threads(unsigned size, thread_t* threads[size], thread_f threadmain, void* arg)
{
   s_nrthreads_started = 0;
   s_nrthreads_stopped = 0;
   for (unsigned i=0; i<size; ++i) {
      TEST(0 == new_thread(&threads[i], threadmain, arg));
   }
   TEST(0 == check_nrstarted(size));
   TEST(0 == check_nrstopped(0));

   return 0;
ONERR:
   return EINVAL;
}

static int delete_threads(unsigned size, thread_t* threads[size])
{
   for (unsigned i=0; i<size; ++i) {
      TEST(0 == delete_thread(&threads[i]));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   eventcount_t counter = eventcount_FREE;
   thread_t *   threads[16] = {0};

   // TEST eventcount_FREE
   TEST( 0 == check_isfree(&counter));
   TEST( isfree_eventcount(&counter));

   // TEST eventcount_INIT: same as eventcount_FREE
   counter = (eventcount_t) eventcount_INIT;
   TEST( 0 == check_isfree(&counter));
   TEST( isfree_eventcount(&counter));

   // TEST init_eventcount
   memset(&counter, 255, sizeof(counter));
   init_eventcount(&counter);
   TEST( 0 == check_isfree(&counter));

   // TEST free_eventcount: already free
   TEST( 0 == free_eventcount(&counter));
   TEST( 0 == check_isfree(&counter));

   // TEST free_eventcount: wakeup sleeping threads
   // prepare
   start_threads(lengthof(threads), threads, &testthread_wait, &counter);
   // test
   TEST( lengthof(threads) == - (uint32_t) read_atomicint(&counter.nrevents));
   TEST( 0 != counter.last);
   TEST( 0 == free_eventcount(&counter));
   // check
   TEST( 0 == check_isfree(&counter));
   for (unsigned i=0; i<lengthof(threads); ++i) {
      TEST(0 == join_thread(threads[i]));
   }
   TEST( 0 == check_nrstopped(lengthof(threads)));
   TEST( 0 == delete_threads(lengthof(threads), threads));

   return 0;
ONERR:
   (void) free_eventcount(&counter);
   return EINVAL;
}

static int test_query(void)
{
   eventcount_t counter = eventcount_FREE;

   // TEST isfree_eventcount
   TEST( isfree_eventcount(&counter));
   counter.nrevents = 1;
   TEST( 0 == isfree_eventcount(&counter));
   counter.nrevents = 0;
   counter.last = (void*)1;
   TEST( 0 == isfree_eventcount(&counter));
   counter.last = (void*)0;
   counter.lockflag = 1;
   TEST( 0 == isfree_eventcount(&counter));
   counter.lockflag = 0;
   TEST( isfree_eventcount(&counter));

   // TEST nrevents_eventcount: free counter
   TEST( 0 == nrevents_eventcount(&counter));

   // TEST nrwaiting_eventcount: free counter
   TEST( 0 == nrwaiting_eventcount(&counter));

   // TEST nrevents_eventcount: different values
   const int32_t nrevents[] = { 1, 2, 3, 4, 5, 100, 1000, 65535, 65536, INT32_MAX-3, INT32_MAX-2, INT32_MAX-1, INT32_MAX };
   for (unsigned i=0; i<lengthof(nrevents); ++i) {
      counter.nrevents = nrevents[i];
      TEST( (uint32_t)nrevents[i] == nrevents_eventcount(&counter));
      counter.nrevents = -counter.nrevents;
      TEST( 0 == nrevents_eventcount(&counter));
   }

   // TEST nrwaiting_eventcount: different values
   const int32_t nrwaiting[] = { -1, -2, -3, -4, -5, -100, -1000, -65535, -65536, INT32_MIN+3, INT32_MIN+2, INT32_MIN+1, INT32_MIN };
   for (unsigned i=0; i<lengthof(nrevents); ++i) {
      counter.nrevents = nrwaiting[i];
      TEST( -(uint32_t)nrwaiting[i] == nrwaiting_eventcount(&counter));
      if (counter.nrevents != INT32_MIN) {
         counter.nrevents = -counter.nrevents;
         TEST( 0 == nrwaiting_eventcount(&counter));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_helper(void)
{
   eventcount_t counter = eventcount_FREE;
   thread_t   * thread  = 0;
   thread_t   * thread2 = 0;

   // TEST lock_counter: sets lock
   lock_counter(&counter);
   TEST( 0 != counter.lockflag);

   // TEST unlock_counter: removes lock
   unlock_counter(&counter);
   TEST( 0 == check_isfree(&counter));

   // TEST lock_counter: waits for lock
   lock_counter(&counter);
   TEST(0 == start_threads(1, &thread, &testthread_lock, &counter));
   sleepms_thread(5);
   // check
   TEST( 0 == check_nrstopped(0));  // waiting in loop
   unlock_counter(&counter);        // unlock counter => thread continues
   TEST( 0 == join_thread(thread)); // wait until thread has run
   TEST( 0 == check_nrstopped(1));  // thread has exited wait loop
   TEST( 0 != counter.lockflag);    // counter locked in thread
   // reset
   TEST( 0 == delete_thread(&thread));
   unlock_counter(&counter);
   TEST( 0 == check_isfree(&counter));

   // TEST wakeup_thread: wakes up first thread
   TEST(0 == start_threads(1, &thread, &testthread_wait, &counter));
   TEST(0 == start_threads(1, &thread2, &testthread_wait, &counter));
   sleepms_thread(5);
   slist_node_t* F = (slist_node_t*) &thread->nextwait;
   slist_node_t* L = (slist_node_t*) &thread2->nextwait;
   TEST(-2 == counter.nrevents);
   TEST(L == counter.last);
   TEST(0 == counter.lockflag);
   TEST(F == thread2->nextwait);
   TEST(L == thread->nextwait);
   TEST(0 == check_nrstopped(0));
   for (unsigned tnr=0; tnr<2; ++tnr) {
      // test
      wakeup_thread(&counter);
      // check
      TEST( -2 == counter.nrevents);   // unchanged
      TEST( (tnr?0:L) == counter.last);
      TEST( 0 == counter.lockflag);    // unchanged
      TEST( (tnr?0:L) == thread2->nextwait);
      TEST( 0 == thread->nextwait);
      TEST( 0 == join_thread(tnr?thread2:thread)); // check resumed
      TEST( 0 == check_nrstopped(tnr+1));          // check resumed
   }
   // reset
   counter.nrevents = 0;
   TEST( 0 == delete_thread(&thread));
   TEST( 0 == delete_thread(&thread2));

   // TEST wakeup_thread: acquires thread lock / counter not locked
   TEST(0 == start_threads(1, &thread, &testthread_wait, &counter));
   L = (slist_node_t*) &thread->nextwait;
   TEST(-1 == counter.nrevents);
   TEST(L == counter.last);
   TEST(0 == counter.lockflag);
   TEST(L == thread->nextwait);
   TEST(0 == check_nrstopped(0));
   // test
   lock_counter(&counter);
   lockflag_thread(thread);
   TEST(0 == start_threads(1, &thread2, &testthread_wakeup, &counter));
   for (unsigned cnr=0; cnr<2; ++cnr) {
      if (cnr) {
         unlockflag_thread(thread);
         TEST( 0 == join_thread(thread2));  // did wakeup
         TEST( 0 == join_thread(thread));   // resumed
      }
      // check
      TEST( -1 == counter.nrevents);   // unchanged
      TEST( (cnr?0:L) == counter.last);
      TEST( 0 != counter.lockflag);    // unchanged
      TEST( (cnr?0:L) == thread->nextwait);
      TEST( 0 == check_nrstopped(2*cnr)); // 0:unchanged 1:check resumed
   }
   // reset
   counter.nrevents = 0;
   unlock_counter(&counter);
   TEST( 0 == check_isfree(&counter));
   TEST( 0 == delete_thread(&thread));
   TEST( 0 == delete_thread(&thread2));

   return 0;
ONERR:
   delete_thread(&thread);
   delete_thread(&thread2);
   return EINVAL;
}

static int test_update(void)
{
   eventcount_t counter = eventcount_FREE;
   process_t    child   = process_FREE;
   process_result_t child_result;
   thread_t *   threads[6] = {0};

   // prepare
   init_eventcount(&counter);

   // TEST count_eventcount: increment by one
   for (unsigned i=1; i<15; ++i) {
      count_eventcount(&counter);
      TEST( i == (uint32_t) counter.nrevents);
   }
   counter.nrevents = 0; // reset

   // TEST count_eventcount: INT32_MAX
   counter.nrevents = INT32_MAX;
   TEST( 0 == init_process(&child, &testthread_count, &counter, 0))
   // check
   TEST( 0 == wait_process(&child, &child_result));
   TEST( process_state_ABORTED == child_result.state); // assert (checked precondition violated)
   // reset
   TEST( 0 == free_process(&child));
   counter.nrevents = 0;

   // TEST count_eventcount: wakeup
   TEST(0 == start_threads(lengthof(threads), threads, &testthread_wait, &counter));
   for (unsigned i=1; i<=lengthof(threads); ++i) {
      // test
      thread_t* thr = first_threadlist(cast_slist(&counter));
      count_eventcount(&counter);
      // check
      TEST( lengthof(threads)-i == nrwaiting_eventcount(&counter));
      TEST( 0 == join_thread(thr));
      TEST( lengthof(threads)-i == nrwaiting_eventcount(&counter));
   }
   // reset
   TEST(0 == delete_threads(lengthof(threads), threads));

   // TEST count_eventcount: acquire locks
   for (unsigned locknr=0; locknr<2; ++locknr) {
      unsigned N=3;
      TEST(0 == start_threads(N-1, threads+1, &testthread_wait, &counter));
      for (unsigned i=1; i<N; ++i) {
         thread_t* thr = first_threadlist(cast_slist(&counter));
         // test
         if (0==locknr) {
            lock_counter(&counter);
         } else {
            lockflag_thread(thr);
         }
         TEST( 0 == new_thread(&threads[0], &testthread_count, &counter));
         // check before
         TEST( 0 == check_nrstarted(N-1+i));
         sleepms_thread(5);
         TEST( 0 == check_nrstopped(2*i-2));
         TEST( N-1-i == nrwaiting_eventcount(&counter)); // changed (event incremented)
         TEST( 0 != thr->nextwait); // unchanged (hangs in lock)
         // remove lock
         if (0==locknr) {
            unlock_counter(&counter);
         } else {
            unlockflag_thread(thr);
         }
         // check after
         TEST( 0 == join_thread(threads[0]));
         TEST( 0 == thr->nextwait); // changed
         TEST( 0 == join_thread(thr));
         TEST( 0 == check_nrstopped(2*i));
         // reset
         TEST( 0 == delete_thread(&threads[0]));
      }
      TEST(0 == delete_threads(lengthof(threads), threads));
   }

   // TEST trywait_eventcount: decrement nrevents
   // TEST wait_eventcount: decrement nrevents
   // TEST wait2_eventcount: decrement nrevents
   for (unsigned tc=0; tc<3; ++tc) {
      counter.nrevents = 40;
      for (unsigned i=40; i>0; --i) {
         switch (tc) {
            case 0: TEST( 0 == trywait_eventcount(&counter)); break;
            case 1: wait2_eventcount(&counter); break;
            case 2: wait_eventcount(&counter); break;
         }
         TEST( i-1 == (uint32_t) counter.nrevents);
         TEST( 0 == counter.last);
         TEST( 0 == counter.lockflag);
      }
   }

   // TEST trywait_eventcount: does not acquire lock
   // TEST wait_eventcount: does not acquire lock
   for (unsigned tc=0; tc<2; ++tc) {
      lock_counter(&counter);
      counter.nrevents = INT32_MAX;
      switch (tc) {
         case 0: TEST( 0 == trywait_eventcount(&counter)); break;
         case 1: wait_eventcount(&counter); break;
      }
      TEST( INT32_MAX-1 == counter.nrevents);
      TEST( 0 == counter.last);
      TEST( 0 != counter.lockflag);
      unlock_counter(&counter);
   }

   // TEST wait2_eventcount: acquires lock before decrement
   lock_counter(&counter);
   counter.nrevents = INT32_MAX;
   start_threads(1, threads, &testthread_wait2, &counter);
   sleepms_thread(1);
   TEST( INT32_MAX == read_atomicint(&counter.nrevents)); // thread waits for lock
   TEST( 0 == check_nrstopped(0));
   // check
   unlock_counter(&counter);
   TEST( 0 == join_thread(threads[0]));
   TEST( 0 == check_nrstopped(1));
   TEST( INT32_MAX-1 == counter.nrevents);
   TEST( 0 == counter.last);
   TEST( 0 == counter.lockflag);
   // reset
   TEST(0 == delete_thread(&threads[0]));

   // TEST trywait_eventcount: EAGAIN, no change if nrevents <= 0
   const int32_t tryval[][2] = { {INT32_MIN, INT32_MIN+10}, {INT32_MIN+65535, INT32_MIN+65535+3}, {-10, 0} };
   for (unsigned i=0; i<lengthof(tryval); ++i) {
      unsigned range = (unsigned) (tryval[i][1] - tryval[i][0]);
      for (unsigned valoff=0; valoff<=range; ++valoff) {
         const int32_t N = tryval[i][0] + (int32_t)valoff;
         counter.nrevents = N;
         TEST( EAGAIN == trywait_eventcount(&counter));
         TEST( N == counter.nrevents);
         TEST( 0 == counter.last);
         TEST( 0 == counter.lockflag);
      }
   }

   // TEST wait2_eventcount: INT32_MIN
   // TEST wait_eventcount: INT32_MIN
   for (unsigned tc=0; tc<2; ++tc) {
      counter.nrevents = INT32_MIN;
      switch (tc) {
         case 0: TEST( 0 == init_process(&child, &testthread_wait2, &counter, 0)); break;
         case 1: TEST( 0 == init_process(&child, &testthread_wait, &counter, 0)); break;
      }
      // check
      TEST( 0 == wait_process(&child, &child_result));
      TEST( process_state_ABORTED == child_result.state); // assert (checked precondition violated)
      // reset
      TEST( 0 == free_process(&child));
   }

   // TEST wait2_eventcount: add to wait list as last element
   // TEST wait_eventcount: add to wait list as last element
   counter.nrevents = 0;
   for (unsigned tc=0; tc<2; ++tc) {
      for (unsigned i=0; i<lengthof(threads); ++i) {
         switch (tc) {
            case 0: start_threads(1, threads+i, &testthread_wait2, &counter); break;
            case 1: start_threads(1, threads+i, &testthread_wait, &counter); break;
         }
         // check
         slist_node_t* L = (slist_node_t*) &threads[i]->nextwait;
         TEST( i+1 == -(uint32_t)counter.nrevents);
         TEST( L == counter.last);
         TEST( 0 == counter.lockflag);
         unsigned i2=0;
         foreach (_threadlist, node, cast_slist(&counter)) {
            TEST( i2<=i);
            TEST( node == threads[i2++]);
         }
         TEST( i2==i+1);
         TEST( 0 == check_nrstopped(0));
      }
      // reset
      for (unsigned i=0; i<lengthof(threads); ++i) {
         count_eventcount(&counter);
      }
      for (unsigned i=0; i<lengthof(threads); ++i) {
         TEST(0 == join_thread(threads[i]));
         TEST(0 == delete_thread(&threads[i]));
      }
      TEST(0 == check_nrstopped(lengthof(threads)));
   }

   // TEST wait2_eventcount: acquires lock before adding to list
   // TEST wait_eventcount: acquires lock before adding to list
   counter.nrevents = 0;
   for (unsigned tc=0; tc<2; ++tc) {
      lock_counter(&counter);
      switch (tc) {
         case 0: start_threads(1, threads, &testthread_wait2, &counter); break;
         case 1: start_threads(1, threads, &testthread_wait, &counter); break;
      }
      // check
      slist_node_t* L = (slist_node_t*) &threads[0]->nextwait;
      TEST( 0 == read_atomicint(&counter.nrevents));
      TEST( 0 == read_atomicint(&counter.last));
      TEST( 0 != counter.lockflag);
      unlock_counter(&counter); // resume waiting thread
      sleepms_thread(5);
      TEST( -1 == read_atomicint(&counter.nrevents));
      TEST( L == read_atomicint(&counter.last));
      TEST( 0 == counter.lockflag);
      TEST( 0 == check_nrstopped(0));
      // reset
      count_eventcount(&counter);
      TEST(0 == join_thread(threads[0]));
      TEST(0 == delete_thread(&threads[0]));
      TEST(0 == check_nrstopped(1));
   }

   return 0;
ONERR:
   (void) free_eventcount(&counter);
   (void) free_process(&child);
   (void) delete_threads(lengthof(threads), threads);
   return EINVAL;
}

int unittest_platform_sync_eventcount()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_helper())      goto ONERR;
   if (test_update())      goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
