/* title: AtomicOps impl

   Implements <AtomicOps>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/math/int/atomic.h
    Header file <AtomicOps>.

   file: C-kern/math/int/atomic.c
    Implementation file <AtomicOps impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/atomic.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: atomic_uint32_t


// group: test

#ifdef KONFIG_UNITTEST

typedef struct intargs_t   intargs_t ;

struct intargs_t {
   uint32_t    u32 ;
   uint64_t    u64 ;
   uintptr_t   uptr ;
   uint8_t     intop ;
} ;

static int thread_readwrite(intargs_t * intargs)
{
   for (uint32_t i = 1; i; i <<= 1) {
      for (;;) {
         uint32_t val = atomicread_int(&intargs->u32) ;
         if (i == val) break ;
         assert((i >> 1) == val) ;
         yield_thread() ;
      }
      atomicwrite_int(&intargs->u64, ((uint64_t)i << 32)) ;
   }

   return 0 ;
}

static int test_readwrite(void)
{
   thread_t *  threads[8] = { 0 } ;
   intargs_t   intargs ;

   // TEST atomicread_int
   intargs.u32 = 0 ;
   intargs.u64 = 0 ;
   intargs.uptr = 0 ;
   TEST(0 == atomicread_int(&intargs.u32)) ;
   TEST(0 == atomicread_int(&intargs.u64)) ;
   TEST(0 == atomicread_int(&intargs.uptr)) ;
   for (uint32_t i = 1; i; i <<= 1) {
      TEST(i == atomicread_int(&i)) ;
   }
   for (uint64_t i = 1; i; i <<= 1) {
      TEST(i == atomicread_int(&i)) ;
   }
   for (uintptr_t i = 1; i; i <<= 1) {
      TEST(i == atomicread_int(&i)) ;
   }

   // TEST atomicwrite_int
   atomicwrite_int(&intargs.u32, 0) ;
   atomicwrite_int(&intargs.u64, 0) ;
   atomicwrite_int(&intargs.uptr, 0) ;
   TEST(0 == atomicread_int(&intargs.u32)) ;
   TEST(0 == atomicread_int(&intargs.u64)) ;
   TEST(0 == atomicread_int(&intargs.uptr)) ;
   for (uint32_t i = 1; i; i <<= 1) {
      atomicwrite_int(&intargs.u32, i) ;
      TEST(i == intargs.u32) ;
   }
   for (uint64_t i = 1; i; i <<= 1) {
      atomicwrite_int(&intargs.u64, i) ;
      TEST(i == intargs.u64) ;
   }
   for (uintptr_t i = 1; i; i <<= 1) {
      atomicwrite_int(&intargs.uptr, i) ;
      TEST(i == intargs.uptr) ;
   }

   // TEST atomicread_int, atomicwrite_int: multi thread
   intargs.u32  = 0 ;
   intargs.u64  = 0 ;
   intargs.uptr = 0 ;
   TEST(0 == newgeneric_thread(&threads[0], &thread_readwrite, &intargs)) ;
   for (uint32_t i = 1, old = 0; i; old = i, i <<= 1) {
      atomicwrite_int(&intargs.u32, i) ;
      for (;;) {
         uint64_t val = atomicread_int(&intargs.u64) ;
         if (val == (uint64_t)i << 32) break ;
         TEST(val == (uint64_t)old << 32) ;
         yield_thread() ;
      }
   }
   TEST(0 == delete_thread(&threads[0])) ;

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      TEST(0 == delete_thread(&threads[i])) ;
   }
   return EINVAL ;
}

enum intop_e {
   intop_add32,
   intop_add64,
   intop_addptr,
   intop_sub32,
   intop_sub64,
   intop_subptr,
   intop_swap32,
   intop_swap64,
   intop_swapptr,
   intop_NROFINTOPS
} ;

typedef enum intop_e    intop_e ;

static int thread_addsub(intargs_t * intargs)
{
   for (unsigned i = 0, o = 0; i < 100000; ++i) {
      switch (intargs->intop) {
      case intop_add32:
         atomicadd_int(&intargs->u32, 1) ;
         break ;
      case intop_add64:
         atomicadd_int(&intargs->u64, 1) ;
         break ;
      case intop_addptr:
         atomicadd_int(&intargs->uptr, 1) ;
         break ;
      case intop_sub32:
         atomicsub_int(&intargs->u32, 1) ;
         break ;
      case intop_sub64:
         atomicsub_int(&intargs->u64, 1) ;
         break ;
      case intop_subptr:
         atomicsub_int(&intargs->uptr, 1) ;
         break ;
      case intop_swap32:
         o = (unsigned) atomicswap_int(&intargs->u32, o, i+1) ;
         i = o?o-1:0 ;
         break ;
      case intop_swap64:
         o = (unsigned) atomicswap_int(&intargs->u64, o, i+1) ;
         i = o?o-1:0 ;
         break ;
      case intop_swapptr:
         o = (unsigned) atomicswap_int(&intargs->uptr, o, i+1) ;
         i = o?o-1:0 ;
         break ;
      }
   }

   return 0 ;
}

static int test_addsubswap(void)
{
   thread_t *  threads[4] = { 0 } ;
   intargs_t   intargs ;

   // TEST atomicadd_int: single thread
   intargs.u32 = 0 ;
   intargs.u64 = 0 ;
   intargs.uptr = 0 ;
   for (uint32_t i = 1; i; i <<= 1) {
      uint32_t o = (i-1) ;
      TEST(o == atomicadd_int(&intargs.u32, i)) ;
   }
   TEST(UINT32_MAX == atomicread_int(&intargs.u32)) ;
   for (uint64_t i = 1; i; i <<= 1) {
      uint64_t o = (i-1) ;
      TEST(o == atomicadd_int(&intargs.u64, i)) ;
   }
   TEST(UINT64_MAX == atomicread_int(&intargs.u64)) ;
   for (uintptr_t i = 1; i; i <<= 1) {
      uintptr_t o = (i-1) ;
      TEST(o == atomicadd_int(&intargs.uptr, i)) ;
   }
   TEST(UINTPTR_MAX == atomicread_int(&intargs.uptr)) ;

   // TEST atomicsub_int: single thread
   intargs.u32 = UINT32_MAX ;
   intargs.u64 = UINT64_MAX ;
   intargs.uptr = UINTPTR_MAX ;
   for (uint32_t i = 1; i; i <<= 1) {
      uint32_t o = UINT32_MAX - (i-1) ;
      TEST(o == atomicsub_int(&intargs.u32, i)) ;
   }
   TEST(0 == atomicread_int(&intargs.u32)) ;
   for (uint64_t i = 1; i; i <<= 1) {
      uint64_t o = UINT64_MAX - (i-1) ;
      TEST(o == atomicsub_int(&intargs.u64, i)) ;
   }
   TEST(0 == atomicread_int(&intargs.u64)) ;
   for (uintptr_t i = 1; i; i <<= 1) {
      uintptr_t o = UINTPTR_MAX - (i-1) ;
      TEST(o == atomicsub_int(&intargs.uptr, i)) ;
   }
   TEST(0 == atomicread_int(&intargs.uptr)) ;

   // TEST atomicswap_int: single thread
   intargs.u32 = 0 ;
   intargs.u64 = 0 ;
   intargs.uptr = 0 ;
   for (uint32_t i = 1, o = 0; i; o = i, i <<= 1) {
      TEST(o == atomicswap_int(&intargs.u32, o, i)) ;
      TEST(i == atomicswap_int(&intargs.u32, i, i)) ;
   }
   TEST(((uint32_t)1<<31) == atomicread_int(&intargs.u32)) ;
   for (uint64_t i = 1, o = 0; i; o = i, i <<= 1) {
      TEST(o == atomicswap_int(&intargs.u64, o, i)) ;
      TEST(i == atomicswap_int(&intargs.u64, i, i)) ;
   }
   TEST(((uint64_t)1<<63) == atomicread_int(&intargs.u64)) ;
   for (uintptr_t i = 1, o = 0; i; o = i, i <<= 1) {
      TEST(o == atomicswap_int(&intargs.uptr, o, i)) ;
      TEST(i == atomicswap_int(&intargs.uptr, i, i)) ;
   }
   TEST(((uintptr_t)1<<(bitsof(uintptr_t)-1)) == atomicread_int(&intargs.uptr)) ;

   // TEST atomicadd_int, atomicsub_int, atomicswap_int: multi thread
   intargs.u32 = 0 ;
   intargs.u64 = 0 ;
   intargs.uptr = 0 ;
   for (intop_e intop = 0; intop < intop_NROFINTOPS; ++intop) {
      intargs.intop  = intop ;
      for (unsigned i = 0; i < lengthof(threads); i += 1) {
         TEST(0 == newgeneric_thread(&threads[i], &thread_addsub, &intargs)) ;
      }
      for (unsigned i = 0; i < lengthof(threads); i += 1) {
         TEST(0 == delete_thread(&threads[i])) ;
      }
      switch (intop) {
      case intop_add32:
         TEST(lengthof(threads)*100000 == atomicread_int(&intargs.u32)) ;
         break ;
      case intop_add64:
         TEST(lengthof(threads)*100000 == atomicread_int(&intargs.u64)) ;
         break ;
      case intop_addptr:
         TEST(lengthof(threads)*100000 == atomicread_int(&intargs.uptr)) ;
         break ;
      case intop_sub32:
         TEST(0 == atomicread_int(&intargs.u32)) ;
         break ;
      case intop_sub64:
         TEST(0 == atomicread_int(&intargs.u64)) ;
         break ;
      case intop_subptr:
         TEST(0 == atomicread_int(&intargs.uptr)) ;
         break ;
      case intop_swap32:
         TEST(100000 == atomicread_int(&intargs.u32)) ;
         break ;
      case intop_swap64:
         TEST(100000 == atomicread_int(&intargs.u64)) ;
         break ;
      case intop_swapptr:
         TEST(100000 == atomicread_int(&intargs.uptr)) ;
         break ;
      case intop_NROFINTOPS:
         break ;
      }
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      TEST(0 == delete_thread(&threads[i])) ;
   }
   return EINVAL ;
}

typedef struct flagarg_t   flagarg_t ;

struct flagarg_t {
   uint8_t  *  flag ;
   volatile
   uint32_t *  value ;
} ;

static volatile int s_flag_dummy = 0 ;

static int thread_setclear(flagarg_t * arg)
{
   for (uint32_t i = 0; i < 10000; ++i) {
      for (unsigned w = 0; ; ++w) {
         if (0 == atomicset_int(arg->flag)) break ;
         if (w == 10) {
            w = 0 ;
            yield_thread() ;
         }
      }
      uint32_t val = *arg->value ;
      s_flag_dummy += 1000 ;
      s_flag_dummy /= 31 ;
      *arg->value = val + 1 ;
      atomicclear_int(arg->flag) ;
   }

   return 0 ;
}

static int test_setclear(void)
{
   thread_t *  threads[8] = { 0 } ;
   flagarg_t   arg[8] ;
   uint8_t     flag ;
   uint8_t     oldflag ;

   // TEST atomicset_int: single thread
   flag = 0 ;
   TEST(0 == atomicset_int(&flag)) ;
   TEST(0 != flag) ;
   oldflag = flag ;
   for (unsigned i = 0; i < 10; ++i) {
      TEST(oldflag == atomicset_int(&flag)) ;
      TEST(oldflag == flag) ;
   }

   // TEST atomicclear_int: single thread
   TEST(0 != oldflag) ;
   for (unsigned i = 0; i < 10; ++i) {
      atomicclear_int(&flag) ;
      TEST(0 == flag) ;
      atomicclear_int(&flag) ;
      TEST(0 == flag) ;
      flag = oldflag ;
   }

   // TEST atomicset_int, atomicclear_int: multi thread
   uint32_t value = 0 ;
   flag = 0 ;
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      arg[i].flag  = &flag ;
      arg[i].value = &value ;
      TEST(0 == newgeneric_thread(&threads[i], &thread_setclear, &arg[i])) ;
   }
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      TEST(0 == delete_thread(&threads[i])) ;
   }
   TEST(atomicread_int(&flag)  == 0) ;
   TEST(atomicread_int(&value) == lengthof(threads)*10000) ;

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      TEST(0 == delete_thread(&threads[i])) ;
   }
   return EINVAL ;
}

int unittest_math_int_atomic()
{
   if (test_readwrite())      goto ONABORT ;
   if (test_addsubswap())     goto ONABORT ;
   if (test_setclear())       goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
