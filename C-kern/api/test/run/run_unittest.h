/* title: Run-Unit-Test
   Offers function for calling every registered unit test.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/test/run/run_unittest.h
    Header file of <Run-Unit-Test>.

   file: C-kern/test/run/run_unittest.c
    Implementation file <Run-Unit-Test impl>.
*/
#ifndef CKERN_TEST_RUN_UNITTEST_HEADER
#define CKERN_TEST_RUN_UNITTEST_HEADER


// struct: unittest_t

// group: execute

/* function: run_unittest
 * Calls every registered unittest of the C-kern(el) system.
 * The list of all unit tests must maintained manually for newly written tests.
 * There is a static test »call_all_unittest.sh« in »C-kern/test/static/«
 * which lists every test which is not called. */
int run_unittest(int argc, const char* argv[]);

#endif