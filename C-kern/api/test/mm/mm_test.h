/* title: TestMemoryMacros

   Wrap memory manager macros into TEST macros which
   produce an error value with help of <test_errortimer_t>.
   Every macro expects an <test_errortimer_t> as first parameter.

   In case <KONFIG_UNITTEST> is not defined, every TEST macro calls
   the corresponding <ALLOC_MM>, <RESIZE_MM>, or <FREE_MM> macro.

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
#ifndef CKERN_TEST_MM_MM_TEST_HEADER
#define CKERN_TEST_MM_MM_TEST_HEADER

#include "C-kern/api/memory/mm/mm_macros.h"


// section: Functions

// group: allocate-test

#ifdef KONFIG_UNITTEST

/* define: ALLOC_TEST
 * Allocates a new memory block with error. See also <malloc_mmimpl>.
 * errtimer is of type <test_errortimer_t>. */
#define ALLOC_TEST(errtimer, size, mblock) \
         ( __extension__ ({                           \
            err = process_testerrortimer(errtimer);   \
            if (! err) err = ALLOC_MM(size, mblock);  \
            err ;                                     \
         }))

/* define: RESIZE_TEST
 * Resizes memory block with error. See also <mresize_mmimpl>.
 * errtimer is of type <test_errortimer_t>. */
#define RESIZE_TEST(errtimer, newsize, mblock) \
         ( __extension__ ({                           \
            err = process_testerrortimer(errtimer);   \
            if (! err) err = RESIZE_MM(newsize, mblock);  \
            err ;                                     \
         }))

/* define: FREE_TEST
 * Frees memory block with error. See also <mfree_mmimpl>.
 * errtimer is of type <test_errortimer_t>. */
#define FREE_TEST(errtimer, mblock) \
         ( __extension__ ({            \
            err = FREE_MM(mblock) ;    \
            if (! err) err = process_testerrortimer(errtimer) ;  \
            err ;                      \
         }))

// group: test

/* function: unittest_test_mm_mm_test
 * Test <test_t> functionality. */
int unittest_test_mm_mm_test(void) ;

#else

// no unit test ==> delegate to memory manage macros

#define ALLOC_TEST(errtimer, size, mblock) \
         ALLOC_MM(size, mblock)

#define RESIZE_TEST(errtimer, newsize, mblock) \
         RESIZE_MM(newsize, mblock)

#define FREE_TEST(errtimer, mblock) \
         FREE_MM(mblock)

#endif


#endif
