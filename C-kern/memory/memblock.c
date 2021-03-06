/* title: MemoryBlock impl
   Implements <MemoryBlock>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/memory/memblock.h
    Header file of <MemoryBlock>.

   file: C-kern/memory/memblock.c
    Implementation file <MemoryBlock impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   memblock_t  mblock = memblock_FREE;

   // TEST static init / memblock_FREE
   TEST(0 == mblock.addr);
   TEST(0 == mblock.size);

   // TEST addr_memblock & size_memblock
   for (uintptr_t i = 0; i < 299; ++i) {
      mblock = (memblock_t) { .addr = (uint8_t*) (10 * i), .size = i+5 };
      TEST( 10*i == (uintptr_t) addr_memblock(&mblock));
      TEST( 5 + i == size_memblock(&mblock));
   }

   // TEST isfree_memblock
   mblock = (memblock_t) memblock_FREE;
   TEST(1 == isfree_memblock(&mblock));
   mblock.addr = (void*)1;
   TEST(0 == isfree_memblock(&mblock));
   mblock.size = 100;
   TEST(0 == isfree_memblock(&mblock));
   mblock.addr = 0;
   TEST(0 == isfree_memblock(&mblock));
   mblock.size = 0;
   TEST(1 == isfree_memblock(&mblock));

   // TEST isvalid_memblock
   mblock = (memblock_t) memblock_FREE;
   TEST(0 == isvalid_memblock(&mblock));
   mblock.size = 1;
   TEST(0 == isvalid_memblock(&mblock));
   mblock.addr = (uint8_t*) 1;
   TEST(1 == isvalid_memblock(&mblock));
   mblock.size = 0;
   TEST(0 == isvalid_memblock(&mblock));

   return 0;
ONERR:
   return EINVAL;
}

static int test_fill(void)
{
   uint8_t     buffer[100];
   memblock_t  mblock = memblock_INIT(sizeof(buffer), buffer);

   // TEST clear_memblock
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      memset(buffer, 255, sizeof(buffer));
      mblock.size = i;
      clear_memblock(&mblock);
      for (unsigned i2 = 0; i2 < sizeof(buffer); ++i2) {
         if (i2 < i) {
            TEST(0 == buffer[i2]);
         } else {
            TEST(255 == buffer[i2]);
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_resize(void)
{
   memblock_t  mblock = memblock_FREE;

   // TEST growleft_memblock with 0
   TEST(0 == growleft_memblock(&mblock, 0));
   TEST(isfree_memblock(&mblock));

   // TEST growleft_memblock
   for (unsigned i = 0; i <= 1000000; ++i) {
      if (i == 1000) i = 1000000-1000;
      mblock = (memblock_t) memblock_INIT(0, (uint8_t*)1000000);
      TEST(0 == growleft_memblock(&mblock, i));
      TEST(1000000-i == (uintptr_t) addr_memblock(&mblock));
      TEST(i == size_memblock(&mblock));
   }

   // TEST growleft_memblock ENOMEM
   mblock = (memblock_t) memblock_INIT(0, (uint8_t*)10000);
   TEST(ENOMEM == growleft_memblock(&mblock, 10000 + 1));
   TEST(10000 == (uintptr_t) addr_memblock(&mblock));
   TEST(0 == size_memblock(&mblock));

   // TEST growright_memblock with 0
   mblock = (memblock_t) memblock_FREE;
   TEST( 0 == growright_memblock(&mblock, 0));
   TEST( 0 == addr_memblock(&mblock));
   TEST( 0 == size_memblock(&mblock));

   // TEST growright_memblock with SIZE_MAX
   mblock = (memblock_t) memblock_FREE;
   TEST( 0 == growright_memblock(&mblock, SIZE_MAX));
   TEST( 0 == addr_memblock(&mblock));
   TEST( SIZE_MAX == size_memblock(&mblock));

   // TEST growright_memblock
   for (uintptr_t i = 0; i <= 1000000; ++i) {
      mblock = (memblock_t) memblock_INIT((i/1000), (uint8_t*)(1000000+i));
      TEST( 0 == growright_memblock(&mblock, i));
      TEST( 1000000+i == (uintptr_t) addr_memblock(&mblock));
      TEST( i +i/1000 == size_memblock(&mblock));
   }

   // TEST growright_memblock ENOMEM
   mblock = (memblock_t) memblock_INIT(65536, (uint8_t*)10000);
   TEST(ENOMEM == growright_memblock(&mblock, SIZE_MAX/*size overflows*/));
   TEST(addr_memblock(&mblock) == (uint8_t*)10000);
   TEST(size_memblock(&mblock) == 65536);
   TEST(ENOMEM == growright_memblock(&mblock, SIZE_MAX-10000-65536+1/*addr overflows*/));
   TEST(10000 == (uintptr_t) addr_memblock(&mblock));
   TEST(65536 == size_memblock(&mblock));

   // TEST shrinkleft_memblock: incr == 0
   mblock = (memblock_t) memblock_FREE;
   TEST(0 == shrinkleft_memblock(&mblock, 0));
   TEST(isfree_memblock(&mblock));

   // TEST shrinkleft_memblock
   for (uintptr_t i = 0; i < 1000000; ++i) {
      mblock = (memblock_t) memblock_INIT(1000000, (uint8_t*)(i/1000));
      TEST( 0 == shrinkleft_memblock(&mblock, i));
      TEST( i+i/1000  == (uintptr_t) addr_memblock(&mblock));
      TEST( 1000000-i == size_memblock(&mblock));
   }

   // TEST shrinkleft_memblock ENOMEM
   mblock = (memblock_t) memblock_INIT(10000, (uint8_t*)1);
   TEST(ENOMEM == shrinkleft_memblock(&mblock, 10000 + 1));
   TEST(addr_memblock(&mblock) == (uint8_t*)1);
   TEST(size_memblock(&mblock) == 10000);

   // TEST shrinkright_memblock: decr == 0
   mblock = (memblock_t) memblock_FREE;
   TEST(0 == shrinkright_memblock(&mblock, 0));
   TEST(isfree_memblock(&mblock));

   // TEST shrinkright_memblock
   for (uintptr_t i = 0; i < 1000000; ++i) {
      mblock = (memblock_t) memblock_INIT(1000000, (uint8_t*)i);
      TEST( 0 == shrinkright_memblock(&mblock, i));
      TEST( i == (uintptr_t) addr_memblock(&mblock));
      TEST( 1000000-i == size_memblock(&mblock));
   }

   // TEST shrinkright_memblock ENOMEM
   mblock = (memblock_t) memblock_INIT(10000, (uint8_t*)2);
   TEST(ENOMEM == shrinkleft_memblock(&mblock, 10000 + 1));
   TEST( 2     == (uintptr_t) addr_memblock(&mblock));
   TEST( 10000 == size_memblock(&mblock));

   return 0;
ONERR:
   return EINVAL;
}

static int test_generic(void)
{
   uint8_t     buffer[100];
   struct {
      int dummy1;
      uint8_t  * pre_addr;
      size_t   pre_size;
      int dummy2;
   }           mblock = { 0, 0, 0, 0 };
   struct {
      uint8_t  * addr;
      size_t   size;
   }           mblock2 = { 0, 0 };

   // TEST cast_memblock
   TEST((memblock_t*)&mblock.pre_addr == cast_memblock(&mblock, pre_));
   TEST((memblock_t*)&mblock2         == cast_memblock(&mblock2, ));

   // TEST cast_memblock: memblock_INIT
   *cast_memblock(&mblock, pre_) = (memblock_t) memblock_INIT(sizeof(buffer), buffer);
   *cast_memblock(&mblock2, )    = (memblock_t) memblock_INIT(sizeof(buffer), buffer);
   TEST(mblock.pre_addr == buffer)
   TEST(mblock2.addr    == buffer)
   TEST(mblock.pre_size == sizeof(buffer));
   TEST(mblock2.size    == sizeof(buffer));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_memory_memblock()
{
   if (test_initfree())    goto ONERR;
   if (test_fill())        goto ONERR;
   if (test_resize())      goto ONERR;
   if (test_generic())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
