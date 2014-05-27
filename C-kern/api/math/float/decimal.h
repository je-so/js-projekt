/* title: Decimal
   Implements a fixed size decimal nummber.

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

   file: C-kern/api/math/float/decimal.h
    Header file <Decimal>.

   file: C-kern/math/float/decimal.c
    Implementation file <Decimal impl>.
*/
#ifndef CKERN_MATH_FLOAT_DECIMAL_HEADER
#define CKERN_MATH_FLOAT_DECIMAL_HEADER

// forward
struct cstring_t ;

/* tyepdef: struct decimal_t
 * Export <decimal_t>. */
typedef struct decimal_t               decimal_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_float_decimal
 * Test interface of <decimal_t>. */
int unittest_math_float_decimal(void) ;
#endif


/* struct: decimal_t
 * Represents a decimal number with at most 1143 decimal digits.
 * The decimal exponent must be in the range [-<expmax_decimal>..+<expmax_decimal>],
 * where <expmax_decimal> returns the value <digitsperint_decimal>*INT16_MAX (==9*32767).
 *
 * Exponent:
 * The exponent is always aligned to the next lower multiple of <digitsperint_decimal>.
 * The reason for the alignment is to allow binary operations on decimals without any
 * need for shifting the decimal number of one operand by a power of 10.
 *
 * Internal representation:
 * The number is represented internally as a number of integer <digits> and a <exponent>.
 * Every integer digit contains a value in the range [0..999999999]. The representation of the
 * number is to the base 1000000000.
 * The value of a <decimal_t> number can be determined with:
 * > double calc_value(decimal_t * dec) {
 * >    // it is possible that double over- or underflows.
 * >    double   value = 0 ;
 * >    uint32_t base  = pow(10, digitsperint_decimal()) ;
 * >    for (uint8_t i = 0; i < size_decimal(dec); ++i)
 * >       value += dec->digits[i] * pow(base, i) ;
 * >    return pow(10, exponent_decimal(dec)) * value * sign_decimal(dec) ;
 * > }
 *
 * Result of an operation:
 * The *result* parameter of an operation is reallocated in case
 * the preallocated size is not big enough and if it is of type pointer to pointer to <decimal_t>.
 * In case of an error the result parameter will be either left untouched, contains the correct
 * result, or it will be cleared.
 * Clearing the result is necessary if it is computed digit by digit and an error occurs
 * before the last digit could be computed.
 *
 * Returned Error Codes:
 * EOVERFLOW - This error can have following reasons.
 *             The value of the decimal exponent is outside the range of [-<expmax_decimal>..+<expmax_decimal>].
 *             The number of digits to represent the result exactly is bigger than the value returned from <nrdigitsmax_decimal>,
 *             except for the division which has an extra parameter to round the result to a certain precision.
 * ENOMEM    - Every operation which produces more decimal digits than preallocted needs to reallocate
 *             the result parameter. If this reallocation fails ENOMEM is returned and the result
 *             is not changed.
 * EINVAL    - One input parameter is not valid (syntax error in input string in <setfromchar_decimal> or
 *             precision parameter either 0 or bigger than <nrdigitsmax_decimal> in <div_decimal> ...).
 * */
struct decimal_t {
   /* variable: size_allocated
    * The number of allocated integers in <digits>.
    * This number is always lower or equal than 0x7f (INT8_MAX).
    * The reason is that <sign_and_used_digits> is signed and therefore
    * only 7 bits can be used to represent the magnitude. */
   uint8_t     size_allocated;
   /* variable: sign_and_used_digits
    * The sign bit (0x80) of this variable is the sign of the number.
    * The lower 7 bits give the number of used integers in <digits>.
    * One such integer represents 9 decimal digits. */
   uint8_t     sign_and_used_digits;
   /* variable: exponent
    * The exponent of base number 1000000000.
    * To get a decimal exponent multiply this number with <digitsperint_decimal>.
    * The value of the exponentiation is calculated as pow(10,<digitsperint_decimal>*exponent). */
   int16_t     exponent ;
   /* variable: digits
    * 9 decimal digits encoded as integer are stored per digit (uint32_t).
    * The value of every digit is in the range [0 .. 999999999].
    * The value of digits[i+1] must be multiplied with pow(10, digitsperint_decimal()) before
    * it can be added to digits[i].
    * Only digits[i] with i < abs(sign_and_used_digits) are valid. */
   uint32_t    digits[/*0..size_allocated-1*/] ;
} ;

