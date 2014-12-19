/* title: AtomicOps impl

   Implements <AtomicOps>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/atomic.h
    Header file <AtomicOps>.

   file: C-kern/memory/atomic.c
    Implementation file <AtomicOps impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

typedef struct intargs_t {
   uint32_t    u32;
   uint64_t    u64;
   uintptr_t   uptr;
   uint8_t     intop;
} intargs_t;

static int thread_readwrite(intargs_t* intargs)
{
   for (uint32_t i = 1; i; i <<= 1) {
      for (;;) {
         uint32_t val = read_atomicint(&intargs->u32);
         if (i == val) break;
         assert((i >> 1) == val);
         yield_thread();
      }
      write_atomicint(&intargs->u64, ((uint64_t)i << 32));
   }

   return 0;
}

static int test_readwrite(void)
{
   thread_t*  threads[8] = { 0 };
   intargs_t  intargs;

   // TEST read_atomicint
   intargs.u32 = 0;
   intargs.u64 = 0;
   intargs.uptr = 0;
   TEST(0 == read_atomicint(&intargs.u32));
   TEST(0 == read_atomicint(&intargs.u64));
   TEST(0 == read_atomicint(&intargs.uptr));
   for (uint32_t i = 1; i; i <<= 1) {
      TEST(i == read_atomicint(&i));
   }
   for (uint64_t i = 1; i; i <<= 1) {
      TEST(i == read_atomicint(&i));
   }
   for (uintptr_t i = 1; i; i <<= 1) {
      TEST(i == read_atomicint(&i));
   }

   // TEST write_atomicint
   write_atomicint(&intargs.u32, 0);
   write_atomicint(&intargs.u64, 0);
   write_atomicint(&intargs.uptr, 0);
   TEST(0 == read_atomicint(&intargs.u32));
   TEST(0 == read_atomicint(&intargs.u64));
   TEST(0 == read_atomicint(&intargs.uptr));
   for (uint32_t i = 1; i; i <<= 1) {
      write_atomicint(&intargs.u32, i);
      TEST(i == intargs.u32);
   }
   for (uint64_t i = 1; i; i <<= 1) {
      write_atomicint(&intargs.u64, i);
      TEST(i == intargs.u64);
   }
   for (uintptr_t i = 1; i; i <<= 1) {
      write_atomicint(&intargs.uptr, i);
      TEST(i == intargs.uptr);
   }

   // TEST read_atomicint, write_atomicint: multi thread
   intargs.u32  = 0;
   intargs.u64  = 0;
   intargs.uptr = 0;
   TEST(0 == newgeneric_thread(&threads[0], &thread_readwrite, &intargs));
   for (uint32_t i = 1, old = 0; i; old = i, i <<= 1) {
      write_atomicint(&intargs.u32, i);
      for (;;) {
         uint64_t val = read_atomicint(&intargs.u64);
         if (val == (uint64_t)i << 32) break;
         TEST(val == (uint64_t)old << 32);
         yield_thread();
      }
   }
   TEST(0 == delete_thread(&threads[0]));

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      TEST(0 == delete_thread(&threads[i]));
   }
   return EINVAL;
}

typedef enum intop_e {
   intop_add32,
   intop_add64,
   intop_addptr,
   intop_sub32,
   intop_sub64,
   intop_subptr,
   intop_cmpxchg32,
   intop_cmpxchg64,
   intop_cmpxchgptr,
   intop_clear32,
   intop_clear64,
   intop_clearptr
} intop_e;

#define intop__NROF (intop_clearptr + 1)

static int thread_atomicop(intargs_t * intargs)
{
   uint32_t u32;
   uint64_t u64;
   uintptr_t uptr;
   for (unsigned i = 0; i < 100000; ++i) {
      switch (intargs->intop) {
      case intop_add32:
         add_atomicint(&intargs->u32, 1);
         break;
      case intop_add64:
         add_atomicint(&intargs->u64, 1);
         break;
      case intop_addptr:
         add_atomicint(&intargs->uptr, 1);
         break;
      case intop_sub32:
         sub_atomicint(&intargs->u32, 1);
         break;
      case intop_sub64:
         sub_atomicint(&intargs->u64, 1);
         break;
      case intop_subptr:
         sub_atomicint(&intargs->uptr, 1);
         break;
      case intop_cmpxchg32:
         u32 = intargs->u32;
         while (u32 != cmpxchg_atomicint(&intargs->u32, u32, u32+1)) {
            u32 = intargs->u32;
         }
         break;
      case intop_cmpxchg64:
         u64 = intargs->u64;
         while (u64 != cmpxchg_atomicint(&intargs->u64, u64, u64+1)) {
            u64 = intargs->u64;
         }
         break;
      case intop_cmpxchgptr:
         uptr = intargs->uptr;
         while (uptr != cmpxchg_atomicint(&intargs->uptr, uptr, uptr+1)) {
            uptr = intargs->uptr;
         }
         break;
      case intop_clear32:
         u32 = clear_atomicint(&intargs->u32)-1;
         add_atomicint(&intargs->u32, u32);
         break;
      case intop_clear64:
         u64 = clear_atomicint(&intargs->u64)-1;
         add_atomicint(&intargs->u64, u64);
         break;
      case intop_clearptr:
         uptr = clear_atomicint(&intargs->uptr)-1;
         add_atomicint(&intargs->uptr, uptr);
         break;
      }
   }

   return 0;
}

static int test_atomicops(void)
{
   thread_t*  threads[4] = { 0 };
   intargs_t  intargs;

   // TEST add_atomicint: single thread
   intargs.u32 = 0;
   intargs.u64 = 0;
   intargs.uptr = 0;
   for (uint32_t i = 1; i; i <<= 1) {
      uint32_t o = (i-1);
      TEST(o == add_atomicint(&intargs.u32, i));
   }
   TEST(UINT32_MAX == read_atomicint(&intargs.u32));
   for (uint64_t i = 1; i; i <<= 1) {
      uint64_t o = (i-1);
      TEST(o == add_atomicint(&intargs.u64, i));
   }
   TEST(UINT64_MAX == read_atomicint(&intargs.u64));
   for (uintptr_t i = 1; i; i <<= 1) {
      uintptr_t o = (i-1);
      TEST(o == add_atomicint(&intargs.uptr, i));
   }
   TEST(UINTPTR_MAX == read_atomicint(&intargs.uptr));

   // TEST sub_atomicint: single thread
   intargs.u32 = UINT32_MAX;
   intargs.u64 = UINT64_MAX;
   intargs.uptr = UINTPTR_MAX;
   for (uint32_t i = 1; i; i <<= 1) {
      uint32_t o = UINT32_MAX - (i-1);
      TEST(o == sub_atomicint(&intargs.u32, i));
   }
   TEST(0 == read_atomicint(&intargs.u32));
   for (uint64_t i = 1; i; i <<= 1) {
      uint64_t o = UINT64_MAX - (i-1);
      TEST(o == sub_atomicint(&intargs.u64, i));
   }
   TEST(0 == read_atomicint(&intargs.u64));
   for (uintptr_t i = 1; i; i <<= 1) {
      uintptr_t o = UINTPTR_MAX - (i-1);
      TEST(o == sub_atomicint(&intargs.uptr, i));
   }
   TEST(0 == read_atomicint(&intargs.uptr));

   // TEST cmpxchg_atomicint: single thread
   intargs.u32 = 0;
   intargs.u64 = 0;
   intargs.uptr = 0;
   for (uint32_t i = 1, o = 0; i; o = i, i <<= 1) {
      TEST(o == cmpxchg_atomicint(&intargs.u32, o, i));
      TEST(i == cmpxchg_atomicint(&intargs.u32, i, i));
   }
   TEST(((uint32_t)1<<31) == read_atomicint(&intargs.u32));
   for (uint64_t i = 1, o = 0; i; o = i, i <<= 1) {
      TEST(o == cmpxchg_atomicint(&intargs.u64, o, i));
      TEST(i == cmpxchg_atomicint(&intargs.u64, i, i));
   }
   TEST(((uint64_t)1<<63) == read_atomicint(&intargs.u64));
   for (uintptr_t i = 1, o = 0; i; o = i, i <<= 1) {
      TEST(o == cmpxchg_atomicint(&intargs.uptr, o, i));
      TEST(i == cmpxchg_atomicint(&intargs.uptr, i, i));
   }
   TEST(((uintptr_t)1<<(bitsof(uintptr_t)-1)) == read_atomicint(&intargs.uptr));

   // TEST clear_atomicint: single thread
   for (uint32_t i = 1; i; i <<= 1) {
      uint32_t o = UINT32_MAX - (i-1);
      intargs.u32 = o;
      TEST(o == clear_atomicint(&intargs.u32));
      TEST(0 == intargs.u32);
   }
   for (uint64_t i = 1; i; i <<= 1) {
      uint64_t o = UINT64_MAX - (i-1);
      intargs.u64 = o;
      TEST(o == clear_atomicint(&intargs.u64));
      TEST(0 == intargs.u64);
   }
   for (uintptr_t i = 1; i; i <<= 1) {
      uintptr_t o = UINTPTR_MAX - (i-1);
      intargs.uptr = o;
      TEST(o == clear_atomicint(&intargs.uptr));
      TEST(0 == intargs.uptr);
   }

   // TEST add_atomicint, sub_atomicint, cmpxchg_atomicint, clear_atomicint: multi thread
   intargs.u32 = 0;
   intargs.u64 = 0;
   intargs.uptr = 0;
   for (intop_e intop = 0; intop < intop__NROF; ++intop) {
      intargs.intop  = intop;
      for (unsigned i = 0; i < lengthof(threads); i += 1) {
         TEST(0 == newgeneric_thread(&threads[i], &thread_atomicop, &intargs));
      }
      for (unsigned i = 0; i < lengthof(threads); i += 1) {
         TEST(0 == delete_thread(&threads[i]));
      }
      switch (intop) {
      case intop_add32:
         TEST(lengthof(threads)*100000 == read_atomicint(&intargs.u32));
         break;
      case intop_add64:
         TEST(lengthof(threads)*100000 == read_atomicint(&intargs.u64));
         break;
      case intop_addptr:
         TEST(lengthof(threads)*100000 == read_atomicint(&intargs.uptr));
         break;
      case intop_sub32:
         TEST(0 == read_atomicint(&intargs.u32));
         break;
      case intop_sub64:
         TEST(0 == read_atomicint(&intargs.u64));
         break;
      case intop_subptr:
         TEST(0 == read_atomicint(&intargs.uptr));
         break;
      case intop_cmpxchg32:
         TEST(lengthof(threads)*100000 == read_atomicint(&intargs.u32));
         break;
      case intop_cmpxchg64:
         TEST(lengthof(threads)*100000 == read_atomicint(&intargs.u64));
         break;
      case intop_cmpxchgptr:
         TEST(lengthof(threads)*100000 == read_atomicint(&intargs.uptr));
         break;
      case intop_clear32:
         TEST(0 == read_atomicint(&intargs.u32));
         break;
      case intop_clear64:
         TEST(0 == read_atomicint(&intargs.u64));
         break;
      case intop_clearptr:
         TEST(0 == read_atomicint(&intargs.uptr));
         break;
      }
   }

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      TEST(0 == delete_thread(&threads[i]));
   }
   return EINVAL;
}

typedef struct flagarg_t {
   uint8_t*  flag;
   volatile
   uint32_t* value;
} flagarg_t;

static volatile int s_flag_dummy = 0;

static int thread_setclear(flagarg_t* arg)
{
   for (uint32_t i = 0; i < 10000; ++i) {
      for (unsigned w = 0;; ++w) {
         if (0 == set_atomicflag(arg->flag)) break;
         if (w == 10) {
            w = 0;
            yield_thread();
         }
      }
      uint32_t val = *arg->value;
      s_flag_dummy += 1000;
      s_flag_dummy /= 31;
      *arg->value = val + 1;
      clear_atomicflag(arg->flag);
   }

   return 0;
}

static int test_atomicflag(void)
{
   thread_t*  threads[8] = { 0 };
   flagarg_t  arg[8];
   uint8_t    flag;
   uint8_t    oldflag;

   // TEST set_atomicflag: single thread
   flag = 0;
   TEST(0 == set_atomicflag(&flag));
   TEST(0 != flag);
   oldflag = flag;
   for (unsigned i = 0; i < 10; ++i) {
      TEST(oldflag == set_atomicflag(&flag));
      TEST(oldflag == flag);
   }

   // TEST clear_atomicflag: single thread
   TEST(0 != oldflag);
   for (unsigned i = 0; i < 10; ++i) {
      clear_atomicflag(&flag);
      TEST(0 == flag);
      clear_atomicflag(&flag);
      TEST(0 == flag);
      flag = oldflag;
   }

   // TEST set_atomicflag, clear_atomicflag: multi thread
   uint32_t value = 0;
   flag = 0;
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      arg[i].flag  = &flag;
      arg[i].value = &value;
      TEST(0 == newgeneric_thread(&threads[i], &thread_setclear, &arg[i]));
   }
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      TEST(0 == delete_thread(&threads[i]));
   }
   TEST(read_atomicint(&flag)  == 0);
   TEST(read_atomicint(&value) == lengthof(threads)*10000);

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(threads); i += 1) {
      TEST(0 == delete_thread(&threads[i]));
   }
   return EINVAL;
}

int unittest_memory_atomic()
{
   if (test_readwrite())   goto ONERR;
   if (test_atomicops())   goto ONERR;
   if (test_atomicflag())  goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
