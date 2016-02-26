/* title: PageCacheImpl impl

   Implements <PageCacheImpl>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/pagecache_impl.h
    Header file <PageCacheImpl>.

   file: C-kern/memory/pagecache_impl.c
    Implementation file <PageCacheImpl impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/inmem/dlist.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

/* typedef: struct freepage_t
 * Export type <freepage_t> into global namespace. */
typedef struct freepage_t freepage_t;

/* typedef: pagecache_impl_it
 * Defines interface pagecache_impl_it - see <pagecache_IT>.
 * The interface supports object type <pagecache_impl_t> and is compatible with <pagecache_it>. */
typedef pagecache_IT(pagecache_impl_t) pagecache_impl_it;



/* struct: freepage_t
 * Header of free page located in <pagecache_block_t.blockaddr>. */
struct freepage_t {
   dlist_node_t*      next;
   dlist_node_t*      prev;
   pagecache_block_t* marker;
};

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
   /* variable:   threadcontext
    * Thread which allocated the memory block. */
   threadcontext_t* threadcontext;
   /* variable: blockaddr
    * Address of allocated system memory page of size <pagecache_block_BLOCKSIZE>.
    * One allocated block contains one or more pages of size <pagecache_t.pagesizeinbytes_pagecache>(<pagesize__>). */
   uint8_t*       blockaddr;
   /* variable: next_block
    * Used to store all allocated <pagecache_block_t> in a list. */
   dlist_node_t   next_block;
   /* variable: next_freeblock
    * Used to store <pagecache_block_t> in a free list if this block contains free pages. */
   dlist_node_t   next_freeblock;
   /* variable: freepagelist
    * List of free pages. If this list is empty <next_freeblock> is not in use. */
   dlist_t        freepagelist;
   /* variable: usedpagecount
    * Counts the number of allocated pages in use.
    * If this value is 0 the whole block could be freed. */
   uint16_t       usedpagecount;
   /* variable: pgsize
    * Size of page encoded as <pagesize_e>.
    * Used as index into <pagecache_impl_t.freeblocklist>. */
   uint8_t        pgsize;
};


// section: pagecache_blockmap_t

// group: lifetime

