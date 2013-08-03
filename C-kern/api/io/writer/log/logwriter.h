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

// forward
struct log_it ;
struct log_header_t ;
struct logbuffer_t ;
typedef void (*log_text_f) (struct logbuffer_t * logbuffer, va_list vargs) ;

/* typedef: struct logwriter_t
 * Exports <logwriter_t>. */
typedef struct logwriter_t                logwriter_t ;

/* typedef: struct logwriter_chan_t
 * Exports <logwriter_chan_t>. */
typedef struct logwriter_chan_t           logwriter_chan_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_writer_log_logwriter
 * Tests <initthread_logwriter> and functionality of <logwriter_t>. */
int unittest_io_writer_log_logwriter(void) ;
#endif


/* struct: logwriter_t
 * A logwriter writes the console channel messages to STDOUT any other channels to STDERR.
 * Before anything is written out the messages are stored in an internal buffer.
 * If less then log_config_MINSIZE plus "terminating \0 byte" bytes are free the buffer
 * is written out (flushed) before any new mesage is appended. If messages should be appended
 * the buffer is not written out until the last message was appended. In this case messages
 * are truncated if they are bigger than log_config_MINSIZE.
 *
 * Threads:
 * In case of KONFIG_SUBSYS does not contain KONFIG_thread static <logmain_t> is used instead of <logwriter_t>.
 * */
struct logwriter_t {
   /* variable: addr
    * Address of allocated buffer. */
   uint8_t  *           addr ;
   /* variable: size
    * Size in bytes of allocated buffer. */
   size_t               size ;
   /* variable: chan
    * Array of <logwriter_chan_t>.
    * A <log_channel_e> number is mapped to a <logwriter_chan_t> with help of this array. */
   logwriter_chan_t *   chan[4] ;
} ;

// group: initthread

/* function: interface_logwriter
 * This function is called from <init_threadcontext>.
 * Used to initialize interface of <log_t>. */
struct log_it * interface_logwriter(void) ;

// group: lifetime

/* define: logwriter_INIT_FREEABLE
 * Static initializer. */
#define logwriter_INIT_FREEABLE           { 0, 0, { 0, 0, 0, 0 } }

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
 * Returns content of log buffer as C-string and its size in bytes.
 * The string has a trailing NULL byte, i.e. buffer[size] == 0.
 * The address of the buffer is valid as long as no call <free_logwriter> is made.
 * The content changes if the buffer is flushed or cleared and new log entries are written.
 * Do not free the returned buffer. It points to an internal buffer used by the implementation. */
void getbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel, /*out*/char ** buffer, /*out*/size_t * size) ;

// group: change

/* function: clearbuffer_logwriter
 * Clears log buffer (sets length of logbuffer to 0).
 * This call is ignored if the log is not configured to be in buffered mode. */
void clearbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel) ;

/* function: flushbuffer_logwriter
 * Writes content of buffer to STDERR or configured file descriptor and clears log buffer.
 * This call is ignored if the log is not configured to be in buffered mode. */
void flushbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel) ;

/* function: printf_logwriter
 * Writes new log entry to internal buffer.
 * If the entry is bigger than <log_config_MINSIZE> it may be truncated if internal buffer size is lower. */
void printf_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, ...) __attribute__ ((__format__ (__printf__, 5, 6))) ;

/* function: printtext_logwriter
 * Writes a text resource to internal buffer.
 * If the entry is bigger than <log_config_MINSIZE> it may be truncated if internal buffer size is lower. */
void printtext_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, log_text_f textf, ...) ;

/* function: vprintf_logwriter
 * Same as <printf_logwriter> except that variadic arguments are replaced by args.
 * Function used internally to implement <printf_logwriter>. */
void vprintf_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, va_list args) ;



// section: inline implementation

// group: KONFIG_thread

#define KONFIG_thread 1
#if ((KONFIG_SUBSYS&KONFIG_thread) == 0)

/* define: interface_logwriter
 * Implement <logwriter_t.interface_logwriter> if there is no thread subsystem. */
#define interface_logwriter()             (0)

/* define: init_logwriter
 * Implement <logwriter_t.init_logwriter> if there is no thread subsystem. */
#define init_logwriter(log)               (ENOSYS)

/* define: free_logwriter
 * Implement <logwriter_t.free_logwriter> if there is no thread subsystem. */
#define free_logwriter(log)               (ENOSYS)

#endif
#undef KONFIG_thread

#endif
