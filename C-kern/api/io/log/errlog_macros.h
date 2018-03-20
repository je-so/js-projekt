/* title: ErrlogMacros

   Defines error logging macros.

   - Includes text resource file which contains errorlog messages and
     defines <TRACE_ERRLOG> to log them.
   - All logging macros log to the error channel.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/log/errlog_macros.h
    Header file of <ErrlogMacros>.

   file: C-kern/io/log/errlog.c
    Implementation file <Errorlog-Writer>.
*/
#ifndef CKERN_IO_LOG_ERRLOG_MACROS_HEADER
#define CKERN_IO_LOG_ERRLOG_MACROS_HEADER

// forward
struct logbuffer_t;

#include "C-kern/api/io/log/log_macros.h"
#include "C-kern/resource/generated/errlog.h"


// section: Functions

// group: query

/* define: GETBUFFER_ERRLOG
 * See <GETBUFFER_LOG>. */
#define GETBUFFER_ERRLOG(buffer, size)    GETBUFFER_LOG(,log_channel_ERR, buffer, size)

/* define: COMPARE_ERRLOG
 * See <COMPARE_LOG>. */
#define COMPARE_ERRLOG(size, logbuffer)   COMPARE_LOG(,log_channel_ERR, size, logbuffer)

// group: change

/* define: CLEARBUFFER_ERRLOG
 * See <TRUNCATEBUFFER_LOG>. Parameter size is set to 0. */
#define CLEARBUFFER_ERRLOG()              TRUNCATEBUFFER_LOG(,log_channel_ERR, 0)

/* define: FLUSHBUFFER_ERRLOG
 * See <FLUSHBUFFER_LOG>. */
#define FLUSHBUFFER_ERRLOG()              FLUSHBUFFER_LOG(,log_channel_ERR)

/* define: TRUNCATEBUFFER_ERRLOG
 * See <TRUNCATEBUFFER_LOG>. */
#define TRUNCATEBUFFER_ERRLOG(size)       TRUNCATEBUFFER_LOG(,log_channel_ERR, size)

// group: log-text

/* define: PRINTF_ERRLOG
 * Logs a generic printf type format string as error.
 *
 * Parameter:
 * FORMAT     - The format string as in the standard library function printf.
 * ...        - Additional value parameters of the correct type as determined by the <FORMAT>
 *              parameter.
 *
 * Example:
 * > int i ; PRINTF_ERRLOG("%d", i) */
#define PRINTF_ERRLOG(...)                PRINTF_LOG(,log_channel_ERR, log_flags_NONE, 0, __VA_ARGS__)

/* define: PRINTTEXT_ERRLOG
 * Logs an TEXTID from C-kern/resource/errlog.text.
 * The parameters after TEXTID must match the parameters of the resource TEXTID. */
#define PRINTTEXT_ERRLOG(TEXTID, ...)  \
         do {                          \
            PRINTTEXT_LOG(,            \
               log_channel_ERR,        \
               log_flags_NONE,         \
               0, TEXTID ## _ERRLOG,   \
               __VA_ARGS__) ;          \
         }  while(0)

/* define: PRINTTEXT_USER_ERRLOG
 * Logs an TEXTID from C-kern/resource/errlog.text to <log_channel_USERERR>.
 * The parameters after TEXTID must match the parameters of the resource TEXTID. */
#define PRINTTEXT_USER_ERRLOG(TEXTID, ...)   \
         do {                          \
            PRINTTEXT_LOG(,            \
               log_channel_USERERR,    \
               log_flags_NONE,         \
               0, TEXTID ## _ERRLOG,   \
               __VA_ARGS__) ;          \
         }  while(0)

/* define: TRACEEXIT_ERRLOG
 * Logs the abortion of a function and the its corresponding error code.
 * If a function encounters an error from which it cannot recover
 * it should roll back the system to its previous state before it was
 * called and use
 * > TRACEEXIT_ERRLOG(return_error_code)
 * to signal this fact. */
#define TRACEEXIT_ERRLOG(err)             TRACE_ERRLOG(log_flags_LAST, FUNCTION_EXIT, err)

/* define: TRACEEXITFREE_ERRLOG
 * Logs that an error occurred during free_XXX or delete_XXX.
 * This means that not all resources could have been freed
 * only as many as possible. */
#define TRACEEXITFREE_ERRLOG(err)         TRACE_ERRLOG(log_flags_LAST, FUNCTION_EXIT_FREE_RESOURCE, err)

/* define: TRACECALL_ERRLOG
 * Log name of called function and error code.
 * Use this macro to log an error of a called library
 * function which does no logging on its own. */
#define TRACECALL_ERRLOG(fct_name, err) \
         TRACE_ERRLOG(log_flags_NONE, FUNCTION_CALL, fct_name, err)

/* define: TRACEOUTOFMEM_ERRLOG
 * Logs "out of memory" reason for function abort.
 * If a function could not allocate memory of size bytes and therefore aborts
 * with an error code
 * > TRACEOUTOFMEM_ERRLOG(size_of_memory_in_bytes)
 * should be called before <TRACEEXIT_ERRLOG> to document the event leading to an abort. */
#define TRACEOUTOFMEM_ERRLOG(size, err) \
         TRACE_ERRLOG(log_flags_NONE, MEMORY_OUT_OF, size, err)

/* define: TRACESYSCALL_ERRLOG
 * Logs reason of failure and name of called system function.
 * In POSIX compatible systems sys_errno should be set to
 * the C error variable: errno. */
#define TRACESYSCALL_ERRLOG(sys_fctname, err) \
         TRACE_ERRLOG(log_flags_NONE, FUNCTION_SYSCALL, sys_fctname, err)

