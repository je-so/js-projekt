/* title: Intop-SquareRoot impl
   Implements <Intop-SquareRoot>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/math/int/sqroot.h
    Header file <Intop-SquareRoot>.

   file: C-kern/math/int/sqroot.c
    Implementation file <Intop-SquareRoot impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/sqroot.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/iotimer.h"
#include "C-kern/api/time/timevalue.h"
#endif


// section: int_t

// group: compute

/* about: High School Square Root Algorithm
 * This explains the mathematical background of the high school algorithm.
 *
 * Definition:
 * The square of a number n is the sum of the first n uneven numbers
 * > n² = 1 + 3 + ... + 2n-1
 *
 * Proof by Induction:
 * > 1² = 1
 * > 2² = 1 + 3
 * > Now (n²=1+3+...+2n-1) is true
 * > ==> (n+1)² = n² + 2n + 1
 * >            = (1+3+...+2n-1) + 2(n+1)-1
 * > ==> (n+1)² = (1+3+...+2(n+1)-3 + 2(n+1)-1)
 *
 * Definition:
 * 1. B is the base of the number (10 for decimal, 2 for binary numbers)
 * 2. a,b,c are numbers in base B and d₀,d₁,d₂... digits in base B
 * 3. sqrt(N) = a * B⁽ⁿ⁻¹⁾ + b * B⁽ⁿ⁻²⁾ + c
 * 4. c = d₍ₙ₋₃₎B⁽ⁿ⁻³⁾ + ... + d₁B + d₀
 *
 * Now with these definitions N is
 * > N = (a * B⁽ⁿ⁻¹⁾ + b * B⁽ⁿ⁻²⁾ + c)²
 * >   = a²B⁽²ⁿ⁻²⁾ + 2abB⁽²ⁿ⁻³⁾ + b²B⁽²ⁿ⁻⁴⁾ + 2acB⁽ⁿ⁻¹⁾ + 2bcB⁽ⁿ⁻²⁾ + c²
 *
 * Partitioning N into pairs of digits and determine a:
 * >     ╭─────────┬───────────┬─────────┬─────╮
 * > N:  │d₂ₙ|d₂ₙ₋₁│d₂ₙ₋₂|d₂ₙ₋₃│   ...   │d₁|d₀│
 * >     ╰─────────┴───────────┴─────────┴─────╯
 * >     ╭─────────┬───────────────────────────╮
 * >     │   a²    │     B⁽²ⁿ⁻²⁾               │
 * >     ╰─────────┴───────────────────────────╯
 * You can determine a by adding the first a uneven numbers
 * until (a+1)² > (d₂ₙ* B + d₂ₙ₋₁)
 *
 * The reason that you can do so is that the values of b and c does not determine
 * the value of a. Choosing instead (a+1) and b=0 and c=0 would result in
 * > (a+1)² * B⁽²ⁿ⁻²⁾ > N
 *
 * You can proof this by setting b and c to their possible maximum values
 * and than add all terms and simplify them so that (a+1)² * B⁽²ⁿ⁻²⁾ is bigger than N.
 * > b = B-1
 * > c = B⁽ⁿ⁻⁴⁾ - 1
 * > N = a²B⁽²ⁿ⁻²⁾ + 2abB⁽²ⁿ⁻³⁾ + b²B⁽²ⁿ⁻⁴⁾ + 2acB⁽ⁿ⁻¹⁾ + 2bcB⁽ⁿ⁻²⁾ + c²
 * >   = a²B⁽²ⁿ⁻²⁾ + 2a(B-1)B⁽²ⁿ⁻³⁾ + (B-1)²B⁽²ⁿ⁻⁴⁾ + 2a(B⁽ⁿ⁻⁴⁾ - 1)B⁽ⁿ⁻¹⁾ + 2(B-1)(B⁽ⁿ⁻⁴⁾-1)B⁽ⁿ⁻²⁾ + (B⁽ⁿ⁻⁴⁾ - 1)²
 * >   = a²B⁽²ⁿ⁻²⁾ + 2aB⁽²ⁿ⁻²⁾-2aB⁽²ⁿ⁻³⁾ +(B²-2B+1)B⁽²ⁿ⁻⁴⁾ + 2aB²ⁿ⁻⁵-2aB⁽ⁿ⁻¹⁾ + 2(B-1)(B⁽ⁿ⁻⁴⁾-1)B⁽ⁿ⁻²⁾ + (B⁽²ⁿ⁻⁸⁾ - 2B⁽ⁿ⁻⁴⁾ + 1)
 * > With B >= 2
 * > ==> N < a²B⁽²ⁿ⁻²⁾ + 2aB⁽²ⁿ⁻²⁾-2aB⁽²ⁿ⁻³⁾ +(B²-3)B⁽²ⁿ⁻⁴⁾+ 2aB²ⁿ⁻⁵-2aB⁽ⁿ⁻¹⁾ + 2(B⁽ⁿ⁻⁴⁾-1)B⁽ⁿ⁻²⁾ + (B⁽²ⁿ⁻⁸⁾ - 2B⁽ⁿ⁻⁴⁾ + 1)
 * >       < a²B⁽²ⁿ⁻²⁾ + 2aB⁽²ⁿ⁻²⁾ + B⁽²ⁿ⁻²⁾ -2aB⁽²ⁿ⁻³⁾ -3B⁽²ⁿ⁻⁴⁾ + 2aB²ⁿ⁻⁵-2aB⁽ⁿ⁻¹⁾+ 2B⁽²ⁿ⁻⁶⁾ + (B⁽²ⁿ⁻⁸⁾ - 2B⁽ⁿ⁻⁴⁾ + 1)
 * > With -2aB⁽²ⁿ⁻³⁾ - 2aB⁽ⁿ⁻¹⁾ <= 0 <= 2aB²ⁿ⁻⁵
 * > and  -3B⁽²ⁿ⁻⁴⁾ - 2B⁽ⁿ⁻⁴⁾ < 0 < 2B⁽²ⁿ⁻⁶⁾ + B⁽²ⁿ⁻⁸⁾ + 1
 * > ==>   N < a²B⁽²ⁿ⁻²⁾ + 2aB⁽²ⁿ⁻²⁾ + B⁽²ⁿ⁻²⁾
 * >       N < (a+1)² * B⁽²ⁿ⁻²⁾
 *
 * Determine b:
 * Calculate r = (d₂ₙ * B + d₂ₙ₋₁) - a²
 *
 * >       ╭─────────┬───────────┬─────────┬─────╮
 * > N-a²: │r₂ₙ|r₂ₙ₋₁│d₂ₙ₋₂|d₂ₙ₋₃│   ...   │d₁|d₀│
 * >       ╰─────────┴───────────┴─────────┴─────╯
 * >       ╭─────────────────────┬───────────────╮
 * >       │           2ab |  0  │   B⁽²ⁿ⁻⁴⁾     │
 * >       │         │   + b²    │               │
 * >       ╰─────────────────────┴───────────────╯
 * You can determine b by adding the first b uneven numbers + b times 2aB
 * until (b+1)² + 2a(b+1)B > (r₂ₙB³+r₂ₙ₋₁B²+d₂ₙ₋₂B+d₂ₙ₋₃)
 *
 * The reason that you can do so is that the value of c does not determine
 * the value of b. Choosing instead (b+1) and c=0 would result in
 * > a²B⁽²ⁿ⁻²⁾ + 2a(b+1)B⁽²ⁿ⁻³⁾ + (b+1)²B⁽²ⁿ⁻⁴⁾ > N
 *
 * You can proof this by setting c to its possible maximum value (B⁽ⁿ⁻⁴⁾ - 1)
 * Its the same prrof as for the value a. I'll leave it up to you.
 *
 * Shift:
 * Calculate N-a²-2abB - b²
 * >        ╭─────────┬───────────┬─────────┬─────╮
 * > N-...: │r₂ₙ|r₂ₙ₋₁│r₂ₙ₋₂|r₂ₙ₋₃│   ...   │d₁|d₀│
 * >        ╰─────────┴───────────┴─────────┴─────╯
 * Then combine the digits a and b into a₂=aB+b and compute N₂
 * > N₂ = (a₂ * B⁽ⁿ⁻²⁾ + b₂ * B⁽ⁿ⁻³⁾ + c₂)²
 *
 * The number a₂ is known so you can compute b₂ as done for the number b.
 * Then combine the digits a₂ and b₂ into a₃=a₂B+b₂ and compute N₃ and so on.
 * Repeat this shift step n-2 times.
 *
 * aₙ is the value of sqrt(N).
 *
 * If you do the whole computation in base 2 with binary numbers
 * a computed digit is either 0 or 1. Therefore the test becomes
 * > 1 + 4a > (r₂ₙB³+r₂ₙ₋₁B²+d₂ₙ₋₂B+d₂ₙ₋₃)
 *
 * */

