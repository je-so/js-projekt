/* title: Objectcache
   Offers a simple cache mechanism for objects needed in submodules
   which are costly to construct or deconstruct.

   Implements <initumgebung_objectcache>/<freeumgebung_objectcache> to allocate
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

   file: C-kern/api/cache/objectcache.h
    Header file of <Objectcache>.

   file: C-kern/cache/objectcache.c
    Implementation file <Objectcache impl>.
*/
#ifndef CKERN_CACHE_OBJECTCACHE_HEADER
#define CKERN_CACHE_OBJECTCACHE_HEADER

#include "C-kern/api/memory/memblock.h"

/* typedef: struct objectcache_t
 * Export <objectcache_t>. */
typedef struct objectcache_t     objectcache_t ;


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
   memblock_t    iobuffer ;
} ;

// group: init

/* function: initumgebung_objectcache
 * Calls <init_objectcache> and wraps object into interface object <objectcache_oit>. */
extern int initumgebung_objectcache(/*out*/objectcache_oit * objectcache) ;

/* function: freeumgebung_objectcache
 * Calls <free_objectcache> with object pointer from <objectcache_oit>. */
extern int freeumgebung_objectcache(objectcache_oit * objectcache) ;

// group: lifetime

/* define: objectcache_INIT_FREEABLE
 * Static initializer. */
#define objectcache_INIT_FREEABLE      { memblock_INIT_FREEABLE }

/* function: init_objectcache
 * Inits <objectcache_t> and all contained objects. */
extern int init_objectcache(objectcache_t * objectcache) ;

/* function: free_objectcache
 * Frees <objectcache_t> and all contained objects. */
extern int free_objectcache(objectcache_t * objectcache) ;

// group: query

/* function: lockiobuffer_objectcache
 * Locks the io buffer and returns a pointer to it in iobuffer.
 * The buffer is of type <memblock_t> (equal to <vm_block_t>). */
extern void lockiobuffer_objectcache(objectcache_t * objectcache, /*out*/memblock_t ** iobuffer) ;

/* function: unlockiobuffer_objectcache
 * Unlocks the locked io buffer and sets the pointer to NULL.
 * The pointer to the buffer must be acquired by a previous call to <lockiobuffer_objectcache>.
 * Calling unlock with a NULL pointer is a no op. */
extern void unlockiobuffer_objectcache(objectcache_t * objectcache, memblock_t ** iobuffer) ;

// group: change

/* Moves content of cached objects from source to destination.
 * Both objects must have been initialized.
 * After successfull return all cached objects of source are in a freed state
 * and the previous content is transfered to destination.
 * Before anything is copied to destiniation all cached objects in destination are freed. */
extern int move_objectcache( objectcache_t * destination, objectcache_t * source ) ;

#endif
