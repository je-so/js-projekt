/* title: Log-Object

   Interface to access log service. An interface is a structure which lists function pointers.
   These function pointers point to functions exported by a service implementation.
   Used in <LogWriter> and <LogMain>.

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

   file: C-kern/api/io/writer/log/log.h
    Header file of <Log-Object>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOG_HEADER
#define CKERN_IO_WRITER_LOG_LOG_HEADER

// forward
struct logbuffer_t;

/* typedef: struct log_t
 * Export <log_t>. Interfaceable log object. */
typedef struct log_t log_t;

/* typedef: struct log_it
 * Export interface <log_it>.
 * See <log_it_DECLARE> for a description of the functional interface. */
typedef struct log_it log_it;

/* typedef: struct log_header_t
 * Export <log_header_t>. */
typedef struct log_header_t log_header_t;

/* typedef: log_text_f
 * Declare function pointer which writes a text resource into <logbuffer_t>. */
typedef void (*log_text_f)(struct logbuffer_t * logbuffer, va_list vargs);


/* enums: log_config_e
 * Used to configure system wide restrictions.
 *
 * log_config_MINSIZE - The minimum size in bytes of one log entry before it is possibly truncated.
 *                      If the buffer is bigger it is not truncated.
 *
 * */
enum log_config_e {
   log_config_MINSIZE = 512
};

typedef enum log_config_e log_config_e;

/* enums: log_flags_e
 * Used to configure system wide restrictions.
 *
 * log_flags_NONE      - Indicates that the log entry does not change append or not append state.
 *                       If the last call to print contained <log_flags_START> the new entry is appended.
 *                       If the last call to print contained <log_flags_END> the new entry is not appended.
 * log_flags_START     - Indicates that this is the beginning of a new log entry.
 *                       This flag implicitly ends the previous log entry if not done explicitly.
 *                       The log entry is therefore not appended to a previous one.
 *                       Every following log output is now appended to a memory buffer and truncated if necessary
 *                       until another call to print is done with flag set to <log_flags_END> or <log_flags_START>.
 * log_flags_END       - Indicates that this is the last part of a log entry.
 *                       You can set <log_flags_START> and <log_flags_END> at the same time to indicate
 *                       that the printed log entry should not be appended to a previous one and the following
 *                       entries should not be appended to this one.
 * log_flags_OPTIONALHEADER - A given <log_header_t> is only written before the log entry if the pointer to the function name
 *                            in <log_header_t> differs from the pointer given in the previous call.
 *                            The flag <log_flags_END> sets the remembered pointer to the function name to 0.
 *                            Set this flag on the last part of the log entry if another header could be written in the same function
 *                            with flag <log_flags_START>.
 *                            Both log macros <TRACEABORT_ERRLOG> and <TRACEABORTFREE_ERRLOG> set this flag. They do not know
 *                            if another call to <TRACESYSCALL_ERRLOG> already printed a header.
 * */
enum log_flags_e {
   log_flags_NONE      = 0,
   log_flags_START     = 1,
   log_flags_END       = 2,
   log_flags_OPTIONALHEADER = 4
};

typedef enum log_flags_e log_flags_e;

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
 * log_channel_NROFCHANNEL - Use this value to determine the number of different channels
 *                           numbered from 0 up to (log_channel_NROFCHANNEL-1).
 * */
enum log_channel_e {
   log_channel_USERERR,
   log_channel_TEST,
   log_channel_WARN,
   log_channel_ERR
};

typedef enum log_channel_e log_channel_e;

#define log_channel_NROFCHANNEL (log_channel_ERR + 1)


/* enums: log_state_e
 * Used to configure the state of a <log_channel_e>.
 *
 * log_state_IGNORED    - Ignore any writing to this channel.
 * log_state_BUFFERED   - Normal operation mode. All log entries are buffered until the buffer is full and then the buffer is written
 *                        all at once.
 * log_state_UNBUFFERED - Every log entry is constructed in a buffer and if the last part of the entry is added to it the log entry is
 *                        written out as a whole.
 * log_state_IMMEDIATE  - Every part of a log entry is written immediately without waiting for the last part end.
 *
 * log_state_NROFSTATE  - Use this value to determine the number of different states
 *                        numbered from 0 up to (log_state_NROFSTATE-1).
 * */
enum log_state_e {
   log_state_IGNORED    = 0,
   log_state_BUFFERED   = 1,
   log_state_UNBUFFERED = 2,
   log_state_IMMEDIATE  = 3,
};

typedef enum log_state_e log_state_e;

#define log_state_NROFSTATE (log_state_IMMEDIATE + 1)


/* struct: log_t
 * Uses <iobj_DECLARE> to declare object supporting interface <log_it>. */
iobj_DECLARE(log_t, log);

// group: generic

/* function: genericcast_log
 * Casts parameter iobj to pointer to <log_t>.
 * iobj must be a pointer to an anonymous interfaceable log object. */
log_t * genericcast_log(void * iobj);


/* struct: log_it
 * The function table which describes the log service. */
