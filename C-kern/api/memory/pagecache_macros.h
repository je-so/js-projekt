/* title: PageCacheMacros

   Define macros to access thread local object of type <pagecache_t>
   storead in <threadcontext_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/pagecache_macros.h
    Header file <PageCacheMacros>.

   file: C-kern/memory/pagecache_macros.c
    Implementation file <PageCacheMacros impl>.
*/
#ifndef CKERN_MEMORY_PAGECACHE_MACROS_HEADER
#define CKERN_MEMORY_PAGECACHE_MACROS_HEADER

#include "C-kern/api/memory/pagecache.h"


// section: Functions

// group: alloc

/* define: ALLOC_PAGECACHE
 * Allocates a single memory page of size pgsize (see <pagesize_e>) and returns it in page.
 * See <pagecache_it.allocpage> for a description. */
#define ALLOC_PAGECACHE(pgsize, page)  \
         (allocpage_pagecache(pagecache_maincontext(), (pgsize), (page)))

/* define: RELEASE_PAGECACHE
 * Releases previously allocated memory page.
 * See <pagecache_it.releasepage> for a description. */
#define RELEASE_PAGECACHE(page)  \
         (releasepage_pagecache(pagecache_maincontext(), (page)))

// group: query

/* define: SIZEALLOCATED_PAGECACHE
 * Returns sum of size of all allocated pages.
 * See <pagecache_it.sizeallocated> for a description. */
#define SIZEALLOCATED_PAGECACHE()  \
         (sizeallocated_pagecache(pagecache_maincontext()))

// group: cache

/* define: EMPTYCACHE_PAGECACHE
 * Returns unused memory blocks back to OS.
 * See <pagecache_it.emptycache> for a description. */
#define EMPTYCACHE_PAGECACHE()  \
         (emptycache_pagecache(pagecache_maincontext()))

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_pagecache_macros
 * Test <pagecache_t> functionality. */
int unittest_memory_pagecache_macros(void);
#endif


#endif
