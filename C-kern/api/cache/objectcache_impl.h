/* title: Objectcache

   Offers a simple cache mechanism for objects needed in submodules
   which are costly to construct or deconstruct.

   Implements <initthread_objectcacheimpl> and <freethread_objectcacheimpl> to allocate
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

   file: C-kern/api/cache/objectcache_impl.h
    Header file of <Objectcache>.

   file: C-kern/cache/objectcache_impl.c
    Implementation file <Objectcache impl>.
*/
#ifndef CKERN_CACHE_OBJECTCACHE_IMPL_HEADER
#define CKERN_CACHE_OBJECTCACHE_IMPL_HEADER

#include "C-kern/api/memory/memblock.h"

/* typedef: struct objectcache_impl_t
 * Export <objectcache_impl_t>. */
typedef struct objectcache_impl_t         objectcache_impl_t ;


// section: Functions

// group: init

#ifdef KONFIG_UNITTEST
/* function: unittest_cache_objectcacheimpl
 * Test allocation and free works. */
int unittest_cache_objectcacheimpl(void) ;
#endif


/* struct: objectcache_impl_t
 * Implements <objectcache_impl_t>.
 * Holds pointers to all cached objects. */
struct objectcache_impl_t {
   /* variable: iobuffer
    * Used in <init_vmmappedregions>. */
   memblock_t    iobuffer ;
} ;

// group: init

/* function: initthread_objectcacheimpl
 * Calls <init_objectcacheimpl> and wraps object into interfaceable object <objectcache_t>.
 * This function is called from <init_threadcontext>. */
int initthread_objectcacheimpl(/*out*/objectcache_t * objectcache) ;

/* function: freethread_objectcacheimpl
 * Calls <free_objectcacheimpl> with object pointer from <objectcache_t>.
 * This function is called from <free_threadcontext>. */
int freethread_objectcacheimpl(objectcache_t * objectcache) ;

// group: lifetime

/* define: objectcache_impl_INIT_FREEABLE
 * Static initializer. */
#define objectcache_impl_INIT_FREEABLE      { memblock_INIT_FREEABLE }

/* function: init_objectcacheimpl
 * Inits <objectcache_impl_t> and all contained objects. */
int init_objectcacheimpl(/*out*/objectcache_impl_t * objectcache) ;

/* function: free_objectcacheimpl
 * Frees <objectcache_impl_t> and all contained objects. */
int free_objectcacheimpl(objectcache_impl_t * objectcache) ;

// group: query

/* function: lockiobuffer_objectcacheimpl
 * Locks the io buffer and returns a pointer to it in iobuffer.
 * The buffer is of type <memblock_t> (equal to <vm_block_t>). */
void lockiobuffer_objectcacheimpl(objectcache_impl_t * objectcache, /*out*/memblock_t ** iobuffer) ;

/* function: unlockiobuffer_objectcacheimpl
 * Unlocks the locked io buffer and sets the pointer to NULL.
 * The pointer to the buffer must be acquired by a previous call to <lockiobuffer_objectcacheimpl>.
 * Calling unlock with a NULL pointer is a no op. */
void unlockiobuffer_objectcacheimpl(objectcache_impl_t * objectcache, memblock_t ** iobuffer) ;

// group: change

/* Moves content of cached objects from source to destination.
 * Both objects must have been initialized.
 * After successfull return all cached objects of source are in a freed state
 * and the previous content is transfered to destination.
 * Before anything is copied to destiniation all cached objects in destination are freed. */
int move_objectcacheimpl(objectcache_impl_t * destination, objectcache_impl_t * source) ;

#endif
