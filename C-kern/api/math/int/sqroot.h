/* title: Intop-SquareRoot
   Computes the square root of an integer number.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/math/int/sqroot.h
    Header file <Intop-SquareRoot>.

   file: C-kern/math/int/sqroot.c
    Implementation file <Intop-SquareRoot impl>.
*/
#ifndef CKERN_MATH_INT_SQROOT_HEADER
#define CKERN_MATH_INT_SQROOT_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_sqroot
 * Tests implementation of <sqroot_int>. */
int unittest_math_int_sqroot(void);
#endif


// struct: int_t

// group: compute

/* function: sqroot_int
 * Returns the square root of an unsigned integer number.
 * This function is generic in that it accepts either 32 or 64 bit
 * unsigned integers and selects the corresponding implementation.
 * The returned value has the following property:
 * > number >= sqroot_int(number) * sqroot_int(number)
 * > && number < (sqroot_int(number)+1) * (sqroot_int(number)+1)
 * */
unsigned sqroot_int(unsigned number);

/* function: sqroot_int32
 * Returns the square root from an unsigned 32 bit integer.
 * This function is called from <sqroot_int>. */
uint16_t sqroot_int32(uint32_t number);

/* function: sqroot_int64
 * Returns the square root from an unsigned 64 bit integer.
 * This function is called from <sqroot_int>. */
uint32_t sqroot_int64(uint64_t number);


// section: inline implementation

/* function: sqroot_int
 * Implements <int_t.sqroot_int>.
 * Calls <sys_sqroot_int64> for a faster implementation of the default implementation <sqroot_int64>. */
#define sqroot_int(number) \
         ( _Generic((number),                               \
            uint8_t:  sqroot_int32((uint32_t)(number)),     \
            uint16_t: sqroot_int32((uint32_t)(number)),     \
            uint32_t: sqroot_int32((uint32_t)(number)),     \
            uint64_t: (uint32_t)sys_sqroot_int64(number)))

#endif
