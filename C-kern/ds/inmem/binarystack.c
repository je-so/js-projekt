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
#include "C-kern/api/platform/virtmemory.h"
#include "C-kern/api/err.h"
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
   blockheader_t  * next ;
   /* variable: size
    * The size this allocated block.
    * The start address in memory is the same as <blockheader_t>. */
   uint32_t       size ;
   /* variable: usedsize
    * The size of pushed objects stored in this block. */
   uint32_t       usedsize ;
} ;

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
/* variable: s_testerror_allocateblock
 * Error code returned in <allocateblock_binarystack> during test. */
static int  s_testerror_allocateblock = 0 ;
/* variable: s_testerror_freeblock
 * Error code returned in <freeblock_binarystack> during test. */
static int  s_testerror_freeblock     = 0 ;
/* define: ADDCODE_IFUNITTEST
 * Generates code if KONFIG_UNITTEST is defined.
 * The only parameter defines the to be generated code. */
#define ADDCODE_IFUNITTEST(code)    code
#endif

#ifndef ADDCODE_IFUNITTEST
#define ADDCODE_IFUNITTEST(code)
#endif


// group: memory allocation

/* function: allocateblock_binarystack
 * Allocates a block of memory.
 * The newly allocated block is the new first entry in the list of allocated blocks.
 * The stack variables are adapted. */
static int allocateblock_binarystack(binarystack_t * stack, uint32_t size)
{
   blockheader_t  * header ;

   if (  ! stack->blockcache
         || freesize_blockheader((blockheader_t*)stack->blockcache) < size) {
      int err ;
      memblock_t mem ;
      uint32_t   pagesize = pagesize_vm() ;
      uint32_t   nrpages  = 1 ;

      size += headersize_blockheader() ;
      if (size < headersize_blockheader()) return ENOMEM ;

      if (pagesize < size) {
         nrpages = (pagesize + size-1) ;
         if (nrpages < pagesize) return ENOMEM ;
         nrpages /= pagesize ;
      }

      ADDCODE_IFUNITTEST(if (s_testerror_allocateblock) return s_testerror_allocateblock ;)
      err = init_vmblock(&mem, nrpages) ;
      if (err) return err ;

      header = (blockheader_t *) mem.addr ;
      header->size     = (uint32_t) mem.size ;
      header->usedsize = 0 ;
   } else {
      header = stack->blockcache ;
      stack->blockcache = 0 ;
   }

   if (stack->blockstart) {
      blockheader_t * oldheader = header_blockheader(stack->blockstart) ;
      oldheader->usedsize = stack->blocksize - stack->freeblocksize ;
      header->next = oldheader ;
   } else {
      header->next = 0 ;
   }

   stack->freeblocksize = blocksize_blockheader(header) ;
   stack->blocksize     = blocksize_blockheader(header) ;
   stack->blockstart    = blockstart_blockheader(header) ;

   return 0 ;
}

