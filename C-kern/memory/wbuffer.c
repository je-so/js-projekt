/* title: WBuffer impl
   Implements <WBuffer>.

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
    Header file <WBuffer>.

   file: C-kern/memory/wbuffer.c
    Implementation file <WBuffer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif



// section: wbuffer_t

// group: implementation

const wbuffer_it  g_wbuffer_interface = {
    &free_wbuffer
   ,&grow_wbuffer
} ;

int init_wbuffer(wbuffer_t * wbuf, size_t preallocate_size)
{
   int err ;
   uint8_t * memblock = 0 ;

   VALIDATE_INPARAM_TEST(0 == wbuf->addr, ABBRUCH, ) ;

   if (preallocate_size) {
      memblock = ((ssize_t)preallocate_size < 0) ? 0 : malloc(preallocate_size) ;
      if (!memblock) {
         err = ENOMEM ;
         LOG_OUTOFMEMORY(preallocate_size) ;
         goto ABBRUCH ;
      }
   }

   wbuf->free_size      = preallocate_size ;
   wbuf->free           = memblock ;
   wbuf->addr           = memblock ;
   wbuf->interface      = &g_wbuffer_interface ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int free_wbuffer(wbuffer_t * wbuf)
{
   if (&g_wbuffer_interface != wbuf->interface) {
      if (wbuf->interface) {
         return wbuf->interface->free_wbuffer(wbuf) ;
      }
      // ! wbuf->free_wbuffer => static case
   } else {
      if (wbuf->addr) {
         free(wbuf->addr) ;
      }
   }

   memset(wbuf, 0, sizeof(*wbuf)) ;
   return 0 ;
}

int appendalloc2_wbuffer(wbuffer_t * wbuf, size_t buffer_size, uint8_t ** buffer)
{
   int err ;

   err = grow_wbuffer(wbuf, buffer_size) ;
   if (err) goto ABBRUCH ;

   wbuf->free_size -= buffer_size ;
   *buffer          = wbuf->free ;
   wbuf->free      += buffer_size ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int appendchar2_wbuffer(wbuffer_t * wbuf, const char c)
{
   int err ;

   err = grow_wbuffer(wbuf, 1) ;
   if (err) goto ABBRUCH ;

   static_assert(1 == sizeof(c), ) ;

   wbuf->free_size -= 1 ;
   *wbuf->free      = (uint8_t) c ;
   wbuf->free      += sizeof(char) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int grow_wbuffer(wbuffer_t * wbuf, size_t free_size)
{
   int err ;

   size_t old_sizefree = sizefree_wbuffer(wbuf) ;

   if (old_sizefree < free_size) {

      if (&g_wbuffer_interface != wbuf->interface) {
         if (wbuf->interface) {
            return wbuf->interface->grow_wbuffer(wbuf, free_size) ;
         }
         // ! wbuf->grow_wbuffer => static case
         err = ENOMEM ;
         goto ABBRUCH ;
      }

      uint8_t  * memblock ;
      size_t   old_sizeused = (size_t) (wbuf->free - wbuf->addr) ;
      size_t   old_size     = wbuf->free_size + old_sizeused ;
      size_t   new_size     = (old_size + old_sizefree) >= free_size ? (2 * old_size) : (old_sizeused + free_size) ;

      if (  (ssize_t)new_size < 0  /* realloc would cause segmentation fault */
         || new_size <= old_size) {
         new_size = (size_t) -1 ;
         memblock = 0 ;
      } else {
         memblock = realloc(wbuf->addr, new_size) ;
      }

      if (!memblock) {
         err = ENOMEM ;
         LOG_OUTOFMEMORY(new_size) ;
         goto ABBRUCH ;
      }

      wbuf->free_size   = new_size - old_sizeused ;
      wbuf->free        = old_sizeused + memblock ;
      wbuf->addr        = memblock ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

void popbytes_wbuffer(wbuffer_t * wbuf, size_t size)
{
   size_t used = sizecontent_wbuffer(wbuf) ;

   if (used <= size) {
      wbuf->free_size += used ;
      wbuf->free       = wbuf->addr ;
   } else {
      wbuf->free_size += size ;
      wbuf->free      -= size ;
   }
}

