/* title: MemoryErrorTestMacros

   Wrap memory manager macros into error generating macros which
   use <test_errortimer_t> to return an error at a certain time step.
   Every macro expects an <test_errortimer_t> as first parameter.

   In case <KONFIG_UNITTEST> is not defined, every TEST macro calls
   the corresponding <ALLOC_MM>, <RESIZE_MM>, or <FREE_MM> macro.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/test/mm/err_macros.h
    Header file <MemoryErrorTestMacros>.

   file: C-kern/test/mm/err_macros.c
    Implementation file <MemoryErrorTestMacros impl>.
*/
#ifndef CKERN_TEST_MM_ERR_MACROS_HEADER
#define CKERN_TEST_MM_ERR_MACROS_HEADER

#include "C-kern/api/memory/mm/mm_macros.h"


// section: Functions

// group: allocate-test

#ifdef KONFIG_UNITTEST

/* define: ALLOC_ERR_MM
 * Allocates a new memory block with error. See also <malloc_mmimpl>.
 * errtimer is of type <test_errortimer_t>. */
#define ALLOC_ERR_MM(errtimer, size, mblock) \
         ( __extension__ ({                              \
            int _err2;                                   \
            _err2 = process_testerrortimer(errtimer);    \
            if (! _err2) _err2 = ALLOC_MM(size, mblock); \
            _err2;                                       \
         }))

/* define: RESIZE_ERR_MM
 * Resizes memory block with error. See also <mresize_mmimpl>.
 * errtimer is of type <test_errortimer_t>. */
#define RESIZE_ERR_MM(errtimer, newsize, mblock) \
         ( __extension__ ({                                 \
            int _err2;                                      \
            _err2 = process_testerrortimer(errtimer);       \
            if (! _err2) _err2 = RESIZE_MM(newsize, mblock); \
            _err2;                                          \
         }))

/* define: FREE_ERR_MM
 * Frees memory block with error. See also <mfree_mmimpl>.
 * errtimer is of type <test_errortimer_t>. */
#define FREE_ERR_MM(errtimer, mblock)  \
         ( __extension__ ({            \
            int _err2;                 \
            _err2 = FREE_MM(mblock) ;  \
            if (! _err2) _err2 = process_testerrortimer(errtimer); \
            _err2;                     \
         }))

// group: test

/* function: unittest_test_mm_mm_test
 * Test <test_t> functionality. */
int unittest_test_mm_mm_test(void) ;

#else

// no unit test ==> delegate to memory manager macros

#define ALLOC_ERR_MM(errtimer, size, mblock) \
         ALLOC_MM(size, mblock)

#define RESIZE_ERR_MM(errtimer, newsize, mblock) \
         RESIZE_MM(newsize, mblock)

#define FREE_ERR_MM(errtimer, mblock) \
         FREE_MM(mblock)

#endif


#endif
