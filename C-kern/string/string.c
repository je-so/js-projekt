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

// group: lifetime

int initfl_string(/*out*/string_t * str, const uint8_t * first, const uint8_t * last)
{
   int err ;

   VALIDATE_INPARAM_TEST((last+1) > last, ONABORT, PRINTPTR_LOG(last)) ;

   str->addr = first ;
   str->size = last < first ? 0 : (size_t) (1 + last - first) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int initse_string(/*out*/string_t * str, const uint8_t * start, const uint8_t * end)
{
   int err ;

   VALIDATE_INPARAM_TEST(end >= start, ONABORT, PRINTPTR_LOG(end); PRINTPTR_LOG(start) ) ;

   str->addr = start ;
   str->size = (size_t) (end - start) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: compare

bool isequalasciicase_string(const string_t * str, const string_t * str2)
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


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   string_t    str ;
   uint8_t     test[256] ;

   // TEST string_INIT_FREEABLE
   str = (string_t) string_INIT_FREEABLE ;
   TEST(0 == str.addr) ;
   TEST(0 == str.size) ;
   TEST(1 == isempty_string(&str)) ;
   TEST(1 == isfree_string(&str)) ;

   // TEST string_INIT
   str = (string_t) string_INIT(5, &test[10]) ;
   TEST(str.addr == &test[10]) ;
   TEST(str.size == 5) ;
   str = (string_t) string_INIT(3, &test[11]) ;
   TEST(str.addr == &test[11]) ;
   TEST(str.size == 3) ;

   // TEST string_INIT_CSTR
   const char * cstr[] = { "123", "123456" } ;
   for (unsigned i = 0; i < nrelementsof(cstr); ++i) {
      str = (string_t) string_INIT_CSTR(cstr[i]) ;
      TEST(str.addr == (const uint8_t*)cstr[i]) ;
      TEST(str.size == strlen(cstr[i])) ;
   }

   // TEST init_string
   init_string(&str, 3, &test[12]) ;
   TEST(str.addr == &test[12]) ;
   TEST(str.size == 3) ;
   init_string(&str, 5, &test[13]) ;
   TEST(str.addr == &test[13]) ;
   TEST(str.size == 5) ;

   // TEST init_string: free string
   init_string(&str, 0, 0) ;
   TEST(0 == str.addr) ;
   TEST(0 == str.size) ;

   // TEST init_string: empty string
   init_string(&str, 0, (const uint8_t*)"") ;
   TEST(str.addr == (const uint8_t*)"") ;
   TEST(str.size == 0) ;

   // TEST initfl_string
   initfl_string(&str, &test[21], &test[23]) ;
   TEST(str.addr == &test[21]) ;
   TEST(str.size == 3) ;
   initfl_string(&str, &test[24], &test[28]) ;
   TEST(str.addr == &test[24]) ;
   TEST(str.size == 5) ;
   initfl_string(&str, &test[1], &test[1]) ;
   TEST(str.addr == &test[1]) ;
   TEST(str.size == 1) ;

   // TEST initfl_string: empty string
   initfl_string(&str, &test[1], &test[0]) ;
   TEST(str.addr == &test[1]) ;
   TEST(str.size == 0) ;

   // TEST initse_string
   initse_string(&str,  &test[21], &test[24]) ;
   TEST(str.addr == &test[21]) ;
   TEST(str.size == 3) ;
   initse_string(&str, &test[24], &test[29]) ;
   TEST(str.addr == &test[24]) ;
   TEST(str.size == 5) ;
   initse_string(&str, &test[25], &test[26]) ;
   TEST(str.addr == &test[25]) ;
   TEST(str.size == 1) ;

   // TEST initse_string: empty string
   initse_string(&str,  &test[2], &test[2]) ;
   TEST(str.addr == &test[2]) ;
   TEST(str.size == 0) ;

   // TEST isempty_string
   str.addr = (const uint8_t*) "" ;
   str.size = 0 ;
   TEST(1 == isempty_string(&str)) ;
   str.size = 1 ;
   TEST(0 == isempty_string(&str)) ;
   str.addr = 0 ;
   TEST(0 == isempty_string(&str)) ;
   str.size = 0 ;
   TEST(1 == isempty_string(&str)) ;

   // TEST isfree_string
   str = (string_t) string_INIT_FREEABLE ;
   TEST(1 == isfree_string(&str)) ;
   str.addr = (const uint8_t*) "" ;
   TEST(0 == isfree_string(&str)) ;
   str.addr = 0 ;
   str.size = 1 ;
   TEST(0 == isfree_string(&str)) ;
   str.size = 0 ;
   TEST(1 == isfree_string(&str)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_compare(void)
{
   string_t    str1 ;
   string_t    str2 ;
   const char  * utf8str1 ;
   const char  * utf8str2 ;

   // TEST isequalasciicase_string simple utf8 string
   utf8str1 = "\U001fffff abcd\U0000ffff abcd\u07ff abcd\x7f abcd" ;
   utf8str2 = "\U001fffff ABCD\U0000ffff ABCD\u07ff abcd\x7f ABCD" ;
   init_string(&str1, strlen(utf8str1), (const uint8_t*)utf8str1) ;
   init_string(&str2, strlen(utf8str2), (const uint8_t*)utf8str2) ;
   TEST(true == isequalasciicase_string(&str1, &str2)) ;

   // TEST isequalasciicase_string with all characters from A-Z
   utf8str1 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@^¬^°üöä" ;
   utf8str2 = "abcdefghijklmnopqrstuvwxyz1234567890!@^¬^°üöä" ;
   init_string(&str1, strlen(utf8str1), (const uint8_t*)utf8str1) ;
   init_string(&str2, strlen(utf8str2), (const uint8_t*)utf8str2) ;
   TEST(true == isequalasciicase_string(&str1, &str2)) ;
   TEST(true == isequalasciicase_string(&str1, &str1)) ;
   TEST(true == isequalasciicase_string(&str2, &str2)) ;

   // TEST isequalasciicase_string inequality for all other 256 characters
   uint8_t buffer1[256] ;
   uint8_t buffer2[256] ;
   for(unsigned i = 0; i < sizeof(buffer1); ++i) {
      buffer1[i] = (uint8_t)i ;
      buffer2[i] = (uint8_t)i ;
   }
   init_string(&str1, sizeof(buffer1), buffer1) ;
   init_string(&str2, sizeof(buffer2), buffer2) ;
   TEST(true == isequalasciicase_string(&str1, &str2)) ;
   for(unsigned i = 0; i < sizeof(buffer1); ++i) {
      bool isEqual = (  ('a' <= i && i <= 'z')
                     || ('A' <= i && i <= 'Z')) ;
      buffer1[i] ^= 0x20 ; // change case (works only for a-z or A-Z)
      TEST(isEqual == isequalasciicase_string(&str1, &str2)) ;
      buffer1[i] = (uint8_t)i ;
      buffer2[i] ^= 0x20 ; // change case (works only for a-z or A-Z)
      TEST(isEqual == isequalasciicase_string(&str1, &str2)) ;
      buffer2[i] = (uint8_t)i ;
   }

   // TEST isequalasciicase_string inequality for size
   for(unsigned i = 0; i < sizeof(buffer1); ++i) {
      TEST(true == isequalasciicase_string(&str1, &str2)) ;
      str1.size = i ;
      TEST(false == isequalasciicase_string(&str1, &str2)) ;
      str1.size = sizeof(buffer1) ;
      str2.size = i ;
      TEST(false == isequalasciicase_string(&str1, &str2)) ;
      str2.size = sizeof(buffer2) ;
   }

   // TEST isequalasciicase_string inequality for one differing char
   for(unsigned i = 0; i < sizeof(buffer1); ++i) {
      TEST(true == isequalasciicase_string(&str1, &str2)) ;
      buffer1[i] = (uint8_t)(i+1) ;
      TEST(false == isequalasciicase_string(&str1, &str2)) ;
      buffer1[i] = (uint8_t)i ;
      buffer2[i] = (uint8_t)(i+1) ;
      TEST(false == isequalasciicase_string(&str1, &str2)) ;
      buffer2[i] = (uint8_t)i ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_skip(void)
{
   string_t str ;

   // TEST skipbyte_string
   str = (string_t) string_INIT(0, (uint8_t*)0) ;
   skipbyte_string(&str) ;
   TEST(str.addr == (uint8_t*)1) ;
   TEST(str.size == (size_t)-1) ;
   str = (string_t) string_INIT(44, (uint8_t*)0) ;
   for (unsigned i = 1; i <= 44; ++i) {
      skipbyte_string(&str) ;
      TEST(str.addr == (uint8_t*)i) ;
      TEST(str.size == 44u-i) ;
   }

   // TEST skipbytes_string
   str = (string_t) string_INIT(1000, (uint8_t*)3) ;
   skipbytes_string(&str, 1000) ;
   TEST(str.addr == (void*)1003) ;
   TEST(str.size == 0) ;
   for (unsigned i = 0; i < 5; ++i) {
      str = (string_t) string_INIT(100+i, (void*)i) ;
      skipbytes_string(&str, 10*i) ;
      TEST(str.addr == (void*)(i+10*i)) ;
      TEST(str.size == 100+i-(10*i)) ;
   }

   // TEST tryskipbytes_string
   str = (string_t) string_INIT(1000, (uint8_t*)3) ;
   TEST(0 == tryskipbytes_string(&str, 1000)) ;
   TEST(str.addr == (void*)1003) ;
   TEST(str.size == 0) ;
   for(unsigned i = 0; i < 5; ++i) {
      str = (string_t) string_INIT(100+i, (void*)i) ;
      TEST(0 == tryskipbytes_string(&str, 10*i)) ;
      TEST(str.addr == (void*)(i+10*i)) ;
      TEST(str.size == 100+i-(10*i)) ;
   }

   // TEST EINVAL
   str = (string_t) string_INIT(100, (uint8_t*)0) ;
   TEST(EINVAL == tryskipbytes_string(&str, 101)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_generic(void)
{
   struct {
      int            dummy1 ;
      const uint8_t  * addr ;
      size_t         size ;
      int            dummy2 ;
   }        str1 ;

   // TEST genericcast_string
   TEST((const string_t*)&str1.addr == genericcast_string(&str1)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_string()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_compare())        goto ONABORT ;
   if (test_skip())           goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
