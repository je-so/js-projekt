/* title: BlockArray impl

   Implements <BlockArray>.

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

   file: C-kern/api/ds/inmem/blockarray.h
    Header file <BlockArray>.

   file: C-kern/ds/inmem/blockarray.c
    Implementation file <BlockArray impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/blockarray.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* typedef: struct datablock_t
 * Export <datablock_t> into global namespace. */
typedef struct datablock_t                datablock_t ;

/* typedef: struct ptrblock_t
 * Export <ptrblock_t> into global namespace. */
typedef struct ptrblock_t                 ptrblock_t ;



// section: Function

// group: helper

static inline int new_memoryblock(/*out*/void ** block, pagesize_e pagesize)
{
   int err ;
   memblock_t page ;

   err = ALLOC_PAGECACHE(pagesize, &page) ;
   if (err) return err ;
   memset(page.addr, 0, page.size) ;

   *block = page.addr ;
   return 0 ;
}

static inline int delete_memoryblock(void * block, size_t blocksize_in_bytes)
{
   int err ;
   memblock_t page = memblock_INIT(blocksize_in_bytes, block) ;

   err = RELEASE_PAGECACHE(&page) ;

   return err ;
}



/* struct: datablock_t
 * A memory block which contains the array elements. */
struct datablock_t {
   uint8_t elements[16/*dummy size*/] ;
} ;

// group: lifetime

static inline int new_datablock(/*out*/datablock_t ** datablock, pagesize_e pagesize)
{
   return new_memoryblock((void**)datablock, pagesize) ;
}



/* struct: ptrblock_t
 * A memory block which contains pointers to other <ptrblock_t> or <datablock_t>.
 * The current implementation supports only one memory block size which is the same
 * for <datablock_t> and <ptrblock_t>. */
struct ptrblock_t {
   void * childs[16/*dummy size*/] ;
} ;

// group: lifetime

static inline int new_ptrblock(/*out*/ptrblock_t ** ptrblock, pagesize_e pagesize)
{
   return new_memoryblock((void**)ptrblock, pagesize) ;
}



// section: blockarray_t

// group: variable
#ifdef KONFIG_UNITTEST
static test_errortimer_t      s_blockarray_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

int init_blockarray(/*out*/blockarray_t * barray, pagesize_e pagesize, uint16_t elementsize)
{
   int err ;
   void *   datablock ;
   size_t   blocksize_in_bytes = pagesizeinbytes_pagecacheit(pagesize) ;

   static_assert(pagesize_NROFPAGESIZE < 255, "fits in uint8_t") ;
   static_assert(4 == sizeof(void*) || 8 == sizeof(void*), "ptr_per_block is power of two") ;

   VALIDATE_INPARAM_TEST(pagesize < pagesize_NROFPAGESIZE, ONABORT,) ;
   VALIDATE_INPARAM_TEST(0 < elementsize && elementsize <= blocksize_in_bytes, ONABORT,) ;

   err = new_memoryblock(&datablock, pagesize);
   if (err) goto ONABORT ;

   size_t ptr_per_block = blocksize_in_bytes / sizeof(void*) ;

   barray->elements_per_block = blocksize_in_bytes / elementsize ;
   barray->root               = datablock ;
   barray->elementsize        = elementsize ;
   if (ispowerof2_int(barray->elements_per_block)) {
      barray->log2elements_per_block = (uint8_t)(1 + log2_int(barray->elements_per_block)) ;
   } else {
      barray->log2elements_per_block = 0 ;
   }
   barray->depth              = 0 ;
   barray->log2ptr_per_block  = log2_int(ptr_per_block) ;
   barray->pagesize           = (uint8_t)pagesize ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_blockarray(blockarray_t * barray)
{
   int err ;

   err = 0 ;

   if (barray->root) {
      size_t   blocksize_in_bytes = pagesizeinbytes_pagecacheit(barray->pagesize) ;
      size_t   ptr_per_block      = (size_t)1 << barray->log2ptr_per_block ;

      struct {
         ptrblock_t *   block ;
         size_t         index ;
      }        treepath[barray->depth] ;

      treepath[0].block = barray->root ;
      treepath[0].index = 0 ;

      for (uint8_t depth = 0;;) {
         // scan ptrblock
         // find non NULL child => step down
         // end scan or reached datablock => delete block => step up
         if (  depth == barray->depth /*memory block is datablock*/
               || treepath[depth].index >= ptr_per_block) {
            int err2 ;
#ifdef KONFIG_UNITTEST
            err2 = process_testerrortimer(&s_blockarray_errtimer) ;
            if (err2) err = err2 ;
#endif
            err2 = delete_memoryblock(treepath[depth].block, blocksize_in_bytes) ;
            if (err2) err = err2 ;

            if (!depth) break ;  // deleted root => done
            -- depth ;

         } else {
            void * child = treepath[depth].block->childs[treepath[depth].index] ;
            treepath[depth].index ++ ;

            if (child) {
               ++ depth ;
               treepath[depth].block = child ;
               treepath[depth].index = 0 ;
            }
         }
      }
   }

   memset(barray, 0, sizeof(*barray)) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: internal

/* function: adaptdepth_blockarray
 * Adds a new root ptrblock_t and sets <ptrblock_t.childs[0]> to old root.
 * This step is repeated until barray->depth >= depth.
 * There is always added at least one new root even if depth is <= barray->depth.
 *
 * (unchecked) Precondition:
 * o depth > barray->depth */
static int adaptdepth_blockarray(blockarray_t * barray, uint8_t depth)
{
   int err ;

   for (;;) {
      ptrblock_t * block ;
      err = new_ptrblock(&block, barray->pagesize) ;
      if (err) return err ;

      block->childs[0] = barray->root ;
      barray->root     = block ;

      ++ barray->depth ;
      if (barray->depth >= depth) break ;
   }

   return 0 ;
}

void * assign2_blockarray(blockarray_t * barray, size_t arrayindex, bool is_allocate, /*err*/int * errcode)
{
   int err ;
   size_t   blockindex ;
   size_t   elementindex ;
   unsigned shiftright = (unsigned)barray->log2ptr_per_block * barray->depth ;
   uint8_t  depth      = barray->depth ;

   if (arrayindex < barray->elements_per_block) {
      shiftright  -= shiftright ? (unsigned)barray->log2ptr_per_block : 0u ;
      blockindex   = 0 ;
      elementindex = arrayindex ;

   } else {
      if (barray->log2elements_per_block) {
         blockindex   = arrayindex >> (barray->log2elements_per_block-1) ;
         size_t  mask = (size_t)1 << (barray->log2elements_per_block-1) ;
         elementindex = arrayindex & (mask-1) ;
      } else {
         blockindex   = arrayindex / barray->elements_per_block ;
         elementindex = arrayindex % barray->elements_per_block ;
      }
      // assert: blockindex >= 1

      // set shiftright to correct value
      // and adapt depth if blockindex is too big
      while (  shiftright < bitsof(size_t)
               && blockindex >= ((size_t)1 << shiftright)) {
         ++ depth ;
         shiftright += barray->log2ptr_per_block ;
      }
      shiftright -= barray->log2ptr_per_block ;

      if (depth > barray->depth) {
         // allocate new root at correct depth level
         if (!is_allocate) goto ONNOTALLOCATE ;
         ONERROR_testerrortimer(&s_blockarray_errtimer, ONABORT) ;
         err = adaptdepth_blockarray(barray, depth) ;
         if (err) goto ONABORT ;
      }
   }

   // follow path and build blocks if needed
   datablock_t *  datablock ;
   ptrblock_t *   ptrblock ;
   if (depth) {    // barray->root points to ptrblock_t
      ptrblock = barray->root ;
      const
      size_t indexmask  = ((size_t)1 << barray->log2ptr_per_block) - 1 ;
      size_t childindex = (blockindex >> shiftright) ;

      while ( (--depth) ) {   // child is also of type ptrblock_t
         ptrblock_t * child = ptrblock->childs[childindex] ;
         if (! child) {
            // allocate new ptrblock_t
            if (!is_allocate) goto ONNOTALLOCATE ;
            ONERROR_testerrortimer(&s_blockarray_errtimer, ONABORT) ;
            err = new_ptrblock(&child, barray->pagesize) ;
            if (err) goto ONABORT ;
            ptrblock->childs[childindex] = child ;
         }
         ptrblock    = child ;
         shiftright -= barray->log2ptr_per_block ;
         childindex  = (blockindex >> shiftright) & indexmask ;
      }

      if (! ptrblock->childs[childindex]) {
         // allocate new datablock_t
         if (!is_allocate) goto ONNOTALLOCATE ;
         ONERROR_testerrortimer(&s_blockarray_errtimer, ONABORT) ;
         err = new_datablock((datablock_t**)&ptrblock->childs[childindex], barray->pagesize) ;
         if (err) goto ONABORT ;
      }

      datablock = ptrblock->childs[childindex] ;

   } else {
      // is always allocated in init_blockarray
      datablock = barray->root ;
   }

   return &datablock->elements[elementindex * barray->elementsize] ;
ONNOTALLOCATE:
   if (errcode) *errcode = ENODATA ;
   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   if (errcode) *errcode = err ;
   return 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_helpertypes(void)
{
   ptrblock_t *   ptrblock  = 0 ;
   datablock_t *  datablock = 0 ;
   size_t         blocksize = 0 ;

   // TEST new_ptrblock, delete_memoryblock
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      blocksize = pagesizeinbytes_pagecacheit(pgsize) ;
      TEST(blocksize >= 256) ;
      TEST(ispowerof2_int(blocksize)) ;

      size_t oldsize = SIZEALLOCATED_PAGECACHE() ;
      TEST(0 == new_ptrblock(&ptrblock, pgsize)) ;
      TEST(0 != ptrblock) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + blocksize) ;
      for (size_t i = 0 ; i < blocksize; ++i) {
         TEST(0 == ((uint8_t*)ptrblock)[i]) ;
      }

      TEST(0 == delete_memoryblock(ptrblock, blocksize)) ;
      ptrblock = 0 ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize) ;
   }

   // TEST new_datablock, delete_memoryblock
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      blocksize = pagesizeinbytes_pagecacheit(pgsize) ;

      size_t oldsize = SIZEALLOCATED_PAGECACHE() ;

      TEST(0 == new_datablock(&datablock, pgsize)) ;
      TEST(0 != datablock) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + blocksize) ;
      for (size_t i = 0 ; i < blocksize; ++i) {
         TEST(0 == ((uint8_t*)datablock)[i]) ;
      }

      TEST(0 == delete_memoryblock(datablock, blocksize)) ;
      datablock = 0 ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize) ;
   }

   return 0 ;
