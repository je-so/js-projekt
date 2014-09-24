/* title: Intop-Sign impl
   Implements <Intop-Sign> (only <unittest_math_int_sign>).

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
ONERR:
   return EINVAL ;
}

int unittest_math_int_sign()
{
   if (test_sign())     goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
