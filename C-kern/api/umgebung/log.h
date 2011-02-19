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

/* typedef: typedef log_interface_t
 * Shortcut for <log_interface_t>. */
typedef struct log_interface_t log_interface_t ;

/* variable: g_service_log
 * Is used to switch between different implementations (see <log_interface_t>) of global log service. */
extern log_interface_t  g_service_log ;


// section: functions

// group: query

/* define: LOG_GETBUFFER
 * Returns C-string of buffered log and its length. See also <log_interface_t.getlogbuffer>.
 * > #define LOG_GETBUFFER(buffer, size) g_service_log.getlogbuffer(buffer, size) */
#define LOG_GETBUFFER(/*out char ** */buffer, /*out size_t * */size) \
      (g_service_log.getlogbuffer(buffer, size))

/* define: LOG_ISON
 * Returns *true* if logging is on. See also <log_interface_t.isOn>.
 * > #define LOG_ISON()  (g_service_log.isOn) */
#define LOG_ISON()       (g_service_log.isOn)

/* variable: LOG_ISBUFFERED
 * Returns *true* if buffering is on. See also <log_interface_t.isBuffered>.
 * > #define LOG_ISBUFFERED() (g_service_log.isBuffered) */
#define LOG_ISBUFFERED()      (g_service_log.isBuffered)

// group: configuration

/* define: LOG_PUSH_ONOFFSTATE
 * Saves g_service_log.isOn in local variable.
 * > #define LOG_PUSH_ONOFFSTATE()  bool pushed_onoff_log = g_service_log.isOn */
#define LOG_PUSH_ONOFFSTATE()       bool pushed_onoff_log = g_service_log.isOn
/* define: LOG_POP_ONOFFSTATE
 * Restores g_service_log.isOn from saved state in local variable.
 * > #define LOG_POP_ONOFFSTATE()   g_service_log.config_onoff(pushed_onoff_log) */
#define LOG_POP_ONOFFSTATE()        g_service_log.config_onoff(pushed_onoff_log)
/* define: LOG_TURNOFF
 * Turns logging off.
 * > #define LOG_TURNOFF()          g_service_log.config_onoff(false) */
#define LOG_TURNOFF()               g_service_log.config_onoff(false)
/* define: LOG_TURNON
 * Turns logging on (default state).
 * > #define LOG_TURNON()          g_service_log.config_onoff(true) */
#define LOG_TURNON()               g_service_log.config_onoff(true)
/* define: LOG_CONFIG_BUFFERED
 * Turns buffering on (true) or off (false).
 * Buffering turned *off* is default state.
 * > #define LOG_CONFIG_BUFFERED(on) g_service_log.config_buffered(on) */
#define LOG_CONFIG_BUFFERED(on)      g_service_log.config_buffered(on)

/* define: LOG_CLEARBUFFER
 * Clears log buffer (sets length of logbuffer to 0). See also <log_interface_t.clearlogbuffer>.
 * > #define  LOG_CLEARBUFFER()     (g_service_log.clearlogbuffer()) */
#define  LOG_CLEARBUFFER()          (g_service_log.clearlogbuffer())

// group: write


/* define: LOG_TEXT
 * Logs text resource.
 * Example:
 * > LOG_TEXT(MEMORY_ERROR_OUT_OF(size)) */
#define LOG_TEXT( TEXTID )          g_service_log.printf( TEXTID )
/* define: LOG_STRING
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; LOG_STRING(name) ; */
#define LOG_STRING(varname)         g_service_log.printf( #varname "='%s'\n", (varname))
/* define: LOG_INT
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; LOG_INT(max) ; */
#define LOG_INT(varname)            g_service_log.printf( #varname "=%d\n", (varname))
/* define: LOG_SIZE
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; LOG_SIZE(maxsize) ; */
#define LOG_SIZE(varname)           g_service_log.printf( #varname "=%" PRIuSIZE "\n", (varname))
/* define: LOG_UINT32
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; LOG_UINT32(max) ; */
#define LOG_UINT32(varname)         g_service_log.printf( #varname "=%" PRIu32 "\n", (varname))
/* define: LOG_UINT64
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; LOG_UINT64(max) ; */
#define LOG_UINT64(varname)         g_service_log.printf( #varname "=%" PRIu64 "\n", (varname))
/* define: LOG_PTR
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; LOG_PTR(ptr) ; */
#define LOG_PTR(varname)            g_service_log.printf( #varname "=%p\n", (varname))
/* define: LOG_ARRAYSTRING
 * Log "array[i]=value" of string variable stored in array at offset i.
 * Example:
 * > const char * names[] = { "Jo", "Jane" } ; LOG_ARRAYSTRING(names,0) ; LOG_ARRAYSTRING(names,1) ; */
#define LOG_ARRAYSTRING(arrname,i)  g_service_log.printf( #arrname "[%d]=%s\n", i, (arrname)[i])
/* define: LOG_ARRAYINT
 * Log "array[i]=value" of int variable stored in array at offset i.
 * Example:
 * > const int max[2] = { 100, 200 } ; LOG_ARRAYINT(max, 0) ; LOG_ARRAYINT(max, 1) ; */
#define LOG_ARRAYINT(arrname,i)     g_service_log.printf( #arrname "[%d]=%d\n", i, (arrname)[i])


/* struct: log_interface_t
 * */
struct log_interface_t
{
   // group: mutator
   /* variable: printf
    * Print formatted output to log stream. */
   void (*printf) (const char * format, ... ) __attribute__ ((__format__ (__printf__, 1, 2))) ;
   /* variable: config_onoff
    * Switches logging on (onoff==true) or off (onoff==false). */
   void (* const config_onoff)    (bool onoff) ;
   /* variable: config_buffered
    * Switches buffered mode on (bufferedstate==true) or off (bufferedstate==false). */
   void (* const config_buffered) (bool bufferedstate) ;
   /* variable: clearlogbuffer
    * Clears log buffer (sets length of logbuffer to 0). */
   void (* const clearlogbuffer)  (void) ;

   // group: query
   /* variable: getlogbuffer
    * Returns C-string of buffered log and its length. */
   void (* const getlogbuffer)    (/*out*/char ** buffer, /*out*/size_t * size) ;
   /* variable: isOn
    * Allows fast query of state which was set with <config_onoff>.
    * If logging is on isOn is set to *true* else *false*. */
   const bool isOn ;
   /* variable: isBuffered
    * Allows fast query of state which was set with <config_buffered>.
    * If buffering is on isBuffered is set to *true* else *false*. */
   const bool isBuffered ;
} ;


#endif