/* define: TRACE_ERRLOG
 * Logs TEXTID from C-kern/resource/errlog.text and a header.
 * Calls <TRACE_LOG> with default binding and channel set to log_channel_ERR.
 *
 * Parameter:
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - Error text ID from C-kern/resource/errlog.text.
 * ...        - The following parameter are used to parameterize TEXTID. */
#define TRACE_ERRLOG(FLAGS, TEXTID, ...)   \
         TRACE_LOG(, log_channel_ERR, FLAGS, TEXTID ## _ERRLOG, __VA_ARGS__)

/* define: TRACE2_ERRLOG
 * Logs TEXTID from C-kern/resource/errlog.text and a header.
 * Calls <TRACE2_LOG> with default binding and channel set to log_channel_ERR.
 *
 * Parameter:
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - Error text ID from C-kern/resource/errlog.text.
 * funcname   - Name of function - used to describe error position.
 * filename   - Name of source file - used to describe error position.
 * linenr     - Number of current source line - used to describe error position.
 * ...        - The following parameter are used to parameterize TEXTID. */
#define TRACE2_ERRLOG(FLAGS, TEXTID, funcname, filename, linenr, ...) \
         TRACE2_LOG(, log_channel_ERR, FLAGS, TEXTID ## _ERRLOG, funcname, filename, linenr, __VA_ARGS__)

/* define: TRACE_NOARG_ERRLOG
 * Logs TEXTID from C-kern/resource/errlog.text and a header.
 * Use this macro to log any language specific text which needs no additional parameters.
 *
 * Parameter:
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - Error text ID from C-kern/resource/errlog.text. */
#define TRACE_NOARG_ERRLOG(FLAGS, TEXTID) \
         TRACE_NOARG_LOG(, log_channel_ERR, FLAGS, TEXTID ## _ERRLOG)

/* define: TRACE_PRINTF_ERRLOG
 * Logs header and prints format string with additional arguments.
 * Parameter FLAGS carries additional flags to control the logging process - see <log_flags_e>.
 * The next argumen after FLAGS is mandatory and is the printf format string. */
#define TRACE_PRINTF_ERRLOG(FLAGS, ...) \
         do {                             \
            log_header_t _header =        \
               log_header_INIT(__func__,  \
                  __FILE__, __LINE__);    \
            PRINTF_LOG(,                  \
               log_channel_ERR, FLAGS,    \
               &_header, __VA_ARGS__);    \
         }  while(0)

// group: log-variables

/* define: PRINTARRAYFIELD_ERRLOG
 * Log value of variable stored in array at offset i.
 * The logged text is "array[i]=value".
 *
 * Parameter:
 * format   - Type of the variable as string in printf format. Use "s" for C strings.
 *            Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * arrname  - The name of the array.
 * index    - The index of the array entry whose value is to be logged.
 *
 * Example:
 * This code logs "names[0]=Jo\n" and "names[1]=Jane\n".
 * > const char * names[] = { "Jo", "Jane" } ;
 * > for(int i = 0; i < 2; ++i) { PRINTARRAYFIELD_ERRLOG("s", names, i) ; } */
#define PRINTARRAYFIELD_ERRLOG(format, arrname, index)   \
         PRINTARRAYFIELD_LOG(,log_channel_ERR, format, arrname, index)

/* define: PRINTCSTR_ERRLOG
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; PRINTCSTR_ERRLOG(name) ; */
#define PRINTCSTR_ERRLOG(varname)         PRINTCSTR_LOG(,log_channel_ERR, varname)

/* define: PRINTINT_ERRLOG
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; PRINTINT_ERRLOG(max) ; */
#define PRINTINT_ERRLOG(varname)          PRINTINT_LOG(,log_channel_ERR, varname)

/* define: PRINTINT64_ERRLOG
 * Log "name=value" of int variable.
 * Example:
 * > const int64_t min = 100 ; PRINTINT64_ERRLOG(min) ; */
#define PRINTINT64_ERRLOG(varname)        PRINTINT64_LOG(,log_channel_ERR, varname)

/* define: PRINTSIZE_ERRLOG
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; PRINTSIZE_ERRLOG(maxsize) ; */
#define PRINTSIZE_ERRLOG(varname)         PRINTSIZE_LOG(,log_channel_ERR, varname)

/* define: PRINTUINT8_ERRLOG
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; PRINTUINT8_ERRLOG(limit) ; */
#define PRINTUINT8_ERRLOG(varname)        PRINTUINT8_LOG(,log_channel_ERR, varname)

/* define: PRINTUINT16_ERRLOG
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; PRINTUINT16_ERRLOG(limit) ; */
#define PRINTUINT16_ERRLOG(varname)       PRINTUINT16_LOG(,log_channel_ERR, varname)

/* define: PRINTUINT32_ERRLOG
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; PRINTUINT32_ERRLOG(max) ; */
#define PRINTUINT32_ERRLOG(varname)       PRINTUINT32_LOG(,log_channel_ERR, varname)

/* define: PRINTUINT64_ERRLOG
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; PRINTUINT64_ERRLOG(max) ; */
#define PRINTUINT64_ERRLOG(varname)       PRINTUINT64_LOG(,log_channel_ERR, varname)

/* define: PRINTPTR_ERRLOG
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; PRINTPTR_ERRLOG(ptr) ; */
#define PRINTPTR_ERRLOG(varname)          PRINTPTR_LOG(,log_channel_ERR, varname)

/* define: PRINTDOUBLE_ERRLOG
 * Log "name=value" of double or float variable.
 * Example:
 * > const double d = 1.234 ; PRINTDOUBLE_ERRLOG(d) ; */
#define PRINTDOUBLE_ERRLOG(varname)       PRINTDOUBLE_LOG(,log_channel_ERR, varname)

#endif
