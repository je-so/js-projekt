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

   file: C-kern/api/memory/pagecache_impl.h
    Header file <MemoryPageCache>.

   file: C-kern/memory/pagecache_impl.c
    Implementation file <MemoryPageCache impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
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

/* typedef: pagecache_impl_it
 * Defines interface pagecache_impl_it - see <pagecache_it_DECLARE>.
 * The interface supports object type <pagecache_impl_t> and is compatible with <pagecache_it>. */
pagecache_it_DECLARE(pagecache_impl_it, pagecache_impl_t)

/* typedef: struct staticpage_t
 * Export type <staticpage_t> into global namespace. */
typedef struct staticpage_t               staticpage_t ;


/* struct: staticpage_t
 * Header of last allocated static memory page. */
struct staticpage_t {
   dlist_node_t *       next ;
   dlist_node_t *       prev ;
   memblock_t           memblock ;
} ;

// group: helper

/* define: INTERFACE_staticpagelist
 * Macro <dlist_IMPLEMENT> generates dlist interface for <staticpage_t>. */
dlist_IMPLEMENT(_staticpagelist, staticpage_t, )

/* function: alignedsize_staticpage
 * Returns sizeof(staticpage_t) aligned to <KONFIG_MEMALIGN>. */
static inline size_t alignedsize_staticpage(void)
{
   const size_t alignedsize = (sizeof(staticpage_t) + KONFIG_MEMALIGN-1u) & (~(KONFIG_MEMALIGN-1u)) ;
   return alignedsize ;
}

// group: lifetime

/* function: init_staticpage
 * Initializes a <staticpage_t> as head of page and returns a pointer to it.
 *
 * (Unchecked) Precondition:
 * o page.size > sizeof(staticpage_t) */
static staticpage_t * init_staticpage(memblock_t * page)
{
   staticpage_t * staticpage  = (staticpage_t*) page->addr ;
   const size_t   alignedsize = alignedsize_staticpage() ;

   staticpage->memblock = (memblock_t) memblock_INIT(page->size - alignedsize, page->addr + alignedsize) ;

   return staticpage ;
}

// group: query

uint8_t * startaddr_staticpage(staticpage_t * staticpage)
{
   const size_t alignedsize = alignedsize_staticpage() ;
   return alignedsize + (uint8_t*)staticpage ;
}

bool isempty_staticpage(staticpage_t * staticpage)
{
   const size_t alignedsize = alignedsize_staticpage() ;
   return staticpage->memblock.addr == alignedsize + (uint8_t*)staticpage ;
}


/* struct: freepage_t
 * Header of free page located in <pagecache_block_t.pageblock>. */
struct freepage_t {
   dlist_node_t *       next ;
   dlist_node_t *       prev ;
   pagecache_block_t *  marker ;
} ;

// group: helper

/* define: INTERFACE_freepagelist
 * Macro <dlist_IMPLEMENT> generates dlist interface for <freepage_t>. */
dlist_IMPLEMENT(_freepagelist, freepage_t, )


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
   dlist_t        freepagelist ;
   /* variable: pagesize
    * Size of a single page stored in freepagelist. */
   size_t         pagesize ;
   /* variable: usedpagecount
    * Counts the number of allocated pages in use. */
   uint16_t       usedpagecount ;
   /* variable: freelistidx
    * Index into <pagecache_impl_t.freeblocklist>. */
   uint8_t        freelistidx ;
} ;

// group: variable
#ifdef KONFIG_UNITTEST
static test_errortimer_t   s_pagecacheblock_errtimer = test_errortimer_INIT_FREEABLE ;
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
   vmpage_t       pageblock = vmpage_INIT_FREEABLE ;
   size_t         blocksize = blocksize_pagecacheblock(pagesize_vm()) ;

   VALIDATE_INPARAM_TEST(pagesize <= blocksize, ONABORT,) ;

   // TODO: change second argument of init_vmpage from nrofpages into size_in_bytes
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

   // init new object
   pagecache_block_t * new_block = (pagecache_block_t*) malloc(sizeof(pagecache_block_t)) ;
   if (!new_block) {
      err = ENOMEM ;
      goto ONABORT ;
   }
   new_block->pageblock    = pageblock ;
   // Member new_block->next_block set in calling function
   // Member new_block->next_freeblock set in calling function
   new_block->freepagelist = (dlist_t) dlist_INIT ;
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
   pagecache_block_t * del_block = *block ;

   if (del_block) {
      *block = 0 ;

      err = free_vmpage(&del_block->pageblock) ;
#ifdef KONFIG_UNITTEST
      int err2 = process_testerrortimer(&s_pagecacheblock_errtimer) ;
      if (err2) err = err2 ;
#endif

      free(del_block) ;

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

// group: alloc

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

   insertfirst_freepagelist(&block->freepagelist, freepage) ;
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



// section: pagecache_impl_t

// group: variables

/* variable: s_pagecacheimpl_interface
 * Stores single instance of interface <pagecache_it>. */
static pagecache_impl_it   s_pagecacheimpl_interface = pagecache_it_INIT(
                              &allocpage_pagecacheimpl,
                              &releasepage_pagecacheimpl,
                              &sizeallocated_pagecacheimpl,
                              &allocstatic_pagecacheimpl,
                              &freestatic_pagecacheimpl,
                              &sizestatic_pagecacheimpl,
                              &releasecached_pagecacheimpl
                           ) ;

// group: init

int initthread_pagecacheimpl(/*out*/pagecache_t * pgcache)
{
   int err ;
   pagecache_impl_t  temppgcache = pagecache_impl_INIT_FREEABLE ;
   memblock_t        pgcacheobj ;

   // TODO: Let init_threadcontext call initthread_pagecacheimpl before intthread_mmtransient
   //       Change object alloc in mm_transient_t - let it call allocstatic.

   VALIDATE_INPARAM_TEST(0 == pgcache->object, ONABORT, ) ;

   err = init_pagecacheimpl(&temppgcache) ;
   if (err) goto ONABORT ;

   err = allocstatic_pagecacheimpl(&temppgcache, sizeof(temppgcache), &pgcacheobj) ;
   if (err) goto ONABORT ;

   memcpy(pgcacheobj.addr, &temppgcache, sizeof(temppgcache)) ;

   pgcache->object = (struct pagecache_t*) pgcacheobj.addr ;
   pgcache->iimpl  = genericcast_pagecacheit(&s_pagecacheimpl_interface, pagecache_impl_t) ;

   return 0 ;
ONABORT:
   free_pagecacheimpl(&temppgcache) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freethread_pagecacheimpl(pagecache_t * pgcache)
{
   int err ;
   pagecache_impl_t * delobj = (pagecache_impl_t*) pgcache->object ;

   if (delobj) {
      assert(genericcast_pagecacheit(&s_pagecacheimpl_interface, pagecache_impl_t) == pgcache->iimpl) ;

      pgcache->object = 0 ;
      pgcache->iimpl  = 0 ;

      pagecache_impl_t temppgcache = *delobj ;

      memblock_t memblock = memblock_INIT(sizeof(*delobj), (uint8_t*)delobj) ;
      err = freestatic_pagecacheimpl(&temppgcache, &memblock) ;

      int err2 = free_pagecacheimpl(&temppgcache) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: helper

/* function: findblock_pagecacheimpl
 * Find block who owns pageaddr in pgcache.
 * The block is searched with help of <pagecache_impl_t.blocklist>.
 * The result is returned in block. If no block is found ESRCH is returned. */
static inline int findblock_pagecacheimpl(pagecache_impl_t * pgcache, const uint8_t * pageaddr, /*out*/pagecache_block_t ** block)
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

/* function: findfreeblock_pagecacheimpl
 * Find empty <pageblock_block_t> on freelist and return it in freeblock.
 * The freelist <pagecache_impl_t.freeblocklist> at index pgsize is searched.
 * All encountered <pageblock_block_t> which are empty are removed from the list.
 * If there is no empty block for the corresponding pgsize the error ESRCH is returned. */
static inline int findfreeblock_pagecacheimpl(pagecache_impl_t * pgcache, pagesize_e pgsize, pagecache_block_t ** freeblock)
{
   int err ;

   // Use some kind of priority queue ?

   err = ESRCH ;

   foreach (_freeblocklist, block, genericcast_dlist(&pgcache->freeblocklist[pgsize])) {
      if (!isempty_dlist(&block->freepagelist)) {
         *freeblock = block ;
         err = 0 ;
         break ;
      }
   }

   return err ;
}

/* function: allocblock_pagecacheimpl
 * Allocate new <pageblock_block_t> and add it to freelist.
 * The new block is returned in block if this value is not NULL.
 * The allocated block is inserted in freelist <pagecache_impl_t.freeblocklist> at index pgsize.
 * The allocated block is also inserted into list <pagecache_impl_t.blocklist>.
 * In case of error ENOMEM is returned. */
static inline int allocblock_pagecacheimpl(pagecache_impl_t * pgcache, pagesize_e pgsize, /*out*/pagecache_block_t ** block)
{
   int err ;
   pagecache_block_t *  freeblock ;

   err = new_pagecacheblock(&freeblock, pagesizeinbytes_pagecacheit(pgsize), pgsize) ;
   if (err) return err ;

   insertlast_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[pgsize]), freeblock) ;
   insertlast_blocklist(genericcast_dlist(&pgcache->blocklist), freeblock) ;

   if (block) *block = freeblock ;

   return 0 ;
}

/* function: freeblock_pagecacheimpl
 * Frees <pageblock_block_t> and removes it from freelist and blocklist. */
static inline int freeblock_pagecacheimpl(pagecache_impl_t * pgcache, pagecache_block_t * block)
{
   int err ;

   err = remove_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[block->freelistidx]), block) ;
   if (err) return err ;
   err = remove_blocklist(genericcast_dlist(&pgcache->blocklist), block) ;
   if (err) return err ;
   err = delete_pagecacheblock(&block) ;
   if (err) return err ;

   return 0 ;
}

// group: lifetime

