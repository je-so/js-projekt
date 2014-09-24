/* title: Intop-Bitorder

   Reverses the bits of an integer.
   This means for a integer type of 32-bit:
   bit0 and bit31 are swapped, bit1 and bit30 are swapped ...

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/math/int/bitorder.h
    Header file <Intop-Bitorder>.

   file: C-kern/math/int/bitorder.c
    Implementation file <Intop-Bitorder impl>.
*/
#ifndef CKERN_MATH_INT_REVERSE_HEADER
#define CKERN_MATH_INT_REVERSE_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_bitorder
 * Test function <int_t.reversebits_int>. */
int unittest_math_int_bitorder(void) ;
#endif


// struct: int_t

// group: bit-operations

/* function: reversebits_int
 * Reverses the bits of an integer.
 * Every set bit of the value i is shifted to position (bitsof(i)-1-bi)
 * where bi is the bit index of the set bit beginning with 0.
 * 0x80 is reversed to 0x01. 0x4010 is reversed into 0x0802.*/
unsigned reversebits_int(unsigned i) ;



// section: inline implementation

/* define: reversebits_int
 * Implements <int_t.reversebits_int>. */
#define reversebits_int(i)                            \
         ( __extension__ ({                           \
            /* unsigned */                            \
            static_assert(((typeof(i))-1) > 0, ) ;    \
            typeof(i) _i  = (i) ;                     \
            typeof(i) _ri = 0 ;                       \
            unsigned  _nrbits = bitsof(_ri) ;         \
            for (; _nrbits; --_nrbits) {              \
               _ri = (typeof(i))(_ri << 1) ;          \
               _ri = (typeof(i))(_ri | (_i & 0x01)) ; \
               _i  = (typeof(i))(_i >> 1) ;           \
            }                                         \
            _ri ;                                     \
         }))


#endif
