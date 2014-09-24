/* title: MemoryErrorTestMacros impl

   Implements <MemoryErrorTestMacros>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/test/mm/err_macros.h
    Header file <MemoryErrorTestMacros>.

   file: C-kern/test/mm/err_macros.c
    Implementation file <MemoryErrorTestMacros impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/mm/err_macros.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/test/errortimer.h"
#endif


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_mm_macros(void)
{
   memblock_t        mblocks[2] ;
   test_errortimer_t errtimer ;
   size_t            size = SIZEALLOCATED_MM() ;

   // TEST ALLOC_ERR_MM
   memset(mblocks, 255 ,sizeof(mblocks)) ;
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      init_testerrortimer(&errtimer, 2, ENOMEM) ;
      TEST(0 == ALLOC_ERR_MM(&errtimer, 32 + 32 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 32 + 32 * i) ;
      size += mblocks[i].size ;
      TEST(size == SIZEALLOCATED_MM()) ;
      memblock_t dummy = mblocks[i] ;
      TEST(ENOMEM == ALLOC_ERR_MM(&errtimer, 32 + 32 * i, &dummy)) ;
      TEST(dummy.addr == mblocks[i].addr) ;
      TEST(dummy.size == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   // TEST RESIZE_ERR_MM
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      init_testerrortimer(&errtimer, 2, ENOMEM) ;
      size -= mblocks[i].size ;
      TEST(0 == RESIZE_ERR_MM(&errtimer, 1024, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 1024) ;
      size += mblocks[i].size ;
      TEST(size == SIZEALLOCATED_MM()) ;
      memblock_t dummy = mblocks[i] ;
      TEST(ENOMEM == RESIZE_ERR_MM(&errtimer, 3000, &dummy)) ;
      TEST(dummy.addr == mblocks[i].addr) ;
      TEST(dummy.size == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   // TEST FREE_ERR_MM
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      init_testerrortimer(&errtimer, 1, ENOMEM) ;
      size -= mblocks[i].size ;
      TEST(ENOMEM == FREE_ERR_MM(&errtimer, &mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

#undef CKERN_TEST_MM_ERR_MACROS_HEADER
#undef KONFIG_UNITTEST
#undef ALLOC_ERR_MM
#undef RESIZE_ERR_MM
#undef FREE_ERR_MM

#include "C-kern/api/test/mm/err_macros.h"

static int test_releasemode(void)
{
   memblock_t        mblock = memblock_FREE ;
   test_errortimer_t errtimer ;
   size_t            size = SIZEALLOCATED_MM() ;

   // prepare
   init_testerrortimer(&errtimer, 1, ENOMEM) ;

   // TEST ALLOC_ERR_MM
   TEST(0 == ALLOC_ERR_MM(&errtimer, 64, &mblock)) ;
   TEST(0 != mblock.addr) ;
   TEST(64 <= mblock.size) ;
   size += mblock.size ;
   TEST(size == SIZEALLOCATED_MM()) ;
   TEST(1 == errtimer.timercount) ;

   // TEST RESIZE_ERR_MM
   size -= mblock.size ;
   TEST(0 == RESIZE_ERR_MM(&errtimer, 1024, &mblock)) ;
   TEST(0 != mblock.addr) ;
   TEST(1024 <= mblock.size) ;
   size += mblock.size ;
   TEST(size == SIZEALLOCATED_MM()) ;
   TEST(1 == errtimer.timercount) ;

   // TEST FREE_ERR_MM
   size -= mblock.size ;
   TEST(0 == FREE_ERR_MM(&errtimer, &mblock)) ;
   TEST(0 == mblock.addr) ;
   TEST(0 == mblock.size) ;
   TEST(size == SIZEALLOCATED_MM()) ;
   TEST(1 == errtimer.timercount) ;

   return 0 ;
ONERR:
   return EINVAL ;
}


int unittest_test_mm_mm_test()
{
   if (test_mm_macros())      goto ONERR;
   if (test_releasemode())    goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