static inline int freeblock_binarystack(binarystack_t * stack, blockheader_t * block)
{
   memblock_t mem = (memblock_t) memblock_INIT(block->size, (uint8_t*)block) ;

   if (0 == stack->blockcache) {
      block->usedsize   = 0 ;
      stack->blockcache = block ;
      return 0 ;
   }

   return free_vmblock(&mem) ADDCODE_IFUNITTEST(+ s_testerror_freeblock) ;
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

      if (stack->blockcache) {
         err2 = freeblock_binarystack(stack, (blockheader_t*)stack->blockcache) ;
         if (err2) err = err2 ;
         stack->blockcache = 0 ;
      }

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

   if (stack->blockstart) {
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

   if (stack->freeblocksize < size) {
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

   // walk list of blocks

   blockheader_t * header    = header_blockheader(stack->blockstart) ;
   blockheader_t * endheader = header ;
   size_t        offset      = size ;

   header->usedsize = stack->blocksize - stack->freeblocksize ;

   while (endheader->usedsize < offset) {
      offset   -= endheader->usedsize ;
      endheader = endheader->next ;

      if (!endheader) {
         err = EINVAL ;
         goto ONABORT ;
      }
   }

   err = 0 ;

   while (header != endheader) {
      blockheader_t * next = header->next ;
      err2 = freeblock_binarystack(stack, header) ;
      if (err2) err = err2 ;
      header = next ;
   }

   if (  header->usedsize == offset
         && header->next) {
      blockheader_t * next = header->next ;
      err2 = freeblock_binarystack(stack, header) ;
      if (err2) err = err2 ;
      header = next ;
      offset = 0 ;
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
   TEST(0 == stack.blockcache) ;

   // TEST init, double free
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 != top_binarystack(&stack)) ;
   TEST(stack.blockstart == (uint8_t*)top_binarystack(&stack) - stack.blocksize) ;
   TEST(stack.blocksize  == pagesize_vm() - sizeof(blockheader_t)) ;
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == stack.freeblocksize) ;
   TEST(0 == stack.blocksize) ;
   TEST(0 == stack.blockstart) ;
   TEST(0 == stack.blockcache) ;
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == stack.freeblocksize) ;
   TEST(0 == stack.blocksize) ;
   TEST(0 == stack.blockstart) ;
   TEST(0 == stack.blockcache) ;

   // TEST initial size (size==0 => one page is preallocated)
   for(unsigned i = 0; i <= 20; ++i) {
      TEST(0 == init_binarystack(&stack, i * pagesize_vm())) ;
      TEST(0 == size_binarystack(&stack)) ;
      TEST(1 == isempty_binarystack(&stack)) ;
      TEST(stack.blockstart != 0) ;
      TEST(stack.blocksize  == (i+1)*pagesize_vm() - sizeof(blockheader_t)) ;
      TEST(0 == free_binarystack(&stack)) ;
      TEST(0 == init_binarystack(&stack, i * pagesize_vm()+(pagesize_vm()+1-sizeof(blockheader_t)))) ;
      TEST(stack.blockstart != 0) ;
      TEST(stack.blocksize  == (i+2)*pagesize_vm() - sizeof(blockheader_t)) ;
      TEST(0 == free_binarystack(&stack)) ;
   }

   // TEST init_binarystack: ENOMEM
   s_testerror_allocateblock = ENOMEM ;
   TEST(ENOMEM == init_binarystack(&stack, UINT32_MAX)) ;
   TEST(0 == stack.freeblocksize) ;
   TEST(0 == stack.blocksize) ;
   TEST(0 == stack.blockstart) ;
   TEST(0 == stack.blockcache) ;
   s_testerror_allocateblock = 0 ;

   // TEST free_binarystack: ENOMEM
   TEST(0 == init_binarystack(&stack, 16)) ;
   TEST(stack.freeblocksize -= 16) ;
   for (unsigned i = 1; i <= 8; ++i) {
      // allocate 8 blocks
      TEST(16*i == size_binarystack(&stack)) ;
      TEST(0 == allocateblock_binarystack(&stack, 16)) ;
      TEST(stack.freeblocksize -= 16) ;
   }
   s_testerror_freeblock = ENOMEM ;
   TEST(ENOMEM == free_binarystack(&stack)) ;
   TEST(0 == stack.freeblocksize) ;
   TEST(0 == stack.blocksize) ;
   TEST(0 == stack.blockstart) ;
   TEST(0 == stack.blockcache) ;
   s_testerror_freeblock = 0 ;

   return 0 ;
ONABORT:
   s_testerror_allocateblock = 0 ;
   s_testerror_freeblock     = 0 ;
   free_binarystack(&stack) ;
   return EINVAL ;
}

