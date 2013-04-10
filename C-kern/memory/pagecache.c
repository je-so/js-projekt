/* title: MemoryPageCache impl

   Implements <MemoryPageCache>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/memory/pagecache.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/ds/inmem/dlist.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

/* typedef: struct pagecache_block_t
 * Export opaque type <pagecache_block_t> into global namespace.
 * This type stores information about a large block of memory of at least 1MB. */
typedef struct pagecache_block_t          pagecache_block_t ;

/* typedef: struct freepage_t
 * Export type <freepage_t> into global namespace. */
typedef struct freepage_t                 freepage_t ;


/* struct: freepage_t
 * Header of free page located in <pagecache_block_t.pageblock>. */
struct freepage_t {
   slist_node_t *       next ;
   pagecache_block_t *  marker ;
} ;

// group: helper

/* define: slist_IMPLEMENT _freepagelist
 * Macro generates slist interface FCT_freepagelist for <freepage_t>. */
slist_IMPLEMENT(_freepagelist, freepage_t, next)


/* struct: pagecache_block_t
 * Stores information about a block of memory pages.
 * This type allocates and frees such a block of contiguous memory pages.
 * It stores a reference to the first page and the size of all pages together.
 * It also manages allocation and releasing of pages cut out from the big memory block. */
struct pagecache_block_t {
   vmpage_t       pageblock ;
   /* variable: next_block
    * Used to store all allocated <pagecache_block_t> in a list. */
   dlist_node_t   next_block ;
   /* variable: next_freeblock
    * Used to store <pagecache_block_t> which contain free pages. */
   dlist_node_t   next_freeblock ;
   /* variable: freepagelist
    * List of free pages. */
   slist_t        freepagelist ;
   /* variable: pagesize
    * Size of a single page stored in freepagelist. */
   size_t         pagesize ;
   /* variable: usedpagecount
    * Counts the number of allocated pages in use. */
   uint16_t       usedpagecount ;
   /* variable: freelistidx
    * Index into <pagecache_t.freeblocklist>. */
   uint8_t        freelistidx ;
} ;

// group: variable
#ifdef KONFIG_UNITTEST
static test_errortimer_t               s_pagecacheblock_errtimer ;
#endif

// group: constants

#define pagecache_block_BLOCKSIZE      (1024*1024)

// group: helper

/* define: slist_IMPLEMENT _blocklist
 * Macro generates slist interface FCT_blocklist for <pagecache_block_t>. */
dlist_IMPLEMENT(_blocklist, pagecache_block_t, next_block.)

/* define: slist_IMPLEMENT _freeblocklist
 * Macro generates slist interface FCT_freeblocklist for <pagecache_block_t>. */
dlist_IMPLEMENT(_freeblocklist, pagecache_block_t, next_freeblock.)

static inline size_t blocksize_pagecacheblock(const size_t syspagesize)
{
   return (syspagesize < pagecache_block_BLOCKSIZE) ? pagecache_block_BLOCKSIZE : syspagesize ;
}

// group: lifetime

/* function: new_pagecacheblock
 * Allocates a big block of memory and returns its description in <pagecache_block_t>.
 * The returned <pagecache_block_t> is allocated on the heap. */
