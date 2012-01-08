/* title: Intop-Power2
   Determines if integer i is: (i == 0) || (i == pow(2,x))
   or calculates 2 to the power of x such that (pow(2,x) >= i) && (pow(2,x-1) < i).

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

   file: C-kern/api/math/int/power2.h
    Header file of <Intop-Power2>.

   file: C-kern/math/int/power2.c
    Implementation file <Intop-Power2 impl>.
*/
#ifndef CKERN_MATH_INT_POWER2_HEADER
#define CKERN_MATH_INT_POWER2_HEADER


// section: Functions

// group: query

/* function: ispowerof2_int
 * Determines if argument is of power of 2 or 0.
 * An integer is of power two if exactly one bit is set.
 *
 * Parameter:
 * i - Integer argument to be testet
 *
 * Every integer of the form (binary) 0..00100..0 is of power of 2 (only one bit is set).
 * If a second bit is set i (==01000100) => (i-1) == 01000011 => (i&(i-1)==01000000) != 0 */
extern int ispowerof2_int(unsigned i) ;

/* function: makepowerof2_int
 * Returns power of 2 value of the argument or 0. The returned value is bigger or equal to the given argument.
 *
 * Parameter:
 * i - The argument which is transformed into a power of 2 value and returned.
 *
 * The returned value is the same as argument if
 * 1. *i* == 0
 * 2. <ispowerof2_int>(*i*) is true
 * 3. the next higher power of 2 value can not be represented by this type of integer
 *
 * Algorithm:
 * Set all bits right of highest to one (00011111111) and then add +1 (00100000000). */
extern unsigned makepowerof2_int(unsigned i) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_power2
 * Tests <ispowerof2_int>, and <makepowerof2>. */
extern int unittest_math_int_power2(void) ;
#endif


// section: inline implementation

/* define: inline ispowerof2_int
 * Implements <ispowerof2_int> as a generic function. */
#define ispowerof2_int(i)              !( (typeof(i)) (((i)-1) & (i)) )

/* define: inline makepowerof2_int
 * Implements <makepowerof2_int> as a generic function. */
#define makepowerof2_int(i)                                             \
         (  __extension__ ({                                            \
            typedef typeof(i) _int ;                                    \
            /* unsigned ! */                                            \
            static_assert( ((typeof(i))-1) > 0, ) ;                     \
            /* long long maximum support */                             \
            static_assert( sizeof(i) <= sizeof(long long), ) ;          \
            _int _i = (i) ;                                             \
            if (     !ispowerof2_int(_i)                                \
                  && _i < (_int) (_i << 1) ) {                          \
               _i = (_int) (_i << 1) ;                                  \
               if (  sizeof(unsigned) == 4                              \
                  && sizeof(unsigned) >= sizeof(i)) {                   \
                  static_assert( 4 == sizeof(unsigned), ) ;             \
                  _i = (_int)((1u << (31*(4 == sizeof(unsigned))))      \
                               >> __builtin_clzl((unsigned)_i)) ;       \
               } else if (    sizeof(long) == 8                         \
                         &&   sizeof(long) >= sizeof(i)) {              \
                  _i = (_int)((1ul << (63*(8 == sizeof(long))))         \
                               >> __builtin_clzl((unsigned long)_i)) ;  \
               } else if (sizeof(long long) == 8) {                     \
                  static_assert( 8 <= sizeof(long long), ) ;            \
                  _i = (_int)((1ull << (63*(8 == sizeof(long long))))   \
                               >> __builtin_clzll(_i)) ;                \
               } else if (sizeof(long long) == 16) {                    \
                  static_assert( 16 >= sizeof(long long), ) ;           \
                  _i = (_int)((1ull << (127*(16 == sizeof(long long)))) \
                               >> __builtin_clzll(_i)) ;                \
               }                                                        \
            }                                                           \
            _i ;                                                        \
         }))

#endif
