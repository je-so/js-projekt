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
#include "C-kern/api/test/unittest.h"
#endif


/* struct: mm_impl_it
 * Adapts <mm_it> to <mm_impl_t>. See <mm_it_DECLARE>. */
mm_it_DECLARE(mm_impl_it, mm_impl_t)

// group: variables

/* variable: s_mmimpl_interface
 * Contains single instance of interface <mm_impl_it>. */
static mm_impl_it    s_mmimpl_interface = mm_it_INIT(
                           &malloc_mmimpl,
                           &mresize_mmimpl,
                           &mfree_mmimpl,
                           &sizeallocated_mmimpl
                        ) ;


// section: mm_impl_t

// group: initthread

mm_it * interface_mmimpl(void)
{
   return genericcast_mmit(&s_mmimpl_interface, mm_impl_t) ;
}

// group: lifetime

int init_mmimpl(/*out*/mm_impl_t * mman)
{
   mman->size_allocated = 0 ;
   return 0 ;
}

int free_mmimpl(mm_impl_t * mman)
{
   mman->size_allocated = 0 ;
   return 0 ;
}

// group: query

size_t sizeallocated_mmimpl(mm_impl_t * mman)
{
   // TODO: implement without allocatedsize_malloc (remove malloc module from subsys/context-mini !)
   return mman->size_allocated ;
}

// group: allocate

