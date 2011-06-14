/* title: ObjectCache
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

   file: C-kern/api/umgebung/object_cache.h
    Header file of <ObjectCache>.

   file: C-kern/umgebung/object_cache.c
    Implementation file of <ObjectCache>.
*/
#ifndef CKERN_UMGEBUNG_OBJECTCACHE_HEADER
#define CKERN_UMGEBUNG_OBJECTCACHE_HEADER

// forward:
struct vm_block_t ;

/* typedef: typedef object_cache_t
 * Shortcut for <object_cache_t>. */
typedef struct object_cache_t    object_cache_t ;

/* Used to support caching in main thread before anything is initialized. */
extern object_cache_t            g_main_objectcache ;


// section: Functions

// group: init

/* function: initumgebung_objectcache
 * */
extern int initumgebung_objectcache(/*out*/object_cache_t ** cache) ;

/* function: freeumgebung_objectcache
 * */
extern int freeumgebung_objectcache(object_cache_t ** cache) ;


#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_objectcache
 * Test allocation and free works. */
extern int unittest_umgebung_objectcache(void) ;
#endif


/* struct: object_cache_t
 * */
struct object_cache_t {
   /* variable: vm_rootbuffer
    * Used in <init_vmmappedregions>. */
   struct vm_block_t  * vm_rootbuffer ;
} ;

// group: change

/* Moves content of cached objects from source to destination.
 * Both objects must have been initialized. After successfull return
 * all cached objects of source are in a freed state and the previous
 * content was transfered to destination. Before the transfer
 * all cached objects in destination are freed. */
extern int move_objectcache( object_cache_t * destination, object_cache_t * source ) ;

#endif
