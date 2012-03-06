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
   for(unsigned i = 0; i < 299; ++i) {
      mblock = (memblock_t) { .addr = (uint8_t*) (10 * i), .size = i+5 } ;
      TEST((uint8_t*) (10*i) == addr_memblock(&mblock)) ;
      TEST( 5 + i == size_memblock(&mblock)) ;
   }

   // TEST isfree_memblock
   mblock = (memblock_t) memblock_INIT_FREEABLE ;
   TEST(1 == isfree_memblock(&mblock)) ;
   mblock.size = 100 ;
   TEST(1 == isfree_memblock(&mblock)) ;
   mblock.addr = (void*)1 ;
   TEST(0 == isfree_memblock(&mblock)) ;
   mblock.addr = (void*) ~(uintptr_t)0 ;
   TEST(0 == isfree_memblock(&mblock)) ;
   mblock.addr = 0 ;
   TEST(1 == isfree_memblock(&mblock)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_memory_memblock()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   // store current memory mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
