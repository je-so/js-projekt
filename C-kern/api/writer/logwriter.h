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

   file: C-kern/api/writer/logwriter.h
    Header file of <LogWriter>.

   file: C-kern/writer/logwriter.c
    Implementation file <LogWriter impl>.
*/
#ifndef CKERN_WRITER_LOGWRITER_HEADER
#define CKERN_WRITER_LOGWRITER_HEADER

#include "C-kern/api/aspect/memoryblock.h"

/* typedef: logwriter_t typedef
 * Exports <logwriter_t>. */
typedef struct logwriter_t          logwriter_t ;

/* enums: log_channel_e
 * Used to switch between log channels.
 *
 * log_channel_ERR  - Normal error log channel which is represented by <logwriter_t>.
 * log_channel_TEST - Test log output which is implemented as a call to standard
 *                    printf library function which writes to STDOUT.
 * */
enum log_channel_e {
    log_channel_ERR
   ,log_channel_TEST
} ;

typedef enum log_channel_e          log_channel_e ;

enum log_constants_e {
   log_PRINTF_MAXSIZE = 511
} ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_writer_logwriter
 * Tests <initumgebung_logwriter> and functionality of <logwriter_t>. */
extern int unittest_writer_logwriter(void) ;
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
   memoryblock_aspect_t    buffer ;
   /* variable: logsize
    * Stores the size in bytes of the buffered log entries.
    * If the buffer is empty this entry is 0. */
   size_t                  logsize ;
} ;

// group: lifetime

/* define: logwriter_INIT_FREEABLE
 * Static initializer. */
#define logwriter_INIT_FREEABLE     {  memoryblock_aspect_INIT_FREEABLE, 0 }

/* function: init_logwriter
 * Allocates memory for the structure and initializes all variables to default values.
 * The default configuration is to write the log to standard error.
 * This log service is *not* thread safe. So use it only within a single thread. */
extern int init_logwriter(/*out*/logwriter_t * log) ;

/* function: free_logwriter
 * Frees resources and frees memory of log object.
 * After return log is set to NULL even in case of an error occured.
 * If it is called more than once log is already set to NULL and the function does nothing in this case. */
extern int free_logwriter(logwriter_t * log) ;

// group: query

/* function: getbuffer_logwriter
 * Returns content of log buffer as C-string and its length in bytes.
 * The string has a trailing NULL byte, i.e. buffer[size] == 0.
 * The address of the buffer is valid as long as no call <free_logwriter> is made.
 * The content changes if the buffer is flushed or cleared and new log entries are written.
 * Do not free the returned buffer. It points to an internal buffer used by the implementation. */
extern void getbuffer_logwriter(logwriter_t * log, /*out*/char ** buffer, /*out*/size_t * size) ;

// group: change

/* function: clearbuffer_logwriter
 * Clears log buffer (sets length of logbuffer to 0).
 * This call is ignored if the log is not configured to be in buffered mode. */
extern void clearbuffer_logwriter(logwriter_t * log) ;

/* function: flushbuffer_logwriter
 * Writes content of buffer to STDERR or configured file descriptor and clears log buffer.
 * This call is ignored if the log is not configured to be in buffered mode. */
extern void flushbuffer_logwriter(logwriter_t * log) ;

/* function: printf_logwriter
 * Writes new log entry to file descriptor (STDERR) or in internal buffer.
 * The output is only written in case logging is switched on (see <setonoff_logwriter>). */
extern void printf_logwriter(logwriter_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;

/* function: vprintf_logwriter
 * Function used internally to implement <printf_logwriter>.
 * Do not use this function directly except from within a subtype. */
extern void vprintf_logwriter(logwriter_t * log, const char * format, va_list args) ;

#endif