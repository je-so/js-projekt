/* title: TestModuleHelper1

   Implement some dummy functions called from implementation of <TestModule>.

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

   file: C-kern/main/test/helper/testmodule_helper1.h
    Header file <TestModuleHelper1>.

   file: C-kern/main/test/helper/testmodule_helper1.c
    Implementation file <TestModuleHelper1 impl>.
*/
#ifndef CKERN_MAIN_TEST_HELPER_TESTMODULE_HELPER1_HEADER
#define CKERN_MAIN_TEST_HELPER_TESTMODULE_HELPER1_HEADER

/* typedef: struct testmodule_functable_t
 * Export <testmodule_functable_t> into global namespace. */
typedef struct testmodule_functable_t     testmodule_functable_t ;



/* struct: testmodule_functable_t
 * Table of imlemented functions. */
struct testmodule_functable_t {
   int (* add)  (int arg1, int arg2) ;
   int (* sub)  (int arg1, int arg2) ;
   int (* mult) (int arg1, int arg2) ;
} ;

// group: lifetime

int init_testmodulefunctable(/*out*/testmodule_functable_t * functable) ;



#endif