ONABORT:
   if (ptrblock) {
      delete_memoryblock(ptrblock, blocksize) ;
   }
   if (datablock) {
      delete_memoryblock(datablock, blocksize) ;
   }
   return EINVAL ;
}

/* function: build_test_node
 * Allocates a tree with pow(2, depth+1)-1 nodes.
 * All nodes at a depth > 0 are allocated as <ptrblock_t>
 * and all nodes at depth == 0 are allocated as <datablock_t>.
 * A <ptrblock_t> contains two childs at index 0 and maxindex. */
static int build_test_node(void ** block, pagesize_e pgsize, uint8_t depth)
{
   if (0 == depth) {
      datablock_t * datablock ;
      TEST(0 == new_datablock(&datablock, pgsize)) ;
      *block = datablock ;

   } else {
      ptrblock_t * ptrblock ;
      TEST(0 == new_ptrblock(&ptrblock, pgsize)) ;
      *block = ptrblock ;

      void * child ;
      TEST(0 == build_test_node(&child, pgsize, (uint8_t)(depth-1))) ;
      ptrblock->childs[0] = child ;
      TEST(0 == build_test_node(&child, pgsize, (uint8_t)(depth-1))) ;
      size_t maxindex = pagesizeinbytes_pagecacheit(pgsize)/sizeof(void*)-1 ;
      ptrblock->childs[maxindex] = child ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

/* function: build_test_tree
 * Calls <build_test_node> to build the test tree.
 * The old root is expected at depth 0 and it is deallocated and replaced
 * by the new root returned from build_test_node.
 * The depth is changed. */
static int build_test_tree(blockarray_t * barray, pagesize_e pgsize, uint8_t depth)
{
   TEST(0 == barray->depth) ;
   TEST(0 == delete_memoryblock(barray->root, pagesizeinbytes_pagecacheit(pgsize))) ;
   barray->root  = 0 ;
   barray->depth = depth ;
   TEST(0 == build_test_node(&barray->root, pgsize, depth)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   blockarray_t barray = blockarray_INIT_FREEABLE ;

   // TEST blockarray_INIT_FREEABLE
   TEST(0 == barray.elements_per_block) ;
   TEST(0 == barray.root) ;
   TEST(0 == barray.elementsize) ;
   TEST(0 == barray.log2elements_per_block) ;
   TEST(0 == barray.depth) ;
   TEST(0 == barray.log2ptr_per_block) ;
   TEST(0 == barray.pagesize) ;

   // TEST init_blockarray, free_blockarray: elementsize not power of two
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      size_t oldsize   = SIZEALLOCATED_PAGECACHE() ;
      size_t blocksize = pagesizeinbytes_pagecacheit(pgsize) ;

      barray.depth = 1 ;
      barray.log2elements_per_block = 1 ;
      TEST(0 == init_blockarray(&barray, pgsize, (uint16_t)(3+4*pgsize))) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + blocksize) ;
      TEST(barray.elements_per_block     == blocksize / (3+4*pgsize)) ;
      TEST(barray.root                   != 0) ;
      TEST(barray.elementsize            == 3+4*pgsize) ;
      TEST(barray.log2elements_per_block == 0) ;
      TEST(barray.depth                  == 0) ;
      TEST(blocksize / sizeof(void*)     == (size_t)1 << barray.log2ptr_per_block) ;
      TEST(barray.pagesize               == pgsize) ;

      TEST(0 == free_blockarray(&barray)) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize) ;
      TEST(0 == barray.elements_per_block) ;
      TEST(0 == barray.root) ;
      TEST(0 == barray.elementsize) ;
      TEST(0 == barray.log2elements_per_block) ;
      TEST(0 == barray.depth) ;
      TEST(0 == barray.log2ptr_per_block) ;
      TEST(0 == barray.pagesize) ;
      TEST(0 == free_blockarray(&barray)) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize) ;
   }

   // TEST init_blockarray, free_blockarray: elementsize power of two
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      size_t oldsize   = SIZEALLOCATED_PAGECACHE() ;
      size_t blocksize = pagesizeinbytes_pagecacheit(pgsize) ;

      barray.depth = 1 ;
      TEST(0 == init_blockarray(&barray, pgsize, 32)) ;
      TEST(barray.elements_per_block     == blocksize / 32) ;
      TEST(barray.root                   != 0) ;
      TEST(barray.elementsize            == 32) ;
      TEST(barray.log2elements_per_block == 1 + log2_int(blocksize) - 5) ;
      TEST(barray.depth                  == 0) ;
      TEST(blocksize / sizeof(void*)     == (size_t)1 << barray.log2ptr_per_block) ;
      TEST(barray.pagesize               == pgsize) ;

      TEST(0 == free_blockarray(&barray)) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize) ;
      TEST(0 == barray.elements_per_block) ;
      TEST(0 == barray.root) ;
      TEST(0 == barray.elementsize) ;
      TEST(0 == barray.log2elements_per_block) ;
      TEST(0 == barray.depth) ;
      TEST(0 == barray.log2ptr_per_block) ;
      TEST(0 == barray.pagesize) ;
   }

   // TEST init_blockarray: EINVAL
   TEST(EINVAL == init_blockarray(&barray, -1, 16)) ;
   TEST(EINVAL == init_blockarray(&barray, pagesize_NROFPAGESIZE, 16)) ;
   TEST(EINVAL == init_blockarray(&barray, pagesize_16384, 0)) ;
   TEST(EINVAL == init_blockarray(&barray, pagesize_16384, 16385)) ;

   // TEST init_blockarray, free_blockarray: one per page, free whole tree
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      size_t oldsize   = SIZEALLOCATED_PAGECACHE() ;
      size_t blocksize = pagesizeinbytes_pagecacheit(pgsize) ;
      uint16_t elemsize = (uint16_t) (blocksize < UINT16_MAX ? blocksize : 32768) ;

      TEST(0 == init_blockarray(&barray, pgsize, elemsize)) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + blocksize) ;
      TEST(barray.elements_per_block     == blocksize / elemsize) ;
      TEST(barray.root                   != 0) ;
      TEST(barray.elementsize            == elemsize) ;
      TEST(barray.log2elements_per_block == 1 + log2_int(blocksize) -log2_int(elemsize)) ;
      TEST(barray.depth                  == 0) ;
      TEST(blocksize / sizeof(void*)     == (size_t)1 << barray.log2ptr_per_block) ;
      TEST(barray.pagesize               == pgsize) ;

      // build whole tree and free all pages
      TEST(0 == build_test_tree(&barray, pgsize, 5)) ;
      TEST(0 == free_blockarray(&barray)) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize) ;
      TEST(0 == barray.elements_per_block) ;
      TEST(0 == barray.root) ;
      TEST(0 == barray.elementsize) ;
      TEST(0 == barray.log2elements_per_block) ;
      TEST(0 == barray.depth) ;
      TEST(0 == barray.log2ptr_per_block) ;
      TEST(0 == barray.pagesize) ;
   }

   // TEST free_blockarray: EFAULT
   for (unsigned i = 1; i <= 15; ++i) {
      TEST(0 == init_blockarray(&barray, pagesize_1024, 128)) ;
      TEST(0 == build_test_tree(&barray, pagesize_1024, 3)) ;
      init_testerrortimer(&s_blockarray_errtimer, i, EFAULT) ;
      TEST(EFAULT == free_blockarray(&barray)) ;
      TEST(0 == barray.elements_per_block) ;
      TEST(0 == barray.root) ;
      TEST(0 == barray.elementsize) ;
      TEST(0 == barray.log2elements_per_block) ;
      TEST(0 == barray.depth) ;
      TEST(0 == barray.log2ptr_per_block) ;
      TEST(0 == barray.pagesize) ;
   }

   return 0 ;
