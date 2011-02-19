/*
   C-System-Layer: C-kern/api/generic/integer.h
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/
#ifndef CKERN_GENERIC_INTEGER_HEADER
#define CKERN_GENERIC_INTEGER_HEADER


/* function: ispowerof2
 * Determines if argument is of power of 2 or 0 (at most one bit is set all oother zero)
 *
 * Parameter:
 * i - Integer argument to be testet
 *
 * Every integer of the form (binary) 0..00100..0 is of power of 2 (only one bit is set).
 * If a second bit is set i (==01000100) => (i-1) == 01000011 => (i&(i-1)==01000000) != 0 */
extern int ispowerof2(int i) ;

/* function: makepowerof2
 * Return power of 2 value of the argument which is bigger or equal to.
 *
 * Paramter:
 * i - The argument which is transformed into a power of 2 value and returned.
 *
 * If *i* is already a power of 2 value or if the next higher power of 2 value can not
 * be represented by this type of integer nothing is done.
 *
 * Algorithm:
 * Multiply argument by 2 and clear all bits except for the highest. */
extern int makepowerof2(int i) ;

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

/* define: inline ispowerof2
 * Implements <ispowerof2> as a generic function.
 * > #define ispowerof2(i)   !( (typeof(i)) (((i)-1) & (i)) ) */
#define ispowerof2(i)   !( (typeof(i)) (((i)-1) & (i)) )

/* define: inline makepowerof2
 * Implements <makepowerof2> as a generic function. */
#define makepowerof2(_Int) \
         ( __extension__ ({ \
            typedef typeof(_Int) _IntType ; \
            _IntType _result = _Int ; \
            if (  !ispowerof2(_result) \
                  && _result > 0 \
                  && _result < (_IntType) (_result << 1) ) { \
               _IntType _mask = _result ; \
               _mask |= (_IntType) (_mask>>1) ; \
               _mask |= (_IntType) (_mask>>2) ; \
               _result = (_IntType) (~_mask) & (_IntType) (_result << 1) ; \
               while( !ispowerof2(_result) ) { \
                  _result = (_IntType) ((_result-1) & _result) ; \
               } \
            } \
            _result ; \
         }))

/* define: inline signum
 * Implements <signum> as a generic function.
 * > #define signum(_Int)  ( ((_Int) != 0) | (v >> (sizeof(_Int) * CHAR_BIT - 1)) )  */
#define signum(_Int) \
   ( ((_Int) != 0) | ((_Int) >> (sizeof(_Int) * CHAR_BIT - 1)) )


#ifdef KONFIG_UNITTEST
extern int unittest_generic_integer(void) ;
#endif

#endif
