/* title: Objectcache-MT
   See <Objectcache>.

   Implements <initumgebung_objectcachemt>/<freeumgebung_objectcachemt> to allocate
   storage for cached objects before a new thread is created and frees all storage
   before the thread exits.

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

   file: C-kern/api/cache/objectcachemt.h
    Header file of <Objectcache-MT>.

   file: C-kern/cache/objectcachemt.c
    Implementation file <Objectcache-MT impl>.
*/
#ifndef CKERN_CACHE_OBJECTCACHEMT_HEADER
#define CKERN_CACHE_OBJECTCACHEMT_HEADER

#include "C-kern/api/cache/objectcache.h"

/* typedef: struct objectcachemt_t
 * Export <objectcache_t>. */
typedef struct objectcachemt_t      objectcachemt_t ;


// section: Functions

// group: init

#ifdef KONFIG_UNITTEST
/* function: unittest_cache_objectcachemt
 * Test allocation and free works. */
extern int unittest_cache_objectcachemt(void) ;
#endif


/* struct: objectcachemt_t
 * Makes <objectcache_t> multithread safe. Adds a mutex. */
struct objectcachemt_t {
   objectcache_t  objectcache ;
   sys_mutex_t    lock ;
} ;

// group: init

/* function: initumgebung_objectcachemt
 * Calls <init_objectcachemt> and wraps object into interface object <objectcache_oit>. */
extern int initumgebung_objectcachemt(/*out*/objectcache_oit * objectcache) ;

/* function: freeumgebung_objectcachemt
 * Calls <free_objectcachemt> with object pointer from <objectcache_oit>. */
extern int freeumgebung_objectcachemt(objectcache_oit * objectcache) ;

// group: lifetime

/* define: objectcachemt_INIT_FREEABLE
 * Static initializer. */
#define objectcachemt_INIT_FREEABLE       { objectcache_INIT_FREEABLE, sys_mutex_INIT_DEFAULT }

/* function: init_objectcachemt
 * Extends <init_objectcache> with an init call for a mutex. */
extern int init_objectcachemt(objectcachemt_t * objectcache) ;

/* function: free_objectcachemt
 * Frees mutex and calls <free_objectcache>. */
extern int free_objectcachemt(objectcachemt_t * objectcache) ;

// group: query

/* function: lockiobuffer_objectcachemt
 * Thread safe version of <lockiobuffer_objectcache>. */
extern void lockiobuffer_objectcachemt(objectcachemt_t * objectcache, /*out*/memblock_t ** iobuffer) ;

/* function: unlockiobuffer_objectcachemt
 * Thread safe version of <unlockiobuffer_objectcache>. */
extern void unlockiobuffer_objectcachemt(objectcachemt_t * objectcache, memblock_t ** iobuffer) ;


#endif
