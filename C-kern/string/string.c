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

void init_string(/*out*/string_t * str, size_t size, const char string[size])
{
   str->addr = string ;
   str->size = size ;
}

int initfl_string(/*out*/string_t * str, const uint8_t * first, const uint8_t * last)
{
   int err ;

   VALIDATE_INPARAM_TEST(last >= first, ABBRUCH, LOG_PTR(first); LOG_PTR(last) ) ;

   str->addr = (const char*) first ;
   str->size = (size_t) (1 + last - first) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initse_string(/*out*/string_t * str, const uint8_t * start, const uint8_t * end)
{
   int err ;

   VALIDATE_INPARAM_TEST(end >= start, ABBRUCH, LOG_PTR(end); LOG_PTR(start) ) ;

   str->addr = (const char*) start ;
   str->size = (size_t) (end - start) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_init(void)
{
   string_t    str ;
   const char  * test ;

   // TEST string_INIT_FREEABLE
   str = (string_t) string_INIT_FREEABLE ;
   TEST(0 == str.addr) ;
   TEST(0 == str.size) ;

   // TEST string_INIT
   test = "12345" ;
   str = (string_t) string_INIT(strlen(test), test) ;
   TEST(str.addr == test) ;
   TEST(str.size == 5) ;
   test = "xx5" ;
   str = (string_t) string_INIT(strlen(test), test) ;
   TEST(str.addr == test) ;
   TEST(str.size == 3) ;

   // TEST init_string
   test = "xyc" ;
   init_string(&str, strlen(test), test) ;
   TEST(str.addr == test) ;
   TEST(str.size == 3) ;
   test = "xx5yy" ;
   init_string(&str, strlen(test), test) ;
   TEST(str.addr == test) ;
   TEST(str.size == 5) ;

   // TEST initfl_string
   test = "xyc" ;
   initfl_string(&str, (const uint8_t*)test, (const uint8_t*)test+2) ;
   TEST(str.addr == test) ;
   TEST(str.size == 3) ;
   test = "xx5yy" ;
   initfl_string(&str, (const uint8_t*)test, (const uint8_t*)test+4) ;
   TEST(str.addr == test) ;
   TEST(str.size == 5) ;

   // TEST initse_string
   test = "xyc" ;
   initse_string(&str, (const uint8_t*)test, (const uint8_t*)test+3) ;
   TEST(str.addr == test) ;
   TEST(str.size == 3) ;
   test = "xx5yy" ;
   initse_string(&str, (const uint8_t*)test, (const uint8_t*)test+5) ;
   TEST(str.addr == test) ;
   TEST(str.size == 5) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_string()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_init())  goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
