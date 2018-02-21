/* title: Intop-Abs impl
   Implements <Intop-Abs>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int test_abs(void)
{
   // TEST abs_int: 0
   TEST(0 == abs_int32(0));
   TEST(0 == abs_int32((int8_t)0));
   TEST(0 == abs_int32((int16_t)0));
   TEST(0 == abs_int32((int32_t)0));
   TEST(0 == abs_int64((int8_t)0));
   TEST(0 == abs_int64((int16_t)0));
   TEST(0 == abs_int64((int32_t)0));
   TEST(0 == abs_int64((int64_t)0));
   TEST(0 == abs_int((int8_t)0));
   TEST(0 == abs_int((int16_t)0));
   TEST(0 == abs_int((int32_t)0));
   TEST(0 == abs_int((int64_t)0));

   // TEST abs_int: INT_MAX
   TEST((uint32_t)INT8_MAX  == abs_int32((int8_t)INT8_MAX));
   TEST((uint32_t)INT16_MAX == abs_int32((int16_t)INT16_MAX));
   TEST((uint32_t)INT32_MAX == abs_int32((int32_t)INT32_MAX));
   TEST((uint64_t)INT8_MAX  == abs_int64((int8_t)INT8_MAX));
   TEST((uint64_t)INT16_MAX == abs_int64((int16_t)INT16_MAX));
   TEST((uint64_t)INT32_MAX == abs_int64((int32_t)INT32_MAX));
   TEST((uint64_t)INT64_MAX == abs_int64((int64_t)INT64_MAX));
   TEST((uint64_t)INT8_MAX  == abs_int((int8_t)INT8_MAX));
   TEST((uint64_t)INT16_MAX == abs_int((int16_t)INT16_MAX));
   TEST((uint64_t)INT32_MAX == abs_int((int32_t)INT32_MAX));
   TEST((uint64_t)INT64_MAX == abs_int((int64_t)INT64_MAX));

   // TEST abs_int: INT_MIN
   TEST(-(uint32_t)INT8_MIN  == abs_int32((int8_t)INT8_MIN));
   TEST(-(uint32_t)INT16_MIN == abs_int32((int16_t)INT16_MIN));
   TEST(-(uint32_t)INT32_MIN == abs_int32((int32_t)INT32_MIN));
   TEST(-(uint64_t)INT8_MIN  == abs_int64((int8_t)INT8_MIN));
   TEST(-(uint64_t)INT16_MIN == abs_int64((int16_t)INT16_MIN));
   TEST(-(uint64_t)INT32_MIN == abs_int64((int32_t)INT32_MIN));
   TEST(-(uint64_t)INT64_MIN == abs_int64((int64_t)INT64_MIN));
   TEST(-(uint64_t)INT8_MIN  == abs_int((int8_t)INT8_MIN));
   TEST(-(uint64_t)INT16_MIN == abs_int((int16_t)INT16_MIN));
   TEST(-(uint64_t)INT32_MIN == abs_int((int32_t)INT32_MIN));
   TEST(-(uint64_t)INT64_MIN == abs_int((int64_t)INT64_MIN));

   //  TEST abs_int: int8_t
   for (uint8_t i = 0; i < INT8_MAX; ++i) {
      TEST(i == abs_int32((int8_t)i));
      TEST(i == abs_int64((int8_t)i));
      TEST(i == abs_int32((int8_t)-(int8_t)i));
      TEST(i == abs_int64((int8_t)-(int8_t)i));
      TEST(i == abs_int((int8_t)i));
      TEST(i == abs_int((int8_t)-(int8_t)i));
   }

   // TEST abs_int: int16_t
   for (uint16_t i = 0; i < INT16_MAX; ++i) {
      TEST(i == abs_int32((int16_t)i));
      TEST(i == abs_int64((int16_t)i));
      TEST(i == abs_int32((int16_t)-(int16_t)i));
      TEST(i == abs_int64((int16_t)-(int16_t)i));
      TEST(i == abs_int((int16_t)i));
      TEST(i == abs_int((int16_t)-(int16_t)i));
   }

   // TEST abs_int: int32_t
   for (uint32_t i = 0; i < INT32_MAX; i += 0x1001) {
      TEST(i == abs_int32((int32_t)i));
      TEST(i == abs_int64((int32_t)i));
      TEST(i == abs_int32((int32_t)-(int32_t)i));
      TEST(i == abs_int64((int32_t)-(int32_t)i));
      TEST(i == abs_int((int32_t)i));
      TEST(i == abs_int((int32_t)-(int32_t)i));
   }

   // TEST abs_int: int64_t
   for (uint64_t i = 0; i < INT64_MAX; i += 0x1001000101001) {
      TEST(i == abs_int64((int64_t)i));
      TEST(i == abs_int64(-(int64_t)i));
      TEST(i == abs_int((int64_t)i));
      TEST(i == abs_int(-(int64_t)i));
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_math_int_abs()
{
   if (test_abs())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