int new_pagecacheblock(/*out*/struct pagecache_block_t ** block, size_t pagesize, uint8_t freelistindex)
{
   int err ;
   memblock_t     memblock  = memblock_INIT_FREEABLE ;
   vmpage_t       pageblock = vmpage_INIT_FREEABLE ;
   size_t         blocksize = blocksize_pagecacheblock(pagesize_vm()) ;

   // TODO: change second argument of init_vmpage from nrofpages into into size_in_bytes
   ONERROR_testerrortimer(&s_pagecacheblock_errtimer, ONABORT) ;
   err = init_vmpage(&pageblock, blocksize / pagesize_vm()) ;
   if (err) goto ONABORT ;

   // align pageblock to boundary of pagecache_block_BLOCKSIZE
   // TODO: implement initaligned_vmpage( power of two size ) => aligned to size of page
   //       shift functionality from this place into module vm.c
   if (blocksize > pagesize_vm()) {
      if (0 != ((uintptr_t)pageblock.addr % pagecache_block_BLOCKSIZE)) {
         err = free_vmpage(&pageblock) ;
         if (err) goto ONABORT ;
         err = init_vmpage(&pageblock, 2*blocksize / pagesize_vm()) ;
         if (err) goto ONABORT ;

         uintptr_t offset = (uintptr_t)pageblock.addr % pagecache_block_BLOCKSIZE ;
         size_t   hdsize = offset ? pagecache_block_BLOCKSIZE - offset : 0 ;

         vmpage_t header = vmpage_INIT(hdsize, pageblock.addr) ;
         pageblock.addr += hdsize ;
         pageblock.size -= hdsize ;
         err = free_vmpage(&header) ;
         if (err) goto ONABORT ;

         vmpage_t trailer = vmpage_INIT(pageblock.size - pagecache_block_BLOCKSIZE, pageblock.addr + pagecache_block_BLOCKSIZE) ;
         pageblock.size = pagecache_block_BLOCKSIZE ;
         err = free_vmpage(&trailer) ;
         if (err) goto ONABORT ;
      }
   }

   err = RESIZE_MM(sizeof(pagecache_block_t), &memblock) ;
   if (err) goto ONABORT ;

   // init new object
   pagecache_block_t * new_block = (pagecache_block_t *) memblock.addr ;
   new_block->pageblock = pageblock ;
   // Member new_block->next_block set in calling function
   // Member new_block->next_freeblock set in calling function
   new_block->freepagelist = (slist_t) slist_INIT ;
   new_block->pagesize     = pagesize ;
   new_block->usedpagecount= 0 ;
   new_block->freelistidx  = freelistindex ;
   for (size_t pageoffset = 0 ; pageoffset < pagecache_block_BLOCKSIZE; pageoffset += pagesize) {
      freepage_t * freepage = (freepage_t*) (pageblock.addr + pageoffset) ;
      freepage->marker = new_block ;
      insertlast_freepagelist(&new_block->freepagelist, freepage) ;
   }

   // init out value
   *block = new_block ;

   return 0 ;
ONABORT:
   free_vmpage(&pageblock) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

/* function: delete_pagecacheblock
 * Frees block and also the block of memory pages it references. */
int delete_pagecacheblock(/*out*/struct pagecache_block_t ** block)
{
   int err ;
   int err2 ;
   pagecache_block_t * del_block = *block ;

   if (del_block) {
      *block = 0 ;

      err = free_vmpage(&del_block->pageblock) ;
#ifdef KONFIG_UNITTEST
      err2 = process_testerrortimer(&s_pagecacheblock_errtimer) ;
      if (err2) err = err2 ;
#endif

      memblock_t memblock = memblock_INIT(sizeof(pagecache_block_t), (uint8_t*)del_block) ;
      err2 = FREE_MM(&memblock) ;
      if (err2) err = err2 ;
#ifdef KONFIG_UNITTEST
      err2 = process_testerrortimer(&s_pagecacheblock_errtimer) ;
      if (err2) err = err2 ;
#endif

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: update

int releasepage_pagecacheblock(struct pagecache_block_t * block, freepage_t * freepage)
{
   // marker indicates that it is already stored in free list
   if (block == freepage->marker) {
      foreach (_freepagelist, nextfreepage, &block->freepagelist) {
         if (freepage == nextfreepage) {
            return EALREADY ;  // already stored in freepagelist
         }
      }
   } else {
      freepage->marker = block ;
   }

   insertlast_freepagelist(&block->freepagelist, freepage) ;
   -- block->usedpagecount ;

   return 0 ;
}

int allocpage_pagecacheblock(struct pagecache_block_t * block, freepage_t ** freepage)
{
   int err ;

   err = removefirst_freepagelist(&block->freepagelist, freepage) ;
   if (err) return err ;

   (*freepage)->marker = 0 ;
   ++ block->usedpagecount ;

   return 0 ;
}


// section: pagecache_t

// group: helper

/* function: pagesizeinbytes_pagecache
 * Translates enum <pagesize_e> into size in bytes. */
static inline size_t pagesizeinbytes_pagecache(pagesize_e pgsize)
{
   return (size_t)256 << (2*pgsize) ;
}

/* function: findblock_pagecache
 * Find block who owns pageaddr in pgcache.
 * The block is searched with help of <pagecache_t.blocklist>.
 * The result is returned in block. If no block is found ESRCH is returned. */
static inline int findblock_pagecache(pagecache_t * pgcache, const uint8_t * pageaddr, /*out*/pagecache_block_t ** block)
{
   // TODO: use array data type to replace linear search with simple array access
   //       replace list with implementation of arrayf_t !!

   foreach (_blocklist, nextblock, genericcast_dlist(&pgcache->blocklist)) {
      if (  nextblock->pageblock.addr <= pageaddr
            && pageaddr < nextblock->pageblock.addr + nextblock->pageblock.size) {
            *block = nextblock ;
         return 0 ;
      }
   }

   return ESRCH ;
}

/* function: findfreeblock_pagecache
 * Find empty <pageblock_block_t> on freelist and return it in freeblock.
 * The freelist <pagecache_t.freeblocklist> at index pgsize is searched.
 * All encountered <pageblock_block_t> which are empty are removed from the list.
 * If there is no empty block for the corresponding pgsize the error ESRCH is returned. */
static inline int findfreeblock_pagecache(pagecache_t * pgcache, pagesize_e pgsize, pagecache_block_t ** freeblock)
{
   int err ;

   // Use some kind of priority queue ?

   err = ESRCH ;

   foreach (_freeblocklist, block, genericcast_dlist(&pgcache->freeblocklist[pgsize])) {
      if (!isempty_slist(&block->freepagelist)) {
         *freeblock = block ;
         err = 0 ;
         break ;
      }
   }

   return err ;
}

/* function: allocblock_pagecache
 * Allocate new <pageblock_block_t> and add it to freelist.
 * The new block is returned in block if this value is not NULL.
 * The allocated block is inserted in freelist <pagecache_t.freeblocklist> at index pgsize.
 * The allocated block is also inserted into list <pagecache_t.blocklist>.
 * In case of error ENOMEM is returned. */
static inline int allocblock_pagecache(pagecache_t * pgcache, pagesize_e pgsize, /*out*/pagecache_block_t ** block)
{
   int err ;
   pagecache_block_t *  freeblock ;

   err = new_pagecacheblock(&freeblock, pagesizeinbytes_pagecache(pgsize), pgsize) ;
   if (err) return err ;

   insertlast_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[pgsize]), freeblock) ;
   insertlast_blocklist(genericcast_dlist(&pgcache->blocklist), freeblock) ;

   if (block) *block = freeblock ;

   return 0 ;
}

// group: lifetime

int init_pagecache(/*out*/pagecache_t * pgcache)
{
   int err ;

   *pgcache = (pagecache_t) pagecache_INIT_FREEABLE ;

   err = allocblock_pagecache(pgcache, pagesize_4096, 0) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_pagecache(pagecache_t * pgcache)
{
   int err ;
   int err2 ;

   err = 0 ;

   foreach (_blocklist, nextblock, genericcast_dlist(&pgcache->blocklist)) {
      err2 = delete_pagecacheblock(&nextblock) ;
      if (err2) err = err2 ;
   }

   *pgcache = (pagecache_t) pagecache_INIT_FREEABLE ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

bool isfree_pagecache(const pagecache_t * pgcache)
{
   if (0 != pgcache->blocklist.last) return false ;

   for (unsigned i = 0; i < lengthof(pgcache->freeblocklist); ++i) {
      if (0 != pgcache->freeblocklist[i].last) return false ;
   }

   return 0 == pgcache->sizeallocated ;
}

// group: update

int allocpage_pagecache(pagecache_t * pgcache, pagesize_e pgsize, /*out*/struct memblock_t * page)
{
   int err ;
   VALIDATE_INPARAM_TEST(pgsize < lengthof(pgcache->freeblocklist), ONABORT, ) ;

   pagecache_block_t * freeblock ;

   err = findfreeblock_pagecache(pgcache, pgsize, &freeblock) ;
   if (err == ESRCH) {
      err = allocblock_pagecache(pgcache, pgsize, &freeblock) ;
   }
   if (err) goto ONABORT ;

   freepage_t * freepage ;
   err = allocpage_pagecacheblock(freeblock, &freepage) ;
   if (err) goto ONABORT ;
   if (isempty_slist(&freeblock->freepagelist)) {
      err = removefirst_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[pgsize]), &freeblock) ;
      if (err) goto ONABORT ;
   }

   size_t pgsizeinbytes = pagesizeinbytes_pagecache(pgsize) ;
   pgcache->sizeallocated += pgsizeinbytes ;
   *page = (memblock_t) memblock_INIT(pgsizeinbytes, (uint8_t*)freepage) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int releasepage_pagecache(pagecache_t * pgcache, struct memblock_t * page)
{
   int err ;
   pagecache_block_t * block ;

   if (!isfree_memblock(page)) {
      if (  findblock_pagecache(pgcache, page->addr, &block)
            || block->pagesize != page->size
            || 0 != (((uintptr_t)page->addr) & ((uintptr_t)block->pagesize-1))) {
         err = EINVAL ;
         goto ONABORT ;
      }

      if (0 == releasepage_pagecacheblock(block, (freepage_t*)page->addr)) {
         pgcache->sizeallocated -= block->pagesize ;
         if (!isinlist_freeblocklist(block)) {
            insertfirst_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[block->freelistidx]), block) ;
         }

         // delete block if not used and at least another block is stored in free list
         pagecache_block_t * firstblock = last_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[block->freelistidx])) ;
         pagecache_block_t * lastblock  = first_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[block->freelistidx])) ;
         if (  ! block->usedpagecount
               && firstblock != lastblock) {
            err = remove_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[block->freelistidx]), block) ;
            if (err) goto ONABORT ;
            err = remove_blocklist(genericcast_dlist(&pgcache->blocklist), block) ;
            if (err) goto ONABORT ;
            err = delete_pagecacheblock(&block) ;
            if (err) goto ONABORT ;
         }
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_block(void)
{
   pagecache_block_t * block[12] = { 0 } ;

   // TEST blocksize_pagecacheblock
   for (size_t i = 1; i; i *= 2) {
      if (i <= pagecache_block_BLOCKSIZE) {
         TEST(blocksize_pagecacheblock(i) == pagecache_block_BLOCKSIZE) ;
      } else {
         TEST(blocksize_pagecacheblock(i) == i) ;
      }
   }

   // TEST new_pagecacheblock
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == new_pagecacheblock(&block[i], (size_t)256 << i, i)) ;
      TEST(0 != block[i]) ;
      TEST(0 == ((uintptr_t)block[i]->pageblock.addr % pagecache_block_BLOCKSIZE)) ;
      TEST(0 == block[i]->pageblock.size - pagecache_block_BLOCKSIZE) ;
      TEST(0 != block[i]->freepagelist.last) ;
      TEST(0 == block[i]->pagesize - ((size_t)256 << i)) ;
      TEST(0 == block[i]->usedpagecount) ;
      TEST(i == block[i]->freelistidx) ;
      // check list of free pages
      size_t pgoffset = 0 ;
      foreach (_slist, freepage, &block[i]->freepagelist) {
         TEST(freepage     == (void*)(block[i]->pageblock.addr + pgoffset)) ;
         TEST(block[i] == *(pagecache_block_t**)&freepage[1]) ;
         pgoffset += block[i]->pagesize ;
      }
   }

   // TEST delete_pagecacheblock
   for (unsigned i = 0; i < lengthof(block); ++i) {
      // check list of free pages
      size_t pgoffset = 0 ;
      foreach (_slist, freepage, &block[i]->freepagelist) {
         TEST(freepage     == (void*)(block[i]->pageblock.addr + pgoffset)) ;
         TEST(block[i] == *(pagecache_block_t**)&freepage[1]) ;
         pgoffset += block[i]->pagesize ;
      }
      TEST(0 == delete_pagecacheblock(&block[i])) ;
      TEST(0 == block[i]) ;
      TEST(0 == delete_pagecacheblock(&block[i])) ;
      TEST(0 == block[i]) ;
   }

   // TEST new_pagecacheblock: ENOMEM
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM   == new_pagecacheblock(&block[0], 4096, pagesize_4096)) ;
   TEST(block[0] == 0) ;

   // TEST delete_pagecacheblock: ENOMEM
   TEST(0 == new_pagecacheblock(&block[0], 4096, pagesize_4096)) ;
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM   == delete_pagecacheblock(&block[0])) ;
   TEST(block[0] == 0) ;
   TEST(0 == new_pagecacheblock(&block[0], 4096, pagesize_4096)) ;
   init_testerrortimer(&s_pagecacheblock_errtimer, 2, ENOMEM) ;
   TEST(ENOMEM   == delete_pagecacheblock(&block[0])) ;
   TEST(block[0] == 0) ;

   // TEST allocpage_pagecacheblock
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == new_pagecacheblock(&block[i], (size_t)256 << i, i)) ;
   }
   for (unsigned i = 0; i < lengthof(block); ++i) {
      size_t offset ;
      for (offset = 0; offset < block[i]->pageblock.size; offset += block[i]->pagesize) {
         freepage_t * freepage = 0 ;
         TEST(0 == allocpage_pagecacheblock(block[i], &freepage)) ;
         TEST(0 != freepage) ;
         TEST(0 == freepage->marker) ;
         TEST(freepage == (void*) (block[i]->pageblock.addr + offset)) ;
         TEST(0 == block[i]->usedpagecount - 1u - (offset/block[i]->pagesize)) ;
      }
      TEST(offset == block[i]->pageblock.size) ;
      TEST(isempty_freepagelist(&block[i]->freepagelist)) ;
   }

   // TEST allocpage_pagecacheblock: EINVAL
   for (unsigned i = 0; i < lengthof(block); ++i) {
      freepage_t * freepage = 0 ;
      TEST(isempty_freepagelist(&block[i]->freepagelist)) ;
      TEST(EINVAL   == allocpage_pagecacheblock(block[i], &freepage)) ;
      TEST(freepage == 0) ;
      TEST(0 == block[i]->usedpagecount - (block[i]->pageblock.size / block[i]->pagesize)) ;
   }

   // TEST releasepage_pagecacheblock
   for (unsigned i = 0; i < lengthof(block); ++i) {
      TEST(0 == block[i]->freepagelist.last) ;
      for (size_t offset = 0; offset < block[i]->pageblock.size; offset += block[i]->pagesize) {
         freepage_t * freepage = (void*) (block[i]->pageblock.addr + offset) ;
         TEST(0 == releasepage_pagecacheblock(block[i], freepage)) ;
         TEST(0 == block[i]->usedpagecount + 1u - ((block[i]->pageblock.size - offset) / block[i]->pagesize)) ;
         TEST(freepage         == (void*) block[i]->freepagelist.last) ;
         TEST(freepage->marker == block[i]) ;
         // double free does nothing
         TEST(EALREADY == releasepage_pagecacheblock(block[i], freepage)) ;
         TEST(freepage         == (void*) block[i]->freepagelist.last) ;
         TEST(freepage->marker == block[i]) ;
      }
   }
   for (unsigned i = 0; i < lengthof(block); ++i) {
      // check list of free pages
      size_t pgoffset = 0 ;
      foreach (_slist, freepage, &block[i]->freepagelist) {
         TEST(freepage     == (void*)(block[i]->pageblock.addr + pgoffset)) ;
         TEST(block[i] == *(pagecache_block_t**)&freepage[1]) ;
         pgoffset += block[i]->pagesize ;
      }
      TEST(0 == delete_pagecacheblock(&block[i])) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i <= lengthof(block); ++i) {
      delete_pagecacheblock(&block[i]) ;
   }
   return EINVAL ;
}

static int test_initfree(void)
{
   pagecache_t pgcache = pagecache_INIT_FREEABLE ;

   // TEST pagecache_INIT_FREEABLE
   TEST(0 == pgcache.blocklist.last) ;
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      TEST(0 == pgcache.freeblocklist[i].last) ;
   }
   TEST(0 == pgcache.sizeallocated) ;

   // TEST init_pagecache, free_pagecache
   memset(&pgcache, 255, sizeof(pgcache)) ;
   pgcache.freeblocklist[pagesize_4096].last = 0 ;
   TEST(0 == init_pagecache(&pgcache)) ;
   TEST(0 != pgcache.blocklist.last) ;
   TEST(0 != pgcache.freeblocklist[pagesize_4096].last) ;
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      if (i == pagesize_4096) {
         TEST(asobject_blocklist(pgcache.blocklist.last) == asobject_freeblocklist(pgcache.freeblocklist[i].last)) ;
      } else {
         TEST(0 == pgcache.freeblocklist[i].last) ;
      }
   }
   TEST(0 == pgcache.sizeallocated) ;
   TEST(0 == free_pagecache(&pgcache)) ;
   TEST(0 == pgcache.blocklist.last) ;
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      TEST(0 == pgcache.freeblocklist[i].last) ;
   }
   TEST(0 == pgcache.sizeallocated) ;
   TEST(0 == free_pagecache(&pgcache)) ;

   // TEST init_pagecache: ENOMEM
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
   memset(&pgcache, 255, sizeof(pgcache)) ;
   TEST(ENOMEM == init_pagecache(&pgcache)) ;
   TEST(1 == isfree_pagecache(&pgcache)) ;

   // TEST free_pagecache: ENOMEM
   TEST(0 == init_pagecache(&pgcache)) ;
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == free_pagecache(&pgcache)) ;
   TEST(1 == isfree_pagecache(&pgcache)) ;
   TEST(0 == init_pagecache(&pgcache)) ;
   TEST(0 == allocblock_pagecache(&pgcache, pagesize_256, 0)) ;
   init_testerrortimer(&s_pagecacheblock_errtimer, 4, ENOMEM) ;
   TEST(ENOMEM == free_pagecache(&pgcache)) ;
   TEST(1 == isfree_pagecache(&pgcache)) ;

   return 0 ;
