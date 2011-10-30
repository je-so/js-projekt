/* title: Log
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

   file: C-kern/api/aspect/interface/log_it.h
    Header file of <Log>.

   file: C-kern/api/aspect/interface/log_oit.h
    Contains object interface definition <Log-Object>.
*/
#ifndef CKERN_ASPECT_INTERFACE_LOG_IT_HEADER
#define CKERN_ASPECT_INTERFACE_LOG_IT_HEADER

/* typedef: struct log_it
 * Export interface (function table) <log_it>.
 * See <log_it_DECLARE> for a description of the functional interface. */
typedef struct log_it            log_it ;

/* enums: log_constants_e
 * Used to configure system wide restrictions.
 *
 * log_PRINTF_MAXSIZE - The maximum byte size of one log entry written with <log_it.printf>.
 *
 * */
enum log_constants_e {
   log_PRINTF_MAXSIZE = 511
} ;

typedef enum log_constants_e     log_constants_e ;

/* define: log_it_DECLARE
 * Declares a function table for accessing a log service.
 * Use this macro to define an interface which is structural compatible
 * with the generic interface <log_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the object. Every interface function takes a pointer to this object type
 * as its first parameter.
 *
 * Parameter:
 * is_typedef       - Set it to 1 if you also want to emit code:
 *                    "typedef struct declared_type_it declared_type_it ;"
 * declared_type_it - The name of the structure which is declared as the functional log interface.
 *                    The name should have the suffix "_it".
 * object_type_t    - The type of the log object which is the first parameter of all log functions.
 *
 * List of declared functions:
 * printf       - See <logwriter_t.printf_logwriter>
 * flushbuffer  - See <logwriter_t.flushbuffer_logwriter>
 * clearbuffer  - See <logwriter_t.clearbuffer_logwriter>
 * getbuffer    - See <logwriter_t.getbuffer_logwriter>
 *
 * (start code)
 * #define log_it_DECLARE(is_typedef, declared_type_it, object_type_t)  \
 *   CONCAT(EMITCODE_, is_typedef)(typedef struct declared_type_it  declared_type_it ;)  \
 *   struct declared_type_it {   \
 *       void  (*printf)      (object_type_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ; \
 *       void  (*flushbuffer) (object_type_t * log) ; \
 *       void  (*clearbuffer) (object_type_t * log) ; \
 *       void  (*getbuffer)   (object_type_t * log, char ** buffer, size_t * size) ; \
 *    } ;
 * (end code)
 * */
#define log_it_DECLARE(is_typedef, declared_type_it, object_type_t)  \
   CONCAT(EMITCODE_, is_typedef)(typedef struct declared_type_it  declared_type_it ;)  \
   struct declared_type_it {                                                           \
      void  (*printf)      (object_type_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ; \
      void  (*flushbuffer) (object_type_t * log) ;                                      \
      void  (*clearbuffer) (object_type_t * log) ;                                      \
      void  (*getbuffer)   (object_type_t * log, /*out*/char ** buffer, /*out*/size_t * size) ; \
   } ;


#if 0
/* struct: log_it
 * The function table which describes the log service. */
struct log_it {
   /* variable: printf
    * See <logwriter_t.printf_logwriter> for an implementation. */
   void  (*printf)      (void * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;
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
#endif

log_it_DECLARE(0, log_it, void)

#endif
