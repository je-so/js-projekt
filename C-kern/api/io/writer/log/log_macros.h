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

// group: query

/* define: GETBUFFER_LOG
 * Returns C-string of buffered log and its length.
 * See also <getbuffer_logwriter>.
 *
 * Parameter:
 * LOGCHANNEL - The number of the log channel - see <log_channel_e>.
 * buffer     - Contains pointer to C string after return. Must have type (char**).
 *              The string is terminated with a 0 byte.
 * size       - Contains size of of C string after return. The does does not include the 0 byte. */
#define GETBUFFER_LOG(LOGCHANNEL, /*out char ** */buffer, /*out size_t * */size) \
         log_maincontext().iimpl->getbuffer(log_maincontext().object, LOGCHANNEL, buffer, size)

/* define: COMPARE_LOG
 * Compare logbuffer[size] to buffered log entries.
 * Returns 0 if they are equal (timestamps are ignored during comparison).
 * See also <compare_logwriter>.
 *
 * Parameter:
 * LOGCHANNEL - The number of the log channel - see <log_channel_e>.
 * logsize    - Contains size of logbuffer.
 * buffer     - Contains pointer to the logbuffer in memory which is compared to the internal buffer. */
#define COMPARE_LOG(LOGCHANNEL, /*size_t*/size, /*const char[size]*/logbuffer) \
         log_maincontext().iimpl->compare(log_maincontext().object, LOGCHANNEL, size, logbuffer)

/* define: GETSTATE_LOG
 * Returns <log_state_e> for LOGCHANNEL.
 *
 * Parameter:
 * LOGCHANNEL - The number of the log channel - see <log_channel_e>. */
#define GETSTATE_LOG(LOGCHANNEL) \
         log_maincontext().iimpl->getstate(log_maincontext().object, LOGCHANNEL)

// group: change

/* define: TRUNCATEBUFFER_LOG
 * Sets length of logbuffer to a smaller size. See also <truncatebuffer_logwriter>. */
#define TRUNCATEBUFFER_LOG(LOGCHANNEL, /*size_t*/size)  \
         log_maincontext().iimpl->truncatebuffer(log_maincontext().object, LOGCHANNEL, size)

/* define: FLUSHBUFFER_LOG
 * Writes content of internal buffer and then clears it. See also <flushbuffer_logwriter>. */
#define FLUSHBUFFER_LOG(LOGCHANNEL)  \
         log_maincontext().iimpl->flushbuffer(log_maincontext().object, LOGCHANNEL)

/* define: SETSTATE_LOG
 * Sets LOGSTATE for LOGCHANNEL.
 *
 * Parameter:
 * LOGCHANNEL - The number of the log channel - see <log_channel_e>.
 * LOGSTATE   - The state of the LOGCHANNEL which will be set - see <log_state_e>. */
#define SETSTATE_LOG(LOGCHANNEL, LOGSTATE) \
         log_maincontext().iimpl->setstate(log_maincontext().object, LOGCHANNEL, LOGSTATE)

// group: log-text

/* define: PRINTF_LOG
 * Logs a generic printf type format string.
 *
 * Parameter:
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * HEADER     - The pointer to a struct of type <log_header_t>. Could be set to 0 if no header should be printed.
 * FORMAT     - The format string as in the standard library function printf.
 * ...        - Additional value parameters of the correct type as determined by the <FORMAT>
 *              parameter.
 *
 * Example:
 * > int i ; PRINTF_LOG(log_channel_ERR, log_flags_NONE, 0, "%d", i) */
#define PRINTF_LOG(LOGCHANNEL, FLAGS, HEADER, ... )  \
         do {                                \
            log_maincontext().iimpl->printf( \
               log_maincontext().object,     \
               LOGCHANNEL, FLAGS, HEADER,    \
               __VA_ARGS__ ) ;               \
         } while(0)

/* define: PRINTTEXT_LOG
 * Logs a text resource string.
 *
 * Parameter:
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - The name of the text resource, for example FUNCTION_ABORT_ERRLOG.
 * HEADER     - The pointer to a struct of type <log_header_t>. Could be set to 0 if no header should be printed.
 * ...        - Additional value parameters of the correct type as determined by the text resource.
 *
 * Example:
 * > int err ; PRINTTEXT_LOG(log_channel_ERR, log_flags_NONE, 0, &vRESOURCE_NAME_ERRLOG, err) */
#define PRINTTEXT_LOG(LOGCHANNEL, FLAGS, HEADER, TEXTID, ... )  \
         do {                                   \
            if (0) {                            \
               /* test for correct parameter */ \
               TEXTID(                          \
                  (struct logbuffer_t*)0,       \
                  ## __VA_ARGS__);              \
            }                                   \
            log_maincontext().iimpl->printtext( \
               log_maincontext().object,        \
               LOGCHANNEL, FLAGS, HEADER,       \
               & v ## TEXTID, __VA_ARGS__);     \
         } while(0)

/* define: PRINTTEXT_NOARG_LOG
 * Logs a text resource string.
 *
 * Parameter:
 * LOGCHANNEL - The name of the channel where the log is written to. See <log_channel_e>.
 * FLAGS      - Additional flags to control the logging process. See <log_flags_e>.
 * TEXTID     - The name of the text resource, for example FUNCTION_ABORT_ERRLOG.
 * HEADER     - The pointer to a struct of type <log_header_t>. Could be set to 0 if no header should be printed.
 *
 * Example:
 * > int err ; PRINTTEXT_NOARG_LOG(log_channel_ERR, log_flags_NONE, 0, &vRESOURCE_NAME_ERRLOG) */
#define PRINTTEXT_NOARG_LOG(LOGCHANNEL, FLAGS, HEADER, TEXTID)  \
         do {                                   \
            if (0) {                            \
            /* test for correct parameter */    \
            TEXTID(                             \
               (struct logbuffer_t*)0) ;        \
            }                                   \
            log_maincontext().iimpl->printtext( \
               log_maincontext().object,        \
               LOGCHANNEL, FLAGS, HEADER,       \
               & v ## TEXTID) ;                 \
         } while(0)

// group: log-variables

/* define: PRINTVAR_LOG
 * Logs "<varname>=varvalue" of a variable with name varname.
 *
 * Parameter:
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
#define PRINTVAR_LOG(LOGCHANNEL, format, varname, cast) \
         PRINTF_LOG(LOGCHANNEL, log_flags_NONE, 0, #varname "=%" format "\n", cast (varname))

/* define: PRINTARRAYFIELD_LOG
 * Log value of variable stored in array at offset i.
 * The logged text is "array[i]=value".
 *
 * Parameter:
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
#define PRINTARRAYFIELD_LOG(LOGCHANNEL, format, arrname, index)  \
         PRINTF_LOG(LOGCHANNEL, log_flags_NONE, 0, #arrname "[%d]=%" format "\n", i, (arrname)[i])

/* define: PRINTCSTR_LOG
 * Log "name=value" of string variable.
 * Example:
 * > const char * name = "Jo" ; PRINTCSTR_LOG(log_channel_ERR, name) ; */
#define PRINTCSTR_LOG(LOGCHANNEL, varname)  \
         PRINTVAR_LOG(LOGCHANNEL, "s", varname, )

/* define: PRINTINT_LOG
 * Log "name=value" of int variable.
 * Example:
 * > const int max = 100 ; PRINTINT_LOG(log_channel_ERR, max) ; */
#define PRINTINT_LOG(LOGCHANNEL, varname)   \
         PRINTVAR_LOG(LOGCHANNEL, "d", varname, )

/* define: PRINTINT64_LOG
 * Log "name=value" of int64_t variable.
 * Example:
 * > const int64_t min = -100 ; PRINTINT64_LOG(log_channel_ERR, min) ; */
#define PRINTINT64_LOG(LOGCHANNEL, varname) \
         PRINTVAR_LOG(LOGCHANNEL, PRId64, varname, )

/* define: PRINTSIZE_LOG
 * Log "name=value" of size_t variable.
 * Example:
 * > const size_t maxsize = 100 ; PRINTSIZE_LOG(log_channel_ERR, maxsize) ; */
#define PRINTSIZE_LOG(LOGCHANNEL, varname)  \
         PRINTVAR_LOG(LOGCHANNEL, PRIuSIZE, varname, )

/* define: PRINTUINT8_LOG
 * Log "name=value" of uint8_t variable.
 * Example:
 * > const uint8_t limit = 255 ; PRINTUINT8_LOG(log_channel_ERR, limit) ; */
#define PRINTUINT8_LOG(LOGCHANNEL, varname) \
         PRINTVAR_LOG(LOGCHANNEL, PRIu8, varname, )

/* define: PRINTUINT16_LOG
 * Log "name=value" of uint16_t variable.
 * Example:
 * > const uint16_t limit = 65535 ; PRINTUINT16_LOG(log_channel_ERR, limit) ; */
#define PRINTUINT16_LOG(LOGCHANNEL, varname)   \
         PRINTVAR_LOG(LOGCHANNEL, PRIu16, varname, )

/* define: PRINTUINT32_LOG
 * Log "name=value" of uint32_t variable.
 * Example:
 * > const uint32_t max = 100 ; PRINTUINT32_LOG(log_channel_ERR, max) ; */
#define PRINTUINT32_LOG(LOGCHANNEL, varname)   \
         PRINTVAR_LOG(LOGCHANNEL, PRIu32, varname, )

/* define: PRINTUINT64_LOG
 * Log "name=value" of uint64_t variable.
 * Example:
 * > const uint64_t max = 100 ; PRINTUINT64_LOG(log_channel_ERR, max) ; */
#define PRINTUINT64_LOG(LOGCHANNEL, varname)   \
         PRINTVAR_LOG(LOGCHANNEL, PRIu64, varname, )

/* define: PRINTPTR_LOG
 * Log "name=value" of pointer variable.
 * Example:
 * > const void * ptr = &g_variable ; PRINTPTR_LOG(log_channel_ERR, ptr) ; */
#define PRINTPTR_LOG(LOGCHANNEL, varname)   \
         PRINTVAR_LOG(LOGCHANNEL, "p", varname, (const void*))

/* define: PRINTDOUBLE_LOG
 * Log "name=value" of double or float variable.
 * Example:
 * > const double d = 1.234 ; PRINTDOUBLE_LOG(log_channel_ERR, d) ; */
#define PRINTDOUBLE_LOG(LOGCHANNEL, varname)   \
         PRINTVAR_LOG(LOGCHANNEL, "g", varname, )

#endif
