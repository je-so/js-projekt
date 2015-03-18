/* title: PageCacheImpl

   Cache for virtual memory pages.

   Virtual memory pages are allocated in big chunks with help of <init_vmpage>.
   This component makes calling into the operating system unnecessary
   every time you need a new page or want ot free one.

   It offers pages of size 4096 bytes no matter
   of the page size of the operating system.

   Other page sizes could be added supported if necessary.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/pagecache_impl.h
    Header file <PageCacheImpl>.

   file: C-kern/memory/pagecache_impl.c
    Implementation file <PageCacheImpl impl>.
*/
#ifndef CKERN_MEMORY_PAGECACHEIMPL_HEADER
#define CKERN_MEMORY_PAGECACHEIMPL_HEADER

#include "C-kern/api/memory/pagecache.h"

// forward
struct memblock_t;
struct dlist_node_t;

/* typedef: struct pagecache_impl_t
 * Export <pagecache_impl_t> into global namespace. */
typedef struct pagecache_impl_t pagecache_impl_t;

/* typedef: struct pagecache_block_t
 * Export opaque type <pagecache_block_t> into global namespace.
 * This type stores information about a large block of memory of at least 1MB. */
typedef struct pagecache_block_t pagecache_block_t;

/* typedef: struct pagecache_blockmap_t
 * Export type <pagecache_blockmap_t> into global namespace.
 * It serves as hashtable for a set of <pagecache_block_t>.
 * The hashtable is shared between all <pagecache_impl_t>. */
typedef struct pagecache_blockmap_t pagecache_blockmap_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_pagecacheimpl
 * Test <pagecache_impl_t> functionality. */
int unittest_memory_pagecacheimpl(void);
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
 *       -- See also "Region-based memory management"
 *
 */
struct pagecache_impl_t {
   // group: private fields
   /* variable: blocklist
    * A list of <pagecache_block_t>.
    * This collection is used to free all allocated memory blocks.
    * Objects of type <pagecache_block_t> are stored on the memory heap
    * (managed by <pagecache_blockmap_t>) and are not part of the allocated memory blocks. */
   struct {
      struct dlist_node_t* last;
   }        blocklist;
   /* variable: freelist
    * An array of list of allocated blocks which contain free pages.
    * A single large block contains only pages of one size. Different blocks serve different sizes.
    * Therefore every page size has its own free list.
    * This may be changed for example by using the the "buddy memory allocation" technique. */
   struct {
      struct dlist_node_t* last;
   }        freeblocklist[13];
   /* variable: sizeallocated
    * Number of allocated bytes.
    * This number is incremented by every call to <allocpage_pagecacheimpl>
    * and decremented by every call to <releasepage_pagecacheimpl>. */
   size_t   sizeallocated;
};

// group: initthread

/* function: interface_pagecacheimpl
 * This function is called from <threadcontext_t.init_threadcontext>.
 * Used to initialize interface part of <pagecache_t>. */
struct pagecache_it* interface_pagecacheimpl(void);

// group: lifetime

/* define: pagecache_impl_FREE
 * Static initializer. */
#define pagecache_impl_FREE \
         { { 0 }, { { 0 } }, 0 }

/* function: init_pagecacheimpl
 * Preallocates at least 1MB of memory and initializes pgcache. */
int init_pagecacheimpl(/*out*/pagecache_impl_t* pgcache);

/* function: free_pagecacheimpl
 * Frees all allocated memory pages even if they are in use.
 * All allocated memory pages are returned to the operating system.
 * Make sure that every component has called <releasepage_pagecacheimpl>
 * before this function is called. Or that allocated pages are never
 * used after calling this function.
 * In not all memory has been freed the error ENOTEMPTY is returned but
 * memory is freed nevertheless. */
int free_pagecacheimpl(pagecache_impl_t* pgcache);

// group: query

/* function: sizeallocated_pagecacheimpl
 * Returns the sum of the size of all allocated pages.
 * Static memory is also allocated with help of <allocpage_pagecacheimpl>. */
size_t sizeallocated_pagecacheimpl(const pagecache_impl_t* pgcache);

/* function: isfree_pagecacheimpl
 * Returns true if pgcache equals <pagecache_impl_FREE>. */
bool isfree_pagecacheimpl(const pagecache_impl_t* pgcache);

// group: alloc

/* function: allocpage_pagecacheimpl
 * Allocates a single memory page of size pgsize.
 * The page is aligned to its own size. */
int allocpage_pagecacheimpl(pagecache_impl_t* pgcache, uint8_t pgsize, /*out*/struct memblock_t* page);

/* function: releasepage_pagecacheimpl
 * Releases a single memory page. It is kept in the cache and only returned to
 * the operating system if a big chunk of memory is not in use.
 * After return page is set to <memblock_FREE>. Calling this function with
 * page set to <memblock_FREE> does nothing. */
int releasepage_pagecacheimpl(pagecache_impl_t* pgcache, struct memblock_t* page);

// group: cache

/* function: emptycache_pagecacheimpl
 * Releases all unused memory blocks back to the operating system. */
int emptycache_pagecacheimpl(pagecache_impl_t* pgcache);


/* struct: pagecache_blockmap_t
 * A simple shared hash map which maps an index <pagecache_block_t>.
 * The array size (table size) is static which allows for efficient access
 * without <rwlock_t> or any other type of locks.
 * Atomic operations to read and write <pagecache_block_t.threadcontext> ensure
 * that an entry is used by only one thread.
 *
 * Use <pagecache_blockmap_ARRAYSIZE> to configure the static size of the array. */
struct pagecache_blockmap_t {
   uint8_t* array_addr;
   size_t   array_size;
   size_t   array_len;
   size_t   indexmask;
};

// group: config

/* define: pagecache_blockmap_ARRAYSIZE
 * Static configuration of number of bytes used for the hash table.
 * The number of supported <pagecache_block_t> is fixed cause only
 * one entry per hash bucket is supported with no overflow handling.
 * pagecache_blockmap_t is considered to be implemented in hardware. */
#define pagecache_blockmap_ARRAYSIZE \
         (2*1024*1024)

// group: lifetime

/* define: pagecache_blockmap_FREE
 * Static initializer. */
#define pagecache_blockmap_FREE \
         { 0, 0, 0, 0 }

/* function: init_pagecacheblockmap
 * Allocates a shared hash table of size <pagecache_blockmap_FREE>. */
int init_pagecacheblockmap(/*out*/pagecache_blockmap_t* blockmap);

/* function: free_pagecacheblockmap
 * Frees allocated hash table. Make sure that no <pagecache_block_t> is in use. */
int free_pagecacheblockmap(pagecache_blockmap_t* blockmap);


#endif
