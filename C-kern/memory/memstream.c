/* title: MemoryStream impl

   Implements <MemoryStream>.

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

   file: C-kern/api/memory/memstream.h
    Header file <MemoryStream>.

   file: C-kern/memory/memstream.c
    Implementation file <MemoryStream impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/memstream.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: memstream_t

// group: lifetime


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   memstream_t memstr = memstream_FREE ;

   // TEST memstream_FREE
   TEST(0 == memstr.next) ;
   TEST(0 == memstr.end) ;

   // TEST memstream_INIT
   memstr = (memstream_t) memstream_INIT((uint8_t*)1, (uint8_t*)3) ;
   TEST(1 == (uintptr_t)memstr.next) ;
   TEST(3 == (uintptr_t)memstr.end) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_query(void)
{
   memstream_t memstr ;
   uint8_t     buffer[100] ;

   // TEST size_memstream
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      memstr = (memstream_t) memstream_INIT(buffer, buffer+i) ;
      TEST(i == size_memstream(&memstr)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_generic(void)
{
   struct {
      int x ;
      uint8_t * next ;
      uint8_t * end ;
      int y ;
   }           gobj1 ;
   struct {
      int x ;
      uint8_t * pre_next ;
      uint8_t * pre_end ;
      int y ;
   }           gobj2 ;

   TEST((memstream_t*)&gobj1.next == genericcast_memstream(&gobj1,)) ;
   TEST((memstream_t*)&gobj2.pre_next == genericcast_memstream(&gobj2, pre_)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_memory_memstream()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_generic())        goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
