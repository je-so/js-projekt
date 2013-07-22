/* title: BigInteger

   Defines interface for arbitrary precision integers.

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

   file: C-kern/api/math/int/bigint.h
    Header file <BigInteger>.

   file: C-kern/math/int/bigint.c
    Implementation file <BigInteger impl>.
*/
#ifndef CKERN_MATH_INT_BIGINT_HEADER
#define CKERN_MATH_INT_BIGINT_HEADER

/* typedef: struct bigint_t
 * Export <bigint_t>. Arbitrary precision integer. */
typedef struct bigint_t                bigint_t ;

/* typedef: struct bigint_fixed_t
 * Export <bigint_fixed_t>. Fixed precision integer. */
typedef struct bigint_fixed_t          bigint_fixed_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_biginteger
 * Tests interface of <bigint_t>. */
int unittest_math_int_biginteger(void) ;
#endif


/* struct: bigint_t
 * Big integer object type.
 * Supports arbitrary precision integer arithmetic.
 * Only basic arithmetic operations are supported.
 *
 * Result of an operation:
 * The *result* parameter of an operation is reallocated in case
 * the preallocated size is not big enough and if it is of type pointer to pointer to <bigint_t>.
 * In case of an error the result parameter will be either left untouched, contains the correct
 * result, or it will be cleared.
 * Clearing the result is necessary if it is computed digit by digit and an error occurs
 * before the last digit could be computed.
 *
 * Returned Error Codes:
 * EOVERFLOW - If an operation needs more digits than <nrdigitsmax_bigint> to represent the
 *             result accurately or if the value of <bigint_t->exponent> does not fit into 16 bit
 *             this error is returned.
 * ENOMEM    - Every operation which produces more decimal digits than preallocted needs to reallocate
 *             the resulting <bigint_t>. If this reallocation fails ENOMEM is returned and the result
 *             is not changed.
 * */
struct bigint_t {
   /* variable: allocated_digits
    * The number of allocated digits. The number of elements of the <digits> array. */
   uint16_t    allocated_digits ;
   /* variable: sign_and_used_digits
    * The absolute value of this number is the number of valid digits.
    * If this value is 0 the value of <bigint_t> is 0.
    * The sign of <sign_and_used_digits> encodes the sign of this big integer. */
   int16_t     sign_and_used_digits ;
   /* variable: exponent
    * The exponent with base »2 raised to 32«. The exponent gives the number of digits
    * the number has to be shifted left (multiplied). After the shift the value of the
    * lowest exponent digits are zero. The exponent is an optimization to save storage
    * for numbers with a lot of trailing zeros (after conversions from double to bigint). */
   uint16_t    exponent ;
   /* variable: digits
    * The values of digits. Only the first abs(<sign_and_used_digits>) digits are valid.
    * The array is of size <allocated_digits>.
    * Every digit encodes the value
    * > (digit[digit_pos] << (32 * (digit_pos + exponent))) */
   uint32_t    digits[/* 0 <= digit_pos < abs(sign_and_used_digits*/] ;
} ;

// group: lifetime

/* function: new_bigint
 * Allocates a new big integer object.
 * The new big integer has at least 128 bits (4 digits) or nrdigits which one is higher.
 * The big integer value is initialized to zero.
 * The maximum supported value of nrdigits can be obtained with a call to <nrdigitsmax_bigint>. */
int new_bigint(/*out*/bigint_t ** big, uint32_t nrdigits) ;

/* function: newcopy_bigint
 * Makes a copy from a big integer object.
 * The newly allocated big integer is returned in parameter *big*.
 * It has the same value as the number supplied in the second parameter *copyfrom*. */
int newcopy_bigint(/*out*/bigint_t ** big, const bigint_t * copyfrom) ;

/* function: delete_bigint
 * Frees any allocated memory and sets (*big) to 0. */
int delete_bigint(bigint_t ** big) ;

// group: query

/* function: cmp_bigint
 * Compares two big integers and returns -1,0 or +1.
 *
 * Returns:
 * -1  - lbig is lower than rbig
 * 0   - both numbers are equal
 * +1  - lbig is greater than rbig */
int cmp_bigint(const bigint_t * lbig, const bigint_t * rbig) ;

/* function: cmpmagnitude_bigint
 * Compares magnitude of two big integers and returns -1,0 or +1.
 * This function is the same as <cmp_bigint> except that the sign
 * of both numbers is considered to be positive.
 *
 * Returns:
 * -1  - Absolute value of lbig is lower than absolute value of rbig
 * 0   - Both absolute values are equal
 * +1  - Absolute value of lbig is greater than absolute value of rbig */
int cmpmagnitude_bigint(const bigint_t * lbig, const bigint_t * rbig) ;

