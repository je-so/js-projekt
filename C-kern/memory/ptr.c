/* title: MemoryPointer impl

   Implements <MemoryPointer>.

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
ONABORT:
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

   // TEST lsbits_ptr: all bits 0
   ptr = 0;
   for (unsigned nrbits = 0; nrbits < bitsof(void*); ++nrbits) {
      TEST(0 == lsbits_ptr(ptr, nrbits));
   }

   // TEST lsbits_ptr: all bits 1
   ptr = (ptr_t) (uintptr_t)-1;
   for (uintptr_t nrbits = 0, mask = 0; nrbits < bitsof(void*); ++nrbits, mask <<= 1, ++mask) {
      TEST(mask == lsbits_ptr(ptr, nrbits));
   }

   // TEST lsbits_ptr
   for (unsigned off = 0; off < bitsof(void*); off += 8) {
      for (uintptr_t value = 0; value <= 255; ++value) {
         for (unsigned nrbits = off+1; nrbits <= off+8 && nrbits < bitsof(void*); ++nrbits) {
            uintptr_t mask = ~ ((uintptr_t)-1 << nrbits);
            uintptr_t sval = value << off;
            ptr = (ptr_t) sval;
            TEST((sval&mask) == lsbits_ptr(ptr, nrbits));
            ptr = (ptr_t) ~sval;
            TEST((~sval&mask) == lsbits_ptr(ptr, nrbits));
         }
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_update(void)
{
   ptr_t ptr = ptr_FREE;

   // TEST clearlsbits_ptr: nrbits = 0
   ptr = (ptr_t) (uintptr_t)-1;
   ptr = clearlsbits_ptr(ptr, 0);
   TEST(0 == ~(uintptr_t)ptr);

   // TEST clearlsbits_ptr
   for (unsigned nrbits = 1; nrbits < bitsof(void*); ++nrbits) {
      ptr = (ptr_t) (uintptr_t)-1;
      ptr = clearlsbits_ptr(ptr, nrbits);
      TEST(0 == lsbits_ptr(ptr, nrbits));
      uintptr_t P = ((uintptr_t)-1 << nrbits) & (uintptr_t)-1 >> 1;
      TEST(P == lsbits_ptr(ptr, bitsof(void*)-1));
   }

   // TEST orlsbits_ptr: nrbits = 0
   ptr = 0;
   ptr = orlsbits_ptr(ptr, 0, (uintptr_t)-1);
   TEST(0 == ptr);

   // TEST orlsbits_ptr
   for (unsigned nrbits = 1; nrbits < bitsof(void*); ++nrbits) {
      // upper bits not set
      const uintptr_t M = ~ ((uintptr_t)-1 << nrbits);
      ptr = 0;
      ptr = orlsbits_ptr(ptr, nrbits, (uintptr_t)-1);
      TEST(M == (uintptr_t)ptr);
      // upper bits not cleared
      ptr = (ptr_t)~M;
      ptr = orlsbits_ptr(ptr, nrbits, (uintptr_t)-1);
      TEST(0 == ~(uintptr_t)ptr);
      // 0 ored
      ptr = 0;
      ptr = orlsbits_ptr(ptr, nrbits, 0);
      TEST(0 == (uintptr_t)ptr);
      // 1 ored
      ptr = 0;
      ptr = orlsbits_ptr(ptr, nrbits, 1);
      TEST(1 == (uintptr_t)ptr);
      // single bit ored
      ptr = 0;
      ptr = orlsbits_ptr(ptr, bitsof(void*)-1, (uintptr_t)1 << (nrbits-1));
      TEST(((uintptr_t)1 << (nrbits-1)) == (uintptr_t)ptr);
   }

   return 0;
ONABORT:
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

   // TEST clearlsbits_ptr: returns same pointer type
   TEST(&x1 == clearlsbits_ptr(&x1, 1));
   TEST(&x2 == clearlsbits_ptr(&x2, 1));
   // gives warning:
   // TEST(&x1 == clearlsbits_ptr(&x2, 1));

   // TEST orlsbits_ptr: returns same pointer type
   TEST(&x1 == orlsbits_ptr(&x1, 1, 0));
   TEST(&x2 == orlsbits_ptr(&x2, 1, 0));
   // gives warning:
   // TEST(&x2 == orlsbits_ptr(&x1, 1, 0));

   return 0;
ONABORT:
   return EINVAL;
}

int unittest_memory_ptr()
{
   if (test_initfree())    goto ONABORT;
   if (test_query())       goto ONABORT;
   if (test_update())      goto ONABORT;
   if (test_generic())     goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
