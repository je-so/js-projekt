/* title: Intop-Factorisation impl

   Implements <Intop-Factorisation>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/math/int/factorise.h
    Header file <Intop-Factorisation>.

   file: C-kern/math/int/factorise.c
    Implementation file <Intop-Factorisation impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/factorise.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: int_t

// group: compute

/* function: commonfactors_int32
 * Implementierung: r(0) = a, r(1) = b, und r(k+1) = r(k-1) - q(k) * r(k),
 *                  wobei q(k) = (int) (r(k-1)/r(k)).
 *                  Falls r(k+1) == 0 ===> r(k) ist gesuchter Wert.
 *
 * Beweis:
 *
 * Sei r(k) < r(k-1). Falls a < b und damit r(0) < r(1), dann wird im
 * ersten Schritt r(2) auf r(0) gesetzt (q==0). D.h. r(1) = b, r(2) = a.
 * Dann gilt die folgende Argumentation beginnend mit r(3).
 *
 * Sei f das Produkt der Primfaktoren, die sowohl in a als b vorkommen.
 * Dann folgt, es existiert ein af und bf, so daß a = af*f und b = bf*f.
 * Sei rf(k) = f*r(k).
 *
 * r(k+1) = f*rf(k-1) - q(k) * f * rf(k) = f*(rf(k-1) - q(k)*rf(k)) = f*rf(k+1) und
 * 0 <= rf(k+1) < rf(k). Wäre rf(k+1) >= rf(k), dann wäre q(k) zu klein berechnet worden.
 * D.h. r(k+1) ist durch f teilbar und kleiner als r(k). Da r(k+1) aber >= 0,
 * da q(k)*r(k) <= r(k-1), muss die berechnete Reihe mit irgendeinem r(ke) == 0
 * zu Ende kommen. rf(ke-1) = 1 und q(ke-1) == rf(ke-2), da rf(k) und rf(k-1) teilerfremd
 *
 * (1): Werden zwei teilerfremde Zahlen subtrahiert (addiert), dann ist das Resultat teilerfremd
 * zu beiden Zahlen.
 *
 * Beweis zu (1):
 * Sei z = x - y und x und y teilerfremd, dann sind z und y teilerfremd.
 * Seien z und y nicht teilerfremd, dann gilt z=zf*f und y=yf*f. Da x = z+y,
 * folgt daraus, daß x = (zf+zf)*f. Damit wären x und y nicht teilerfremd.
 * D.h falls z = x - y - y - y - y ... -y = x - q*y, damm sind z und y auch teilerfremd.
 * Dieselbe Argumentation gilt auch für z und x.
 *
 * rf(0) und rf(1) sind teilerfremd, da f alle Primfaktoren umfasst, die sich sowohl
 * a als b teilen. rf(k+1) = rf(k-1) - q(k) * rf(k). Da rf(k-1) und rf(k) teilerfremd sind,
 * muss rf(k+1) zu rf(k) nach (1) teilerfremd sein.
 *
 * */
uint32_t commonfactors_int32(uint32_t a, uint32_t b)
{
   uint32_t r0, r1;
   r0 = a; // r0 == a == f * rf0
   r1 = b; // r1 == b == f * rf1
           // rf0 and rf1 have no factor in common except 1 (coprime)

   // if (a < b) ==> q == 0 in first iteration ==> r2 == r0 == a
   // ==> r0 == b, r1 == a after first iteration
   // ==> assume a >= b
   while (r1 != 0) {
      uint32_t q = r0 / r1;      // r0 >= r1 ==> q >= 1 ;
                                 // r1*q + d == r0 && d >= 0 && d < r0 ==> r1*q <= r0
      uint32_t r2 = r0 - q * r1; // r2 < r1 (Proof: if r2 >= r1 ==> d >= 0 && r2 == r1 + d
                                 // ==> r0 == q * r1 + r1 + d == (q+1)*r1 + d
                                 // ==> r0 / r1 == ((q+1)*r1+d) / r1 == q+1 + d/r1
                                 // ==> r0 / r1 >= q+1 ==> contradiction to (r0 / r1 == q)! ;
                                 // r2 == f * (rf0 - q * rf1)
                                 // r2 == f * rf2 &&
                                 // rf2 is coprime with rf1
                                 // Proof: rf0 coprime with rf1 ==> (rf0 - rf1) coprime with rf1
                                 // cause if not (x is factor common to (rf0 - rf1) and rf1)
                                 // (rf0 - rf1) == (rf0 - x*rfx1) == x*D ==> rf0 == x*(rfx1+D)
                                 // ==> rf0 would be not coprime with rf1 (common factor x).
      r0 = r1; // r0 == f * rf1
      r1 = r2; // r1 == f * rf2
      // r2 < r1 && "before assignment" ==> r0 >= r2 && "after assignment"
      // rf1 coprime with rf2 ==> product of common prime factors is f
   }

   return r0;
}

