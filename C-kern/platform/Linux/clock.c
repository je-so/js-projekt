/* title: Timeclock Linux
   Implements <Timeclock>.

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

   file: C-kern/api/time/clock.h
    Header file <Timeclock>.

   file: C-kern/platform/Linux/clock.c
    Implementation file <Timeclock Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/time/clock.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/thread.h"
#endif


// section: timeclock

// group: helper

static inline void compiletime_(void)
{
   static_assert( CLOCK_REALTIME  == timeclock_REAL, "timeclock_e is compatible to clockid_t" ) ;
   static_assert( CLOCK_MONOTONIC == timeclock_MONOTONIC, "timeclock_e is compatible to clockid_t" ) ;
}

/* define: convertclockid
 * Converts <timeclock_e> into <clockid_t>. */
#define convertclockid(/*timeclock_e*/clock_type) \
   ((clockid_t) (clock_type))

// group: query

int resolution_timeclock(timeclock_e clock_type, /*out*/timevalue_t * resolution)
{
   int err ;
   struct timespec   tspec ;
   clockid_t         clockid = convertclockid(clock_type) ;

   if (clock_getres(clockid, &tspec)) {
      err = errno ;
      PRINTSYSERR_LOG("clock_getres", err) ;
      PRINTINT_LOG(clock_type) ;
      goto ONABORT ;
   }

   resolution->seconds = tspec.tv_sec ;
   resolution->nanosec = tspec.tv_nsec ;

   return 0 ;
ONABORT:
   PRINTABORT_LOG(err) ;
   return err ;
}

int time_timeclock(timeclock_e clock_type, /*out*/timevalue_t * clock_time)
{
   int err ;
   struct timespec   tspec ;
   clockid_t         clockid = convertclockid(clock_type) ;

   if (clock_gettime(clockid, &tspec)) {
      err = errno ;
      PRINTSYSERR_LOG("clock_gettime", err) ;
      PRINTINT_LOG(clock_type) ;
      goto ONABORT ;
   }

   clock_time->seconds = tspec.tv_sec ;
   clock_time->nanosec = tspec.tv_nsec ;

   return 0 ;
ONABORT:
   PRINTABORT_LOG(err) ;
   return err ;
}

