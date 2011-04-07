/* title: Log API
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
   (C) 2010 Jörg Seebohn

   file: C-kern/api/umgebung/log.h
    Header file of <Log API>.

   file: C-kern/umgebung/log.c
    Implementation file of <Log API>.
*/
#ifndef CKERN_UMGEBUNG_LOG_HEADER
#define CKERN_UMGEBUNG_LOG_HEADER

/* typedef: typedef log_config_t
 * Shortcut for <log_config_t>. */
typedef struct log_config_t log_config_t ;

/* typedef: typedef log_buffer_t
 * Shortcut for internal type <log_buffer_t>. */
typedef struct log_buffer_t log_buffer_t ;

/* Used to support basic logging in main thread before anything is initialized. */
extern log_config_t  g_main_logservice ;

/* Used to support safe logging after freeing the log resource in <umgebung_t>.
 * This logservice is thread safe but supports only rudimentary logging and
 * its configuration can not be changed. */
extern log_config_t  g_safe_logservice ;

// section: Functions

// group: init

extern int init_once_per_thread_log(umgebung_t * umg) ;

extern int free_once_per_thread_log(umgebung_t * umg) ;

// group: query

/* define: LOG_GETBUFFER
 * Returns C-string of buffered log and its length. See also <getlogbuffer_logconfig>.
 * > #define LOG_GETBUFFER(buffer, size) getlogbuffer_logconfig(log_umgebung(), buffer, size) */
#define LOG_GETBUFFER(/*out char ** */buffer, /*out size_t * */size) \
   getlogbuffer_logconfig(log_umgebung(), buffer, size)

/* define: LOG_ISON
 * Returns *true* if logging is on. See also <log_config_t.isOn>.
 * > #define LOG_ISON()  (log_umgebung()->isOn) */
#define LOG_ISON()       (log_umgebung()->isOn)

/* variable: LOG_ISBUFFERED
 * Returns *true* if buffering is on. See also <log_config_t.isBuffered>.
 * > #define LOG_ISBUFFERED() (log_umgebung()->isBuffered) */
#define LOG_ISBUFFERED()      (log_umgebung()->isBuffered)

// group: configuration

/* define: LOG_PUSH_ONOFFSTATE
 * Saves <log_config_t.isOn> in local variable.
 * > #define LOG_PUSH_ONOFFSTATE()  bool pushed_onoff_log = log_umgebung()->isOn */
#define LOG_PUSH_ONOFFSTATE()       bool pushed_onoff_log = log_umgebung()->isOn
/* define: LOG_POP_ONOFFSTATE
 * Restores <log_config_t.isOn> from state previously saved in local variable.
 * > #define LOG_POP_ONOFFSTATE()   setonoff_logconfig(log_umgebung(), pushed_onoff_log) */
#define LOG_POP_ONOFFSTATE()        setonoff_logconfig(log_umgebung(), pushed_onoff_log)
/* define: LOG_TURNOFF
 * Turns logging off.
 * > #define LOG_TURNOFF()          setonoff_logconfig(log_umgebung(), false) */
#define LOG_TURNOFF()               setonoff_logconfig(log_umgebung(), false)
/* define: LOG_TURNON
 * Turns logging on (default state).
 * > #define LOG_TURNON()           setonoff_logconfig(log_umgebung(), true) */
#define LOG_TURNON()                setonoff_logconfig(log_umgebung(), true)
/* define: LOG_CONFIG_BUFFERED
 * Turns buffering on (true) or off (false). See also <setbuffermode_logconfig>.
 * Buffering turned *off* is default state.
 * > #define LOG_CONFIG_BUFFERED(on) setbuffermode_logconfig(log_umgebung(), on) */
#define LOG_CONFIG_BUFFERED(on)     setbuffermode_logconfig(log_umgebung(), on)

// group: buffered log

/* define: LOG_CLEARBUFFER
 * Clears log buffer (sets length of logbuffer to 0). See also <clearbuffer_logconfig>.
 * > #define  LOG_CLEARBUFFER()     clearbuffer_logconfig(log_umgebung()) */
#define  LOG_CLEARBUFFER()          clearbuffer_logconfig(log_umgebung())
/* define: LOG_WRITEBUFFER
 * Writes content of log buffer and then clears it. See also <writebuffer_logconfig>.
 * > #define  LOG_WRITEBUFFER()     writebuffer_logconfig(log_umgebung()) */
#define  LOG_WRITEBUFFER()          writebuffer_logconfig(log_umgebung())


// group: write

/* define: LOG_TEXT
 * Logs text resource.
 * Example:
 * > LOG_TEXT(MEMORY_ERROR_OUT_OF(size)) */
#define LOG_TEXT( TEXTID )          log_umgebung()->printf( log_umgebung(), TEXTID )
/* define: LOG_STRING
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; LOG_STRING(name) ; */
#define LOG_STRING(varname)         LOG_VAR("s", varname)
/* define: LOG_INT
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; LOG_INT(max) ; */
#define LOG_INT(varname)            LOG_VAR("d", varname)
/* define: LOG_SIZE
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; LOG_SIZE(maxsize) ; */
#define LOG_SIZE(varname)           LOG_VAR(PRIuSIZE, varname)
/* define: LOG_UINT8
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; LOG_UINT8(limit) ; */
#define LOG_UINT8(varname)         LOG_VAR(PRIu8, varname)
/* define: LOG_UINT16
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; LOG_UINT16(limit) ; */
#define LOG_UINT16(varname)         LOG_VAR(PRIu16, varname)
/* define: LOG_UINT32
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; LOG_UINT32(max) ; */
#define LOG_UINT32(varname)         LOG_VAR(PRIu32, varname)
/* define: LOG_UINT64
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; LOG_UINT64(max) ; */
#define LOG_UINT64(varname)         LOG_VAR(PRIu64, varname)
/* define: LOG_PTR
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; LOG_PTR(ptr) ; */
#define LOG_PTR(varname)            LOG_VAR("p", varname)
/* define: LOG_VAR
 * Logs "<varname>=varvalue" of a variable with name varname.
 *
 * Parameter:
 * printf_typespec_str  - Type of the variable as string in printf format. Use "d" for signed int or "u" for unsigned int.
 *                        Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * varname              - The name of the variable to log.
 *
 * Example:
 * This code logs "memsize=1024\n"
 * > const size_t memsize = 1024 ;
 * > LOG_VAR(PRIuSIZE,memsize) ; */
#define LOG_VAR(printf_typespec_str,varname)          log_umgebung()->printf( log_umgebung(), #varname "=%" printf_typespec_str "\n", (varname))
/* define: LOG_INDEX
 * Log "array[i]=value" of variable stored in array at offset i.
 *
 * Parameter:
 * printf_typespec_str  - Type of the variable as string in printf format. Use "s" for C strings.
 *                        Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * arrname              - The name of the array.
 * index                - The index of the array entry whose value is to be logged.
 *
 * Example:
 * This code logs "names[0]=Jo\n" and "names[1]=Jane\n".
 * > const char * names[] = { "Jo", "Jane" } ;
 * > for(int i = 0; i < 2; ++i) { LOG_INDEX(s,names,i) ; } */
#define LOG_INDEX(printf_typespec_str,arrname,index)  log_umgebung()->printf( log_umgebung(), #arrname "[%d]=%" printf_typespec_str "\n", i, (arrname)[i])

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_log
 * Test initialization process succeeds and functionality of service types. */
extern int unittest_umgebung_log(void) ;
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
