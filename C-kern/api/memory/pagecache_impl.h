/* title: MemoryPageCache

   Cache for virtual memory pages.

   Virtual memory pages are allocated in big chunks with help of <init_vmpage>.
   This component makes calling into the operating system unnecessary
   every time you need a new page or want ot free one.

   It offers pages of size 4096 bytes no matter
   of the page size of the operating system.

   Other page sizes could be added supported if necessary.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/pagecache_impl.h
    Header file <MemoryPageCache>.

   file: C-kern/memory/pagecache_impl.c
    Implementation file <MemoryPageCache impl>.
*/
#ifndef CKERN_MEMORY_PAGECACHEIMPL_HEADER
#define CKERN_MEMORY_PAGECACHEIMPL_HEADER

// forward
struct memblock_t ;
struct dlist_node_t ;
struct pagecache_t ;
struct pagecache_blockarray_t ;

/* typedef: struct pagecache_impl_t
 * Export <pagecache_impl_t> into global namespace. */
typedef struct pagecache_impl_t           pagecache_impl_t ;

/* typedef: struct pagecache_block_t
 * Export opaque type <pagecache_block_t> into global namespace.
 * This type stores information about a large block of memory of at least 1MB. */
typedef struct pagecache_block_t          pagecache_block_t ;

/* typedef: struct pagecache_blockmap_t
 * Export type <pagecache_blockmap_t> into global namespace.
 * It serves as hashtable for a set of <pagecache_block_t>.
 * The hashtable is shared between all <pagecache_impl_t>. */
typedef struct pagecache_blockmap_t       pagecache_blockmap_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_pagecacheimpl
 * Test <pagecache_impl_t> functionality. */
int unittest_memory_pagecacheimpl(void) ;
#endif


/* struct: pagecache_impl_t
 * Allocates and frees virtual memory pages and caches them.
 * The <pagecache_impl_t> allocates always blocks of memory pages <pagecache_block_t>.
 * Every <pagecache_block_t> maintains a list of free pages of the same size.
 * For every pagesize a new <pagecache_block_t> is allocated.
 * If several <pagecache_block_t> are allocated and one of them is not used at all
 * it is returned to the operating system.
 *
 * TODO: To prevent fragmentation of large blocks introduce page lifetime (long, short lifetime, ...).
 *       This helps to place longer living pages on one big block and short living pages on another.
 *       Blocks with pages with short lifetime can be reclaimed faster.
 */
struct pagecache_impl_t {
   /* variable: blocklist
    * A list of <pagecache_block_t>.
    * This list contains all allocated blocks and serves to free all allocated blocks.
    * Objects of type <pagecache_block_t> are managed on the memory heap and are not
    * part of the block of the allocated pages. */
   struct {
      struct dlist_node_t *   last ;
   }                       blocklist ;
   /* variable: freelist
    * An array of list of allocated blocks which contain free pages.
    * A single large block contains only pages of one size. Different blocks serve different sizes.
    * Therefore every page size has its own free list.
    * This my be changed for example by using the the "buddy memory allocation" technique. */
   struct {
      struct dlist_node_t *   last ;
   }                       freeblocklist[6] ;
   /* variable: staticpagelist
    * A list of static memory blocks.
    * Functions <allocstatic_pagecacheimpl> and <freestatic_pagecacheimpl> could change this list. */
   struct {
      struct dlist_node_t *   last ;
   }                       staticpagelist ;
   /* variable: sizeallocated
    * Number of allocated bytes.
    * This number is incremented by every call to <allocpage_pagecacheimpl>
    * and decremented by every call to <releasepage_pagecacheimpl>. */
   size_t                  sizeallocated ;
   /* variable: sizestatic
    * Number of allocated bytes from static memory. Value is incremented by every
    * call to <allocstatic_pagecacheimpl> and decremented by every call to <freestatic_pagecacheimpl>. */
   size_t                  sizestatic ;
} ;

// group: init

/* function: initthread_pagecacheimpl
 * Calls <init_pagecacheimpl> and adds interface <pagecache_it> to object.
 * This function is called from <init_threadcontext>. */
int initthread_pagecacheimpl(/*out*/struct pagecache_t * pagecache) ;

/* function: freethread_pagecacheimpl
 * Calls <free_pagecacheimpl> with for object pointer in <pagecache_t>.
 * This function is called from <free_threadcontext>. */
int freethread_pagecacheimpl(struct pagecache_t * pagecache) ;

// group: lifetime

/* define: pagecache_impl_INIT_FREEABLE
 * Static initializer. */
#define pagecache_impl_INIT_FREEABLE      { { 0 }, { { 0 } }, { 0 }, 0, 0 }

