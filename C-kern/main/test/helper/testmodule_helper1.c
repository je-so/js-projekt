/* title: TestModuleHelper1 impl

   Implements <TestModuleHelper1>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/main/test/helper/testmodule_helper1.h
    Header file <TestModuleHelper1>.

   file: C-kern/main/test/helper/testmodule_helper1.c
    Implementation file <TestModuleHelper1 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/main/test/helper/testmodule_helper1.h"


// section: Functions

// group: operations

static int add_testmodule(int arg1, int arg2)
{
   return arg1 + arg2 ;
}

static int sub_testmodule(int arg1, int arg2)
{
   return arg1 - arg2 ;
}

static int mult_testmodule(int arg1, int arg2)
{
   return arg1 * arg2 ;
}


// section: testmodule_functable_t

// group: lifetime

int init_testmodulefunctable(/*out*/testmodule_functable_t * functable)
{
   // addresses of static functions work
   functable->add  = &add_testmodule ;
   functable->sub  = &sub_testmodule ;
   functable->mult = &mult_testmodule ;
   return 0 ;
}
