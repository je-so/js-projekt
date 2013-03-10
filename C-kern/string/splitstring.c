/* title: SplitString impl

   Implements <SplitString>.

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

   file: C-kern/api/string/splitstring.h
    Header file <SplitString>.

   file: C-kern/string/splitstring.c
    Implementation file <SplitString impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/splitstring.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

// section: splitstring_t

// group: query

bool isfree_splitstring(const splitstring_t * spstr)
{
   return   0 == spstr->nrofparts
            && 0 == spstr->stringpart[0].addr
            && 0 == spstr->stringpart[0].size
            && 0 == spstr->stringpart[1].addr
            && 0 == spstr->stringpart[1].size ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   splitstring_t spstr = splitstring_INIT_FREEABLE ;

   // TEST splitstring_INIT_FREEABLE
   TEST(0 == spstr.nrofparts) ;
   TEST(0 == spstr.stringpart[0].addr) ;
   TEST(0 == spstr.stringpart[0].size) ;
   TEST(0 == spstr.stringpart[1].addr) ;
   TEST(0 == spstr.stringpart[1].size) ;

   // TEST free_splitstring
   memset(&spstr, 255, sizeof(spstr)) ;
   free_splitstring(&spstr) ;
   TEST(spstr.nrofparts          == 0) ;
   TEST(spstr.stringpart[0].addr == 0) ;
   TEST(spstr.stringpart[0].size == 0) ;
   TEST(spstr.stringpart[1].addr == 0) ;
   TEST(spstr.stringpart[1].size == 0) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   splitstring_t spstr = splitstring_INIT_FREEABLE ;

   // TEST isfree_splitstring
   TEST(1 == isfree_splitstring(&spstr)) ;
   for (size_t i = 0; i < sizeof(spstr); ++i) {
      memset(i + (uint8_t*)&spstr, 1, 1) ;
      TEST(0 == isfree_splitstring(&spstr)) ;
      memset(i + (uint8_t*)&spstr, 0, 1) ;
      TEST(1 == isfree_splitstring(&spstr)) ;
   }

   // TEST nrofparts_splitstring
   for (uint8_t i = 15; i <= 15; --i) {
      spstr.nrofparts = i ;
      TEST(i == nrofparts_splitstring(&spstr)) ;
   }

   // TEST addr_splitstring
   for (size_t i = 15; i <= 15; --i) {
      spstr.stringpart[0].addr = (const uint8_t*)i ;
      TEST((void*)i == addr_splitstring(&spstr, 0)) ;
      TEST((void*)0 == addr_splitstring(&spstr, 1)) ;
   }
   for (size_t i = 15; i <= 15; --i) {
      spstr.stringpart[1].addr = (const uint8_t*)i ;
      TEST((void*)0 == addr_splitstring(&spstr, 0)) ;
      TEST((void*)i == addr_splitstring(&spstr, 1)) ;
   }

   // TEST size_splitstring
   for (size_t i = 15; i <= 15; --i) {
      spstr.stringpart[0].size = i ;
      TEST(i == size_splitstring(&spstr, 0)) ;
      TEST(0 == size_splitstring(&spstr, 1)) ;
   }
   for (size_t i = 15; i <= 15; --i) {
      spstr.stringpart[1].size = i ;
      TEST(0 == size_splitstring(&spstr, 0)) ;
      TEST(i == size_splitstring(&spstr, 1)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_update(void)
{
   splitstring_t spstr = splitstring_INIT_FREEABLE ;

   // TEST setnrofparts_splitstring
   for (uint8_t i = 15; i <= 15; --i) {
      setnrofparts_splitstring(&spstr, i) ;
      TEST(nrofparts_splitstring(&spstr) == i) ;
   }

   // TEST setaddr_splitstring
   for (size_t i = 15; i <= 15; --i) {
      setaddr_splitstring(&spstr, 0, (const uint8_t*)i) ;
      setaddr_splitstring(&spstr, 1, (const uint8_t*)(2*i)) ;
      TEST(addr_splitstring(&spstr, 0) == (const uint8_t*)i) ;
      TEST(addr_splitstring(&spstr, 1) == (const uint8_t*)(2*i)) ;
      TEST(size_splitstring(&spstr, 0) == 0) ;
      TEST(size_splitstring(&spstr, 1) == 0) ;
   }

   // TEST setpart_splitstring
   for (size_t i = 15; i <= 15; --i) {
      setpart_splitstring(&spstr, 0, 3*i+1, (const uint8_t*)i) ;
      setpart_splitstring(&spstr, 1, 3*i+2, (const uint8_t*)(2*i)) ;
      TEST(addr_splitstring(&spstr, 0) == (const uint8_t*)i) ;
      TEST(addr_splitstring(&spstr, 1) == (const uint8_t*)(2*i)) ;
      TEST(size_splitstring(&spstr, 0) == 3*i+1) ;
      TEST(size_splitstring(&spstr, 1) == 3*i+2) ;
   }

   // TEST setsize_splitstring
   for (size_t i = 15; i <= 15; --i) {
      setsize_splitstring(&spstr, 0, i) ;
      setsize_splitstring(&spstr, 1, 2*i) ;
      TEST(size_splitstring(&spstr, 0) == i) ;
      TEST(size_splitstring(&spstr, 1) == 2*i) ;
      TEST(addr_splitstring(&spstr, 0) == 0) ;
      TEST(addr_splitstring(&spstr, 1) == 0) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_string_splitstring()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_update())         goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif