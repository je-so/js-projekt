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

   file: C-kern/api/memory/pagecache.h
    Header file <MemoryPageCache>.

   file: C-kern/memory/pagecache.c
    Implementation file <MemoryPageCache impl>.
*/
#ifndef CKERN_MEMORY_PAGECACHE_HEADER
#define CKERN_MEMORY_PAGECACHE_HEADER

// forward
struct memblock_t ;
struct dlist_node_t ;

/* typedef: struct pagecache_t
 * Export <pagecache_t> into global namespace. */
typedef struct pagecache_t                pagecache_t ;

/* enums: pagesize_e
 * List of supported page sizes.
 * pagesize_256   - Request page size of 256 byte aligned to a 256 byte boundary.
 * pagesize_1024  - Request page size of 1024 byte aligned to a 1024 byte boundary.
 * pagesize_4096  - Request page size of 4096 byte aligned to a 4096 byte boundary.
 * pagesize_16384 - Request page size of 16384 byte aligned to a 4096 byte boundary.
 * */
enum pagesize_e {
   pagesize_256,
   pagesize_1024,
   pagesize_4096,
   pagesize_16384,
   pagesize_NROFPAGESIZE
} ;

typedef enum pagesize_e                   pagesize_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_pagecache
 * Test <pagecache_t> functionality. */
int unittest_memory_pagecache(void) ;
#endif


/* struct: pagecache_t
 * Allocates and frees virtual memory pages and caches them.
 * The <pagecache_t> allocates always blocks of memory pages <pagecache_block_t>.
 * Every <pagecache_block_t> maintains a list of free pages of the same size.
 * For every pagesize a new <pagecache_block_t> is allocated.
 * If several <pagecache_block_t> are allocated and one of them is not used at all
 * it is returned to the operating system.
 *
 * TODO: To prevent fragmentation of large blocks introduce page lifetime (long, short lifetime, ...).
 *       This helps to place longer living pages on one big block and short living pages on another.
 *       Blocks with pages with short lifetime can be reclaimed faster.
 */
struct pagecache_t {
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
   }                       freeblocklist[pagesize_NROFPAGESIZE] ;
   /* variable: sizeallocated
    * Number of allocated bytes.
    * This number is incremented by every call to <allocpage_pagecache>
    * and decremented by every call to <releasepage_pagecache>. */
   size_t                  sizeallocated ;
} ;

// group: lifetime

/* define: pagecache_INIT_FREEABLE
 * Static initializer. */
#define pagecache_INIT_FREEABLE           { { 0 }, { { 0 } }, 0 }

/* function: init_pagecache
 * Preallocates at least 1MB of memory and initializes pgcache. */
int init_pagecache(/*out*/pagecache_t * pgcache) ;

/* function: free_pagecache
 * Frees all allocated memory pages even if they are in use.
 * All allocated memory pages are returned to the operating system.
 * Make sure that every component has called <releasepage_pagecache>
 * before this function is called. Or that allocated pages are never
 * used after calling this function. */
int free_pagecache(pagecache_t * pgcache) ;

// group: query

/* function: sizeallocated_pagecache
 * Returns the sum of the size of all allocated pages. */
size_t sizeallocated_pagecache(const pagecache_t * pgcache) ;

/* function: isfree_pagecache
 * Returns true if pgcache equals <pagecache_INIT_FREEABLE>. */
bool isfree_pagecache(const pagecache_t * pgcache) ;

// group: update

/* function: allocpage_pagecache
 * Allocates a single memory page of size pgsize.
 * The page is aligned to its own size except pages bigger than 4096 bytes which
 * are aligned to 4096 bytes. */
int allocpage_pagecache(pagecache_t * pgcache, pagesize_e pgsize, /*out*/struct memblock_t * page) ;

/* function: releasepage_pagecache
 * Releases a single memory page. It is kept in the cache and only returned to
 * the operating system if a big chunk of memory is not in use.
 * After return page is set to <memblock_INIT_FREEABLE>. Calling this function with
 * page set to <memblock_INIT_FREEABLE> does nothing. */
int releasepage_pagecache(pagecache_t * pgcache, struct memblock_t * page) ;



// section: inline implementation

/* define: sizeallocated_pagecache
 * Implements <pagecache_t.sizeallocated_pagecache>. */
#define sizeallocated_pagecache(pgcache)  \
         ((size_t)(pgcache)->sizeallocated)

#endif
