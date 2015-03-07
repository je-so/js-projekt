/* title: ITCCounter impl

   Implements <ITCCounter>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/task/itc/itccounter.h
    Header file <ITCCounter>.

   file: C-kern/platform/Linux/sync/itccounter.c
    Implementation file <ITCCounter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/itc/itccounter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/platform/task/thread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/time/timevalue.h"
#endif


// section: itccounter_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_itccounter_errtimer
 * Simuliert Fehler in <init_itccounter> und <free_itccounter>. */
static test_errortimer_t s_itccounter_errtimer = test_errortimer_FREE;
#endif

// group: lifetime

int init_itccounter(/*out*/itccounter_t* counter)
{
   int err;
   int fd;

   if (PROCESS_testerrortimer(&s_itccounter_errtimer)) {
      errno = ERRCODE_testerrortimer(&s_itccounter_errtimer);
      fd = -1;
   } else {
      fd = eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK);
   }

   if (-1 == fd) {
      err = errno;
      TRACESYSCALL_ERRLOG("eventfd", err);
      goto ONERR;
   }

   counter->sysio = fd;
   counter->count = 0;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_itccounter(itccounter_t* counter)
{
   int err;

   err = free_iochannel(&counter->sysio);
   SETONERROR_testerrortimer(&s_itccounter_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: writer

uint32_t increment_itccounter(itccounter_t* counter)
{
   int err;
   uint32_t count = counter->count;
   uint32_t oldval;

   while (count != (oldval = cmpxchg_atomicint(&counter->count, count, count+(count!=UINT32_MAX)))) {
      count = oldval;
   }

   if (PROCESS_testerrortimer(&s_itccounter_errtimer)) {
      suspend_thread(); // test race
   }

   if (0 == count) {
      uint64_t syscount = 1;
      err = write(counter->sysio, &syscount, sizeof(syscount));
      if (err < 0) {
         err = errno;
         TRACESYSCALL_ERRLOG("write", err);
         PRINTINT_ERRLOG(counter->sysio);
      }
   }

   return count;
}

uint32_t add_itccounter(itccounter_t* counter, uint16_t incr)
{
   int err;
   uint32_t count;
   uint32_t oldval = counter->count;
   uint32_t newcount;

   if (0 == incr) {
      return read_atomicint(&counter->count);
   }

   do {
      newcount = oldval + incr;
      count = oldval;
      if (newcount < oldval) {
         newcount = UINT32_MAX;
      }
   } while (count != (oldval = cmpxchg_atomicint(&counter->count, count, newcount)));

   if (PROCESS_testerrortimer(&s_itccounter_errtimer)) {
      suspend_thread(); // test race
   }

   if (0 == count) {
      uint64_t syscount = 1;
      err = write(counter->sysio, &syscount, sizeof(syscount));
      if (err < 0) {
         err = errno;
         TRACESYSCALL_ERRLOG("write", err);
         PRINTINT_ERRLOG(counter->sysio);
      }
   }

   return count;
}

// group: reader

int wait_itccounter(itccounter_t* counter, const int32_t msec_timeout/*<0: infinite timeout*/)
{
   int err;

   struct pollfd pfd = { .events = POLLIN, .fd = counter->sysio };
   err = poll(&pfd, 1, msec_timeout);

   if (err == 0) {
      return ETIME;
   } else if (err < 0) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

uint32_t reset_itccounter(itccounter_t* counter/*is reset to 0 before return*/)
{
   int err;

   uint64_t syscount;
   err = read(counter->sysio, &syscount, sizeof(syscount));
   // prevents race reading count>0 but no signal from sysio
   if (err < 0) {
      err = errno;
      if (err == EAGAIN) return 0;
      TRACESYSCALL_ERRLOG("read", err);
      PRINTINT_ERRLOG(counter->sysio);
   }

   if (PROCESS_testerrortimer(&s_itccounter_errtimer)) {
      suspend_thread(); // test race
   }

   uint32_t count = clear_atomicint(&counter->count);

   // counter->count == 0 ==> next write generates event

   return count;
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   itccounter_t counter = itccounter_FREE;

   // TEST itccounter_FREE
   TEST( isfree_iochannel(counter.sysio));
   TEST(0 == counter.count);

   // TEST init_itccounter
   memset(&counter, 255, sizeof(counter));
   TEST(0 == init_itccounter(&counter));
   // check counter
   TEST( isvalid_iochannel(counter.sysio));
   TEST( accessmode_RDWR == accessmode_iochannel(counter.sysio));
   TEST(0 == counter.count);

   // TEST free_itccounter
   counter.count = 3;
   TEST(0 == free_itccounter(&counter));
   // check counter
   TEST( isfree_iochannel(counter.sysio));
   TEST(3 == counter.count);        // unchanged

   // TEST init_itccounter: simulated ERROR
   for (int i = 1; i <= 1; ++i) {
      init_testerrortimer(&s_itccounter_errtimer, (unsigned)i, i);
      TEST(i == init_itccounter(&counter));
      TEST( isfree_iochannel(counter.sysio));
      TEST(3 == counter.count);        // unchanged
   }

   // TEST free_itccounter: simulated ERROR
   for (int i = 1; i <= 1; ++i) {
      TEST(0 == init_itccounter(&counter));
      init_testerrortimer(&s_itccounter_errtimer, (unsigned)i, i);
      TEST(i == free_itccounter(&counter));
      TEST( isfree_iochannel(counter.sysio));
   }

   return 0;
ONERR:
   free_itccounter(&counter);
   return EINVAL;
}

static int test_query(void)
{
   itccounter_t counter = itccounter_FREE;

   // TEST isfree_itccounter: itccounter_FREE
   TEST(1 == isfree_itccounter(&counter));

   // TEST isfree_itccounter: after init_itccounter
   // prepare
   TEST(0 == init_itccounter(&counter));
   // test
   TEST(0 == isfree_itccounter(&counter));

   // TEST isfree_itccounter: after free_itccounter
   // prepare
   TEST(0 == free_itccounter(&counter));
   // test
   TEST(1 == isfree_itccounter(&counter));

   // TEST isfree_itccounter: count is not checked
   // prepare
   TEST(0 == init_itccounter(&counter));
   TEST(0 == increment_itccounter(&counter));
   TEST(0 == free_itccounter(&counter));
   TEST(0 < counter.count);
   // test
   TEST(1 == isfree_itccounter(&counter));

   return 0;
ONERR:
   free_itccounter(&counter);
   return EINVAL;
}

static thread_t* s_self = 0;

static int thread_callwait(itccounter_t* counter)
{
   resume_thread(s_self);
   int err = wait_itccounter(counter, -1);
   return err;
}

static int thread_callreset(itccounter_t* counter)
{
   resume_thread(s_self);
   int count = (int) reset_itccounter(counter);
   return count;
}

static int test_reader(void)
{
   itccounter_t  counter = itccounter_FREE;
   struct pollfd pfd = { .events = POLLIN, .fd = -1 };
   thread_t*  thread = 0;
   systimer_t timer = systimer_FREE;
   uint64_t   msec;
   uint64_t   count;
   int        sysio = -1;
   uint8_t* logbuffer;
   size_t   logsize1=0;
   size_t   logsize2=0;

   // prepare0
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));
   TEST(0 == init_itccounter(&counter));
   sysio = counter.sysio;
   pfd.fd = sysio;

   // TEST io_itccounter
   for (int i = 1; i; i <<= 1) {
      counter.sysio = i;
      TEST(i == io_itccounter(&counter));
   }
   // reset
   counter.sysio = sysio;
   TEST(sysio == io_itccounter(&counter));

   // TEST wait_itccounter: no sysio event
   for (uint16_t c = 0; c <= 1; c = (uint16_t) (c + 1)) {
      counter.count = c;
      TEST(0 == startinterval_systimer(timer, & (timevalue_t) { .nanosec = 1000000 } ));
      TEST(ETIME == wait_itccounter(&counter, 30));
      // check timeout expired 30 msec
      TEST(0 == expirationcount_systimer(timer, &msec));
      TESTP(25 <= msec && msec <= 35, "msec:%"PRIu64, msec);
      // check counter
      TEST(counter.sysio == sysio);
      TEST(counter.count == c);
      // check no sys event
      TEST(0 == poll(&pfd, 1, 0));
   }
   // reset
   counter.count = 0;

   // TEST wait_itccounter: sysio event
   TEST(sizeof(count) == write(sysio, &count, sizeof(count)));
   for (uint16_t c = 0; c <= 10; c = (uint16_t) (c + 1)) {
      counter.count = c;
      // test
      TEST(0 == wait_itccounter(&counter, 30));
      // check counter
      TEST(sysio == counter.sysio);
      TEST(c == counter.count);
      // check sys events not changed
      TEST(1 == poll(&pfd, 1, 0));
   }
   // reset
   TEST(0 != read(sysio, &count, sizeof(count)));
   counter.count = 0;

   // TEST wait_itccounter: timeout == -1
   trysuspend_thread();
   s_self = self_thread();
   TEST(0 == newgeneric_thread(&thread, &thread_callwait, &counter));
   // check thread is waiting
   suspend_thread();
   sleepms_thread(10);
   TEST(EBUSY == tryjoin_thread(thread));
   // generate sys event
   count = 1;
   TEST(sizeof(count) == write(sysio, &count, sizeof(count)));
   // check thread returning from wait
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   // check counter
   TEST(counter.sysio == sysio);
   TEST(counter.count == 0);
   // check sysio is readable
   TEST(1 == poll(&pfd, 1, 0));
   // reset
   TEST(sizeof(count) == read(sysio, &count, sizeof(count)));
   TEST(0 == delete_thread(&thread));

   // TEST reset_itccounter: no sysio event
   for (uint16_t c = 0; c <= 256; c = (uint16_t)(c + 1)) {
      counter.count = c;
      // test
      TEST(0 == reset_itccounter(&counter));
      // check counter
      TEST(counter.sysio == sysio);
      TEST(counter.count == c);
      // check sys events not changed
      TEST(0 == poll(&pfd, 1, 0));
   }

   // TEST reset_itccounter: sysio event
   for (uint16_t c = 0; c <= 256; c = (uint16_t)(c + 1)) {
      count = 1;
      TEST(sizeof(count) == write(sysio, &count, sizeof(count)));
      counter.count = c;
      // test
      TEST(c == reset_itccounter(&counter));
      // check counter
      TEST(counter.sysio == sysio);
      TEST(counter.count == 0); // cleared
      // check sys events cleared
      TEST(0 == poll(&pfd, 1, 0));
   }

   // TEST reset_itccounter: race condition
   trysuspend_thread();
   s_self = self_thread();
   counter.count = 7;
   count = 10;
   TEST(sizeof(count) == write(sysio, &count, sizeof(count)));
   init_testerrortimer(&s_itccounter_errtimer, 1, 1);
   TEST(0 == newgeneric_thread(&thread, &thread_callreset, &counter));
   // check reset clears sysio first
   suspend_thread();
   for (int i = 0; i < 100; ++i) {
      if (0 == poll(&pfd, 1, 0)) break;
      sleepms_thread(1);
   }
   TEST(0 == poll(&pfd, 1, 0));
   // check count not reset
   sleepms_thread(1);
   TEST(7 == read_atomicint(&counter.count));
   // simulate writer thread (sysio no signalled cause count > 0)
   add_atomicint(&counter.count, 1);
   // check reset returns 7+1
   resume_thread(thread);
   TEST(0 == join_thread(thread));
   TEST(8 == returncode_thread(thread));
   // check counter
   TEST(counter.sysio == sysio);
   TEST(counter.count == 0);
   // check sysio keeps cleared
   TEST(0 == poll(&pfd, 1, 0));
   // reset
   counter.count = 0;
   TEST(0 == delete_thread(&thread));

   // TEST reset_itccounter: bad sysio (read fails)
   counter.sysio = -1;
   counter.count = 8;
   count = 1;
   TEST(sizeof(count) == write(sysio, &count, sizeof(count)));
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   TEST(8 == reset_itccounter(&counter));
   // check counter
   TEST(counter.sysio == -1);
   TEST(counter.count == 0);
   // check sysio not changed instead read generated error log entry
   TEST(1 == poll(&pfd, 1, 0));
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize2 > logsize1);
   // reset
   TEST(sizeof(count) == read(sysio, &count, sizeof(count)));
   counter.sysio = sysio;

   // reset0
   TEST(0 == free_itccounter(&counter));
   TEST(0 == free_systimer(&timer));

   return 0;
ONERR:
   if (thread) {
      resume_thread(thread);
      delete_thread(&thread);
   }
   free_systimer(&timer);
   free_itccounter(&counter);
   close(sysio);
   return EINVAL;
}

