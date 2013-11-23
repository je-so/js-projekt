/* title: Test-MemoryManager impl
   Implements <TestMemoryManager>.

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

   file: C-kern/api/test/mm/testmm.h
    Header file <Test-MemoryManager>.

   file: C-kern/test/mm/testmm.c
    Implementation file <Test-MemoryManager impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/mm/testmm.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

/* typedef: struct testmm_block_t
 * Shortcut for <testmm_block_t>. */
typedef struct testmm_block_t          testmm_block_t ;

/* typedef: struct testmm_page_t
 * Shortcut for <testmm_page_t>. */
typedef struct testmm_page_t           testmm_page_t ;


/* struct: testmm_it
 * Declares <testmm_it> as subtype of <mm_it>. See <mm_it_DECLARE>. */
mm_it_DECLARE(testmm_it, testmm_t)


/* struct: testmm_block_t
 * Describes the header of an allocated memory block. */
struct testmm_block_t {
   struct {
      size_t               datasize ;
      size_t               alignsize ;
      uint8_t              fillvalue ;
      uint8_t              * userdata ;
      void                 * fill[1] ;
   } header ;

   // user data

   struct {
      testmm_block_t       * header[2] ;
   } trailer ;
} ;

/* function: alignsize_testmmblock
 * Aligns a value to the next multiple of KONFIG_MEMALIGN.
 * If KONFIG_MEMALIGN is less than sizeof(void*) the value sizeof(void*)
 * is chosen instead of KONFIG_MEMALIGN.
 *
 * Precondition:
 * KONFIG_MEMALIGN must be a power of two for this algorithm to work.
 * > return (bytesize + (memalign-1)) & (~(memalign-1)) ;
 * > memalign == 0b001000
 * > (~(memalign-1)) == ~ (0b001000 - 1) == ~ (0b000111) == 0b111000 */
static inline size_t alignsize_testmmblock(size_t bytesize)
{
   const size_t mmalign = ((KONFIG_MEMALIGN > sizeof(void*)) ? KONFIG_MEMALIGN : sizeof(void*)) ;
   static_assert(0 == (mmalign & (mmalign-1)), "mmalign must be power of two") ;
   return (bytesize + (mmalign-1)) & (~(mmalign-1)) ;
}

static void init_testmmblock(
    testmm_block_t * block,
    size_t           datasize,
    size_t           alignsize)
{
   const size_t headersize  =  alignsize_testmmblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_testmmblock(sizeof(block->trailer)) ;

   block->header.datasize  = datasize ;
   block->header.alignsize = alignsize ;
   block->header.fillvalue = (uint8_t) ((uintptr_t)block / 128) ;
   block->header.userdata  = headersize + (uint8_t*) block ;

   typeof(block->trailer) * trailer = (typeof(block->trailer)*) (block->header.userdata + alignsize) ;

   for (unsigned i = 0; i <= (headersize - sizeof(block->header)) / sizeof(void*); ++i) {
      block->header.fill[i] = (void*)trailer ;
   }

   for (size_t i = datasize; i < alignsize; ++i) {
      block->header.userdata[i] = block->header.fillvalue ;
   }

   for (unsigned i = 0; i < trailersize / sizeof(void*); ++i) {
      trailer->header[i] = block ;
   }
}

static bool isvalid_testmmblock(testmm_block_t * block, struct memblock_t * memblock)
{
   const size_t headersize  =  alignsize_testmmblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_testmmblock(sizeof(block->trailer)) ;
   const size_t alignsize   =  alignsize_testmmblock(memblock->size) ;

   if (block->header.datasize  != memblock->size)  return false ;
   if (block->header.alignsize != alignsize)       return false ;
   if (block->header.fillvalue != (uint8_t) ((uintptr_t)block / 128)) return false ;
   if (block->header.userdata  != memblock->addr)  return false ;

   typeof(block->trailer) * trailer = (typeof(block->trailer)*) (block->header.userdata + alignsize) ;

   for (unsigned i = 0; i <= (headersize - sizeof(block->header)) / sizeof(void*); ++i) {
      if (block->header.fill[i] != trailer) return false ;
   }

   for (size_t i = memblock->size; i < alignsize; ++i) {
      if (memblock->addr[i] != block->header.fillvalue) return false ;
   }

   for (unsigned i = 0; i < trailersize / sizeof(void*); ++i) {
      if (trailer->header[i] != block) return false ;
   }

   return true ;
}


/* struct: testmm_page_t
 * Holds one big data block consisting of many vm memory pages.
 * The memory allocation strategy is simple. Memory allocation
 * requests are always satified from the beginning of the last free block.
 * If it is too small allocation fails. Freed blocks are only marked as freed.
 * If a memory block marked as free is directly before the last free block
 * it is merged with. The last free block grows in this manner until all
 * freed blocks are merged.
 */
struct testmm_page_t {
   vmpage_t       vmblock ;
   memblock_t     datablock ;
   memblock_t     freeblock ;
   testmm_page_t* next ;
} ;

// group: lifetime

