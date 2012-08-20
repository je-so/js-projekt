/* title: Decimal impl
   Implements interface <Decimal>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/math/float/decimal.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/abs.h"
#include "C-kern/api/math/int/bigint.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/log10.h"
#include "C-kern/api/math/int/sign.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/cstring.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/memory/mm/mmtest.h"
#include "C-kern/api/test/errortimer.h"
#endif

/* typedef: struct decimal_frombigint_t
 * Export <decimal_frombigint_t> into global namespace. */
typedef struct decimal_frombigint_t       decimal_frombigint_t  ;

/* typedef: struct decimal_frombigint_state_t
 * Export <decimal_frombigint_state_t> into global namespace. */
typedef struct decimal_frombigint_state_t decimal_frombigint_state_t ;

/* typedef: struct alignedexpandshift_t
 * Export <alignedexpandshift_t> into global namespace. */
typedef struct alignedexpandshift_t       alignedexpandshift_t ;

/* typedef: struct decimal_divstate_t
 * Export <decimal_divstate_t> into global namespace. */
typedef struct decimal_divstate_t         decimal_divstate_t ;

/* typedef: shiftleft10p_f
 * A function which computes the decimal left shift of digit.
 * The computed return value is:
 * > shiftcarry + (shiftdigit % pow(10,9-x)) * pow(10,x)
 * Where x is number of decimal digits this function shifts to the left.
 * The processing order for a left shift is from least significant first up to
 * most significant last.
 * Before return parameter *shiftcarry* is set to:
 * > shiftdigit / pow(10,9-x)
 * which becomes the next shiftcarry. */
typedef uint32_t                       (* shiftleft10p_f) (/*inout*/uint32_t * shiftcarry, uint32_t shiftdigit) ;


/* struct: alignedexpandshift_t
 * Stores an exponent of DIGITSBASE and a reference to a shift-left function.
 * Call this function to shift all decimal digits left starting
 * from least significant digit. */
struct alignedexpandshift_t {
   int16_t           alignedexp ;
   shiftleft10p_f    shiftleft ;
} ;


/* struct: decimal_frombigint_state_t
 * Holds the modulo result of a division of a <bigint_t>. */
struct decimal_frombigint_state_t {
   /* variable: big
    * A preallocated <bigint_t>. It will hold the modulo result of a division operation.
    * The divisor is referenced with <tabidx>. */
   bigint_t    * big ;
   /* variable: tabidx
    * References a divisor in <s_decimal_powbase>. */
   unsigned    tabidx ;
} ;


/* struct: decimal_frombigint_t
 * Type holding state for conversion from <bigint_t> into <decimal_t>. */
struct decimal_frombigint_t {
   /* variable: state
    * state[0] holds the result of the first division.
    * > ---------------------------------------------------------------
    * > | state[0] | state[1] | ... | 1 == size_bigint(state[X]->big) |
    * > ---------------------------------------------------------------
    * The division process is repeated until the quotient becomes a number in
    * the range [0 .. <DIGITSBASE>-1]. */
   decimal_frombigint_state_t state[7] ;
   bigint_t                   * quotient[2] ;
} ;


/* struct: decimal_divstate_t
 * Used in <div_decimalhelper> to store current state of division computation. */
struct decimal_divstate_t {
   uint64_t       dividend ;
   const uint64_t divisor ;
   uint32_t       nextdigit ;
   uint8_t        loffset ;
   const uint8_t  lsize ;
   const uint8_t  rsize ;
   uint8_t        size ;
   uint32_t       * const ldigits ;
   const uint32_t * const rdigits ;
} ;

// section: decimal_t

// group: variables

static const bigint_fixed_DECLARE(1)   s_decimal_10raised9   = bigint_fixed_INIT(1, 0, 0x3b9aca00) ;
static const bigint_fixed_DECLARE(2)   s_decimal_10raised18  = bigint_fixed_INIT(2, 0, 0xa7640000, 0xde0b6b3) ;
static const bigint_fixed_DECLARE(3)   s_decimal_10raised36  = bigint_fixed_INIT(3, 1, 0xb34b9f10, 0x7bc90715, 0xc097ce) ;
static const bigint_fixed_DECLARE(6)   s_decimal_10raised72  = bigint_fixed_INIT(6, 2, 0xf634e100, 0x31cdcf66, 0x55e946fe, 0x3a4abc89, 0xfbeea1d, 0x90e4) ;
static const bigint_fixed_DECLARE(11)  s_decimal_10raised144 = bigint_fixed_INIT(11, 4, 0x2dc10000, 0xfccaf758, 0x4b28b664, 0x9780697c, 0xb0c5d058, 0x6b17c82d, 0x1ce577b7, 0x4e3104d3, 0xaf8b1036, 0xd469d373, 0x52015ce2) ;
static const bigint_fixed_DECLARE(21)  s_decimal_10raised288 = bigint_fixed_INIT(21, 9, 0xeadd6b81, 0xaff733d, 0xab383823, 0x83ff0d96, 0x247c750, 0xb1ac51bf, 0x6cf9382, 0x827793bd, 0xdf3c40f, 0x7d3b9e1b, 0x7426d5ff, 0x3878e1ea, 0x338693b8, 0x1e4133c0, 0x4ebcf8fd, 0xe92c2430, 0x3c445197, 0x8dffe622, 0x8e7065dd, 0x2b8d45f1, 0x1a44df83) ;
static const bigint_fixed_DECLARE(42)  s_decimal_10raised576 = bigint_fixed_INIT(42, 18, 0x9ddf1701, 0xf292a984, 0x1597b6c4, 0xd0fb3140, 0xe231037e, 0xd5c6eb33, 0xa32d343c, 0xfdd0ce83, 0xac2a909b, 0x103361ea, 0x14d38f80, 0xb3c4af27, 0x40f3d492, 0xc59ce56c, 0xe9a5b505, 0xc3eec8db, 0xd241cca1, 0xa6c46b6e, 0xe53b6c25, 0x3b0676e0, 0x373117f8, 0x10438a3e, 0x9e0bcb40, 0xb7138edc, 0x2740270b, 0x47d901e2, 0x113b15cc, 0x34dee7b7, 0xdf0c483a, 0xbe24abec, 0xcf52a5bf, 0x6e3904cb, 0xb7a19d16, 0xe74c04c9, 0x477e1c03, 0x89296058, 0x4be2cd1f, 0x16cf9026, 0xfc186d93, 0xf31baf3b, 0x25ad9e57, 0x2b20fee) ;

/* variable: s_decimal_powbase
 * Tables of powers of pow(10,9) used to convert <bigint_t>, float, double, into <decimal_t>.
 * The entries s_decimal_powbase[i] are calculated as pow(pow(10,9), i+1).
 */
static const bigint_t          * s_decimal_powbase[7] = {
   (const bigint_t*) &s_decimal_10raised9
   ,(const bigint_t*) &s_decimal_10raised18
   ,(const bigint_t*) &s_decimal_10raised36
   ,(const bigint_t*) &s_decimal_10raised72
   ,(const bigint_t*) &s_decimal_10raised144
   ,(const bigint_t*) &s_decimal_10raised288
   ,(const bigint_t*) &s_decimal_10raised576
} ;

// group: decimal_powbase

/* function: tableindex_decimalpowbase
 * Returns index into table <s_decimal_powbase>.
 * The index is calculated from the size of the <bigint_t>.
 * Check the returned index if it is < nrelementsof(s_decimal_powbase).
 * If it is out of range than bigintsize is too big.
 *
 * Returns:
 * 1. bigintsize <= BIGINT_MAXSIZE
 *    The highest index *ti* is returned for which the following is true.
 *    size_bigint(s_decimal_powbase[ti)) <= bigintsize
 * 2. bigintsize > BIGINT_MAXSIZE
 *    Returns index *ti* overflowing size of s_decimal_powbase.
 * */
static inline unsigned tableindex_decimalpowbase(uint32_t bigintsize)
{
   return log2_int(bigintsize + (bigintsize / 15)) ;
}

/* function: tableindexfromdecsize_decimalpowbase
 * Returns index into table <s_decimal_powbase>.
 * If the preconditions are not met the returned index is either wrong
 * or out of range.
 *
 * Preconditions are not checked.
 *
 * Precondition:
 * 1. decsize >= 1
 * 2. decsize <= 64 (64*9 == 576) */
static inline unsigned tableindexfromdecsize_decimalpowbase(uint32_t decsize)
{
   return log2_int(decsize) ;
}

/* function: decsize_decimalpowbase
 * Returns size of decimal_t needed to represent <s_decimal_powbase>[tableindex]. */
static inline unsigned decsize_decimalpowbase(unsigned tableindex)
{
   return (1u << tableindex) ;
}

// group: decimal_frombigint_t

int new_decimalfrombigint(/*out*/decimal_frombigint_t ** converter)
{
   int err ;
   decimal_frombigint_t * newobj  = 0 ;
   memblock_t           objmem    = memblock_INIT_FREEABLE ;
   const size_t         objsize   = sizeof(decimal_frombigint_t) ;

   static_assert(nrelementsof(s_decimal_powbase) == nrelementsof(newobj->state), "for every table entry a newobj->state entry") ;

   err = MM_RESIZE(objsize, &objmem) ;
   if (err) goto ONABORT ;

   newobj = (decimal_frombigint_t*) objmem.addr ;

   memset(newobj, 0, sizeof(*newobj)) ;

   int maxindex = (int) nrelementsof(newobj->state)-1 ;

   // For all 0 <= i < maxindex: size_bigint(newobj->state[i].big) > size_bigint(newobj->state[i+1].big)
   for (int i = maxindex; i >= 0; --i) {
      err = new_bigint(&newobj->state[maxindex-i].big, size_bigint(s_decimal_powbase[i])) ;
      if (err) goto ONABORT ;
   }

   for (int i = 1; i >= 0; --i) {
      err = new_bigint(&newobj->quotient[i], size_bigint(s_decimal_powbase[maxindex])) ;
      if (err) goto ONABORT ;
   }

   *converter = newobj ;

   return 0 ;
ONABORT:
   if (newobj) {
      for (int i = (int)maxindex; i >= 0; --i) {
         delete_bigint(&newobj->state[i].big) ;
      }
      delete_bigint(&newobj->quotient[0]) ;
      delete_bigint(&newobj->quotient[1]) ;
   }
   MM_FREE(&objmem) ;
   LOG_ABORT(err) ;
   return err ;
}