int init_pagecacheblockmap(/*out*/pagecache_blockmap_t* blockmap)
{
   int err;

   // TODO: Add konfig variable to diff. between 32/64 bit mode !!
   // Change pagecache_blockmap_ARRAYSIZE to support 256 GB in 64 bit mode
   // And only 4 GB in 32 bit mode
   err = init_vmpage(cast_vmpage(blockmap, array_), pagecache_blockmap_ARRAYSIZE);
   if (err) goto ONERR;

   memset(blockmap->array_addr, 0, blockmap->array_size);
   static_assert(sizeof(pagecache_block_t) == 32 || sizeof(pagecache_block_t) == 64, "Size optimized for access (only shift required)");
   blockmap->array_len = blockmap->array_size / sizeof(pagecache_block_t);
   blockmap->indexmask = makepowerof2_int(blockmap->array_len) - 1;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_pagecacheblockmap(pagecache_blockmap_t* blockmap)
{
   int err;

   if (blockmap->array_len) {
      err = free_vmpage(cast_vmpage(blockmap, array_));

      blockmap->array_len = 0;
      blockmap->indexmask = 0;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

static inline pagecache_block_t* at_pagecacheblockmap(pagecache_blockmap_t* blockmap, const size_t arrayindex)
{
   size_t             idx = (arrayindex & blockmap->indexmask);
   pagecache_block_t* blockentry;

   blockentry = (pagecache_block_t*) (blockmap->array_addr + sizeof(pagecache_block_t) * idx);

   if (  idx >= blockmap->array_len
         || tcontext_maincontext() != (threadcontext_t*)read_atomicint((uintptr_t*)&blockentry->threadcontext)) {
      goto ONERR;
   }

   return blockentry;
ONERR:
   return 0;
}

// group: update

static inline int assign_pagecacheblockmap(pagecache_blockmap_t* blockmap, const size_t arrayindex, /*out*/pagecache_block_t** block)
{
   int err;
   size_t             idx = (arrayindex & blockmap->indexmask);
   pagecache_block_t* blockentry;

   blockentry = (pagecache_block_t*) (blockmap->array_addr + sizeof(pagecache_block_t) * idx);

   if (  idx >= blockmap->array_len
         || 0 != cmpxchg_atomicint((uintptr_t*)&blockentry->threadcontext, 0, (uintptr_t)tcontext_maincontext())) {
      err = ENOMEM;
      goto ONERR;
   }

   *block = blockentry;

   return 0;
ONERR:
   return err;
}

static inline void clear_pagecacheblockmap(pagecache_blockmap_t* blockmap, pagecache_block_t* block)
{
   (void) blockmap;
   write_atomicint((uintptr_t*)&block->threadcontext, 0);
}



// section: pagecache_block_t

// group: static variables
#ifdef KONFIG_UNITTEST
static test_errortimer_t   s_pagecacheblock_errtimer = test_errortimer_FREE;
#endif

// group: constants

#define pagecache_block_BLOCKSIZE (1024*1024)

// group: helper

/* define: slist_IMPLEMENT _blocklist
 * Macro generates slist interface FCT_blocklist for <pagecache_block_t>. */
dlist_IMPLEMENT(_blocklist, pagecache_block_t, next_block.)

/* define: slist_IMPLEMENT _freeblocklist
 * Macro generates slist interface FCT_freeblocklist for <pagecache_block_t>. */
dlist_IMPLEMENT(_freeblocklist, pagecache_block_t, next_freeblock.)

/* function: arrayindex_pagecacheblock
 * Returns an array index for an addr located in
 * [<pagecache_block_t.blockaddr>..<pagecache_block_t.blockaddr>+pagecache_block_BLOCKSIZE-1].
 * It is used to access <pagecache_blockmap_t>. */
static inline size_t arrayindex_pagecacheblock(const void* addr)
{
   return (uintptr_t)addr / pagecache_block_BLOCKSIZE;
}

// group: lifetime

/* function: freepageblock_pagecacheblock
 * Frees resources of <pagecache_block_t> stored in <pagecache_block_t.pageblock>. */
static inline int freepageblock_pagecacheblock(vmpage_t* pageblock)
{
   return free_vmpage(pageblock);
}

/* function: new_pagecacheblock
 * Allocates a big block of memory and returns its description in <pagecache_block_t>.
 * The returned <pagecache_block_t> is allocated on the heap. */
static int new_pagecacheblock(
         /*out*/pagecache_block_t** block,
         pagesize_e                 pgsize,
         pagecache_blockmap_t*      blockmap)
{
   int err;
   vmpage_t           pageblock = vmpage_FREE;
   pagecache_block_t* newblock;

   static_assert( pagecache_block_BLOCKSIZE >= (1024*1024)
                  && pagesize_1MB + 1u == pagesize__NROF,
                  "pagecache_block_BLOCKSIZE supports the largest value of pagesize_e");

   if (! PROCESS_testerrortimer(&s_pagecacheblock_errtimer, &err)) {
      err = initaligned_vmpage(&pageblock, pagecache_block_BLOCKSIZE);
   }
   if (err) goto ONERR;

   if (! PROCESS_testerrortimer(&s_pagecacheblock_errtimer, &err)) {
      err = assign_pagecacheblockmap(blockmap, arrayindex_pagecacheblock(pageblock.addr), &newblock);
   }
   if (err) goto ONERR;

   // Member newblock->threadcontext was set in assign_pagecacheblockmap
   newblock->blockaddr    = pageblock.addr;
   // Member newblock->next_block will be set in calling function
   // Member newblock->next_freeblock will be set in calling function
   newblock->freepagelist  = (dlist_t) dlist_INIT;
   newblock->usedpagecount = 0;
   newblock->pgsize        = pgsize;
   size_t pagesizeinbytes = pagesizeinbytes_pagecache(pgsize);
   for (size_t pageoffset = 0; pageoffset < pagecache_block_BLOCKSIZE; pageoffset += pagesizeinbytes) {
      freepage_t* freepage = (freepage_t*) (pageblock.addr + pageoffset);
      freepage->marker = newblock;
      insertlast_freepagelist(&newblock->freepagelist, freepage);
   }

   // init out value
   *block = newblock;

   return 0;
ONERR:
   free_vmpage(&pageblock);
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: free_pagecacheblock
 * Frees <pagecache_block_t.pageblock> but keeps empty block structure.
 * Another call to <new_pagecacheblock> will reuse it. */
static int free_pagecacheblock(pagecache_block_t* block, pagecache_blockmap_t* blockmap)
{
   int err;

   clear_pagecacheblockmap(blockmap, block);

   if (block->blockaddr) {
      vmpage_t pageblock = vmpage_INIT(pagecache_block_BLOCKSIZE, block->blockaddr);
      block->blockaddr = 0;

      err = free_vmpage(&pageblock);
      (void) PROCESS_testerrortimer(&s_pagecacheblock_errtimer, &err);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: alloc

static int releasepage_pagecacheblock(struct pagecache_block_t* block, freepage_t* freepage)
{
   // marker indicates that it is already stored in free list
   if (block == freepage->marker) {
      foreach (_freepagelist, nextfreepage, &block->freepagelist) {
         if (freepage == nextfreepage) {
            return EALREADY;  // already stored in freepagelist
         }
      }
      // false positive
   } else {
      freepage->marker = block;
   }

   insertfirst_freepagelist(&block->freepagelist, freepage);
   -- block->usedpagecount;

   return 0;
}

/* function: allocpage_pagecacheblock
 *
 * Ungeprüfte Precondition:
 * o ! isempty_dlist(&block->freepagelist) */
static void allocpage_pagecacheblock(struct pagecache_block_t* block, freepage_t** freepage)
{
   removefirst_freepagelist(&block->freepagelist, freepage);

   (*freepage)->marker = 0;
   ++ block->usedpagecount;
}



// section: pagecache_impl_t

// group: static variables

/* variable: s_pagecacheimpl_interface
 * Stores single instance of interface <pagecache_it>. */
static pagecache_impl_it   s_pagecacheimpl_interface = pagecache_it_INIT(
                              &allocpage_pagecacheimpl,
                              &releasepage_pagecacheimpl,
                              &sizeallocated_pagecacheimpl,
                              &emptycache_pagecacheimpl
                           );

// group: initthread

struct pagecache_it* interface_pagecacheimpl(void)
{
   return cast_pagecacheit(&s_pagecacheimpl_interface, pagecache_impl_t);
}

// group: helper

/* function: findfreeblock_pagecacheimpl
 * Find empty <pageblock_block_t> on freelist and return it in freeblock.
 * The freelist <pagecache_impl_t.freeblocklist> at index pgsize is searched.
 * All encountered <pageblock_block_t> which are empty are removed from the list.
 * If there is no empty block for the corresponding pgsize the error ESRCH is returned. */
static inline int findfreeblock_pagecacheimpl(pagecache_impl_t* pgcache, pagesize_e pgsize, pagecache_block_t** freeblock)
{
   int err;

   // Use some kind of priority queue ?

   err = ESRCH;

   foreach (_freeblocklist, block, cast_dlist(&pgcache->freeblocklist[pgsize])) {
      if (!isempty_dlist(&block->freepagelist)) {
         *freeblock = block;
         err = 0;
         break;
      }
   }

   return err;
}

/* function: allocblock_pagecacheimpl
 * Allocate new <pageblock_block_t> and add it to freelist.
 * The new block is returned in block if this value is not NULL.
 * The allocated block is inserted in freelist <pagecache_impl_t.freeblocklist> at index pgsize.
 * The allocated block is also inserted into list <pagecache_impl_t.blocklist>.
 * In case of error ENOMEM is returned. */
static inline int allocblock_pagecacheimpl(pagecache_impl_t* pgcache, pagesize_e pgsize, /*out*/pagecache_block_t** block)
{
   int err;

   err = new_pagecacheblock(block, pgsize, blockmap_maincontext());
   if (err) return err;

   insertlast_freeblocklist(cast_dlist(&pgcache->freeblocklist[pgsize]), *block);
   insertlast_blocklist(cast_dlist(&pgcache->blocklist), *block);

   return 0;
}

/* function: freeblock_pagecacheimpl
 * Frees <pageblock_block_t> and removes it from freelist and blocklist.
 *
 * Ungeprüfte Precondition:
 * o block ∊ pgcache->freeblocklist
 * o block ∊ pgcache->blocklist */
static inline int freeblock_pagecacheimpl(pagecache_impl_t* pgcache, pagecache_block_t* block)
{
   int err;

   remove_freeblocklist(cast_dlist(&pgcache->freeblocklist[block->pgsize]), block);
   remove_blocklist(cast_dlist(&pgcache->blocklist), block);

   err = free_pagecacheblock(block, blockmap_maincontext());

   return err;
}

// group: lifetime

int init_pagecacheimpl(/*out*/pagecache_impl_t* pgcache)
{
   int err;

   static_assert(lengthof(pgcache->freeblocklist) == pagesize__NROF, "every pagesize has its own free list");

   *pgcache = (pagecache_impl_t) pagecache_impl_FREE;

   pagecache_block_t* block;

   err = allocblock_pagecacheimpl(pgcache, pagesize_4096, &block);
   if (err) goto ONERR;

   return 0;
ONERR:
   free_pagecacheimpl(pgcache);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_pagecacheimpl(pagecache_impl_t* pgcache)
{
   int err;
   int err2;

   err = 0;

   foreach (_blocklist, nextblock, cast_dlist(&pgcache->blocklist)) {
      err2 = free_pagecacheblock(nextblock, blockmap_maincontext());
      if (err2) err = err2;
   }

   if (0 != sizeallocated_pagecacheimpl(pgcache)) {
      err = ENOTEMPTY;
   }
   (void) PROCESS_testerrortimer(&s_pagecacheblock_errtimer, &err);

   *pgcache = (pagecache_impl_t) pagecache_impl_FREE;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

bool isfree_pagecacheimpl(const pagecache_impl_t* pgcache)
{
   if (0 != pgcache->blocklist.last) {
      return false;
   }

   for (unsigned i = 0; i < lengthof(pgcache->freeblocklist); ++i) {
      if (0 != pgcache->freeblocklist[i].last) return false;
   }

   return  0 == pgcache->sizeallocated;
}

size_t sizeallocated_pagecacheimpl(const pagecache_impl_t* pgcache)
{
   return pgcache->sizeallocated;
}

// group: alloc

int allocpage_pagecacheimpl(pagecache_impl_t* pgcache, uint8_t pgsize, /*out*/struct memblock_t* page)
{
   int err;

   VALIDATE_INPARAM_TEST(pgsize < pagesize__NROF, ONERR, );

   pagecache_block_t* freeblock;

   err = findfreeblock_pagecacheimpl(pgcache, pgsize, &freeblock);
   if (err == ESRCH) {
      err = allocblock_pagecacheimpl(pgcache, pgsize, &freeblock);
   }
   if (err) goto ONERR;

   freepage_t* freepage;
   allocpage_pagecacheblock(freeblock, &freepage);
   if (isempty_dlist(&freeblock->freepagelist)) {
      // freeblock is full => remove it from list of free blocks
      remove_freeblocklist(cast_dlist(&pgcache->freeblocklist[pgsize]), freeblock);
   }

   size_t pgsizeinbytes = pagesizeinbytes_pagecache(pgsize);
   pgcache->sizeallocated += pgsizeinbytes;
   *page = (memblock_t) memblock_INIT(pgsizeinbytes, (uint8_t*)freepage);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int releasepage_pagecacheimpl(pagecache_impl_t* pgcache, struct memblock_t* page)
{
   int err;

   if (!isfree_memblock(page)) {
      size_t pagesizeinbytes;
      pagecache_block_t* block = at_pagecacheblockmap(blockmap_maincontext(), arrayindex_pagecacheblock(page->addr));
      if (  0 == block
            || page->size != (pagesizeinbytes = pagesizeinbytes_pagecache(block->pgsize))
            || 0 != (((uintptr_t)page->addr) & ((uintptr_t)pagesizeinbytes-1))) {
         err = EINVAL;
         goto ONERR;
      }

      freepage_t* freepage = (freepage_t*) page->addr;

      // support page located on freepage
      *page = (memblock_t) memblock_FREE;

      err = releasepage_pagecacheblock(block, freepage);
      if (err) goto ONERR;

      pgcache->sizeallocated -= pagesizeinbytes;
      if (!isinlist_freeblocklist(block)) {
         insertfirst_freeblocklist(cast_dlist(&pgcache->freeblocklist[block->pgsize]), block);
      }

      if (! block->usedpagecount) {
         // delete block if not used and at least another block is stored in free list
         pagecache_block_t* firstblock = last_freeblocklist(cast_dlist(&pgcache->freeblocklist[block->pgsize]));
         pagecache_block_t* lastblock  = first_freeblocklist(cast_dlist(&pgcache->freeblocklist[block->pgsize]));
         if (firstblock != lastblock) {
            err = freeblock_pagecacheimpl(pgcache, block);
            if (err) goto ONERR;
         }
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: cache

int emptycache_pagecacheimpl(pagecache_impl_t* pgcache)
{
   int err;

   for (unsigned pgsize = 0; pgsize < lengthof(pgcache->freeblocklist); ++pgsize) {
      foreach (_freeblocklist, block, cast_dlist(&pgcache->freeblocklist[pgsize])) {
         if (! block->usedpagecount) {
            err = freeblock_pagecacheimpl(pgcache, block);
            if (err) goto ONERR;
         }
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_blockmap(void)
{
   pagecache_blockmap_t blockmap = pagecache_blockmap_FREE;

   // TEST pagecache_blockmap_FREE
   TEST(0 == blockmap.array_addr);
   TEST(0 == blockmap.array_size);
   TEST(0 == blockmap.array_len);
   TEST(0 == blockmap.indexmask);

   // TEST init_pagecacheblockmap
   TEST(0 == init_pagecacheblockmap(&blockmap));
   TEST(blockmap.array_addr != 0);
   TEST(blockmap.array_size == 2*1024*1024);
   TEST(blockmap.array_len  == blockmap.array_size / sizeof(pagecache_block_t));
   TEST(blockmap.indexmask  >= blockmap.array_len-1);
   TEST(blockmap.indexmask/2 < blockmap.array_len);
   TEST(1 == ispowerof2_int(1+blockmap.indexmask));
   TEST(1 == ismapped_vm(cast_vmpage(&blockmap,array_), accessmode_RDWR_PRIVATE));

   // TEST free_pagecacheblockmap(&blockmap);
   TEST(0 == free_pagecacheblockmap(&blockmap));
   TEST(0 == blockmap.array_addr);
   TEST(0 == blockmap.array_size);
   TEST(0 == blockmap.array_len);
   TEST(0 == blockmap.indexmask);
   TEST(0 == free_pagecacheblockmap(&blockmap));
   TEST(0 == blockmap.array_addr);
   TEST(0 == blockmap.array_size);
   TEST(0 == blockmap.array_len);
   TEST(0 == blockmap.indexmask);
   TEST(1 == isunmapped_vm(cast_vmpage(&blockmap,array_)));

   // TEST at_pagecacheblockmap
   TEST(0 == init_pagecacheblockmap(&blockmap));
   for (size_t i = 0; i < blockmap.array_len; ++i) {
      pagecache_block_t* block = (pagecache_block_t*) (blockmap.array_addr + sizeof(pagecache_block_t) * i);
      TEST(0 == at_pagecacheblockmap(&blockmap, i));
      TEST(0 == block->threadcontext);
      block->threadcontext = tcontext_maincontext();  // allocation marker
      TEST(block == at_pagecacheblockmap(&blockmap, i));
      TEST(block == at_pagecacheblockmap(&blockmap, i + (blockmap.indexmask+1)));
   }
   if (blockmap.array_len <= blockmap.indexmask) {
      TEST(0 == at_pagecacheblockmap(&blockmap, blockmap.array_len));
   }
   TEST(0 == free_pagecacheblockmap(&blockmap));

   // TEST assign_pagecacheblockmap, clear_pagecacheblockmap
   TEST(0 == init_pagecacheblockmap(&blockmap));
   for (size_t i = 0; i < blockmap.array_len; ++i) {
      pagecache_block_t* block  = (pagecache_block_t*) (blockmap.array_addr + sizeof(pagecache_block_t) * i);
      pagecache_block_t* block2 = 0;
      TEST(0 == block->threadcontext);
      TEST(0 == assign_pagecacheblockmap(&blockmap, i, &block2));
      TEST(block->threadcontext == tcontext_maincontext());    // allocation marker set
      TEST(block == block2);
      clear_pagecacheblockmap(&blockmap, block);
      TEST(0 == block->threadcontext);
      TEST(0 == assign_pagecacheblockmap(&blockmap, i + (blockmap.indexmask+1), &block2));
      TEST(block->threadcontext == tcontext_maincontext());    // allocation marker set
      TEST(block == block2);
      TEST(ENOMEM == assign_pagecacheblockmap(&blockmap, i, 0/*not used*/)); // already marked
   }
   if (blockmap.array_len <= blockmap.indexmask) {
      TEST(ENOMEM == assign_pagecacheblockmap(&blockmap, blockmap.array_len, 0/*not used*/));
   }
   TEST(0 == free_pagecacheblockmap(&blockmap));

   return 0;
ONERR:
   free_pagecacheblockmap(&blockmap);
   return EINVAL;
}

static int test_block(void)
{
   pagecache_block_t*   block[pagesize__NROF] = { 0 };
   pagecache_blockmap_t blockmap              = pagecache_blockmap_FREE;

   // prepare
   TEST(0 == init_pagecacheblockmap(&blockmap));

   // TEST arrayindex_pagecacheblock
   for (size_t i = 0; i < 99; ++i) {
      uint8_t* addr = (void*) (i * pagecache_block_BLOCKSIZE);
      TEST(i == arrayindex_pagecacheblock(addr));
      TEST(i == arrayindex_pagecacheblock(addr+1));
      TEST(i == arrayindex_pagecacheblock(addr+pagecache_block_BLOCKSIZE-1));
   }

   // TEST new_pagecacheblock
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == new_pagecacheblock(&block[i], i, &blockmap));
      vmpage_t pageblock = vmpage_INIT(pagecache_block_BLOCKSIZE, block[i]->blockaddr);
      TEST(1 == ismapped_vm(&pageblock, accessmode_RDWR_PRIVATE));
      TEST(0 != block[i]);
      TEST(block[i]->threadcontext == tcontext_maincontext());
      TEST(0 != block[i]->blockaddr);
      TEST(0 == ((uintptr_t)block[i]->blockaddr % pagecache_block_BLOCKSIZE));
      TEST(0 != block[i]->freepagelist.last);
      TEST(0 == block[i]->usedpagecount);
      TEST(i == block[i]->pgsize);
      // check list of free pages
      size_t pgoffset = 0;
      foreach (_dlist, freepage, &block[i]->freepagelist) {
         TEST(freepage == (void*)(block[i]->blockaddr + pgoffset));
         TEST(block[i] == *(pagecache_block_t**)&freepage[1]);
         pgoffset += pagesizeinbytes_pagecache(i);
      }
   }

   // TEST free_pagecacheblock
   for (unsigned i = 0; i < lengthof(block); ++i) {
      vmpage_t pageblock = vmpage_INIT(pagecache_block_BLOCKSIZE, block[i]->blockaddr);
      TEST(0 != block[i]->threadcontext);
      TEST(0 != block[i]->blockaddr);
      TEST(0 == free_pagecacheblock(block[i], &blockmap));
      TEST(1 == isunmapped_vm(&pageblock));
      TEST(0 == block[i]->threadcontext);
      TEST(0 == block[i]->blockaddr);
      TEST(0 == free_pagecacheblock(block[i], &blockmap));
      TEST(0 == block[i]->threadcontext);
      TEST(0 == block[i]->blockaddr);
      block[i] = 0;
   }

   // TEST new_pagecacheblock: ENOMEM
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM);
   TEST(ENOMEM   == new_pagecacheblock(&block[0], pagesize_4096, &blockmap));
   TEST(block[0] == 0);
   init_testerrortimer(&s_pagecacheblock_errtimer, 2, ENOMEM);
   TEST(ENOMEM   == new_pagecacheblock(&block[0], pagesize_4096, &blockmap));
   TEST(block[0] == 0);

   // TEST free_pagecacheblock: ENOMEM
   {
      TEST(0 == new_pagecacheblock(&block[0], pagesize_4096, &blockmap));
      vmpage_t pageblock = vmpage_INIT(pagecache_block_BLOCKSIZE, block[0]->blockaddr);
      TEST(0 == isunmapped_vm(&pageblock));
      init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM);
      TEST(ENOMEM == free_pagecacheblock(block[0], &blockmap));
      TEST(1 == isunmapped_vm(&pageblock));
      TEST(0 == block[0]->threadcontext);
      TEST(0 == block[0]->blockaddr);
   }

   // TEST new_pagecacheblock: blockmap is filled correctly
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == new_pagecacheblock(&block[i], pagesize_16384, &blockmap));
   }
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      for (size_t offset = 0; offset < pagecache_block_BLOCKSIZE-3; offset += 16384) {
         TEST(block[i] == at_pagecacheblockmap(&blockmap, arrayindex_pagecacheblock(block[i]->blockaddr + offset)));
         TEST(block[i] == at_pagecacheblockmap(&blockmap, arrayindex_pagecacheblock(block[i]->blockaddr + offset + 3)));
      }
   }

   // TEST free_pagecacheblock: blockmap is updated
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      void* addr = block[i]->blockaddr;
      TEST(0 == free_pagecacheblock(block[i], &blockmap));
      TEST(0 == at_pagecacheblockmap(&blockmap, arrayindex_pagecacheblock(addr)));
   }

   // TEST allocpage_pagecacheblock
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      TEST(0 == new_pagecacheblock(&block[i], i, &blockmap));
   }
   for (unsigned i = 0; i < lengthof(block); ++i) {
      size_t offset;
      for (offset = 0; offset < pagecache_block_BLOCKSIZE; offset += pagesizeinbytes_pagecache(i)) {
         freepage_t* freepage = 0;
         allocpage_pagecacheblock(block[i], &freepage);
         TEST(0 != freepage);
         TEST(0 == freepage->marker);
         TEST(freepage == (void*) (block[i]->blockaddr + offset));
         TEST(0 == block[i]->usedpagecount - 1u - (offset/pagesizeinbytes_pagecache(i)));
      }
      TEST(offset == pagecache_block_BLOCKSIZE);
      TEST(isempty_freepagelist(&block[i]->freepagelist));
   }

   // TEST releasepage_pagecacheblock
   for (unsigned i = 0; i < lengthof(block); ++i) {
      TEST(0 == block[i]->freepagelist.last);
      for (size_t offset = pagecache_block_BLOCKSIZE; offset > 0; ) {
         offset -= pagesizeinbytes_pagecache(i);
         freepage_t* freepage = (void*) (block[i]->blockaddr + offset);
         TEST(0 == releasepage_pagecacheblock(block[i], freepage));
         TEST(0 == block[i]->usedpagecount - (offset / pagesizeinbytes_pagecache(i)));
         TEST(freepage         == first_freepagelist(cast_dlist(&block[i]->freepagelist)));
         TEST(freepage->marker == block[i]);
         // double free does nothing
         TEST(EALREADY == releasepage_pagecacheblock(block[i], freepage));
         TEST(freepage         == first_freepagelist(cast_dlist(&block[i]->freepagelist)));
         TEST(freepage->marker == block[i]);
      }
   }
   for (unsigned i = 0; i < lengthof(block); ++i) {
      // check list of free pages
      size_t pgoffset = 0;
      foreach (_dlist, freepage, &block[i]->freepagelist) {
         TEST(freepage == (void*)(block[i]->blockaddr + pgoffset));
         TEST(block[i] == *(pagecache_block_t**)&freepage[1]);
         pgoffset += pagesizeinbytes_pagecache(i);
      }
      TEST(0 == free_pagecacheblock(block[i], &blockmap));
   }

   // unprepare
   TEST(0 == free_pagecacheblockmap(&blockmap));

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(block); ++i) {
      if (block[i]) free_pagecacheblock(block[i], &blockmap);
   }
   free_pagecacheblockmap(&blockmap);
   return EINVAL;
}

static int test_initfree(void)
{
   pagecache_impl_t   pgcache = pagecache_impl_FREE;
   pagecache_block_t* block;

   // TEST pagecache_impl_FREE
   TEST(0 == pgcache.blocklist.last);
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      TEST(0 == pgcache.freeblocklist[i].last);
   }
   TEST(0 == pgcache.sizeallocated);

   // TEST init_pagecacheimpl
   memset(&pgcache, 255, sizeof(pgcache));
   pgcache.freeblocklist[pagesize_4096].last = 0;
   TEST(0 == init_pagecacheimpl(&pgcache));
   TEST(0 != pgcache.blocklist.last);
   TEST(0 != pgcache.freeblocklist[pagesize_4096].last);
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      if (i == pagesize_4096) {
         TEST(cast2object_blocklist(pgcache.blocklist.last) == cast2object_freeblocklist(pgcache.freeblocklist[i].last));
      } else {
         TEST(0 == pgcache.freeblocklist[i].last);
      }
   }
   TEST(0 == pgcache.sizeallocated);

   // TEST free_pagecacheimpl
   for (unsigned i = 0; i < pagesize__NROF; ++i) {
      TEST(0 == allocblock_pagecacheimpl(&pgcache, i, &block));
   }
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      TEST(0 != pgcache.freeblocklist[i].last);
   }
   TEST(0 == free_pagecacheimpl(&pgcache));
   TEST(1 == isfree_pagecacheimpl(&pgcache));
   TEST(0 == free_pagecacheimpl(&pgcache));
   TEST(1 == isfree_pagecacheimpl(&pgcache));

   // TEST init_pagecacheimpl: ENOMEM
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM);
   memset(&pgcache, 255, sizeof(pgcache));
   TEST(ENOMEM == init_pagecacheimpl(&pgcache));
   TEST(1 == isfree_pagecacheimpl(&pgcache));
   init_testerrortimer(&s_pagecacheblock_errtimer, 2, ENOMEM);
   memset(&pgcache, 255, sizeof(pgcache));
   TEST(ENOMEM == init_pagecacheimpl(&pgcache));
   TEST(1 == isfree_pagecacheimpl(&pgcache));

   // TEST free_pagecacheimpl: ENOTEMPTY
   TEST(0 == init_pagecacheimpl(&pgcache));
   TEST(0 == allocblock_pagecacheimpl(&pgcache, pagesize_16384, &block));
   pgcache.sizeallocated = 1;
   TEST(ENOTEMPTY == free_pagecacheimpl(&pgcache));
   TEST(1 == isfree_pagecacheimpl(&pgcache));

   // TEST free_pagecacheimpl: ENOMEM
   TEST(0 == init_pagecacheimpl(&pgcache));
   init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM);
   TEST(ENOMEM == free_pagecacheimpl(&pgcache));
   TEST(1 == isfree_pagecacheimpl(&pgcache));
   TEST(0 == init_pagecacheimpl(&pgcache));
   init_testerrortimer(&s_pagecacheblock_errtimer, 2, ENOMEM);
   TEST(ENOMEM == free_pagecacheimpl(&pgcache));
   TEST(1 == isfree_pagecacheimpl(&pgcache));

   return 0;
ONERR:
   free_pagecacheimpl(&pgcache);
   return EINVAL;
}

