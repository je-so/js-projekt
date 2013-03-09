/* title: TransC-Token impl

   Implements <TransC-Token>.

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

   file: C-kern/api/lang/transC/transCtoken.h
    Header file <TransC-Token>.

   file: C-kern/lang/transC/transCtoken.c
    Implementation file <TransC-Token impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/lang/transC/transCtoken.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: transCtoken_t


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   transCtoken_t token = transCtoken_INIT_FREEABLE ;

   // TEST transCtoken_INIT_FREEABLE
   TEST(0 == token.type) ;
   TEST(0 == token.attr.id) ;

   // TEST transCtoken_INIT_ID
   token = (transCtoken_t) transCtoken_INIT_ID(transCtoken_BLOCK, transCtoken_id_OPEN_CURLY) ;
   TEST(token.type            == transCtoken_BLOCK) ;
   TEST(token.attr.id         == transCtoken_id_OPEN_CURLY) ;
   TEST(token.attr.precedence == 0) ;


   // TEST transCtoken_INIT_OPERATOR
   token = (transCtoken_t) transCtoken_INIT_OPERATOR(10, 12) ;
   TEST(token.type            == transCtoken_OPERATOR) ;
   TEST(token.attr.id         == 10) ;
   TEST(token.attr.precedence == 12) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   transCtoken_t token ;

   // TEST type_transctoken
   for (uint8_t i = 0; i < 15; ++i) {
      token.type = i ;
      TEST(i == type_transctoken(&token)) ;
   }

   // TEST idattr_transctoken
   for (uint8_t i = 0; i < 15; ++i) {
      token.attr.id = i ;
      TEST(i == idattr_transctoken(&token)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_lang_transc_transCtoken()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
