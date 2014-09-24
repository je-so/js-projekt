/* title: TimeValue
   Export type for specifing a time value.

   The time value can be either an absolute point in time
   or a positive relative time offset.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/time/timevalue.h
    Header file <TimeValue>.
*/
#ifndef CKERN_TIME_TIMEVALUE_HEADER
#define CKERN_TIME_TIMEVALUE_HEADER


/* typedef: struct timevalue_t
 * Exports <timevalue_t>. */
typedef struct timevalue_t            timevalue_t ;


/* struct: timevalue_t
 * The time value specifies either an aboslute time or a relative time offset.
 * An absolute time value is normally measured in seconds since Epoch,
 * 1970-01-01 00:00:00 +0000 (UTC).
 * But this can differ between different clock types. */
struct timevalue_t {
   /* variable: seconds
    * The seconds counted from some point in time.
    * The value is of signed type to allow computing the difference. */
   int64_t  seconds ;
   /* variable: nanosec
    * The nano seconds counted from some point in time.
    * This value is between 0 and 999999999.
    * The value is of signed type to allow computing the difference. */
   int32_t  nanosec ;
} ;

// group: lifetime

/* define: timevalue_INIT
 * Static initializer. */
#define timevalue_INIT(seconds, nano_secconds)     { seconds, nano_secconds }

// group: query

/* function: isvalid_timevalue
 * Returns true if tv contains a valid value. */
bool isvalid_timevalue(timevalue_t * tv) ;

/* function: diffms_timevalue
 * Returns time difference of (endtv - starttv) in milliseconds.
 * The computed value is not checked for overflow ! */
int64_t diffms_timevalue(timevalue_t * endtv, timevalue_t * starttv) ;


// section: inline implementation

/* define: isvalid_timevalue
 * Implements <timevalue_t.isvalid_timevalue>. */
#define isvalid_timevalue(tv)                (((uint32_t)(tv)->nanosec) <= 999999999)

/* define: diffms_timevalue
 * Implements <timevalue_t.diffms_timevalue>. */
#define diffms_timevalue(endtv, starttv)              \
   ( __extension__ ({                                 \
      timevalue_t * _etv = (endtv) ;                  \
      timevalue_t * _stv = (starttv) ;                \
      (_etv->seconds - _stv->seconds) * 1000          \
      + (_etv->nanosec - _stv->nanosec) / 1000000 ;   \
   }))

#endif
