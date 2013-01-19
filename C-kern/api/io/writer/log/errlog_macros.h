/* title: ErrlogMacros
   Defines error logging macros.

   - Includes text resource file which contains errorlog messages and
     defines <TRACEERR_LOG> to log them.
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

#include "C-kern/api/io/writer/log/log_macros.h"
#include "C-kern/resource/generated/errlog.h"


// section: Functions

// group: log-text

/* define: PRINTF_LOG
 * Logs a generic printf type format string as error.
 *
 * Parameter:
 * FORMAT     - The format string as in the standard library function printf.
 * ...        - Additional value parameters of the correct type as determined by the <FORMAT>
 *              parameter.
 *
 * Example:
 * > int i ; PRINTF_LOG("%d", i) */
#define PRINTF_LOG(...)                CPRINTF_LOG(ERR, __VA_ARGS__)

/* define: TRACEABORT_LOG
 * Logs the abortion of a function and the its corresponding error code.
 * If a function encounters an error from which it cannot recover
 * it should roll back the system to its previous state before it was
 * called and use
 * > TRACEABORT_LOG(return_error_code)
 * to signal this fact. */
#define TRACEABORT_LOG(err)            TRACEERR_LOG(FUNCTION_ABORT,err)

/* define: TRACEABORTFREE_LOG
 * Logs that an error occurred during free_XXX or delete_XXX.
 * This means that not all resources could have been freed
 * only as many as possible. */
#define TRACEABORTFREE_LOG(err)        TRACEERR_LOG(FUNCTION_ABORT_FREE,err)

/* define: TRACECALLERR_LOG
 * Logs errorlog text resource TRACEERR_LOG_FUNCTION_ERROR.
 * Use this function to log an error in the function which calls a user library function
 * which does not do logging on its own.
 *
 * TODO: Support own error IDs
 * TODO: Replace strerror(err) with own string_error_function(int sys_err)
 * */
#define TRACECALLERR_LOG(fct_name,err) TRACEERR_LOG(FUNCTION_ERROR, fct_name, err, strerror(err))

/* define: TRACEERR_LOG
 * Logs an errorlog text resource with arguments.
 * Use <TRACEERR_LOG> to log any language specific text with additional parameter values. */
#define TRACEERR_LOG(TEXTID,...)       do {  ERROR_LOCATION_ERRLOG(log_channel_ERR, __FILE__, __LINE__, __FUNCTION__) ;  \
                                             TEXTID ## _ERRLOG(log_channel_ERR, __VA_ARGS__ ) ;                          \
                                       }  while(0)
/* define: TRACEERR_NOARG_LOG
 * Logs an errorlog text resource without any arguments. */
#define TRACEERR_NOARG_LOG(TEXTID)     do {  ERROR_LOCATION_ERRLOG(log_channel_ERR, __FILE__, __LINE__, __FUNCTION__) ;  \
                                             TEXTID ## _ERRLOG(log_channel_ERR) ;                                        \
                                       }  while(0)

/* define: TRACEOUTOFMEM_LOG
 * Logs "out of memory" reason for function abort.
 * If a function could not allocate memory of size bytes and therefore aborts
 * with an error code
 * > TRACEOUTOFMEM_LOG(size_of_memory_in_bytes)
 * should be called before <TRACEABORT_LOG> to document the event leading to an abort. */
#define TRACEOUTOFMEM_LOG(size)        TRACEERR_LOG(MEMORY_OUT_OF,size)

/* define: TRACESYSERR_LOG
 * Logs reason of failure and name of called system function.
 * In POSIX compatible systems sys_errno should be set to
 * the C error variable: errno. */
#define TRACESYSERR_LOG(sys_fctname,sys_errno)  TRACEERR_LOG(FUNCTION_SYSERR, sys_fctname, sys_errno, strerror(sys_errno))

// group: log-variables

/* define: PRINTARRAYFIELD_LOG
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
 * > for(int i = 0; i < 2; ++i) { PRINTARRAYFIELD_LOG(s,names,i) ; } */
#define PRINTARRAYFIELD_LOG(format, arrname, index)   CPRINTARRAYFIELD_LOG(ERR, format, arrname, index)

/* define: PRINTCSTR_LOG
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; PRINTCSTR_LOG(name) ; */
#define PRINTCSTR_LOG(varname)                  CPRINTCSTR_LOG(ERR, varname)

/* define: PRINTINT_LOG
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; PRINTINT_LOG(max) ; */
#define PRINTINT_LOG(varname)                   CPRINTINT_LOG(ERR, varname)

/* define: PRINTINT64_LOG
 * Log "name=value" of int variable.
 * Example:
 * > const int64_t min = 100 ; PRINTINT64_LOG(min) ; */
#define PRINTINT64_LOG(varname)                 CPRINTINT64_LOG(ERR, varname)

/* define: PRINTSIZE_LOG
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; PRINTSIZE_LOG(maxsize) ; */
#define PRINTSIZE_LOG(varname)                  CPRINTSIZE_LOG(ERR, varname)

/* define: PRINTUINT8_LOG
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; PRINTUINT8_LOG(limit) ; */
#define PRINTUINT8_LOG(varname)                 CPRINTUINT8_LOG(ERR, varname)

/* define: PRINTUINT16_LOG
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; PRINTUINT16_LOG(limit) ; */
#define PRINTUINT16_LOG(varname)                CPRINTUINT16_LOG(ERR, varname)

/* define: PRINTUINT32_LOG
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; PRINTUINT32_LOG(max) ; */
#define PRINTUINT32_LOG(varname)                CPRINTUINT32_LOG(ERR, varname)

/* define: PRINTUINT64_LOG
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; PRINTUINT64_LOG(max) ; */
#define PRINTUINT64_LOG(varname)                CPRINTUINT64_LOG(ERR, varname)

/* define: PRINTPTR_LOG
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; PRINTPTR_LOG(ptr) ; */
#define PRINTPTR_LOG(varname)                   CPRINTPTR_LOG(ERR, varname)

/* define: PRINTDOUBLE_LOG
 * Log "name=value" of double or float variable.
 * Example:
 * > const double d = 1.234 ; PRINTDOUBLE_LOG(d) ; */
#define PRINTDOUBLE_LOG(varname)                CPRINTDOUBLE_LOG(ERR, varname)

#endif
