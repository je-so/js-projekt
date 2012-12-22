/* title: Unittest
   Offers function for calling every registered unit test.

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

   file: C-kern/api/test/run/unittest.h
    Header file of <Unittest>.

   file: C-kern/test/run_unittest.c
    Implementation file <Unittest impl>.
*/
#ifndef CKERN_TEST_RUN_UNITTEST_HEADER
#define CKERN_TEST_RUN_UNITTEST_HEADER

/* function: run_unittest
 * Calls every registered unittest of the C-kern(el) system.
 * The list of all unit tests must maintained manually for newly written tests.
 * There is a static test »call_all_unittest.sh« in »C-kern/test/static/«
 * which lists every test which is not called. */
int run_unittest(void) ;

#endif
