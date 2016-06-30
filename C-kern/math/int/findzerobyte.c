/* title: Intop-FindZeroByte impl

   Implements <Intop-FindZeroByte>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/math/int/findzerobyte.h
    Header file <Intop-FindZeroByte>.

   file: C-kern/math/int/findzerobyte.c
    Implementation file <Intop-FindZeroByte impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/findzerobyte.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: int_t

// group: lifetime


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_findzero(void)
{
   uint64_t val;

   // TEST findzerobyte_int: Wert 0 falls kein 0-Byte
   TEST(0 == findzerobyte_int((uint32_t)0x11223344));
   TEST(0 == findzerobyte_int((uint64_t)0x1122334455667788));
   for (uint64_t b = 1; b < 256; ++b) {
      const uint64_t base[] = { 0x0101010101010101, 0x8080808080808080, 0xffffffffffffffff };
      for (unsigned i = 0; i < lengthof(base); ++i) {
         for (unsigned pos = 0; pos < 8; ++pos) {
            val = base[i];
            TEST(0 == findzerobyte_int((uint32_t)val));
            TEST(0 == findzerobyte_int((uint64_t)val));
            val &= ~ ((uint64_t)0xff << (8*pos));
            val |= b << (8*pos);
            TEST(0 == findzerobyte_int((uint32_t)val));
            TEST(0 == findzerobyte_int((uint64_t)val));
         }
      }
   }

   // TEST findzerobyte_int: Korrekter Positionswert
   TEST(1 == findzerobyte_int((uint32_t)0x11223300));
   TEST(1 == findzerobyte_int((uint64_t)0x1122334455667700));
   TEST(4 == findzerobyte_int((uint32_t)0x00223311));
   TEST(8 == findzerobyte_int((uint64_t)0x0022334455667788));
   for (uint64_t b = 1; b < 256; ++b) {
      val = b;
      for (unsigned pos = 0; pos < 8; ++pos) {
         val |= b << (8*pos);
      }
      for (unsigned pos = 0; pos < 8; ++pos) {
         uint64_t val2 = val & ~ ((uint64_t)0xff << (8*pos));
         const unsigned p1 = pos < 4 ? pos + 1 : 0;
         const unsigned p2 = pos + 1;
         TEST(p1 == findzerobyte_int((uint32_t)val2));
         TEST(p2 == findzerobyte_int((uint64_t)val2));
      }
   }

   // TEST findzerobyte_int: Nur Position des geringwertigsten 0-Bytes
   TEST(1 == findzerobyte_int((uint32_t)0x00223300));
   TEST(1 == findzerobyte_int((uint64_t)0x0022334455667700));
   TEST(3 == findzerobyte_int((uint32_t)0x00003311));
   TEST(7 == findzerobyte_int((uint64_t)0x0000334455667788));
   for (uint64_t b = 1; b < 256; ++b) {
      val = b;
      for (unsigned pos = 0; pos < 8; ++pos) {
         val |= b << (8*pos);
      }
      for (unsigned pos0 = 0; pos0 < 255; ++pos0) {
         unsigned minpos = (unsigned)-1;
         uint64_t val2 = val;
         for (unsigned pos = 0; pos < 8; ++pos) {
            if ((pos0 & (1u << pos)) == 0) {
               // Bit ist 0 in pos0 ==> setze Byte an pos auf 0x00
               val2 &= ~ ((uint64_t)0xff << (8*pos));
               // merke minimales pos
               if (minpos == (unsigned)-1) minpos = pos;
            }
         }
         const unsigned m1 = minpos < 4 ? minpos + 1 : 0;
         const unsigned m2 = minpos + 1;
         TEST(m1 == findzerobyte_int((uint32_t)val2) );
         TEST(m2 == findzerobyte_int(val2));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_math_int_findzerobyte()
{
   if (test_findzero()) goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