int init_pagecacheimpl(/*out*/pagecache_impl_t * pgcache)
{
   int err ;

   *pgcache = (pagecache_impl_t) pagecache_impl_INIT_FREEABLE ;

   err = allocblock_pagecacheimpl(pgcache, pagesize_4096, 0) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_pagecacheimpl(pagecache_impl_t * pgcache)
{
   int err ;
   int err2 ;

   err = 0 ;

   foreach (_blocklist, nextblock, genericcast_dlist(&pgcache->blocklist)) {
      err2 = delete_pagecacheblock(&nextblock) ;
      if (err2) err = err2 ;
   }

   *pgcache = (pagecache_impl_t) pagecache_impl_INIT_FREEABLE ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

bool isfree_pagecacheimpl(const pagecache_impl_t * pgcache)
{
   if (0 != pgcache->blocklist.last) return false ;

   for (unsigned i = 0; i < lengthof(pgcache->freeblocklist); ++i) {
      if (0 != pgcache->freeblocklist[i].last) return false ;
   }

   return   0 == pgcache->staticpagelist.last
            && 0 == pgcache->sizeallocated && 0 == pgcache->sizestatic ;
}

size_t sizeallocated_pagecacheimpl(const pagecache_impl_t * pgcache)
{
   return pgcache->sizeallocated ;
}

size_t sizestatic_pagecacheimpl(const pagecache_impl_t * pgcache)
{
   return pgcache->sizestatic ;
}

// group: alloc

int allocpage_pagecacheimpl(pagecache_impl_t * pgcache, pagesize_e pgsize, /*out*/struct memblock_t * page)
{
   int err ;

   VALIDATE_INPARAM_TEST(pgsize < lengthof(pgcache->freeblocklist), ONABORT, ) ;

   pagecache_block_t * freeblock ;

   err = findfreeblock_pagecacheimpl(pgcache, pgsize, &freeblock) ;
   if (err == ESRCH) {
      err = allocblock_pagecacheimpl(pgcache, pgsize, &freeblock) ;
   }
   if (err) goto ONABORT ;

   freepage_t * freepage ;
   err = allocpage_pagecacheblock(freeblock, &freepage) ;
   if (err) goto ONABORT ;
   if (isempty_dlist(&freeblock->freepagelist)) {
      // freeblock is full => remove it from list of free blocks
      err = remove_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[pgsize]), freeblock) ;
      if (err) goto ONABORT ;
   }

   size_t pgsizeinbytes = pagesizeinbytes_pagecacheit(pgsize) ;
   pgcache->sizeallocated += pgsizeinbytes ;
   *page = (memblock_t) memblock_INIT(pgsizeinbytes, (uint8_t*)freepage) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int releasepage_pagecacheimpl(pagecache_impl_t * pgcache, struct memblock_t * page)
{
   int err ;
   pagecache_block_t * block ;

   if (!isfree_memblock(page)) {
      if (  findblock_pagecacheimpl(pgcache, page->addr, &block)
            || block->pagesize != page->size
            || 0 != (((uintptr_t)page->addr) & ((uintptr_t)block->pagesize-1))) {
         err = EINVAL ;
         goto ONABORT ;
      }

      err = releasepage_pagecacheblock(block, (freepage_t*)page->addr) ;
      if (err) goto ONABORT ;

      pgcache->sizeallocated -= block->pagesize ;
      if (!isinlist_freeblocklist(block)) {
         insertfirst_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[block->freelistidx]), block) ;
      }

      if (! block->usedpagecount) {
         // delete block if not used and at least another block is stored in free list
         pagecache_block_t * firstblock = last_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[block->freelistidx])) ;
         pagecache_block_t * lastblock  = first_freeblocklist(genericcast_dlist(&pgcache->freeblocklist[block->freelistidx])) ;
         if (firstblock != lastblock) {
            err = freeblock_pagecacheimpl(pgcache, block) ;
            if (err) goto ONABORT ;
         }
      }

      *page = (memblock_t) memblock_INIT_FREEABLE ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int allocstatic_pagecacheimpl(pagecache_impl_t * pgcache, size_t bytesize, /*out*/struct memblock_t * memblock)
{
   int err ;
   size_t         alignedsize = (bytesize + KONFIG_MEMALIGN-1u) & (~(KONFIG_MEMALIGN-1u)) ;
   staticpage_t * staticpage  = last_staticpagelist(genericcast_dlist(&pgcache->staticpagelist)) ;

   VALIDATE_INPARAM_TEST(0 < alignedsize && alignedsize <= 128, ONABORT, ) ;

   if (  !staticpage
         || staticpage->memblock.size < alignedsize) {
      // waste staticpage->memblock.size bytes !
      memblock_t page ;
      err = allocpage_pagecacheimpl(pgcache, pagesize_4096, &page) ;
      if (err) goto ONABORT ;
      staticpage = init_staticpage(&page) ;
      insertlast_staticpagelist(genericcast_dlist(&pgcache->staticpagelist), staticpage) ;
   }

   memblock->addr = staticpage->memblock.addr ;
   memblock->size = alignedsize ;

   staticpage->memblock.addr += alignedsize ;
   staticpage->memblock.size -= alignedsize ;
   pgcache->sizestatic       += alignedsize ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int freestatic_pagecacheimpl(pagecache_impl_t * pgcache, struct memblock_t * memblock)
{
   int err ;
   staticpage_t * staticpage = last_staticpagelist(genericcast_dlist(&pgcache->staticpagelist)) ;

   if (!isfree_memblock(memblock)) {
      size_t   alignedsize = (memblock->size + KONFIG_MEMALIGN-1u) & (~(KONFIG_MEMALIGN-1u)) ;

      VALIDATE_INPARAM_TEST(  staticpage
                              && memblock->addr < staticpage->memblock.addr
                              && memblock->addr + alignedsize == staticpage->memblock.addr
                              && memblock->addr >= startaddr_staticpage(staticpage), ONABORT,) ;

      staticpage->memblock.addr -= alignedsize ;
      staticpage->memblock.size += alignedsize ;
      pgcache->sizestatic       -= alignedsize ;

      if (isempty_staticpage(staticpage)) {
         err = remove_staticpagelist(genericcast_dlist(&pgcache->staticpagelist), staticpage) ;
         if (err) goto ONABORT ;
         memblock_t page = memblock_INIT(4096, (uint8_t*)staticpage) ;
         err = releasepage_pagecacheimpl(pgcache, &page) ;
         if (err) goto ONABORT ;
      }

      *memblock = (memblock_t) memblock_INIT_FREEABLE ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: cache

int releasecached_pagecacheimpl(pagecache_impl_t * pgcache)
{
   int err ;

   for (unsigned pgsize = 0; pgsize < lengthof(pgcache->freeblocklist); ++pgsize) {
      foreach (_freeblocklist, block, genericcast_dlist(&pgcache->freeblocklist[pgsize])) {
         if (! block->usedpagecount) {
            err = freeblock_pagecacheimpl(pgcache, block) ;
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
   pagecache_block_t * block[13] = { 0 } ;

   // TEST blocksize_pagecacheblock
   for (size_t i = 1; i; i *= 2) {
      if (i <= pagecache_block_BLOCKSIZE) {
         TEST(blocksize_pagecacheblock(i) == pagecache_block_BLOCKSIZE) ;
      } else {
         TEST(blocksize_pagecacheblock(i) == i) ;
      }
   }

   // TEST new_pagecacheblock
   static_assert( (256 << (lengthof(block)-1)) == 1024*1024
                  && pagesize_1MB + 1 == pagesize_NROFPAGESIZE, "max size") ;
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
      foreach (_dlist, freepage, &block[i]->freepagelist) {
         TEST(freepage     == (void*)(block[i]->pageblock.addr + pgoffset)) ;
         TEST(block[i] == *(pagecache_block_t**)&freepage[1]) ;
         pgoffset += block[i]->pagesize ;
      }
   }

   // TEST delete_pagecacheblock
   for (unsigned i = 0; i < lengthof(block); ++i) {
      // check list of free pages
      size_t pgoffset = 0 ;
      foreach (_dlist, freepage, &block[i]->freepagelist) {
         TEST(freepage     == (void*)(block[i]->pageblock.addr + pgoffset)) ;
         TEST(block[i] == *(pagecache_block_t**)&freepage[1]) ;
         pgoffset += block[i]->pagesize ;
      }
      TEST(0 == delete_pagecacheblock(&block[i])) ;
      TEST(0 == block[i]) ;
      TEST(0 == delete_pagecacheblock(&block[i])) ;
      TEST(0 == block[i]) ;
   }

   // TEST new_pagecacheblock: EINVAL
   TEST(EINVAL == new_pagecacheblock(&block[0], 2*pagecache_block_BLOCKSIZE, 0)) ;
   TEST(block[0] == 0) ;

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
      TEST(block[i]->usedpagecount == (block[i]->pageblock.size / block[i]->pagesize)) ;
   }

   // TEST releasepage_pagecacheblock
   for (unsigned i = 0; i < lengthof(block); ++i) {
      TEST(0 == block[i]->freepagelist.last) ;
      for (size_t offset = block[i]->pageblock.size; offset > 0; ) {
         offset -= block[i]->pagesize ;
         freepage_t * freepage = (void*) (block[i]->pageblock.addr + offset) ;
         TEST(0 == releasepage_pagecacheblock(block[i], freepage)) ;
         TEST(0 == block[i]->usedpagecount - (offset / block[i]->pagesize)) ;
         TEST(freepage         == first_freepagelist(genericcast_dlist(&block[i]->freepagelist))) ;
         TEST(freepage->marker == block[i]) ;
         // double free does nothing
         TEST(EALREADY == releasepage_pagecacheblock(block[i], freepage)) ;
         TEST(freepage         == first_freepagelist(genericcast_dlist(&block[i]->freepagelist))) ;
         TEST(freepage->marker == block[i]) ;
      }
   }
   for (unsigned i = 0; i < lengthof(block); ++i) {
      // check list of free pages
      size_t pgoffset = 0 ;
      foreach (_dlist, freepage, &block[i]->freepagelist) {
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
   pagecache_impl_t pgcache = pagecache_impl_INIT_FREEABLE ;

   // TEST pagecache_impl_INIT_FREEABLE
   TEST(0 == pgcache.blocklist.last) ;
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      TEST(0 == pgcache.freeblocklist[i].last) ;
   }
   TEST(0 == pgcache.staticpagelist.last) ;
   TEST(0 == pgcache.sizeallocated) ;
   TEST(0 == pgcache.sizestatic) ;

   // TEST init_pagecacheimpl, free_pagecacheimpl
   memset(&pgcache, 255, sizeof(pgcache)) ;
   pgcache.freeblocklist[pagesize_4096].last = 0 ;
   TEST(0 == init_pagecacheimpl(&pgcache)) ;
   TEST(0 != pgcache.blocklist.last) ;
   TEST(0 != pgcache.freeblocklist[pagesize_4096].last) ;
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      if (i == pagesize_4096) {
         TEST(asobject_blocklist(pgcache.blocklist.last) == asobject_freeblocklist(pgcache.freeblocklist[i].last)) ;
      } else {
         TEST(0 == pgcache.freeblocklist[i].last) ;
      }
   }
   TEST(0 == pgcache.staticpagelist.last) ;
   TEST(0 == pgcache.sizeallocated) ;
   TEST(0 == pgcache.sizestatic) ;
   for (unsigned i = 0; i < pagesize_NROFPAGESIZE; ++i) {
      TEST(0 == allocblock_pagecacheimpl(&pgcache, i, 0)) ;
   }
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      TEST(0 != pgcache.freeblocklist[i].last) ;
   }
   pgcache.staticpagelist.last = (void*)1 ;
   pgcache.sizeallocated    = 1 ;
   pgcache.sizestatic       = 1 ;
   TEST(0 == free_pagecacheimpl(&pgcache)) ;
   TEST(0 == pgcache.blocklist.last) ;
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      TEST(0 == pgcache.freeblocklist[i].last) ;
   }
   TEST(0 == pgcache.staticpagelist.last) ;
   TEST(0 == pgcache.sizeallocated) ;
   TEST(0 == pgcache.sizestatic) ;
   TEST(0 == free_pagecacheimpl(&pgcache)) ;

   // TEST init_pagecacheimpl: ENOMEM
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
   memset(&pgcache, 255, sizeof(pgcache)) ;
   TEST(ENOMEM == init_pagecacheimpl(&pgcache)) ;
   TEST(1 == isfree_pagecacheimpl(&pgcache)) ;

   // TEST free_pagecacheimpl: ENOMEM
   TEST(0 == init_pagecacheimpl(&pgcache)) ;
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == free_pagecacheimpl(&pgcache)) ;
   TEST(1 == isfree_pagecacheimpl(&pgcache)) ;
   TEST(0 == init_pagecacheimpl(&pgcache)) ;
   TEST(0 == allocblock_pagecacheimpl(&pgcache, pagesize_256, 0)) ;
   init_testerrortimer(&s_pagecacheblock_errtimer, 4, ENOMEM) ;
   TEST(ENOMEM == free_pagecacheimpl(&pgcache)) ;
   TEST(1 == isfree_pagecacheimpl(&pgcache)) ;

   return 0 ;
ONABORT:
   free_pagecacheimpl(&pgcache) ;
   return EINVAL ;
}

static int test_helper(void)
{
   pagecache_impl_t     pgcache  = pagecache_impl_INIT_FREEABLE ;
   pagecache_block_t *  block[8] = { 0 } ;

   // TEST findblock_pagecacheimpl
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == new_pagecacheblock(&block[i], 16384, pagesize_16384)) ;
      insertfirst_blocklist(genericcast_dlist(&pgcache.blocklist), block[i]) ;
   }
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      for (size_t offset = 0; offset < block[i]->pageblock.size; offset += 16384) {
         pagecache_block_t * foundblock = 0 ;
         TEST(0 == findblock_pagecacheimpl(&pgcache, block[i]->pageblock.addr + offset, &foundblock)) ;
      }
   }

   // TEST findblock_pagecacheimpl: ESRCH
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      pagecache_block_t * foundblock = 0 ;
      TEST(ESRCH == findblock_pagecacheimpl(&pgcache, (uint8_t*)block[i], &foundblock)) ;
   }

   // TEST findfreeblock_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      pgcache = (pagecache_impl_t) pagecache_impl_INIT_FREEABLE ;
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == delete_pagecacheblock(&block[i])) ;
         TEST(0 == new_pagecacheblock(&block[i], pagesizeinbytes_pagecacheit(pgsize), pgsize)) ;
         insertlast_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]), block[i]) ;
      }
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         pagecache_block_t * freeblock = 0 ;
         // returns first block not empty (empty blocks keep in list, removed by higher level function)
         TEST(1 == isinlist_freeblocklist(block[i])) ;
         TEST(0 == findfreeblock_pagecacheimpl(&pgcache, pgsize, &freeblock)) ;
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
      TEST(ESRCH == findfreeblock_pagecacheimpl(&pgcache, pgsize, &freeblock)) ;
      pgcache.freeblocklist[pgsize].last = 0 ;
      TEST(ESRCH == findfreeblock_pagecacheimpl(&pgcache, pgsize, &freeblock)) ;
      TEST(0 == freeblock) ;
   }

   // TEST allocblock_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == delete_pagecacheblock(&block[i])) ;
      }
      pgcache = (pagecache_impl_t) pagecache_impl_INIT_FREEABLE ;
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == allocblock_pagecacheimpl(&pgcache, pgsize, &block[i])) ;
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

   // TEST allocblock_pagecacheimpl: ENOMEM
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == delete_pagecacheblock(&block[i])) ;
   }
   pgcache = (pagecache_impl_t) pagecache_impl_INIT_FREEABLE ;
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
      TEST(ENOMEM == allocblock_pagecacheimpl(&pgcache, pgsize, &block[0])) ;
      TEST(0 == block[0]) ;
      TEST(1 == isfree_pagecacheimpl(&pgcache)) ;
   }

   // TEST freeblock_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      pgcache = (pagecache_impl_t) pagecache_impl_INIT_FREEABLE ;
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == allocblock_pagecacheimpl(&pgcache, pgsize, &block[i])) ;
         TEST(block[i] == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
         TEST(block[i] == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      }
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(block[i] == first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
         TEST(block[i] == first_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
         TEST(0 == freeblock_pagecacheimpl(&pgcache, block[i])) ;
      }
      TEST(0 == pgcache.freeblocklist[pgsize].last) ;
      TEST(0 == pgcache.blocklist.last) ;
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
   pagecache_impl_t pgcache = pagecache_impl_INIT_FREEABLE ;

   // TEST isfree_pagecacheimpl
   pgcache.blocklist.last = (void*)1 ;
   TEST(0 == isfree_pagecacheimpl(&pgcache)) ;
   pgcache.blocklist.last = (void*)0 ;
   TEST(1 == isfree_pagecacheimpl(&pgcache)) ;
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      pgcache.freeblocklist[i].last = (void*)1 ;
      TEST(0 == isfree_pagecacheimpl(&pgcache)) ;
      pgcache.freeblocklist[i].last = (void*)0 ;
      TEST(1 == isfree_pagecacheimpl(&pgcache)) ;
   }
   pgcache.staticpagelist.last = (void*)1 ;
   TEST(0 == isfree_pagecacheimpl(&pgcache)) ;
   pgcache.staticpagelist.last = (void*)0 ;
   TEST(1 == isfree_pagecacheimpl(&pgcache)) ;
   pgcache.sizeallocated = 1 ;
   TEST(0 == isfree_pagecacheimpl(&pgcache)) ;
   pgcache.sizeallocated = 0 ;
   TEST(1 == isfree_pagecacheimpl(&pgcache)) ;
   pgcache.sizestatic = 1 ;
   TEST(0 == isfree_pagecacheimpl(&pgcache)) ;
   pgcache.sizestatic = 0 ;
   TEST(1 == isfree_pagecacheimpl(&pgcache)) ;

   // TEST sizeallocated_pagecacheimpl
   TEST(0 == sizeallocated_pagecacheimpl(&pgcache)) ;
   for (size_t i = 1; i; i <<= 1) {
      pgcache.sizeallocated = i ;
      TEST(i == sizeallocated_pagecacheimpl(&pgcache)) ;
   }

   // TEST sizestatic_pagecacheimpl
   TEST(0 == sizestatic_pagecacheimpl(&pgcache)) ;
   for (size_t i = 1; i; i <<= 1) {
      pgcache.sizestatic = i ;
      TEST(i == sizestatic_pagecacheimpl(&pgcache)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_alloc(void)
{
   pagecache_impl_t     pgcache = pagecache_impl_INIT_FREEABLE ;
   pagecache_block_t *  block   = 0 ;
   memblock_t           page    = memblock_INIT_FREEABLE ;

   // prepare
   TEST(0 == init_pagecacheimpl(&pgcache)) ;
   // remove preallocation
   TEST(0 == removefirst_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pagesize_4096]), &block)) ;
   TEST(0 == removefirst_blocklist(genericcast_dlist(&pgcache.blocklist), &block)) ;
   TEST(0 == delete_pagecacheblock(&block)) ;

   // TEST allocpage_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      TEST(0 == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      TEST(0 == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      page = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page)) ;
      block = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      TEST(block != 0) ;   // new block allocated and stored in list
      TEST(block->pagesize == pagesizeinbytes_pagecacheit(pgsize)) ;
      for (size_t offset = 0; offset < block->pageblock.size; offset += block->pagesize) {
         TEST(page.addr == block->pageblock.addr + offset) ;
         TEST(page.size == block->pagesize) ;
         TEST(0         == ((uintptr_t)page.addr % block->pagesize)/*aligned to pagesize*/) ;
         TEST(pgcache.sizeallocated == offset + block->pagesize) ;
         TEST(pgcache.sizestatic    == 0) ;
         TEST(block == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;                 // reuse block
         if (offset == block->pageblock.size - block->pagesize) {
            TEST(0 == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;  // removed from free list
         } else {
            TEST(block == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ; // reuse block
         }
         TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page)) ;
      }
      TEST(block == first_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      TEST(block != last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;              // new block
      block = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      if (block->pagesize < block->pageblock.size) {
         TEST(0 != last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;  // new block
         TEST(block == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
         TEST(block == first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      }
      TEST(page.addr == block->pageblock.addr) ;
      TEST(page.size == block->pagesize) ;
      // free blocks
      block = first_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      TEST(0 == delete_pagecacheblock(&block)) ;
      block = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      TEST(0 == delete_pagecacheblock(&block)) ;
      pgcache.blocklist.last             = 0 ;
      pgcache.freeblocklist[pgsize].last = 0 ;
      pgcache.sizeallocated              = 0 ;
   }

   // TEST allocpage_pagecacheimpl
   TEST(EINVAL == allocpage_pagecacheimpl(&pgcache, (pagesize_e)-1, &page)) ;
   TEST(EINVAL == allocpage_pagecacheimpl(&pgcache, pagesize_NROFPAGESIZE, &page)) ;
   TEST(0 == pgcache.blocklist.last) ;
   TEST(0 == pgcache.sizeallocated) ;

   // TEST releasepage_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      page = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page)) ;
      pagecache_block_t * firstblock = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      for (size_t offset = 0; offset < firstblock->pageblock.size; offset += firstblock->pagesize) {
         TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page)) ;
      }
      TEST(firstblock == first_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      block = last_blocklist(genericcast_dlist(&pgcache.blocklist)) ;
      if (block->pagesize < block->pageblock.size) {
         TEST(block == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
         TEST(block == first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize]))) ;
      }
      TEST(block != firstblock) ;
      TEST(pgcache.sizeallocated == block->pageblock.size + block->pagesize)
      TEST(0 == releasepage_pagecacheimpl(&pgcache, &page)) ;
      TEST(0 == page.addr) ;
      TEST(0 == page.size) ;
      TEST(pgcache.sizeallocated == block->pageblock.size)
      TEST(pgcache.sizestatic    == 0) ;
      TEST(block->usedpagecount  == 0) ;
      for (size_t offset = 0; offset < block->pageblock.size; offset += block->pagesize) {
         page.addr = firstblock->pageblock.addr + offset ;
         page.size = firstblock->pagesize ;
         TEST(pgcache.sizeallocated     == block->pageblock.size-offset)
         TEST(pgcache.sizestatic        == 0) ;
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
         TEST(0 == releasepage_pagecacheimpl(&pgcache, &page)) ;
         TEST(0 == page.addr) ;
         TEST(0 == page.size) ;
         // isfree_memblock(&page) ==> second call does nothing !
         TEST(0 == releasepage_pagecacheimpl(&pgcache, &page)) ;
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

   // TEST releasepage_pagecacheimpl: EALREADY
   TEST(0 == init_pagecacheimpl(&pgcache)) ;
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_4096, &page)) ;
   memblock_t  page2 = page ;
   TEST(0 == releasepage_pagecacheimpl(&pgcache, &page)) ;
   TEST(EALREADY == releasepage_pagecacheimpl(&pgcache, &page2)) ;
   TEST(0 == free_pagecacheimpl(&pgcache)) ;

   // TEST allocstatic_pagecacheimpl: 1 byte -> 128 bytes
   memblock_t  mem ;
   size_t      alignedheadersize = !(sizeof(staticpage_t) % KONFIG_MEMALIGN) ? sizeof(staticpage_t) : sizeof(staticpage_t) + KONFIG_MEMALIGN - (sizeof(staticpage_t) % KONFIG_MEMALIGN) ;
   TEST(0 == init_pagecacheimpl(&pgcache)) ;
   block = first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pagesize_4096])) ;
   TEST(0 != block) ;
   size_t sizestatic = 0 ;
   for (unsigned i = 1; i <= 128; i *= 2) {
      unsigned alignedsize = (i % KONFIG_MEMALIGN) ? i + KONFIG_MEMALIGN - (i % KONFIG_MEMALIGN) : i ;
      TEST(0 == allocstatic_pagecacheimpl(&pgcache, i, &mem)) ;
      sizestatic += alignedsize ;
      TEST(pgcache.sizeallocated       == 4096) ;
      TEST(pgcache.sizestatic          == sizestatic) ;
      TEST(pgcache.staticpagelist.last == (void*)block->pageblock.addr) ;
      TEST(mem.addr == block->pageblock.addr + alignedheadersize + sizestatic - alignedsize) ;
      TEST(mem.size == alignedsize) ;
   }

   // TEST allocstatic_pagecacheimpl: EINVAL
   TEST(EINVAL == allocstatic_pagecacheimpl(&pgcache, 129, &mem)) ;
   TEST(EINVAL == allocstatic_pagecacheimpl(&pgcache, 0,   &mem)) ;
   // nothing changed
   TEST(pgcache.sizeallocated       == 4096) ;
   TEST(pgcache.sizestatic          == sizestatic) ;
   TEST(pgcache.staticpagelist.last == (void*)block->pageblock.addr) ;
   TEST(mem.addr == block->pageblock.addr + alignedheadersize + sizestatic - 128) ;
   TEST(mem.size == 128) ;

   // TEST freestatic_pagecacheimpl: 1 byte -> 128 bytes
   for (unsigned i = 128; i >= 1; i /= 2) {
      unsigned alignedsize = (i % KONFIG_MEMALIGN) ? i + KONFIG_MEMALIGN - (i % KONFIG_MEMALIGN) : i ;
      sizestatic -= alignedsize ;
      mem = (memblock_t) memblock_INIT(i, block->pageblock.addr + alignedheadersize + sizestatic) ;
      TEST(0 == freestatic_pagecacheimpl(&pgcache, &mem)) ;
      TEST(0 == mem.addr) ;
      TEST(0 == mem.size) ;
      TEST(pgcache.sizeallocated       == (i > 1 ? 4096 : 0)) ;
      TEST(pgcache.sizestatic          == sizestatic) ;
      TEST(pgcache.staticpagelist.last == (i > 1 ? (void*)block->pageblock.addr : 0)) ;
      // calling it twice does nothing
      TEST(0 == freestatic_pagecacheimpl(&pgcache, &mem)) ;
      TEST(0 == mem.addr) ;
      TEST(0 == mem.size) ;
   }

   // TEST allocstatic_pagecacheimpl: several pages
   size_t size ;
   size_t sizest ;
   for (size = 0, sizest = 0; pgcache.freeblocklist[pagesize_4096].last; size += 4096) {
      block = first_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pagesize_4096])) ;
      for (size_t offset = alignedheadersize; offset + 128 <= 4096; offset += 128) {
         TEST(0 == allocstatic_pagecacheimpl(&pgcache, 128, &mem)) ;
         sizest += 128 ;
         TEST(pgcache.sizeallocated    == size + 4096) ;
         TEST(pgcache.sizestatic       == sizest) ;
         TEST(mem.addr == block->pageblock.addr + size + offset) ;
         TEST(mem.size == 128) ;
      }
   }

   // TEST freestatic_pagecacheimpl: several pages
   for (; size; size -= 4096) {
      for (size_t offset = alignedheadersize+(4096-alignedheadersize)/128*128; offset > 128; sizest -= 128, offset -= 128) {
         mem.addr = block->pageblock.addr + size - 4096 + offset - 128;
         mem.size = 128 ;
         TEST(pgcache.sizeallocated == size) ;
         TEST(pgcache.sizestatic    == sizest) ;
         TEST(0 == freestatic_pagecacheimpl(&pgcache, &mem)) ;
         TEST(0 == mem.addr) ;
         TEST(0 == mem.size) ;
      }
   }
   TEST(0 == pgcache.sizeallocated) ;
   TEST(0 == pgcache.sizestatic) ;

   // TEST allocstatic_pagecacheimpl: ENOMEM
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM) ;
   dlist_node_t * oldlast = pgcache.freeblocklist[pagesize_4096].last ;
   pgcache.freeblocklist[pagesize_4096].last = 0 ;
   TEST(ENOMEM == allocstatic_pagecacheimpl(&pgcache, 1, &mem)) ;
   TEST(0 == pgcache.sizeallocated) ;
   TEST(0 == pgcache.sizestatic) ;
   TEST(0 == pgcache.staticpagelist.last) ;
   pgcache.freeblocklist[pagesize_4096].last = oldlast ;

   // TEST freestatic_pagecacheimpl: EINVAL
   // empty list
   TEST(isempty_staticpagelist(genericcast_dlist(&pgcache.staticpagelist))) ;
   memblock_t badmem = memblock_INIT(KONFIG_MEMALIGN, 0) ;
   TEST(EINVAL == freestatic_pagecacheimpl(&pgcache, &badmem)) ;
   // addr too low
   TEST(0 == allocstatic_pagecacheimpl(&pgcache, 1, &mem)) ;
   badmem = (memblock_t) memblock_INIT(mem.size + KONFIG_MEMALIGN, mem.addr - KONFIG_MEMALIGN) ;
   TEST(EINVAL == freestatic_pagecacheimpl(&pgcache, &badmem)) ;
   // addr too high
   badmem = (memblock_t) memblock_INIT(mem.size - KONFIG_MEMALIGN, mem.addr + KONFIG_MEMALIGN) ;
   TEST(EINVAL == freestatic_pagecacheimpl(&pgcache, &badmem)) ;
   // invalid size
   badmem = (memblock_t) memblock_INIT(mem.size - KONFIG_MEMALIGN, mem.addr) ;
   TEST(EINVAL == freestatic_pagecacheimpl(&pgcache, &badmem)) ;
   TEST(0 == freestatic_pagecacheimpl(&pgcache, &mem)) ;
   TEST(0 == pgcache.sizeallocated) ;
   TEST(0 == pgcache.sizestatic) ;

   // unprepare
   TEST(0 == free_pagecacheimpl(&pgcache)) ;

   return 0 ;