/* function: sqroot_int32
 * Implements the <High School Square Root Algorithm> for 32 bit unsigned integer. */
uint16_t sqroot_int32(uint32_t number)
{
   uint32_t one = 0x40000000 ;
   uint32_t a   = 0 ;
   uint32_t r   = number ;

   // determine next b

   /* r:
    * > ╭─────────┬───────────┬─────────┬───────╮
    * > │r₂ₙ|r₂ₙ₋₁│r₂ₙ₋₂|r₂ₙ₋₃│   ...   │r₁ | r₀│
    * > ╰─────────┴───────────┴─────────┴───────╯
    *
    * a4_plus_1:
    * > ╭─────────┬───────────┬─────────┬───────╮
    * > │ aₙ|aₙ₋₁ │  0  │  1  │   ...   │ 0 | 0 │
    * > ╰─────────┴───────────┴─────────┴───────╯
    *
    * a after shift:
    * > ╭─────────┬───────────┬─────────┬───────╮
    * > │       aₙ|aₙ₋₁ │  ?  │ (next b)│ 0 | 0 │
    * > ╰─────────┴───────────┴─────────┴───────╯
    * */
   uint32_t a4_plus_1 = one ;
   if (a4_plus_1 <= r) {
      a += one ;
      r -= a4_plus_1 ;
   }
   #define NEXT                  \
   {                             \
      one >>= 2 ; /*next pair*/  \
      a4_plus_1 = a + one ;      \
      a >>= 1 ;                  \
      if (a4_plus_1 <= r) {      \
         a += one ;              \
         r -= a4_plus_1 ;        \
      }                          \
   }
   /*2*/NEXT
   /*3*/NEXT
   /*4*/NEXT
   /*5*/NEXT
   /*6*/NEXT
   /*7*/NEXT
   /*8*/NEXT
   /*9*/NEXT
   /*10*/NEXT
   /*11*/NEXT
   /*12*/NEXT
   /*13*/NEXT
   /*14*/NEXT
   /*15*/NEXT
   /*16*/NEXT
#undef NEXT

   return (uint16_t) a ;
}