ONABORT:
   free_pagecache(&pgcache) ;
   return EINVAL ;
}

static int test_helper(void)
{
   pagecache_t          pgcache  = pagecache_INIT_FREEABLE ;
   pagecache_block_t *  block[8] = { 0 } ;

   // TEST pagesizeinbytes_pagecache
   static_assert(4 == pagesize_NROFPAGESIZE, "tested all possible values") ;
   TEST(256   == pagesizeinbytes_pagecache(pagesize_256)) ;
   TEST(1024  == pagesizeinbytes_pagecache(pagesize_1024)) ;
   TEST(4096  == pagesizeinbytes_pagecache(pagesize_4096)) ;
   TEST(16384 == pagesizeinbytes_pagecache(pagesize_16384)) ;

   // TEST findblock_pagecache
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == new_pagecacheblock(&block[i], 16384, pagesize_16384)) ;
      insertfirst_blocklist(genericcast_dlist(&pgcache.blocklist), block[i]) ;
   }
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      for (size_t offset = 0; offset < block[i]->pageblock.size; offset += 16384) {
         pagecache_block_t * foundblock = 0 ;
         TEST(0 == findblock_pagecache(&pgcache, block[i]->pageblock.addr + offset, &foundblock)) ;
      }
   }

   // TEST findblock_pagecache: ESRCH
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      pagecache_block_t * foundblock = 0 ;
      TEST(ESRCH == findblock_pagecache(&pgcache, (uint8_t*)block[i], &foundblock)) ;
   }

   // TEST findfreeblock_pagecache
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      pgcache = (pagecache_t) pagecache_INIT_FREEABLE ;
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == delete_pagecacheblock(&block[i])) ;
         TEST(0 == new_pagecacheblock(&block[i], pagesizeinbytes_pagecache(pgsize), pgsize)) ;
         insertlast_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]), block[i]) ;
      }
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         pagecache_block_t * freeblock = 0 ;
         // returns first block not empty (empty blocks keep in list, removed by higher level function)
         TEST(1 == isinlist_freeblocklist(block[i])) ;
         TEST(0 == findfreeblock_pagecache(&pgcache, pgsize, &freeblock)) ;
         TEST(freeblock == block[i]) ;
         freeblock->freepagelist.last = 0 ;   // simulate empty block
      }
      unsigned i = 0 ;
      foreach (_freeblocklist, freeblock, genericcast_dlist(&pgcache.freeblocklist[pgsize])) {
         TEST(freeblock == block[i]) ;
         ++ i ;
      }
      TEST(i == lengthof(block)) ;
      // returns ESRCH if no free blocks available
      pagecache_block_t * freeblock = 0 ;
      TEST(ESRCH == findfreeblock_pagecache(&pgcache, pgsize, &freeblock)) ;
      pgcache.freeblocklist[pgsize].last = 0 ;
      TEST(ESRCH == findfreeblock_pagecache(&pgcache, pgsize, &freeblock)) ;
      TEST(0 == freeblock) ;
   }

   // TEST allocblock_pagecache
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == delete_pagecacheblock(&block[i])) ;
      }
      pgcache = (pagecache_t) pagecache_INIT_FREEABLE ;
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == allocblock_pagecache(&pgcache, pgsize, &block[i])) ;
         TEST(0 != block[i]) ;
         TEST(block[i] == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
         TEST(block[i] == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      }
      for (pagesize_e pgsize2 = 0; pgsize2 < pagesize_NROFPAGESIZE; ++pgsize2) {
         if (pgsize == pgsize2) continue ;
         TEST(0 == pgcache.freeblocklist[pgsize2].last) ;
      }
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         size_t offset = 0 ;
         foreach (_freepagelist, nextpage, &block[i]->freepagelist) {
            TEST(nextpage == (void*) (block[i]->pageblock.addr + offset)) ;
            offset += block[i]->pagesize ;
         }
         TEST(offset == block[i]->pageblock.size) ;
      }
   }

   // TEST allocblock_pagecache: ENOMEM
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == delete_pagecacheblock(&block[i])) ;
   }
   pgcache = (pagecache_t) pagecache_INIT_FREEABLE ;
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
      TEST(ENOMEM == allocblock_pagecache(&pgcache, pgsize, &block[0])) ;
      TEST(0 == block[0]) ;
      TEST(1 == isfree_pagecache(&pgcache)) ;
   }

   // unprepare

   return 0 ;
