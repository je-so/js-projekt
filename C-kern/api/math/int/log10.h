/* title: Intop-Log10
   Calculates x = log10 of integer i such that pow(10,x) <= i.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_log10
 * Tests implementation of <log10_int>. */
int unittest_math_int_log10(void) ;
#endif


// struct: int_t

// group: compute

/* function: log10_int
 * Returns the logarithm to base 2 as integer of an integer value.
 * This value is the same as the index of the most significant bit.
 *
 * This function is generic in that it accepts either 32 or 64 bit
 * unsigned integers and selects the corresponding implementation.
 *
 * Special case: In case the parameter value is 0 the function returns 0.
 *
 * The returned value is exact if <ispowerof2_int>(*i*) is true else it is truncated
 * to the next lower int value. The following equation always holds true:
 * > i >= pow(10, log10_int(i))
 *
 * Parameter:
 * i - The argument whose logarithm to base 10 is caclulated and returned. */
unsigned log10_int(unsigned i) ;

/* function: log10_int32
 * Implements <log10_int> for 32 bit values. */
unsigned log10_int32(uint32_t i) ;

/* function: log10_int64
 * Implements <log10_int> for 64 bit values. */
unsigned log10_int64(uint64_t i) ;


// section: inline implementation

/* function: log10_int
 * Implements <int_t.log10_int>.
 * TODO: reimplement it with _Generic */
#define log10_int(number)                                               \
   ( __extension__ ({                                                   \
      static_assert(0 < (typeof(number))-1, "only unsigned allowed") ;  \
      (sizeof(number) <= sizeof(uint32_t))                              \
         ? log10_int32((uint32_t)number)                                \
         : log10_int64(number) ;                                        \
   }))

#endif