static int new_testmmpage(testmm_page_t ** mmpage, size_t minblocksize, testmm_page_t * next)
{
   int err ;
   const size_t   headersize  = alignsize_testmmblock(sizeof(((testmm_block_t*)0)->header)) ;
   const size_t   trailersize = alignsize_testmmblock(sizeof(((testmm_block_t*)0)->trailer)) ;
   const size_t   blocksize = (minblocksize + headersize + trailersize) < 1024 * 1024 ? 1024 * 1024 : (minblocksize + headersize + trailersize) ;
   const size_t   pgsize    = pagesize_vm() ;
   const size_t   datasize  = (blocksize + (pgsize-1)) & ~(pgsize-1) ;
   const size_t   extrasize = 2 * pgsize + ((sizeof(testmm_page_t) + (pgsize-1)) & ~(pgsize-1)) ;
   vmpage_t       vmblock   = vmpage_INIT_FREEABLE ;
   testmm_page_t* new_mmpage ;

   if (minblocksize >= 16*1024*1024) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   err = init_vmpage(&vmblock, datasize + extrasize) ;
   if (err) goto ONABORT ;

   new_mmpage = (testmm_page_t*) vmblock.addr ;
   new_mmpage->vmblock   = vmblock ;
   new_mmpage->datablock = (memblock_t) memblock_INIT(datasize, vmblock.addr + vmblock.size - pagesize_vm() - datasize) ;
   new_mmpage->freeblock = new_mmpage->datablock ;
   new_mmpage->next      = next ;

   err = protect_vmpage(&(vmpage_t)vmpage_INIT(pagesize_vm(), new_mmpage->datablock.addr - pagesize_vm()), accessmode_NONE) ;
   if (err) goto ONABORT ;
   err = protect_vmpage(&(vmpage_t)vmpage_INIT(pagesize_vm(), new_mmpage->datablock.addr + new_mmpage->datablock.size), accessmode_NONE) ;
   if (err) goto ONABORT ;

   *mmpage = new_mmpage ;

   return 0 ;
ONABORT:
   free_vmpage(&vmblock) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int delete_testmmpage(testmm_page_t ** mmpage)
{
   int err ;
   testmm_page_t * del_mmpage = *mmpage ;

   if (del_mmpage) {
      *mmpage = 0 ;

      vmpage_t  vmblock = del_mmpage->vmblock ;
      err = free_vmpage(&vmblock) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

static int ispagefree_testmmpage(testmm_page_t * mmpage)
{
   return (mmpage->datablock.addr == mmpage->freeblock.addr) ;
}

static int isblockvalid_testmmpage(testmm_page_t * mmpage, struct memblock_t * memblock)
{
   const size_t headersize  =  alignsize_testmmblock(sizeof(((testmm_block_t*)0)->header)) ;
   const size_t trailersize =  alignsize_testmmblock(sizeof(((testmm_block_t*)0)->trailer)) ;

   testmm_block_t * block = (testmm_block_t*) (memblock->addr - headersize) ;

   if (     mmpage->datablock.addr + headersize > memblock->addr
         || mmpage->freeblock.addr <= memblock->addr
         || mmpage->datablock.size <= block->header.alignsize
         || (size_t)(mmpage->freeblock.addr - memblock->addr) < block->header.alignsize + trailersize) {
      return false ;
   }

   return isvalid_testmmblock(block, memblock) ;
}

static int freeblock_testmmpage(testmm_page_t * mmpage, struct memblock_t * memblock)
{
   int err ;
   testmm_block_t  * block ;
   const size_t headersize  =  alignsize_testmmblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_testmmblock(sizeof(block->trailer)) ;

   VALIDATE_INPARAM_TEST(isblockvalid_testmmpage(mmpage, memblock), ONABORT, ) ;

   block = (testmm_block_t*) (memblock->addr - headersize) ;
   block->header.datasize = 0 ;

   if (mmpage->freeblock.addr == memblock->addr + trailersize + block->header.alignsize) {
      // block is adjacent to the free block
      // instead of marking it only as freed let freeblock grow

      // check if the block before is also free and use it as new border
      while ((uint8_t*)block > mmpage->datablock.addr + trailersize) {
         typeof(block->trailer) * trailer = (typeof(block->trailer)*) ((uint8_t*)block - trailersize) ;
         testmm_block_t * block2 = trailer->header[0] ;

         if (  (uint8_t*)block2 < mmpage->datablock.addr
            || 0 != block2->header.datasize) {
            break ; // block2 in use
         }

         block = block2 ;
      }

      (void) growleft_memblock(&mmpage->freeblock, (size_t)(mmpage->freeblock.addr - (uint8_t*)block)) ;
   }

   *memblock = (memblock_t) memblock_INIT_FREEABLE ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int newblock_testmmpage(testmm_page_t * mmpage, size_t newsize, struct memblock_t * memblock)
{
   int err ;
   testmm_block_t * block ;
   const size_t headersize  =  alignsize_testmmblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_testmmblock(sizeof(block->trailer)) ;
   const size_t alignsize   =  alignsize_testmmblock(newsize) ;
   const size_t blocksize   =  headersize + trailersize + alignsize ;

   block = (testmm_block_t*) mmpage->freeblock.addr ;

   err = shrinkleft_memblock(&mmpage->freeblock, blocksize) ;
   if (err) return err ;

   init_testmmblock(block, newsize, alignsize) ;

   memblock->addr = block->header.userdata ;
   memblock->size = newsize ;

   return 0 ;
}

static int resizeblock_testmmpage(testmm_page_t * mmpage, size_t newsize, struct memblock_t * memblock)
{
   int err ;
   testmm_block_t  * block ;
   const size_t headersize  =  alignsize_testmmblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_testmmblock(sizeof(block->trailer)) ;
   const size_t alignsize   =  alignsize_testmmblock(newsize) ;

   VALIDATE_INPARAM_TEST(isblockvalid_testmmpage(mmpage, memblock), ONABORT, ) ;

   block = (testmm_block_t*) (memblock->addr - headersize) ;

   if (mmpage->freeblock.addr != memblock->addr + trailersize + block->header.alignsize) {
      return ENOMEM ;
   }

   if (  alignsize < newsize
         || (  alignsize > block->header.alignsize
               && mmpage->freeblock.size < (alignsize - block->header.alignsize)))  {
      return ENOMEM ;
   }

   mmpage->freeblock.addr -= block->header.alignsize ;
   mmpage->freeblock.size += block->header.alignsize ;
   mmpage->freeblock.addr += alignsize ;
   mmpage->freeblock.size -= alignsize ;

   init_testmmblock(block, newsize, alignsize) ;

   memblock->size = newsize ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int getblock_testmmpage(testmm_page_t * mmpage, size_t blockindex, /*out*/struct memblock_t * memblock)
{
   int err ;
   testmm_block_t * block   =  (testmm_block_t*) mmpage->datablock.addr ;
   const size_t headersize  =  alignsize_testmmblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_testmmblock(sizeof(block->trailer)) ;

   for (size_t i = 0; i < blockindex; ++i) {

      if (block->header.datasize) {
         memblock_t temp = memblock_INIT(block->header.datasize, block->header.userdata) ;
         if (!isblockvalid_testmmpage(mmpage, &temp)) {
            err = EINVAL ;
            PRINTSIZE_ERRLOG(blockindex) ;
            goto ONABORT ;
         }
      }

      block = (testmm_block_t*) ( ((uint8_t*)block) + headersize + trailersize + block->header.alignsize) ;
   }

   *memblock = (memblock_t) memblock_INIT(block->header.datasize, ((uint8_t*)block) + headersize) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// section: testmm_t

// group: variables

/* variable: s_testmm_interface
 * Contains single instance of interface <testmm_it>. */
static testmm_it     s_testmm_interface = mm_it_INIT(
                        &malloc_testmm,
                        &mresize_testmm,
                        &mfree_testmm,
                        &sizeallocated_testmm
                     ) ;

// group: helper

static int addpage_testmm(testmm_t * mman, size_t newsize)
{
   int err ;
   testmm_page_t * mmpage ;

   err = new_testmmpage(&mmpage, newsize, mman->mmpage) ;
   if (err) return err ;

   mman->mmpage = mmpage ;
   return 0 ;
}

static testmm_page_t * findpage_testmm(testmm_t * mman, uint8_t * blockaddr)
{
   testmm_page_t * mmpage = mman->mmpage ;

   do {
      if (  mmpage->datablock.addr <= blockaddr
         && (size_t) (blockaddr - mmpage->datablock.addr) < mmpage->datablock.size) {
         break ; // found
      }
      mmpage = mmpage->next ;
   } while (mmpage) ;

   return mmpage ;
}

// group: context

testmm_t * mmcontext_testmm(void)
{
   if (genericcast_mmit(&s_testmm_interface, testmm_t) != mm_maincontext().iimpl) {
      return 0 ;
   }

   return (testmm_t*) mm_maincontext().object ;
}

int switchon_testmm()
{
   int  err ;
   mm_t              testmm   = mm_INIT_FREEABLE ;
   threadcontext_t * tcontext = tcontext_maincontext() ;

   if (genericcast_mmit(&s_testmm_interface, testmm_t) != tcontext->mm.iimpl) {
      memblock_t  previous_mm ;

      err = initasmm_testmm(&testmm) ;
      if (err) goto ONABORT ;

      err = malloc_mm(testmm, sizeof(mm_t), &previous_mm) ;
      if (err) goto ONABORT ;

      initcopy_iobj((mm_t*)previous_mm.addr, &tcontext->mm) ;
      setmm_threadcontext(tcontext, &testmm) ;
   }

   return 0 ;
ONABORT:
   freeasmm_testmm(&testmm) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int switchoff_testmm()
{
   int err ;

   if (genericcast_mmit(&s_testmm_interface, testmm_t) == mm_maincontext().iimpl) {
      testmm_t *        testmm      = (testmm_t*)mm_maincontext().object ;
      memblock_t        previous_mm = memblock_INIT_FREEABLE ;
      testmm_page_t *   mmpage      = testmm->mmpage ;

      while (mmpage->next) {
         mmpage = mmpage->next ;
      }

      err = getblock_testmmpage(mmpage, 1, &previous_mm) ;
      if (err) goto ONABORT ;

      if (sizeof(mm_t) != previous_mm.size) {
         err = EINVAL ;
         goto ONABORT ;
      }

      mm_t mmobj ;
      initcopy_iobj(&mmobj, &mm_maincontext()) ;
      initcopy_iobj(&mm_maincontext(), (mm_t*)previous_mm.addr) ;

      err = freeasmm_testmm(&mmobj) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: lifetime

int init_testmm(/*out*/testmm_t * mman)
{
   int err ;
   testmm_page_t  * mmpage = 0 ;

   err = new_testmmpage(&mmpage, 0, 0) ;
   if (err) goto ONABORT ;

   mman->mmpage        = mmpage ;
   mman->sizeallocated = 0 ;
   mman->simulateResizeError = 0 ;
   mman->simulateFreeError   = 0 ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_testmm(testmm_t * mman)
{
   int err ;
   int err2 ;

   if (mman->mmpage) {
      testmm_page_t * mmpage = mman->mmpage->next ;

      err = delete_testmmpage(&mman->mmpage) ;
      while (mmpage) {
         testmm_page_t * delmmpage = mmpage ;
         mmpage = mmpage->next ;
         err2 = delete_testmmpage(&delmmpage) ;
         if (err2) err = err2 ;
      }

      mman->sizeallocated = 0 ;
      mman->simulateResizeError = 0 ;
      mman->simulateFreeError   = 0 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

int initasmm_testmm(/*out*/mm_t * testmm)
{
   int err ;
   memblock_t     memblock ;
   testmm_t       testmmobj = testmm_INIT_FREEABLE ;
   const size_t   objsize   = sizeof(testmm_t) ;

   err = init_testmm(&testmmobj) ;
   if (err) goto ONABORT ;

   err = malloc_testmm(&testmmobj, objsize, &memblock) ;
   if (err) goto ONABORT ;

   memcpy(memblock.addr, &testmmobj, objsize) ;

   *testmm = (mm_t) mm_INIT((struct mm_t*) memblock.addr, genericcast_mmit(&s_testmm_interface, testmm_t)) ;

   return err ;
ONABORT:
   free_testmm(&testmmobj) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int freeasmm_testmm(mm_t * testmm)
{
   int err ;
   testmm_t       testmmobj = testmm_INIT_FREEABLE ;
   const size_t   objsize   = sizeof(testmm_t) ;

   if (testmm->object) {
      assert(testmm->iimpl == genericcast_mmit(&s_testmm_interface, testmm_t)) ;

      memcpy(&testmmobj, testmm->object, objsize) ;
      err = free_testmm(&testmmobj) ;

      *testmm = (mm_t) mm_INIT_FREEABLE ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

size_t sizeallocated_testmm(testmm_t * mman)
{
   return mman->sizeallocated ;
}

// group: simulation

void setresizeerr_testmm(testmm_t * mman, struct test_errortimer_t * errtimer)
{
   mman->simulateResizeError = errtimer ;
}

void setfreeerr_testmm(testmm_t * mman, struct test_errortimer_t * errtimer)
{
   mman->simulateFreeError = errtimer ;
}

// group: allocate

int malloc_testmm(testmm_t * mman, size_t size, /*out*/struct memblock_t * memblock)
{
   int err ;

   err = newblock_testmmpage(mman->mmpage, size, memblock) ;

   if (err) {
      if (ENOMEM != err) return err ;

      err = addpage_testmm(mman, size) ;
      if (err) return err ;

      err = newblock_testmmpage(mman->mmpage, size, memblock) ;
   }

   if (!err) {
      mman->sizeallocated += memblock->size ;
   }

   return err ;
}

int mresize_testmm(testmm_t * mman, size_t newsize, struct memblock_t * memblock)
{
   int err ;

   if (0 == newsize) {
      return mfree_testmm(mman, memblock) ;
   }

   if (mman->simulateResizeError) {
      err = process_testerrortimer(mman->simulateResizeError) ;
      if (err) {
         mman->simulateResizeError = 0 ;
         goto ONABORT ;
      }
   }

   if (isfree_memblock(memblock)) {
      err = malloc_testmm(mman, newsize, memblock) ;
      if (err) goto ONABORT ;
   } else {
      testmm_page_t * mmpage = findpage_testmm(mman, memblock->addr) ;
      if (!mmpage) {
         err = EINVAL ;
         goto ONABORT ;
      }

      size_t freesize = memblock->size ;

      err = resizeblock_testmmpage(mmpage, newsize, memblock) ;
      if (!err) {
         mman->sizeallocated += memblock->size ;

      } else {
         if (ENOMEM != err) goto ONABORT ;

         memblock_t newmemblock ;

         err = malloc_testmm(mman, newsize, &newmemblock) ;
         if (err) goto ONABORT ;

         // copy content
         memcpy(newmemblock.addr, memblock->addr, memblock->size < newsize ? memblock->size : newsize) ;

         err = freeblock_testmmpage(mmpage, memblock) ;
         if (err) {
            (void) freeblock_testmmpage(mman->mmpage, &newmemblock) ;
            goto ONABORT ;
         }
         *memblock = newmemblock ;
      }

      mman->sizeallocated -= freesize ;
   }

   return 0 ;
ONABORT:
   if (ENOMEM == err) {
      TRACEOUTOFMEM_ERRLOG(newsize, err) ;
   }
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int mfree_testmm(testmm_t  * mman, struct memblock_t * memblock)
{
   int err ;

   if (!isfree_memblock(memblock)) {
      testmm_page_t * mmpage = findpage_testmm(mman, memblock->addr) ;
      if (!mmpage) {
         err = EINVAL ;
         goto ONABORT ;
      }

      size_t freesize = memblock->size ;

      err = freeblock_testmmpage(mmpage, memblock) ;
      if (err) goto ONABORT ;

      *memblock = (memblock_t) memblock_INIT_FREEABLE ;
      mman->sizeallocated -= freesize ;

      if (ispagefree_testmmpage(mman->mmpage)) {
         while (  mman->mmpage->next
                  && (  ispagefree_testmmpage(mman->mmpage->next)
                        || !mman->mmpage->next->next)/*is first page*/) {
            mmpage = mman->mmpage;
            mman->mmpage = mman->mmpage->next ;
            err = delete_testmmpage(&mmpage) ;
            if (err) goto ONABORT ;
         }
      }

   }

   if (mman->simulateFreeError) {
      err = process_testerrortimer(mman->simulateFreeError) ;
      if (err) {
         mman->simulateFreeError = 0 ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_testmmpage(void)
{
   testmm_page_t      * mmpage  = 0 ;
   vm_mappedregions_t   mapping = vm_mappedregions_INIT_FREEABLE ;
   vmpage_t             page[4] ;
   memblock_t           memblock ;
   const size_t         headersize  = alignsize_testmmblock(sizeof(((testmm_block_t*)0)->header)) ;
   const size_t         trailersize = alignsize_testmmblock(sizeof(((testmm_block_t*)0)->trailer)) ;

   // TEST init, double free
   TEST(0 == new_testmmpage(&mmpage, 0, 0)) ;
   TEST(0 != mmpage) ;
   TEST(0 == delete_testmmpage(&mmpage)) ;
   TEST(0 == mmpage) ;
   TEST(0 == delete_testmmpage(&mmpage)) ;
   TEST(0 == mmpage) ;

   // TEST init: field contents
   TEST(0 == new_testmmpage(&mmpage, 0, (testmm_page_t*)5)) ;
   TEST(0 != mmpage) ;
   TEST(1024*1024 <= mmpage->datablock.size) ;
   TEST(mmpage->datablock.addr == mmpage->freeblock.addr) ;
   TEST(mmpage->datablock.size == mmpage->freeblock.size) ;
   TEST(mmpage->vmblock.addr   == (uint8_t*)mmpage) ;
      // assumes sizeof(testmm_page_t) <= pagesize_vm()
   TEST(mmpage->vmblock.addr   == mmpage->datablock.addr - 2*pagesize_vm()) ;
   TEST(mmpage->vmblock.size   == mmpage->freeblock.size + 3*pagesize_vm()) ;
   TEST(5 == (uintptr_t)mmpage->next) ;
   TEST(0 == delete_testmmpage(&mmpage)) ;
   TEST(0 == mmpage) ;

   // TEST init: memory is mapped
   TEST(0 == new_testmmpage(&mmpage, 0, 0)) ;
   TEST(0 != mmpage) ;
   page[0] = (vmpage_t) vmpage_INIT(pagesize_vm(), mmpage->vmblock.addr) ;
   page[1] = (vmpage_t) vmpage_INIT(mmpage->datablock.size, mmpage->datablock.addr) ;
   page[2] = (vmpage_t) vmpage_INIT(pagesize_vm(), mmpage->datablock.addr + mmpage->datablock.size) ;
   page[3] = (vmpage_t) vmpage_INIT(pagesize_vm(), mmpage->datablock.addr - pagesize_vm()) ;
   TEST(0 == init_vmmappedregions(&mapping)) ;
   TEST(1 == ismapped_vmmappedregions(&mapping, &page[0], (accessmode_RDWR|accessmode_PRIVATE))) ;
   TEST(1 == ismapped_vmmappedregions(&mapping, &page[1], (accessmode_RDWR|accessmode_PRIVATE))) ;
   TEST(1 == ismapped_vmmappedregions(&mapping, &page[2], (accessmode_NONE|accessmode_PRIVATE))) ;
   TEST(1 == ismapped_vmmappedregions(&mapping, &page[3], (accessmode_NONE|accessmode_PRIVATE))) ;
   TEST(0 == free_vmmappedregions(&mapping)) ;

   // TEST free: memory is unmapped
   TEST(0 == delete_testmmpage(&mmpage)) ;
   TEST(0 == mmpage) ;
   TEST(0 == init_vmmappedregions(&mapping)) ;
   TEST(1 == isunmapped_vmmappedregions(&mapping, &page[0])) ;
   TEST(1 == isunmapped_vmmappedregions(&mapping, &page[1])) ;
   TEST(1 == isunmapped_vmmappedregions(&mapping, &page[2])) ;
   TEST(1 == isunmapped_vmmappedregions(&mapping, &page[3])) ;
   TEST(0 == free_vmmappedregions(&mapping)) ;

   // TEST init ENOMEM
   TEST(ENOMEM == new_testmmpage(&mmpage, 16*1024*1024, 0)) ;
   TEST(0 == mmpage) ;

   // TEST newblock_testmmpage
   TEST(0 == new_testmmpage(&mmpage, 0, 0)) ;
   memblock_t nextfree = mmpage->freeblock ;
   for (unsigned i = 1; i <= 1000; ++i) {
      const size_t   alignsize = alignsize_testmmblock(i) ;
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0      == newblock_testmmpage(mmpage, i, &memblock)) ;
      TEST(true   == isblockvalid_testmmpage(mmpage, &memblock)) ;
      TEST(ENOMEM == newblock_testmmpage(mmpage, nextfree.size+1-headersize-trailersize, &memblock)) ;
      nextfree.addr += headersize ;
      TEST(memblock.addr == nextfree.addr) ;
      TEST(memblock.size == i) ;
      nextfree.addr += trailersize + alignsize;
      nextfree.size -= headersize + trailersize + alignsize ;
      TEST(nextfree.addr == mmpage->freeblock.addr) ;
      TEST(nextfree.size == mmpage->freeblock.size) ;
   }
   TEST(0 == delete_testmmpage(&mmpage)) ;

   // TEST resizeblock_testmmpage
   TEST(0 == new_testmmpage(&mmpage, 0, 0)) ;
   nextfree = mmpage->freeblock ;
   memblock_t oldblock = memblock_INIT_FREEABLE ;
   for (unsigned i = 1; i <= 1000; ++i) {
      const size_t   alignsize = alignsize_testmmblock(i+1) ;
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == newblock_testmmpage(mmpage, i, &memblock)) ;
      TEST(0 == resizeblock_testmmpage(mmpage, i+1, &memblock)) ;
      TEST(1 == isblockvalid_testmmpage(mmpage, &memblock)) ;
      nextfree.addr += headersize ;
      TEST(memblock.addr == nextfree.addr) ;
      TEST(memblock.size == i+1) ;
      nextfree.addr += trailersize + alignsize;
      nextfree.size -= headersize + trailersize + alignsize ;
      TEST(nextfree.addr == mmpage->freeblock.addr) ;
      TEST(nextfree.size == mmpage->freeblock.size) ;
      if (!isfree_memblock(&oldblock)) {
         TEST(ENOMEM == resizeblock_testmmpage(mmpage, i+1, &oldblock)) ;
      }
      oldblock = memblock ;
   }
   TEST(0 == delete_testmmpage(&mmpage)) ;

   // TEST: resizeblock_testmmpage
   TEST(0 == new_testmmpage(&mmpage, 0, 0)) ;
   TEST(0 == newblock_testmmpage(mmpage, 1024, &memblock)) ;
   TEST(0 == alignsize_testmmblock(SIZE_MAX)) ;
   TEST(ENOMEM == resizeblock_testmmpage(mmpage, SIZE_MAX, &memblock)) ;
   TEST(ENOMEM == resizeblock_testmmpage(mmpage, SIZE_MAX/2, &memblock)) ;
   TEST(0 == delete_testmmpage(&mmpage)) ;

   // TEST freeblock_testmmpage
   TEST(0 == new_testmmpage(&mmpage, 0, 0)) ;
   nextfree = mmpage->freeblock ;
   oldblock = (memblock_t) memblock_INIT_FREEABLE ;
   for (unsigned i = 1; i <= 1000; ++i) {
      const size_t   alignsize = alignsize_testmmblock(i+1) ;
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == newblock_testmmpage(mmpage, i, &memblock)) ;
      TEST(0 == resizeblock_testmmpage(mmpage, i+1, &memblock)) ;
      TEST(1 == isblockvalid_testmmpage(mmpage, &memblock)) ;
      nextfree.addr += headersize ;
      TEST(memblock.addr == nextfree.addr) ;
      TEST(memblock.size == i+1) ;
      nextfree.addr += trailersize + alignsize;
      nextfree.size -= headersize + trailersize + alignsize ;
      TEST(nextfree.addr == mmpage->freeblock.addr) ;
      TEST(nextfree.size == mmpage->freeblock.size) ;
      if (!isfree_memblock(&oldblock)) {
         // is only marked as free
         TEST(0 == freeblock_testmmpage(mmpage, &oldblock)) ;
         TEST(0 == oldblock.addr) ;
         TEST(0 == oldblock.size) ;
      }
      oldblock = memblock ;
   }
   // all free blocks are merged
   TEST(0 == freeblock_testmmpage(mmpage, &oldblock)) ;
   TEST(isfree_memblock(&oldblock)) ;
   TEST(mmpage->freeblock.addr == mmpage->datablock.addr) ;
   TEST(mmpage->freeblock.size == mmpage->datablock.size) ;
   TEST(0 == delete_testmmpage(&mmpage)) ;

   // TEST isblockvalid_testmmpage
   TEST(0 == new_testmmpage(&mmpage, 0, 0)) ;
   nextfree = mmpage->freeblock ;
   TEST(0 == newblock_testmmpage(mmpage, 1000000, &memblock)) ;
   mmpage->datablock.addr += 1 ;
   TEST(0 == isblockvalid_testmmpage(mmpage, &memblock)) ;
   mmpage->datablock.addr -= 1 ;
   TEST(1 == isblockvalid_testmmpage(mmpage, &memblock)) ;
   mmpage->freeblock.addr -= 1 ;
   TEST(0 == isblockvalid_testmmpage(mmpage, &memblock)) ;
   mmpage->freeblock.addr += 1 ;
   TEST(1 == isblockvalid_testmmpage(mmpage, &memblock)) ;
   ((testmm_block_t*)(memblock.addr-headersize))->header.alignsize = mmpage->datablock.size ;
   TEST(0 == isblockvalid_testmmpage(mmpage, &memblock)) ;
   ((testmm_block_t*)(memblock.addr-headersize))->header.alignsize = alignsize_testmmblock(memblock.size) ;
   TEST(1 == isblockvalid_testmmpage(mmpage, &memblock)) ;
   TEST(0 == delete_testmmpage(&mmpage)) ;

   return 0 ;
ONABORT:
   delete_testmmpage(&mmpage) ;
   free_vmmappedregions(&mapping) ;
   return EINVAL ;
}

static int test_initfree(void)
{
   mm_t         mmobj    = mm_INIT_FREEABLE ;
   testmm_t     testmm   = testmm_INIT_FREEABLE ;
   memblock_t   memblock = memblock_INIT_FREEABLE ;
   const size_t headersize = alignsize_testmmblock(sizeof(((testmm_block_t*)0)->header)) ;

   // TEST static init
   TEST(0 == testmm.mmpage) ;
   TEST(0 == testmm.sizeallocated) ;
   TEST(0 == testmm.simulateResizeError) ;
   TEST(0 == testmm.simulateFreeError) ;

   // TEST init, double free
   testmm.sizeallocated = 1 ;
   testmm.simulateResizeError = (void*) 1 ;
   testmm.simulateFreeError   = (void*) 1 ;
   TEST(0 == init_testmm(&testmm)) ;
   TEST(0 != testmm.mmpage) ;
   TEST(0 == testmm.sizeallocated) ;
   TEST(0 == testmm.simulateResizeError) ;
   TEST(0 == testmm.simulateFreeError) ;
   TEST(0 == mresize_testmm(&testmm, 1, &memblock)) ;
   TEST(1 == testmm.sizeallocated) ;
   testmm.simulateResizeError = (void*) 1 ;
   testmm.simulateFreeError   = (void*) 1 ;
   TEST(0 == free_testmm(&testmm)) ;
   TEST(0 == testmm.mmpage) ;
   TEST(0 == testmm.sizeallocated) ;
   TEST(0 == testmm.simulateResizeError) ;
   TEST(0 == testmm.simulateFreeError) ;
   TEST(0 == free_testmm(&testmm)) ;
   TEST(0 == testmm.mmpage) ;
   TEST(0 == testmm.sizeallocated) ;

   // TEST free: free all pages
   TEST(0 == init_testmm(&testmm)) ;
   for (unsigned i = 0; i < 10; ++i) {
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_testmm(&testmm, 1024*1024-1000, &memblock)) ;
   }
   TEST(10*(1024*1024-1000) == testmm.sizeallocated) ;
   TEST(0 == free_testmm(&testmm)) ;
   TEST(0 == testmm.mmpage) ;
   TEST(0 == testmm.sizeallocated) ;
   TEST(0 == free_testmm(&testmm)) ;
   TEST(0 == testmm.mmpage) ;
   TEST(0 == testmm.sizeallocated) ;

   // TEST initiot, double freeiot
   TEST(0 == mmobj.object) ;
   TEST(0 == mmobj.iimpl) ;
   TEST(0 == initasmm_testmm(&mmobj)) ;
   TEST(mmobj.object == (void*) (((testmm_t*)mmobj.object)->mmpage->datablock.addr + headersize)) ;
   TEST(mmobj.iimpl  == genericcast_mmit(&s_testmm_interface, testmm_t)) ;
   TEST(0 == freeasmm_testmm(&mmobj)) ;
   TEST(0 == mmobj.object) ;
   TEST(0 == mmobj.iimpl) ;

   return 0 ;
ONABORT:
   free_testmm(&testmm) ;
   freeasmm_testmm(&mmobj) ;
   return EINVAL ;
}

static int test_allocate(void)
{
   memblock_t     memblocks[1000] ;
   const size_t   blocksize   = 10*1024*1024 / lengthof(memblocks) ;
   testmm_t       testmm      = testmm_INIT_FREEABLE ;
   const size_t   headersize  = alignsize_testmmblock(sizeof(((testmm_block_t*)0)->header)) ;

   // prepare
   TEST(0 == init_testmm(&testmm)) ;

   // TEST mresize_testmm: alloc, realloc in FIFO order
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      // allocate blocksize/2 and then resize to blocksize
      memblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_testmm(&testmm, blocksize/2, &memblocks[i])) ;
      TEST(i * blocksize + blocksize/2 == sizeallocated_testmm(&testmm))
      TEST(memblocks[i].addr != 0) ;
      TEST(memblocks[i].size == blocksize/2) ;
      uint8_t        * oldaddr = memblocks[i].addr ;
      testmm_page_t  * oldpage = testmm.mmpage ;
      TEST(0 == mresize_testmm(&testmm, blocksize, &memblocks[i])) ;
      TEST((i+1) * blocksize == sizeallocated_testmm(&testmm))
      TEST(memblocks[i].addr != 0) ;
      TEST(memblocks[i].size == blocksize) ;
      if (oldpage == testmm.mmpage) { // oldpage had enough space
         TEST(memblocks[i].addr == oldaddr) ;
      } else { // new page allocated, new block is first on page
         TEST(memblocks[i].addr == testmm.mmpage->datablock.addr + headersize) ;
      }
   }

   // TEST mfree_testmm, mresize_testmm: free in FIFO order
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      testmm_page_t  * oldpage = testmm.mmpage ;
      if (i % 2) {
         TEST(0 == mresize_testmm(&testmm, 0, &memblocks[i])) ;
      } else {
         TEST(0 == mfree_testmm(&testmm, &memblocks[i])) ;
      }
      TEST(0 == memblocks[i].addr) ;
      TEST(0 == memblocks[i].size) ;
      TEST((lengthof(memblocks)-1-i) * blocksize == sizeallocated_testmm(&testmm)) ;
      if (i != lengthof(memblocks)-1) {  // no pages are freed
         TEST(oldpage == testmm.mmpage) ;
      } else {         // testmm has now only a single empty page
         TEST(0 == testmm.mmpage->next) ;
         TEST(1 == ispagefree_testmmpage(testmm.mmpage)) ;
      }
   }
   TEST(0 == sizeallocated_testmm(&testmm)) ;

   // TEST malloc_testmm, mfree_testmm: LIFO order
   memset(memblocks, 255, sizeof(memblocks)) ;
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      TEST(0 == malloc_testmm(&testmm, blocksize, &memblocks[i])) ;
      TEST((i+1) * blocksize == sizeallocated_testmm(&testmm))
   }
   for (unsigned i = lengthof(memblocks); (i--) > 0; ) {
      testmm_page_t  * oldpage   = testmm.mmpage ;
      testmm_page_t  * foundpage = findpage_testmm(&testmm, memblocks[i].addr) ;
      bool           isfirst     = (foundpage->datablock.addr + headersize == memblocks[i].addr) ;
      bool           isRootPage  = (0 != foundpage->next) && (0 == foundpage->next->next) ;
      testmm_page_t  * rootpage  = isRootPage ? foundpage->next : 0 ;

      if (foundpage == oldpage) {
         TEST(!ispagefree_testmmpage(oldpage)) ;
      } else {
         TEST(ispagefree_testmmpage(oldpage)) ;
         TEST(foundpage == oldpage->next) ;
      }
      TEST(0 == mfree_testmm(&testmm, &memblocks[i])) ;
      TEST(i * blocksize == sizeallocated_testmm(&testmm)) ;
      TEST(0 == memblocks[i].addr) ;
      TEST(0 == memblocks[i].size) ;
      if (isfirst && oldpage != foundpage) {
         if (isRootPage) {
            TEST(rootpage == testmm.mmpage) ;
         } else {
            TEST(foundpage == testmm.mmpage) ;
         }
      } else {
         TEST(oldpage == testmm.mmpage) ;
      }
   }
   TEST(0 == testmm.mmpage->next) ;
   TEST(1 == ispagefree_testmmpage(testmm.mmpage)) ;

   // TEST malloc_testmm, mresize_testmm, mfree_testmm: random order
   srandom(10000) ;
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      memblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
   }
   for (size_t ri = 0, datasize = 0; ri < 100000; ++ri) {
      unsigned i = (unsigned)random() % lengthof(memblocks) ;
      if (isfree_memblock(&memblocks[i])) {
         datasize += blocksize/2 ;
         TEST(0 == malloc_testmm(&testmm, blocksize/2, &memblocks[i])) ;
         TEST(0 != memblocks[i].addr) ;
         TEST(blocksize/2 == memblocks[i].size) ;
      } else if (blocksize == memblocks[i].size) {
         datasize -= blocksize ;
         TEST(0 == mfree_testmm(&testmm, &memblocks[i])) ;
         TEST(0 == memblocks[i].addr) ;
         TEST(0 == memblocks[i].size) ;
      } else {
         datasize -= memblocks[i].size ;
         datasize += blocksize ;
         TEST(0 == mresize_testmm(&testmm, blocksize, &memblocks[i])) ;
         TEST(0 != memblocks[i].addr) ;
         TEST(blocksize == memblocks[i].size) ;
      }
      TEST(datasize == sizeallocated_testmm(&testmm)) ;
   }
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      TEST(0 == mfree_testmm(&testmm, &memblocks[i])) ;
   }
   TEST(0 == sizeallocated_testmm(&testmm)) ;
   TEST(0 == testmm.mmpage->next) ;
   TEST(1 == ispagefree_testmmpage(testmm.mmpage)) ;

   // TEST reallocation preserves content
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      memblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_testmm(&testmm, sizeof(unsigned)*20, &memblocks[i])) ;
      TEST(sizeof(unsigned)*20 == memblocks[i].size) ;
      for (unsigned off = 0; off < 20; ++off) {
         ((unsigned*)(memblocks[i].addr))[off] = i + off ;
      }
   }
   for (unsigned i = lengthof(memblocks); (i--) > 0; ) {
      TEST(0 == mresize_testmm(&testmm, sizeof(unsigned)*21, &memblocks[i])) ;
      TEST(sizeof(unsigned)*21 == memblocks[i].size) ;
      for (unsigned off = 20; off < 21; ++off) {
         ((unsigned*)(memblocks[i].addr))[off] = i + off ;
      }
   }
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      TEST(0 == mresize_testmm(&testmm, sizeof(unsigned)*22, &memblocks[i])) ;
      TEST(sizeof(unsigned)*22 == memblocks[i].size) ;
      for (unsigned off = 21; off < 22; ++off) {
         ((unsigned*)(memblocks[i].addr))[off] = i + off ;
      }
   }
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      for (unsigned off = 0; off < 22; ++off) {
         TEST(((unsigned*)(memblocks[i].addr))[off] == i + off) ;
      }
   }
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      TEST(0 == mfree_testmm(&testmm, &memblocks[i])) ;
      TEST(0 == memblocks[i].addr) ;
      TEST(0 == memblocks[i].size) ;
   }
   TEST(0 == sizeallocated_testmm(&testmm)) ;
   TEST(0 == testmm.mmpage->next) ;
   TEST(1 == ispagefree_testmmpage(testmm.mmpage)) ;

   // TEST setresizeerr_testmm, setfreeeerr_testmm
   test_errortimer_t errtimer1 ;
   test_errortimer_t errtimer2 ;
   init_testerrortimer(&errtimer1, 2, EPROTO) ;
   init_testerrortimer(&errtimer2, 5, EPERM) ;
   TEST(0 == testmm.simulateResizeError)
   setresizeerr_testmm(&testmm, &errtimer1) ;
   TEST(&errtimer1 == testmm.simulateResizeError)
   TEST(0 == testmm.simulateFreeError)
   setfreeerr_testmm(&testmm, &errtimer2) ;
   TEST(&errtimer2 == testmm.simulateFreeError)
   static_assert(lengthof(memblocks) > 2, "") ;
   memblocks[0] = (memblock_t) memblock_INIT_FREEABLE ;
   TEST(0 == mresize_testmm(&testmm, sizeof(unsigned)*20, &memblocks[0])) ;
   memblocks[1] = (memblock_t) memblock_INIT_FREEABLE ;
   TEST(EPROTO == mresize_testmm(&testmm, sizeof(unsigned)*20, &memblocks[1])) ;
   TEST(0 == mresize_testmm(&testmm, sizeof(unsigned)*20, &memblocks[1])) ;
   for (unsigned i = 0; i < 2; ++i) {
      TEST(0 == mfree_testmm(&testmm, &memblocks[0])) ;
      TEST(0 == mfree_testmm(&testmm, &memblocks[1])) ;
   }
   TEST(EPERM == mfree_testmm(&testmm, &memblocks[0])) ;
   TEST(0 == mfree_testmm(&testmm, &memblocks[1])) ;

   // TEST setresizeerr_testmm, setfreeeerr_testmm: after firing pointer to timer are cleared
   init_testerrortimer(&errtimer1, 1, ENOMEM) ;
   init_testerrortimer(&errtimer2, 1, EINVAL) ;
   TEST(0 == testmm.simulateResizeError) ;
   setresizeerr_testmm(&testmm, &errtimer1) ;
   TEST(&errtimer1 == testmm.simulateResizeError) ;
   TEST(ENOMEM == mresize_testmm(&testmm, sizeof(unsigned)*20, &memblocks[0])) ;
   TEST(0 == testmm.simulateResizeError) ;
   TEST(0 == testmm.simulateFreeError)
   setfreeerr_testmm(&testmm, &errtimer2) ;
   TEST(&errtimer2 == testmm.simulateFreeError)
   TEST(EINVAL == mfree_testmm(&testmm, &memblocks[0])) ;
   TEST(0 == testmm.simulateFreeError)

   // TEST setresizeerr_testmm, setfreeeerr_testmm: value 0 disarms error timer
   init_testerrortimer(&errtimer1, 1, ENOMEM) ;
   init_testerrortimer(&errtimer2, 1, EINVAL) ;
   setresizeerr_testmm(&testmm, &errtimer1) ;
   setfreeerr_testmm(&testmm, &errtimer2) ;
   setresizeerr_testmm(&testmm, 0) ;
   setfreeerr_testmm(&testmm, 0) ;
   TEST(0 == mresize_testmm(&testmm, sizeof(unsigned)*20, &memblocks[0])) ;
   TEST(0 == mfree_testmm(&testmm, &memblocks[0])) ;

   // unprepare
   TEST(0 == free_testmm(&testmm)) ;

   return 0 ;
ONABORT:
   free_testmm(&testmm) ;
   return EINVAL ;
}

static int test_context(void)
{
   mm_t oldmm ;
   initcopy_iobj(&oldmm, &mm_maincontext()) ;

   // TEST double call switchon_testmm
   TEST(genericcast_mmit(&s_testmm_interface, testmm_t) != mm_maincontext().iimpl) ;
   TEST(0 == switchon_testmm()) ;
   TEST(genericcast_mmit(&s_testmm_interface, testmm_t) == mm_maincontext().iimpl) ;
   TEST(0 == switchon_testmm()) ;
   TEST(genericcast_mmit(&s_testmm_interface, testmm_t) == mm_maincontext().iimpl) ;

   // TEST double call switchoff_testmm
   TEST(genericcast_mmit(&s_testmm_interface, testmm_t) == mm_maincontext().iimpl) ;
   TEST(0 == switchoff_testmm()) ;
   TEST(genericcast_mmit(&s_testmm_interface, testmm_t) != mm_maincontext().iimpl) ;
   TEST(oldmm.object == mm_maincontext().object) ;
   TEST(oldmm.iimpl  == mm_maincontext().iimpl) ;
   TEST(0 == switchoff_testmm()) ;
   TEST(oldmm.object == mm_maincontext().object) ;
   TEST(oldmm.iimpl  == mm_maincontext().iimpl) ;

   return 0 ;
ONABORT:
   switchoff_testmm() ;
   return EINVAL ;
}

int unittest_test_mm_testmm()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   const bool istestmm = isinstalled_testmm() ;

   if (istestmm) {
      switchoff_testmm() ;
   }

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_testmmpage())  goto ONABORT ;
   if (test_initfree())    goto ONABORT ;
   if (test_allocate())    goto ONABORT ;
   if (test_context())     goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   if (istestmm) {
      switchon_testmm() ;
   }

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
