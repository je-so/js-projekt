/* title: ErrlogMacros

   Defines error logging macros.

   - Includes text resource file which contains errorlog messages and
     defines <TRACE_ERRLOG> to log them.
   - All logging macros log to the error channel.

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

   file: C-kern/api/io/writer/log/errlog_macros.h
    Header file of <ErrlogMacros>.

   file: C-kern/io/writer/errlog.c
    Implementation file <Errorlog-Writer>.
*/
#ifndef CKERN_IO_WRITER_LOG_ERRLOG_MACROS_HEADER
#define CKERN_IO_WRITER_LOG_ERRLOG_MACROS_HEADER

// forward
struct logbuffer_t ;

#include "C-kern/api/io/writer/log/log_macros.h"
#include "C-kern/resource/generated/errlog.h"


// section: Functions

// group: query

/* define: GETBUFFER_ERRLOG
 * See <GETBUFFER_LOG>. */
#define GETBUFFER_ERRLOG(buffer, size)    GETBUFFER_LOG(log_channel_ERR, buffer, size)

// group: change

/* define: CLEARBUFFER_ERRLOG
 * See <CLEARBUFFER_LOG>. */
#define CLEARBUFFER_ERRLOG()              CLEARBUFFER_LOG(log_channel_ERR)

/* define: FLUSHBUFFER_LOG
 * See <FLUSHBUFFER_LOG>. */
#define FLUSHBUFFER_ERRLOG()              FLUSHBUFFER_LOG(log_channel_ERR)

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
#define PRINTF_ERRLOG(...)                PRINTF_LOG(log_channel_ERR, log_flags_NONE, 0, __VA_ARGS__)

/* define: TRACEABORT_ERRLOG
 * Logs the abortion of a function and the its corresponding error code.
 * If a function encounters an error from which it cannot recover
 * it should roll back the system to its previous state before it was
 * called and use
 * > TRACEABORT_ERRLOG(return_error_code)
 * to signal this fact. */
#define TRACEABORT_ERRLOG(err)            TRACE_NOARG_ERRLOG(log_flags_END, FUNCTION_ABORT, err)

/* define: TRACEABORTFREE_ERRLOG
 * Logs that an error occurred during free_XXX or delete_XXX.
 * This means that not all resources could have been freed
 * only as many as possible. */
#define TRACEABORTFREE_ERRLOG(err)        TRACE_NOARG_ERRLOG(log_flags_END, FUNCTION_ABORT_FREE, err)

/* define: TRACECALL_ERRLOG
 * Logs reason of failure and name of called app function.
 * Use this function to log an error in a function which calls a library
 * function which does no logging on its own.
 *
 * TODO: Support own error IDs */
#define TRACECALL_ERRLOG(fct_name, err)   \
         TRACE_ERRLOG(log_flags_START, FUNCTION_CALL, err, fct_name)

/* define: TRACEOUTOFMEM_ERRLOG
 * Logs "out of memory" reason for function abort.
 * If a function could not allocate memory of size bytes and therefore aborts
 * with an error code
 * > TRACEOUTOFMEM_ERRLOG(size_of_memory_in_bytes)
 * should be called before <TRACEABORT_ERRLOG> to document the event leading to an abort. */
#define TRACEOUTOFMEM_ERRLOG(size, err)   \
         TRACE_ERRLOG(log_flags_START, MEMORY_OUT_OF, err, size)

/* define: TRACESYSCALL_ERRLOG
 * Logs reason of failure and name of called system function.
 * In POSIX compatible systems sys_errno should be set to
 * the C error variable: errno. */
#define TRACESYSCALL_ERRLOG(sys_fctname, err)  \
         TRACE_ERRLOG(log_flags_START, FUNCTION_SYSCALL, err, sys_fctname)

/* define: TRACE_ERRLOG
 * Logs an TEXTID from C-kern/resource/errlog.text and error number err.
 * Parameter FLAGS carries additional flags to control the logging process - see <log_flags_e>.
 * Any additional arguments are listed after err.
 * Use <TRACE_ERRLOG> to log any language specific text with additional parameter values. */
#define TRACE_ERRLOG(FLAGS, TEXTID, err, ...)   \
         TRACE2_ERRLOG(FLAGS, TEXTID, __FUNCTION__, __FILE__, __LINE__, err, __VA_ARGS__)

