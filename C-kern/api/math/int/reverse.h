/* title: Intop-Reverse
   Reverses the bits of an integer.
   This means for a integer type of 32-bit:
   bit0 and bit31 are swapped, bit1 and bit30 are swapped ...

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

   file: C-kern/api/math/int/reverse.h
    Header file <Intop-Reverse>.

   file: C-kern/math/int/reverse.c
    Implementation file <Intop-Reverse impl>.
*/
#ifndef CKERN_MATH_INT_REVERSE_HEADER
#define CKERN_MATH_INT_REVERSE_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_reverse
 * Test function <int_t.reverse_int>. */
int unittest_math_int_reverse(void) ;
#endif


// struct: int_t

// group: compute

/* function: reverse_int
 * Reverses the bits of an integer.
 * Every set bit of the value i is shifted to position (bitsof(i)-1-bi)
 * where bi is the bit index of the set bit beginning with 0.
 * 0x80 is reversed to 0x01. 0x4010 is reversed into 0x0802.*/
unsigned reverse_int(unsigned i) ;


// section: inline implementation

/* define: reverse_int
 * Implements <int_t.reverse_int>. */
#define reverse_int(i)                                \
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
