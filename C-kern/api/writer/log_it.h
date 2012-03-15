/* title: Log-Interface
   Interface to access log service.
   An interface is a structure which lists function pointers.
   These function pointers point to functions exported by a service implementation.
   Used in <LogWriter>.

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

   file: C-kern/api/writer/log_it.h
    Header file of <Log-Interface>.

   file: C-kern/api/writer/log_oit.h
    Contains object interface definition <Log-ObjectInterface>.
*/
#ifndef CKERN_WRITER_LOG_IT_HEADER
#define CKERN_WRITER_LOG_IT_HEADER

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

/* define: log_it_DECLARE
 * Declares a function table for accessing a log service.
 * Use this macro to define an interface which is structural compatible
 * with the generic interface <log_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the object. Every interface function takes a pointer to this object type
 * as its first parameter.
 *
 * Parameter:
 * declared_it - The name of the structure which is declared as the functional log interface.
 *               The name should have the suffix "_it".
 * object_t    - The type of the log object which is the first parameter of all log functions.
 *
 * List of declared functions:
 * printf       - See <logwriter_t.printf_logwriter>
 * flushbuffer  - See <logwriter_t.flushbuffer_logwriter>
 * clearbuffer  - See <logwriter_t.clearbuffer_logwriter>
 * getbuffer    - See <logwriter_t.getbuffer_logwriter>
 *
 * */
#define log_it_DECLARE(declared_it, object_t)                                                \
   struct declared_it {                                                                      \
      void  (*printf)      (object_t * log, log_channel_e channel, const char * format, ... ) __attribute__ ((__format__ (__printf__, 3, 4))) ; \
      void  (*flushbuffer) (object_t * log) ;                                                \
      void  (*clearbuffer) (object_t * log) ;                                                \
      void  (*getbuffer)   (object_t * log, /*out*/char ** buffer, /*out*/size_t * size) ;   \
   } ;

#endif
