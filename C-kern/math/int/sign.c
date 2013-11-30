/* title: Intop-Sign impl
   Implements <Intop-Sign> (only <unittest_math_int_sign>).

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

   file: C-kern/api/math/int/sign.h
    Header file of <Intop-Sign>.

   file: C-kern/math/int/sign.c
    Implementation file <Intop-Sign impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/sign.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: int_t


// group: test

#ifdef KONFIG_UNITTEST

static int test_sign(void)
{
   for(int i = -128; i <= +127 ; ++i) {
      int8_t in = (int8_t) i ;
      int     s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == sign_int(in)) ;
   }

   for(int32_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      int16_t in = (int16_t) i ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == sign_int(in)) ;
   }

   for(int32_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      int32_t in = (i==0) ? 0 : ((i < 0) ? (INT32_MIN - INT16_MIN + i) : (INT32_MAX - INT16_MAX + i)) ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == sign_int(in)) ;
   }

   for(int64_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      int64_t in = (i==0) ? 0 : ((i < 0) ? (INT64_MIN - INT16_MIN + i) : (INT64_MAX - INT16_MAX + i)) ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == sign_int(in)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_math_int_sign()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_sign())     goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
