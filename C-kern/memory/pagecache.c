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
    Implementation file <PageCache-Object Impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/pagecache.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: pagecache_t


// group: test

#ifdef KONFIG_UNITTEST

static int allocpage1_dummy(pagecache_t * pgcache, pagesize_e pgsize, /*out*/struct memblock_t * page)
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

static int test_initfreeit(void)
{
   pagecache_it   pgcacheif = pagecache_it_INIT_FREEABLE ;

   // TEST pagecache_it_INIT_FREEABLE
   TEST(0 == pgcacheif.allocpage) ;
   TEST(0 == pgcacheif.releasepage) ;
   TEST(0 == pgcacheif.sizeallocated) ;

   // TEST pagecache_it_INIT
   pgcacheif = (pagecache_it) pagecache_it_INIT(&allocpage1_dummy, &releasepage1_dummy, &sizeallocated1_dummy, &allocstatic1_dummy, &freestatic1_dummy, &sizestatic1_dummy) ;
   TEST(pgcacheif.allocpage     == &allocpage1_dummy) ;
   TEST(pgcacheif.releasepage   == &releasepage1_dummy) ;
   TEST(pgcacheif.sizeallocated == &sizeallocated1_dummy) ;
   TEST(pgcacheif.allocstatic   == &allocstatic1_dummy) ;
   TEST(pgcacheif.freestatic    == &freestatic1_dummy) ;
   TEST(pgcacheif.sizestatic    == &sizestatic1_dummy) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   pagecache_t pgcacheobj = pagecache_INIT_FREEABLE ;

   // TEST pagecache_INIT_FREEABLE
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

struct pagecachex_t ;

static int allocpage2_dummy(struct pagecachex_t * pgcache, pagesize_e pgsize, /*out*/struct memblock_t * page)
{
   (void)pgcache ;
   (void)pgsize ;
   (void)page;
   return 0 ;
}

static int releasepage2_dummy(struct pagecachex_t * pgcache, struct memblock_t * page)
{
   (void)pgcache ;
   (void)page;
   return 0 ;
}

static size_t sizeallocated2_dummy(const struct pagecachex_t * pgcache)
{
   (void)pgcache ;
   return 0 ;
}

static int allocstatic2_dummy(struct pagecachex_t * pgcache, size_t bytesize, /*out*/struct memblock_t * memblock)
{
   (void)pgcache ;
   (void)bytesize ;
   (void)memblock ;
   return 0 ;
}

static int freestatic2_dummy(struct pagecachex_t * pgcache, struct memblock_t * memblock)
{
   (void)pgcache ;
   (void)memblock ;
   return 0 ;
}

static size_t sizestatic2_dummy(const struct pagecachex_t * pgcache)
{
   (void)pgcache ;
   return 0 ;
}

// TEST pagecache_it_DECLARE
pagecache_it_DECLARE(pagecachex_it, struct pagecachex_t)

static int test_genericit(void)
{
   pagecachex_it  pgcacheif = pagecache_it_INIT_FREEABLE ;

   // TEST pagecache_it_INIT_FREEABLE
   TEST(0 == pgcacheif.allocpage) ;
   TEST(0 == pgcacheif.releasepage) ;
   TEST(0 == pgcacheif.sizeallocated) ;

   // TEST pagecache_it_INIT
   pgcacheif = (pagecachex_it) pagecache_it_INIT(&allocpage2_dummy, &releasepage2_dummy, &sizeallocated2_dummy, &allocstatic2_dummy, &freestatic2_dummy, &sizestatic2_dummy) ;
   TEST(pgcacheif.allocpage     == &allocpage2_dummy) ;
   TEST(pgcacheif.releasepage   == &releasepage2_dummy) ;
   TEST(pgcacheif.sizeallocated == &sizeallocated2_dummy) ;
   TEST(pgcacheif.allocstatic   == &allocstatic2_dummy) ;
   TEST(pgcacheif.freestatic    == &freestatic2_dummy) ;
   TEST(pgcacheif.sizestatic    == &sizestatic2_dummy) ;

   // TEST genericcast_pagecacheit
   TEST((pagecache_it*)&pgcacheif == genericcast_pagecacheit(&pgcacheif, struct pagecachex_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_memory_pagecache()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfreeit())     goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_queryit())        goto ONABORT ;
   if (test_genericit())      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
