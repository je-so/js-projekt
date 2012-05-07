/* title: Intop-Sign
   Calculates sign of integer.

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
int unittest_math_int_sign(void) ;
#endif


// section: int_t

// group: query

/* function: sign_int
 * Return the sign of an integer (signum function).
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
