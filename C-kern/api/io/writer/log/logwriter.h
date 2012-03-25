/* title: LogWriter
   Write error messages to STDERR for diagnostic purposes.
   This module is *not* thread safe.

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

   file: C-kern/api/io/writer/log/logwriter.h
    Header file of <LogWriter>.

   file: C-kern/io/writer/log/logwriter.c
    Implementation file <LogWriter impl>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOGWRITER_HEADER
#define CKERN_IO_WRITER_LOG_LOGWRITER_HEADER

#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/io/writer/log/log_it.h"


/* typedef: logwriter_t typedef
 * Exports <logwriter_t>. */
typedef struct logwriter_t          logwriter_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_writer_log_logwriter
 * Tests <initthread_logwriter> and functionality of <logwriter_t>. */
int unittest_io_writer_log_logwriter(void) ;
#endif


/* struct: logwriter_t
 * A logwriter writes error messages to STDERR and test messages to STDOUT.
 * Internally it uses a buffer to preformat the message before it is written
 * to any file descriptor. It is possible to switch into buffered mode in which
 * messages are buffered until the buffer is full and only then the buffer is
 * written to any any configured file descriptor.
 * This object is *not thread safe*. */
struct logwriter_t
{
   /* variable: buffer
    * Holds memory address and size of internal buffer.
    * If buffering mode is off this buffer can hold only one entry.
    * If buffering mode is one this buffer can hold several log entries. */
   memblock_t     buffer ;
   /* variable: logsize
    * Stores the size in bytes of the buffered log entries.
    * If the buffer is empty this entry is 0. */
   size_t         logsize ;
} ;

// group: init

/* function: initthread_logwriter
 * Uses <init_logwriter> - called from <init_threadcontext>. */
int initthread_logwriter(/*out*/log_oit * ilog) ;

/* function: freethread_logwriter
 * Uses  <free_logwriter> - called from <free_threadcontext>.
 * After return log is not set to NULL instead it is set to <g_logmain>.
 * To support the most basic logging. */
int freethread_logwriter(log_oit * ilog) ;

// group: lifetime

/* define: logwriter_INIT_FREEABLE
 * Static initializer. */
#define logwriter_INIT_FREEABLE        {  memblock_INIT_FREEABLE, 0 }

/* function: init_logwriter
 * Allocates memory for the structure and initializes all variables to default values.
 * The default configuration is to write the log to standard error.
 * This log service is *not* thread safe. So use it only within a single thread. */
int init_logwriter(/*out*/logwriter_t * lgwrt) ;

/* function: free_logwriter
 * Frees resources and frees memory of log object.
 * In case the function is called more than it does nothing. */
int free_logwriter(logwriter_t * lgwrt) ;

// group: query

/* function: getbuffer_logwriter
 * Returns content of log buffer as C-string and its length in bytes.
 * The string has a trailing NULL byte, i.e. buffer[size] == 0.
 * The address of the buffer is valid as long as no call <free_logwriter> is made.
 * The content changes if the buffer is flushed or cleared and new log entries are written.
 * Do not free the returned buffer. It points to an internal buffer used by the implementation. */
void getbuffer_logwriter(logwriter_t * lgwrt, /*out*/char ** buffer, /*out*/size_t * size) ;

// group: change

/* function: clearbuffer_logwriter
 * Clears log buffer (sets length of logbuffer to 0).
 * This call is ignored if the log is not configured to be in buffered mode. */
void clearbuffer_logwriter(logwriter_t * lgwrt) ;

/* function: flushbuffer_logwriter
 * Writes content of buffer to STDERR or configured file descriptor and clears log buffer.
 * This call is ignored if the log is not configured to be in buffered mode. */
void flushbuffer_logwriter(logwriter_t * lgwrt) ;

/* function: printf_logwriter
 * Writes new log entry to file descriptor (STDERR) or in internal buffer.
 * The output is only written in case logging is switched on (see <setonoff_logwriter>). */
void printf_logwriter(logwriter_t * lgwrt, enum log_channel_e channel, const char * format, ... ) __attribute__ ((__format__ (__printf__, 3, 4))) ;

/* function: vprintf_logwriter
 * Function used internally to implement <printf_logwriter>.
 * Do not use this function directly except from within a subtype. */
void vprintf_logwriter(logwriter_t * lgwrt, const char * format, va_list args) ;


// section: inline implementation

// group: KONFIG_SUBSYS

/* about: Thread
 * In case of only 1 thread use static <logmain_t> instead of <logwriter_t>.
 * The following code tests for submodule THREAD and replaces
 * <freethread_logwriter> and <initthread_logwriter> with (0) in
 * case THREAD submodule is not configured. */

#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
/* define: initthread_logwriter
 * Implement <logwriter_t.initthread_logwriter> as a no op if !((KONFIG_SUBSYS)&THREAD)
 * The default logging service is <logmain_t> which may not be thread safe. */
#define initthread_logwriter(ilog)   (0)
/* define: freethread_logwriter
 * Implement <logwriter_t.freethread_logwriter> as a no op if !((KONFIG_SUBSYS)&THREAD) */
#define freethread_logwriter(ilog)   (0)
#endif
#undef THREAD

#endif
