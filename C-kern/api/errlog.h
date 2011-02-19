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

#define LOG_ABORT(err)           LOG_TEXT(FUNCTION_ABORT(__FILE__, __FUNCTION__, err))
#define LOG_OUTOFMEMORY(size)    LOG_TEXT(MEMORY_ERROR_OUT_OF(size))
#define LOG_SYSERRNO(fct_name)         LOG_TEXT(FUNCTION_SYSERROR(fct_name, errno, strerror(errno)))
#define LOG_SYSERROR(fct_name,_error)  LOG_TEXT(FUNCTION_SYSERROR(fct_name, _error, strerror(_error)))

/* define: LOG_ABORT
 * Logs the name of an aborted of a function as well as the returned error code.
 *
 * Parameters:
 *    err - The error number.
 *
 * Context:
 * If a function encounters an error from which it cannot recover
 * it should roll back the system to its previous state before it was
 * called and use LOG_ABORT(returned_error_code) to signal this fact.
 */

/* define: LOG_OUTOFMEMORY
 * Logs the reason why a function has to abort.
 *
 * Parameters:
 *    size - The number of bytes for which the allocation of memory failed.
 *
 * Context:
 * If a function could not allocate memory of size bytes and therefore aborts
 * whith an error code
 * > LOG_OUTOFMEMORY(size)
 * should be called before <LOG_ABORT> to document the event leading to an abort.
 */

#endif
