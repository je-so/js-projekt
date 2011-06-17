/* title: LogWriter
   Write error messages to STDERR or log file for diagnostic purposes.
   Output to log file is currently not possible / not implemented.

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

   file: C-kern/api/writer/log.h
    Header file of <LogWriter>.

   file: C-kern/writer/log.c
    Implementation file <LogWriter impl>.
*/
#ifndef CKERN_WRITER_LOG_HEADER
#define CKERN_WRITER_LOG_HEADER

/* typedef: typedef log_config_t
 * Shortcut for <log_config_t>. */
typedef struct log_config_t   log_config_t ;

/* typedef: typedef log_buffer_t
 * Shortcut for internal type <log_buffer_t>. */
typedef struct log_buffer_t   log_buffer_t ;

/* variable: g_main_logservice
 * Used to support basic logging in main thread.
 * Before anything is initialized.
 * Supports also safe logging after freeing the log resource in <umgebung_t>.
 * This logservice is thread safe but supports only rudimentary logging and
 * its configuration can not be changed. */
extern log_config_t           g_main_logservice ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_writer_log
 * Test initialization process succeeds and functionality of service types. */
extern int unittest_writer_log(void) ;
#endif


/* struct: log_config_t
 * A certain log configuration stores log information about one thread.
 * It is therefore *not* thread safe. Every thread must have its own configuration. */
struct log_config_t
{
   // group: process
   /* variable: printf
    * Print formatted output to log stream.
    * Used to switch between different implementations. */
   void        (* printf) (log_config_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;
   // group: query
   /* variable: isOn
    * Allows fast query of state which was set with <config_onoff>.
    * If logging is on isOn is set to *true* else *false*. */
   bool           isOn ;
   /* variable: isBuffered
    * Allows fast query of state which was set with <config_buffered>.
    * If buffering is on isBuffered is set to *true* else *false*. */
   bool           isBuffered ;
   /* variable: isConst
    * Indicates if log configutation can not be changed (true).
    * If this value is set to false *false* the log can be configured. */
   bool           isConstConfig ;
   // group: implementation
   log_buffer_t   * log_buffer ;
} ;

// group: initumgebung

/* function: initumgebung_log
 * Called from <init_umgebung>: same as <new_logconfig>. */
extern int initumgebung_log(/*out*/log_config_t ** log) ;

/* function: freeumgebung_log
 * Called from <free_umgebung>: same as <delete_logconfig>.
 * The only difference is that log is not NULL after return
 * but set to <g_main_logservice>. */
extern int freeumgebung_log(log_config_t ** log) ;

// group: lifetime

/* function: new_logconfig
 * Allocates memory for the structure and initializes all variables to default values.
 * The default configuration is to write the log to standard error.
 * This log service is *not* thread safe. So use it only within a single thread. */
extern int new_logconfig(/*out*/log_config_t ** log) ;

/* function: delete_logconfig
 * Frees resources and frees memory of log object.
 * After return log is set to NULL even in case of an error occured.
 * If it is called more than once log is already set to NULL and the function does nothing in this case. */
extern int delete_logconfig(log_config_t ** log) ;

// group: configuration

/* function: setonoff_logconfig
 * Switches logging on (onoff==true) or off (onoff==false). */
extern int setonoff_logconfig(log_config_t * log, bool onoff) ;

/* function: setbuffermode_logconfig
 * Switches buffered mode on (mode == true) or off (mode == false). */
extern int setbuffermode_logconfig(log_config_t * log, bool mode) ;

// group: buffered log

/* function: clearbuffer_logconfig
 * Clears log buffer (sets length of logbuffer to 0).
 * This call is ignored if the log is not configured to be in buffered mode. */
extern void clearbuffer_logconfig(log_config_t * log) ;

/* function: writebuffer_logconfig
 * Writes content of buffer to standard error and clears log buffer (sets length of logbuffer to 0).
 * This call is ignored if the log is not configured to be in buffered mode. */
extern void writebuffer_logconfig(log_config_t * log) ;

/* function: getlogbuffer_logconfig
 * Returns content of log buffer as C-string and its length.
 * The buffer is valid as long as buffermode is on.
 * The content and size changes if a new log sentence is written.
 * Do not free the buffer. It points to an internal buffer used by the implementation. */
extern void getlogbuffer_logconfig(log_config_t * log, /*out*/char ** buffer, /*out*/size_t * size) ;

#endif