ONABORT:
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      delete_pagecacheblock(&block[i]) ;
   }
   return EINVAL ;
}

static int test_query(void)
{
   pagecache_t pgcache = pagecache_INIT_FREEABLE ;

   // TEST isfree_pagecache
   pgcache.sizeallocated = 1 ;
   TEST(0 == isfree_pagecache(&pgcache)) ;
   pgcache.sizeallocated = 0 ;
   TEST(1 == isfree_pagecache(&pgcache)) ;
   pgcache.blocklist.last = (void*)1 ;
   TEST(0 == isfree_pagecache(&pgcache)) ;
   pgcache.blocklist.last = (void*)0 ;
   TEST(1 == isfree_pagecache(&pgcache)) ;
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      pgcache.freeblocklist[i].last = (void*)1 ;
      TEST(0 == isfree_pagecache(&pgcache)) ;
      pgcache.freeblocklist[i].last = (void*)0 ;
      TEST(1 == isfree_pagecache(&pgcache)) ;
   }

   // TEST sizeallocated_pagecache
   TEST(0 == sizeallocated_pagecache(&pgcache)) ;
   for (size_t i = 1; i; i <<= 1) {
      pgcache.sizeallocated = i ;
      TEST(i == sizeallocated_pagecache(&pgcache)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_update(void)
{
   pagecache_t          pgcache = pagecache_INIT_FREEABLE ;
   pagecache_block_t *  block   = 0 ;

   // prepare
   TEST(0 == init_pagecache(&pgcache)) ;
   // remove preallocation
   TEST(0 == removefirst_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pagesize_4096]), &block)) ;
   TEST(0 == removefirst_blocklist(genericcast_dlist(&pgcache.blocklist), &block)) ;
   TEST(0 == delete_pagecacheblock(&block)) ;

   // TEST allocpage_pagecache
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      TEST(0 == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      TEST(0 == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      memblock_t page = memblock_INIT_FREEABLE ;
      TEST(0 == allocpage_pagecache(&pgcache, pgsize, &page)) ;
      block = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      TEST(block != 0) ;   // new block allocated and stored in list
      for (size_t offset = 0; offset < block->pageblock.size; offset += block->pagesize) {
         TEST(page.addr == block->pageblock.addr + offset) ;
         TEST(page.size == block->pagesize) ;
         TEST(pgcache.sizeallocated == offset + block->pagesize) ;
         TEST(block == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;                 // reuse block
         if (offset == block->pageblock.size - block->pagesize) {
            TEST(0 == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;  // removed from free list
         } else {
            TEST(block == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ; // reuse block
         }
         TEST(0 == allocpage_pagecache(&pgcache, pgsize, &page)) ;
      }
      TEST(block == first_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      TEST(block != last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;              // new block
      TEST(0 != last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;  // new block
      // free allocated blocks
      TEST(0 == delete_pagecacheblock(&block)) ;
      block = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      TEST(block == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      TEST(block == first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      TEST(page.addr == block->pageblock.addr) ;
      TEST(page.size == block->pagesize) ;
      TEST(0 == delete_pagecacheblock(&block)) ;
      pgcache.blocklist.last             = 0 ;
      pgcache.freeblocklist[pgsize].last = 0 ;
      pgcache.sizeallocated              = 0 ;
   }

   // TEST releasepage_pagecache
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      memblock_t page = memblock_INIT_FREEABLE ;
      TEST(0 == allocpage_pagecache(&pgcache, pgsize, &page)) ;
      pagecache_block_t * firstblock = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      for (size_t offset = 0; offset < firstblock->pageblock.size; offset += firstblock->pagesize) {
         TEST(0 == allocpage_pagecache(&pgcache, pgsize, &page)) ;
      }
      TEST(firstblock == first_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      block = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      TEST(block == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      TEST(block == first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      TEST(block != firstblock) ;
      TEST(pgcache.sizeallocated == block->pageblock.size + block->pagesize)
      TEST(0 == releasepage_pagecache(&pgcache, &page)) ;
      TEST(pgcache.sizeallocated == block->pageblock.size)
      TEST(block->usedpagecount  == 0) ;
      for (size_t offset = 0; offset < block->pageblock.size; offset += block->pagesize) {
         page.addr = firstblock->pageblock.addr + offset ;
         page.size = firstblock->pagesize ;
         TEST(pgcache.sizeallocated     == block->pageblock.size-offset)
         TEST(firstblock->usedpagecount == (block->pageblock.size-offset)/block->pagesize) ;
         TEST(block == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
         TEST(firstblock == first_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
         if (offset) {
            // firstblock added to freelist
            TEST(block      == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
            TEST(firstblock == first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
         } else {
            TEST(block == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
            TEST(block == first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
         }
         TEST(0 == releasepage_pagecache(&pgcache, &page)) ;
      }
      TEST(pgcache.sizeallocated == 0) ;
      // firstblock deleted !
      TEST(block == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      TEST(block == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      TEST(block == first_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      TEST(block == first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      // free allocated blocks
      TEST(0 == delete_pagecacheblock(&block)) ;
      pgcache.blocklist.last             = 0 ;
      pgcache.freeblocklist[pgsize].last = 0 ;
   }

   return 0 ;
ONABORT:
   free_pagecache(&pgcache) ;
   return EINVAL ;
}

int unittest_memory_pagecache()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_block())       goto ONABORT ;
   if (test_initfree())    goto ONABORT ;
   if (test_helper())      goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_update())      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
