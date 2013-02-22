/* title: SplitToken impl

   Implements <SplitToken>.

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

   file: C-kern/api/lang/splittoken.h
    Header file <SplitToken>.

   file: C-kern/lang/splittoken.c
    Implementation file <SplitToken impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/lang/splittoken.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

// section: splittoken_t

// group: query

bool isfree_splittoken(const splittoken_t * sptok)
{
   return   0 == sptok->tokentype
            && 0 == sptok->tokensubtype
            && 0 == sptok->nrofstrings
            && 0 == sptok->stringpart[0].addr
            && 0 == sptok->stringpart[0].size
            && 0 == sptok->stringpart[1].addr
            && 0 == sptok->stringpart[1].size ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   splittoken_t sptok = splittoken_INIT_FREEABLE ;

   // TEST splittoken_INIT_FREEABLE
   TEST(0 == sptok.tokentype) ;
   TEST(0 == sptok.tokensubtype) ;
   TEST(0 == sptok.nrofstrings) ;
   TEST(0 == sptok.stringpart[0].addr) ;
   TEST(0 == sptok.stringpart[0].size) ;
   TEST(0 == sptok.stringpart[1].addr) ;
   TEST(0 == sptok.stringpart[1].size) ;

   // TEST free_splittoken
   memset(&sptok, 255, sizeof(sptok)) ;
   free_splittoken(&sptok) ;
   TEST(sptok.tokentype          == 0) ;
   TEST(sptok.tokensubtype       == 0) ;
   TEST(sptok.nrofstrings        == 0) ;
   TEST(sptok.stringpart[0].addr == 0) ;
   TEST(sptok.stringpart[0].size == 0) ;
   TEST(sptok.stringpart[1].addr == 0) ;
   TEST(sptok.stringpart[1].size == 0) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   splittoken_t sptok = splittoken_INIT_FREEABLE ;

   // TEST isfree_splittoken
   TEST(1 == isfree_splittoken(&sptok)) ;
   for (size_t i = 0; i < sizeof(sptok); ++i) {
      memset(i + (uint8_t*)&sptok, 1, 1) ;
      TEST(0 == isfree_splittoken(&sptok)) ;
      memset(i + (uint8_t*)&sptok, 0, 1) ;
      TEST(1 == isfree_splittoken(&sptok)) ;
   }

   // TEST type_splittoken
   for (uint16_t i = 15; i <= 15; --i) {
      sptok.tokentype = i ;
      TEST(i == type_splittoken(&sptok)) ;
   }

   // TEST subtype_splittoken
   for (uint8_t i = 15; i <= 15; --i) {
      sptok.tokensubtype = i ;
      TEST(i == subtype_splittoken(&sptok)) ;
   }

   // TEST nrofstrings_splittoken
   for (uint8_t i = 15; i <= 15; --i) {
      sptok.nrofstrings = i ;
      TEST(i == nrofstrings_splittoken(&sptok)) ;
   }

   // TEST stringaddr_splittoken
   for (size_t i = 15; i <= 15; --i) {
      sptok.stringpart[0].addr = (const uint8_t*)i ;
      TEST((void*)i == stringaddr_splittoken(&sptok, 0)) ;
      TEST((void*)0 == stringaddr_splittoken(&sptok, 1)) ;
   }
   for (size_t i = 15; i <= 15; --i) {
      sptok.stringpart[1].addr = (const uint8_t*)i ;
      TEST((void*)0 == stringaddr_splittoken(&sptok, 0)) ;
      TEST((void*)i == stringaddr_splittoken(&sptok, 1)) ;
   }

   // TEST stringsize_splittoken
   for (size_t i = 15; i <= 15; --i) {
      sptok.stringpart[0].size = i ;
      TEST(i == stringsize_splittoken(&sptok, 0)) ;
      TEST(0 == stringsize_splittoken(&sptok, 1)) ;
   }
   for (size_t i = 15; i <= 15; --i) {
      sptok.stringpart[1].size = i ;
      TEST(0 == stringsize_splittoken(&sptok, 0)) ;
      TEST(i == stringsize_splittoken(&sptok, 1)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_update(void)
{
   splittoken_t sptok = splittoken_INIT_FREEABLE ;

   // TEST settype_splittoken
   for (uint16_t i = 15; i <= 15; --i) {
      settype_splittoken(&sptok, i, (uint8_t)(2*i)) ;
      TEST(i   == type_splittoken(&sptok)) ;
      TEST(2*i == subtype_splittoken(&sptok)) ;
      TEST(0   == nrofstrings_splittoken(&sptok)) ;
      TEST(stringsize_splittoken(&sptok, 0) == 0) ;
      TEST(stringaddr_splittoken(&sptok, 0) == 0) ;
      TEST(stringsize_splittoken(&sptok, 1) == 0) ;
      TEST(stringaddr_splittoken(&sptok, 1) == 0) ;
   }

   // TEST setnrofstrings_splittoken
   for (uint8_t i = 15; i <= 15; --i) {
      setnrofstrings_splittoken(&sptok, i) ;
      TEST(nrofstrings_splittoken(&sptok) == i) ;
   }

   // TEST setstringaddr_splittoken
   for (size_t i = 15; i <= 15; --i) {
      setstringaddr_splittoken(&sptok, 0, (const uint8_t*)i) ;
      setstringaddr_splittoken(&sptok, 1, (const uint8_t*)(2*i)) ;
      TEST(stringaddr_splittoken(&sptok, 0) == (const uint8_t*)i) ;
      TEST(stringaddr_splittoken(&sptok, 1) == (const uint8_t*)(2*i)) ;
      TEST(stringsize_splittoken(&sptok, 0) == 0) ;
      TEST(stringsize_splittoken(&sptok, 1) == 0) ;
   }

   // TEST setstringsize_splittoken
   for (size_t i = 15; i <= 15; --i) {
      setstringsize_splittoken(&sptok, 0, i) ;
      setstringsize_splittoken(&sptok, 1, 2*i) ;
      TEST(stringsize_splittoken(&sptok, 0) == i) ;
      TEST(stringsize_splittoken(&sptok, 1) == 2*i) ;
      TEST(stringaddr_splittoken(&sptok, 0) == 0) ;
      TEST(stringaddr_splittoken(&sptok, 1) == 0) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_lang_splittoken()
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