static int thread_callincrement(itccounter_t* counter)
{
   resume_thread(s_self);
   int count = (int) increment_itccounter(counter);
   return count;
}

static int thread_calladd(itccounter_t* counter)
{
   resume_thread(s_self);
   int count = (int) add_itccounter(counter, 256);
   return count;
}

static int test_writer(void)
{
   itccounter_t  counter = itccounter_FREE;
   struct pollfd pfd = { .events = POLLIN, .fd = -1 };
   thread_t*  thread = 0;
   uint64_t count;
   int sysio = -1;
   uint8_t* logbuffer;
   size_t   logsize1=0;
   size_t   logsize2=0;

   // prepare0
   TEST(0 == init_itccounter(&counter));
   sysio  = counter.sysio;
   pfd.fd = sysio;

   // TEST increment_itccounter: count == 0
   TEST(0 == increment_itccounter(&counter));
   // check counter
   TEST(sysio == counter.sysio);
   TEST(1 == counter.count);
   // check sysio received increment
   TEST(sizeof(count) == read(sysio, &count, sizeof(count)));
   TEST(1 == count);

   // TEST increment_itccounter: count > 0
   counter.count = 1;
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(i == increment_itccounter(&counter));
      // check counter
      TEST(sysio == counter.sysio);
      TEST(i+1 == counter.count);
      // check sysio not changed
      TEST(0 == poll(&pfd, 1, 0));
   }

   // TEST increment_itccounter: count == UINT32_MAX
   counter.count = UINT32_MAX;
   TEST(UINT32_MAX == increment_itccounter(&counter));
   // check counter
   TEST(sysio == counter.sysio);
   TEST(UINT32_MAX == counter.count); // no overflow
   // check sysio not changed
   TEST(0 == poll(&pfd, 1, 0));

   // TEST increment_itccounter: race condition
   trysuspend_thread();
   s_self = self_thread();
   counter.count = 0;
   init_testerrortimer(&s_itccounter_errtimer, 1, 1);
   TEST(0 == newgeneric_thread(&thread, &thread_callincrement, &counter));
   // check increment increments count first
   suspend_thread();
   for (int i = 0; i < 100; ++i) {
      if (read_atomicint(&counter.count)) break;
      sleepms_thread(1);
   }
   TEST(1 == read_atomicint(&counter.count));
   // check sysio not signalled
   sleepms_thread(1);
   TEST(0 == poll(&pfd, 1, 0));
   // simulate reader thread (sysio not signalled but count > 0)
   TEST(0 == reset_itccounter(&counter)); // does nothing
   // check increment returns 0
   resume_thread(thread);
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   // check counter
   TEST(counter.sysio == sysio);
   TEST(counter.count == 1);
   // check sysio signalled
   TEST(1 == poll(&pfd, 1, 0));
   // reset
   counter.count = 0;
   TEST(sizeof(count) == read(sysio, &count, sizeof(count)));
   TEST(0 == delete_thread(&thread));

   // TEST increment_itccounter: bad sysio (write fails)
   counter.sysio = -1;
   counter.count = 0;
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   TEST(0 == increment_itccounter(&counter));
   // check counter
   TEST(-1 == counter.sysio);
   TEST(1  == counter.count);
   // check sysio not changed instead write generated error log entry
   TEST(0 == poll(&pfd, 1, 0));
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize2 > logsize1);
   // reset
   counter.sysio = sysio;

   // TEST add_itccounter: incr == 0
   for (uint32_t i = 1; true; i <<= 1) {
      counter.count = i;
      TEST(i == add_itccounter(&counter, 0));
      // check counter
      TEST(sysio == counter.sysio);
      TEST(i == counter.count);
      // check sysio not changed
      TEST(0 == poll(&pfd, 1, 0));
      if (!i) break;
   }

   // TEST add_itccounter: count == 0
   for (uint16_t i = 1; i; i = (uint16_t) (i << 1)) {
      TEST(0 == add_itccounter(&counter, i));
      // check counter
      TEST(sysio == counter.sysio);
      TEST(i == counter.count);
      // check sysio received increment
      TEST(sizeof(count) == read(sysio, &count, sizeof(count)));
      TEST(1 == count);
      // reset
      counter.count = 0;
   }

   // TEST add_itccounter: count > 0
   counter.count = 1;
   for (unsigned i = 1, s=1; i <= 256; s += i, ++i) {
      TEST(s == add_itccounter(&counter, (uint16_t)i));
      // check counter
      TEST(sysio == counter.sysio);
      TEST(i+s == counter.count);
      // check sysio not changed
      TEST(0 == poll(&pfd, 1, 0));
   }

   // TEST add_itccounter: incr + count == UINT32_MAX-d
   for (unsigned d = 0; d <= 1; ++d) {
      for (uint16_t i = 10000; i < 30000; i = (uint16_t) (i + 5000)) {
         counter.count = UINT32_MAX-i-d;
         TEST(UINT32_MAX-i-d == add_itccounter(&counter, i));
         // check counter
         TEST(sysio == counter.sysio);
         TEST(UINT32_MAX-d == counter.count); // no overflow
         // check sysio not changed
         TEST(0 == poll(&pfd, 1, 0));
      }
   }

   // TEST add_itccounter: incr + count >= UINT32_MAX
   for (uint16_t i = 10; i >= 10; i = (uint16_t) (i << 1)) {
      counter.count = UINT32_MAX-9;
      TEST(UINT32_MAX-9 == add_itccounter(&counter, i));
      // check counter
      TEST(sysio == counter.sysio);
      TEST(UINT32_MAX == counter.count); // no overflow
      // check sysio not changed
      TEST(0 == poll(&pfd, 1, 0));
   }

   // TEST add_itccounter: race condition
   trysuspend_thread();
   s_self = self_thread();
   counter.count = 0;
   init_testerrortimer(&s_itccounter_errtimer, 1, 1);
   TEST(0 == newgeneric_thread(&thread, &thread_calladd, &counter));
   // check add increments count first
   suspend_thread();
   for (int i = 0; i < 100; ++i) {
      if (read_atomicint(&counter.count)) break;
      sleepms_thread(1);
   }
   TEST(256 == read_atomicint(&counter.count));
   // check sysio not signalled
   sleepms_thread(1);
   TEST(0 == poll(&pfd, 1, 0));
   // simulate reader thread (sysio not signalled but count > 0)
   TEST(0 == reset_itccounter(&counter)); // does nothing
   // check add returns 0
   resume_thread(thread);
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   // check counter
   TEST(counter.sysio == sysio);
   TEST(counter.count == 256);
   // check sysio signalled
   TEST(1 == poll(&pfd, 1, 0));
   // reset
   counter.count = 0;
   TEST(sizeof(count) == read(sysio, &count, sizeof(count)));
   TEST(0 == delete_thread(&thread));

   // TEST add_itccounter: bad sysio (write fails)
   counter.sysio = -1;
   counter.count = 0;
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   TEST(0 == add_itccounter(&counter, 18));
   // check counter
   TEST(-1 == counter.sysio);
   TEST(18 == counter.count);
   // check sysio not changed instead write generated error log entry
   TEST(0 == poll(&pfd, 1, 0));
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize2 > logsize1);
   // reset
   counter.sysio = sysio;

   // reset0
   TEST(0 == free_itccounter(&counter));

   return 0;
ONERR:
   if (thread) {
      resume_thread(thread);
      delete_thread(&thread);
   }
   free_itccounter(&counter);
   return EINVAL;
}

int unittest_task_itc_itccounter()
{
   if (test_initfree()) goto ONERR;
   if (test_query())    goto ONERR;
   if (test_reader())   goto ONERR;
   if (test_writer())   goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
