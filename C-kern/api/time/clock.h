/* title: Timeclock
   Interface to read (or set) the system clock.

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
#ifndef CKERN_TIME_CLOCK_HEADER
#define CKERN_TIME_CLOCK_HEADER

#include "C-kern/api/time/timespec.h"


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_time_clock
 * Unittest for clock functions. */
extern int unittest_time_clock(void) ;
#endif


// section: timeclock

// group: query

/* function: resolution_timeclock
 * Returns the timer resolution of the underlying clock. */
extern int resolution_timeclock(timeclock_e clock_type, /*out*/timevalue_t * resolution) ;

/* function: time_timeclock
 * Returns the absolute time value of the underlying clock. */
extern int time_timeclock(timeclock_e clock_type, /*out*/timevalue_t * clock_time) ;

// group: wait

/* function: sleep_timeclock
 * Sleeps relative_time seconds (+nanoseconds).
 * The time is obtained from clock *clock_type* (see <timeclock_e>). */
extern int sleep_timeclock(timeclock_e clock_type, timevalue_t * relative_time) ;

/* function: sleepms_timeclock
 * Sleeps *millisec* milliseconds.
 * The time is obtained from clock *clock_type* (see <timeclock_e>). */
extern int sleepms_timeclock(timeclock_e clock_type, uint32_t millisec) ;

#endif
