/* title: WriteBuffer impl
   Implements <WriteBuffer>.

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

   file: C-kern/api/memory/wbuffer.h
    Header file <WriteBuffer>.

   file: C-kern/memory/wbuffer.c
    Implementation file <WriteBuffer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/test/testmm.h"
#endif


// section: wbuffer_t

// group: memory allocation strategy

#define inline_size_wbuffer(wbuf)               \
         ((size_t) (wbuf->next - (uint8_t*)wbuf->addr))

#define inline_allocatedsize_wbuffer(wbuf)      \
         ((size_t)(wbuf->end - (uint8_t*)wbuf->addr))

#define inline_reservedsize_wbuffer(wbuf)       \
         ((size_t)(wbuf->end - wbuf->next))

static int dynamicimpl_reserve_wbuffer(wbuffer_t * wbuf, size_t reserve_size)
{
   int err ;
   memblock_t memblock  = (memblock_t) memblock_INIT(inline_allocatedsize_wbuffer(wbuf), wbuf->addr) ;
   size_t     used_size = inline_size_wbuffer(wbuf) ;
   size_t     reserved  = inline_reservedsize_wbuffer(wbuf) ;

   if (reserved < reserve_size) {

      size_t   new_size   = (memblock.size + reserved >= reserve_size)
                          ? (2 * memblock.size)
                          : (memblock.size - reserved + reserve_size) ;

      if (new_size <= memblock.size) {
         err = ENOMEM ;
         new_size = (size_t) -1 ;
         TRACEOUTOFMEM_LOG(new_size) ;
         goto ONABORT ;
      }

      err = RESIZE_MM(new_size, &memblock) ;
      if (err) goto ONABORT ;

      wbuf->next = addr_memblock(&memblock) + used_size ;
      wbuf->end  = addr_memblock(&memblock) + size_memblock(&memblock) ;
      wbuf->addr = addr_memblock(&memblock) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int staticimpl_reserve_wbuffer(wbuffer_t * wbuf, size_t reserve_size)
{
   int err = ENOMEM ;
   (void) wbuf ;
   (void) reserve_size ;
   TRACEABORT_LOG(err) ;
   return err ;
}

static int dynamicimpl_free_wbuffer(wbuffer_t * wbuf)
{
   int err ;
   memblock_t memblock = (memblock_t) memblock_INIT(inline_allocatedsize_wbuffer(wbuf), wbuf->addr) ;

   err = FREE_MM(&memblock) ;
   memset(wbuf, 0, sizeof(*wbuf)) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

static int staticimpl_free_wbuffer(wbuffer_t * wbuf)
{
   memset(wbuf, 0, sizeof(*wbuf)) ;
   return 0 ;
}

static void impl_clear_wbuffer(wbuffer_t * wbuf)
{
   wbuf->next = wbuf->addr ;
}

static size_t impl_size_wbuffer(const wbuffer_t * wbuf)
{
   return inline_size_wbuffer(wbuf) ;
}

static bool impl_iterate_wbuffer(const wbuffer_t * wbuf, void ** next, /*out*/memblock_t * memblock)
{
   if (*next) return false ;

   size_t size = inline_size_wbuffer(wbuf) ;

   if (!size) return false ;

   *next     = (void*)1 ;
   *memblock = (memblock_t) memblock_INIT(size, wbuf->addr) ;

   return true ;
}

// group: variables

wbuffer_it  g_wbuffer_dynamic = { &dynamicimpl_reserve_wbuffer, &impl_size_wbuffer, &impl_iterate_wbuffer, &dynamicimpl_free_wbuffer, &impl_clear_wbuffer } ;

wbuffer_it  g_wbuffer_static  = { &staticimpl_reserve_wbuffer, &impl_size_wbuffer,  &impl_iterate_wbuffer, &staticimpl_free_wbuffer, &impl_clear_wbuffer } ;

// group: lifetime

