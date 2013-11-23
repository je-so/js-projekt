/* title: Intop-Abs impl
   Implements <Intop-Abs>.

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

   file: C-kern/api/math/int/abs.h
    Header file <Intop-Abs>.

   file: C-kern/math/int/abs.c
    Implementation file <Intop-Abs impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/abs.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int test_abs(void)
{
   // TEST value 0
   TEST(0 == abs_int32(0)) ;
   TEST(0 == abs_int32((int8_t)0)) ;
   TEST(0 == abs_int32((int16_t)0)) ;
   TEST(0 == abs_int32((int32_t)0)) ;
   TEST(0 == abs_int64((int8_t)0)) ;
   TEST(0 == abs_int64((int16_t)0)) ;
   TEST(0 == abs_int64((int32_t)0)) ;
   TEST(0 == abs_int64((int64_t)0)) ;

   // TEST abs_int32, abs_int64 for int8_t
   for(int8_t i = INT8_MIN; i; ++i) {
      uint32_t r = (uint32_t) (- (int32_t)i) ;
      TEST(r == abs_int32(i)) ;
      TEST((uint64_t)r == abs_int64(i)) ;
   }

   // TEST abs_int32, abs_int64 for int16_t
   for(int16_t i = INT16_MIN; i; ++i) {
      uint32_t r = (uint32_t) (- (int32_t)i) ;
      TEST(r == abs_int32(i)) ;
      TEST((uint64_t)r == abs_int64(i)) ;
   }

   // TEST abs_int32, abs_int64 for int32_t
   for(int32_t i = INT32_MIN; i < 0; i += 0x1001) {
      uint32_t r = (uint32_t)-i ;
      TEST(r == abs_int32(i)) ;
      TEST((uint64_t)r == abs_int64(i)) ;
   }

   // TEST abs_int64 for int64_t
   for(int64_t i = INT64_MIN; i < 0; i += 0x100100010001) {
      int64_t r = -i ;
      TEST((uint64_t)r == abs_int64(i)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_math_int_abs()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_abs())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
