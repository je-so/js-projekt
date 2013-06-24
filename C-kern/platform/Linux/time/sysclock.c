/* title: SystemClock Linux
   Implements <SystemClock>.

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

   file: C-kern/api/time/sysclock.h
    Header file <SystemClock>.

   file: C-kern/platform/Linux/time/sysclock.c
    Implementation file <SystemClock Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/time/sysclock.h"
#include "C-kern/api/time/timevalue.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: sysclock

// group: helper

static inline void compiletime_(void)
{
   static_assert(CLOCK_REALTIME  == sysclock_REAL, "sysclock_e is compatible to clockid_t") ;
   static_assert(CLOCK_MONOTONIC == sysclock_MONOTONIC, "sysclock_e is compatible to clockid_t") ;
   static_assert(sizeof(uint64_t) >= sizeof(((struct timespec*)0)->tv_sec), "conversion possible") ;
   static_assert(sizeof(uint32_t) <= sizeof(((struct timespec*)0)->tv_sec), "conversion possible") ;
   static_assert(sizeof(((struct timespec*)0)->tv_sec) == sizeof(time_t), "tv_sec is time_t") ;
   static_assert((typeof(((struct timespec*)0)->tv_sec))-1 < 0, "tv_sec is time_t") ;
   static_assert(sizeof(((timevalue_t*)0)->seconds) == sizeof(uint64_t), "Cast into uint64_t works") ;
   static_assert((time_t)-1 < 0, "time_t is signed") ;
}

/* define: convertclockid
 * Converts <sysclock_e> into <clockid_t>. */
#define convertclockid(/*sysclock_e*/clock_type) \
   ((clockid_t) (clock_type))

/* function: timespec2timevalue_sysclock
 * Converts struct timespec into <timevalue_t>. */
