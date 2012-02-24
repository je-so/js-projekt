/* title: ErrlogMacros
   Defines error logging macros.

   - Includes text resource file which contains errorlog messages and
     defines <LOG_ERRTEXT> to log them.
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

   file: C-kern/api/writer/errlog_macros.h
    Header file of <ErrlogMacros>.
*/
#ifndef CKERN_WRITER_ERRLOG_MACROS_HEADER
#define CKERN_WRITER_ERRLOG_MACROS_HEADER

#include "C-kern/api/resource/errorlog.h"
#include "C-kern/api/writer/log_macros.h"


// section: Functions

// group: write-text

/* define: LOG_ABORT
 * Logs the abortion of a function and the its corresponding error code.
 * If a function encounters an error from which it cannot recover
 * it should roll back the system to its previous state before it was
 * called and use
 * > LOG_ABORT(returned_error_code)
 * to signal this fact. */
#define LOG_ABORT(err)                       LOG_ERRTEXT(FUNCTION_ABORT(err))

/* define: LOG_ABORT_FREE
 * Logs that an error occurred during free_XXX or delete_XXX.
 * This means that not all resources could not have been freed but
 * as many as possible. */
#define LOG_ABORT_FREE(err)                  LOG_ERRTEXT(FUNCTION_ABORT_FREE(err))

/* define: LOG_ERRTEXT
 * Logs an errorlog text resource.
 * Use <LOG_ERRTEXT> instead of <LOG_TEXTRES> so you
 * do not have to prefix every resource name with "TEXTRES_ERRORLOG_".
 *
 * TODO: Support own error IDs
 * TODO: Replace strerror(err) with own string_error_function(int sys_err)
 * */
#define LOG_CALLERR(fct_name,err)            LOG_ERRTEXT(FUNCTION_ERROR(fct_name, err, strerror(err)))

/* define: LOG_ERRTEXT
 * Logs an errorlog text resource.
 * Use <LOG_ERRTEXT> instead of <LOG_TEXTRES> so you
 * do not have to prefix every resource name with "TEXTRES_ERRORLOG_". */
#define LOG_ERRTEXT( TEXTID )                do {  LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ERROR_LOCATION(__FILE__, __LINE__, __FUNCTION__)) ; \
                                                   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ ## TEXTID ) ; \
                                                }  while(0)

/* define: LOG_OUTOFMEMORY
 * Logs "out of memory" reason for function abort.
 * If a function could not allocate memory of size bytes and therefore aborts
 * with an error code
 * > LOG_OUTOFMEMORY(size_of_memory_in_bytes)
 * should be called before <LOG_ABORT> to document the event leading to an abort. */
#define LOG_OUTOFMEMORY(size)                LOG_ERRTEXT(MEMORY_OUT_OF(size))

/* define: LOG_PRINTF
 * Logs a generic printf type format string as error.
 *
 * Parameter:
 * FORMAT     - The format string as in the standard library function printf.
 * ...        - Additional value parameters of the correct type as determined by the <FORMAT>
 *              parameter.
 *
 * Example:
 * > int i ; LOG_PRINTF("%d", i) */
#define LOG_PRINTF(...)   \
   LOGC_PRINTF(ERR, __VA_ARGS__)

/* define: LOG_SYSERR
 * Logs reason of failure and name of called system function.
 * In POSIX compatible systems sys_errno should be set to
 * the C error variable: errno. */
#define LOG_SYSERR(sys_fctname,sys_errno)    LOG_ERRTEXT(FUNCTION_SYSERR(sys_fctname, sys_errno, strerror(sys_errno)))

// group: write-variables

/* define: LOG_INDEX
 * Log "array[i]=value" of variable stored in array at offset i.
 *
 * Parameter:
 * printf_type - Type of the variable as string in printf format. Use "s" for C strings.
 *               Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * arrname     - The name of the array.
 * index       - The index of the array entry whose value is to be logged.
 *
 * Example:
 * This code logs "names[0]=Jo\n" and "names[1]=Jane\n".
 * > const char * names[] = { "Jo", "Jane" } ;
 * > for(int i = 0; i < 2; ++i) { LOG_INDEX(s,names,i) ; } */
#define LOG_INDEX(printf_type, arrname, index)  LOGC_INDEX(ERR, printf_type, arrname, index)

/* define: LOG_STRING
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; LOG_STRING(name) ; */
#define LOG_STRING(varname)                     LOGC_STRING(ERR, varname)

/* define: LOG_INT
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; LOG_INT(max) ; */
#define LOG_INT(varname)                        LOGC_INT(ERR, varname)

/* define: LOG_SIZE
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; LOG_SIZE(maxsize) ; */
#define LOG_SIZE(varname)                       LOGC_SIZE(ERR, varname)

/* define: LOG_UINT8
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; LOG_UINT8(limit) ; */
#define LOG_UINT8(varname)                      LOGC_UINT8(ERR, varname)

/* define: LOG_UINT16
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; LOG_UINT16(limit) ; */
#define LOG_UINT16(varname)                     LOGC_UINT16(ERR, varname)

/* define: LOG_UINT32
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; LOG_UINT32(max) ; */
#define LOG_UINT32(varname)                     LOGC_UINT32(ERR, varname)

/* define: LOG_UINT64
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; LOG_UINT64(max) ; */
#define LOG_UINT64(varname)                     LOGC_UINT64(ERR, varname)

/* define: LOG_PTR
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; LOG_PTR(ptr) ; */
#define LOG_PTR(varname)                        LOGC_PTR(ERR, varname)

/* define: LOG_DOUBLE
 * Log "name=value" of double or float variable.
 * Example:
 * > const double d = 1.234 ; LOG_DOUBLE(d) ; */
#define LOG_DOUBLE(varname)                     LOGC_DOUBLE(ERR, varname)

#endif