int sleep_timeclock(timeclock_e clock_type, timevalue_t * relative_time)
{
   int err ;
   struct timespec   tspec   = { .tv_sec = relative_time->seconds, .tv_nsec = relative_time->nanosec } ;
   clockid_t         clockid = convertclockid(clock_type) ;

   while (clock_nanosleep(clockid, 0, &tspec, &tspec)) {
      err = errno ;
      if (EINTR == err) continue ;
      PRINTSYSERR_LOG("clock_gettime", err) ;
      PRINTINT_LOG(clock_type) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   PRINTABORT_LOG(err) ;
   return err ;
}

int sleepms_timeclock(timeclock_e clock_type, uint32_t millisec)
{
   int err ;
   struct timespec   tspec   = { .tv_sec = (int32_t) (millisec/1000), .tv_nsec = (int)(millisec%1000) * 1000000 } ;
   clockid_t         clockid = convertclockid(clock_type) ;

   while (clock_nanosleep(clockid, 0, &tspec, &tspec)) {
      err = errno ;
      if (EINTR == err) continue ;
      PRINTSYSERR_LOG("clock_gettime", err) ;
      PRINTINT_LOG(clock_type) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   PRINTABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_query(void)
{
   timevalue_t timeval ;
   timevalue_t timeval2 ;

   // TEST: resolution_timeclock (at least 1ms)
   TEST(0 == resolution_timeclock(timeclock_REAL, &timeval)) ;
   TEST(0 == timeval.seconds) ;
   TEST(1000000 >= timeval.nanosec) ;
   TEST(0 == resolution_timeclock(timeclock_MONOTONIC, &timeval2)) ;
   TEST(0 == timeval2.seconds) ;
   TEST(1000000  >= timeval2.nanosec) ;
   TEST(timeval.nanosec == timeval2.nanosec) ;

   // TEST: time_timeclock (works only if scheduling is in bounds !)
   static_assert(timeclock_REAL == 0 && timeclock_MONOTONIC == 1, ) ;
   for(int i = timeclock_REAL; i <= timeclock_MONOTONIC; ++i) {
      pthread_yield() ;
      timeclock_e clock_type = (timeclock_e) i ;
      TEST(0 == time_timeclock(clock_type, &timeval)) ;
      sleepms_thread(10) ;
      TEST(0 == time_timeclock(clock_type, &timeval2)) ;
      uint64_t nanosec = (uint64_t) (timeval2.seconds - timeval.seconds) * 1000000000
                       + (uint64_t) timeval2.nanosec - (uint64_t) timeval.nanosec ;
      TEST( llabs(10000000 /*10msec*/- (int64_t)nanosec) < 1000000/*1msec*/) ;
      pthread_yield() ;
      TEST(0 == time_timeclock(clock_type, &timeval)) ;
      sleepms_thread(1) ;
      TEST(0 == time_timeclock(clock_type, &timeval2)) ;
      nanosec = (uint64_t) (timeval2.seconds - timeval.seconds) * 1000000000
              + (uint64_t) timeval2.nanosec - (uint64_t) timeval.nanosec ;
      TEST( llabs(1000000 /*1msec*/- (int64_t)nanosec) < 1000000/*1msec*/) ;
   }

   // TEST: sleep_timeclock (works only if scheduling is in bounds !)
   static_assert(timeclock_REAL == 0 && timeclock_MONOTONIC == 1, ) ;
   for(int i = timeclock_REAL; i <= timeclock_MONOTONIC; ++i) {
      pthread_yield() ;
      timeclock_e clock_type = (timeclock_e) i ;
      TEST(0 == time_timeclock(clock_type, &timeval)) ;
      timeval2.seconds = 0 ; timeval2.nanosec = 10000000 /*10msec*/;
      TEST(0 == sleep_timeclock(clock_type, &timeval2)) ;
      TEST(0 == time_timeclock(clock_type, &timeval2)) ;
      uint64_t nanosec = (uint64_t) (timeval2.seconds - timeval.seconds) * 1000000000
                       + (uint64_t) timeval2.nanosec - (uint64_t) timeval.nanosec ;
      TEST( llabs(10000000 /*10msec*/- (int64_t)nanosec) < 1000000/*1msec*/) ;
      pthread_yield() ;
      TEST(0 == time_timeclock(clock_type, &timeval)) ;
      timeval2.seconds = 0 ; timeval2.nanosec = 1000000 /*1msec*/;
      TEST(0 == sleep_timeclock(clock_type, &timeval2)) ;
      TEST(0 == time_timeclock(clock_type, &timeval2)) ;
      nanosec = (uint64_t) (timeval2.seconds - timeval.seconds) * 1000000000
              + (uint64_t) timeval2.nanosec - (uint64_t) timeval.nanosec ;
      TEST( llabs(1000000 /*1msec*/- (int64_t)nanosec) < 1000000/*1msec*/) ;
   }

   // TEST: sleepms_timeclock (works only if scheduling is in bounds !)
   static_assert(timeclock_REAL == 0 && timeclock_MONOTONIC == 1, ) ;
   for(int i = timeclock_REAL; i <= timeclock_MONOTONIC; ++i) {
      pthread_yield() ;
      timeclock_e clock_type = (timeclock_e) i ;
      TEST(0 == time_timeclock(clock_type, &timeval)) ;
      TEST(0 == sleepms_timeclock(clock_type, 10)) ;
      TEST(0 == time_timeclock(clock_type, &timeval2)) ;
      uint64_t nanosec = (uint64_t) (timeval2.seconds - timeval.seconds) * 1000000000
                       + (uint64_t) timeval2.nanosec - (uint64_t) timeval.nanosec ;
      TEST( llabs(10000000 /*10msec*/- (int64_t)nanosec) < 1000000/*1msec*/) ;
      TEST(0 == time_timeclock(clock_type, &timeval)) ;
      TEST(0 == sleepms_timeclock(clock_type, 1)) ;
      TEST(0 == time_timeclock(clock_type, &timeval2)) ;
      nanosec = (uint64_t) (timeval2.seconds - timeval.seconds) * 1000000000
              + (uint64_t) timeval2.nanosec - (uint64_t) timeval.nanosec ;
      TEST( llabs(1000000 /*1msec*/- (int64_t)nanosec) < 1000000/*1msec*/) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_time_clock()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_query())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
