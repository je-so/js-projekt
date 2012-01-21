/* title: IOTimer
   Offers a timer which signals timeout via input ready state
   of <filedescr_t>. This allows you to handle timers like any
   other io-device.

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
#ifndef CKERN_IO_IOTIMER_HEADER
#define CKERN_IO_IOTIMER_HEADER

#include "C-kern/api/time/timespec.h"

/* typedef: iotimer_t
 * Exports <iotimer_t> as alias for <filedescr_t>.
 * See <filedescr_t> or <sys_filedescr_t>. */
typedef sys_filedescr_t                iotimer_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_iotimer
 * Unittest for <iotimer_t>. */
extern int unittest_io_iotimer(void) ;
#endif


// section: iotimer

// group: lifetime

/* define: iotimer_INIT_FREEABLE
 * Static initializer. */
#define iotimer_INIT_FREEABLE          sys_filedescr_INIT_FREEABLE

/* function: init_iotimer
 * Allocates a new system timer.
 * See <timeclock_e> for the different clocks the timer can use to measure time. */
extern int init_iotimer(/*out*/iotimer_t * timer, timeclock_e clock_type) ;

/* function: free_iotimer
 * Frees any resources associated with the timer.
 * If the timer is running it will be stopped.*/
extern int free_iotimer(iotimer_t * timer) ;

// group: start/stop

/* function: start_iotimer
 * This function starts (arms) a one shot timer.
 * The started timer fires (expires) only once after *relative_time* seconds (+nanoseconds).
 * After the timer has expired it is stopped. */
extern int start_iotimer(iotimer_t timer, timevalue_t * relative_time) ;

/* function: startinterval_iotimer
 * This function starts (arms) a periodic timer.
 * The started timer fires (expires) at regular intervals after *interval_time* seconds (+nanoseconds).
 * After the timer has expired it is restarted with *interval_time*. */
extern int startinterval_iotimer(iotimer_t timer, timevalue_t * interval_time) ;

/* function: stop_iotimer
 * Stops a timer. If a timer is stopped the remaining time
 * and the expiration count are reset to 0. */
extern int stop_iotimer(iotimer_t timer) ;

/* function: wait_iotimer
 * Waits until timer expires. It returns EINVAL if a stopped timer is waited for.
 * After a successfull call to this function calling expirationcount_iotimer will return
 * a value != 0. If the timer has already expired (expirationcount_iotimer return != 0)
 * calling this function has no effect. It will return immediately.
 *
 * Event:
 * The timer is also a file descriptor. If the timer expires this is signaled as
 * file descriptor being readable. */
extern int wait_iotimer(iotimer_t timer) ;

// group: query

/* function: remainingtime_iotimer
 * Returns the remaining time until the timer expires the next time.
 * This value is a relative time value. */
extern int remainingtime_iotimer(iotimer_t timer, timevalue_t * remaining_time) ;

/* function: expirationcount_iotimer
 * Returns the number of times the timer has expired. After reading it is reset to 0.
 * If timer which is not an interval timer expires only once and after that it is considered stopped. */
extern int expirationcount_iotimer(iotimer_t timer, /*out*/uint64_t * expiration_count) ;


#endif
