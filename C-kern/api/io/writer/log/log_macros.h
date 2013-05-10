/* title: LogMacros
   Makes <LogWriter> service more accessible with simple defined macros.

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

   file: C-kern/api/io/writer/log/log_macros.h
    Header file of <LogMacros>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOG_MACROS_HEADER
#define CKERN_IO_WRITER_LOG_LOG_MACROS_HEADER

#include "C-kern/api/io/writer/log/log.h"


// section: Functions

// group: query

/* define: GETBUFFER_LOG
 * Returns C-string of buffered log and its length. See also <getbuffer_logwriter>.
 * > #define GETBUFFER_LOG(buffer, size) getbuffer_logwritermt(log_maincontext(), buffer, size) */
#define GETBUFFER_LOG(/*out char ** */buffer, /*out size_t * */size) \
         log_maincontext().iimpl->getbuffer(log_maincontext().object, buffer, size)

// group: change

/* define: CLEARBUFFER_LOG
 * Clears log buffer (sets length of logbuffer to 0). See also <clearbuffer_logwriter>. */
#define CLEARBUFFER_LOG()  \
         log_maincontext().iimpl->clearbuffer(log_maincontext().object)

/* define: FLUSHBUFFER_LOG
 * Writes content of internal buffer and then clears it. See also <flushbuffer_logwriter>. */
#define FLUSHBUFFER_LOG()  \
         log_maincontext().iimpl->flushbuffer(log_maincontext().object)

// group: log-text

/* about: LOGCHANNEL
 * The parameter LOGCHANNEL is the first parameter of all log macros writing text.
 *
 * See also <log_channel_e>.
 *
 * Supported values:
 * ERR  - Writes error log to current <log_maincontext>.
 * TEST - Writes to STDOUT channel used for test output when (unit-) tests are run.
 * */

/* define: CPRINTF_LOG
 * Logs a generic printf type format string.
 *
 * Parameter:
 * LOGCHANNEL - The name of the channel where the log is written to. See <LOGCHANNEL>.
 * FORMAT     - The format string as in the standard library function printf.
 * ...        - Additional value parameters of the correct type as determined by the <FORMAT>
 *              parameter.
 *
 * Example:
 * > int i ; CPRINTF_LOG(ERR, "%d", i) */
#define CPRINTF_LOG(LOGCHANNEL, ... )              \
   do {                                            \
         log_maincontext().iimpl->printf(          \
               log_maincontext().object,           \
               CONCAT(log_channel_,LOGCHANNEL),    \
               __VA_ARGS__ ) ; \
   } while(0)

// group: log-variables

/* define: CPRINTVAR_LOG
 * Logs "<varname>=varvalue" of a variable with name varname.
 *
 * Parameter:
 * LOGCHANNEL  - The name of the channel where the log is written to. See <LOGCHANNEL>.
 * format      - Type of the variable as string in printf format. Use "d" for signed int or "u" for unsigned int.
 *               Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * varname     - The name of the variable to log.
 * cast        - A type cast expression like "(void*)" without enclosing "". Use this to adapt the type of the variable.
 *               Leave it empty if you do not need a type cast.
 *
 * Example:
 * This code logs "memsize=1024\n"
 * > const size_t memsize = 1024 ;
 * > CPRINTVAR_LOG(ERR, PRIuSIZE, memsize, ) ; */
#define CPRINTVAR_LOG(LOGCHANNEL, format, varname, cast)          CPRINTF_LOG(LOGCHANNEL, #varname "=%" format "\n", cast (varname))

/* define: CPRINTARRAYFIELD_LOG
 * Log value of variable stored in array at offset i.
 * The logged text is "array[i]=value".
 *
 * Parameter:
 * LOGCHANNEL  - The name of the channel where the log is written to. See <LOGCHANNEL>.
 * format      - Type of the variable as string in printf format. Use "s" for C strings.
 *               Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * arrname     - The name of the array.
 * index       - The index of the array entry whose value is to be logged.
 *
 * Example:
 * This code logs "names[0]=Jo\n" and "names[1]=Jane\n".
 * > const char * names[] = { "Jo", "Jane" } ;
 * > for(int i = 0; i < 2; ++i) { CPRINTARRAYFIELD_LOG(ERR, s,names,i) ; } */
#define CPRINTARRAYFIELD_LOG(LOGCHANNEL, format, arrname, index)  CPRINTF_LOG(LOGCHANNEL, #arrname "[%d]=%" format "\n", i, (arrname)[i])

/* define: CPRINTCSTR_LOG
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; CPRINTCSTR_LOG(ERR, name) ; */
#define CPRINTCSTR_LOG(LOGCHANNEL, varname)     CPRINTVAR_LOG(LOGCHANNEL, "s", varname, )

/* define: CPRINTINT_LOG
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; CPRINTINT_LOG(ERR, max) ; */
#define CPRINTINT_LOG(LOGCHANNEL, varname)      CPRINTVAR_LOG(LOGCHANNEL, "d", varname, )

/* define: CPRINTINT64_LOG
 * Log "name=value" of int64_t variable.
 * Example:
 * > const int64_t min = -100 ; CPRINTINT64_LOG(ERR, min) ; */
#define CPRINTINT64_LOG(LOGCHANNEL, varname)    CPRINTVAR_LOG(LOGCHANNEL, PRId64, varname, )

/* define: CPRINTSIZE_LOG
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; CPRINTSIZE_LOG(ERR, maxsize) ; */
#define CPRINTSIZE_LOG(LOGCHANNEL, varname)     CPRINTVAR_LOG(LOGCHANNEL, PRIuSIZE, varname, )

/* define: CPRINTUINT8_LOG
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; CPRINTUINT8_LOG(ERR, limit) ; */
#define CPRINTUINT8_LOG(LOGCHANNEL, varname)    CPRINTVAR_LOG(LOGCHANNEL, PRIu8, varname, )

/* define: CPRINTUINT16_LOG
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; CPRINTUINT16_LOG(ERR, limit) ; */
#define CPRINTUINT16_LOG(LOGCHANNEL, varname)   CPRINTVAR_LOG(LOGCHANNEL, PRIu16, varname, )

/* define: CPRINTUINT32_LOG
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; CPRINTUINT32_LOG(ERR, max) ; */
#define CPRINTUINT32_LOG(LOGCHANNEL, varname)   CPRINTVAR_LOG(LOGCHANNEL, PRIu32, varname, )

/* define: CPRINTUINT64_LOG
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; CPRINTUINT64_LOG(ERR, max) ; */
#define CPRINTUINT64_LOG(LOGCHANNEL, varname)   CPRINTVAR_LOG(LOGCHANNEL, PRIu64, varname, )

/* define: CPRINTPTR_LOG
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; CPRINTPTR_LOG(ERR, ptr) ; */
#define CPRINTPTR_LOG(LOGCHANNEL, varname)      CPRINTVAR_LOG(LOGCHANNEL, "p", varname, (const void*))

/* define: CPRINTDOUBLE_LOG
 * Log "name=value" of double or float variable.
 * Example:
 * > const double d = 1.234 ; CPRINTDOUBLE_LOG(ERR, d) ; */
#define CPRINTDOUBLE_LOG(LOGCHANNEL, varname)   CPRINTVAR_LOG(LOGCHANNEL, "g", varname, )

#endif
