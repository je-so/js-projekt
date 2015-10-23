/* title: LogMacros
   Makes <LogWriter> service more accessible with simple defined macros.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/writer/log/log_macros.h
    Header file of <LogMacros>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOG_MACROS_HEADER
#define CKERN_IO_WRITER_LOG_LOG_MACROS_HEADER

#include "C-kern/api/io/writer/log/log.h"


// section: Functions

// group: log-service-binding
// Defines macros CALLFUNCTION_LOG... to bind the macro to a log service object
// The function <CALLFUNCTION_LOG> is the default binding during normal operation.
// The macro <CALLFUNCTION_LOGINIT> binds the macros to the init log service.
// The macro <CALLFUNCTION_LOGAUTO> uses <CALLFUNCTION_LOG> if <maincontext_t> is
// initialized else <CALLFUNCTION_LOGINIT>.
// To select between the three bindings set parameter BIND to
// INIT for CALLFUNCTION_LOGINIT, AUTO for CALLFUNCTION_LOGAUTO and
// leave it empty for CALLFUNCTION_LOG.

/* define: CALLFUNCTION_LOG
 * Calls the function fname on the default log object.
 * The default log object is obtained with <maincontext_t.log_maincontext>. */
#define CALLFUNCTION_LOG(fname, ...) \
         ( __extension__ ({                                    \
            threadcontext_log_t _iobj = log_maincontext();     \
            _iobj.iimpl-> fname (_iobj.object, __VA_ARGS__);   \
         }))

/* define: CALLFUNCTION_LOGINIT
 * Calls the function fname on the init log object.
 * The init log object is obtained with g_maincontext.initlog.
 * This binding does not work in a module environment. */
#define CALLFUNCTION_LOGINIT(fname, ...) \
         fname ## _logwriter(g_maincontext.initlog, __VA_ARGS__)

/* define: CALLFUNCTION_LOGAUTO
 * Calls the function fname on either the init log object or defaukt log object.
 * This binding does not work in a module environment. */
#define CALLFUNCTION_LOGAUTO(fname, ...) \
         if (maincontext_STATIC != g_maincontext.type) { \
            CALLFUNCTION_LOG(fname, __VA_ARGS__);        \
         } else {                                        \
            CALLFUNCTION_LOGINIT(fname, __VA_ARGS__);    \
         }

// group: query

/* define: GETBUFFER_LOG
 * Returns C-string of buffered log and its length.
 * See also <getbuffer_logwriter>.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The number of the log channel - see <log_channel_e>.
 * buffer     - Contains pointer to C string after return. Must have type (char**).
 *              The string is terminated with a 0 byte.
 * size       - Contains size of of C string after return. The does does not include the 0 byte. */
#define GETBUFFER_LOG(BIND, LOGCHANNEL, /*out char ** */buffer, /*out size_t * */size) \
         CALLFUNCTION_LOG ## BIND(getbuffer, LOGCHANNEL, buffer, size)

/* define: COMPARE_LOG
 * Compare logbuffer[size] to buffered log entries.
 * Returns 0 if they are equal (timestamps are ignored during comparison).
 * See also <compare_logwriter>.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The number of the log channel - see <log_channel_e>.
 * logsize    - Contains size of logbuffer.
 * buffer     - Contains pointer to the logbuffer in memory which is compared to the internal buffer. */
#define COMPARE_LOG(BIND, LOGCHANNEL, /*size_t*/size, /*const char[size]*/logbuffer) \
         CALLFUNCTION_LOG ## BIND(compare, LOGCHANNEL, size, logbuffer)

/* define: GETSTATE_LOG
 * Returns <log_state_e> for LOGCHANNEL.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The number of the log channel - see <log_channel_e>. */
#define GETSTATE_LOG(BIND, LOGCHANNEL) \
         CALLFUNCTION_LOG ## BIND(getstate, LOGCHANNEL)

// group: change

/* define: TRUNCATEBUFFER_LOG
 * Sets length of logbuffer to a smaller size. See also <truncatebuffer_logwriter>. */
#define TRUNCATEBUFFER_LOG(BIND, LOGCHANNEL, /*size_t*/size)  \
         CALLFUNCTION_LOG ## BIND(truncatebuffer, LOGCHANNEL, size)

/* define: FLUSHBUFFER_LOG
 * Writes content of internal buffer and then clears it. See also <flushbuffer_logwriter>. */
#define FLUSHBUFFER_LOG(BIND, LOGCHANNEL)  \
         CALLFUNCTION_LOG ## BIND(flushbuffer, LOGCHANNEL)

/* define: SETSTATE_LOG
 * Sets LOGSTATE for LOGCHANNEL.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The number of the log channel - see <log_channel_e>.
 * LOGSTATE   - The state of the LOGCHANNEL which will be set - see <log_state_e>. */
#define SETSTATE_LOG(BIND, LOGCHANNEL, LOGSTATE) \
         CALLFUNCTION_LOG ## BIND(setstate, LOGCHANNEL, LOGSTATE)

// group: log-text

/* define: PRINTF_LOG
 * Logs a generic printf type format string.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * HEADER     - The pointer to a struct of type <log_header_t>. Could be set to 0 if no header should be printed.
 * FORMAT     - The format string as in the standard library function printf.
 * ...        - Additional value parameters of the correct type as determined by the <FORMAT>
 *              parameter.
 *
 * Example:
 * > int i ; PRINTF_LOG(log_channel_ERR, log_flags_NONE, 0, "%d", i) */
#define PRINTF_LOG(BIND, LOGCHANNEL, FLAGS, HEADER, ... )  \
         do {                                \
            CALLFUNCTION_LOG ## BIND(        \
               printf,                       \
               LOGCHANNEL, FLAGS, HEADER,    \
               __VA_ARGS__ );                \
         } while(0)

/* define: PRINTTEXT_LOG
 * Logs a text resource string.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - The name of the text resource, for example FUNCTION_ABORT_ERRLOG.
 * HEADER     - The pointer to a struct of type <log_header_t>. Could be set to 0 if no header should be printed.
 * ...        - Additional value parameters of correct type as determined by the text resource.
 *
 * Example:
 * > int err ; PRINTTEXT_LOG(log_channel_ERR, log_flags_NONE, 0, &vRESOURCE_NAME_ERRLOG, err) */
#define PRINTTEXT_LOG(BIND, LOGCHANNEL, FLAGS, HEADER, TEXTID, ...) \
         do {                                   \
            struct p_ ## TEXTID                 \
               _p = { __VA_ARGS__ };            \
            CALLFUNCTION_LOG ## BIND(           \
               printtext,                       \
               LOGCHANNEL, FLAGS, HEADER,       \
               & TEXTID, &_p);                  \
         } while(0)

/* define: PRINTTEXT_NOARG_LOG
 * Logs a text resource string.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - The name of the text resource, for example FUNCTION_ABORT_ERRLOG.
 * HEADER     - The pointer to a struct of type <log_header_t>. Could be set to 0 if no header should be printed.
 *
 * Example:
 * > int err ; PRINTTEXT_NOARG_LOG(log_channel_ERR, log_flags_NONE, 0, &vRESOURCE_NAME_ERRLOG) */
#define PRINTTEXT_NOARG_LOG(BIND, LOGCHANNEL, FLAGS, HEADER, TEXTID)  \
         do {                                   \
            /* test for no parameter */         \
            ( p_noarg_ ## TEXTID) 0;            \
            CALLFUNCTION_LOG ## BIND(           \
               printtext,                       \
               LOGCHANNEL, FLAGS, HEADER,       \
               & TEXTID, 0);                    \
         } while(0)

/* define: TRACE_LOG
 * Logs any TEXTID and a header.
 * Calls <TRACE2_LOG> with parameters funcname, filename, and linenr
 * set to __FUNCTION__, __FILE__, and __LINE__ respectively.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - The name of the text resource, for example FUNCTION_ABORT_ERRLOG.
 * ...        - Additional value parameters of correct type as determined by the text resource. */
#define TRACE_LOG(BIND, LOGCHANNEL, FLAGS, TEXTID, ...)   \
         TRACE2_LOG(BIND, LOGCHANNEL, FLAGS, TEXTID, __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

/* define: TRACE2_LOG
 * Logs any TEXTID and a header.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - The name of the text resource, for example FUNCTION_ABORT_ERRLOG.
 * funcname   - Name of function - used to describe error position.
 * filename   - Name of source file - used to describe error position.
 * linenr     - Number of current source line - used to describe error position.
 * ...        - The following parameter are used to parameterize TEXTID. */
#define TRACE2_LOG(BIND, LOGCHANNEL, FLAGS, TEXTID, funcname, filename, linenr, ...) \
         do {                                \
            log_header_t _header =           \
               log_header_INIT(funcname,     \
                  filename, linenr);         \
            PRINTTEXT_LOG(BIND,              \
               LOGCHANNEL, FLAGS,            \
               &_header, TEXTID,             \
               __VA_ARGS__);                 \
         }  while(0)

/* define: TRACE_NOARG_LOG
 * Logs any TEXTID and a header.
 * Use this macro to log any language specific text which needs no additional parameters.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - Error text ID from C-kern/resource/errlog.text. */
#define TRACE_NOARG_LOG(BIND, LOGCHANNEL, FLAGS, TEXTID) \
         do {                                \
            log_header_t _header =           \
               log_header_INIT(__FUNCTION__, \
                  __FILE__, __LINE__);       \
            PRINTTEXT_NOARG_LOG(BIND,        \
               LOGCHANNEL, FLAGS,            \
               &_header, TEXTID);            \
         }  while(0)


// group: log-variables

/* define: PRINTVAR_LOG
 * Logs "<varname>=varvalue" of a variable with name varname.
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * format     - Type of the variable as string in printf format. Use "d" for signed int or "u" for unsigned int.
 *              Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * varname    - The name of the variable to log.
 * cast       - A type cast expression like "(void*)" without enclosing "". Use this to adapt the type of the variable.
 *              Leave it empty if you do not need a type cast.
 *
 * Example:
 * This code logs "memsize=1024\n"
 * > const size_t memsize = 1024 ;
 * > PRINTVAR_LOG(log_channel_ERR, PRIuSIZE, memsize, ) ; */
#define PRINTVAR_LOG(BIND, LOGCHANNEL, format, varname, cast) \
         PRINTF_LOG(BIND, LOGCHANNEL, log_flags_NONE, 0, #varname "=%" format "\n", cast (varname))

/* define: PRINTARRAYFIELD_LOG
 * Log value of variable stored in array at offset i.
 * The logged text is "array[i]=value".
 *
 * Parameter:
 * BIND       - Chooses binding to log service. Leave it empty for default service or use INIT for init log.
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * format     - Type of the variable as string in printf format. Use "s" for C strings.
 *              Use the C99 standard conforming names PRIx64 for hexadecimal output of uint64_t/int64_t ...
 * arrname    - The name of the array.
 * index      - The index of the array entry whose value is to be logged.
 *
 * Example:
 * This code logs "names[0]=Jo\n" and "names[1]=Jane\n".
 * > const char * names[] = { "Jo", "Jane" } ;
 * > for(int i = 0; i < 2; ++i) { PRINTARRAYFIELD_LOG(log_channel_ERR, s,names,i) ; } */
#define PRINTARRAYFIELD_LOG(BIND, LOGCHANNEL, format, arrname, index)  \
         PRINTF_LOG(BIND, LOGCHANNEL, log_flags_NONE, 0, #arrname "[%d]=%" format "\n", i, (arrname)[i])

/* define: PRINTCSTR_LOG
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; PRINTCSTR_LOG(log_channel_ERR, name) ; */
#define PRINTCSTR_LOG(BIND, LOGCHANNEL, varname)  \
         PRINTVAR_LOG(BIND, LOGCHANNEL, "s", varname, )

/* define: PRINTINT_LOG
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; PRINTINT_LOG(log_channel_ERR, max) ; */
#define PRINTINT_LOG(BIND, LOGCHANNEL, varname)   \
         PRINTVAR_LOG(BIND, LOGCHANNEL, "d", varname, )

/* define: PRINTINT64_LOG
 * Log "name=value" of int64_t variable.
 * Example:
 * > const int64_t min = -100 ; PRINTINT64_LOG(log_channel_ERR, min) ; */
#define PRINTINT64_LOG(BIND, LOGCHANNEL, varname) \
         PRINTVAR_LOG(BIND, LOGCHANNEL, PRId64, varname, )

/* define: PRINTSIZE_LOG
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; PRINTSIZE_LOG(log_channel_ERR, maxsize) ; */
#define PRINTSIZE_LOG(BIND, LOGCHANNEL, varname)  \
         PRINTVAR_LOG(BIND, LOGCHANNEL, PRIuSIZE, varname, )

/* define: PRINTUINT8_LOG
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; PRINTUINT8_LOG(log_channel_ERR, limit) ; */
#define PRINTUINT8_LOG(BIND, LOGCHANNEL, varname) \
         PRINTVAR_LOG(BIND, LOGCHANNEL, PRIu8, varname, )

/* define: PRINTUINT16_LOG
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; PRINTUINT16_LOG(log_channel_ERR, limit) ; */
#define PRINTUINT16_LOG(BIND, LOGCHANNEL, varname)   \
         PRINTVAR_LOG(BIND, LOGCHANNEL, PRIu16, varname, )

/* define: PRINTUINT32_LOG
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; PRINTUINT32_LOG(log_channel_ERR, max) ; */
#define PRINTUINT32_LOG(BIND, LOGCHANNEL, varname)   \
         PRINTVAR_LOG(BIND, LOGCHANNEL, PRIu32, varname, )

/* define: PRINTUINT64_LOG
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; PRINTUINT64_LOG(log_channel_ERR, max) ; */
#define PRINTUINT64_LOG(BIND, LOGCHANNEL, varname)   \
         PRINTVAR_LOG(BIND, LOGCHANNEL, PRIu64, varname, )

/* define: PRINTPTR_LOG
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; PRINTPTR_LOG(log_channel_ERR, ptr) ; */
#define PRINTPTR_LOG(BIND, LOGCHANNEL, varname)   \
         PRINTVAR_LOG(BIND, LOGCHANNEL, "p", varname, (const void*))

/* define: PRINTDOUBLE_LOG
 * Log "name=value" of double or float variable.
 * Example:
 * > const double d = 1.234 ; PRINTDOUBLE_LOG(log_channel_ERR, d) ; */
#define PRINTDOUBLE_LOG(BIND, LOGCHANNEL, varname)   \
         PRINTVAR_LOG(BIND, LOGCHANNEL, "g", varname, )

#endif
