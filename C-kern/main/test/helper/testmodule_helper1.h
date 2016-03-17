/* title: TestModuleHelper1

   Implement some dummy functions called from implementation of <TestModule>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/main/test/helper/testmodule_helper1.h
    Header file <TestModuleHelper1>.

   file: C-kern/main/test/helper/testmodule_helper1.c
    Implementation file <TestModuleHelper1 impl>.
*/
#ifndef CKERN_MAIN_TEST_HELPER_TESTMODULE_HELPER1_HEADER
#define CKERN_MAIN_TEST_HELPER_TESTMODULE_HELPER1_HEADER

// === exported types
struct testmodule_functable_t;


/* struct: testmodule_functable_t
 * Table of imlemented functions. */
typedef struct testmodule_functable_t {
   int (* add)  (int arg1, int arg2) ;
   int (* sub)  (int arg1, int arg2) ;
   int (* mult) (int arg1, int arg2) ;
} testmodule_functable_t;

// group: lifetime

int init_testmodulefunctable(/*out*/testmodule_functable_t * functable);



#endif
