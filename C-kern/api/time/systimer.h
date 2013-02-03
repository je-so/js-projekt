/* title: SysTimer
   Offers a timer which signals timeout via input ready state of <sys_file_t>.
   This allows you to handle timers like any other io-device.

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

   file: C-kern/api/time/systimer.h
    Header file <SysTimer>.

   file: C-kern/platform/Linux/time/systimer.c
    Linux implementation file <SysTimer Linux>.
*/
#ifndef CKERN_TIME_SYSTIMER_HEADER
#define CKERN_TIME_SYSTIMER_HEADER

#include "C-kern/api/time/sysclock.h"

// forward
struct timevalue_t ;


/* typedef: systimer_t
 * Exports <systimer_t> as alias for <sys_file_t>. */
typedef sys_file_t                     systimer_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_time_systimer
 * Unittest for <systimer_t>. */
int unittest_time_systimer(void) ;
#endif


/* struct: systimer_t
 * Implements system timer to measure periods of time. */

// group: lifetime

/* define: systimer_INIT_FREEABLE
 * Static initializer. */
#define systimer_INIT_FREEABLE         sys_file_INIT_FREEABLE

/* function: init_systimer
 * Allocates a new system timer.
 * See <sysclock_e> for the different clocks the timer can use to measure time. */
int init_systimer(/*out*/systimer_t * timer, sysclock_e clock_type) ;

/* function: free_systimer
 * Frees any resources associated with the timer.
 * If the timer is running it will be stopped.*/
int free_systimer(systimer_t * timer) ;

// group: measure

/* function: start_systimer
 * This function starts (arms) a one shot timer.
 * The started timer fires (expires) only once after *relative_time* seconds (+nanoseconds).
 * After the timer has expired it is stopped. */
int start_systimer(systimer_t timer, struct timevalue_t * relative_time) ;

/* function: startinterval_systimer
 * This function starts (arms) a periodic timer.
 * The started timer fires (expires) at regular intervals after *interval_time* seconds (+nanoseconds).
 * After the timer has expired it is restarted with *interval_time*. */
int startinterval_systimer(systimer_t timer, struct timevalue_t * interval_time) ;

/* function: stop_systimer
 * Stops a timer. If a timer is stopped the remaining time
 * and the expiration count are reset to 0. */
int stop_systimer(systimer_t timer) ;

// group: wait

/* function: wait_systimer
 * Waits until timer expires. It returns EINVAL if a stopped timer is waited for.
 * After a successfull call to this function calling expirationcount_systimer will return
 * a value != 0. If the timer has already expired (expirationcount_systimer return != 0)
 * calling this function has no effect. It will return immediately.
 *
 * Event:
 * The timer is also a file descriptor. If the timer expires this is signaled as
 * file descriptor being readable. */
int wait_systimer(systimer_t timer) ;

// group: query

/* function: remainingtime_systimer
 * Returns the remaining time until the timer expires the next time.
 * This value is a relative time value. */
int remainingtime_systimer(systimer_t timer, struct timevalue_t * remaining_time) ;

/* function: expirationcount_systimer
 * Returns the number of times the timer has expired. After reading it is reset to 0.
 * If timer which is not an interval timer expires only once and after that it is considered stopped. */
int expirationcount_systimer(systimer_t timer, /*out*/uint64_t * expiration_count) ;


#endif
