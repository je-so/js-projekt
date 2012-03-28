/* title: Log-ImplementationObject
   Exports <log_iot> - to use this interface implementaion you need to include <Log-Interface> also.
   See also <log_it>.

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

   file: C-kern/api/io/writer/log/log_iot.h
    Contains interface implementing object <Log-ImplementationObject>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOG_IOT_HEADER
#define CKERN_IO_WRITER_LOG_LOG_IOT_HEADER

/* typedef: struct log_iot
 * Export <log_iot> - log service implementing object. */
typedef struct log_iot                 log_iot ;


/* struct: log_iot
 * A log service implementing object. See also <log_it>. */
struct log_iot {
   /* variable: object
    * The implementing object of interface <log_it>.
    * This is the value of the first argument of functions defined in interface <log_it>. */
   void           * object ;
   /* variable: iimpl
    * A pointer to implementation of interface <log_it> which operates on <object>. */
   struct log_it  * iimpl ;
} ;

// group: lifetime

/* define: log_iot_INIT_FREEABLE
 * Static initializer. */
#define log_iot_INIT_FREEABLE    { (void*)0, (struct log_it*)0 }

#endif