ONABORT:
   free_blockarray(&barray) ;
   return EINVAL ;
}

static int test_internal(void)
{
   blockarray_t barray = blockarray_INIT_FREEABLE ;

   // TEST adaptdepth_blockarray
   for (uint8_t d = 1; d < 64; ++d) {
      TEST(0 == init_blockarray(&barray, pagesize_256, 256)) ;
      TEST(0 == barray.depth) ;
      void * oldroot = barray.root ;
      TEST(0 == adaptdepth_blockarray(&barray, d)) ;
      TEST(d == barray.depth) ;
      TEST(oldroot != barray.root) ;
      void * block = barray.root ;
      for (unsigned i = d; i > 0; --i) {
         TEST(0 != ((ptrblock_t*)block)->childs[0]) ;
         TEST(0 == ((ptrblock_t*)block)->childs[1]) ;
         block = ((ptrblock_t*)block)->childs[0] ;
      }
      TEST(oldroot == block) ;
      TEST(0 == free_blockarray(&barray)) ;
   }

   // TEST adaptdepth_blockarray: add always new root even if parameter depth <= barray.depth
   TEST(0 == init_blockarray(&barray, pagesize_65536, 256)) ;
   void * oldroot = barray.root ;
   TEST(0 == adaptdepth_blockarray(&barray, 0)) ;
   TEST(1 == barray.depth) ;  // one layer added
   TEST(oldroot != barray.root) ;
   TEST(oldroot == ((ptrblock_t*)barray.root)->childs[0]) ;
   TEST(0 == free_blockarray(&barray)) ;

   // TEST assign2_blockarray: first block
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      size_t blocksize = pagesizeinbytes_pagecacheit(pgsize) ;
      // test elemsize power of two and not
      uint16_t maxelemsize = (uint16_t) (blocksize <= UINT16_MAX ? blocksize : UINT16_MAX) ;
      uint16_t elemsize[5] = { 1, 2, (uint16_t)(maxelemsize/2-1), (uint16_t)(maxelemsize-1), maxelemsize } ;
      for (size_t sizeindex = 0; sizeindex < lengthof(elemsize); ++sizeindex) {
         TEST(0 == init_blockarray(&barray, pgsize, elemsize[sizeindex])) ;
         uint8_t * block = barray.root ;
         TEST(barray.elements_per_block == blocksize / elemsize[sizeindex]) ;
         for (size_t i = 0; i < barray.elements_per_block; ++i) {
            // no allocation wanted
            uint8_t * elem = assign2_blockarray(&barray, i, false, 0) ;
            TEST(elem == block + i*elemsize[sizeindex]) ;
            // no allocation needed
            TEST(elem == assign2_blockarray(&barray, i, true, 0)) ;
         }
         // increase depth (repeat same test)
         TEST(0 == adaptdepth_blockarray(&barray, 3)) ;
         for (size_t i = 0; i < barray.elements_per_block; ++i) {
            // no allocation wanted
            uint8_t * elem = assign2_blockarray(&barray, i, false, 0) ;
            TEST(elem == block + i*elemsize[sizeindex]) ;
            // no allocation needed
            TEST(elem == assign2_blockarray(&barray, i, true, 0)) ;
         }
         TEST(0 == free_blockarray(&barray)) ;
      }
   }

   // TEST assign2_blockarray: test hierarchy
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      size_t blocksize = pagesizeinbytes_pagecacheit(pgsize) ;
      // test elemsize power of two and not
      uint16_t maxelemsize = (uint16_t) (blocksize <= UINT16_MAX ? blocksize : UINT16_MAX) ;
      uint16_t elemsize[5] = { 1, 2, (uint16_t)(maxelemsize/2-1), (uint16_t)(maxelemsize-1), maxelemsize } ;
      for (size_t sizeindex = 0; sizeindex < lengthof(elemsize); ++sizeindex) {
         size_t nreleminblock = blocksize / elemsize[sizeindex] ;
         size_t nrptrinblock  = blocksize / sizeof(void*) ;
         size_t maxblockindex = SIZE_MAX  / nreleminblock ;
         size_t maxdepth      = (maxblockindex > 0) ;
         for (size_t i = maxblockindex; i >= nrptrinblock; i /= nrptrinblock) {
            ++ maxdepth ;
         }
         size_t maxpath = (size_t)1 << maxdepth ;
         for (size_t path = 0, elemindex = 0; path < maxpath; ++path, ++elemindex) {
            if (elemindex >= nreleminblock) elemindex = 0 ;
            // index is a variation of (000011110000) where the number of bits in one block is log2_int(nrptrinblock)
            size_t blockindex  = 0 ;
            size_t expectdepth = 0 ;
            for (size_t depth = maxdepth; (depth--) > 0; ) {
               blockindex *= nrptrinblock ;
               if (0 != (path & ((size_t)1 << depth))) {
                  blockindex += nrptrinblock - 1 ;
                  if (!expectdepth) expectdepth = depth + 1 ;
               }
            }
            if (blockindex > maxblockindex) blockindex = maxblockindex ;
            size_t pathindex[1+maxdepth] ;
            for (size_t depth = 0, i = blockindex; depth < maxdepth; ++depth) {
               pathindex[depth] = i % nrptrinblock ;
               i /= nrptrinblock ;
            }
            const size_t arrayindex = blockindex * nreleminblock + elemindex ;
            TEST(0 == init_blockarray(&barray, pgsize, elemsize[sizeindex])) ;
            uint8_t * elem = assign2_blockarray(&barray, arrayindex, true, 0) ;
            TEST(0 != elem) ;
            TEST(expectdepth == barray.depth) ;
            ptrblock_t *  ptrblock = barray.root ;
            TEST(ptrblock) ;
            for (size_t depth = expectdepth; (depth--) > 0;) {
               ptrblock = ptrblock->childs[pathindex[depth]] ;
               TEST(ptrblock) ;
            }
            datablock_t * datablock = (datablock_t*) ptrblock ;   // last is of type data
            TEST(elem == &datablock->elements[elemsize[sizeindex] * elemindex]) ;
            // only reading
            TEST(elem == assign2_blockarray(&barray, arrayindex, false, 0)) ;
            // no need for allocation
            TEST(elem == assign2_blockarray(&barray, arrayindex, true, 0)) ;
            TEST(0 == free_blockarray(&barray)) ;
         }
      }
   }

   // TEST assign2_blockarray: ENODATA reading returns NULL (all possible allocation points)
   int errcode = 0 ;
   TEST(0 == init_blockarray(&barray, pagesize_256, 1)) ;
   // no root block at correct depth level
   TEST(0 == assign2_blockarray(&barray, 256, false, &errcode)) ;
   TEST(errcode == ENODATA) ;
   // no datablock
   TEST(0 == adaptdepth_blockarray(&barray, 1)) ;
   TEST(0 == assign2_blockarray(&barray, 256, false, &errcode)) ;
   TEST(errcode == ENODATA) ;
   // no ptrblock
   TEST(0 == adaptdepth_blockarray(&barray, 2)) ;
   TEST(0 == assign2_blockarray(&barray, 256*(256/sizeof(void*)), false, &errcode)) ;
   TEST(errcode == ENODATA) ;
   TEST(0 == free_blockarray(&barray)) ;

   // TEST assign2_blockarray: ENOMEM (all possible allocation points)
   errcode = 0 ;
   TEST(0 == init_blockarray(&barray, pagesize_256, 1)) ;
   // ENOMEM: no root block at correct depth level
   init_testerrortimer(&s_blockarray_errtimer, 1, ENOMEM) ;
   TEST(0 == assign2_blockarray(&barray, 256, true, &errcode)) ;
   TEST(0 == barray.depth) ;
   TEST(errcode == ENOMEM) ;
   // ENOMEM: no datablock
   TEST(0 == adaptdepth_blockarray(&barray, 1)) ;
   init_testerrortimer(&s_blockarray_errtimer, 1, ENOMEM) ;
   TEST(0 == assign2_blockarray(&barray, 256, true, &errcode)) ;
   TEST(1 == barray.depth) ;
   TEST(0 == ((ptrblock_t*)barray.root)->childs[1]) ;
   // check that another call is positive
   TEST(0 != assign2_blockarray(&barray, 256, true, &errcode)) ;
   TEST(1 == barray.depth) ;
   TEST(0 != ((ptrblock_t*)barray.root)->childs[1]) ;
   TEST(errcode == ENOMEM) ;
   // ENOMEM: no ptrblock
   TEST(0 == adaptdepth_blockarray(&barray, 2)) ;
   init_testerrortimer(&s_blockarray_errtimer, 1, ENOMEM) ;
   TEST(0 == assign2_blockarray(&barray, 256*(256/sizeof(void*)), true, &errcode)) ;
   TEST(2 == barray.depth) ;
   TEST(0 == ((ptrblock_t*)barray.root)->childs[1]) ;
   TEST(errcode == ENOMEM) ;
   // check that another call is positive
   TEST(0 != assign2_blockarray(&barray, 256*(256/sizeof(void*)), true, &errcode)) ;
   TEST(2 == barray.depth) ;
   TEST(0 != ((ptrblock_t*)barray.root)->childs[1]) ;
   TEST(0 == free_blockarray(&barray)) ;

   return 0 ;
