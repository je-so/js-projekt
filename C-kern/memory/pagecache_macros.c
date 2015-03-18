/* title: PageCacheMacros impl

   Implements <PageCacheMacros>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
   memblock_t  page[10];

   // TEST SIZEALLOCATED_PAGECACHE
   size_t oldsize = SIZEALLOCATED_PAGECACHE();
   TEST(oldsize > 0);
   for (unsigned i = 0; i < lengthof(page); ++i) {
      TEST(0 == pagecache_maincontext().iimpl->allocpage(pagecache_maincontext().object, pagesize_4096, &page[i]));
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + (i+1)*4096);
   }
   for (unsigned i = lengthof(page); (i--) > 0; ) {
      TEST(0 == pagecache_maincontext().iimpl->releasepage(pagecache_maincontext().object, &page[i]));
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + i*4096);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_alloc(void)
{
   memblock_t page[10];

   // TEST ALLOC_PAGECACHE
   size_t oldsize = SIZEALLOCATED_PAGECACHE();
   for (unsigned i = 0; i < lengthof(page); ++i) {
      page[i] = (memblock_t) memblock_FREE;
      TEST(0 == ALLOC_PAGECACHE(pagesize_4096, &page[i]));
      TEST(page[i].addr != 0);
      TEST(page[i].size == 4096);
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + (i+1)*4096);
   }

   // TEST RELEASE_PAGECACHE
   for (unsigned i = lengthof(page); (i--) > 0; ) {
      TEST(0 == RELEASE_PAGECACHE(&page[i]));
      TEST(0 == page[i].addr);
      TEST(0 == page[i].size);
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + i*4096);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_cache(void)
{
   pagecache_impl_t pgimpl   = pagecache_impl_FREE;
   pagecache_t oldpagecache  = pagecache_FREE;
   pagecache_t testpagecache = pagecache_INIT((struct pagecache_t*)&pgimpl, interface_pagecacheimpl());
   memblock_t  page;

   // prepare
   TEST(0 == init_pagecacheimpl(&pgimpl));
   initcopy_iobj(&oldpagecache, &pagecache_maincontext());
   initcopy_iobj(&pagecache_maincontext(), &testpagecache);

   // TEST EMPTYCACHE_PAGECACHE
   pagecache_impl_t* pagecache = (pagecache_impl_t*) testpagecache.object;
   TEST(0 == pagecache->freeblocklist[pagesize_256].last);
   TEST(0 == ALLOC_PAGECACHE(pagesize_256, &page));
   TEST(0 == RELEASE_PAGECACHE(&page));
   TEST(0 != pagecache->freeblocklist[pagesize_256].last);
   TEST(0 == EMPTYCACHE_PAGECACHE());
   TEST(0 == pagecache->freeblocklist[pagesize_256].last);

   // unprepare
   initcopy_iobj(&pagecache_maincontext(), &oldpagecache);
   TEST(0 == free_pagecacheimpl(&pgimpl));

   return 0;
ONERR:
   if (oldpagecache.object) {
      initcopy_iobj(&pagecache_maincontext(), &oldpagecache);
   }
   free_pagecacheimpl(&pgimpl);
   return EINVAL;
}

int unittest_memory_pagecache_macros()
{
   if (test_query())       goto ONERR;
   if (test_alloc())       goto ONERR;
   if (test_cache())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