// group: lifetime

/* function: new_decimal
 * Allocates a new decimal number.
 * The value is initialized to zero.
 * The maximum number of supported decimal digits is 1143 (9 * INT8_MAX).
 * The newly allocated object always can represent at least 9 decimal digits (<digitsperint_decimal>).
 * To uniquely represent a double you need at least 17 digits. For a float you
 * need in same rare cases 9 digits. */
int new_decimal(/*out*/decimal_t ** dec, uint32_t nrdigits) ;

/* function: newcopy_decimal
 * Allocates a new decimal number and initializes its value with a copy of *copyfrom*.
 * The newly allocated number is returned in parameter *dec*. */
int newcopy_decimal(/*out*/decimal_t ** dec, const decimal_t * copyfrom) ;

/* function: delete_decimal
 * Frees any allocated memory and sets (*dec) to 0. */
int delete_decimal(decimal_t ** dec) ;

// group: query

/* function: bitsperint_decimal
 * Returns the size in bytes of the internally used integer type.
 * This is an implementation detail you should not be concerned about. */
uint8_t bitsperint_decimal(void) ;

/* function: cmp_decimal
 * Compares two decimal numbers and returns result.
 *
 * Returns:
 * -1     - ldec is lower than rdec
 * 0      - both numbers are equal
 * +1     - ldec is greater than rdec
 */
int cmp_decimal(const decimal_t * ldec, const decimal_t * rdec) ;

/* function: cmpmagnitude_decimal
 * Compares magnitude of two decimal numbers returns result.
 * This function is the same as <cmp_decimal> except that the sign
 * of both numbers is considered to be positive.
 *
 * Returns:
 * -1     - Absolute value of ldec is lower than absolute value of rdec
 * 0      - Both absolute values are equal
 * +1     - Absolute value of ldec is greater than absolute value of rdec
 */
int cmpmagnitude_decimal(const decimal_t * ldec, const decimal_t * rdec) ;

/* function: digitsperint_decimal
 * Returns the number of decimal digits stored internally per integer.
 * This is an implementation detail you should not be concerned about. */
uint8_t digitsperint_decimal(void) ;

/* function: exponent_decimal
 * Returns the decimal exponent of the number.
 * The returned value is in the range [-<expmax_decimal>..+<expmax_decimal>]. */
int32_t exponent_decimal(const decimal_t * dec) ;

/* function: expmax_decimal
 * Returns the maximum magnitude of the decimal exponent.
 * The returned value is <digitsperint_decimal>*INT16_MAX (==9*32767). */
int32_t expmax_decimal(void) ;

/* function: first9digits_decimal
 * Returns the 9 most significant decimal digits with correct sign.
 * The exponent is returned in *decimal_exponent*. */
int32_t first9digits_decimal(decimal_t * dec, int32_t * decimal_exponent) ;

/* function: first18digits_decimal
 * Returns the 18 most significant decimal digits with correct sign.
 * The exponent is returned in *decimal_exponent*. */
int64_t first18digits_decimal(decimal_t * dec, int32_t * decimal_exponent) ;

/* function: isnegative_decimal
 * Returns true if dec is negative else false. */
bool isnegative_decimal(const decimal_t * dec) ;

/* function: iszero_decimal
 * Returns true if *dec* has value 0 else false. */
bool iszero_decimal(const decimal_t * dec) ;

/* function: nrdigits_decimal
 * Returns the number of decimal digits stored. */
uint16_t nrdigits_decimal(const decimal_t * dec) ;

/* function: nrdigitsmax_decimal
 * Returns the maximum number of decimal digits supported by a <decimal_t>. */
uint16_t nrdigitsmax_decimal(void) ;

/* function: sign_decimal
 * Returns -1, 0 or +1 if dec is negative, zero or positive. */
int sign_decimal(const decimal_t * dec);

/* function: size_decimal
 * Returns number of integers needed to store all decimal digits.
 * Every integer can store up to <digitsperint_decimal> decimal digits. */
uint8_t size_decimal(decimal_t * dec) ;

/* function: sizemax_decimal
 * Returns the maximum number of integers which can be allocated. */
uint8_t sizemax_decimal(void) ;

/* function: tocstring_decimal
 * Converts *dec* to <cstring_t>.
 * The result is returned in the already initialized parameter *cstr*.
 * If during conversion an error occurs cstr is set to the empty string.
 */
int tocstring_decimal(const decimal_t * dec, struct cstring_t * cstr) ;

// group: assign

/* function: clear_decimal
 * Sets the value to 0. */
void clear_decimal(decimal_t * dec) ;

/* function: copy_decimal
 * Copies the value from *copyfrom* to *dec*.
 * If *dec* is not big enough it is reallocated. In case the reallocation
 * fails ENOMEM is returned. */
int copy_decimal(decimal_t *restrict* dec, const decimal_t * restrict copyfrom) ;

/* function: setfromint32_decimal
 * Sets decimal to *value* mutliplied by pow(10,*decimal_exponent*).
 * See <decimal_t> for a description of the error codes. */
int setfromint32_decimal(decimal_t *restrict* dec, int32_t value, int32_t decimal_exponent) ;

/* function: setfromint64_decimal
 * Sets decimal to *value* mutliplied by pow(10,*decimal_exponent*).
 * See <decimal_t> for a description of the error codes. */
int setfromint64_decimal(decimal_t *restrict* dec, int64_t value, int32_t decimal_exponent) ;

/* function: setfromfloat_decimal
 * Sets decimal to floating point *value*.
 * See <decimal_t> for a description of the error codes. */
int setfromfloat_decimal(decimal_t *restrict* dec, float value) ;

/* function: setfromchar_decimal
 * Sets decimal *dec* to the value represented in decimal/scientific notation.
 * The character string must be of the format "[-]ddd.ddde±dd" where d is a digit from "0"-"9".
 * The base 10 exponent value is started with e followed by an optional sign and one or more digots.
 * The parsing of the string takes no localization into account. The string is considered to be in
 * utf8 encoding. */
int setfromchar_decimal(decimal_t *restrict* dec, const size_t nrchars, const char decimalstr[nrchars]) ;

// group: unary operations

/* function: negate_decimal
 * Inverts the sign of the number.
 * A positive signed number becomes negative. A negative one positive and zero keeps zero. */
void negate_decimal(decimal_t * dec) ;

/* function: setnegative_decimal
 * Changes the sign to be negative.
 * If the sign is zero or already negative nothing is changed. */
void setnegative_decimal(decimal_t * dec) ;

/* function: setpositive_decimal
 * Changes the sign to be positive.
 * If the sign is already positive nothing is changed. */
void setpositive_decimal(decimal_t * dec) ;

// group: ternary operations

/* function: add_decimal
 * Adds the last two parameters and returns the sum in the first.
 * See <decimal_t> for a discussion of the *result* parameter. */
int add_decimal(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec) ;

/* function: sub_decimal
 * Subracts the third from the second parameter and returns the difference in the first.
 * See <decimal_t> for a discussion of the *result* parameter. */
int sub_decimal(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec) ;

/* function: mult_decimal
 * Multiplies the two last parameters and returns the product in the first.
 * See <decimal_t> for a discussion of the *result* parameter. */
int mult_decimal(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec) ;

/* function: div_decimal
 * Divides parameter *ldec* by *rdec* and returns the quotient in the first.
 * See <decimal_t> for a discussion of the *result* parameter.
 * The last parameter *result_size* gives the number of decimal digits of the result
 * in multiple of <digitsperint_decimal>. A value of 1 results in size_decimal(*result) returning 1.
 * If this value is bigger than <sizemax_decimal> it is silently set to <sizemax_decimal>.
 *
 * Examples:
 * 1. Calculating »1/3« with nrdigits_result==5 results in »0.33333«.
 * 2. Calculating »12345/1« with nrdigits_result=4 results in »1234.0«.
 * 3. Calculating »12355/1« with nrdigits_result=4 results in »1236.0«.
 *
 * Error:
 * It is necessary to compute the result of the division to determine if a range overflow
 * of the decimal exponent occurs for the given precision. In this case the partially
 * computed result is cleared before return. (TODO: check if this is necessary)
 *
 * Rounding (round-half-to-even):
 * If the digit after nrdigits_result is less than 5 the returned value is rounded down.
 * If the digit after nrdigits_result is greater than 5 (or equal to 5 and the digits after
 * it are not 0) the returned value is rounded up.
 * If the digit after nrdigits_result is equal to 5 and all following digits are 0 the returned
 * value is only rounded up if the last digit in result is odd. */
