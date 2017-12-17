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
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/memory/ptr.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/process.h"
#endif

// INFO

/*
   == Memory Layout ==

   |<- pagecache_impl_BLOCKSIZE                                                                              ->|
            (subheader_t)           (subheader_t)                                           (subheader_t)
   --------------⯆-----------------------⯆-------------------------------------------------------⯆-------------
   |         | (page) |     | (page) | (page) | (page) |      | (page) |     | (page) |      | (page) | (page) |
   | block_t | (free) | ... |        | (free) |        |  ... |        | ... | (used) |  ... | (free) | (used) |
   |         |        |     |        |        |        |      |        |     |        |      |        |        |
   ------------------------------------------------------------ ------------------------------------------------
   |<- pagecache_impl_SUBBLOCKSIZE ->|<- pagecache_impl_SUBBLOCKSIZE ->| ... |<- pagecache_impl_SUBBLOCKSIZE ->|

   +++
   (1) subheader_t is stored at first page during initialization time (or first possible in case of first subblock)
   (2) Freeing a page on a subblock whose pages are all used stores the subheader on the first free page
   +++

*/

// forward
struct block_t;

// private types

/* typedef: pagecache_impl_it
 * Defines interface pagecache_impl_it - see <pagecache_IT>.
 * The interface supports object type <pagecache_impl_t> and is compatible with <pagecache_it>. */
typedef pagecache_IT(pagecache_impl_t) pagecache_impl_it;


/* struct: freepage_t
 * Header of free page located in corresponding <subblock_t> of <block_t>. */
typedef struct freepage_t {
   struct block_t*marker;
   dlist_node_t  *next;
   dlist_node_t  *prev;
} freepage_t;

// group: helper

/* define: INTERFACE_freepagelist
 * Macro <dlist_IMPLEMENT> generates dlist interface for <freepage_t>. */
dlist_IMPLEMENT(_freepagelist, freepage_t, )


/* struct: subheader_t
 * Verwaltet Speicherseiten einer bestimmten Größe.
 * Siehe <pagesize_e> für die verschiedenen Größen.
 *
 * Alle verwalteten Speciherseiten zusammen belegen genau pagecache_impl_SUBBLOCKSIZE bytes.
 *
 * Der erste sub-block innerhalb von <block_t> ist speziell dahingehend,
 * daß die erste Seite die <block_t> STruktur aufnimmt.
 *
 * */
typedef struct subheader_t {
   /* variable: marker
    * Same marker as used in <freepage_t> to mark a page as free.
    * The page <subheader_t> is located on is used as last one in
    * the allocation process. */
   struct block_t*marker;
   /* variable: self
    * Additional marker for subheader_t. */
   struct subheader_t* self;
   /* variable: freenode
    * Used to store <subheader_t> in a list of free or unused sub-blocks
    * if it contains free pages or is totally unused. */
   dlist_node_t   freenode;
   /* variable: freepagelist
    * Liste freier Speicherseiten. */
   dlist_t        freepagelist;
   /* variable: nrnextfree
    * The number of free pages not registered in freepagelist.
    * If this value is 0 all free pages are registered on freepagelist if any. */
   unsigned       nrnextfree;
   /* variable: nrusedpages
    * Number of used pages allocated by the caller. */
   unsigned       nrusedpages;
   /* variable: nextfree
    * Address of next free page not registered in freepagelist. */
   uint8_t*       nextfree;   // uint8_t nextfree[nrnextfree][pagesize_in_bytes];
} subheader_t;

// group: helper

/* define: INTERFACE_subheaderlist
 * Macro <dlist_IMPLEMENT> generates dlist interface for <subheader_t>. */
dlist_IMPLEMENT(_subheaderlist, subheader_t, freenode.)


/* struct: block_t
 * Manages a big chunk of aligned memory which is divided into a number of <subblock_t>.
 * Every subblock_t manages a set of allocatable memory pages of a certain <pagesize_e>.
 * This type defines the granularity a thread grows or shrinks its own memory usage.
 * */
typedef struct block_t {
   /* variable: owner
    * Address of pagecache_impl_t which called new_block this block and therefore owns it. */
   pagecache_impl_t *owner;
   /* variable: threadcontext
    * Thread which allocated the memory block. */
   threadcontext_t  *threadcontext;
   /* variable: blocknode
    * Used to store all allocated <block_t> in a list. */
   dlist_node_t      blocknode;
   /* variable: unusedblocknode
    * Used to store all <block_t> in a list which contains unused <subblock_t>. */
   dlist_node_t      unusedblocknode;
   /* variable: freeblocknode
    * Used to store all <block_t> in a list which contains free <sublock_t> suuporting a certain <pagesize_e>. */
   dlist_node_t      freeblocknode[pagesize__NROF];
   /* variable: freesublist
    * List of partially free <subblock_t> supporting a certain <pagesize_e>. */
   dlist_t           freesublist[pagesize__NROF];
   /* variable: unusedsublock
    * List of unused <subblock_t> supporting any <pagesize_e>. */
   dlist_t           unusedsublist;
   /* variable: nextunused
    * Index (starting from 0) of next unused <subblock_t> not registered at <unusedsublist>. */
   unsigned          nextunused;
   /* variable: nrusedpages
    * Number of used pages allocated by the caller. This value is the sum of all used pages
    * in all sub.blocks. */
   uint32_t          nrusedpages;
   /* variable: pgsize
    * Größe der Speicherseiten, die von einem Subblock verwaltet werden.
    * Wertebereich aus <pagesize_e>. Benutzt als Index für <freesublist>. */
   uint8_t           pgsize[pagecache_impl_NRSUBBLOCKS];
   /* variable: header
    * The index plus one of the page <subheader_t> is located on in a sub-block.
    * If this value is 0 the sub-block has no subheader_t and is not listed on any
    * list. Either every page on the corresponding sub-block is used or all pages
    * are unused and the unused sub-block is not managed by <unusedsublist> but
    * instead by <nextunused>. */
   uint16_t          header[pagecache_impl_NRSUBBLOCKS];
} block_t;

// group: static variables
#ifdef KONFIG_UNITTEST
static test_errortimer_t   s_block_errtimer = test_errortimer_FREE;
#endif

// group: low-level-helper

static inline block_t* align_block(void* addr)
{
   return (block_t*) ((uintptr_t)addr & ~((uintptr_t)pagecache_impl_BLOCKSIZE-1));
}

static inline uint16_t index_subblock(void* addr)
{
   return (uint16_t) (((uintptr_t)addr & ((uintptr_t)pagecache_impl_BLOCKSIZE-1)) / pagecache_impl_SUBBLOCKSIZE);
}

static inline subheader_t* align_subheader(subheader_t* header)
{
   return (subheader_t*) ((uintptr_t)header & ~((uintptr_t)pagecache_impl_SUBBLOCKSIZE-1));
}

// group: subblock-helper

// -- lifetime

/* function: initfree_subheader
 * Inits header for sub-block whose pages are all free.
 *
 * Parameter:
 * block     - Pointer to block which contains this sub-block.
 * subindex  - Index of the sub-block (0 == sub-block has same start address as block)
 * pageindex - Index of the page this header is located at. Index 0 indicates the first page.
 *             The address of the first page is also the start address of the subblock.
 *
 * Unchecked Precondition:
 * - header points to start address (first page) of whole sub-block or one of the following pages.
 * - All pages are free. This sub-block is no more referenced. */
static inline void initfree_subheader(block_t* block, subheader_t* header, unsigned subindex, unsigned pageindex, pagesize_e pgsize)
{
   static_assert( offsetof(subheader_t, marker) == offsetof(freepage_t, marker)
                  && sizeof(((subheader_t*)0)->marker) == sizeof(((freepage_t*)0)->marker),
                  "overlay");
   unsigned nrpages = (pagecache_impl_SUBBLOCKSIZE >> log2pagesizeinbytes_pagecache(pgsize));
   header->marker = block;
   header->self = header;
   header->freepagelist = (dlist_t) dlist_INIT;
   header->nrusedpages = pageindex;
   ++ pageindex;
   header->nrnextfree = (nrpages-pageindex); // -pageindex-1 (skip page where header is located)
   header->nextfree = (uint8_t*) header + pagesizeinbytes_pagecache(pgsize);
   block->pgsize[subindex] = pgsize;
   block->header[subindex] = (uint16_t) pageindex; // pageindex + 1 means located at pageindex
}

/* function: initused_subheader
 * Inits header for sub-block whose pages ar all used except the one the header is located on.
 *
 * Parameter:
 * block     - Pointer to block which contains this sub-block.
 * subindex  - Index of the sub-block (0 == sub-block has same start address as block)
 * pageindex - Index of the page this header is located at. Index 0 indicates the first page.
 *             The address of the first page is also the start address of the subblock.
 *
 * Unchecked Precondition:
 * - header points to address of page with index pageindex.
 * - All pages are used. This sub-block is no more referenced. */
static inline void initused_subheader(block_t* block, subheader_t* header, unsigned subindex, unsigned pageindex, pagesize_e pgsize)
{
   unsigned nrpages = pagecache_impl_SUBBLOCKSIZE >> log2pagesizeinbytes_pagecache(pgsize);
   header->marker = block;
   header->self = header;
   header->freepagelist = (dlist_t) dlist_INIT;
   header->nrnextfree = 0;
   header->nrusedpages = (nrpages - 1);
   header->nextfree = (uint8_t*)align_subheader(header) + pagecache_impl_SUBBLOCKSIZE;
   block->header[subindex] = (uint16_t) (1 + pageindex);
}

/* function: initunused_subheader
 * Inits header for sub-block whose pages ar all free and which is moved in an unused state.
 *
 * Parameter:
 * block     - Pointer to block which contains this sub-block.
 * subindex  - Index of the sub-block (0 == sub-block has same start address as block)
 *
 * Unchecked Precondition:
 * - header points to address of first page with index 0.
 * - All pages are free. This sub-block is no more referenced. */
static inline void initunused_subheader(block_t* block, unsigned subindex)
{
   block->pgsize[subindex] = pagesize__NROF;
   block->header[subindex] = 1;
}

