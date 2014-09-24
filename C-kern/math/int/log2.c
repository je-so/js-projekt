/* title: Intop-Log2 impl
   Implements <Intop-Log2>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/math/int/log2.h
    Header file <Intop-Log2>.

   file: C-kern/math/int/log2.c
    Implementation file <Intop-Log2 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int test_log2(void)
{
   unsigned l ;

   // TEST value 0
   TEST(0 == log2_int((unsigned)0)) ;

   // TEST uint8_t
   l = 0 ;
   for(uint8_t i = 1; i ; ++i) {
      if (i > 1 && ispowerof2_int(i)) {
         ++ l ;
      }
      TEST(l == log2_int(i)) ;
   }

   // TEST uint16_t
   l = 0 ;
   for(uint16_t i = 1; i ; ++i) {
      if (i > 1 && ispowerof2_int(i)) {
         ++ l ;
      }
      TEST(l == log2_int(i)) ;
   }

   // TEST uint32_t
   l = 0 ;
   for(uint32_t i = 1; i ; i <<= 1, ++l) {
      TEST(l == log2_int(i)) ;
      TEST(l == log2_int(i | (i >> 1))) ;
      TEST(l == log2_int(i | (i >> 1) | (i >> 2))) ;
      TEST(l == log2_int(i | (i -1))) ;
      TEST(l == log2_int(i | 1)) ;
   }

   // TEST uint64_t
   l = 0 ;
   for(uint64_t i = 1; i ; i <<= 1, ++l) {
      TEST(l == log2_int(i)) ;
      TEST(l == log2_int(i | (i >> 1))) ;
      TEST(l == log2_int(i | (i >> 1) | (i >> 2))) ;
      TEST(l == log2_int(i | (i -1))) ;
      TEST(l == log2_int(i | 1)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_math_int_log2()
{
   if (test_log2())    goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
