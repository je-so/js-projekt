/* title: MemoryManager-Object impl

   Implements unittest of <MemoryManager-Object>.

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

   file: C-kern/api/memory/mm/mm.h
    Header file of <MemoryManager-Object>.

   file: C-kern/memory/mm/mm.c
    Implements unittest <MemoryManager-Object impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/mm/mm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int mresize_dummy(struct mm_t * mman, size_t newsize, struct memblock_t * memblock)
{
   (void) mman ;
   (void) newsize ;
   (void) memblock ;
   return 0 ;
}

static int mfree_dummy(struct mm_t * mman, struct memblock_t * memblock)
{
   (void) mman ;
   (void) memblock ;
   return 0 ;
}

size_t sizeallocated_dummy(struct mm_t * mman)
{
   (void) mman ;
   return 0 ;
}

/* function: test_initfree
 * Test lifetime functions of <mm_t> and <mm_it>. */
static int test_initfree(void)
{
   mm_it mminterface = mm_it_INIT_FREEABLE ;
   mm_t  mman        = mm_INIT_FREEABLE ;

   // TEST mm_INIT_FREEABLE
   TEST(0 == mman.object) ;
   TEST(0 == mman.iimpl) ;

   // TEST mm_INIT
   mman = (mm_t) mm_INIT((mm_t*)2, (mm_it*)3) ;
   TEST(2 == (uintptr_t)mman.object) ;
   TEST(3 == (uintptr_t)mman.iimpl) ;

   // TEST mm_it_INIT_FREEABLE
   TEST(0 == mminterface.mresize) ;
   TEST(0 == mminterface.mfree) ;
   TEST(0 == mminterface.sizeallocated) ;

   // TEST mm_it_INIT
   mminterface = (mm_it) mm_it_INIT(&mresize_dummy, &mfree_dummy, &sizeallocated_dummy) ;
   TEST(mminterface.mresize       == &mresize_dummy) ;
   TEST(mminterface.mfree         == &mfree_dummy) ;
   TEST(mminterface.sizeallocated == &sizeallocated_dummy) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

typedef struct mmx_t    mmx_t ;

struct mmx_t {
   mmx_t *        mm ;
   size_t         newsize ;
   struct
   memblock_t *   memblock ;
   uint8_t        opid ;
} ;

static int mresize_mmx(struct mmx_t * mm, size_t newsize, struct memblock_t * memblock)
{
   mm->mm       = mm ;
   mm->newsize  = newsize ;
   mm->memblock = memblock ;
   mm->opid     = 1 ;
   return 0 ;
}

static int mfree_mmx(struct mmx_t * mm, struct memblock_t * memblock)
{
   mm->mm       = mm ;
   mm->memblock = memblock ;
   mm->opid     = 2 ;
   return 0 ;
}

size_t sizeallocated_mmx(struct mmx_t * mm)
{
   mm->mm   = mm ;
   mm->opid = 3 ;
   return 0 ;
}

// TEST mm_it_DECLARE
mm_it_DECLARE(mmx_it, struct mmx_t)

/* function:test_generic
 * Test generic functions of <mm_it>. */
static int test_generic(void)
{
   mmx_it mmxif = mm_it_INIT_FREEABLE ;

   // TEST mm_it_INIT_FREEABLE
   TEST(0 == mmxif.mresize) ;
   TEST(0 == mmxif.mfree) ;
   TEST(0 == mmxif.sizeallocated) ;

   // TEST mm_it_INIT
   mmxif = (mmx_it) mm_it_INIT(&mresize_mmx, &mfree_mmx, &sizeallocated_mmx) ;
   TEST(mmxif.mresize       == &mresize_mmx) ;
   TEST(mmxif.mfree         == &mfree_mmx) ;
   TEST(mmxif.sizeallocated == &sizeallocated_mmx) ;

   // TEST genericcast_mmit
   TEST((mm_it*)&mmxif == genericcast_mmit(&mmxif, struct mmx_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

/* function: test_call
 * Test call functions of <mm_t>. */
static int test_call(void)
{
   mmx_it mmxif = mm_it_INIT(&mresize_mmx, &mfree_mmx, &sizeallocated_mmx) ;
   mmx_t  mmx   = { 0, 0, 0, 0 } ;
   mm_t   mm    = mm_INIT((mm_t*)&mmx, genericcast_mmit(&mmxif, struct mmx_t)) ;

   // TEST mresize_mm
   TEST(0 == mresize_mm(mm, 1000, (struct memblock_t*)10001)) ;
   TEST(mmx.mm       == &mmx) ;
   TEST(mmx.newsize  == 1000) ;
   TEST(mmx.memblock == (struct memblock_t*)10001) ;
   TEST(mmx.opid     == 1) ;

   // TEST mfree_mm
   memset(&mmx, 0, sizeof(mmx)) ;
   TEST(0 == mfree_mm(mm, (struct memblock_t*)10002)) ;
   TEST(mmx.mm       == &mmx) ;
   TEST(mmx.newsize  == 0) ;
   TEST(mmx.memblock == (struct memblock_t*)10002) ;
   TEST(mmx.opid     == 2) ;

   // TEST sizeallocated_mm
   memset(&mmx, 0, sizeof(mmx)) ;
   TEST(0 == sizeallocated_mm(mm)) ;
   TEST(mmx.mm       == &mmx) ;
   TEST(mmx.newsize  == 0) ;
   TEST(mmx.memblock == 0) ;
   TEST(mmx.opid     == 3) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_memory_mm_mm()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_generic())     goto ONABORT ;
   if (test_call())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif