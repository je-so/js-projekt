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
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log10.h"
#include "C-kern/api/math/int/log2.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: int_t

// group: local variables

#define B/*illion*/       1000000000ull
#define Q/*uintillian*/   (B*B)

/* variable: s_pow10
 * Contains powers of 10 (10ⁿ).
 * > s_pow10[n] == 10ⁿ
 * > s_pow10[n] == pow(10,n)
 * Except for (s_pow10[0]==0) instead of (s_pow10[0]==1).
 * The reason is that <log10_int>(0) is undefined in a mathematical sense
 * but the implementation is defined to return 0. */
static uint64_t s_pow10[20] = {
   0,   10,   100,   1000,   10000,   100000,   1000000,   10000000,   100000000,
   B, B*10, B*100, B*1000, B*10000, B*100000, B*1000000, B*10000000, B*100000000,
   Q, Q*10
} ;

#undef Q
#undef B

// group: compute

/* function: log10_int32
 * Uses <s_pow10> table to decrement result if necessary. */
unsigned log10_int32(uint32_t i)
{
   unsigned lg10 = log2_int(i) ;

   // 0 <= lg10 <= 31
   lg10 /= 3 ;
   // 0 <= lg10 <= 10

   static_assert(lengthof(s_pow10) > 10, "array index in bounds") ;

   if (s_pow10[lg10] > i) {
      -- lg10 ;
   }

   return lg10 ;
}

/* function: log10_int64
 * Uses <s_pow10> table to decrement result if necessary. */
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

   static_assert(lengthof(s_pow10) >= 19, "array index in bounds") ;

   if (s_pow10[lg10] > i) {
      -- lg10 ;
   }

   return lg10 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_tablepow10(void)
{
   // TEST s_pow10[0] == 0 instead of 1
   // log10_int64(0) => lg10 = 0 => ! (s_pow10[0] > 0) => special value 0 is not decremented !
   TEST(0/*not 1*/ == s_pow10[0]) ;

   // TEST all
   for (uint64_t i = 1, power10 = 10; i < lengthof(s_pow10); ++i, power10 *= 10) {
      TEST(power10 == s_pow10[i]) ;
   }

   // TEST last element is last (overflow)
   TEST(20 == lengthof(s_pow10)) ;
   uint64_t last = 10 * s_pow10[19] ;
   TEST((last/10) < s_pow10[19]) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_log10(void)
{
   // TEST values 0 .. 9
   for (unsigned i= 0; i < 10; ++i) {
      TEST(0 == log10_int(i)) ;
      TEST(0 == log10_int32(i)) ;
      TEST(0 == log10_int64(i)) ;
   }

   // TEST UINT_MAX 32,64 bit values
   TEST(9 == log10_int(UINT32_MAX)) ;
   TEST(9 == log10_int32(UINT32_MAX)) ;
   TEST(19 == log10_int(UINT64_MAX)) ;
   TEST(19 == log10_int64(UINT64_MAX)) ;

   // TEST all 32 bit values multiples of 10 +/- 1
   for (unsigned lg10 = 1, i = 10, iprev = 1; (i/10) == iprev; iprev = i, i *= 10, ++lg10) {
      TEST(lg10 == log10_int(i)) ;
      TEST(lg10 == log10_int(i+1)) ;
      TEST(lg10 == 1+log10_int(i-1)) ;
      TEST(lg10 == log10_int32(i)) ;
      TEST(lg10 == log10_int32(i+1)) ;
      TEST(lg10 == 1+log10_int32(i-1)) ;
   }

   // TEST all 64 bit values multiples of 10 +/- 1
   for (uint64_t lg10 = 1, i = 10, iprev = 1; (i/10) == iprev; iprev = i, i *= 10, ++lg10) {
      TEST(lg10 == log10_int(i)) ;
      TEST(lg10 == log10_int(i+1)) ;
      TEST(lg10 == 1+log10_int(i-1)) ;
      TEST(lg10 == log10_int64(i)) ;
      TEST(lg10 == log10_int64(i+1)) ;
      TEST(lg10 == 1+log10_int64(i-1)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_math_int_log10()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_tablepow10())  goto ONABORT ;
   if (test_log10())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