ONABORT:
   free_blockarray(&barray) ;
   return EINVAL ;
}

static int test_read(void)
{
   blockarray_t   barray = blockarray_INIT_FREEABLE ;
   uint16_t       elemsize[]   = { 1, 3, 4, 8, 12, 16, 24, 30, 32, 55 } ;

   // TEST at_blockarray
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      size_t blocksize    = pagesizeinbytes_pagecacheit(pgsize) ;
      for (size_t sizeindex = 0; sizeindex < lengthof(elemsize); ++sizeindex) {
         size_t nreleminblock = blocksize / elemsize[sizeindex] ;
         size_t maxblockindex = SIZE_MAX / nreleminblock ;
         size_t blockindex[]  = { 0, 1, 2, 3, maxblockindex-3, maxblockindex-1, maxblockindex } ;
         size_t elemindex[]   = { 0, 1, nreleminblock/2, nreleminblock-2, nreleminblock-1 } ;
         for (size_t bi = 0; bi < lengthof(blockindex); ++bi) {
            TEST(0 == init_blockarray(&barray, pgsize, elemsize[sizeindex])) ;
            size_t arrayindex = blockindex[bi] * nreleminblock ;
            uint8_t * elem = assign2_blockarray(&barray, arrayindex, true, 0) ;
            TEST(elem != 0) ;
            for (size_t ei = 0; ei < lengthof(elemindex); ++ei) {
               size_t offset = elemindex[ei] ;
               if (SIZE_MAX - arrayindex < offset) {
                  offset = SIZE_MAX - arrayindex - (lengthof(elemindex)-1) + ei ;
                  if (SIZE_MAX - arrayindex < offset) continue /*SIZE_MAX - arrayindex + ei < (lengthof(elemindex)-1)*/ ;
               }
               uint8_t * elem2 = at_blockarray(&barray, arrayindex + offset) ;
               TEST(elem2 == elem + offset * elemsize[sizeindex]) ;
            }
            TEST(0 == free_blockarray(&barray)) ;
         }
      }
   }

   // TEST assign_blockarray: returns 0 in case no allocation was made
   TEST(0 == init_blockarray(&barray, pagesize_256, 1)) ;
   TEST(0 == at_blockarray(&barray, 256)) ;
   TEST(0 == barray.depth) ;
   TEST(0 == free_blockarray(&barray)) ;

   return 0 ;
