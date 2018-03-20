/* title: Intop-Log10 impl
   Implements <Intop-Log10>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
};

#undef Q
#undef B

// group: compute

/* function: log10_int32
 * Uses <s_pow10> table to decrement result if necessary. */
unsigned log10_int32(uint32_t i)
{
   unsigned lg10 = log2_int(i);

   // 0 <= lg10 <= 31
   lg10 /= 3;
   // 0 <= lg10 <= 10

   static_assert(lengthof(s_pow10) > 10, "array index in bounds");

   if (s_pow10[lg10] > i) {
      -- lg10;
   }

   return lg10;
}

/* function: log10_int64
 * Uses <s_pow10> table to decrement result if necessary. */
unsigned log10_int64(uint64_t i)
{
   unsigned lg10 = log2_int(i);

   // 0 <= lg10 <= 63
   lg10 /= 3;
   // 0 <= lg10 <= 21
   lg10 -= (lg10 >= 10);
   // 0 <= lg10 <= 20
   lg10 -= (lg10 >= 20);
   // 0 <= lg10 <= 19

   static_assert(lengthof(s_pow10) >= 19, "array index in bounds");

   if (s_pow10[lg10] > i) {
      -- lg10;
   }

   return lg10;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_tablepow10(void)
{
   // TEST s_pow10[0] == 0 instead of 1
   // log10_int64(0) => lg10 = 0 => ! (s_pow10[0] > 0) => special value 0 is not decremented !
   TEST(0/*not 1*/ == s_pow10[0]);

   // TEST all
   for (uint64_t i = 1, power10 = 10; i < lengthof(s_pow10); ++i, power10 *= 10) {
      TEST(power10 == s_pow10[i]);
   }

   // TEST last element is last (overflow)
   TEST(20 == lengthof(s_pow10));
   uint64_t last = 10 * s_pow10[19];
   TEST((last/10) < s_pow10[19]);

   return 0;
ONERR:
   return EINVAL;
}

static int test_log10(void)
{
   // TEST values 0 .. 9
   for (unsigned i=0; i<10; ++i) {
      TEST(0 == log10_int(i));
      TEST(0 == log10_int32(i));
      TEST(0 == log10_int64(i));
   }

   // TEST UINT_MAX 32,64 bit values
   TEST(9 == log10_int(UINT32_MAX));
   TEST(9 == log10_int32(UINT32_MAX));
   TEST(19 == log10_int(UINT64_MAX));
   TEST(19 == log10_int64(UINT64_MAX));

   // TEST all 32 bit values multiples of 10 +/- 1
   for (unsigned lg10 = 1, i = 10, iprev = 1; (i/10) == iprev; iprev = i, i *= 10, ++lg10) {
      TEST(lg10 == log10_int(i));
      TEST(lg10 == log10_int(i+1));
      TEST(lg10 == 1+log10_int(i-1));
      TEST(lg10 == log10_int32(i));
      TEST(lg10 == log10_int32(i+1));
      TEST(lg10 == 1+log10_int32(i-1));
   }

   // TEST all 64 bit values multiples of 10 +/- 1
   for (uint64_t lg10 = 1, i = 10, iprev = 1; (i/10) == iprev; iprev = i, i *= 10, ++lg10) {
      TEST(lg10 == log10_int(i));
      TEST(lg10 == log10_int(i+1));
      TEST(lg10 == 1+log10_int(i-1));
      TEST(lg10 == log10_int64(i));
      TEST(lg10 == log10_int64(i+1));
      TEST(lg10 == 1+log10_int64(i-1));
   }

   // TEST uint8_t
   for (uint8_t lg10=0, i=1; i<=100; i=(uint8_t)(i<=100?i*10:255), ++lg10) {
      TEST(lg10 == log10_int(i));
   }

   // TEST uint16_t
   for (uint16_t lg10=0, i=1; i<=10000; i=(uint16_t)(i<=10000?i*10:65535), ++lg10) {
      TEST(lg10 == log10_int(i));
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_math_int_log10()
{
   if (test_tablepow10())  goto ONERR;
   if (test_log10())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