/* function: sqroot_int64
 * Implements the <High School Square Root Algorithm> for 64 bit unsigned integer.
 *
 * This function is only included if there is no fast inline implementation of <sqroot_int64>. */
uint32_t sqroot_int64(uint64_t number)
{
   if (!(uint32_t)(number >> 32)) {
      return sqroot_int32((uint32_t)number) ;
   }

   // determine first 16 bit from first 32 bit

   uint32_t one = 0x40000000 ;
   uint32_t a2  = 0 ;
   uint32_t r2  = (uint32_t)(number >> 32) ;

   uint32_t a4_plus_1_2 = one ;
   if (a4_plus_1_2 <= r2) {
      a2 += one ;
      r2 -= a4_plus_1_2 ;
   }
   #define NEXT                  \
   {                             \
      one >>= 2 ; /*next pair*/  \
      a4_plus_1_2 = a2 + one ;   \
      a2 >>= 1 ;                 \
      if (a4_plus_1_2 <= r2) {   \
         a2 += one ;             \
         r2 -= a4_plus_1_2 ;     \
      }                          \
   }
   /*2*/NEXT
   /*3*/NEXT
   /*4*/NEXT
   /*5*/NEXT
   /*6*/NEXT
   /*7*/NEXT
   /*8*/NEXT
   /*9*/NEXT
   /*10*/NEXT
   /*11*/NEXT
   /*12*/NEXT
   /*13*/NEXT
   /*14*/NEXT
   /*15*/NEXT
   /*16*/NEXT
#undef NEXT

   // determine last 16 bit from last 32 bit

   one = 0x40000000 ;
   uint64_t a   = (uint64_t)a2 << 32 ;
   uint64_t r   = ((uint64_t)r2 << 32) + (uint32_t)number ;

   // determine next b

   /* r:
    * > ╭─────────┬───────────┬─────────┬───────╮
    * > │r₂ₙ|r₂ₙ₋₁│r₂ₙ₋₂|r₂ₙ₋₃│   ...   │r₁ | r₀│
    * > ╰─────────┴───────────┴─────────┴───────╯
    *
    * a4_plus_1:
    * > ╭─────────┬───────────┬─────────┬───────╮
    * > │ aₙ|aₙ₋₁ │  0  │  1  │   ...   │ 0 | 0 │
    * > ╰─────────┴───────────┴─────────┴───────╯
    *
    * a after shift:
    * > ╭─────────┬───────────┬─────────┬───────╮
    * > │       aₙ|aₙ₋₁ │  ?  │ (next b)│ 0 | 0 │
    * > ╰─────────┴───────────┴─────────┴───────╯
    * */
   uint64_t a4_plus_1 = a | one ;
   a >>= 1 ;
   if (a4_plus_1 <= r) {
      a |= one ;
      r -= a4_plus_1 ;
   }
   #define NEXT                  \
   {                             \
      one >>= 2 ; /*next pair*/  \
      a4_plus_1 = a | one ;      \
      a >>= 1 ;                  \
      if (a4_plus_1 <= r) {      \
         a |= one ;              \
         r -= a4_plus_1 ;        \
      }                          \
   }
   /*2*/NEXT
   /*3*/NEXT
   /*4*/NEXT
   /*5*/NEXT
   /*6*/NEXT
   /*7*/NEXT
   /*8*/NEXT
   /*9*/NEXT
   /*10*/NEXT
   /*11*/NEXT
   /*12*/NEXT
   /*13*/NEXT
   /*14*/NEXT
   /*15*/NEXT
   /*16*/NEXT
#undef NEXT

   return (uint32_t) a ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_squareroot(void)
{
   // TEST sqroot_int,sqroot_int32,sqroot_int64 for all return values 0..UINT16_MAX
   for (uint32_t r = 0; r <= UINT16_MAX; ++r) {
      uint32_t N = r * r ;
      TEST(r == sqroot_int(N)) ;
      TEST(r == sqroot_int32(N)) ;
      TEST(r == sqroot_int64(N)) ;
      if (r) {
         TEST(r == 1u+sqroot_int(N-1u)) ;
         TEST(r == 1u+sqroot_int32(N-1u)) ;
         TEST(r == 1u+sqroot_int64(N-1u)) ;
      }
   }

   // TEST sqroot_int,sqroot_int32,sqroot_int64 for all values returning UINT16_MAX
   for (uint32_t N = (uint32_t)UINT16_MAX*UINT16_MAX; N; ++N) {
      TEST(UINT16_MAX == sqroot_int(N)) ;
      TEST(UINT16_MAX == sqroot_int32(N)) ;
      TEST(UINT16_MAX == sqroot_int64(N)) ;
   }

   // TEST sqroot_int,sqroot_int64 for some return values > UINT16_MAX
   TEST(0x80000000 == sqroot_int((uint64_t)0x80000000*0x80000000)) ;
   TEST(0x80000000 == sqroot_int64((uint64_t)0x80000000*0x80000000)) ;
   TEST(0x80000001 == sqroot_int((uint64_t)0x80000001*0x80000001)) ;
   TEST(0x80000001 == sqroot_int64((uint64_t)0x80000001*0x80000001)) ;
   TEST(0x80010001 == sqroot_int((uint64_t)0x80010001*0x80010001)) ;
   TEST(0x80010001 == sqroot_int64((uint64_t)0x80010001*0x80010001)) ;
   for (uint32_t r = UINT16_MAX+1, incr = 1; r > UINT16_MAX; r += incr) {
      uint64_t N = (uint64_t)r * r ;
      TEST(r == sqroot_int(N)) ;
      TEST(r == sqroot_int64(N)) ;
      TEST(r == 1u+sqroot_int(N-1u)) ;
      TEST(r == 1u+sqroot_int64(N-1u)) ;
      incr <<= 1 ;
      if (incr > UINT16_MAX) incr = 1 ;
   }
   for (uint32_t r = UINT32_MAX; r > UINT32_MAX-10000; --r) {
      uint64_t N = (uint64_t)r * r ;
      TEST(r == sqroot_int(N)) ;
      TEST(r == sqroot_int64(N)) ;
      TEST(r == 1u+sqroot_int(N-1u)) ;
      TEST(r == 1u+sqroot_int64(N-1u)) ;
   }

   // TEST sqroot_int,sqroot_int64 for values returning UINT32_MAX
   TEST(UINT32_MAX == sqroot_int(UINT64_MAX)) ;
   TEST(UINT32_MAX == sqroot_int64(UINT64_MAX)) ;
   for (uint64_t N = (uint64_t)UINT32_MAX*UINT32_MAX; N > UINT32_MAX; N += UINT32_MAX) {
      TEST(UINT32_MAX == sqroot_int(N)) ;
      TEST(UINT32_MAX == sqroot_int64(N)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_speedcompare(void)
{
   iotimer_t timer = iotimer_INIT_FREEABLE ;
   uint64_t  time_in_microseconds_slow = 0 ;
   uint64_t  time_in_microseconds_fast = 0 ;
   uint32_t  sum_slow = 0 ;
   uint32_t  sum_fast = 0 ;

   if ((uint32_t(*)(uint64_t))&sys_sqroot_int64 == &sqroot_int64) {
      // compare default implementation with itself does not make sense
      return 0 ;
   }

   TEST(0 == init_iotimer(&timer, sysclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t) { .nanosec = 1000 } )) ;
   for (unsigned i = 0; i < 500000; ++i) {
      sum_slow += sqroot_int64(UINT64_MAX-i) ;
   }
   TEST(0 == expirationcount_iotimer(timer, &time_in_microseconds_slow)) ;
   for (unsigned i = 0; i < 500000; ++i) {
      sum_fast += (uint32_t)sys_sqroot_int64(UINT64_MAX-i) ;
   }
   TEST(0 == expirationcount_iotimer(timer, &time_in_microseconds_fast)) ;
   TEST(0 == stop_iotimer(timer)) ;
   TEST(0 == free_iotimer(&timer)) ;

   // TEST sqroot_int64 has same result as sys_sqroot_int64
   TEST(sum_slow == sum_fast) ;

   // TEST sqroot_int64 is slower than sys_sqroot_int64
   TEST(time_in_microseconds_slow > time_in_microseconds_fast) ;

   return 0 ;
ONABORT:
   free_iotimer(&timer) ;
   return EINVAL ;
}

int unittest_math_int_sqroot()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_squareroot())     goto ONABORT ;
   if (test_speedcompare())   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
