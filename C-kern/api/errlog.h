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


// Default Subsystem

/* define: LOG_ABORT
 * Logs the abortion of a function and the its corresponding error code. */
#define LOG_ABORT(err)                       LOG_ERROR(FUNCTION_ABORT(__FILE__, __FUNCTION__, err))
/* define: LOG_ABORT_FREE
 * Logs that an error occurred during free_XXX or delete_XXX.
 * This means that not all resources could not have been freed but
 * as many as possible. */
#define LOG_ABORT_FREE(err)                  LOG_ERROR(FUNCTION_ABORT_FREE(err))
/* define: LOG_ERROR
 * Logs an errorlog text resource.
 * Use <LOG_ERROR> instead of <LOG_TEXTRES> so you
 * do not have to prefix every resource name with "TEXTRES_ERRORLOG_". */
#define LOG_ERROR( TEXTID )                  LOG_TEXTRES( TEXTRES_ERRORLOG_ ## TEXTID )
#define LOG_OUTOFMEMORY(size)                LOG_ERROR(MEMORY_OUT_OF(size))
#define LOG_SYSERRNO(fct_name)               LOG_ERROR(FUNCTION_SYSERROR(fct_name, errno, strerror(errno)))
#define LOG_SYSERROR(fct_name,err)           LOG_ERROR(FUNCTION_SYSERROR(fct_name, err, strerror(err)))



/* about: Usage
 *
 * LOG_ABORT:
 * If a function encounters an error from which it cannot recover
 * it should roll back the system to its previous state before it was
 * called and use
 * > LOG_ABORT(returned_error_code)
 * to signal this fact.
 *
 * LOG_OUTOFMEMORY:
 * If a function could not allocate memory of size bytes and therefore aborts
 * with an error code
 * > LOG_OUTOFMEMORY(size_of_memory_in_bytes)
 * should be called before <LOG_ABORT> to document the event leading to an abort.
 *
 */

#endif
