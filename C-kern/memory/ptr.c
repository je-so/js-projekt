/* title: MemoryPointer impl

   Implements <MemoryPointer>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/memory/ptr.h
    Header file <MemoryPointer>.

   file: C-kern/memory/ptr.c
    Implementation file <MemoryPointer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/ptr.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: ptr_t

// group: lifetime


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   ptr_t ptr = ptr_FREE;

   // TEST ptr_FREE
   TEST(0 == ptr);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   ptr_t ptr = ptr_FREE;

   // TEST isaligned_ptr: ptr_FREE
   for (unsigned nrbits = 1; nrbits < bitsof(void*); ++nrbits) {
      TEST(1 == isaligned_ptr(ptr, nrbits));
   }

   // TEST isaligned_ptr: shift bits to left
   for (unsigned nrbits = 1; nrbits < bitsof(void*); ++nrbits) {
      ptr = (ptr_t)(uintptr_t)-1;
      for (unsigned shift = 1; shift <= nrbits; ++shift) {
         TEST(0 == isaligned_ptr(ptr, nrbits));
         ptr = (ptr_t) ((uintptr_t)ptr << 1);
      }
      TEST(0 != ptr);
      TEST(1 == isaligned_ptr(ptr, nrbits));
      ptr = (ptr_t) ~(uintptr_t)ptr;
      TEST(0 == isaligned_ptr(ptr, nrbits));

      ptr = (ptr_t)(uintptr_t)1;
      for (unsigned shift = 1; shift <= nrbits; ++shift) {
         TEST(0 == isaligned_ptr(ptr, nrbits));
         ptr = (ptr_t) ((uintptr_t)ptr << 1);
      }
      TEST(0 != ptr);
      TEST(1 == isaligned_ptr(ptr, nrbits));
      ptr = (ptr_t) ~(uintptr_t)ptr;
      TEST(0 == isaligned_ptr(ptr, nrbits));
   }

   // TEST lobits_ptr: all bits 0
   ptr = 0;
   for (unsigned nrbits = 0; nrbits < bitsof(void*); ++nrbits) {
      TEST(0 == lobits_ptr(ptr, nrbits));
   }

   // TEST lobits_ptr: all bits 1
   ptr = (ptr_t) (uintptr_t)-1;
   for (uintptr_t nrbits = 0, mask = 0; nrbits < bitsof(void*); ++nrbits, mask <<= 1, ++mask) {
      TEST(mask == lobits_ptr(ptr, nrbits));
   }

   // TEST lobits_ptr
   for (unsigned off = 0; off < bitsof(void*); off += 8) {
      for (uintptr_t value = 0; value <= 255; ++value) {
         for (unsigned nrbits = off+1; nrbits <= off+8 && nrbits < bitsof(void*); ++nrbits) {
            uintptr_t mask = ~ ((uintptr_t)-1 << nrbits);
            uintptr_t sval = value << off;
            ptr = (ptr_t) sval;
            TEST((sval&mask) == lobits_ptr(ptr, nrbits));
            ptr = (ptr_t) ~sval;
            TEST((~sval&mask) == lobits_ptr(ptr, nrbits));
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   ptr_t ptr = ptr_FREE;

   // TEST align_ptr: nrbits = 0
   ptr = (ptr_t) (uintptr_t)-1;
   ptr = align_ptr(ptr, 0);
   TEST(0 == ~(uintptr_t)ptr);

   // TEST align_ptr
   for (unsigned nrbits = 1; nrbits < bitsof(void*); ++nrbits) {
      ptr = (ptr_t) (uintptr_t)-1;
      ptr = align_ptr(ptr, nrbits);
      TEST(0 == lobits_ptr(ptr, nrbits));
      uintptr_t P = ((uintptr_t)-1 << nrbits) & (uintptr_t)-1 >> 1;
      TEST(P == lobits_ptr(ptr, bitsof(void*)-1));
   }

   // TEST orbits_ptr: (nrbits < bitsof(void*)) /* precondition unchecked */
   ptr = 0;
   ptr = orbits_ptr(ptr, bitsof(void*) + 10, 0);
   TEST(0 == ptr);

   // TEST orbits_ptr: (value >= (1 << nrbits)) /* precondition unchecked */
   for (uintptr_t V = (uintptr_t)-1; V; V >>= 1) {
      ptr = 0;
      ptr = orbits_ptr(ptr, 0, V);
      TEST(V == (uintptr_t)ptr);
   }

   for (unsigned nrbits = 1; nrbits < bitsof(void*); ++nrbits) {
      const uintptr_t P = ((uintptr_t)-1 << nrbits);

      for (uintptr_t V = 0; V <= ~P; V = ! V ? (uintptr_t)1 << (nrbits-1) : V < ~P ? ~P : (uintptr_t)-1) {
         // TEST orbits_ptr: upper bits cleared
         ptr = 0;
         ptr = orbits_ptr(ptr, nrbits, V);
         TEST(V == (uintptr_t)ptr);

         // TEST orbits_ptr: upper bits set
         ptr = (ptr_t)P;
         ptr = orbits_ptr(ptr, nrbits, V);
         TEST((P|V) == (uintptr_t)ptr);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

struct x1_t {
   double d;
};

struct x2_t {
   double d;
};

static int test_generic(void)
{
   struct x1_t x1;
   struct x2_t x2;

   // TEST align_ptr: returns same pointer type
   TEST(&x1 == align_ptr(&x1, 1));
   TEST(&x2 == align_ptr(&x2, 1));
   // gives warning:
   // TEST(&x1 == align_ptr(&x2, 1));

   // TEST orbits_ptr: returns same pointer type
   TEST(&x1 == orbits_ptr(&x1, 1, 0));
   TEST(&x2 == orbits_ptr(&x2, 1, 0));
   // gives warning:
   // TEST(&x2 == orbits_ptr(&x1, 1, 0));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_memory_ptr()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;
   if (test_generic())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
