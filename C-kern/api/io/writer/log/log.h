/* title: Log-Object

   Interface to access log service. An interface is a structure which lists function pointers.
   These function pointers point to functions exported by a service implementation.
   Used in <LogWriter> and <LogMain>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/writer/log/log.h
    Header file of <Log-Object>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOG_HEADER
#define CKERN_IO_WRITER_LOG_LOG_HEADER

// forward
struct logbuffer_t;
struct log_header_t;

/* typedef: log_text_f
 * Declare function pointer which writes a text resource into <logbuffer_t>. */
typedef void (*log_text_f) (struct logbuffer_t * logbuffer, void * params);


/* enums: log_config_e
 * Used to configure system wide restrictions.
 *
 * log_config_MINSIZE - The minimum size in bytes of one log entry before it is possibly truncated.
 *                      If the buffer is bigger it is not truncated.
 *
 * */
typedef enum log_config_e {
   log_config_MINSIZE = 512
} log_config_e;


/* enums: log_flags_e
 * Controls additional information written in addition to a log message.
 *
 *
 * log_flags_NONE      - The partial log entry is appended to a memory buffer and truncated if necessary
 *                       until another call to print is done with flag set to <log_flags_LAST>.
 *                       If the <log_state_e> is set to <log_state_IMMEDIATE> the partial log entry
 *                       is written out immediately without waiting for its end.
 *                       A header is written if it is different then the last written header.
 *
 * log_flags_LAST      - Indicates that this is the last part of a log entry.
 *                       A header is written if it is different then the last written header.
 *                       The last header is reset to 0.
 *                       After a possible header the last part of the log entry is appended
 *                       to a memory buffer and truncated if necessary.
 *                       The log is written out if state is not <log_state_BUFFERED> or
 *                       the free buffersize is less than <log_config_MINSIZE>.
 *
 * */
typedef enum log_flags_e {
   log_flags_NONE = 0,
   log_flags_LAST = 1
} log_flags_e;


/* enums: log_channel_e
 * Used to switch between log channels.
 *
 * log_channel_USERERR - Uses STDERR channel for log output.
 *                       This channel is used for user error messages.
 *                       The logged content is written immediately to STDERR
 *                       without buffering. In case of a daemon process the
 *                       channel should be redirected to log_channel_ERR and/or
 *                       an additional entry into log_channel_ERR should be written.
 * log_channel_TEST    - Uses test channel for log output.
 *                       The test channel is used for additional TEST output in the running production system.
 *                       This channel is written to STDERR if not configured otherwise.
 * log_channel_WARN    - Uses warning channel for log output.
 *                       The warning channel is used to warn of non critical conditions.
 *                       This channel is written to STDERR if not configured otherwise.
 * log_channel_ERR     - Uses error channel for log output.
 *                       The error channel is used to log system error which should not occur
 *                       and which are not critical to the running process.
 *                       This channel is written to STDERR if not configured otherwise.
 * log_channel__NROF   - Use this value to determine the number of different channels.
 *                       They are numbered from 0 up to (log_channel__NROF-1).
 * */
typedef enum log_channel_e {
   log_channel_USERERR,
   log_channel_TEST,
   log_channel_WARN,
   log_channel_ERR
} log_channel_e;

#define log_channel__NROF (log_channel_ERR + 1)


/* enums: log_state_e
 * Used to configure the state of a <log_channel_e>.
 *
 * log_state_IGNORED    - Ignore any output to this channel.
 * log_state_BUFFERED   - Normal operation mode. All log entries are buffered until the buffer is full and then the buffer is written
 *                        all at once.
 * log_state_UNBUFFERED - Every log entry is constructed in a buffer and if the last part of the entry is added to it the log entry is
 *                        written out as a whole.
 * log_state_IMMEDIATE  - Every part of a log entry is written immediately without waiting for the last part.
 *
 * log_state__NROF      - Use this value to determine the number of different states
 *                        numbered from 0 up to (log_state__NROF-1).
 * */
