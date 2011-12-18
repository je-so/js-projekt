/* title: Test
   Defines a macro for use in unit- and other types of tests.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/test.h
    Header file of <Test>.

   file: C-kern/test/test.c
    Implementation file <Test impl>.
*/
#ifndef CKERN_API_TEST_HEADER
#define CKERN_API_TEST_HEADER

#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/umg/log_macros.h"

// section: Functions

// group: helper

/* function: logfailed_test
 * Prints "<filename>:<line_number>: FAILED TEST\n". */
extern void logfailed_test(const char * filename, unsigned line_number) ;

// group: macros

/* define: TEST_ONERROR_GOTO
 * Tests CONDITION and exits on error.
 * If CONDITION fails an error is printed and further tests are skipped cause it jumps to
 * ERROR_LABEL with help of goto.
 *
 * Parameters:
 * CONDITION          - Condition which is tested to be true.
 * ERROR_LABEL        - Name of label the test macro jumps in case of error.
 *
 * Usage:
 * The following demonstrates how this macro is used:
 *
 * > #define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION, ABBRUCH)
 * > int test_demonstration()
 * > {
 * >    testtype_t testtype = type_INIT_FREEABLE ;
 * >    TEST(0 == init_testtype(&testtype)) ;
 * >    TEST(0 == free_testtype(&testtype)) ;
 * >    return 0 ; // success
 * > ABBRUCH:
 * >    free_testtype(&testtype) ;
 * >    return EINVAL ; // any error code
 * > }
 * */
#define TEST_ONERROR_GOTO(CONDITION, ERROR_LABEL)  \
   if ( !(CONDITION) ) {                           \
      logfailed_test(__FILE__, __LINE__) ;         \
      goto ERROR_LABEL ;                           \
   }

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_functions
 * Unittest for exported function in <Test>. */
extern int unittest_test_functions(void) ;
#endif


#endif