int delete_decimalfrombigint(decimal_frombigint_t ** converter)
{
   int err ;
   int err2 ;
   decimal_frombigint_t * delobj = *converter ;

   if (delobj) {
      *converter = 0 ;

      err  = delete_bigint(&delobj->quotient[0]) ;
      err2 = delete_bigint(&delobj->quotient[1]) ;
      if (err2) err = err2 ;

      for (int i = (int)nrelementsof(delobj->state)-1; i >= 0; --i) {
         err2 = delete_bigint(&delobj->state[i].big) ;
         if (err2) err = err2 ;
      }

      const size_t objsize   = sizeof(decimal_frombigint_t) ;
      memblock_t   objmem    = memblock_INIT(objsize, (uint8_t*)delobj) ;

      err2 = MM_FREE(&objmem) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: helper

/* define: DIGITSBASE
 * The base of a digit in array <decimal_t.digits>.
 * To calculate the value of a single digit use the formula:
 * > decimal_t * dec = ... ;
 * > double    digit_value[size_decimal(dec)] ;
 * > for (unsigned i = 0; i < size_decimal(dec); ++i)
 * >    digit_value[i] = dec->digits[i] * pow(DIGITSBASE, i) * pow(10, dec->decimal_exponent) ;
 * */
#define DIGITSBASE                     ((uint32_t)1000000000)

/* define: BIGINT_MAXSIZE
 * The number of digits of <bigint_t> which may be converted to <decimal_t> without overflow.
 * A <bigint_t> number whose size is bigger will overflow for sure.
 * A <bigint_t> number whose size is smaller will never overflow. */
#define BIGINT_MAXSIZE                 ((uint32_t)119)

static inline uint32_t objectsize_decimal(uint8_t size_allocated)
{
   return (uint32_t) (sizeof(decimal_t) + sizeof(((decimal_t*)0)->digits[0]) * size_allocated) ;
}

/* function: power10_decimalhelper
 * Calculates pow(10, exponent) for 0 <= exponent <= 9. */
static inline uint32_t power10_decimalhelper(unsigned exponent)
{
   switch(exponent)
   {
   case 0: return 1 ;
   case 1: return 10 ;
   case 2: return 100 ;
   case 3: return 1000 ;
   case 4: return 10000 ;
   case 5: return 100000 ;
   case 6: return 1000000 ;
   case 7: return 10000000 ;
   case 8: return 100000000 ;
   case 9: return 1000000000 ;
   }

   return 0 ;
}

#define DEFINE_SHIFTLEFT10PX_DECIMALHELPER(X, _10PX)     \
   static uint32_t/*divresult*/ CONCAT(CONCAT(shiftleft10p,X),_decimalhelper)(/*inout*/uint32_t * shiftcarry, uint32_t shiftdigit) \
   {                                                     \
      uint32_t shiftval = *shiftcarry ;                  \
      *shiftcarry = shiftdigit / (1000000000 / _10PX) ;  \
      shiftval   += (shiftdigit % (1000000000 / _10PX))  \
                    * _10PX ;                            \
      return shiftval ;                                  \
   }

DEFINE_SHIFTLEFT10PX_DECIMALHELPER(0,1)
DEFINE_SHIFTLEFT10PX_DECIMALHELPER(1,10)
DEFINE_SHIFTLEFT10PX_DECIMALHELPER(2,100)
DEFINE_SHIFTLEFT10PX_DECIMALHELPER(3,1000)
DEFINE_SHIFTLEFT10PX_DECIMALHELPER(4,10000)
DEFINE_SHIFTLEFT10PX_DECIMALHELPER(5,100000)
DEFINE_SHIFTLEFT10PX_DECIMALHELPER(6,1000000)
DEFINE_SHIFTLEFT10PX_DECIMALHELPER(7,10000000)
DEFINE_SHIFTLEFT10PX_DECIMALHELPER(8,100000000)

#undef DEFINE_SHIFTLEFT10PX_DECIMALHELPER

/* function: determinehshiftleft_decimalhelper
 * Returns <shiftleft10p_f> for argument *decimal_exponent_x*.
 * The argument must be a value between 0 and 8.
 * The value 0 calls a function which returns an unshifted number. */
static shiftleft10p_f determinehshiftleft_decimalhelper(uint32_t shiftcount)
{
#define RETURN_RESULT(X)   case X: return &CONCAT(CONCAT(shiftleft10p,X),_decimalhelper)
   switch(shiftcount) {
      RETURN_RESULT(0) ;
      RETURN_RESULT(1) ;
      RETURN_RESULT(2) ;
      RETURN_RESULT(3) ;
      RETURN_RESULT(4) ;
      RETURN_RESULT(5) ;
      RETURN_RESULT(6) ;
      RETURN_RESULT(7) ;
      RETURN_RESULT(8) ;
   }
#undef RETURN_RESULT

   return (shiftleft10p_f)0 ;
}

/* function: alignexponent_decimalhelper
 * Returns the difference between *decimal_exponent* and the aligned exponent.
 * If you subtract the returned value in *exponent_correction* from the decimal
 * exponent you get the aligned exponent. */
static inline int alignexponent_decimalhelper(/*out*/uint32_t * exponent_correction, int32_t decimal_exponent)
{
   if (  abs_int(decimal_exponent) > expmax_decimal()) {
      return EOVERFLOW ;
   }

   int32_t aligndiff = decimal_exponent % digitsperint_decimal() ;
   if (aligndiff < 0) aligndiff += digitsperint_decimal() ;

   *exponent_correction = (uint32_t) aligndiff ;
   return 0 ;
}

static int alignedexpandshift_decimalhelper(/*out*/alignedexpandshift_t * expshift, int32_t decimal_exponent)
{
   int err ;
   uint32_t exponent_correction ;

   err = alignexponent_decimalhelper(&exponent_correction, decimal_exponent) ;
   if (err) return err ;

   expshift->alignedexp = (int16_t) ((decimal_exponent - (int32_t)exponent_correction) / digitsperint_decimal()) ;
   expshift->shiftleft  = determinehshiftleft_decimalhelper(exponent_correction) ;

   return 0 ;
}

/* function: allocate_decimalhelper
 * Does the real allocation / reallocation.
 * In case parameter *dec* does not point to a NULL pointer, it is considered
 * to point to a valid reference to a <decimal_t> previously allocated by this function.
 * In this case a reallocation is done - if *allocate_digits* is higher than <decimal_t.size_allocated>. */
static int allocate_decimalhelper(decimal_t *restrict* dec, uint32_t size_allocate)
{
   int err ;
   decimal_t * olddec  = *dec ;

   if (  olddec
         && olddec->size_allocated >= size_allocate) {
      return 0 ;
   }

   // check that size_allocate can be cast to int8_t
   if (!size_allocate || size_allocate > INT8_MAX) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   uint32_t oldobjsize = olddec ? objectsize_decimal(olddec->size_allocated) : 0 ;
   uint32_t newobjsize = objectsize_decimal((uint8_t)size_allocate) ;
   memblock_t  mblock  = memblock_INIT(oldobjsize, (uint8_t*)olddec) ;

   // TODO: implement resize in memory manager which does not preserve content
   err = MM_RESIZE(newobjsize, &mblock) ;
   if (err) goto ONABORT ;

   decimal_t     * newdec = (decimal_t*) mblock.addr ;
   newdec->size_allocated = (uint8_t)size_allocate ;

   *dec = newdec ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

static int allocategroup_decimal(uint32_t nrobjects, /*out*/decimal_t * dec[nrobjects], const uint32_t allocate_digits[nrobjects])
{
   int err ;
   uint32_t objectindex ;

   for (objectindex = 0; objectindex < nrobjects; ++objectindex) {
      uint32_t size = allocate_digits[objectindex] ;

      // check that allocate_digits can be cast to int8_t
      if (!size || size > INT8_MAX) {
         err = EOVERFLOW ;
         goto ONABORT ;
      }

      // TODO: implement group allocate in memory manager
      memblock_t  mblock = memblock_INIT_FREEABLE ;
      err = MM_RESIZE(objectsize_decimal((uint8_t)size), &mblock) ;
      if (err) goto ONABORT ;

      ((decimal_t*) mblock.addr)->size_allocated = (uint8_t)size ;

      // partial assign of out param
      dec[objectindex] = ((decimal_t*) mblock.addr) ;
   }

   return 0 ;
ONABORT:
   while (objectindex) {
      err = delete_decimal(&dec[--objectindex]) ;
      if (err) goto ONABORT ;
   }
   LOG_ABORT(err) ;
   return err ;
}


/* function: add_decimalhelper
 * Implements adding of two decimal numbers.
 * The sign of the *result* is determined by parameter *ldec*.
 * The sign of parameter *rdec* is not taken into account.
 * */
static int add_decimalhelper(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec)
{
   int err ;
   uint8_t        lsize     = size_decimal(ldec) ;
   uint8_t        rsize     = size_decimal(rdec) ;
   const bool     isNegSign = isnegative_decimal(ldec) ;
   bool           carry ;
   int32_t        lexp      = ldec->exponent ;
   int32_t        rexp      = rdec->exponent ;
   const uint32_t * ldigits = ldec->digits ;
   const uint32_t * rdigits = rdec->digits ;

   // skip trailing zero in both operands
   while (lsize && 0 == ldigits[0]) {
      -- lsize  ;
      ++ ldigits ;
      ++ lexp ;
   }
   while (rsize && 0 == rdigits[0]) {
      -- rsize ;
      ++ rdigits ;
      ++ rexp ;
   }

   if (!rsize) {
      return copy_decimal(result, ldec) ;
   }
   if (!lsize) {
      err = copy_decimal(result, rdec) ;
      if (err) return err ;
      setpositive_decimal(*result) ;
      return 0 ;
   }

   int32_t lorder  = lexp + lsize ;
   int32_t rorder  = rexp + rsize ;

   if (lorder < rorder) {
      // swap rdec / ldec to make ldec the bigger number
      SWAP(ldec,    rdec) ;
      SWAP(lsize,   rsize) ;
      SWAP(lexp,    rexp) ;
      SWAP(ldigits, rdigits) ;
   }

   /*
    * bigger number:    ldec { lsize, lexp, ldigits }
    * smaller number:   rdec { rsize, rexp, rdigits }
    * Invariant: lexp + lsize >= rexp + rsize
    *
    * Calculate result = pow(-1,isNegSign) * (ldec + rdec)
    */

   int32_t expdiff = lexp - rexp ;

   /*
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭────────────┬─────┬──────────────────┬───┬──────────────────╮
    *   │  ... lexp ...                  │ ldigits[0] │ ... │ ldigits[X]       │...│ ldigits[lsize-1] │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰────────────┴─────┴──────────────────┴───┴──────────────────╯
    *                   |-  expdiff>0   -|                                                    lorder -╯
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭────────────┬───┬──────────────────┬──────────────────╮
    *   │  ... rexp ... │ rdigits[0] │...│      ...         │ rdigits[rsize-1] │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰────────────┴───┴──────────────────┴──────────────────╯
    *                                    |-  expdiff<0     -|
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭──────────────────╮
    *   │  ... rexp ...                                     │        ...       │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰──────────────────╯
    *                                                                  rorder -╯
    */

   uint32_t size = (uint32_t)lsize + 1/*carry*/ ;
   if (expdiff > 0) size += (uint32_t)expdiff ;

   err = allocate_decimalhelper(result, size) ;
   if (err) goto ONABORT ;

   uint32_t * digits = (*result)->digits ;

   /* handle trail
    * */
   if (expdiff < 0) {
      // simple copy
      expdiff = -expdiff ;
      memcpy(digits, ldigits, sizeof(digits[0]) * (uint32_t)expdiff) ;
      lsize    = (uint8_t) (lsize - expdiff) ; // result always > 0 cause ldec has at least rsize more digits
      ldigits += expdiff ;
      digits  += expdiff ;
   } else if (expdiff > 0) {
      // simple copy
      if (rsize < expdiff) {
         memcpy(digits, rdigits, sizeof(digits[0]) * rsize) ;
         memset(digits + rsize, 0, sizeof(digits[0]) * ((uint32_t)expdiff-rsize)) ;
         rsize    = 0 ;
      } else {
         memcpy(digits, rdigits, sizeof(digits[0]) * (uint32_t)expdiff) ;
         rsize    = (uint8_t) (rsize - expdiff) ;
         rdigits += expdiff ;
      }
      digits += expdiff ;
   }

   /* handle overlapping part
    * */
   static_assert(2*((uint64_t)DIGITSBASE) + 1/*carry*/ < INT32_MAX, "32bit sumdigit will suffice") ;
   uint32_t sum ;
   carry   = false ;
   lsize   = (uint8_t) (lsize - rsize) ;
   for (; rsize; --rsize) {
      sum   = *(ldigits++)+ *(rdigits++) + carry ;
      carry = (sum >= DIGITSBASE) ;
      sum   = (sum - DIGITSBASE * carry) ;
      *(digits++) = sum ;
   }

   /* handle leading part
    * */
   for (; carry && lsize; --lsize) {
      sum   = *(ldigits++) + carry ;
      carry = (sum >= DIGITSBASE) ;
      sum   = (sum - DIGITSBASE * carry) ;
      *(digits++) = sum ;
   }
   for (; lsize; --lsize) {
      *(digits++) = *(ldigits++) ;
   }

   /* handle carry part
    * */
   size     -= (0 == carry) ;
   digits[0] = carry ;

   (*result)->sign_and_used_digits = (int8_t)  (isNegSign ? - (int32_t) size : (int32_t) size) ;
   (*result)->exponent             = (int16_t) (lexp < rexp ? lexp : rexp) ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

/* function: sub_decimalhelper
 * Implements subtraction of two decimal numbers.
 * The sign of the *result* is determined by parameter *ldec*.
 * The sign of parameter *rdec* is not taken into account.
 * */
static int sub_decimalhelper(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec)
{
   int err ;
   uint8_t        lsize     = size_decimal(ldec) ;
   uint8_t        rsize     = size_decimal(rdec) ;
   bool           isNegSign = isnegative_decimal(ldec) ;
   bool           carry ;
   bool           isSwap ;
   int32_t        lexp      = ldec->exponent ;
   int32_t        rexp      = rdec->exponent ;
   const uint32_t * ldigits = ldec->digits ;
   const uint32_t * rdigits = rdec->digits ;

   // skip trailing zero in both operands
   while (lsize && 0 == ldigits[0]) {
      -- lsize  ;
      ++ ldigits ;
      ++ lexp ;
   }
   while (rsize && 0 == rdigits[0]) {
      -- rsize ;
      ++ rdigits ;
      ++ rexp ;
   }

   if (!rsize) {
      return copy_decimal(result, ldec) ;
   }
   if (!lsize) {
      err = copy_decimal(result, rdec) ;
      if (err) return err ;
      setnegative_decimal(*result) ;
      return 0 ;
   }

   int32_t lorder  = lexp + lsize ;
   int32_t rorder  = rexp + rsize ;

   // compare numbers to see which one is bigger
   if (lorder == rorder) {
      // compare values
      uint8_t minsize = (lsize < rsize ? lsize : rsize) ;
      lsize   = (uint8_t) (lsize - minsize) ;
      rsize   = (uint8_t) (rsize - minsize) ;
      for (;;) {
         if (ldigits[lsize+minsize-1] != rdigits[rsize+minsize-1]) {
            isSwap = (ldigits[lsize+minsize-1] < rdigits[rsize+minsize-1]) ;
            lsize  = (uint8_t) (lsize + minsize) ;
            rsize  = (uint8_t) (rsize + minsize) ;
            break ;
         }
         --minsize ;
         if (0 == minsize) {
            isSwap = (0 != rsize) ;
            if (isSwap || lsize) break ;
            // both numbers are equal
            return setfromint32_decimal(result, 0, 0) ;
         }
      }

   } else {
      // compare exponent of first digit
      isSwap = (lorder < rorder) ;
   }

   if (isSwap) {
      // swap rdec / ldec to make ldec the bigger number
      isNegSign = !isNegSign ;
      SWAP(ldec,    rdec) ;
      SWAP(lsize,   rsize) ;
      SWAP(lexp,    rexp) ;
      SWAP(ldigits, rdigits) ;
   }

   /*
    * bigger number:    ldec { lsize, lexp, ldigits }
    * smaller number:   rdec { rsize, rexp, rdigits }
    * Invariant: cmpmagnitude_decimal(ldec,rdec) > 0 !
    *
    * Calculate result = pow(-1,isNegSign) * (ldec - rdec)
    */

   int32_t expdiff = lexp - rexp ;

   /*
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭────────────┬─────┬──────────────────┬───┬──────────────────╮
    *   │  ... lexp ...                  │ ldigits[0] │ ... │ ldigits[X]       │...│ ldigits[lsize-1] │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰────────────┴─────┴──────────────────┴───┴──────────────────╯
    *                   |-  expdiff>0   -|                                                    lorder -╯
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭────────────┬───┬──────────────────┬──────────────────╮
    *   │  ... rexp ... │ rdigits[0] │...│      ...         │ rdigits[rsize-1] │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰────────────┴───┴──────────────────┴──────────────────╯
    *                                    |-  expdiff<0     -|
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭──────────────────╮
    *   │  ... rexp ...                                     │        ...       │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰──────────────────╯
    *                                                                  rorder -╯
    */

   uint32_t size = lsize ;
   if (expdiff > 0) size += (uint32_t)expdiff ;

   err = allocate_decimalhelper(result, size) ;
   if (err) goto ONABORT ;

   uint32_t * digits = (*result)->digits ;

   /* handle trail
    * */
   carry = false ;
   if (expdiff < 0) {
      // simple copy
      expdiff = -expdiff ;
      memcpy(digits, ldigits, sizeof(digits[0]) * (uint32_t)expdiff) ;
      lsize    = (uint8_t) (lsize - expdiff) ; // result always >= 0 cause ldec has at least rsize(possibly 0) more digits
      ldigits += expdiff ;
      digits  += expdiff ;
   } else if (expdiff > 0) {
      // copy inverted digits (rdigits[0] != 0) =>  (DIGITSBASE - *(rdigits++)) < DIGITSBASE && carry_is_set
      *(digits++) = DIGITSBASE - *(rdigits++) ;
      if (rsize < expdiff) {
         expdiff -= rsize ;
         while (--rsize) {
            *(digits++) = DIGITSBASE - 1/*carry*/ - *(rdigits++) ;
         }
         do {
            *(digits++) = DIGITSBASE - 1/*carry*/ ;
         } while (--expdiff) ;
      } else {
         rsize = (uint8_t) (rsize - expdiff) ;
         while (--expdiff) {
            *(digits++) = DIGITSBASE - 1/*carry*/ - *(rdigits++) ;
         }
      }
      carry = true ;
   }

   /* handle overlapping part
    * */
   static_assert(2*((uint64_t)DIGITSBASE) + 1/*carry*/ < INT32_MAX, "32bit sumdigit will suffice") ;
   lsize = (uint8_t) (lsize - rsize) ;
   for (; rsize; --rsize) {
      int32_t diff = (int32_t)*(ldigits++) - (int32_t)*(rdigits++) - carry ;
      carry = (diff < 0) ;
      diff += (int32_t)DIGITSBASE * carry ;
      *(digits++) = (uint32_t) diff ;
   }

   /* handle leading part
    * */
   for (; carry && lsize; --lsize) {
      int32_t diff = (int32_t)*(ldigits++) - carry ;
      carry = (diff < 0) ;
      diff += (int32_t)DIGITSBASE * carry ;
      *(digits++) = (uint32_t) diff ;
   }
   for (; lsize; --lsize) {
      *(digits++) = *(ldigits++) ;
   }

   while (0 == digits[-1]) {
      -- digits ;
      -- size ;
   }
   (*result)->sign_and_used_digits = (int8_t)  (isNegSign ? - (int32_t) size : (int32_t) size) ;
   (*result)->exponent             = (int16_t) (lexp < rexp ? lexp : rexp) ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

/* function: mult_decimalhelper
 * Multiplies two decimal numbers and returns the positive result.
 * The numbers are represented with their size and digits array.
 * This allows for splitting the nummbers without allocating a new <decimal_t>.
 *
 * (unchecked) Preconditions:
 * 1. result must be allocated with size == (lsize + rsize)
 * 2. (lsize > 0 && rsize > 0)
 * 3. lsize <= rsize
 * 4. ldigits[lsize-1] != 0 && rdigits[rsize-1] != 0
 * */
static void mult_decimalhelper(decimal_t * dec, uint8_t lsize, const uint32_t * ldigits, uint8_t rsize, const uint32_t * rdigits, int16_t exponent)
{
   uint32_t size     = (uint32_t)lsize + rsize ;
   uint32_t carry    = 0 ;
   uint32_t factor   = rdigits[0] ;
   uint32_t * digits = dec->digits ;

   if (0 == factor) {
      memset(digits, 0, sizeof(digits[0]) * (1u/*carry*/+lsize)) ;
   } else {
      unsigned li = 0;
      do { /*loop ldigits*/
         uint64_t multdigit = ldigits[li] * (uint64_t)factor + carry ;
         carry      = (uint32_t) (multdigit / DIGITSBASE) ;
         digits[li] = (uint32_t) (multdigit % DIGITSBASE) ;
         ++ li ;
      } while (li < lsize) ;
      digits[lsize] = carry ;
   }

   for (unsigned ri = 1; ri < rsize; ++ri) {
      factor = rdigits[ri] ;
      carry  = 0 ;
      ++ digits ;
      if (factor) {
         unsigned li = 0;
         do { /*loop ldigits*/
            uint64_t multdigit = ldigits[li] * (uint64_t)factor + (digits[li] + carry) ;
            static_assert( DIGITSBASE > (((uint64_t)DIGITSBASE-1)*(DIGITSBASE-1) + 2 *(DIGITSBASE-1)) / DIGITSBASE, "(multdigit / DIGITSBASE) is a valid digit") ;
            static_assert( UINT32_MAX > (DIGITSBASE-1 + DIGITSBASE-1), "digits[li] + carry fit in uint32_t") ;
            carry      = (uint32_t) (multdigit / DIGITSBASE) ;
            digits[li] = (uint32_t) (multdigit % DIGITSBASE) ;
            ++ li ;
         } while (li < lsize) ;
      }
      digits[lsize] = carry ;
   }

   size -= (0 == carry) ;  // if no carry occurred make result one digit smaller
   dec->sign_and_used_digits = (int8_t) size ;
   dec->exponent             = exponent ;

   return ;
}

/* function: addsplit_decimalhelper
 * Implements adding of two positive decimal numbers.
 *
 * (unchecked) Preconditions:
 * 1. result must be allocated with size >= (lsize + rsize)
 * 2. (lsize > 0 && rsize > 0)
 * 3. ldigits[lsize-1] != 0 && rdigits[rsize-1] != 0
 * 4. Either ldigits[0] != 0 || rdigits[0] != 0 => result->baseexponent == 0 !
 * */
static void addsplit_decimalhelper(decimal_t * restrict result, uint8_t digitsoffset, uint8_t lsize, const uint32_t * ldigits, uint8_t rsize, const uint32_t * rdigits)
{
   /*   ╭────────────┬───┬───┬──────────────────╮
    *   │ ldigits[0] │...│...│ ldigits[lsize-1] │
    *   ╰────────────┴───┴───┴──────────────────╯
    *   ╭────────────┬───┬───┬──────────────────╮
    *   │ rdigits[0] │...│...│ rdigits[rsize-1] │
    *   ╰────────────┴───┴───┴──────────────────╯
    *   ╭───────────────────────────────────────┬───────╮
    *   │        ...                            │ carry |
    *   ╰───────────────────────────────────────┴───────╯
    */

   uint32_t sum ;
   uint32_t * digits = &result->digits[digitsoffset] ;

   if (lsize > rsize) {
      // swap to make lsize <= rsize
      SWAP(lsize,   rsize) ;
      SWAP(ldigits, rdigits) ;
   }

   bool     carry ;
   uint8_t  size = (uint8_t) (rsize + digitsoffset /*no carry*/) ;

   /* handle overlapping part
    * */
   carry = false ;
   rsize = (uint8_t) (rsize - lsize) ;
   for (; lsize; --lsize) {
      sum   = *(ldigits++) + *(rdigits++) + carry ;
      carry = (sum >= DIGITSBASE) ;
      sum   = (sum - DIGITSBASE * carry) ;
      *(digits++) = sum ;
   }

   /* handle leading part
    * */
   for (; carry && rsize; --rsize) {
      sum   = *(rdigits++) + carry ;
      carry = (sum >= DIGITSBASE) ;
      sum   = (sum - DIGITSBASE * carry) ;
      *(digits++) = sum ;
   }
   for (; rsize; --rsize) {
      *(digits++) = *(rdigits++) ;
   }

   /* handle carry part
    * */
   if (carry) {
      ++ size ;
      digits[0] = 1 ;
   }

   result->sign_and_used_digits = (int8_t) size ;

   return ;
}

/* function: multsplit_decimalhelper
 * Multiplies two decimal numbers and returns the positive result.
 * The multiplication is done with splitting of numbers to save one multiplication.
 *
 * (unchecked) Preconditions:
 * 1. result must be allocated with size == (lsize + rsize)
 * 2. After skipping of trailing zeros: (lsize > 0 && rsize > 0)
 * 3. (ldigits[lsize-1] != 0 && rdigits[rsize-1] != 0)
 * */
static int multsplit_decimalhelper(decimal_t * restrict * result, uint8_t lsize, const uint32_t * ldigits, uint8_t rsize, const uint32_t * rdigits)
{
   int err ;
   decimal_t   * t[5] ;
   int32_t     exponent = 0 ;
   t[nrelementsof(t)-1] = 0 ;  // (t[3] == 0) ==> t[] is *not* allocated

   // skip trailing zero in both operands
   while (lsize && 0 == ldigits[0]) {
      -- lsize  ;
      ++ ldigits ;
      ++ exponent ;
   }
   while (rsize && 0 == rdigits[0]) {
      -- rsize ;
      ++ rdigits ;
      ++ exponent ;
   }

   if (lsize > rsize) {
      // swap rdec / ldec to make ldec the number with less or equal digits
      SWAP(lsize,   rsize) ;
      SWAP(ldigits, rdigits) ;
   }

   if (lsize < 4) {
      // simple multiplication
      mult_decimalhelper(*result, lsize, ldigits, rsize, rdigits, (int16_t)exponent) ;
      return 0 ;
   }

   const uint8_t split  = (uint8_t) ((lsize+1) / 2) ;
   uint8_t  lsplit ;
   uint8_t  rsplit ;

   lsplit = split ;
   while (0 == ldigits[lsplit-1]) --lsplit ; // ldigits[0] != 0 => loop ends !

   rsplit = split ;
   while (0 == rdigits[rsplit-1]) --rsplit ; // rdigits[0] != 0 => loop ends !

   /* Define: X = pow(DIGITSBASE, split)
    *         Multiplying by X means to increment the exponent with X !
    *
    * Divide: ldec:(lsize,ldigits) -> lh:(lsize-split, &ldigits[split]) * X + ll:(split, ldigits)
    *         rdec:(rsize,rdigits) -> rh:(rsize-split, &rdigits[split]) * X + rl:(split, rdigits)
    *
    * Compute: result = (ldec * rdec) !
    *          => result = (lh * X + ll) * (rh * X + rl)
    *    With: t0 = (lh * rh), t1 = (ll * rl)
    *          t2 = (lh + ll), t3 = (rh + rl)
    *          t4 = t2 * t3
    *          => result = t0 *X*X + t1 + (t4 - t0 - t1) * X
    *
    * Hint: Use  ll:(lsplit, ldigits), rl:(rsplit, rdigits)
    *       with lsplit <= split, rsplit <= split
    *       lsplit and rsplit must decremented until
    *       (ldigits[lsplit-1] != 0) && (rdigits[rsplit-1] != 0)
    */

   // calculate size of t0..t4
   uint32_t tsize[5] = {
      /*tOsize*/  ((uint32_t)rsize - split) + ((uint32_t)lsize - split)
      ,/*t1size*/ (uint32_t)lsplit + rsplit
      ,/*t2size*/ 1u/*carry*/ + split
      ,/*t3size*/ 1u/*carry*/ + (rsize > lsize ? (uint32_t)(rsize - split) : (uint32_t)split)
      ,/*t4size*/ 2u + rsize /* exact: Either (1+split+1+split-1) or (1+split+1+rsize-split-1)
                              * - The (-1) comes from the fact that if t2&t3 overflows into carry
                              * - multiplication of two carry digits result into a single digit only.
                              * case (rsize>lsize):
                              * (1+split+1+rsize-split-1) == (1+rsize)
                              * case (rsize==lsize):
                              * (1+split+1+split-1) == 1 + 2*split
                              * if   (0 == lsize%2) => rsize == 2*split   => (1 + 2*split) == (1+rsize)
                              * else (0 != lsize%2) => rsize == 2*split-1 => (1 + 2*split) == (2+rsize) */
   } ;
   static_assert(nrelementsof(tsize) == nrelementsof(t), "arrays must have same size") ;
   err = allocategroup_decimal(nrelementsof(t), t, tsize) ;
   if (err) goto ONABORT ;

   // compute t0
   err = multsplit_decimalhelper(&t[0], (uint8_t)(lsize - split), &ldigits[split], (uint8_t)(rsize - split), &rdigits[split]) ;
   if (err) goto ONABORT ;
   // compute t1
   err = multsplit_decimalhelper(&t[1], lsplit, ldigits, rsplit, rdigits) ;
   if (err) goto ONABORT ;
   // compute t2 (t[2]->baseexponent == 0)
   addsplit_decimalhelper(t[2], 0, (uint8_t)(lsize - split), &ldigits[split], lsplit, ldigits) ;
   // compute t3 (t[3]->baseexponent == 0)
   addsplit_decimalhelper(t[3], 0, (uint8_t)(rsize - split), &rdigits[split], rsplit, rdigits) ;
   // compute t4
   err = multsplit_decimalhelper(&t[4], (uint8_t)t[2]->sign_and_used_digits, t[2]->digits, (uint8_t)t[3]->sign_and_used_digits, t[3]->digits) ;
   if (err) goto ONABORT ;
   // compute t4 = (t4 - t0 - t1)
   err = sub_decimal(result, t[4], t[0]) ; // result->size_allocated >= lsize + rsize > 3u + rsize
   if (err) goto ONABORT ;
   err = sub_decimal(&t[4], *result, t[1]) ;
   if (err) goto ONABORT ;

   // compute result = t0*X*X + t1 (t0 and t1 do not overlap, add_ is a simple copy)
   t[0]->exponent = (int16_t) (t[0]->exponent + ((int32_t)split + (int32_t)split)) ;
   err = add_decimal(result, t[0], t[1]) ; // result->size_allocated >= lsize + rsize == t[0]size + t[1]size
   if (err) goto ONABORT ;

   // compute result += t4 * X
   uint8_t offset = (uint8_t) (t[4]->exponent + split/*X*/ - (*result)->exponent) ;
   addsplit_decimalhelper(*result, offset, (uint8_t)((*result)->sign_and_used_digits-offset), &(*result)->digits[offset], (uint8_t)t[4]->sign_and_used_digits, t[4]->digits) ;

   (*result)->exponent = (int16_t) ((*result)->exponent + exponent) ;

   // free resources
   for (unsigned i = 0; i < nrelementsof(t); ++i) {
      err = delete_decimal(&t[i]) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   if (t[nrelementsof(t)-1]) {
      for (unsigned i = 0; i < nrelementsof(t); ++i) {
         (void) delete_decimal(&t[i]) ;
      }
   }
   clear_decimal(*result) ;
   LOG_ABORT(err) ;
   return err ;
}

/* function: div3by2digits_decimalhelper
 * Computes ((dividend * DIGITSBASE + nextdigit) / divisor).
 * The value returned in nextdigit is always in range [1..DIGITSBASE-1] (in case preconditions are met).
 *
 * Changed values:
 * 1. nextdigit contains result of (dividend * DIGITSBASE + nextdigit) / dividend
 * 2. dividend  contains result of (dividend * DIGITSBASE + nextdigit) % dividend
 *
 * (unchecked) Preconditions:
 * 1. divisor < DIGITSBASE*DIGITSBASE && dividend < DIGITSBASE*DIGITSBASE && nextdigit < DIGITSBASE
 * 2. divisor > dividend
 * 3. divisor < (dividend * DIGITSBASE + nextdigit)
 * */
static void div3by2digits_decimalhelper(decimal_divstate_t * state)
{
   uint32_t carry ;

   // calculate (dividend * DIGITSBASE + nextdigit) * 4
   static_assert(((DIGITSBASE-1) * 4) > DIGITSBASE, "multiplication by 4 does not overflow uint32_t") ;
   carry = (uint32_t) (state->dividend >> 32) ;
   state->dividend  = (uint32_t)state->dividend * (uint64_t)(4*DIGITSBASE) + (state->nextdigit << 2) ;
   state->nextdigit = (uint32_t)state->dividend ;
   state->dividend  = carry * (uint64_t)(4*DIGITSBASE) + (uint32_t) (/*carry*/state->dividend >> 32) ;

   uint32_t quot = 0 ;

   // calculate only 30 lower bits cause the 2 top most sign. bits are always zero.
   // therefore dividend*DIGITSBASE+nextdigit are multiplied by 4
   for (unsigned i = 30; i > 0; --i) {
      quot <<= 1 ;
      state->dividend <<= 1 ;
      state->dividend += (0 != (state->nextdigit & 0x80000000)) ;
      state->nextdigit <<= 1 ;
      if (state->dividend >= state->divisor) {
         state->dividend -= state->divisor ;
         ++ quot ;
      }
   }

   state->nextdigit = quot ;

   return ;
}

/* function: submul_decimalhelper
 * Computes »ldigits[*]-=nextdigit*rdigits[*]« and corrects nextdigit with -1 if necessary.
 *
 * Special case (nextdigit == DIGITSBASE-1):
 * No multiplication is done.
 *
 * Special case (nextdigit == 1):
 * No multiplication is done.
 *
 * (unchecked) Precondition:
 * 1. nextdigit > 0 && nextdigit < DIGITSBASE
 * 2. ldigits[loffset] == 0 && ldigits[loffset+1] == 0
 * 3. dividend contains the result of ldigits["top digits"]-=nextdigit*rdigits["top digits"]
 * */
static void submul_decimalhelper(decimal_divstate_t * state)
{
   /*    ╭────────────┬───┬───┬──────────────────┬─────────────────╮
    *    │ ldigits[F] │...│[L]│   0  [loffset]   │  0 [loffset+1]  │
    *    ╰────────────┴───┴───┴──────────────────┴─────────────────╯
    *  - ╭────────────┬───┬───┬──────────────────┬─────────────────╮
    *    │ rdigits[0] │...│...│ rdigits[rsize-2] │ rdigits[rsize-1]│
    *    ╰────────────┴───┴───┴──────────────────┴─────────────────╯
    * ================================================================
    *    ╭────────────┬───┬───┬──────────────────┬─────────────────╮
    *    │ diff       │...│...│              dividend              │
    *    ╰────────────┴───┴───┴──────────────────┴─────────────────╯
    *         the result of the 2 topmost digits is already computed and stored in dividend
    *         ldigits[loffset] and ldigits[loffset+1] are set to 0 (ringbuffer)
    * where F(irst) = loffset - (rsize-2)
    *       L(ast)  = loffset - 1
    *
    * (rsize == 2) ==> nothing must be computed, dividend contains the result
    *
    */

   if (state->rsize > 2) {
      uint32_t       carry     = 0 ;
      const uint32_t * rdigits = state->rdigits ;
      int32_t        i ;

      i = state->loffset + (int32_t)2 - state->rsize ;
      if (i < 0) i += state->lsize ; /*(state->lsize >= state->rsize) ==> i > 0 && i < lsize*/

      if (  state->nextdigit == 1)  {                 // no multiplication needed
         for (int32_t diff; i != state->loffset; ++i, ++rdigits) {
            if (i == state->lsize) i = 0 ; // ringbuffer
            diff  = (int32_t)state->ldigits[i] - (int32_t)rdigits[0] - (int32_t)carry ;
            carry = (diff < 0) ;
            if (carry) diff += (int32_t)DIGITSBASE ;
            state->ldigits[i] = (uint32_t)diff ;
         }

      } else if (state->nextdigit == DIGITSBASE-1) {  // no multiplication needed
         uint32_t lastdigit = 0 ;
         carry = 1 ; // redefine carry: 1 means "no carry", 0 means "subtract 1", 2 means "add 1"
         for (int32_t diff; i != state->loffset; ++i, ++rdigits) {
            if (i == state->lsize) i = 0 ; // ringbuffer
            static_assert( (uint64_t)DIGITSBASE + DIGITSBASE < INT32_MAX, "diff is big enough" ) ;
            diff = (int32_t)state->ldigits[i] + (int32_t)rdigits[0] - (int32_t)lastdigit + (int32_t)carry - 1/*signed carry*/ ;
            lastdigit = rdigits[0] ;
            carry = 1 ;
            if (diff >= (int32_t)DIGITSBASE) {
               ++ carry ;
               diff -= (int32_t)DIGITSBASE ;
            } else if (diff < 0) {
               -- carry ;
               diff += (int32_t)DIGITSBASE ;
            }
            state->ldigits[i] = (uint32_t)diff ;
         }

         // case not possible: (lastdigit == 0) && (carry == 2) ; Why ?
         // (lastdigit == 0) && (carry == 2) ==> previous rdigits[0] == 0 && carry == 2 and so on ==> contradiction to carry = 1 as start condition
         carry = lastdigit + 1u - carry ;

      } else {
         for (int32_t diff; i != state->loffset; ++i, ++rdigits) {
            if (i == state->lsize) i = 0 ; // ringbuffer
            uint64_t multdigit = rdigits[0] * (uint64_t)state->nextdigit + carry ;
            static_assert( DIGITSBASE > (((uint64_t)DIGITSBASE-1) * DIGITSBASE + (DIGITSBASE-1)) / DIGITSBASE, "(multdigit / DIGITSBASE) is a valid digit") ;
            carry = (uint32_t) (multdigit / DIGITSBASE) ;
            diff  = (int32_t)state->ldigits[i] - (int32_t) (uint32_t) (multdigit % DIGITSBASE) ;
            if (diff < 0) {
               ++ carry ;
               diff += (int32_t)DIGITSBASE ;
            }
            state->ldigits[i] = (uint32_t)diff ;
         }
      }

      if (carry <= state->dividend) {
         state->dividend -= carry ;

      } else {    // needs correction
         state->dividend -= carry ;
         carry   = 0 ;
         rdigits = state->rdigits ;

         -- state->nextdigit ;

         i = state->loffset + (int32_t)2 - state->rsize ;
         if (i < 0) i += state->lsize ;

         for (; i != state->loffset; ++i, ++rdigits) {
            if (i == state->lsize) i = 0 ; // ringbuffer
            static_assert( UINT32_MAX > 2*((uint64_t)DIGITSBASE-1) + 1, "sum is a valid digit") ;
            uint32_t sum = state->ldigits[i] + rdigits[0] + carry ;
            carry = (sum >= DIGITSBASE) ;
            if (carry) sum -= (int32_t)DIGITSBASE ;
            state->ldigits[i] = sum ;
         }
         state->dividend += state->divisor ;
         state->dividend += carry ;
      }
   }

   return ;
}

/* function: div_decimalhelper
 * Divides two decimal numbers and returns the positive quotient.
 * The division is done as you have learned in school.
 * The content of ldigits is destroyed.
 *
 * (unchecked) Preconditions:
 * 1. result must be allocated with result_size && result_size > 0
 * 2. lsize >= rsize && rsize >= 2
 * 3. lsize[lsize-1] != 0 && rsize[rsize-1] != 0
 * 4. exponent must be in range of int16_t
 * */
static int div_decimalhelper(decimal_t *restrict result, bool isNegSign, int32_t exponent, const uint8_t lsize, uint32_t * const ldigits, const uint8_t rsize, const uint32_t * const rdigits, const uint8_t result_size)
{
   decimal_divstate_t  state = {
            // .dividend contains modulo after division operation
            // its value replaces ldigits[loffset+1]°ldigits[loffset]
            .dividend  = ldigits[lsize-1] * (uint64_t)DIGITSBASE + ldigits[lsize-2],
            .divisor   = rdigits[rsize-1] * (uint64_t)DIGITSBASE + rdigits[rsize-2],
            .nextdigit = 0,
            .loffset = lsize,
            .lsize   = lsize,
            .rsize   = rsize,
            .size    = result_size,
            .ldigits = ldigits,
            .rdigits = rdigits,
   } ;

   uint32_t * resultdigits = result->digits ;

   // ldigits is accessed as ringbuffer starting from loffset going to 0
   // the last read digit is set to 0 to make it appear that ldigits has an inifinite extension of 0 digits
   ldigits[--state.loffset] = 0 ;
   ldigits[--state.loffset] = 0 ;
   if (!state.loffset) state.loffset = lsize ;

   if (state.divisor <= state.dividend) {
      state.nextdigit = (uint32_t) (state.dividend / state.divisor) ;
      state.dividend  = (state.dividend % state.divisor) ;
      submul_decimalhelper(&state) ;
      if (0 == state.nextdigit) {
         -- exponent ;
      }
   } else {
      -- exponent ;
   }

   if (  abs_int(exponent) > INT16_MAX) {
      return EOVERFLOW ;
   }

   if (  state.nextdigit) {
      resultdigits[--state.size] = state.nextdigit ;
   }

   while (state.size) {
      state.nextdigit = ldigits[--state.loffset] ;
      ldigits[state.loffset] = 0 ;
      if (!state.loffset) state.loffset = lsize ;

      if (state.dividend == state.divisor) {
         // (dividend = dividend*DIGITSBASE+dividenlow - (DIGITSBASE-1)*divisor) == dividend+nextdigit
         state.dividend += state.nextdigit ;
         state.nextdigit = DIGITSBASE-1 ;
         submul_decimalhelper(&state) ;

      } else if ((uint32_t)(state.dividend >> 32)) {
         div3by2digits_decimalhelper(&state) ;
         submul_decimalhelper(&state) ;

      } else if ((uint32_t)state.dividend) {    // state.dividend <= UINT32_MAX can be handled in this case (even more)
         static_assert(UINT32_MAX * (uint64_t)DIGITSBASE + (DIGITSBASE-1) == 0x3b9ac9ffffffffff, "check that uint64_t is enough" ) ;
         state.dividend = (uint32_t)state.dividend * (uint64_t)DIGITSBASE + state.nextdigit ;

         if (state.divisor <= state.dividend) {
            state.nextdigit = (uint32_t) (state.dividend / state.divisor) ;
            state.dividend  = (state.dividend % state.divisor) ;
            submul_decimalhelper(&state) ;
         } else {
            state.nextdigit = 0 ;
         }

      } else {
         state.dividend  = state.nextdigit ;
         state.nextdigit = 0 ;
      }

      resultdigits[--state.size] = state.nextdigit ;
   }

   result->sign_and_used_digits = (int8_t)  (isNegSign ? - (int32_t)result_size : (int32_t)result_size) ;
   result->exponent             = (int16_t) (exponent) ;

   return 0 ;
}

/* function: divi32_decimalhelper
 * Divides decimal number by integer and returns the positive quotient.
 * The division is done as you have learned in school.
 *
 * (unchecked) Preconditions:
 * 1. result must be allocated with result_size && result_size > 0
 * 2. lsize > 0 && ldigits[lsize-1] != 0
 * 3. divisor < DIGITSBASE
 * 4. exponent must be in range of int16_t
 * */
static int divi32_decimalhelper(decimal_t *restrict result, bool isNegSign, int32_t exponent, uint8_t lsize, const uint32_t * ldigits, uint32_t divisor, const uint32_t result_size)
{
   uint8_t  loffset = lsize ;
   uint32_t size    = result_size ;
   uint32_t quotient ;
   uint32_t digit ;
   uint32_t nextdigit ;
   uint64_t dividend ;

   digit = ldigits[--loffset/*lsize > 0*/] ;

   if (digit >= divisor) {
      quotient = digit / divisor ;
      digit    = digit % divisor ;
      result->digits[--size/*result_size > 0*/] = quotient ;
   } else {
      /* (ldigits[lsize-1] != 0) => (digit != 0) => result->digits[result_size-1] != 0 */
      -- exponent ;
   }

   if (  abs_int(exponent) > INT16_MAX) {
      return EOVERFLOW ;
   }

   while (size) {
      nextdigit = (loffset ? ldigits[--loffset] : 0) ;
      if (digit) {
         // divide 2 digits by 1
         dividend = (digit * (uint64_t)DIGITSBASE) + nextdigit ;
         quotient = (uint32_t) (dividend / divisor) ;
         digit    = (uint32_t) (dividend % divisor) ;
      } else if (nextdigit >= divisor) {
         quotient = nextdigit / divisor ;
         digit    = nextdigit % divisor ;
      } else {
         quotient = 0 ;
         digit    = nextdigit ;
      }
      result->digits[--size] = quotient ;
   }

   result->sign_and_used_digits = (int8_t)  (isNegSign ? - (int32_t)result_size : (int32_t)result_size) ;
   result->exponent             = (int16_t) (exponent) ;

   return 0 ;
}

/* function: nrzerobits2nrdigits_decimalhelper
 * Returns the nr of zero decimal digits calculated from the number of zero bits.
 * The binary number "0b0.0000111111" has 4 zero bits after the binamal point
 * and this function returns the number of zero digits "0.0009" which
 * would translate into the binary number. The returned number is valid for a
 * 64(or bigger) bit mantissa. For smaller mantissa the nr of leading zero decimal
 * digits returned could be too small by one.
 *
 * Implementation hint:
 * pow(10,294912) needs 32 * 30615 bits to be stored
 * Tested only up to 1080 bits. */
static inline uint32_t nrzerobits2nrdigits_decimalhelper(uint32_t nrleadingzerobits)
{
   return (294912 * nrleadingzerobits) / (32 * 30615) ;
}

/* function: nrzerobits2decsize_decimalhelper
 * See <nrzerobits2nrdigits_decimalhelper>.
 * The difference is that <nrzerobits2decsize_decimalhelper>
 * returns <nrzerobits2nrdigits_decimalhelper> divided by <digitsperint_decimal>(). */
static inline uint32_t nrzerobits2decsize_decimalhelper(uint32_t nrleadingzerobits)
{
   return (294912 * nrleadingzerobits) / (32 * 30615 * (uint32_t)digitsperint_decimal()) ;
}

/* function: nrzerodigits2nrbits_decimalhelper
 * Returns the nr of zero bits calculated from the number of zero decimal digits.
 * The decimal number "0.0000999" has 4 zero digits after the decimal point
 * and this function returns the number of zero bits "0b0.00...001" which
 * would translate into the decimal number. The returned number is valid for a
 * 1 bit mantissa. For bigger mantissa the nr of leading zero bits
 * returned could be too small by one.
 *
 * Implementation hint:
 * pow(10,294912) needs 32 * 30615 bits to be stored
 * Tested only up to 1080 bits. */
static uint32_t nrzerodigits2nrbits_decimalhelper(uint32_t nrleadingzerodigits)
{
   return (nrleadingzerodigits * (32 * 30615)) / 294912 ;
}

/* function: calcfractionsize_decimalhelper
 * Calculates the number of digits needed to represent a binary fraction.
 * The binary fraction 0.00000...0111...1 has *fractionalbits* digits after the binamal point.
 * This function assumes that all digits are 1 except for the first *nrleadingzerobits*.
 * After return the parameter *leadingzerosize* contains the nr of full integer digits
 * which are zero. */
static uint32_t calcfractionsize_decimalhelper(/*out*/uint32_t * leadingzerosize, uint32_t fractionalbits, uint32_t nrleadingzerobits)
{
   if (  fractionalbits > digitsperint_decimal() * sizemax_decimal()
         || nrleadingzerobits >= fractionalbits) {
      // (possible) overflow
      *leadingzerosize = 0 ;
      return 1 + sizemax_decimal() ;
   }

   uint32_t zerosize = nrzerobits2decsize_decimalhelper(nrleadingzerobits) ;
   uint32_t nrdigits = (fractionalbits + digitsperint_decimal()-1/*ROUND UP*/) ;
   uint32_t size     = nrdigits / digitsperint_decimal() - zerosize ;

   *leadingzerosize = zerosize ;

   return size ;
}

/* function: digit2str_decimalhelper
 * Writes a decimal representation of *digit* into string *str*.
 * Exactly *digitsize* least significant digits of *digit* are written to the string.
 * */
static inline void digit2str_decimalhelper(uint8_t * str, uint32_t digit, uint8_t digitsize)
{
   uint32_t ch ;
   while (digit && digitsize) {
      ch    = digit % 10 ;
      digit = digit / 10 ;
      str[--digitsize] = (uint8_t) ('0' + ch) ;
   }

   while (digitsize) {
      str[--digitsize] = '0' ;
   }
}

// group: lifetime

int new_decimal(/*out*/decimal_t ** dec, uint32_t nrdigits)
{
   int err ;
   decimal_t   * newobj      = 0 ;
   uint32_t    size_allocate = ((digitsperint_decimal()-1) + nrdigits) / digitsperint_decimal() ;

   VALIDATE_INPARAM_TEST(nrdigits && nrdigits <= nrdigitsmax_decimal(), ONABORT, ) ;

   err = allocate_decimalhelper(&newobj, size_allocate ? size_allocate : 1) ;
   if (err) goto ONABORT ;

   newobj->sign_and_used_digits = 0 ;
   newobj->exponent             = 0 ;

   *dec = newobj ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int delete_decimal(decimal_t ** dec)
{
   int err ;

   decimal_t * del_dec = *dec ;

   if (del_dec) {
      *dec = 0 ;

      memblock_t  mblock = memblock_INIT(objectsize_decimal(del_dec->size_allocated), (uint8_t*) del_dec) ;

      err = MM_FREE(&mblock) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: query

int cmp_decimal(const decimal_t * ldec, const decimal_t * rdec)
{
   /* compare sign */
   const int lsign = sign_decimal(ldec) ;
   const int rsign = sign_decimal(rdec) ;

   if (lsign != rsign) {
      return sign_int(lsign - rsign) ;
   } else if (lsign < 0) {
      /* both values negative => return negative of comparison */
      return cmpmagnitude_decimal(rdec, ldec) ;
   }

   return cmpmagnitude_decimal(ldec, rdec) ;
}

int cmpmagnitude_decimal(const decimal_t * ldec, const decimal_t * rdec)
{
   uint8_t lsize = size_decimal(ldec) ;
   uint8_t rsize = size_decimal(rdec) ;
   uint8_t minsize ;

   /* check for 0 values */

   if (  0 == lsize
         || 0 == rsize) {
      return sign_int((int32_t)lsize - (int32_t)rsize) ;
   }

   /* max exponent determines max number */

   const int32_t lorder = ldec->exponent + (int32_t)lsize ;
   const int32_t rorder = rdec->exponent + (int32_t)rsize ;

   int32_t orderdiff = lorder - rorder ;

   if (orderdiff)  {
      /* order of magnitude of numbers differ */
      return sign_int(orderdiff) ;

   } else {
      /* compare unshifted values */
      const uint32_t  * ldigits = &ldec->digits[lsize] ;
      const uint32_t  * rdigits = &rdec->digits[rsize] ;
      uint32_t rdigit ;
      uint32_t ldigit ;

      minsize = (uint8_t) (lsize < rsize ? lsize : rsize) ;
      rsize = (uint8_t) (rsize - minsize) ;
      lsize = (uint8_t) (lsize - minsize) ;

      do {
         rdigit  = *(--rdigits) ;
         ldigit  = *(--ldigits) ;
         if (ldigit != rdigit) {
            return ldigit < rdigit ? -1 : 1 ;
         }
      } while (--minsize) ;

      if (rsize) {
         do {
            rdigit = *(--rdigits) ;
            if (rdigit) return -1 ;
         } while (--rsize) ;
      } else {
         for (; lsize; --lsize) {
            ldigit = *(--ldigits) ;
            if (ldigit) return 1 ;
         }
      }
   }

   return 0 ;
}

int32_t first9digits_decimal(decimal_t * dec, int32_t * decimal_exponent)
{
   uint32_t digits   = 0 ;
   uint8_t  nrblocks = size_decimal(dec) ;
   int32_t  dec_expo = exponent_decimal(dec) ;

   if (nrblocks) {
      -- nrblocks ;
      digits = dec->digits[nrblocks] ;
      if (nrblocks) {
         dec_expo += digitsperint_decimal() * nrblocks ;
         if (digits < DIGITSBASE/10) {
            unsigned l10 = log10_int(digits) + 1 ;
            dec_expo -= (int) (digitsperint_decimal() - l10) ;
            digits   *= power10_decimalhelper(digitsperint_decimal() - l10) ;
            digits   += dec->digits[nrblocks-1] / power10_decimalhelper(l10) ;
         }
      }
   }

   *decimal_exponent = dec_expo ;

   return (dec->sign_and_used_digits < 0) ? - (int32_t) digits : (int32_t) digits ;
}

int64_t first18digits_decimal(decimal_t * dec, int32_t * decimal_exponent)
{
   uint64_t digits   = 0 ;
   uint8_t  nrblocks = size_decimal(dec) ;
   int      dec_expo = exponent_decimal(dec) ;

   if (nrblocks) {
      -- nrblocks ;
      digits = dec->digits[nrblocks] ;
      if (nrblocks) {
         -- nrblocks ;
         unsigned l10 = log10_int64(digits) + 1 ;
         digits *= DIGITSBASE ;
         digits += dec->digits[nrblocks] ;
         if (nrblocks) {
            dec_expo += digitsperint_decimal() * nrblocks ;
            if (l10 < digitsperint_decimal()) {
               dec_expo -= (int) (digitsperint_decimal() - l10) ;
               digits   *= power10_decimalhelper(digitsperint_decimal() - l10) ;
               digits   += dec->digits[nrblocks-1] / power10_decimalhelper(l10) ;
            }
         }
      }
   }

   *decimal_exponent = dec_expo ;

   return (dec->sign_and_used_digits < 0) ? - (int64_t) digits : (int64_t) digits ;
}

uint16_t nrdigits_decimal(const decimal_t * dec)
{
   uint16_t nrdigits = size_decimal(dec) ;

   if (nrdigits) {
      -- nrdigits ;
      nrdigits = (uint16_t) (nrdigits * (unsigned)digitsperint_decimal() + 1u + log10_int(dec->digits[nrdigits])) ;
   }

   return nrdigits ;
}

int tocstring_decimal(const decimal_t * dec, struct cstring_t * cstr)
{
   int err ;
   uint16_t       size     = size_decimal(dec) ;
   int32_t        exponent = dec->exponent ;
   const uint32_t * digits = dec->digits ;

   // skip trailing 0 digits
   while (  size
            && 0 == digits[0]) {
      -- size ;
      ++ digits ;
      ++ exponent ;
   }

   /*size == 0 || 0 != digits[0]*/

   if (size) {
      uint8_t  nrzeropos ;
      uint8_t  digitsize ;
      uint8_t  expsize   ;
      uint32_t lastdigit ;
      uint32_t digit ;
      uint8_t  * str ;

      exponent *= digitsperint_decimal() ;

      lastdigit = digits[0] /*always != 0*/;
      for (nrzeropos = 0; 0 == (lastdigit % 10); ++nrzeropos, ++exponent) {
         lastdigit /= 10 ;
      }

      digit     = digits[--size] ;
      digitsize = (uint8_t) (1u + log10_int(digit)) ;
      expsize   = (uint8_t) (exponent ? (exponent < 0) + 1u/*e*/+ 1u+log10_int(abs_int(exponent)) : 0u) ;

      const size_t strsize = (isnegative_decimal(dec) + (uint32_t)expsize + (uint32_t)digitsize
                              + digitsperint_decimal() * (uint32_t)size) - (uint32_t)nrzeropos ;
      err = resize_cstring(cstr, strsize) ;
      if (err) goto ONABORT ;

      str = (uint8_t*) str_cstring(cstr) ;

      if (isnegative_decimal(dec)) {
         *(str++) = '-' ;
      }

      if (size) {
         // the first digit is different from the last digit
         digit2str_decimalhelper(str, digit, digitsize) ;
         str += digitsize ;

         while (--size) {
            digit = digits[size] ;
            digit2str_decimalhelper(str, digit, digitsperint_decimal()) ;
            str += digitsperint_decimal() ;
         }

         digitsize = digitsperint_decimal() ; // indicate last != first digit
      }

      // handles last digit: digitsize instead of digitsperint_decimal() is used to handle
      // case where first and last digit are equal !
      digit2str_decimalhelper(str, lastdigit, (uint8_t)(digitsize-nrzeropos)) ;
      str += digitsize-nrzeropos ;

      if (expsize) {
         *(str++) = 'e', -- expsize ;
         if (exponent < 0) {
            *(str++) = '-' ;
            exponent = - exponent ;
            -- expsize;
         }
         digit2str_decimalhelper(str, (uint32_t) exponent, expsize) ;
         str += expsize ;
      }

      err = truncate_cstring(cstr, (size_t) (str - (uint8_t*)str_cstring(cstr))) ;
      if (err) goto ONABORT ;

   } else {
      clear_cstring(cstr) ;
      err = append_cstring(cstr, 1, "0") ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   clear_cstring(cstr) ;
   LOG_ABORT(err) ;
   return err ;
}

// group: assign

void clear_decimal(decimal_t * dec)
{
   dec->sign_and_used_digits = 0 ;
   dec->exponent             = 0 ;
}

int copy_decimal(decimal_t *restrict* dec, const decimal_t * restrict copyfrom)
{
   int err ;
   const uint8_t copysize = size_decimal(copyfrom) ;

   err = allocate_decimalhelper(dec, copysize) ;
   if (err) goto ONABORT ;

   (*dec)->sign_and_used_digits = copyfrom->sign_and_used_digits ;
   (*dec)->exponent             = copyfrom->exponent ;
   memcpy((*dec)->digits, copyfrom->digits, copysize * sizeof(copyfrom->digits[0])) ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int setfromint32_decimal(decimal_t *restrict* dec, int32_t value, int32_t decimal_exponent)
{
   int err ;
   uint32_t             digit      = abs_int(value) ;
   uint32_t             shiftcarry = 0 ;
   alignedexpandshift_t alignshift ;

   err = alignedexpandshift_decimalhelper(&alignshift, decimal_exponent) ;
   if (err) goto ONABORT ;

   static_assert((uint64_t)(INT32_MAX / DIGITSBASE) * (DIGITSBASE/10/*maxshift*/) < DIGITSBASE, "always fit in 2 digits" ) ;

   digit = alignshift.shiftleft(&shiftcarry, digit) ;

   if (shiftcarry) {
      err = allocate_decimalhelper(dec, 2) ;
      if (err) goto ONABORT ;

      (*dec)->sign_and_used_digits = (int8_t) ((value > 0) ? +2 : -2) ;
      (*dec)->exponent             = alignshift.alignedexp ;
      (*dec)->digits[0] = digit ;
      (*dec)->digits[1] = shiftcarry ;
   } else if (digit) {
      (*dec)->sign_and_used_digits = (int8_t)  ((value > 0) ? +1 : -1) ;
      (*dec)->exponent             = alignshift.alignedexp ;
      (*dec)->digits[0] = digit ;
   } else {
      (*dec)->sign_and_used_digits = 0 ;
      (*dec)->exponent             = 0 ;
   }

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int setfromint64_decimal(decimal_t *restrict* dec, int64_t value, int32_t decimal_exponent)
{
   int err ;
   uint64_t             digit      = abs_int64(value) ;
   uint32_t             shiftcarry = 0 ;
   uint32_t             size       = 0 ;
   uint32_t             decdigit[3] ;
   alignedexpandshift_t alignshift ;

   if (value) {
      err = alignedexpandshift_decimalhelper(&alignshift, decimal_exponent) ;
      if (err) goto ONABORT ;

      static_assert((INT64_MAX / ((int64_t)DIGITSBASE*DIGITSBASE)) * (DIGITSBASE/10/*maxshift*/) < DIGITSBASE, "always fits in 3 digits" ) ;

      do {
         uint64_t temp = (digit % DIGITSBASE) ;
         digit = digit / DIGITSBASE ;
         decdigit[size++] = alignshift.shiftleft(&shiftcarry, (uint32_t)temp) ;
      } while (digit || shiftcarry) ;

      err = allocate_decimalhelper(dec, size) ;
      if (err) goto ONABORT ;

      memcpy((*dec)->digits, decdigit, sizeof((*dec)->digits[0]) * size) ;
      (*dec)->sign_and_used_digits = (int8_t) ((value > 0) ? size : -size) ;
      (*dec)->exponent             = alignshift.alignedexp ;

   } else {
      (*dec)->sign_and_used_digits = 0 ;
      (*dec)->exponent             = 0 ;
   }

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int setfromfloat_decimal(decimal_t *restrict* dec, float value)
{
   int err ;

   decimal_frombigint_t * converter = 0 ;
   bigint_t    * big[2] = { 0, 0 } ;
   uint32_t    size ;
   float       fraction ;
   float       integral ;
   int         fexp ;

   VALIDATE_INPARAM_TEST(isfinite(value), ONABORT, ) ;

   fraction = modff(fabsf(value), &integral) ;

   if (fraction) {      // assume 24 bit mantissa; validated in test_setfromfloat

      if (integral) {   // decode fractional & integral part
         static_assert(0x00ffffff < DIGITSBASE, "24 bit mantissa smaller than DIGITSBASE => one digit for integral") ;
         static_assert(0 == (DIGITSBASE & 0x1ff) && 0 != (DIGITSBASE & 0x200), "DIGITSBASE shifts 9 bits => at max 3 mults for fractional part") ;
         uint64_t ifraction = (uint32_t) ldexpf(fraction, 32) ;
         size = 2u + (0 != (ifraction & 0x007fffff)) + (0 != (ifraction & 0x00003fff)) ;
         err = allocate_decimalhelper(dec, size) ;
         if (err) goto ONABORT ;
         decimal_t * dec2 = *dec ;
         dec2->digits[size-1]       = (uint32_t) integral ;
         dec2->sign_and_used_digits = (int8_t)   (value < 0 ? - (int32_t) size : (int32_t) size) ;
         dec2->exponent             = (int16_t) -(int32_t)(--size) ;
         do {
            ifraction  = (uint32_t) ifraction ;
            ifraction *= DIGITSBASE ;
            dec2->digits[--size] = (uint32_t) (ifraction >> 32) ;
         } while (size) ;

      } else {    // decode only fractional part
         fraction = frexpf(fraction, &fexp) ;
         fexp = - fexp ;
         const uint32_t nrleadingzerobits = (uint32_t) fexp ;
         // fraction == 0.1XXXXXXX-XXXXXXXX-XXXXXXXX-00000000 (normalized, subnormals begin with 0.0XXX)
         uint32_t ifraction       = (uint32_t) ldexpf(fraction, 24) ;
         uint32_t mantissabits    = 24 ;
         uint32_t leadingzerosize = 0 ;

         if (ifraction/*only for safety must always be true*/) {
            for (; 0 == (ifraction & 0x1); ifraction >>= 1) {
               -- mantissabits ;
            }
         }

         size = calcfractionsize_decimalhelper(&leadingzerosize, nrleadingzerobits + mantissabits, nrleadingzerobits) ;
         err = allocate_decimalhelper(dec, size) ;
         if (err) goto ONABORT ;

         int32_t        exponent           = - (int32_t) size ;
         const uint32_t fillbitsshift      = (nrleadingzerobits + mantissabits) % bitsperdigit_bigint() ;
         const uint32_t bigintfractionsize = (nrleadingzerobits + mantissabits) / bitsperdigit_bigint() + (0 != fillbitsshift) ;
         for (int i = 1; i >= 0; --i) {
            err = new_bigint(&big[i], 1/*leading int digit*/ + bigintfractionsize) ;
            if (err) goto ONABORT ;
         }

         // multiplicate big to fill leading zero bits ?

         if (leadingzerosize) {
            exponent -= (int32_t) leadingzerosize ;
            // leadingzerosize must be in range
            // else calcfractionsize_decimalhelper would have returned size too big for allocate_decimalhelper
            unsigned       ti     = tableindexfromdecsize_decimalpowbase(leadingzerosize) ;
            const bigint_t * mult = s_decimal_powbase[ti] ;
            leadingzerosize -= decsize_decimalpowbase(ti) ;

            while (leadingzerosize) {
               ti = tableindexfromdecsize_decimalpowbase(leadingzerosize) ;
               leadingzerosize -= decsize_decimalpowbase(ti) ;
               err = mult_bigint(&big[0], mult, s_decimal_powbase[ti]) ;
               if (err) goto ONABORT ;
               mult = big[0] ;
               bigint_t * tempbig ;
               tempbig = big[0], big[0] = big[1], big[1] = tempbig /* swap(&big[0],&big[1]) */ ;
            }

            err = multui32_bigint(&big[0], mult, ifraction) ;
            if (err) goto ONABORT ;
         } else {
            setfromuint32_bigint(big[0], ifraction) ;
         }

         // shift big to skip bits used to fill up to next multiple of bitsperdigit_bigint()
         if (fillbitsshift) {
            err = shiftleft_bigint(&big[0], bitsperdigit_bigint()-fillbitsshift) ;
            if (err) goto ONABORT ;
         }

         // leadingzerosize could be one too small ; check it !
         err = multui32_bigint(&big[1], big[0], DIGITSBASE) ;
         if (err) goto ONABORT ;
         if (size_bigint(big[1]) <= bigintfractionsize) {
            -- size ;
         }

         (*dec)->sign_and_used_digits = (int8_t)  (value < 0 ? - (int32_t) size : (int32_t) size) ;
         (*dec)->exponent             = (int16_t) exponent ;

         if (size_bigint(big[1]) > bigintfractionsize) {
            (*dec)->digits[--size] = firstdigit_bigint(big[1]) ;
            clearfirstdigit_bigint(big[1]) ;
         }

         while (size) {
            bigint_t * tempbig ;
            tempbig = big[0], big[0] = big[1], big[1] = tempbig /* swap(&big[0],&big[1]) */ ;
            err = multui32_bigint(&big[1], big[0], DIGITSBASE) ;
            if (err) goto ONABORT ;
            if (size_bigint(big[1]) > bigintfractionsize) {
               (*dec)->digits[--size] = firstdigit_bigint(big[1]) ;
               clearfirstdigit_bigint(big[1]) ;
            } else {
               assert(0) ; // TODO: remove line after tested this scenario with double !!
               (*dec)->digits[--size] = 0 ;
            }
            removetrailingzero_bigint(big[1]) ;
         }

         for (int i = 1; i >= 0; --i) {
            err = delete_bigint(&big[i]) ;
            if (err) goto ONABORT ;
         }
      }
   } else if (integral < DIGITSBASE) {
      return setfromint32_decimal(dec, (int32_t)value, 0) ;
   } else {
      // decode only integral part
      bigint_fixed_DECLARE(3) fbig = bigint_fixed_INIT(3, 0, 0, 0, 0) ;
      err = setfromdouble_bigint((bigint_t*)&fbig, integral) ;
      if (err) goto ONABORT ;

      // 1. determine size of result
      unsigned tabidx = tableindex_decimalpowbase(size_bigint(&fbig)) ;

      if ((unsigned)tabidx >= nrelementsof(s_decimal_powbase)) {
         err = EOVERFLOW ;
         goto ONABORT ;
      }

      err = new_decimalfrombigint(&converter) ;
      if (err) goto ONABORT ;

      if (cmpmagnitude_bigint((bigint_t*)&fbig, s_decimal_powbase[tabidx]) < 0) {
         // (fbig >= DIGITSBASE && DIGITSBASE == s_decimal_powbase[0]) ==> (tabidx > 0)
         -- tabidx ;
      }
      converter->state[0].tabidx = tabidx ;
      err = divmod_bigint(&converter->quotient[0], &converter->state[0].big, (bigint_t*)&fbig, s_decimal_powbase[tabidx]) ;
      if (err) goto ONABORT ;

      size = 1u + decsize_decimalpowbase(tabidx) ;
      bigint_t * quot1 = converter->quotient[0] ;
      bigint_t * quot2 = converter->quotient[1] ;
      bigint_t * tempquot ;

      unsigned stateidx = 1 ;
      for (unsigned ti = tabidx; ti > 0; ) {
         if (cmpmagnitude_bigint(quot1, s_decimal_powbase[--ti]) < 0) {
            continue ;
         }
         converter->state[stateidx].tabidx = ti ;
         err = divmod_bigint(&quot2, &converter->state[stateidx].big, quot1, s_decimal_powbase[ti]) ;
         if (err) goto ONABORT ;
         size += decsize_decimalpowbase(ti) ;
         tempquot = quot1, quot1 = quot2, quot2 = tempquot /* swap(&quot1,&quot2) */ ;
         ++ stateidx ;
      }

      // 2. allocate and check size
      err = allocate_decimalhelper(dec, size) ;
      if (err) goto ONABORT ;

      (*dec)->sign_and_used_digits = (int8_t) (value < 0 ? - (int32_t) size : (int32_t) size) ;
      (*dec)->exponent             = 0 ;

      // 3. decompose integer into decimal parts
      (*dec)->digits[--size] = quot1->digits[0] ;

      while (stateidx > 0) {
         unsigned ti = converter->state[-- stateidx].tabidx ;
         if (iszero_bigint(converter->state[stateidx].big)) {
            // fill dec digits with 0
            for (unsigned i = decsize_decimalpowbase(ti); i > 0; --i) {
               (*dec)->digits[--size] = 0 ;
            }
         } else if (0 == ti) {
            (*dec)->digits[--size] = converter->state[stateidx].big->digits[0] ;
         } else {
            err = copy_bigint(&quot1, converter->state[stateidx].big) ;
            if (err) goto ONABORT ;
            while (ti > 0) {
               if (cmpmagnitude_bigint(quot1, s_decimal_powbase[-- ti]) < 0) {
                  // fill dec digits with 0
                  for (unsigned i = decsize_decimalpowbase(ti); i > 0; --i) {
                     (*dec)->digits[--size] = 0 ;
                  }
                  continue ;
               }
               converter->state[stateidx].tabidx = ti ;
               err = divmod_bigint(&quot2, &converter->state[stateidx].big, quot1, s_decimal_powbase[ti]) ;
               if (err) goto ONABORT ;
               tempquot = quot1, quot1 = quot2, quot2 = tempquot /* swap(&quot1,&quot2) */ ;
               ++ stateidx ;
            }
            (*dec)->digits[--size] = quot1->digits[0] ;
         }
      }
      assert(0 == size) ;
      err = delete_decimalfrombigint(&converter) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   delete_decimalfrombigint(&converter) ;
   for (int i = 1; i >= 0; --i) {
      delete_bigint(&big[i]) ;
   }
   clear_decimal(*dec) ; // result computed digit by digit
   LOG_ABORT(err) ;
   return err ;
}

int setfromchar_decimal(decimal_t *restrict* dec, const size_t nrchars, const char decimalstr[nrchars])
{
   int         err ;
   const bool  isNegSign = (nrchars && '-' == decimalstr[0]) ;
   size_t      offset    = isNegSign ;
   size_t      leadzero  = 0 ;
   size_t      trailzero = 0 ;
   int32_t     exponent  = 0 ;

   // parse 123 ...  89 "." 123  ...  789
   //       nrintdigits     nrfractdigits

   size_t startoffset = offset ;
   while (offset < nrchars && '0' == decimalstr[offset]) {
      ++ offset ;
      ++ leadzero ;
   }
   while (offset < nrchars && '0' <= decimalstr[offset] && decimalstr[offset] <= '9') {
      const bool isZero = ('0' == decimalstr[offset]) ;
      trailzero += isZero ;
      if (!isZero) trailzero = 0 ;
      ++ offset ;
   }
   const size_t nrintdigits      = offset - startoffset ;
   const bool   isFractionalPart = (offset < nrchars && '.' == decimalstr[offset]) ;
   offset += isFractionalPart ;

   startoffset = offset ;
   if (nrintdigits == leadzero) {
      while (offset < nrchars && '0' == decimalstr[offset]) {
         ++ offset ;
         ++ leadzero ;
      }
   }
   while (offset < nrchars && '0' <= decimalstr[offset] && decimalstr[offset] <= '9') {
      const bool isZero = ('0' == decimalstr[offset]) ;
      trailzero += isZero ;
      if (!isZero) trailzero = 0 ;
      ++ offset ;
   }
   const size_t nrfractdigits = offset - startoffset ;

   // check number OVERFLOW

   size_t nrdigits = (nrfractdigits + nrintdigits - leadzero - trailzero) ;
   if (nrdigits > nrdigitsmax_decimal()) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   const bool isExponentPart = (offset < nrchars && 'e' == decimalstr[offset]) ;
   offset += isExponentPart ;

   if (isExponentPart) {
      const bool isNegExponent = (offset < nrchars && '-' == decimalstr[offset]) ;
      const bool isPosExponent = (offset < nrchars && '+' == decimalstr[offset]) ;
      offset += (unsigned) (isNegExponent + isPosExponent) ;

      startoffset = offset ;

      // compute value of exponent only if number is not 0
      while (offset < nrchars && '0' <= decimalstr[offset] && decimalstr[offset] <= '9') {
         if (nrdigits) {
            exponent *= 10 ;
            exponent += (decimalstr[offset] - '0') ;

            // check exponent OVERFLOW

            if (  exponent > (expmax_decimal() + nrdigitsmax_decimal())) {
               err = EOVERFLOW ;
               goto ONABORT ;
            }
         }
         ++ offset ;
      }
      if (isNegExponent) exponent = -exponent ;
   }

   size_t nrexponentdigits = offset - startoffset ;

   // check syntax ERROR

   if (  (0 == nrfractdigits + nrintdigits)
         || (0 == nrexponentdigits && isExponentPart)
         || (offset != nrchars) /*additional characters at end of input*/) {
      err = EINVAL ;
      goto ONABORT ;
   }

   uint32_t size = 0 ;

   if (nrdigits) {

      // adjust exponent
      if (nrfractdigits >= trailzero) {
         exponent -= (int32_t) (nrfractdigits - trailzero) ;
      } else {
         exponent += (int32_t) (trailzero - nrfractdigits) ;
      }

      uint32_t nr_additional_zerodigits ;
      err = alignexponent_decimalhelper(&nr_additional_zerodigits, exponent) ;
      if (err) goto ONABORT ;

      exponent -= (int32_t) nr_additional_zerodigits ;
      exponent /= digitsperint_decimal() ;
      nrdigits += nr_additional_zerodigits ;
      size      = (uint32_t) (nrdigits + digitsperint_decimal()-1) / digitsperint_decimal() ;

      err = allocate_decimalhelper(dec, size) ;
      if (err) goto ONABORT ;

      offset = isNegSign + leadzero + (nrintdigits <= leadzero) ;

      uint32_t digitindex   = size ;
      uint32_t nrcharsdigit = (uint32_t) (nrdigits % digitsperint_decimal()) ;

      if (!nrcharsdigit) nrcharsdigit = digitsperint_decimal() ;

      do {
         -- digitindex ;
         if (0 == digitindex) {
            nrcharsdigit -= nr_additional_zerodigits ;
         }

         uint32_t digitvalue = 0 ;
         do {
            if ('.' != decimalstr[offset]) {
               digitvalue *= 10 ;
               digitvalue += (uint32_t) (decimalstr[offset] - '0') ;
               -- nrcharsdigit;
            }
            ++ offset ;
         } while (nrcharsdigit) ;

         nrcharsdigit = digitsperint_decimal() ;

         if (  0 == digitindex
               && nr_additional_zerodigits) {
            digitvalue *= power10_decimalhelper(nr_additional_zerodigits) ;
         }

         (*dec)->digits[digitindex] = digitvalue ;
      } while (digitindex) ;
   }

   (*dec)->sign_and_used_digits = (int8_t)  (isNegSign ? - (int32_t) size : (int32_t) size) ;
   (*dec)->exponent             = (int16_t) exponent ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

// group: ternary operations

int add_decimal(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec)
{
   int err ;
   const bool lNegSign = isnegative_decimal(ldec) ;
   const bool rNegSign = isnegative_decimal(rdec) ;

   if (lNegSign == rNegSign) {
      err = add_decimalhelper(result, ldec, rdec) ;
   } else {
      err = sub_decimalhelper(result, ldec, rdec) ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int sub_decimal(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec)
{
   int err ;
   const bool lNegSign = isnegative_decimal(ldec) ;
   const bool rNegSign = isnegative_decimal(rdec) ;

   if (lNegSign == rNegSign) {
      err = sub_decimalhelper(result, ldec, rdec) ;
   } else {
      err = add_decimalhelper(result, ldec, rdec) ;
   }
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int mult_decimal(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec)
{
   int err ;
   uint8_t        lsize     = size_decimal(ldec) ;
   uint8_t        rsize     = size_decimal(rdec) ;
   const bool     isNegSign = (bool) (isnegative_decimal(ldec) ^ isnegative_decimal(rdec)) ;
   int32_t        exponent  = (int32_t)ldec->exponent + rdec->exponent ;
   const uint32_t * ldigits = ldec->digits ;
   const uint32_t * rdigits = rdec->digits ;

   // skip trailing zero in both operands
   while (lsize && 0 == ldigits[0]) {
      -- lsize  ;
      ++ ldigits ;
      ++ exponent ;
   }
   while (rsize && 0 == rdigits[0]) {
      -- rsize ;
      ++ rdigits ;
      ++ exponent ;
   }

   if (!rsize  || !lsize) {
      // ldec == 0 || rdec == 0
      clear_decimal(*result) ;
      return 0 ;
   }

   uint32_t size = (uint32_t)lsize + rsize ;

   if (  abs_int(exponent) > INT16_MAX) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   err = allocate_decimalhelper(result, size) ;
   if (err) goto ONABORT ;

   err = multsplit_decimalhelper(result, lsize, ldigits, rsize, rdigits) ;
   if (err) goto ONABORT ;

   (*result)->sign_and_used_digits = (int8_t)  (isNegSign ? - (*result)->sign_and_used_digits : (*result)->sign_and_used_digits) ;
   (*result)->exponent             = (int16_t) ((*result)->exponent + exponent) ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int div_decimal(decimal_t *restrict* result, const decimal_t * ldec, const decimal_t * rdec, uint8_t result_size)
{
   int err ;
   uint8_t        lsize     = size_decimal(ldec) ;
   uint8_t        rsize     = size_decimal(rdec) ;
   uint8_t        maxsize ;
   const bool     isNegSign = (bool) (isnegative_decimal(ldec) ^ isnegative_decimal(rdec)) ;
   int32_t        exponent  = (int32_t)ldec->exponent - rdec->exponent + (int32_t)lsize - (int32_t)rsize ;
   const uint32_t * ldigits = ldec->digits ;
   const uint32_t * rdigits = rdec->digits ;
   decimal_t      * intermediate = 0 ;

   result_size = (uint8_t) (result_size + (0 == result_size)) ;
   if (result_size > sizemax_decimal()) result_size = sizemax_decimal() ;

   // skip trailing zero in both operands
   while (lsize && 0 == ldigits[0]) {
      -- lsize  ;
      ++ ldigits ;
   }

   while (rsize && 0 == rdigits[0]) {
      -- rsize ;
      ++ rdigits ;
   }

   if (!rsize) {
      err = EINVAL ;
      goto ONABORT ;
   }

   if (!lsize) {
      clear_decimal(*result) ;
      return 0 ;
   }

   exponent -= (int32_t)(result_size-1) ;

   err = allocate_decimalhelper(result, result_size) ;
   if (err) goto ONABORT ;

   if (rsize == 1) {
      err = divi32_decimalhelper(*result, isNegSign, exponent, lsize, ldigits, rdigits[0], result_size) ;
      if (err) goto ONABORT ;
      return 0 ;
   }

   maxsize = (uint8_t) (lsize > rsize ? lsize : rsize) ;

   err = allocate_decimalhelper(&intermediate, maxsize) ;
   if (err) goto ONABORT ;

   uint32_t offset = (uint32_t)maxsize - (uint32_t)lsize ;

   if (offset > 0/*rsize > lsize*/) {
      memset(&intermediate->digits[0], 0, offset * sizeof(intermediate->digits[0])) ;
   }
   memcpy(&intermediate->digits[offset], ldigits, (uint32_t)lsize * sizeof(intermediate->digits[0])) ;

   err = div_decimalhelper(*result, isNegSign, exponent, maxsize, intermediate->digits, rsize, rdigits, result_size) ;
   if (err) goto ONABORT ;

   err = delete_decimal(&intermediate) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   delete_decimal(&intermediate) ;
   LOG_ABORT(err) ;
   return err ;
}

int divi32_decimal(decimal_t *restrict* result, const decimal_t * ldec, int32_t rdivisor, uint8_t result_size)
{
   int err ;
   uint32_t divisor = abs_int(rdivisor) ;

   VALIDATE_INPARAM_TEST(divisor != 0 && divisor <= DIGITSBASE, ONABORT, ) ;

   result_size = (uint8_t) (result_size + (0 == result_size)) ;
   if (result_size > sizemax_decimal()) result_size = sizemax_decimal() ;

   uint8_t        lsize     = size_decimal(ldec) ;
   const bool     isNegSign = (bool) (isnegative_decimal(ldec) ^ (rdivisor < 0)) ;
   int32_t        exponent  = (int32_t)ldec->exponent + (int32_t)lsize - 1 ;
   const uint32_t * ldigits = ldec->digits ;

   // skip trailing zero in both operands
   while (lsize && 0 == ldigits[0]) {
      -- lsize  ;
      ++ ldigits ;
   }

   exponent -= (int32_t)(result_size-1) ;

   if (divisor == DIGITSBASE) {
      -- exponent ;
      divisor = 1 ;
   }

   err = allocate_decimalhelper(result, result_size) ;
   if (err) goto ONABORT ;

   err = divi32_decimalhelper(*result, isNegSign, exponent, lsize, ldigits, divisor, result_size) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_decimaltables(void)
{
   bigint_t    * big   = 0 ;
   bigint_t    * temp1 = 0 ;

   // prepare
   TEST(0 == new_bigint(&big, nrdigitsmax_bigint())) ;
   TEST(0 == new_bigint(&temp1, nrdigitsmax_bigint())) ;

   // TEST bigint_t uses 32 bit integer same size as decimal_t
   //      used implicit in tableindex_decimalpowbase
   TEST(bitsperint_decimal() == 32) ;
   TEST(bitsperint_decimal() == bitsperdigit_bigint()) ;

   // TEST power10_decimalhelper
   TEST(0 == power10_decimalhelper(10)) ;
   for (unsigned i = 0; i < 10; ++i) {
      const uint32_t expect = (uint32_t)pow(10, i) ;
      TEST(expect == power10_decimalhelper(i)) ;
   }

   // TEST power10_decimalhelper: invalid argument returns 0
   unsigned invalid_args[] = { (unsigned)-1, (unsigned)INT_MIN, UINT_MAX, 10 } ;
   for (unsigned i = 0; i < nrelementsof(invalid_args); ++i) {
      TEST(0 == power10_decimalhelper(invalid_args[i])) ;
   }

   // TEST s_decimal_powbase: entries has values == pow(10, digitsperint_decimal() * pow(2,i))
   setfromuint32_bigint(big, DIGITSBASE) ;
   for (unsigned i = 0; i < nrelementsof(s_decimal_powbase); ++i) {
      TEST(0 == cmp_bigint(big, s_decimal_powbase[i])) ;
      TEST(0 == mult_bigint(&temp1, big, big)) ;
      removetrailingzero_bigint(temp1) ;
      TEST(0 == copy_bigint(&big, temp1)) ;
   }

   // TEST s_decimal_powbase: pow(10, nrdigitsmax_decimal()) fits in BIGINT_MAXSIZE digits
   unsigned nrdigitsmax = 0 ;
   setfromuint32_bigint(big, 1) ;
   for (unsigned e10 = digitsperint_decimal(), i = 0; i < nrelementsof(s_decimal_powbase); ++i, e10 *= 2) {
      TEST(0 == mult_bigint(&temp1, big, s_decimal_powbase[i])) ;
      removetrailingzero_bigint(temp1) ;
      TEST(0 == copy_bigint(&big, temp1)) ;
      nrdigitsmax += e10 ;
   }
   TEST(nrdigitsmax == nrdigitsmax_decimal()) ;
   TEST(BIGINT_MAXSIZE == size_bigint(big)) ;

   // TEST s_decimal_powbase: decsize_decimalpowbase
   for (unsigned e10 = digitsperint_decimal(), i = 0; i < nrelementsof(s_decimal_powbase); ++i, e10 *= 2) {
      TEST(e10 == decsize_decimalpowbase(i) * digitsperint_decimal()) ;
   }

   // TEST s_decimal_powbase: number 15 used in tableindex_decimalpowbase
   TEST((const bigint_t*)&s_decimal_10raised144 == s_decimal_powbase[4]) ;
   for (unsigned i = 4, expectsize = 15; i < nrelementsof(s_decimal_powbase); ++i, expectsize *= 2) {
      // pow(10,16*9)) needs only 15 integer digits, pow(10,32*9)) only 30, ...
      TEST(expectsize == size_bigint(s_decimal_powbase[i])) ;
   }

   // TEST s_decimal_powbase: last entry overflows if used in fractional multiplication
   // => algorithm can rely on fact that s_decimal_powbase is big enough for multiplication
   uint32_t zerosize = 0 ;
   uint32_t nrzerobits   = bitsperdigit_bigint()*size_bigint(s_decimal_powbase[nrelementsof(s_decimal_powbase)-1]) ;
   uint32_t fractionbits = nrzerobits + 1 ;
   uint32_t fsize = calcfractionsize_decimalhelper(&zerosize, fractionbits, nrzerobits) ;
   TEST(fsize == sizemax_decimal() + 1/*OVERFLOW*/) ;
   TEST(0     == zerosize) ;
   // TEST s_decimal_powbase: second last entry overflows not if used in fractional multiplication
   nrzerobits   = bitsperdigit_bigint()*size_bigint(s_decimal_powbase[nrelementsof(s_decimal_powbase)-2]) ;
   fractionbits = nrzerobits + 1 ;
   fsize = calcfractionsize_decimalhelper(&zerosize, fractionbits, nrzerobits) ;
   TEST(fsize == 75/*adapt this value to table size*/) ;
   TEST(fsize  < sizemax_decimal()) ;
   TEST(zerosize == decsize_decimalpowbase(nrelementsof(s_decimal_powbase)-2)) ;

   // TEST s_decimal_powbase: tableindex_decimalpowbase values <= BIGINT_MAXSIZE returns the correct index
   for (unsigned size = 0, expected = 0; size <= BIGINT_MAXSIZE; ++size) {
      if (  expected <  nrelementsof(s_decimal_powbase)-1
            && size  >= size_bigint(s_decimal_powbase[expected+1])) {
         ++ expected ;
      }
      const unsigned ti = tableindex_decimalpowbase(size) ;
      TEST(ti == expected) ;
   }

   // TEST s_decimal_powbase: tableindexfromdecsize_decimalpowbase
   TEST(0 == tableindexfromdecsize_decimalpowbase(0)) ;
   TEST(0 == tableindexfromdecsize_decimalpowbase(1)) ;
   for (unsigned decsize = 1, expected = 0; expected < nrelementsof(s_decimal_powbase); ++expected, decsize *= 2) {
      TEST(expected == tableindexfromdecsize_decimalpowbase(decsize)) ;
      TEST(expected == tableindexfromdecsize_decimalpowbase(decsize + decsize-1)) ;
   }

   // TEST s_decimal_powbase: tableindex_decimalpowbase values > BIGINT_MAXSIZE
   TEST(nrelementsof(s_decimal_powbase)     == tableindex_decimalpowbase(BIGINT_MAXSIZE+1)) ;
   TEST(nrelementsof(s_decimal_powbase)+1   == tableindex_decimalpowbase(BIGINT_MAXSIZE+129)) ;
   TEST(2*nrelementsof(s_decimal_powbase)+2 == tableindex_decimalpowbase((uint32_t)2*INT16_MAX)) ;

   // unprepare
   TEST(0 == delete_bigint(&big)) ;
   TEST(0 == delete_bigint(&temp1)) ;

   return 0 ;
ONABORT:
   delete_bigint(&big) ;
   delete_bigint(&temp1) ;
   return EINVAL ;
}

static int test_helper(void)
{
   uint32_t fsize ;
   uint32_t zerosize ;
   uint32_t nrzerodigits ;
   uint32_t nrzerobits ;
   bigint_t * big[3] = { 0, 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == new_bigint(&big[i], nrdigitsmax_bigint())) ;
   }

   // TEST nrzerobits2nrdigits_decimalhelper, nrzerodigits2nrbits_decimalhelper: 1 bit mantissa
   setfromuint32_bigint(big[0], 5 * 0x1/*1 bit mantissa*/) ;
   setfromuint32_bigint(big[1], 10 * 1) ;
   for (unsigned i = 1, nrdigits = 1, expect_nrleadingnrzerobits = 0; i < 1080/*works for size of double*/; ++i) {
      const uint32_t decexpo = 1 + i ;
      TEST(0 == multui32_bigint(&big[2], big[0], 5)) ;
      TEST(0 == copy_bigint(&big[0], big[2])) ;
      if (cmp_bigint(big[1], big[0]) <= 0) {
         TEST(0 == multui32_bigint(&big[2], big[1], 10)) ;
         TEST(0 == copy_bigint(&big[1], big[2])) ;
         ++ nrdigits ;
      } else { // leading zero digits increments
         expect_nrleadingnrzerobits = i ;
      }
      uint32_t expect_nrleadingzerodigits = decexpo - nrdigits ;
      nrzerobits   = nrzerodigits2nrbits_decimalhelper(expect_nrleadingzerodigits) ;
      nrzerodigits = nrzerobits2nrdigits_decimalhelper(i) ;
      // nrzerobits2nrdigits_decimalhelper is either exact or one digit too small (for 1 bit mantissa)
      if (expect_nrleadingzerodigits != nrzerodigits) {
         nrzerodigits = nrzerobits2nrdigits_decimalhelper(i+1/*one zero bit more*/) ;
      }
      TEST(expect_nrleadingzerodigits == nrzerodigits) ;
      TEST(expect_nrleadingnrzerobits == nrzerobits) ;
   }

   // TEST nrzerobits2nrdigits_decimalhelper, nrzerodigits2nrbits_decimalhelper: 64 bit mantissa
   setfromuint32_bigint(big[1], 0xffffffff/*64 bit mantissa*/) ;
   TEST(0 == shiftleft_bigint(&big[1], 32)) ;
   setfromuint32_bigint(big[2], 0xffffffff) ;
   TEST(0 == add_bigint(&big[0], big[1], big[2])) ;   // 0xffffffffffffffff
   setfromuint32_bigint(big[1], 1) ;                  // pow(10,X)
   for (unsigned i = 0; i < 64; ++i) {
      TEST(0 == multui32_bigint(&big[2], big[0], 5)) ;
      TEST(0 == copy_bigint(&big[0], big[2])) ;
      TEST(cmp_bigint(big[1], big[0]) <= 0) ;
      TEST(0 == multui32_bigint(&big[2], big[1], 10)) ;
      TEST(0 == copy_bigint(&big[1], big[2])) ;
   }
   TEST(cmp_bigint(big[1], big[0]) > 0) ;
   for (unsigned i = 1, nrdigits = 64, expect_nrleadingnrzerobits = 0; i < 1080/*works for size of double*/; ++i) {
      const uint32_t decexpo = 64 + i ;
      TEST(0 == multui32_bigint(&big[2], big[0], 5)) ;
      TEST(0 == copy_bigint(&big[0], big[2])) ;
      if (cmp_bigint(big[1], big[0]) <= 0) {
         TEST(0 == multui32_bigint(&big[2], big[1], 10)) ;
         TEST(0 == copy_bigint(&big[1], big[2])) ;
         ++ nrdigits ;
      } else { // leading zero digits increments
         expect_nrleadingnrzerobits = i ;
      }
      const uint32_t expect_nrleadingzerodigits = decexpo - nrdigits ;
      nrzerobits   = nrzerodigits2nrbits_decimalhelper(expect_nrleadingzerodigits) ;
      nrzerodigits = nrzerobits2nrdigits_decimalhelper(i) ;
      TEST(expect_nrleadingzerodigits == nrzerodigits) ;
      TEST(expect_nrleadingnrzerobits == nrzerobits+(nrzerobits!=0)/*returns one bit too little*/) ;
   }

   // TEST nrzerobits2decsize_decimalhelper
   for (uint32_t i = 0, expect_zerosize; i < 1080/*works for size of double*/; ++i) {
      expect_zerosize = nrzerobits2nrdigits_decimalhelper(i) / digitsperint_decimal();
      zerosize        = nrzerobits2decsize_decimalhelper(i) ;
      TEST(expect_zerosize == zerosize) ;
   }

   // TEST calcfractionsize_decimalhelper: fractional part of (IEEE) double & float fit in decimal_t
   int fexp ;
   frexpf(FLT_MIN, &fexp) ;
   fexp = 1 + -fexp ;
   TEST(fexp > 0) ;
   fsize = calcfractionsize_decimalhelper(&zerosize, (uint32_t)fexp+1+24, (uint32_t)fexp+1) ;
   TEST(fsize == 13) ;
   TEST(fsize < sizemax_decimal()) ;
   frexp(DBL_MIN, &fexp) ;
   fexp = 1 + -fexp ;
   TEST(fexp > 0) ;
   fsize = calcfractionsize_decimalhelper(&zerosize, (uint32_t)fexp+1+54, (uint32_t)fexp+1) ;
   TEST(fsize == 86) ;
   TEST(fsize < sizemax_decimal()) ;

   // TEST calcfractionsize_decimalhelper: value 1
   zerosize = 1 ;
   TEST(1 == calcfractionsize_decimalhelper(&zerosize, 1, 0)) ;
   TEST(0 == zerosize) ;

   // TEST calcfractionsize_decimalhelper: value sizemax_decimal()
   zerosize = 1 ;
   TEST(sizemax_decimal() == calcfractionsize_decimalhelper(&zerosize, nrdigitsmax_decimal(), 0)) ;
   TEST(0 == zerosize) ;

   // TEST calcfractionsize_decimalhelper: value nrleadingzerobits out of range
   zerosize = 1 ;
   TEST(1+sizemax_decimal() == calcfractionsize_decimalhelper(&zerosize, 0, 0)) ;
   TEST(0 == zerosize) ;
   zerosize = 1 ;
   TEST(1+sizemax_decimal() == calcfractionsize_decimalhelper(&zerosize, nrdigitsmax_decimal(), nrdigitsmax_decimal())) ;
   TEST(0 == zerosize) ;

   // TEST calcfractionsize_decimalhelper: return values are valid
   for (unsigned bits = 1; bits <= nrdigitsmax_decimal(); ++bits) {
      fsize = calcfractionsize_decimalhelper(&zerosize, bits, 0) ;
      TEST(0 == zerosize) ;
      TEST(fsize == (bits+digitsperint_decimal()-1)/digitsperint_decimal()) ;
      for (unsigned zerobits = 1; zerobits < bits; ++zerobits) {
         uint32_t zerosize2 ;
         uint32_t fsize2 = calcfractionsize_decimalhelper(&zerosize2, bits, zerobits) ;
         TEST(zerosize2 == nrzerobits2decsize_decimalhelper(zerobits)) ;
         if (zerosize2 > zerosize) {
            TEST(fsize2  == fsize-1) ;
            fsize    = fsize2 ;
            zerosize = zerosize2 ;
         } else {
            TEST(fsize2 == fsize) ;
         }
      }
   }

   // TEST div3by2digits_decimalhelper
   decimal_divstate_t divteststate[] = {
      {  .dividend = (DIGITSBASE-1) * (uint64_t)DIGITSBASE + DIGITSBASE-1, .nextdigit = DIGITSBASE-1, .divisor = (uint64_t)(DIGITSBASE-1)*DIGITSBASE + (DIGITSBASE-1) }
      ,{ .dividend = (DIGITSBASE-1) * (uint64_t)DIGITSBASE + DIGITSBASE-2, .nextdigit = DIGITSBASE-1, .divisor = (uint64_t)(DIGITSBASE-1)*DIGITSBASE + (DIGITSBASE-1) }
      ,{ .dividend = 0  * (uint64_t)DIGITSBASE + DIGITSBASE-1,    .nextdigit = DIGITSBASE-1, .divisor = (uint64_t)1*DIGITSBASE + 0 }
      ,{ .dividend = 1  * (uint64_t)DIGITSBASE + 0,               .nextdigit = DIGITSBASE-1, .divisor = (uint64_t)1*DIGITSBASE + 0 }
      ,{ .dividend = 123456789 * (uint64_t)DIGITSBASE + 993456789,.nextdigit = DIGITSBASE-1, .divisor = (uint64_t)(DIGITSBASE-1)*DIGITSBASE + (DIGITSBASE-1) }
      ,{ .dividend = 100 * (uint64_t)DIGITSBASE + DIGITSBASE-1,   .nextdigit = DIGITSBASE-1, .divisor = (uint64_t)(DIGITSBASE-1)*DIGITSBASE + (DIGITSBASE-1) }
      ,{ .dividend = 550044 * (uint64_t)DIGITSBASE + 887766,      .nextdigit = DIGITSBASE-1, .divisor = ((uint64_t)550044*DIGITSBASE + 887766)*11 }
   } ;
   div3by2digits_decimalhelper(&divteststate[0]) ;
   TEST(DIGITSBASE == divteststate[0].nextdigit) ;
   div3by2digits_decimalhelper(&divteststate[1]) ;
   TEST(DIGITSBASE-1 == divteststate[1].nextdigit) ;
   div3by2digits_decimalhelper(&divteststate[2]) ;
   TEST(DIGITSBASE-1 == divteststate[2].nextdigit) ;
   div3by2digits_decimalhelper(&divteststate[3]) ;
   TEST(DIGITSBASE == divteststate[3].nextdigit) ;
   div3by2digits_decimalhelper(&divteststate[4]) ;
   TEST(123456789 == divteststate[4].nextdigit) ;
   div3by2digits_decimalhelper(&divteststate[5]) ;
   TEST(      101 == divteststate[5].nextdigit) ;
   div3by2digits_decimalhelper(&divteststate[6]) ;
   TEST(DIGITSBASE/11 == divteststate[6].nextdigit) ;

   // unprepare
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == delete_bigint(&big[i])) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      delete_bigint(&big[i]) ;
   }
   return EINVAL ;
}

static int test_initfree(void)
{
   decimal_t   * dec = 0 ;

   // TEST init, double free
   TEST(0 == new_decimal(&dec, 1)) ;
   TEST(0 != dec) ;
   TEST(0 == delete_decimal(&dec)) ;
   TEST(0 == dec) ;
   TEST(0 == delete_decimal(&dec)) ;
   TEST(0 == dec) ;

   // TEST init: for nrdecimaldigits in [1 .. 127*9]
   TEST(9 == digitsperint_decimal()) ;
   for (uint16_t nrdecimaldigits = 1; nrdecimaldigits <= nrdigitsmax_decimal(); ++nrdecimaldigits) {
      TEST(0 == new_decimal(&dec, nrdecimaldigits)) ;
      TEST(0 != dec) ;
      if (nrdecimaldigits) {
         TEST((nrdecimaldigits+8)/9 == dec->size_allocated) ;
      } else {
         TEST(1 == dec->size_allocated) ;
      }
      TEST(0 == dec->sign_and_used_digits) ;
      TEST(0 == size_decimal(dec)) ;
      TEST(0 == nrdigits_decimal(dec)) ;
      TEST(0 == dec->exponent) ;
      int32_t decexp = 1 ;
      TEST(0 == first9digits_decimal(dec, &decexp)) ;
      TEST(0 == decexp) ;
      TEST(0 == delete_decimal(&dec)) ;
      TEST(0 == dec) ;
   }

   // TEST init: EINVAL
   TEST(EINVAL == new_decimal(&dec,0)) ;
   TEST(EINVAL == new_decimal(&dec,nrdigitsmax_decimal()+1)) ;

   // TEST init: ENOMEM
   test_errortimer_t errtimer ;
   TEST(0 == init_testerrortimer(&errtimer, 1, ENOMEM)) ;
   setresizeerr_mmtest(mmcontext_mmtest(), &errtimer) ;
   TEST(ENOMEM == new_decimal(&dec, 1)) ;

   // TEST free: ENOMEM
   TEST(0 == new_decimal(&dec, 1)) ;
   TEST(0 == init_testerrortimer(&errtimer, 1, ENOMEM)) ;
   setfreeerr_mmtest(mmcontext_mmtest(), &errtimer) ;
   TEST(ENOMEM == delete_decimal(&dec)) ;
   TEST(0 == dec) ;

   // TEST bitsperint_decimal
   TEST(32 == bitsperint_decimal()) ;

   // TEST digitsperint_decimal
   TEST(9 == digitsperint_decimal()) ;

   // TEST expmax_decimal
   TEST(294903 == expmax_decimal()) ;

   // TEST nrdigitsmax_decimal
   TEST(9*127 == nrdigitsmax_decimal()) ;

   // TEST sizemax_decimal
   TEST(127 == sizemax_decimal()) ;

   // TEST sign_decimal
   TEST(0 == new_decimal(&dec, 127*9)) ;
   for (unsigned i = 0; i <= dec->size_allocated; ++i) {
      dec->sign_and_used_digits = (int8_t) i ;
      TEST((0 != i) == sign_decimal(dec)) ;
      dec->sign_and_used_digits = (int8_t) -(int) i ;
      TEST(-(0 != i) == sign_decimal(dec)) ;
   }
   TEST(0 == delete_decimal(&dec)) ;

   // TEST size_decimal
   TEST(0 == new_decimal(&dec, 127*9)) ;
   for (unsigned i = 0; i <= dec->size_allocated; ++i) {
      dec->sign_and_used_digits = (int8_t) i ;
      TEST(i == size_decimal(dec)) ;
      dec->sign_and_used_digits = (int8_t) -(int) i ;
      TEST(i == size_decimal(dec)) ;
   }
   TEST(0 == delete_decimal(&dec)) ;

   // TEST nrdigits_decimal
   TEST(0 == new_decimal(&dec, 127*9)) ;
   dec->sign_and_used_digits = 0 ;
   TEST(0 == nrdigits_decimal(dec)) ;
   for (unsigned i = 1; i <= dec->size_allocated; ++i) {
      for (uint32_t value = 1, ndigit = 1, nrdigit = 1; ndigit != 1000000000; ndigit *= 10, value += ndigit, ++ nrdigit) {
         dec->sign_and_used_digits = (int8_t) i ;
         dec->digits[i-1] = value ;
         TEST(nrdigits_decimal(dec) == digitsperint_decimal()*(i-1) + nrdigit) ;
         dec->digits[i-1] = 9 * value ;
         TEST(nrdigits_decimal(dec) == digitsperint_decimal()*(i-1) + nrdigit) ;
         dec->sign_and_used_digits = (int8_t) -(int) i ;
         dec->digits[i-1] = ndigit ;
         TEST(nrdigits_decimal(dec) == digitsperint_decimal()*(i-1) + nrdigit) ;
      }
   }
   TEST(0 == delete_decimal(&dec)) ;

   // TEST clear_decimal
   TEST(0 == new_decimal(&dec, nrdigitsmax_decimal())) ;
   for (unsigned di = 0; di < dec->size_allocated; ++di) {
      dec->digits[di] = 1 + di ;
   }
   for (int i = 1; i < dec->size_allocated; ++i) {
      for (int s = -1; s <= +2; s += 2) {
         dec->sign_and_used_digits = (int8_t)  (s * i) ;
         dec->exponent             = (int16_t) (i * (-s)) ;
         TEST(size_decimal(dec)     == i) ;
         TEST(exponent_decimal(dec) == digitsperint_decimal() * i * (-s)) ;
         TEST(sign_decimal(dec)     == s) ;
         clear_decimal(dec) ;
         TEST(sign_decimal(dec)     == 0) ;
         TEST(nrdigits_decimal(dec) == 0) ;
         TEST(exponent_decimal(dec) == 0) ;
         // allocation & content of digits not changed
         TEST(dec->size_allocated == sizemax_decimal()) ;
         for (unsigned di = 0; di < dec->size_allocated; ++di) {
            TEST(dec->digits[di] == 1 + di) ;
         }
      }
   }
   TEST(0 == delete_decimal(&dec)) ;

   return 0 ;
ONABORT:
   delete_decimal(&dec) ;
   return EINVAL ;
}

static int test_signops(void)
{
   decimal_t   * dec = 0 ;

   // TEST negate_decimal, setnegative_decimal, setpositive_decimal
   TEST(0 == new_decimal(&dec, 127*9)) ;
   dec->sign_and_used_digits = 0 ;
   negate_decimal(dec) ;
   TEST(0 == dec->sign_and_used_digits) ;
   setnegative_decimal(dec) ;
   TEST(0 == dec->sign_and_used_digits) ;
   setpositive_decimal(dec) ;
   TEST(0 == dec->sign_and_used_digits) ;
   for (unsigned i = 1; i <= dec->size_allocated; ++i) {
      int8_t n =   (int8_t) - (int) i ;
      int8_t p =   (int8_t) i ;
      dec->sign_and_used_digits = p ;
      negate_decimal(dec) ;
      TEST(-1 == sign_decimal(dec)) ;
      TEST(n  == dec->sign_and_used_digits) ;
      negate_decimal(dec) ;
      TEST(+1 == sign_decimal(dec)) ;
      TEST(p  == dec->sign_and_used_digits) ;
      setnegative_decimal(dec) ;
      TEST(-1 == sign_decimal(dec)) ;
      TEST(n  == dec->sign_and_used_digits) ;
      setpositive_decimal(dec) ;
      TEST(+1 == sign_decimal(dec)) ;
      TEST(p  == dec->sign_and_used_digits) ;
   }
   TEST(0 == delete_decimal(&dec)) ;

   return 0 ;
ONABORT:
   delete_decimal(&dec) ;
   return EINVAL ;
}

static int test_copy(void)
{
   decimal_t   * dec  = 0 ;
   decimal_t   * copy = 0 ;

   // prepare
   TEST(0 == new_decimal(&dec, nrdigitsmax_decimal())) ;
   TEST(0 == new_decimal(&copy, 1)) ;

   // TEST copy_decimal: value 0
   TEST(0 == setfromint32_decimal(&copy, -1, -9)) ;
   TEST(0 == setfromint32_decimal(&dec, 0, 0)) ;
   TEST(0 == copy_decimal(&copy, dec)) ;
   TEST(1 == copy->size_allocated) ;
   TEST(0 == sign_decimal(copy)) ;
   TEST(0 == exponent_decimal(copy)) ;

   // TEST copy_decimal
   struct {
      const int8_t   nrdigits ;
      const uint32_t digits[10] ;
   } testvalues[] = {
      { 1, { 1 } }
      ,{ 2, { 123456789, 1 } }
      ,{ 3, { 2, 123456789, 1 } }
      ,{ 4, { 4, 2, 123456789, 1 } }
      ,{ 5, { 10000, 4, 2, 123456789, 1 } }
      ,{ 6, { 10000, 10004, 2001, 123456789, 999999999 } }
      ,{ 7, { 10000, 10004, 2001, 123456789, 999999999, 88888 } }
      ,{ 8, { 222, 10000, 10004, 2001, 123456789, 999999999, 88888 } }
      ,{ 9, { 3, 222, 10000, 10004, 0, 0, 999999999, 88888 } }
      ,{ 10, { 3, 222, 10000, 0, 2001, 123456789, 999999999, 0, 4 } }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues); ++tvi) {
      for (int s = -1; s <= 1; s += 2) {
         for (int32_t e = -INT16_MAX; e <= INT16_MAX; e += INT16_MAX) {
            const int n = testvalues[tvi].nrdigits ;
            dec->sign_and_used_digits = (int8_t) (s * n) ;
            dec->exponent             = (int16_t) e ;
            for (int i = 0; i < n; ++i) {
               dec->digits[i] = testvalues[tvi].digits[i] ;
            }
            TEST(0 == delete_decimal(&copy)) ;
            TEST(0 == new_decimal(&copy, 1)) ;
            TEST(0 == copy_decimal(&copy, dec)) ;
            TEST(n == copy->size_allocated) ;
            TEST(e == copy->exponent) ;
            TEST(s == sign_decimal(copy)) ;
            for (int i = 0; i < n; ++i) {
               TEST(testvalues[tvi].digits[i] == copy->digits[i]) ;
            }
         }
      }
   }

   // unprepare
   TEST(0 == delete_decimal(&dec)) ;
   TEST(0 == delete_decimal(&copy)) ;

   return 0 ;
ONABORT:
   delete_decimal(&dec) ;
   delete_decimal(&copy) ;
   return EINVAL ;
}

static int32_t alignexp_test(int32_t decexp)
{
   int32_t diff = decexp % digitsperint_decimal() ;

   if (diff == 0) {
      return decexp ;
   }

   if (diff < 0) {
      return (int32_t) (decexp - diff - digitsperint_decimal()) ;
   }

   return (int32_t) (decexp - diff) ;
}

static int test_setfromint(void)
{
   decimal_t   * dec = 0 ;

   // TEST setfromint32_decimal (values 1 and 0)
   TEST(0 == new_decimal(&dec, 1)) ;
   TEST(0 == setfromint32_decimal(&dec, 1, expmax_decimal())) ;
   TEST(1 == nrdigits_decimal(dec)) ;
   TEST(1 == sign_decimal(dec)) ;
   TEST(1 == dec->sign_and_used_digits) ;
   TEST(INT16_MAX == dec->exponent) ;
   TEST(1 == dec->digits[0]) ;
   TEST(0 == setfromint32_decimal(&dec, 0, expmax_decimal())) ;
   TEST(0 == nrdigits_decimal(dec)) ;
   TEST(0 == sign_decimal(dec)) ;
   TEST(0 == dec->sign_and_used_digits) ;
   TEST(0 == dec->exponent) ;
   TEST(0 == delete_decimal(&dec)) ;

   // TEST setfromint64_decimal (values 1 and 0)
   TEST(0 == new_decimal(&dec, 1)) ;
   TEST(0 == setfromint64_decimal(&dec, 1, expmax_decimal())) ;
   TEST(1 == nrdigits_decimal(dec)) ;
   TEST(1 == sign_decimal(dec)) ;
   TEST(1 == dec->sign_and_used_digits) ;
   TEST(INT16_MAX == dec->exponent) ;
   TEST(1 == dec->digits[0]) ;
   TEST(0 == setfromint64_decimal(&dec, 0, expmax_decimal())) ;
   TEST(0 == nrdigits_decimal(dec)) ;
   TEST(0 == sign_decimal(dec)) ;
   TEST(0 == dec->sign_and_used_digits) ;
   TEST(0 == dec->exponent) ;
   TEST(0 == delete_decimal(&dec)) ;

   // TEST setfromint32_decimal, setfromint64_decimal: 32 bit values
   int32_t testvalues[]   = { 1, 2, 100, 999, 999999, 999000000, DIGITSBASE-1, DIGITSBASE, INT32_MAX } ;
   int32_t testexponent[] = { expmax_decimal(), expmax_decimal()-8, 1, 0, -1, -(expmax_decimal()-8), -expmax_decimal() } ;
   for (unsigned i = 0; i < nrelementsof(testvalues); ++i) {
      for (unsigned ei = 0; ei < nrelementsof(testexponent); ++ei) {
         for (int s = -1; s <= +1; s += 2) {
            for (int bits=32; bits <= 64; bits += 32) {
               int32_t  expdiff      = testexponent[ei] - alignexp_test(testexponent[ei]) ;
               uint64_t shifteddigit = (uint64_t)testvalues[i] * power10_decimalhelper((uint32_t)expdiff) ;
               TEST(0 == new_decimal(&dec, 1)) ;
               if (32 == bits) {
                  TEST(0 == setfromint32_decimal(&dec, s * testvalues[i], testexponent[ei])) ;
               } else {
                  TEST(0 == setfromint64_decimal(&dec, s * testvalues[i], testexponent[ei])) ;
               }
               TEST(dec->size_allocated   == 1 + (shifteddigit > 999999999)) ;
               TEST(dec->sign_and_used_digits == s * (1 + (shifteddigit > 999999999))) ;
               TEST(size_decimal(dec)     == 1 + (shifteddigit > 999999999)) ;
               TEST(nrdigits_decimal(dec) == 1 + log10_int((uint32_t)testvalues[i]) + (uint32_t)expdiff) ;
               TEST(sign_decimal(dec)     == s) ;
               TEST(exponent_decimal(dec) == alignexp_test(testexponent[ei])) ;
               TEST(dec->digits[0]     == (uint32_t) (shifteddigit % 1000000000)) ;
               if (1 < size_decimal(dec)) {
                  TEST(dec->digits[1]  == (uint32_t) (shifteddigit / 1000000000)) ;
               }
               int32_t expectexp = alignexp_test(testexponent[ei]) ;
               int64_t expectval = (int64_t)shifteddigit ;
               while (expectval > 999999999) {
                  expectval /= 10 ;
                  ++ expectexp ;
               }
               expectval *= s ;
               int32_t decexp  = 2 ;
               TEST(expectval == first9digits_decimal(dec, &decexp)) ;
               TEST(expectexp == decexp) ;
               expectexp = alignexp_test(testexponent[ei]) ;
               expectval = s * (int64_t)shifteddigit ;
               decexp    = 2 ;
               TEST(expectval == first18digits_decimal(dec, &decexp)) ;
               TEST(expectexp == decexp) ;
               TEST(0 == delete_decimal(&dec)) ;
            }
         }
      }
   }

   // TEST setfromint64_decimal: sign, exponent, values >= DIGITSBASE*DIGITSBASE
   int64_t testvalues2[]   = { (int64_t)8999999998999999996, (int64_t)999999999000000000, (int64_t)123456789987654321, INT64_MAX } ;
   int32_t testexponent2[] = { expmax_decimal()-18, expmax_decimal()-26, 11, 0, -10, -(expmax_decimal()-5), -expmax_decimal() } ;
   for (unsigned i = 0; i < nrelementsof(testvalues2); ++i) {
      for (unsigned ei = 0; ei < nrelementsof(testexponent2); ++ei) {
         for (int s = -1; s <= +1; s += 2) {
            uint16_t expdiff         = (uint16_t) (testexponent2[ei] - alignexp_test(testexponent2[ei])) ;
            uint32_t shifteddigit[3] ;
            for (int64_t si = 0, value = testvalues2[i], carry = 0; si < 3; ++si, value /= DIGITSBASE) {
               int64_t  shifted = (value % DIGITSBASE) * power10_decimalhelper(expdiff) ;
               shifteddigit[si] = (uint32_t)carry + (uint32_t)(shifted % DIGITSBASE) ;
               carry            = shifted / DIGITSBASE ;
            }
            TEST(0 == new_decimal(&dec, 1)) ;
            TEST(0 == setfromint64_decimal(&dec, s * testvalues2[i], testexponent2[ei])) ;
            TEST(dec->size_allocated     == (2 + (0 != shifteddigit[2]))) ;
            TEST(dec->sign_and_used_digits == s * (2 + (0 != shifteddigit[2]))) ;
            TEST(size_decimal(dec)     == 2 + (0 != shifteddigit[2])) ;
            TEST(nrdigits_decimal(dec) == 1 + log10_int64((uint64_t)testvalues2[i]) + expdiff) ;
            TEST(sign_decimal(dec)     == s) ;
            TEST(dec->exponent         == alignexp_test(testexponent2[ei]) / digitsperint_decimal()) ;
            for (unsigned si = 0; si < size_decimal(dec); ++si) {
               TEST(dec->digits[si]    == shifteddigit[si]) ;
            }
            int32_t expectexp = testexponent2[ei] ;
            int64_t expectval = testvalues2[i] ;
            int32_t decexp = 2 ;
            for (; expectval >= DIGITSBASE; expectval /= 10, ++expectexp) ;
            expectval *= s ;
            TEST(expectval == first9digits_decimal(dec, &decexp)) ;
            TEST(expectexp == decexp) ;
            decexp    = 2 ;
            expectexp = testexponent2[ei] ;
            expectval = testvalues2[i] ;
            for (; expectval >= ((int64_t)DIGITSBASE * DIGITSBASE); expectval /= 10, ++expectexp) ;
            expectval *= s ;
            TEST(expectval == first18digits_decimal(dec, &decexp)) ;
            TEST(expectexp == decexp) ;
            TEST(0 == delete_decimal(&dec)) ;
         }
      }
   }

   // TEST setfromint32_decimal: ENOMEM
   test_errortimer_t errtimer ;
   TEST(0 == new_decimal(&dec, 1)) ;
   TEST(0 == init_testerrortimer(&errtimer, 1, ENOMEM)) ;
   setresizeerr_mmtest(mmcontext_mmtest(), &errtimer) ;
   TEST(ENOMEM == setfromint32_decimal(&dec, DIGITSBASE, 0)) ;
   TEST(0 == delete_decimal(&dec)) ;

   // TEST setfromint64_decimal: ENOMEM
   TEST(0 == new_decimal(&dec, 2)) ;
   TEST(0 == init_testerrortimer(&errtimer, 1, ENOMEM)) ;
   setresizeerr_mmtest(mmcontext_mmtest(), &errtimer) ;
   TEST(ENOMEM == setfromint64_decimal(&dec, (uint64_t)DIGITSBASE*DIGITSBASE, 0)) ;
   TEST(0 == delete_decimal(&dec)) ;

   // TEST setfromint32_decimal: EOVERFLOW
   TEST(0 == new_decimal(&dec, 1)) ;
   TEST(0 == setfromint32_decimal(&dec, DIGITSBASE, expmax_decimal())) ;
   TEST(0 == setfromint32_decimal(&dec, DIGITSBASE, -expmax_decimal())) ;
   TEST(EOVERFLOW == setfromint32_decimal(&dec, DIGITSBASE, expmax_decimal()+1)) ;
   TEST(EOVERFLOW == setfromint32_decimal(&dec, DIGITSBASE, -(expmax_decimal()+1))) ;
   TEST(0 == delete_decimal(&dec)) ;

   // TEST setfromint64_decimal: EOVERFLOW
   TEST(0 == new_decimal(&dec, 1)) ;
   TEST(0 == setfromint64_decimal(&dec, (uint64_t)DIGITSBASE*DIGITSBASE, expmax_decimal())) ;
   TEST(0 == setfromint64_decimal(&dec, (uint64_t)DIGITSBASE*DIGITSBASE, -expmax_decimal())) ;
   TEST(EOVERFLOW == setfromint64_decimal(&dec, (uint64_t)DIGITSBASE*DIGITSBASE, expmax_decimal()+1)) ;
   TEST(EOVERFLOW == setfromint64_decimal(&dec, (uint64_t)DIGITSBASE*DIGITSBASE, -(expmax_decimal()+1))) ;
   TEST(0 == delete_decimal(&dec)) ;

   return 0 ;
ONABORT:
   delete_decimal(&dec) ;
   return EINVAL ;
}

static int test_setfromfloat(void)
{
   decimal_t   * dec    = 0 ;
   bigint_t    * big[4] = { 0, 0 } ;
   float       fvalue ;
   int         fmaxexp ;

   // prepare
   frexp(FLT_MAX, &fmaxexp) ;
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == new_bigint(&big[i], nrdigitsmax_bigint())) ;
   }
   TEST(0 == new_decimal(&dec, nrdigitsmax_decimal())) ;

   // TEST float has 24 bit accuracy
   fvalue = (float) 0x01ffffff ;
   TEST((int32_t)fvalue == 0x02000000) ;
   fvalue = (float) 0x00ffffff ;
   TEST((int32_t)fvalue == 0x00ffffff) ;

   // TEST setfromfloat_decimal: value == 0
   dec->exponent             = 1 ;
   dec->sign_and_used_digits = 1 ;
   TEST(0 == setfromfloat_decimal(&dec, 0.0f)) ;
   TEST(sign_decimal(dec)     == 0) ;
   TEST(size_decimal(dec)     == 0) ;
   TEST(exponent_decimal(dec) == 0) ;

   // TEST setfromfloat_decimal: fraction != 0 && integral != 0
   uint32_t testvalues1[] = { 0x00ffffff, 0x00fffff9, 0x00800001, 0x1f3, 0x3f5, 0x707, 0x70001, 0x80001 } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues1); ++tvi) {
      fvalue = (float) testvalues1[tvi] / 2.0f ;
      for (int nrshift = 1; fvalue > 2; ++ nrshift, fvalue /= 2) {
         float integral, fraction = modff(fvalue, &integral) ;
         TEST(0 != integral && 0 != fraction) ;
         // compute expected result
         setfromuint32_bigint(big[0], (testvalues1[tvi] << (32-nrshift))) ;
         for (int mulcount = nrshift; mulcount > 0; mulcount -= digitsperint_decimal()) {
            TEST(0 == multui32_bigint(&big[1], big[0], DIGITSBASE)) ;
            TEST(0 == copy_bigint(&big[0], big[1])) ;
         }
         TEST(0 == shiftright_bigint(&big[0], 32)) ;
         uint32_t expecteddigits[4] ;
         unsigned expectedsize = 0 ;
         for (; expectedsize < nrelementsof(expecteddigits) && 0 != sign_bigint(big[0]); ++expectedsize) {
            TEST(0 == divmodui32_bigint(&big[2], &big[1], big[0], DIGITSBASE)) ;
            TEST(0 == copy_bigint(&big[0], big[2])) ;
            expecteddigits[expectedsize] = firstdigit_bigint(big[1]) ;
         }
         expecteddigits[expectedsize++] = (testvalues1[tvi] >> nrshift) ;
         // call setfromfloat_decimal + compare result
         for (int s =-1; s <= +1; s += 2) {
            TEST(0 == setfromfloat_decimal(&dec, (float)s * fvalue)) ;
            TEST(sign_decimal(dec)     == s)
            TEST(size_decimal(dec)     == expectedsize) ;
            TEST(exponent_decimal(dec) == digitsperint_decimal() * (1 - (int)expectedsize)) ;
            for (unsigned i = expectedsize; i > 0 ; --i) {
               TEST(dec->digits[i-1] == expecteddigits[i-1]) ;
            }
         }
      }
   }

   // TEST setfromfloat_decimal: only integral part
   uint32_t testvalues2[] = { 0xffffff, 1, 3, 7, 9999999, 0x800001, 0x888888, 0x123456, 0xf, 0xff, 0xfff, 0xffff, 0xfffff } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues2); ++tvi) {
      int fexp ;
      fvalue = (float) testvalues2[tvi] ;
      frexp(fvalue, &fexp) ;
      setfromuint32_bigint(big[3], testvalues2[tvi]) ;
      for (unsigned nrshift = 0; fexp < fmaxexp; ++ nrshift, ++ fexp, fvalue = (fexp <= fmaxexp ? fvalue * 2 : fvalue)) {
         for (int s =-1; s <= +1; s += 2) {
            TEST(0 == setfromfloat_decimal(&dec, (float)s * fvalue)) ;
            TEST(sign_decimal(dec)     == s) ;
            TEST(exponent_decimal(dec) == 0) ;
            // convert decimal to integer
            setfromuint32_bigint(big[0], 0) ;
            for (unsigned i = size_decimal(dec); i > 0 ; --i) {
               TEST(0 == multui32_bigint(&big[1], big[0], DIGITSBASE)) ;
               setfromuint32_bigint(big[2], dec->digits[i-1]) ;
               TEST(0 == add_bigint(&big[0], big[1], big[2])) ;
            }
            // compare decimal with expected value
            TEST(0 == cmp_bigint(big[3], big[0])) ;
         }
         TEST(0 == shiftleft_bigint(&big[3], 1)) ;
      }
   }

   // TEST setfromfloat_decimal: only fractional part
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues2); ++tvi) {
      fvalue = (float) testvalues2[tvi] ;
      while (fvalue >= 1) fvalue /= 2 ;
      // this check tests for denormalized float (1==testvalues2[tvi] && 0!=fvalue)
      for (unsigned nrleadingzero = 0; fvalue > FLT_MIN || (1==testvalues2[tvi] && 0!=fvalue); ++ nrleadingzero, fvalue /= 2) {
         unsigned mantissabits ;
         setfromuint32_bigint(big[3], testvalues2[tvi]) ;
         while (0 == (firstdigit_bigint(big[3]) & 0x80000000)) {
            TEST(0 == shiftleft_bigint(&big[3], 1)) ;
         }
         for (mantissabits = 32; 0 == (firstdigit_bigint(big[3]) & 0x01); --mantissabits) {
            TEST(0 == shiftright_bigint(&big[3], 1)) ;
         }
         for (unsigned i = mantissabits+nrleadingzero; i > 0; --i) {
            TEST(0 == multui32_bigint(&big[2], big[3], 5)) ;
            TEST(0 == copy_bigint(&big[3], big[2])) ;
         }
         int32_t exponent = (int32_t)(mantissabits + nrleadingzero) ;
         for (int s =-1; s <= +1; s += 2) {
            TEST(0 == setfromfloat_decimal(&dec, (float)s * fvalue)) ;
            TEST(sign_decimal(dec)     == s) ;
            TEST(exponent_decimal(dec) <= -exponent) ;
            // convert decimal to integer
            setfromuint32_bigint(big[0], 0) ;
            for (unsigned i = size_decimal(dec); i > 0 ; --i) {
               TEST(0 == multui32_bigint(&big[1], big[0], DIGITSBASE)) ;
               setfromuint32_bigint(big[2], dec->digits[i-1]) ;
               TEST(0 == add_bigint(&big[0], big[1], big[2])) ;
            }
            for (int32_t e = exponent_decimal(dec); e < -exponent; ++e) {
               TEST(0 == divmodui32_bigint(&big[1], &big[2], big[0], 10)) ;
               TEST(0 == sign_bigint(big[2])) ;
               TEST(0 == copy_bigint(&big[0], big[1])) ;
            }
            // compare decimal with expected value
            TEST(0 == cmpmagnitude_bigint(big[3], big[0])) ;
         }
      }
   }

   // TEST setfromfloat_decimal: 1023.9933
   int32_t  decexp ;
   TEST(0 == setfromfloat_decimal(&dec, 1023.9933f)) ;
   TEST(102399328 == first9digits_decimal(dec, &decexp)) ;
   TEST(-5 == decexp) ;

   // TEST setfromfloat_decimal: 1023.9933 (+1 ulp)
   {
      int      fexp ;
      uint32_t ivalue ;
      fvalue = frexpf(1023.9933f, &fexp) ;
      ivalue = (uint32_t) ldexpf(fvalue, 24) ;
      ++ivalue ;
      fvalue = ldexpf((float)ivalue, fexp-24) ;
   }
   TEST(0 == setfromfloat_decimal(&dec, fvalue)) ;
   TEST(102399334 == first9digits_decimal(dec, &decexp)) ;
   TEST(-5 == decexp) ;

   // TEST setfromfloat_decimal: EINVAL
   TEST(EINVAL == setfromfloat_decimal(&dec, INFINITY)) ;
   TEST(EINVAL == setfromfloat_decimal(&dec, -INFINITY)) ;
   TEST(EINVAL == setfromfloat_decimal(&dec, NAN)) ;

   // unprepare
   TEST(0 == delete_decimal(&dec)) ;
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == delete_bigint(&big[i])) ;
   }

   return 0 ;
ONABORT:
   delete_decimal(&dec) ;
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      delete_bigint(&big[i]) ;
   }
   return EINVAL ;
}

static int test_setfromchar(void)
{
   decimal_t   * dec    = 0 ;
   char        strbuf[nrdigitsmax_decimal()+10] ;

   // prepare
   TEST(0 == new_decimal(&dec, nrdigitsmax_decimal())) ;

   // TEST setfromchar_decimal
   struct {
      const char  * decimalstr ;
      int16_t     exponent10 ;
      uint16_t    nrdigits ;
      uint32_t    digits[127] ;
   }  testvalues[] = {
      { "-0", 0, 0, { 0 } }, { "-1", 0, 1, { 1 } }, { "-2", 0, 1, { 2 } }, { "-3", 0, 1, { 3 } }, { "-4", 0, 1, { 4 } }
      ,{ "-5", 0, 1, { 5 } }, { "-6", 0, 1, { 6 } }, { "-7", 0, 1, { 7 } }, { "-8", 0, 1, { 8 } }, { "-9", 0, 1, { 9 } }
      ,{ "-110e36", 4*9, 3, { 110 } }
      ,{ "-12345000e36", 4*9, 8, { 12345000 } }
      ,{ "-000000000000000000000e100000011111", 0, 0, { 0 } }
      ,{ "-.000000000000000000000e+99999999999999000", 0, 0, { 0 } }
      ,{ "-00000000000000.0000000000000000000e-01234567890", 0, 0, { 0 } }
      ,{ "-0000000000123456789111111111222222222333333333.e-12345", -12348, 4*9+3, { 123, 456789111, 111111222, 222222333, 333333000 } }
      ,{ "-00000000003123456789000000000.e-1", 0, 1+1*9+8, { 312345678, 900000000 } }
      ,{ "-121234567890000000090000000080000000.00000000e-32", -32+7-2, 2+3*9+2, { 1212, 345678900, 900, 800 } }
      ,{ "-0000.0001234567899876543210000000000000000000000e21", 0, 2*9, { 123456789, 987654321 } }
      ,{ "-000034.0567812345678900000000000000000e32765", 32751, 16, { 3405678, 123456789 } }
      ,{ "-000034.0567812345678900000000000000000e-32746", -32760, 16, { 3405678, 123456789 } }
      ,{ "-000034.0567812345678900000000000000000e-32745", -32760, 16+1, { 34056781, 234567890 } }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues); ++tvi) {
      for (int s = -1; s <= 1; s += 2) {
         uint32_t    size = (uint32_t) (testvalues[tvi].nrdigits + digitsperint_decimal()-1) / digitsperint_decimal() ;
         const char * str = testvalues[tvi].decimalstr + (s == +1) ;
         TEST(0 == setfromchar_decimal(&dec, strlen(str), str)) ;
         TEST(nrdigits_decimal(dec) == testvalues[tvi].nrdigits) ;
         TEST(size_decimal(dec)     == size) ;
         TEST(sign_decimal(dec)     == s * (0 != testvalues[tvi].nrdigits)) ;
         TEST(exponent_decimal(dec) == testvalues[tvi].exponent10) ;
         for (uint32_t i = 0; i < size; ++i) {
            TEST(dec->digits[size-1-i] == testvalues[tvi].digits[i]) ;
         }
      }
   }

   // TEST setfromchar_decimal: maximum length
   strcpy(strbuf, "0.") ;
   strcpy(strbuf+2+nrdigitsmax_decimal(), "000") ;
   for (unsigned i = 0; i < nrdigitsmax_decimal(); ++i) {
      strbuf[2+i] = (char) ('1' + (i%9)) ;
   }
   TEST(0 == setfromchar_decimal(&dec, strlen(strbuf), strbuf)) ;
   TEST(nrdigits_decimal(dec) == nrdigitsmax_decimal()) ;
   TEST(size_decimal(dec)     == sizemax_decimal()) ;
   TEST(sign_decimal(dec)     == +1) ;
   TEST(exponent_decimal(dec) == -(int)nrdigitsmax_decimal()) ;
   for (unsigned i = 0; i < sizemax_decimal(); ++i) {
      TEST(123456789 == dec->digits[i]) ;
   }

   // TEST setfromchar_decimal: EINVAL
   const char * errvalues1[] = { "", "-", "-.", "-.e+10", "1.2e", "1.2e+", "1.2e-", "1.2e-100x", "1.x", "1x", "-.123y" } ;
   for (unsigned i = 0; i < nrelementsof(errvalues1); ++i) {
      TEST(EINVAL == setfromchar_decimal(&dec, strlen(errvalues1[i]), errvalues1[i])) ;
   }

   // TEST setfromchar_decimal: EOVERFLOW
   const char * errvalues2[] = { "1e-294904", ".0000001e-294897", "100000.000e+294899", "123456789123456789e+294904" } ;
   for (unsigned i = 0; i < nrelementsof(errvalues2); ++i) {
      TEST(EOVERFLOW  == setfromchar_decimal(&dec, strlen(errvalues2[i]), errvalues2[i])) ;
   }
   memset(strbuf, '1', sizeof(strbuf)) ;
   TEST(EOVERFLOW  == setfromchar_decimal(&dec, nrdigitsmax_decimal()+1, strbuf)) ;
   strbuf[0]  = '-' ;
   strbuf[10] = '.' ;
   TEST(EOVERFLOW  == setfromchar_decimal(&dec, nrdigitsmax_decimal()+3, strbuf)) ;

   // unprepare
   TEST(0 == delete_decimal(&dec)) ;

   return 0 ;
ONABORT:
   delete_decimal(&dec) ;
   return EINVAL ;
}

static int test_compare(void)
{
   decimal_t   * dec[2] = { 0, 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      TEST(0 == new_decimal(&dec[i], nrdigitsmax_decimal())) ;
   }

   // TEST cmp_decimal: unequal
   const char * testvalues[][2] = {    // format { "small value", "big value" }
      // zero
      { "-1", "0" }
      ,{ "0",  "1234567890123456789099999e100" }
      // aligned
      ,{ "-1234567890123456789099999e90",  "1234567890123456789099999e90" }
      ,{ "10000001",     "10000002" }
      ,{ "89123456789",  "789123456789" }
      ,{ "1123456789123456789", "2e18" }
      ,{ "1e18", "1000000000123456789" }
      // exponent
      ,{ "123456789123456789", "12345678912345678e9" }
      ,{ "1e-2000", "1e1000" }
      // unaligned (assign aligns)
      ,{ "123456789123456780", "123456789123456789" }
      ,{ "1234567891e8", "123456789100000000.123456789" }
      ,{ "123456789123456789", "123456789123456789.0000000001234",  }
      ,{ "123456789123456789", "123456789123456789.1234567891234",  }
      ,{ "123456789123456788.1234567891234", "123456789123456789" }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testvalues[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues[tvi][i]), testvalues[tvi][i])) ;
      }
      for (int s = -1;  s <= 1; s += 2) {
         TEST(0 == cmp_decimal(dec[0], dec[0])) ;
         TEST(0 == cmp_decimal(dec[1], dec[1])) ;
         TEST(s == cmp_decimal(dec[0], dec[1])) ;
         TEST(s == - cmp_decimal(dec[1], dec[0])) ;
         negate_decimal(dec[0]) ;
         negate_decimal(dec[1]) ;
      }
   }

   // TEST cmp_decimal: equal
   const char * testvalues2[][2] = {
      // zero
      { "0", "0" }
      // aligned
      ,{ "123456789123456789e9",  "123456789123456789000000001" }
      ,{ "1e18", "1000000000000000001" }
      // unaligned (assign aligns)
      ,{ "12345678912345678e10", "123456789123456780000000001" }
      ,{ "123456789123456789e18", "123456789123456789000000000000000010" }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues2); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testvalues2[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues2[tvi][i]), testvalues2[tvi][i])) ;
      }
      dec[1]->digits[0] = 0 ;
      for (int s = -1;  s <= 1; s += 2) {
         TEST(0 == cmp_decimal(dec[0], dec[0])) ;
         TEST(0 == cmp_decimal(dec[1], dec[1])) ;
         TEST(0 == cmp_decimal(dec[0], dec[1])) ;
         TEST(0 == cmp_decimal(dec[1], dec[0])) ;
         negate_decimal(dec[0]) ;
         negate_decimal(dec[1]) ;
      }
   }

   // unprepare
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      TEST(0 == delete_decimal(&dec[i])) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      delete_decimal(&dec[i]) ;
   }
   return EINVAL ;
}

