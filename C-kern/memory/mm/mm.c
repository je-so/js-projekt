/* title: MemoryManager-Object impl

   Implements unittest of <MemoryManager-Object>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int malloc_dummy(struct mm_t * mman, size_t size, /*out*/struct memblock_t * memblock)
{
   (void) mman ;
   (void) size ;
   (void) memblock ;
   return 0 ;
}

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
   mm_it mminterface = mm_it_FREE ;
   mm_t  mman        = mm_FREE ;

   // TEST mm_FREE
   TEST(0 == mman.object) ;
   TEST(0 == mman.iimpl) ;

   // TEST mm_INIT
   mman = (mm_t) mm_INIT((mm_t*)2, (mm_it*)3) ;
   TEST(2 == (uintptr_t)mman.object) ;
   TEST(3 == (uintptr_t)mman.iimpl) ;

   // TEST mm_it_FREE
   TEST(0 == mminterface.mresize) ;
   TEST(0 == mminterface.mfree) ;
   TEST(0 == mminterface.sizeallocated) ;

   // TEST mm_it_INIT
   mminterface = (mm_it) mm_it_INIT(&malloc_dummy, &mresize_dummy, &mfree_dummy, &sizeallocated_dummy) ;
   TEST(mminterface.malloc        == &malloc_dummy) ;
   TEST(mminterface.mresize       == &mresize_dummy) ;
   TEST(mminterface.mfree         == &mfree_dummy) ;
   TEST(mminterface.sizeallocated == &sizeallocated_dummy) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

typedef struct mmx_t mmx_t ;

struct mmx_t {
   mmx_t *        mm ;
   size_t         newsize ;
   struct
   memblock_t *   memblock ;
   uint8_t        opid ;
} ;

static int malloc_mmx(struct mmx_t * mm, size_t size, /*out*/struct memblock_t * memblock)
{
   mm->mm       = mm ;
   mm->newsize  = size ;
   mm->memblock = memblock ;
   mm->opid     = 1 ;
   return 0 ;
}

static int mresize_mmx(struct mmx_t * mm, size_t newsize, struct memblock_t * memblock)
{
   mm->mm       = mm ;
   mm->newsize  = newsize ;
   mm->memblock = memblock ;
   mm->opid     = 2 ;
   return 0 ;
}

static int mfree_mmx(struct mmx_t * mm, struct memblock_t * memblock)
{
   mm->mm       = mm ;
   mm->memblock = memblock ;
   mm->opid     = 3 ;
   return 0 ;
}

size_t sizeallocated_mmx(struct mmx_t * mm)
{
   mm->mm   = mm ;
   mm->opid = 4 ;
   return 0 ;
}

// TEST mm_it_DECLARE
mm_it_DECLARE(mmx_it, struct mmx_t)

/* function:test_generic
 * Test generic functions of <mm_it>. */
static int test_generic(void)
{
   mmx_it mmxif = mm_it_FREE ;

   // TEST mm_it_FREE
   TEST(0 == mmxif.mresize) ;
   TEST(0 == mmxif.mfree) ;
   TEST(0 == mmxif.sizeallocated) ;

   // TEST mm_it_INIT
   mmxif = (mmx_it) mm_it_INIT(&malloc_mmx, &mresize_mmx, &mfree_mmx, &sizeallocated_mmx) ;
   TEST(mmxif.malloc        == &malloc_mmx) ;
   TEST(mmxif.mresize       == &mresize_mmx) ;
   TEST(mmxif.mfree         == &mfree_mmx) ;
   TEST(mmxif.sizeallocated == &sizeallocated_mmx) ;

   // TEST genericcast_mmit
   TEST((mm_it*)&mmxif == genericcast_mmit(&mmxif, struct mmx_t)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

/* function: test_call
 * Test call functions of <mm_t>. */
static int test_call(void)
{
   mmx_it mmxif = mm_it_INIT(&malloc_mmx, &mresize_mmx, &mfree_mmx, &sizeallocated_mmx) ;
   mmx_t  mmx   = { 0, 0, 0, 0 } ;
   mm_t   mm    = mm_INIT((mm_t*)&mmx, genericcast_mmit(&mmxif, struct mmx_t)) ;

   // TEST malloc_mm
   TEST(0 == malloc_mm(mm, 1000, (struct memblock_t*)10001)) ;
   TEST(mmx.mm       == &mmx) ;
   TEST(mmx.newsize  == 1000) ;
   TEST(mmx.memblock == (struct memblock_t*)10001) ;
   TEST(mmx.opid     == 1) ;

   // TEST mresize_mm
   TEST(0 == mresize_mm(mm, 2000, (struct memblock_t*)10002)) ;
   TEST(mmx.mm       == &mmx) ;
   TEST(mmx.newsize  == 2000) ;
   TEST(mmx.memblock == (struct memblock_t*)10002) ;
   TEST(mmx.opid     == 2) ;

   // TEST mfree_mm
   memset(&mmx, 0, sizeof(mmx)) ;
   TEST(0 == mfree_mm(mm, (struct memblock_t*)10003)) ;
   TEST(mmx.mm       == &mmx) ;
   TEST(mmx.newsize  == 0) ;
   TEST(mmx.memblock == (struct memblock_t*)10003) ;
   TEST(mmx.opid     == 3) ;

   // TEST sizeallocated_mm
   memset(&mmx, 0, sizeof(mmx)) ;
   TEST(0 == sizeallocated_mm(mm)) ;
   TEST(mmx.mm       == &mmx) ;
   TEST(mmx.newsize  == 0) ;
   TEST(mmx.memblock == 0) ;
   TEST(mmx.opid     == 4) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_memory_mm_mm()
{
   if (test_initfree())    goto ONERR;
   if (test_generic())     goto ONERR;
   if (test_call())        goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