static inline void timespec2timevalue_sysclock(/*out*/timevalue_t * tval, const struct timespec * tspec)
{
   tval->seconds = tspec->tv_sec ;
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

// group: query

int resolution_sysclock(sysclock_e clock_type, /*out*/timevalue_t * resolution)
{
   int err ;
   struct timespec   tspec ;
   clockid_t         clockid = convertclockid(clock_type) ;

   if (clock_getres(clockid, &tspec)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("clock_getres", err) ;
      PRINTINT_ERRLOG(clock_type) ;
      goto ONABORT ;
   }

   timespec2timevalue_sysclock(resolution, &tspec) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int time_sysclock(sysclock_e clock_type, /*out*/timevalue_t * clock_time)
{
   int err ;
   struct timespec   tspec ;
   clockid_t         clockid = convertclockid(clock_type) ;

   if (clock_gettime(clockid, &tspec)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("clock_gettime", err) ;
      PRINTINT_ERRLOG(clock_type) ;
      goto ONABORT ;
   }

   timespec2timevalue_sysclock(clock_time, &tspec) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int sleep_sysclock(sysclock_e clock_type, const timevalue_t * relative_time)
{
   int err ;

   VALIDATE_INPARAM_TEST(isvalid_timevalue(relative_time), ONABORT, ) ;
   VALIDATE_INPARAM_TEST((uint64_t)relative_time->seconds < timespec_MAXSECONDS(), ONABORT, ) ;

   struct timespec   tspec   = { .tv_sec = (time_t) relative_time->seconds, .tv_nsec = relative_time->nanosec } ;
   clockid_t         clockid = convertclockid(clock_type) ;

   while (clock_nanosleep(clockid, 0, &tspec, &tspec)) {
      err = errno ;
      if (EINTR == err) continue ;
      TRACESYSCALL_ERRLOG("clock_gettime", err) ;
      PRINTINT_ERRLOG(clock_type) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int sleepms_sysclock(sysclock_e clock_type, uint32_t millisec)
{
   int err ;
   struct timespec   tspec   = { .tv_sec = (int32_t) (millisec/1000), .tv_nsec = (int)(millisec%1000) * 1000000 } ;
   clockid_t         clockid = convertclockid(clock_type) ;

   while (clock_nanosleep(clockid, 0, &tspec, &tspec)) {
      err = errno ;
      if (EINTR == err) continue ;
      TRACESYSCALL_ERRLOG("clock_gettime", err) ;
      PRINTINT_ERRLOG(clock_type) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_timevalue(void)
{
   timevalue_t timeval ;
   timevalue_t timeval2 ;

   // TEST timevalue_INIT
   timeval = (timevalue_t) timevalue_INIT(10, 1000) ;
   TEST(timeval.seconds == 10) ;
   TEST(timeval.nanosec == 1000) ;
   timeval = (timevalue_t) timevalue_INIT(0, 1) ;
   TEST(timeval.seconds == 0) ;
   TEST(timeval.nanosec == 1) ;

   // TEST isvalid_timevalue
   timeval = (timevalue_t) timevalue_INIT(0, 0) ;
   TEST(true == isvalid_timevalue(&timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(0, 999999999u) ;
   TEST(true == isvalid_timevalue(&timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(INT64_MAX, 999999999u) ;
   TEST(true == isvalid_timevalue(&timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(INT64_MIN, 999999999u) ;
   TEST(true == isvalid_timevalue(&timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(0, 1 + 999999999u) ;
   TEST(false == isvalid_timevalue(&timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(INT64_MIN, 1 + 999999999u) ;
   TEST(false == isvalid_timevalue(&timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(INT64_MAX, 1 + 999999999u) ;
   TEST(false == isvalid_timevalue(&timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(0, -1) ;
   TEST(false == isvalid_timevalue(&timeval)) ;

   // TEST diffms_timevalue
   timeval  = (timevalue_t) timevalue_INIT(0, 0) ;
   timeval2 = (timevalue_t) timevalue_INIT(0, 0) ;
   TEST(0 == diffms_timevalue(&timeval2, &timeval)) ;
   timeval2 = (timevalue_t) timevalue_INIT(3, 0) ;
   TEST(3000 == diffms_timevalue(&timeval2, &timeval)) ;
   timeval2 = (timevalue_t) timevalue_INIT(3, 999999) ;
   TEST(3000 == diffms_timevalue(&timeval2, &timeval)) ;
   timeval2 = (timevalue_t) timevalue_INIT(3, 1000000) ;
   TEST(3001 == diffms_timevalue(&timeval2, &timeval)) ;
   timeval  = (timevalue_t) timevalue_INIT(1, 0) ;
   TEST(2001 == diffms_timevalue(&timeval2, &timeval)) ;
   timeval  = (timevalue_t) timevalue_INIT(1, 4000000) ;
   TEST(1997 == diffms_timevalue(&timeval2, &timeval)) ;
   timeval  = (timevalue_t) timevalue_INIT(2, 999999999) ;
   timeval2 = (timevalue_t) timevalue_INIT(1, 1000000) ;
   TEST(-1998 == diffms_timevalue(&timeval2, &timeval)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_clockquery(void)
{
   timevalue_t timeval ;
   timevalue_t timeval2 ;

   // TEST resolution_sysclock: (at least 1ms)
   TEST(0 == resolution_sysclock(sysclock_REAL, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(1000000 >= timeval.nanosec) ;
   TEST(0 == resolution_sysclock(sysclock_MONOTONIC, &timeval2)) ;
   TEST(0 == timeval2.seconds) ;
   TEST(1000000  >= timeval2.nanosec) ;
   TEST(timeval.nanosec == timeval2.nanosec) ;

   // TEST time_sysclock: (works only if scheduling is in bounds !)
   static_assert(sysclock_REAL == 0 && sysclock_MONOTONIC == 1, ) ;
   for (int i = sysclock_REAL; i <= sysclock_MONOTONIC; ++i) {
      pthread_yield() ;
      sysclock_e clock_type = (sysclock_e) i ;
      TEST(0 == time_sysclock(clock_type, &timeval)) ;
      sleepms_thread(10) ;
      TEST(0 == time_sysclock(clock_type, &timeval2)) ;
      int64_t nanosec = (timeval2.seconds - timeval.seconds) * 1000000000
                      + timeval2.nanosec - timeval.nanosec ;
      TEST(llabs(10000000 /*10msec*/- nanosec) < 1000000/*1msec*/) ;
      pthread_yield() ;
      TEST(0 == time_sysclock(clock_type, &timeval)) ;
      sleepms_thread(1) ;
      TEST(0 == time_sysclock(clock_type, &timeval2)) ;
      nanosec = (timeval2.seconds - timeval.seconds) * 1000000000
              + timeval2.nanosec - timeval.nanosec ;
      TEST(llabs(1000000 /*1msec*/- nanosec) < 1000000/*1msec*/) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_clockwait(void)
{
   timevalue_t timeval ;
   timevalue_t timeval2 ;

   // TEST sleep_sysclock: (works only if scheduling is in bounds !)
   static_assert(sysclock_REAL == 0 && sysclock_MONOTONIC == 1, ) ;
   for (int i = sysclock_REAL; i <= sysclock_MONOTONIC; ++i) {
      pthread_yield() ;
      sysclock_e clock_type = (sysclock_e) i ;
      TEST(0 == time_sysclock(clock_type, &timeval)) ;
      timeval2 = (timevalue_t) timevalue_INIT(0, 10000000) /*10msec*/ ;
      TEST(0 == sleep_sysclock(clock_type, &timeval2)) ;
      TEST(0 == time_sysclock(clock_type, &timeval2)) ;
      int64_t nanosec = (timeval2.seconds - timeval.seconds) * 1000000000
                      + timeval2.nanosec - timeval.nanosec ;
      TEST(llabs(10000000 /*10msec*/- nanosec) < 1000000/*1msec*/) ;
      pthread_yield() ;
      TEST(0 == time_sysclock(clock_type, &timeval)) ;
      timeval2 = (timevalue_t) timevalue_INIT(0, 1000000) /*1msec*/;
      TEST(0 == sleep_sysclock(clock_type, &timeval2)) ;
      TEST(0 == time_sysclock(clock_type, &timeval2)) ;
      nanosec = (timeval2.seconds - timeval.seconds) * 1000000000
              + timeval2.nanosec - timeval.nanosec ;
      TEST(llabs(1000000 /*1msec*/- nanosec) < 1000000/*1msec*/) ;
   }

   // TEST sleep_sysclock: EINVAL
   timeval = (timevalue_t) timevalue_INIT((int64_t)timespec_MAXSECONDS(), 0) ;
   TEST(EINVAL == sleep_sysclock(sysclock_REAL, &timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(-1, 0) ;
   TEST(EINVAL == sleep_sysclock(sysclock_REAL, &timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(1, 1 + 999999999) ;
   TEST(EINVAL == sleep_sysclock(sysclock_REAL, &timeval)) ;
   timeval = (timevalue_t) timevalue_INIT(1, -1) ;
   TEST(EINVAL == sleep_sysclock(sysclock_REAL, &timeval)) ;

   // TEST sleepms_sysclock: (works only if scheduling is in bounds !)
   static_assert(sysclock_REAL == 0 && sysclock_MONOTONIC == 1, ) ;
   for (int i = sysclock_REAL; i <= sysclock_MONOTONIC; ++i) {
      pthread_yield() ;
      sysclock_e clock_type = (sysclock_e) i ;
      TEST(0 == time_sysclock(clock_type, &timeval)) ;
      TEST(0 == sleepms_sysclock(clock_type, 10)) ;
      TEST(0 == time_sysclock(clock_type, &timeval2)) ;
      int64_t nanosec = (timeval2.seconds - timeval.seconds) * 1000000000
                      + timeval2.nanosec - timeval.nanosec ;
      TEST(llabs(10000000 /*10msec*/- nanosec) < 1000000/*1msec*/) ;
      TEST(0 == time_sysclock(clock_type, &timeval)) ;
      TEST(0 == sleepms_sysclock(clock_type, 1)) ;
      TEST(0 == time_sysclock(clock_type, &timeval2)) ;
      nanosec = (timeval2.seconds - timeval.seconds) * 1000000000
              + timeval2.nanosec - timeval.nanosec ;
      TEST(llabs(1000000 /*1msec*/- nanosec) < 1000000/*1msec*/) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_time_sysclock()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_timevalue())   goto ONABORT ;
   if (test_clockquery())  goto ONABORT ;
   if (test_clockwait())   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