static int test_addsub(void)
{
   decimal_t   * dec[4] = { 0, 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      TEST(0 == new_decimal(&dec[i], nrdigitsmax_decimal())) ;
   }

   // TEST: add_decimal, sub_decimal, add_decimalhelper: no trailing zero in summands
   const char * testvalues[][3] = { // format { "summand1", "summand2", "result of addition" }
      { "0", "0", "0" }
      ,{ "0", "9.87654321", "9.87654321" }
      ,{ "0", "20000000000000000009.333333387654321e1234", "20000000000000000009.333333387654321e1234" }
      ,{ "1.23456789", "9.87654322", "11.11111111" }
      ,{ "999999999.999999999", "999999999.999999999", "1999999999.999999998" }
      ,{ "499999999.999999999", "499999999.999999999", "0999999999.999999998" }
      ,{ "987654321", "123456789e9", "123456789987654321" }
      ,{ "987654321", "123456789e27", "123456789000000000000000000987654321" }
      ,{ "2987654321", "123456789999999999e9", "123456790000000001987654321" }
      ,{ "888888888888888888888888889", "111111111111111111111111111", "1e27" }
      ,{ "123456789123456789888888888888888888888888889", "111111111111111111111111111", "123456789123456790e27" }
      ,{ "999999999999999999999999999", "1", "1e27" }
      ,{ "123456789123456789123456789123456789", "876543210e18", "123456789999999999123456789123456789" }
      ,{ "123456789123456789123456789123456789", "876543211e18", "123456790000000000123456789123456789" }
      ,{ "123456789123456789123456789123456789", "100000000876543211e18",  "223456790000000000123456789123456789" }
      ,{ "123456789123456789123456789123456789", "900000000876543211e18", "1023456790000000000123456789123456789" }
      ,{ "123456789123456788999999999999999999887766555", "112233445", "123456789123456789e27" }
      ,{ "123456789123456788999999999899999999887766555", "100000000112233445", "123456789123456789e27" }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testvalues[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues[tvi][i]), testvalues[tvi][i])) ;
      }
      for (int s = -1; s <= +1; s += 2) {
         TEST(0 == add_decimal(&dec[3], dec[0], dec[1])) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         TEST(0 == add_decimal(&dec[3], dec[1], dec[0])) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[1]) ;
         TEST(0 == setfromint32_decimal(&dec[3], -1, -1)) ;
         TEST(0 == sub_decimal(&dec[3], dec[0], dec[1])) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[0]) ;
         TEST(0 == sub_decimal(&dec[3], dec[1], dec[0])) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[2]) ;
      }
   }

   // TEST: add_decimal, sub_decimal, sub_decimalhelper: no trailing zero in summands
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testvalues[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues[tvi][i]), testvalues[tvi][i])) ;
      }
      for (int s = -1; s <= +1; s += 2) {
         TEST(0 == sub_decimal(&dec[3], dec[2], dec[1])) ;
         TEST(0 == cmp_decimal(dec[3], dec[0])) ;
         TEST(0 == sub_decimal(&dec[3], dec[2], dec[0])) ;
         TEST(0 == cmp_decimal(dec[3], dec[1])) ;
         TEST(0 == sub_decimal(&dec[3], dec[1], dec[2])) ;
         negate_decimal(dec[3]) ;
         TEST(0 == cmp_decimal(dec[3], dec[0])) ;
         TEST(0 == sub_decimal(&dec[3], dec[0], dec[2])) ;
         negate_decimal(dec[3]) ;
         TEST(0 == cmp_decimal(dec[3], dec[1])) ;
         negate_decimal(dec[1]) ;
         TEST(0 == add_decimal(&dec[3], dec[2], dec[1])) ;
         TEST(0 == cmp_decimal(dec[3], dec[0])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[0]) ;
         TEST(0 == add_decimal(&dec[3], dec[2], dec[0])) ;
         TEST(0 == cmp_decimal(dec[3], dec[1])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[2]) ;
      }
   }


   // TEST: add_decimal, sub_decimal, add_decimalhelper: trailing zero in summands
   const char * testvalues2[][3] = { // format { "summand1", "summand2", "result of addition" }
      { "1000000000000000001", "987654321e18", "987654322e18" }
      ,{ "11000000000000000000000000000000000001", "99000000000000000000000000000000000001", "110e36" }
      ,{ "123456789123456789123456789123456789000000000000000001", "900000000876543211000000000000000001e18", "1023456790000000000123456789123456789e18" }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues2); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testvalues2[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues2[tvi][i]), testvalues2[tvi][i])) ;
         if (i <= 1 && 1 == dec[i]->digits[0]) {
            dec[i]->digits[0] = 0 ;
         }
      }
      for (int s = -1; s <= +1; s += 2) {
         TEST(0 == add_decimal(&dec[3], dec[0], dec[1])) ;
         TEST(nrdigits_decimal(dec[3]) == nrdigits_decimal(dec[2])/*trailing zero skipped*/) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         TEST(0 == add_decimal(&dec[3], dec[1], dec[0])) ;
         TEST(nrdigits_decimal(dec[3]) == nrdigits_decimal(dec[2])/*trailing zero skipped*/) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[1]) ;
         TEST(0 == setfromint32_decimal(&dec[3], -1, -1)) ;
         TEST(0 == sub_decimal(&dec[3], dec[0], dec[1])) ;
         TEST(nrdigits_decimal(dec[3]) == nrdigits_decimal(dec[2])/*trailing zero skipped*/) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[0]) ;
         TEST(0 == sub_decimal(&dec[3], dec[1], dec[0])) ;
         TEST(nrdigits_decimal(dec[3]) == nrdigits_decimal(dec[2])/*trailing zero skipped*/) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[2]) ;
      }
   }

   // TEST: add_decimal, sub_decimal, sub_decimalhelper: trailing zero in summands
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues2); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testvalues2[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues2[tvi][i]), testvalues2[tvi][i])) ;
         if (i <= 1 && 1 == dec[i]->digits[0]) {
            dec[i]->digits[0] = 0 ;
         }
      }
      for (int s = -1; s <= +1; s += 2) {
         TEST(0 == sub_decimal(&dec[3], dec[2], dec[1])) ;
         TEST(0 == cmp_decimal(dec[3], dec[0])) ;
         TEST(0 == sub_decimal(&dec[3], dec[2], dec[0])) ;
         TEST(0 == cmp_decimal(dec[3], dec[1])) ;
         TEST(0 == sub_decimal(&dec[3], dec[1], dec[2])) ;
         negate_decimal(dec[3]) ;
         TEST(0 == cmp_decimal(dec[3], dec[0])) ;
         TEST(0 == sub_decimal(&dec[3], dec[0], dec[2])) ;
         negate_decimal(dec[3]) ;
         TEST(0 == cmp_decimal(dec[3], dec[1])) ;
         negate_decimal(dec[1]) ;
         TEST(0 == add_decimal(&dec[3], dec[2], dec[1])) ;
         TEST(0 == cmp_decimal(dec[3], dec[0])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[0]) ;
         TEST(0 == add_decimal(&dec[3], dec[2], dec[0])) ;
         TEST(0 == cmp_decimal(dec[3], dec[1])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[2]) ;
      }
   }

   // TEST add_decimal, sub_decimal: EOVERFLOW
   const char * testerrvalues[][2] = { // format { "summand1", "summand2" }
      { "1e9000", "123" /*size overflow*/}
      ,{ "1e1143", "123" /*size overflow*/}
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testerrvalues); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testerrvalues[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testerrvalues[tvi][i]), testerrvalues[tvi][i])) ;
      }
      TEST(EOVERFLOW == add_decimal(&dec[3], dec[0], dec[1])) ;
      TEST(EOVERFLOW == sub_decimal(&dec[3], dec[0], dec[1])) ;
   }

   // unprepare
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      TEST(0 == delete_decimal(&dec[i])) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      delete_decimal(&dec[i]) ;
   }
   return EINVAL ;
}

static int test_mult(void)
{
   decimal_t   * dec[5] = { 0, 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      TEST(0 == new_decimal(&dec[i], nrdigitsmax_decimal())) ;
   }

   // TEST mult_decimal
   const char * testvalues[][3] = { // format { "multiplicant1", "multiplicant2", "result of mult" }
      { "0", "100", "0" }
      ,{ "9", "12", "108" }
      ,{ "999999999999999999", "9", "8999999999999999991" }
      ,{ "999999999999999999999999999999999999", "999999999999999999999999999999999999", "999999999999999999999999999999999998000000000000000000000000000000000001" }
      // test splitting with 0
      ,{ "123456789000000000000000000000000000000000000000000000987654321", "223456789000000000000000000000000000000000000000000000997654321", "27587257650190521000000000000000000000000000000000000343865262215270538000000000000000000000000000000000000985337600999971041" }
      ,{ "123456789010203040506070809000000000000000000000000000987654321", "223456789987654321987654321000000000000000987654321000997654321", "27587257774403090914168495151643508283974672173446812056211306359771945029340668120786730000000975461057790956378600999971041" }
      // test splitting with (t[2]*t[3])->exponent != 0
      ,{ "999999999000000001999999999999999999", "999999999999999999999999999000000001", "999999999000000001999999999000000000999999997000000002000000000999999999" }
      // test splitting with result=t0*X*X+t1 && result->exponent != 0
      ,{ "123456789123456789123456789123456789e8", "123456789123456789123456789123456789e8", "15241578780673678546105778311537878046486820281054720515622620750190521e16" }
      // test splitting with t[0]/t[1]/t[2]/t[3]->exponent != 0 => result->exponent != 0 && t4->exponent != 0
      ,{ "999999999100000000999999999900000000", "999999999100000000999999999900000000", "999999998200000002809999998000000001179999999800000000010000000000000000" }
      // test splitting adding t[4] to t[0]*X*X + t[1] produces carry overflow (see multsplit_decimalhelper)
      ,{ "999999999999999999000000000000000001", "000000001000000000999999999999999999", "1000000000999999998999999998000000001000000001999999999999999999" }
      // test add exponents
      ,{ "9e-123", "5e-300", "45e-423" }
      ,{ "3e-30999", "111111111222222222333333333e+32000", "333333333666666666999999999e1001" }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testvalues[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues[tvi][i]), testvalues[tvi][i])) ;
      }
      for (int s = -1; s <= +1; s += 2) {
         TEST(0 == mult_decimal(&dec[3], dec[0], dec[1])) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[2]) ;
         TEST(0 == mult_decimal(&dec[3], dec[1], dec[0])) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         int e1 = (random() % (INT16_MAX/2)) ;
         int e2 = (random() % (INT16_MAX/2)) ;
         dec[0]->exponent = (int16_t) (dec[0]->exponent - e1) ;
         dec[1]->exponent = (int16_t) (dec[1]->exponent + e2) ;
         dec[2]->exponent = (int16_t) (dec[2]->exponent - e1 + e2) ;
         negate_decimal(dec[0]) ;
         negate_decimal(dec[2]) ;
      }
      if (0 == exponent_decimal(dec[0])) {
         // generate trailing zero by adding 1 to first number
         setpositive_decimal(dec[0]) ;
         setpositive_decimal(dec[1]) ;
         TEST(0 == setfromint32_decimal(&dec[4], 1, 0)) ;
         TEST(0 == add_decimal(&dec[3], dec[0], dec[4])) ;
         TEST(0 == copy_decimal(&dec[0], dec[3])) ;         // dec[0] = dec[0] +1
         TEST(0 == add_decimal(&dec[3], dec[2], dec[1])) ;
         TEST(0 == copy_decimal(&dec[2], dec[3])) ;         // dec[2] = dec[2] + dec[1]
         TEST(0 == mult_decimal(&dec[3], dec[0], dec[1])) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         TEST(0 == mult_decimal(&dec[3], dec[1], dec[0])) ; // dec[0]*dec[1] == dec[1]*dec[0]
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
      }
   }

   // TEST mult_decimal: random numbers (compare multsplit_decimalhelper with mult_decimalhelper)
   for (int ti = 0; ti < 50; ++ti) {
      clear_decimal(dec[0]) ;
      clear_decimal(dec[1]) ;
      clear_decimal(dec[2]) ;
      const uint8_t S = sizemax_decimal()/2 ;
      dec[1]->sign_and_used_digits = (int8_t)S ;
      dec[2]->sign_and_used_digits = (int8_t)S ;
      for (unsigned i = 0; i < S; ++i) {
         dec[1]->digits[i] = (uint32_t) random() % DIGITSBASE ;
         dec[2]->digits[i] = (uint32_t) random() % DIGITSBASE ;
      }
      while (!dec[1]->digits[S-1]) dec[1]->digits[S-1] = (uint32_t) random() % DIGITSBASE ;
      while (!dec[2]->digits[S-1]) dec[2]->digits[S-1] = (uint32_t) random() % DIGITSBASE ;
      mult_decimalhelper(dec[0], S, dec[1]->digits, S, dec[2]->digits, 0) ;
      for (int s =-1; s <= +1; s += 2) {
         setpositive_decimal(dec[0]) ;
         TEST(0 == mult_decimal(&dec[3], dec[1], dec[2])) ;
         TEST(0 == cmp_decimal(dec[0], dec[3])) ;
         negate_decimal(dec[1]) ;
         setnegative_decimal(dec[0]) ;
         TEST(0 == mult_decimal(&dec[3], dec[2], dec[1])) ;
         TEST(0 == cmp_decimal(dec[0], dec[3])) ;
         negate_decimal(dec[2]) ;
      }
   }

   // TEST mult_decimal: EOVERFLOW
   dec[1]->sign_and_used_digits = (sizemax_decimal()+1)/2 ;
   dec[2]->sign_and_used_digits = (sizemax_decimal()+1)/2 ;
   TEST(EOVERFLOW == mult_decimal(&dec[0], dec[1], dec[2])) ;
   const char * str1 = "9e294903" ;
   TEST(0 == setfromchar_decimal(&dec[1], strlen(str1), str1)) ;
   str1 = "1e9" ;
   TEST(0 == setfromchar_decimal(&dec[2], strlen(str1), str1)) ;
   TEST(EOVERFLOW == mult_decimal(&dec[0], dec[1], dec[2])) ;
   str1 = "9e-200000" ;
   TEST(0 == setfromchar_decimal(&dec[1], strlen(str1), str1)) ;
   TEST(EOVERFLOW == mult_decimal(&dec[0], dec[1], dec[1])) ;

   // unprepare
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      TEST(0 == delete_decimal(&dec[i])) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      delete_decimal(&dec[i]) ;
   }
   return EINVAL ;
}

