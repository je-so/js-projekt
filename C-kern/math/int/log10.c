/* title: Intop-Log10 impl
   Implements <Intop-Log10>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/math/int/log10.h
    Header file <Intop-Log10>.

   file: C-kern/math/int/log10.c
    Implementation file <Intop-Log10 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/log10.h"
#include "C-kern/api/math/int/log2.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

#define B/*illion*/       1000000000ull

#define Q/*uintillian*/   (B*B)

static uint64_t s_pow10[20] = {
   0,   10,   100,   1000,   10000,   100000,   1000000,   10000000,   100000000,
   B, B*10, B*100, B*1000, B*10000, B*100000, B*1000000, B*10000000, B*100000000,
   Q, Q*10
} ;

#undef B


unsigned log10_int(uint32_t i)
{
   unsigned lg10 = log2_int(i) ;

   // 0 <= lg10 <= 31
   lg10 /= 3 ;
   // 0 <= lg10 <= 10

   static_assert(nrelementsof(s_pow10) >= 10, "array index in bounds") ;

   if (s_pow10[lg10] > i) {
      -- lg10 ;
   }

   return lg10 ;
}

unsigned log10_int64(uint64_t i)
{
   unsigned lg10 = log2_int(i) ;

   // 0 <= lg10 <= 63
   lg10 /= 3 ;
   // 0 <= lg10 <= 21
   lg10 -= (lg10 >= 10) ;
   // 0 <= lg10 <= 20
   lg10 -= (lg10 >= 20) ;
   // 0 <= lg10 <= 19

   static_assert(nrelementsof(s_pow10) >= 19, "array index in bounds") ;

   if (s_pow10[lg10] > i) {
      -- lg10 ;
   }

   return lg10 ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_tablepow10(void)
{
   TEST(20 == nrelementsof(s_pow10)) ;

   // lg10 == 0 => never decrement it
   TEST(0/*not 1*/ == s_pow10[0]) ;

   for(uint64_t i = 1, pow10 = 10; i < 20; ++i, pow10 *= 10) {
      TEST(pow10 == s_pow10[i]) ;
   }

   // TEST last element is last (overflow)
   uint64_t last = 10 * s_pow10[19] ;
   TEST((last/10) < s_pow10[19]) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_log10(void)
{
   // TEST value 0 .. 9
   for(unsigned i= 0; i < 10; ++i) {
      TEST(0 == log10_int(i)) ;
      TEST(0 == log10_int64(i)) ;
   }

   // TEST UINT_MAX value
   TEST(9 == log10_int(UINT32_MAX)) ;
   TEST(19 == log10_int64(UINT64_MAX)) ;

   // TEST 32 bit values
   for(unsigned lg10 = 1, i = 10, iprev = 1; (i/10) == iprev; iprev = i, i *= 10, ++lg10) {
      TEST(lg10 == log10_int(i)) ;
      TEST(lg10 == log10_int(i+1)) ;
      TEST(lg10 == 1+log10_int(i-1)) ;
   }

   // TEST 64 bit values
   for(uint64_t lg10 = 1, i = 10, iprev = 1; (i/10) == iprev; iprev = i, i *= 10, ++lg10) {
      TEST(lg10 == log10_int64(i)) ;
      TEST(lg10 == log10_int64(i+1)) ;
      TEST(lg10 == 1+log10_int64(i-1)) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_math_int_log10()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_tablepow10())  goto ABBRUCH ;
   if (test_log10())       goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif


