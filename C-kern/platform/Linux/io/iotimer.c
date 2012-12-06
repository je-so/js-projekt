/* title: IOTimer Linux
   Linux specific implementation of <IOTimer>.

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

   file: C-kern/api/io/iotimer.h
    Header file <IOTimer>.

   file: C-kern/platform/Linux/io/iotimer.c
    Linux implementation file <IOTimer Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iotimer.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/time/timevalue.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/thread.h"
#endif


// section: iotimer_t

// group: helper

static inline void compiletime_(void)
{
   static_assert(CLOCK_REALTIME  == timeclock_REAL, "timeclock_e is compatible to clockid_t") ;
   static_assert(CLOCK_MONOTONIC == timeclock_MONOTONIC, "timeclock_e is compatible to clockid_t") ;
   static_assert(sizeof(uint64_t) >= sizeof(((struct itimerspec*)0)->it_interval.tv_sec), "conversion possible") ;
   static_assert(sizeof(uint32_t) <= sizeof(((struct itimerspec*)0)->it_interval.tv_sec), "conversion possible") ;
   static_assert(sizeof(((struct itimerspec*)0)->it_interval.tv_sec) == sizeof(time_t), "tv_sec is time_t") ;
   static_assert(sizeof(((struct itimerspec*)0)->it_value.tv_sec) == sizeof(time_t), "tv_sec is time_t") ;
   static_assert((typeof(((struct itimerspec*)0)->it_interval.tv_sec))-1 < 0, "tv_sec is time_t") ;
   static_assert((typeof(((struct itimerspec*)0)->it_value.tv_sec))-1 < 0, "tv_sec is time_t") ;
   static_assert((time_t)-1 < 0, "time_t is signed") ;
}

/* define: convertclockid
 * Converts <timeclock_e> into <clockid_t>. */
#define convertclockid(/*timeclock_e*/clock_type) \
   ((clockid_t) (clock_type))

/* function: timespec2timevalue_iotimer
 * Converts struct timespec into <timevalue_t>. */
static inline void timespec2timevalue_iotimer(/*out*/timevalue_t * tval, const struct timespec * tspec)
{
   tval->seconds = (uint64_t) tspec->tv_sec ;
   tval->nanosec = tspec->tv_nsec ;
}

/* function: timespec_MAXSECONDS
 * Returns the maximum value timespec->tv_sec can hold. */
static inline uint64_t timespec_MAXSECONDS(void)
{
   if (sizeof(((struct timespec*)0)->tv_sec) == sizeof(uint32_t)) {
      return INT32_MAX ;
   } else if (sizeof(((struct timespec*)0)->tv_sec) == sizeof(uint64_t)) {
      return INT64_MAX ;
   }
}

// group: lifetime

int init_iotimer(/*out*/iotimer_t* timer, timeclock_e clock_type)
{
   int err ;
   clockid_t   clockid = convertclockid(clock_type) ;
   int         fd ;

   fd = timerfd_create(clockid, TFD_NONBLOCK|TFD_CLOEXEC) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("timerfd_create", err) ;
      PRINTINT_LOG(clock_type) ;
      goto ONABORT ;
   } else {
      *timer = fd ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_iotimer(iotimer_t * timer)
{
   int err ;

   err = free_filedescr(timer) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int start_iotimer(iotimer_t timer, timevalue_t * relative_time)
{
   int err ;

   VALIDATE_INPARAM_TEST(relative_time->seconds < timespec_MAXSECONDS(), ONABORT, ) ;

   struct itimerspec new_timeout = {
       .it_interval = { 0, 0 }
      ,.it_value    = { .tv_sec = (time_t) relative_time->seconds, .tv_nsec = relative_time->nanosec }
   } ;

   if (timerfd_settime(timer, 0, &new_timeout, /*old timeout*/0)) {
      err = errno ;
      TRACESYSERR_LOG("timerfd_settime", err) ;
      PRINTINT_LOG(timer) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int startinterval_iotimer(iotimer_t timer, timevalue_t * interval_time)
{
   int err ;

   VALIDATE_INPARAM_TEST(interval_time->seconds < timespec_MAXSECONDS(), ONABORT, ) ;

   struct itimerspec new_timeout = {
       .it_interval = { .tv_sec = (time_t) interval_time->seconds, .tv_nsec = interval_time->nanosec }
      ,.it_value    = { .tv_sec = (time_t) interval_time->seconds, .tv_nsec = interval_time->nanosec }
   } ;

   if (timerfd_settime(timer, 0, &new_timeout, /*old timeout*/0)) {
      err = errno ;
      TRACESYSERR_LOG("timerfd_settime", err) ;
      PRINTINT_LOG(timer) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int stop_iotimer(iotimer_t timer)
{
   int err ;
   struct itimerspec new_timeout = {
       .it_interval = { 0, 0 }
      ,.it_value    = { 0, 0 }  /* it_value == (0, 0) => disarm */
   } ;

   if (timerfd_settime(timer, 0, &new_timeout, /*old timeout*/0)) {
      err = errno ;
      TRACESYSERR_LOG("timerfd_settime", err) ;
      PRINTINT_LOG(timer) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int wait_iotimer(iotimer_t timer)
{
   int err ;
   struct pollfd  pfds[1] = { { .fd = timer, .events = POLLIN } } ;
   timevalue_t remaining_time ;

   err = remainingtime_iotimer(timer, &remaining_time ) ;
   if (err) goto ONABORT ;

   do {
      err = poll( pfds, 1, (remaining_time.nanosec || remaining_time.seconds) ? -1/*wait indefinitely*/ : 0 ) ;
   } while (-1 == err && errno == EINTR) ;

   if (1 != err) {
      if (-1 == err) {
         err = errno ;
         TRACESYSERR_LOG("poll", err) ;
         PRINTINT_LOG(timer) ;
      } else {
         err = EINVAL ;
      }
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remainingtime_iotimer(iotimer_t timer, timevalue_t * remaining_time)
{
   int err ;
   struct itimerspec next_timeout ;

   if (timerfd_gettime(timer, &next_timeout)) {
      err = errno ;
      TRACESYSERR_LOG("timerfd_gettime", err) ;
      PRINTINT_LOG(timer) ;
      goto ONABORT ;
   } else {
      timespec2timevalue_iotimer(remaining_time, &next_timeout.it_value) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int expirationcount_iotimer(iotimer_t timer, /*out*/uint64_t * expiration_count)
{
   int err ;

   static_assert( sizeof(*expiration_count) == sizeof(uint64_t), ) ;

   if (sizeof(uint64_t) != read(timer, expiration_count, sizeof(uint64_t))) {
      if (errno != EAGAIN) {
         err = errno ;
         TRACESYSERR_LOG("read", err) ;
         PRINTINT_LOG(timer) ;
         goto ONABORT ;
      }
      *expiration_count = 0 ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   iotimer_t   iotimer = iotimer_INIT_FREEABLE ;
   size_t      openfds[2] ;
   uint64_t    expcount ;
   timevalue_t timeval ;

   // TEST static init
   TEST(-1 == iotimer) ;

   // TEST init, double free
   TEST(0 == nropen_filedescr(&openfds[0])) ;
   TEST(0 == init_iotimer(&iotimer, timeclock_MONOTONIC)) ;
   TEST(0 < iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]+1) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]) ;

   // TEST free a started timer
   TEST(0 == nropen_filedescr(&openfds[0])) ;
   TEST(0 == init_iotimer(&iotimer, timeclock_REAL)) ;
   TEST(0 < iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]+1) ;
   TEST(0 == start_iotimer(iotimer, &(timevalue_t){ .seconds =1 })) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]) ;

   // TEST free a started interval timer
   TEST(0 == nropen_filedescr(&openfds[0])) ;
   TEST(0 == init_iotimer(&iotimer, timeclock_MONOTONIC)) ;
   TEST(0 < iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]+1) ;
   TEST(0 == startinterval_iotimer(iotimer, &(timevalue_t){ .seconds =1 })) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;
   TEST(0 == nropen_filedescr(&openfds[1])) ;
   TEST(openfds[1] == openfds[0]) ;

   // TEST start
   TEST(0 == init_iotimer(&iotimer, timeclock_MONOTONIC)) ;
   TEST(0 < iotimer) ;
   TEST(0 == start_iotimer(iotimer, &(timevalue_t){ .nanosec = 100000 })) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 <  timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(0 == expcount) ;
   sleepms_thread(1) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 == timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(1 == expcount) ;

   // TEST startinterval_iotimer
   TEST(0 == startinterval_iotimer(iotimer, &(timevalue_t){ .nanosec = 100000 })) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 <  timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(0 == expcount) ;
   sleepms_thread(1) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 <  timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(9 <= expcount) ;
   sleepms_thread(1) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 <  timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(9 <= expcount) ;

   // TEST start_iotimer, stop_iotimer
   TEST(0 == start_iotimer(iotimer, &(timevalue_t){ .seconds = 10 })) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(9 == timeval.seconds) ;
   TEST(0 <  timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(0 == expcount) ;
   TEST(0 == stop_iotimer(iotimer)) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 == timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(0 == expcount) ;

   // TEST startinterval, stop_iotimer
   TEST(0 == startinterval_iotimer(iotimer, &(timevalue_t){ .nanosec = 100000 })) ;
   sleepms_thread(1) ;
   // expiratiocount > 0
   TEST(0 == stop_iotimer(iotimer)) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 == timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(0 == expcount) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;

   // TEST startinterval_iotimer, start_iotimer: EINVAL
   timeval = (timevalue_t) timevalue_INIT(timespec_MAXSECONDS(), 0) ;
   TEST(EINVAL == startinterval_iotimer(iotimer, &timeval)) ;
   TEST(EINVAL == start_iotimer(iotimer, &timeval)) ;

   // TEST wait timer
   TEST(0 == init_iotimer(&iotimer, timeclock_REAL)) ;
   TEST(0 < iotimer) ;
   TEST(0 == start_iotimer(iotimer, &(timevalue_t){ .nanosec = 100000/*0.1ms*/ })) ;
   TEST(0 == wait_iotimer(iotimer)) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 == timeval.nanosec) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(1 == expcount) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(0 == expcount) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;

   // TEST wait on stopped timer
   TEST(0 == init_iotimer(&iotimer, timeclock_REAL)) ;
   TEST(0 < iotimer) ;
   TEST(0 == start_iotimer(iotimer, &(timevalue_t){ .seconds = 10 })) ;
   TEST(0 == stop_iotimer(iotimer)) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(0 == expcount) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(0 == timeval.nanosec) ;
   TEST(EINVAL == wait_iotimer(iotimer)) ;

   // TEST wait on expired timer
   TEST(0 == start_iotimer(iotimer, &(timevalue_t){ .nanosec = 1 })) ;
   pthread_yield() ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(1 == expcount) ;
   TEST(EINVAL == wait_iotimer(iotimer)) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;

   // TEST wait on interval timer
   TEST(0 == init_iotimer(&iotimer, timeclock_MONOTONIC)) ;
   TEST(0 < iotimer) ;
   TEST(0 == startinterval_iotimer(iotimer, &(timevalue_t){ .nanosec = 100000 })) ;
   sleepms_thread(1) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(9 < expcount) ;
   TEST(0 == wait_iotimer(iotimer)) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(1 <= expcount && expcount < 3) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(10000 < timeval.nanosec) ;
   TEST(100000 > timeval.nanosec) ;
   TEST(0 == wait_iotimer(iotimer)) ;
   TEST(0 == expirationcount_iotimer(iotimer, &expcount)) ;
   TEST(1 == expcount) ;
   TEST(0 == remainingtime_iotimer(iotimer, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(10000 < timeval.nanosec) ;
   TEST(100000 > timeval.nanosec) ;
   TEST(0 == free_iotimer(&iotimer)) ;
   TEST(-1 == iotimer) ;

   return 0 ;
ONABORT:
   (void) free_iotimer(&iotimer) ;
   return EINVAL ;
}

static int test_timing(void)
{
   iotimer_t   iotimer[3] = { iotimer_INIT_FREEABLE, iotimer_INIT_FREEABLE, iotimer_INIT_FREEABLE } ;
   timeclock_e clocks[2]  = { timeclock_REAL, timeclock_MONOTONIC } ;
   unsigned    iclock     = 0 ;
   timeclock_e clock_type ;
   timevalue_t timeval ;
   timevalue_t starttime ;
   timevalue_t endtime ;
   uint64_t    expcount ;

RESTART_TEST:

   // prepare
   clock_type = clocks[iclock] ;

   for(unsigned i = 0; i < nrelementsof(iotimer); ++i) {
      TEST(0 == init_iotimer(iotimer + i, clock_type)) ;
   }

   // TEST 3 one shot timers running at different speed
   sleepms_thread(1) ;
   TEST(0 == time_timeclock(clock_type, &starttime)) ;
   TEST(0 == start_iotimer(iotimer[0], &(timevalue_t){ .nanosec = 1000000 })) ;
   TEST(0 == start_iotimer(iotimer[1], &(timevalue_t){ .nanosec = 5000000 })) ;
   TEST(0 == start_iotimer(iotimer[2], &(timevalue_t){ .nanosec = 9000000 })) ;
   for(unsigned i = 0; i < nrelementsof(iotimer); ++i) {
      TEST(0 == remainingtime_iotimer(iotimer[i], &timeval)) ;
      TEST(0 == timeval.seconds) ;
      TEST( 900000 < timeval.nanosec) ;
      TEST(9000000 > timeval.nanosec) ;
   }
   TEST(0 == wait_iotimer(iotimer[0])) ;
   TEST(0 == expirationcount_iotimer(iotimer[0], &expcount)) ;
   TEST(1 == expcount) ;
   TEST(0 == remainingtime_iotimer(iotimer[1], &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(3900000 < timeval.nanosec) ;
   TEST(4000000 > timeval.nanosec) ;
   TEST(0 == remainingtime_iotimer(iotimer[2], &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(7900000 < timeval.nanosec) ;
   TEST(8000000 > timeval.nanosec) ;
   for(unsigned i = 0; i < nrelementsof(iotimer); ++i) {
      TEST(0 == expirationcount_iotimer(iotimer[i], &expcount)) ;
      TEST(0 == expcount) ;
   }
   TEST(0 == wait_iotimer(iotimer[1])) ;
   TEST(0 == expirationcount_iotimer(iotimer[1], &expcount)) ;
   TEST(1 == expcount) ;
   TEST(0 == remainingtime_iotimer(iotimer[2], &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(3900000 < timeval.nanosec) ;
   TEST(4000000 > timeval.nanosec) ;
   for(unsigned i = 0; i < nrelementsof(iotimer); ++i) {
      TEST(0 == expirationcount_iotimer(iotimer[i], &expcount)) ;
      TEST(0 == expcount) ;
   }
   TEST(0 == wait_iotimer(iotimer[2])) ;
   TEST(0 == expirationcount_iotimer(iotimer[2], &expcount)) ;
   TEST(1 == expcount) ;
   TEST(0 == time_timeclock(clock_type, &endtime)) ;
   uint64_t elapsed_nanosec = 1000000000 * (uint64_t) (endtime.seconds - starttime.seconds)
                            + (uint64_t) endtime.nanosec - (uint64_t) starttime.nanosec ;
   TEST(9000000 < elapsed_nanosec) ;
   TEST(9100000 > elapsed_nanosec) ;

   // TEST 3 interval timers running at different speed
   sleepms_thread(1) ;
   TEST(0 == time_timeclock(clock_type, &starttime)) ;
   TEST(0 == startinterval_iotimer(iotimer[0], &(timevalue_t){ .nanosec = 1000000 })) ;
   TEST(0 == startinterval_iotimer(iotimer[1], &(timevalue_t){ .nanosec = 2000000 })) ;
   TEST(0 == startinterval_iotimer(iotimer[2], &(timevalue_t){ .nanosec = 3000000 })) ;
   for(int i = 1; i <= 10; ++i) {
      TEST(0 == wait_iotimer(iotimer[0])) ;
      TEST(0 == expirationcount_iotimer(iotimer[0], &expcount)) ;
      TEST(1 == expcount) ;
      if (0 == (i % 2)) {
         TEST(0 == expirationcount_iotimer(iotimer[1], &expcount)) ;
         TEST(1 == expcount) ;
      }
      if (0 == (i % 3)) {
         TEST(0 == expirationcount_iotimer(iotimer[2], &expcount)) ;
         TEST(1 == expcount) ;
      }
   }
   TEST(0 == time_timeclock(clock_type, &endtime)) ;
   elapsed_nanosec = 1000000000 * (uint64_t) (endtime.seconds - starttime.seconds)
                   + (uint64_t) endtime.nanosec - (uint64_t) starttime.nanosec ;
   TEST(10000000 < elapsed_nanosec) ;
   TEST(10100000 > elapsed_nanosec) ;

   // unprepare
   for(unsigned i = 0; i < nrelementsof(iotimer); ++i) {
      TEST(0 == free_iotimer(iotimer + i)) ;
   }

   ++ iclock ;
   if (iclock < nrelementsof(clocks)) goto RESTART_TEST ;

   return 0 ;
ONABORT:
   for(unsigned i = 0; i < nrelementsof(iotimer); ++i) {
      (void) free_iotimer(iotimer + i) ;
   }
   return EINVAL ;
}

int unittest_io_iotimer()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_timing())      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