void reset_wbuffer(wbuffer_t * wbuf)
{
   size_t used = sizecontent_wbuffer(wbuf) ;

   popbytes_wbuffer(wbuf, used) ;
}



// group: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_stat_initfree(void)
{
   uint8_t     buffer[1000] = { 0 } ;
   wbuffer_t   wbuf  = wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
   uint8_t     * mem = 0 ;

   // TEST wbuffer_INIT_STATIC
   TEST(1000   == wbuf.free_size) ;
   TEST(buffer == wbuf.free) ;
   TEST(buffer == wbuf.addr) ;
   TEST(0      == wbuf.interface) ;

   // TEST init, double free
   for(unsigned b = 0 ; b < 256; ++b) {
      memset(buffer, (int)b, sizeof(buffer)) ;
      wbuf = (wbuffer_t) wbuffer_INIT_STATIC(b, buffer) ;
      TEST(b      == wbuf.free_size) ;
      TEST(buffer == wbuf.free) ;
      TEST(buffer == wbuf.addr) ;
      TEST(buffer == content_wbuffer(&wbuf)) ;
      TEST(0      == wbuf.interface) ;
      TEST(b == sizefree_wbuffer(&wbuf)) ;
      TEST(0 == sizecontent_wbuffer(&wbuf)) ;
      wbuffer_t wfree = wbuffer_INIT_FREEABLE ;
      TEST(0 == free_wbuffer(&wbuf)) ;
      TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
      TEST(0 == free_wbuffer(&wbuf)) ;
      TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
      for(unsigned i = 0; i < sizeof(buffer); ++i) {
         TEST(b == buffer[i]) ;
      }
   }

   // TEST appendchar
   wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;
   for(unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST(b == sizecontent_wbuffer(&wbuf)) ;
      TEST(b == sizeof(buffer) - sizefree_wbuffer(&wbuf)) ;
      TEST(0 == appendchar_wbuffer(&wbuf, (char)b )) ;
      TEST(buffer == content_wbuffer(&wbuf)) ;
      TEST(b+1 == sizecontent_wbuffer(&wbuf)) ;
      TEST(b+1 == sizeof(buffer) - sizefree_wbuffer(&wbuf)) ;
   }
   TEST(ENOMEM == appendchar_wbuffer(&wbuf, 'z' )) ;
   for(unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST((uint8_t)b == buffer[b]) ;
   }

   // TEST popbytes
   for(unsigned b = 0 ; b <= sizeof(buffer); ++b) {
      popbytes_wbuffer(&wbuf, b ? 1 : 0) ;
      TEST(b == (size_t) (buffer + sizeof(buffer) - wbuf.free)) ;
      TEST(b == sizefree_wbuffer(&wbuf)) ;
   }
   TEST(0 == sizecontent_wbuffer(&wbuf)) ;
   popbytes_wbuffer(&wbuf, 1) ;
   TEST(0 == sizecontent_wbuffer(&wbuf)) ;
   TEST(sizeof(buffer) == sizefree_wbuffer(&wbuf)) ;
   for(unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST((uint8_t)b == buffer[b]) ;
   }

   // TEST appendalloc
   for(unsigned b = 0 ; b < sizeof(buffer); ++b) {
      popbytes_wbuffer(&wbuf, sizeof(buffer)) ;
      TEST(sizeof(buffer) == wbuf.free_size) ;
      TEST(buffer         == wbuf.free) ;
      TEST(0 == appendalloc_wbuffer(&wbuf, b, &mem)) ;
      TEST(mem == &buffer[0]) ;
      TEST(b == (size_t) (wbuf.free - buffer)) ;
      TEST(b == sizeof(buffer) - sizefree_wbuffer(&wbuf)) ;
      TEST(0 == appendalloc_wbuffer(&wbuf, sizeof(buffer)-b, &mem)) ;
      TEST(mem == &buffer[b]) ;
      TEST(buffer + sizeof(buffer) == wbuf.free) ;
      TEST(0 == sizefree_wbuffer(&wbuf)) ;
   }
   TEST(ENOMEM == appendalloc_wbuffer(&wbuf, 1, &mem)) ;
   for(unsigned b = 0 ; b < sizeof(buffer); ++b) {
      TEST((uint8_t)b == buffer[b]) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_dyn_initfree(void)
{
   wbuffer_t   wbuf   = wbuffer_INIT_FREEABLE ;
   wbuffer_t   wfree  = wbuffer_INIT_FREEABLE ;
   uint8_t     * addr = 0 ;
   uint8_t     * mem  = 0 ;

   // TEST wbuffer_INIT_FREEABLE, free
   TEST(0 == wbuf.free_size) ;
   TEST(0 == wbuf.free) ;
   TEST(0 == wbuf.addr) ;
   TEST(0 == wbuf.interface) ;
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == wbuf.free_size) ;
   TEST(0 == wbuf.free) ;
   TEST(0 == wbuf.addr) ;
   TEST(0 == wbuf.interface) ;

   // TEST wbuffer_INIT, free
   wbuf = (wbuffer_t) wbuffer_INIT ;
   TEST(0 == wbuf.free_size) ;
   TEST(0 == wbuf.free) ;
   TEST(0 == wbuf.addr) ;
   TEST(&g_wbuffer_interface == wbuf.interface) ;
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == wbuf.free_size) ;
   TEST(0 == wbuf.free) ;
   TEST(0 == wbuf.addr) ;
   TEST(0 == wbuf.interface) ;

   // TEST init, double free
   wbuf = (wbuffer_t) wbuffer_INIT_FREEABLE ;
   TEST(0 == init_wbuffer(&wbuf, 256)) ;
   TEST(0 != wbuf.addr) ;
   TEST(0 == wbuf.free - wbuf.addr) ;
   TEST(256 == sizefree_wbuffer(&wbuf)) ;
   TEST(&g_wbuffer_interface == wbuf.interface) ;
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;
   TEST(0 == wbuf.addr) ;
   TEST(0 == sizefree_wbuffer(&wbuf)) ;

   // TEST init, no preallocate
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   TEST(0 == wbuf.free_size) ;
   TEST(0 == wbuf.free) ;
   TEST(0 == wbuf.addr) ;
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;

   // TEST grow
   for(unsigned b = 0; b < 256; ++b) {
      TEST(0 == init_wbuffer(&wbuf, 0)) ;
      TEST(0 == sizefree_wbuffer(&wbuf)) ;
      TEST(0 == grow_wbuffer(&wbuf, b)) ;
      TEST(b == sizefree_wbuffer(&wbuf)) ;
      TEST(0 == free_wbuffer(&wbuf)) ;
   }

   // TEST grow doubles the size
   TEST(0 == init_wbuffer(&wbuf, 1)) ;
   for(uint32_t b = 1; b < 4096*4096; b *= 2) {
      TEST(0 == wbuf.free - wbuf.addr) ;
      TEST(b == sizefree_wbuffer(&wbuf)) ;
      memset(wbuf.addr, (int)b, b) ;
      TEST(0 == grow_wbuffer(&wbuf, b+1)) ;
      TEST(0 != wbuf.addr) ;
      TEST(0 == wbuf.free - wbuf.addr) ;
      TEST(2*b == sizefree_wbuffer(&wbuf)) ;
      mem = (uint8_t*) malloc(b) ;
      TEST(mem) ;
      memset(mem, (int)b, b) ;
      TEST(0 == memcmp(mem, wbuf.addr, b)) ;
      free(mem) ;
      mem = 0 ;
   }
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;

   // TEST grow with argument
   TEST(0 == init_wbuffer(&wbuf, 1)) ;
   for(uint32_t b = 100, size = 1; b < 4096*4096; size = b, b *= 5) {
      TEST(0 == wbuf.free - wbuf.addr) ;
      TEST(size == sizefree_wbuffer(&wbuf)) ;
      memset(content_wbuffer(&wbuf), (int)b, size) ;
      TEST(0 == grow_wbuffer(&wbuf, b)) ;
      TEST(0 != wbuf.addr) ;
      TEST(0 == sizecontent_wbuffer(&wbuf)) ;
      TEST(b == sizefree_wbuffer(&wbuf)) ;
      mem = (uint8_t*) malloc(size) ;
      TEST(mem) ;
      memset(mem, (int)b, size) ;
      TEST(0 == memcmp(mem, wbuf.addr, size)) ;
      free(mem) ;
      mem = 0 ;
   }
   TEST(0 == free_wbuffer(&wbuf)) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wbuf))) ;

   // TEST appendchar grow double
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   for(int c = 'A', size = 1; c <= 'Z'; ++c) {
      TEST((c-'A') == wbuf.free - wbuf.addr) ;
      TEST(0 == appendchar_wbuffer(&wbuf, (char)c)) ;
      TEST((1+c-'A') == wbuf.free - wbuf.addr) ;
      TEST(size == wbuf.free - wbuf.addr + (int)sizefree_wbuffer(&wbuf)) ;
      if (!sizefree_wbuffer(&wbuf)) size *= 2 ;
   }
   for(unsigned c = 'A'; c <= 'Z'; ++c) {
      TEST(c == wbuf.addr[c-'A']) ;
   }
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST appendalloc_wbuffer grow double
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   for(int c = 'A', size = 1; c <= 'Z'; ++c) {
      TEST((c-'A') == wbuf.free - wbuf.addr) ;
      TEST(0 == appendalloc_wbuffer(&wbuf, 1, &addr)) ;
      *addr = (uint8_t)c ;
      TEST((1+c-'A') == wbuf.free - wbuf.addr) ;
      TEST(size == wbuf.free - wbuf.addr + (int)sizefree_wbuffer(&wbuf)) ;
      if (!sizefree_wbuffer(&wbuf)) size *= 2 ;
   }
   for(unsigned c = 'A'; c <= 'Z'; ++c) {
      TEST(c == wbuf.addr[c-'A']) ;
   }
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST appendalloc grow with parameter
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   for(int c = 'A', used = 0; c <= 'Z'; ++c) {
      TEST(used == wbuf.free - wbuf.addr) ;
      int size = used + 1 ;
      TEST(0 == appendalloc_wbuffer(&wbuf, (unsigned) size, &addr)) ;
      memset(addr, c, (unsigned)size) ;
      used += size ;
      TEST(used == wbuf.free - wbuf.addr) ;
      TEST(0 == sizefree_wbuffer(&wbuf)) ;
   }
   addr = wbuf.addr ;
   for(unsigned c = 'A', size = 1; c <= 'Z'; ++c, size *= 2) {
      for(unsigned i = 0; i < size; ++i, ++addr) {
         TEST(c == *addr) ;
      }
   }
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST popbytes
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   for(unsigned c = 'a'; c <= 'z'; ++c) {
      TEST(0 == appendchar_wbuffer(&wbuf, (char)c)) ;
   }
   for(int c = 'a'; c <= 'z'; ++c) {
      TEST((1+'z'-c) == wbuf.free - wbuf.addr) ;
      size_t oldfree = sizefree_wbuffer(&wbuf) ;
      popbytes_wbuffer(&wbuf, 1) ;
      TEST(('z'-c)   == wbuf.free - wbuf.addr) ;
      TEST(oldfree+1 == sizefree_wbuffer(&wbuf)) ;
   }
   for(unsigned c = 'a'; c <= 'z'; ++c) {
      TEST(c == wbuf.addr[c-'a']) ;
   }
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST reset
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   for(int c = '0'; c <= '9'; ++c) {
      TEST(0 == appendchar_wbuffer(&wbuf, (char)c)) ;
      TEST(1+c-'0' == wbuf.free - wbuf.addr) ;
   }
   TEST(0 == appendchar_wbuffer(&wbuf, 0)) ;
   TEST(11 == sizecontent_wbuffer(&wbuf)) ;
   size_t oldfree = sizefree_wbuffer(&wbuf) ;
   TEST(0 == strcmp((char*)wbuf.addr, "0123456789")) ;
   reset_wbuffer(&wbuf) ;
   TEST(oldfree + 11 == sizefree_wbuffer(&wbuf)) ;
   TEST(0 == sizecontent_wbuffer(&wbuf)) ;
   TEST(0 == strcmp((char*)wbuf.addr, "0123456789")) ;
   TEST(0 == free_wbuffer(&wbuf)) ;

   // TEST grow ENOMEM
   TEST(0 == init_wbuffer(&wbuf, 0)) ;
   TEST(0 == appendalloc_wbuffer(&wbuf, 100000, &addr)) ;
   TEST(ENOMEM == grow_wbuffer(&wbuf, ((size_t)-1)/2)) ;
   TEST(ENOMEM == grow_wbuffer(&wbuf, (size_t)-1)) ;
   TEST(0 == free_wbuffer(&wbuf)) ;
   wbuf = (wbuffer_t) wbuffer_INIT ;
   wbuf.free = (uint8_t*) 0 + ((size_t)-1) ;
   TEST(ENOMEM == grow_wbuffer(&wbuf, 1)) ;
   wbuf = (wbuffer_t) wbuffer_INIT_FREEABLE ;

   return 0 ;
