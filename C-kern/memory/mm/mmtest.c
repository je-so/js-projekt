/* title: TestMemoryManager impl
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

   file: C-kern/api/memory/mm/mmtest.h
    Header file <TestMemoryManager>.

   file: C-kern/memory/mm/mmtest.c
    Implementation file <TestMemoryManager impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/mm/mmtest.h"
#include "C-kern/api/memory/mm/mm_it.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/virtmemory.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

/* typedef: struct mmtest_it
 * Shortcut for <mmtest_it>. */
typedef struct mmtest_it               mmtest_it ;

/* typedef: struct mmtestblock_t
 * Shortcut for <mmtestblock_t>. */
typedef struct mmtestblock_t           mmtestblock_t ;

/* typedef: struct mmtest_page_t
 * Shortcut for <mmtest_page_t>. */
typedef struct mmtest_page_t           mmtest_page_t ;


/* struct: mmtest_it
 * Declares <mmtest_it> as subtype of <mm_it>. See <mm_it_DECLARE>. */
mm_it_DECLARE(mmtest_it, mmtest_t)


/* struct: mmtestblock_t
 * Describes the header of an allocated memory block. */
struct mmtestblock_t {
   struct {
      size_t               datasize ;
      size_t               alignsize ;
      uint8_t              fillvalue ;
      uint8_t              * userdata ;
      void                 * fill[1] ;
   } header ;

   // user data

   struct {
      mmtestblock_t        * header[2] ;
   } trailer ;
} ;

/* function: alignsize_mmtestblock
 * Aligns a value to the next multiple of KONFIG_MEMALIGN.
 * If KONFIG_MEMALIGN is less than sizeof(void*) the value sizeof(void*)
 * is chosen instead of KONFIG_MEMALIGN.
 *
 * Precondition:
 * KONFIG_MEMALIGN must be a power of two for this algorithm to work.
 * > return (bytesize + (memalign-1)) & (~(memalign-1)) ;
 * > memalign == 0b001000
 * > (~(memalign-1)) == ~ (0b001000 - 1) == ~ (0b000111) == 0b111000 */
static inline size_t alignsize_mmtestblock(size_t bytesize)
{
   const size_t memalign = ((KONFIG_MEMALIGN > sizeof(void*)) ? KONFIG_MEMALIGN : sizeof(void*)) ;
   static_assert(0 == (memalign & (memalign-1)), "memalign must be power of two") ;
   return (bytesize + (memalign-1)) & (~(memalign-1)) ;
}

static void init_mmtestblock(
    mmtestblock_t        * block
   ,size_t               datasize
   ,size_t               alignsize)
{
   const size_t headersize  =  alignsize_mmtestblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_mmtestblock(sizeof(block->trailer)) ;

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

static bool isvalid_mmtestblock(mmtestblock_t * block, struct memblock_t * memblock)
{
   const size_t headersize  =  alignsize_mmtestblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_mmtestblock(sizeof(block->trailer)) ;
   const size_t alignsize   =  alignsize_mmtestblock(memblock->size) ;

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


/* struct: mmtest_page_t
 * Holds one big data block consisting of many vm memory pages.
 * The memory allocation strategy is simple. Memory allocation
 * requests are always satified from the beginning of the last free block.
 * If it is too small allocation fails. Freed blocks are only marked as freed.
 * If a memory block marked as free is directly before the last free block
 * it is merged with. The last free block grows in this manner until all
 * freed blocks are merged.
 */
struct mmtest_page_t {
   vm_block_t     vmblock ;
   memblock_t     datablock ;
   memblock_t     freeblock ;
   mmtest_page_t  * next ;
} ;

// group: lifetime

static int new_mmtestpage(mmtest_page_t ** mmpage, size_t minblocksize, mmtest_page_t * next)
{
   int err ;
   const size_t   blocksize = 1024 * 1024 ;
   const size_t   nrpages   = ((blocksize - 1) + pagesize_vm()) / pagesize_vm() ;
   const size_t   nrpages2  = 2 + ((sizeof(mmtest_page_t) - 1 + pagesize_vm()) / pagesize_vm()) ;
   vm_block_t     vmblock   = vm_block_INIT_FREEABLE ;
   mmtest_page_t  * new_mmpage ;

   const size_t headersize  =  alignsize_mmtestblock(sizeof(((mmtestblock_t*)0)->header)) ;
   const size_t trailersize =  alignsize_mmtestblock(sizeof(((mmtestblock_t*)0)->trailer)) ;

   if (blocksize < minblocksize + headersize + trailersize) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   err = init_vmblock(&vmblock, nrpages + nrpages2) ;
   if (err) goto ONABORT ;

   new_mmpage = (mmtest_page_t*) vmblock.addr ;
   new_mmpage->vmblock   = vmblock ;
   const size_t datasize = nrpages * pagesize_vm() ;
   new_mmpage->datablock = (memblock_t) memblock_INIT(datasize, vmblock.addr + vmblock.size - pagesize_vm() - datasize) ;
   new_mmpage->freeblock = new_mmpage->datablock ;
   new_mmpage->next      = next ;

   err = protect_vmblock(&(memblock_t)memblock_INIT(pagesize_vm(), new_mmpage->datablock.addr - pagesize_vm()), accessmode_NONE) ;
   if (err) goto ONABORT ;
   err = protect_vmblock(&(memblock_t)memblock_INIT(pagesize_vm(), new_mmpage->datablock.addr + new_mmpage->datablock.size), accessmode_NONE) ;
   if (err) goto ONABORT ;

   *mmpage = new_mmpage ;

   return 0 ;
ONABORT:
   free_vmblock(&vmblock) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

static int delete_mmtestpage(mmtest_page_t ** mmpage)
{
   int err ;
   mmtest_page_t * del_mmpage = *mmpage ;

   if (del_mmpage) {
      *mmpage = 0 ;

      vm_block_t  vmblock = del_mmpage->vmblock ;
      err = free_vmblock(&vmblock) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

static int ispagefree_mmtestpage(mmtest_page_t * mmpage)
{
   return (mmpage->datablock.addr == mmpage->freeblock.addr) ;
}

static int isblockvalid_mmtestpage(mmtest_page_t * mmpage, struct memblock_t * memblock)
{
   const size_t headersize  =  alignsize_mmtestblock(sizeof(((mmtestblock_t*)0)->header)) ;
   const size_t trailersize =  alignsize_mmtestblock(sizeof(((mmtestblock_t*)0)->trailer)) ;

   mmtestblock_t * block = (mmtestblock_t*) (memblock->addr - headersize) ;

   if (     mmpage->datablock.addr + headersize > memblock->addr
         || mmpage->freeblock.addr <= memblock->addr
         || mmpage->datablock.size <= block->header.alignsize
         || (size_t)(mmpage->freeblock.addr - memblock->addr) < block->header.alignsize + trailersize) {
      return false ;
   }

   return isvalid_mmtestblock(block, memblock) ;
}

static int freeblock_mmtestpage(mmtest_page_t * mmpage, struct memblock_t * memblock)
{
   int err ;
   mmtestblock_t  * block ;
   const size_t headersize  =  alignsize_mmtestblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_mmtestblock(sizeof(block->trailer)) ;

   VALIDATE_INPARAM_TEST(isblockvalid_mmtestpage(mmpage, memblock), ONABORT, ) ;

   block = (mmtestblock_t*) (memblock->addr - headersize) ;
   block->header.datasize = 0 ;

   if (mmpage->freeblock.addr == memblock->addr + trailersize + block->header.alignsize) {
      // block is adjacent to the free block
      // instead of marking it only as freed let freeblock grow

      // check if the block before is also free and use it as new border
      while ((uint8_t*)block > mmpage->datablock.addr + trailersize) {
         typeof(block->trailer) * trailer = (typeof(block->trailer)*) ((uint8_t*)block - trailersize) ;
         mmtestblock_t * block2 = trailer->header[0] ;

         if (  (uint8_t*)block2 < mmpage->datablock.addr
            || 0 != block2->header.datasize) {
            break ; // block2 in use
         }

         block = block2 ;
      }

      (void) grow_memblock(&mmpage->freeblock, (size_t)(mmpage->freeblock.addr - (uint8_t*)block)) ;
   }

   *memblock = (memblock_t) memblock_INIT_FREEABLE ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int newblock_mmtestpage(mmtest_page_t * mmpage, size_t newsize, struct memblock_t * memblock)
{
   int err ;
   mmtestblock_t           * block ;
   const size_t headersize  =  alignsize_mmtestblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_mmtestblock(sizeof(block->trailer)) ;
   const size_t alignsize   =  alignsize_mmtestblock(newsize) ;
   const size_t blocksize   =  headersize + trailersize + alignsize ;

   block = (mmtestblock_t*) mmpage->freeblock.addr ;

   err = shrink_memblock(&mmpage->freeblock, blocksize) ;
   if (err) return err ;

   init_mmtestblock(block, newsize, alignsize) ;

   memblock->addr = block->header.userdata ;
   memblock->size = newsize ;

   return 0 ;
}

static int resizeblock_mmtestpage(mmtest_page_t * mmpage, size_t newsize, struct memblock_t * memblock)
{
   int err ;
   mmtestblock_t  * block ;
   const size_t headersize  =  alignsize_mmtestblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_mmtestblock(sizeof(block->trailer)) ;
   const size_t alignsize   =  alignsize_mmtestblock(newsize) ;

   VALIDATE_INPARAM_TEST(isblockvalid_mmtestpage(mmpage, memblock), ONABORT, ) ;

   block = (mmtestblock_t*) (memblock->addr - headersize) ;

   if (mmpage->freeblock.addr != memblock->addr + trailersize + block->header.alignsize) {
      return ENOMEM ;
   }

   if (     alignsize > block->header.alignsize
         && mmpage->freeblock.size < (alignsize - block->header.alignsize))  {
      return ENOMEM ;
   }

   mmpage->freeblock.addr -= block->header.alignsize ;
   mmpage->freeblock.size += block->header.alignsize ;
   mmpage->freeblock.addr += alignsize ;
   mmpage->freeblock.size -= alignsize ;

   init_mmtestblock(block, newsize, alignsize) ;

   memblock->size = newsize ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int getblock_mmtestpage(mmtest_page_t * mmpage, size_t blockindex, /*out*/struct memblock_t * memblock)
{
   int err ;
   mmtestblock_t   * block   =  (mmtestblock_t*) mmpage->datablock.addr ;
   const size_t headersize  =  alignsize_mmtestblock(sizeof(block->header)) ;
   const size_t trailersize =  alignsize_mmtestblock(sizeof(block->trailer)) ;

   for (size_t i = 0; i < blockindex; ++i) {

      if (block->header.datasize) {
         memblock_t temp = memblock_INIT(block->header.datasize, block->header.userdata) ;
         if (!isblockvalid_mmtestpage(mmpage, &temp)) {
            err = EINVAL ;
            PRINTSIZE_LOG(blockindex) ;
            goto ONABORT ;
         }
      }

      block = (mmtestblock_t*) ( ((uint8_t*)block) + headersize + trailersize + block->header.alignsize) ;
   }

   *memblock = (memblock_t) memblock_INIT(block->header.datasize, ((uint8_t*)block) + headersize) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// section: mmtest_t

// group: variables

/* variable: s_mmtest_interface
 * Contains single instance of interface <mmtest_it>. */
static mmtest_it     s_mmtest_interface = mm_it_INIT(
                         &mresize_mmtest
                        ,&mfree_mmtest
                        ,&sizeallocated_mmtest
                     ) ;

// group: helper

static int addpage_mmtest(mmtest_t * mman, size_t newsize)
{
   int err ;
   mmtest_page_t * mmpage ;

   err = new_mmtestpage(&mmpage, newsize, mman->mmpage) ;
   if (err) return err ;

   mman->mmpage = mmpage ;
   return 0 ;
}

static mmtest_page_t * findpage_mmtest(mmtest_t * mman, uint8_t * blockaddr)
{
   mmtest_page_t * mmpage = mman->mmpage ;

   do {
      if (  mmpage->datablock.addr <= blockaddr
         && (size_t) (blockaddr - mmpage->datablock.addr) < mmpage->datablock.size) {
         break ; // found
      }
      mmpage = mmpage->next ;
   } while (mmpage) ;

   return mmpage ;
}

static int mallocate_mmtest(mmtest_t * mman, size_t newsize, struct memblock_t * memblock)
{
   int err ;

   err = newblock_mmtestpage(mman->mmpage, newsize, memblock) ;

   if (err) {
      if (ENOMEM != err) return err ;

      err = addpage_mmtest(mman, newsize) ;
      if (err) return err ;

      err = newblock_mmtestpage(mman->mmpage, newsize, memblock) ;
   }

   return err ;
}

// group: context

mmtest_t * mmcontext_mmtest(void)
{
   if (&s_mmtest_interface.generic != mmtransient_maincontext().iimpl) {
      return 0 ;
   }

   return (mmtest_t*) mmtransient_maincontext().object ;
}

int switchon_mmtest()
{
   int err ;
   mm_iot mmtest = mm_iot_INIT_FREEABLE ;

   if (&s_mmtest_interface.generic != mmtransient_maincontext().iimpl) {
      memblock_t  previous_mm = memblock_INIT_FREEABLE ;

      err = initiot_mmtest(&mmtest) ;
      if (err) goto ONABORT ;

      err = mmtest.iimpl->mresize(mmtest.object, sizeof(mm_iot), &previous_mm) ;
      if (err) goto ONABORT ;

      *((mm_iot*)previous_mm.addr) = mmtransient_maincontext() ;

      mmtransient_maincontext() = mmtest ;
   }

   return 0 ;
ONABORT:
   freeiot_mmtest(&mmtest) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int switchoff_mmtest()
{
   int err ;

   if (&s_mmtest_interface.generic == mmtransient_maincontext().iimpl) {
      mm_iot      mmiot       = mmtransient_maincontext() ;
      mmtest_t    * mmtest    = ((mmtest_t*)mmiot.object) ;
      memblock_t  previous_mm = memblock_INIT_FREEABLE ;
      mmtest_page_t  * mmpage = mmtest->mmpage ;

      while (mmpage->next) {
         mmpage = mmpage->next ;
      }

      err = getblock_mmtestpage(mmpage, 1, &previous_mm) ;
      if (err) goto ONABORT ;

      if (sizeof(mm_iot) != previous_mm.size) {
         err = EINVAL ;
         goto ONABORT ;
      }

      mmtransient_maincontext() = *((mm_iot*)previous_mm.addr) ;

      err = freeiot_mmtest(&mmiot) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: lifetime

int init_mmtest(/*out*/mmtest_t * mman)
{
   int err ;
   mmtest_page_t  * mmpage = 0 ;

   err = new_mmtestpage(&mmpage, 0, 0) ;
   if (err) goto ONABORT ;

   mman->mmpage        = mmpage ;
   mman->sizeallocated = 0 ;
   mman->simulateResizeError = 0 ;
   mman->simulateFreeError   = 0 ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_mmtest(mmtest_t * mman)
{
   int err ;
   int err2 ;

   if (mman->mmpage) {
      mmtest_page_t * mmpage = mman->mmpage->next ;

      err = delete_mmtestpage(&mman->mmpage) ;
      while (mmpage) {
         mmtest_page_t * delmmpage = mmpage ;
         mmpage = mmpage->next ;
         err2 = delete_mmtestpage(&delmmpage) ;
         if (err2) err = err2 ;
      }

      mman->sizeallocated = 0 ;
      mman->simulateResizeError = 0 ;
      mman->simulateFreeError   = 0 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int initiot_mmtest(/*out*/mm_iot * mmtest)
{
   int err ;
   mmtest_t       mmtestobj = mmtest_INIT_FREEABLE ;
   memblock_t     memblock  = memblock_INIT_FREEABLE ;
   const size_t   objsize   = sizeof(mmtest_t) ;

   err = init_mmtest(&mmtestobj) ;
   if (err) goto ONABORT ;

   err = mresize_mmtest(&mmtestobj, objsize, &memblock) ;
   if (err) goto ONABORT ;

   memcpy(memblock.addr, &mmtestobj, objsize) ;

   *mmtest = (mm_iot) mm_iot_INIT((struct mm_t*) memblock.addr, &s_mmtest_interface.generic) ;

   return err ;
ONABORT:
   free_mmtest(&mmtestobj) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freeiot_mmtest(mm_iot * mmtest)
{
   int err ;
   mmtest_t       mmtestobj = mmtest_INIT_FREEABLE ;
   const size_t   objsize   = sizeof(mmtest_t) ;

   if (mmtest->object) {
      assert(mmtest->iimpl == &s_mmtest_interface.generic) ;

      memcpy(&mmtestobj, mmtest->object, objsize) ;
      err = free_mmtest(&mmtestobj) ;

      *mmtest = (mm_iot) mm_iot_INIT_FREEABLE ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

size_t sizeallocated_mmtest(mmtest_t * mman)
{
   return mman->sizeallocated ;
}

// group: simulation

void setresizeerr_mmtest(mmtest_t * mman, struct test_errortimer_t * errtimer)
{
   mman->simulateResizeError = errtimer ;
}

void setfreeerr_mmtest(mmtest_t * mman, struct test_errortimer_t * errtimer)
{
   mman->simulateFreeError = errtimer ;
}

// group: allocate

int mresize_mmtest(mmtest_t * mman, size_t newsize, struct memblock_t * memblock)
{
   int err ;

   if (0 == newsize) {
      return mfree_mmtest(mman, memblock) ;
   }

   if (mman->simulateResizeError) {
      err = process_testerrortimer(mman->simulateResizeError) ;
      if (err) {
         mman->simulateResizeError = 0 ;
         goto ONABORT ;
      }
   }

   if (isfree_memblock(memblock)) {
      err = mallocate_mmtest(mman, newsize, memblock) ;
      if (err) goto ONABORT ;
   } else {
      mmtest_page_t * mmpage = findpage_mmtest(mman, memblock->addr) ;
      if (!mmpage) {
         err = EINVAL ;
         goto ONABORT ;
      }

      size_t freesize = memblock->size ;

      err = resizeblock_mmtestpage(mmpage, newsize, memblock) ;
      if (err) {
         if (ENOMEM != err) goto ONABORT ;

         memblock_t newmemblock ;

         err = mallocate_mmtest(mman, newsize, &newmemblock) ;
         if (err) goto ONABORT ;

         // copy content
         memcpy(newmemblock.addr, memblock->addr, memblock->size < newsize ? memblock->size : newsize) ;

         err = freeblock_mmtestpage(mmpage, memblock) ;
         if (err) {
            (void) freeblock_mmtestpage(mman->mmpage, &newmemblock) ;
            goto ONABORT ;
         }
         *memblock = newmemblock ;
      }

      mman->sizeallocated -= freesize ;
   }

   mman->sizeallocated += newsize ;

   return 0 ;
ONABORT:
   if (ENOMEM == err) {
      TRACEOUTOFMEM_LOG(newsize) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int mfree_mmtest(mmtest_t  * mman, struct memblock_t * memblock)
{
   int err ;

   if (!isfree_memblock(memblock)) {
      mmtest_page_t * mmpage = findpage_mmtest(mman, memblock->addr) ;
      if (!mmpage) {
         err = EINVAL ;
         goto ONABORT ;
      }

      size_t freesize = memblock->size ;

      err = freeblock_mmtestpage(mmpage, memblock) ;
      if (err) goto ONABORT ;

      *memblock = (memblock_t) memblock_INIT_FREEABLE ;
      mman->sizeallocated -= freesize ;

      if (ispagefree_mmtestpage(mman->mmpage)) {
         while (  mman->mmpage->next
                  && (  ispagefree_mmtestpage(mman->mmpage->next)
                        || !mman->mmpage->next->next)/*is first page*/) {
            mmpage = mman->mmpage;
            mman->mmpage = mman->mmpage->next ;
            err = delete_mmtestpage(&mmpage) ;
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
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_mmtestpage(void)
{
   mmtest_page_t        * mmpage = 0 ;
   vm_mappedregions_t   mapping  = vm_mappedregions_INIT_FREEABLE ;
   memblock_t           page[4] ;
   memblock_t           memblock ;
   const size_t         headersize  = alignsize_mmtestblock(sizeof(((mmtestblock_t*)0)->header)) ;
   const size_t         trailersize = alignsize_mmtestblock(sizeof(((mmtestblock_t*)0)->trailer)) ;

   // TEST init, double free
   TEST(0 == new_mmtestpage(&mmpage, 0, 0)) ;
   TEST(0 != mmpage) ;
   TEST(0 == delete_mmtestpage(&mmpage)) ;
   TEST(0 == mmpage) ;
   TEST(0 == delete_mmtestpage(&mmpage)) ;
   TEST(0 == mmpage) ;

   // TEST init: field contents
   TEST(0 == new_mmtestpage(&mmpage, 0, (mmtest_page_t*)5)) ;
   TEST(0 != mmpage) ;
   TEST(1024*1024 <= mmpage->datablock.size) ;
   TEST(mmpage->datablock.addr == mmpage->freeblock.addr) ;
   TEST(mmpage->datablock.size == mmpage->freeblock.size) ;
   TEST(mmpage->vmblock.addr   == (uint8_t*)mmpage) ;
      // assumes sizeof(mmtest_page_t) <= pagesize_vm()
   TEST(mmpage->vmblock.addr   == mmpage->datablock.addr - 2*pagesize_vm()) ;
   TEST(mmpage->vmblock.size   == mmpage->freeblock.size + 3*pagesize_vm()) ;
   TEST(5 == (uintptr_t)mmpage->next) ;
   TEST(0 == delete_mmtestpage(&mmpage)) ;
   TEST(0 == mmpage) ;

   // TEST init: memory is mapped
   TEST(0 == new_mmtestpage(&mmpage, 0, 0)) ;
   TEST(0 != mmpage) ;
   page[0] = (memblock_t) memblock_INIT(pagesize_vm(), mmpage->vmblock.addr) ;
   page[1] = mmpage->datablock ;
   page[2] = (memblock_t) memblock_INIT(pagesize_vm(), mmpage->datablock.addr + mmpage->datablock.size) ;
   page[3] = (memblock_t) memblock_INIT(pagesize_vm(), mmpage->datablock.addr - pagesize_vm()) ;
   TEST(0 == init_vmmappedregions(&mapping)) ;
   TEST(1 == iscontained_vmmappedregions(&mapping, &page[0], (accessmode_RDWR|accessmode_PRIVATE))) ;
   TEST(1 == iscontained_vmmappedregions(&mapping, &page[1], (accessmode_RDWR|accessmode_PRIVATE))) ;
   TEST(1 == iscontained_vmmappedregions(&mapping, &page[2], (accessmode_NONE|accessmode_PRIVATE))) ;
   TEST(1 == iscontained_vmmappedregions(&mapping, &page[3], (accessmode_NONE|accessmode_PRIVATE))) ;
   TEST(0 == free_vmmappedregions(&mapping)) ;

   // TEST free: memory is unmapped
   TEST(0 == delete_mmtestpage(&mmpage)) ;
   TEST(0 == mmpage) ;
   TEST(0 == init_vmmappedregions(&mapping)) ;
   TEST(0 == iscontained_vmmappedregions(&mapping, &page[0], (accessmode_RDWR|accessmode_PRIVATE))) ;
   TEST(0 == iscontained_vmmappedregions(&mapping, &page[1], (accessmode_RDWR|accessmode_PRIVATE))) ;
   TEST(0 == iscontained_vmmappedregions(&mapping, &page[2], (accessmode_NONE|accessmode_PRIVATE))) ;
   TEST(0 == iscontained_vmmappedregions(&mapping, &page[3], (accessmode_NONE|accessmode_PRIVATE))) ;
   TEST(0 == free_vmmappedregions(&mapping)) ;

   // TEST init ENOMEM
   TEST(ENOMEM == new_mmtestpage(&mmpage, 1024*1024, 0)) ;
   TEST(0 == mmpage) ;

   // TEST newblock_mmtestpage
   TEST(0 == new_mmtestpage(&mmpage, 0, 0)) ;
   memblock_t nextfree = mmpage->freeblock ;
   for(unsigned i = 1; i <= 1000; ++i) {
      const size_t   alignsize = alignsize_mmtestblock(i) ;
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0      == newblock_mmtestpage(mmpage, i, &memblock)) ;
      TEST(true   == isblockvalid_mmtestpage(mmpage, &memblock)) ;
      TEST(ENOMEM == newblock_mmtestpage(mmpage, nextfree.size+1-headersize-trailersize, &memblock)) ;
      nextfree.addr += headersize ;
      TEST(memblock.addr == nextfree.addr) ;
      TEST(memblock.size == i) ;
      nextfree.addr += trailersize + alignsize;
      nextfree.size -= headersize + trailersize + alignsize ;
      TEST(nextfree.addr == mmpage->freeblock.addr) ;
      TEST(nextfree.size == mmpage->freeblock.size) ;
   }
   TEST(0 == delete_mmtestpage(&mmpage)) ;

   // TEST resizeblock_mmtestpage
   TEST(0 == new_mmtestpage(&mmpage, 0, 0)) ;
   nextfree = mmpage->freeblock ;
   memblock_t oldblock = memblock_INIT_FREEABLE ;
   for(unsigned i = 1; i <= 1000; ++i) {
      const size_t   alignsize = alignsize_mmtestblock(i+1) ;
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == newblock_mmtestpage(mmpage, i, &memblock)) ;
      TEST(0 == resizeblock_mmtestpage(mmpage, i+1, &memblock)) ;
      TEST(1 == isblockvalid_mmtestpage(mmpage, &memblock)) ;
      nextfree.addr += headersize ;
      TEST(memblock.addr == nextfree.addr) ;
      TEST(memblock.size == i+1) ;
      nextfree.addr += trailersize + alignsize;
      nextfree.size -= headersize + trailersize + alignsize ;
      TEST(nextfree.addr == mmpage->freeblock.addr) ;
      TEST(nextfree.size == mmpage->freeblock.size) ;
      if (!isfree_memblock(&oldblock)) {
         TEST(ENOMEM == resizeblock_mmtestpage(mmpage, i+1, &oldblock)) ;
      }
      oldblock = memblock ;
   }
   TEST(0 == delete_mmtestpage(&mmpage)) ;

   // TEST freeblock_mmtestpage
   TEST(0 == new_mmtestpage(&mmpage, 0, 0)) ;
   nextfree = mmpage->freeblock ;
   oldblock = (memblock_t) memblock_INIT_FREEABLE ;
   for(unsigned i = 1; i <= 1000; ++i) {
      const size_t   alignsize = alignsize_mmtestblock(i+1) ;
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == newblock_mmtestpage(mmpage, i, &memblock)) ;
      TEST(0 == resizeblock_mmtestpage(mmpage, i+1, &memblock)) ;
      TEST(1 == isblockvalid_mmtestpage(mmpage, &memblock)) ;
      nextfree.addr += headersize ;
      TEST(memblock.addr == nextfree.addr) ;
      TEST(memblock.size == i+1) ;
      nextfree.addr += trailersize + alignsize;
      nextfree.size -= headersize + trailersize + alignsize ;
      TEST(nextfree.addr == mmpage->freeblock.addr) ;
      TEST(nextfree.size == mmpage->freeblock.size) ;
      if (!isfree_memblock(&oldblock)) {
         // is only marked as free
         TEST(0 == freeblock_mmtestpage(mmpage, &oldblock)) ;
         TEST(0 == oldblock.addr) ;
         TEST(0 == oldblock.size) ;
      }
      oldblock = memblock ;
   }
   // all free blocks are merged
   TEST(0 == freeblock_mmtestpage(mmpage, &oldblock)) ;
   TEST(isfree_memblock(&oldblock)) ;
   TEST(mmpage->freeblock.addr == mmpage->datablock.addr) ;
   TEST(mmpage->freeblock.size == mmpage->datablock.size) ;
   TEST(0 == delete_mmtestpage(&mmpage)) ;

   // TEST isblockvalid_mmtestpage
   TEST(0 == new_mmtestpage(&mmpage, 0, 0)) ;
   nextfree = mmpage->freeblock ;
   TEST(0 == newblock_mmtestpage(mmpage, 1000000, &memblock)) ;
   mmpage->datablock.addr += 1 ;
   TEST(0 == isblockvalid_mmtestpage(mmpage, &memblock)) ;
   mmpage->datablock.addr -= 1 ;
   TEST(1 == isblockvalid_mmtestpage(mmpage, &memblock)) ;
   mmpage->freeblock.addr -= 1 ;
   TEST(0 == isblockvalid_mmtestpage(mmpage, &memblock)) ;
   mmpage->freeblock.addr += 1 ;
   TEST(1 == isblockvalid_mmtestpage(mmpage, &memblock)) ;
   ((mmtestblock_t*)(memblock.addr-headersize))->header.alignsize = mmpage->datablock.size ;
   TEST(0 == isblockvalid_mmtestpage(mmpage, &memblock)) ;
   ((mmtestblock_t*)(memblock.addr-headersize))->header.alignsize = alignsize_mmtestblock(memblock.size) ;
   TEST(1 == isblockvalid_mmtestpage(mmpage, &memblock)) ;
   TEST(0 == delete_mmtestpage(&mmpage)) ;

   return 0 ;
ONABORT:
   delete_mmtestpage(&mmpage) ;
   free_vmmappedregions(&mapping) ;
   return EINVAL ;
}

static int test_initfree(void)
{
   mm_iot       mmiot    = mm_iot_INIT_FREEABLE ;
   mmtest_t     mmtest   = mmtest_INIT_FREEABLE ;
   memblock_t   memblock = memblock_INIT_FREEABLE ;
   const size_t headersize = alignsize_mmtestblock(sizeof(((mmtestblock_t*)0)->header)) ;

   // TEST static init
   TEST(0 == mmtest.mmpage) ;
   TEST(0 == mmtest.sizeallocated) ;
   TEST(0 == mmtest.simulateResizeError) ;
   TEST(0 == mmtest.simulateFreeError) ;

   // TEST init, double free
   mmtest.sizeallocated = 1 ;
   mmtest.simulateResizeError = (void*) 1 ;
   mmtest.simulateFreeError   = (void*) 1 ;
   TEST(0 == init_mmtest(&mmtest)) ;
   TEST(0 != mmtest.mmpage) ;
   TEST(0 == mmtest.sizeallocated) ;
   TEST(0 == mmtest.simulateResizeError) ;
   TEST(0 == mmtest.simulateFreeError) ;
   TEST(0 == mresize_mmtest(&mmtest, 1, &memblock)) ;
   TEST(1 == mmtest.sizeallocated) ;
   mmtest.simulateResizeError = (void*) 1 ;
   mmtest.simulateFreeError   = (void*) 1 ;
   TEST(0 == free_mmtest(&mmtest)) ;
   TEST(0 == mmtest.mmpage) ;
   TEST(0 == mmtest.sizeallocated) ;
   TEST(0 == mmtest.simulateResizeError) ;
   TEST(0 == mmtest.simulateFreeError) ;
   TEST(0 == free_mmtest(&mmtest)) ;
   TEST(0 == mmtest.mmpage) ;
   TEST(0 == mmtest.sizeallocated) ;

   // TEST free: free all pages
   TEST(0 == init_mmtest(&mmtest)) ;
   for (unsigned i = 0; i < 10; ++i) {
      memblock = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_mmtest(&mmtest, 1024*1024-1000, &memblock)) ;
   }
   TEST(10*(1024*1024-1000) == mmtest.sizeallocated) ;
   TEST(0 == free_mmtest(&mmtest)) ;
   TEST(0 == mmtest.mmpage) ;
   TEST(0 == mmtest.sizeallocated) ;
   TEST(0 == free_mmtest(&mmtest)) ;
   TEST(0 == mmtest.mmpage) ;
   TEST(0 == mmtest.sizeallocated) ;

   // TEST initiot, double freeiot
   TEST(0 == mmiot.object) ;
   TEST(0 == mmiot.iimpl) ;
   TEST(0 == initiot_mmtest(&mmiot)) ;
   TEST(mmiot.object == (void*) (((mmtest_t*)mmiot.object)->mmpage->datablock.addr + headersize)) ;
   TEST(mmiot.iimpl  == &s_mmtest_interface.generic) ;
   TEST(0 == freeiot_mmtest(&mmiot)) ;
   TEST(0 == mmiot.object) ;
   TEST(0 == mmiot.iimpl) ;

   return 0 ;
ONABORT:
   free_mmtest(&mmtest) ;
   freeiot_mmtest(&mmiot) ;
   return EINVAL ;
}

static int test_allocate(void)
{
   memblock_t     memblocks[1000] ;
   const size_t   blocksize   = 10*1024*1024 / nrelementsof(memblocks) ;
   mmtest_t       mmtest      = mmtest_INIT_FREEABLE ;
   const size_t   headersize  = alignsize_mmtestblock(sizeof(((mmtestblock_t*)0)->header)) ;

   // prepare
   TEST(0 == init_mmtest(&mmtest)) ;

   // TEST alloc, realloc, free in FIFO order
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      // allocate blocksize/2 and then resize to blocksize
      memblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_mmtest(&mmtest, blocksize/2, &memblocks[i])) ;
      TEST(i * blocksize + blocksize/2 == sizeallocated_mmtest(&mmtest))
      TEST(memblocks[i].addr != 0) ;
      TEST(memblocks[i].size == blocksize/2) ;
      uint8_t        * oldaddr = memblocks[i].addr ;
      mmtest_page_t  * oldpage = mmtest.mmpage ;
      TEST(0 == mresize_mmtest(&mmtest, blocksize, &memblocks[i])) ;
      TEST((i+1) * blocksize == sizeallocated_mmtest(&mmtest))
      TEST(memblocks[i].addr != 0) ;
      TEST(memblocks[i].size == blocksize) ;
      if (oldpage == mmtest.mmpage) { // oldpage had enough space
         TEST(memblocks[i].addr == oldaddr) ;
      } else { // new page allocated, new block is first on page
         TEST(memblocks[i].addr == mmtest.mmpage->datablock.addr + headersize) ;
      }
   }
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      mmtest_page_t  * oldpage = mmtest.mmpage ;
      if (i % 2) {
         TEST(0 == mresize_mmtest(&mmtest, 0, &memblocks[i])) ;
      } else {
         TEST(0 == mfree_mmtest(&mmtest, &memblocks[i])) ;
      }
      TEST(0 == memblocks[i].addr) ;
      TEST(0 == memblocks[i].size) ;
      TEST((nrelementsof(memblocks)-1-i) * blocksize == sizeallocated_mmtest(&mmtest)) ;
      if (i != nrelementsof(memblocks)-1) {  // no pages are freed
         TEST(oldpage == mmtest.mmpage) ;
      } else {         // mmtest has now only a single empty page
         TEST(0 == mmtest.mmpage->next) ;
         TEST(1 == ispagefree_mmtestpage(mmtest.mmpage)) ;
      }
   }
   TEST(0 == sizeallocated_mmtest(&mmtest)) ;

   // TEST alloc, realloc, free in LIFO order
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      memblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_mmtest(&mmtest, blocksize, &memblocks[i])) ;
      TEST((i+1) * blocksize == sizeallocated_mmtest(&mmtest))
   }
   for (unsigned i = nrelementsof(memblocks); (i--) > 0; ) {
      mmtest_page_t  * oldpage   = mmtest.mmpage ;
      mmtest_page_t  * foundpage = findpage_mmtest(&mmtest, memblocks[i].addr) ;
      bool           isfirst     = (foundpage->datablock.addr + headersize == memblocks[i].addr) ;
      bool           isRootPage  = (0 != foundpage->next) && (0 == foundpage->next->next) ;
      mmtest_page_t  * rootpage  = isRootPage ? foundpage->next : 0 ;

      if (foundpage == oldpage) {
         TEST(!ispagefree_mmtestpage(oldpage)) ;
      } else {
         TEST(ispagefree_mmtestpage(oldpage)) ;
         TEST(foundpage == oldpage->next) ;
      }
      TEST(0 == mfree_mmtest(&mmtest, &memblocks[i])) ;
      TEST(i * blocksize == sizeallocated_mmtest(&mmtest)) ;
      TEST(0 == memblocks[i].addr) ;
      TEST(0 == memblocks[i].size) ;
      if (isfirst && oldpage != foundpage) {
         if (isRootPage) {
            TEST(rootpage == mmtest.mmpage) ;
         } else {
            TEST(foundpage == mmtest.mmpage) ;
         }
      } else {
         TEST(oldpage == mmtest.mmpage) ;
      }
   }
   TEST(0 == mmtest.mmpage->next) ;
   TEST(1 == ispagefree_mmtestpage(mmtest.mmpage)) ;

   // TEST alloc, realloc, free in random order
   srandom(10000) ;
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      memblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
   }
   for (size_t ri = 0, datasize = 0; ri < 100000; ++ri) {
      unsigned i = (unsigned)random() % nrelementsof(memblocks) ;
      if (isfree_memblock(&memblocks[i])) {
         datasize += blocksize/2 ;
         TEST(0 == mresize_mmtest(&mmtest, blocksize/2, &memblocks[i])) ;
         TEST(0 != memblocks[i].addr) ;
         TEST(blocksize/2 == memblocks[i].size) ;
      } else if (blocksize == memblocks[i].size) {
         datasize -= blocksize ;
         TEST(0 == mfree_mmtest(&mmtest, &memblocks[i])) ;
         TEST(0 == memblocks[i].addr) ;
         TEST(0 == memblocks[i].size) ;
      } else {
         datasize -= memblocks[i].size ;
         datasize += blocksize ;
         TEST(0 == mresize_mmtest(&mmtest, blocksize, &memblocks[i])) ;
         TEST(0 != memblocks[i].addr) ;
         TEST(blocksize == memblocks[i].size) ;
      }
      TEST(datasize == sizeallocated_mmtest(&mmtest)) ;
   }
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      TEST(0 == mfree_mmtest(&mmtest, &memblocks[i])) ;
   }
   TEST(0 == sizeallocated_mmtest(&mmtest)) ;
   TEST(0 == mmtest.mmpage->next) ;
   TEST(1 == ispagefree_mmtestpage(mmtest.mmpage)) ;

   // TEST reallocation preserves content
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      memblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_mmtest(&mmtest, sizeof(unsigned)*20, &memblocks[i])) ;
      TEST(sizeof(unsigned)*20 == memblocks[i].size) ;
      for (unsigned off = 0; off < 20; ++off) {
         ((unsigned*)(memblocks[i].addr))[off] = i + off ;
      }
   }
   for (unsigned i = nrelementsof(memblocks); (i--) > 0; ) {
      TEST(0 == mresize_mmtest(&mmtest, sizeof(unsigned)*21, &memblocks[i])) ;
      TEST(sizeof(unsigned)*21 == memblocks[i].size) ;
      for (unsigned off = 20; off < 21; ++off) {
         ((unsigned*)(memblocks[i].addr))[off] = i + off ;
      }
   }
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      TEST(0 == mresize_mmtest(&mmtest, sizeof(unsigned)*22, &memblocks[i])) ;
      TEST(sizeof(unsigned)*22 == memblocks[i].size) ;
      for (unsigned off = 21; off < 22; ++off) {
         ((unsigned*)(memblocks[i].addr))[off] = i + off ;
      }
   }
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      for (unsigned off = 0; off < 22; ++off) {
         TEST(((unsigned*)(memblocks[i].addr))[off] == i + off) ;
      }
   }
   for (unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      TEST(0 == mfree_mmtest(&mmtest, &memblocks[i])) ;
      TEST(0 == memblocks[i].addr) ;
      TEST(0 == memblocks[i].size) ;
   }
   TEST(0 == sizeallocated_mmtest(&mmtest)) ;
   TEST(0 == mmtest.mmpage->next) ;
   TEST(1 == ispagefree_mmtestpage(mmtest.mmpage)) ;

   // TEST setresizeerr_mmtest, setfreeeerr_mmtest
   test_errortimer_t errtimer1 ;
   test_errortimer_t errtimer2 ;
   TEST(0 == init_testerrortimer(&errtimer1, 2, EPROTO)) ;
   TEST(0 == init_testerrortimer(&errtimer2, 5, EPERM)) ;
   TEST(0 == mmtest.simulateResizeError)
   setresizeerr_mmtest(&mmtest, &errtimer1) ;
   TEST(&errtimer1 == mmtest.simulateResizeError)
   TEST(0 == mmtest.simulateFreeError)
   setfreeerr_mmtest(&mmtest, &errtimer2) ;
   TEST(&errtimer2 == mmtest.simulateFreeError)
   static_assert(nrelementsof(memblocks) > 2, "") ;
   memblocks[0] = (memblock_t) memblock_INIT_FREEABLE ;
   TEST(0 == mresize_mmtest(&mmtest, sizeof(unsigned)*20, &memblocks[0])) ;
   memblocks[1] = (memblock_t) memblock_INIT_FREEABLE ;
   TEST(EPROTO == mresize_mmtest(&mmtest, sizeof(unsigned)*20, &memblocks[1])) ;
   TEST(0 == mresize_mmtest(&mmtest, sizeof(unsigned)*20, &memblocks[1])) ;
   for (unsigned i = 0; i < 2; ++i) {
      TEST(0 == mfree_mmtest(&mmtest, &memblocks[0])) ;
      TEST(0 == mfree_mmtest(&mmtest, &memblocks[1])) ;
   }
   TEST(EPERM == mfree_mmtest(&mmtest, &memblocks[0])) ;
   TEST(0 == mfree_mmtest(&mmtest, &memblocks[1])) ;

   // TEST setresizeerr_mmtest, setfreeeerr_mmtest: after firing pointer to timer are cleared
   TEST(0 == init_testerrortimer(&errtimer1, 1, ENOMEM)) ;
   TEST(0 == init_testerrortimer(&errtimer2, 1, EINVAL)) ;
   TEST(0 == mmtest.simulateResizeError) ;
   setresizeerr_mmtest(&mmtest, &errtimer1) ;
   TEST(&errtimer1 == mmtest.simulateResizeError) ;
   TEST(ENOMEM == mresize_mmtest(&mmtest, sizeof(unsigned)*20, &memblocks[0])) ;
   TEST(0 == mmtest.simulateResizeError) ;
   TEST(0 == mmtest.simulateFreeError)
   setfreeerr_mmtest(&mmtest, &errtimer2) ;
   TEST(&errtimer2 == mmtest.simulateFreeError)
   TEST(EINVAL == mfree_mmtest(&mmtest, &memblocks[0])) ;
   TEST(0 == mmtest.simulateFreeError)

   // TEST setresizeerr_mmtest, setfreeeerr_mmtest: value 0 disarms error timer
   TEST(0 == init_testerrortimer(&errtimer1, 1, ENOMEM)) ;
   TEST(0 == init_testerrortimer(&errtimer2, 1, EINVAL)) ;
   setresizeerr_mmtest(&mmtest, &errtimer1) ;
   setfreeerr_mmtest(&mmtest, &errtimer2) ;
   setresizeerr_mmtest(&mmtest, 0) ;
   setfreeerr_mmtest(&mmtest, 0) ;
   TEST(0 == mresize_mmtest(&mmtest, sizeof(unsigned)*20, &memblocks[0])) ;
   TEST(0 == mfree_mmtest(&mmtest, &memblocks[0])) ;

   // unprepare
   TEST(0 == free_mmtest(&mmtest)) ;

   return 0 ;
ONABORT:
   free_mmtest(&mmtest) ;
   return EINVAL ;
}

static int test_context(void)
{
   mm_iot oldmm = mmtransient_maincontext() ;

   // TEST double call switchon_mmtest
   TEST(&s_mmtest_interface.generic != mmtransient_maincontext().iimpl) ;
   TEST(0 == switchon_mmtest()) ;
   TEST(&s_mmtest_interface.generic == mmtransient_maincontext().iimpl) ;
   TEST(0 == switchon_mmtest()) ;
   TEST(&s_mmtest_interface.generic == mmtransient_maincontext().iimpl) ;

   // TEST double call switchoff_mmtest
   TEST(&s_mmtest_interface.generic == mmtransient_maincontext().iimpl) ;
   TEST(0 == switchoff_mmtest()) ;
   TEST(&s_mmtest_interface.generic != mmtransient_maincontext().iimpl) ;
   TEST(oldmm.object == mmtransient_maincontext().object) ;
   TEST(oldmm.iimpl  == mmtransient_maincontext().iimpl) ;
   TEST(0 == switchoff_mmtest()) ;
   TEST(oldmm.object == mmtransient_maincontext().object) ;
   TEST(oldmm.iimpl  == mmtransient_maincontext().iimpl) ;

   return 0 ;
ONABORT:
   switchoff_mmtest() ;
   return EINVAL ;
}

int unittest_memory_manager_mmtest()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_mmtestpage())  goto ONABORT ;
   if (test_initfree())    goto ONABORT ;
   if (test_allocate())    goto ONABORT ;
   if (test_context())     goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
