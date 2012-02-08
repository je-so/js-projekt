/* title: Intop-Log10
   Calculates x = log10 of integer i such that pow(10,x) <= i.

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

   file: C-kern/api/math/int/log10.h
    Header file <Intop-Log10>.

   file: C-kern/math/int/log10.c
    Implementation file <Intop-Log10 impl>.
*/
#ifndef CKERN_MATH_INT_LOG10_HEADER
#define CKERN_MATH_INT_LOG10_HEADER


// section: Functions

// group: query

/* function: log10_int
 * Returns the logarithm to base 2 as integer of an integer value.
 * This value is the same as the index of the most significant bit.
 *
 * Special case: In case the parameter value is 0 the function returns 0.
 *
 * The returned value is exact if <ispowerof2_int>(*i*) is true else it is truncated
 * to the next lower int value. The following equation always holds true:
 * > i >= pow(10, log10_int(i))
 *
 * Parameter:
 * i - The argument whose logarithm to base 10 is caclulated and returned. */
unsigned log10_int(uint32_t i) ;

/* function: log10_int64
 * Same as <log10_int>, except for supporting 64 bit values. */
unsigned log10_int64(uint64_t i) ;


// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_log10
 * Tests <log10_int>. */
int unittest_math_int_log10(void) ;
#endif



#endif