/* function: bitsperdigit_bigint
 * Returns the number of bits used to represent one digit.
 * The value should be either 32 or 64 depending on the
 * type of architecture. */
uint8_t bitsperdigit_bigint(void) ;

/* function: exponent_bigint
 * The number of trailing zero digits which are not explicitly stored.
 * If the number is 0 then the exponent must also be 0. */
uint16_t exponent_bigint(const bigint_t * big) ;

/* function: firstdigit_bigint
 * Returns most significant digit of the number.
 * If *big* is zero the returned value is 0 else it is always number greater zero. */
uint32_t firstdigit_bigint(const bigint_t * big) ;

/* function: isnegative_bigint
 * Returns true if *big* is negative else false. */
bool isnegative_bigint(const bigint_t * big) ;

/* function: iszero_bigint
 * Returns true in case big has value 0 else false. */
bool iszero_bigint(const bigint_t * big) ;

/* function: nrdigits_bigint
 * Returns the number of stored digits (32 bit words) of the big integer.
 * The value of a single digit is determined by
 * > digit[digit_pos] << (32 * (digit_pos + exponent_bigint(big))) */
uint16_t nrdigits_bigint(const bigint_t * big) ;

/* function: nrdigitsmax_bigint
 * Returns the maximum number of supported digits stored in a big integer. */
uint16_t nrdigitsmax_bigint(void) ;

/* function: size_bigint
 * Returns the sum of <exponent_bigint> and <nrdigits_bigint>.
 * This sum must be smaller or equal to INT16_MAX in case a number is divided
 * with a call to <divmodui32_bigint>. */
uint32_t size_bigint(const bigint_t * big) ;

/* function: sign_bigint
 * Returns -1, 0 or +1 if big is negative, zero or positive. */
int sign_bigint(const bigint_t * big) ;

/* function: todouble_bigint
 * Converts a big integer value into a double.
 * The value +/- INFINITY is returned if *big* cannot be represented by a double.
 * Only the first (highest value) 53 bits are used (if double is IEEE 754 conformant)
 * for the mantissa and their offset encoded as exponent.
 * The other bits are discarded and the precision is lost. Only if all bits except for
 * the first 53 bits are zero no precision is lost.
 *
 * Rounding mode:
 * The default rounding mode of uint64_t -> long double is used, which is architecture dependent.
 * On x84 fpu the uint64_t is converted into long double without loss and then rounded according
 * to the current rounding mode of the fpu.
 * */
double todouble_bigint(const bigint_t * big) ;

// group: assign

/* function: clear_bigint
 * Sets the value to 0. */
void clear_bigint(bigint_t * big) ;

/* function: copy_bigint
 * Copies the value from *copyfrom* to *big*.
 * If *big* is not big enough it is reallocated. In case the reallocation
 * fails ENOMEM is returned. */
int copy_bigint(bigint_t *restrict* big, const bigint_t * restrict copyfrom) ;

/* function: setfromint32_bigint
 * Sets the value of big to the value of the provided parameter. */
void setfromint32_bigint(bigint_t * big, int32_t value) ;

/* function: setfromuint32_bigint
 * Sets the value of big to the positive value of the provided parameter . */
void setfromuint32_bigint(bigint_t * big, uint32_t value) ;

/* function: setfromuint64_bigint
 * Sets the value of big to the positive value of the provided parameter . */
void setfromuint64_bigint(bigint_t * big, uint64_t value) ;

/* function: setfromdouble_bigint
 * Sets the value of big to the integer value of the provided parameter.
 * The fractional part is discarded.
 * If the integer part is zero the value is set to 0.
 * In case parameter *value* is NAN or INFINITY the error EINVAL is returned.
 * The assigned <bigint_t> must have at least 3 allocated integer digits. */
int setfromdouble_bigint(bigint_t * big, double value) ;

/* function: setbigfirst_bigint
 * Sets the value of big integer from an array of integers.
 * The integer *numbers[0]* is considered the most significant digit
 * and *numbers[size-1]* the least significant digit of the integer value.
 * If parameter exponent is set to a value > 0 it is considered the number of trailing zero digits
 * which are not explicitly stored in the *numbers* array.
 * The parameter sign should be set to either +1 or -1. */
int setbigfirst_bigint(bigint_t * restrict * big, int sign, uint16_t size, const uint32_t numbers[size], uint16_t exponent) ;

/* function: setlittlefirst_bigint
 * Sets the value of big integer from an array of integers.
 * The integer *numbers[size-1]* is considered the most significant digit
 * and *numbers[0]* the least significant digit of the integer value.
 * If parameter exponent is set to a value > 0 it is considered the number of trailing zero digits
 * which are not explicitly stored in the *numbers* array.
 * The parameter sign should be set to either +1 or -1. */
