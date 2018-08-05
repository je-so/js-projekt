/* title: Linear-feedback-shift-register impl

   Implements <Linear-feedback-shift-register>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2018 Jörg Seebohn

   file: C-kern/api/math/int/lfsr.h
    Header file <Linear-feedback-shift-register>.

   file: C-kern/math/int/lfsr.c
    Implementation file <Linear-feedback-shift-register impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/lfsr.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// section: lfsr_t

// group: lifetime

void init_lfsr(/*out*/lfsr_t *lfsr, uint64_t state, uint64_t tapbits)
{
   *lfsr = (lfsr_t) lfsr_INIT(state, tapbits);
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   lfsr_t lfsr = lfsr_FREE;

   // TEST lfsr_FREE
   TEST( 0 == lfsr.state);
   TEST( 0 == lfsr.tapbits);

   // TEST lfsr_INIT
   for (uint64_t state=1; state; state <<= 1) {
      for (uint64_t tapbits=1; tapbits; tapbits <<= 1) {
         lfsr = (lfsr_t) lfsr_INIT(state, tapbits);
         TEST( state == lfsr.state);
         TEST( tapbits == lfsr.tapbits);
      }
   }

   // TEST init_lfsr
   for (uint64_t state=1; state; state <<= 1) {
      for (uint64_t tapbits=1; tapbits; tapbits <<= 1) {
         init_lfsr(&lfsr, state, tapbits);
         TEST( state == lfsr.state);
         TEST( tapbits == lfsr.tapbits);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   lfsr_t lfsr = lfsr_FREE;

   // TEST state_lfsr: state == 0
   TEST( 0 == state_lfsr(&lfsr));

   // TEST state_lfsr: state != 0
   for (uint64_t state=1; state; state <<= 1) {
      lfsr.state = state;
      TEST( state == state_lfsr(&lfsr));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   lfsr_t lfsr = lfsr_FREE;
   lfsr_t lfsr2 = lfsr_FREE;
   uint64_t same;

   // TEST reset_lfsr: state != 0
   lfsr.tapbits = 0x1234;
   for (uint64_t state=1; state; state <<= 1) {
      reset_lfsr(&lfsr, state);
      TEST( state == state_lfsr(&lfsr));  // reset
      TEST( 0x1234 == lfsr.tapbits);      // unchanged
   }

   // TEST next_lfsr: 16 bit counter repeats after 65535 steps
   init_lfsr(&lfsr, 1, 0xb400); // bits 16,14,13,11 set (counting from 1 not 0)
   init_lfsr(&lfsr2, 1, 0x8016); // mirror polygon (bits [16][15]...[1] set bit (x) mirrors to (16-x), bit 16 must always be set)
   same = 0;
   for (uint32_t i=1; i<65536; ++i) {
      uint64_t state = next_lfsr(&lfsr);
      TEST( state == lfsr.state);
      TEST( 0xb400 == lfsr.tapbits);
      if (1 == state) {
         TESTP(i == 65535, "i:%d", i);
      }
      state = next_lfsr(&lfsr2);
      TEST( state == lfsr2.state);
      TEST( 0x8016 == lfsr2.tapbits);
      if (1 == state) {
         TESTP(i == 65535, "i:%d", i);
         break;
      }
      same += (lfsr.state == lfsr2.state);
   }
   TEST( 1 == lfsr.state);
   TEST( 1 == lfsr2.state);
   TEST( 15 == same);

   // TEST next_lfsr: 20 bit counter repeats after 1048575 steps
   init_lfsr(&lfsr, 1, 0x90000); // bits 20,17
   init_lfsr(&lfsr2, 1, 0x80004); // mirror polygon bits 20, 3
   same = 0;
   for (uint32_t i=1; i<16*65536; ++i) {
      uint64_t state = next_lfsr(&lfsr);
      TEST( state == lfsr.state);
      TEST( 0x90000 == lfsr.tapbits);
      if (1 == state) {
         TESTP(i == 1048575, "i:%d", i);
      }
      state = next_lfsr(&lfsr2);
      TEST( state == lfsr2.state);
      TEST( 0x80004 == lfsr2.tapbits);
      if (1 == state) {
         TESTP(i == 0xfffff, "i:%d", i);
         break;
      }
      same += (lfsr.state == lfsr2.state);
   }
   TEST( 1 == lfsr.state);
   TEST( 1 == lfsr2.state);
   TEST( 19 == same);

   return 0;
ONERR:
   return EINVAL;
}

static int test_combine(void)
{
   lfsr_t lfsr = lfsr_FREE;
   lfsr_t lfsr2 = lfsr_FREE;
   uint64_t state, state2;
   uint8_t flag[7*64+64];

   init_lfsr(&lfsr, 1, 0x6);  // 3 bit register with period length 7 (2**3 - 1)
   memset(flag, 0, sizeof(flag));
   for (unsigned i=0; i<7; ++i) {
      state = next_lfsr(&lfsr);
      TEST(0<state && state<8);
      TEST(0 == flag[state]);
      flag[state] = 1;
   }
   TEST(1 == state_lfsr(&lfsr)); // reached start state

   init_lfsr(&lfsr, 1, 0x14); // 5 bit register with period length 31 (2**5 - 1)
   memset(flag, 0, sizeof(flag));
   for (unsigned i=0; i<31; ++i) {
      state = next_lfsr(&lfsr);
      TEST(0<state && state<32);
      TEST(0 == flag[state]);
      flag[state] = 1;
   }
   TEST(1 == state_lfsr(&lfsr)); // reached start state

   // combination of 3 and 5 bit register with maximum period length possible
   // cause 7 and 31 have no common divisor other than 1

   init_lfsr(&lfsr, 1, 0x6);     // 3 bit register with period length 7 (2**3 - 1)
   init_lfsr(&lfsr2, 1, 0x14);   // 5 bit register with period length 31 (2**5 - 1)
   memset(flag, 0, sizeof(flag));
   for (unsigned i=0; i<7*31; ++i) {
      state = next_lfsr(&lfsr);
      state2 = next_lfsr(&lfsr2);
      TEST(state*32+state2 < lengthof(flag));
      TEST(0 == flag[state*32+state2]);
      flag[state*32+state2] = 1;
   }
   TEST(1 == state_lfsr(&lfsr)); // reached start state
   TEST(1 == state_lfsr(&lfsr2)); // reached start state

   return 0;
ONERR:
   return EINVAL;
}

int unittest_math_int_lfsr()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_update())         goto ONERR;
   if (test_combine())        goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
