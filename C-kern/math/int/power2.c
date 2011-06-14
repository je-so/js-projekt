/* title: Intop-Power2 impl
   Implement <unittest_generic_bits_power2>.

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

   file: C-kern/api/math/int/power2.h
    Header file of <Intop-Power2>.

   file: C-kern/math/int/power2.c
    Implementation file <Intop-Power2 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/power2.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_math_int_power2,ABBRUCH)

int unittest_math_int_power2()
{

   for(uint8_t i = 1; i; i = (uint8_t) (i << 1)) {
      uint8_t i2 = i ;
      TEST(ispowerof2(i)) ;
      i2 = makepowerof2(i2) ;
      TEST(i2 == i) ;
      i2 = (uint8_t) (i + 1) ;
      i2 = makepowerof2(i2) ;
      TEST(i2 == 2*i || (i2 == i+1 && ((int8_t)i) <= 0) ) ;
   }

   for(uint16_t i = 1; i; i = (uint16_t) (i << 1)) {
      uint16_t i2 = i ;
      TEST(ispowerof2(i2)) ;
      i2 = makepowerof2(i2) ;
      TEST(i2 == i) ;
      if (i > 1) {
         i2 = (uint16_t) (i + 1) ;
         TEST(!ispowerof2(i2)) ;
         i2 = makepowerof2(i2) ;
         if ( ((int16_t)i) < 0 ) {
            TEST(i2 == (i+1)) ;
         } else {
            TEST(i2 == 2*i) ;
         }
      }
   }

   for(uint32_t i = 2; i; i = (uint32_t) (i << 1)) {
      uint32_t i2 = 0x55555555 & (i | (i-1)) ;
      i2 = (uint32_t)(i | i2) ;
      TEST(!ispowerof2(i2)) ;
      i2 = makepowerof2(i2) ;
      // makepowerof2(0x8xxxxxxx) unchanged
      if ((int)i < 0) {
         TEST(i2 == (unsigned)(i|0x55555555)) ;
      } else {
         TEST(i2 == 2*(unsigned)i) ;
      }
   }

   for(uint32_t i = 2; i; i<<=1) {
      uint32_t i2 = i + 1 ;
      TEST(!ispowerof2(i2)) ;
      i2 = makepowerof2(i2) ;
      // makepowerof2(0x8xxxxxxx) unchanged
      if ((int32_t)i < 0) {
         TEST(i2 == (i+1)) ;
      } else {
         TEST(i2 == 2*i) ;
      }
   }

   for(uint64_t i = 4; i; i<<=1) {
      uint64_t i2 = i - 1 ;
      TEST(!ispowerof2(i2)) ;
      i2 = makepowerof2(i2) ;
      TEST(i2 == i) ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif
