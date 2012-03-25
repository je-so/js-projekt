/* title: Log-ObjectInterface
   Exports <log_oit> -- pointer to object and to its implementation of interface <log_it>.
   To use this object you need to include <Log-Interface>.

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

   file: C-kern/api/io/writer/log/log_oit.h
    Contains object implementing interface <Log-ObjectInterface>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOG_OIT_HEADER
#define CKERN_IO_WRITER_LOG_LOG_OIT_HEADER

/* typedef: struct log_oit
 * Export <log_oit> - log service object interface implementation.
 * See also <interface_oit>. */
typedef struct log_oit                 log_oit ;


/* struct: log_oit
 * An interfaceable log service object implementing interface <log_it>. */
struct log_oit {
   /* variable: object
    * A pointer to the object which is operated on by the interface <log_it>. */
   void           * object ;
   /* variable: functable
    * A pointer to a function table interface <log_it> which operates on <object>. */
   struct log_it  * functable ;
} ;

// group: lifetime

/* define: log_oit_INIT_FREEABLE
 * Static initializer. */
#define log_oit_INIT_FREEABLE    { (void*)0, (struct log_it*)0 }

#endif