int malloc_mmimpl(mm_impl_t * mman, size_t size, /*out*/struct memblock_t * memblock)
{
   (void) mman ;
   int err ;
   void * addr ;

   // TODO: implement malloc_mmimpl without malloc

   if (  (ssize_t)size < 0
      || !(addr = malloc(size))) {
      err = ENOMEM ;
      TRACEOUTOFMEM_ERRLOG(size, err) ;
      goto ONABORT ;
   }

   memblock->addr = addr ;
   memblock->size = sizeusable_malloc(addr) ;

   mman->size_allocated += memblock->size ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int mresize_mmimpl(mm_impl_t * mman, size_t newsize, struct memblock_t * memblock)
{
   (void) mman ;
   int err ;
   void * addr ;

   // TODO: implement mresize_mmimpl without realloc

   if (0 == newsize) {
      return mfree_mmimpl(mman, memblock) ;
   }

   VALIDATE_INPARAM_TEST(isfree_memblock(memblock) || isvalid_memblock(memblock), ONABORT, ) ;

   mman->size_allocated -= sizeusable_malloc(memblock->addr) ;

   if (  (ssize_t)newsize < 0
      || !(addr = realloc(memblock->addr, newsize))) {
      mman->size_allocated += sizeusable_malloc(memblock->addr) ;
      err = ENOMEM ;
      TRACEOUTOFMEM_ERRLOG(newsize, err) ;
      goto ONABORT ;
   }

   memblock->addr = addr ;
   memblock->size = sizeusable_malloc(addr) ;

   mman->size_allocated += memblock->size ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int mfree_mmimpl(mm_impl_t * mman, struct memblock_t * memblock)
{
   int err ;
   (void) mman ;

   if (memblock->addr) {

      VALIDATE_INPARAM_TEST(isvalid_memblock(memblock), ONABORT, ) ;

      mman->size_allocated -= sizeusable_malloc(memblock->addr) ;

      // TODO: implement mfree_mmimpl without free

      free(memblock->addr) ;
      memblock->addr = 0 ;
      memblock->size = 0 ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   mm_impl_t mman = mmimpl_INIT_FREEABLE ;

   // TEST mmimpl_INIT_FREEABLE
   TEST(0 == mman.size_allocated) ;

   // TEST init_mmimpl
   TEST(0 == init_mmimpl(&mman)) ;
   TEST(0 == mman.size_allocated) ;

   // TEST free_mmimpl
   TEST(0 == free_mmimpl(&mman)) ;
   TEST(0 == mman.size_allocated) ;
   TEST(0 == free_mmimpl(&mman)) ;
   TEST(0 == mman.size_allocated) ;

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

   // TEST interface_mmimpl
   TEST(interface_mmimpl() == genericcast_mmit(&s_mmimpl_interface, mm_impl_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   mm_impl_t mman = mmimpl_INIT_FREEABLE ;

   // TEST sizeallocated_mmimpl: 0
   TEST(0 == init_mmimpl(&mman)) ;
   TEST(0 == sizeallocated_mmimpl(&mman)) ;
   TEST(0 == free_mmimpl(&mman)) ;
   TEST(0 == sizeallocated_mmimpl(&mman)) ;

   // TEST sizeallocated_mmimpl
   mman.size_allocated = 1000 ;
   TEST(1000 == sizeallocated_mmimpl(&mman)) ;
   mman.size_allocated = 2000 ;
   TEST(2000 == sizeallocated_mmimpl(&mman)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_allocate(void)
{
   mm_impl_t      mman = mmimpl_INIT_FREEABLE ;
   size_t         size = 0 ;
   memblock_t     mblocks[100] ;

   // prepare
   TEST(0 == init_mmimpl(&mman)) ;

   // TEST malloc_mmimpl
   memset(mblocks, 255 ,sizeof(mblocks)) ;
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      TEST(0 == malloc_mmimpl(&mman, 16 * (1 + i), &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 16 * (1 + i)) ;
      size += mblocks[i].size ;
      TEST(size == sizeallocated_mmimpl(&mman)) ;
   }

   // TEST mfree_mmimpl
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      size -= mblocks[i].size ;
      TEST(0 == mfree_mmimpl(&mman, &mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].size) ;
      TEST(size == sizeallocated_mmimpl(&mman)) ;
   }

   // TEST mresize_mmimpl: empty block,
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      mblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == mresize_mmimpl(&mman, 16 * (1 + i), &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 16 * (1 + i)) ;
      size += mblocks[i].size ;
      TEST(size == sizeallocated_mmimpl(&mman)) ;
   }

   // TEST mresize_mmimpl: allocated block
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      size -= mblocks[i].size ;
      TEST(0 == mresize_mmimpl(&mman, 2000, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 2000) ;
      size += mblocks[i].size ;
      TEST(size == sizeallocated_mmimpl(&mman)) ;
   }

   // TEST mfree_mmimpl
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      size -= mblocks[i].size ;
      TEST(0 == mfree_mmimpl(&mman, &mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].size) ;
      TEST(size == sizeallocated_mmimpl(&mman)) ;
   }

   // TEST malloc_mmimpl: ENOMEM
   TEST(ENOMEM == malloc_mmimpl(&mman, ((size_t)-1), &mblocks[0]));

   // TEST mresize_mmimpl: EINVAL
   mblocks[0] = (memblock_t)memblock_INIT_FREEABLE;
   mblocks[0].addr = (void*)1;
   TEST(EINVAL == mresize_mmimpl(&mman, 10, &mblocks[0]));
   mblocks[0] = (memblock_t)memblock_INIT_FREEABLE;
   mblocks[0].size = 1;
   TEST(EINVAL == mresize_mmimpl(&mman, 10, &mblocks[0]));

   // TEST mresize_mmimpl: ENOMEM
   mblocks[0] = (memblock_t)memblock_INIT_FREEABLE;
   TEST(ENOMEM == mresize_mmimpl(&mman, ((size_t)-1), &mblocks[0]));

   // TEST mfree_mmimpl: EINVAL
   mblocks[0] = (memblock_t)memblock_INIT_FREEABLE;
   mblocks[0].addr = (void*)1;
   TEST(EINVAL == mfree_mmimpl(&mman, &mblocks[0]));

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
   size_t      size = SIZEALLOCATED_MM() ;

   // TEST ALLOC_MM
   memset(mblocks, 255 ,sizeof(mblocks)) ;
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      TEST(0 == ALLOC_MM(32 + 32 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 32 + 32 * i) ;
      size += mblocks[i].size ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   // TEST FREE_MM
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      size -= mblocks[i].size ;
      TEST(0 == FREE_MM(&mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
      TEST(0 == FREE_MM(&mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   // TEST RESIZE_MM, SIZEALLOCATED_MM: empty block
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      mblocks[i] = (memblock_t) memblock_INIT_FREEABLE ;
      TEST(0 == RESIZE_MM(32 + 32 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 32 + 32 * i) ;
      size += mblocks[i].size ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   // TEST RESIZE_MM, SIZEALLOCATED_MM: resize allocated block
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      void * oldaddr = mblocks[i].addr ;
      size -= mblocks[i].size ;
      TEST(0 == RESIZE_MM(256 + 256 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].addr != oldaddr) ;
      TEST(mblocks[i].size >= 256 + 256 * i) ;
      size += mblocks[i].size ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   // TEST FREE_MM, SIZEALLOCATED_MM
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      size -= mblocks[i].size ;
      TEST(0 == FREE_MM(&mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
      TEST(0 == FREE_MM(&mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_allocate())    goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_initthread())  goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_allocate())    goto ONABORT ;
   if (test_mm_macros())   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_memory_mm_mmimpl()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return 0;
ONABORT:
   return EINVAL;
}

#endif