static int test_helper(void)
{
   pagecache_impl_t   pgcache  = pagecache_impl_FREE;
   pagecache_block_t* block[8] = { 0 };

   // TEST findfreeblock_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      pgcache = (pagecache_impl_t) pagecache_impl_FREE;
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == new_pagecacheblock(&block[i], pgsize, blockmap_maincontext()));
         insertlast_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize]), block[i]);
      }
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         pagecache_block_t* freeblock = 0;
         // returns first block not empty (empty blocks keep in list, removed by higher level function)
         TEST(1 == isinlist_freeblocklist(block[i]));
         TEST(0 == findfreeblock_pagecacheimpl(&pgcache, pgsize, &freeblock));
         TEST(freeblock == block[i]);
         freeblock->freepagelist.last = 0;   // simulate empty block
      }
      unsigned i = 0;
      foreach (_freeblocklist, freeblock, cast_dlist(&pgcache.freeblocklist[pgsize])) {
         TEST(freeblock == block[i]);
         ++ i;
      }
      TEST(i == lengthof(block));
      // returns ESRCH if no free blocks available
      pagecache_block_t* freeblock = 0;
      TEST(ESRCH == findfreeblock_pagecacheimpl(&pgcache, pgsize, &freeblock));
      pgcache.freeblocklist[pgsize].last = 0;
      TEST(ESRCH == findfreeblock_pagecacheimpl(&pgcache, pgsize, &freeblock));
      TEST(0 == freeblock);
      for (i = 0; i < lengthof(block); ++i) {
         TEST(0 == free_pagecacheblock(block[i], blockmap_maincontext()));
      }
   }

   // TEST allocblock_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      TEST(0 == init_pagecacheimpl(&pgcache));
      TEST(0 == freeblock_pagecacheimpl(&pgcache, last_blocklist(cast_dlist(&pgcache.blocklist))));
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == allocblock_pagecacheimpl(&pgcache, pgsize, &block[i]));
         TEST(0 != block[i]);
         TEST(block[i] == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
         TEST(block[i] == last_blocklist(cast_dlist(&pgcache.blocklist)));
         TEST(block[i] == at_pagecacheblockmap(blockmap_maincontext(), arrayindex_pagecacheblock(block[i]->blockaddr)));
         TEST(block[i] == at_pagecacheblockmap(blockmap_maincontext(), arrayindex_pagecacheblock(block[i]->blockaddr + pagecache_block_BLOCKSIZE-1)));
         TEST(block[i] != at_pagecacheblockmap(blockmap_maincontext(), arrayindex_pagecacheblock(block[i]->blockaddr -1)));
         TEST(block[i] != at_pagecacheblockmap(blockmap_maincontext(), arrayindex_pagecacheblock(block[i]->blockaddr + pagecache_block_BLOCKSIZE)));
      }
      for (pagesize_e pgsize2 = 0; pgsize2 < pagesize__NROF; ++pgsize2) {
         if (pgsize == pgsize2) continue;
         TEST(0 == pgcache.freeblocklist[pgsize2].last);
      }
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         size_t offset = 0;
         foreach (_freepagelist, nextpage, &block[i]->freepagelist) {
            TEST(nextpage == (void*) (block[i]->blockaddr + offset));
            offset += pagesizeinbytes_pagecache(pgsize);
         }
         TEST(offset == pagecache_block_BLOCKSIZE);
      }
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         block[i] = 0;
      }
      TEST(0 == free_pagecacheimpl(&pgcache));
   }

   // TEST allocblock_pagecacheimpl: ENOMEM
   TEST(0 == init_pagecacheimpl(&pgcache));
   TEST(0 == freeblock_pagecacheimpl(&pgcache, last_blocklist(cast_dlist(&pgcache.blocklist))));
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      init_testerrortimer(&s_pagecacheblock_errtimer, 1, ENOMEM);
      TEST(ENOMEM == allocblock_pagecacheimpl(&pgcache, pgsize, &block[0]));
      TEST(0 == block[0]);
      for (pagesize_e pgsize2 = 0; pgsize2 < pagesize__NROF; ++pgsize2) {
         TEST(0 == pgcache.freeblocklist[pgsize2].last);
      }
   }
   TEST(0 == free_pagecacheimpl(&pgcache));

   // TEST freeblock_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      TEST(0 == init_pagecacheimpl(&pgcache));
      TEST(0 == freeblock_pagecacheimpl(&pgcache, last_blocklist(cast_dlist(&pgcache.blocklist))));
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == allocblock_pagecacheimpl(&pgcache, pgsize, &block[i]));
         TEST(block[i] == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
         TEST(block[i] == last_blocklist(cast_dlist(&pgcache.blocklist)));
      }
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(block[i] == first_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
         TEST(block[i] == first_blocklist(cast_dlist(&pgcache.blocklist)));
         void* blockaddr = block[i]->blockaddr;
         TEST(block[i] == at_pagecacheblockmap(blockmap_maincontext(), arrayindex_pagecacheblock(blockaddr)));
         TEST(0 == freeblock_pagecacheimpl(&pgcache, block[i]));
         TEST(0 == at_pagecacheblockmap(blockmap_maincontext(), arrayindex_pagecacheblock(blockaddr)));
      }
      TEST(0 == pgcache.freeblocklist[pgsize].last);
      TEST(0 == pgcache.blocklist.last);
      TEST(0 == free_pagecacheimpl(&pgcache));
   }

   return 0;
ONERR:
   for (uint8_t i = 0; i < lengthof(block); ++i) {
      if (block[i]) free_pagecacheblock(block[i], blockmap_maincontext());
   }
   return EINVAL;
}

static int test_query(void)
{
   pagecache_impl_t pgcache = pagecache_impl_FREE;

   // TEST isfree_pagecacheimpl
   pgcache.blocklist.last = (void*)1;
   TEST(0 == isfree_pagecacheimpl(&pgcache));
   pgcache.blocklist.last = (void*)0;
   TEST(1 == isfree_pagecacheimpl(&pgcache));
   for (unsigned i = 0; i < lengthof(pgcache.freeblocklist); ++i) {
      pgcache.freeblocklist[i].last = (void*)1;
      TEST(0 == isfree_pagecacheimpl(&pgcache));
      pgcache.freeblocklist[i].last = (void*)0;
      TEST(1 == isfree_pagecacheimpl(&pgcache));
   }
   pgcache.sizeallocated = 1;
   TEST(0 == isfree_pagecacheimpl(&pgcache));
   pgcache.sizeallocated = 0;
   TEST(1 == isfree_pagecacheimpl(&pgcache));

   // TEST sizeallocated_pagecacheimpl
   TEST(0 == sizeallocated_pagecacheimpl(&pgcache));
   for (size_t i = 1; i; i <<= 1) {
      pgcache.sizeallocated = i;
      TEST(i == sizeallocated_pagecacheimpl(&pgcache));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_alloc(void)
{
   pagecache_impl_t   pgcache = pagecache_impl_FREE;
   pagecache_block_t* block   = 0;
   memblock_t         page    = memblock_FREE;
   size_t             oldsize;

   // TEST allocpage_pagecacheimpl
   TEST(0 == init_pagecacheimpl(&pgcache));
   TEST(0 == freeblock_pagecacheimpl(&pgcache, last_blocklist(cast_dlist(&pgcache.blocklist))));
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      TEST(0 == last_blocklist(cast_dlist(&pgcache.blocklist)));
      TEST(0 == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
      page = (memblock_t) memblock_FREE;
      oldsize = pgcache.sizeallocated;
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      block = last_blocklist(cast_dlist(&pgcache.blocklist));
      TEST(block != 0);   // new block allocated and stored in list
      TEST(block->pgsize == pgsize);
      for (size_t offset = 0; offset < pagecache_block_BLOCKSIZE; offset += pagesizeinbytes_pagecache(pgsize)) {
         TEST(page.addr == block->blockaddr + offset);
         TEST(page.size == pagesizeinbytes_pagecache(pgsize));
         TEST(0         == ((uintptr_t)page.addr % pagesizeinbytes_pagecache(pgsize))/*aligned to pagesize*/);
         TEST(pgcache.sizeallocated == oldsize + offset + pagesizeinbytes_pagecache(pgsize));
         TEST(block == last_blocklist(cast_dlist(&pgcache.blocklist)));                 // reuse block
         if (offset == pagecache_block_BLOCKSIZE - pagesizeinbytes_pagecache(pgsize)) {
            TEST(0 == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));  // removed from free list
         } else {
            TEST(block == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize]))); // reuse block
         }
         TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      }
      TEST(block == first_blocklist(cast_dlist(&pgcache.blocklist)));
      TEST(block != last_blocklist(cast_dlist(&pgcache.blocklist)));              // new block
      block = last_blocklist(cast_dlist(&pgcache.blocklist));
      if (pagesizeinbytes_pagecache(pgsize) < pagecache_block_BLOCKSIZE) {
         TEST(0 != last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));  // new block
         TEST(block == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
         TEST(block == first_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
      }
      TEST(page.addr == block->blockaddr);
      TEST(page.size == pagesizeinbytes_pagecache(pgsize));
      // free blocks
      block = first_blocklist(cast_dlist(&pgcache.blocklist));
      TEST(0 == free_pagecacheblock(block, blockmap_maincontext()));
      block = last_blocklist(cast_dlist(&pgcache.blocklist));
      TEST(0 == free_pagecacheblock(block, blockmap_maincontext()));
      pgcache.blocklist.last             = 0;
      pgcache.freeblocklist[pgsize].last = 0;
      pgcache.sizeallocated              = oldsize;
   }

   // TEST allocpage_pagecacheimpl
   TEST(EINVAL == allocpage_pagecacheimpl(&pgcache, (uint8_t)-1, &page));
   TEST(EINVAL == allocpage_pagecacheimpl(&pgcache, pagesize__NROF, &page));
   TEST(pgcache.blocklist.last == 0);
   TEST(pgcache.sizeallocated  == oldsize);

   // TEST releasepage_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      page = (memblock_t) memblock_FREE;
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      pagecache_block_t* firstblock = last_blocklist(cast_dlist(&pgcache.blocklist));
      for (size_t offset = 0; offset < pagecache_block_BLOCKSIZE; offset += pagesizeinbytes_pagecache(pgsize)) {
         TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      }
      TEST(firstblock == first_blocklist(cast_dlist(&pgcache.blocklist)));
      block = last_blocklist(cast_dlist(&pgcache.blocklist));
      if (pagesizeinbytes_pagecache(pgsize) < pagecache_block_BLOCKSIZE) {
         TEST(block == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
         TEST(block == first_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
      }
      TEST(block != firstblock);
      TEST(pgcache.sizeallocated == oldsize + pagecache_block_BLOCKSIZE + pagesizeinbytes_pagecache(pgsize))
      TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
      TEST(0 == page.addr);
      TEST(0 == page.size);
      TEST(pgcache.sizeallocated == oldsize + pagecache_block_BLOCKSIZE)
      TEST(block->usedpagecount  == 0);
      for (size_t offset = 0; offset < pagecache_block_BLOCKSIZE; offset += pagesizeinbytes_pagecache(pgsize)) {
         page.addr = firstblock->blockaddr + offset;
         page.size = pagesizeinbytes_pagecache(pgsize);
         TEST(pgcache.sizeallocated     == oldsize + pagecache_block_BLOCKSIZE-offset)
         TEST(firstblock->usedpagecount == (pagecache_block_BLOCKSIZE-offset)/pagesizeinbytes_pagecache(pgsize));
         TEST(block == last_blocklist(cast_dlist(&pgcache.blocklist)));
         TEST(firstblock == first_blocklist(cast_dlist(&pgcache.blocklist)));
         if (offset) {
            // firstblock added to freelist
            TEST(block      == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
            TEST(firstblock == first_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
         } else {
            TEST(block == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
            TEST(block == first_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
         }
         TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
         TEST(0 == page.addr);
         TEST(0 == page.size);
         // isfree_memblock(&page) ==> second call does nothing !
         TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
      }
      TEST(pgcache.sizeallocated == oldsize);
      // firstblock deleted !
      TEST(block == last_blocklist(cast_dlist(&pgcache.blocklist)));
      TEST(block == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
      TEST(block == first_blocklist(cast_dlist(&pgcache.blocklist)));
      TEST(block == first_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize])));
      // free allocated blocks
      TEST(0 == free_pagecacheblock(block, blockmap_maincontext()));
      pgcache.blocklist.last             = 0;
      pgcache.freeblocklist[pgsize].last = 0;
   }
   TEST(0 == free_pagecacheimpl(&pgcache));

   // TEST releasepage_pagecacheimpl: free memblock_t located on allocated page
   TEST(0 == init_pagecacheimpl(&pgcache));
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_1MB, &page));
   memblock_t* onpage = (memblock_t*) page.addr;
   *onpage = page;
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_1MB, &page));
   TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));     // generate entry in free list
   TEST(0 == isunmapped_vm(&(vmpage_t)vmpage_INIT(1024*1024,(uint8_t*)onpage)));
   TEST(0 == releasepage_pagecacheimpl(&pgcache, onpage));    // now free whole block with page located on it
   TEST(1 == isunmapped_vm(&(vmpage_t)vmpage_INIT(1024*1024,(uint8_t*)onpage)));
   TEST(0 == free_pagecacheimpl(&pgcache));

   // TEST releasepage_pagecacheimpl: EALREADY
   TEST(0 == init_pagecacheimpl(&pgcache));
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_4096, &page));
   memblock_t page2 = page;
   TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
   TEST(EALREADY == releasepage_pagecacheimpl(&pgcache, &page2));
   TEST(0 == free_pagecacheimpl(&pgcache));

   // TEST releasepage_pagecacheimpl: EINVAL
   TEST(0 == init_pagecacheimpl(&pgcache));
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_4096, &page));
   // outside of block (not found)
   memblock_t badpage = memblock_INIT(page.size, page.addr-1);
   TEST(EINVAL == releasepage_pagecacheimpl(&pgcache, &badpage));
   // addr not aligned
   badpage = (memblock_t) memblock_INIT(page.size, page.addr+1);
   TEST(EINVAL == releasepage_pagecacheimpl(&pgcache, &badpage));
   // wrong size
   badpage = (memblock_t) memblock_INIT(page.size+1, page.addr);
   TEST(EINVAL == releasepage_pagecacheimpl(&pgcache, &badpage));
   // block already freed (not found)
   block = at_pagecacheblockmap(blockmap_maincontext(), arrayindex_pagecacheblock(page.addr));
   TEST(0 != block);
   TEST(0 == freeblock_pagecacheimpl(&pgcache, block));
   TEST(EINVAL == releasepage_pagecacheimpl(&pgcache, &page));
   pgcache.sizeallocated -= 4096;
   TEST(0 == free_pagecacheimpl(&pgcache));

   // unprepare
   TEST(0 == free_pagecacheimpl(&pgcache));

   return 0;
