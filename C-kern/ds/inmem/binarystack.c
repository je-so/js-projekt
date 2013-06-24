/* title: BinaryStack impl
   Implements <BinaryStack>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/binarystack.h
    Header file <BinaryStack>.

   file: C-kern/ds/inmem/binarystack.c
    Implementation file <BinaryStack impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/binarystack.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


typedef struct blockheader_t           blockheader_t ;

/* struct: blockheader_t
 * Header information of every allocated block.
 * This structure links a header to a previously allocated block.
 * And it contains some book-keeping information.
 * */
struct blockheader_t {
   /* variable: next
    * Points to previously allocated block. */
   blockheader_t *   next ;
   /* variable: size
    * Size in bytes of this block.
    * The start address in memory is the same as <blockheader_t>. */
   uint32_t          size ;
   /* variable: usedsize
    * The size of pushed objects stored in this block. */
   uint32_t          usedsize ;
} ;

// group: lifetime

static inline void init_blockheader(/*out*/blockheader_t * header, uint32_t size, blockheader_t * next)
{
   header->next     = next ;
   header->size     = size ;
   header->usedsize = 0 ;
}

// group: query

/* function: blocksize_blockheader
 * Returns the size of the allocated block usable pushing objects. */
static inline uint32_t blocksize_blockheader(blockheader_t * header)
{
   return header->size - sizeof(blockheader_t) ;
}

/* function: usedsize_blockheader
 * Returns the number bytes used by pushed objects. */
static inline uint32_t usedsize_blockheader(blockheader_t * header)
{
   return header->usedsize ;
}

/* function: freesize_blockheader
 * Returns the number of unused bytes. */
static inline uint32_t freesize_blockheader(blockheader_t * header)
{
   return header->size - sizeof(blockheader_t) - header->usedsize ;
}

/* function: blockstart_blockheader
 * Returns the start address where pushed objects are stored. */
static inline uint8_t * blockstart_blockheader(blockheader_t * header)
{
   return ((uint8_t*)header) + sizeof(blockheader_t) ;
}

/* function: header_blockheader
 * Inverse operation to <blockstart_blockheader>.
 * Use it to get the address of <blockheader_t> computed from the value return from <blockstart_blockheader>. */
static inline blockheader_t * header_blockheader(uint8_t * blockstart)
{
   return (blockheader_t *) (blockstart - sizeof(blockheader_t)) ;
}

/* function: headersize_blockheader
 * Returns the size which needs to be allocated additional. */
static inline uint32_t headersize_blockheader(void)
{
   return sizeof(blockheader_t) ;
}


// section: binarystack_t

// group: test

#ifdef KONFIG_UNITTEST
/* variable: s_binarystack_errtimer
 *  * Simulates an error in <allocateblock_binarystack> and <freeblock_binarystack>. */
static test_errortimer_t   s_binarystack_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: helper

/* function: allocateblock_binarystack
 * Allocates a block of memory.
 * The newly allocated block is the new first entry in the list of allocated blocks.
 * The stack variables are adapted. */
static int allocateblock_binarystack(binarystack_t * stack, uint32_t size)
{
   int err ;
   blockheader_t  * header ;

   memblock_t  mem ;

   ONERROR_testerrortimer(&s_binarystack_errtimer, ONABORT) ;

   if (size <= 65536-headersize_blockheader()) {
      err = ALLOC_PAGECACHE(pagesize_65536, &mem) ;
      if (err) goto ONABORT ;
   } else if (size <= 1024*1024-headersize_blockheader()) {
      err = ALLOC_PAGECACHE(pagesize_1MB, &mem) ;
      if (err) goto ONABORT ;
   } else {
      return E2BIG ;
   }

   blockheader_t * oldheader ;
   if (stack->blockstart) {
      oldheader = header_blockheader(stack->blockstart) ;
      oldheader->usedsize = stack->blocksize - stack->freeblocksize ;
   } else {
      oldheader = 0 ;
   }

   header = (blockheader_t *) mem.addr ;
   init_blockheader(header, (uint32_t) mem.size, oldheader) ;

   stack->freeblocksize = blocksize_blockheader(header) ;
   stack->blocksize     = blocksize_blockheader(header) ;
   stack->blockstart    = blockstart_blockheader(header) ;

   return 0 ;
ONABORT:
   return err ;
}

static inline int freeblock_binarystack(binarystack_t * stack, blockheader_t * block)
{
   (void) stack ;
   int err ;
   memblock_t mem = (memblock_t) memblock_INIT(block->size, (uint8_t*)block) ;

   err = RELEASE_PAGECACHE(&mem) ;
   if (err) goto ONABORT ;

   ONERROR_testerrortimer(&s_binarystack_errtimer, ONABORT) ;

   return 0 ;
ONABORT:
   return err ;
}

// group: lifetime

int init_binarystack(/*out*/binarystack_t * stack, uint32_t preallocate_size)
{
   int err ;

   *stack = (binarystack_t) binarystack_INIT_FREEABLE ;

   err = allocateblock_binarystack(stack, preallocate_size) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_binarystack(binarystack_t * stack)
{
   int err ;
   int err2 ;

   if (stack->blockstart) {
      blockheader_t * header = header_blockheader(stack->blockstart) ;

      stack->freeblocksize = 0 ;
      stack->blocksize     = 0 ;
      stack->blockstart    = 0 ;

      err = 0 ;

      do {
         blockheader_t * next = header->next ;
         err2 = freeblock_binarystack(stack, header) ;
         if (err2) err = err2 ;
         header = next ;
      } while (header) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;

}

// group: change

/* function: size_binarystack
 * This function walks through all allocated blocks and counts the allocated size. */
size_t size_binarystack(binarystack_t * stack)
{
   size_t size = stack->blocksize - stack->freeblocksize ;

   if (stack->blockstart) {   // works also in case stack == binarystack_INIT_FREEABLE
      blockheader_t * header = header_blockheader(stack->blockstart) ;

      while (header->next) {
         header = header->next ;
         size  += usedsize_blockheader(header) ;
      }
   }

   return size ;
}

// group: change

int push2_binarystack(binarystack_t * stack, uint32_t size, /*out*/uint8_t ** lastpushed)
{
   int err ;

   if (size > stack->freeblocksize) {
      err = allocateblock_binarystack(stack, size) ;
      if (err) goto ONABORT ;
   }

   stack->freeblocksize -= size ;
   *lastpushed = &stack->blockstart[stack->freeblocksize] ;

   return 0;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int pop2_binarystack(binarystack_t * stack, size_t size)
{
   int err ;
   int err2 ;
   blockheader_t * header = header_blockheader(stack->blockstart) ;

   header->usedsize = stack->blocksize - stack->freeblocksize ;

   // walk list of blocks (pop_binarystack implements fast track)

   blockheader_t * endheader = header ;
   size_t        offset      = size ;

   while (endheader->usedsize < offset) {
      offset   -= endheader->usedsize ;
      endheader = endheader->next ;

      if (!endheader) {
         err = EINVAL ;
         goto ONABORT ;
      }
   }

   if (  endheader->usedsize == offset
         && endheader->next) {
      offset    = 0 ;
      endheader = endheader->next ;
   }

   err = 0 ;

   while (header != endheader) {
      blockheader_t * next = header->next ;
      err2 = freeblock_binarystack(stack, header) ;
      if (err2) err = err2 ;
      header = next ;
   }

   stack->freeblocksize = freesize_blockheader(header) + offset ;
   stack->blocksize     = blocksize_blockheader(header) ;
   stack->blockstart    = blockstart_blockheader(header) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   binarystack_t  stack = binarystack_INIT_FREEABLE ;

   // TEST static init
   TEST(0 == stack.freeblocksize) ;
   TEST(0 == stack.blocksize) ;
   TEST(0 == stack.blockstart) ;

   // TEST init_binarystack
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 != top_binarystack(&stack)) ;
   TEST(stack.blockstart == (uint8_t*)top_binarystack(&stack) - stack.blocksize) ;
   TEST(stack.blocksize  == 65536 - sizeof(blockheader_t)) ;

   // TEST free_binarystack
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == stack.freeblocksize) ;
   TEST(0 == stack.blocksize) ;
   TEST(0 == stack.blockstart) ;
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == stack.freeblocksize) ;
   TEST(0 == stack.blocksize) ;
   TEST(0 == stack.blockstart) ;

   // TEST init_binarystack: preallocate_size (0 => one page is preallocated)
   for(unsigned i = 0; i <= 65536; i += 16384) {
      TEST(0 == init_binarystack(&stack, i)) ;
      TEST(0 == size_binarystack(&stack)) ;
      TEST(1 == isempty_binarystack(&stack)) ;
      TEST(stack.blockstart != 0) ;
      TEST(stack.blocksize  == (i < 65536 ? 65536 : 1024*1024) - sizeof(blockheader_t)) ;
      TEST(0 == free_binarystack(&stack)) ;
   }

   // TEST init_binarystack: E2BIG
   TEST(E2BIG == init_binarystack(&stack, 1024*1024+1-sizeof(blockheader_t))) ;

   // TEST init_binarystack: ENOMEM
   init_testerrortimer(&s_binarystack_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == init_binarystack(&stack, 1)) ;
   TEST(0 == stack.freeblocksize) ;
   TEST(0 == stack.blocksize) ;
   TEST(0 == stack.blockstart) ;

   // TEST free_binarystack: ENOMEM
   for (unsigned errcnt = 1; errcnt <= 8; ++errcnt) {
      TEST(0 == init_binarystack(&stack, 1)) ;
      blockheader_t * header = header_blockheader(stack.blockstart) ;
      TEST(header->next == 0) ;
      for (unsigned i = 1; i <= 8; ++i) {
         blockheader_t * old = header ;
         TEST(0 == allocateblock_binarystack(&stack, 16)) ;
         header = header_blockheader(stack.blockstart) ;
         TEST(header->next == old) ;
      }
      init_testerrortimer(&s_binarystack_errtimer, errcnt, ENOMEM) ;
      TEST(ENOMEM == free_binarystack(&stack)) ;
      TEST(0 == stack.freeblocksize) ;
      TEST(0 == stack.blocksize) ;
      TEST(0 == stack.blockstart) ;
   }

   return 0 ;
ONABORT:
   free_testerrortimer(&s_binarystack_errtimer) ;
   free_binarystack(&stack) ;
   return EINVAL ;
}

static int test_query(void)
{
   binarystack_t     stack      = binarystack_INIT_FREEABLE ;
   blockheader_t *   header[11] = { 0 } ;

   // TEST isempty_binarystack
   for (size_t i = 1000; i <= 1000; --i) {
      stack.blocksize     = i ;
      stack.freeblocksize = stack.blocksize + 1 ;
      TEST(0 == isempty_binarystack(&stack)) ;
      stack.freeblocksize = stack.blocksize - 1 ;
      TEST(0 == isempty_binarystack(&stack)) ;
      stack.freeblocksize = stack.blocksize ;
      TEST(1 == isempty_binarystack(&stack)) ;
   }

   // TEST size_binarystack: freed stack
   TEST(0 == size_binarystack(&stack)) ;

   // TEST top_binarystack: freed stack
   TEST(0 == top_binarystack(&stack)) ;

   // prepare
   TEST(0 == init_binarystack(&stack, 0)) ;
   header[0] = header_blockheader(stack.blockstart) ;

   // TEST size_binarystack, top_binarystack: single block
   for (unsigned i = 0; i <= stack.blocksize; ++i) {
      stack.freeblocksize = stack.blocksize - i ;
      uint8_t * t = stack.blockstart + stack.freeblocksize ;
      TEST(i == size_binarystack(&stack)) ;
      TEST(t == top_binarystack(&stack)) ;
   }

   // TEST size_binarystack, top_binarystack: multiple allocated block
   for (unsigned i = 1, s = stack.blocksize; i < lengthof(header); ++i) {
      TEST(0 == allocateblock_binarystack(&stack, i <= 5 ? 1 : 99990)) ;
      header[i] = header_blockheader(stack.blockstart) ;
      TEST(header[i]->next == header[i-1]) ;
      TEST(stack.blocksize > 9999*i) ;
      s += 9999 * i ;
      stack.freeblocksize = stack.blocksize - 9999 * i ;
      uint8_t * t = stack.blockstart + stack.freeblocksize ;
      TEST(s == size_binarystack(&stack)) ;
      TEST(t == top_binarystack(&stack)) ;
   }
   for (unsigned i = lengthof(header)-1, s = size_binarystack(&stack); i >= 1; --i) {
      TEST(0 == freeblock_binarystack(&stack, header[i])) ;
      header[i] = 0 ;
      s -= 9999 * i ;
      stack.freeblocksize = freesize_blockheader(header[i-1]) ;
      stack.blocksize     = blocksize_blockheader(header[i-1]) ;
      stack.blockstart    = blockstart_blockheader(header[i-1]) ;
      uint8_t * t = stack.blockstart + stack.freeblocksize ;
      TEST(s == size_binarystack(&stack)) ;
      TEST(t == top_binarystack(&stack)) ;
   }
   TEST(stack.blocksize == size_binarystack(&stack)) ;
   stack.freeblocksize = stack.blocksize ;
   TEST(0 == size_binarystack(&stack)) ;

   // unprepare
   TEST(0 == free_binarystack(&stack)) ;

   return 0 ;
ONABORT:
   free_binarystack(&stack) ;
   return EINVAL ;
}

