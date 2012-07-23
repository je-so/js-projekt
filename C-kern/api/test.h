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


// section: Functions

// group: helper

/* function: logfailed_test
 * Prints "<filename>:<line_number>: FAILED TEST\n". */
void logfailed_test(const char * filename, unsigned line_number) ;

/* function: logworking_test
 * Prints "OK\n". */
void logworking_test(void) ;

/* function: logrun_test
 * Prints "RUN %s: ". */
void logrun_test(const char * testname) ;

// group: macros

/* define: TEST
 * Tests CONDITION and exits on error with jump to ONABORT: label.
 * If CONDITION fails an error is printed and further tests are skipped.
 * The macro jump jumps to label ONABORT: with help of goto.
 *
 * Parameters:
 * CONDITION          - Condition which is tested to be true.
 *
 * Usage:
 * The following demonstrates how this macro is used:
 *
 * > int unittest_demonstration()
 * > {
 * >    testtype_t testtype = type_INIT_FREEABLE ;
 * >    TEST(0 == init_testtype(&testtype)) ;
 * >    TEST(0 == free_testtype(&testtype)) ;
 * >    return 0 ; // success
 * > ONABORT:
 * >    free_testtype(&testtype) ;
 * >    return EINVAL ; // any error code
 * > }
 * */
#define TEST(CONDITION)                      \
   if ( !(CONDITION) ) {                     \
      logfailed_test(__FILE__, __LINE__) ;   \
      goto ONABORT ;                         \
   }

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_functions
 * Unittest for exported function in <Test>. */
int unittest_test_functions(void) ;
#endif


#endif
