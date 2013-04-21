/* title: PageCacheMacros

   Define macros to access thread local object of type <pagecache_t>
   storead in <threadcontext_t>.

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
#ifndef CKERN_MEMORY_PAGECACHE_MACROS_HEADER
#define CKERN_MEMORY_PAGECACHE_MACROS_HEADER


// section: Functions

// group: alloc

/* define: ALLOC_PAGECACHE
 * Allocates a single memory page of size pgsize (see <pagesize_e>) and returns it in page.
 * See <pagecache_it.allocpage> for a description. */
#define ALLOC_PAGECACHE(pgsize, page)  \
         (pagecache_maincontext().iimpl->allocpage(pagecache_maincontext().object, (pgsize), (page)))

/* define: RELEASE_PAGECACHE
 * Releases previously allocated memory page.
 * See <pagecache_it.releasepage> for a description. */
#define RELEASE_PAGECACHE(page)  \
         (pagecache_maincontext().iimpl->releasepage(pagecache_maincontext().object, (page)))

/* define: ALLOCSTATIC_PAGECACHE
 * Allocates static block of memory and returns it in memblock.
 * See <pagecache_it.allocstatic> for a description. */
#define ALLOCSTATIC_PAGECACHE(bytesize, memblock)  \
         (pagecache_maincontext().iimpl->allocstatic(pagecache_maincontext().object, (bytesize), (memblock)))

/* define: FREESTATIC_PAGECACHE
 * Frees static block of memory and clears memblock.
 * See <pagecache_it.freestatic> for a description. */
#define FREESTATIC_PAGECACHE(memblock)  \
         (pagecache_maincontext().iimpl->freestatic(pagecache_maincontext().object, (memblock)))

// group: query

/* define: SIZEALLOCATED_PAGECACHE
 * Returns sum of size of all allocated pages.
 * See <pagecache_it.sizeallocated> for a description. */
#define SIZEALLOCATED_PAGECACHE()  \
         (pagecache_maincontext().iimpl->sizeallocated(pagecache_maincontext().object))

/* define: SIZESTATIC_PAGECACHE
 * Returns size of static allocated memory.
 * See <pagecache_it.sizestatic> for a description. */
#define SIZESTATIC_PAGECACHE()  \
         (pagecache_maincontext().iimpl->sizestatic(pagecache_maincontext().object))

// group: cache

/* define: RELEASECACHED_PAGECACHE
 * Returns unused memory blocks back to OS.
 * See <pagecache_it.releasecached> for a description. */
#define RELEASECACHED_PAGECACHE()  \
         (pagecache_maincontext().iimpl->releasecached(pagecache_maincontext().object))

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_pagecache_macros
 * Test <pagecache_t> functionality. */
int unittest_memory_pagecache_macros(void) ;
#endif


#endif
