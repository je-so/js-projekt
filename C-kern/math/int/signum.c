/* title: Intop-Signum impl
   Implement <unittest_generic_bits_signum>.

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

   file: C-kern/api/math/int/signum.h
    Header file of <Intop-Signum>.

   file: C-kern/math/int/signum.c
    Implementation file <Intop-Signum impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/signum.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_math_int_signum,ABBRUCH)


int unittest_math_int_signum()
{
   for(int i = -128; i <= +127 ; ++i) {
      int8_t in = (int8_t) i ;
      int     s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == signum(in)) ;
   }

   for(int32_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      int16_t in = (int16_t) i ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == signum(in)) ;
   }

   for(int32_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      int32_t in = (i==0) ? 0 : ((i < 0) ? (INT32_MIN - INT16_MIN + i) : (INT32_MAX - INT16_MAX + i)) ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == signum(in)) ;
   }

   for(int64_t i = INT16_MIN; i <= INT16_MAX ; ++i) {
      int64_t in = (i==0) ? 0 : ((i < 0) ? (INT64_MIN - INT16_MIN + i) : (INT64_MAX - INT16_MAX + i)) ;
      int      s = (i == 0) ? 0 : ((i < 0) ? -1 : +1) ;
      TEST(s == signum(in)) ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif
