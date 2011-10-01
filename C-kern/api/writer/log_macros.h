/* title: LogMacros
   Makes <LogWriter> more accessible with simple defined macros.

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

   file: C-kern/api/writer/log_macros.h
    Header file of <LogMacros>.
*/
#ifndef CKERN_WRITER_LOG_MACROS_HEADER
#define CKERN_WRITER_LOG_MACROS_HEADER

#include "C-kern/api/writer/log.h"

/* about: LOGCHANNEL
 * The parameter LOGCHANNEL is the first parameter of log writing macros.
 *
 * Supported values:
 * ERR  - Writes error log to current <log_umgebung>.
 * TEST - Writes to STDOUT channel used for test output when (unit-) tests are run.
 * */

// section: Functions

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

// group: config

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

// group: change

/* define: LOG_CLEARBUFFER
 * Clears log buffer (sets length of logbuffer to 0). See also <clearbuffer_logconfig>.
 * > #define  LOG_CLEARBUFFER()     clearbuffer_logconfig(log_umgebung()) */
#define  LOG_CLEARBUFFER()          clearbuffer_logconfig(log_umgebung())

/* define: LOG_WRITEBUFFER
 * Writes content of log buffer and then clears it. See also <writebuffer_logconfig>.
 * > #define  LOG_WRITEBUFFER()     writebuffer_logconfig(log_umgebung()) */
#define  LOG_WRITEBUFFER()          writebuffer_logconfig(log_umgebung())

// group: write-text

/* define: LOGC_PRINTF
 * Logs a generic printf type format string.
 *
 * Parameter:
 * LOGCHANNEL - The name of the channel where the log is written to. See <LOGCHANNEL>.
 * FORMAT     - The format string as in the standard library function printf.
 * ...        - Additional value parameters of the correct type as determined by the <FORMAT>
 *              parameter.
 *
 * Example:
 * > int i ; LOGC_PRINTF(ERR, "%d", i) */
#define LOGC_PRINTF(LOGCHANNEL, ... )   \
   do {                                \
      switch(CONCAT(log_channel_,LOGCHANNEL)) {                   \
      case log_channel_ERR:                                       \
         log_umgebung()->printf( log_umgebung(), __VA_ARGS__ ) ;  \
         break ;                                                  \
      case log_channel_TEST:                                      \
         printf( __VA_ARGS__ ) ;                                  \
         break ;                                                  \
      }                                                           \
   } while(0)

/* define: LOGC_TEXTRES
 * Logs text resource produced by resource text compiler.
 *
 * Parameter:
 * LOGCHANNEL - The name of the channel where the log is written to. See <LOGCHANNEL>.
 * TEXTID     - ID of a localized text string
 * ...        - Additional value parameters of the correct type as determined by the <TEXTID>
 *              parameter.
 *
 * Example:
 * > LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_MEMORY_OUT_OF(size)) */
#define LOGC_TEXTRES(LOGCHANNEL, TEXTID)      LOGC_PRINTF(LOGCHANNEL, TEXTID)

// group: write-variables

/* define: LOGC_VAR
 * Logs "<varname>=varvalue" of a variable with name varname.
 *
 * Parameter:
 * LOGCHANNEL  - The name of the channel where the log is written to. See <LOGCHANNEL>.
 * printf_type - Type of the variable as string in printf format. Use "d" for signed int or "u" for unsigned int.
 *               Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * varname     - The name of the variable to log.
 *
 * Example:
 * This code logs "memsize=1024\n"
 * > const size_t memsize = 1024 ;
 * > LOGC_VAR(ERR, PRIuSIZE,memsize) ; */
#define LOGC_VAR(LOGCHANNEL, printf_type, varname)     LOGC_PRINTF(LOGCHANNEL, #varname "=%" printf_type "\n", (varname))

/* define: LOGC_INDEX
 * Log "array[i]=value" of variable stored in array at offset i.
 *
 * Parameter:
 * LOGCHANNEL  - The name of the channel where the log is written to. See <LOGCHANNEL>.
 * printf_type - Type of the variable as string in printf format. Use "s" for C strings.
 *               Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * arrname     - The name of the array.
 * index       - The index of the array entry whose value is to be logged.
 *
 * Example:
 * This code logs "names[0]=Jo\n" and "names[1]=Jane\n".
 * > const char * names[] = { "Jo", "Jane" } ;
 * > for(int i = 0; i < 2; ++i) { LOGC_INDEX(ERR, s,names,i) ; } */
#define LOGC_INDEX(LOGCHANNEL, printf_type, arrname, index)  LOGC_PRINTF(LOGCHANNEL, #arrname "[%d]=%" printf_type "\n", i, (arrname)[i])

/* define: LOGC_STRING
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; LOGC_STRING(ERR, name) ; */
#define LOGC_STRING(LOGCHANNEL, varname)     LOGC_VAR(ERR, "s", varname)

/* define: LOGC_INT
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; LOGC_INT(ERR, max) ; */
#define LOGC_INT(LOGCHANNEL, varname)        LOGC_VAR(ERR, "d", varname)

/* define: LOGC_SIZE
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; LOGC_SIZE(ERR, maxsize) ; */
#define LOGC_SIZE(LOGCHANNEL, varname)       LOGC_VAR(ERR, PRIuSIZE, varname)

/* define: LOGC_UINT8
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; LOGC_UINT8(ERR, limit) ; */
#define LOGC_UINT8(LOGCHANNEL, varname)      LOGC_VAR(ERR, PRIu8, varname)

/* define: LOGC_UINT16
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; LOGC_UINT16(ERR, limit) ; */
#define LOGC_UINT16(LOGCHANNEL, varname)     LOGC_VAR(ERR, PRIu16, varname)

/* define: LOGC_UINT32
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; LOGC_UINT32(ERR, max) ; */
#define LOGC_UINT32(LOGCHANNEL, varname)     LOGC_VAR(ERR, PRIu32, varname)

/* define: LOGC_UINT64
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; LOGC_UINT64(ERR, max) ; */
#define LOGC_UINT64(LOGCHANNEL, varname)     LOGC_VAR(ERR, PRIu64, varname)

/* define: LOGC_PTR
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; LOGC_PTR(ERR, ptr) ; */
#define LOGC_PTR(LOGCHANNEL, varname)        LOGC_VAR(ERR, "p", varname)

/* define: LOGC_DOUBLE
 * Log "name=value" of double or float variable.
 * Example:
 * > const double d = 1.234 ; LOGC_DOUBLE(ERR, d) ; */
#define LOGC_DOUBLE(LOGCHANNEL, varname)     LOGC_VAR(ERR, "g", varname)

#endif