static int test_div(void)
{
   decimal_t   * dec[5] = { 0, 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      TEST(0 == new_decimal(&dec[i], nrdigitsmax_decimal())) ;
   }

   const char * testvalues1[][3] = {   // format { "dividend", "divisor (integer)", "result" }
      { "999999999", "9", "111111111" }
      ,{ "999999999999999999", "999999999", "1000000001" }
      ,{ "2999999997", "3", "999999999" }
      ,{ "1e9",        "3", "333333333.333333333" }
      ,{ "123499004370324769803247640e90", "923000917", "133801605280880527e90" }
      ,{ "10999999999", "9", "1222222222" }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues1); ++tvi) {
      // TEST divi32_decimal: divisor < DIGITSBASE
      for (unsigned i = 0; i < nrelementsof(testvalues1[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues1[tvi][i]), testvalues1[tvi][i])) ;
      }
      for (int s = +1; s >= -1; s -= 2) {
         TEST(0 == divi32_decimal(&dec[3], dec[0], s * (int32_t)dec[1]->digits[0], size_decimal(dec[2]))) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[2]) ;
         TEST(0 == divi32_decimal(&dec[3], dec[0], -s * (int32_t)dec[1]->digits[0], size_decimal(dec[2]))) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[0]) ;
         negate_decimal(dec[2]) ;
      }

      // TEST divi32_decimal: divisor == DIGITSBASE only exponent is decremented
      TEST(0 == divi32_decimal(&dec[3], dec[0], DIGITSBASE, size_decimal(dec[0]))) ;
      TEST(exponent_decimal(dec[3]) == exponent_decimal(dec[0]) - digitsperint_decimal()) ;
      ++ dec[3]->exponent ;
      TEST(0 == cmp_decimal(dec[3], dec[0])) ;

      // TEST divi32_decimal: trailing zero by adding 1 to first number
      if (0 == exponent_decimal(dec[0])) {
         TEST(0 == setfromint32_decimal(&dec[4], 1, 0)) ;
         TEST(0 == add_decimal(&dec[3], dec[0], dec[4])) ;
         TEST(0 == copy_decimal(&dec[0], dec[3])) ;   // dec[0] = dec[0] +1
         TEST(0 == divi32_decimal(&dec[3], dec[0], (int32_t)dec[1]->digits[0], size_decimal(dec[2]))) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
      }
   }

   // TEST divi32_decimal: result_size is silently corrected
   TEST(0 == setfromint32_decimal(&dec[0], 1, 0)) ;
   TEST(0 == divi32_decimal(&dec[1], dec[0], 1, 0)) ;
   TEST(0 == exponent_decimal(dec[1])) ;
   TEST(1 == size_decimal(dec[1])) ;
   TEST(0 == setfromint32_decimal(&dec[0], 1, 0)) ;
   TEST(0 == divi32_decimal(&dec[1], dec[0], 2, 2*sizemax_decimal())) ;
   TEST(sizemax_decimal() == - exponent_decimal(dec[1]) / digitsperint_decimal()) ;
   TEST(sizemax_decimal() == size_decimal(dec[1])/*silently truncated*/) ;

   // TEST divi32_decimal: EINVAL
   TEST(0 == setfromint32_decimal(&dec[1], 1, 0)) ;
   TEST(EINVAL == divi32_decimal(&dec[0], dec[1], 0/*divisor!=0*/, 1)) ;
   TEST(EINVAL == divi32_decimal(&dec[0], dec[1], DIGITSBASE+1/*divisor<=DIGITSBASE*/, 1)) ;

   // TEST divi32_decimal: EOVERFLOW
   TEST(0 == setfromint32_decimal(&dec[1], 1, -expmax_decimal())) ;
   TEST(0 == divi32_decimal(&dec[0], dec[1], 1, 1)) ;
   TEST(EOVERFLOW == divi32_decimal(&dec[0], dec[1], 1, 1+1)/*negative overflow*/) ;
   TEST(0 == setfromint32_decimal(&dec[1], DIGITSBASE, expmax_decimal())) ;
   TEST(EOVERFLOW == divi32_decimal(&dec[0], dec[1], 1, 1)/*positive overflow*/) ;

   // TEST div_decimal
   const char * testvalues2[][3] = {   // format { "dividend", "divisor (integer)", "result" }
      // -- single digit divisor test cases --
      { "999999999", "9", "111111111" }
      ,{ "999999999999999999", "999999999", "1000000001" }
      ,{ "2999999997", "3", "999999999" }
      ,{ "1e9",        "3", "333333333.333333333" }
      ,{ "123499004370324769803247640e90", "923000917", "133801605280880527e90" }
      ,{ "10999999999", "9", "1222222222" }
      ,{ "499543004370324769e-9", "1e9", "499543004370324769e-18" }
      // -- multi digit divisor test cases --
      ,{ "899888889010000010010000008889999991000000001999999541", "100000001000000000999999999", "8998888800111112008999992087" }
      ,{ "9881245440914852140098478367561748879876754510009578359098870959089528125721598908215098209581235908125078125092359082350900000009888888888888888888000000000000000000", "1230099574900153608764876090342450000235269878923498729385783403078950", "8032882575149988549864114671242186043971041932827507062390650202637896000132386890637429153470535.219383671" }
      ,{ "1107036002107000063123455555", "123004000123000007", "9000000008.129816909" }
      ,{ "11000001010000009010000095889999991000000001999999541", "100000001000000000999999999", "110000008999999999000000879" }
      // test left operand has lesser digits than right operand
      ,{ "999999999", "999999999999999999", "999999999000000000999999999000000000999999999e-54" }
      // test nextdigit = 1 and then corrected to 0 at first digit
      ,{ "999999999999999999000000000000000000000000008", "999999999999999999000000000000000000000000009", "999999999999999999999999999e-27" }
      // test nextdigit = 1 and then corrected to 0 at third digit
      ,{ "13591124284850460851201491210955792000000007", "123555666330955728000000008", "110000008000000000.999999999" }
      // test nextdigit = 1 and then corrected to 0 at last digit
      ,{ "28982192085943117416180110727744619210955792000000007", "123555666330955728000000008", "234567891110000008e9" }
      // test correction sum = DIGITSBASE
      ,{ "9000000015999999999999999992e27", "1000000001999999999999999999", "8999999998000000003999999993" }
      // test correction sum = DIGITSBASE in special case
      ,{ "1000000001000000001000000000000000000000000000000000000000000000", "1000000001000000001999999999", "999999999999999999000000001999999999" }
      // test dividend == divisor
      ,{ "1000000001888888888888888888000000000", "1000000001888888888888888889", "999999999999999999e-9" }
      ,{ "9000000009888888888888888888000000000000000000", "9000000009888888888888888889", "999999999999999999" }
   } ;
   for (unsigned tvi = 0; tvi < nrelementsof(testvalues2); ++tvi) {
      for (unsigned i = 0; i < nrelementsof(testvalues2[0]); ++i) {
         TEST(0 == setfromchar_decimal(&dec[i], strlen(testvalues2[tvi][i]), testvalues2[tvi][i])) ;
      }
      for (int s = -1; s <= +1; s += 2) {
         TEST(0 == div_decimal(&dec[3], dec[0], dec[1], size_decimal(dec[2]))) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[1]) ;
         negate_decimal(dec[2]) ;
         TEST(0 == div_decimal(&dec[3], dec[0], dec[1], size_decimal(dec[2]))) ;
         TEST(0 == cmp_decimal(dec[3], dec[2])) ;
         negate_decimal(dec[0]) ;
         negate_decimal(dec[2]) ;
      }
   }

   // TEST div_decimal: EINVAL
   TEST(0 == setfromint32_decimal(&dec[1], 1, 0)) ;
   TEST(0 == setfromint32_decimal(&dec[2], 0, 0)) ;
   TEST(EINVAL == div_decimal(&dec[0], dec[1], dec[2]/*divisor == 0*/, digitsperint_decimal())) ;

   // TEST div_decimal: EOVERFLOW
   TEST(0 == setfromint32_decimal(&dec[1], DIGITSBASE, expmax_decimal())) ;
   TEST(0 == setfromint32_decimal(&dec[2], 1, 0)) ;
   TEST(EOVERFLOW == div_decimal(&dec[0], dec[1], dec[2], 1)) ;   // exponent too big
   TEST(0 == setfromint32_decimal(&dec[1], 1, -expmax_decimal())) ;
   TEST(0 == setfromint32_decimal(&dec[2], 1, digitsperint_decimal())) ;
   TEST(EOVERFLOW == div_decimal(&dec[0], dec[1], dec[2], 1)) ;   // exponent too small
   TEST(0 == setfromint32_decimal(&dec[1], 1, -expmax_decimal())) ;
   TEST(0 == setfromint32_decimal(&dec[2], 1, 0)) ;
   TEST(EOVERFLOW == div_decimal(&dec[0], dec[1], dec[2], 2)) ;   // exponent too small


   // unprepare
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      TEST(0 == delete_decimal(&dec[i])) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < nrelementsof(dec); ++i) {
      delete_decimal(&dec[i]) ;
   }
   return EINVAL ;
}

