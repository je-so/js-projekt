/* title: Intop-Log2
   Calculates x = log2 of integer i such that pow(2,x) <= i.

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

   file: C-kern/api/math/int/log2.h
    Header file <Intop-Log2>.

   file: C-kern/math/int/log2.c
    Implementation file <Intop-Log2 impl>.
*/
#ifndef CKERN_MATH_INT_LOG2_HEADER
#define CKERN_MATH_INT_LOG2_HEADER


// section: Functions

// group: query

/* function: log2_int
 * Returns the logarithm to base 2 as integer of an integer value.
 * This value is the same as the index of the most significant bit.
 *
 * Special case: In case the parameter value is 0 the function returns 0.
 *
 * The returned value is exact if <ispowerof2_int>(*i*) is true else it is truncated
 * to the next lower int value. The following equation always holds true:
 * > i >= pow(2, log2_int(i))
 *
 * Parameter:
 * i - The argument whose logarithm to base 2 is caclulated and returned. */
unsigned log2_int(unsigned i) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_power2
 * Tests <log2_int>. */
int unittest_math_int_log2(void) ;
#endif


// section: inline implementation

/* define: inline log2_int
 * Implements <log2_int> as a generic function. */
#define log2_int(i)                                               \
         ( __extension__ ({                                       \
            int _result ;                                         \
            /* unsigned ! */                                      \
            static_assert( ((typeof(i))-1) > 0, ) ;               \
            /* long long maximum support */                       \
            static_assert( sizeof(i) <= sizeof(long long), ) ;    \
            if (  sizeof(unsigned) == 4                           \
               && sizeof(unsigned) >= sizeof(i)) {                \
               static_assert( 4 == sizeof(unsigned), ) ;          \
               unsigned _i = (unsigned) (i) ;                     \
               _result = (_i != 0) * (31 ^ __builtin_clz(_i)) ;   \
            } else if (    sizeof(long) == 8                      \
                      &&   sizeof(long) >= sizeof(i)) {           \
               unsigned long _i = (unsigned long) (i) ;           \
               _result = (_i != 0) * (63 ^ __builtin_clzl(_i)) ;  \
            } else if (sizeof(long long) == 8) {                  \
               static_assert( 8 <= sizeof(long long), ) ;         \
               unsigned long long _i = (i) ;                      \
               _result = (_i != 0) * (63 ^ __builtin_clzll(_i)) ; \
            } else if (sizeof(long long) == 16) {                 \
               static_assert( 16 >= sizeof(long long), ) ;        \
               unsigned long long _i = (i) ;                      \
               _result = (_i != 0) * (127^ __builtin_clzll(_i)) ; \
            }                                                     \
            (unsigned) _result ;                                  \
         }))

#endif
