/* title: Intop-Power2
   Determines if integer i is: (i == 0) || (i == pow(2,x))
   or calculates 2^x such that (pow(2,x) >= i) && (pow(2,x-1) < i).

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


/* function: ispowerof2
 * Determines if argument is of power of 2 (<=> at most one bit is set all other zero) or 0.
 *
 * Parameter:
 * i - Integer argument to be testet
 *
 * Every integer of the form (binary) 0..00100..0 is of power of 2 (only one bit is set).
 * If a second bit is set i (==01000100) => (i-1) == 01000011 => (i&(i-1)==01000000) != 0 */
extern int ispowerof2(unsigned i) ;

/* function: makepowerof2
 * Returns power of 2 value of the argument or 0. The returned value is bigger or equal to the given argument.
 *
 * Parameter:
 * i - The argument which is transformed into a power of 2 value and returned.
 *
 * The returned value is the same as argument if
 * 1. *i* <= 0
 * 2. ispowerof2(*i*)
 * 3. the next higher power of 2 value can not be represented by this type of integer
 *
 * Algorithm:
 * Set all bits right of highest to one (00011111111) and then add +1 (00100000000). */
extern int makepowerof2(unsigned i) ;


// section: inline implementation

/* define: inline ispowerof2
 * Implements <ispowerof2> as a generic function.
 * > #define ispowerof2(i)   !( (typeof(i)) (((i)-1) & (i)) ) */
#define ispowerof2(i)   !( (typeof(i)) (((i)-1) & (i)) )

/* define: inline makepowerof2
 * Implements <makepowerof2> as a generic function. */
#define makepowerof2(i)                         \
         ( __extension__ ({                        \
            typedef typeof(i) _int_t ;          \
            static_assert( ((_int_t)-1) > 0, "only unsigned integer supported" ) ; \
            static_assert( sizeof(_int_t) <= 8, "64bit maximum support" ) ; \
            _int_t _result = i ;                \
            if (     !ispowerof2(_result)          \
                  && _result < (_int_t) (_result << 1) ) { \
               _result |= (_int_t) (_result>>1) ;  \
               _result |= (_int_t) (_result>>2) ;  \
               _result |= (_int_t) (_result>>4) ;  \
               if (2 <= sizeof(_int_t)) {          \
                  _result |= (_int_t) (_result>>((2 <= sizeof(_int_t))*8)) ;  \
               }                                   \
               if (4 <= sizeof(_int_t)) {          \
                  _result |= (_int_t) (_result>>((4 <= sizeof(_int_t))*16)) ; \
               }                                   \
               if (8 == sizeof(_int_t)) {          \
                  _result |= (_int_t) (_result>>((8 == sizeof(_int_t))*32)) ; \
               }                                   \
               ++ _result ;                        \
            }                                      \
            _result ;                              \
         }))

#ifdef KONFIG_UNITTEST
extern int unittest_math_int_power2(void) ;
#endif

#endif
