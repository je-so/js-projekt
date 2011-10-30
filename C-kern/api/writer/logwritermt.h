/* title: LogWriter-MT
   Write error messages to STDERR or log file for diagnostic purposes.

   * MT - Multi Thread safe

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

   file: C-kern/api/writer/logwritermt.h
    Header file of <LogWriter-MT>.

   file: C-kern/writer/logwritermt.c
    Implementation file <LogWriter-MT impl>.
*/
#ifndef CKERN_WRITER_LOGWRITERMT_HEADER
#define CKERN_WRITER_LOGWRITERMT_HEADER

#include "C-kern/api/writer/logwriter.h"

/* typedef: struct logwritermt_t
 * Export <logwritermt_t>. */
typedef struct logwritermt_t              logwritermt_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_writer_logwritermt
 * Tests <initumgebung_logwritermt> and functionality of <logwritermt_t>. */
extern int unittest_writer_logwritermt(void) ;
#endif


/* struct: logwritermt_t
 * Same functionality as <logwriter_t>.
 * This structure contains an additional mutex lock.
 * Calls to functions which handle <logwritermt_t> are therefore *thread safe*. */
struct logwritermt_t
{
   logwriter_t    logwriter ;
   sys_mutex_t    lock ;
} ;

// group: init

/* function: initumgebung_logwritermt
 * Uses <init_logwritermt> - called from <init_umgebung>. */
extern int initumgebung_logwritermt(/*out*/log_oit * ilog) ;

/* function: freeumgebung_logwritermt
 * Uses  <free_logwritermt> - called from <free_umgebung>.
 * After return log is not set to NULL instead it is set to <g_main_logwriter>.
 * To support the most basic logging. */
extern int freeumgebung_logwritermt(log_oit * ilog) ;

// group: lifetime

/* define: logwritermt_INIT_FREEABLE
 * Static initializer. */
#define logwritermt_INIT_FREEABLE     {  logwriter_INIT_FREEABLE, sys_mutex_INIT_DEFAULT }

/* function: init_logwritermt
 * Allocates memory for the structure and initializes all variables to default values.
 * The default configuration is to write the log to standard error.
 * This log service is *not* thread safe. So use it only within a single thread. */
extern int init_logwritermt(/*out*/logwritermt_t * log) ;

/* function: free_logwritermt
 * Frees resources and frees memory of log object.
 * After return log is set to NULL even in case of an error occured.
 * If it is called more than once log is already set to NULL and the function does nothing in this case. */
extern int free_logwritermt(logwritermt_t * log) ;

// group: query

/* function: getbuffer_logwritermt
 * Returns content of log buffer as C-string and its length.
 * Same as <getbuffer_logwriter> except that this function is thread safe. */
extern void getbuffer_logwritermt(logwritermt_t * log, /*out*/char ** buffer, /*out*/size_t * size) ;

// group: change

/* function: clearbuffer_logwritermt
 * Clears log buffer (sets length of logbuffer to 0).
 * This call is ignored if the log is not configured to be in buffered mode. */
extern void clearbuffer_logwritermt(logwritermt_t * log) ;

/* function: flushbuffer_logwritermt
 * Writes content of buffer to STDERR or configured file descriptor and clears log buffer.
 * This call is ignored if the log is not configured to be in buffered mode. */
extern void flushbuffer_logwritermt(logwritermt_t * log) ;

/* function: printf_logwritermt
 * Writes new log entry to file descriptor (STDERR) or in internal buffer.
 * The output is only written in case logging is switched on (see <setonoff_logwriter>). */
extern void printf_logwritermt(logwritermt_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;


#endif