static int test_change(void)
{
   binarystack_t  stack = binarystack_INIT_FREEABLE ;
   binarystack_t  old ;
   uint8_t *      blockstart ;
   uint8_t *      addr ;

   // prepare
   TEST(0 == init_binarystack(&stack, 1)) ;
   blockstart = stack.blockstart ;

   // TEST push_binarystack: single block
   old = stack ;
   for (unsigned i = 1; i <= 100; ++i) {
      TEST(0 == push_binarystack(&stack, (uint64_t**)&addr)) ;
      TEST(stack.freeblocksize == old.freeblocksize - i*sizeof(uint64_t) - (i-1)*sizeof(binarystack_t)) ;
      TEST(stack.blocksize     == old.blocksize) ;
      TEST(stack.blockstart    == old.blockstart) ;
      TEST(addr                == old.blockstart + stack.freeblocksize) ;
      TEST(0 == push_binarystack(&stack, (binarystack_t**)&addr)) ;
      TEST(stack.freeblocksize == old.freeblocksize - i*sizeof(uint64_t) - i*sizeof(binarystack_t)) ;
      TEST(stack.blocksize     == old.blocksize) ;
      TEST(stack.blockstart    == old.blockstart) ;
      TEST(addr                == old.blockstart + stack.freeblocksize) ;
   }

   // TEST pop_binarystack: single block
   for (unsigned i = 100; i >= 1; --i) {
      TEST(0 == pop_binarystack(&stack, sizeof(binarystack_t))) ;
      TEST(stack.freeblocksize == old.freeblocksize - i*sizeof(uint64_t) - (i-1)*sizeof(binarystack_t)) ;
      TEST(stack.blocksize     == old.blocksize) ;
      TEST(stack.blockstart    == old.blockstart) ;
      TEST(0 == pop_binarystack(&stack, sizeof(uint64_t))) ;
      TEST(stack.freeblocksize == old.freeblocksize - (i-1)*sizeof(uint64_t) - (i-1)*sizeof(binarystack_t)) ;
      TEST(stack.blocksize     == old.blocksize) ;
      TEST(stack.blockstart    == old.blockstart) ;
   }

   // TEST push2_binarystack: single block
   for (unsigned i = 1; i <= stack.blocksize; ++i) {
      TEST(0 == push2_binarystack(&stack, 1, &addr)) ;
      TEST(stack.freeblocksize == old.freeblocksize - i) ;
      TEST(stack.blocksize     == old.blocksize) ;
      TEST(stack.blockstart    == old.blockstart) ;
      TEST(addr                == old.blockstart + stack.freeblocksize) ;
   }
   for (unsigned i = 1; i <= stack.blocksize; ++i) {
      stack = old ;
      TEST(0 == push2_binarystack(&stack, i, &addr)) ;
      TEST(stack.freeblocksize == old.freeblocksize - i) ;
      TEST(stack.blocksize     == old.blocksize) ;
      TEST(stack.blockstart    == old.blockstart) ;
      TEST(addr                == old.blockstart + stack.freeblocksize) ;
   }

   // TEST pop2_binarystack: single block
   for (unsigned i = stack.blocksize; i >= 1; --i) {
      TEST(0 == pop2_binarystack(&stack, 1)) ;
      TEST(stack.freeblocksize == old.freeblocksize - i +1) ;
      TEST(stack.blocksize     == old.blocksize) ;
      TEST(stack.blockstart    == old.blockstart) ;
   }
   for (unsigned i = 1; i <= stack.blocksize; ++i) {
      stack.freeblocksize = old.freeblocksize - i ;
      TEST(0 == pop2_binarystack(&stack, i)) ;
      TEST(stack.freeblocksize == old.freeblocksize) ;
      TEST(stack.blocksize     == old.blocksize) ;
      TEST(stack.blockstart    == old.blockstart) ;
   }

   // TEST push_binarystack: multiple blocks of size 65536
   TEST(0 == size_binarystack(&stack)) ;
   blockstart = stack.blockstart ;
   for(uint32_t i = 1; i <= 65536; ++i) {
      uint32_t *  ptri     = 0 ;
      size_t      freesize = stack.freeblocksize ;
      TEST(0 == push_binarystack(&stack, &ptri)) ;
      TEST(0 != ptri) ;
      *ptri = i ;
      if (0 == freesize) {
         TEST(blockstart != stack.blockstart) ;
         TEST(header_blockheader(stack.blockstart)->next == header_blockheader(blockstart)) ;
         blockstart = stack.blockstart ;
         freesize   = 65536 - headersize_blockheader() ;
      }
      TEST(stack.freeblocksize == freesize - sizeof(uint32_t)) ;
      TEST(stack.blocksize     == 65536 - headersize_blockheader()) ;
      TEST(stack.blockstart    == blockstart) ;
      TEST(size_binarystack(&stack) == i*sizeof(uint32_t)) ;
   }

   // TEST pop_binarystack: multiple blocks of size 65536
   blockheader_t * next = header_blockheader(blockstart)->next ;
   for(uint32_t i = 65536; i; --i) {
      uint32_t *  ptri     = top_binarystack(&stack) ;
      uint32_t    freesize = stack.freeblocksize + sizeof(uint32_t) ;
      TEST(0 != ptri) ;
      TEST(i == *ptri) ;
      TEST(0 == pop_binarystack(&stack, sizeof(i))) ;
      if (0 == (stack.blocksize - freesize) && i > 1) {
         TEST(stack.blockstart == blockstart_blockheader(next)) ;
         blockstart = stack.blockstart ;
         next       = header_blockheader(blockstart)->next ;
         freesize   = 0 ;
      }
      TEST(stack.freeblocksize == freesize) ;
      TEST(stack.blocksize     == 65536 - headersize_blockheader()) ;
      TEST(stack.blockstart    == blockstart) ;
      TEST(size_binarystack(&stack) == (i-1)*sizeof(uint32_t)) ;
   }
   TEST(stack.freeblocksize == old.freeblocksize) ;
   TEST(stack.blocksize     == old.blocksize) ;
   TEST(stack.blockstart    == old.blockstart) ;

   // TEST push2_binarystack: big size
   for (unsigned i = 1; i <= 20; ++i) {
      blockstart = stack.blockstart ;
      TEST(0 == push2_binarystack(&stack, 1024*1024-headersize_blockheader(), &addr)) ;
      next = header_blockheader(stack.blockstart)->next ;
      TEST(stack.freeblocksize == 0) ;
      TEST(stack.blocksize     == 1024*1024 - headersize_blockheader()) ;
      TEST(stack.blockstart    != blockstart) ;
      TEST(next                == header_blockheader(blockstart)) ;
      TEST(size_binarystack(&stack) == i * (1024*1024-headersize_blockheader())) ;
   }

   // TEST pop2_binarystack: big size
   for (unsigned i = 20; i >= 1; --i) {
      TEST(stack.freeblocksize == 0) ;
      TEST(stack.blocksize     == 1024*1024 - headersize_blockheader()) ;
      TEST(size_binarystack(&stack) == i * (1024*1024-headersize_blockheader())) ;
      TEST(0 == pop2_binarystack(&stack, 1024*1024-headersize_blockheader())) ;
      TEST(stack.blockstart == blockstart_blockheader(next)) ;
      next = next->next ;
   }
   TEST(0 == next) ;
   TEST(stack.freeblocksize == old.freeblocksize) ;
   TEST(stack.blocksize     == old.blocksize) ;
   TEST(stack.blockstart    == old.blockstart) ;

   // TEST push_binarystack: E2BIG
   typedef uint8_t ARRAY[1024*1024] ;
   ARRAY * array = 0 ;
   TEST(E2BIG == push_binarystack(&stack, &array)) ;

   // TEST push2_binarystack: ENOMEM
   init_testerrortimer(&s_binarystack_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == push2_binarystack(&stack, 65536, &addr)) ;
   TEST(stack.freeblocksize == old.freeblocksize) ;
   TEST(stack.blocksize     == old.blocksize) ;
   TEST(stack.blockstart    == old.blockstart) ;

   // TEST pop2_binarystack: EINVAL
   for (unsigned i = 1; i <= 32; ++i) {
      TEST(0 == push2_binarystack(&stack, 60000, &addr)) ;
   }
   TEST(32 * 60000 == size_binarystack(&stack)) ;
   TEST(EINVAL == pop2_binarystack(&stack, 32 * 60000 + 1)) ;
   TEST(32 * 60000 == size_binarystack(&stack)) ;  // nothing changed

   // TEST pop2_binarystack: ENOMEM
   init_testerrortimer(&s_binarystack_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == pop2_binarystack(&stack, 32*60000)) ;
   // freed other nevertheless
   TEST(stack.freeblocksize == old.freeblocksize) ;
   TEST(stack.blocksize     == old.blocksize) ;
   TEST(stack.blockstart    == old.blockstart) ;

   // unprepare
   TEST(0 == free_binarystack(&stack)) ;

   return 0 ;
ONABORT:
   free_testerrortimer(&s_binarystack_errtimer) ;
   free_binarystack(&stack) ;
   return EINVAL ;
}

int unittest_ds_inmem_binarystack()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_change())         goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif


