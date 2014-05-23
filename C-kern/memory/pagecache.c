/* title: PageCache-Object impl

   Implements <PageCache-Object>.

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

   file: C-kern/api/memory/pagecache.h
    Header file <PageCache-Object>.

   file: C-kern/memory/pagecache.c
    Implementation file <PageCache-Object impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/pagecache.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: pagecache_t


// group: test

#ifdef KONFIG_UNITTEST

static int allocpage1_dummy(pagecache_t * pgcache, uint8_t pgsize, /*out*/struct memblock_t * page)
{
   (void)pgcache ;
   (void)pgsize ;
   (void)page;
   return 0 ;
}

static int releasepage1_dummy(pagecache_t * pgcache, struct memblock_t * page)
{
   (void)pgcache ;
   (void)page;
   return 0 ;
}

static size_t sizeallocated1_dummy(const pagecache_t * pgcache)
{
   (void)pgcache ;
   return 0 ;
}

static int allocstatic1_dummy(pagecache_t * pgcache, size_t bytesize, /*out*/struct memblock_t * memblock)
{
   (void)pgcache ;
   (void)bytesize ;
   (void)memblock ;
   return 0 ;
}

static int freestatic1_dummy(pagecache_t * pgcache, struct memblock_t * memblock)
{
   (void)pgcache ;
   (void)memblock ;
   return 0 ;
}

static size_t sizestatic1_dummy(const pagecache_t * pgcache)
{
   (void)pgcache ;
   return 0 ;
}

static int emptycache1_dummy(pagecache_t * pgcache)
{
   (void)pgcache ;
   return 0 ;
}