static int test_tocstring(void)
{
   decimal_t   * dec = 0 ;
   cstring_t   cstr  = cstring_INIT ;
   char        buffer[20] ;

   // prepare
   TEST(0 == new_decimal(&dec, nrdigitsmax_decimal())) ;

   // TEST tocstring_decimal: only one digit (leading zero digits)
   for (int32_t ti = 1, digit = 1; ti <= 9; ++ti, digit *= 10, digit += ti) {
      const size_t L = (size_t) ti ;
      TEST(0 == setfromint32_decimal(&dec, digit, 0)) ;
      TEST(0 == tocstring_decimal(dec, &cstr)) ;
      TEST(L == length_cstring(&cstr)) ;
      snprintf(buffer, sizeof(buffer), "%"PRIu32, digit) ;
      TEST(0 == strcmp(buffer, str_cstring(&cstr))) ;
   }

   // TEST tocstring_decimal: only one digit (trailing zero digits)
   for (int32_t ti = 1, digit = 100000000; ti <= 9; ++ti, digit /= 10, digit += ti*100000000) {
      const size_t L = (size_t) (ti == 9 ? 9 : ti + 2) ;
      TEST(0 == setfromint32_decimal(&dec, digit, 0)) ;
      TEST(0 == tocstring_decimal(dec, &cstr)) ;
      TEST(L == length_cstring(&cstr)) ;
      snprintf(buffer, sizeof(buffer)-2, "%"PRIu32, digit) ;
      unsigned exponent = 0 ;
      for (size_t i = strlen(buffer); i && buffer[i-1] == '0'; --i) {
         ++ exponent ;
      }
      if (exponent) {
         snprintf(buffer+strlen(buffer)-exponent, sizeof(buffer)-strlen(buffer)+exponent, "e%d", exponent) ;
      }
      TEST(0 == strcmp(buffer, str_cstring(&cstr))) ;
   }

   // TEST tocstring_decimal: only one digit (mixed leading and trailing zeros)
   uint32_t testmixed[] = { 1, 9, 22, 88, 678, 901, 1008, 9999, 10204, 99999, 405123, 999999, 9050602, 9999999, 10013101, 11111111 } ;
   for (unsigned ti = 0; ti < nrelementsof(testmixed); ++ti) {
      for (uint64_t digit = testmixed[ti] * (uint64_t)100000000; digit; digit /= 10) {
         int      exponent = 0 ;
         uint32_t d = (uint32_t) (digit % 1000000000) ;
         size_t   L = 2 * (0 == (d % 10)) ;
         while (0 == (d % 10)) {
            d /= 10 ;
            ++ exponent ;
         }
         L += 1 + log10_int(d) ;
         TEST(0 == setfromint32_decimal(&dec, (int32_t) (digit % 1000000000), 0)) ;
         TEST(0 == tocstring_decimal(dec, &cstr)) ;
         TEST(L == length_cstring(&cstr)) ;
         if (exponent) {
            snprintf(buffer, sizeof(buffer), "%"PRIu32"e%d", d, exponent) ;
         } else {
            snprintf(buffer, sizeof(buffer), "%"PRIu32, d) ;
         }
         TEST(0 == strcmp(buffer, str_cstring(&cstr))) ;
      }
   }

   // TEST tocstring_decimal: maximum size
   for (unsigned ti = 1; ti <= 9; ++ti) {
      uint32_t testdigit = ti * 111111111 ;
      for (unsigned i = 0; i < sizemax_decimal(); ++i) {
         dec->digits[i] = testdigit ;
      }
      dec->sign_and_used_digits = (int8_t) - (int32_t) sizemax_decimal() ;
      dec->exponent             = (int16_t) - INT16_MAX ;
      TEST(0 == tocstring_decimal(dec, &cstr)) ;
      size_t L = 1 + nrdigitsmax_decimal() + 8 ;
      TEST(L == length_cstring(&cstr)) ;
      TEST(0 == str_cstring(&cstr)[L]) ;
      TEST('-' == (uint8_t)str_cstring(&cstr)[0]) ;
      for (unsigned i = 1; i < L-8; ++i) {
         TEST(('0'+ti) == (uint8_t)str_cstring(&cstr)[i]) ;
      }
      TEST(0 == strcmp(str_cstring(&cstr)+L-8, "e-294903")) ;
   }

   // TEST tocstring_decimal: exponent, positive/negative
   const char * testvalues[] = {    "9e-5", "1e-8", "1333e-20000", "123456789e-11"
                                    ,"123456789e3", "123456789e-3", "123456789e-18"
                                    ,"12345678900000000000000000000000123456789e211"
                                    ,"12345678900000000000000000000000123456789e212"
                                    ,"12345678900000000000000000000000123456789e214"
                                    ,"1000000000009000000000008e31234"
                                    ,"10000000000090000000000080007e-32001"
                                    ,"99009444403332194566e-32001"
                                    ,"1230456780912345657809e-32760"
                               } ;
   for (unsigned ti = 0; ti < nrelementsof(testvalues); ++ti) {
      size_t L = strlen(testvalues[ti]) ;
      TEST(0 == setfromchar_decimal(&dec, L, testvalues[ti])) ;
      TEST(0 == tocstring_decimal(dec, &cstr)) ;
      TEST(L == length_cstring(&cstr)) ;
      TEST(0 == strcmp(str_cstring(&cstr), testvalues[ti])) ;
      negate_decimal(dec) ;
      ++ L ;
      TEST(0 == tocstring_decimal(dec, &cstr)) ;
      TEST(L == length_cstring(&cstr)) ;
      TEST(0 == strncmp(str_cstring(&cstr), "-", 1)) ;
      TEST(0 == strcmp(str_cstring(&cstr)+1, testvalues[ti])) ;
   }

   // unprepare
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == delete_decimal(&dec)) ;

   return 0 ;
