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

// section: conststring_t

// group: compare

bool isequalasciicase_conststring(const conststring_t * str, const conststring_t * str2)
{
   if (str->size != str2->size) return false ;

   for(size_t i = str->size; (i --);) {
      uint8_t c1 = str->addr[i] ;
      uint8_t c2 = str2->addr[i] ;
      uint8_t x  = c1 ^ c2 ;
      if (x) {
         static_assert(0x20 == ('a' - 'A'), ) ;
         if (0x20 == x) {
            x |= c2 ;
            if ('a' <= x && x <= 'z') continue ;
         }
         return false ;
      }
   }

   return true ;
}


// section: string_t

// group: lifetime

void init_string(/*out*/string_t * str, size_t size, uint8_t string[size])
{
   str->addr = string ;
   str->size = size ;
}

int initfl_string(/*out*/string_t * str, uint8_t * first, uint8_t * last)
{
   int err ;

   VALIDATE_INPARAM_TEST((last >= first) && ((last+1) > last), ABBRUCH, LOG_PTR(first); LOG_PTR(last) ) ;

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

static int test_initfreeconststr(void)
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

static int test_initfreestr(void)
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

static int test_compare(void)
{
   conststring_t  str1 ;
   conststring_t  str2 ;
   const char     * utf8str1 ;
   const char     * utf8str2 ;

   // TEST isequalasciicase_conststring simple utf8 string
   utf8str1 = "\U001fffff abcd\U0000ffff abcd\u07ff abcd\x7f abcd" ;
   utf8str2 = "\U001fffff ABCD\U0000ffff ABCD\u07ff abcd\x7f ABCD" ;
   init_conststring(&str1, strlen(utf8str1), (const uint8_t*)utf8str1) ;
   init_conststring(&str2, strlen(utf8str2), (const uint8_t*)utf8str2) ;
   TEST(true == isequalasciicase_conststring(&str1, &str2)) ;

   // TEST isequalasciicase_conststring with all characters from A-Z
   utf8str1 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@^¬^°üöä" ;
   utf8str2 = "abcdefghijklmnopqrstuvwxyz1234567890!@^¬^°üöä" ;
   init_conststring(&str1, strlen(utf8str1), (const uint8_t*)utf8str1) ;
   init_conststring(&str2, strlen(utf8str2), (const uint8_t*)utf8str2) ;
   TEST(true == isequalasciicase_conststring(&str1, &str2)) ;
   TEST(true == isequalasciicase_conststring(&str1, &str1)) ;
   TEST(true == isequalasciicase_conststring(&str2, &str2)) ;

   // TEST isequalasciicase_conststring inequality for all other 256 characters
   uint8_t buffer1[256] ;
   uint8_t buffer2[256] ;
   for(unsigned i = 0; i < sizeof(buffer1); ++i) {
      buffer1[i] = (uint8_t)i ;
      buffer2[i] = (uint8_t)i ;
   }
   init_conststring(&str1, sizeof(buffer1), buffer1) ;
   init_conststring(&str2, sizeof(buffer2), buffer2) ;
   TEST(true == isequalasciicase_conststring(&str1, &str2)) ;
   for(unsigned i = 0; i < sizeof(buffer1); ++i) {
      bool isEqual = (  ('a' <= i && i <= 'z')
                     || ('A' <= i && i <= 'Z')) ;
      buffer1[i] ^= 0x20 ; // change case (works only for a-z or A-Z)
      TEST(isEqual == isequalasciicase_conststring(&str1, &str2)) ;
      buffer1[i] = (uint8_t)i ;
      buffer2[i] ^= 0x20 ; // change case (works only for a-z or A-Z)
      TEST(isEqual == isequalasciicase_conststring(&str1, &str2)) ;
      buffer2[i] = (uint8_t)i ;
   }

   // TEST isequalasciicase_conststring inequality for size
   for(unsigned i = 0; i < sizeof(buffer1); ++i) {
      TEST(true == isequalasciicase_conststring(&str1, &str2)) ;
      str1.size = i ;
      TEST(false == isequalasciicase_conststring(&str1, &str2)) ;
      str1.size = sizeof(buffer1) ;
      str2.size = i ;
      TEST(false == isequalasciicase_conststring(&str1, &str2)) ;
      str2.size = sizeof(buffer2) ;
   }

   // TEST isequalasciicase_conststring inequality for one differing char
   for(unsigned i = 0; i < sizeof(buffer1); ++i) {
      TEST(true == isequalasciicase_conststring(&str1, &str2)) ;
      buffer1[i] = (uint8_t)(i+1) ;
      TEST(false == isequalasciicase_conststring(&str1, &str2)) ;
      buffer1[i] = (uint8_t)i ;
      buffer2[i] = (uint8_t)(i+1) ;
      TEST(false == isequalasciicase_conststring(&str1, &str2)) ;
      buffer2[i] = (uint8_t)i ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_string()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfreeconststr())  goto ABBRUCH ;
   if (test_initfreestr())       goto ABBRUCH ;
   if (test_compare())           goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