static int test_initfreeit(void)
{
   pagecache_it   pgcacheif = pagecache_it_FREE ;

   // TEST pagecache_it_FREE
   TEST(0 == pgcacheif.allocpage) ;
   TEST(0 == pgcacheif.releasepage) ;
   TEST(0 == pgcacheif.sizeallocated) ;

   // TEST pagecache_it_INIT
   pgcacheif = (pagecache_it) pagecache_it_INIT(   &allocpage1_dummy, &releasepage1_dummy, &sizeallocated1_dummy,
                                                   &allocstatic1_dummy, &freestatic1_dummy, &sizestatic1_dummy,
                                                   &emptycache1_dummy) ;
   TEST(pgcacheif.allocpage     == &allocpage1_dummy) ;
   TEST(pgcacheif.releasepage   == &releasepage1_dummy) ;
   TEST(pgcacheif.sizeallocated == &sizeallocated1_dummy) ;
   TEST(pgcacheif.allocstatic   == &allocstatic1_dummy) ;
   TEST(pgcacheif.freestatic    == &freestatic1_dummy) ;
   TEST(pgcacheif.sizestatic    == &sizestatic1_dummy) ;
   TEST(pgcacheif.emptycache    == &emptycache1_dummy) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   pagecache_t pgcacheobj = pagecache_FREE ;

   // TEST pagecache_FREE
   TEST(0 == pgcacheobj.object) ;
   TEST(0 == pgcacheobj.iimpl) ;

   // TEST pagecache_INIT
   pgcacheobj = (pagecache_t) pagecache_INIT((pagecache_t*)4, (pagecache_it*)5) ;
   TEST(4 == (uintptr_t)pgcacheobj.object) ;
   TEST(5 == (uintptr_t)pgcacheobj.iimpl) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   pagecache_t pgcache = pagecache_FREE ;

   // TEST isobject_pagecache
   pgcache.object = (void*)1 ;
   TEST(1 == isobject_pagecache(&pgcache)) ;
   pgcache.object = (void*)0 ;
   TEST(0 == isobject_pagecache(&pgcache)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_queryit(void)
{
   // TEST pagesizeinbytes_pagecacheit
   static_assert(6 == pagesize_NROFPAGESIZE, "tested all possible values") ;
   TEST(256   == pagesizeinbytes_pagecacheit(pagesize_256)) ;
   TEST(1024  == pagesizeinbytes_pagecacheit(pagesize_1024)) ;
   TEST(4096  == pagesizeinbytes_pagecacheit(pagesize_4096)) ;
   TEST(16384 == pagesizeinbytes_pagecacheit(pagesize_16384)) ;
   TEST(65536 == pagesizeinbytes_pagecacheit(pagesize_65536)) ;
   TEST(1048576 == pagesizeinbytes_pagecacheit(pagesize_1MB)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

struct pagecachex_t {
   unsigned       is_allocpage ;
   unsigned       is_releasepage ;
   unsigned       is_sizeallocated ;
   unsigned       is_allocstatic ;
   unsigned       is_freestatic ;
   unsigned       is_sizestatic ;
   unsigned       is_emptycache ;
   pagesize_e     pgsize ;
   struct
   memblock_t *   page ;
   size_t         bytesize ;
   struct
   memblock_t *   memblock ;
} ;

static int allocpage2_dummy(struct pagecachex_t * pgcache, uint8_t pgsize, /*out*/struct memblock_t * page)
{
   ++ pgcache->is_allocpage ;
   pgcache->pgsize = pgsize ;
   pgcache->page   = page ;
   return 0 ;
}

static int releasepage2_dummy(struct pagecachex_t * pgcache, struct memblock_t * page)
{
   ++ pgcache->is_releasepage ;
   pgcache->page = page ;
   return 0 ;
}

static size_t sizeallocated2_dummy(const struct pagecachex_t * pgcache)
{
   ++ CONST_CAST(struct pagecachex_t,pgcache)->is_sizeallocated ;
   return 0 ;
}

static int allocstatic2_dummy(struct pagecachex_t * pgcache, size_t bytesize, /*out*/struct memblock_t * memblock)
{
   ++ pgcache->is_allocstatic ;
   pgcache->bytesize = bytesize ;
   pgcache->memblock = memblock ;
   return 0 ;
}

static int freestatic2_dummy(struct pagecachex_t * pgcache, struct memblock_t * memblock)
{
   ++ pgcache->is_freestatic ;
   pgcache->memblock = memblock ;
   return 0 ;
}

static size_t sizestatic2_dummy(const struct pagecachex_t * pgcache)
{
   ++ CONST_CAST(struct pagecachex_t,pgcache)->is_sizestatic ;
   return 0 ;
}

static int emptycache2_dummy(struct pagecachex_t * pgcache)
{
   ++ pgcache->is_emptycache ;
   return 0 ;
}

// TEST pagecache_it_DECLARE
pagecache_it_DECLARE(pagecachex_it, struct pagecachex_t)

static int test_genericit(void)
{
   pagecachex_it  pgcacheif = pagecache_it_FREE ;

   // TEST pagecache_it_FREE
   TEST(0 == pgcacheif.allocpage) ;
   TEST(0 == pgcacheif.releasepage) ;
   TEST(0 == pgcacheif.sizeallocated) ;

   // TEST pagecache_it_INIT
   pgcacheif = (pagecachex_it) pagecache_it_INIT(  &allocpage2_dummy, &releasepage2_dummy, &sizeallocated2_dummy,
                                                   &allocstatic2_dummy, &freestatic2_dummy, &sizestatic2_dummy,
                                                   &emptycache2_dummy) ;
   TEST(pgcacheif.allocpage     == &allocpage2_dummy) ;
   TEST(pgcacheif.releasepage   == &releasepage2_dummy) ;
   TEST(pgcacheif.sizeallocated == &sizeallocated2_dummy) ;
   TEST(pgcacheif.allocstatic   == &allocstatic2_dummy) ;
   TEST(pgcacheif.freestatic    == &freestatic2_dummy) ;
   TEST(pgcacheif.sizestatic    == &sizestatic2_dummy) ;
   TEST(pgcacheif.emptycache    == &emptycache2_dummy) ;

   // TEST genericcast_pagecacheit
   TEST((pagecache_it*)&pgcacheif == genericcast_pagecacheit(&pgcacheif, struct pagecachex_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int test_call(void)
{
   struct
   pagecachex_t   obj   = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } ;
   pagecachex_it  iimpl = (pagecachex_it) pagecache_it_INIT(
                           &allocpage2_dummy, &releasepage2_dummy, &sizeallocated2_dummy,
                           &allocstatic2_dummy, &freestatic2_dummy, &sizestatic2_dummy,
                           &emptycache2_dummy
                        ) ;
   pagecache_t    pgcache = pagecache_INIT((pagecache_t*)&obj, genericcast_pagecacheit(&iimpl, struct pagecachex_t)) ;

   // TEST allocpage_pagecache
   for (unsigned i = 0; i <= 10; ++i) {
      TEST(0 == allocpage_pagecache(pgcache, (pagesize_e)(i+2), (struct memblock_t*)(i+3))) ;
      TEST(i+1 == obj.is_allocpage) ;
      TEST(i+2 == obj.pgsize) ;
      TEST(i+3 == (uintptr_t)obj.page) ;
   }

   // TEST releasepage_pagecache
   for (unsigned  i = 0; i <= 10; ++i) {
      TEST(0 == releasepage_pagecache(pgcache, (struct memblock_t*)(i+2))) ;
      TEST(i+1 == obj.is_releasepage) ;
      TEST(i+2 == (uintptr_t)obj.page) ;
   }

   // TEST sizeallocated_pagecache
   for (unsigned  i = 0; i <= 10; ++i) {
      TEST(0 == sizeallocated_pagecache(pgcache)) ;
      TEST(i+1 == obj.is_sizeallocated) ;
   }

   // TEST allocstatic_pagecache
   for (unsigned  i = 0; i <= 10; ++i) {
      TEST(0 == allocstatic_pagecache(pgcache, (size_t)(i+2), (struct memblock_t*)(i+3))) ;
      TEST(i+1 == obj.is_allocstatic) ;
      TEST(i+2 == obj.bytesize) ;
      TEST(i+3 == (uintptr_t)obj.memblock) ;
   }

   // TEST freestatic_pagecache
   for (unsigned  i = 0; i <= 10; ++i) {
      TEST(0 == freestatic_pagecache(pgcache, (struct memblock_t*)(i+2))) ;
      TEST(i+1 == obj.is_freestatic) ;
      TEST(i+2 == (uintptr_t)obj.memblock) ;
   }

   // TEST sizestatic_pagecache
   for (unsigned  i = 0; i <= 10; ++i) {
      TEST(0 == sizestatic_pagecache(pgcache)) ;
      TEST(i+1 == obj.is_sizestatic) ;
   }

   // TEST emptycache_pagecache
   for (unsigned  i = 0; i <= 10; ++i) {
      TEST(0 == emptycache_pagecache(pgcache)) ;
      TEST(i+1 == obj.is_emptycache) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_memory_pagecache()
{
   if (test_initfreeit())     goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_queryit())        goto ONABORT ;
   if (test_genericit())      goto ONABORT ;
   if (test_call())           goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