int div_decimal(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec, uint8_t result_size) ;

/* function: divi32_decimal
 * Divides parameter *ldec* by *rdivisor* and returns quotient in the first.
 * See <div_decimal> for a description of the last parameter.
 * The absolute value of *rdivisor* must be in the range [1..pow(10,digitsperint_decimal())]. */
int divi32_decimal(decimal_t *restrict* result, const decimal_t * ldec, int32_t rdivisor, uint8_t result_size) ;


// section: inline implementation

/* define: bitsperint_decimal
 * Implements <decimal_t.bitsperint_decimal>. */
#define bitsperint_decimal()           ((uint8_t)(bitsof(((decimal_t*)0)->digits[0])))

/* define: digitsperint_decimal
 * Implements <decimal_t.digitsperint_decimal>. */
#define digitsperint_decimal()         ((uint8_t)(9 * bitsperint_decimal()/32))

/* define: expmax_decimal
 * Implements <decimal_t.expmax_decimal>. */
#define expmax_decimal(dec)            ((int32_t)INT16_MAX * digitsperint_decimal())

/* define: exponent_decimal
 * Implements <decimal_t.exponent_decimal>. */
#define exponent_decimal(dec)          ((int32_t)(dec)->exponent * digitsperint_decimal())

/* define: isnegative_decimal
 * Implements <decimal_t.isnegative_decimal>. */
#define isnegative_decimal(dec)        (((dec)->sign_and_used_digits & 0x80) != 0)

/* define: iszero_decimal
 * Implements <decimal_t.iszero_decimal>. */
#define iszero_decimal(dec)            ((dec)->sign_and_used_digits == 0)

/* define: negate_decimal
 * Implements <decimal_t.negate_decimal>. */
#define negate_decimal(dec) \
         do {                                   \
            decimal_t * _d = (dec);             \
            _d->sign_and_used_digits =          \
               (uint8_t) (                      \
                  _d->sign_and_used_digits      \
                  ^ (_d->sign_and_used_digits   \
                     ? 0x80 : 0));              \
         } while (0)

/* define: nrdigitsmax_decimal
 * Implements <decimal_t.nrdigitsmax_decimal>. */
#define nrdigitsmax_decimal()          ((uint16_t)(digitsperint_decimal() * sizemax_decimal()))

/* define: setnegative_decimal
 * Implements <decimal_t.setnegative_decimal>. */
#define setnegative_decimal(dec) \
         do {                                   \
            decimal_t * _d = (dec);             \
            _d->sign_and_used_digits =          \
               (uint8_t) (                      \
                  _d->sign_and_used_digits      \
                  | (_d->sign_and_used_digits   \
                     ? 0x80 : 0));              \
         } while (0)

/* function: setpositive_decimal
 * Implements <decimal_t.setpositive_decimal>. */
#define setpositive_decimal(dec) \
         do {                                   \
            decimal_t * _d = (dec);             \
            _d->sign_and_used_digits =          \
               (uint8_t) (                      \
                  _d->sign_and_used_digits      \
                  & 0x7f);                      \
         } while (0)

/* define: sign_decimal
 * Implements <decimal_t.sign_decimal>. */
#define sign_decimal(dec) \
         ( __extension__ ({                        \
            const decimal_t * _d = (dec);          \
            (  0 < _d->sign_and_used_digits        \
               && _d->sign_and_used_digits < 0x80) \
            - (_d->sign_and_used_digits >= 0x80);  \
         }))

/* define: size_decimal
 * Implements <decimal_t.size_decimal>. */
#define size_decimal(dec)              ((uint8_t)((dec)->sign_and_used_digits & 0x7f))

/* define: sizemax_decimal
 * Implements <decimal_t.sizemax_decimal>. */
#define sizemax_decimal()              ((uint8_t)0x7f)


#endif