/* function: commonfactors_int64
 * Siehe <commonfactors_int32> für Implementierungsbeschreibung. */
uint64_t commonfactors_int64(uint64_t a, uint64_t b)
{
   uint64_t r0, r1;
   r0 = a;
   r1 = b;

   while (r1 != 0) {
      uint64_t q = r0 / r1;
      uint64_t r2 = r0 - q * r1;
      r0 = r1;
      r1 = r2;
   }

   return r0;
}

/* function: extCommonfactors_int32
 * See <extCommonfactors_int64> for documentation of implementation. */
uint32_t extCommonfactors_int32(uint32_t a, uint32_t b, /*out*/uint32_t* s, /*out*/uint32_t* t)
{
   uint32_t r0 = a, r1 = b;
   uint32_t s0 = 1, t0 = 0;
   uint32_t s1 = 0, t1 = 1;

   while (r1 != 0) {
      uint32_t q = r0 / r1;
      r0 = r0 - q * r1;
      s0 = s0 + q * s1;
      t0 = t0 + q * t1;
      if (r0 == 0) {
         *s = s1;
         *t = t1;
         return r1;
      }
      q = r1 / r0;
      r1 = r1 - q * r0;
      s1 = s1 + q * s0;
      t1 = t1 + q * t0;
   }

   if (s1 > s0) {
      s0 = s1 - s0;
      t0 = t1 - t0;
   }
   *s = s0;
   *t = t0;

   return r0;
}

/* function: extCommonfactors_int64
 *
 * Löst die folgende Gleichung:
 * > b * (*t) - a * (*s) == commonfactors_int64(a,b)
 * wobei *t und *s minimal sind.
 *
 * Es gilt für *s und *t:
 * > *t < a / commonfactors_int64(a,b)
 * > && *s < b / commonfactors_int64(a,b)
 *
 * Sei x == (b / commonfactors_int64(a,b) && y == (a / commonfactors_int64(a,b))
 * dann ist 0 == b * y - a * x
 * und x,y sind die kleinstmöglichen Zahlen.
 *
 * Sei k ein ganzzahliger Wert, dann beschreiben die Tupel (*s + k*x, *t + k*y)
 * alle Lösungsmöglichkeiten der Gleichung.
 *
 * */
