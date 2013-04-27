/* title: Valuecache
   Offers a simple cache mechanism for objects
   needed in submodules either often or which are costly
   to construct or deconstruct.

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

   file: C-kern/api/cache/valuecache.h
    Header file of <Valuecache>.

   file: C-kern/cache/valuecache.c
    Implementation file <Valuecache impl>.
*/
#ifndef CKERN_CACHE_VALUECACHE_HEADER
#define CKERN_CACHE_VALUECACHE_HEADER

/* typedef: struct valuecache_t
 * Exports <valuecache_t>. */
typedef struct valuecache_t            valuecache_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_cache_valuecache
 * Test allocation and free works. */
int unittest_cache_valuecache(void) ;
#endif


/* struct: valuecache_t
 * Caches values which have to be computed only once. */
struct valuecache_t {
   /* variable: pagesize_vm
    * The size of a virtual memory page in bytes.
    * Same value as returned by <sys_pagesize_vm>.
    * This value can be queried with function pagesize_vm. */
   uint32_t       pagesize_vm ;
   /* variable: log2pagesize_vm
    * The <log2_int> value of <pagesize_vm>. */
   uint8_t        log2pagesize_vm ;
} ;

// group: init

/* function: initonce_valuecache
 * Sets valuecache pointer to a singleton object. */
int initonce_valuecache(/*out*/valuecache_t ** valuecache) ;

/* function: freeonce_valuecache
 * Resets the pointer to null. Singleton is never freed. */
int freeonce_valuecache(valuecache_t ** valuecache) ;

// group: lifetime

/* define: valuecache_INIT_FREEABLE
 * Static initializer. */
#define valuecache_INIT_FREEABLE \
         { 0, 0 }

#endif
