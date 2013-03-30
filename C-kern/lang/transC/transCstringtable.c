/* title: TransC-StringTable impl

   Implements <TransC-StringTable>.

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

   file: C-kern/api/lang/transC/transCstringtable.h
    Header file <TransC-StringTable>.

   file: C-kern/lang/transC/transCstringtable.c
    Implementation file <TransC-StringTable impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/lang/transC/transCstringtable.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/string/stringstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/ds/foreach.h"
#endif


/* struct: transCstringtable_page_t
 * Header of memory page which stores <transCstringtable_entry_t>.
 * The header of a memory page is stored within the page itself at offset 0. */
struct transCstringtable_page_t {
   slist_node_t *       next ;
   vmpage_t             vmblock ;
} ;

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_transCstringtablepage_error
 * Simulates <init_vmpage> error in <new_transCstringtablepage>. */
test_errortimer_t          s_transCstringtablepage_error = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

/* function: new_transCstringtablepage
 * Allocates a new memory page. */
int new_transCstringtablepage(/*out*/transCstringtable_page_t ** page)
{
   int err ;
   vmpage_t                   vmblock ;
   transCstringtable_page_t * newpage ;

   static_assert(sizeof(transCstringtable_page_t) <= 128, "pagesize_vm() >= 256") ;

   ONERROR_testerrortimer(&s_transCstringtablepage_error, ONABORT) ;
   err = init_vmpage(&vmblock, 1) ;
   if (err) goto ONABORT ;

   newpage = (transCstringtable_page_t *) vmblock.addr ;
   newpage->next    = 0 ;
   newpage->vmblock = vmblock ;

   *page = newpage ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

/* function: delete_transCstringtablepage
 * Frees a previously allocated memory page. */
int delete_transCstringtablepage(transCstringtable_page_t ** page)
{
   int err ;
   transCstringtable_page_t * delpage = *page ;

   if (delpage) {
      *page = 0 ;

      delpage->next  = 0 ;

      err = free_vmpage(&delpage->vmblock) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


// section transCstringtable_iterator_t

// group: lifetime

int initfirst_transcstringtableiterator(/*out*/transCstringtable_iterator_t * iter, transCstringtable_t * strtable, void * strid)
{
   int err ;
   const size_t                     pgsize = pagesize_vm() ;
   const transCstringtable_page_t * page   = (const transCstringtable_page_t*) ((uintptr_t)strid - (uintptr_t)strid % pgsize) ;

   VALIDATE_INPARAM_TEST(strtable->first && strid, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(page->next && page == (void*)page->vmblock.addr && page->vmblock.size == pgsize, ONABORT, ) ;

   transCstringtable_entry_t * entry   = strid ;
   uint16_t                    strsize = strsize_transCstringtableentry(entry) ;

   VALIDATE_INPARAM_TEST(  &entry->strdata[strsize] <= (const uint8_t*)page+pgsize
                           && 0 == isextension_transCstringtableentry(entry), ONABORT, ) ;

   iter->next = entry ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

bool next_transcstringtableiterator(transCstringtable_iterator_t * iter, /*out*/struct memblock_t * data)
{
   if (!iter->next) return false ;

   transCstringtable_entry_t * entry = iter->next ;

   *data = (memblock_t) memblock_INIT(strsize_transCstringtableentry(entry), entry->strdata) ;

   if (  entry->next
         && isextension_transCstringtableentry(entry->next)) {
      iter->next = entry->next ;
   } else {
      iter->next = 0 ;
   }

   return true ;
}


// section: transCstringtable_t

slist_IMPLEMENT(_pagelist, transCstringtable_page_t, next)

// group: lifetime

int init_transcstringtable(/*out*/transCstringtable_t * strtable)
{
   int err ;
   transCstringtable_page_t * page ;

   err = new_transCstringtablepage(&page) ;
   if (err) goto ONABORT ;

   strtable->next  = (uint8_t*) &page[1] ;
   strtable->end   = page->vmblock.addr + page->vmblock.size ;
   strtable->first = 0 ;
   strtable->prev  = &strtable->first ;
   genericcast_slist(&strtable->pagelist) ;
   initsingle_pagelist(genericcast_slist(&strtable->pagelist), page) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_transcstringtable(transCstringtable_t * strtable)
{
   int err ;

   strtable->next  = 0 ;
   strtable->end   = 0 ;
   strtable->first = 0 ;
   strtable->prev  = 0 ;

   if (! isempty_pagelist(genericcast_slist(&strtable->pagelist))) {
      transCstringtable_page_t * delpage = first_pagelist(genericcast_slist(&strtable->pagelist)) ;
      strtable->pagelist.last->next = 0 ;
      strtable->pagelist.last = 0 ;

      transCstringtable_page_t * next = next_pagelist(delpage) ;
      err = delete_transCstringtablepage(&delpage) ;
      while (next) {
         delpage = next ;
         next    = next_pagelist(next) ;
         int err2 = delete_transCstringtablepage(&delpage) ;
         if (err2) err = err2 ;
      }

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: update

int insertstring_transcstringtable(transCstringtable_t * strtable, /*out*/void ** strid, /*out*/uint8_t ** addr, uint8_t size)
{
   int err ;
   size_t freesize  = size_stringstream(genericcast_stringstream(strtable)) ;
   size_t entrysize = objectsize_transCstringtableentry(size) ;

   if (freesize < entrysize) {
      transCstringtable_page_t * page ;
      err = new_transCstringtablepage(&page) ;
      if (err) goto ONABORT ;
      strtable->next  = (uint8_t*) &page[1] ;
      strtable->end   = page->vmblock.addr + page->vmblock.size ;
      insertlast_pagelist(genericcast_slist(&strtable->pagelist), page) ;
   }

   transCstringtable_entry_t * const entry = (transCstringtable_entry_t*) strtable->next ;

   *entry = (transCstringtable_entry_t) transCstringtable_entry_INIT(size) ;

   *strtable->prev = entry ;
   strtable->prev  = &entry->next ;
   strtable->next += entrysize ;

   *strid = entry ;
   *addr  = entry->strdata ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int shrinkstring_transcstringtable(transCstringtable_t * strtable, uint8_t * endaddr)
{
   int err ;
   transCstringtable_entry_t * const entry = structof(transCstringtable_entry_t, next, strtable->prev) ;
   size_t                            size  = strsize_transCstringtableentry(entry) ;
   size_t                            newsize = (size_t) (endaddr - entry->strdata) ;

   VALIDATE_INPARAM_TEST(strtable->prev != &strtable->first, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(newsize <= size, ONABORT, ) ;

   entry->strsize = (uint16_t)(entry->strsize - size + newsize) ;
   strtable->next = (uint8_t*)entry + objectsize_transCstringtableentry(newsize) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int appendstring_transcstringtable(transCstringtable_t * strtable, /*out*/uint8_t ** addr, uint8_t size)
{
   int err ;
   transCstringtable_entry_t * entry = structof(transCstringtable_entry_t, next, strtable->prev) ;

   VALIDATE_INPARAM_TEST(strtable->prev != &strtable->first, ONABORT, ) ;

   size_t oldsize  = strsize_transCstringtableentry(entry) ;
   size_t newsize  = oldsize + (size_t) size ;
   size_t freesize = size_stringstream(genericcast_stringstream(strtable)) ;

   if (  freesize < size
         || newsize > strsizemax_transCstringtableentry()) {
      err = insertstring_transcstringtable(strtable, (void**)&entry, addr, size) ;
      if (err) goto ONABORT ;
      setextbit_transCstringtableentry(entry) ;

   } else {
      entry->strsize = (uint16_t)(entry->strsize + size) ;
      strtable->next = (uint8_t*)entry + objectsize_transCstringtableentry(newsize) ;
      *addr = &entry->strdata[oldsize] ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_entry(void)
{
   // TEST transCstringtable_entry_INIT
   for (uint16_t size = 0; size < 255; ++size) {
      transCstringtable_entry_t entry = transCstringtable_entry_INIT(size) ;
      TEST(entry.next    == 0) ;
      TEST(entry.strsize == size) ;
   }

   // TEST transCstringtable_entry_INIT_EXTENSION
   for (uint16_t size = 0; size < 255; ++size) {
      transCstringtable_entry_t entry = transCstringtable_entry_INIT_EXTENSION(size) ;
      TEST(entry.next    == 0) ;
      TEST(entry.strsize == size + 32768) ;
   }

   // TEST isextension_transCstringtableentry, strsize_transCstringtableentry
   for (uint16_t size = 0; size < 255; ++size) {
      transCstringtable_entry_t entry = transCstringtable_entry_INIT(size) ;
      TEST(isextension_transCstringtableentry(&entry) == 0) ;
      TEST(strsize_transCstringtableentry(&entry)     == size) ;
      entry.strsize = (uint16_t) (size + 32768) ;
      TEST(isextension_transCstringtableentry(&entry) == 32768) ;
      TEST(strsize_transCstringtableentry(&entry)     == size) ;
   }

   // TEST objectsize_transCstringtableentry
   for (uint16_t i = 0; i <= 255; ++i) {
      size_t objsize = objectsize_transCstringtableentry(i) ;
      TEST(0 == objsize % KONFIG_MEMALIGN) ;
   }

   // TEST strsizemax_transCstringtableentry
   TEST(32767 == strsizemax_transCstringtableentry()) ;

   // TEST setextbit_transCstringtableentry
   for (uint16_t size = 0; size < 255; ++size) {
      transCstringtable_entry_t entry = transCstringtable_entry_INIT(size) ;
      TEST(entry.strsize == size) ;
      setextbit_transCstringtableentry(&entry) ;
      TEST(entry.strsize == 32768 + size) ;
      entry.strsize = (uint16_t) (32767 - size) ;
      setextbit_transCstringtableentry(&entry) ;
      TEST(entry.strsize == 65535 - size) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   transCstringtable_t strtable = transCstringtable_INIT_FREEABLE ;

   // TEST transCstringtable_INIT_FREEABLE
   TEST(strtable.next  == 0) ;
   TEST(strtable.end   == 0) ;
   TEST(strtable.first == 0) ;
   TEST(strtable.prev  == 0) ;
   TEST(strtable.pagelist.last  == 0) ;

   // TEST init_transcstringtable, free_transcstringtable: one page
   TEST(0 == init_transcstringtable(&strtable)) ;
   transCstringtable_page_t * page = (transCstringtable_page_t*)strtable.pagelist.last ;
   TEST(strtable.next  == (uint8_t*)&page[1]) ;
   TEST(strtable.end   == pagesize_vm() + (uint8_t*)strtable.pagelist.last) ;
   TEST(strtable.first == 0) ;
   TEST(strtable.prev  == &strtable.first) ;
   TEST(strtable.pagelist.last != 0) ;
   TEST(page->next  == strtable.pagelist.last) ;
   TEST(page->vmblock.addr == (void*)strtable.pagelist.last) ;
   TEST(page->vmblock.size == pagesize_vm()) ;
   TEST(0 == free_transcstringtable(&strtable)) ;
   TEST(strtable.next  == 0) ;
   TEST(strtable.end   == 0) ;
   TEST(strtable.first == 0) ;
   TEST(strtable.prev  == 0) ;
   TEST(strtable.pagelist.last  == 0) ;
   TEST(0 == free_transcstringtable(&strtable)) ;
   TEST(strtable.next  == 0) ;
   TEST(strtable.end   == 0) ;
   TEST(strtable.first == 0) ;
   TEST(strtable.prev  == 0) ;
   TEST(strtable.pagelist.last  == 0) ;

   // TEST free_transcstringtable: many pages
   TEST(0 == init_transcstringtable(&strtable)) ;
   page = (transCstringtable_page_t*)strtable.pagelist.last ;
   for (int i = 0; i < 10; ++i) {
      void *      strid ;
      uint8_t *   addr ;
      strtable.next = strtable.end ;
      TEST(0 == insertstring_transcstringtable(&strtable, &strid, &addr, 255)) ;
      TEST(strtable.pagelist.last != 0) ;
      TEST(strtable.pagelist.last != (void*)page) ;
      TEST(strtable.pagelist.last == (void*)page->next) ;
      page = (transCstringtable_page_t*)strtable.pagelist.last ;
   }
   TEST(0 == free_transcstringtable(&strtable)) ;
   TEST(strtable.next  == 0) ;
   TEST(strtable.end   == 0) ;
   TEST(strtable.first == 0) ;
   TEST(strtable.prev  == 0) ;
   TEST(strtable.pagelist.last  == 0) ;
   TEST(0 == free_transcstringtable(&strtable)) ;
   TEST(strtable.next  == 0) ;
   TEST(strtable.end   == 0) ;
   TEST(strtable.first == 0) ;
   TEST(strtable.prev  == 0) ;
   TEST(strtable.pagelist.last  == 0) ;

   return 0 ;
ONABORT:
   free_transcstringtable(&strtable) ;
   return EINVAL ;
}

static uint8_t * alignaddr(uint8_t * addr)
{
   return (uint8_t*) ((((uintptr_t)addr + KONFIG_MEMALIGN-1) / KONFIG_MEMALIGN) * KONFIG_MEMALIGN) ;
}

static int test_update(void)
{
   transCstringtable_t         strtable = transCstringtable_INIT_FREEABLE ;
   uint8_t *                   addr ;
   transCstringtable_entry_t * entry ;
   transCstringtable_page_t *  page ;

   TEST(0 == init_transcstringtable(&strtable)) ;
   page = (transCstringtable_page_t*)strtable.pagelist.last ;
   for (ssize_t i = 0; i <= strtable.end - strtable.next && i <= 255; i += 1) {
      transCstringtable_t  oldstrtable = strtable ;

      // TEST insertstring_transcstringtable: first element
      TEST(0 == insertstring_transcstringtable(&strtable, (void**)&entry, &addr, (uint8_t)i)) ;
      TEST(strtable.first == entry) ;
      TEST(strtable.prev  == &entry->next) ;
      TEST(entry == (void*)&page[1]) ;
      TEST(i     == strsize_transCstringtableentry(entry)) ;
      TEST(0     == isextension_transCstringtableentry(entry)) ;
      TEST(addr  == entry->strdata) ;
      TEST(strtable.next == alignaddr(&entry->strdata[i])) ;

      // TEST shrinkstring_transcstringtable: no extension bit
      for (ssize_t i2 = i; i2 >= 0; --i2) {
         TEST(0  == shrinkstring_transcstringtable(&strtable, addr+i2)) ;
         TEST(i2 == strsize_transCstringtableentry(entry)) ;
         TEST(0 == isextension_transCstringtableentry(entry)) ;
         TEST(strtable.next == alignaddr(&entry->strdata[i2])) ;
      }

      // TEST appendstring_transcstringtable: no extension bit
      TEST(0 == appendstring_transcstringtable(&strtable, &addr, (uint8_t)(i/2))) ;
      TEST(i/2  == strsize_transCstringtableentry(entry)) ;
      TEST(0    == isextension_transCstringtableentry(entry)) ;
      TEST(addr == entry->strdata) ;
      TEST(strtable.next == alignaddr(&entry->strdata[i/2])) ;

      // TEST appendstring_transcstringtable: extension bit
      setextbit_transCstringtableentry(entry) ;
      TEST(0 == appendstring_transcstringtable(&strtable, &addr, (uint8_t)(i-i/2))) ;
      TEST(i    == strsize_transCstringtableentry(entry)) ;
      TEST(0    != isextension_transCstringtableentry(entry)) ;
      TEST(addr == &entry->strdata[i/2])
      TEST(strtable.next == alignaddr(&entry->strdata[i])) ;

      // TEST shrinkstring_transcstringtable: extension bit
      for (ssize_t i2 = i; i2 >= 0; --i2) {
         TEST(0  == shrinkstring_transcstringtable(&strtable, entry->strdata+i2)) ;
         TEST(i2 == strsize_transCstringtableentry(entry)) ;
         TEST(0  != isextension_transCstringtableentry(entry)) ;
         TEST(strtable.next == alignaddr(&entry->strdata[i2])) ;
      }

      TEST(strtable.end   == oldstrtable.end)
      TEST(strtable.first == entry) ;
      TEST(strtable.prev  == &entry->next) ;
      TEST(strtable.pagelist.last == oldstrtable.pagelist.last) ;
      strtable = oldstrtable ;
   }
   TEST(0 == free_transcstringtable(&strtable)) ;

   // TEST insertstring_transcstringtable: multiple pages
   TEST(0 == init_transcstringtable(&strtable)) ;
   transCstringtable_page_t * pages[16] = { 0 } ;
   uint8_t strsize64 = 0 ;
   while (objectsize_transCstringtableentry(strsize64) < 64/*use 64 bytes per entry*/) {
      ++ strsize64 ;
   }
   size_t nrentryperpage = (size_t) (strtable.end - strtable.next) / 64 ;
   uint8_t strsize64_2 = strsize64 ;
   for ( size_t unused = (size_t)(strtable.end - strtable.next) - 64 * nrentryperpage;
         objectsize_transCstringtableentry(strsize64_2) < 64 + unused; ) {
      ++ strsize64_2 ;
   }
   for (size_t nrpage = 0; nrpage < 16; ++nrpage) {
      for (size_t i = 0; i < nrentryperpage; ++i) {
         TEST(0 == insertstring_transcstringtable(&strtable, (void**)&entry, &addr, (uint8_t) ((i < nrentryperpage-1) ? strsize64 : strsize64_2))) ;
         if (!i) pages[nrpage] = (transCstringtable_page_t*)strtable.pagelist.last ;
         TEST(entry == (void*)(i*64+(uint8_t*)&pages[nrpage][1])) ;
         TEST(addr  == entry->strdata) ;
      }
      TEST(strtable.next == strtable.end) ;
      TEST(pages[nrpage] == (transCstringtable_page_t*)strtable.pagelist.last) ;
      for (size_t nrpage2 = 0; nrpage2 < nrpage; ++nrpage2) {
         TEST(pages[nrpage2] != pages[nrpage]) ;
      }
   }
   // list of pages
   page = (transCstringtable_page_t*)strtable.pagelist.last ;
   for (size_t nrpage = 0; nrpage <= 15; ++nrpage) {
      page = next_pagelist(page) ;
      TEST(page == pages[nrpage]);
   }
   // list of entries
   entry = strtable.first ;
   for (size_t nrpage = 0; nrpage < 16; ++nrpage) {
      for (size_t i = 0; i < nrentryperpage; ++i) {
         TEST(entry == (void*)(i*64+(uint8_t*)&pages[nrpage][1])) ;
         TEST(isextension_transCstringtableentry(entry) == 0) ;
         TEST(strsize_transCstringtableentry(entry)     == ((i < nrentryperpage-1) ? strsize64 : strsize64_2)) ;
         entry = entry->next ;
      }
   }
   TEST(0 == entry) ;
   TEST(0 == free_transcstringtable(&strtable)) ;

   // TEST appendstring_transcstringtable: multiple pages
   TEST(strsizemax_transCstringtableentry() >= pagesize_vm()) ;
   TEST(0 == init_transcstringtable(&strtable)) ;
   for (size_t nrpage = 0; nrpage < 16; ++nrpage) {
      entry = 0 ;
      for (size_t i = 0; i < nrentryperpage; ++i) {
         if (!nrpage && !i) {
            TEST(0 == insertstring_transcstringtable(&strtable, (void**)&entry, &addr, strsize64)) ;
         } else {
            if (entry) entry->strsize = (uint16_t) (isextension_transCstringtableentry(entry) + strsizemax_transCstringtableentry()) ;
            TEST(0 == appendstring_transcstringtable(&strtable, &addr, (uint8_t) ((i < nrentryperpage-1) ? strsize64 : strsize64_2))) ;
            if (entry) entry->strsize = (uint16_t) (isextension_transCstringtableentry(entry) + strsize64) ;
         }
         if (!i) pages[nrpage] = (transCstringtable_page_t*)strtable.pagelist.last ;
         entry = (void*)(i*64+(uint8_t*)&pages[nrpage][1]) ;
         TEST(addr == entry->strdata) ;
      }
      TEST(strtable.next == strtable.end) ;
      TEST(pages[nrpage] == (transCstringtable_page_t*)strtable.pagelist.last) ;
      for (size_t nrpage2 = 0; nrpage2 < nrpage; ++nrpage2) {
         TEST(pages[nrpage2] != pages[nrpage]) ;
      }
   }
   // list of pages
   page = (transCstringtable_page_t*)strtable.pagelist.last ;
   for (size_t nrpage = 0; nrpage <= 15; ++nrpage) {
      page = next_pagelist(page) ;
      TEST(page == pages[nrpage]);
   }
   // list of entries
   entry = strtable.first ;
   for (size_t nrpage = 0; nrpage < 16; ++nrpage) {
      for (size_t i = 0; i < nrentryperpage; ++i) {
         TEST(entry == (void*)(i*64+(uint8_t*)&pages[nrpage][1])) ;
         TEST(isextension_transCstringtableentry(entry) == (i || nrpage ? 32768 : 0)) ;
         TEST(strsize_transCstringtableentry(entry)     == ((i < nrentryperpage-1) ? strsize64 : strsize64_2)) ;
         entry = entry->next ;
      }
   }
   TEST(0 == entry) ;
   TEST(0 == free_transcstringtable(&strtable)) ;

   // TEST init_transcstringtable: ENOMEM
   transCstringtable_t oldstrtable ;
   MEMCOPY(&oldstrtable, &strtable) ;
   init_testerrortimer(&s_transCstringtablepage_error, 1, ENOMEM) ;
   TEST(ENOMEM == init_transcstringtable(&strtable)) ;
   TEST(0 == memcmp(&oldstrtable, &strtable, sizeof(strtable))) ;

   // TEST insertstring_transcstringtable: ENOMEM
   TEST(0 == init_transcstringtable(&strtable)) ;
   strtable.next = strtable.end ;
   MEMCOPY(&oldstrtable, &strtable) ;
   entry = 0 ;
   addr  = 0 ;
   init_testerrortimer(&s_transCstringtablepage_error, 1, ENOMEM) ;
   TEST(ENOMEM == insertstring_transcstringtable(&strtable, (void**)&entry, &addr, 1)) ;
   TEST(0 == entry) ;
   TEST(0 == addr) ;
   TEST(0 == memcmp(&oldstrtable, &strtable, sizeof(strtable))) ;
   TEST(0 == free_transcstringtable(&strtable)) ;

   // TEST appendstring_transcstringtable: ENOMEM
   TEST(0 == init_transcstringtable(&strtable)) ;
   TEST(0 == insertstring_transcstringtable(&strtable, (void**)&entry, &addr, 1)) ;
   strtable.next = strtable.end ;
   MEMCOPY(&oldstrtable, &strtable) ;
   init_testerrortimer(&s_transCstringtablepage_error, 1, ENOMEM) ;
   addr = 0 ;
   TEST(ENOMEM == appendstring_transcstringtable(&strtable, &addr, 1)) ;
   TEST(0 == addr) ;
   TEST(0 == memcmp(&oldstrtable, &strtable, sizeof(strtable))) ;
   TEST(0 == free_transcstringtable(&strtable)) ;

   // TEST appendstring_transcstringtable: EINVAL
   TEST(0 == init_transcstringtable(&strtable)) ;
   TEST(EINVAL == appendstring_transcstringtable(&strtable, &addr, 1)) ;
   TEST(0 == free_transcstringtable(&strtable)) ;

   // TEST shrinkstring_transcstringtable: EINVAL
   TEST(0 == init_transcstringtable(&strtable)) ;
   TEST(EINVAL == shrinkstring_transcstringtable(&strtable, strtable.next)) ;
   TEST(0 == insertstring_transcstringtable(&strtable, (void**)&entry, &addr, 13)) ;
   TEST(0 == insertstring_transcstringtable(&strtable, (void**)&entry, &addr, 13)) ;
   MEMCOPY(&oldstrtable, &strtable) ;
   TEST(EINVAL == shrinkstring_transcstringtable(&strtable, addr-1)) ;
   TEST(EINVAL == shrinkstring_transcstringtable(&strtable, addr+14)) ;
   TEST(0 == memcmp(&oldstrtable, &strtable, sizeof(strtable))) ;
   TEST(0 == free_transcstringtable(&strtable)) ;

   return 0 ;
ONABORT:
   free_transcstringtable(&strtable) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   transCstringtable_t           strtable = transCstringtable_INIT_FREEABLE ;
   transCstringtable_iterator_t  iter     = transCstringtable_iterator_INIT_FREEABLE ;
   void *                        strid[100] ;
   uint8_t *                     addr ;

   // prepare
   TEST(0 == init_transcstringtable(&strtable)) ;
   for (size_t i = 0; i < lengthof(strid); ++i) {
      TEST(0 == insertstring_transcstringtable(&strtable, &strid[i], &addr, 1)) ;
      *addr = (uint8_t)i ;
      for (size_t i2 = 0; i2 < i; ++i2) {
         TEST(0 == appendstring_transcstringtable(&strtable, &addr, 1)) ;
         *addr = (uint8_t)i ;
      }
   }

   // TEST transCstringtable_iterator_INIT_FREEABLE
   TEST(0 == iter.next) ;

   for (size_t i = 0; i < lengthof(strid); ++i) {

      // TEST initfirst_transcstringtableiterator
      TEST(0 == initfirst_transcstringtableiterator(&iter, &strtable, strid[i])) ;
      TEST(strid[i] == iter.next) ;

      // TEST next_transcstringtableiterator
      size_t size = 0 ;
      {
         memblock_t data = memblock_INIT_FREEABLE ;
         while (next_transcstringtableiterator(&iter, &data)) {
            TEST(data.addr != 0) ;
            TEST(data.size >= 1) ;
            for (size_t i2 = 0; i2 < data.size; ++i2) {
               TEST(data.addr[i2] == (uint8_t)i) ;
            }
            size += data.size ;
            data = (memblock_t) memblock_INIT_FREEABLE ;
         }
         TEST(1+i == size) ;
      }

      // TEST free_transcstringtableiterator
      free_transcstringtableiterator(&iter) ;
      TEST(0 == iter.next) ;

      // TEST foreach
      size = 0 ;
      foreach (_transcstringtable, data, &strtable, strid[i]) {
         TEST(data.addr != 0) ;
         TEST(data.size >= 1) ;
         for (size_t i2 = 0; i2 < data.size; ++i2) {
            TEST(data.addr[i2] == (uint8_t)i) ;
         }
         size += data.size ;
         data = (memblock_t) memblock_INIT_FREEABLE ;
      }
      TEST(1+i == size) ;
   }

   // TEST initfirst_transcstringtableiterator: EINVAL
   TEST(0 == free_transcstringtable(&strtable)) ;
   TEST(EINVAL == initfirst_transcstringtableiterator(&iter, &strtable, strid[0])) ;

   return 0 ;
ONABORT:
   free_transcstringtable(&strtable) ;
   return EINVAL ;
}

int unittest_lang_transc_transCstringtable()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_entry())          goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_update())         goto ONABORT ;
   if (test_iterator())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
