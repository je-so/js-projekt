/* title: LogWriter-Locked
   Write error messages to STDERR or log file for diagnostic purposes.
   This module is thread safe.

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

   file: C-kern/api/writer/logwriter_locked.h
    Header file of <LogWriter-Locked>.

   file: C-kern/writer/logwriter_locked.c
    Implementation file <LogWriter-Locked impl>.
*/
#ifndef CKERN_WRITER_LOGWRITER_LOCKED_HEADER
#define CKERN_WRITER_LOGWRITER_LOCKED_HEADER

#include "C-kern/api/writer/logwriter.h"

/* typedef: typedef logwriter_locked_t
 * Shortcut for <logwriter_locked_t>. */
typedef struct logwriter_locked_t         logwriter_locked_t ;

/* variable: g_main_logwriterlocked
 * Used to support basic logging in main thread.
 * Before anything is initialized.
 * Supports also safe logging after freeing the log resource in <umgebung_t>.
 * This logservice is thread safe. */
extern logwriter_locked_t                 g_main_logwriterlocked ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_writer_logwriterlocked
 * Tests <initumgebung_logwriterlocked>, functionality of <logwriter_locked_t>
 * and <g_main_logwriterlocked>. */
extern int unittest_writer_logwriterlocked(void) ;
#endif


/* struct: logwriter_locked_t
 * Same functionality as <logwriter_t>.
 * This structure contains an additional mutex lock.
 * Calls to functions which handle <logwriter_locked_t> are therefore *thread safe*. */
struct logwriter_locked_t
{
   logwriter_t    logwriter ;
   sys_mutex_t    lock ;
} ;

// group: initumgebung

/* function: initumgebung_logwriterlocked
 * Uses <init_logwriterlocked> - called from <init_umgebung>. */
extern int initumgebung_logwriterlocked(/*out*/logwriter_locked_t ** log) ;

/* function: freeumgebung_logwriter
 * Uses  <free_logwriterlocked> - called from <free_umgebung>.
 * After return log is not set to NULL instead it is set to <g_main_logwriterlocked>.
 * To support the most basic logging. */
extern int freeumgebung_logwriterlocked(logwriter_locked_t ** log) ;

// group: lifetime

/* define: logwriter_locked_INIT_FREEABLE
 * Static initializer. */
#define logwriter_locked_INIT_FREEABLE     {  logwriter_INIT_FREEABLE, sys_mutex_INIT_DEFAULT }

/* function: init_logwriterlocked
 * Allocates memory for the structure and initializes all variables to default values.
 * The default configuration is to write the log to standard error.
 * This log service is *not* thread safe. So use it only within a single thread. */
extern int init_logwriterlocked(/*out*/logwriter_locked_t * log) ;

/* function: free_logwriterlocked
 * Frees resources and frees memory of log object.
 * After return log is set to NULL even in case of an error occured.
 * If it is called more than once log is already set to NULL and the function does nothing in this case. */
extern int free_logwriterlocked(logwriter_locked_t * log) ;

// group: query

/* function: getbuffer_logwriterlocked
 * Returns content of log buffer as C-string and its length.
 * Same as <getbuffer_logwriter> except that this function is thread safe. */
extern void getbuffer_logwriterlocked(logwriter_locked_t * log, /*out*/char ** buffer, /*out*/size_t * size) ;

// group: change

/* function: clearbuffer_logwriterlocked
 * Clears log buffer (sets length of logbuffer to 0).
 * This call is ignored if the log is not configured to be in buffered mode. */
extern void clearbuffer_logwriterlocked(logwriter_locked_t * log) ;

/* function: flushbuffer_logwriterlocked
 * Writes content of buffer to STDERR or configured file descriptor and clears log buffer.
 * This call is ignored if the log is not configured to be in buffered mode. */
extern void flushbuffer_logwriterlocked(logwriter_locked_t * log) ;

/* function: printf_logwriterlocked
 * Writes new log entry to file descriptor (STDERR) or in internal buffer.
 * The output is only written in case logging is switched on (see <setonoff_logwriter>). */
extern void printf_logwriterlocked(logwriter_locked_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;


#endif
