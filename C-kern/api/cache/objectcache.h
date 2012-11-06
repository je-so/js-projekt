/* title: Objectcache-Object

   Exports <objectcache_t>: a pointer to object and its implementation of interface <objectcache_it>.
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

   file: C-kern/api/cache/objectcache.h
    Contains interface implementing object <Objectcache-Object>.
*/
#ifndef CKERN_CACHE_OBJECTCACHE_HEADER
#define CKERN_CACHE_OBJECTCACHE_HEADER

// forward
struct objectcache_t ;
struct objectcache_it ;

/* typedef: struct objectcache_t
 * Export <objectcache_t>. <objectcache_it> implementing object. */
typedef struct objectcache_t           objectcache_t ;


/* struct: objectcache_t
 * An object which implements interface <objectcache_it>. */
struct objectcache_t {
   /* variable: object
    * A pointer to the object which is operated on by the interface <objectcache_it>. */
   struct objectcache_t    * object ;
   /* variable: iimpl
    * A pointer to an implementation of interface <objectcache_it> which operates on <object>. */
   struct objectcache_it   * iimpl ;
} ;

// group: lifetime

/* define: objectcache_INIT_FREEABLE
 * Static initializer. */
#define objectcache_INIT_FREEABLE      { (struct objectcache_t*)0, (struct objectcache_it*)0 }

#endif
