/* title: TransC-Parser impl

   Implements <TransC-Parser>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/clang/transC/transCparser.h
    Header file <TransC-Parser>.

   file: C-kern/clang/transC/transCparser.c
    Implementation file <TransC-Parser impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/clang/transC/transCparser.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   transCparser_t tcparser = transCparser_INIT_FREEABLE ;

   // TEST transCparser_INIT_FREEABLE
   TEST(0 == tcparser.dummy) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_lang_transc_transCparser()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