// -- query

static inline int is_header(block_t* block, uint16_t subindex)
{
   return block->header[subindex] != 0;
}

static inline unsigned get_subindex(subheader_t* header)
{
   return (unsigned) (((uintptr_t)header & (pagecache_impl_BLOCKSIZE-1)) / pagecache_impl_SUBBLOCKSIZE);
}

static inline unsigned get_pageindex(freepage_t* page, pagesize_e pgsize)
{
   return (unsigned) (((uintptr_t)page & (pagecache_impl_SUBBLOCKSIZE-1)) >> log2pagesizeinbytes_pagecache(pgsize));
}

static inline subheader_t* get_header(block_t* block, uint16_t subindex, pagesize_e pgsize)
{
   uint8_t* firstpage = (uint8_t*)block + (subindex * pagecache_impl_SUBBLOCKSIZE);
   return (subheader_t*) (firstpage + ((size_t)(block->header[subindex]-1) << log2pagesizeinbytes_pagecache(pgsize)));
}

// -- update

static inline int allocpage_subheader(block_t* block, subheader_t* header, pagesize_e pgsize, /*out*/freepage_t** page)
{
   int         isLast = 0;
   freepage_t* freepage;

   if (isempty_freepagelist(&header->freepagelist)) {
      if (header->nrnextfree) {
         -- header->nrnextfree;
         freepage = (freepage_t*) header->nextfree;
         header->nextfree += pagesizeinbytes_pagecache(pgsize);
      } else {
         // use header as last page
         freepage = (freepage_t*) header;
         isLast = 1;
         block->header[get_subindex(header)] = 0; // no header
      }
   } else {
      freepage = removefirst_freepagelist(&header->freepagelist);
   }

   freepage->marker = 0;
   ++ header->nrusedpages;
   // set out
   *page = freepage;

   return isLast;
}

static inline int freepage_subheader(block_t* block, subheader_t* header, freepage_t* page)
{
   if ((uint8_t*)page >= header->nextfree)
      return EALREADY; // page unused and not on freelist
   if (block == page->marker) {
      if (page == (freepage_t*) header) {
         return EALREADY; // already stored in freepagelist
      }
      // marker indicates that it is already stored in free list
      foreach (_freepagelist, nextfreepage, &header->freepagelist) {
         if (page == nextfreepage) {
            return EALREADY; // already stored in freepagelist
         }
      }
      // false positive
   }

   insertfirst_freepagelist(&header->freepagelist, page);
   page->marker = block;
   -- header->nrusedpages;
   return 0;
}

// group: lifetime

/* function: new_block
 * Allocates a big block of memory and returns its description in <pagecache_block_t>.
 * The returned <pagecache_block_t> is allocated on the heap. */