typedef enum log_state_e {
   log_state_IGNORED    = 0,
   log_state_BUFFERED   = 1,
   log_state_UNBUFFERED = 2,
   log_state_IMMEDIATE  = 3,
} log_state_e;

#define log_state__NROF (log_state_IMMEDIATE + 1)


/* struct: log_it
 * The function table which describes the log service. */
typedef struct log_it {
   /* variable: printf
    * Writes a new log entry to in internal buffer.
    * Parameter channel must be set to a value from <log_channel_e>.
    * If header is not 0 a header is written which contains "[thread id, timestamp]\nfuncname() file:linenr\n, Error NR - DESCRIPTION\n".
    * If another call to <printf> or <printtext> contains the same <log_header_t.funcname> as the last header
    * the new header is ignored. If flags is set to <log_flags_LAST> the remembered header is reset to 0
    * so the next valid header is always printed.
    * If the entry is bigger than <log_config_MINSIZE> it may be truncated if internal buffer size is lower.
    * See <logwriter_t.printf_logwriter> for an implementation.
    * If format == 0 only the header is written. */
   void  (*printf)      (void * log, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, ... ) __attribute__ ((__format__ (__printf__, 5, 6)));
   /* variable: printtext
    * Writes text resource as new log entry to in internal buffer.
    * See <printf> for a description of the parameter. The variable parameter list must match the resource.
    * If textf == 0 only the header is written. */
   void  (*printtext)   (void * log, uint8_t channel, uint8_t flags, const struct log_header_t * header, log_text_f textf, void * params);
   /* variable: flushbuffer
    * Writes content of buffer to configured file descriptor and clears log buffer.
    * This call is ignored if buffer is empty or log is not configured to be in buffered mode.
    * See <logwriter_t.flushbuffer_logwriter> for an implementation. */
   void  (*flushbuffer) (void * log, uint8_t channel);
   /* variable: truncatebuffer
    * Sets length of logbuffer to size.
    * This call is ignored if the log is not configured to be in buffered mode
    * or if the current size of the buffered log is lower or equal to the value
    * of the parameter size.
    * See <logwriter_t.truncatebuffer_logwriter> for an implementation. */
   void  (*truncatebuffer) (void * log, uint8_t channel, size_t size);
   // -- query --
   /* variable: getbuffer
    * Returns content of log buffer as C-string and its size in bytes.
    * The returned values are valid as long as no other function is called on log.
    * The string has a trailing NULL byte, i.e. buffer[size] == 0.
    * See <logwriter_t.getbuffer_logwriter> for an implementation. */
   void  (*getbuffer)   (const void * log, uint8_t channel, /*out*/uint8_t ** buffer, /*out*/size_t * size);
   /* variable: getstate
    * Returns configured <log_state_e> for a specific <log_channel_e> channel.
    * You can switch <log_state_e> by calling <setstate>. */
   uint8_t (*getstate)  (const void * log, uint8_t channel);
   /* variable: compare
    * Returns 0 if logbuffer compares equal to content in log for a specific <log_channel_e> channel.
    * The return value EINVAL indicates not equal. The comparison should ignore timestamps. */
   int   (*compare)     (const void * log, uint8_t channel, size_t logsize, const uint8_t logbuffer[logsize]);
   // -- configuration --
   /* variable: setstate
    * Sets <log_state_e> logstate for a specific <log_channel_e> channel.
    * You can query the current <log_state_e> by calling <getstate>. */
   void  (*setstate)    (void * log, uint8_t channel, uint8_t logstate);
} log_it;

// group: generic

/* function: cast_logit
 * Casts pointer logif into pointer to interface <log_it>.
 * Parameter *logif* must point to a type declared with <log_it_DECLARE>.
 * The second parameter must be the same as the second one in <log_it_DECLARE>. */
log_it * cast_logit(void * logif, TYPENAME log_t);

/* function: log_it_DECLARE
 * Declares a function table for accessing a log service.
 * The declared interface is structural compatible with <log_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the first parameter.
 *
 * See <log_it> for a list of declared functions.
 *
 * Parameter:
 * declared_it - The name of the structure which is declared as the interface.
 *               The name should have the suffix "_it".
 * log_t       - The type of the log object which is the first parameter of all interface functions. */
