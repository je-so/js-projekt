/* title: Intop-Power2
   Determines if integer i is: (i == 0) || (i == pow(2,x))
   or calculates 2 to the power of x such that (pow(2,x) >= i) && (pow(2,x-1) < i).

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/math/int/power2.h
    Header file of <Intop-Power2>.

   file: C-kern/math/int/power2.c
    Implementation file <Intop-Power2 impl>.
*/
#ifndef CKERN_MATH_INT_POWER2_HEADER
#define CKERN_MATH_INT_POWER2_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_power2
 * Tests <ispowerof2_int> and <makepowerof2_int>. */
int unittest_math_int_power2(void);
#endif


// struct: int_t

// group: query

/* function: ispowerof2_int
 * Determines if argument is of power of 2 or 0.
 * An integer is of power two if exactly one bit is set.
 * This function is implemented as a generic function for all integer types.
 *
 * Parameter:
 * i - Integer argument to be testet
 *
 * Every integer of the form (binary) 0..00100..0 is of power of 2 (only one bit is set).
 * If a second bit is set i (==01000100) => (i-1) == 01000011 => (i&(i-1)==01000000) != 0 */
int ispowerof2_int(unsigned i);

// group: compute

/* function: makepowerof2_int
 * Returns value >= i which is a power of 2 or the value 0.
 * This function is implemented as a generic function for all integer types.
 *
 * Parameter:
 * i - The argument which is transformed into a power of 2 value and returned.
 *
 * The returned value is the same as argument i in case of
 * 1. *i* == 0
 * 2. <ispowerof2_int>(*i*) is true
 * 3. the next higher power of 2 value can not be represented by this type of integer
 *
 * Algorithm:
 * Set all bits right of highest to one (00011111111) and then add +1 (00100000000). */
unsigned makepowerof2_int(unsigned i);

/* function: alignpower2_int
 * Returns value >= i which is a multiple of size.
 *
 * Unchecked Precondition:
 * 1 == ispowerof2_int(size) && size <= i*/
unsigned alignpower2_int(unsigned i, unsigned size);

// section: inline implementation

/* define: alignpower2_int
 * Implements <int_t.alignpower2_int> as a generic function. */
#define alignpower2_int(i, size) \
         (  __extension__ ({                       \
            typeof(i) _m = (typeof(i))((size)-1);  \
            (typeof(i)) (((i) + _m) & ~_m);        \
         }))

/* define: ispowerof2_int
 * Implements <int_t.ispowerof2_int> as a generic function. */
#define ispowerof2_int(i) \
         (!( (typeof(i)) (((i)-1) & (i)) ))

/* define: makepowerof2_int
 * Implements <int_t.makepowerof2_int> as a generic function. */
#define makepowerof2_int(i) \
         (  __extension__ ({                                            \
            typedef typeof(i) _int;                                     \
            /* unsigned ! */                                            \
            static_assert( ((typeof(i))-1) > 0, );                      \
            /* long long maximum support */                             \
            static_assert( sizeof(i) <= sizeof(long long), );           \
            _int _i = (i);                                              \
            if (  !ispowerof2_int(_i)                                   \
                  && _i < (_int) (_i << 1) ) {                          \
               _i = (_int) (_i << 1);                                   \
               if (sizeof(unsigned) >= sizeof(i)) {                     \
                  _i = (_int)((1u << (bitsof(unsigned)-1))              \
                               >> __builtin_clz((unsigned)_i));         \
               } else if (sizeof(long) >= sizeof(i)) {                  \
                  _i = (_int)((1ul << (bitsof(unsigned long)-1))        \
                               >> __builtin_clzl((unsigned long)_i));   \
               } else {                                                 \
                  _i = (_int)((1ull << (bitsof(unsigned long long)-1))  \
                               >> __builtin_clzll(_i));                 \
               }                                                        \
            }                                                           \
            _i;                                                         \
         }))

#endif
