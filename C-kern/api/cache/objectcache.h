/* title: Objectcache
   Offers a simple cache mechanism for objects
   needed in submodules either often or which are costly
   to construct or deconstruct.

   Implements init/free functions to allocate
   storage for cached objects before a new thread
   is created and frees it before the thread exits.

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

   file: C-kern/api/cache/objectcache.h
    Header file of <Objectcache>.

   file: C-kern/cache/objectcache.c
    Implementation file <Objectcache impl>.
*/
#ifndef CKERN_CACHE_OBJECTCACHE_HEADER
#define CKERN_CACHE_OBJECTCACHE_HEADER

#include "C-kern/api/aspect/memoryblock.h"

/* typedef: objectcache_t typedef
 * Export <objectcache_t>. */
typedef struct objectcache_t    objectcache_t ;


// section: Functions

// group: init

#ifdef KONFIG_UNITTEST
/* function: unittest_cache_objectcache
 * Test allocation and free works. */
extern int unittest_cache_objectcache(void) ;
#endif


/* struct: objectcache_t
 * Holds pointers to all cached objects. */
struct objectcache_t {
   /* variable: iobuffer
    * Used in <init_vmmappedregions>. */
   memoryblock_aspect_t    iobuffer ;
} ;

// group: initumgebung

/* function: initumgebung_objectcache
 * Creates new <objectcache_t> and all referenced objects. */
extern int initumgebung_objectcache(/*out*/objectcache_t ** objectcache) ;

/* function: freeumgebung_objectcache
 * Frees <objectcache_t> and all referenced objects. */
extern int freeumgebung_objectcache(objectcache_t ** objectcache) ;

// group: lifetime

/* function: new_objectcache
 * Creates new <objectcache_t> and all referenced objects. */
extern int new_objectcache(objectcache_t ** objectcache) ;

/* function: delete_objectcache
 * Frees <objectcache_t> and all referenced objects. */
extern int delete_objectcache(objectcache_t ** objectcache) ;

// group: change

/* Moves content of cached objects from source to destination.
 * Both objects must have been initialized. After successfull return
 * all cached objects of source are in a freed state and the previous
 * content was transfered to destination. Before the transfer
 * all cached objects in destination are freed. */
extern int move_objectcache( objectcache_t * destination, objectcache_t * source ) ;

#endif
