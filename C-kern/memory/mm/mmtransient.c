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
#include "C-kern/api/err.h"
#include "C-kern/api/platform/malloc.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* typedef: struct mmtransient_it
 * Shortcut for <mmtransient_it>. */
typedef struct mmtransient_it          mmtransient_it ;


/* struct: mmtransient_it
 * Adapts <mm_it> to <mmtransient_t>. See <mm_it_DECLARE>. */
mm_it_DECLARE(mmtransient_it, mmtransient_t)

// group: variables

/* variable: s_mmtransient_interface
 * Contains single instance of interface <mmtransient_it>. */
static mmtransient_it   s_mmtransient_interface = mm_it_INIT(
                            &mresize_mmtransient
                           ,&mfree_mmtransient
                           ,&sizeallocated_mmtransient
                         ) ;


// section: mmtransient_t

// group: init

int initthread_mmtransient(/*out*/mm_t * mm_transient)
{
   int err ;
   const size_t      objsize    = sizeof(mmtransient_t) ;
   mmtransient_t     tempobject = mmtransient_INIT_FREEABLE ;
   memblock_t        newobject  = memblock_INIT_FREEABLE ;


   VALIDATE_INPARAM_TEST(0 == mm_transient->object, ONABORT, ) ;

   err = init_mmtransient(&tempobject) ;
   if (err) goto ONABORT ;

   err = mresize_mmtransient(&tempobject, objsize, &newobject) ;
   if (err) goto ONABORT ;

   memcpy(newobject.addr, &tempobject, objsize) ;

   mm_transient->object = (struct mm_t*) newobject.addr;
   mm_transient->iimpl  = (mm_it*) &s_mmtransient_interface ;

   return 0 ;
ONABORT:
   free_mmtransient(&tempobject) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freethread_mmtransient(mm_t * mm_transient)
{
   int err ;
   int err2 ;
   mmtransient_t * delobject = (mmtransient_t*) mm_transient->object ;

   if (delobject) {
      assert(&s_mmtransient_interface == (mmtransient_it*) mm_transient->iimpl) ;

      mm_transient->object = 0 ;
      mm_transient->iimpl  = 0 ;

      mmtransient_t  tempobject = *delobject ;
      memblock_t     memobject  = memblock_INIT(sizeof(mmtransient_t), (uint8_t*)delobject) ;

      err = mfree_mmtransient(&tempobject, &memobject) ;

      err2 = free_mmtransient(&tempobject) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: lifetime

int init_mmtransient(mmtransient_t * mman)
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

   VALIDATE_INPARAM_TEST(isvalid_memblock(memblock), ONABORT, ) ;

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

   VALIDATE_INPARAM_TEST(isvalid_memblock(memblock), ONABORT, ) ;

   // TODO: implement mfree_mmtransient without free

   if (!isfree_memblock(memblock)) {
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
   mmtransient_t   mman = mmtransient_INIT_FREEABLE ;

   // TEST static init memoryman_transient_INIT_FREEABLE
   TEST(0 == mman.todo__implement_without_malloc__) ;

   // TEST init_mmtransient, double free_mmtransient
   memset(&mman, 255, sizeof(mman)) ;
   TEST(0 == init_mmtransient(&mman)) ;
   TEST(0 == mman.todo__implement_without_malloc__) ;
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

   // TEST static init
   TEST(0 == mman.object) ;
   TEST(0 == mman.iimpl) ;

   // TEST exported interface
   TEST(s_mmtransient_interface.mresize == &mresize_mmtransient) ;
   TEST(s_mmtransient_interface.mfree   == &mfree_mmtransient) ;

   // TEST initthread and double free
   TEST(0 == initthread_mmtransient(&mman)) ;
   TEST(mman.object != 0) ;
   TEST(mman.iimpl  == (mm_it*)&s_mmtransient_interface) ;
   TEST(0 == freethread_mmtransient(&mman)) ;
   TEST(0 == mman.object) ;
   TEST(0 == mman.iimpl) ;
   TEST(0 == freethread_mmtransient(&mman)) ;
   TEST(0 == mman.object) ;
   TEST(0 == mman.iimpl) ;

   // TEST EINVAL initthread
   mman.object = (struct mm_t*) 1 ;
   TEST(EINVAL == initthread_mmtransient(&mman)) ;

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
   for(unsigned i = 0; i < nrelementsof(mblocks); ++i) {
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
   for(unsigned i = 0; i < nrelementsof(mblocks); ++i) {
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
   for(unsigned i = 0; i < nrelementsof(mblocks); ++i) {
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
   for(unsigned i = 0; i < nrelementsof(mblocks); ++i) {
      mblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == MM_RESIZE(32 + 32 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 32 + 32 * i) ;
   }

   // TEST mresize allocated block
   for(unsigned i = 0; i < nrelementsof(mblocks); ++i) {
      void * oldaddr = mblocks[i].addr ;
      TEST(0 == MM_RESIZE(256 + 256 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].addr != oldaddr) ;
      TEST(mblocks[i].size >= 256 + 256 * i) ;
   }

   // TEST mfree
   for(unsigned i = 0; i < nrelementsof(mblocks); ++i) {
      TEST(0 == MM_FREE(&mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].addr) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_memory_manager_transient()
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
