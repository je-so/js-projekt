/*
   C-System-Layer: C-kern/api/errlog.h
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/
#ifndef CKERN_API_ERRLOG_HEADER
#define CKERN_API_ERRLOG_HEADER

#include "C-kern/api/umgebung/log.h"
#include "C-kern/api/resource/errorlog.h"


// section: Functions

// group: default

/* define: LOG_ABORT
 * Logs the abortion of a function and the its corresponding error code.
 * If a function encounters an error from which it cannot recover
 * it should roll back the system to its previous state before it was
 * called and use
 * > LOG_ABORT(returned_error_code)
 * to signal this fact. */
#define LOG_ABORT(err)                       LOG_ERROR(FUNCTION_ABORT(err))

/* define: LOG_ABORT_FREE
 * Logs that an error occurred during free_XXX or delete_XXX.
 * This means that not all resources could not have been freed but
 * as many as possible. */
#define LOG_ABORT_FREE(err)                  LOG_ERROR(FUNCTION_ABORT_FREE(err))

/* define: LOG_ERROR
 * Logs an errorlog text resource.
 * Use <LOG_ERROR> instead of <LOG_TEXTRES> so you
 * do not have to prefix every resource name with "TEXTRES_ERRORLOG_". */
#define LOG_ERROR( TEXTID )                  do {  LOG_TEXTRES( TEXTRES_ERRORLOG_ERROR_LOCATION ) ; \
                                                   LOG_TEXTRES( TEXTRES_ERRORLOG_ ## TEXTID ) ; \
                                                }  while(0)
/* define: LOG_OUTOFMEMORY
 * Logs "out of memory" reason for function abort.
 * If a function could not allocate memory of size bytes and therefore aborts
 * with an error code
 * > LOG_OUTOFMEMORY(size_of_memory_in_bytes)
 * should be called before <LOG_ABORT> to document the event leading to an abort. */
#define LOG_OUTOFMEMORY(size)                LOG_ERROR(MEMORY_OUT_OF(size))

#define LOG_SYSERRNO(fct_name)               LOG_SYSERRNO2(fct_name, errno)

#define LOG_SYSERRNO2(fct_name,_ERRNO)       LOG_ERROR(FUNCTION_SYSERRNO(fct_name, _ERRNO, strerror(_ERRNO)))

// TODO: replace strerror(err) with string_errlog(int sys_err) in umgebung/errlog.c
#define LOG_SYSERROR(fct_name,err)           LOG_ERROR(FUNCTION_SYSERROR(fct_name, err, strerror(err)))


#endif
