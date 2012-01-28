/* title: Objectcache-Object
   Exports <objectcache_oit>: a pointer to object and its interface <objectcache_it>.
   To use this object you need to include <Objectcache-Interface>

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

   file: C-kern/api/cache/objectcache_it.h
    Header file of <Objectcache-Interface>.

   file: C-kern/api/cache/objectcache_oit.h
    Contains interfaceable object <Objectcache-Object>.
*/
#ifndef CKERN_CACHE_INTERFACE_OBJECTCACHE_OIT_HEADER
#define CKERN_CACHE_INTERFACE_OBJECTCACHE_OIT_HEADER

// forward
struct objectcache_t ;
struct objectcache_it ;

/* typedef: struct objectcache_oit
 * Export <objectcache_oit> (object + interface pointer).
 * See also <interface_oit>. */
typedef struct objectcache_oit         objectcache_oit ;


/* struct: objectcache_oit
 * An object which exports interface <objectcache_it>. */
struct objectcache_oit {
   /* variable: object
    * A pointer to the object which is operated on by the interface <objectcache_it>. */
   struct objectcache_t    * object ;
   /* variable: functable
    * A pointer to a function table interface <objectcache_it> which operates on <object>. */
   struct objectcache_it   * functable ;
} ;

// group: lifetime

/* define: objectcache_oit_INIT_FREEABLE
 * Static initializer. */
#define objectcache_oit_INIT_FREEABLE    { (struct objectcache_t*)0, (struct objectcache_it*)0 }

#endif

