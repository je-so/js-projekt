/* title: LogContext

   Helps to manage additional information sources needed during logging.
   This access to information during logging is possible even
   if <maincontext_t> / <threadcontext_t> is not set-up.

   Therrefore every logging object must link to either an initial
   <logcontext_t> or the default one accessed by <maincontex_t.logcontext_maincontext>.

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
 * Initializes logcontext lc. This contructor is used
 * to set-up an initial logging object before the main
 * and thread context is initialized. */
void initstatic_logcontext(/*out*/logcontext_t* lc);

/* function: freestatic_logcontext
 * Frees lc which has been initialized by a call to <initstatic_logcontext>. */
void freestatic_logcontext(logcontext_t* lc);

/* function: init_logcontext
 * Initializes lc with the default logcontext.
 * This constructor is called from <maincontext_t.init_maincontext>.
 * This context is shared between all threads. s*/
int init_logcontext(/*out*/logcontext_t* lc);

/* function: free_logcontext
 * Frees lc which has been initialized by a call to <init_logcontext>. */
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
 * Converts error code err into a textual error description.
 * This string is localized.
 * Error numbers 1..255 are operating system error codes.
 * Error numbers 256..511 are application specific error codes. */
const uint8_t* errstr_logcontext(const logcontext_t* lc, int err);

/* function: errstr2_logcontext
 * Same as errstr_logcontext except that it accepts an unsigned instead of int type. */
static inline const uint8_t* errstr2_logcontext(const logcontext_t* lc, unsigned err);



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