ONABORT:
   free_blockarray(&barray) ;
   return EINVAL ;
}

static int test_update(void)
{
   blockarray_t   barray = blockarray_INIT_FREEABLE ;
   uint16_t       elemsize[]   = { 1, 3, 4, 8, 12, 16, 24, 30, 32, 55 } ;

   // TEST assign_blockarray
   for (pagesize_e pgsize = 0; pgsize < pagesize_NROFPAGESIZE; ++pgsize) {
      size_t blocksize    = pagesizeinbytes_pagecacheit(pgsize) ;
      for (size_t sizeindex = 0; sizeindex < lengthof(elemsize); ++sizeindex) {
         size_t nreleminblock = blocksize / elemsize[sizeindex] ;
         size_t maxblockindex = SIZE_MAX / nreleminblock ;
         size_t blockindex[]  = { 0, 1, 2, 3, maxblockindex-3, maxblockindex-1, maxblockindex } ;
         size_t elemindex[]   = { 0, 1, nreleminblock/2, nreleminblock-2, nreleminblock-1 } ;
         for (size_t bi = 0; bi < lengthof(blockindex); ++bi) {
            TEST(0 == init_blockarray(&barray, pgsize, elemsize[sizeindex])) ;
            size_t arrayindex = blockindex[bi] * nreleminblock ;
            uint8_t * elem = assign_blockarray(&barray, arrayindex, 0) ;
            TEST(elem == assign2_blockarray(&barray, arrayindex, true, 0)) ;
            TEST(elem != 0) ;
            for (size_t ei = 0; ei < lengthof(elemindex); ++ei) {
               size_t offset = elemindex[ei] ;
               if (SIZE_MAX - arrayindex < offset) {
                  offset = SIZE_MAX - arrayindex - (lengthof(elemindex)-1) + ei ;
                  if (SIZE_MAX - arrayindex < offset) continue /*SIZE_MAX - arrayindex + ei < (lengthof(elemindex)-1)*/ ;
               }
               uint8_t * elem2 = assign_blockarray(&barray, arrayindex + offset, 0) ;
               TEST(elem2 == elem + offset * elemsize[sizeindex]) ;
            }
            TEST(0 == free_blockarray(&barray)) ;
         }
      }
   }

   // TEST assign_blockarray: ENOMEM
   int errcode = 0 ;
   TEST(0 == init_blockarray(&barray, pagesize_256, 1)) ;
   init_testerrortimer(&s_blockarray_errtimer, 1, ENOMEM) ;
   TEST(0 == assign_blockarray(&barray, 256, &errcode)) ;
   TEST(0 == barray.depth) ;
   TEST(errcode == ENOMEM) ;
   TEST(0 == free_blockarray(&barray)) ;

   return 0 ;
ONABORT:
   free_blockarray(&barray) ;
   return EINVAL ;
}

