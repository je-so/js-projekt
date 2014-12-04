/* title: MemoryBlockVector impl

   Implements <MemoryBlockVector>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/memory/memvec.h
    Header file <MemoryBlockVector>.

   file: C-kern/memory/memvec.c
    Implementation file <MemoryBlockVector impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/memvec.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   memvec_t     vec = memvec_FREE;
   memvec_T(33) vec33 = memvec_FREE;

   // TEST memvec_FREE
   TEST(0 == vec.size);
   TEST(0 == vec.vec[0].addr);
   TEST(0 == vec.vec[0].size);
   TEST(0 == vec33.size);
   TEST(0 == vec33.vec[0].addr);
   TEST(0 == vec33.vec[0].size);

   // TEST init_memvec
   init_memvec(&vec);
   TEST(1 == vec.size);
   init_memvec(&vec33);
   TEST(33 == vec33.size);

   return 0;
ONERR:
   return EINVAL;
}

static int test_generic(void)
{
   struct {
      size_t      size;
      memblock_t  vec[99];
      int xxx_unused_xxx; // test that extra fields are ignored genericcast_memblocklist
   }            vec99;
   memvec_T(10) vec10 = memvec_FREE;
   memvec_T(25) vec25 = memvec_FREE;

   // TEST memvec_T: parameter defines array size
   static_assert(10 == lengthof(vec10.vec), "");
   static_assert(25 == lengthof(vec25.vec), "");

   // TEST memvec_DECLARE: type has correct size
   static_assert(sizeof(vec10) == 10*sizeof(memblock_t) + sizeof(size_t), "");
   static_assert(sizeof(vec25) == 25*sizeof(memblock_t) + sizeof(size_t), "");

   // TEST memvec_DECLARE: all fields are available
   TEST(0 == vec10.size);
   TEST(0 == vec25.size);
   TEST(0 == vec10.vec[0].addr);
   TEST(0 == vec25.vec[0].addr);
   TEST(0 == vec10.vec[0].size);
   TEST(0 == vec25.vec[0].size);

   // TEST cast_memvec
   TEST((memvec_t*)&vec10 == cast_memvec(&vec10));
   TEST((memvec_t*)&vec25 == cast_memvec(&vec25));
   TEST((memvec_t*)&vec99 == cast_memvec(&vec99));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_memory_memvec()
{
   if (test_initfree())    goto ONERR;
   if (test_generic())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
