/* title: Intop-Byteorder impl

   Implements <Intop-Byteorder>.

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

   file: C-kern/api/math/int/byteorder.h
    Header file <Intop-Byteorder>.

   file: C-kern/math/int/byteorder.c
    Implementation file <Intop-Byteorder impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/byteorder.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: int_t

// group: lifetime


// group: test

#ifdef KONFIG_UNITTEST

static int test_byteoperations(void)
{
   union {
      uint8_t  bytes[8] ;
      uint16_t u16 ;
      uint32_t u32 ;
      uint64_t u64 ;
   }  val ;

   // TEST htobe_int: 16 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      uint16_t u16 = (uint16_t) (b * 256 + (b+1) % 256) ;
      val.u16 = htobe_int(u16) ;
      TEST(val.bytes[0] == b) ;
      TEST(val.bytes[1] == (b+1)%256) ;
   }

   // TEST htobe_int: 32 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      uint32_t u32 = (uint32_t) (b * 256*256*256 + ((b+1) % 256)*256*256 + ((b+2) % 256)*256 + ((b+3) % 256)) ;
      val.u32 = htobe_int(u32) ;
      TEST(val.bytes[0] == b) ;
      TEST(val.bytes[1] == (b+1)%256) ;
      TEST(val.bytes[2] == (b+2)%256) ;
      TEST(val.bytes[3] == (b+3)%256) ;
   }

   // TEST htobe_int: 64 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      uint64_t u64 = (uint64_t) (b * 256*256*256*256*256*256*256 + ((b+1) % 256)*256*256*256*256*256*256
                                 + ((b+2) % 256)*256*256*256*256*256 + ((b+3) % 256)*256*256*256*256
                                 + ((b+4) % 256)*256*256*256 + ((b+5) % 256)*256*256
                                 + ((b+6) % 256)*256 + ((b+7) % 256)) ;
      val.u64 = htobe_int(u64) ;
      TEST(val.bytes[0] == b) ;
      TEST(val.bytes[1] == (b+1)%256) ;
      TEST(val.bytes[2] == (b+2)%256) ;
      TEST(val.bytes[3] == (b+3)%256) ;
      TEST(val.bytes[4] == (b+4)%256) ;
      TEST(val.bytes[5] == (b+5)%256) ;
      TEST(val.bytes[6] == (b+6)%256) ;
      TEST(val.bytes[7] == (b+7)%256) ;
   }

   // TEST htole_int: 16 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      uint16_t u16 = (uint16_t) (b * 256 + (b+1) % 256) ;
      val.u16 = htole_int(u16) ;
      TEST(val.bytes[1] == b) ;
      TEST(val.bytes[0] == (b+1)%256) ;
   }

   // TEST htole_int: 32 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      uint32_t u32 = (uint32_t) (b * 256*256*256 + ((b+1) % 256)*256*256 + ((b+2) % 256)*256 + ((b+3) % 256)) ;
      val.u32 = htole_int(u32) ;
      TEST(val.bytes[3] == b) ;
      TEST(val.bytes[2] == (b+1)%256) ;
      TEST(val.bytes[1] == (b+2)%256) ;
      TEST(val.bytes[0] == (b+3)%256) ;
   }

   // TEST htole_int: 64 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      uint64_t u64 = (uint64_t) (b * 256*256*256*256*256*256*256 + ((b+1) % 256)*256*256*256*256*256*256
                                 + ((b+2) % 256)*256*256*256*256*256 + ((b+3) % 256)*256*256*256*256
                                 + ((b+4) % 256)*256*256*256 + ((b+5) % 256)*256*256
                                 + ((b+6) % 256)*256 + ((b+7) % 256)) ;
      val.u64 = htole_int(u64) ;
      TEST(val.bytes[7] == b) ;
      TEST(val.bytes[6] == (b+1)%256) ;
      TEST(val.bytes[5] == (b+2)%256) ;
      TEST(val.bytes[4] == (b+3)%256) ;
      TEST(val.bytes[3] == (b+4)%256) ;
      TEST(val.bytes[2] == (b+5)%256) ;
      TEST(val.bytes[1] == (b+6)%256) ;
      TEST(val.bytes[0] == (b+7)%256) ;
   }

   // TEST betoh_int: 16 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      val.bytes[0] = (uint8_t) b ;
      val.bytes[1] = (uint8_t) (b+1) ;
      uint16_t u16 = betoh_int(val.u16) ;
      TEST(u16 == (uint16_t) (b * 256 + (b+1) % 256)) ;
   }

   // TEST betoh_int: 32 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      val.bytes[0] = (uint8_t) b ;
      val.bytes[1] = (uint8_t) (b+1) ;
      val.bytes[2] = (uint8_t) (b+2) ;
      val.bytes[3] = (uint8_t) (b+3) ;
      uint32_t u32 = betoh_int(val.u32) ;
      TEST(u32 == (uint32_t) (b * 256*256*256 + ((b+1) % 256)*256*256 + ((b+2) % 256)*256 + ((b+3) % 256))) ;
   }

   // TEST betoh_int: 64 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      val.bytes[0] = (uint8_t) b ;
      val.bytes[1] = (uint8_t) (b+1) ;
      val.bytes[2] = (uint8_t) (b+2) ;
      val.bytes[3] = (uint8_t) (b+3) ;
      val.bytes[4] = (uint8_t) (b+4) ;
      val.bytes[5] = (uint8_t) (b+5) ;
      val.bytes[6] = (uint8_t) (b+6) ;
      val.bytes[7] = (uint8_t) (b+7) ;
      uint64_t u64 = betoh_int(val.u64) ;
      TEST(u64 == (uint64_t) (b * 256*256*256*256*256*256*256 + ((b+1) % 256)*256*256*256*256*256*256
                             + ((b+2) % 256)*256*256*256*256*256 + ((b+3) % 256)*256*256*256*256
                             + ((b+4) % 256)*256*256*256 + ((b+5) % 256)*256*256
                             + ((b+6) % 256)*256 + ((b+7) % 256))) ;
   }

   // TEST letoh_int: 16 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      val.bytes[1] = (uint8_t) b ;
      val.bytes[0] = (uint8_t) (b+1) ;
      uint16_t u16 = letoh_int(val.u16) ;
      TEST(u16 == (uint16_t) (b * 256 + (b+1) % 256)) ;
   }

   // TEST letoh_int: 32 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      val.bytes[3] = (uint8_t) b ;
      val.bytes[2] = (uint8_t) (b+1) ;
      val.bytes[1] = (uint8_t) (b+2) ;
      val.bytes[0] = (uint8_t) (b+3) ;
      uint32_t u32 = letoh_int(val.u32) ;
      TEST(u32 == (uint32_t) (b * 256*256*256 + ((b+1) % 256)*256*256 + ((b+2) % 256)*256 + ((b+3) % 256))) ;
   }

   // TEST letoh_int: 64 bit
   for (uint64_t b = 0; b <= 255; ++b) {
      val.bytes[7] = (uint8_t) b ;
      val.bytes[6] = (uint8_t) (b+1) ;
      val.bytes[5] = (uint8_t) (b+2) ;
      val.bytes[4] = (uint8_t) (b+3) ;
      val.bytes[3] = (uint8_t) (b+4) ;
      val.bytes[2] = (uint8_t) (b+5) ;
      val.bytes[1] = (uint8_t) (b+6) ;
      val.bytes[0] = (uint8_t) (b+7) ;
      uint64_t u64 = letoh_int(val.u64) ;
      TEST(u64 == (uint64_t) (b * 256*256*256*256*256*256*256 + ((b+1) % 256)*256*256*256*256*256*256
                             + ((b+2) % 256)*256*256*256*256*256 + ((b+3) % 256)*256*256*256*256
                             + ((b+4) % 256)*256*256*256 + ((b+5) % 256)*256*256
                             + ((b+6) % 256)*256 + ((b+7) % 256))) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_math_int_byteorder()
{
   if (test_byteoperations())    goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
