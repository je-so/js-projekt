/* title: Intop-Sign
   Calculates sign of integer.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/math/int/sign.h
    Header file of <Intop-Sign>.

   file: C-kern/math/int/sign.c
    Implementation file <Intop-Sign impl>.
*/
#ifndef CKERN_MATH_INT_SIGN_HEADER
#define CKERN_MATH_INT_SIGN_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_sign
 * Tests implementation of <sign_int>. */
int unittest_math_int_sign(void) ;
#endif


// struct: int_t

// group: query

/* function: sign_int
 * Return the sign of an integer (signum function).
 * This function is implemented as a generic function for all integer types.
 *
 * Paramter:
 * i - The argument whose sign is returned.
 *
 * Returns:
 * -1  - i is megative
 * 0   - i is zero
 * +1  - i is positive */
int sign_int(int i) ;


// section: inline implementation

/* define: sign_int
 * Implements <int_t.sign_int> as a generic function. */
#define sign_int(i) \
         ( __extension__ ({ \
            typedef typeof(i) _int_t ; \
            static_assert( ((_int_t)-1) < 0, "works only with signed integers") ; \
            _int_t _temp = (i) ; \
            ( (_temp > 0) - (_temp < 0) ) ; \
         }))

#endif
