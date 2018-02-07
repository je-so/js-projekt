/* title: Intop-Power2 impl
   Implement <unittest_generic_bits_power2>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/math/int/power2.h
    Header file of <Intop-Power2>.

   file: C-kern/math/int/power2.c
    Implementation file <Intop-Power2 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/power2.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int test_align(void)
{
   // TEST alignpower2_int: value 0
   for (int i=-1; i<2; ++i) {
      TEST(0 == alignpower2_int((uint8_t)0, i));
      TEST(0 == alignpower2_int((uint16_t)0, i));
      TEST(0 == alignpower2_int((uint32_t)0, i));
      TEST(0 == alignpower2_int((uint64_t)0, i));
   }

   // TEST alignpower2_int: value 1
   for (unsigned i=1; i<5; ++i) {
      TEST(i == alignpower2_int((uint8_t)i, 1));
      TEST(i == alignpower2_int((uint16_t)i, 1));
      TEST(i == alignpower2_int((uint32_t)i, 1));
      TEST(i == alignpower2_int((uint64_t)i, 1));
   }

   // TEST alignpower2_int: uint8_t && (aligned size > UINT8_MAX)
   TEST(0 == alignpower2_int((uint8_t)255, 128));
   TEST(0 == alignpower2_int((uint8_t)255, 256));

   // TEST alignpower2_int: uint16_t && (aligned size > UINT16_MAX)
   TEST(0 == alignpower2_int((uint16_t)65535, 32768));
   TEST(0 == alignpower2_int((uint16_t)65535, 65536));

   // TEST alignpower2_int: uint32_t && (aligned size > UINT32_MAX)
   TEST(0 == alignpower2_int((uint32_t)-1, 0x80000000));
   TEST(0 == alignpower2_int((uint32_t)-1, (uint64_t)0x100000000));

   // TEST alignpower2_int: uint64_t && (aligned size > UINT64_MAX)
   TEST(0 == alignpower2_int((uint64_t)-1, (uint64_t)0x8000000000000000));

   // TEST alignpower2_int: uint8_t
   for(uint8_t i=2; i; i = (uint8_t) (i << 1)) {
      TEST(i == alignpower2_int(i, 1));
      TEST(i == alignpower2_int((uint8_t)(i-1), i));
      if (i < 128) {
         TEST(2*i == alignpower2_int((uint8_t)(i+1), i));
      }
   }

   // TEST alignpower2_int: uint16_t
   for(uint16_t i=2; i; i = (uint16_t) (i << 1)) {
      TEST(i == alignpower2_int(i, 1));
      TEST(i == alignpower2_int((uint16_t)(i-1), i));
      if (i < 32768) {
         TEST(2*i == alignpower2_int((uint16_t)(i+1), i));
      }
   }

   // TEST alignpower2_int: uint32_t
   for(uint32_t i=2; i; i = (uint32_t) (i << 1)) {
      TEST(i == alignpower2_int(i, 1));
      TEST(i == alignpower2_int((uint32_t)(i-1), i));
      if (i < 0x80000000) {
         TEST(2*i == alignpower2_int((uint32_t)(i+1), i));
      }
   }

   // TEST alignpower2_int: uint64_t
   for(uint64_t i = 2; i; i<<=1) {
      TEST(i == alignpower2_int(i, 1));
      TEST(i == alignpower2_int((uint64_t)(i-1), i));
      if (i < 0x8000000000000000) {
         TEST(2*i == alignpower2_int((uint64_t)(i+1), i));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_powerof2(void)
{
   // TEST value 0
   TEST(1 == ispowerof2_int(0)) ;
   TEST(0 == makepowerof2_int((unsigned)0)) ;

   // TEST uint8_t
   for(uint8_t i = 1; i; i = (uint8_t) (i << 1)) {
      uint8_t i2 = i ;
      TEST(ispowerof2_int(i)) ;
      i2 = makepowerof2_int(i2) ;
      TEST(i2 == i) ;
      i2 = (uint8_t) (i + 1) ;
      i2 = makepowerof2_int(i2) ;
      TEST(i2 == 2*i || (i2 == i+1 && ((int8_t)i) <= 0) ) ;
   }

   // TEST uint16_t
   for(uint16_t i = 1; i; i = (uint16_t) (i << 1)) {
      uint16_t i2 = i ;
      TEST(ispowerof2_int(i2)) ;
      i2 = makepowerof2_int(i2) ;
      TEST(i2 == i) ;
      if (i > 1) {
         i2 = (uint16_t) (i + 1) ;
         TEST(!ispowerof2_int(i2)) ;
         i2 = makepowerof2_int(i2) ;
         if ( ((int16_t)i) < 0 ) {
            TEST(i2 == (i+1)) ;
         } else {
            TEST(i2 == 2*i) ;
         }
      }
   }

   // TEST uint32_t
   for(uint32_t i = 2; i; i = (uint32_t) (i << 1)) {
      uint32_t i2 = 0x55555555 & (i | (i-1)) ;
      i2 = (uint32_t)(i | i2) ;
      TEST(!ispowerof2_int(i2)) ;
      i2 = makepowerof2_int(i2) ;
      // makepowerof2_int(0x8xxxxxxx) unchanged
      if ((int)i < 0) {
         TEST(i2 == (unsigned)(i|0x55555555)) ;
      } else {
         TEST(i2 == 2*(unsigned)i) ;
      }
   }

   // TEST uint32_t
   for(uint32_t i = 2; i; i<<=1) {
      uint32_t i2 = i + 1 ;
      TEST(!ispowerof2_int(i2)) ;
      i2 = makepowerof2_int(i2) ;
      // makepowerof2_int(0x8xxxxxxx) unchanged
      if ((int32_t)i < 0) {
         TEST(i2 == (i+1)) ;
      } else {
         TEST(i2 == 2*i) ;
      }
   }

   // TEST uint64_t
   for(uint64_t i = 4; i; i<<=1) {
      uint64_t i2 = i - 1 ;
      TEST(!ispowerof2_int(i2)) ;
      i2 = makepowerof2_int(i2) ;
      TEST(i2 == i) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_math_int_power2()
{
   if (test_align())       goto ONERR;
   if (test_powerof2())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