uint64_t extCommonfactors_int64(uint64_t a, uint64_t b, /*out*/uint64_t* s, /*out*/uint64_t* t)
{
   uint64_t r0 = a, r1 = b;
   uint64_t s0 = 1, t0 = 0;
   uint64_t s1 = 0, t1 = 1;
   // r(0) == a * s0 + b * t0;
   // r(1) == a * s1 + b * t1;
   // r(k) == a * s(k) + b * t(k)

   // Iteration until r(k+1) == 0 (r(k) is product of common factors of a and b)
   // r(k+1) == r(k-1) - q(k) * r(k) && q(k) == r(k-1)/r(k)
   // s(k+1) == s(k-1) - q(k) * s(k)
   // t(k+1) == t(k-1) - q(k) * t(k)

   // Proof of r(k) == - a * s(k) + b * t(k)
   // ------------------------------------
   // Assume r(k) == a * s(k) + b * t(k)
   //        r(k-1) == a * s(k-1) + b * t(k-1)
   //        (Both r(0) && r(1) are proofen)
   // ==> r(k+1) == r(k-1) - q(k) * r(k) == (a * s(k-1) + b * t(k-1)) - q(k) * (a * s(k) + b * t(k))
   // ==> r(k+1) == a * (s(k-1) - q(k) * s(k)) + b * (t(k-1) - q(k) * t(k)
   // ==> r(k+1) == a * s(k+1) + b * t(k+1)

#if 0 // original loop
   while (r1 != 0) {
      uint64_t q = r0 / r1;
      uint64_t r2 = r0 - q * r1;
      int64_t s2 = s0 - (int64_t) q * s1;
      int64_t t2 = t0 - (int64_t) q * t1;
      r0 = r1;
      r1 = r2;
      s0 = s1;
      s1 = s2;
      t0 = t1;
      t1 = t2;
   }
#endif

   // observation s0 >  0 && s1 <= 0 && s2 >  0 && s3 <= 0 ...
   // observation t0 <= 0 && t1 >  0 && t2 <= 0 && t3 >  0 ...
   // ==> if s(k) > 0 ==> s(k+1) <= 0 && t(k+1) >  0
   // ==> if t(k) > 0 ==> s(k+1) >  0 && t(k+1) <= 0

   // s0 is positive && t0 is negative
   // s1 is negative && t1 is positive

   while (r1 != 0) {
      uint64_t q = r0 / r1;
      r0 = r0 - q * r1;
      s0 = s0 + q * s1;
      t0 = t0 + q * t1;
      if (r0 == 0) {
         // s0 is negative && t0 is positive
         *s = s1; // s1, s0 are swapped
         *t = t1; // t1, t0 are swapped
         return r1; // r1, r0 are swapped
      }
      q = r1 / r0;
      r1 = r1 - q * r0;
      s1 = s1 + q * s0;
      t1 = t1 + q * t0;
      // s0 is postive && t0 is negative
   }

   // s0 is postive ==> t0 is negative

   if (s1 > s0) {
      // 0 == r1 ==> r1 == a * s1 + b * t1 == 0
      // ==> Adding s1, t1 to s0,t0 does not change the result r0 == a*s0 + b*t0
      s0 = s1 - s0; // s0 is positive ==> s1 is negative
      t0 = t1 - t0; // t0 is negative ==> t1 is positive
   }
   *s = s0; // s0 negative
   *t = t0; // t0 positive

   return r0;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_commonfactors(void)
{
    // TEST commonfactors_int: a==0 && b==0
   TEST(0 == commonfactors_int32(0, 0));
   TEST(0 == commonfactors_int64(0, 0));

   // TEST commonfactors_int: a==0 || b==0
   for (uint32_t a = 1; a; a <<= 1) {
      TEST(a == commonfactors_int(a, 0));
      TEST(a == commonfactors_int(0, a));
   }
   for (uint64_t a = 1; a; a <<= 1) {
      TEST(a == commonfactors_int(a, 0));
      TEST(a == commonfactors_int(0, a));
   }

   // TEST commonfactors_int: a==1 || b==1
   for (uint32_t a = 1; a; a <<= 1) {
      TEST(1 == commonfactors_int(a, 1));
      TEST(1 == commonfactors_int(1, a));
   }
   for (uint64_t a = 1; a; a <<= 1) {
      TEST(1 == commonfactors_int(a, 1));
      TEST(1 == commonfactors_int(1, a));
   }

   // TEST commonfactors_int: a,b coprime (return value == 1)
   TEST(1 == commonfactors_int((uint32_t)65536, 3));
   TEST(1 == commonfactors_int((uint64_t)1 << 35, 123456789));

   // TEST commonfactors_int: 2 factor several times
   for (unsigned i = 1; i < 32; ++i) {
      for (unsigned j = 1; j <= i; ++j) {
         TEST(commonfactors_int((uint32_t)1 << i, (uint32_t)1 << j) == (uint32_t)1 << j);
         TEST(commonfactors_int((uint32_t)1 << j, (uint32_t)1 << i) == (uint32_t)1 << j);
      }
   }
   for (unsigned i = 1; i < 64; ++i) {
      for (unsigned j = 1; j <= i; ++j) {
         TEST(commonfactors_int((uint64_t)1 << i, (uint64_t)1 << j) == (uint64_t)1 << j);
         TEST(commonfactors_int((uint64_t)1 << j, (uint64_t)1 << i) == (uint64_t)1 << j);
      }
   }

   // TEST commonfactors_int: multiple factors
   TEST(commonfactors_int((uint32_t)(2*2*2*3*3*5*11*17), (uint32_t) (2*2*3*17*31)) == 2*2*3*17);
   TEST(commonfactors_int((uint64_t)2*2*2*3*3*5*11*17, (uint64_t)2*2*3*17*31) == 2*2*3*17);
   TEST(commonfactors_int((uint32_t)(1031*1031*13*5), (uint32_t) (1031*1031*24)) == 1031*1031);
   TEST(commonfactors_int((uint64_t)(1031*1031*13*5), (uint64_t) (1031*1031*24)) == 1031*1031);
   TEST(commonfactors_int((uint64_t)1031*1031*1031*113*113*113*5*3, (uint64_t)1031*113*113*5*5*3*3) == (uint64_t)1031*113*113*5*3);

   return 0;
ONERR:
   return EINVAL;
}

static int test_extCommonfactors(void)
{
   uint32_t s, t;
   uint64_t m, n;

   // TEST extCommonfactors_int: a == 0 && b == 0
   TEST(0 == extCommonfactors_int32((uint32_t)0, 0, &s, &t));
   TEST(1 == s);
   TEST(0 == t);
   TEST(0 == extCommonfactors_int64((uint64_t)0, 0, &m, &n));
   TEST(1 == s);
   TEST(0 == t);

   // TEST extCommonfactors_int: a == 0 || b == 0
   for (uint32_t a = 1; a; a <<= 1) {
      TEST(a == extCommonfactors_int32(a, 0, &s, &t));
      TEST(1 == s);
      TEST(0 == t);
      TEST(a == extCommonfactors_int32(0, a, &s, &t));
      TEST(0 == s);
      TEST(1 == t);
   }
   for (uint64_t a = 1; a; a <<= 1) {
      TEST(a == extCommonfactors_int64(a, 0, &m, &n));
      TEST(1 == m);
      TEST(0 == n);
      TEST(a == extCommonfactors_int64(0, a, &m, &n));
      TEST(0 == m);
      TEST(1 == n);
   }

   // TEST extCommonfactors_int: a == 1 || b == 1
   for (uint32_t a = 1; a; a <<= 1) {
      TEST(1 == extCommonfactors_int32(a, 1, &s, &t));
      TEST(0 == s);
      TEST(1 == t);
      if (a != 1) {
         TEST(1 == extCommonfactors_int32(1, a, &s, &t));
         TEST(a-1 == s);
         TEST(1 == t);
      }
   }
   for (uint64_t a = 1; a; a <<= 1) {
      TEST(1 == extCommonfactors_int64(a, 1, &m, &n));
      TEST(0 == m);
      TEST(1 == n);
      if (a != 1) {
         TEST(1 == extCommonfactors_int64(1, a, &m, &n));
         TEST(a-1 == m);
         TEST(1 == n);
      }
   }

   // TEST extCommonfactors_int: Modular multiplicative inverse (n == 1/b mod (1<<32)))
   TEST(1 == extCommonfactors_int32((uint32_t)1<<16, 1, &s, &t));
   TEST(0 == s);
   TEST(1 == t);
   TEST(1 == extCommonfactors_int64((uint64_t)1<<32, 1, &m, &n));
   TEST(0 == m);
   TEST(1 == n);
   for (uint32_t b = 3; b < 256; b += 2) {
      TEST(1 == extCommonfactors_int32((uint32_t)1<<16, b, &s, &t));
      TEST(b * t == ((uint32_t)1<<16) * s + 1); // (uint16_t) ((i * b) * n) == i !!
      TEST(1 == extCommonfactors_int32(b, (uint32_t)1<<16, &t, &s));
      TEST(b * t == ((uint32_t)1<<16) * s - 1); // (uint16_t) ((i * b) * n) == -i !!
      TEST(1 == extCommonfactors_int64((uint64_t)1<<32, b, &m, &n));
      TEST(b * n == ((uint64_t)1<<32) * m + 1); // (uint32_t) ((i * b) * n) == i !!
      TEST(1 == extCommonfactors_int64(b, (uint64_t)1<<32, &n, &m));
      TEST(b * n == ((uint64_t)1<<32) * m - 1); // (uint32_t) ((i * b) * n) == -i !!
   }

   // TEST extCommonfactors_int: different prime values
   uint64_t primepairs[] = { 1013, 1033, 1000099, 1000003, 100000000091, 100000000019 };
   for (unsigned i = 0; i < lengthof(primepairs); ++i) {
      uint32_t p1 = (uint32_t) primepairs[i];
      for (unsigned i2 = i+1; i2 < lengthof(primepairs); ++i2) {
         uint32_t p2 = (uint32_t) primepairs[i2];
         if (primepairs[i] == p1 && primepairs[i2] == p2) {
            TEST(1 == extCommonfactors_int32(p1, p2, &s, &t));
            TEST(primepairs[i2] * t == primepairs[i] * s + 1);
            TEST(1 == extCommonfactors_int32(p2, p1, &t, &s));
            TEST(primepairs[i2] * t == primepairs[i] * s - 1);
         }
         TEST(1 == extCommonfactors_int64(primepairs[i], primepairs[i2], &m, &n));
         TEST(primepairs[i2] * n == primepairs[i] * m + 1);
         TEST(1 == extCommonfactors_int64(primepairs[i2], primepairs[i], &n, &m));
         TEST(primepairs[i2] * n == primepairs[i] * m - 1);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_math_int_factorise()
{
   if (test_commonfactors())     goto ONERR;
   if (test_extCommonfactors())  goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