ONERR:
   free_pagecacheimpl(&pgcache);
   return EINVAL;
}

static int test_cache(void)
{
   pagecache_impl_t   pgcache = pagecache_impl_FREE;
   pagecache_block_t* block[10];

   // TEST emptycache_pagecacheimpl
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      TEST(0 == init_pagecacheimpl(&pgcache));
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == allocblock_pagecacheimpl(&pgcache, pgsize, &block[i]));
      }
      block[2]->usedpagecount = 1; // mark as in use
      for (uint8_t i = 0; i < lengthof(block); ++i) {
         TEST(0 == emptycache_pagecacheimpl(&pgcache));
      }
      TEST(block[2] == last_blocklist(cast_dlist(&pgcache.blocklist)));
      for (pagesize_e pgsize2 = 0; pgsize2 < pagesize__NROF; ++pgsize2) {
         if (pgsize2 == pgsize) {
            TEST(block[2] == last_freeblocklist(cast_dlist(&pgcache.freeblocklist[pgsize2])));
         } else {
            TEST(0 == pgcache.freeblocklist[pgsize2].last);
         }
      }
      block[2]->usedpagecount = 0; // mark as unused
      TEST(0 == emptycache_pagecacheimpl(&pgcache));
      TEST(0 == pgcache.freeblocklist[pgsize].last);
      TEST(0 == pgcache.blocklist.last);
      TEST(0 == free_pagecacheimpl(&pgcache));
   }

   return 0;
ONERR:
   free_pagecacheimpl(&pgcache);
   return EINVAL;
}

int unittest_memory_pagecacheimpl()
{
   if (test_blockmap())    goto ONERR;
   if (test_block())       goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_helper())      goto ONERR;
   if (test_query())       goto ONERR;
   if (test_alloc())       goto ONERR;
   if (test_cache())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
