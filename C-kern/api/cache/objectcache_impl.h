/* title: Objectcache

   Offers a simple cache mechanism for objects needed in submodules
   which are costly to construct or deconstruct.

   Implements <initthread_objectcacheimpl> and <freethread_objectcacheimpl> to allocate
   storage for cached objects before a new thread is created and frees all storage
   before the thread exits.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/cache/objectcache_impl.h
    Header file of <Objectcache>.

   file: C-kern/cache/objectcache_impl.c
    Implementation file <Objectcache impl>.
*/
#ifndef CKERN_CACHE_OBJECTCACHE_IMPL_HEADER
#define CKERN_CACHE_OBJECTCACHE_IMPL_HEADER

// forward
struct memblock_t ;

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
 * Holds pointers to all cached objects.
 * TODO: remove iobuffer and add other types ! */
struct objectcache_impl_t {
   /* variable: iobuffer
    * Used in <ALLOC_PAGECACHE>. */
   struct {
      uint8_t *   addr ;
      size_t      size ;
   }           iobuffer ;
} ;

// group: initthread

/* function: interface_objectcacheimpl
 * This function is called from <threadcontext_t.init_threadcontext>.
 * Used to initialize interface of <objectcache_t>. */
struct objectcache_it * interface_objectcacheimpl(void) ;

// group: lifetime

/* define: objectcache_impl_FREE
 * Static initializer. */
#define objectcache_impl_FREE \
         { vmpage_FREE }

/* function: init_objectcacheimpl
 * Inits <objectcache_impl_t> and all contained objects. */
int init_objectcacheimpl(/*out*/objectcache_impl_t * objectcache) ;

/* function: free_objectcacheimpl
 * Frees <objectcache_impl_t> and all contained objects. */
int free_objectcacheimpl(objectcache_impl_t * objectcache) ;

// group: access

/* function: lockiobuffer_objectcacheimpl
 * Locks the io buffer and returns a pointer to it in iobuffer.
 * The buffer is of type <vmpage_t> (compatible with <memblock_t>). */
void lockiobuffer_objectcacheimpl(objectcache_impl_t * objectcache, /*out*/struct memblock_t ** iobuffer) ;

/* function: unlockiobuffer_objectcacheimpl
 * Unlocks the locked io buffer and sets the pointer to NULL.
 * The pointer to the buffer must be acquired by a previous call to <lockiobuffer_objectcacheimpl>.
 * Calling unlock with a NULL pointer is a no op. */
void unlockiobuffer_objectcacheimpl(objectcache_impl_t * objectcache, struct memblock_t ** iobuffer) ;


#endif