static int test_query(void)
{
   binarystack_t  stack = binarystack_INIT_FREEABLE ;
   const unsigned SIZE  = pagesize_vm() - sizeof(blockheader_t) ;

   // TEST isempty_binarystack, size_binarystack, top_binarystack: freed stack
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == top_binarystack(&stack)) ;

   // TEST isempty_binarystack, size_binarystack: empty stack
   TEST(0 == init_binarystack(&stack, SIZE)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(stack.blockstart == (uint8_t*)top_binarystack(&stack) - stack.blocksize) ;
   TEST(stack.blocksize  == SIZE) ;

   // TEST isempty_binarystack, size_binarystack, top_binarystack: single allocated block
   for (unsigned i = 1; i <= SIZE; ++i) {
      uint32_t oldfreeblocksize = stack.freeblocksize ;
      stack.freeblocksize -= i ;
      TEST(i == size_binarystack(&stack)) ;
      TEST(0 == isempty_binarystack(&stack)) ;
      TEST(top_binarystack(&stack) == stack.blockstart + stack.freeblocksize) ;
      stack.freeblocksize = oldfreeblocksize ;
   }
   TEST(0 == size_binarystack(&stack)) ;
   TEST(1 == isempty_binarystack(&stack)) ;

   // TEST isempty_binarystack, size_binarystack, top_binarystack: multiple allocated block
   for (unsigned i = 1, s = 0; i <= SIZE; i += SIZE/7) {
      TEST(SIZE == stack.freeblocksize) ;
      uint8_t * t = stack.blockstart + stack.freeblocksize ;
      TEST(1 == isempty_binarystack(&stack)) ;
      TEST(s == size_binarystack(&stack)) ;
      TEST(t == top_binarystack(&stack)) ;
      stack.freeblocksize -= i ;
      s += i ;
      t -= i ;
      TEST(0 == isempty_binarystack(&stack)) ;
      TEST(s == size_binarystack(&stack)) ;
      TEST(t == top_binarystack(&stack)) ;
      TEST(0 == allocateblock_binarystack(&stack, SIZE)) ;
   }
   for (unsigned i = SIZE-(SIZE-1)%(SIZE/7), s = size_binarystack(&stack); i >= 1; i -= (i > (SIZE/7) ? (SIZE/7) : i)) {
      TEST(0 == pop_binarystack(&stack, 0)) ; // cancel last allocateblock_binarystack
      TEST(SIZE-i == stack.freeblocksize) ;
      uint8_t * t = stack.blockstart + stack.freeblocksize ;
      TEST(0 == isempty_binarystack(&stack)) ;
      TEST(s == size_binarystack(&stack)) ;
      TEST(t == top_binarystack(&stack)) ;
      stack.freeblocksize += i ;
      s -= i ;
      t += i ;
      TEST(1 == isempty_binarystack(&stack)) ;
      TEST(s == size_binarystack(&stack)) ;
      TEST(t == top_binarystack(&stack)) ;
   }
   TEST(0 == size_binarystack(&stack)) ;
   TEST(1 == isempty_binarystack(&stack)) ;

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
   const unsigned SIZE  = 10000 ;
   uint8_t        * addr ;
   uint8_t        * addr2 ;

   // prepare
   TEST(0 == init_binarystack(&stack, SIZE)) ;

   // TEST push_binarystack, pop_binarystack, push2_binarystack, pop2_binarystack
   TEST(0 == push_binarystack(&stack, (uint64_t**)&addr)) ;
   TEST(8 == size_binarystack(&stack)) ;
   TEST(0 == isempty_binarystack(&stack)) ;
   TEST(addr == stack.blockstart + stack.freeblocksize) ;
   TEST(addr == top_binarystack(&stack)) ;
   *(uint64_t*)addr = 0x1122334455667788 ;
   TEST(0 == push2_binarystack(&stack, 4, &addr2)) ;
   TEST(addr2 == (uint8_t*)addr - 4) ;
   TEST(addr2 == top_binarystack(&stack)) ;
   TEST(12 == size_binarystack(&stack)) ;
   TEST(0 == pop2_binarystack(&stack, 4)) ;
   TEST(addr == top_binarystack(&stack)) ;
   TEST(8 == size_binarystack(&stack)) ;
   TEST(*(uint64_t*)addr == 0x1122334455667788) ;
   TEST(0 == pop_binarystack(&stack, 8)) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(addr+8 == top_binarystack(&stack)) ;

   // TEST push_binarystack SIZE uint32_t
   TEST(1 == isempty_binarystack(&stack)) ;
   for(uint32_t i = 1, newcount=0; i <= SIZE; ++i) {
      uint8_t  * blockstart = stack.blockstart ;
      uint32_t freesize     = stack.freeblocksize ;
      uint32_t * ptri = 0 ;
      bool     isNew  = stack.freeblocksize < sizeof(i) ;
      TEST(0 == push_binarystack(&stack, &ptri)) ;
      TEST(0 != ptri) ;
      *ptri = i ;
      if (isNew) {
         ++ newcount ;
         TEST(blockstart != stack.blockstart) ;
         TEST(blockstart == blockstart_blockheader(header_blockheader(stack.blockstart)->next)) ;
         freesize = blocksize_blockheader(header_blockheader(stack.blockstart)) ;
      } else {
         TEST(blockstart == stack.blockstart) ;
      }
      TEST(freesize    == stack.freeblocksize + sizeof(i)) ;
      TEST(i*sizeof(i) == size_binarystack(&stack)) ;
      TEST(0           == isempty_binarystack(&stack)) ;
      TEST(i < SIZE || newcount) ;
   }

   // TEST pop_binarystack SIZE uint32_t
   for(uint32_t i = SIZE; i; --i) {
      uint8_t  * blockstart = stack.blockstart ;
      blockheader_t * next  = header_blockheader(stack.blockstart)->next ;
      size_t   nextfree = next ? freesize_blockheader(next) : 0 ;
      uint32_t freesize = stack.freeblocksize ;
      bool     isNew  = (stack.blocksize - freesize == sizeof(i)) ;
      uint32_t * ptri = top_binarystack(&stack) ;
      TEST(0 != ptri) ;
      TEST(i == *ptri) ;
      TEST(0 == pop_binarystack(&stack, sizeof(i))) ;
      if (isNew && i > 1) {
         TEST(stack.blockstart == blockstart_blockheader(next)) ;
         TEST(stack.freeblocksize == nextfree) ;
      } else {
         TEST(stack.blockstart == blockstart) ;
         TEST(i != 1 || isNew) ;
         TEST(stack.freeblocksize == freesize + sizeof(i)) ;
      }
      TEST((i-1)*sizeof(i) == size_binarystack(&stack)) ;
      TEST((i==1)          == isempty_binarystack(&stack)) ;
   }

   // TEST push_binarystack, pop_binarystack: one pagesize
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == init_binarystack(&stack, 1)) ;
   addr2 = stack.blockstart ;
   TEST(0 == push2_binarystack(&stack, 1, &addr)) ;
   TEST(addr2 == stack.blockstart) ;
   for (unsigned i = 1; i <= 20; ++i) {
      TEST(0 == push2_binarystack(&stack, pagesize_vm(), &addr)) ;
      TEST(i*pagesize_vm()+1 == size_binarystack(&stack)) ;
   }
   for (unsigned i = 20; i >= 1; --i) {
      TEST(addr2 != stack.blockstart) ;
      TEST(0 == pop_binarystack(&stack, pagesize_vm())) ;
      TEST((i-1)*pagesize_vm()+1 == size_binarystack(&stack)) ;
   }
   TEST(addr2 == stack.blockstart) ;
   TEST(1     == size_binarystack(&stack)) ;

   // TEST pop_binarystack: change size of last pushed
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(0 == push2_binarystack(&stack, 1, &addr)) ;
   for (unsigned i = 1; i <= 20; ++i) {
      addr2 = stack.blockstart ;
      TEST(0 == push2_binarystack(&stack, pagesize_vm()-sizeof(blockheader_t), &addr)) ;
      TEST(addr != addr2) ;
      TEST(addr == stack.blockstart) ;
      TEST(addr == top_binarystack(&stack)) ;
      for (unsigned shrink = 1; shrink < pagesize_vm()-sizeof(blockheader_t); ++shrink) {
         TEST(0 == pop_binarystack(&stack, 1)) ;
         TEST(addr + shrink == top_binarystack(&stack)) ;
      }
   }
   for (unsigned i = 1; i <= 20; ++i) {
      addr = stack.blockstart ;
      TEST(0 == pop_binarystack(&stack, 1)) ;
      TEST(addr != stack.blockstart) ;
      addr = stack.blockstart + pagesize_vm() -sizeof(blockheader_t) - 1 ;
      TEST(addr == top_binarystack(&stack)) ;
   }
   addr = stack.blockstart ;
   TEST(0 == pop_binarystack(&stack, 1)) ;
   TEST(addr == stack.blockstart) ;
   addr = stack.blockstart + pagesize_vm() -sizeof(blockheader_t) ;
   TEST(addr == top_binarystack(&stack)) ;

   // TEST push_binarystack: ENOMEM
   typedef uint8_t ARRAY[32768] ;
   ARRAY * array = 0 ;
   TEST(sizeof(ARRAY) >= pagesize_vm()) ;
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == init_binarystack(&stack, sizeof(ARRAY))) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == push_binarystack(&stack, &array)) ;
   s_testerror_allocateblock = ENOMEM ;
   addr = stack.blockstart ;
   TEST(ENOMEM == push_binarystack(&stack, &array)) ;
   TEST(sizeof(ARRAY) == size_binarystack(&stack)) ;
   TEST(array == top_binarystack(&stack)) ;
   TEST(addr == stack.blockstart) ;
   s_testerror_allocateblock = 0 ;

   // TEST pop_binarystack: ENOMEM
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == init_binarystack(&stack, sizeof(ARRAY))) ;
   addr = stack.blockstart ;
   for (unsigned i = 1; i <= 32; ++i) {
      array = 0 ;
      TEST(0 == push_binarystack(&stack, &array)) ;
      TEST(0 != array) ;
      TEST(i * sizeof(ARRAY) == size_binarystack(&stack)) ;
   }
   s_testerror_freeblock = ENOMEM ;
   TEST(ENOMEM == pop_binarystack(&stack, 32 * sizeof(ARRAY))) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(addr == stack.blockstart) ;
   s_testerror_freeblock = 0 ;

   // TEST pop_binarystack: EINVAL
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == init_binarystack(&stack, 8)) ;
   addr = stack.blockstart ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(EINVAL == pop_binarystack(&stack, 1)) ;
   TEST(stack.freeblocksize -= 8) ;
   for (unsigned i = 0; i < 7; ++i) {
      // allocate 8 blocks
      TEST(0 == allocateblock_binarystack(&stack, 32)) ;
      TEST(stack.freeblocksize -= 32) ;
   }
   TEST(EINVAL == pop_binarystack(&stack, 1 + 8 + 7*32)) ;
   TEST(addr != stack.blockstart) ;
   TEST(0 == pop_binarystack(&stack, 8 + 7*32)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(addr == stack.blockstart) ;

   // unprepare
   TEST(0 == free_binarystack(&stack)) ;

   return 0 ;
ONABORT:
   s_testerror_allocateblock = 0 ;
   s_testerror_freeblock     = 0 ;
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


