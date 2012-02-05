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
#include "C-kern/api/math/int/abs.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_abs(void)
{

   // TEST value 0
   TEST(0 == abs_int(0)) ;
   TEST(0 == abs_int((int8_t)0)) ;
   TEST(0 == abs_int((int16_t)0)) ;
   TEST(0 == abs_int((int32_t)0)) ;
   TEST(0 == abs_int64((int64_t)0)) ;

   // TEST int8_t
   for(int8_t i = INT8_MIN; i; ++i) {
      int r = -i ;
      TEST((unsigned)r == abs_int(i)) ;
   }

   // TEST int16_t
   for(int16_t i = INT16_MIN; i; ++i) {
      int r = -i ;
      TEST((unsigned)r == abs_int(i)) ;
   }

   // TEST int32_t
   for(int32_t i = INT32_MIN; i < 0; i += 0x1001) {
      int r = -i ;
      TEST((unsigned)r == abs_int(i)) ;
   }

   // TEST int64_t
   for(int64_t i = INT64_MIN; i < 0; i += 0x100100010001) {
      int64_t r = -i ;
      TEST((uint64_t)r == abs_int64(i)) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_math_int_abs()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_abs())    goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif

