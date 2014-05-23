/* title: TestMemoryMacros impl

   Implements <TestMemoryMacros>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/test/mm/mm_test.h
    Header file <TestMemoryMacros>.

   file: C-kern/test/mm/mm_test.c
    Implementation file <TestMemoryMacros impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/mm/mm_test.h"
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
   int err ;
   memblock_t        mblocks[2] ;
   test_errortimer_t errtimer ;
   size_t            size = SIZEALLOCATED_MM() ;

   // TEST ALLOC_TEST
   memset(mblocks, 255 ,sizeof(mblocks)) ;
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      init_testerrortimer(&errtimer, 2, ENOMEM) ;
      TEST(0 == ALLOC_TEST(&errtimer, 32 + 32 * i, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 32 + 32 * i) ;
      size += mblocks[i].size ;
      TEST(size == SIZEALLOCATED_MM()) ;
      memblock_t dummy = mblocks[i] ;
      TEST(ENOMEM == ALLOC_TEST(&errtimer, 32 + 32 * i, &dummy)) ;
      TEST(dummy.addr == mblocks[i].addr) ;
      TEST(dummy.size == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   // TEST RESIZE_TEST
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      init_testerrortimer(&errtimer, 2, ENOMEM) ;
      size -= mblocks[i].size ;
      TEST(0 == RESIZE_TEST(&errtimer, 1024, &mblocks[i])) ;
      TEST(mblocks[i].addr != 0) ;
      TEST(mblocks[i].size >= 1024) ;
      size += mblocks[i].size ;
      TEST(size == SIZEALLOCATED_MM()) ;
      memblock_t dummy = mblocks[i] ;
      TEST(ENOMEM == RESIZE_TEST(&errtimer, 3000, &dummy)) ;
      TEST(dummy.addr == mblocks[i].addr) ;
      TEST(dummy.size == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   // TEST FREE_TEST
   for (unsigned i = 0; i < lengthof(mblocks); ++i) {
      init_testerrortimer(&errtimer, 1, ENOMEM) ;
      size -= mblocks[i].size ;
      TEST(ENOMEM == FREE_TEST(&errtimer, &mblocks[i])) ;
      TEST(0 == mblocks[i].addr) ;
      TEST(0 == mblocks[i].size) ;
      TEST(size == SIZEALLOCATED_MM()) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

#undef ALLOC_TEST
#undef RESIZE_TEST
#undef FREE_TEST
#undef CKERN_TEST_MM_MM_TEST_HEADER
#undef KONFIG_UNITTEST

#include "C-kern/api/test/mm/mm_test.h"

static int test_mm_macros_release(void)
{
   memblock_t        mblock = memblock_FREE ;
   test_errortimer_t errtimer ;
   size_t            size = SIZEALLOCATED_MM() ;

   // prepare
   init_testerrortimer(&errtimer, 1, ENOMEM) ;

   // TEST ALLOC_TEST
   TEST(0 == ALLOC_TEST(&errtimer, 64, &mblock)) ;
   TEST(0 != mblock.addr) ;
   TEST(64 <= mblock.size) ;
   size += mblock.size ;
   TEST(size == SIZEALLOCATED_MM()) ;
   TEST(1 == errtimer.timercount) ;

   // TEST RESIZE_TEST
   size -= mblock.size ;
   TEST(0 == RESIZE_TEST(&errtimer, 1024, &mblock)) ;
   TEST(0 != mblock.addr) ;
   TEST(1024 <= mblock.size) ;
   size += mblock.size ;
   TEST(size == SIZEALLOCATED_MM()) ;
   TEST(1 == errtimer.timercount) ;

   // TEST FREE_TEST
   size -= mblock.size ;
   TEST(0 == FREE_TEST(&errtimer, &mblock)) ;
   TEST(0 == mblock.addr) ;
   TEST(0 == mblock.size) ;
   TEST(size == SIZEALLOCATED_MM()) ;
   TEST(1 == errtimer.timercount) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}


int unittest_test_mm_mm_test()
{
   if (test_mm_macros())         goto ONABORT ;
   if (test_mm_macros_release()) goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