ONABORT:
   free_pagecacheimpl(&pgcache) ;
   return EINVAL ;
}

static int test_cache(void)
{
   pagecache_impl_t     pgcache   = pagecache_impl_INIT_FREEABLE ;
   pagecache_block_t *  block[10] ;

   // TEST releasecached_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      TEST(0 == init_pagecacheimpl(&pgcache)) ;
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == allocblock_pagecacheimpl(&pgcache, pgsize, &block[i])) ;
      }
      block[2]->usedpagecount = 1 ; // mark as in use
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == releasecached_pagecacheimpl(&pgcache)) ;
      }
      TEST(block[2] == last_blocklist(genericcast_dlist(&pgcache.blocklist))) ;
      for (pagesize_e pgsize2 = 0; pgsize2 < pagesize_NROFPAGESIZE; ++pgsize2) {
         if (pgsize2 == pgsize) {
            TEST(block[2] == last_freeblocklist(genericcast_dlist(&pgcache.freeblocklist[pgsize2]))) ;
         } else {
            TEST(0 == pgcache.freeblocklist[pgsize2].last) ;
         }
      }
      block[2]->usedpagecount = 0 ; // mark as unused
      TEST(0 == releasecached_pagecacheimpl(&pgcache)) ;
      TEST(0 == pgcache.freeblocklist[pgsize].last) ;
      TEST(0 == pgcache.blocklist.last) ;
      TEST(0 == free_pagecacheimpl(&pgcache)) ;
   }

   return 0 ;
ONABORT:
   free_pagecacheimpl(&pgcache) ;
   return EINVAL ;
}

static int test_initthread(void)
{
   pagecache_t          pgcache = pagecache_INIT_FREEABLE ;
   pagecache_impl_t *   pgcacheimpl ;

   // TEST s_pagecacheimpl_interface
   TEST(s_pagecacheimpl_interface.allocpage     == &allocpage_pagecacheimpl) ;
   TEST(s_pagecacheimpl_interface.releasepage   == &releasepage_pagecacheimpl) ;
   TEST(s_pagecacheimpl_interface.allocstatic   == &allocstatic_pagecacheimpl) ;
   TEST(s_pagecacheimpl_interface.freestatic    == &freestatic_pagecacheimpl) ;
   TEST(s_pagecacheimpl_interface.sizeallocated == &sizeallocated_pagecacheimpl) ;

   // TEST initthread_pagecacheimpl
   TEST(0 == initthread_pagecacheimpl(&pgcache)) ;
   pgcacheimpl = (pagecache_impl_t*)pgcache.object ;
   TEST(pgcacheimpl != 0) ;
   size_t alignedobjsize = !(sizeof(pagecache_impl_t) % KONFIG_MEMALIGN) ? sizeof(pagecache_impl_t) : sizeof(pagecache_impl_t) + KONFIG_MEMALIGN - (sizeof(pagecache_impl_t) % KONFIG_MEMALIGN) ;
   TEST(pgcacheimpl->sizestatic == alignedobjsize)
   size_t  alignedheadersize = !(sizeof(staticpage_t) % KONFIG_MEMALIGN) ? sizeof(staticpage_t) : sizeof(staticpage_t) + KONFIG_MEMALIGN - (sizeof(staticpage_t) % KONFIG_MEMALIGN) ;
   pagecache_block_t * block = first_freeblocklist(genericcast_dlist(&pgcacheimpl->freeblocklist[pagesize_4096])) ;
   TEST(pgcache.object == (void*)(block->pageblock.addr + alignedheadersize)) ;
   TEST(pgcache.iimpl  == genericcast_pagecacheit(&s_pagecacheimpl_interface, pagecache_impl_t)) ;

   // TEST initthread_pagecacheimpl: EINVAL (pgcache.object != 0)
   pagecache_t oldpgcache = pgcache ;
   TEST(EINVAL == initthread_pagecacheimpl(&pgcache)) ;
   TEST(oldpgcache.object == pgcache.object) ;
   TEST(oldpgcache.iimpl  == pgcache.iimpl) ;

   // TEST freethread_pagecacheimpl
   TEST(0 == freethread_pagecacheimpl(&pgcache)) ;
   TEST(0 == pgcache.object) ;
   TEST(0 == pgcache.iimpl) ;
   TEST(0 == freethread_pagecacheimpl(&pgcache)) ;
   TEST(0 == pgcache.object) ;
   TEST(0 == pgcache.iimpl) ;

   return 0 ;
ONABORT:
   (void) freethread_pagecacheimpl(&pgcache) ;
   return EINVAL ;
}

int unittest_memory_pagecacheimpl()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_block())       goto ONABORT ;
   if (test_initfree())    goto ONABORT ;
   if (test_helper())      goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_alloc())       goto ONABORT ;
   if (test_cache())       goto ONABORT ;
   if (test_initthread())  goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
