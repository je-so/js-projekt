/* title: String impl
   Implements <String>.

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

   file: C-kern/api/string/string.h
    Header file of <String>.

   file: C-kern/string/string.c
    Implementation file <String impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: string_t

// group: implementation

void init_string(/*out*/string_t * str, size_t size, uint8_t string[size])
{
   str->addr = string ;
   str->size = size ;
}

int initfl_string(/*out*/string_t * str, uint8_t * first, uint8_t * last)
{
   int err ;

   VALIDATE_INPARAM_TEST(last >= first, ABBRUCH, LOG_PTR(first); LOG_PTR(last) ) ;

   str->addr = first ;
   str->size = (size_t) (1 + last - first) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initse_string(/*out*/string_t * str, uint8_t * start, uint8_t * end)
{
   int err ;

   VALIDATE_INPARAM_TEST(end >= start, ABBRUCH, LOG_PTR(end); LOG_PTR(start) ) ;

   str->addr = start ;
   str->size = (size_t) (end - start) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_conststring(void)
{
   conststring_t  str ;
   const uint8_t  test[256] ;

   // TEST conststring_INIT_FREEABLE
   str = (conststring_t) conststring_INIT_FREEABLE ;
   TEST(0 == str.addr) ;
   TEST(0 == str.size) ;

   // TEST conststring_INIT
   str = (conststring_t) conststring_INIT(5, &test[10]) ;
   TEST(str.addr == &test[10]) ;
   TEST(str.size == 5) ;
   str = (conststring_t) conststring_INIT(3, &test[11]) ;
   TEST(str.addr == &test[11]) ;
   TEST(str.size == 3) ;

   // TEST init_conststring
   init_conststring(&str, 3, &test[12]) ;
   TEST(str.addr == &test[12]) ;
   TEST(str.size == 3) ;
   init_conststring(&str, 5, &test[13]) ;
   TEST(str.addr == &test[13]) ;
   TEST(str.size == 5) ;

   // TEST initfl_conststring
   initfl_conststring(&str, &test[21], &test[23]) ;
   TEST(str.addr == &test[21]) ;
   TEST(str.size == 3) ;
   initfl_conststring(&str, &test[24], &test[28]) ;
   TEST(str.addr == &test[24]) ;
   TEST(str.size == 5) ;

   // TEST initse_conststring
   initse_conststring(&str,  &test[21], &test[24]) ;
   TEST(str.addr == &test[21]) ;
   TEST(str.size == 3) ;
   initse_conststring(&str, &test[24], &test[29]) ;
   TEST(str.addr == &test[24]) ;
   TEST(str.size == 5) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_string(void)
{
   string_t    str ;
   uint8_t     test[256] ;

   // TEST string_INIT_FREEABLE
   str = (string_t) string_INIT_FREEABLE ;
   TEST(0 == str.addr) ;
   TEST(0 == str.size) ;

   // TEST string_INIT
   str = (string_t) string_INIT(5, &test[10]) ;
   TEST(str.addr == &test[10]) ;
   TEST(str.size == 5) ;
   str = (string_t) string_INIT(3, &test[11]) ;
   TEST(str.addr == &test[11]) ;
   TEST(str.size == 3) ;

   // TEST init_string
   init_string(&str, 3, &test[12]) ;
   TEST(str.addr == &test[12]) ;
   TEST(str.size == 3) ;
   init_string(&str, 5, &test[13]) ;
   TEST(str.addr == &test[13]) ;
   TEST(str.size == 5) ;

   // TEST initfl_string
   initfl_string(&str, &test[21], &test[23]) ;
   TEST(str.addr == &test[21]) ;
   TEST(str.size == 3) ;
   initfl_string(&str, &test[24], &test[28]) ;
   TEST(str.addr == &test[24]) ;
   TEST(str.size == 5) ;

   // TEST initse_string
   initse_string(&str,  &test[21], &test[24]) ;
   TEST(str.addr == &test[21]) ;
   TEST(str.size == 3) ;
   initse_string(&str, &test[24], &test[29]) ;
   TEST(str.addr == &test[24]) ;
   TEST(str.size == 5) ;

   // TEST asconst_string
   for(int i = 0; i < 5; ++i) {
      string_t       * str1 = (string_t *) i ;
      conststring_t  * str2 = (conststring_t *) i ;
      TEST(str2 == asconst_string(str1)) ;
   }

   // TEST trimleft_string
   str = (string_t) string_INIT(1000, (uint8_t*)3) ;
   TEST(0 == trytrimleft_string(&str, 1000)) ;
   TEST(str.addr == (void*)1003) ;
   TEST(str.size == 0) ;
   for(unsigned i = 0; i < 5; ++i) {
      str = (string_t) string_INIT(100+i, (void*)i) ;
      TEST(0 == trytrimleft_string(&str, 10*i)) ;
      TEST(str.addr == (void*)(i+10*i)) ;
      TEST(str.size == 100+i-(10*i)) ;
   }

   // TEST ENOMEM
   str = (string_t) string_INIT(100, (void*)3) ;
   TEST(ENOMEM == trytrimleft_string(&str, 101)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_string()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_conststring())    goto ABBRUCH ;
   if (test_string())         goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