/* function: init_pagecacheimpl
 * Preallocates at least 1MB of memory and initializes pgcache. */
int init_pagecacheimpl(/*out*/pagecache_impl_t * pgcache) ;

/* function: free_pagecacheimpl
 * Frees all allocated memory pages even if they are in use.
 * All allocated memory pages are returned to the operating system.
 * Make sure that every component has called <releasepage_pagecacheimpl>
 * before this function is called. Or that allocated pages are never
 * used after calling this function.
 * In not all memory has been freed the error ENOTEMPTY is returned but
 * memory is freed nevertheless. */
int free_pagecacheimpl(pagecache_impl_t * pgcache) ;

// group: query

/* function: sizeallocated_pagecacheimpl
 * Returns the sum of the size of all allocated pages.
 * Static memory is also allocated with help of <allocpage_pagecacheimpl>. */
size_t sizeallocated_pagecacheimpl(const pagecache_impl_t * pgcache) ;

/* function: sizestatic_pagecacheimpl
 * Returns wize of memory allocated with <allocstatic>. */
size_t sizestatic_pagecacheimpl(const pagecache_impl_t * pgcache) ;

/* function: isfree_pagecacheimpl
 * Returns true if pgcache equals <pagecache_impl_INIT_FREEABLE>. */
bool isfree_pagecacheimpl(const pagecache_impl_t * pgcache) ;

// group: alloc

/* function: allocpage_pagecacheimpl
 * Allocates a single memory page of size pgsize.
 * The page is aligned to its own size. */
int allocpage_pagecacheimpl(pagecache_impl_t * pgcache, uint8_t pgsize, /*out*/struct memblock_t * page) ;

/* function: releasepage_pagecacheimpl
 * Releases a single memory page. It is kept in the cache and only returned to
 * the operating system if a big chunk of memory is not in use.
 * After return page is set to <memblock_INIT_FREEABLE>. Calling this function with
 * page set to <memblock_INIT_FREEABLE> does nothing. */
int releasepage_pagecacheimpl(pagecache_impl_t * pgcache, struct memblock_t * page) ;

/* function: allocstatic_pagecacheimpl
 * Allocates static memory which should live as long as pgcache.
 * These blocks can only be freed in the reverse order they hae been allocated.
 * The size of memory is set in bytes with parameter bytesize.
 * The allocated memory block (aligned to <KONFIG_MEMALIGN>) is returned in memblock.
 * In case of no memory ENOMEM is returned. The size of the block if restricted to 128 bytes.
 * Calling this function increases <sizeallocated_pagecacheimpl> or not if bytesize bytes fits
 * on a previously allocated page. */
int allocstatic_pagecacheimpl(pagecache_impl_t * pgcache, size_t bytesize, /*out*/struct memblock_t * memblock) ;

/* function: freestatic_pagecacheimpl
 * Frees static memory.
 * If this function is not called in the reverse order of the call sequence of <allocstatic_pagecacheimpl>
 * the value EINVAL is returned and nothing is done.
 * After return memblock is set to <memblock_INIT_FREEABLE>.
 * Calling this function with memblock set to <memblock_INIT_FREEABLE> does nothing. */
int freestatic_pagecacheimpl(pagecache_impl_t * pgcache, struct memblock_t * memblock) ;

// group: cache

/* function: emptycache_pagecacheimpl
 * Releases all unused memory blocks back to the operating system. */
int emptycache_pagecacheimpl(pagecache_impl_t * pgcache) ;


/* struct: pagecache_blockmap_t
 * TODO: */
struct pagecache_blockmap_t {
   uint8_t *   array_addr ;
   size_t      array_size ;
   size_t      array_len ;
   size_t      indexmask ;
} ;

// group: config

/* define: pagecache_blockmap_ARRAYSIZE
 * Static configuration of number of bytes used for the hash table.
 * The number of supported <pagecache_block_t> is fixed cause only
 * one entry per hash bucket is supported with no overflow handling.
 * pagecache_blockmap_t is considered to be implemented in hardware. */
#define pagecache_blockmap_ARRAYSIZE      (2*1024*1024)

// group: lifetime

/* define: pagecache_blockmap_INIT_FREEABLE
 * Static initializer. */
#define pagecache_blockmap_INIT_FREEABLE  { 0, 0, 0, 0 }

/* function: init_pagecacheblockmap
 * TODO: */
int init_pagecacheblockmap(pagecache_blockmap_t * blockmap) ;

/* function: free_pagecacheblockmap
 * TODO: */
int free_pagecacheblockmap(pagecache_blockmap_t * blockmap) ;


#endif