typedef struct test_t      test_t ;

struct test_t {
   size_t i ;
} ;

// TEST blockarray_IMPLEMENT
blockarray_IMPLEMENT(_testarray, test_t)

static int test_generic(void)
{
   blockarray_t barray = blockarray_INIT_FREEABLE ;

   // TEST init_blockarray
   TEST(0 == init_testarray(&barray, pagesize_256)) ;
   TEST(barray.elements_per_block == 256 / sizeof(test_t)) ;
   TEST(barray.root               != 0) ;
   TEST(barray.elementsize        == sizeof(test_t)) ;
   TEST(barray.pagesize           == pagesize_256) ;

   // TEST free_blockarray
   TEST(0 == free_testarray(&barray)) ;
   TEST(0 == barray.elements_per_block) ;
   TEST(0 == barray.root) ;
   TEST(0 == barray.elementsize) ;
   TEST(0 == barray.pagesize) ;

   // TEST at_blockarray
   TEST(0 == init_testarray(&barray, pagesize_16384)) ;
   uint8_t * data = barray.root ;
   for (size_t i = 0 ; i < barray.elements_per_block; ++i) {
      TEST(at_blockarray(&barray, i) == data + i * sizeof(test_t)) ;
   }
   TEST(0 == at_blockarray(&barray, barray.elements_per_block)) ;
   TEST(data == barray.root) ;

   // TEST assign_blockarray
   for (size_t i = 0 ; i < barray.elements_per_block; ++i) {
      TEST(assign_blockarray(&barray, i, 0) == data + i * sizeof(test_t)) ;
   }
   TEST(data == barray.root) ;
   TEST(0 != assign_blockarray(&barray, barray.elements_per_block, 0)) ;
   TEST(data != barray.root) ;
   TEST(data == ((ptrblock_t*)barray.root)->childs[0]) ;
   uint8_t * data2 = ((ptrblock_t*)barray.root)->childs[1] ;
   TEST(data2 == assign_blockarray(&barray, barray.elements_per_block, 0)) ;

   // unprepare
   TEST(0 == free_testarray(&barray)) ;

   return 0 ;
ONABORT:
   free_blockarray(&barray) ;
   return EINVAL ;
}

int unittest_ds_inmem_blockarray()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   RELEASECACHED_PAGECACHE() ;
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_helpertypes())    goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_internal())       goto ONABORT ;
   if (test_read())           goto ONABORT ;
   if (test_update())         goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   RELEASECACHED_PAGECACHE() ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
