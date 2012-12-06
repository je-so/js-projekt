/* title: SystemClock
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

   file: C-kern/api/time/sysclock.h
    Header file <SystemClock>.

   file: C-kern/platform/Linux/time/sysclock.c
    Implementation file <SystemClock Linux>.
*/
#ifndef CKERN_TIME_SYSCLOCK_HEADER
#define CKERN_TIME_SYSCLOCK_HEADER

// forward
struct timevalue_t ;

/* enums: sysclock_e
 * This value selects the clock type.
 * It specifies the properties of a clock.
 * For example <ostimer_t> can use any clock type to measure time.
 *
 * sysclock_REAL      - System-wide real-time clock.
 * sysclock_MONOTONIC - Clock that cannot be set and represents monotonic time since some
 *                      unspecified starting point. */
enum sysclock_e {
    sysclock_REAL
   ,sysclock_MONOTONIC
} ;

typedef enum sysclock_e                sysclock_e ;



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_time_sysclock
 * Unittest for clock functions. */
int unittest_time_sysclock(void) ;
#endif


/* struct: sysclock
 * Interface to hardware system clock. */

// group: query

/* function: resolution_sysclock
 * Returns the timer resolution of the underlying clock. */
int resolution_sysclock(sysclock_e clock_type, /*out*/struct timevalue_t * resolution) ;

/* function: time_sysclock
 * Returns the absolute time value of the underlying clock. */
int time_sysclock(sysclock_e clock_type, /*out*/struct timevalue_t * clock_time) ;

// group: wait

/* function: sleep_sysclock
 * Sleeps relative_time seconds (+nanoseconds).
 * The time is obtained from clock *clock_type* (see <sysclock_e>). */
int sleep_sysclock(sysclock_e clock_type, const struct timevalue_t * relative_time) ;

/* function: sleepms_sysclock
 * Sleeps *millisec* milliseconds.
 * The time is obtained from clock *clock_type* (see <sysclock_e>). */
int sleepms_sysclock(sysclock_e clock_type, uint32_t millisec) ;

#endif
