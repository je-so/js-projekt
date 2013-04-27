/* title: TransientMemoryManager impl
   Implements <TransientMemoryManager>.

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

   file: C-kern/api/memory/mm/mmtransient.h
    Header file <TransientMemoryManager>.

   file: C-kern/memory/mm/mmtransient.c
    Implementation file <TransientMemoryManager impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/mm/mmtransient.h"
#include "C-kern/api/memory/mm/mm_it.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/malloc.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* struct: mmtransient_it
 * Adapts <mm_it> to <mmtransient_t>. See <mm_it_DECLARE>. */
mm_it_DECLARE(mmtransient_it, mmtransient_t)

// group: variables

/* variable: s_mmtransient_interface
 * Contains single instance of interface <mmtransient_it>. */
static mmtransient_it   s_mmtransient_interface = mm_it_INIT(
                           &mresize_mmtransient,
                           &mfree_mmtransient,
                           &sizeallocated_mmtransient
                        ) ;


// section: mmtransient_t

// group: init

int initthread_mmtransient(/*out*/mm_t * mm_transient)
{
   int err ;
   memblock_t memobject = memblock_INIT_FREEABLE ;

   VALIDATE_INPARAM_TEST(0 == mm_transient->object, ONABORT, ) ;

   err = ALLOCSTATIC_PAGECACHE(sizeof(mmtransient_t), &memobject) ;
   if (err) goto ONABORT ;

   mmtransient_t * newobj = (mmtransient_t*) memobject.addr ;

   err = init_mmtransient(newobj) ;
   if (err) goto ONABORT ;

   *mm_transient = (mm_t) mm_INIT((struct mm_t*)newobj, genericcast_mmit(&s_mmtransient_interface, mmtransient_t)) ;

   return 0 ;
ONABORT:
   FREESTATIC_PAGECACHE(&memobject) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freethread_mmtransient(mm_t * mm_transient)
{
   int err ;
   int err2 ;
   mmtransient_t * delobj = (mmtransient_t*) mm_transient->object ;

   if (delobj) {
      assert(genericcast_mmit(&s_mmtransient_interface, mmtransient_t) == mm_transient->iimpl) ;

      *mm_transient = (mm_t) mm_INIT_FREEABLE ;

      err = free_mmtransient(delobj) ;

      memblock_t memobject = memblock_INIT(sizeof(mmtransient_t), (uint8_t*)delobj) ;
      err2 = FREESTATIC_PAGECACHE(&memobject) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: lifetime

int init_mmtransient(/*out*/mmtransient_t * mman)
{
   mman->todo__implement_without_malloc__ = 0 ;
   return 0 ;
}

int free_mmtransient(mmtransient_t * mman)
{
   mman->todo__implement_without_malloc__ = 0 ;
   return 0 ;
}

// group: query

size_t sizeallocated_mmtransient(mmtransient_t * mman)
{
   // TODO: implement sizeallocated_mmtransient
   (void) mman ;
   return 0 ;
}

// group: allocate

int mresize_mmtransient(mmtransient_t * mman, size_t newsize, struct memblock_t * memblock)
{
   int err ;
   void * newaddr ;

   // TODO: implement mresize_mmtransient without realloc

   (void) mman ;
   if (0 == newsize) {
      return mfree_mmtransient(mman, memblock) ;
   }

   VALIDATE_INPARAM_TEST(isfree_memblock(memblock) || isvalid_memblock(memblock), ONABORT, ) ;

   if (  (ssize_t)newsize < 0
      || !(newaddr = realloc(memblock->addr, newsize))) {
      TRACEOUTOFMEM_LOG(newsize) ;
      err = ENOMEM ;
      goto ONABORT ;
   }

   memblock->addr = newaddr ;
   memblock->size = newsize ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int mfree_mmtransient(mmtransient_t * mman, struct memblock_t * memblock)
{
   int err ;
   (void) mman ;

   if (memblock->addr) {

      VALIDATE_INPARAM_TEST(isvalid_memblock(memblock), ONABORT, ) ;

      // TODO: implement mfree_mmtransient without free

      free(memblock->addr) ;
      memblock->addr = 0 ;
      memblock->size = 0 ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   mmtransient_t mman = mmtransient_INIT_FREEABLE ;

   // TEST mmtransient_INIT_FREEABLE
   TEST(0 == mman.todo__implement_without_malloc__) ;

   // TEST init_mmtransient
   memset(&mman, 255, sizeof(mman)) ;
   TEST(0 == init_mmtransient(&mman)) ;
   TEST(0 == mman.todo__implement_without_malloc__) ;

   // TEST free_mmtransient
   mman.todo__implement_without_malloc__ = 1 ;
   TEST(0 == free_mmtransient(&mman)) ;
   TEST(0 == mman.todo__implement_without_malloc__) ;
   TEST(0 == free_mmtransient(&mman)) ;
   TEST(0 == mman.todo__implement_without_malloc__) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initthread(void)
{
   mm_t  mman = mm_INIT_FREEABLE ;

   // TEST s_mmtransient_interface
   TEST(s_mmtransient_interface.mresize == &mresize_mmtransient) ;
   TEST(s_mmtransient_interface.mfree   == &mfree_mmtransient) ;

   // TEST initthread_mmtransient
   size_t sizestatic = SIZESTATIC_PAGECACHE() ;
   TEST(0 == initthread_mmtransient(&mman)) ;
   TEST(mman.object != 0) ;
   TEST(mman.iimpl  == genericcast_mmit(&s_mmtransient_interface, mmtransient_t)) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic + sizeof(mmtransient_t)) ;

   // TEST freethread_mmtransient
   TEST(0 == freethread_mmtransient(&mman)) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
   TEST(0 == mman.object) ;
   TEST(0 == mman.iimpl) ;
   TEST(0 == freethread_mmtransient(&mman)) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
   TEST(0 == mman.object) ;
   TEST(0 == mman.iimpl) ;

   // TEST initthread_mmtransient: EINVAL
   mman.object = (struct mm_t*) 1 ;
   TEST(EINVAL == initthread_mmtransient(&mman)) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;

   return 0 ;
ONABORT:
   (void) freethread_mmtransient(&mman) ;
   return EINVAL ;
}

static int test_allocate(void)
{
   mmtransient_t  mman = mmtransient_INIT_FREEABLE ;
   size_t         number_of_allocated_bytes ;
   size_t         number_of_allocated_bytes2 ;
   memblock_t     mblocks[100] ;

   // prepare
   TEST(0 == init_mmtransient(&mman)) ;
   TEST(0 == allocatedsize_malloc(&number_of_allocated_bytes)) ;

   // TEST mresize_mmtransient empty block, sizeallocated_mmtransient
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      mblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_mmtransient(&mman, 16 * (1 + i), &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 16 * (1 + i)) ;

      TEST(0 == allocatedsize_malloc(&number_of_allocated_bytes2)) ;
      TEST(number_of_allocated_bytes + mblocks[i].size <= number_of_allocated_bytes2) ;
      number_of_allocated_bytes = number_of_allocated_bytes2 ;

      TEST(0 == sizeallocated_mmtransient(&mman)) ;
   }

   // TEST mresize_mmtransient allocated block, sizeallocated_mmtransient
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      void * oldaddr = mblocks[i].addr ;
      TEST(0 == mresize_mmtransient(&mman, 2000, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].addr != oldaddr) ;
      TEST(mblocks[i].size >= 2000) ;

      TEST(0 == allocatedsize_malloc(&number_of_allocated_bytes2)) ;
      TEST(number_of_allocated_bytes + 2000 - 24 * (1+i) <= number_of_allocated_bytes2) ;
      number_of_allocated_bytes = number_of_allocated_bytes2 ;

      TEST(0 == sizeallocated_mmtransient(&mman)) ;
   }

   // TEST mfree_mmtransient, sizeallocated_mmtransient
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      TEST(0 == mfree_mmtransient(&mman, &mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].addr) ;

      TEST(0 == allocatedsize_malloc(&number_of_allocated_bytes2)) ;
      TEST(number_of_allocated_bytes >= 2000 + number_of_allocated_bytes2) ;
      number_of_allocated_bytes = number_of_allocated_bytes2 ;

      TEST(0 == sizeallocated_mmtransient(&mman)) ;
   }


   // unprepare
   TEST(0 == free_mmtransient(&mman)) ;

   return 0 ;
ONABORT:
   free_mmtransient(&mman) ;
   return EINVAL ;
}

static int test_mm_macros(void)
{
   memblock_t  mblocks[2] ;

   // TEST mresize empty block
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      mblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == RESIZE_MM(32 + 32 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 32 + 32 * i) ;
   }

   // TEST mresize allocated block
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      void * oldaddr = mblocks[i].addr ;
      TEST(0 == RESIZE_MM(256 + 256 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].addr != oldaddr) ;
      TEST(mblocks[i].size >= 256 + 256 * i) ;
   }

   // TEST mfree
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      TEST(0 == FREE_MM(&mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].addr) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_memory_mm_mmtransient()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_initthread())  goto ONABORT ;
   if (test_allocate())    goto ONABORT ;
   if (test_mm_macros())   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
