/* title: MemoryBlock impl
   Implements <MemoryBlock>.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/memory/memblock.h
    Header file of <MemoryBlock>.

   file: C-kern/memory/memblock.c
    Implementation file <MemoryBlock impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/virtmemory.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   memblock_t  mblock = memblock_INIT_FREEABLE ;

   // TEST static init / memblock_INIT_FREEABLE
   TEST(0 == mblock.addr) ;
   TEST(0 == mblock.size) ;

   // TEST addr_memblock & size_memblock
   for (unsigned i = 0; i < 299; ++i) {
      mblock = (memblock_t) { .addr = (uint8_t*) (10 * i), .size = i+5 } ;
      TEST((uint8_t*) (10*i) == addr_memblock(&mblock)) ;
      TEST( 5 + i == size_memblock(&mblock)) ;
   }

   // TEST isfree_memblock
   mblock = (memblock_t) memblock_INIT_FREEABLE ;
   TEST(1 == isfree_memblock(&mblock)) ;
   mblock.size = 100 ;
   TEST(0 == isfree_memblock(&mblock)) ;
   mblock.size = 0 ;
   TEST(1 == isfree_memblock(&mblock)) ;
   mblock.addr = (void*)1 ;
   TEST(0 == isfree_memblock(&mblock)) ;
   mblock.addr = (void*) ~(uintptr_t)0 ;
   TEST(0 == isfree_memblock(&mblock)) ;
   mblock.addr = 0 ;
   TEST(1 == isfree_memblock(&mblock)) ;

   // TEST isvalid_memblock
   mblock = (memblock_t) memblock_INIT_FREEABLE ;
   TEST(1 == isvalid_memblock(&mblock)) ;
   /* null pointer with a size != 0 is invalid*/
   mblock.size = 1 ;
   TEST(0 == isvalid_memblock(&mblock)) ;
   mblock.addr = (uint8_t*) 1 ;
   TEST(1 == isvalid_memblock(&mblock)) ;
   mblock.size = 0 ;
   TEST(1 == isvalid_memblock(&mblock)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_fill(void)
{
   uint8_t     buffer[100] ;
   memblock_t  mblock = memblock_INIT(sizeof(buffer), buffer) ;

   // TEST clear_memblock
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      memset(buffer, 255, sizeof(buffer)) ;
      mblock.size = i ;
      clear_memblock(&mblock) ;
      for (unsigned i2 = 0; i2 < sizeof(buffer); ++i2) {
         if (i2 < i) {
            TEST(0 == buffer[i2]) ;
         } else {
            TEST(255 == buffer[i2]) ;
         }
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_resize(void)
{
   memblock_t  mblock = memblock_INIT_FREEABLE ;
   uint8_t     * addr ;
   size_t      size   ;

   // TEST shrink_memblock with 0
   TEST(0 == shrink_memblock(&mblock, 0)) ;
   TEST(0 == mblock.addr) ;
   TEST(0 == mblock.size) ;

   // TEST grow_memblock with 0
   TEST(ENOMEM == grow_memblock(&mblock, 0)) ;
   TEST(0 == mblock.addr) ;
   TEST(0 == mblock.size) ;

   // TEST shrink_memblock
   mblock.addr = addr = 0 ;
   mblock.size = size = 1000000 ;
   for (unsigned i = 0; size > i; ++i) {
      addr += i ;
      size -= i ;
      TEST(0 == shrink_memblock(&mblock, i)) ;
      TEST(addr == addr_memblock(&mblock)) ;
      TEST(size == size_memblock(&mblock)) ;
   }
   TEST(0 == shrink_memblock(&mblock, size)) ;
   TEST((uint8_t*)1000000 == addr_memblock(&mblock)) ;
   TEST(0 == size_memblock(&mblock)) ;

   // TEST shrink_memblock ENOMEM
   mblock.addr = 0 ;
   mblock.size = 10000 ;
   TEST(ENOMEM == shrink_memblock(&mblock, 10000 + 1)) ;

   // TEST grow_memblock
   mblock.addr = addr = (uint8_t*) 1000000 ;
   mblock.size = size = 0 ;
   for (unsigned i = 0; addr > (uint8_t*)i; ++i) {
      addr -= i ;
      size += i ;
      TEST(0 == grow_memblock(&mblock, i)) ;
      TEST(addr == addr_memblock(&mblock)) ;
      TEST(size == size_memblock(&mblock)) ;
   }
   TEST(0 == grow_memblock(&mblock, (uintptr_t)(addr) - 1)) ;
   TEST((uint8_t*)1 == addr_memblock(&mblock)) ;
   TEST(999999 == size_memblock(&mblock)) ;

   // TEST grow_memblock ENOMEM
   mblock.addr = (uint8_t*)10000 ;
   mblock.size = 0 ;
   TEST(ENOMEM == grow_memblock(&mblock, 10000)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_memory_memblock()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   // store current memory mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_fill())        goto ONABORT ;
   if (test_resize())      goto ONABORT ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