ONABORT:
   free_cstring(&cstr) ;
   delete_decimal(&dec) ;
   return EINVAL ;
}

static int test_example1(void)
{
   // evaluate f(a = 77617, b = 33096)
   // f = 333.75*pow(b,6) + pow(a,2)*(11*pow(a,2)*pow(b,2) - pow(b,6) - 121*pow(b,4) - 2) + 5.5*pow(b,8) + a/(2*b)
   // exact result: - (54767 / 66192)

   long double a = 77617 ;
   long double b = 33096 ;

   long double f = 2 * b * (333.75*powl(b,6) + powl(a,2)*(11*powl(a,2)*powl(b,2) - powl(b,6) - 121*powl(b,4) - 2) + 5.5*powl(b,8)) + a ;

   TEST(f != -54767) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_math_float_decimal()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == switchon_mmtest()) ;
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_decimaltables())  goto ONABORT ;
   if (test_helper())         goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_signops())        goto ONABORT ;
   if (test_copy())           goto ONABORT ;
   if (test_setfromint())     goto ONABORT ;
   if (test_setfromfloat())   goto ONABORT ;
   if (test_setfromchar())    goto ONABORT ;
   if (test_compare())        goto ONABORT ;
   if (test_addsub())         goto ONABORT ;
   if (test_mult())           goto ONABORT ;
   if (test_div())            goto ONABORT ;
   if (test_tocstring())      goto ONABORT ;
   if (test_example1())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == switchoff_mmtest()) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   switchoff_mmtest() ;
   return EINVAL ;
}

#endif