/* define: TRACE2_ERRLOG
 * Logs an TEXTID from C-kern/resource/errlog.text and error number err.
 *
 * Parameter:
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - Error text ID from C-kern/resource/errlog.text.
 * funcname   - Name of function - used to describe error position.
 * filename   - Name of source file - used to describe error position.
 * linenr     - Number of current source line - used to describe error position.
 * err        - Error number.
 * ...        - The following parameter are used to parameterize TEXTID. */
#define TRACE2_ERRLOG(FLAGS, TEXTID, funcname, filename, linenr, err, ...)  \
         do {                                   \
            log_header_t _header =              \
               log_header_INIT(funcname,        \
                  filename, linenr, err) ;      \
            if (0) {                            \
            /* test for correct parameter */    \
            TEXTID ## _ERRLOG(                  \
               (struct logbuffer_t*)0,          \
               __VA_ARGS__) ;                   \
            }                                   \
            PRINTTEXT_LOG(log_channel_ERR,      \
               FLAGS, &_header,                 \
               & v ## TEXTID ## _ERRLOG,        \
               __VA_ARGS__) ;                   \
         }  while(0)

/* define: TRACE_NOARG_ERRLOG
 * Logs an TEXTID from C-kern/resource/errlog.text and error number err.
 * Parameter FLAGS carries additional flags to control the logging process - see <log_flags_e>.
 * There are no additional arguments.
 * Use <TRACE_NOARG_ERRLOG> to log any language specific text with no parameter values. */
#define TRACE_NOARG_ERRLOG(FLAGS, TEXTID, err)  \
         do {                                   \
            log_header_t _header =              \
               log_header_INIT(__FUNCTION__,    \
                  __FILE__, __LINE__, err) ;    \
            if (0) {                            \
            /* test for correct parameter */    \
            TEXTID ## _ERRLOG(                  \
               (struct logbuffer_t*)0) ;        \
            }                                   \
            PRINTTEXT_LOG(log_channel_ERR,      \
               FLAGS, &_header,                 \
               & v ## TEXTID ## _ERRLOG) ;      \
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
         PRINTARRAYFIELD_LOG(log_channel_ERR, format, arrname, index)

/* define: PRINTCSTR_ERRLOG
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; PRINTCSTR_ERRLOG(name) ; */
#define PRINTCSTR_ERRLOG(varname)         PRINTCSTR_LOG(log_channel_ERR, varname)

/* define: PRINTINT_ERRLOG
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; PRINTINT_ERRLOG(max) ; */
#define PRINTINT_ERRLOG(varname)          PRINTINT_LOG(log_channel_ERR, varname)

/* define: PRINTINT64_ERRLOG
 * Log "name=value" of int variable.
 * Example:
 * > const int64_t min = 100 ; PRINTINT64_ERRLOG(min) ; */
#define PRINTINT64_ERRLOG(varname)        PRINTINT64_LOG(log_channel_ERR, varname)

/* define: PRINTSIZE_ERRLOG
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; PRINTSIZE_ERRLOG(maxsize) ; */
#define PRINTSIZE_ERRLOG(varname)         PRINTSIZE_LOG(log_channel_ERR, varname)

/* define: PRINTUINT8_ERRLOG
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; PRINTUINT8_ERRLOG(limit) ; */
#define PRINTUINT8_ERRLOG(varname)        PRINTUINT8_LOG(log_channel_ERR, varname)

/* define: PRINTUINT16_ERRLOG
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; PRINTUINT16_ERRLOG(limit) ; */
#define PRINTUINT16_ERRLOG(varname)       PRINTUINT16_LOG(log_channel_ERR, varname)

/* define: PRINTUINT32_ERRLOG
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; PRINTUINT32_ERRLOG(max) ; */
#define PRINTUINT32_ERRLOG(varname)       PRINTUINT32_LOG(log_channel_ERR, varname)

/* define: PRINTUINT64_ERRLOG
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; PRINTUINT64_ERRLOG(max) ; */
#define PRINTUINT64_ERRLOG(varname)       PRINTUINT64_LOG(log_channel_ERR, varname)

/* define: PRINTPTR_ERRLOG
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; PRINTPTR_ERRLOG(ptr) ; */
#define PRINTPTR_ERRLOG(varname)          PRINTPTR_LOG(log_channel_ERR, varname)

/* define: PRINTDOUBLE_ERRLOG
 * Log "name=value" of double or float variable.
 * Example:
 * > const double d = 1.234 ; PRINTDOUBLE_ERRLOG(d) ; */
#define PRINTDOUBLE_ERRLOG(varname)       PRINTDOUBLE_LOG(log_channel_ERR, varname)

#endif
