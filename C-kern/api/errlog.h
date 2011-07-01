/* title: Errorlog
   Includes text resource file which contains errorlog messages
   and defines <LOG_ERRTEXT> to log them.

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

   file: C-kern/api/errlog.h
    Header file of <Errorlog>.
*/
#ifndef CKERN_API_ERRLOG_HEADER
#define CKERN_API_ERRLOG_HEADER

#include "C-kern/api/resource/errorlog.h"
#include "C-kern/api/test/argument.h"
#include "C-kern/api/writer/log_helper.h"


// section: Functions

// group: log-error

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
 * do not have to prefix every resource name with "TEXTRES_ERRORLOG_". */
#define LOG_ERRTEXT( TEXTID )                do {  LOG_TEXTRES( TEXTRES_ERRORLOG_ ## ERROR_LOCATION ) ; \
                                                   LOG_TEXTRES( TEXTRES_ERRORLOG_ ## TEXTID ) ; \
                                                }  while(0)

// TODO: support own error IDs: replace strerror(err) with own string_error_function(int sys_err)
#define LOG_CALLERR(fct_name,err)            LOG_ERRTEXT(FUNCTION_ERROR(fct_name, err, strerror(err)))

/* define: LOG_OUTOFMEMORY
 * Logs "out of memory" reason for function abort.
 * If a function could not allocate memory of size bytes and therefore aborts
 * with an error code
 * > LOG_OUTOFMEMORY(size_of_memory_in_bytes)
 * should be called before <LOG_ABORT> to document the event leading to an abort. */
#define LOG_OUTOFMEMORY(size)                LOG_ERRTEXT(MEMORY_OUT_OF(size))

/* define: LOG_SYSERR
 * Logs reason of failure and name of called system function.
 * In POSIX compatible systems sys_errno should be set to
 * the C error variable: errno. */
#define LOG_SYSERR(sys_fctname,sys_errno)    LOG_ERRTEXT(FUNCTION_SYSERR(sys_fctname, sys_errno, strerror(sys_errno)))


#endif
