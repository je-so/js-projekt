/* title: Timeclock
   Interface to read (or set) the system clock.

   You need to include "C-kern/api/time/timevalue.h" before you can use the interface.

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

// forward
struct timevalue_t ;

/* enums: timeclock_e
 * This value selects the clock type.
 * It specifies the properties of a clock.
 * For example <ostimer_t> can use any clock type to measure time.
 *
 * timeclock_REAL      - System-wide real-time clock.
 * timeclock_MONOTONIC - Clock that cannot be set and represents monotonic time since some
 *                       unspecified starting point. */
enum timeclock_e {
    timeclock_REAL
   ,timeclock_MONOTONIC
} ;

typedef enum timeclock_e               timeclock_e ;



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_time_clock
 * Unittest for clock functions. */
int unittest_time_clock(void) ;
#endif


// section: timeclock

// group: query

/* function: resolution_timeclock
 * Returns the timer resolution of the underlying clock. */
int resolution_timeclock(timeclock_e clock_type, /*out*/struct timevalue_t * resolution) ;

/* function: time_timeclock
 * Returns the absolute time value of the underlying clock. */
int time_timeclock(timeclock_e clock_type, /*out*/struct timevalue_t * clock_time) ;

// group: wait

/* function: sleep_timeclock
 * Sleeps relative_time seconds (+nanoseconds).
 * The time is obtained from clock *clock_type* (see <timeclock_e>). */
int sleep_timeclock(timeclock_e clock_type, const struct timevalue_t * relative_time) ;

/* function: sleepms_timeclock
 * Sleeps *millisec* milliseconds.
 * The time is obtained from clock *clock_type* (see <timeclock_e>). */
int sleepms_timeclock(timeclock_e clock_type, uint32_t millisec) ;

#endif
