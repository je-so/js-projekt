/* title: BigInteger impl

   Implements <BigInteger>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/bigint.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/sign.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/math/fpu.h"
#include "C-kern/api/memory/mm/mmtest.h"
#include "C-kern/api/test/errortimer.h"
#endif


// section: bigint_t

// group: helper

#define print_biginthelper(big)                                \
      {                                                        \
            uint16_t _i = nrdigits_bigint(big) ;               \
            while (_i > 0 && 0 == big->digits[_i-1]) --_i ;    \
            if (_i) {                                          \
               while (_i > 0) {                                \
                  -- _i ;                                      \
                  printf("%u: %x\n", _i, big->digits[_i]) ;    \
               }                                               \
            } else {                                           \
               printf("0: 0\n") ;                              \
            }                                                  \
            printf("-----\n") ;                                \
      }                                                        \

static inline uint32_t objectsize_bigint(uint16_t allocated_digits)
{
   return (uint32_t) (sizeof(bigint_t) + sizeof(((bigint_t*)0)->digits[0]) * allocated_digits) ;
}

static int allocate_bigint(bigint_t *restrict* big, uint32_t allocate_digits)
{
   int err ;

   if (allocate_digits < 4) allocate_digits = 4 ;

   // check that allocate_digits can be cast to int16_t (so sign_and_used_digits does not overflow)
   if (allocate_digits > INT16_MAX) {
      static_assert(    0 > (typeof((*big)->sign_and_used_digits))-1
                     && 2 == sizeof((*big)->sign_and_used_digits), "Test sign_and_used_digits is of type INT16") ;
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   bigint_t * oldbig   = *big ;
   uint32_t oldobjsize = oldbig ? objectsize_bigint(oldbig->allocated_digits) : 0 ;

   if (  oldbig
      && !oldbig->allocated_digits) {
      // bigint_fixed_t can not be reallocated
      err = EINVAL ;
      goto ONABORT ;
   }

   uint32_t newobjsize = objectsize_bigint((uint16_t)allocate_digits) ;
   memblock_t  mblock  = memblock_INIT(oldobjsize, (uint8_t*)oldbig) ;

   err = MM_RESIZE(newobjsize, &mblock) ;
   if (err) goto ONABORT ;

   bigint_t       * newbig      = (bigint_t*) mblock.addr ;
   newbig->allocated_digits     = (uint16_t) allocate_digits ;
   newbig->sign_and_used_digits = 0 ;
   newbig->exponent             = 0 ;

   *big = newbig ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

/* function: xorsign_biginthelper
 * Returns the sign of combined <bigint_t.sign_and_used_digits> fields.
 * The returned value is either < 0 if exactly one of the parameter is negative.
 * The returned value is >= 0 if both parameter are either negative (<0) or positive (>=0).
 *
 * Compiler Implementation Dependent:
 * This function uses bit operations on signed integers which is implementation-defined. */
static inline int16_t xorsign_biginthelper(int16_t lsign, int16_t rsign)
{
   return lsign ^ rsign ;
}

/* function: add_biginthelper
 * Adds two integers.
 * The result has the same sign as *lbig*. */