int init_wbuffer(/*out*/wbuffer_t * wbuf, size_t preallocate_size)
{
   int err ;
   memblock_t memblock = memblock_INIT_FREEABLE ;

   if (preallocate_size) {
      err = RESIZE_MM(preallocate_size, &memblock) ;
      if (err) goto ONABORT ;
   }

   wbuf->next  = addr_memblock(&memblock) ;
   wbuf->end   = addr_memblock(&memblock) + size_memblock(&memblock) ;
   wbuf->addr  = addr_memblock(&memblock) ;
   wbuf->iimpl = &g_wbuffer_dynamic ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_variables(void)
{
   // TEST g_wbuffer_dynamic
   TEST(g_wbuffer_dynamic.reserve == &dynamicimpl_reserve_wbuffer) ;
   TEST(g_wbuffer_dynamic.size    == &impl_size_wbuffer) ;
   TEST(g_wbuffer_dynamic.iterate == &impl_iterate_wbuffer) ;
   TEST(g_wbuffer_dynamic.free    == &dynamicimpl_free_wbuffer) ;
   TEST(g_wbuffer_dynamic.clear   == &impl_clear_wbuffer) ;

   // TEST g_wbuffer_static
   TEST(g_wbuffer_static.reserve == &staticimpl_reserve_wbuffer) ;
   TEST(g_wbuffer_static.size    == &impl_size_wbuffer) ;
   TEST(g_wbuffer_static.iterate == &impl_iterate_wbuffer) ;
   TEST(g_wbuffer_static.free    == &staticimpl_free_wbuffer) ;
   TEST(g_wbuffer_static.clear   == &impl_clear_wbuffer) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   uint8_t     buffer[1000] = { 0 } ;
   wbuffer_t   wbuf  = wbuffer_INIT_FREEABLE ;
   wbuffer_t   wfree = wbuffer_INIT_FREEABLE ;

   // TEST wbuffer_INIT_FREEABLE
   TEST(0 == wbuf.next) ;
   TEST(0 == wbuf.end) ;
   TEST(0 == wbuf.addr) ;
   TEST(0 == wbuf.iimpl) ;

   // TEST wbuffer_INIT_STATIC, free_wbuffer
   for (unsigned b = 0 ; b <= sizeof(buffer); ++b) {
      memset(buffer, (int)b, sizeof(buffer)) ;
      wbuf = (wbuffer_t) wbuffer_INIT_STATIC(b, buffer) ;
      TEST(wbuf.next == buffer) ;
      TEST(wbuf.end  == buffer + b) ;
      TEST(wbuf.addr == buffer) ;
      TEST(wbuf.iimpl == &g_wbuffer_static) ;
      TEST(0 == free_wbuffer(&wbuf)) ;
      TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
      TEST(0 == free_wbuffer(&wbuf)) ;
      TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
      for (unsigned i = 0; i < sizeof(buffer); ++i) {
         TEST((uint8_t)b == buffer[i]) ;
      }
   }

   // TEST wbuffer_INIT_DYNAMIC, free_wbuffer
   wbuf = (wbuffer_t) wbuffer_INIT_DYNAMIC ;
   TEST(wbuf.next == 0) ;
   TEST(wbuf.end  == 0) ;
   TEST(wbuf.addr == 0) ;
   TEST(wbuf.iimpl == &g_wbuffer_dynamic) ;
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;

   // TEST init_wbuffer, free_wbuffer
   for (unsigned b = 0 ; b <= sizeof(buffer); ++b) {
      TEST(0 == init_wbuffer(&wbuf, b)) ;
      TEST(wbuf.next == (uint8_t*)wbuf.addr) ;
      TEST(wbuf.end  == (uint8_t*)wbuf.addr + b) ;
      if (b) {
         TEST(wbuf.addr != 0) ;
      } else {
         TEST(wbuf.addr == 0) ;
      }
      TEST(wbuf.iimpl == &g_wbuffer_dynamic) ;
      TEST(0 == free_wbuffer(&wbuf)) ;
      TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
      TEST(0 == free_wbuffer(&wbuf)) ;
      TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_wbufferiterator(void)
{
   wbuffer_iterator_t iter = wbuffer_iterator_INIT_FREEABLE ;
   wbuffer_t          wbuf = wbuffer_INIT_STATIC(1000, (uint8_t*)1) ;
   memblock_t         data ;

   // TEST wbuffer_iterator_INIT_FREEABLE
   TEST(iter.next == 0) ;
   TEST(iter.wbuf == 0) ;

   // TEST init_wbufferiterator
   memset(&iter, 1, sizeof(iter)) ;
   TEST(0 == initfirst_wbufferiterator(&iter, &wbuf)) ;
   TEST(iter.next == 0) ;
   TEST(iter.wbuf == &wbuf) ;

   // TEST free_wbufferiterator
   iter.next = (void*) 1 ;
   iter.wbuf = &wbuf ;
   TEST(0 == free_wbufferiterator(&iter)) ;
   TEST(iter.next == 0) ;
   TEST(iter.wbuf == &wbuf) ;

   // TEST next_wbufferiterator
   wbuf.next = wbuf.end ;
   TEST(0 == initfirst_wbufferiterator(&iter, &wbuf)) ;
   TEST(true == next_wbufferiterator(&iter, &data)) ;
   TEST(data.addr == (void*)1) ;
   TEST(data.size == 1000) ;
   TEST(false == next_wbufferiterator(&iter, &data)) ;
   TEST(false == next_wbufferiterator(&iter, &data)) ;

   // TEST next_wbufferiterator: empty wbuffer_t
   wbuf.next = wbuf.addr ;
   TEST(0 == initfirst_wbufferiterator(&iter, &wbuf)) ;
   TEST(false == next_wbufferiterator(&iter, &data)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   uint8_t     buffer[1000] = { 0 } ;
   wbuffer_t   wbuf  = wbuffer_INIT_FREEABLE ;
   memblock_t  data ;

   // TEST isreserved_wbuffer
   TEST(false == isreserved_wbuffer(&wbuf)) ;
   for (unsigned t = 0; t < 2; ++t) {
      if (!t) wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
      else    wbuf = (wbuffer_t) wbuffer_INIT_DYNAMIC ;
      for (unsigned i = 0; i < 16; ++i) {
         wbuf.next = (void*)(i + 1) ;
         wbuf.end  = (void*)i ;
         TEST(false == isreserved_wbuffer(&wbuf)) ;
         wbuf.end = (void*)(i+2) ;
         TEST(true == isreserved_wbuffer(&wbuf)) ;
         wbuf.end = (void*)(i+1000) ;
         TEST(true == isreserved_wbuffer(&wbuf)) ;
         wbuf.next = wbuf.end ;
         TEST(false == isreserved_wbuffer(&wbuf)) ;
      }
   }

   // TEST reservedsize_wbuffer
   for (unsigned t = 0; t < 2; ++t) {
      if (!t) wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
      else    wbuf = (wbuffer_t) wbuffer_INIT_DYNAMIC ;
      for (unsigned i = 0; i < 16; ++i) {
         wbuf.next = (void*)(i + 1) ;
         wbuf.end  = (void*)i ;
         TEST((size_t)-1 == reservedsize_wbuffer(&wbuf)) ;
         wbuf.end = (void*)(i+2) ;
         TEST(1 == reservedsize_wbuffer(&wbuf)) ;
         wbuf.end = (void*)(5*i+2) ;
         TEST(4*i+1 == reservedsize_wbuffer(&wbuf)) ;
         wbuf.next = wbuf.end ;
         TEST(0 == reservedsize_wbuffer(&wbuf)) ;
      }
   }

   // TEST size_wbuffer
   for (unsigned t = 0; t < 2; ++t) {
      if (!t) wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
      else    wbuf = (wbuffer_t) wbuffer_INIT_DYNAMIC ;
      for (unsigned i = 0; i < 16; ++i) {
         wbuf.next = (void*)i ;
         wbuf.addr = (void*)(i + 1);
         TEST((size_t)-1 == size_wbuffer(&wbuf)) ;
         wbuf.next = (void*)(i+2) ;
         TEST(1 == size_wbuffer(&wbuf)) ;
         wbuf.next = (void*)(5*i+2) ;
         TEST(4*i+1 == size_wbuffer(&wbuf)) ;
         wbuf.next = wbuf.addr ;
         TEST(0 == size_wbuffer(&wbuf)) ;
      }
   }

   // TEST firstmemblock_wbuffer
   for (unsigned t = 0; t < 2; ++t) {
      if (!t) wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
      else    wbuf = (wbuffer_t) wbuffer_INIT_DYNAMIC ;
      for (unsigned i = 0; i < 16; ++i) {
         wbuf.next = (void*)i ;
         wbuf.addr = (void*)(i + 1) ;
         TEST(0 == firstmemblock_wbuffer(&wbuf, &data)) ;
         TEST(data.addr == (void*)(i + 1)) ;
         TEST(data.size == (size_t)-1) ;
         wbuf.next = (void*)(i+2) ;
         TEST(0 == firstmemblock_wbuffer(&wbuf, &data)) ;
         TEST(data.addr == (void*)(i + 1)) ;
         TEST(data.size == 1) ;
         wbuf.next = (void*)(5*i+2) ;
         TEST(0 == firstmemblock_wbuffer(&wbuf, &data)) ;
         TEST(data.addr == (void*)(i + 1)) ;
         TEST(data.size == 4*i+1) ;
         wbuf.next = wbuf.addr ;
         TEST(ENODATA == firstmemblock_wbuffer(&wbuf, &data)) ;
         TEST(data.addr == (void*)(i + 1)) ; // not changed
         TEST(data.size == 4*i+1) ;          // not changed
      }
   }

   // TEST foreach
   for (unsigned t = 0; t < 2; ++t) {
      if (!t) wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
      else    wbuf = (wbuffer_t) wbuffer_INIT_DYNAMIC ;
      for (unsigned i = 0; i < 16; ++i) {
         unsigned count = 0 ;
         wbuf.next = (void*)i ;
         wbuf.addr = (void*)(i + 1) ;
         foreach (_wbuffer, memblock, &wbuf) {
            ++ count ;
            TEST(memblock.addr == (void*)(i + 1)) ;
            TEST(memblock.size == (size_t)-1) ;
         }
         TEST(count == 1) ;
         wbuf.next = (void*)(i+2) ;
         foreach (_wbuffer, memblock, &wbuf) {
            ++ count ;
            TEST(memblock.addr == (void*)(i + 1)) ;
            TEST(memblock.size == 1) ;
         }
         TEST(count == 2) ;
         wbuf.next = (void*)(5*i+2) ;
         foreach (_wbuffer, memblock, &wbuf) {
            ++ count ;
            TEST(memblock.addr == (void*)(i + 1)) ;
            TEST(memblock.size == 4*i+1) ;
         }
         TEST(count == 3) ;
         wbuf.next = wbuf.addr ;
         foreach (_wbuffer, memblock, &wbuf) {
            ++ count ;
            (void) memblock ;
         }
         TEST(count == 3) ;
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_change_static(void)
{
   uint8_t     buffer[1000] = { 0 } ;
   wbuffer_t   wbuf  = wbuffer_INIT_FREEABLE ;
   memblock_t  data ;

   // TEST appendbyte_wbuffer
   wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
   for (unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST(0 == appendbyte_wbuffer(&wbuf, (uint8_t)b)) ;
      TEST(wbuf.next == buffer+b+1) ;
      TEST(wbuf.end  == buffer+sizeof(buffer)) ;
      TEST(wbuf.addr == buffer) ;
   }
   TEST(ENOMEM == appendbyte_wbuffer(&wbuf, 'z')) ;
   TEST(wbuf.next == buffer+sizeof(buffer)) ;
   TEST(wbuf.end  == buffer+sizeof(buffer)) ;
   TEST(wbuf.addr == buffer) ;
   for (unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST((uint8_t)b == buffer[b]) ;
   }

   // TEST appendbytes_wbuffer
   for (unsigned b = 0 ; b <= sizeof(buffer); ++b) {
      wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
      TEST(0 == appendbytes_wbuffer(&wbuf, b, &data.addr)) ;
      TEST(data.addr == buffer) ;
      TEST(wbuf.next == buffer+b) ;
      TEST(wbuf.end  == buffer+sizeof(buffer)) ;
      TEST(wbuf.addr == buffer) ;
      TEST(0 == appendbytes_wbuffer(&wbuf, sizeof(buffer)-b, &data.addr)) ;
      TEST(data.addr == buffer+b) ;
      TEST(wbuf.next == buffer+sizeof(buffer)) ;
      TEST(wbuf.end  == buffer+sizeof(buffer)) ;
      TEST(wbuf.addr == buffer) ;
   }
   TEST(ENOMEM == appendbytes_wbuffer(&wbuf, 1, &data.addr)) ;
   for (unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST((uint8_t)b == buffer[b]) ;
   }

   // TEST reserve_wbuffer
   for (unsigned b = 0 ; b <= sizeof(buffer); ++b) {
      wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
      TEST(0 == reserve_wbuffer(&wbuf, b, &data.addr)) ;
      TEST(data.addr == buffer) ;
      TEST(wbuf.next == buffer) ;
      TEST(wbuf.end  == buffer+sizeof(buffer)) ;
      TEST(wbuf.addr == buffer) ;
      wbuf.next = buffer + b ;
      TEST(0 == reserve_wbuffer(&wbuf, sizeof(buffer)-b, &data.addr)) ;
      TEST(data.addr == buffer+b) ;
      TEST(wbuf.next == buffer+b) ;
      TEST(wbuf.end  == buffer+sizeof(buffer)) ;
      TEST(wbuf.addr == buffer) ;
   }
   TEST(ENOMEM == reserve_wbuffer(&wbuf, 1, &data.addr)) ;
   for (unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST((uint8_t)b == buffer[b]) ;
   }

   // TEST clear_wbuffer
   for (unsigned b = 0 ; b <= sizeof(buffer); ++b) {
      wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
      TEST(0 == appendbytes_wbuffer(&wbuf, b, &data.addr)) ;
      clear_wbuffer(&wbuf) ;
      TEST(wbuf.next == buffer) ;
      TEST(wbuf.end  == buffer+sizeof(buffer)) ;
      TEST(wbuf.addr == buffer) ;
   }
   for (unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST((uint8_t)b == buffer[b]) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_change_dynamic(void)
{
   wbuffer_t   wbuf   = wbuffer_INIT_FREEABLE ;
   wbuffer_t   wfree  = wbuffer_INIT_FREEABLE ;
   memblock_t  mblock = memblock_INIT_FREEABLE ;
   uint8_t     * buffer ;

   // TEST reserve_wbuffer: memory block grows the same as argument
   for (unsigned b = 0; b < 256; ++b) {
      TEST(0 == init_wbuffer(&wbuf, 0)) ;
      TEST(wbuf.next == 0) ;
      TEST(wbuf.end  == 0) ;
      TEST(wbuf.addr == 0) ;
      TEST(0 == reserve_wbuffer(&wbuf, b, &buffer)) ;
      TEST(b == 0 || buffer != 0) ;
      TEST(wbuf.next == buffer) ;
      TEST(wbuf.end  == buffer+b) ;
      TEST(wbuf.addr == buffer) ;
      TEST(0 == reserve_wbuffer(&wbuf, 2*b+1, &buffer)) ;
      TEST(buffer != 0) ;
      TEST(wbuf.next == buffer) ;
      TEST(wbuf.end  == buffer+2*b+1) ;
      TEST(wbuf.addr == buffer) ;
      TEST(0 == free_wbuffer(&wbuf)) ;
      TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
   }

   // TEST reserve_wbuffer: memory block doubles in size
   TEST(0 == init_wbuffer(&wbuf, 1)) ;
   for (uint32_t b = 1; b < 2048*2048; b *= 2) {
      TEST(wbuf.next == wbuf.addr) ;
      TEST(wbuf.end  == wbuf.next+b) ;
      TEST(wbuf.addr != 0) ;
      memset(wbuf.addr, (uint8_t)b, b) ;
      TEST(b == reservedsize_wbuffer(&wbuf)) ;
      TEST(0 == reserve_wbuffer(&wbuf, b+1, &buffer)) ;
      TEST(buffer != 0) ;
      TEST(wbuf.next == buffer) ;
      TEST(wbuf.end  == buffer+2*b) ;
      TEST(wbuf.addr == buffer) ;
      for (uint32_t i = 0; i < b; ++i) {
         TEST((uint8_t)b == wbuf.next[i]) ;
      }
   }
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;

   // TEST reserve_wbuffer: ENOMEM
   TEST(0 == init_wbuffer(&wbuf, 1000)) ;
   TEST(ENOMEM == reserve_wbuffer(&wbuf, SIZE_MAX/2, &buffer)) ;
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST appendbyte_wbuffer
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   for (uint8_t c = 'A', size = 1; c <= 'Z'; ++c) {
      TEST((c-'A')   == wbuf.next - (uint8_t*)wbuf.addr) ;
      TEST(0 == appendbyte_wbuffer(&wbuf, c)) ;
      TEST((1+c-'A') == wbuf.next - (uint8_t*)wbuf.addr) ;
      TEST(size == wbuf.end - (uint8_t*)wbuf.addr) ;
      if (wbuf.next == wbuf.end) size = (uint8_t)(size * 2) ;
   }
   for (unsigned c = 'A'; c <= 'Z'; ++c) {
      TEST(c == ((uint8_t*)wbuf.addr)[c-'A']) ;
   }
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST appendbyte_wbuffer
   TEST(0 == init_wbuffer(&wbuf, 1)) ;
   test_errortimer_t errtimer ;
   init_testerrortimer(&errtimer, 1, ENOMEM) ;
   setresizeerr_testmm(mmcontext_testmm(), &errtimer) ;
   buffer = wbuf.addr ;
   TEST(buffer != 0) ;
   TEST(0 == appendbyte_wbuffer(&wbuf, 1)) ;
   TEST(ENOMEM == appendbyte_wbuffer(&wbuf, 1)) ;
   TEST(wbuf.next == 1 + buffer) ;
   TEST(wbuf.end  == 1 + buffer) ;
   TEST(wbuf.addr == buffer) ;
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST appendbytes_wbuffer: 0 bytes, wbuf.addr != 0
   TEST(0 == init_wbuffer(&wbuf, 1)) ;
   TEST(0 == appendbytes_wbuffer(&wbuf, 0, &buffer)) ;
   TEST(buffer != 0) ;
   TEST(wbuf.next == buffer) ;
   TEST(wbuf.end  == buffer+1) ;
   TEST(wbuf.addr == buffer) ;
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST appendbytes_wbuffer: 0 bytes, wbuf.addr == 0
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   TEST(0 == appendbytes_wbuffer(&wbuf, 0, &buffer)) ;
   TEST(buffer == 0) ;
   TEST(wbuf.next == buffer) ;
   TEST(wbuf.end  == buffer) ;
   TEST(wbuf.addr == buffer) ;

   // TEST appendbytes_wbuffer
   for (size_t i = 1, size = 1, offset = 0; i <= 16; ++i) {
      if (i == 2) size = 3 ;
      else if (i > 1 && (size_t)(wbuf.end - wbuf.next) < i) size *= 2 ;
      TEST(0 == appendbytes_wbuffer(&wbuf, i, &buffer)) ;
      TEST(buffer    == offset + (uint8_t*)wbuf.addr) ;
      offset += i ;
      TEST(wbuf.next == offset + (uint8_t*)wbuf.addr) ;
      TEST(wbuf.end  == size + (uint8_t*)wbuf.addr) ;
      TEST(wbuf.addr != 0) ;
      memset(buffer, (int)i, i) ;
   }
   for (size_t i = 1, offset = 0; i <= 16; ++i) {
      for (size_t c = offset; c < offset+i; ++c) {
         TEST(i == ((uint8_t*)wbuf.addr)[c]) ;
      }
      offset += i ;
   }
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST appendbytes_wbuffer: returns same buffer pointer as reserve_wbuffer
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   for (size_t i = 1, size = 1, offset = 0; i <= 16; ++i) {
      if (i == 2) size = 3 ;
      else if (i > 1 && (size_t)(wbuf.end - wbuf.next) < i) size *= 2 ;
      TEST(0 == reserve_wbuffer(&wbuf, i, &buffer)) ;
      TEST(buffer    == offset + (uint8_t*)wbuf.addr) ;
      TEST(wbuf.next == offset + (uint8_t*)wbuf.addr) ;
      TEST(wbuf.end  == size + (uint8_t*)wbuf.addr) ;
      TEST(wbuf.addr != 0) ;
      TEST(0 == appendbytes_wbuffer(&wbuf, i, &buffer)) ;
      TEST(buffer    == offset + (uint8_t*)wbuf.addr) ;
      offset += i ;
      TEST(wbuf.next == offset + (uint8_t*)wbuf.addr) ;
      TEST(wbuf.end  == size + (uint8_t*)wbuf.addr) ;
      TEST(wbuf.addr != 0) ;
      memset(buffer, (int)i, i) ;
   }
   for (size_t i = 1, offset = 0; i <= 16; ++i) {
      for (size_t c = offset; c < offset+i; ++c) {
         TEST(i == ((uint8_t*)wbuf.addr)[c]) ;
      }
      offset += i ;
   }
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST appendbytes_wbuffer: ENOMEM
   TEST(0 == init_wbuffer(&wbuf, 1000)) ;
   TEST(ENOMEM == appendbytes_wbuffer(&wbuf, SIZE_MAX/2, &buffer)) ;
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST clear_wbuffer
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   for (size_t i = 1; i <= 1024; i *= 2) {
      TEST(0 == appendbytes_wbuffer(&wbuf, i, &buffer)) ;
      TEST(wbuf.next == buffer + i) ;
      TEST(wbuf.end  == buffer + i) ;
      TEST(wbuf.addr == buffer)
      memset(buffer, (uint8_t)i, i) ;
      clear_wbuffer(&wbuf) ;
      TEST(wbuf.next == buffer) ;
      TEST(wbuf.end  == buffer + i) ;
      TEST(wbuf.addr == buffer)
      for (unsigned c = 0; c < i; c++) {
         TEST((uint8_t)i == buffer[c]) ;
      }
   }
   TEST(0 == free_wbuffer(&wbuf)) ;

   return 0 ;
ONABORT:
   FREE_MM(&mblock) ;
   free_wbuffer(&wbuf) ;
   return EINVAL ;
}

typedef struct subtype_memblock_t   subtype_memblock_t ;

struct subtype_memblock_t {
   uint8_t  * addr ;
   size_t   size ;
   size_t   used ;
   subtype_memblock_t * next ;
} ;

static int s_count_subtypereserve = 0 ;
static int s_count_subtypesize    = 0 ;
static int s_count_subtypeiterate = 0 ;
static int s_count_subtypefree    = 0 ;
static int s_count_subtypeclear   = 0 ;

static int subtype_reserve(wbuffer_t * wbuf, size_t reserve_size)
{
   ++ s_count_subtypereserve ;

   if (reservedsize_wbuffer(wbuf) >= reserve_size) {
      return 0 ;
   }

   subtype_memblock_t * next = ((subtype_memblock_t*) wbuf->addr) ;
   next->used = (size_t) (wbuf->next - next->addr) ;

   next = next->next ;
   if (  next->used
         || next->size < reserve_size) {
      return ENOMEM ;
   }

   wbuf->next = next->addr ;
   wbuf->end  = next->addr + next->size ;
   wbuf->addr = next ;

   return 0 ;
}

static size_t subtype_size(const wbuffer_t * wbuf)
{
   ++ s_count_subtypesize ;

   size_t             size    = 0 ;
   subtype_memblock_t * first = ((subtype_memblock_t*) wbuf->addr) ;
   subtype_memblock_t * next  = first->next ;

   while (next != first) {
      size += next->used ;
      next = next->next ;
   }

   size += (size_t) (wbuf->next - first->addr) ;

   return size ;
}

static bool subtype_iterate(const wbuffer_t * wbuf, void ** next, /*out*/struct memblock_t * memblock)
{
   subtype_memblock_t * next2 = (subtype_memblock_t*) *next ;
   subtype_memblock_t * first = (subtype_memblock_t*) wbuf->addr ;

   first->used = (size_t) (wbuf->next - first->addr) ;

   ++ s_count_subtypeiterate ;

   if (!next2) {
      next2 = first->next ;

      while (!next2->used) {
         if (next2 == first) return false ; /*empty*/
         next2 = next2->next ;
      }

   } else if (next2 == first->next || !next2->used) {
      return false ;
   }

   *next = next2->next ;
   *memblock = (memblock_t) memblock_INIT(next2->used, next2->addr) ;

   return true ;
}

static int subtype_free(wbuffer_t * wbuf)
{
   ++ s_count_subtypefree ;

   clear_wbuffer(wbuf) ;

   memset(wbuf, 0, sizeof(*wbuf)) ;
   return 0 ;
}

static void subtype_clear(wbuffer_t * wbuf)
{
   ++ s_count_subtypeclear ;

   subtype_memblock_t * first = ((subtype_memblock_t*) wbuf->addr) ;
   subtype_memblock_t * next  = first ;

   if (first) {
      do {
         next->used = 0 ;
         next = next->next ;
      } while (next != first) ;

      wbuf->next = first->addr ;
      wbuf->end  = first->addr + first->size ;
   }
}

static int test_implstrategy(void)
{
   wbuffer_t   wbuf   = wbuffer_INIT_FREEABLE ;
   wbuffer_t   wfree  = wbuffer_INIT_FREEABLE ;
   wbuffer_it  sub_it = { &subtype_reserve, &subtype_size, &subtype_iterate, &subtype_free, &subtype_clear };
   uint8_t     buffer[64] = { 0 } ;
   memblock_t  data ;
   // smemblocks builds a list of 4 blocks of 16 byte of memory
   subtype_memblock_t smemblocks[4] = {
      { &buffer[ 0], 16, 0, &smemblocks[1] },
      { &buffer[16], 16, 0, &smemblocks[2] },
      { &buffer[32], 16, 0, &smemblocks[3] },
      { &buffer[48], 16, 0, &smemblocks[0] },
   } ;

   // TEST wbuffer_INIT_IMPL, free_wbuffer
   wbuf = (wbuffer_t) wbuffer_INIT_IMPL(smemblocks[0].size, smemblocks[0].addr, &smemblocks[0], &sub_it) ;
   TEST(wbuf.next == buffer) ;
   TEST(wbuf.end  == buffer+16) ;
   TEST(wbuf.addr == &smemblocks[0]) ;
   TEST(wbuf.iimpl == &sub_it) ;
   s_count_subtypefree = 0 ;
   free_wbuffer(&wbuf) ;
   TEST(1 == s_count_subtypefree) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wfree))) ;
   free_wbuffer(&wbuf) ;
   TEST(1 == s_count_subtypefree) ;
   wbuf.iimpl = &sub_it ;
   free_wbuffer(&wbuf) ;
   TEST(2 == s_count_subtypefree) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wfree))) ;

   // == query ==

   wbuf = (wbuffer_t) wbuffer_INIT_IMPL(smemblocks[0].size, smemblocks[0].addr, &smemblocks[0], &sub_it) ;

   // TEST isreserved_wbuffer
   wbuf.next = wbuf.end ;
   TEST(false == isreserved_wbuffer(&wbuf)) ;
   wbuf.next = buffer ;
   TEST(true == isreserved_wbuffer(&wbuf)) ;

   // TEST reservedsize_wbuffer
   wbuf.next = wbuf.end ;
   TEST(0 == reservedsize_wbuffer(&wbuf)) ;
   wbuf.next = buffer ;
   TEST(16 == reservedsize_wbuffer(&wbuf)) ;

   // TEST size_wbuffer
   wbuf.next = wbuf.end ;
   TEST(16 == size_wbuffer(&wbuf)) ;
   wbuf.next = buffer ;
   TEST(0 == size_wbuffer(&wbuf)) ;
   for (unsigned i = 0; i < lengthof(smemblocks); ++i) {
      smemblocks[i].used = (1 + i) ;
   }
   TEST(2+3+4 == size_wbuffer(&wbuf)) ; /* first block uses wbuf.next - smemblocks[0].addr */
   for (unsigned i = 0; i < lengthof(smemblocks); ++i) {
      smemblocks[i].used = 0 ;
   }

   // TEST firstmemblock_wbuffer
   wbuf.next = &buffer[5] ;
   TEST(0 == firstmemblock_wbuffer(&wbuf, &data)) ;
   TEST(data.addr == buffer) ;
   TEST(data.size == 5) ;
   smemblocks[3].used = 4 ;
   TEST(0 == firstmemblock_wbuffer(&wbuf, &data)) ;
   TEST(data.addr == &buffer[48]) ;
   TEST(data.size == 4) ;
   smemblocks[3].used = 0 ;
   wbuf.next = buffer ;

   // TEST foreach
   wbuf.next = &buffer[4] ;
   for (unsigned i = 0; i < lengthof(smemblocks); ++i) {
      smemblocks[i].used = 4-i ;
   }
   for (unsigned i = 1; i == 1; i = 0) {
      foreach (_wbuffer, memblock, &wbuf) {
         TEST(memblock.addr == &buffer[(i%4)*16]) ;
         TEST(memblock.size == 4-(i%4)) ;
         ++ i ;
      }
      TEST(i == 5) ;
      i = 1 ;
      wbuf.next = buffer ;
      foreach (_wbuffer, memblock, &wbuf) {
         TEST(memblock.addr == &buffer[(i%4)*16]) ;
         TEST(memblock.size == 4-(i%4)) ;
         ++ i ;
      }
      TEST(i == 4) ;
   }
   for (unsigned i = 0; i < lengthof(smemblocks); ++i) {
      smemblocks[i].used = 0 ;
   }

   // == change ==

   // TEST appendbyte_wbuffer
   s_count_subtypereserve = 0 ;
   for (uint8_t b = 0; b < sizeof(buffer); ++b) {
      TEST(0 == appendbyte_wbuffer(&wbuf, b)) ;
   }
   TEST(s_count_subtypereserve == lengthof(smemblocks)-1) ;
   TEST(size_wbuffer(&wbuf)    == sizeof(buffer)) ;
   TEST(ENOMEM == appendbyte_wbuffer(&wbuf, 0)) ;
   TEST(wbuf.next == wbuf.end) ;
   TEST(wbuf.end  == &buffer[lengthof(buffer)]) ;
   TEST(wbuf.addr == &smemblocks[3]) ;
   for (unsigned i = 0; i < lengthof(smemblocks); ++i) {
      TEST(16 == smemblocks[i].used) ;
   }

   // TEST clear_wbuffer
   clear_wbuffer(&wbuf) ;
   TEST(wbuf.next == smemblocks[3].addr) ;
   TEST(wbuf.end  == &buffer[lengthof(buffer)]) ;
   TEST(wbuf.addr == &smemblocks[3]) ;
   for (unsigned i = 0; i < lengthof(smemblocks); ++i) {
      TEST(0 == smemblocks[i].used) ;
   }

   // TEST reserve_wbuffer
   wbuf = (wbuffer_t) wbuffer_INIT_IMPL(smemblocks[0].size, smemblocks[0].addr, &smemblocks[0], &sub_it) ;
   for (unsigned i = 0; i <= 16; ++i) {
      TEST(0 == reserve_wbuffer(&wbuf, i, &data.addr)) ;
      TEST(data.addr == buffer) ;
      TEST(wbuf.next == buffer) ;
      TEST(wbuf.end  == buffer+16) ;
      TEST(wbuf.addr == &smemblocks[0]) ;
   }

   // TEST reserve_wbuffer: ENOMEM
   TEST(ENOMEM == reserve_wbuffer(&wbuf, 17, &data.addr))
   smemblocks[1].used = 1 ;
   wbuf.next = wbuf.end ;
   TEST(ENOMEM == reserve_wbuffer(&wbuf, 1, &data.addr))
   smemblocks[1].used = 0 ;
   wbuf.next = buffer ;

   // TEST appendbytes_wbuffer
   s_count_subtypereserve = 0 ;
   for (unsigned i = 0; i < lengthof(smemblocks); ++i) {
      TEST(0 == appendbytes_wbuffer(&wbuf, 16, &data.addr)) ;
      TEST(data.addr == &buffer[i*16]) ;
   }
   TEST(s_count_subtypereserve == lengthof(smemblocks)-1) ;

   // TEST appendbytes_wbuffer
   TEST(ENOMEM == appendbytes_wbuffer(&wbuf, 1, &data.addr)) ;
   TEST(s_count_subtypereserve == lengthof(smemblocks)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_memory_wbuffer()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_variables())         goto ONABORT ;
   if (test_initfree())          goto ONABORT ;
   if (test_wbufferiterator())   goto ONABORT ;
   if (test_query())             goto ONABORT ;
   if (test_change_static())     goto ONABORT ;
   if (test_change_dynamic())    goto ONABORT ;
   if (test_implstrategy())      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif

