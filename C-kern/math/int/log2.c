/* title: Intop-Log2 impl
   Implements <Intop-Log2>.

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

   file: C-kern/api/math/int/log2.h
    Header file <Intop-Log2>.

   file: C-kern/math/int/log2.c
    Implementation file <Intop-Log2 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
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
ONABORT:
   return EINVAL ;
}

int unittest_math_int_log2()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_log2())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