void log_it_DECLARE(TYPENAME declared_it, TYPENAME log_t);


/* struct: log_header_t
 * Contains information for a log header.
 * It describes the function name, file name and line number
 * of the log statement. */
typedef struct log_header_t {
   /* variable: funcname
    * The name of the function which writes the log. */
   const char *   funcname;
   /* variable: filename
    * The name of the file the function is located. */
   const char *   filename;
   /* variable: linenr
    * The log commans's line number in the source file. A function could have several calls
    * to write a log entry. With linenr one can discriminate between the different calls
    * even if they would write the same log text. */
   int            linenr;
} log_header_t;

// group: lifetime

#define log_header_INIT(funcname, filename, linenr) \
         { funcname, filename, linenr }



// section: inline implementation

// group: log_it

/* define: cast_logit
 * Implements <log_it.cast_logit>. */
#define cast_logit(logif, log_t) \
         ( __extension__ ({                            \
            typeof(logif) _l;                          \
            _l = (logif);                              \
            static_assert(                             \
               &_l->printf                             \
               == (void(**)(log_t*,uint8_t,uint8_t,    \
                     const log_header_t*,              \
                     const char*,...))                 \
                     &((log_it*) _l)->printf           \
               && &_l->printtext                       \
                  == (void(**)(log_t*,uint8_t,uint8_t, \
                               const log_header_t*,    \
                               log_text_f,void*))      \
                     &((log_it*) _l)->printtext        \
               && &_l->flushbuffer                     \
                  == (void(**)(log_t*,uint8_t))        \
                     &((log_it*) _l)->flushbuffer      \
               && &_l->truncatebuffer                  \
                  == (void(**)(log_t*,uint8_t,size_t)) \
                     &((log_it*) _l)->truncatebuffer   \
               && &_l->getbuffer                       \
                  == (void(**)(const log_t*,uint8_t,   \
                        uint8_t**,size_t*))            \
                     &((log_it*) _l)->getbuffer        \
               && &_l->getstate                        \
                  == (uint8_t(**)(const log_t*,        \
                                  uint8_t))            \
                     &((log_it*) _l)->getstate         \
               && &_l->compare                         \
                  == (int(**)(const log_t*, uint8_t,   \
                              size_t,const uint8_t*))  \
                     &((log_it*) _l)->compare          \
               && &_l->setstate                        \
                  == (void(**)(log_t*,uint8_t,         \
                               uint8_t))               \
                     &((log_it*) _l)->setstate,        \
               "ensure compatible structure");         \
            (log_it*) _l;                              \
         }))


/* define: log_it_DECLARE
 * Implements <log_it.log_it_DECLARE>. */
#define log_it_DECLARE(declared_it, log_t) \
         typedef struct declared_it declared_it;                                          \
         struct declared_it {                                                             \
            void  (*printf)      (log_t * log, uint8_t channel, uint8_t flags,            \
                                  const log_header_t * header, const char * format, ... ) \
                                  __attribute__ ((__format__ (__printf__, 5, 6)));        \
            void  (*printtext)   (log_t * log, uint8_t channel, uint8_t flags,            \
                                  const log_header_t * header, log_text_f textf, void*p); \
            void  (*flushbuffer) (log_t * log, uint8_t channel);                          \
            void  (*truncatebuffer) (log_t * log, uint8_t channel, size_t size);          \
            void  (*getbuffer)   (const log_t * log, uint8_t channel,                     \
                                  /*out*/uint8_t ** buffer, /*out*/size_t * size);        \
            uint8_t (*getstate)  (const log_t * log, uint8_t channel);                    \
            int     (*compare)   (const log_t * log, uint8_t channel, size_t logsize,     \
                                  const uint8_t logbuffer[logsize]);                      \
            void    (*setstate)  (log_t * log, uint8_t channel, uint8_t logstate);        \
         };

#endif
