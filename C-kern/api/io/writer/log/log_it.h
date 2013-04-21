/* title: Log-Interface

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

   file: C-kern/api/io/writer/log/log_it.h
    Header file of <Log-Interface>.

   file: C-kern/api/io/writer/log/log.h
    Contains interface implementing object <Log-Object>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOG_INTERFACE_HEADER
#define CKERN_IO_WRITER_LOG_LOG_INTERFACE_HEADER

/* typedef: struct log_it
 * Export interface (function table) <log_it>.
 * See <log_it_DECLARE> for a description of the functional interface. */
typedef struct log_it                  log_it ;

/* enums: log_constants_e
 * Used to configure system wide restrictions.
 *
 * log_PRINTF_MAXSIZE - The maximum byte size of one log entry written with <log_it.printf>.
 *
 * */
enum log_constants_e {
   log_PRINTF_MAXSIZE = 511
} ;

typedef enum log_constants_e           log_constants_e ;

/* enums: log_channel_e
 * Used to switch between log channels.
 *
 * log_channel_ERR  - Normal error log channel which is represented by interface <log_it>.
 * log_channel_TEST - Test log output which is implemented as a call to standard
 *                    printf library function which writes to STDOUT.
 * */
enum log_channel_e {
    log_channel_ERR
   ,log_channel_TEST
} ;

typedef enum log_channel_e       log_channel_e ;


/* struct: log_it
 * The function table which describes the log service. */
struct log_it {
   /* variable: printf
    * See <logwriter_t.printf_logwriter> for an implementation. */
   void  (*printf)      (void * log, log_channel_e channel, const char * format, ... ) __attribute__ ((__format__ (__printf__, 3, 4))) ;
   /* variable: flushbuffer
    * See <logwriter_t.flushbuffer_logwriter> for an implementation. */
   void  (*flushbuffer) (void * log) ;
   /* variable: clearbuffer
    * See <logwriter_t.clearbuffer_logwriter> for an implementation. */
   void  (*clearbuffer) (void * log) ;
   /* variable: getbuffer
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
         offsetof(typeof(*(logif)), printf)              \
         == offsetof(log_it, printf)                     \
         && offsetof(typeof(*(logif)), flushbuffer)      \
            == offsetof(log_it, flushbuffer)             \
         && offsetof(typeof(*(logif)), clearbuffer)      \
            == offsetof(log_it, clearbuffer)             \
         && offsetof(typeof(*(logif)), getbuffer)        \
            == offsetof(log_it, getbuffer),              \
         "ensure same structure") ;                      \
      if (0) {                                           \
         (logif)->printf(  (log_t*)0, log_channel_ERR,   \
                           "%d%s", 1, "") ;              \
         (logif)->flushbuffer((log_t*)0) ;               \
         (logif)->clearbuffer((log_t*)0) ;               \
         (logif)->getbuffer(  (log_t*)0, (char**)0,      \
                              (size_t*)0) ;              \
      }                                                  \
      (log_it*) (logif) ;                                \
   }))


/* define: log_it_DECLARE
 * Implements <log_it.log_it_DECLARE>. */
#define log_it_DECLARE(declared_it, log_t)      \
   typedef struct declared_it    declared_it ;  \
   struct declared_it {                         \
      void  (*printf)      (log_t * log, log_channel_e channel, const char * format, ... ) __attribute__ ((__format__ (__printf__, 3, 4))) ; \
      void  (*flushbuffer) (log_t * log) ;                                                \
      void  (*clearbuffer) (log_t * log) ;                                                \
      void  (*getbuffer)   (log_t * log, /*out*/char ** buffer, /*out*/size_t * size) ;   \
   } ;

#endif