static int add_biginthelper(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig)
{
   int err ;
   uint16_t       lnrdigits = nrdigits_bigint(lbig) ;
   uint16_t       rnrdigits = nrdigits_bigint(rbig) ;
   bool           isNegSign = isnegative_bigint(lbig) ;
   bool           carry ;
   uint32_t       lexp      = exponent_bigint(lbig) ;
   uint32_t       rexp      = exponent_bigint(rbig) ;
   const uint32_t * ldigits = lbig->digits ;
   const uint32_t * rdigits = rbig->digits ;

   // skip trailing zero in both operands
   while (lnrdigits && 0 == ldigits[0]) {
      -- lnrdigits  ;
      ++ ldigits ;
      ++ lexp ;
   }
   while (rnrdigits && 0 == rdigits[0]) {
      -- rnrdigits ;
      ++ rdigits ;
      ++ rexp ;
   }

   if (lexp > UINT16_MAX && rexp > UINT16_MAX) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   if (!rnrdigits) {
      return copy_bigint(result, lbig) ;
   }
   if (!lnrdigits) {
      err = copy_bigint(result, rbig) ;
      if (err) return err ;
      setpositive_bigint(*result) ;
      return 0 ;
   }

   uint32_t lorder  = lexp + lnrdigits ;
   uint32_t rorder  = rexp + rnrdigits ;

   if (lorder < rorder) {
      // swap rbig / lbig to make lbig the bigger number
      SWAP(lbig,    rbig) ;
      SWAP(lnrdigits, rnrdigits) ;
      SWAP(lexp,    rexp) ;
      SWAP(ldigits, rdigits) ;
   }

   /*
    * bigger number:    lbig { lnrdigits, lexp, ldigits }
    * smaller number:   rbig { rnrdigits, rexp, rdigits }
    * Invariant: lexp + lnrdigits >= rexp + rnrdigits
    *
    * Calculate result = pow(-1,isNegSign) * (lbig + rbig)
    */

   int32_t expdiff = (int32_t)lexp - (int32_t)rexp ;

   /*
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭────────────┬─────┬──────────────────┬───┬──────────────────────╮
    *   │  ... lexp ...                  │ ldigits[0] │ ... │ ldigits[X]       │...│ ldigits[lnrdigits-1] │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰────────────┴─────┴──────────────────┴───┴──────────────────────╯
    *                   |-  expdiff>0   -|                                                    lorder -╯
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭────────────┬───┬──────────────────┬──────────────────────╮
    *   │  ... rexp ... │ rdigits[0] │...│      ...         │ rdigits[rnrdigits-1] │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰────────────┴───┴──────────────────┴──────────────────────╯
    *                                    |-  expdiff<0     -|
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭──────────────────╮
    *   │  ... rexp ...                                     │        ...       │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰──────────────────╯
    *                                                                  rorder -╯
    */

   uint32_t size = lnrdigits + (uint32_t)1/*carry*/ ;
   if (expdiff > 0) size += (uint32_t)expdiff ;

   if ((*result)->allocated_digits < size) {
      err = allocate_bigint(result, size) ;
      if (err) goto ONABORT ;
   }

   uint32_t * destdigits = (*result)->digits ;

   /* handle trail
    * */
   if (expdiff < 0) {
      // simple copy
      expdiff = -expdiff ;
      memcpy(destdigits, ldigits, sizeof(destdigits[0]) * (uint32_t)expdiff) ;
      lnrdigits = (uint16_t) (lnrdigits - expdiff) ; // result always >= 0 cause lbig has at least rsize(possibly 0) more digits
      ldigits  += expdiff ;
      destdigits += expdiff ;
   } else if (expdiff > 0) {
      // simple copy
      if (rnrdigits < expdiff) {
         memcpy(destdigits, rdigits, sizeof(destdigits[0]) * rnrdigits) ;
         memset(destdigits + rnrdigits, 0, sizeof(destdigits[0]) * ((uint32_t)expdiff-rnrdigits)) ;
         rnrdigits = 0 ;
      } else {
         memcpy(destdigits, rdigits, sizeof(destdigits[0]) * (uint32_t)expdiff) ;
         rnrdigits = (uint16_t) (rnrdigits - expdiff) ;
         rdigits  += expdiff ;
      }
      destdigits += expdiff ;
   }

   /* handle overlapping part
    * */
   carry = false ;
   lnrdigits = (uint16_t) (lnrdigits - rnrdigits) ;
   for (; rnrdigits; --rnrdigits) {
      uint32_t dl  = *(ldigits++) + carry ;
      uint32_t sum = *(rdigits++) + dl ;
      *(destdigits++) = sum ;
      carry = (sum < dl) || (carry && (0 == dl)) ;
   }

   /* handle leading part
    * */
   for (; lnrdigits; --lnrdigits) {
      uint32_t dl = *(ldigits++) + carry ;
      *(destdigits++) = dl ;
      carry = carry && (0 == dl) ;
   }

   /* handle carry part
    * */
   size -= (0 == carry) ;
   destdigits[0] = carry ;

   (*result)->sign_and_used_digits = (int16_t)  (isNegSign ? -(int32_t)size : (int32_t)size) ;
   (*result)->exponent             = (uint16_t) (lexp < rexp ? lexp : rexp) ; ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

/* function: sub_biginthelper
 * Subtracts two integers.
 * The sign of lbig is used to determine the sign of the result.
 * If the magnitude of lbig is bigger than rbig (<cmpmagnitude_bigint>)
 * the result has the same sign as lbig else it is reverted. */
static int sub_biginthelper(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig)
{
   int err ;
   uint16_t       lnrdigits = nrdigits_bigint(lbig) ;
   uint16_t       rnrdigits = nrdigits_bigint(rbig) ;
   bool           isNegSign = isnegative_bigint(lbig) ;
   bool           carry ;
   bool           isSwap ;
   uint32_t       lexp      = exponent_bigint(lbig) ;
   uint32_t       rexp      = exponent_bigint(rbig) ;
   const uint32_t * ldigits = lbig->digits ;
   const uint32_t * rdigits = rbig->digits ;

   // skip trailing zero in both operands
   while (lnrdigits && 0 == ldigits[0]) {
      -- lnrdigits  ;
      ++ ldigits ;
      ++ lexp ;
   }
   while (rnrdigits && 0 == rdigits[0]) {
      -- rnrdigits ;
      ++ rdigits ;
      ++ rexp ;
   }

   if (lexp > UINT16_MAX && rexp > UINT16_MAX) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   if (!rnrdigits) {
      return copy_bigint(result, lbig) ;
   }
   if (!lnrdigits) {
      err = copy_bigint(result, rbig) ;
      if (err) return err ;
      setnegative_bigint(*result) ;
      return 0 ;
   }

   uint32_t lorder  = lexp + lnrdigits ;
   uint32_t rorder  = rexp + rnrdigits ;

   // determine which number is bigger

   if (lorder == rorder) {
      // compare digits to check if numbers are equal
      uint16_t minsize = (uint16_t) (lnrdigits < rnrdigits ? lnrdigits : rnrdigits) ;
      lnrdigits = (uint16_t) (lnrdigits - minsize) ;
      rnrdigits = (uint16_t) (rnrdigits - minsize) ;
      for (;;) {
         if (ldigits[lnrdigits+minsize-1] != rdigits[rnrdigits+minsize-1]) {
            isSwap = (ldigits[lnrdigits+minsize-1] < rdigits[rnrdigits+minsize-1]) ;
            lnrdigits = (uint16_t) (lnrdigits + minsize) ;
            rnrdigits = (uint16_t) (rnrdigits + minsize) ;
            break ;
         }
         --minsize ;
         if (0 == minsize) {
            isSwap = (0 != rnrdigits) ;
            if (isSwap || lnrdigits) break ;
            // both numbers are equal
            setfromuint32_bigint(*result, 0) ;
            return 0 ;
         }
      }

   } else {
      // compare exponent of first digit
      isSwap = (lorder < rorder) ;
   }

   if (isSwap) {
      // swap rbig / lbig to make lbig the bigger number
      isNegSign = !isNegSign ;
      SWAP(lbig,    rbig) ;
      SWAP(lnrdigits, rnrdigits) ;
      SWAP(lexp,    rexp) ;
      SWAP(ldigits, rdigits) ;
   }

   /*
    * bigger number:    lbig { lnrdigits, lexp, ldigits }
    * smaller number:   rbig { rnrdigits, rexp, rdigits }
    * Invariant: cmpmagnitude_bigint(lbig,rbig) > 0 !
    *
    * Calculate result = pow(-1,isNegSign) * (lbig - rbig)
    */

   int32_t expdiff = (int32_t)lexp - (int32_t)rexp ;

   /*
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭────────────┬─────┬──────────────────┬───┬──────────────────────╮
    *   │  ... lexp ...                  │ ldigits[0] │ ... │ ldigits[X]       │...│ ldigits[lnrdigits-1] │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰────────────┴─────┴──────────────────┴───┴──────────────────────╯
    *                   |-  expdiff>0   -|                                                    lorder -╯
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭────────────┬───┬──────────────────┬──────────────────────╮
    *   │  ... rexp ... │ rdigits[0] │...│      ...         │ rdigits[rnrdigits-1] │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰────────────┴───┴──────────────────┴──────────────────────╯
    *                                    |-  expdiff<0     -|
    *   ╭┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╭──────────────────╮
    *   │  ... rexp ...                                     │        ...       │
    *   ╰┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈╰──────────────────╯
    *                                                                  rorder -╯
    */

   uint32_t size = lnrdigits ;
   if (expdiff > 0) size += (uint32_t)expdiff ;

   if ((*result)->allocated_digits < size) {
      err = allocate_bigint(result, size) ;
      if (err) goto ONABORT ;
   }

   uint32_t * destdigits = (*result)->digits ;

   /* handle trail
    * */
   carry = false ;
   if (expdiff < 0) {
      // simple copy
      expdiff = -expdiff ;
      memcpy(destdigits, ldigits, sizeof(destdigits[0]) * (uint32_t)expdiff) ;
      lnrdigits = (uint16_t) (lnrdigits - expdiff) ; // result always >= 0 cause lbig has at least rsize(possibly 0) more digits
      ldigits  += expdiff ;
      destdigits += expdiff ;
   } else if (expdiff > 0) {
      // copy inverted digits (rdigits[0] != 0) => (UINT32_MAX+1 - *(rdigits++)) <= UINT32_MAX && carry_is_set
      *(destdigits++) = UINT32_MAX - *(rdigits++) + 1 ;
      if (rnrdigits < expdiff) {
         expdiff -= rnrdigits ;
         while (--rnrdigits) {
            *(destdigits++) = UINT32_MAX - *(rdigits++) ;
         }
         do {
            *(destdigits++) = UINT32_MAX ;
         } while (--expdiff) ;
      } else {
         rnrdigits = (uint16_t) (rnrdigits - expdiff) ;
         while (--expdiff) {
            *(destdigits++) = UINT32_MAX - *(rdigits++) ;
         }
      }
      carry = true ;
   }

   /* handle overlapping part
    * */
   lnrdigits = (uint16_t) (lnrdigits - rnrdigits) ;
   for (; rnrdigits; --rnrdigits) {
      uint32_t dl = *(ldigits++) ;
      uint32_t dr = *(rdigits++) ;
      bool isUnderflow = (dl < dr) || (dl == dr && carry) ;
      *(destdigits++) = dl - dr - carry ;
      carry = isUnderflow ;
   }

   /* handle leading part
    * */
   for (; lnrdigits; --lnrdigits) {
      uint32_t dl = *(ldigits++) ;
      *(destdigits++) = dl - carry ;
      carry = (dl < carry) ;
   }

   while (0 == destdigits[-1]) {
      -- destdigits ;
      -- size ;
   }

   (*result)->sign_and_used_digits = (int16_t)  (isNegSign ? -(int32_t)size : (int32_t)size) ;
   (*result)->exponent             = (uint16_t) (lexp < rexp ? lexp : rexp) ; ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

/* function: mult_biginthelper
 * Multiplies two numbers with the simple schoolbook algorithm.
 *
 * (unchecked) Preconditions:
 * 1. big must be allocated with size == (lnrdigits + rnrdigits)
 * 2. lnrdigits > 0 && rnrdigits > 0
 * 3. lnrdigits <= rnrdigits
 * 4. ldigits[lnrdigits-1] != 0 && rdigits[rnrdigits-1] != 0
 * */
static void mult_biginthelper(bigint_t * big, uint16_t lnrdigits, const uint32_t * ldigits, uint16_t rnrdigits, const uint32_t * rdigits, const uint16_t exponent)
{
   uint32_t size   = (uint32_t)lnrdigits + rnrdigits ;
   uint32_t carry  = 0 ;
   uint32_t factor = rdigits[0] ;
   uint32_t * digits = big->digits ;

   if (0 == factor) {
      memset(digits, 0, sizeof(digits[0]) * (1u/*carry*/+lnrdigits)) ;
   } else {
      for (uint16_t li = 0; li < lnrdigits; ++li) {
         uint64_t product = ldigits[li] * (uint64_t) factor ;
         product += carry ;

         digits[li] = (uint32_t) product ;
         carry      = (uint32_t) (product >> 32) ;
      }
      digits[lnrdigits] = carry ;
   }

   for (uint16_t ri = 1; ri < rnrdigits; ++ri) {
      factor = rdigits[ri] ;
      carry  = 0 ;
      ++ digits ;
      if (factor) {
         for (uint16_t li = 0; li < lnrdigits; ++li) {
            uint64_t product = ldigits[li] * (uint64_t) factor ;
            product += digits[li] ;
            product += carry ;

            digits[li] = (uint32_t) product ;
            carry      = (uint32_t) (product >> 32) ;
         }
      }
      digits[lnrdigits] = carry ;
   }

   size -= (0 == carry) ; // if no carry occurred => make result one digit smaller
   big->sign_and_used_digits = (int16_t) size ;
   big->exponent             = exponent ;

   return ;
}

static void addsplit_biginthelper(bigint_t * big, uint16_t digitsoffset, uint16_t lnrdigits, const uint32_t * ldigits, uint16_t rnrdigits, const uint32_t * rdigits)
{
   uint32_t sum ;
   uint32_t * digits = &big->digits[digitsoffset] ;

   if (lnrdigits < rnrdigits) {
      // swap to make lnrdigits >= rnrdigits
      SWAP(lnrdigits, rnrdigits) ;
      SWAP(ldigits, rdigits) ;
   }

   uint16_t size = (uint16_t) (lnrdigits + digitsoffset/*no carry*/) ;
   bool     carry ;

   /* handle overlapping part
    * */
   carry = false ;
   lnrdigits = (uint16_t) (lnrdigits - rnrdigits) ;
   for (; rnrdigits; --rnrdigits) {
      uint32_t dl = *(ldigits++) + carry ;
      sum = *(rdigits++) + dl ;
      *(digits++) = sum ;
      carry = (sum < dl) || (carry && (0 == dl)) ;
   }

   /* handle leading part
    * */
   for (; lnrdigits; --lnrdigits) {
      sum = *(ldigits++) + carry ;
      *(digits++) = sum ;
      carry = carry && (0 == sum) ;
   }

   /* handle carry part
    * */
   if (carry) {
      ++ size ;
      digits[0] = 1 ;
   }

   big->sign_and_used_digits = (int16_t) size ;

   return ;
}

/* function: multsplit_biginthelper
 * Multiplies two big integer numbers and returns the positive result.
 * The multiplication is done with splitting of numbers to save one multiplication.
 *
 * (unchecked) Preconditions:
 * 1. result must be allocated with size == (lnrdigits + rnrdigits)
 * 2. After skipping of trailing zeros: (lnrdigits > 0 && rnrdigits > 0)
 * 3. (ldigits[lnrdigits-1] != 0 && rdigits[rnrdigits-1] != 0)
 * */
static int multsplit_biginthelper(bigint_t *restrict * result, uint16_t lnrdigits, const uint32_t * ldigits, uint16_t rnrdigits, const uint32_t * rdigits)
{
   int err ;
   bigint_t * t[5]   = { 0, 0, 0, 0, 0 } ;
   uint32_t exponent = 0 ;

   // skip trailing zero in both operands
   while (lnrdigits && 0 == ldigits[0]) {
      -- lnrdigits  ;
      ++ ldigits ;
      ++ exponent ;
   }
   while (rnrdigits && 0 == rdigits[0]) {
      -- rnrdigits ;
      ++ rdigits ;
      ++ exponent ;
   }

   if (lnrdigits > rnrdigits) {
      // swap to make lnrdigits <= rnrdigits
      SWAP(lnrdigits, rnrdigits) ;
      SWAP(ldigits, rdigits) ;
   }

   if (lnrdigits <= 48) {
      mult_biginthelper(*result, lnrdigits, ldigits, rnrdigits, rdigits, (uint16_t) exponent) ;
      return 0 ;
   }

   /* define: X = pow(UINT32_MAX+1, split)
    *         Multiplying by X means to increment the exponent with X
    *
    * divide  [lnrdigits,ldigits] into [ l0 * X + l1 ]
    * and     [rnrdigits,rdigits] into [ r0 * X + r1 ]
    *
    *
    * Calculate t0 = l0 * r0
    *           t1 = l1 * r1
    * and   result = t0 *X*X + t1 + ((l0+l1) * (r0+r1) - t0 - t1) * X
    *
    * */
   uint16_t split  = (uint16_t) (lnrdigits / 2) ;  // => (lnrdigits-split) >= split
   uint16_t lsplit = split ;
   while (0 == ldigits[lsplit-1]) --lsplit ; // ldigits[0] != 0 => loop ends !
   uint16_t rsplit = split ;
   while (0 == rdigits[rsplit-1]) --rsplit ; // rdigits[0] != 0 => loop ends !

   // compute t0
   uint32_t size = (uint32_t) ((lnrdigits - split) + (rnrdigits - split)) ;
   err = allocate_bigint(&t[0], size) ;
   if (err) goto ONABORT ;
   err = multsplit_biginthelper(&t[0], (uint16_t)(lnrdigits - split), &ldigits[split], (uint16_t)(rnrdigits - split), &rdigits[split]) ;
   if (err) goto ONABORT ;

   // compute t1
   size = (uint32_t)lsplit + rsplit ;
   err = allocate_bigint(&t[1], size) ;
   if (err) goto ONABORT ;
   err = multsplit_biginthelper(&t[1], lsplit, ldigits, rsplit, rdigits) ;
   if (err) goto ONABORT ;

   // compute t2 == (l0+l1)
   size = (uint32_t)lnrdigits - split + 1/*carry*/ ;
   err = allocate_bigint(&t[2], size) ;
   if (err) goto ONABORT ;
   addsplit_biginthelper(t[2], 0, (uint16_t)(lnrdigits - split), &ldigits[split], lsplit, ldigits) ;

   // compute t3 == (r0+r1)
   size = (uint32_t)rnrdigits - split + 1/*carry*/ ;
   err = allocate_bigint(&t[3], size) ;
   if (err) goto ONABORT ;
   addsplit_biginthelper(t[3], 0, (uint16_t)(rnrdigits - split), &rdigits[split], rsplit, rdigits) ;

   // compute t4 == (l0+l1) * (r0+r1)
   size = (uint32_t)t[2]->sign_and_used_digits + (uint32_t)t[3]->sign_and_used_digits ;
   err = allocate_bigint(&t[4], size) ;
   if (err) goto ONABORT ;
   err = multsplit_biginthelper(&t[4], (uint16_t)t[2]->sign_and_used_digits, t[2]->digits, (uint16_t)t[3]->sign_and_used_digits, t[3]->digits) ;
   if (err) goto ONABORT ;

   // compute t4 = (l0+l1) * (r0+r1) - t0 - t1
   err = sub_bigint(result, t[4], t[0]) ;
   if (err) goto ONABORT ;
   err = sub_bigint(&t[4], *result, t[1]) ;
   if (err) goto ONABORT ;

   // compute *result = t0 *X*X + t1
   t[0]->exponent = (uint16_t) (t[0]->exponent + 2 * split) ;
   err = add_bigint(result, t[0], t[1]) ;
   if (err) goto ONABORT ;

   // compute *result = t0 *X*X + t1 + t4 * X
   uint16_t offset = (uint16_t) (t[4]->exponent + split/*X*/ - (*result)->exponent) ;
   addsplit_biginthelper(*result, offset, (uint16_t)((*result)->sign_and_used_digits-offset), &(*result)->digits[offset], (uint16_t)t[4]->sign_and_used_digits, t[4]->digits) ;

   (*result)->exponent = (uint16_t) ((*result)->exponent + exponent) ;

   // free resources
   for (unsigned i = 0; i < nrelementsof(t); ++i) {
      err = delete_bigint(&t[i]) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < nrelementsof(t); ++i) {
      delete_bigint(&t[i]) ;
   }
   clear_bigint(*result) ;
   LOG_ABORT(err) ;
   return err ;
}

static int divisorisbigger_biginthelper(bigint_t *restrict* divresult, bigint_t *restrict* modresult, const bigint_t * lbig)
{
   int err ;

   const uint16_t lnrdigits = nrdigits_bigint(lbig) ;

   // handle special case (lbig < divisor)

   if (0 == lnrdigits) {

      // (lbig == 0)

      if (divresult) {
         setfromuint32_bigint(*divresult, 0) ;
      }
      if (modresult) {
         setfromuint32_bigint(*modresult, 0) ;
      }

   } else {

      // (lbig != 0)

      if (divresult) {
         setfromuint32_bigint(*divresult, 0) ;
      }
      if (modresult) {
         err = copy_bigint(modresult, lbig) ;
         if (err) goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

/* function: estimatedigit2_biginthelper
 * Estimates the next digit of the quotient.
 * The returned number is (dividend / divisor).
 *
 * The estimated digit is either correct or one too large.
 *
 * Proof:
 * 1. divisor[1..2] * estimated_digit <= dividend
 * 2. divisor[1..len] * estimated_digit - divisor[1..2] * estimated_digit < estimated_digit * pow(base, len - 2)
 * 3. divisor[1..len] * estimated_digit < dividend + estimated_digit * pow(base, divisor_length - 2)
 * 4. divisor[1..len] * estimated_digit - estimated_digit * pow(base, divisor_length - 2) < dividend
 * 5. divisor[1..len] > pow(base, len - 2) =>
 * 6. divisor[1..len] * estimated_digit - estimated_digit * divisor[1..len] < dividend
 * 7. divisor[1..len] * (estimated_digit - 1) < dividend
 * */
static uint32_t estimatedigit2_biginthelper(const uint64_t dividend, const uint64_t divisor)
{
   uint32_t estimated_digit ;

   if (dividend == divisor) {

      estimated_digit = 1 ;
   } else if (dividend < divisor) {

      estimated_digit = 0 ;
   } else /*(dividend > divisor)*/ {

      // calculate:  dividend / divisor
      const unsigned shift_left = (63u ^ log2_int(divisor)) ;
      uint64_t denominator = divisor << shift_left ;
      uint64_t numerator   = dividend ;

      estimated_digit = 0 ;
      for (unsigned i = shift_left; i > 0; --i) {
         if (numerator >= denominator) {
            numerator -= denominator ;
            ++ estimated_digit ;
         }

         denominator >>= 1 ;
         estimated_digit <<= 1 ;
      }

      if (numerator >= denominator) {
         ++ estimated_digit ;
      }
   }

   return estimated_digit ;
}

/* function: estimatedigit3_biginthelper
 * Estimates the next digit of the quotient.
 * The returned number is ((((uint128_t)dividend << 32) + dividend2) / divisor).
 *
 * The estimated digit is either correct or one too large.
 *
 * Precondition:
 * 1. dividend <= divisor
 *
 * Special case (dividend == divisor):
 * In this case the returned value is UINT32_MAX and not the correct value (UINT32_MAX+1).
 * This special case can only occur if <estimatedigit2_biginthelper> estimated 1 (dividend == divisor)
 * but <submul_biginthelper> corrected this value to 0. (UINT32_MAX+1) is therefore too large for the
 * estimated digit and the returned value of UINT32_MAX is therefore the correct one.
 *
 * */
static uint32_t estimatedigit3_biginthelper(const uint64_t dividend, const uint32_t dividend2, const uint64_t divisor)
{
   uint32_t estimated_digit = 0 ;

   uint64_t numerator  = dividend ;
   uint32_t numerator2 = dividend2 ;

   for (unsigned i = 32; i > 0; --i) {
      estimated_digit <<= 1 ;

      bool isHighbit = (0 != (numerator & (uint64_t)0x8000000000000000)) ;
      numerator <<= 1 ;

      if (0 != (numerator2 & 0x80000000)) {
         ++ numerator ;
      }
      numerator2 <<= 1 ;

      if (  isHighbit
         || numerator >= divisor) {
         ++ estimated_digit ;
         numerator -= divisor ;
      }
   }

   return estimated_digit ;
}

/* function: submul_biginthelper
 * Multiplied estimated_digit with divisor and subtract it from diff.
 * The calculated difference is
 * >> diff = diff - (estimated_digit * divisor * pow(base, leftshift)) ;
 * It is returned in diff. If this difference is negative
 * estimated_digit is decremented and diff is incremented by 1 * divisor.
 *
 * The corrected estimated_digit is returned as return value.
 *
 * The parameter *diff_has_carry* is used to communicate the following:
 * true  - The number *estimated_digit* is constructed from 3 digits from diff. The number *diff* has
 *         one more digit to the left than *divisor* shifted left by *leftshift*.
 * false - The number *estimated_digit* is constructed from 2 digits from diff. The number *diff* has
 *         the same number of digits than *divisor* shifted left by *leftshift*.
 *
 * Preconditions:
 * 1. The big integer diff must have an exponent value which is 0.
 * 2. The shifted divisor (multiplying it with pow(base, leftshift)) must be lower
 *    or equal than diff.
 * 3. *estimated_digit > 0.
 * 4. (*estimated_digit - 1) * divisor * pow(base,leftshit) < diff
 * 5. diff must have an extra digit set to 0 for carry overflow
 * */
static uint32_t submul_biginthelper(bigint_t * diff, const bool diff_has_carry, uint32_t estimated_digit, const uint32_t leftshift, const uint16_t divnrdigits, const bigint_t * divisor)
{
   uint16_t diffoffset   = (uint16_t) (leftshift + divisor->exponent) ;

   uint32_t factor = estimated_digit ;
   uint32_t carry = 0 ;

   for (uint16_t i = 0; i < divnrdigits; ++i) {
      uint64_t product ;

      product  = divisor->digits[i] ;
      product *= factor ;
      product += carry ;

      uint64_t ddiff = diff->digits[diffoffset + i] ;
      ddiff -= (uint32_t) product ;
      diff->digits[diffoffset + i] = (uint32_t) ddiff ;

      carry  = (uint32_t) (product >> 32) + (ddiff > UINT32_MAX) ;
   }

   if (carry) {

      bool needCorrection = !diff_has_carry ;

      if (!needCorrection) {
         needCorrection = (diff->digits[diffoffset + divnrdigits] < carry) ;
         diff->digits[diffoffset + divnrdigits] -= carry ;
      }

      if (needCorrection) {
         -- factor ;

         carry = 0 ;
         for (uint16_t i = 0; i < divnrdigits; ++i) {

            uint64_t sum = divisor->digits[i] ;
            sum += carry ;
            sum += diff->digits[diffoffset + i] ;
            diff->digits[diffoffset + i] = (uint32_t) sum ;

            carry  = (sum > UINT32_MAX) ;
         }

         if (diff_has_carry) {
            diff->digits[diffoffset + divnrdigits] += carry ;
         }
      }
   }

   return factor ;
}

// group: lifetime

int new_bigint(/*out*/bigint_t ** big, uint32_t nrdigits)
{
   int err ;

   bigint_t * new_big = 0 ;

   err = allocate_bigint(&new_big, nrdigits) ;
   if (err) goto ONABORT ;

   *big = new_big ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int newcopy_bigint(/*out*/bigint_t ** big, const bigint_t * copyfrom)
{
   int err ;
   bigint_t    * new_big  = 0 ;
   uint16_t    digits     = nrdigits_bigint(copyfrom) ;

   err = allocate_bigint(&new_big, (digits < 4 ? (uint32_t)4 : (uint32_t)digits)) ;
   if (err) goto ONABORT ;

   err = copy_bigint(&new_big, copyfrom) ;
   if (err) goto ONABORT ;

   *big = new_big ;

   return 0 ;
ONABORT:
   delete_bigint(&new_big) ;
   LOG_ABORT(err) ;
   return err ;
}

int delete_bigint(bigint_t ** big)
{
   int err ;
   bigint_t * del_big = *big ;

   if (del_big) {
      *big = 0 ;

      memblock_t  mblock = memblock_INIT(objectsize_bigint(del_big->allocated_digits), (uint8_t*) del_big) ;

      err = MM_FREE(&mblock) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: query

int cmp_bigint(const bigint_t * lbig, const bigint_t * rbig)
{
   /* compare sign */
   const int lsign = sign_bigint(lbig) ;
   const int rsign = sign_bigint(rbig) ;

   if (lsign != rsign) {
      return sign_int(lsign - rsign) ;
   } else if (0 > lsign) {
      /* both values negative => return negative of comparison */
      return cmpmagnitude_bigint(rbig, lbig) ;
   }

   return cmpmagnitude_bigint(lbig, rbig) ;
}

int cmpmagnitude_bigint(const bigint_t * lbig, const bigint_t * rbig)
{
   const uint16_t lnrdigits = nrdigits_bigint(lbig) ;
   const uint16_t rnrdigits = nrdigits_bigint(rbig) ;
   const int32_t  lmaxexp   = (int32_t)lnrdigits + lbig->exponent ;
   const int32_t  rmaxexp   = (int32_t)rnrdigits + rbig->exponent ;

   /* check for 0 values */

   if (  0 == lnrdigits
      || 0 == rnrdigits) {
      return sign_int((int32_t)lnrdigits - (int32_t)rnrdigits) ;
   }

   /* compare values: both values != 0 */

   if (lmaxexp != rmaxexp) {
      return sign_int(lmaxexp - rmaxexp) ;
   }

   uint16_t       mindigits = (uint16_t) (lnrdigits < rnrdigits ? lnrdigits : rnrdigits) ;
   const uint32_t * ldigits = &lbig->digits[lnrdigits] ;
   const uint32_t * rdigits = &rbig->digits[rnrdigits] ;
   do {
      uint32_t ld = *(--ldigits) ;
      uint32_t rd = *(--rdigits) ;
      if (ld != rd) {
         return ld > rd ? +1 : -1 ;
      }
   } while (--mindigits) ;

   if (lnrdigits != rnrdigits) {
      if (lnrdigits < rnrdigits) {
         mindigits = (uint16_t) (rnrdigits - lnrdigits) ;
         do {
            uint32_t rd = *(--rdigits) ;
            if (rd) return -1 ;
         } while (--mindigits) ;
      } else {
         mindigits = (uint16_t) (lnrdigits - rnrdigits) ;
         do {
            uint32_t ld = *(--ldigits) ;
            if (ld) return +1 ;
         } while (--mindigits) ;
      }
   }

   return 0 ;
}


double todouble_bigint(const bigint_t * big)
{
   const uint16_t digits = nrdigits_bigint(big) ;
   long double value ;
   uint64_t    ivalue ;
   uint32_t    first ;
   unsigned    bits ;
   int32_t     expadd ;

   switch (digits) {
   case 0:
            return 0 ;
   case 1:
            value  = (long double) big->digits[0] ;
            expadd = 32*big->exponent ;
            break ;
   case 2:
            ivalue = ((uint64_t)big->digits[1] << 32) + big->digits[0] ;
            value  = (long double) ivalue ;
            expadd = 32*big->exponent ;
            break ;
   default:
            expadd  = 32 * (big->exponent + (int32_t)digits - 2) ;
            first   = big->digits[digits-1] ;
            bits    = 31 - log2_int(first) ;
            expadd -= (int) bits ;
            ivalue  = ((uint64_t)first << 32) + big->digits[digits-2] ;
            if (bits) {
               ivalue <<= bits ;
               ivalue  |= big->digits[digits-3] >> (32 - bits) ;
            }
            value  = (long double) ivalue ;
            break ;
   }

   if (big->sign_and_used_digits < 0) value = -value ;
   if (expadd) value = ldexpl(value, expadd) ;

   return (double) value ;
}

// group: assign

void clear_bigint(bigint_t * big)
{
   big->sign_and_used_digits = 0 ;
   big->exponent             = 0 ;
}

int copy_bigint(bigint_t *restrict* big, const bigint_t * restrict copyfrom)
{
   int err ;
   bigint_t    * copy = *big ;
   uint16_t    digits = nrdigits_bigint(copyfrom) ;

   if (copy->allocated_digits < digits) {

      err = allocate_bigint(big, digits) ;
      if (err) goto ONABORT ;

      copy = *big ;
   }

   copy->sign_and_used_digits = copyfrom->sign_and_used_digits ;
   copy->exponent             = copyfrom->exponent ;

   if (digits) {
      memcpy(copy->digits, (const void*)copyfrom->digits, digits * sizeof(copy->digits[0])) ;
   }

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

void setfromint32_bigint(bigint_t * big, int32_t value)
{
   if (0 == value) {
      big->sign_and_used_digits = 0 ;
   } else if (value < 0) {
      big->sign_and_used_digits = -1 ;
      big->digits[0] = (uint32_t) (- value) ;
   } else {
      big->sign_and_used_digits = 1 ;
      big->digits[0] = (uint32_t) value ;
   }

   big->exponent = 0 ;
}

void setfromuint32_bigint(bigint_t * big, uint32_t value)
{
   if (0 == value) {
      big->sign_and_used_digits = 0 ;
   } else {
      big->sign_and_used_digits = 1 ;
      big->digits[0] = value ;
   }

   big->exponent = 0 ;
}

int setfromdouble_bigint(bigint_t * big, double value)
{
   int err ;

   assert(big->allocated_digits > 2 || (0 == big->allocated_digits && big->sign_and_used_digits > 2)) ;

   VALIDATE_INPARAM_TEST(isfinite(value), ONABORT, ) ;

   const int16_t  negativemult = (value < 0) ? -1 : 1 ;
   const double   magnitudeval = fabs(value) ;

   if (magnitudeval < 1) {
      big->sign_and_used_digits = 0 ;
      big->exponent             = 0 ;
   } else {
      static_assert(sizeof(int) >= sizeof(big->sign_and_used_digits), "frexp works without overflow") ;

      int    iscale = 0 ;
      (void) frexp(magnitudeval, &iscale) ;

      if (iscale <= 64) {
         if (iscale <= 32) {
            big->sign_and_used_digits = negativemult ;
            big->exponent             = 0 ;
            big->digits[0] = (uint32_t) magnitudeval ;
         } else {
            uint64_t ivalue = (uint64_t) magnitudeval ;
            big->sign_and_used_digits = (int16_t) (2 * negativemult) ;
            big->exponent             = 0 ;
            big->digits[0] = (uint32_t) ivalue ;
            big->digits[1] = (uint32_t) (ivalue >> 32) ;
         }
      } else {

         iscale -= 64 ;

         if (iscale > (UINT16_MAX*32)) {
            err = EOVERFLOW ;
            goto ONABORT ;
         }

         int     digit_exp   = iscale / 32 ;
         int     digit_shift = iscale % 32 ;
         int16_t  nr_digits  = (int16_t) (2 + (digit_shift != 0)) ;

         // shift magnitudeval iscale bits to the right
         uint64_t ivalue = (uint64_t) ldexp(magnitudeval, -iscale) ;

         big->sign_and_used_digits = (int16_t) (nr_digits * negativemult) ;
         big->exponent             = (uint16_t) digit_exp ;
         if (0 == digit_shift) {
            big->digits[0] = (uint32_t) ivalue ;
            big->digits[1] = (uint32_t) (ivalue >> 32) ;
         } else {
            big->digits[0] = (uint32_t)  (ivalue << digit_shift) ;
            big->digits[1] = (uint32_t) ((ivalue << digit_shift) >> 32) ;
            big->digits[2] = (uint32_t) (((uint32_t)(ivalue >> 32)) >> (32-digit_shift)) ;
         }
      }
   }

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int setbigfirst_bigint(bigint_t *restrict* big, int sign, uint16_t size, const uint32_t numbers[size], uint16_t exponent)
{
   int err ;

   VALIDATE_INPARAM_TEST(sign != 0, ONABORT, ) ;

   uint32_t expo2  = exponent ;
   uint16_t size2  = size ;

   while (size2 && 0 == numbers[size2-1]) {
      ++ expo2 ;
      -- size2 ;
   }

   if (0 == size2) {
      // all values are 0
      setfromuint32_bigint(*big, 0) ;
      return 0 ;
   }

   if (expo2 > UINT16_MAX) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   uint16_t offset = 0 ;

   while (0 == numbers[offset]) {
      ++ offset ;
      -- size2 ;
   }

   if ((*big)->allocated_digits < size2) {
      err = allocate_bigint(big, size2) ;
      if (err) goto ONABORT ;
   }

   bigint_t * big2 = *big ;

   big2->sign_and_used_digits = (int16_t) (sign < 0 ? - size2 : size2) ;
   big2->exponent             = (uint16_t) expo2 ;

   do {
      big2->digits[--size2] = numbers[offset] ;
      ++ offset ;
   } while (size2) ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int setlittlefirst_bigint(bigint_t *restrict* big, int sign, uint16_t size, const uint32_t numbers[size], uint16_t exponent)
{
   int err ;

   VALIDATE_INPARAM_TEST(sign != 0, ONABORT, ) ;

   uint32_t expo2  = exponent ;
   uint16_t size2  = size ;

   while (size2 && 0 == numbers[size2-1]) {
      -- size2 ;
   }

   if (0 == size2) {
      // all values are 0
      setfromuint32_bigint(*big, 0) ;
      return 0 ;
   }

   uint16_t offset = 0 ;

   while (0 == numbers[offset]) {
      ++ expo2 ;
      ++ offset ;
      -- size2 ;
   }

   if (expo2 > UINT16_MAX) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   if ((*big)->allocated_digits < size2) {
      err = allocate_bigint(big, size2) ;
      if (err) goto ONABORT ;
   }

   bigint_t * big2 = *big ;

   big2->sign_and_used_digits =  (int16_t) (sign < 0 ? - size2 : size2) ;
   big2->exponent             =  (uint16_t) expo2 ;

   memcpy(big2->digits, &numbers[offset], size2 * sizeof(big2->digits[0])) ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

// group: unary operations

void clearfirstdigit_bigint(bigint_t * big)
{
   if (big->sign_and_used_digits < 0) {
      while (0 != (++ big->sign_and_used_digits)) {
         if (big->digits[-big->sign_and_used_digits-1]) break ;
      }
   } else if (big->sign_and_used_digits > 0) {
      while (0 != (-- big->sign_and_used_digits)) {
         if (big->digits[big->sign_and_used_digits-1]) break ;
      }
   }

   if (!big->sign_and_used_digits) {
      big->exponent = 0 ;
   }
}

void removetrailingzero_bigint(bigint_t * big)
{
   uint16_t nrdigits = nrdigits_bigint(big) ;
   uint16_t rshift   = 0 ;

   while (  1 <  nrdigits
         && 0 == big->digits[rshift]
         && UINT16_MAX != big->exponent) {
      ++ rshift ;
      -- nrdigits ;
      ++ big->exponent ;
   }

   if (rshift) {
      big->sign_and_used_digits = (int16_t) (big->sign_and_used_digits < 0 ? - (int16_t) nrdigits : (int16_t) nrdigits) ;
      memmove(&big->digits[0], &big->digits[rshift], nrdigits * sizeof(big->digits[0])) ;
   }

}

// group: binary operations

int shiftleft_bigint(bigint_t ** result, uint32_t shift_count)
{
   int err ;
   const int16_t  resultsign  = (*result)->sign_and_used_digits ;
   const uint16_t nrdigits    = nrdigits_bigint(*result) ;
   const uint32_t incr_expont = shift_count / 32 ;
   const uint32_t digit_shift = shift_count % 32 ;

   // Do not change 0 values
   if (0 == nrdigits) return 0 ;

   // calculate new exponent
   uint32_t new_exponent = incr_expont + (*result)->exponent ;

   if (new_exponent > UINT16_MAX) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   // shift the whole number
   if (digit_shift) {
      const uint32_t maxdigit          = (*result)->digits[nrdigits-1] ;
      const uint64_t shifted_maxdigit  = ((uint64_t)maxdigit) << digit_shift ;
      const uint32_t overflow_maxdigit = (uint32_t) (shifted_maxdigit >> 32) ;

      if (overflow_maxdigit) {
         uint32_t size = 1 + (uint32_t)nrdigits ;
         if ((*result)->allocated_digits < size) {
            err = allocate_bigint(result, size) ;
            if (err) goto ONABORT ;
         }

         (*result)->digits[nrdigits]     = overflow_maxdigit ;
         (*result)->sign_and_used_digits = (int16_t) (resultsign < 0 ? - size : size) ;
      }

      uint32_t digit          = (*result)->digits[0] ;
      uint64_t shifted_digit  = ((uint64_t)digit << digit_shift) ;
      (*result)->digits[0]    = (uint32_t) shifted_digit ;

      for (uint16_t i = 1; i < nrdigits; ++i) {
         uint32_t overflow_digit = (uint32_t) (shifted_digit >> 32) ;
         digit         = (*result)->digits[i] ;
         shifted_digit = ((uint64_t)digit << digit_shift) | overflow_digit ;
         (*result)->digits[i] = (uint32_t) shifted_digit ;
      }
   }

   (*result)->exponent = (uint16_t) new_exponent ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int shiftright_bigint(bigint_t ** result, uint32_t shift_count)
{
   int err ;
   const int16_t  resultsign   = (*result)->sign_and_used_digits ;
   const uint16_t nrdigits     = nrdigits_bigint(*result) ;
   const uint16_t exponent     = exponent_bigint(*result) ;
   const uint32_t decr_expont  = shift_count / 32 ;
   const uint32_t digit_shift  = shift_count % 32 ;
   uint32_t       new_nrdigits = nrdigits ;
   uint32_t       new_exponent = exponent ;
   uint32_t       skip_digits ;

   if (decr_expont < new_exponent) {
      skip_digits   = 0 ;
      new_exponent -= decr_expont ;
      // nrdigits > 0 && exponent > 0 && new_exponent > 0
   } else {
      skip_digits   = decr_expont - new_exponent ;
      new_exponent  = 0 ;

      if (new_nrdigits <= skip_digits) {
         (*result)->sign_and_used_digits = 0 ;
         (*result)->exponent             = 0 ;
         return 0 ;
      }

      new_nrdigits -= skip_digits ;
   }

   if (digit_shift) {
      uint32_t * destdigits = &(*result)->digits[0] ;
      uint32_t digit        = (*result)->digits[skip_digits] >> digit_shift ;

      if (new_exponent) {
         // do we need to PRESERVE_RIGHT_BITS (assert(0 == skip_digits))
         uint32_t rightbitsdigit = (*result)->digits[0] << (32 - digit_shift) ;
         if (rightbitsdigit) {
            // PRESERVE_RIGHT_BITS: shift them into empty digit
            -- new_exponent ;
            ++ new_nrdigits ;
            if ((*result)->allocated_digits < new_nrdigits) {
               err = allocate_bigint(result, new_nrdigits) ;
               if (err) goto ONABORT ;
            }

            *(destdigits++) = rightbitsdigit ;
         }
      }

      for (uint16_t i = (uint16_t)(skip_digits+1); i < nrdigits; ++i, ++destdigits) {
         uint32_t leftdigit     = (*result)->digits[i] ;
         uint64_t shifted_digit = ((uint64_t)leftdigit << (32 - digit_shift)) | digit ;
         destdigits[0] = (uint32_t) shifted_digit ;
         digit         = (uint32_t) (shifted_digit >> 32) ;
      }

      destdigits[0]  = digit ;
      new_nrdigits  -= (0 == digit) ;

   } else if (skip_digits) {

      memmove((*result)->digits, &(*result)->digits[skip_digits], new_nrdigits * sizeof((*result)->digits[0])) ;
   }

   (*result)->sign_and_used_digits = (int16_t)  (resultsign < 0 ? - (int32_t)new_nrdigits : (int32_t)new_nrdigits) ;
   (*result)->exponent             = (uint16_t) new_exponent ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

// group: 3 address operations

int add_bigint(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig)
{
   int err ;
   const bool lNegSign = isnegative_bigint(lbig) ;
   const bool rNegSign = isnegative_bigint(rbig) ;

   if (lNegSign == rNegSign) {
      // same sign
      err = add_biginthelper(result, lbig, rbig) ;
   } else {
      // different sign => subtract
      err = sub_biginthelper(result, lbig, rbig) ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int sub_bigint(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig)
{
   int err ;
   const bool lNegSign = isnegative_bigint(lbig) ;
   const bool rNegSign = isnegative_bigint(rbig) ;

   if (lNegSign == rNegSign) {
      // same sign
      err = sub_biginthelper(result, lbig, rbig) ;
   } else {
      // different sign => add
      err = add_biginthelper(result, lbig, rbig) ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int multui32_bigint(bigint_t *restrict* result, const bigint_t * lbig, const uint32_t factor)
{
   int err ;

   const uint16_t lnrdigits = nrdigits_bigint(lbig) ;
   uint32_t       size      = 1/*always expect mult to overflow*/ + (uint32_t)lnrdigits ;

   if (0 == lnrdigits || 0 == factor) {
      // check for multiplication with 0
      setfromuint32_bigint(*result, 0) ;
      return 0 ;
   }

   if ((*result)->allocated_digits < size) {
      err = allocate_bigint(result, size) ;
      if (err) goto ONABORT ;
   }

   bigint_t * big = *result ;

   uint32_t carry = 0 ;
   for (uint16_t i = 0; i < lnrdigits; ++i) {

      uint64_t product = lbig->digits[i] * (uint64_t) factor ;
      product += carry ;

      big->digits[i] = (uint32_t) product ;
      carry          = (uint32_t) (product >> 32) ;
   }

   big->digits[lnrdigits] = carry ;

   size -= (0 == carry) ; // if no carry occurred => make result one digit smaller
   big->sign_and_used_digits = (int16_t) (lbig->sign_and_used_digits < 0 ? -size : size) ;
   big->exponent             = lbig->exponent ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int mult_bigint(bigint_t *restrict* result, const bigint_t * lbig, const bigint_t * rbig)
{
   int err ;

   uint16_t lnrdigits = nrdigits_bigint(lbig) ;
   uint16_t rnrdigits = nrdigits_bigint(rbig) ;
   int16_t  xorsign   = xorsign_biginthelper(lbig->sign_and_used_digits, rbig->sign_and_used_digits) ;
   uint32_t exponent  = (uint32_t)lbig->exponent + rbig->exponent ;
   const uint32_t * ldigits = lbig->digits ;
   const uint32_t * rdigits = rbig->digits ;

   // skip trailing zero in both operands
   while (lnrdigits && 0 == ldigits[0]) {
      -- lnrdigits  ;
      ++ ldigits ;
      ++ exponent ;
   }
   while (rnrdigits && 0 == rdigits[0]) {
      -- rnrdigits ;
      ++ rdigits ;
      ++ exponent ;
   }

   if (!rnrdigits  || !lnrdigits) {
      // one or both numbers are 0
      clear_bigint(*result) ;
      return 0 ;
   }

   uint32_t size = (uint32_t)lnrdigits + rnrdigits ;

   if (  exponent > UINT16_MAX) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   if ((*result)->allocated_digits < size) {
      err = allocate_bigint(result, size) ;
      if (err) goto ONABORT ;
   }

   err = multsplit_biginthelper(result, lnrdigits, ldigits, rnrdigits, rdigits) ;
   if (err) goto ONABORT ;

   (*result)->sign_and_used_digits = (int16_t)  (xorsign < 0 ? - (*result)->sign_and_used_digits : (*result)->sign_and_used_digits) ;
   (*result)->exponent             = (uint16_t) ((*result)->exponent + exponent) ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int divmodui32_bigint(bigint_t *restrict* divresult, bigint_t *restrict* modresult, const bigint_t * lbig, const uint32_t divisor)
{
   int err ;

   uint16_t lnrdigits   = nrdigits_bigint(lbig) ;
   uint16_t exponent    = lbig->exponent ;
   bool     is_divisor_zero = (0 == divisor) ;
   uint32_t dhigh       = 0 ;

   VALIDATE_INPARAM_TEST(false == is_divisor_zero, ONABORT, ) ;

   if (  lnrdigits
      && lbig->digits[lnrdigits-1] < divisor) {
      dhigh = lbig->digits[lnrdigits-1] ;
      -- lnrdigits ;
   }

   uint32_t size  = (uint32_t) lnrdigits + exponent ;

   if (0 == size) {
      err = divisorisbigger_biginthelper(divresult, modresult, lbig) ;
      if (err) goto ONABORT ;
      return 0 ;
   }

   if (divresult) {

      if ((*divresult)->allocated_digits < size) {
         err = allocate_bigint(divresult, size) ;
         if (err) goto ONABORT ;
      }

      bigint_t * big = (*divresult) ;

      for (int32_t i = lnrdigits; --i >= 0; ) {

         const uint32_t d1 = lbig->digits[i] ;
         const uint64_t d2 = ((uint64_t)dhigh << 32) + d1 ;

         const uint32_t quot = (uint32_t) (d2 / divisor) ;
         dhigh = (uint32_t) (d2 % divisor) ;

         big->digits[i + exponent] = quot ;
      }

      for (; dhigh && exponent > 0; ) {
         --exponent ;

         const uint64_t d2 = ((uint64_t)dhigh << 32) ;

         const uint32_t quot = (uint32_t) (d2 / divisor) ;
         dhigh = (uint32_t) (d2 % divisor) ;

         big->digits[exponent] = quot ;
      }

      if (exponent) {
         size -= exponent ;
         memmove(&big->digits[0], &big->digits[exponent], size * sizeof(big->digits[0])) ;
      }

      big->sign_and_used_digits = (int16_t) (lbig->sign_and_used_digits < 0 ? - size : size) ;
      big->exponent             = exponent ;
   } else {

      for (int32_t i = lnrdigits; --i >= 0; ) {

         const uint32_t d1 = lbig->digits[i] ;
         const uint64_t d2 = ((uint64_t)dhigh << 32) + d1 ;

         dhigh = (uint32_t) (d2 % divisor) ;
      }

      for (; dhigh && exponent > 0; ) {
         --exponent ;

         const uint64_t d2 = ((uint64_t)dhigh << 32) ;

         dhigh = (uint32_t) (d2 % divisor) ;
      }
   }

   if (modresult) {
      setfromuint32_bigint(*modresult, dhigh) ;
      if (lbig->sign_and_used_digits < 0) negate_bigint(*modresult) ;
   }

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

int divmod_bigint(bigint_t *restrict* divresult, bigint_t *restrict* modresult, const bigint_t * lbig, const bigint_t * rbig)
{
   int err ;
   const uint16_t lnrdigits   = nrdigits_bigint(lbig) ;
   const uint16_t rnrdigits   = nrdigits_bigint(rbig) ;
   const bool is_divisor_zero = (0 == rnrdigits) ;
   const uint16_t lexponent   = lbig->exponent ;
   const uint16_t rexponent   = rbig->exponent ;
   uint32_t       lsize       = (uint32_t)lnrdigits + lexponent ;
   uint32_t       rsize       = (uint32_t)rnrdigits + rexponent ;
   bigint_t       * diff      = 0 ;

   VALIDATE_INPARAM_TEST(false == is_divisor_zero, ONABORT, ) ;

   if (  lsize < rsize
      || (  lsize == rsize
         && cmpmagnitude_bigint(lbig, rbig) < 0)) {
      err = divisorisbigger_biginthelper(divresult, modresult, lbig) ;
      if (err) goto ONABORT ;
      return 0 ;
   }

   if (1 == rsize) {
      err = divmodui32_bigint(divresult, modresult, lbig, rbig->digits[0]) ;
      if (err) goto ONABORT ;
      if (  rbig->sign_and_used_digits < 0
         && divresult) {
         negate_bigint(*divresult) ;
      }
      return 0 ;
   }

   // (lsize >= rsize && rsize > 0 && rsize != 1)  =>  (lsize >= 2)

   err = allocate_bigint(&diff, lsize) ;
   if (err) goto ONABORT ;
   diff->sign_and_used_digits = (int16_t) lsize ;
   // copy lbig into diff with lbig's exponent digits set to 0
   memset(diff->digits, 0, sizeof(diff->digits[0]) * lexponent) ;
   memcpy(&diff->digits[lexponent], lbig->digits, sizeof(diff->digits[0]) * lnrdigits) ;

   const uint64_t divisor    = ((uint64_t)rbig->digits[rnrdigits-1] << 32) + (rnrdigits >= 2 ? rbig->digits[rnrdigits-2] : 0) ;
   uint16_t       diffoffset = (uint16_t) (lsize - 1) ;
   uint64_t       dividend   = ((uint64_t)diff->digits[diffoffset] << 32) + diff->digits[diffoffset-1] ;
   uint32_t       dvrlshift  = (lsize - rsize) ;
   uint32_t       nextdigit  = estimatedigit2_biginthelper(dividend, divisor) ;

   if (nextdigit) {
      nextdigit = submul_biginthelper(diff, false, nextdigit, dvrlshift, rnrdigits, rbig) ;
   }

   // (lbig > rbig) ==> (nextdigit > 0) || (dvrlshift > 0)

   const uint32_t divsize = (nextdigit != 0) + dvrlshift ;
   const uint16_t modexpo = (uint16_t) (rexponent < lexponent ? rexponent : lexponent) ;
   const uint32_t modsize = rsize - modexpo ;

   if (!nextdigit) {
      // (lbig > rbig) ==> (dvrlshift > 0)
      -- dvrlshift ;
      -- diffoffset ;
      nextdigit = estimatedigit3_biginthelper(dividend, diff->digits[diffoffset-1], divisor) ;
      nextdigit = submul_biginthelper(diff, true/*diff has carry*/, nextdigit, dvrlshift, rnrdigits, rbig) ;
   }

   if (  divresult
      && (*divresult)->allocated_digits < divsize) {
      err = allocate_bigint(divresult, divsize) ;
      if (err) goto ONABORT ;
   }

   if (  modresult
      && (*modresult)->allocated_digits < modsize) {
      err = allocate_bigint(modresult, modsize) ;
      if (err) goto ONABORT ;
   }

   uint8_t  buffer[sizeof(bigint_t) + sizeof((*divresult)->digits[0])] ;
   bigint_t * big  = (divresult != 0) ? *divresult : (bigint_t*)buffer ;

   {
      const int16_t xorsign     = xorsign_biginthelper(lbig->sign_and_used_digits, rbig->sign_and_used_digits) ;
      big->exponent             = 0 ;
      big->sign_and_used_digits = (int16_t) (xorsign < 0 ? - (int16_t) divsize : (int16_t) divsize) ;
      big->digits[divresult ? dvrlshift : 0] = nextdigit ;
   }

   for (; dvrlshift > 0; big->digits[divresult ? dvrlshift : 0] = nextdigit) {
      -- dvrlshift ;

      if (diff->digits[diffoffset]) {

         dividend  = ((uint64_t)diff->digits[diffoffset] << 32) + diff->digits[diffoffset-1] ;
         -- diffoffset ;
         nextdigit = estimatedigit3_biginthelper(dividend, diff->digits[diffoffset-1], divisor) ;
         nextdigit = submul_biginthelper(diff, true/*diff has carry*/, nextdigit, dvrlshift, rnrdigits, rbig) ;
      } else {

         -- diffoffset ;
         while (dvrlshift > 0 && 0 == diff->digits[diffoffset]) {
            big->digits[divresult ? dvrlshift : 0] = 0 ;
            -- dvrlshift ;
            -- diffoffset ;
         }

         if (0 == diff->digits[diffoffset]) {

            nextdigit = 0 ;
         } else {

            dividend  = ((uint64_t)diff->digits[diffoffset] << 32) + diff->digits[diffoffset-1] ;
            nextdigit = estimatedigit2_biginthelper(dividend, divisor) ;

            if (nextdigit) {
               nextdigit = submul_biginthelper(diff, false, nextdigit, dvrlshift, rnrdigits, rbig) ;
            }
         }
      }
   }

   if (modresult) {
      while (diffoffset > modexpo && 0 == diff->digits[diffoffset]) {
         -- diffoffset ;
      }

      if (0 == diff->digits[diffoffset]) {

         setfromuint32_bigint(*modresult, 0) ;
      } else {

         memcpy((*modresult)->digits, &diff->digits[modexpo], sizeof(diff->digits[0]) * (1u + diffoffset - modexpo)) ;
         (*modresult)->exponent             = modexpo ;
         (*modresult)->sign_and_used_digits = (int16_t) (1u + diffoffset - modexpo) ;
         if (lbig->sign_and_used_digits < 0) negate_bigint(*modresult) ;
      }
   }

   err = delete_bigint(&diff) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   delete_bigint(&diff) ;
   LOG_ABORT(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_sign(void)
{
   uint8_t  buffer[sizeof(bigint_t)] = { 0 } ;
   bigint_t * big = (bigint_t*) buffer ;

   // TEST xorsign_biginthelper
   int16_t signtestvalues[] = { INT16_MAX, 1, 0, -1, INT16_MIN } ;
   for (unsigned i1 = 0; i1 < nrelementsof(signtestvalues); ++i1) {
      const int s1 = signtestvalues[i1] < 0 ? -1 : +1 ;
      for (unsigned i2 = 0; i2 < nrelementsof(signtestvalues); ++i2) {
         const int s2 = signtestvalues[i2] < 0 ? -1 : +1 ;
         const int expected = s1 * s2 ;
         int16_t xorsign = xorsign_biginthelper(signtestvalues[i1], signtestvalues[i2]) ;
         xorsign = (xorsign < 0) ? -1 : + 1 ;
         TEST(expected == xorsign) ;
      }
   }

   // TEST 0 == sign_bigint
   TEST(0 == sign_bigint(big)) ;
   TEST(0 == big->sign_and_used_digits) ;
   setnegative_bigint(big) ;
   TEST(0 == sign_bigint(big)) ;
   TEST(0 == big->sign_and_used_digits) ;
   setpositive_bigint(big) ;
   TEST(0 == sign_bigint(big)) ;
   TEST(0 == big->sign_and_used_digits) ;
   negate_bigint(big) ;
   TEST(0 == sign_bigint(big)) ;
   TEST(0 == big->sign_and_used_digits) ;

   // TEST +1,0,-1 == sign_bigint
   static_assert(sizeof(big->sign_and_used_digits) == sizeof(int16_t),  "INT16_MAX is valid") ;
   for (int32_t i = 1; i <= INT16_MAX; ++ i) {
      if ( i > 1000 && i < INT16_MAX-1110) {
         i += 110 ;
      }
      big->sign_and_used_digits = (int16_t) i ;
      TEST(+1 == sign_bigint(big)) ;
      setnegative_bigint(big) ;
      TEST(-1 == sign_bigint(big)) ;
      TEST(-(int16_t)i == big->sign_and_used_digits) ;
      setpositive_bigint(big) ;
      TEST(+1 == sign_bigint(big)) ;
      TEST(+(int16_t)i == big->sign_and_used_digits) ;
      negate_bigint(big) ;
      TEST(-1 == sign_bigint(big)) ;
      TEST(-(int16_t)i == big->sign_and_used_digits) ;
      negate_bigint(big) ;
      TEST(+1 == sign_bigint(big)) ;
      TEST(+(int16_t)i == big->sign_and_used_digits) ;

      big->sign_and_used_digits = (int16_t)  -i ;
      TEST(-1 == sign_bigint(big)) ;
      setpositive_bigint(big) ;
      TEST(+1 == sign_bigint(big)) ;
      TEST(+(int16_t)i == big->sign_and_used_digits) ;
      setnegative_bigint(big) ;
      TEST(-1 == sign_bigint(big)) ;
      TEST(-(int16_t)i == big->sign_and_used_digits) ;
      negate_bigint(big) ;
      TEST(+1 == sign_bigint(big)) ;
      TEST(+(int16_t)i == big->sign_and_used_digits) ;
      negate_bigint(big) ;
      TEST(-1 == sign_bigint(big)) ;
      TEST(-(int16_t)i == big->sign_and_used_digits) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_nrdigits(void)
{
   uint8_t  buffer[sizeof(bigint_t)] = { 0 } ;
   bigint_t * big = (bigint_t*) buffer ;

   // TEST 0 == nrdigits_bigint
   TEST(0 == nrdigits_bigint(big)) ;
   setnegative_bigint(big) ;
   TEST(0 == nrdigits_bigint(big)) ;
   setpositive_bigint(big) ;
   TEST(0 == nrdigits_bigint(big)) ;

   // TEST nrdigitsmax_bigint
   TEST(INT16_MAX == nrdigitsmax_bigint()) ;

   // TEST 0 < nrdigits_bigint
   for (int32_t i = 1; i <= INT16_MAX; ++ i) {
      if (abs(i) > 1000 && abs(i) < INT16_MAX-1110) {
         i += 110 ;
      }
      big->sign_and_used_digits = (int16_t) i ;

      TEST(+1 == sign_bigint(big)) ;
      TEST(i == nrdigits_bigint(big)) ;
      setnegative_bigint(big) ;
      TEST(-1 == sign_bigint(big)) ;
      TEST(i == nrdigits_bigint(big)) ;
      setpositive_bigint(big) ;
      TEST(+1 == sign_bigint(big)) ;
      TEST(i == nrdigits_bigint(big)) ;
      negate_bigint(big) ;
      TEST(-1 == sign_bigint(big)) ;
      TEST(i == nrdigits_bigint(big)) ;
      negate_bigint(big) ;
      TEST(+1 == sign_bigint(big)) ;
      TEST(i == nrdigits_bigint(big)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_compare(void)
{
   uint8_t  buffer1[sizeof(bigint_t) + 10 * sizeof(((bigint_t*)0)->digits[0])] = { 0 } ;
   uint8_t  buffer2[sizeof(bigint_t) + 10 * sizeof(((bigint_t*)0)->digits[0])] = { 0 } ;
   bigint_t * big1 = (bigint_t*) buffer1 ;
   bigint_t * big2 = (bigint_t*) buffer2 ;

   // prepare
   big1->allocated_digits = 10 ;
   big2->allocated_digits = 10 ;

   // TEST cmp_bigint: sign 0, 0
   setfromint32_bigint(big1, 0) ;
   setfromint32_bigint(big2, 0) ;
   TEST(0 == cmp_bigint(big1, big2)) ;
   TEST(0 == cmp_bigint(big2, big1)) ;

   // TEST cmp_bigint: sign 0, +1
   setfromint32_bigint(big2, +1) ;
   TEST(-1 == cmp_bigint(big1, big2)) ;
   TEST(+1 == cmp_bigint(big2, big1)) ;

   // TEST cmp_bigint: sign 0, -1
   setfromint32_bigint(big2, -1) ;
   TEST(+1 == cmp_bigint(big1, big2)) ;
   TEST(-1 == cmp_bigint(big2, big1)) ;

   // TEST cmp_bigint: sign +1, -1
   setfromint32_bigint(big1, +1) ;
   TEST(+1 == cmp_bigint(big1, big2)) ;
   TEST(-1 == cmp_bigint(big2, big1)) ;

   // TEST cmp_bigint: extreme values
   big1->exponent = 0;
   big2->exponent = UINT16_MAX ;
   big1->sign_and_used_digits = 1 ;
   big2->sign_and_used_digits = INT16_MAX ;
   TEST(-1 == cmp_bigint(big1, big2)) ;
   TEST(+1 == cmp_bigint(big2, big1)) ;

   // TEST cmp_bigint: compare equal
   uint32_t testvalues[][2][8] = {
                   { { 3, 3, 1, 2, 3 }, { 0, 6, 0, 0, 0, 1, 2, 3 } }
                  ,{ { 500, 5, 0, 10, 12, 13, 1 }, { 501, 4, 10, 12, 13, 1 } }
            } ;
   for (unsigned i = 0; i < nrelementsof(testvalues); ++i) {
      big1->exponent = (uint16_t) testvalues[i][0][0] ;
      big2->exponent = (uint16_t) testvalues[i][1][0] ;
      big1->sign_and_used_digits = (int16_t) testvalues[i][0][1] ;
      big2->sign_and_used_digits = (int16_t) testvalues[i][1][1] ;
      memcpy(big1->digits, &testvalues[i][0][2], testvalues[i][0][1] * sizeof(uint32_t)) ;
      memcpy(big2->digits, &testvalues[i][1][2], testvalues[i][1][1] * sizeof(uint32_t)) ;
      for (int s = 0; s < 2; ++s) {
         TEST(0 == cmp_bigint(big1, big1)) ;
         TEST(0 == cmp_bigint(big2, big2)) ;
         TEST(0 == cmp_bigint(big1, big2)) ;
         TEST(0 == cmp_bigint(big2, big1)) ;
         setnegative_bigint(big1) ;
         setnegative_bigint(big2) ;
      }
   }

   // TEST cmp_bigint: compare not equal
   #define M UINT32_MAX
   uint32_t testvalues2[][2][8] = {
                   { { 3, 3, 1, 2, 3 }, { 0, 6, 1, 0, 0, 1, 2, 3 } }
                  ,{ { 0, 1, M }, { INT16_MAX, 1, M } }
                  ,{ { 1500, 5, 0, M-3, M-2, M-1, M }, { 1500, 5, 1, M-3, M-2, M-1, M } }
                  ,{ { 1, 1, 0 }, { 1, 1, M } }
                  ,{ { 1, 1, M/2-1 }, { 1, 1, M/2 } }
                  ,{ { 0, 5, 0, M-3, M-2, M-1, M }, { 0, 5, M, M-3, M-2, M-1, M } }
                  ,{ { 0, 2, M, M-1 }, { 0, 2, M-1, M } }
            } ;
   #undef M
   for (unsigned i = 0; i < nrelementsof(testvalues2); ++i) {
      big1->exponent = (uint16_t) testvalues2[i][0][0] ;
      big2->exponent = (uint16_t) testvalues2[i][1][0] ;
      big1->sign_and_used_digits = (int16_t) testvalues2[i][0][1] ;
      big2->sign_and_used_digits = (int16_t) testvalues2[i][1][1] ;
      memcpy(big1->digits, &testvalues2[i][0][2], testvalues2[i][0][1] * sizeof(uint32_t)) ;
      memcpy(big2->digits, &testvalues2[i][1][2], testvalues2[i][1][1] * sizeof(uint32_t)) ;
      TEST(-1 == cmp_bigint(big1, big2)) ;
      TEST(+1 == cmp_bigint(big2, big1)) ;
      setnegative_bigint(big1) ;
      setnegative_bigint(big2) ;
      TEST(+1 == cmp_bigint(big1, big2)) ;
      TEST(-1 == cmp_bigint(big2, big1)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int cmpbig2double(bigint_t * big, int iscale, double value)
{
   TEST(fabs(value) <= UINT64_MAX) ;

   uint64_t ivalue   = (uint64_t) fabs(value) ;
   int32_t  exponent = big->exponent ;

   if (iscale < 0) {
      ivalue = ivalue >> (-iscale) ;
      iscale = 0 ;
   }

   while (iscale > 0 && ! (ivalue & 0x8000000000000000)) {
      ivalue <<= 1 ;
      -- iscale ;
   }

   while (iscale >= 32 && exponent) {
      exponent -= 1 ;
      iscale   -= 32 ;
   }

   TEST(0 == exponent) ;

   // test sign
   if (ivalue) {
      if (value < 0) {
         TEST(-1 == sign_bigint(big)) ;
      } else {
         TEST(+1 == sign_bigint(big)) ;
      }
   } else {
      TEST(0 == sign_bigint(big)) ;
   }

   if (iscale == 0) {
      if (ivalue == 0) {
         TEST(0 == nrdigits_bigint(big)) ;
      } else if (ivalue <= UINT32_MAX) {
         TEST(1 == nrdigits_bigint(big)) ;
         TEST((uint32_t)ivalue == big->digits[0]) ;
      } else {
         TEST(2 == nrdigits_bigint(big)) ;
         TEST((uint32_t)ivalue == big->digits[0]) ;
         TEST((uint32_t)(ivalue >> 32) == big->digits[1]) ;
      }
   } else {
      int offset = (iscale / 32) ;
      int shift  = (iscale % 32) ;
      for (int i = 0; i < offset; ++i) {
         TEST(big->digits[i] == 0) ;
      }
      TEST(2 + offset + (shift!=0) == nrdigits_bigint(big)) ;
      TEST(big->digits[offset]   == (uint32_t) (ivalue << shift)) ;
      TEST(big->digits[offset+1] == (uint32_t) (ivalue >> (32 - shift))) ;
      if (shift) {
         TEST(big->digits[offset+2] == (uint32_t) (ivalue >> (64 - shift))) ;
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   bigint_t    * big  = 0 ;
   bigint_t    * big2 = 0 ;
   uint16_t    nrdigits[4]   = { 0, 1, 4, nrdigitsmax_bigint() } ;
   int32_t     copyvalues[5] = { 0, -1, +1, INT32_MAX, INT32_MIN } ;
   int16_t     copylength[3] = { 2, 128, nrdigitsmax_bigint() } ;

   // TEST bitsperdigit_bigint
   TEST(32 == bitsperdigit_bigint()) ;

   // TEST init, double free
   for (unsigned i = 0; i < nrelementsof(nrdigits); ++i) {
      TEST(0 == new_bigint(&big, nrdigits[i])) ;
      TEST(0 != big) ;
      uint16_t nrdig = (uint16_t) (nrdigits[i] < 4 ? 4 : nrdigits[i]) ;
      TEST(nrdig == big->allocated_digits) ;
      TEST(0 == big->sign_and_used_digits) ;
      TEST(0 == big->exponent) ;
      TEST(0 == delete_bigint(&big)) ;
      TEST(0 == big) ;
      TEST(0 == delete_bigint(&big)) ;
      TEST(0 == big) ;
   }

   // TEST init EOVERFLOW
   TEST(EOVERFLOW == new_bigint(&big, nrdigitsmax_bigint()+1)) ;

   // TEST initcopy: simple integers
   TEST(0 == new_bigint(&big, 32)) ;
   for (unsigned i = 0; i < nrelementsof(copyvalues); ++i) {
      setfromint32_bigint(big, copyvalues[i]) ;
      TEST(0 == newcopy_bigint(&big2, big)) ;
      TEST((double)copyvalues[i] == todouble_bigint(big2)) ;
      TEST(0 == delete_bigint(&big2)) ;
   }
   TEST(0 == delete_bigint(&big)) ;

   // TEST initcopy: integers of different lengths
   TEST(0 == new_bigint(&big, nrdigitsmax_bigint())) ;
   for (unsigned i = 0; i < nrelementsof(copylength); ++i) {
      for (unsigned d = 0; d < (unsigned)copylength[i]; ++d) {
         big->digits[d] = i + d ;
      }
      for (int s = -1; s <= 1; s += 2) {
         for (uint16_t e = 0; e <= 1000; e = (uint16_t) (e + 1000)) {
            big->sign_and_used_digits = (int16_t) (s * copylength[i]) ;
            big->exponent             = e ;
            TEST(0 == newcopy_bigint(&big2, big)) ;
            TEST(big2 != 0 && big2 != big) ;
            TEST(big2->allocated_digits     == (copylength[i] < 4 ? 4 : copylength[i])) ;
            TEST(big2->sign_and_used_digits == s * copylength[i]) ;
            TEST(big2->exponent             == e) ;
            for (unsigned d = 0; d < (unsigned)copylength[i]; ++d) {
               TEST((d+i) == big2->digits[d]) ;
               big2->digits[d] = 0 ;
            }
            TEST(0 == delete_bigint(&big2)) ;
         }
      }
   }
   TEST(0 == delete_bigint(&big)) ;

   return 0 ;
ONABORT:
   delete_bigint(&big) ;
   delete_bigint(&big2) ;
   return EINVAL ;
}

static int test_unaryops(void)
{
   bigint_t * big = 0 ;

   // prepare
   TEST(0 == new_bigint(&big, nrdigitsmax_bigint())) ;

   // TEST clearfirstdigit_bigint: nrdigits == 0
   setfromuint32_bigint(big, 0) ;
   clearfirstdigit_bigint(big) ;
   TEST(0 == nrdigits_bigint(big)) ;
   TEST(0 == exponent_bigint(big)) ;

   // TEST clearfirstdigit_bigint: nrdigits == 1
   setfromuint32_bigint(big, 1) ;
   TEST(0 == shiftleft_bigint(&big, bitsperdigit_bigint())) ;
   TEST(1 == nrdigits_bigint(big)) ;
   TEST(1 == exponent_bigint(big)) ;
   clearfirstdigit_bigint(big) ;
   TEST(0 == nrdigits_bigint(big)) ;
   TEST(0 == exponent_bigint(big)) ;
   setfromint32_bigint(big, -1) ;
   TEST(0 == shiftleft_bigint(&big, bitsperdigit_bigint())) ;
   TEST(1 == nrdigits_bigint(big)) ;
   TEST(1 == exponent_bigint(big)) ;
   clearfirstdigit_bigint(big) ;
   TEST(0 == nrdigits_bigint(big)) ;
   TEST(0 == exponent_bigint(big)) ;

   // TEST clearfirstdigit_bigint: nrdigits == nrdigitsmax_bigint
   memset(big->digits, 0, sizeof(big->digits[0]) * nrdigitsmax_bigint()) ;
   for (int16_t i = nrdigitsmax_bigint(); i > 10; i = (int16_t) (i-1000)) {
      big->digits[i-1] = 1 ;
      big->digits[i-2] = 2 ;
      for (int s = -1; s <= 1; s += 2) {
         big->exponent             = (uint16_t) i ;
         big->sign_and_used_digits = (int16_t) (s*i) ;
         TEST(1   == firstdigit_bigint(big)) ;
         clearfirstdigit_bigint(big) ;
         TEST(i-1 == nrdigits_bigint(big)) ;
         TEST(i   == exponent_bigint(big)) ;
         TEST(2   == firstdigit_bigint(big)) ;
         // skips remaining digits cause all are 0
         clearfirstdigit_bigint(big) ;
         TEST(0   == nrdigits_bigint(big)) ;
         TEST(0   == exponent_bigint(big)) ;
         TEST(0   == firstdigit_bigint(big)) ;
      }
   }

   // TEST removetrailingzero_bigint 1 digit
   const uint32_t values1digit[] = { 0, 1, UINT16_MAX-1, UINT16_MAX } ;
   for (unsigned int i = 0 ; i < nrelementsof(values1digit); ++i) {
      setfromuint32_bigint(big, values1digit[i]) ;
      removetrailingzero_bigint(big) ;
      TEST(firstdigit_bigint(big) == values1digit[i]) ;
      TEST(sign_bigint(big) == (values1digit[i] != 0)) ;
      negate_bigint(big) ;
      removetrailingzero_bigint(big) ;
      TEST(firstdigit_bigint(big) == values1digit[i]) ;
      TEST(sign_bigint(big) == -(values1digit[i] != 0)) ;
   }

   // TEST removetrailingzero_bigint x digits
   const uint32_t values10digits[][10] = {
          { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }
         ,{ UINT16_MAX, 0, 0, 2, 1, 0, 0, 4, 6, UINT16_MAX }
         ,{ 1, 0, 0, 0, UINT16_MAX, UINT16_MAX, 0, UINT16_MAX, 1, 2 }
         ,{ UINT16_MAX, 0, UINT16_MAX, 1, 100, 200, 1000, 0xff000000, 0xffff0000, 0xff00 }
      } ;
   for (unsigned int i = 0 ; i < nrelementsof(values10digits); ++i) {
      for (int s =-1; s <= 1; s += 2) {
         // normal shift
         const unsigned offset   = 100 * (1+i) + i ;
         const uint16_t nrdigits = (uint16_t) (10 + offset) ;
         big->sign_and_used_digits = (int16_t) (s * (int16_t)nrdigits) ;
         big->exponent             = (uint16_t) i ;
         memset(big->digits, 0, sizeof(big->digits[0]) * nrdigits) ;
         memmove(&big->digits[offset], &values10digits[i][0], 10 * sizeof(values10digits[0][0])) ;
         removetrailingzero_bigint(big) ;
         TEST(offset + i == big->exponent) ;
         TEST(10         == s * big->sign_and_used_digits) ;
         TEST(0          == memcmp(&big->digits[0], &values10digits[i][0], 10 * sizeof(values10digits[0][0]))) ;
         // exponent overflows
         big->sign_and_used_digits = (int16_t) (s * (int16_t)nrdigits) ;
         big->exponent             = (uint16_t) (UINT16_MAX - i) ;
         memset(big->digits, 0, sizeof(big->digits[0]) * nrdigits) ;
         memmove(&big->digits[offset], &values10digits[i][0], 10 * sizeof(values10digits[0][0])) ;
         removetrailingzero_bigint(big) ;
         TEST(UINT16_MAX       == big->exponent) ;
         TEST((int)(nrdigits-i)== s *big->sign_and_used_digits) ;
         TEST(0                == memcmp(&big->digits[offset-i], &values10digits[i][0], 10 * sizeof(values10digits[0][0]))) ;
      }
   }

   // unprepare
   TEST(0 == delete_bigint(&big)) ;

   return 0 ;
ONABORT:
   delete_bigint(&big) ;
   return EINVAL ;
}

static int test_assign(void)
{
   bigint_t       * big     = 0 ;
   bigint_t       * big2    = 0 ;
   fpu_except_e   oldexcept = getenabled_fpuexcept() ;
   uint16_t       copylength[4] = { 2, 10, 20, 60 } ;
   double         value ;

   // prepare: turn exceptions off
   TEST(0 == new_bigint(&big, 3200)) ;
   TEST(0 == disable_fpuexcept(fpu_except_OVERFLOW)) ;

   // TEST clear_bigint
   for (unsigned di = 0; di < 10; ++di) {
      big->digits[di] = 1 + di ;
   }
   for (int i = 1; i < 10; ++i) {
      for (int s = -1; s <= +2; s += 2) {
         big->sign_and_used_digits = (int8_t)   (s * i) ;
         big->exponent             = (uint16_t) i ;
         TEST(size_bigint(big)     == 2 * (unsigned)i) ;
         TEST(exponent_bigint(big) == i) ;
         TEST(sign_bigint(big)     == s) ;
         clear_bigint(big) ;
         TEST(sign_bigint(big)     == 0) ;
         TEST(nrdigits_bigint(big) == 0) ;
         TEST(exponent_bigint(big) == 0) ;
         // preallocated size & content of digits not changed
         TEST(big->allocated_digits == 3200) ;
         for (unsigned di = 0; di < 10; ++di) {
            TEST(big->digits[di] == 1 + di) ;
         }
      }
   }

   // TEST copy_bigint: with and without allocating memory
   for (unsigned i = 0; i < nrelementsof(copylength); ++i) {
      for (unsigned d = 0; d < copylength[i]; ++d) {
         big->digits[d] = 1 + (i+1) * d ;
      }
      for (int s = -1; s <= 1; s += 2) {
         for (uint16_t e = 0; e <= 10000; e = (uint16_t) (e + 5000)) {
            for (int digits = 1; digits <= copylength[i]; digits += copylength[i]-1) {
               big->sign_and_used_digits = (int16_t) (s * (int16_t)copylength[i]) ;
               big->exponent             = e ;
               TEST(0 == new_bigint(&big2, (uint16_t) digits)) ;
               bigint_t * temp = big2 ;
               TEST(0 == copy_bigint(&big2, big)) ;
               TEST(1 == digits || temp == big2) ;
               TEST(big2->allocated_digits     == (copylength[i] < 4 ? 4 : copylength[i])) ;
               TEST(big2->sign_and_used_digits == s * copylength[i]) ;
               TEST(big2->exponent             == e) ;
               for (unsigned d = 0; d < copylength[i]; ++d) {
                  TEST((1 + (i+1) * d) == big2->digits[d]) ;
                  big2->digits[d] = 0 ;
               }
               TEST(0 == delete_bigint(&big2)) ;
            }
         }
      }
   }

   // TEST setfromint32_bigint, setfromuint32_bigint
   uint32_t testvaluesint[7] = { 0, 1, 0xffffffff, 0x7fffffff, 0x80000000, 0x0f0f0f0f, 0xf0f0f0f0 } ;
   for (unsigned i = 0; i < nrelementsof(testvaluesint); ++i) {
      big->sign_and_used_digits = 0 ;
      big->exponent  = 1 ;
      big->digits[0] = ~ testvaluesint[i] ;
      setfromint32_bigint(big, (int32_t)testvaluesint[i]) ;
      TEST(sign_int((int32_t)testvaluesint[i]) == sign_bigint(big)) ;
      TEST((testvaluesint[i] ? 1 : 0) == nrdigits_bigint(big)) ;
      TEST(0 == big->exponent) ;
      if ((int32_t)testvaluesint[i] < 0) {
         TEST((uint32_t)(-(int32_t)testvaluesint[i]) == big->digits[0]) ;
      } else if (testvaluesint[i]) {
         TEST(testvaluesint[i] == big->digits[0]) ;
      }

      big->sign_and_used_digits = 0 ;
      big->exponent  = 1 ;
      big->digits[0] = ~ testvaluesint[i] ;
      setfromuint32_bigint(big, testvaluesint[i]) ;
      TEST((testvaluesint[i] ? 1 : 0) == sign_bigint(big)) ;
      TEST((testvaluesint[i] ? 1 : 0) == nrdigits_bigint(big)) ;
      TEST(0 == big->exponent) ;
      if (testvaluesint[i]) {
         TEST(testvaluesint[i] == big->digits[0]) ;
      }
   }

   // TEST setfromdouble_bigint: absolute value < 1 (big is set to 0)
   double normalvalues[6] = { 0, -0, 0.9, -0.1, 0x1p-1022, -0x1p-1022 } ;
   for (unsigned i = 0; i < nrelementsof(normalvalues); ++i) {
      TEST(0 == normalvalues[i] || FP_NORMAL == fpclassify(normalvalues[i])) ;
      big->sign_and_used_digits = -1 ;
      big->exponent = 1 ;
      TEST(0 == setfromdouble_bigint(big, normalvalues[i])) ;
      // big value == 0
      TEST(0 == sign_bigint(big)) ;
      TEST(0 == nrdigits_bigint(big)) ;
      TEST(0 == big->exponent) ;
      TEST(0.0 == todouble_bigint(big)) ;
   }

   // TEST setfromdouble_bigint: subnormal (big is set to 0)
   double subnormalvalues[2] = { 0x0.8p-1022, -0x0.8p-1022 } ;
   for (unsigned i = 0; i < nrelementsof(subnormalvalues); ++i) {
      TEST(FP_SUBNORMAL == fpclassify(subnormalvalues[i])) ;
      big->sign_and_used_digits = -1 ;
      big->exponent = 1 ;
      TEST(0 == setfromdouble_bigint(big, subnormalvalues[i])) ;
      // big value == 0
      TEST(0 == sign_bigint(big)) ;
      TEST(0 == nrdigits_bigint(big)) ;
      TEST(0 == big->exponent) ;
      TEST(0.0 == todouble_bigint(big)) ;
   }

   // TEST setfromdouble_bigint: pow(2,0) ... pow(2,63) (fractional part is discarded)
   for (uint64_t iscale = 0, ivalue = 1.0; iscale <= 63; ++iscale, ivalue *= 2) {
      // ivalue2 is: 1. pow(2,0) bit set other 0, 2. all bits set up to pow(2,0)
      for (double fraction = 0; fraction <= 0.5; fraction += 0.25) {
         for (int s = -1; s <= 1; s += 2) {
            big->sign_and_used_digits = -1 ;
            big->exponent = 1 ;
            big->digits[0] = big->digits[1] = big->digits[2] = UINT32_MAX ;
            value = (double)s * (fraction + (double)ivalue) ;
            TEST(0 == setfromdouble_bigint(big, value)) ;
            TEST(2 == nrdigits_bigint(big) + (fabs(value) <= UINT32_MAX)) ;
            TEST(0 == big->exponent) ;
            TEST(0 == cmpbig2double(big, 0, value)) ;
            TEST(s * (double)ivalue == todouble_bigint(big)) ;
         }
      }
   }

   // TEST setfromdouble_bigint: integer part (fractional part is discarded)
   double dvalues[5] = { UINT16_MAX, UINT32_MAX, 0x001FFFFFFFFFFFFFull, 0x123456789ABCDE00ull, 0xFEDCBA9876543800ull } ;
   for (unsigned i = 0; i < nrelementsof(dvalues); ++i) {
      double downscalef = 1.0 ;
      double upscalef   = 1.0 ;
      for (int iscale = 0; iscale <= 63; ++iscale) {
         for (int s = -1; s <= 1; s += 2) {
            // FOR DEBUG: printf("i=%d,iscale=%d,s=%d\n", i, iscale, s) ;
            big->sign_and_used_digits = -1 ;
            big->exponent = INT16_MAX ;
            value = s * upscalef * dvalues[i] ;
            TEST(0 == setfromdouble_bigint(big, value)) ;
            if (fabs(value) <= UINT32_MAX) {
               TEST(1 == nrdigits_bigint(big)) ;
            } else {
               TEST(2 <= nrdigits_bigint(big) && nrdigits_bigint(big) <= 3) ;
            }
            TEST(0 == cmpbig2double(big, iscale, s * dvalues[i])) ;
            TEST(value == todouble_bigint(big)) ;
            big->sign_and_used_digits = -1 ;
            big->exponent = 1 ;
            value = s * downscalef * dvalues[i] ;
            TEST(0 == setfromdouble_bigint(big, value)) ;
            modf(value, &value) ; // discard fractional part for comparison
            TEST(2 == nrdigits_bigint(big) + (fabs(value) <= UINT32_MAX) + (fabs(value) < 1)) ;
            TEST(0 == big->exponent) ;
            TEST(0 == cmpbig2double(big, -iscale, s * dvalues[i])) ;
            TEST(value == todouble_bigint(big)) ;
         }
         downscalef /= 2 ;
         upscalef   *= 2 ;
      }
   }

   // TEST setfromdouble_bigint: DBL_MAX
   {
      value = DBL_MAX ;
      int iscale = 0 ;
      while (value > (double)UINT64_MAX) {
         value /= 2 ;
         ++ iscale ;
      }
      for (double dblmax = DBL_MAX; iscale > -63; dblmax /= 2, --iscale) {
         modf(dblmax, &dblmax) ; // discard fractional part due to rounding effects
         TEST(0 == setfromdouble_bigint(big, dblmax)) ;
         TEST(0 == cmpbig2double(big, iscale, value)) ;
         TEST(dblmax == todouble_bigint(big)) ;
         TEST(0 == setfromdouble_bigint(big, -dblmax)) ;
         TEST(0 == cmpbig2double(big, iscale, -value)) ;
         TEST(-dblmax == todouble_bigint(big)) ;
      }
   }

   // TEST new_bigint allocates enough memory so that setfromdouble_bigint always works
   TEST(0 == delete_bigint(&big)) ;
   TEST(0 == new_bigint(&big, 1)) ;
   TEST(0 == setfromdouble_bigint(big, 16 * (double)UINT64_MAX)) ;
   TEST(3 == nrdigits_bigint(big)) ;

   // TEST todouble_bigint: INFINITY signals exception
   TEST(0 == setfromdouble_bigint(big, DBL_MAX)) ;
   ++ big->exponent ;
   TEST(0 == clear_fpuexcept(fpu_except_OVERFLOW)) ;
   TEST(INFINITY == todouble_bigint(big)) ;
   TEST(fpu_except_OVERFLOW == getsignaled_fpuexcept(fpu_except_OVERFLOW)) ;
   TEST(0 == clear_fpuexcept(fpu_except_OVERFLOW)) ;
   negate_bigint(big) ;
   TEST(-INFINITY == todouble_bigint(big)) ;
   TEST(fpu_except_OVERFLOW == getsignaled_fpuexcept(fpu_except_OVERFLOW)) ;

   // TEST todouble_bigint: (54th bit == 1) => rounding up or down (uneven) according to round mode
   int oldroundmode = fegetround() ;
   double val   = (double)0xfffffffffffff800ul ;
   double upval = val + 0x800 ;
   TEST(0 == setfromdouble_bigint(big, val)) ;
   TEST(big->digits[0] == 0xfffff800) ;
   big->digits[0] = 0xfffff800 + 0x400 ;
   fesetround(FE_TONEAREST) ;
   TEST(upval == todouble_bigint(big)) ;
   fesetround(FE_UPWARD) ;
   TEST(upval == todouble_bigint(big)) ;
   fesetround(FE_DOWNWARD) ;
   TEST((val) == todouble_bigint(big)) ;
   fesetround(FE_TOWARDZERO) ;
   TEST((val) == todouble_bigint(big)) ;
   negate_bigint(big) ;
   TEST(-val == todouble_bigint(big)) ;
   fesetround(FE_DOWNWARD) ;
   TEST(-upval == todouble_bigint(big)) ;
   fesetround(FE_UPWARD) ;
   TEST(-val == todouble_bigint(big)) ;
   fesetround(FE_TONEAREST) ;
   TEST(-upval == todouble_bigint(big)) ;
   fesetround(oldroundmode) ;

   // TEST todouble_bigint: (54th bit == 1) => rounding up or down (even) according to round mode
   oldroundmode = fegetround() ;
   val   = (double)0xfffffffffffff000ul ;
   upval = val + 0x800 ;
   TEST(0 == setfromdouble_bigint(big, val)) ;
   TEST(big->digits[0] == 0xfffff000) ;
   big->digits[0] = 0xfffff000 + 0x400 ;
   fesetround(FE_TONEAREST) ;
   TEST(val == todouble_bigint(big)) ;
   fesetround(FE_UPWARD) ;
   TEST(upval == todouble_bigint(big)) ;
   fesetround(FE_DOWNWARD) ;
   TEST((val) == todouble_bigint(big)) ;
   fesetround(FE_TOWARDZERO) ;
   TEST((val) == todouble_bigint(big)) ;
   negate_bigint(big) ;
   TEST(-val == todouble_bigint(big)) ;
   fesetround(FE_DOWNWARD) ;
   TEST(-upval == todouble_bigint(big)) ;
   fesetround(FE_UPWARD) ;
   TEST(-val == todouble_bigint(big)) ;
   fesetround(FE_TONEAREST) ;
   TEST(-val == todouble_bigint(big)) ;
   fesetround(oldroundmode) ;

   // TEST EINVAL
   TEST(EINVAL == setfromdouble_bigint(big, INFINITY)) ;
   TEST(EINVAL == setfromdouble_bigint(big, NAN)) ;

   // TEST setbigfirst_bigint
   TEST(0 == delete_bigint(&big)) ;
   TEST(0 == new_bigint(&big, 4)) ;
   TEST(0 == setbigfirst_bigint(&big, +1, 0, (uint32_t[2]){1, 2}, 10)) ;
   TEST(0 == big->sign_and_used_digits) ;
   TEST(0 == big->exponent) ;
   TEST(0 == setbigfirst_bigint(&big, +1, 9, (uint32_t[9]){1, 2, 3, 4, 5, 6, 0, 0, 0}, UINT16_MAX-3)) ;
   TEST(6 == big->allocated_digits) ;
   TEST(6 == big->sign_and_used_digits) ;
   TEST(UINT16_MAX == big->exponent) ;
   for (unsigned i = 0; i < 6; ++i) {
      TEST(6-i == big->digits[i]) ;
   }
   TEST(0 == setbigfirst_bigint(&big, -1, 9, (uint32_t[9]){0, 0, 0, 6, 5, 4, 3, 2, 1}, 0)) ;
   TEST(6 == big->allocated_digits) ;
   TEST(-6 == big->sign_and_used_digits) ;
   TEST(0 == big->exponent) ;
   for (unsigned i = 0; i < 6; ++i) {
      TEST(i+1 == big->digits[i]) ;
   }
   TEST(0 == setbigfirst_bigint(&big, +1, 12, (uint32_t[12]){0, 0, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0}, 1)) ;
   TEST(7 == big->allocated_digits) ;
   TEST(7 == big->sign_and_used_digits) ;
   TEST(4 == big->exponent) ;
   for (unsigned i = 0; i < 7; ++i) {
      TEST(i+1 == big->digits[i]) ;
   }
   TEST(0 == setbigfirst_bigint(&big, -1, 11, (uint32_t[11]){0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}, 11)) ;
   TEST(7 == big->allocated_digits) ;
   TEST(-1 == big->sign_and_used_digits) ;
   TEST(15 == big->exponent) ;
   TEST(1 == big->digits[0]) ;
   TEST(0 == setbigfirst_bigint(&big, -1, 11, (uint32_t[11]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 11)) ;
   TEST(7 == big->allocated_digits) ;
   TEST(0 == big->sign_and_used_digits) ;
   TEST(0 == big->exponent) ;

   // TEST setbigfirst_bigint EINVAL
   TEST(EINVAL == setbigfirst_bigint(&big, 0, 2, (uint32_t[2]){1, 1}, 0)) ;

   // TEST setbigfirst_bigint EOVERFLOW
   TEST(EOVERFLOW == setbigfirst_bigint(&big, +1, 3, (uint32_t[3]){1, 0, 0}, 0xFFFE)) ;

   // TEST setlittlefirst_bigint
   TEST(0 == delete_bigint(&big)) ;
   TEST(0 == new_bigint(&big, 4)) ;
   TEST(0 == setlittlefirst_bigint(&big, +1, 0, (uint32_t[2]){1, 2}, 10)) ;
   TEST(0 == big->sign_and_used_digits) ;
   TEST(0 == big->exponent) ;
   TEST(0 == setlittlefirst_bigint(&big, +1, 9, (uint32_t[9]){1, 2, 3, 4, 5, 6, 0, 0, 0}, UINT16_MAX)) ;
   TEST(6 == big->allocated_digits) ;
   TEST(6 == big->sign_and_used_digits) ;
   TEST(UINT16_MAX == big->exponent) ;
   for (unsigned i = 0; i < 6; ++i) {
      TEST(i+1 == big->digits[i]) ;
   }
   TEST(0 == setlittlefirst_bigint(&big, -1, 9, (uint32_t[9]){0, 0, 0, 6, 5, 4, 3, 2, 1}, 0)) ;
   TEST(6 == big->allocated_digits) ;
   TEST(-6 == big->sign_and_used_digits) ;
   TEST(3 == big->exponent) ;
   for (unsigned i = 0; i < 6; ++i) {
      TEST(6-i == big->digits[i]) ;
   }
   TEST(0 == setlittlefirst_bigint(&big, +1, 12, (uint32_t[12]){0, 0, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0}, 1)) ;
   TEST(7 == big->allocated_digits) ;
   TEST(7 == big->sign_and_used_digits) ;
   TEST(3 == big->exponent) ;
   for (unsigned i = 0; i < 7; ++i) {
      TEST(7-i == big->digits[i]) ;
   }
   TEST(0 == setlittlefirst_bigint(&big, -1, 11, (uint32_t[11]){0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}, 3)) ;
   TEST(7 == big->allocated_digits) ;
   TEST(-1 == big->sign_and_used_digits) ;
   TEST(9 == big->exponent) ;
   TEST(1 == big->digits[0]) ;
   TEST(0 == setlittlefirst_bigint(&big, -1, 11, (uint32_t[11]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 11)) ;
   TEST(7 == big->allocated_digits) ;
   TEST(0 == big->sign_and_used_digits) ;
   TEST(0 == big->exponent) ;

   // TEST setlittlefirst_bigint EINVAL
   TEST(EINVAL == setlittlefirst_bigint(&big, 0, 2, (uint32_t[2]){1, 1}, 0)) ;

   // TEST setlittlefirst_bigint EOVERFLOW
   TEST(EOVERFLOW == setlittlefirst_bigint(&big, +1, 3, (uint32_t[3]){0, 0, 1}, 0xFFFE)) ;

   // unprepare: turn exceptions on
   TEST(0 == delete_bigint(&big)) ;
   TEST(0 == delete_bigint(&big2)) ;
   TEST(0 == clear_fpuexcept(fpu_except_OVERFLOW)) ;
   if (oldexcept & fpu_except_OVERFLOW) {
      TEST(0 == enable_fpuexcept(fpu_except_OVERFLOW)) ;
   }

   return 0 ;
ONABORT:
   delete_bigint(&big) ;
   delete_bigint(&big2) ;
   return EINVAL ;
}

static int test_addsub(void)
{
   bigint_t    * big[4] = { 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == new_bigint(&big[i], 1024)) ;
   }

   // TEST add, sub with operands of same sign
#define M UINT32_MAX
   uint32_t testrows[][3][10] = {
                /* { difference }, { big number }, { small number } */
                { { 0 }, { 0 }, { 0 } }
               ,{ { 123, M }, { 123, M }, { 0 } }
               ,{ { 0, M, M, 0, 0 }, { 1, 0, M-1, M, 0}, { 0, 0, M, M, 0 } }
               ,{ { 5, 4, 3, 2, 1 }, { 6, 1, 5, 3, 1}, { 0, M-2, 2, 1, 0 } }
               ,{ { M, 0, 0, 0, 0 }, { M, 0, 0, 0, M}, { 0, 0, 0, 0, M } }
               ,{ { M, M-2, 0, 0, 0 }, { M, M-2, 0, M-1, M}, { 0, 0, 0, M-1, M } }
               ,{ { M/2+1, M-2, 0, 0, 0 }, { M, M-2, 0, M-1, M}, { M/2, 0, 0, M-1, M } }
               ,{ { 0 }, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } }
               ,{ { 0 }, { 1, 2, 3, 0, 0, 0, 0, 0, 0, 129/*special code*/ }, { 1, 2, 3 } }
               ,{ { 0, 0, 3, 4, 5, 6 }, { 1, 2, 3, 4, 5, 6 }, { 1, 2 } }
               ,{ { 1, 2, 0, 0, 0, 6 }, { 1, 2, 3, 4, 5, 6 }, { 0, 0, 3, 4, 5 } }
               ,{ { 1, 1, M, 0, 0, 6 }, { 1, 2, 3, 4, 5, 6 }, { 0, 0, 4, 4, 5 } }
               ,{ { 1, M-1, M-2, M-3, M-3 }, { 2 }, { 0, 1, 2, 3, 4 } }
               ,{ { 1, M, M, 0, 1, 2, 4 }, { 2 }, { 0, 0, 0, M, M-1, M-2, M-3 } }
               ,{ { 0,0,0,0,0,0,0,0,0,1 }, { 1,0,0,0,0,0,0,0,0,0 }, { 0,M,M,M,M,M,M,M,M,M } }
               ,{ { 0,0,0,0,0,0,0,0,0,125 }, { 1,0,0,0,0,0,0,0,0,0 }, { 0,M,M,M,M,M,M,M,M,M-124 } }
               ,{ { 0,0,0,0,0,0,0,0,0,1 }, { 1, 2, 3, 0, 0, 0, 0, 0, 0, 129/*special code*/ }, { 1, 2, 2, M,M,M,M,M,M,M } }
               ,{ { 1,2,2,M,M,M,M,M,M,0 }, { 1, 2, 3, 0, 0, 0, 0, 0, 999, M }, { 0,0,0,0,0,0,0,0,1000,M } }
               ,{ { 0,M,M,M,M,M,M,M,M,0 }, { 1, 0, 0, 0, 0, 0, 0, 0, 999, M }, { 0,0,0,0,0,0,0,0,1000,M } }
               ,{ { 6,7,9,0,0,0,M-1,M,M-1,1 }, { 6,7,9, 0, 0, 0, M, M, 0, 129/*special code*/ }, { 0,0,0,0,0,0,0,M,1,M } }
            } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows); ++i) {
      for (unsigned nr = 0; nr < 3; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows[i][nr], 0)) ;
      }
      const bool isSpecial = (nrdigits_bigint(big[1]) && 129 == big[1]->digits[0]) ;
      if (isSpecial) big[1]->digits[0] = 0 ; // special code produces trailing zeros with exponent == 0
      for (int s = -1; s <= +1; s += 2) {
         TEST(0 == delete_bigint(&big[3])) ;
         TEST(0 == new_bigint(&big[3], 1)) ;
         TEST(0 == add_bigint(&big[3], big[0], big[2])) ;   // allocate
         TEST(0 == cmp_bigint(big[3], big[1])) ;
         negate_bigint(big[2]) ;
         TEST(0 == delete_bigint(&big[3])) ;
         TEST(0 == new_bigint(&big[3], 1)) ;
         TEST(0 == sub_bigint(&big[3], big[0], big[2])) ;   // allocate
         TEST(0 == cmp_bigint(big[3], big[1])) ;
         memset(big[3]->digits, 0, sizeof(big[3]->digits[0] * nrdigits_bigint(big[3]))) ;
         negate_bigint(big[0]) ;
         negate_bigint(big[1]) ;
      }
   }

   // TEST add, sub with operands of different sign
   for (unsigned i = 0; i < nrelementsof(testrows); ++i) {
      for (unsigned nr = 0; nr < 3; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows[i][nr], 0)) ;
      }
      const bool isSpecial = (nrdigits_bigint(big[1]) && 129 == big[1]->digits[0]) ;
      if (isSpecial) big[1]->digits[0] = 0 ; // special code produces trailing zeros with exponent == 0
      for (int s = -1; s <= +1; s += 2) {
         TEST(0 == delete_bigint(&big[3])) ;
         TEST(0 == new_bigint(&big[3], 1)) ;
         TEST(0 == sub_bigint(&big[3], big[1], big[2])) ;   // allocate
         TEST(0 == cmp_bigint(big[3], big[0])) ;
         TEST(0 == sub_bigint(&big[3], big[1], big[0])) ;   // allocate
         TEST(0 == cmp_bigint(big[3], big[2])) ;
         negate_bigint(big[0]) ;
         TEST(0 == sub_bigint(&big[3], big[2], big[1])) ;   // do not allocate
         TEST(0 == cmp_bigint(big[3], big[0])) ;
         negate_bigint(big[0]) ;
         negate_bigint(big[2]) ;
         TEST(0 == sub_bigint(&big[3], big[0], big[1])) ;   // do not allocate
         TEST(0 == cmp_bigint(big[3], big[2])) ;
         TEST(0 == delete_bigint(&big[3])) ;
         TEST(0 == new_bigint(&big[3], 1)) ;
         TEST(0 == add_bigint(&big[3], big[2], big[1])) ;   // allocate
         TEST(0 == cmp_bigint(big[3], big[0])) ;
         negate_bigint(big[1]) ;
         TEST(0 == add_bigint(&big[3], big[1], big[0])) ;   // do not allocate
         TEST(0 == cmp_bigint(big[3], big[2])) ;
         memset(big[3]->digits, 0, sizeof(big[3]->digits[0] * nrdigits_bigint(big[3]))) ;
         negate_bigint(big[0]) ;
      }
   }

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

static int test_mult(void)
{
   bigint_t    * big[4] = { 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == new_bigint(&big[i], nrdigitsmax_bigint())) ;
   }

   // TEST multui32_bigint, mult_bigint (one digit)
#define M UINT32_MAX
   uint32_t testrows[][3][10] = {
                /* { product }, { number }, { factor } */
                { { 0 }, { 0 }, { 0 } }
               ,{ { 0 }, { 10 }, { 0 } }
               ,{ { 0 }, { 0 }, { 100 } }
               ,{ { 100 }, { 1 }, { 0,0,0,0,0,0,0,0,0, 100 } }
               ,{ { 100 }, { 100 }, { 0,0,0,0,0,0,0,0,0, 1 } }
               ,{ { 5, 10, 15, 20, 25 }, { 1, 2, 3, 4, 5 }, { 0,0,0,0,0,0,0,0,0, 5 } }
               ,{ { M-1, 1 }, { 0, M }, { 0,0,0,0,0,0,0,0,0, M } }
               ,{ { M-1, M, M, M, M, 1 }, { 0, M, M, M, M, M }, { 0,0,0,0,0,0,0,0,0, M } }
               ,{ { 1, 3, M-99 }, { 0, 42949672/*M/100*/, M }, { 0,0,0,0,0,0,0,0,0, 100 } }
            } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows); ++i) {
      for (unsigned nr = 0; nr < 3; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows[i][nr], 0)) ;
      }
      uint32_t factor = sign_bigint(big[2]) ? big[2]->digits[0] : 0 ;
      TEST(0 == delete_bigint(&big[3])) ;
      TEST(0 == new_bigint(&big[3], 1)) ;
      TEST(0 == multui32_bigint(&big[3], big[1], factor)) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setnegative_bigint(big[0]) ;
      setnegative_bigint(big[1]) ;
      TEST(0 == multui32_bigint(&big[3], big[1], factor)) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
   }

   // TEST mult_bigint: without splitting
#define M UINT32_MAX
   uint32_t testrows2[][3][10] = {
                /* { product }, { number }, { factor } */
                { { 5, 10, 5+15, 10+20, 15+25, 20, 25 }, { 0,0, 1, 2, 3, 4, 5 }, { 0,0,0,0,0,0,0, 5, 0, 5 } }
               ,{ { M-1, M, M, 1 }, { 0,0,0, M }, { 0,0,0,0,0,0,0, M, M, M } }
               ,{ { M, M-1, M, M, M, 0, 1 }, { 0,0, M, M, M, M, M }, { 0,0,0,0,0,0,0,0, M, M } }
               ,{ { 1, 3, M-99+1, 3+1, M-99+3, M-99 }, { 0,0,0,0, 42949672/*M/100*/, M }, { 0,0,0,0,0,0, 100, 0, 100, 100 } }
               ,{ { 1,2,3,4,0,1,2,3,4,0 }, { 0,0,0,0,0,0, 1, 2, 3, 4 }, { 0,0,0,1,0,0,0,0,1,1/*set to 0*/ } }
           } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows2); ++i) {
      for (unsigned nr = 0; nr < 3; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows2[i][nr], 0)) ;
      }
      if (4 == i) big[2]->digits[0] = 0 ; // value ending in 0
      TEST(0 == delete_bigint(&big[3])) ;
      TEST(0 == new_bigint(&big[3], 1)) ;
      TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setnegative_bigint(big[0]) ;
      setnegative_bigint(big[1]) ;
      TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setpositive_bigint(big[0]) ;
      setnegative_bigint(big[2]) ;
      TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
   }

   // TEST mult_bigint: splitting
#define M UINT32_MAX
#define M8 M,M,M,M,M,M,M,M
#define M7 M,M,M,M,M,M,M
#define Z8 0,0,0,0,0,0,0,0
#define MSB 0x80000000
   uint32_t testrows3[][3][100] = {
               /* { multiplicant1 }, { multiplicant2 }, { product } */
               // test splitting with l0,r0 least significant digits == 0, l1,r1 most sign digits == 0
               { {MSB,M,0,0,0,0,0,0,Z8,Z8,Z8,Z8,Z8,1}, {8,1,0,0,0,0,0,0,Z8,Z8,Z8,Z8,Z8,M}, {0,0x7ffffffc,7,1,Z8,Z8,Z8,Z8,Z8,0,0,0,0,MSB+8,MSB+1,M-1,Z8,Z8,Z8,Z8,Z8,0,0,0,0,0,M} }
               // test splitting with (t[2]*t[3])->exponent != 0
               ,{ {M8,M8,M8,1,M8,M8,M8}, {1,M7,M8,M8,M8,M8,M8,1}, {M,Z8,Z8,Z8+2,M-2,Z8,Z8,0,0,0,0,0,0,M-1,0,M8,M8,M,M,M,M,M,M,3,M-1,M8,M8,M7,1} }
               // test splitting with result=t[0]*X*X+t[1] && result->exponent != 0
               ,{ {MSB,M7,M8,M8,Z8,Z8,Z8,1}, {8,0,0,0,0,0,0,0,Z8,Z8,M8,M8,M8,M}, {0,M-3,M7,M8,M8-M/2+7,Z8,Z8,Z8+7,-(M/2)+M8,M8,M8-1,Z8,Z8,Z8,1} }
               // test splitting with t[0]/t[1]/t[2]/t[3]->exponent != 0 => result->exponent != 0 && T4->exponent != 0
               ,{ {MSB,M7,M8,M8,MSB,M7,Z8,Z8,1}, {MSB,M7,Z8,Z8,MSB,M7,M8,M8,M}, {0,MSB/2,0,0,0,0,0,0,MSB,M8,M7,MSB-1,MSB-1,0,0,0,0,0,0,1,M7,Z8,0,0xbfffffff,M7-M/2,M8,M8-MSB,MSB,M7,Z8,Z8,1} }
               // test adding t[4] to t[0]*X*X + t[1] produces carry overflow (see multsplit_biginthelper)
               ,{ {M8,M8,M8,Z8,Z8,Z8,1}, {1,0,0,0,0,0,0,0,Z8,Z8,M8,M8,M8,M}, {M8,M8,M8,1,0,0,0,0,0,0,0,Z8,Z8,0,M8,M8,M7,M-1,Z8,Z8,Z8,1} }
           } ;
#undef MSB
#undef Z8
#undef M7
#undef M8
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows3); ++i) {
      for (unsigned nr = 0; nr < 3; ++nr) {
         TEST(0 == setlittlefirst_bigint(&big[nr], +1, nrelementsof(testrows3[0][0]), testrows3[i][nr], 0)) ;
      }
      TEST(0 == delete_bigint(&big[3])) ;
      TEST(0 == new_bigint(&big[3], 1)) ;
      TEST(0 == mult_bigint(&big[3], big[0], big[1])) ;
      TEST(0 == cmp_bigint(big[2], big[3])) ;
      int e1 = (random() % (UINT16_MAX/2)) ;
      int e2 = (random() % (UINT16_MAX/2)) ;
      big[0]->exponent = (uint16_t) (big[0]->exponent + e1 );
      big[1]->exponent = (uint16_t) (big[1]->exponent + e2 );
      TEST(0 == mult_bigint(&big[3], big[1], big[0])) ;
      big[2]->exponent = (uint16_t) (big[2]->exponent + e1 + e2 );
      TEST(0 == cmp_bigint(big[2], big[3])) ;
   }

   // TEST mult_bigint: splitting with 4K number
   TEST(0 == delete_bigint(&big[3])) ;
   TEST(0 == new_bigint(&big[3], nrdigitsmax_bigint())) ;
   setfromuint32_bigint(big[0], 0) ;
   setfromuint32_bigint(big[1], 0) ;
   setfromuint32_bigint(big[2], 0) ;
   big[0]->sign_and_used_digits = 0x1FFF ;
   big[1]->sign_and_used_digits = 0x1000 ;
   big[2]->sign_and_used_digits = 0x1000 ;
   for (uint16_t i = 0; i < 0x1000; ++i) {
      big[0]->digits[i] = 1u + i ;
      big[1]->digits[i] = 1 ;
      big[2]->digits[i] = 1 ;
   }
   for (uint16_t i = 0; i < 0x1000; ++i) {
      big[0]->digits[0x1FFF-i] = i ;
   }
   TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;
   TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;

   // TEST mult_bigint: splitting with both right parts 0
   setfromuint32_bigint(big[0], 0) ;
   setfromuint32_bigint(big[1], 0) ;
   setfromuint32_bigint(big[2], 0) ;
   big[0]->sign_and_used_digits = 1 ;
   big[0]->exponent             = 2 * (0x1000-1) ;
   big[1]->sign_and_used_digits = 0x1000 ;
   big[2]->sign_and_used_digits = 0x1000 ;
   for (uint16_t i = 0; i < 0x1000; ++i) {
      big[1]->digits[i] = (i == 0x1000-1) ? 12 : 0 ;
      big[2]->digits[i] = (i == 0x1000-1) ? 13 : 0 ;
   }
   big[0]->digits[0] = 12*13 ;
   setnegative_bigint(big[0]) ;
   setnegative_bigint(big[1]) ;
   TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;
   TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;

   // TEST mult_bigint: splitting with one right part 0
   setfromuint32_bigint(big[0], 0) ;
   setfromuint32_bigint(big[1], 0) ;
   setfromuint32_bigint(big[2], 0) ;
   big[0]->sign_and_used_digits = 0x1000 ;
   big[0]->exponent             = 0x1000-1 ;
   big[1]->sign_and_used_digits = 0x1000 ;
   big[2]->sign_and_used_digits = 0x1000 ;
   for (uint16_t i = 0; i < 0x1000; ++i) {
      big[0]->digits[i] = (i == 0x1000-1) ? 12*13 : 13 ;
      big[1]->digits[i] = (i == 0x1000-1) ? 12 : 1 ;
      big[2]->digits[i] = (i == 0x1000-1) ? 13 : 0 ;
   }
   TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;
   setnegative_bigint(big[0]) ;
   setnegative_bigint(big[1]) ;
   TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;

   // TEST mult_bigint: splitting with right part 0 and one number smaller
   setfromuint32_bigint(big[0], 0) ;
   setfromuint32_bigint(big[1], 0) ;
   setfromuint32_bigint(big[2], 0) ;
   big[0]->sign_and_used_digits = 1 ;
   big[0]->exponent             = 0x1000 + 256 -2 ;
   big[1]->sign_and_used_digits = 0x1000 ;
   big[2]->sign_and_used_digits = 256 ;
   for (uint16_t i = 0; i < 0x1000; ++i) {
      big[1]->digits[i] = (i == 0x1000-1) ? 12 : 0 ;
      big[2]->digits[i] = (i == 255) ? 13 : 0 ;
   }
   big[0]->digits[0] = 12*13 ;
   TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;
   setnegative_bigint(big[1]) ;
   setnegative_bigint(big[2]) ;
   TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;

   // TEST mult_bigint: splitting with one number smaller
   setfromuint32_bigint(big[0], 0) ;
   setfromuint32_bigint(big[1], 0) ;
   setfromuint32_bigint(big[2], 0) ;
   big[0]->sign_and_used_digits = 0x1000 ;
   big[0]->exponent             = 256-1 ;
   big[1]->sign_and_used_digits = 0x1000 ;
   big[2]->sign_and_used_digits = 256 ;
   for (uint16_t i = 0; i < 0x1000; ++i) {
      big[0]->digits[i] = (i == 0x1000-1) ? 12*13 : 13 ;
      big[1]->digits[i] = (i == 0x1000-1) ? 12 : 1 ;
      big[2]->digits[i] = (i == 255) ? 13 : 0 ;
   }
   TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;
   setnegative_bigint(big[0]) ;
   setnegative_bigint(big[1]) ;
   TEST(0 == mult_bigint(&big[3], big[2], big[1])) ;
   TEST(0 == cmp_bigint(big[0], big[3])) ;

   // TEST mult_bigint: splitting with random number
   for (int ti = 0; ti < 50; ++ti) {
      setfromuint32_bigint(big[0], 0) ;
      setfromuint32_bigint(big[1], 0) ;
      setfromuint32_bigint(big[2], 0) ;
      big[1]->sign_and_used_digits = 300 ;
      big[2]->sign_and_used_digits = 300 ;
      for (uint16_t i = 0; i < 300; ++i) {
         big[1]->digits[i] = (uint32_t) random() ;
         big[2]->digits[i] = UINT32_MAX -(uint32_t) random() ;
      }
      mult_biginthelper(big[0], 300, big[1]->digits, 300, big[2]->digits, 0) ;
      for (int s =-1; s <= +1; s += 2) {
         setpositive_bigint(big[0]) ;
         TEST(0 == mult_bigint(&big[3], big[1], big[2])) ;
         TEST(0 == cmp_bigint(big[0], big[3])) ;
         negate_bigint(big[1]) ;
         setnegative_bigint(big[0]) ;
         TEST(0 == mult_bigint(&big[3], big[s < 0 ? 2 : 1], big[s < 0 ? 1 : 2])) ;
         TEST(0 == cmp_bigint(big[0], big[3])) ;
         negate_bigint(big[2]) ;
      }
   }

   // TEST multui32_bigint EOVERFLOW
   setfromuint32_bigint(big[1], 0) ;
   setfromuint32_bigint(big[2], 0) ;
   big[2]->sign_and_used_digits = nrdigitsmax_bigint() ;
   TEST(EOVERFLOW == multui32_bigint(&big[3], big[2], 10)) ;

   // TEST mult_bigint EOVERFLOW
   big[2]->sign_and_used_digits = 0x4000 ;
   big[1]->sign_and_used_digits = 0x4000 ;
   TEST(EOVERFLOW == mult_bigint(&big[3], big[2], big[1])) ;
   setfromuint32_bigint(big[1], 1) ;
   setfromuint32_bigint(big[2], 1) ;
   big[2]->exponent = 0x8000 ;
   big[1]->exponent = 0x8000 ;
   TEST(EOVERFLOW == mult_bigint(&big[3], big[2], big[1])) ;

   // TEST mult_bigint: memory error
   setfromuint32_bigint(big[1], 0) ;
   setfromuint32_bigint(big[2], 0) ;
   big[1]->sign_and_used_digits = 300 ;
   big[2]->sign_and_used_digits = 300 ;
   big[1]->digits[299] = 1 ;
   big[2]->digits[299] = 2 ;
   test_errortimer_t errtimer ;
   TEST(0 == init_testerrortimer(&errtimer, 1, ENOMEM)) ;
   setresizeerr_mmtest(mmcontext_mmtest(), &errtimer) ;
   TEST(ENOMEM == mult_bigint(&big[3], big[2], big[1])) ;
   TEST(0 == init_testerrortimer(&errtimer, 1, EPROTO)) ;
   setfreeerr_mmtest(mmcontext_mmtest(), &errtimer) ;
   TEST(EPROTO == mult_bigint(&big[3], big[2], big[1])) ;

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

static int test_div(void)
{
   bigint_t    * big[5] = { 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == new_bigint(&big[i], nrdigitsmax_bigint())) ;
   }

   // TEST divisor == 0
   setfromint32_bigint(big[0], 0) ;
   setfromint32_bigint(big[1], 0) ;
   TEST(EINVAL == divui32_bigint(&big[2], big[0], 0)) ;
   TEST(EINVAL == divmodui32_bigint(&big[2], &big[3], big[0], 0)) ;
   TEST(EINVAL == div_bigint(&big[2], big[0], big[1])) ;
   TEST(EINVAL == divmod_bigint(&big[2], &big[3], big[0], big[1])) ;
   setfromint32_bigint(big[0], 10) ;
   TEST(EINVAL == divui32_bigint(&big[2], big[0], 0)) ;
   TEST(EINVAL == divmodui32_bigint(&big[2], &big[3], big[0], 0)) ;
   TEST(EINVAL == div_bigint(&big[2], big[0], big[1])) ;
   TEST(EINVAL == divmod_bigint(&big[2], &big[3], big[0], big[1])) ;

   // TEST divmodui32_bigint, divmod_bigint (dividend one digit, divisor bigger)
#define M UINT32_MAX
   uint32_t testrows[][2][1] = {
                /* { dividend }, { divisor } */
                { { 0 }, { 100 } }
               ,{ { 0 }, { M } }
               ,{ { 1 }, { M } }
               ,{ { M-1 }, { M } }
               ,{ { 1 }, { 2 } }
               ,{ { 1 }, { 1001 } }
               ,{ { 1023 }, { 100000 } }
            } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows); ++i) {
      for (unsigned nr = 0; nr < 2; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 1, testrows[i][nr], 0)) ;
      }
      TEST(0 == divmodui32_bigint(&big[2], &big[3], big[0], big[1]->digits[0])) ;
      TEST(0 == sign_bigint(big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setfromuint32_bigint(big[2], 12345) ;
      setfromuint32_bigint(big[3], 12345) ;
      TEST(0 == divmod_bigint(&big[2], &big[3], big[0], big[1])) ;
      TEST(0 == sign_bigint(big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setnegative_bigint(big[1]) ;
      TEST(0 == divmod_bigint(&big[2], &big[3], big[0], big[1])) ;
      TEST(0 == sign_bigint(big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setnegative_bigint(big[0]) ;
      TEST(0 == divmodui32_bigint(&big[2], &big[3], big[0], big[1]->digits[0])) ;
      TEST(0 == sign_bigint(big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setfromuint32_bigint(big[2], 12345) ;
      setfromuint32_bigint(big[3], 12345) ;
      TEST(0 == divmod_bigint(&big[2], &big[3], big[0], big[1])) ;
      TEST(0 == sign_bigint(big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setpositive_bigint(big[1]) ;
      setfromuint32_bigint(big[2], 12345) ;
      setfromuint32_bigint(big[3], 12345) ;
      TEST(0 == divmod_bigint(&big[2], &big[3], big[0], big[1])) ;
      TEST(0 == sign_bigint(big[2])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
   }

   // TEST divmodui32_bigint, divmod_bigint (divisor one digit)
#define M UINT32_MAX
   uint32_t testrows2[][2][10] = {
                /* { multiplicand }, { factor/divisor } */
                { { 0 }, { 100 } }
               ,{ { 1, 2, 3 }, { M/5 } }
               ,{ { 100, 102, M, M/8, M/9, M/10, M-1, M-2 }, { M } }
               ,{ { 1000, 1020, 20000, M, M-1000, M-10000 }, { 1222345 } }
            } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows2); ++i) {
      for (unsigned nr = 0; nr < 2; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows2[i][nr], 0)) ;
      }
      big[1]->exponent = 0 ;
      TEST(0 == mult_bigint(&big[2], big[0], big[1])) ;
      // big[2] = big[0] * big[1] =>  big[0] == (big[2] / big[1]) !
      TEST(0 == divmodui32_bigint(&big[3], 0, big[2], big[1]->digits[0])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == divmodui32_bigint(0, &big[3], big[2], big[1]->digits[0])) ;
      TEST(0 == sign_bigint(big[3])) ;
      TEST(0 == divmod_bigint(&big[3], 0, big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      setnegative_bigint(big[0]) ;
      setnegative_bigint(big[2]) ;
      TEST(0 == divmod_bigint(&big[3], 0, big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == divmod_bigint(0, &big[3], big[2], big[1])) ;
      TEST(0 == sign_bigint(big[3])) ;
      setnegative_bigint(big[1]) ;
      setpositive_bigint(big[2]) ;
      TEST(0 == divmod_bigint(&big[3], 0, big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == divmod_bigint(0, &big[3], big[2], big[1])) ;
      TEST(0 == sign_bigint(big[3])) ;
      setnegative_bigint(big[2]) ;
      setpositive_bigint(big[0]) ;
      TEST(0 == divmod_bigint(&big[3], 0, big[2], big[1])) ;
      TEST(0 == cmp_bigint(big[0], big[3])) ;
      TEST(0 == divmod_bigint(0, &big[3], big[2], big[1])) ;
      TEST(0 == sign_bigint(big[3])) ;
      setpositive_bigint(big[1]) ;
      setpositive_bigint(big[2]) ;
      for (unsigned add = 1; add < 50; add += 13) {
         setfromuint32_bigint(big[3], add) ;
         TEST(0 == add_bigint(&big[4], big[3], big[2])) ;
         // big[4] == big[0] * big[1] + add && (add < big[1]) =>  big[0] == (big[2] / big[1]) !
         TEST(0 == divmod_bigint(&big[3], 0, big[4], big[1])) ;
         TEST(0 == cmp_bigint(big[0], big[3])) ;
         // => add == (big[2] % big[1]) !
         TEST(0 == divmod_bigint(0, &big[3], big[4], big[1])) ;
         setfromuint32_bigint(big[4], add) ;
         TEST(0 == cmp_bigint(big[4], big[3])) ;
      }
   }

   // TEST submul_biginthelper called from divmod_bigint
#define M UINT32_MAX
   uint32_t testrows3[][4][10] = {
                /* { diff }, { divisor }, { result diff }, { factor, corrected factor },  */
                { { M, M, M, M, M, 0, 1, 2 }, /*{ M-1, M, M, M, M, 1 },*/ { 0, M, M, M, M, M }, { 0, M, M, M, M, M, 1, 2 }, { M, M } }
               ,{ { 2, 2, 0, 0, 0, 1 }, /*{ 2, 2, 0, 0, 0, 2 },*/ { 1, 1, 0, 0, 0, 1 }, { 1, 1, 0, 0, 0, 0 }, { 2, 1 } }
               ,{ { 1, 0, 0, 1 }, /*{ 0, 0x8, 0, 0 },*/ { 0, 0x80000000, 0, 0 }, { 0, 0x80000000, 0, 1 }, { 1, 1 } }
               ,{ { 1, 0, 0, 1 }, /*{ 1, 0, 0, 0 },*/ { 0, 0x80000000, 0, 0 }, { 0, 0, 0, 1 }, { 2, 2 } }
               ,{ { 1, 0, 0, 1 }, /*{ 1, 0, 0, 2 },*/ { 0, 0x80000000, 0, 1 }, { 0, 0x80000000, 0, 0 }, { 2, 1 } }
            } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows3); ++i) {
      for (unsigned nr = 0; nr < 3; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows3[i][nr], 0)) ;
      }
      big[1]->exponent = (uint16_t) (big[1]->exponent - big[0]->exponent) ;
      big[2]->exponent = (uint16_t) (big[2]->exponent - big[0]->exponent) ;
      big[0]->exponent = 0 ;
      uint32_t factor  = testrows3[i][3][0] ;
      uint32_t factor2 = testrows3[i][3][1] ;
      const bool big0_has_carry = (size_bigint(big[0]) > size_bigint(big[1])) ;
      TEST(factor2 == submul_biginthelper(big[0], big0_has_carry, factor, 0, nrdigits_bigint(big[1]), big[1])) ;
      while (big[0]->sign_and_used_digits > 0 && 0 == big[0]->digits[big[0]->sign_and_used_digits-1]) {
         -- big[0]->sign_and_used_digits ;
      }
      TEST(0 == cmp_bigint(big[0], big[2])) ;
   }

   // TEST divmod_bigint (divisor multiple digit)
#define M UINT32_MAX
   uint32_t testrows4[][2][10] = {
                /* { multiplicand }, { factor/divisor } */
                { { 1 }, { 88, 99, 0, 0, 0, 0, 0, 0, 0, 0 } }
               ,{ { M, 0, 0, M, M, 0, 0, 0, 1, 2 }, { M, 1, M, 5, 0, 1 } }
               ,{ { 1, 2, 3 }, { M/5, 5 } }
               ,{ { 1, 2, 3 }, { M, M, M } }
               ,{ { 1, 0, 2, 0, 3, 0, 0, 0, 2 }, { M, M, M, M, 0, 3 } }
               ,{ { 100, 102, M, M/8, M/9, M/10, M-1, M-2 }, { M, 0, 0, 1} }
               ,{ { 1000, 1020, 20000, M, M-1000, M-10000, 0, 0, 1 }, { 0, 0, 0, 0, 1222345, 0, 1, 0, 4 } }
            } ;
   uint32_t testadd[][5] = { { 0 }, { 0, 0, 0, 0, M}, { 1, 2, 3, 4, 5}, { M, M, M, M, M } } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows4); ++i) {
      for (unsigned nr = 0; nr < 2; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows4[i][nr], 0)) ;
      }
      for (unsigned addi = 0; addi < nrelementsof(testadd); ++addi) {
         TEST(0 == setbigfirst_bigint(&big[4], +1, 5, testadd[addi], 0)) ; // modulo
         TEST(0 == mult_bigint(&big[3], big[0], big[1])) ;
         TEST(0 == add_bigint(&big[2], big[3], big[4])) ;
         TEST(0 == divmod_bigint(&big[3], 0, big[2], big[1])) ;
         TEST(0 == cmp_bigint(big[3], big[0])) ;
         TEST(0 == divmod_bigint(0, &big[3], big[2], big[1])) ;
         TEST(0 == cmp_bigint(big[4], big[3])) ; // modulo
         negate_bigint(big[1]) ;
         negate_bigint(big[0]) ;
         TEST(0 == divmod_bigint(&big[3], 0, big[2], big[1])) ;
         TEST(0 == cmp_bigint(big[3], big[0])) ;
         TEST(0 == divmod_bigint(0, &big[3], big[2], big[1])) ;
         TEST(0 == cmp_bigint(big[4], big[3])) ; // modulo
         negate_bigint(big[4]) ;
         negate_bigint(big[2]) ;
         negate_bigint(big[1]) ;
         TEST(0 == divmod_bigint(&big[3], 0, big[2], big[1])) ;
         TEST(0 == cmp_bigint(big[3], big[0])) ;
         TEST(0 == divmod_bigint(0, &big[3], big[2], big[1])) ;
         TEST(0 == cmp_bigint(big[4], big[3])) ; // modulo
         negate_bigint(big[0]) ;
         negate_bigint(big[1]) ;
         TEST(0 == divmod_bigint(&big[3], 0, big[2], big[1])) ;
         TEST(0 == cmp_bigint(big[3], big[0])) ;
         TEST(0 == divmod_bigint(0, &big[3], big[2], big[1])) ;
         TEST(0 == cmp_bigint(big[4], big[3])) ; // modulo
         negate_bigint(big[1]) ;
         negate_bigint(big[2]) ;
      }
   }

   // TEST divmod_bigint (dividend (one digit but) with bigger exponent)
#define M UINT32_MAX
   uint32_t testrows5[][3][10] = {
                /* { dividend }, { divisor }, { result } */
                { { 5 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 10 }, { 0, 0x80000000 } }
               ,{ { 5 }, { 0, 0, 0, 0, 0, 0, 0, M, M, M },  { 0, 0, 0, 5, 0, 0, 5, 0, 0, 5 } }
            } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows5); ++i) {
      for (unsigned nr = 0; nr < 3; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows5[i][nr], 0)) ;
      }
      TEST(0 == divmod_bigint(&big[3], 0, big[0], big[1])) ;
      TEST(0 == cmp_bigint(big[3], big[2])) ;
   }

   // TEST divmod_bigint: estimatedigit3_biginthelper special case (divisor == dividend)
#define M UINT32_MAX
   uint32_t testrows6[][3][10] = {
                /* { dividend }, { divisor }, { result } */
                { { 88, 99, }, { 0, 88, 99,0,0,0,0,0,0,1 }, {0,0,0,0,0,0,0,0,0,M} }
            } ;
#undef M
   for (unsigned i = 0; i < nrelementsof(testrows6); ++i) {
      for (unsigned nr = 0; nr < 3; ++nr) {
         TEST(0 == setbigfirst_bigint(&big[nr], +1, 10, testrows6[i][nr], 0)) ;
      }
      TEST(0 == divmod_bigint(&big[3], 0, big[0], big[1])) ;
      TEST(0 == cmp_bigint(big[3], big[2])) ;
   }

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

static int test_shift(void)
{
   bigint_t * big = 0 ;

   // TEST shiftleft_bigint with multiple of 32
   TEST(0 == new_bigint(&big, 100)) ;
   for (unsigned i = 0; i < 100; ++i) {
      big->digits[i] = 0x12345678 ;
   }
   big->sign_and_used_digits = 100 ;
   for (unsigned i = 0; i <= 32 * 0x7fff; i += 32) {
      big->exponent = 0 ;
      TEST(0 == shiftleft_bigint(&big, i)) ;
      TEST(100  == big->allocated_digits) ;
      TEST(100  == nrdigits_bigint(big)) ;
      TEST(i/32 == exponent_bigint(big)) ;
      for (unsigned di = 0; di < 100; ++di) {
         TEST(big->digits[di] == 0x12345678) ;
      }
      if (32 * 0xfff == i) {
         i = 32 * 0x6fff ;  // warp from 32*0xfff to 32*0x6fff
      }
   }
   TEST(0 == delete_bigint(&big)) ;

   // TEST shiftleft_bigint values 1 .. 31
   TEST(0 == new_bigint(&big, 100)) ;
   for (unsigned i = 1; i <= 31; ++i) {
      for (int s = -1; s <= +1; s += 2) {
         for (unsigned di = 0; di < 100; ++di) {
            big->digits[di] = 512*13*(di+1) ;
         }
         big->sign_and_used_digits = (int16_t) (s * 100) ;
         big->exponent             = 0 ;
         TEST(0 == shiftleft_bigint(&big, i + 32*i)) ;
         TEST(s == sign_bigint(big)) ;
         TEST(i == exponent_bigint(big)) ;
         bool isOver = (((uint64_t)512*13*100 << i) > UINT32_MAX) ;
         TEST(100+isOver == big->allocated_digits) ;
         TEST(100+isOver == nrdigits_bigint(big)) ;
         for (unsigned di = 0; di < 100; ++di) {
            uint32_t shiftedvalue = (512*13*(di+1)) << i  ;
            shiftedvalue += (512*13*di) >> (32 - i) ;
            TEST(shiftedvalue == big->digits[di]) ;
         }
         if (isOver) {
            TEST(big->digits[100] == (uint32_t)512*13*100 >> (32-i)) ;
         }
      }
   }
   TEST(0 == delete_bigint(&big)) ;

   // TEST shiftleft_bigint EOVERFLOW
   TEST(0 == new_bigint(&big, 100)) ;
   big->exponent = 0 ;
   big->sign_and_used_digits = 0 ; /*value 0 is ignored no overflow*/
   TEST(0 == shiftleft_bigint(&big, 32u*0xffff + 32)) ;
   big->sign_and_used_digits = 1 ;
   big->digits[0] = 1 ;
   TEST(0 == shiftleft_bigint(&big, 32u*0xffff + 31)) ;
   TEST(0xffff == big->exponent) ;
   TEST(1      == big->sign_and_used_digits) ;
   TEST(1u<<31 == big->digits[0]) ;
   big->exponent  = 0 ;
   big->sign_and_used_digits = 1 ;
   big->digits[0] = 1 ;
   TEST(EOVERFLOW == shiftleft_bigint(&big, 32u*0xffff + 32 + 31)) ;
   TEST(0 == big->exponent) ;
   TEST(1 == big->sign_and_used_digits) ;
   TEST(1 == big->digits[0]) ;
   big->exponent = 1 ;
   TEST(EOVERFLOW == shiftleft_bigint(&big, 32u*0xffff + 31)) ;
   TEST(1 == big->exponent) ;
   TEST(1 == big->sign_and_used_digits) ;
   TEST(1 == big->digits[0]) ;
   TEST(0 == delete_bigint(&big)) ;

   // TEST shiftleft_bigint ENOMEM
   TEST(0 == new_bigint(&big, 100)) ;
   test_errortimer_t errtimer ;
   TEST(0 == init_testerrortimer(&errtimer, 1, ENOMEM)) ;
   setresizeerr_mmtest(mmcontext_mmtest(), &errtimer) ;
   for (unsigned i = 0; i < 100; ++i) {
      big->digits[i] = 0x12345678 ;
   }
   big->sign_and_used_digits = 100 ;
   big->exponent             = 0 ;
   TEST(ENOMEM == shiftleft_bigint(&big, 32 + 8)) ;
   TEST(100  == big->allocated_digits) ;
   TEST(100  == nrdigits_bigint(big)) ;
   TEST(0    == exponent_bigint(big)) ;
   for (unsigned di = 0; di < 100; ++di) {
      TEST(big->digits[di] == 0x12345678) ;
   }
   TEST(0 == delete_bigint(&big)) ;

   // TEST shiftright_bigint with multiple of 32 adjusts only exponent
   TEST(0 == new_bigint(&big, 100)) ;
   for (unsigned i = 0; i < 100; ++i) {
      big->digits[i] = (i << 8) | 0x400000ff ;
   }
   for (uint32_t  shiftcount = 0; shiftcount <= 32 * 100; shiftcount += 32) {
      for (int s = -1; s <= 1; s += 2) {
         big->exponent             = (uint16_t) 100 ;
         big->sign_and_used_digits = (int16_t) (s * 100) ;
         TEST(0 == shiftright_bigint(&big, shiftcount)) ;
         TEST(exponent_bigint(big) == 100-shiftcount/32) ;
         TEST(nrdigits_bigint(big) == 100) ;
         for (unsigned i = 0; i < 100; ++i) {
            TEST(big->digits[i] == ((i << 8) | 0x400000ff)) ;
         }
      }
   }
   TEST(0 == delete_bigint(&big)) ;

   // TEST shiftright_bigint with multiple of 32 adjusts exponent and moves digits
   TEST(0 == new_bigint(&big, 100)) ;
   for (uint32_t shiftcount = 1; shiftcount <= 0x8000u; ++ shiftcount) {
      for (int s = -1; s <= 1; s += 2) {
         for (unsigned i = 0; i < 100; ++i) {
            big->digits[i] = (i << 8) | 0x400000ff ;
         }
         big->exponent             = (uint16_t) (s + 10) ;
         big->sign_and_used_digits = (int16_t) (s * 100) ;
         TEST(0 == shiftright_bigint(&big, 32 * (shiftcount + (uint32_t)s + 10))) ;
         TEST(exponent_bigint(big) == 0) ;
         TEST(nrdigits_bigint(big) == (shiftcount < 100 ? 100 - shiftcount : 0)) ;
         TEST(sign_bigint(big)     == (nrdigits_bigint(big) ? s : 0)) ;
         for (unsigned i = 0; i < nrdigits_bigint(big); ++i) {
            TEST(big->digits[i] == (((i+shiftcount) << 8) | 0x400000ff)) ;
         }
      }
      if (shiftcount == 100) {
         shiftcount = 0x8000u - 1 ;
      }
   }
   TEST(0 == delete_bigint(&big)) ;

   // TEST shiftright_bigint with values 0..31
   for (uint32_t shiftcount = 0; shiftcount <= 31; ++ shiftcount) {
      TEST(0 == new_bigint(&big, 100)) ;
      for (int s = -1; s <= 1; s += 2) {
         for (unsigned i = 0; i < 100; ++i) {
            big->digits[i] = (i << 8) | 0x080000f0 ;
         }
         big->exponent             = 2 ;
         big->sign_and_used_digits = (int16_t) (s * 100) ;
         TEST(0 == shiftright_bigint(&big, shiftcount)) ;
         TEST(big->allocated_digits == 100 + (shiftcount > 4)) ;
         TEST(exponent_bigint(big)  == 1   + (shiftcount <= 4)) ;
         TEST(nrdigits_bigint(big)  == 100 + (shiftcount > 4) - (shiftcount >= 28)) ;
         TEST(sign_bigint(big)      == s) ;
         const unsigned offset = (shiftcount > 4) ;
         TEST(!offset || big->digits[0] == (0x080000f0u << (32 - shiftcount))) ;
         for (unsigned i = 0; i <= 99-(shiftcount >= 28); ++i) {
            uint32_t ldigit = (i+1) < 100 ? ((i+1) << 8) | 0x080000f0 : 0 ;
            uint32_t rdigit = (i << 8) | 0x080000f0 ;
            uint64_t digit  = (((uint64_t)ldigit << 32) + rdigit) >> shiftcount ;
            TEST(big->digits[i+offset] == (uint32_t)digit) ;
         }
      }
      TEST(0 == delete_bigint(&big)) ;
   }

   // TEST shiftright_bigint with values 33..127
   TEST(0 == new_bigint(&big, 100)) ;
   for (uint32_t shiftcount = 33; shiftcount <= 127; ++ shiftcount) {
      for (int s = -1; s <= 1; s += 2) {
         for (unsigned i = 0; i < 100; ++i) {
            big->digits[i] = (i << 8) | 0x080000f0 ;
         }
         big->exponent             = 1 ;
         big->sign_and_used_digits = (int16_t) (s * 100) ;
         TEST(0 == shiftright_bigint(&big, shiftcount)) ;
         const uint32_t skip_digit = (shiftcount/32) - 1 ;
         TEST(big->allocated_digits == 100) ;
         TEST(exponent_bigint(big)  == 0) ;
         TEST(nrdigits_bigint(big)  == 100 - skip_digit - ((shiftcount % 32) >= 28)) ;
         TEST(sign_bigint(big)      == s) ;
         for (unsigned i = skip_digit; i <= 99-((shiftcount%32) >= 28); ++i) {
            uint32_t ldigit = (i+1) < 100 ? ((i+1) << 8) | 0x080000f0 : 0 ;
            uint32_t rdigit = (i << 8) | 0x080000f0 ;
            uint64_t digit  = (((uint64_t)ldigit << 32) + rdigit) >> (shiftcount%32) ;
            TEST(big->digits[i-skip_digit] == (uint32_t)digit) ;
         }
      }
   }
   TEST(0 == delete_bigint(&big)) ;

   // TEST shiftright_bigint ENOMEM
   TEST(0 == new_bigint(&big, 100)) ;
   TEST(0 == init_testerrortimer(&errtimer, 1, ENOMEM)) ;
   setresizeerr_mmtest(mmcontext_mmtest(), &errtimer) ;
   for (unsigned i = 0; i < 100; ++i) {
      big->digits[i] = 0x12345678 ;
   }
   big->sign_and_used_digits = 100 ;
   big->exponent             = 1 ;
   TEST(ENOMEM == shiftright_bigint(&big, 4)) ;
   TEST(100  == big->allocated_digits) ;
   TEST(100  == nrdigits_bigint(big)) ;
   TEST(1    == exponent_bigint(big)) ;
   for (unsigned di = 0; di < 100; ++di) {
      TEST(big->digits[di] == 0x12345678) ;
   }
   TEST(0 == delete_bigint(&big)) ;

   return 0 ;
ONABORT:
   delete_bigint(&big) ;
   return EINVAL ;
}

static int test_example1(void)
{
   bigint_t    * big[5] = { 0 } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == new_bigint(&big[i], nrdigitsmax_bigint())) ;
   }

   /* ( 64919121    -159018721 ) * (x) = (1)
    * ( 41869520.5  -102558961 )   (y) = (0)
    * x = 205117922 , y = 83739041 */

   long double y = 41869520.5 / (-159018721.0 * 41869520.5 - (64919121.0 * (-102558961.0)) ) ;
   long double x = - (-102558961.0 / 41869520.5 ) * y ;

   double xwrong = 205117922 / (double)x ;  // x87 fpu => xwrong = 2
   double ywrong = 83739041 / (double)y ;   // x87 fpu => ywrong = 2

   TEST(fabs(xwrong) > 1.1) ;
   TEST(fabs(ywrong) > 1.1) ;

   // TEST solution of linear equation with big integer math
   // compute y
   setfromdouble_bigint(big[0], -159018721.0) ;
   setfromdouble_bigint(big[1], 2*41869520.5) ;
   TEST(0 == mult_bigint(&big[2], big[0], big[1])) ; // big[2] = -159018721.0 * 41869520.5
   setfromdouble_bigint(big[0], 64919121.0) ;
   setfromdouble_bigint(big[1], -2*102558961.0) ;
   TEST(0 == mult_bigint(&big[3], big[0], big[1])) ; // big[3] = (64919121.0 * (-102558961.0))
   TEST(0 == sub_bigint(&big[4], big[2], big[3])) ; // big[4] = big[2] - big[3]
   setfromdouble_bigint(big[0], 2*41869520.5) ;
   TEST(0 == divmod_bigint(&big[1], 0, big[0], big[4])) ;   // big[1] == y == 83739041
   setfromdouble_bigint(big[0], 83739041) ;
   TEST(0 == cmp_bigint(big[1], big[0])) ;
   // compute x
   setfromdouble_bigint(big[0], 2*102558961.0) ;
   TEST(0 == mult_bigint(&big[2], big[0], big[1])) ; // big[2] = 102558961.0 * y
   setfromdouble_bigint(big[0], 2*41869520.5) ;
   TEST(0 == divmod_bigint(&big[1], 0, big[2], big[0])) ; // big[1] = 102558961.0 * y / 41869520.5 == x
   setfromdouble_bigint(big[0], 205117922) ;
   TEST(0 == cmp_bigint(big[1], big[0])) ;

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

static int test_fixedsize(void)
{
   bigint_t                * big[3] = { 0 } ;
   bigint_fixed_DECLARE(14) ;
   bigint_fixed_DECLARE(4) bigf[3]  = {    bigint_fixed_INIT(4, 0, 1,  2,  3,  4)
                                          ,bigint_fixed_INIT(-4, 8, 9, 10, 11, 12)
                                          ,bigint_fixed_INIT(4, 4, 5, 6, 7, 8)
                                      } ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(big); ++i) {
      TEST(0 == new_bigint(&big[i], nrdigitsmax_bigint())) ;
   }
   setlittlefirst_bigint(&big[0], +1, 12, (uint32_t[12]){1,2,3,4,5,6,7,8,9,10,11,12}, 0) ;

   // TEST bigint_fixed_DECLARE
   static_assert(offsetof(struct bigint_fixed4_t, allocated_digits)     == offsetof(bigint_t, allocated_digits), "") ;
   static_assert(offsetof(struct bigint_fixed4_t, sign_and_used_digits) == offsetof(bigint_t, sign_and_used_digits), "") ;
   static_assert(offsetof(struct bigint_fixed4_t, exponent)             == offsetof(bigint_t, exponent), "") ;
   static_assert(offsetof(struct bigint_fixed4_t, digits)               == offsetof(bigint_t, digits), "") ;
   static_assert(sizeof(((struct bigint_fixed4_t*)0)->digits)           == 4 * sizeof(uint32_t), "") ;
   static_assert(offsetof(struct bigint_fixed14_t, allocated_digits)    == offsetof(bigint_t, allocated_digits), "") ;
   static_assert(offsetof(struct bigint_fixed14_t, sign_and_used_digits)== offsetof(bigint_t, sign_and_used_digits), "") ;
   static_assert(offsetof(struct bigint_fixed14_t, exponent)            == offsetof(bigint_t, exponent), "") ;
   static_assert(offsetof(struct bigint_fixed14_t, digits)              == offsetof(bigint_t, digits), "") ;
   static_assert(sizeof(((struct bigint_fixed14_t*)0)->digits)          == 14 * sizeof(uint32_t), "") ;
   static_assert(sizeof(struct bigint_fixed14_t)                        <= 16 * sizeof(uint32_t), "") ;
   static_assert(sizeof(struct bigint_fixed14_t)                        >  15 * sizeof(uint32_t), "") ;

   // TEST bigint_fixed_INIT
   TEST(0 == bigf[0].allocated_digits) ;
   TEST(0 == bigf[1].allocated_digits) ;
   TEST(0 == bigf[2].allocated_digits) ;
   TEST(4 == nrdigits_bigint(&bigf[0])) ;
   TEST(4 == nrdigits_bigint(&bigf[1])) ;
   TEST(4 == nrdigits_bigint(&bigf[1])) ;
   TEST(+1 == sign_bigint(&bigf[0])) ;
   TEST(-1 == sign_bigint(&bigf[1])) ;
   TEST(+1 == sign_bigint(&bigf[2])) ;
   TEST(0 == exponent_bigint(&bigf[0])) ;
   TEST(8 == exponent_bigint(&bigf[1])) ;
   TEST(4 == exponent_bigint(&bigf[2])) ;
   for (unsigned i = 0; i < 4; ++i) {
      TEST(i+1 == bigf[0].digits[i]) ;
      TEST(i+9 == bigf[1].digits[i]) ;
      TEST(i+5 == bigf[2].digits[i]) ;
   }

   // TEST reallocation fails with EINVAL
   bigint_t * tempf = (bigint_t*)&bigf[0] ;
   TEST(EINVAL == multui32_bigint(&tempf, big[0], 0xffffff)) ;

   // TEST simple calculation
   negate_bigint(&bigf[1]) ;
   TEST(0 == copy_bigint(&big[1], (bigint_t*)&bigf[1])) ;
   TEST(0 == add_bigint(&big[2], big[1], (bigint_t*)&bigf[0])) ;
   TEST(0 == add_bigint(&big[1], big[2], (bigint_t*)&bigf[2])) ;
   TEST(0 == cmp_bigint(big[1], big[0])) ;

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

int unittest_math_int_biginteger()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == switchon_mmtest()) ;
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_sign())        goto ONABORT ;
   if (test_nrdigits())    goto ONABORT ;
   if (test_compare())     goto ONABORT ;
   if (test_initfree())    goto ONABORT ;
   if (test_unaryops())    goto ONABORT ;
   if (test_assign())      goto ONABORT ;
   if (test_addsub())      goto ONABORT ;
   if (test_mult())        goto ONABORT ;
   if (test_div())         goto ONABORT ;
   if (test_shift())       goto ONABORT ;
   if (test_example1())    goto ONABORT ;
   if (test_fixedsize())   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == switchoff_mmtest()) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
