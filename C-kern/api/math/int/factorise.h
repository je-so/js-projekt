/* title: Intop-Factorisation

   * Function <commonfactors_int> computes greatest common divisor (gcd).


   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/math/int/factorise.h
    Header file <Intop-Factorisation>.

   file: C-kern/math/int/factorise.c
    Implementation file <Intop-Factorisation impl>.
*/
#ifndef CKERN_MATH_INT_FACTORISE_HEADER
#define CKERN_MATH_INT_FACTORISE_HEADER

/* typedef: struct int_t
 * Export <int_t> into global namespace. */
typedef struct int_t int_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_factorise
 * Test <int_t> functionality. */
int unittest_math_int_factorise(void);
#endif


// struct: int_t

// group: compute

/* function: commonfactors_int
 * Berechnet den größten gemeinsamen Teiler von a und b.
 * Der größte gemeinsame Teiler ist identisch mit dem Produkt
 * aller Primfaktoren, die sowohl in a als auch in b vorkommen.
 * Kommt ein Primfaktor dabei mehrfach vor und seien Va und Vb die Anzahl
 * des Vorkommens dieses Faktors in a oder b, so wird dieser Primfaktor
 * genau Va mal verwendet, falls Va <= Vb, ansonsten Vb mal.
 *
 * Example:
 * commonfactors_int(2*2*2*3*3*5*11*17, 2*2*3*17*17*31) == 2*2*3*17
 *
 * Implementation:
 * Euclid's algorithm is used (see https://en.wikipedia.org/wiki/Greatest_common_divisor).
 */
unsigned commonfactors_int(unsigned a, unsigned b);

/* function: commonfactors_int32
 * Implements <commonfactors_int> for type uint32_t. */
uint32_t commonfactors_int32(uint32_t a, uint32_t b);

/* function: commonfactors_int64
 * Implements <commonfactors_int> for type uint64_t. */
uint64_t commonfactors_int64(uint64_t a, uint64_t b);

/* function: extCommonfactors_int32
 * See <extCommonfactors_int64> for a description. */
uint32_t extCommonfactors_int32(uint32_t a, uint32_t b, /*out*/uint32_t* s, /*out*/uint32_t* t);

/* function: extCommonfactors_int64
 *
 * Berechnet Werte s und t für gegebene a und b,
 * so daß folgende Gleichung erfüllt ist
 *
 * > b*t - a*s == commonfactors_int(a,b)
 * > a*(-s) + b*t == commonfactors_int(a,b)
 * > b*t == commonfactors_int(a,b) + a*s
 *
 * Bézout's identity:
 * See https://en.wikipedia.org/wiki/B%C3%A9zout%27s_identity.
 *
 * Preconditions:
 * a > 0 && b > 0
 *
 * extCommonfactors_int64(a,0,&s,&t) returns
 * wrong values s == 1 && t == 0 cause s = -1 could not be returned
 *
 * extCommonfactors_int64(0,a,&s,&t) returns
 * wrong values s == 0 && t == 1 which is a valid solution
 *
 * Returns:
 * c=extCommonfactors_int(a,b,&s,&t) with a > 0 && b > 0
 *
 * with the following properties of c, s, t
 *
 * > c == commonfactors_int(a,b)
 * > && b * t - a * s == c
 * > && t < a/c && s < b/c
 *
 * Example: Division by 5 modulo (1<<32):
 *
 * For calculation of modular multiplicative inverse
 * see https://en.wikipedia.org/wiki/Modular_multiplicative_inverse
 *
 * > uint64_t m, n;
 * > extCommonfactors_int((uint64_t)1<<32, 5, &m, &n);
 * > // m == 4
 * > // n == 3435973837
 * > assert( (5 * 6) * n == 6 * m * ((uint64_t)1<<32) + 6 );
 * > assert( (uint32_t) ((5 * 6) * n) == 6 );
 * > assert( (((5 * 6) * n) >> 32) == m * 6 );
 *
 */
uint64_t extCommonfactors_int64(uint64_t a, uint64_t b, /*out*/uint64_t* s, /*out*/uint64_t* t);


// section: inline implementation

/* define: commonfactors_int
 * Implements <int_t.commonfactors_int>. */
#define commonfactors_int(a, b) \
         (  sizeof(a) <= sizeof(uint32_t) && sizeof(b) <= sizeof(uint32_t) \
            ? commonfactors_int32((a), (b))  \
            : commonfactors_int64((a), (b))  \
         )

/* define: extCommonfactors_int
 * Implements <int_t.extCommonfactors_int>. */
#define extCommonfactors_int(a, b, s, t) \
         (  sizeof(a) <= sizeof(uint32_t) && sizeof(b) <= sizeof(uint32_t) \
            ? extCommonfactors_int32((a), (b), (s), (t))  \
            : extCommonfactors_int64((a), (b), (s), (t))  \
         )

#endif
