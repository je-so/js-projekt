/*
   C-System-Layer: C-kern/generic/integer.c
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/generic/integer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_integer_misc,ABBRUCH)

static int test_powerof2(void)
{
   int err = 1 ;

   for(unsigned i = 1; i; i<<=1) {
      unsigned i2 = i ;
      TEST(ispowerof2(i)) ;
      i2 = makepowerof2(i2) ;
      TEST(i2 == i) ;
   }

   for(int8_t i = 1; i; i = (int8_t) (i << 1)) {
      int8_t i2 = i ;
      TEST(ispowerof2(i)) ;
      i2 = makepowerof2(i2) ;
      TEST(i2 == i) ;
      i2 = (int8_t) (i2 + 1) ;
      i2 = makepowerof2(i2) ;
      TEST(i2 == 2*i || (i2 == i+1 && (int8_t)(2*i) <= 0) ) ;
   }

   for(int i = 2; i; i<<=1) {
      unsigned i2 = 0x55555555 & (i | (i-1)) ;
      i2 = (unsigned) i | i2 ;
      TEST(!ispowerof2(i2)) ;
      i2 = makepowerof2(i2) ;
      // makepowerof2(0x8xxxxxxx) unchanged
      if (i < 0) {
         TEST(i2 == (unsigned)(i|0x55555555)) ;
      } else {
         TEST(i2 == 2*(unsigned)i) ;
      }
      int i3 = 0x55555555 & (i | (i-1)) ;
      i3 = i | i3 ;
      TEST(!ispowerof2(i3)) ;
      i3 = makepowerof2(i3) ;
      if ((i<<1) <= 0) {
         TEST(i3 == (i|0x55555555)) ;
      } else {
         TEST(i3 == 2*i) ;
      }
   }

   for(unsigned i = 2; i; i<<=1) {
      unsigned i2 = i + 1 ;
      TEST(!ispowerof2(i2)) ;
      i2 = makepowerof2(i2) ;
      // makepowerof2(0x8xxxxxxx) unchanged
      if ((int)i < 0) {
         TEST(i2 == (i+1)) ;
      } else {
         TEST(i2 == 2*i) ;
      }
      int i3 = (int) (i + 1) ;
      TEST(!ispowerof2(i3)) ;
      i3 = makepowerof2(i3) ;
      if ((int)i < 0 || (int)(2*i) < 0 ) {
         TEST(i3 == (int)(i+1)) ;
      } else {
         TEST(i3 == (int)(2*i)) ;
      }
   }

   for(uint64_t i = 4; i; i<<=1) {
      uint64_t i2 = i - 1 ;
      TEST(!ispowerof2(i2)) ;
      i2 = makepowerof2(i2) ;
      TEST(i2 == i) ;
      int64_t i3 = (int64_t) (i - 1) ;
      TEST(!ispowerof2(i3)) ;
      i3 = makepowerof2(i3) ;
      TEST(i3 == (int64_t)i || (((int64_t)i) < 0 && i3 == (int64_t)(i-1)) ) ;
   }

   err = 0 ;
ABBRUCH:
   return err ;
}

static int test_signum(void)
{
   for(int i = -128; i <= +127 ; ++i) {
      uint8_t un = (uint8_t) i ;
      int8_t  in = (int8_t) i ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == signum(in)) ;
      if (s < 0) s = -s ;
      TEST(s == signum(un)) ;
   }

   for(int32_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      uint16_t un = (uint16_t) i ;
      int16_t  in = (int16_t) i ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == signum(in)) ;
      if (s < 0) s = -s ;
      TEST(s == signum(un)) ;
   }

   for(int32_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      int32_t i2 = (i==0) ? 0 : ((i < 0) ? (INT32_MIN - INT16_MIN + i) : (INT32_MAX - INT16_MAX + i)) ;
      uint32_t un = (uint32_t) i2 ;
      int32_t  in = (int32_t) i2 ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == signum(in)) ;
      if (s < 0) s = -s ;
      TEST( (unsigned)s == signum(un)) ;
   }

   for(int64_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      int64_t i2 = (i==0) ? 0 : ((i < 0) ? (INT64_MIN - INT16_MIN + i) : (INT64_MAX - INT16_MAX + i)) ;
      uint64_t un = (uint64_t) i2 ;
      int64_t  in = (int64_t) i2 ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == signum(in)) ;
      if (s < 0) s = -s ;
      TEST( (unsigned)s == signum(un)) ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

int unittest_generic_integer()
{
   if (test_powerof2()) goto ABBRUCH ;
   if (test_signum()) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif
