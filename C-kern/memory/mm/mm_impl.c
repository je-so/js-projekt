/* title: DefaultMemoryManager impl

   Implements <DefaultMemoryManager>.

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

   file: C-kern/api/memory/mm/mm_impl.h
    Header file <DefaultMemoryManager>.

   file: C-kern/memory/mm/mm_impl.c
    Implementation file <DefaultMemoryManager impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/mm/mm_impl.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/platform/malloc.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* struct: mm_impl_it
 * Adapts <mm_it> to <mm_impl_t>. See <mm_it_DECLARE>. */
mm_it_DECLARE(mm_impl_it, mm_impl_t)

// group: variables

/* variable: s_mmimpl_interface
 * Contains single instance of interface <mm_impl_it>. */
static mm_impl_it    s_mmimpl_interface = mm_it_INIT(
                           &mresize_mmimpl,
                           &mfree_mmimpl,
                           &sizeallocated_mmimpl
                        ) ;


// section: mm_impl_t

// group: initthread

mm_it * interfacethread_mmimpl(void)
{
   return genericcast_mmit(&s_mmimpl_interface, mm_impl_t) ;
}

// group: lifetime

int init_mmimpl(/*out*/mm_impl_t * mman)
{
   mman->todo__implement_without_malloc__ = 0 ;
   return 0 ;
}

int free_mmimpl(mm_impl_t * mman)
{
   mman->todo__implement_without_malloc__ = 0 ;
   return 0 ;
}

// group: query

size_t sizeallocated_mmimpl(mm_impl_t * mman)
{
   // TODO: implement sizeallocated_mmimpl
   (void) mman ;
   return 0 ;
}

// group: allocate

int mresize_mmimpl(mm_impl_t * mman, size_t newsize, struct memblock_t * memblock)
{
   int err ;
   void * newaddr ;

   // TODO: implement mresize_mmimpl without realloc

   (void) mman ;
   if (0 == newsize) {
      return mfree_mmimpl(mman, memblock) ;
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

int mfree_mmimpl(mm_impl_t * mman, struct memblock_t * memblock)
{
   int err ;
   (void) mman ;

   if (memblock->addr) {

      VALIDATE_INPARAM_TEST(isvalid_memblock(memblock), ONABORT, ) ;

      // TODO: implement mfree_mmimpl without free

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
   mm_impl_t mman = mmimpl_INIT_FREEABLE ;

   // TEST mmimpl_INIT_FREEABLE
   TEST(0 == mman.todo__implement_without_malloc__) ;

   // TEST init_mmimpl
   memset(&mman, 255, sizeof(mman)) ;
   TEST(0 == init_mmimpl(&mman)) ;
   TEST(0 == mman.todo__implement_without_malloc__) ;

   // TEST free_mmimpl
   mman.todo__implement_without_malloc__ = 1 ;
   TEST(0 == free_mmimpl(&mman)) ;
   TEST(0 == mman.todo__implement_without_malloc__) ;
   TEST(0 == free_mmimpl(&mman)) ;
   TEST(0 == mman.todo__implement_without_malloc__) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initthread(void)
{
   // TEST s_mmimpl_interface
   TEST(s_mmimpl_interface.mresize == &mresize_mmimpl) ;
   TEST(s_mmimpl_interface.mfree   == &mfree_mmimpl) ;
   TEST(s_mmimpl_interface.sizeallocated == &sizeallocated_mmimpl) ;

   // TEST interfacethread_mmimpl
   TEST(interfacethread_mmimpl() == genericcast_mmit(&s_mmimpl_interface, mm_impl_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_allocate(void)
{
   mm_impl_t      mman = mmimpl_INIT_FREEABLE ;
   size_t         number_of_allocated_bytes ;
   size_t         number_of_allocated_bytes2 ;
   memblock_t     mblocks[100] ;

   // prepare
   TEST(0 == init_mmimpl(&mman)) ;
   TEST(0 == allocatedsize_malloc(&number_of_allocated_bytes)) ;

   // TEST mresize_mmimpl empty block, sizeallocated_mmimpl
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      mblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_mmimpl(&mman, 16 * (1 + i), &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 16 * (1 + i)) ;

      TEST(0 == allocatedsize_malloc(&number_of_allocated_bytes2)) ;
      TEST(number_of_allocated_bytes + mblocks[i].size <= number_of_allocated_bytes2) ;
      number_of_allocated_bytes = number_of_allocated_bytes2 ;

      TEST(0 == sizeallocated_mmimpl(&mman)) ;
   }

   // TEST mresize_mmimpl allocated block, sizeallocated_mmimpl
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      void * oldaddr = mblocks[i].addr ;
      TEST(0 == mresize_mmimpl(&mman, 2000, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].addr != oldaddr) ;
      TEST(mblocks[i].size >= 2000) ;

      TEST(0 == allocatedsize_malloc(&number_of_allocated_bytes2)) ;
      TEST(number_of_allocated_bytes + 2000 - 24 * (1+i) <= number_of_allocated_bytes2) ;
      number_of_allocated_bytes = number_of_allocated_bytes2 ;

      TEST(0 == sizeallocated_mmimpl(&mman)) ;
   }

   // TEST mfree_mmimpl, sizeallocated_mmimpl
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      TEST(0 == mfree_mmimpl(&mman, &mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].addr) ;

      TEST(0 == allocatedsize_malloc(&number_of_allocated_bytes2)) ;
      TEST(number_of_allocated_bytes >= 2000 + number_of_allocated_bytes2) ;
      number_of_allocated_bytes = number_of_allocated_bytes2 ;

      TEST(0 == sizeallocated_mmimpl(&mman)) ;
   }


   // unprepare
   TEST(0 == free_mmimpl(&mman)) ;

   return 0 ;
ONABORT:
   free_mmimpl(&mman) ;
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

int unittest_memory_mm_mmimpl()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_allocate())    goto ONABORT ;

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
