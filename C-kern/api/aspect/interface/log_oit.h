/* title: Log-Object
   Exports <log_oit>: an object which exports interface <log_it>.
   To use this object you need to include <Log>

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

   file: C-kern/api/aspect/interface/log_oit.h
    Header file of <Log-Object>.

   file: C-kern/api/aspect/interface/log_it.h
    Contains interface definition <Log>.
*/
#ifndef CKERN_ASPECT_INTERFACE_LOG_OIT_HEADER
#define CKERN_ASPECT_INTERFACE_LOG_OIT_HEADER

/* typedef: struct log_oit
 * Export <log_oit> - object + interface pointer.
 * See also <interface_oit>. */
typedef struct log_oit           log_oit ;


/* struct: log_oit
 * An object which exports interface <log_it>. */
struct log_oit {
   /* variable: object
    * A pointer to the object which is operated on by the interface <log_it>. */
   void          *   object ;
   /* variable: functable
    * A pointer to a function table interface <log_it> which operates on <object>. */
   struct log_it *   functable ;
} ;

// group: lifetime

/* define: log_oit_INIT_FREEABLE
 * Static initializer. */
#define log_oit_INIT_FREEABLE    { (void*)0, (struct log_it*)0 }

#endif