struct log_it {
   /* variable: printf
    * Writes a new log entry to in internal buffer.
    * Parameter channel must be set to a value from <log_channel_e>.
    * If header is not 0 a header is written which contains "[thread id, timestamp]\nfuncname() file:linenr\n, Error NR - DESCRIPTION\n".
    * If the entry is bigger than <log_config_MINSIZE> it may be truncated if internal buffer size is lower.
    * See <logwriter_t.printf_logwriter> for an implementation. */
   void  (*printf)      (void * log, uint8_t channel, uint8_t flags, const log_header_t * header, const char * format, ... ) __attribute__ ((__format__ (__printf__, 5, 6)));
   /* variable: printtext
    * Writes text resource as new log entry to in internal buffer.
    * See <printf> for a description of the parameter. The variable parameter list must match the resource. */
   void  (*printtext)   (void * log, uint8_t channel, uint8_t flags, const log_header_t * header, log_text_f textf, ... );
   /* variable: flushbuffer
    * Writes content of buffer to configured file descriptor and clears log buffer.
    * This call is ignored if buffer is empty or log is not configured to be in buffered mode.
    * See <logwriter_t.flushbuffer_logwriter> for an implementation. */
   void  (*flushbuffer) (void * log, uint8_t channel);
   /* variable: clearbuffer
    * Clears log buffer (sets length of logbuffer to 0).
    * This call is ignored if the log is not configured to be in buffered mode.
    * See <logwriter_t.clearbuffer_logwriter> for an implementation. */
   void  (*clearbuffer) (void * log, uint8_t channel);
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
   /* variable: comapre
    * Returns 0 if logbuffer compares equal to content in log for a specific <log_channel_e> channel.
    * The return value EINVAL indicates not equal. The comparison should ignore timestamps. */
   int     (*compare)   (const void * log, uint8_t channel, size_t logsize, const uint8_t logbuffer[logsize]);
   // -- configuration --
   /* variable: setstate
    * Sets <log_state_e> logstate for a specific <log_channel_e> channel.
    * You can query the current <log_state_e> by calling <getstate>. */
   void  (*setstate)    (void * log, uint8_t channel, uint8_t logstate);
};

// group: generic

/* function: genericcast_logit
 * Casts pointer logif into pointer to interface <log_it>.
 * Parameter *logif* must point to a type declared with <log_it_DECLARE>.
 * The other parameters must be the same as in <log_it_DECLARE> without the first. */
log_it * genericcast_logit(void * logif, TYPENAME log_t);

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
struct log_header_t {
   const char *   funcname;
   const char *   filename;
   int            linenr;
   int            err;
};

// group: lifetime

#define log_header_INIT(funcname, filename, linenr, err) \
         { funcname, filename, linenr, err }



// section: inline implementation

// group: log_t

/* define: genericcast_log
 * Implements <log_t.genericcast_log>. */
#define genericcast_log(iobj) \
         genericcast_iobj(iobj, log)

// group: log_it

/* define: genericcast_logit
 * Implements <log_it.genericcast_logit>. */
#define genericcast_logit(logif, log_t)               \
         ( __extension__ ({                           \
            static_assert(                            \
               &((typeof(logif))0)->printf            \
               == (void(**)(log_t*,uint8_t,uint8_t,   \
                     const log_header_t*,             \
                     const char*,...))                \
                     &((log_it*)0)->printf            \
               && &((typeof(logif))0)->printtext      \
               == (void(**)(log_t*,uint8_t,uint8_t,   \
                     const log_header_t*,             \
                     log_text_f,...))                 \
                     &((log_it*)0)->printtext         \
               && &((typeof(logif))0)->flushbuffer    \
                  == (void(**)(log_t*,uint8_t))       \
                        &((log_it*)0)->flushbuffer    \
               && &((typeof(logif))0)->clearbuffer    \
                  == (void(**)(log_t*,uint8_t))       \
                        &((log_it*)0)->clearbuffer    \
               && &((typeof(logif))0)->getbuffer      \
                  == (void(**)(const log_t*,uint8_t,  \
                        uint8_t**,size_t*))           \
                        &((log_it*)0)->getbuffer      \
               && &((typeof(logif))0)->getstate       \
                  == (uint8_t(**)(const log_t*,       \
                                  uint8_t))           \
                        &((log_it*)0)->getstate       \
               && &((typeof(logif))0)->compare        \
                  == (int(**)(const log_t*, uint8_t,  \
                              size_t,const uint8_t*)) \
                        &((log_it*)0)->compare        \
               && &((typeof(logif))0)->setstate       \
                  == (void(**)(log_t*,uint8_t,        \
                        uint8_t))                     \
                        &((log_it*)0)->setstate,      \
               "ensure same structure");              \
            (log_it*) (logif);                        \
         }))


/* define: log_it_DECLARE
 * Implements <log_it.log_it_DECLARE>. */
#define log_it_DECLARE(declared_it, log_t)   \
   typedef struct declared_it    declared_it;                                          \
   struct declared_it {                                                                \
      void  (*printf)      (log_t * log, uint8_t channel, uint8_t flags,               \
                            const log_header_t * header, const char * format, ... )    \
                            __attribute__ ((__format__ (__printf__, 5, 6)));           \
      void  (*printtext)   (log_t * log, uint8_t channel, uint8_t flags,               \
                            const log_header_t * header, log_text_f textf, ... );      \
      void  (*flushbuffer) (log_t * log, uint8_t channel);                             \
      void  (*clearbuffer) (log_t * log, uint8_t channel);                             \
      void  (*getbuffer)   (const log_t * log, uint8_t channel,                        \
                            /*out*/uint8_t ** buffer, /*out*/size_t * size);           \
      uint8_t (*getstate)  (const log_t * log, uint8_t channel);                       \
      int     (*compare)   (const log_t * log, uint8_t channel, size_t logsize,        \
                            const uint8_t logbuffer[logsize]);                         \
      void    (*setstate)  (log_t * log, uint8_t channel, uint8_t logstate);           \
   };

#endif
