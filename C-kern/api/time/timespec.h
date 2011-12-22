/* title: Time-Specifications
   Export types for specifing time values or the type of clock.

   The time value can be either an absolute point in time
   or a relative time offset.

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

   file: C-kern/api/time/timespec.h
    Header file <Time-Specifications>.
*/
#ifndef CKERN_TIME_TIMESPEC_HEADER
#define CKERN_TIME_TIMESPEC_HEADER


/* typedef: struct timevalue_t
 * Exports <timevalue_t>. */
typedef struct timevalue_t            timevalue_t ;

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


/* struct: timevalue_t
 * The time value specifies either an aboslute time or a relative time offset.
 * An absolute time value is normally measured in seconds since Epoch,
 * 1970-01-01 00:00:00 +0000 (UTC).
 * But this can differ between different clock types. */
struct timevalue_t {
   time_t   seconds ;
   int32_t  nanosec ;
} ;

// group: query

/* function: isvalid_timevalue
 * Returns true if tv contains a valid value. */
extern bool isvalid_timevalue(timevalue_t * tv) ;


// section: inline implementation

/* define: isvalid_timevalue
 * Implements <timevalue_t.isvalid_timevalue>. */
#define isvalid_timevalue(tv)          (((tv)->seconds >= 0) && (((uint32_t)(tv)->nanosec) <= 999999999))

#endif
