/* title: PageCacheMacros impl

   Implements <PageCacheMacros>.

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

   file: C-kern/api/memory/pagecache_macros.h
    Header file <PageCacheMacros>.

   file: C-kern/memory/pagecache_macros.c
    Implementation file <PageCacheMacros impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/pagecache_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/test/unittest.h"
#endif


// section: pagecache_macros_t

// group: test

#ifdef KONFIG_UNITTEST

static int test_query(void)
{
   memblock_t  page[10] ;

   // TEST SIZEALLOCATED_PAGECACHE
   size_t oldsize = SIZEALLOCATED_PAGECACHE() ;
   TEST(oldsize > 0) ;
   for (unsigned i = 0; i < lengthof(page); ++i) {
      TEST(0 == pagecache_maincontext().iimpl->allocpage(pagecache_maincontext().object, pagesize_4096, &page[i])) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + (i+1)*4096) ;
   }
   for (unsigned i = lengthof(page); (i--) > 0; ) {
      TEST(0 == pagecache_maincontext().iimpl->releasepage(pagecache_maincontext().object, &page[i])) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + i*4096) ;
   }

   // TEST SIZESTATIC_PAGECACHE
   size_t sizestatic = SIZESTATIC_PAGECACHE() ;
   TEST(0 == pagecache_maincontext().iimpl->allocstatic(pagecache_maincontext().object, 128, &page[0])) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic+128) ;
   TEST(0 == pagecache_maincontext().iimpl->allocstatic(pagecache_maincontext().object, 128, &page[1])) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic+256) ;
   TEST(0 == pagecache_maincontext().iimpl->freestatic(pagecache_maincontext().object, &page[1])) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic+128) ;
   TEST(0 == pagecache_maincontext().iimpl->freestatic(pagecache_maincontext().object, &page[0])) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_alloc(void)
{
   memblock_t  page[10] ;

   // TEST ALLOC_PAGECACHE
   size_t oldsize = SIZEALLOCATED_PAGECACHE() ;
   for (unsigned i = 0; i < lengthof(page); ++i) {
      page[i] = (memblock_t) memblock_FREE ;
      TEST(0 == ALLOC_PAGECACHE(pagesize_4096, &page[i])) ;
      TEST(page[i].addr != 0) ;
      TEST(page[i].size == 4096) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + (i+1)*4096) ;
   }

   // TEST RELEASE_PAGECACHE
   for (unsigned i = lengthof(page); (i--) > 0; ) {
      TEST(0 == RELEASE_PAGECACHE(&page[i])) ;
      TEST(0 == page[i].addr) ;
      TEST(0 == page[i].size) ;
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + i*4096) ;
   }

   // TEST ALLOCSTATIC_PAGECACHE
   size_t oldstatic = SIZESTATIC_PAGECACHE() ;
   TEST(0 == ALLOCSTATIC_PAGECACHE(128, &page[0])) ;
   TEST(SIZESTATIC_PAGECACHE() == oldstatic + 128) ;
   TEST(0 == ALLOCSTATIC_PAGECACHE(128, &page[1])) ;
   TEST(SIZESTATIC_PAGECACHE() == oldstatic + 256) ;
   TEST(page[0].addr != 0) ;
   TEST(page[0].size == 128) ;
   TEST(page[1].addr == page[0].addr+128) ;
   TEST(page[1].size == 128) ;

   // TEST FREESTATIC_PAGECACHE
   TEST(0 == FREESTATIC_PAGECACHE(&page[1])) ;
   TEST(SIZESTATIC_PAGECACHE() == oldstatic + 128) ;
   TEST(0 == FREESTATIC_PAGECACHE(&page[0])) ;
   TEST(SIZESTATIC_PAGECACHE() == oldstatic) ;
   TEST(page[0].addr == 0) ;
   TEST(page[0].size == 0) ;
   TEST(page[1].addr == 0) ;
   TEST(page[1].size == 0) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_cache(void)
{
   pagecache_t    oldpagecache  = pagecache_FREE ;
   pagecache_t    testpagecache = pagecache_FREE ;
   memblock_t     page ;

   // prepare
   TEST(0 == initthread_pagecacheimpl(&testpagecache)) ;
   initcopy_iobj(&oldpagecache, &pagecache_maincontext()) ;
   initcopy_iobj(&pagecache_maincontext(), &testpagecache) ;

   // TEST EMPTYCACHE_PAGECACHE
   pagecache_impl_t * pagecache = (pagecache_impl_t*) testpagecache.object ;
   TEST(0 == pagecache->freeblocklist[pagesize_256].last) ;
   TEST(0 == ALLOC_PAGECACHE(pagesize_256, &page)) ;
   TEST(0 == RELEASE_PAGECACHE(&page)) ;
   TEST(0 != pagecache->freeblocklist[pagesize_256].last) ;
   TEST(0 == EMPTYCACHE_PAGECACHE()) ;
   TEST(0 == pagecache->freeblocklist[pagesize_256].last) ;

   // unprepare
   initcopy_iobj(&pagecache_maincontext(), &oldpagecache) ;
   TEST(0 == freethread_pagecacheimpl(&testpagecache)) ;

   return 0 ;
ONABORT:
   if (oldpagecache.object) {
      initcopy_iobj(&pagecache_maincontext(), &oldpagecache) ;
   }
   freethread_pagecacheimpl(&testpagecache) ;
   return EINVAL ;
}

int unittest_memory_pagecache_macros()
{
   if (test_query())       goto ONABORT ;
   if (test_alloc())       goto ONABORT ;
   if (test_cache())       goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
