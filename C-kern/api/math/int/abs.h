/* title: Intop-Abs
   Calculates the absolute value.

   TODO: Introduce abs_int with _Generic and replace direct calls to abs_int64 / abs_int32 !!!
   > _Generic( (i), int8_t : uint8_t, int16_t : uint16_t, int32_t : uint32_t, int64_t : uint64_t )

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/math/int/abs.h
    Header file <Intop-Abs>.

   file: C-kern/math/int/abs.c
    Implementation file <Intop-Abs impl>.
*/
#ifndef CKERN_MATH_INT_ABS_HEADER
#define CKERN_MATH_INT_ABS_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_abs
 * Tests <int_t.abs_int32> and <int_t.abs_int64>. */
int unittest_math_int_abs(void) ;
#endif


// struct: int_t

// group: compute

/* function: abs_int32
 * Returns the absolute value as unsigned value from a signed integer.
 *
 * The returned value is the same value as the parameter in case the parameter
 * is positive else it the first negated and then returned.
 *
 * An unsigned return value can represent all signed values as positive sign.
 * There is no need to handle INT_MIN (maximum negative value of an integer)
 * as special or undefined value.
 *
 * Parameter:
 * i - The argument whose absolute value is returned. */
uint32_t abs_int32(int32_t i) ;

/* function: abs_int64
 * Returns the absolute value as unsigned value from a signed.
 * This function operates on 64 bit integers. See also <abs_int32>. */
uint64_t abs_int64(int64_t i) ;


// section: inline implementation

/* define: abs_int32
 * Implements <int_t.abs_int32>. */
#define abs_int32(i)                                              \
         ( __extension__ ({                                       \
            /* signed ! */                                        \
            static_assert( ((typeof(i))-1) < 0, ) ;               \
            static_assert( sizeof(i) <= sizeof(int32_t), ) ;      \
            int32_t _i = (i) ;                                    \
            ((_i < 0) ? -(uint32_t)_i : (uint32_t)_i) ;           \
         }))

/* define: abs_int64
 * Implements <int_t.abs_int64>. */
#define abs_int64(i)                                              \
         ( __extension__ ({                                       \
            /* signed ! */                                        \
            static_assert( ((typeof(i))-1) < 0, ) ;               \
            static_assert( sizeof(i) <= sizeof(int64_t), ) ;      \
            int64_t _i = (i) ;                                    \
            ((_i < 0) ? -(uint64_t)_i : (uint64_t)_i) ;           \
         }))

#endif