int setlittlefirst_bigint(bigint_t * restrict * big, int sign, uint16_t size, const uint32_t numbers[size], uint16_t exponent) ;

// group: unary operations

/* function: clearfirstdigit_bigint
 * Sets the first digit to 0 and decrements the number of digits.
 * A zero number is not changed. If the next digit is also 0
 * the number of digits is decremented again until a digit is found which is not null.
 * If all remaining digits are 0 *big* is set to 0. */
void clearfirstdigit_bigint(bigint_t * big) ;

/* function: negate_bigint
 * Inverts the sign of the number.
 * A positive signed number becomes negative. A negative one positive and zero keeps zero. */
void negate_bigint(bigint_t * big) ;

/* function: removetrailingzero_bigint
 * Removes all trailing digits which are 0.
 * This optimization removes the least sign. digit and increments the exponent by one
 * until the least significant digit is not 0 or the exponent reached its maximum value. */
void removetrailingzero_bigint(bigint_t * big) ;

/* function: setnegative_bigint
 * Changes the sign to be negative.
 * If the sign is zero or already negative nothing is changed. */
void setnegative_bigint(bigint_t * big) ;

/* function: setpositive_bigint
 * Changes the sign to be positive.
 * If the sign is already positive nothing is changed. */
void setpositive_bigint(bigint_t * big) ;

// group: binary operations

/* function: shiftleft_bigint
 * Multiplies the number by pow(2, *shift_count*).
 * Shifting one position left is the same as multiplying by two.
 * If *shift_count* is a multiple of <bitsperdigit_bigint> then only
 * <bigint_t.exponent> is incremented.
 * If the exponent overflows EOVERFLOW is returned. */
int shiftleft_bigint(bigint_t ** result, uint32_t shift_count) ;

/* function: shiftright_bigint
 * Divides the number by pow(2, *shift_count*).
 * Shifting one position right is the same as dividing by two.
 * If *shift_count* is a multiple of <bitsperdigit_bigint> then
 * only <bigint_t.exponent> is decremented until it becomes 0.
 * The *shift_count* least significant bits get lost. If *result*
 * is smaller than pow(2, *shift_count*) the value 0 is returned.  */
int shiftright_bigint(bigint_t ** result, uint32_t shift_count) ;

// group: ternary operations

/* function: add_bigint
 * Adds the last two parameters and returns the sum in the first.
 * See <bigint_t> for a discussion of the *result* parameter. */
int add_bigint(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig) ;

/* function: sub_bigint
 * Subtracts the third from the 2nd parameter and returns the difference in the first.
 * See <bigint_t> for a discussion of the *result* parameter. */
int sub_bigint(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig) ;

/* function: div_bigint
 * Divides dividend lbig by divisor rbig.
 * See <bigint_t> for a discussion of the *result* parameter. */
int div_bigint(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig) ;

/* function: divmod_bigint
 * Divides lbig by rbig and computes lbig modulo rbig.
 * See <bigint_t> for a discussion of the *result* parameter. */
int divmod_bigint(bigint_t *restrict* divresult, bigint_t *restrict* modresult, const bigint_t * lbig, const bigint_t * rbig) ;

/* function: divmodui32_bigint
 * Divides lbig by divisor of type uint32_t and computes modulo.
 * See <bigint_t> for a discussion of the *result* parameter. */
int divmodui32_bigint(bigint_t *restrict* divresult, bigint_t *restrict* modresult, const bigint_t * lbig, const uint32_t divisor) ;

/* function: divui32_bigint
 * Divides lbig by divisor of type uint32_t.
 * See <bigint_t> for a discussion of the *result* parameter. */
int divui32_bigint(bigint_t *restrict* result, const bigint_t * lbig, const uint32_t divisor) ;

/* function: mod_bigint
 * Computes lbig modulo rbig.
 * See <bigint_t> for a discussion of the *result* parameter. */
int mod_bigint(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig) ;

/* function: mult_bigint
 * Multiplies two big integers and returns the result.
 * See <bigint_t> for a discussion of the *result* parameter. */
int mult_bigint(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig) ;

/* function: multui32_bigint
 * Multiplies the big integer with a 32 bit unsigned int.
 * See <bigint_t> for a discussion of the *result* parameter. */
int multui32_bigint(bigint_t *restrict* result, const bigint_t * lbig, const uint32_t factor) ;


/* struct: bigint_fixed_t
 * Same as <bigint_t> with a fixed size.
 * Use this type for static storage and initialization.
 * Use <bigint_fixed_DECLARE> to declare a <bigint_t> type of fixed size. */
struct bigint_fixed_t ;

// group: lifetime

