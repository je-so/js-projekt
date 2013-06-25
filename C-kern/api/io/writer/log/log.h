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

/* typedef: struct log_t
 * Export <log_t>. Interfaceable log object. */
typedef struct log_t                      log_t ;

/* typedef: struct log_it
 * Export interface <log_it>.
 * See <log_it_DECLARE> for a description of the functional interface. */
typedef struct log_it                     log_it ;

/* enums: log_config_e
 * Used to configure system wide restrictions.
 *
 * log_config_MAXSIZE - The maximum size in bytes of one log entry written with <log_it.printf>.
 *
 * */
enum log_config_e {
   log_config_MAXSIZE = 511
} ;

typedef enum log_config_e                 log_config_e ;

/* enums: log_channel_e
 * Used to switch between log channels.
 *
 * log_channel_CONSOLE - Uses STDOUT channel for log output.
 *                       This channel is used for user messages.
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
   log_channel_CONSOLE,
   log_channel_TEST,
   log_channel_WARN,
   log_channel_ERR
} ;

#define log_channel_NROFCHANNEL           (log_channel_ERR + 1)

typedef enum log_channel_e                log_channel_e ;


/* struct: log_t
 * Uses <iobj_DECLARE> to declare object supporting interface <log_it>. */
iobj_DECLARE(log_t, log) ;


/* struct: log_it
 * The function table which describes the log service. */
struct log_it {
   /* variable: printf
    * Writes a new log entry to in internal buffer.
    * Parameter channel must be set to a value from <log_channel_e>.
    * If the entry is bigger than <log_config_MAXSIZE> it may be truncated if internal buffer size is lower.
    * See <logwriter_t.printf_logwriter> for an implementation. */
   void  (*printf)      (void * log, uint8_t channel, const char * format, ... ) __attribute__ ((__format__ (__printf__, 3, 4))) ;
   /* variable: flushbuffer
    * Writes content of buffer to configured file descriptor and clears log buffer.
    * This call is ignored if buffer is empty or log is not configured to be in buffered mode.
    * See <logwriter_t.flushbuffer_logwriter> for an implementation. */
   void  (*flushbuffer) (void * log) ;
   /* variable: clearbuffer
    * Clears log buffer (sets length of logbuffer to 0).
    * This call is ignored if the log is not configured to be in buffered mode.
    * See <logwriter_t.clearbuffer_logwriter> for an implementation. */
   void  (*clearbuffer) (void * log) ;
   /* variable: getbuffer
    * Returns content of log buffer as C-string and its size in bytes.
    * The returned values are valid as long as no other function is called on log.
    * The string has a trailing NULL byte, i.e. buffer[size] == 0.
    * See <logwriter_t.getbuffer_logwriter> for an implementation. */
   void  (*getbuffer)   (void * log, /*out*/char ** buffer, /*out*/size_t * size) ;
} ;

// group: generic

/* function: genericcast_logit
 * Casts pointer logif into pointer to interface <log_it>.
 * Parameter *logif* must point to a type declared with <log_it_DECLARE>.
 * The other parameters must be the same as in <log_it_DECLARE> without the first. */
log_it * genericcast_logit(void * logif, TYPENAME log_t) ;

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
void log_it_DECLARE(TYPENAME declared_it, TYPENAME log_t) ;



// section: inline implementation

/* define: genericcast_logit
 * Implements <log_it.genericcast_logit>. */
#define genericcast_logit(logif, log_t)                  \
   ( __extension__ ({                                    \
      static_assert(                                     \
         &((typeof(logif))0)->printf                     \
         == (void(**)(log_t*,uint8_t,const char*,...))   \
               &((log_it*)0)->printf                     \
         && &((typeof(logif))0)->flushbuffer             \
            == (void(**)(log_t*))                        \
                  &((log_it*)0)->flushbuffer             \
         && &((typeof(logif))0)->clearbuffer             \
            == (void(**)(log_t*))                        \
                  &((log_it*)0)->clearbuffer             \
         && &((typeof(logif))0)->getbuffer               \
            == (void(**)(log_t*,char**,size_t*))         \
                  &((log_it*)0)->getbuffer,              \
         "ensure same structure") ;                      \
      (log_it*) (logif) ;                                \
   }))


/* define: log_it_DECLARE
 * Implements <log_it.log_it_DECLARE>. */
#define log_it_DECLARE(declared_it, log_t)      \
   typedef struct declared_it    declared_it ;  \
   struct declared_it {                         \
      void  (*printf)      (log_t * log, uint8_t channel, const char * format, ... ) __attribute__ ((__format__ (__printf__, 3, 4))) ; \
      void  (*flushbuffer) (log_t * log) ;                                                \
      void  (*clearbuffer) (log_t * log) ;                                                \
      void  (*getbuffer)   (log_t * log, /*out*/char ** buffer, /*out*/size_t * size) ;   \
   } ;

#endif
