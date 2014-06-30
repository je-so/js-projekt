/* title: Intop-Bitorder impl

   Implements <Intop-Bitorder>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/math/int/bitorder.h
    Header file <Intop-Bitorder>.

   file: C-kern/math/int/bitorder.c
    Implementation file <Intop-Bitorder impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/bitorder.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

static int test_reverse(void)
{
   // TEST reversebits_int: 0, MAX
   TEST(0 == reversebits_int((uint8_t)0)) ;
   TEST(0 == reversebits_int((uint16_t)0)) ;
   TEST(0 == reversebits_int((uint32_t)0)) ;
   TEST(0 == reversebits_int((uint64_t)0)) ;
   TEST(UINT8_MAX  == reversebits_int((uint8_t)-1)) ;
   TEST(UINT16_MAX == reversebits_int((uint16_t)-1)) ;
   TEST(UINT32_MAX == reversebits_int((uint32_t)-1)) ;
   TEST(UINT64_MAX == reversebits_int((uint64_t)-1)) ;

   // TEST reversebits_int: uint8_t
   for (unsigned i1 = 0; i1 <= 8; ++i1) {
      uint32_t  val1 = i1 ? (uint32_t)1 << (i1-1) : 0 ;
      uint32_t rval1 = i1 ? (uint32_t)0x80 >> (i1-1) : 0 ;
      for (unsigned i2 = 0; i2 <= 8; ++i2) {
         uint32_t  val2 = i2 ? (uint32_t)1 << (i2-1) : 0 ;
         uint32_t rval2 = i2 ? (uint32_t)0x80 >> (i2-1) : 0 ;
         for (unsigned i3 = 0; i3 <= 8; ++i3) {
            uint32_t  val3 = i3 ? (uint32_t)1 << (i3-1) : 0 ;
            uint32_t rval3 = i3 ? (uint32_t)0x80 >> (i3-1) : 0 ;
            for (unsigned i4 = 0; i4 <= 8; ++i4) {
               uint32_t  val4 = i4 ? (uint32_t)1 << (i4-1) : 0 ;
               uint32_t rval4 = i4 ? (uint32_t)0x80 >> (i4-1) : 0 ;
               uint32_t  val  = val1 | val2 | val3 | val4 ;
               uint32_t rval  = rval1 | rval2 | rval3 | rval4 ;
               uint8_t rev = reversebits_int((uint8_t)val) ;
               TEST(rval == rev) ;
            }
         }
      }
   }

   // TEST reversebits_int: uint16_t
   for (unsigned i1 = 0; i1 <= 16; ++i1) {
      uint32_t  val1 = i1 ? (uint32_t)1 << (i1-1) : 0 ;
      uint32_t rval1 = i1 ? (uint32_t)0x8000 >> (i1-1) : 0 ;
      for (unsigned i2 = 0; i2 <= 16; ++i2) {
         uint32_t  val2 = i2 ? (uint32_t)1 << (i2-1) : 0 ;
         uint32_t rval2 = i2 ? (uint32_t)0x8000 >> (i2-1) : 0 ;
         for (unsigned i3 = 0; i3 <= 16; ++i3) {
            uint32_t  val3 = i3 ? (uint32_t)1 << (i3-1) : 0 ;
            uint32_t rval3 = i3 ? (uint32_t)0x8000 >> (i3-1) : 0 ;
            for (unsigned i4 = 0; i4 <= 16; ++i4) {
               uint32_t  val4 = i4 ? (uint32_t)1 << (i4-1) : 0 ;
               uint32_t rval4 = i4 ? (uint32_t)0x8000 >> (i4-1) : 0 ;
               uint32_t  val  = val1 | val2 | val3 | val4 ;
               uint32_t rval  = rval1 | rval2 | rval3 | rval4 ;
               uint16_t rev = reversebits_int((uint16_t)val) ;
               TEST(rval == rev) ;
            }
         }
      }
   }

   // TEST reversebits_int: uint32_t
   for (unsigned i1 = 0; i1 <= 32; ++i1) {
      uint32_t  val1 = i1 ? (uint32_t)1 << (i1-1) : 0 ;
      uint32_t rval1 = i1 ? (uint32_t)0x80000000 >> (i1-1) : 0 ;
      for (unsigned i2 = 0; i2 <= 32; ++i2) {
         uint32_t  val2 = i2 ? (uint32_t)1 << (i2-1) : 0 ;
         uint32_t rval2 = i2 ? (uint32_t)0x80000000 >> (i2-1) : 0 ;
         for (unsigned i3 = 0; i3 <= 32; ++i3) {
            uint32_t  val3 = i3 ? (uint32_t)1 << (i3-1) : 0 ;
            uint32_t rval3 = i3 ? (uint32_t)0x80000000 >> (i3-1) : 0 ;
            uint32_t  val  = val1 | val2 | val3 ;
            uint32_t rval  = rval1 | rval2 | rval3 ;
            uint32_t rev = reversebits_int((uint32_t)val) ;
            TEST(rval == rev) ;
         }
      }
   }

   // TEST reversebits_int: uint64_t
   for (unsigned i1 = 0; i1 <= 64; ++i1) {
      uint64_t  val1 = i1 ? (uint64_t)1 << (i1-1) : 0 ;
      uint64_t rval1 = i1 ? (uint64_t)0x8000000000000000 >> (i1-1) : 0 ;
      for (unsigned i2 = 0; i2 <= 64; ++i2) {
         uint64_t  val2 = i2 ? (uint64_t)1 << (i2-1) : 0 ;
         uint64_t rval2 = i2 ? (uint64_t)0x8000000000000000 >> (i2-1) : 0 ;
         for (unsigned i3 = 0; i3 <= 64; ++i3) {
            uint64_t  val3 = i3 ? (uint64_t)1 << (i3-1) : 0 ;
            uint64_t rval3 = i3 ? (uint64_t)0x8000000000000000 >> (i3-1) : 0 ;
            uint64_t  val  = val1 | val2 | val3 ;
            uint64_t rval  = rval1 | rval2 | rval3 ;
            uint64_t rev = reversebits_int((uint64_t)val) ;
            TEST(rval == rev) ;
         }
      }
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_math_int_bitorder()
{
   if (test_reverse())     goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
