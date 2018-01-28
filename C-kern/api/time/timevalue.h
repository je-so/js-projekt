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

   file: C-kern/time/timevalue.c
    Implementation file <TimeValue impl>.
*/
#ifndef CKERN_TIME_TIMEVALUE_HEADER
#define CKERN_TIME_TIMEVALUE_HEADER


// === exported types
struct timevalue_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_time_timevalue
 * Test <timevalue_t> functionality. */
int unittest_time_timevalue(void);
#endif


/* struct: timevalue_t
 * The time value specifies either an aboslute time or a relative time offset.
 * An absolute time value is normally measured in seconds since Epoch,
 * 1970-01-01 00:00:00 +0000 (UTC).
 * But this can differ between different clock types. */
typedef struct timevalue_t {
   /* variable: seconds
    * The seconds counted from some point in time.
    * The value is of signed type to allow computing the difference. */
   int64_t  seconds;
   /* variable: nanosec
    * The nano seconds counted from some point in time.
    * This value is between 0 and 999999999.
    * The value is of signed type to allow computing the difference. */
   int32_t  nanosec;
} timevalue_t;

// group: lifetime

/* define: timevalue_INIT
 * Static initializer. */
#define timevalue_INIT(seconds, nano_secconds) \
         { seconds, nano_secconds }

// group: query

/* function: isvalid_timevalue
 * Returns true if tv contains a valid value. */
static inline int isvalid_timevalue(const timevalue_t* tv);

/* function: diffms_timevalue
 * Returns time difference of (endtv - starttv) in milliseconds.
 * The computed value is not checked for overflow ! */
static inline int64_t diffms_timevalue(timevalue_t* endtv, timevalue_t* starttv);

/* function: diffus_timevalue
 * Returns time difference of (endtv - starttv) in microseconds.
 * The computed value is not checked for overflow ! */
static inline int64_t diffus_timevalue(timevalue_t* endtv, timevalue_t* starttv);

// group: generic

/* function: cast_timevalue
 * Castet Zeiger obj in einen Zeiger auf Typ <timevalue_t>.
 * Der Cast funktioniert nur, wenn obj auf eine Struktur mit
 * zwei Feldern namens seconds und nanosec mit dem korrekten Typ zeigt. */
timevalue_t* cast_timevalue(void* obj);



// section: inline implementation

#define cast_timevalue(obj) \
         ( __extension__ ({                              \
            static_assert(                               \
               sizeof(((timevalue_t*)0)->seconds)        \
               == sizeof((obj)->seconds)                 \
               && (typeof((obj)->seconds))-1 < 0         \
               && sizeof(((timevalue_t*)0)->nanosec)     \
                  == sizeof((obj)->nanosec)              \
               && (typeof((obj)->nanosec))-1 < 0         \
               && offsetof(timevalue_t, nanosec)         \
                  == (size_t)(                           \
                        (uint8_t*)&((obj)->nanosec)      \
                        - (uint8_t*)&((obj)->seconds)),  \
               "*obj compatible with timevalue_t");      \
            (timevalue_t*)&(obj)->seconds;               \
         }))

/* define: diffms_timevalue
 * Implements <timevalue_t.diffms_timevalue>. */
static inline int64_t diffms_timevalue(timevalue_t* endtv, timevalue_t* starttv)
{
            return (endtv->seconds - starttv->seconds) * 1000
                  + (endtv->nanosec - starttv->nanosec) / 1000000;
}

/* define: diffus_timevalue
 * Implements <timevalue_t.diffus_timevalue>. */
static inline int64_t diffus_timevalue(timevalue_t* endtv, timevalue_t* starttv)
{
            return (endtv->seconds - starttv->seconds) * 1000000
                  + (endtv->nanosec - starttv->nanosec) / 1000;
}

/* define: isvalid_timevalue
 * Implements <timevalue_t.isvalid_timevalue>. */
static inline int isvalid_timevalue(const timevalue_t* tv)
{
         return tv->seconds >= 0 && (uint32_t)tv->nanosec <= 999999999;
}

#endif
