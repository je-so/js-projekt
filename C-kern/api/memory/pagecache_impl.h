/* title: PageCacheImpl

   Cache for virtual memory pages.

   Virtual memory pages are allocated in big chunks with help of <init_vmpage>.
   This component makes calling into the operating system unnecessary
   every time you need a new page or want to free one.

   It offers pages of size 4096 bytes no matter
   of the page size of the operating system.

   Other page sizes could be supported if necessary.

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

// export
struct pagecache_impl_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_pagecacheimpl
 * Test <pagecache_impl_t> functionality. */
int unittest_memory_pagecacheimpl(void);
#endif


/* struct: pagecache_impl_t
 * Allocates and frees virtual memory pages and caches them exclusively for one thread.
 * This type is *not* thread safe. So you should use it only in a thread context.
 * The <pagecache_impl_t> allocates always blocks of memory of size pagecache_impl_BLOCKSIZE.
 * Every <block_t> is divided into sub-blocks of size pagecache_impl_SUBBLOCKSIZE.
 * For every allocation of a page with size pagesize either a new unused sub-block is allocated
 * or one which contains free pages is used.
 *
 * TODO: To prevent fragmentation of large blocks introduce page lifetime (long, short lifetime, ...).
 *       This helps to place longer living pages on one big block and short living pages on another.
 *       Blocks with pages with short lifetime can be reclaimed faster.
 *       -- See also "Region-based memory management"
 *
 */
typedef struct pagecache_impl_t {
   // group: private fields
   /* variable: blocklist
    * A list of internal <block_t>.
    * This collection is used to free all allocated memory blocks.
    * Objects of type <block_t> are allocated and managed by the
    * virtual memory subsystem of the OS. */
   struct {
      struct dlist_node_t *last;
   }        blocklist;
   /* variable: unusedblocklist
    * A list of internal <block_t> which contains unused <subblock_t>.
    * A subblock manages a set of size aligned memory pages which could be of any <pagesize_e>. */
   struct {
      struct dlist_node_t *last;
   }        unusedblocklist;
   /* variable: freeblocklist
    * Every list manages a set of <block_t> which contains free pages of a certain <pagesize_e>. */
   struct {
      struct dlist_node_t *last;
   }        freeblocklist[13];
   /* variable: sizeallocated
    * Number of allocated bytes.
    * This number is incremented by every call to <allocpage_pagecacheimpl>
    * and decremented by every call to <releasepage_pagecacheimpl>. */
   size_t   sizeallocated;
} pagecache_impl_t;

// group: config

/* define: pagecache_impl_SUBBLOCKSIZE
 * Size in bytes of a single sub-block which is contained in a block.
 * Currently a sub-block has a size of 1MB. Must be a power of two.
 * A sub-block manages pages of a single size. */
#define pagecache_impl_SUBBLOCKSIZE \
         (1024u*1024u)

/* define: pagecache_impl_NRSUBBLOCKS
 * The number of sub-blocks contained in a single block. Must be a power of two. */
#if (SIZE_MAX <= 0xffffffff)  // 32 bit (or smaller)
#define pagecache_impl_NRSUBBLOCKS 32u
#else                         // 64 bit (or bigger)
#define pagecache_impl_NRSUBBLOCKS 1024u
#endif

/* define: pagecache_impl_BLOCKSIZE
 * Size in bytes of a single block which is divided into many sub-blocks.
 * A block is the unit memory is transfered between OS and this process. */
#define pagecache_impl_BLOCKSIZE \
         (pagecache_impl_NRSUBBLOCKS*pagecache_impl_SUBBLOCKSIZE)


// group: initthread

/* function: interface_pagecacheimpl
 * This function is called from <threadcontext_t.init_threadcontext>.
 * Used to initialize interface part of <pagecache_t>. */
struct pagecache_it* interface_pagecacheimpl(void);

// group: lifetime

/* define: pagecache_impl_FREE
 * Static initializer. */
#define pagecache_impl_FREE \
         { { 0 }, { 0 }, { { 0 } }, 0 }

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


#endif
