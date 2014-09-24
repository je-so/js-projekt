/* title: Valuecache
   Offers a simple cache mechanism for objects
   needed in submodules either often or which are costly
   to construct or deconstruct.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
typedef struct valuecache_t               valuecache_t ;


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

// group: lifetime

/* define: valuecache_FREE
 * Static initializer. */
#define valuecache_FREE { 0, 0 }

/* function: init_valuecache
 * Sets values of valuecache. */
int init_valuecache(/*out*/valuecache_t * valuecache) ;

/* function: free_valuecache
 * Clears values of valuecache. */
int free_valuecache(valuecache_t * valuecache) ;

#endif
