/* title: Intop-Signum
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

   file: C-kern/api/math/int/signum.h
    Header file of <Intop-Signum>.

   file: C-kern/math/int/signum.c
    Implementation file <Intop-Signum impl>.
*/
#ifndef CKERN_MATH_INT_SIGNUM_HEADER
#define CKERN_MATH_INT_SIGNUM_HEADER

/* function: signum
 * Return the sign of an integer.
 *
 * Paramter:
 * i - The argument which sign is returned.
 *
 * Returns:
 * -1  - i is megative
 * 0   - i is zero
 * +1  - i is positive */
extern int signum(int i) ;


// section: inline implementation

/* define: inline signum
 * Implements <signum> as a generic function.
 * > #define signum(i)  ( ((i) > 0) - ((i) < 0) )  */
#define signum(i) \
         ( __extension__ ({ \
            typedef typeof(i) _int_t ; \
            static_assert( ((_int_t)-1) < 0, "works only with signed integers") ; \
            _int_t _temp = (i) ; \
            ( (_temp > 0) - (_temp < 0) ) ; \
         }))


#ifdef KONFIG_UNITTEST
extern int unittest_math_int_signum(void) ;
#endif

#endif
