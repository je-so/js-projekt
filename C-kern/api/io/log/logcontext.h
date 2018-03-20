/* title: LogContext

   TODO: describe module interface

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2018 Jörg Seebohn

   file: C-kern/api/io/log/logcontext.h
    Header file <LogContext>.

   file: C-kern/io/log/logcontext.c
    Implementation file <LogContext impl>.
*/
#ifndef CKERN_IO_LOG_LOGCONTEXT_HEADER
#define CKERN_IO_LOG_LOGCONTEXT_HEADER

// === exported types
struct logcontext_t;
struct logcontext_error_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_log_logcontext
 * Test <logcontext_t> functionality. */
int unittest_io_log_logcontext(void);
#endif

/* struct: logcontext_error_t
 * Stores table of error strings. Each string describing a single error number. */
typedef struct logcontext_error_t {
   /* variable: offsets
    * Table with offset values into <strings>.
    * The tables length is equal to 512.
    * The entries 0..<maxsyserrno_logcontext> are system error codes.
    * The entries <maxsyserrno_logcontext>+1..255 are "unknown error" codes.
    * The entries 256..? are application error codes.
    * The entries ?..511 are "unknown error" codes. */
   const uint16_t*   offsets;
   /* variable: strings
    * String table with system error descriptions in english (german ... see KONFIG_LANG)
    * Last byte of string is a '\0' byte (C string). */
   const uint8_t *   strings;
} logcontext_error_t;

// group: lifetime

/* define: logcontext_error_FREE
 * Static initializer. */
#define logcontext_error_FREE \
         { 0, 0 }

/* define: logcontext_error_INIT
 * Static initializer.
 *
 * Parameter:
 * offsets - Pointer to offset table of type <logcontext_offsets_t>.
 * strings - Pointer to single contatenated string buffer containing all error descriptions. */
#define logcontext_error_INIT(offsets, strings) \
         { offsets, strings }


/* struct: logcontext_t
 * Stores additional information needed during logging. */
typedef struct logcontext_t {
   // group: public
   /* variable: err
    * String table. Every string describes another error code. */
   logcontext_error_t   err;
} logcontext_t;

// group: lifetime

/* define: logcontext_FREE
 * Static initializer. */
#define logcontext_FREE \
         { logcontext_error_FREE }

/* function: initstatic_logcontext
 * TODO: Describe Initializes object. */
void initstatic_logcontext(/*out*/logcontext_t* lc);

/* function: freestatic_logcontext
 * TODO: Describe Initializes object. */
void freestatic_logcontext(logcontext_t* lc);

/* function: init_logcontext
 * TODO: Describe Initializes object. */
int init_logcontext(/*out*/logcontext_t* lc);

/* function: free_logcontext
 * TODO: Describe Frees all associated resources. */
int free_logcontext(logcontext_t* lc);

// group: query

/* function: isfree_logcontext
 * Returns true if *lc == logcontext_FREE. */
int isfree_logcontext(const logcontext_t* lc);

/* function: maxsyserrno_logcontext
 * Returns the number of operating system error entries in <err>.
 * The maximum number of supported entries is lengthof(logcontext_offsets_t). */
uint16_t maxsyserrno_logcontext(void);

/* function: errstr_logcontext
 * TODO: */
const uint8_t* errstr_logcontext(const logcontext_t* lc, int err);

/* function: errstr2_logcontext
 * TODO: */
static inline const uint8_t* errstr2_logcontext(const logcontext_t* lc, unsigned err);

// group: update



// section: inline implementation

/* define: errstr_logcontext
 * Implements <logcontext_t.errstr_logcontext>. */
#define errstr_logcontext(lc,err) _Generic((err), int: errstr2_logcontext(lc,(unsigned)err), unsigned: errstr2_logcontext(lc,(unsigned)err))

/* define: errstr2_logcontext
 * Implements <logcontext_t.errstr2_logcontext>. */
static inline const uint8_t* errstr2_logcontext(const logcontext_t* lc, unsigned err)
{
         return   lc->err.strings
                  + lc->err.offsets[ err > 511 ? 511 : err ];
}

/* define: initstatic_logcontext
 * Implements <logcontext_t.initstatic_logcontext>. */
#define initstatic_logcontext(lc) \
         ((void)init_logcontext(lc))

/* define: freestatic_logcontext
 * Implements <logcontext_t.freestatic_logcontext>. */
#define freestatic_logcontext(lc) \
         ((void)free_logcontext(lc))

#endif