/* define: bigint_fixed_INIT
 * Initializes a static object of type <bigint_fixed_t>.
 *
 * Parameters:
 * nrdigits - The magnitude of nrdigits gives the number of 32 bit digits of the whole number.
 *            The sign of nrdigits is the sign of the big integer value.
 * exponent - The exponent of type uint16_t whose value describes the number of trailing zero digits
 *            which are not stored explicitly in the big integer.
 * ...      - A list of 32-bit digits with nrdigits elements. The least significant digit comes first.
 *            The last digit is the most significant one.
 *
 * Example:
 * > static bigint_fixed_DECLARE(4) s_bigint4 =
 * >        bigint_fixed_INIT(-4, 0, 1, 2, 3, 4 } ;
 * This example defines a 128 bit integer with the value -0x00000004000000030000000200000001.
 * */
#define bigint_fixed_INIT(nrdigits, exponent, ...)   \
   { 0, nrdigits, exponent, { __VA_ARGS__ } }

// group: generic

/* define: bigint_fixed_DECLARE
 * Declares type <bigint_fixedNR_t>.
 * Replace NR with the value of the parameter *nrdigits*.
 * Use the declared type to reserve and initilize static storage
 * for objects of type <bigint_t>. The parameter *nrdigits* gives the
 * number of preallocated digits. */
#define bigint_fixed_DECLARE(nrdigits)                \
   struct CONCAT(CONCAT(bigint_fixed, nrdigits),_t){  \
      uint16_t    allocated_digits ;                  \
      int16_t     sign_and_used_digits ;              \
      uint16_t    exponent ;                          \
      uint32_t    digits[nrdigits] ;                  \
   }


// section: inline implementation

/* define: bitsperdigit_bigint
 * Implements <bigint_t.bitsperdigit_bigint>. */
#define bitsperdigit_bigint()          ((uint8_t)bitsof(((bigint_t*)0)->digits[0]))

/* define: div_bigint
 * Implements <bigint_t.div_bigint>. */
#define div_bigint(result, lbig, rbig) (divmod_bigint(result, 0, lbig, rbig))

/* define: divui32_bigint
 * Implements <bigint_t.divui32_bigint>. */
#define divui32_bigint(result, lbig, divisor)   (divmodui32_bigint(result, 0, lbig, divisor))

/* define: exponent_bigint
 * Implements <bigint_t.exponent_bigint>. */
#define exponent_bigint(big)           ((big)->exponent)

/* define: firstdigit_bigint
 * Implements <bigint_t.firstdigit_bigint>. */
#define firstdigit_bigint(big)         ((big)->sign_and_used_digits ? big->digits[nrdigits_bigint(big)-1] : 0)

/* define: isnegative_bigint
 * Implements <bigint_t.isnegative_bigint>. */
#define isnegative_bigint(big)         ((big)->sign_and_used_digits < 0)

/* define: iszero_bigint
 * Implements <bigint_t.iszero_bigint>. */
#define iszero_bigint(big)             (0 == (big)->sign_and_used_digits)

/* define: mod_bigint
 * Implements <bigint_t.mod_bigint>. */
#define mod_bigint(result, lbig, rbig) (divmod_bigint(0, result, lbig, rbig))

/* define: negate_bigint
 * Implements <bigint_t.negate_bigint>. */
#define negate_bigint(big)             do { (big)->sign_and_used_digits = (int16_t) ( - (big)->sign_and_used_digits) ; } while (0)

/* define: nrdigits_bigint
 * Implements <bigint_t.nrdigits_bigint>. */
#define nrdigits_bigint(big)           ((uint16_t)( (big)->sign_and_used_digits < 0 ? - (big)->sign_and_used_digits : (big)->sign_and_used_digits ))

/* define: nrdigitsmax_bigint
 * Implements <bigint_t.nrdigitsmax_bigint>. */
#define nrdigitsmax_bigint()           (0x7fff)

/* define: setnegative_bigint
 * Implements <bigint_t.setnegative_bigint>. */
#define setnegative_bigint(big)        do { (big)->sign_and_used_digits = (int16_t) ( (big)->sign_and_used_digits < 0 ? (big)->sign_and_used_digits : - (big)->sign_and_used_digits ) ; } while (0)

/* function: setpositive_bigint
 * Implements <bigint_t.setpositive_bigint>. */
#define setpositive_bigint(big)        do { (big)->sign_and_used_digits = (int16_t) ( (big)->sign_and_used_digits < 0 ? - (big)->sign_and_used_digits : (big)->sign_and_used_digits ) ; } while (0)

/* define: sign_bigint
 * Implements <bigint_t.sign_bigint>. */
#define sign_bigint(big)               (((big)->sign_and_used_digits > 0) - ((big)->sign_and_used_digits < 0))

/* define: size_bigint
 * Implements <bigint_t.size_bigint>. */
#define size_bigint(big)               ((uint32_t)exponent_bigint(big) + (uint32_t)nrdigits_bigint(big))


#endif