static int new_block(/*out*/block_t **block, pagecache_impl_t *outer)
{
   int err;
   vmpage_t    pageblock;
   block_t    *newblock;

   static_assert( pagecache_impl_SUBBLOCKSIZE >= (1024*1024)
                  && pagesize_1MB + 1u == pagesize__NROF,
                  && ispowerof2_int(pagecache_impl_SUBBLOCKSIZE)
                  && ispowerof2_int(pagecache_impl_BLOCKSIZE)
                  "largest value of pagesize_e is supprted");

   if (! PROCESS_testerrortimer(&s_block_errtimer, &err)) {
      err = initaligned_vmpage(&pageblock, pagecache_impl_BLOCKSIZE);
   }
   if (err) goto ONERR;

   newblock = (block_t*) pageblock.addr;
   newblock->owner = outer;
   newblock->threadcontext = tcontext_maincontext();
   insertfirst_dlist(cast_dlist(&outer->blocklist), &newblock->blocknode);
   insertfirst_dlist(cast_dlist(&outer->unusedblocklist), &newblock->unusedblocknode);
   memset(newblock->freeblocknode, 0, sizeof(newblock->freeblocknode));
   memset(newblock->freesublist, 0, sizeof(newblock->freesublist));
   newblock->unusedsublist = (dlist_t) dlist_INIT;
   newblock->nextunused = 1;
   newblock->nrusedpages = 0;
   static_assert(sizeof(newblock->pgsize[0]) == 1, "memset with value != 0 works");
   memset(newblock->pgsize, pagesize__NROF, sizeof(newblock->pgsize));  // invalid
   memset(newblock->header, 0, sizeof(newblock->header));               // no header
   // subblock 0 contains block header
   unsigned const pageindex = (unsigned) ((sizeof(block_t)+4095) / 4096);
   unsigned const subindex  = 0;
   subheader_t* header = (subheader_t*) ((uint8_t*)newblock+((sizeof(block_t)+4095)&~(size_t)4095));
   initfree_subheader(newblock, header, subindex, pageindex, pagesize_4096);
   insertfirst_subheaderlist(&newblock->freesublist[pagesize_4096], header);
   insertfirst_dlist(cast_dlist(&outer->freeblocklist[pagesize_4096]), &newblock->freeblocknode[pagesize_4096]);
   // other subblocks need not be initialized, they are initialized on demand (allocunused_block)

   // set out values
   *block = newblock;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: delete_block
 * Deletes <block_t>. Frees up any virtual memory space.
 * */
static int delete_block(block_t *block, pagecache_impl_t *outer)
{
   int err;

   if (block && block->owner == outer) {
      block->owner = 0;

      if (isinlist_dlistnode(&block->blocknode)) {
         remove_dlist(cast_dlist(&outer->blocklist), &block->blocknode);
      }
      if (isinlist_dlistnode(&block->unusedblocknode)) {
         remove_dlist(cast_dlist(&outer->unusedblocklist), &block->unusedblocknode);
      }
      for (unsigned i=0; i<lengthof(block->freeblocknode); ++i) {
         if (isinlist_dlistnode(&block->freeblocknode[i])) {
            remove_dlist(cast_dlist(&outer->freeblocklist[i]), &block->freeblocknode[i]);
         }
      }

      vmpage_t pageblock = vmpage_INIT(pagecache_impl_BLOCKSIZE, (uint8_t*)block);

      err = free_vmpage(&pageblock);
      (void) PROCESS_testerrortimer(&s_block_errtimer, &err);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: alloc

static int releasepage_block(block_t *block, pagecache_impl_t *outer, uint16_t subidx, pagesize_e pgsize, freepage_t *page)
{
   if (is_header(block, subidx)) {
      subheader_t* header = get_header(block, subidx, pgsize);
      int err = freepage_subheader(block, header, page);
      if (err) return err;

      if (0 == header->nrusedpages) {
         // put subblock back to list of unused ones (support any pagesize_e)
         remove_subheaderlist(&block->freesublist[pgsize], header);
         if (isempty_dlist(&block->freesublist[pgsize]))
            remove_dlist(cast_dlist(&outer->freeblocklist[pgsize]), &block->freeblocknode[pgsize]);
         // move header to first page
         header = align_subheader(header);
         initunused_subheader(block, subidx);
         insertfirst_subheaderlist(&block->unusedsublist, header);
         if (!isinlist_dlist(&block->unusedblocknode))
            insertfirst_dlist(cast_dlist(&outer->unusedblocklist), &block->unusedblocknode);
      }

   } else {
      // install header on first free page
      subheader_t* header = (subheader_t*) page;
      initused_subheader(block, header, subidx, get_pageindex(page, pgsize), pgsize);
      if (header->nrusedpages) {
         // contains single free page, but subblock not in freelist
         if (isempty_subheaderlist(&block->freesublist[pgsize])) {
            insertfirst_dlist(cast_dlist(&outer->freeblocklist[pgsize]), &block->freeblocknode[pgsize]);
         }
         insertfirst_subheaderlist(&block->freesublist[pgsize], (subheader_t*)page);
      } else {
         initunused_subheader(block, subidx);
         insertfirst_subheaderlist(&block->unusedsublist, header);
         if (!isinlist_dlist(&block->unusedblocknode))
            insertfirst_dlist(cast_dlist(&outer->unusedblocklist), &block->unusedblocknode);
      }
   }

   -- block->nrusedpages;

   return 0;
}

static void allocpage_block(block_t *block, pagecache_impl_t *outer, pagesize_e pgsize, /*out*/freepage_t **page)
{
   subheader_t* header = first_subheaderlist(&block->freesublist[pgsize]);
   if (/*isLast*/ allocpage_subheader(block, header, pgsize, page)) {
      remove_subheaderlist(&block->freesublist[pgsize], header);
      if (isempty_subheaderlist(&block->freesublist[pgsize])) {
         remove_dlist(cast_dlist(&outer->freeblocklist[pgsize]), &block->freeblocknode[pgsize]);
      }
   }
   ++ block->nrusedpages;
}

static void allocunused_block(block_t *block, pagecache_impl_t *outer, pagesize_e pgsize, /*out*/freepage_t** page)
{
   subheader_t* header;
   unsigned     idx;
   if (isempty_subheaderlist(&block->unusedsublist)) {
      idx = block->nextunused++;
      header = (subheader_t*) ((uint8_t*)block + idx * pagecache_impl_SUBBLOCKSIZE);
   } else {
      header = removefirst_subheaderlist(&block->unusedsublist);
      idx = get_subindex(header);
      if (! isempty_subheaderlist(&block->unusedsublist)) {
         goto NOT_EMPTY_LIST;
      }
   }
   if (block->nextunused == pagecache_impl_NRSUBBLOCKS) {
      remove_dlist(cast_dlist(&outer->unusedblocklist), &block->unusedblocknode);
   }
   NOT_EMPTY_LIST: ;
   initfree_subheader(block, header, idx, 0, pgsize);
   if (isempty_subheaderlist(&block->freesublist[pgsize])) {
      insertfirst_dlist(cast_dlist(&outer->freeblocklist[pgsize]), &block->freeblocknode[pgsize]);
   }
   insertfirst_subheaderlist(&block->freesublist[pgsize], header);
   allocpage_block(block, outer, pgsize, page);
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

// group: lifetime

int init_pagecacheimpl(/*out*/pagecache_impl_t* pgcache)
{
   // int err;

   static_assert(lengthof(pgcache->freeblocklist) == pagesize__NROF, "every pagesize has its own free list");

   *pgcache = (pagecache_impl_t) pagecache_impl_FREE;

   return 0;
}

int free_pagecacheimpl(pagecache_impl_t* pgcache)
{
   int err = 0;

   foreach (_dlist, node, cast_dlist(&pgcache->blocklist)) {
      block_t* block = align_block(node);
      int err2 = delete_block(block, pgcache);
      if (err2) err = err2;
   }

   if (0 != pgcache->sizeallocated) {
      pgcache->sizeallocated = 0;
      err = ENOTEMPTY;
   }

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

bool isfree_pagecacheimpl(const pagecache_impl_t* pgcache)
{
   return 0 == pgcache->sizeallocated && 0 == pgcache->blocklist.last;
}

size_t sizeallocated_pagecacheimpl(const pagecache_impl_t* pgcache)
{
   return pgcache->sizeallocated;
}

// group: alloc

int allocpage_pagecacheimpl(pagecache_impl_t* pgcache, uint8_t pgsize, /*out*/struct memblock_t* page)
{
   int err;
   freepage_t *freepage;
   block_t    *block;

   VALIDATE_INPARAM_TEST(pgsize < pagesize__NROF, ONERR, );

   if (isempty_dlist(&pgcache->freeblocklist[pgsize])) {
      if (isempty_dlist(&pgcache->unusedblocklist)) {
         err = new_block(&block, pgcache);
         if (err) goto ONERR;
         if (!isempty_dlist(&pgcache->freeblocklist[pgsize])) goto USE_FREE_PAGE_LIST;
      } else {
         block = align_block(first_dlist(&pgcache->unusedblocklist));
      }
      allocunused_block(block, pgcache, pgsize, &freepage);
   } else {
      USE_FREE_PAGE_LIST: ;
      block = align_block(first_dlist(&pgcache->freeblocklist[pgsize]));
      allocpage_block(block, pgcache, pgsize, &freepage);
   }

   size_t pgsizeinbytes = pagesizeinbytes_pagecache(pgsize);
   pgcache->sizeallocated += pgsizeinbytes;

   // set out param
   *page = (memblock_t) memblock_INIT(pgsizeinbytes, (uint8_t*)freepage);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int releasepage_pagecacheimpl(pagecache_impl_t *pgcache, struct memblock_t *page)
{
   int err;

   if (!isfree_memblock(page)) {
      block_t *block  = align_block(page->addr);
      uint16_t subidx = index_subblock(page->addr);
      pagesize_e pgsize = block->pgsize[subidx];
      bool isBlockUnused = pgsize >= pagesize__NROF;
      if (  isBlockUnused || block->owner != pgcache || page->size != pagesizeinbytes_pagecache(pgsize)
            || 0 != ((uintptr_t)page->addr & (page->size-1)) ) {
         err = EINVAL;
         goto ONERR;
      }

      // support page located on freepage
      freepage_t* freepage = (freepage_t*) page->addr;
      size_t      pagesize = page->size;
      *page = (memblock_t) memblock_FREE;

      err = releasepage_block(block, pgcache, subidx, pgsize, freepage);
      if (err) goto ONERR;
      pgcache->sizeallocated -= pagesize;

      if (! block->nrusedpages && pgcache->sizeallocated) {
         err = delete_block(block, pgcache);
         if (err) goto ONERR;
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
   uint8_t const nrbits = log2_int(pagecache_impl_BLOCKSIZE);

   foreach (_dlist, node, cast_dlist(&pgcache->blocklist)) {
      dlist_node_t *n = node;
      block_t* block = align_ptr((block_t*)n, nrbits);
      if (! block->nrusedpages) {
         err = delete_block(block, pgcache);
         if (err) goto ONERR;
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_block_llhelper(void)
{
   // TEST align_block
   for (uintptr_t s=0; s<=4096; s=s?2*s:1) {
      block_t* B = (block_t*)(s*pagecache_impl_BLOCKSIZE);
      for (size_t i=0; i<pagecache_impl_BLOCKSIZE; i<<=1, ++i) {
         TEST( B == align_block((block_t*)((uintptr_t)B + i)));
      }
   }

   // TEST index_subblock
   for (uintptr_t s=0; s<pagecache_impl_NRSUBBLOCKS; ++s) {
      subheader_t* B = (subheader_t*)((s?s-1:111)*pagecache_impl_BLOCKSIZE + s*pagecache_impl_SUBBLOCKSIZE);
      for (size_t i=0; i<pagecache_impl_SUBBLOCKSIZE; i<<=1, ++i) {
         TESTP( s == index_subblock((subheader_t*)((uintptr_t)B + i)), "s:%p i:%zu", (void*)s, i);
      }
   }

   // TEST align_subheader
   for (uintptr_t s=0; s<pagecache_impl_NRSUBBLOCKS; ++s) {
      subheader_t* B = (subheader_t*)((s?s-1:111)*pagecache_impl_BLOCKSIZE + s*pagecache_impl_SUBBLOCKSIZE);
      for (size_t i=0; i<pagecache_impl_SUBBLOCKSIZE; i<<=1, ++i) {
         TESTP( B == align_subheader((subheader_t*)((uintptr_t)B + i)), "s:%p i:%zu", (void*)s, i);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_block_subhelper(void)
{
   block_t        block;
   subheader_t    header;
   freepage_t     freepage[16];

   // prepare
   memset(&block, 0, sizeof(block));
   memset(block.pgsize, pagesize__NROF, sizeof(block.pgsize));

   // -- lifetime

   // TEST initfree_subheader
   for (unsigned pagesize=pagesize__NROF-1; pagesize<pagesize__NROF; --pagesize) {
      unsigned bytes = (unsigned) pagesizeinbytes_pagecache(pagesize);
      unsigned nrpages = pagecache_impl_SUBBLOCKSIZE/bytes;
      uint8_t *nextfree = (uint8_t*)&header + (uintptr_t)bytes;
      for (unsigned subindex=0; subindex<pagecache_impl_NRSUBBLOCKS; subindex += pagecache_impl_NRSUBBLOCKS/7) {
         for (unsigned pageindex=nrpages-1; pageindex<pagecache_impl_SUBBLOCKSIZE; --pageindex) {
            memset(&header, 0, sizeof(header));
            header.freenode.next = (void*)1;
            initfree_subheader(&block, &header, subindex, pageindex, (pagesize_e) pagesize);
            // check header
            TEST( &block  == header.marker);
            TEST( &header == header.self);
            TEST( 1       == (uintptr_t)header.freenode.next); // unchanged
            TEST( 1       == isempty_dlist(&header.freepagelist));
            TEST( nrpages-1-pageindex == header.nrnextfree);
            TEST( pageindex == header.nrusedpages); // all pages up to the header are used for block_t
            TEST( nextfree == header.nextfree);
            // check block
            TEST( pagesize    == block.pgsize[subindex]);
            TEST( pageindex+1 == block.header[subindex]);
            // reset
            block.pgsize[subindex] = pagesize__NROF;
            block.header[subindex] = 0;
         }
      }
   }

   // TEST initused_subheader
   for (unsigned pagesize=pagesize__NROF-1; pagesize<pagesize__NROF; --pagesize) {
      unsigned bytes = (unsigned) pagesizeinbytes_pagecache(pagesize);
      unsigned nrpages = pagecache_impl_SUBBLOCKSIZE/bytes;
      for (unsigned subindex=0; subindex<pagecache_impl_NRSUBBLOCKS; subindex += pagecache_impl_NRSUBBLOCKS/7) {
         for (unsigned pageindex=nrpages-1; pageindex<pagecache_impl_SUBBLOCKSIZE; --pageindex) {
            memset(&header, 0, sizeof(header));
            header.freenode.next = (void*)1;
            initused_subheader(&block, &header, subindex, pageindex, (pagesize_e) pagesize);
            // check header
            TEST( &block  == header.marker);
            TEST( &header == header.self);
            TEST( 1       == (uintptr_t)header.freenode.next); // unchanged
            TEST( 1       == isempty_dlist(&header.freepagelist));
            TEST( 0       == header.nrnextfree);
            TEST( nrpages-1 == header.nrusedpages); // all pages up to the header are used for block_t
            TEST( (uint8_t*)align_subheader(&header) == header.nextfree - pagecache_impl_SUBBLOCKSIZE);
            // check block
            TEST( pagesize__NROF == block.pgsize[subindex]); // unchanged (already set with correct value by initfree)
            TEST( pageindex+1 == block.header[subindex]);
            // reset
            block.header[subindex] = 0;
         }
      }
   }

   // TEST initunused_subheader
   for (unsigned subindex=0; subindex<pagecache_impl_NRSUBBLOCKS; ++subindex) {
      block.pgsize[subindex] = (pagesize_e) subindex;
      initunused_subheader(&block, subindex);
      // check block
      TEST( pagesize__NROF == block.pgsize[subindex]);   // reset
      TEST( 1              == block.header[subindex]);   // header will be aligned and added to unused list
      // reset
      block.header[subindex] = 0;
   }

   // -- query

   // TEST is_header
   for (unsigned subindex=0; subindex<pagecache_impl_NRSUBBLOCKS; ++subindex) {
      TEST( 0 == is_header(&block, (uint16_t) subindex));
      block.header[subindex] = (uint16_t) (subindex|0x01);
      TEST( 1 == is_header(&block, (uint16_t) subindex));
      // reset
      block.header[subindex] = 0;
   }

   // TEST get_subindex
   for (uintptr_t s=0; s<pagecache_impl_NRSUBBLOCKS; ++s) {
      subheader_t* B = (subheader_t*)((s?s-1:111)*pagecache_impl_BLOCKSIZE + s*pagecache_impl_SUBBLOCKSIZE);
      for (size_t i=0; i<pagecache_impl_SUBBLOCKSIZE; i<<=1, ++i) {
         TESTP( s == get_subindex((subheader_t*)((uintptr_t)B + i)), "s:%p i:%zu", (void*)s, i);
      }
   }

   // TEST get_pageindex
   for (unsigned pagesize=pagesize__NROF-1; pagesize<pagesize__NROF; --pagesize) {
      unsigned bytes = (unsigned) pagesizeinbytes_pagecache(pagesize);
      unsigned nrpages = pagecache_impl_SUBBLOCKSIZE/bytes;
      for (uintptr_t s=0; s<pagecache_impl_NRSUBBLOCKS; ++s) {
         freepage_t* first = (freepage_t*)((s?s-1:111)*pagecache_impl_BLOCKSIZE + s*pagecache_impl_SUBBLOCKSIZE);
         for (unsigned i=nrpages-1; i<nrpages; --i) {
            TEST( i == get_pageindex((freepage_t*) ((uintptr_t)first + i*bytes), (pagesize_e) pagesize));
         }
      }
   }

   // TEST get_header
   for (unsigned pagesize=pagesize__NROF-1; pagesize<pagesize__NROF; --pagesize) {
      unsigned bytes = (unsigned) pagesizeinbytes_pagecache(pagesize);
      unsigned nrpages = pagecache_impl_SUBBLOCKSIZE/bytes;
      for (unsigned subindex=0; subindex<pagecache_impl_NRSUBBLOCKS; subindex += pagecache_impl_NRSUBBLOCKS/7) {
         for (unsigned i=nrpages-1; i<nrpages; --i) {
            subheader_t* H = (subheader_t*)((uintptr_t)&block + pagecache_impl_SUBBLOCKSIZE*subindex + i*bytes);
            block.header[subindex] = (uint16_t)(i+1);
            TEST( H == get_header(&block, (uint16_t) subindex, (pagesize_e) pagesize));
         }
         // reset
         block.header[subindex] = 0;
      }
   }

   // -- update

   // TEST allocpage_subheader: use nextfree
   TEST(get_subindex(&header) < pagecache_impl_NRSUBBLOCKS);
   TEST( 1 == isempty_dlist(&header.freepagelist));
   for (unsigned pagesize=pagesize__NROF-1; pagesize<pagesize__NROF; --pagesize) {
      unsigned bytes = (unsigned) pagesizeinbytes_pagecache(pagesize);
      unsigned subindex = get_subindex(&header);
      header.nrusedpages = 0;
      for (unsigned i=0; i<lengthof(freepage); ++i) {
         freepage_t* page = 0;
         uint8_t   * E     = (uint8_t*) &freepage[i];
         header.nextfree   = E;
         header.nrnextfree = i+1;
         block.header[subindex] = (uint16_t)-1;
         freepage[i].marker = (void*)1;
         TEST( 0/*isLast*/ == allocpage_subheader(&block, &header, pagesize, &page));
         TEST( E == (uint8_t*)page);         // returned free page
         TEST( 0 == page->marker);           // removed free page marker
         TEST( i+1 == header.nrusedpages);   // one more allocated
         TEST( i == header.nrnextfree);      // subtract one
         TEST( E+bytes == header.nextfree);  // points to start of next page
         // check block
         TEST( (uint16_t)-1 == block.header[subindex]); // not changed
         // reset
         block.header[subindex] = 0;
      }
   }

   // TEST allocpage_subheader: use header
   for (unsigned pagesize=pagesize__NROF-1; pagesize<pagesize__NROF; --pagesize) {
      freepage_t* page = 0;
      unsigned    subindex = get_subindex(&header);
      uint8_t   * E = (uint8_t*)&header;
      header.nextfree   = E+1;
      header.nrnextfree = 0;
      header.marker = (void*)1;
      block.header[subindex] = (uint16_t)-1;
      header.nrusedpages = 0;
      TEST( 1/*isLast*/ == allocpage_subheader(&block, &header, pagesize, &page));
      TEST( E == (uint8_t*)page);      // returned header as last free page
      TEST( 0 == page->marker);        // removed free page marker
      TEST( 1 == header.nrusedpages);  // one more allocated
      TEST( 0 == header.nrnextfree);   // not changed
      TEST( E+1 == header.nextfree);   // not changed
      // check block
      TEST( 0 == block.header[subindex]); // no more header
   }

   // TEST allocpage_subheader: use freelist
   header.nrnextfree   = 0;
   for (unsigned pagesize=pagesize__NROF-1; pagesize<pagesize__NROF; --pagesize) {
      header.freepagelist = (dlist_t) dlist_INIT;
      for (unsigned i=0; i<lengthof(freepage); ++i) {
         freepage[i].marker = &block;
         insertlast_freepagelist(&header.freepagelist, &freepage[i]);
      }
      header.nrusedpages = 0;
      for (unsigned i=0; i<lengthof(freepage); ++i) {
         freepage_t* page = 0;
         TEST( 0 == allocpage_subheader(&block, &header, pagesize, &page));
         TEST( &freepage[i] == page);
         TEST( i+1 == header.nrusedpages);   // one more allocated
         TEST( 0 == header.nrnextfree);      // not changed
         TEST( 0 == freepage[i].marker);
         TEST( 0 == freepage[i].next);
      }
      TEST( 1 == isempty_dlist(&header.freepagelist));
   }

   // TEST freepage_subheader
   header.marker = (block_t*) 0x123;
   header.freepagelist = (dlist_t) dlist_INIT;
   header.nrusedpages  = lengthof(freepage);
   header.nrnextfree   = 0;
   header.nextfree     = (uint8_t*) &freepage[lengthof(freepage)];
   for (unsigned i=0; i<lengthof(freepage); ++i) {
      freepage[i].marker = 0;
      freepage[i].next = 0;
      TEST( 0 == freepage_subheader((block_t*) 0x123, &header, &freepage[i]));
      TEST( &freepage[i] == first_freepagelist(&header.freepagelist)); // added to free list
      TEST( header.nrnextfree  == 0);                       // not changed
      TEST( header.nrusedpages == lengthof(freepage)-1-i);  // one less used
      TEST( freepage[i].marker == (block_t*)0x123);         // marked as free
      TEST( freepage[i].next   != 0);                       // added to a list
   }

   // TEST freepage_subheader: freepage ==> EALREADY
   for (unsigned i=0; i<lengthof(freepage); ++i) {
      TEST( EALREADY == freepage_subheader((block_t*) 0x123, &header, &freepage[i]));
      TEST( &freepage[lengthof(freepage)-1] == first_freepagelist(&header.freepagelist)); // not changed
      TEST( header.nrnextfree  == 0);                 // not changed
      TEST( header.nrusedpages == 0);                 // not changed
      TEST( freepage[i].marker == (block_t*)0x123);   // not changed
   }

   // TEST freepage_subheader: header ==> EALREADY
   TEST( EALREADY == freepage_subheader((block_t*) 0x123, &header, (freepage_t*)&header));
   TEST( &freepage[lengthof(freepage)-1] == first_freepagelist(&header.freepagelist)); // not changed
   TEST( header.marker      == (block_t*)0x123);   // not changed
   TEST( header.nrnextfree  == 0);                 // not changed
   TEST( header.nrusedpages == 0);                 // not changed

   // TEST freepage_subheader: not in range (page>=nextfree) ==> EALREADY
   TEST( EALREADY == freepage_subheader((block_t*) 0x123, &header, (freepage_t*)header.nextfree));
   TEST( &freepage[lengthof(freepage)-1] == first_freepagelist(&header.freepagelist)); // not changed
   TEST( header.nrnextfree  == 0);                 // not changed
   TEST( header.nrusedpages == 0);                 // not changed

   return 0;
ONERR:
   return EINVAL;
}

static int check_new_block(pagecache_impl_t* pgcache, block_t* block)
{
   // block
   TEST(0 != block);
   TEST(0 == ((uintptr_t)block & ((uintptr_t)pagecache_impl_BLOCKSIZE-1)));
   TEST(pgcache == block->owner);
   TEST(tcontext_maincontext() == block->threadcontext);
   TEST(isinlist_dlist(&block->blocknode));
   TEST(isinlist_dlist(&block->unusedblocknode));
   for (unsigned i=0; i<pagesize__NROF; ++i) {
      TEST( (i==pagesize_4096) == isinlist_dlist(&block->freeblocknode[i]));
      TEST( (i!=pagesize_4096) == isempty_dlist(&block->freesublist[i]));
   }
   TEST( 1 == isempty_dlist(&block->unusedsublist));
   TEST( 1 == block->nextunused);
   TEST( 0 == block->nrusedpages);
   TEST( pagesize_4096 == block->pgsize[0]);
   TEST( 2             == block->header[0]);
   for (unsigned i=1; i<pagecache_impl_NRSUBBLOCKS; ++i) {
      TEST( pagesize__NROF == block->pgsize[i]);
      TEST( 0              == block->header[i]);
   }

   // subheader_t
   subheader_t* header = (subheader_t*) ((uint8_t*)block + 4096);
   uint8_t  * nextfree = ((uint8_t*)block + 2*4096);
   TEST( block  == header->marker);
   TEST( header == header->self);
   TEST( 1 == isinlist_dlist(&header->freenode));
   TEST( header == first_subheaderlist(&block->freesublist[pagesize_4096]));
   TEST( 1 == isempty_dlist(&header->freepagelist));
   TEST( (pagecache_impl_SUBBLOCKSIZE/4096-2) == header->nrnextfree);
   TEST( 1 == header->nrusedpages);
   TEST( nextfree == header->nextfree);
   TEST( (uint8_t*)block + pagecache_impl_SUBBLOCKSIZE == header->nextfree + header->nrnextfree*4096);

   // pgcache
   TEST(block == align_block(first_dlist(&pgcache->blocklist)));
   TEST(block == align_block(first_dlist(&pgcache->unusedblocklist)));
   for (unsigned i=0; i<pagesize__NROF; ++i) {
      dlist_node_t * const node = first_dlist(&pgcache->freeblocklist[i]);
      block_t    * const fblock = align_block(node);
      if (i == pagesize_4096) {
         TEST( fblock == block);
      } else {
         TEST( fblock != block);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int check_list(pagecache_impl_t* pgcache, size_t size, block_t* block[size])
{
   size_t i=0;
   foreach (_dlist, node, cast_dlist(&pgcache->blocklist)) {
      block_t *next = align_block(node);
      TEST( next == block[size-(++i)]);
   }
   TEST(i == size);

   i=0;
   foreach (_dlist, node, cast_dlist(&pgcache->freeblocklist[pagesize_4096])) {
      block_t *next = align_block(node);
      TEST( next == block[size-(++i)]);
   }
   TEST(i == size);

   i=0;
   foreach (_dlist, node, cast_dlist(&pgcache->unusedblocklist)) {
      block_t *next = align_block(node);
      TEST( next == block[size-(++i)]);
   }
   TEST(i == size);

   return 0;
ONERR:
   return EINVAL;
}

static int check_isfree(pagecache_impl_t* pgcache)
{
   TEST( 0 == pgcache->blocklist.last);
   TEST( 0 == pgcache->unusedblocklist.last);
   for (unsigned i=0; i<lengthof(pgcache->freeblocklist); ++i) {
      TEST( 0 == pgcache->freeblocklist[i].last);
   }
   TEST( 0 == pgcache->sizeallocated);

   return 0;
ONERR:
   return EINVAL;
}

static int test_block_lifetime(void)
{
   pagecache_impl_t pgcache = pagecache_impl_FREE;
   block_t        * block[11] = {0};

   // TEST new_block
   for (unsigned i=0; i<lengthof(block); ++i) {
      TEST( 0 == new_block(&block[i], &pgcache));
      TEST( 0 == check_new_block(&pgcache, block[i]));
      TEST( 0 == pgcache.sizeallocated);
      TEST( 0 == check_list(&pgcache, i+1, block));
      // check VM
      vmpage_t pageblock = vmpage_INIT(pagecache_impl_BLOCKSIZE, (uint8_t*) block[i]);
      TEST( 1 == ismapped_vm(&pageblock, accessmode_RDWR));
   }

   // TEST delete_block
   for (unsigned i=0; i<lengthof(block); ++i) {
      TEST( 0 == delete_block(block[i], &pgcache));
      TEST( 0 == pgcache.sizeallocated);
      TEST( 0 == check_list(&pgcache, lengthof(block)-1-i, block+1+i));
      // check VM
      vmpage_t pageblock = vmpage_INIT(pagecache_impl_BLOCKSIZE, (uint8_t*) block[i]);
      TEST( 1 == isunmapped_vm(&pageblock));
   }
   TEST( 0 == check_isfree(&pgcache));

   // TEST new_block: ENOMEM
   block[0] = 0;
   init_testerrortimer(&s_block_errtimer, 1, ENOMEM);
   TEST( ENOMEM == new_block(&block[0], &pgcache));
   TEST( 0 == block[0]);
   TEST( 0 == check_isfree(&pgcache));

   // TEST delete_block: ENOMEM
   TEST(0 == new_block(&block[0], &pgcache));
   {
      vmpage_t pageblock = vmpage_INIT(pagecache_impl_BLOCKSIZE, (uint8_t*)block[0]);
      TEST(0 == isunmapped_vm(&pageblock));
      init_testerrortimer(&s_block_errtimer, 1, ENOMEM);
      TEST( ENOMEM == delete_block(block[0], &pgcache));
      TEST( 1 == isunmapped_vm(&pageblock));
      TEST( 0 == check_isfree(&pgcache));
   }

   return 0;
ONERR:
   free_pagecacheimpl(&pgcache);
   return EINVAL;
}

static dlist_node_t* get_node(dlist_t* list, unsigned i)
{
   dlist_node_t* node = first_dlist(list);
   if (node) {
      for (; i; --i) {
         node = node->next;
      }
   }
   return node;
}

typedef enum check_e {
   check_AFTER_ALLOC,
   check_AFTER_RELEASE,
   check_AFTER_ALLOCLIST,
} check_e;

static int check_unused(
         pagecache_impl_t* pgcache,
         block_t    *block,
         check_e     after,
         pagesize_e  pgsize,
         size_t      subidx,
         size_t      nextunused,
         size_t      nrusedpages)
{
   uint8_t *subblock = (uint8_t*)block + subidx*pagecache_impl_SUBBLOCKSIZE;
   bool     hasUnused = (after == check_AFTER_RELEASE) || (subidx != pagecache_impl_NRSUBBLOCKS-1);
   bool     hasFree  = (pgsize != pagesize_1MB) && (after!=check_AFTER_RELEASE || pgsize==pagesize_4096 || subidx>pagesize__NROF);

   // pgcache

   TEST( get_node(cast_dlist(&pgcache->blocklist), 0) == &block->blocknode);
   if (hasUnused) {
      TEST( get_node(cast_dlist(&pgcache->unusedblocklist), 0) == &block->unusedblocknode);
   } else {
      TEST( get_node(cast_dlist(&pgcache->unusedblocklist), 0) != &block->unusedblocknode);
   }
   if (hasFree) {
      TEST( get_node(cast_dlist(&pgcache->freeblocklist[pgsize]), 0) == &block->freeblocknode[pgsize]);
   } else {
      TEST( get_node(cast_dlist(&pgcache->freeblocklist[pgsize]), 0) != &block->freeblocknode[pgsize]);
   }
   TEST( pgcache->sizeallocated == 0); // not changed via block alloc functions

   // block

   TEST( block->owner          == pgcache);
   TEST( block->threadcontext  == tcontext_maincontext());
   TEST( block->blocknode.next != 0);
   if (hasUnused) {
      TEST( block->unusedblocknode.next != 0);
   } else {
      TEST( block->unusedblocknode.next == 0);
   }
   if (!hasFree) {
      TEST( block->freeblocknode[pgsize].next == 0);
      TEST( block->freesublist[pgsize].last   == 0);
   } else {
      TEST( block->freeblocknode[pgsize].next != 0);
      TEST( block->freesublist[pgsize].last   != 0);
      if (after == check_AFTER_RELEASE) {
         TEST( first_subheaderlist(&block->freesublist[pgsize]) != (subheader_t*)subblock);
      } else {
         TEST( first_subheaderlist(&block->freesublist[pgsize]) == (subheader_t*)subblock);
      }
   }
   if (after == check_AFTER_RELEASE) {
      TEST( first_subheaderlist(&block->unusedsublist) == (subheader_t*)subblock);
   } else {
      if (after == check_AFTER_ALLOC || !hasUnused) {
         TEST( block->unusedsublist.last == 0);
      } else {
         TEST( first_subheaderlist(&block->unusedsublist) == (subheader_t*)(subblock + pagecache_impl_SUBBLOCKSIZE));
      }
   }
   TEST( block->nextunused         == nextunused);
   TEST( block->nrusedpages        == nrusedpages);
   TEST( block->pgsize[subidx]     == (after != check_AFTER_RELEASE ? pgsize : pagesize__NROF/*unused*/));
   if (pgsize == pagesize_1MB && after != check_AFTER_RELEASE) {
      TEST( block->header[subidx]  == 0);
   } else {
      TEST( block->header[subidx]  == 1);
   }

   return 0;
ONERR:
   return EINVAL;
}

struct child_allocpage_t {
   block_t*          block;
   pagecache_impl_t* pgcache;
   pagesize_e        pgsize;
};
static int child_allocpage(void *_param)
{
   struct child_allocpage_t * param = _param;
   freepage_t* page;
   allocpage_block(param->block, param->pgcache, param->pgsize, &page);
   return 0;
}

static int check_page(
         pagecache_impl_t* pgcache,
         block_t    *block,
         freepage_t *page,
         check_e     after,
         pagesize_e  pgsize,
         size_t      subidx,
         unsigned    pagecount,
         size_t      nextunused,
         size_t      nrusedpages)
{
   size_t   nrpages  = pagecache_impl_SUBBLOCKSIZE / pagesizeinbytes_pagecache(pgsize);
   uint8_t *subblock = (uint8_t*)block + subidx*pagecache_impl_SUBBLOCKSIZE;

   if (after == check_AFTER_ALLOC) {
      bool     hasFree     = (pagecount+(subidx==0) < nrpages);
      uint8_t *expect_page = (subblock + ((hasFree ? pagecount : 0) + (subidx==0)) * pagesizeinbytes_pagecache(pgsize));
      TEST(page == (freepage_t*) expect_page);
   }

   TEST( get_node(cast_dlist(&pgcache->blocklist), 0) == &block->blocknode);
   TEST( get_node(cast_dlist(&pgcache->unusedblocklist), 0) == &block->unusedblocknode);
   if (  (after==check_AFTER_ALLOC && (pagecount+(subidx==0)) < nrpages)
      || (after==check_AFTER_RELEASE && pagecount < nrpages)
      || (pgsize == pagesize_4096 && subidx != 0)) {
      TEST( get_node(cast_dlist(&pgcache->freeblocklist[pgsize]), 0) == &block->freeblocknode[pgsize]);
   } else {
      TEST( get_node(cast_dlist(&pgcache->freeblocklist[pgsize]), 0) != &block->freeblocknode[pgsize]);
   }
   TEST( pgcache->sizeallocated == 0); // not changed via block alloc functions

   TEST( block->owner          == pgcache);
   TEST( block->threadcontext  == tcontext_maincontext());
   TEST( block->blocknode.next != 0);
   TEST( block->unusedblocknode.next != 0);
   if (  (after==check_AFTER_ALLOC && pagecount+(subidx==0) == nrpages)
      || (after==check_AFTER_RELEASE && pagecount == nrpages)) {
      if (pgsize != pagesize_4096 || subidx==0) {
         TEST( block->freeblocknode[pgsize].next == 0);
         TEST( block->freesublist[pgsize].last   == 0);
      } else {
         TEST( block->freeblocknode[pgsize].next != 0);
         TEST( first_subheaderlist(&block->freesublist[pgsize]) == (subheader_t*)((uint8_t*)block+pagecache_impl_SUBBLOCKSIZE-4096));
      }
   } else {
      TEST( block->freeblocknode[pgsize].next != 0);
      if (after == check_AFTER_ALLOC) {
         TEST( first_subheaderlist(&block->freesublist[pgsize]) == (subheader_t*)(subblock+(subidx==0?4096:0)));
      } else {
         TEST( first_subheaderlist(&block->freesublist[pgsize]) == (subheader_t*)(subblock+(pagecache_impl_SUBBLOCKSIZE - pagesizeinbytes_pagecache(pgsize))));
      }
   }
   if (after == check_AFTER_ALLOC || (after == check_AFTER_RELEASE && subidx == 0)) {
      TEST( block->unusedsublist.last == 0);
   } else {
      if (pagecount == nrpages) {
         TEST( first_subheaderlist(&block->unusedsublist) == (subheader_t*)subblock);
      } else {
         TEST( block->unusedsublist.last == 0);
      }
   }
   TEST( block->nextunused         == nextunused);
   TEST( block->nrusedpages        == nrusedpages);
   if (after == check_AFTER_ALLOC) {
      TEST( block->pgsize[subidx]  == pgsize);
   } else {
      TEST( block->pgsize[subidx]  == (pagecount < nrpages ? pgsize : pagesize__NROF/*unused*/));
   }
   if (after == check_AFTER_ALLOC && (pagecount+(subidx==0)) == nrpages) {
      TEST( block->header[subidx]  == 0);
   } else if (after == check_AFTER_RELEASE && (pagecount < nrpages)) {
      TEST( block->header[subidx]  == nrpages);
   } else {
      TEST( block->header[subidx]  == 1+(subidx==0));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_block_alloc(void)
{
   pagecache_impl_t pgcache = pagecache_impl_FREE;
   block_t        * block[2];
   freepage_t     * page;
   size_t           subidx;
   size_t           nextunused;
   size_t           nrusedpages;
   process_t        child = process_FREE;

   // prepare
   TEST(0 == new_block(&block[1], &pgcache));
   TEST(0 == new_block(&block[0], &pgcache));
   TEST(0 == check_new_block(&pgcache, block[0]));

   /* INFO:
    * 1. Check that state managing lists in block and pgcache
    *    are update to reflect any new state.
    * 2. Check pagesize_4096 which is offered by first subblock
    *    containing block_t header on page 0.
    */

   // TEST allocunused_block: use nextunused until no more unused blocks
   subidx = 1;
   nextunused = 2;
   nrusedpages = 1;
   for (unsigned i=1; i<pagecache_impl_NRSUBBLOCKS; ++i, ++subidx, ++nextunused, ++nrusedpages) {
      pagesize_e pgsize = (pagesize_e) (i % pagesize__NROF);
      uint8_t   *addr   = (uint8_t*)block[0] + i*(uintptr_t)pagecache_impl_SUBBLOCKSIZE
                        + (pagesizeinbytes_pagecache(pgsize) < pagecache_impl_SUBBLOCKSIZE ? pagesizeinbytes_pagecache(pgsize) : 0);
      page = 0;
      allocunused_block(block[0], &pgcache, pgsize, &page);
      TEST( page == (freepage_t*) addr);
      TEST( 0    == check_unused(&pgcache, block[0], check_AFTER_ALLOC, pgsize, subidx, nextunused, nrusedpages));
   }

   // TEST releasepage_block: free last used page ==> put subblock to list of unused
   subidx = pagecache_impl_NRSUBBLOCKS-1;
   nextunused = pagecache_impl_NRSUBBLOCKS;
   nrusedpages = pagecache_impl_NRSUBBLOCKS-2;
   for (unsigned i=pagecache_impl_NRSUBBLOCKS-1; i>=1; --i, --subidx, --nrusedpages) {
      pagesize_e pgsize = (pagesize_e) (i % pagesize__NROF);
      page  = (freepage_t*) ((uint8_t*)block[0] + i*(uintptr_t)pagecache_impl_SUBBLOCKSIZE
            + (pagesizeinbytes_pagecache(pgsize) < pagecache_impl_SUBBLOCKSIZE ? pagesizeinbytes_pagecache(pgsize) : 0));
      TEST(  0 == releasepage_block(block[0], &pgcache, (uint16_t)subidx, pgsize, page));
      TESTP( 0 == check_unused(&pgcache, block[0], check_AFTER_RELEASE, pgsize, subidx, nextunused, nrusedpages), "i:%d, pgsize:%d (%zd)\n", (int)i, (int)pgsize, pagesizeinbytes_pagecache(pgsize));
   }

   // TEST allocunused_block: use list of unused until no more unused blocks
   subidx = 1;
   nrusedpages = 1;
   for (unsigned i=1; i<pagecache_impl_NRSUBBLOCKS; ++i, ++subidx, ++nrusedpages) {
      pagesize_e pgsize = (pagesize_e) (i % pagesize__NROF);
      uint8_t   *addr   = (uint8_t*)block[0] + i*(uintptr_t)pagecache_impl_SUBBLOCKSIZE
                        + (pagesizeinbytes_pagecache(pgsize) < pagecache_impl_SUBBLOCKSIZE ? pagesizeinbytes_pagecache(pgsize) : 0);
      page = 0;
      allocunused_block(block[0], &pgcache, pgsize, &page);
      TEST( page == (freepage_t*) addr);
      TESTP(0    == check_unused(&pgcache, block[0], check_AFTER_ALLOCLIST, pgsize, subidx, nextunused, nrusedpages), "i:%d, pgsize:%d (%zd)\n", (int)i, (int)pgsize, pagesizeinbytes_pagecache(pgsize));
   }

   // TEST releasepage_block: free last used page ==> put subblock to list of unused
   subidx = pagecache_impl_NRSUBBLOCKS-1;
   nextunused = pagecache_impl_NRSUBBLOCKS;
   nrusedpages = pagecache_impl_NRSUBBLOCKS-2;
   for (unsigned i=pagecache_impl_NRSUBBLOCKS-1; i>=1; --i, --subidx, --nrusedpages) {
      pagesize_e pgsize = (pagesize_e) (i % pagesize__NROF);
      page  = (freepage_t*) ((uint8_t*)block[0] + i*(uintptr_t)pagecache_impl_SUBBLOCKSIZE
            + (pagesizeinbytes_pagecache(pgsize) < pagecache_impl_SUBBLOCKSIZE ? pagesizeinbytes_pagecache(pgsize) : 0));
      TEST(  0 == releasepage_block(block[0], &pgcache, (uint16_t)subidx, pgsize, page));
      TESTP( 0 == check_unused(&pgcache, block[0], check_AFTER_RELEASE, pgsize, subidx, nextunused, nrusedpages), "i:%d, pgsize:%d (%zd)\n", (int)i, (int)pgsize, pagesizeinbytes_pagecache(pgsize));
   }

   // reset / prepare
   TEST(0 == free_pagecacheimpl(&pgcache));
   TEST(0 == new_block(&block[1], &pgcache));
   TEST(0 == new_block(&block[0], &pgcache));
   TEST(0 == check_new_block(&pgcache, block[0]));

   // allocpage_block: without calling first allocunused_block ==> exception (except for preallocated first subblock)
   for (pagesize_e pgsize=pagesize_4096; pgsize<pagesize_4096+1; ++pgsize) {
      struct child_allocpage_t param = { .block = block[0], .pgcache = &pgcache, .pgsize = pgsize};
      process_result_t         result;
      TEST(0 == init_process(&child, &child_allocpage, &param, 0));
      TEST(0 == wait_process(&child, &result));
      TEST(0 == free_process(&child));
      if (pgsize == pagesize_4096) {   // first preallocated subblock
         TEST( result.returncode == 0);
         TEST( result.state == process_state_TERMINATED);
      } else {
         TEST( result.state == process_state_ABORTED);
      }
   }

   // TEST allocpage_block: first preallocated subblock (pagesize_4096)
   subidx = 0;
   nextunused = 1;
   nrusedpages = 1;
   for (unsigned i=1; i<pagecache_impl_SUBBLOCKSIZE/4096; ++i, ++nrusedpages) {
      allocpage_block(block[0], &pgcache, pagesize_4096, &page);
      TESTP( 0 == check_page(&pgcache, block[0], page, check_AFTER_ALLOC, pagesize_4096, subidx, i, nextunused, nrusedpages), "i:%u\n", i);
   }

   // TEST releasepage_block: first preallocated subblock (pagesize_4096)
   nrusedpages = pagecache_impl_SUBBLOCKSIZE/4096-2;
   for (unsigned i=1; i<pagecache_impl_SUBBLOCKSIZE/4096; ++i, --nrusedpages) {
      page = (freepage_t*) ((uint8_t*)block[0] + pagecache_impl_SUBBLOCKSIZE - i*4096);
      TEST(  0 == releasepage_block(block[0], &pgcache, (uint16_t) subidx, pagesize_4096, page));
      TESTP( 0 == check_page(&pgcache, block[0], page, check_AFTER_RELEASE, pagesize_4096, subidx, i, nextunused, nrusedpages), "i:%u\n", i);
      TEST(  EALREADY == releasepage_block(block[0], &pgcache, (uint16_t) subidx, pagesize_4096, page));
      TESTP( 0 == check_page(&pgcache, block[0], page, check_AFTER_RELEASE, pagesize_4096, subidx, i, nextunused, nrusedpages), "i:%u\n", i);
   }

   nextunused = 2;
   subidx = 1;
   for (pagesize_e pgsize=0; pgsize<pagesize__NROF; ++pgsize) {
      size_t bytes = pagesizeinbytes_pagecache(pgsize);

      // TEST allocpage_block
      nrusedpages = 1;
      allocunused_block(block[0], &pgcache, pgsize, &page);
      TEST( 0 == check_page(&pgcache, block[0], page, check_AFTER_ALLOC, pgsize, subidx, 1, nextunused, nrusedpages++));
      for (unsigned i=2; i<=pagecache_impl_SUBBLOCKSIZE/bytes; ++i, ++nrusedpages) {
         allocpage_block(block[0], &pgcache, pgsize, &page);
         TESTP( 0 == check_page(&pgcache, block[0], page, check_AFTER_ALLOC, pgsize, subidx, i, nextunused, nrusedpages), "alloc: pgsize:%d, i:%u\n", pgsize, i);
      }

      // TEST releasepage_block
      nrusedpages = pagecache_impl_SUBBLOCKSIZE/bytes-1;
      for (unsigned i=1; i<=pagecache_impl_SUBBLOCKSIZE/bytes; ++i, --nrusedpages) {
         page = (freepage_t*) ((uint8_t*)block[0] + 2*pagecache_impl_SUBBLOCKSIZE - i*bytes);
         TEST(  0 == releasepage_block(block[0], &pgcache, (uint16_t) subidx, pgsize, page));
         TESTP( 0 == check_page(&pgcache, block[0], page, check_AFTER_RELEASE, pgsize, subidx, i, nextunused, nrusedpages), "release: pgsize:%d, i:%u\n", pgsize, i);
         TEST(  EALREADY == releasepage_block(block[0], &pgcache, (uint16_t) subidx, pgsize, page));
         TESTP( 0 == check_page(&pgcache, block[0], page, check_AFTER_RELEASE, pgsize, subidx, i, nextunused, nrusedpages), "release: pgsize:%d, i:%u\n", pgsize, i);
      }

   }

   // unprepare
   TEST(0 == free_pagecacheimpl(&pgcache));

   return 0;
ONERR:
   free_pagecacheimpl(&pgcache);
   free_process(&child);
   return EINVAL;
}

static int test_initfree(void)
{
   pagecache_impl_t   pgcache = pagecache_impl_FREE;
   memblock_t         memblock;
   block_t          * block = 0;

   // TEST pagecache_impl_FREE
   TEST( 0 == check_isfree(&pgcache));

   // TEST init_pagecacheimpl
   memset(&pgcache, 255, sizeof(pgcache));
   TEST( 0 == init_pagecacheimpl(&pgcache));
   TEST( 0 == check_isfree(&pgcache));

   // TEST free_pagecacheimpl
   TEST(0 == new_block(&block, &pgcache));
   TEST( 0 == free_pagecacheimpl(&pgcache));
   TEST( 0 == check_isfree(&pgcache));
   TEST( 0 == free_pagecacheimpl(&pgcache));
   TEST( 0 == check_isfree(&pgcache));

   // TEST free_pagecacheimpl: ENOTEMPTY
   TEST(0 == init_pagecacheimpl(&pgcache));
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_16384, &memblock));
   TEST( ENOTEMPTY == free_pagecacheimpl(&pgcache));
   TEST( 0 == check_isfree(&pgcache));

   // TEST free_pagecacheimpl: ENOMEM
   for (unsigned e=1; e<=3; ++e) {
      TEST(0 == init_pagecacheimpl(&pgcache));
      for (unsigned i=0; i<3; ++i) {
         TEST(0 == new_block(&block, &pgcache));
      }
      init_testerrortimer(&s_block_errtimer, e, ENOMEM);
      TEST( ENOMEM == free_pagecacheimpl(&pgcache));
      TEST( 0 == check_isfree(&pgcache));
   }

   return 0;
ONERR:
   free_pagecacheimpl(&pgcache);
   return EINVAL;
}

static int test_query(void)
{
   pagecache_impl_t pgcache = pagecache_impl_FREE;
   block_t         *block;

   // prepare
   TEST(0 == init_pagecacheimpl(&pgcache));

   // TEST interface_pagecacheimpl
   TEST( (pagecache_it*) &s_pagecacheimpl_interface == interface_pagecacheimpl());
   TEST( s_pagecacheimpl_interface.allocpage     == &allocpage_pagecacheimpl);
   TEST( s_pagecacheimpl_interface.releasepage   == &releasepage_pagecacheimpl);
   TEST( s_pagecacheimpl_interface.sizeallocated == &sizeallocated_pagecacheimpl);
   TEST( s_pagecacheimpl_interface.emptycache    == &emptycache_pagecacheimpl);

   // TEST isfree_pagecacheimpl: sizeallocated is used
   TEST( 0 == pgcache.sizeallocated);
   TEST( 1 == isfree_pagecacheimpl(&pgcache));
   pgcache.sizeallocated = 1;
   TEST( 0 == isfree_pagecacheimpl(&pgcache));
   pgcache.sizeallocated = 0;

   // TEST isfree_pagecacheimpl: blocklist is used
   TEST( 0 == pgcache.blocklist.last);
   TEST( 1 == isfree_pagecacheimpl(&pgcache));
   TEST( 0 == new_block(&block, &pgcache));
   TEST( 0 != pgcache.blocklist.last);
   TEST( 0 == isfree_pagecacheimpl(&pgcache));
   TEST( 0 == emptycache_pagecacheimpl(&pgcache));
   TEST( 1 == isfree_pagecacheimpl(&pgcache));

   // TEST sizeallocated_pagecacheimpl
   for (size_t i = 1; i; i <<= 1) {
      pgcache.sizeallocated = i;
      TEST(i == sizeallocated_pagecacheimpl(&pgcache));
   }
   pgcache.sizeallocated = 0;
   TEST(0 == sizeallocated_pagecacheimpl(&pgcache));

   // reset
   TEST(0 == free_pagecacheimpl(&pgcache));

   return 0;
ONERR:
   free_pagecacheimpl(&pgcache);
   return EINVAL;
}

static int check_blocklist(pagecache_impl_t* pgcache, size_t expected_nrblocks, block_t* block, pagesize_e pgsize, size_t sizeallocated, bool isAlloc)
{
   size_t nrblocks = 0;
   foreach (_dlist, node, cast_dlist(&pgcache->blocklist)) {
      ++ nrblocks;
   }
   TESTP( nrblocks == expected_nrblocks, "pgcache-nrblocks:%zd expected:%zd", nrblocks, expected_nrblocks);
   TEST( block == align_block(first_dlist(&pgcache->blocklist)));
   TEST( block == align_block(first_dlist(&pgcache->unusedblocklist)));
   if (pgsize == pagesize_1MB || (!isAlloc && sizeallocated < 2*1024*1024 && pgsize != pagesize_4096)
      || (isAlloc && pgsize == pagesize_524288 && sizeallocated > 1024*1024)) {
      TEST( 0 == first_dlist(&pgcache->freeblocklist[pgsize]));
   } else {
      TEST( block == align_block(first_dlist(&pgcache->freeblocklist[pgsize])));
   }
   TEST( sizeallocated == pgcache->sizeallocated);

   return 0;
ONERR:
   return EINVAL;
}

static int test_alloc(void)
{
   pagecache_impl_t  pgcache = pagecache_impl_FREE;
   block_t         * block   = 0;
   memblock_t        page    = memblock_FREE;
   uint8_t         * log_buffer;
   size_t            log_size;

   block = 0;
   TEST(0 == init_pagecacheimpl(&pgcache));
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      size_t pagebytes  = pagesizeinbytes_pagecache(pgsize);
      size_t pageoffset = (pgsize == pagesize_4096 ? 2*pagebytes
                        : pagecache_impl_SUBBLOCKSIZE + (pgsize != pagesize_1MB ? pagebytes : 0));

      // TEST allocpage_pagecacheimpl: calls new_block
      TEST( 0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      block = block ? block : align_block(first_dlist(&pgcache.blocklist));
      TEST( page.addr == (uint8_t*)block + pageoffset);
      TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, /*sizeallocated*/pagebytes, true));
      TEST( 1 == block->nrusedpages);

      // TEST releasepage_pagecacheimpl: put back to unused list
      void ** marker = (void**) page.addr;
      TEST( 0 == *marker);       // not marked
      TEST( 0 == releasepage_pagecacheimpl(&pgcache, &page));
      TEST( block == *marker);   // marked as free
      TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, /*sizeallocated*/0, false));
      TEST( 0 == block->nrusedpages);

      // TEST releasepage_pagecacheimpl: EINVAL (EALREADY)
      GETBUFFER_ERRLOG(&log_buffer, &log_size);
      page = (memblock_t) memblock_INIT(pagebytes, (void*) marker);
      if (pgsize == pagesize_4096) {
         TEST( EALREADY == releasepage_pagecacheimpl(&pgcache, &page));
      } else {
         TEST( EINVAL == releasepage_pagecacheimpl(&pgcache, &page));
      }
      TEST( block == *marker);   // marked as free
      TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, /*sizeallocated*/0, false));
      TEST( 0 == block->nrusedpages);
      if (pgsize != 2) { TRUNCATEBUFFER_ERRLOG(log_size); }

   }

   // TEST allocpage_pagecacheimpl: every pagesize has its own block
   block = 0;
   TEST(0 == free_pagecacheimpl(&pgcache));
   TEST(0 == init_pagecacheimpl(&pgcache));
   for (size_t i=0, N=1, sizeallocated=0; i<2; ++i) {
      for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize, ++N) {
         size_t pagebytes  = pagesizeinbytes_pagecache(pgsize);
         size_t pageoffset = (pgsize == pagesize_4096 ? (2+i)*pagebytes
                           : (pgsize+1-(pgsize>pagesize_4096))*pagecache_impl_SUBBLOCKSIZE + (pgsize != pagesize_1MB ? pagebytes : 0)
                             + i*(pgsize != pagesize_524288 ? pagebytes : -pagebytes));

         sizeallocated += pagebytes;
         TEST( 0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
         block = block ? block : align_block(first_dlist(&pgcache.blocklist));
         TEST( page.addr == (uint8_t*)block + pageoffset);
         TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, sizeallocated, true));
         TEST( N == block->nrusedpages);
      }
   }

   // TEST releasepage_pagecacheimpl: release pages on multiple blocks
   for (size_t i=0, N=2*pagesize__NROF-1, sizeallocated=pgcache.sizeallocated; i<2; ++i) {
      for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize, --N) {
         size_t pagebytes  = pagesizeinbytes_pagecache(pgsize);
         size_t pageoffset = (pgsize == pagesize_4096 ? (2+i)*pagebytes
                           : (pgsize+1-(pgsize>pagesize_4096))*pagecache_impl_SUBBLOCKSIZE + (pgsize != pagesize_1MB ? pagebytes : 0)
                             + i*(pgsize != pagesize_524288 ? pagebytes : -pagebytes));

         page = (memblock_t) memblock_INIT(pagebytes, (uint8_t*)block + pageoffset);
         sizeallocated -= pagebytes;
         void ** marker = (void**) page.addr;
         TEST( 0 == *marker);       // not marked
         TEST( 0 == releasepage_pagecacheimpl(&pgcache, &page));
         TEST( block == *marker);   // marked as free
         TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, sizeallocated, false));
         TEST( N == block->nrusedpages);

         // TEST releasepage_pagecacheimpl: EALREADY (EINVAL)
         if (i==0) {
            GETBUFFER_ERRLOG(&log_buffer, &log_size);
            page = (memblock_t) memblock_INIT(pagebytes, (void*) marker);
            if (pgsize != pagesize_1MB) {
               TEST( EALREADY == releasepage_pagecacheimpl(&pgcache, &page));
            } else {
               TEST( EINVAL == releasepage_pagecacheimpl(&pgcache, &page));
            }
            TEST( block == *marker);   // marked as free
            TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, sizeallocated, false));
            TEST( N == block->nrusedpages);
            if (pgsize != 2) { TRUNCATEBUFFER_ERRLOG(log_size); }
         }
      }
   }

   TEST(0 == free_pagecacheimpl(&pgcache));
   TEST(0 == init_pagecacheimpl(&pgcache));

   // TEST allocpage_pagecacheimpl: EINVAL (size of page)
   TEST( EINVAL == allocpage_pagecacheimpl(&pgcache, (uint8_t)-1, &page));
   TEST( 0 == check_isfree(&pgcache));
   TEST( EINVAL == allocpage_pagecacheimpl(&pgcache, pagesize__NROF, &page));
   TEST( 0 == check_isfree(&pgcache));

   // TEST releasepage_pagecacheimpl: EINVAL (size of page)
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      block = align_block(page.addr);
      GETBUFFER_ERRLOG(&log_buffer, &log_size);
      for (unsigned i=0; i<4; ++i) {
         memblock_t page2 = { .addr = page.addr, .size = page.size+(i==0)-(i==1)+page.size*(i==2)-page.size/2*(i==3) };
         TEST( EINVAL == releasepage_pagecacheimpl(&pgcache, &page2));
         TEST( page.addr == page2.addr);
         TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, pagesizeinbytes_pagecache(pgsize), true));
         TEST( 1 == block->nrusedpages);
      }
      TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
      TEST(0 == check_blocklist(&pgcache, 1, block, pgsize, 0, false));
      TEST(0 == block->nrusedpages);
      TRUNCATEBUFFER_ERRLOG(log_size);
   }

   // TEST releasepage_pagecacheimpl: EINVAL (addr not aligned)
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      block = align_block(page.addr);
      GETBUFFER_ERRLOG(&log_buffer, &log_size);
      for (unsigned i=0; i<2; ++i) {
         memblock_t page2 = { .addr = page.addr+(uintptr_t)(i==0)-(uintptr_t)(i==1), .size = page.size };
         TEST( EINVAL == releasepage_pagecacheimpl(&pgcache, &page2));
         TEST( page.size == page2.size);
         TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, pagesizeinbytes_pagecache(pgsize), true));
         TEST( 1 == block->nrusedpages);
      }
      TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
      TEST(0 == check_blocklist(&pgcache, 1, block, pgsize, 0, false));
      TEST(0 == block->nrusedpages);
      TRUNCATEBUFFER_ERRLOG(log_size);
   }

   // TEST releasepage_pagecacheimpl: EALREADY (unused page (not on freelist therefore not marked))
   GETBUFFER_ERRLOG(&log_buffer, &log_size);
   for (pagesize_e pgsize = 0; pgsize < pagesize_524288; ++pgsize) {
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      memblock_t page2 = { .addr = page.addr+page.size, .size = page.size };
      TEST( EALREADY == releasepage_pagecacheimpl(&pgcache, &page2));
      TEST( 0 == page2.size);
      TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, pagesizeinbytes_pagecache(pgsize), true));
      TEST( 1 == block->nrusedpages);
      TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
      TEST(0 == check_blocklist(&pgcache, 1, block, pgsize, 0, false));
      TEST(0 == block->nrusedpages);
   }
   TRUNCATEBUFFER_ERRLOG(log_size);

   // TEST releasepage_pagecacheimpl: EINVAL (unused subblock)
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_1MB, &page));
   TEST(0 == releasepage_pagecacheimpl(&pgcache, &(memblock_t) memblock_INIT(page.size, page.addr)));
   GETBUFFER_ERRLOG(&log_buffer, &log_size);
   for (pagesize_e pgsize = 0; pgsize <= pagesize__NROF; ++pgsize) {
      memblock_t page2 = { .addr = page.addr, .size = pagesizeinbytes_pagecache(pgsize) };
      TEST( EINVAL == releasepage_pagecacheimpl(&pgcache, &page2));
      TEST( 0 == check_blocklist(&pgcache, 1, align_block(page.addr), pgsize, 0, false));
      TEST( 0 == block->nrusedpages);
   }
   TRUNCATEBUFFER_ERRLOG(log_size);

   // TEST releasepage_pagecacheimpl: EINVAL (other pgcache)
   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &page));
      block = align_block(page.addr);
      GETBUFFER_ERRLOG(&log_buffer, &log_size);
      // test
      pagecache_impl_t pgcache2 = pgcache;
      TEST( EINVAL == releasepage_pagecacheimpl(&pgcache2, &page));
      TEST( 0 == check_blocklist(&pgcache, 1, block, pgsize, pagesizeinbytes_pagecache(pgsize), true));
      TEST( 1 == block->nrusedpages);
      // reset
      TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
      TEST(0 == check_blocklist(&pgcache, 1, block, pgsize, 0, false));
      TEST(0 == block->nrusedpages);
      TRUNCATEBUFFER_ERRLOG(log_size);
   }

   // TEST releasepage_pagecacheimpl: free memblock_t located on allocated page
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_1MB, &page));
   block = align_block(page.addr);
   memblock_t* onpage = (memblock_t*) page.addr;
   *onpage = page;
   TEST( 0 == releasepage_pagecacheimpl(&pgcache, onpage));
   TEST( block == (void*) onpage->addr); // free marker set
   TEST( 0 == check_blocklist(&pgcache, 1, block, pagesize_1MB, 0, false));
   TEST( 0 == block->nrusedpages);
   // check subblock inserted into unsed list
   memblock_t page2;
   TEST( 0 == allocpage_pagecacheimpl(&pgcache, pagesize_1MB, &page2));
   TEST( page2.addr == page.addr);
   TEST( 0 == releasepage_pagecacheimpl(&pgcache, &page2));
   TEST( 0 == check_blocklist(&pgcache, 1, block, pagesize_1MB, 0, false));
   TEST( 0 == block->nrusedpages);

   TEST(0 == free_pagecacheimpl(&pgcache));
   TEST(0 == init_pagecacheimpl(&pgcache));

   // TEST allocpage_pagecacheimpl: ENOMEM
   init_testerrortimer(&s_block_errtimer, 1, ENOMEM);
   TEST( ENOMEM == allocpage_pagecacheimpl(&pgcache, pagesize_1MB, &page));
   TEST( 0 == check_isfree(&pgcache));

   // releasepage_pagecacheimpl: ENOMEM
   TEST(0 == new_block(&block, &pgcache));
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_1MB, &page));
   TEST(0 == new_block(&block, &pgcache));
   TEST(0 == allocpage_pagecacheimpl(&pgcache, pagesize_1MB, &page2));
   init_testerrortimer(&s_block_errtimer, 1, ENOMEM);
   TEST( ENOMEM == releasepage_pagecacheimpl(&pgcache, &page2));
   TEST( 1024*1024 == pgcache.sizeallocated);
   // reset
   TEST(0 == releasepage_pagecacheimpl(&pgcache, &page));
   TEST(0 == check_blocklist(&pgcache, 1, align_block(pgcache.blocklist.last), pagesize_1MB, 0, false));

   // emptycache_pagecacheimpl: ENOMEM
   init_testerrortimer(&s_block_errtimer, 1, ENOMEM);
   TEST( ENOMEM == emptycache_pagecacheimpl(&pgcache));
   TEST( 0 == check_isfree(&pgcache));

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
   memblock_t         memblock;

   for (pagesize_e pgsize = 0; pgsize < pagesize__NROF; ++pgsize) {

      // TEST emptycache_pagecacheimpl: allocated page
      TEST(0 == init_pagecacheimpl(&pgcache));
      TEST(0 == allocpage_pagecacheimpl(&pgcache, pgsize, &memblock));
      // test
      TEST( 0 == emptycache_pagecacheimpl(&pgcache));
      TEST( 0 == isempty_dlist(cast_dlist(&pgcache.blocklist)));

      // TEST emptycache_pagecacheimpl: unused block
      TEST(0 == releasepage_pagecacheimpl(&pgcache, &memblock));
      TEST(0 == isempty_dlist(cast_dlist(&pgcache.blocklist)));
      // test
      TEST( 0 == emptycache_pagecacheimpl(&pgcache));
      TEST( 1 == isempty_dlist(cast_dlist(&pgcache.blocklist)));

      // reset
      TEST(0 == free_pagecacheimpl(&pgcache));
   }

   return 0;
ONERR:
   free_pagecacheimpl(&pgcache);
   return EINVAL;
}

int unittest_memory_pagecacheimpl()
{
   if (test_block_llhelper())    goto ONERR;
   if (test_block_subhelper())   goto ONERR;
   if (test_block_lifetime())    goto ONERR;
   if (test_block_alloc())       goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_alloc())       goto ONERR;
   if (test_cache())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