ABBRUCH:
   free(mem) ;
   free_wbuffer(&wbuf) ;
   return EINVAL ;
}

static int s_count_subtypefree = 0 ;
static int s_count_subtypegrow = 0 ;

static int subtype_grow(wbuffer_t * wbuf, size_t free_size)
{
   ++ s_count_subtypegrow ;

   if (free_size > wbuf->free_size + (size_t) (wbuf->free - wbuf->addr)) {
      return ENOMEM ;
   }

   wbuf->free_size += (size_t) (wbuf->free - wbuf->addr) ;
   wbuf->free       = wbuf->addr ;
   return 0 ;
}

static int subtype_free(wbuffer_t * wbuf)
{
   ++ s_count_subtypefree ;

   memset(wbuf, 0, sizeof(*wbuf)) ;
   return 0 ;
}

static int test_subtyping(void)
{
   wbuffer_t   wbuf   = wbuffer_INIT_FREEABLE ;
   wbuffer_t   wfree  = wbuffer_INIT_FREEABLE ;
   wbuffer_it  sub_it = { &subtype_free, &subtype_grow };
   uint8_t     buffer[64] = { 0 } ;
   uint8_t     * addr = 0 ;

#define subtype_INIT  (wbuffer_t) { sizeof(buffer), buffer, buffer, &sub_it } ;

   // TEST init, double free
   wbuf = subtype_INIT ;
   TEST(sizeof(buffer) == sizefree_wbuffer(&wbuf)) ;
   s_count_subtypefree = 0 ;
   free_wbuffer(&wbuf) ;
   TEST(1 == s_count_subtypefree) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wfree))) ;
   free_wbuffer(&wbuf) ;
   TEST(1 == s_count_subtypefree) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wfree))) ;
   wbuf.interface = &sub_it ;
   free_wbuffer(&wbuf) ;
   TEST(2 == s_count_subtypefree) ;
   TEST(0 == memcmp(&wbuf, &wfree, sizeof(wfree))) ;

   // TEST appendchar
   wbuf = subtype_INIT ;
   s_count_subtypegrow = 0 ;
   for(unsigned b = 0; b < sizeof(buffer); ++b) {
      TEST(0 == appendchar_wbuffer(&wbuf, (char)b)) ;
   }
   TEST(0 == s_count_subtypegrow) ;
   for(unsigned b = 0; b < sizeof(buffer); ++b) {
      TEST(buffer[b] == (uint8_t)b) ;
   }
   TEST(0 == appendchar_wbuffer(&wbuf, (char)'x')) ;
   TEST(1 == s_count_subtypegrow) ;
   TEST('x' == buffer[0]) ;

   // TEST appendalloc
   wbuf = subtype_INIT ;
   s_count_subtypegrow = 0 ;
   for(unsigned b = 0; b < sizeof(buffer); ++b) {
      addr = 0 ;
      TEST(0 == appendalloc_wbuffer(&wbuf, 1, &addr)) ;
      TEST(0 != addr) ;
      *addr = (uint8_t) (b + 1) ;
   }
   TEST(0 == s_count_subtypegrow) ;
   for(unsigned b = 0; b < sizeof(buffer); ++b) {
      TEST(buffer[b] == (uint8_t)1+b) ;
   }
   TEST(0 == appendalloc_wbuffer(&wbuf, 1, &addr)) ;
   TEST(1 == s_count_subtypegrow) ;
   TEST(addr == buffer) ;
   TEST(ENOMEM == appendalloc_wbuffer(&wbuf, sizeof(buffer)+1, &addr)) ;
   TEST(2 == s_count_subtypegrow) ;

#undef subtype_INIT

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_memory_wbuffer()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_dyn_initfree())   goto ABBRUCH ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_stat_initfree())  goto ABBRUCH ;
   if (test_dyn_initfree())   goto ABBRUCH ;
   if (test_subtyping())      goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif

