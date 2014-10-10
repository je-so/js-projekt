/* title: WriteBuffer impl
   Implements <WriteBuffer>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/memory/memstream.h"
#include "C-kern/api/string/cstring.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/test/mm/err_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/ds/foreach.h"
#endif


// section: wbuffer_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_wbuffer_errtimer
 * Simulates an error in <reserve_memblock_wbuffer> and <reserve_cstring_wbuffer>. */
static test_errortimer_t   s_wbuffer_errtimer = test_errortimer_FREE ;
#endif

// group: interface implementation

/* function: alloc_cstring_wbuffer
 * Resizes <cstring_t> if necessary and returns additional memory in memstr. */
static int alloc_cstring_wbuffer(void* impl, size_t freesize, /*inout*/memstream_t* memstr)
{
   int err;
   cstring_t* cstr = impl;

   size_t used     = (size_t) (memstr->next - addr_cstring(cstr));
   size_t capacity = used + freesize;

   if (capacity < used) {
      err = ENOMEM;
      TRACEOUTOFMEM_ERRLOG(freesize, err);
      goto ONERR;
   }

   ONERROR_testerrortimer(&s_wbuffer_errtimer, &err, ONERR);
   err = allocate_cstring(cstr, capacity);
   if (err) goto ONERR;

   *memstr = (memstream_t) memstream_INIT(addr_cstring(cstr) + used, addr_cstring(cstr) + capacity_cstring(cstr));

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: shrink_cstring_wbuffer
 * Sets memstr to whole allocated buffer of impl (<cstring_t>) except for the first new_size bytes. */
static int shrink_cstring_wbuffer(void * impl, size_t new_size, /*inout*/memstream_t * memstr)
{
   cstring_t* cstr  = impl;
   uint8_t*   start = addr_cstring(cstr);

   if ((size_t)(memstr->next - start) < new_size) goto ONERR;

   memstr->next = start + new_size;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(EINVAL);
   return EINVAL;
}

/* function: size_cstring_wbuffer
 * Returns size in use of impl (<cstring_t>). */
static size_t size_cstring_wbuffer(void* impl, const memstream_t* memstr)
{
   cstring_t* cstr = impl;
   return (size_t) (memstr->next - addr_cstring(cstr));
}

/* function: alloc_memblock_wbuffer
 * Resizes <memblock_t> if necessary and returns additional memory in memstr. */
static int alloc_memblock_wbuffer(void * impl, size_t freesize, /*inout*/memstream_t * memstr)
{
   int err;
   memblock_t* mb = impl;

   size_t   used    = (size_t) (memstr->next - mb->addr);
   size_t   memsize = mb->size >= freesize
                    ? 2 * mb->size
                    : mb->size + freesize;

   if (memsize <= mb->size) {
      err = ENOMEM ;
      TRACEOUTOFMEM_ERRLOG(freesize, err);
      goto ONERR;
   }

   err = RESIZE_ERR_MM(&s_wbuffer_errtimer, memsize, mb);
   if (err) goto ONERR;

   *memstr = (memstream_t) memstream_INIT(addr_memblock(mb) + used, addr_memblock(mb) + size_memblock(mb));

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: shrink_memblock_wbuffer
 * Sets memstr to whole allocated buffer of impl (<memblock_t>) except for the first keepsize bytes. */
static int shrink_memblock_wbuffer(void* impl, size_t keepsize, /*inout*/memstream_t* memstr)
{
   memblock_t* mb = impl;

   if ((size_t)(memstr->next - addr_memblock(mb)) < keepsize) goto ONERR;

   memstr->next = addr_memblock(mb) + keepsize;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(EINVAL);
   return EINVAL;
}

/* function: size_memblock_wbuffer
 * Returns number of appended bytes of impl (<memblock_t>). */
static size_t size_memblock_wbuffer(void* impl, const memstream_t* memstr)
{
   memblock_t* mb = impl;
   return (size_t) (memstr->next - mb->addr);
}

/* function: alloc_static_wbuffer
 * Returns always ENOMEM. */
static int alloc_static_wbuffer(void* impl, size_t freesize, /*inout*/memstream_t* memstr)
{
   (void) impl;
   (void) freesize;
   (void) memstr;
   return ENOMEM;
}

/* function: shrink_static_wbuffer
 * Sets wbuf->next to start+keepsize of static buffer. */
static int shrink_static_wbuffer(void * impl, size_t keepsize, /*inout*/memstream_t * memstr)
{
   if ((size_t)(memstr->next - (uint8_t*)impl) < keepsize) goto ONERR;
   memstr->next = (uint8_t*)impl + keepsize;
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(EINVAL);
   return EINVAL;
}

/* function: size_static_wbuffer
 * Returns number of appended bytes. */
static size_t size_static_wbuffer(void * impl, const memstream_t * memstr)
{
   return (size_t) (memstr->next - (uint8_t*)impl) ;
}

// group: global variables

wbuffer_it  g_wbuffer_cstring  = { &alloc_cstring_wbuffer, &shrink_cstring_wbuffer, &size_cstring_wbuffer } ;

wbuffer_it  g_wbuffer_memblock = { &alloc_memblock_wbuffer, &shrink_memblock_wbuffer, &size_memblock_wbuffer  } ;

wbuffer_it  g_wbuffer_static   = { &alloc_static_wbuffer, &shrink_static_wbuffer, &size_static_wbuffer  } ;

// group: change

int appendcopy_wbuffer(wbuffer_t* wbuf, size_t buffer_size, const uint8_t* buffer)
{
   int err;
   const size_t free = sizefree_wbuffer(wbuf);

   if (free >= buffer_size) {
      memcpy(wbuf->next, buffer, buffer_size);
      wbuf->next += buffer_size;

   } else {
      memcpy(wbuf->next, buffer, free);
      wbuf->next += free;

      const size_t missing = buffer_size - free;
      err = wbuf->iimpl->alloc(wbuf->impl, missing, (memstream_t*)wbuf);
      if (err) {
         wbuf->next -= free; // remove partially copied content
         goto ONERR;
      }

      memcpy(wbuf->next, buffer + free, missing);
      wbuf->next += missing;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_variables(void)
{
   // TEST g_wbuffer_cstring
   TEST(g_wbuffer_cstring.alloc  == &alloc_cstring_wbuffer) ;
   TEST(g_wbuffer_cstring.shrink == &shrink_cstring_wbuffer) ;
   TEST(g_wbuffer_cstring.size   == &size_cstring_wbuffer) ;

   // TEST g_wbuffer_memblock
   TEST(g_wbuffer_memblock.alloc  == &alloc_memblock_wbuffer) ;
   TEST(g_wbuffer_memblock.shrink == &shrink_memblock_wbuffer) ;
   TEST(g_wbuffer_memblock.size   == &size_memblock_wbuffer) ;

   // TEST g_wbuffer_static
   TEST(g_wbuffer_static.alloc  == &alloc_static_wbuffer) ;
   TEST(g_wbuffer_static.shrink == &shrink_static_wbuffer) ;
   TEST(g_wbuffer_static.size   == &size_static_wbuffer) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_initfree(void)
{
   uint8_t     buffer[1000] = { 0 } ;
   cstring_t   cstr     = cstring_INIT ;
   memblock_t  memblock = memblock_FREE ;
   wbuffer_t   wbuf     = wbuffer_FREE ;

   // TEST wbuffer_t is subtype of memstream_t
   TEST((memstream_t*)&wbuf == cast_memstream(&wbuf,)) ;

   // TEST wbuffer_FREE
   TEST(0 == wbuf.next) ;
   TEST(0 == wbuf.end) ;
   TEST(0 == wbuf.impl) ;
   TEST(0 == wbuf.iimpl) ;

   // TEST wbuffer_INIT_CSTRING: empty cstr
   wbuf = (wbuffer_t) wbuffer_INIT_CSTRING(&cstr);
   TEST(wbuf.next  == 0);
   TEST(wbuf.end   == 0);
   TEST(wbuf.impl  == &cstr);
   TEST(wbuf.iimpl == &g_wbuffer_cstring);

   // TEST wbuffer_INIT_CSTRING: allocated string
   TEST(0 == allocate_cstring(&cstr, 1000));
   wbuf = (wbuffer_t) wbuffer_INIT_CSTRING(&cstr);
   TEST(wbuf.next  == addr_cstring(&cstr));
   TEST(wbuf.end   == wbuf.next + capacity_cstring(&cstr));
   TEST(wbuf.impl  == &cstr);
   TEST(wbuf.iimpl == &g_wbuffer_cstring);

   // TEST wbuffer_INIT_MEMBLOCK: empty memblock
   wbuf = (wbuffer_t) wbuffer_INIT_MEMBLOCK(&memblock);
   TEST(wbuf.next  == 0);
   TEST(wbuf.end   == 0);
   TEST(wbuf.impl  == &memblock);
   TEST(wbuf.iimpl == &g_wbuffer_memblock);

   // TEST wbuffer_INIT_MEMBLOCK: allocated memblock
   TEST(0 == RESIZE_MM(1000, &memblock));
   wbuf = (wbuffer_t) wbuffer_INIT_MEMBLOCK(&memblock);
   TEST(wbuf.next  == addr_memblock(&memblock));
   TEST(wbuf.end   == wbuf.next + size_memblock(&memblock));
   TEST(wbuf.impl  == &memblock);
   TEST(wbuf.iimpl == &g_wbuffer_memblock);

   // TEST wbuffer_INIT_STATIC
   for (unsigned b = 0 ; b <= sizeof(buffer); ++b) {
      wbuf = (wbuffer_t) wbuffer_INIT_STATIC(b, buffer);
      TEST(wbuf.next == buffer);
      TEST(wbuf.end  == buffer + b);
      TEST(wbuf.impl == buffer);
      TEST(wbuf.iimpl == &g_wbuffer_static);
   }

   // TEST wbuffer_INIT_OTHER
   wbuf = (wbuffer_t) wbuffer_INIT_OTHER(sizeof(buffer), buffer, (void*)88, (void*)99);
   TEST(wbuf.next == buffer);
   TEST(wbuf.end  == buffer + sizeof(buffer));
   TEST(wbuf.impl == (void*)88);
   TEST(wbuf.iimpl == (void*)99);

   // unprepare
   TEST(0 == free_cstring(&cstr));
   TEST(0 == FREE_MM(&memblock));

   return 0;
ONERR:
   free_cstring(&cstr);
   FREE_MM(&memblock);
   return EINVAL;
}

static int test_cstring_adapter(void)
{
   cstring_t cstr = cstring_INIT ;
   wbuffer_t wbuf = wbuffer_INIT_CSTRING(&cstr);

   // TEST size_cstring_wbuffer: empty cstr
   TEST(0 == str_cstring(&cstr)) ;
   for (unsigned i = 100; i <= 100; --i) {
      wbuf.next = (void*)i;
      TEST(i == size_cstring_wbuffer(wbuf.impl, cast_memstream(&wbuf,)));
   }

   // TEST shrink_cstring_wbuffer: empty cstr
   wbuf.next = 0;
   wbuf.end  = 0;
   TEST(0 == shrink_cstring_wbuffer(wbuf.impl, 0, cast_memstream(&wbuf,)));
   TEST(0 == wbuf.next);
   TEST(0 == wbuf.end);

   // TEST alloc_cstring_wbuffer: empty cstr
   TEST(0 == alloc_cstring_wbuffer(wbuf.impl, 100, cast_memstream(&wbuf,)));
   TEST(0 == size_cstring(&cstr));
   TEST(100 == capacity_cstring(&cstr));
   TEST(wbuf.next == addr_cstring(&cstr));
   TEST(wbuf.end  == addr_cstring(&cstr) + 100);

   // TEST size_cstring_wbuffer
   for (unsigned i = 100; i <= 100; --i) {
      wbuf.next = i + addr_cstring(&cstr);
      TEST(i == size_cstring_wbuffer(wbuf.impl, cast_memstream(&wbuf,)));
   }

   // TEST shrink_cstring_wbuffer
   for (unsigned i = 0; i <= 70; ++i) {
      wbuf.next = addr_cstring(&cstr) + 70;
      wbuf.end  = addr_cstring(&cstr) + 70;
      TEST(0 == shrink_cstring_wbuffer(wbuf.impl, i, cast_memstream(&wbuf,)));
      TEST(wbuf.next == addr_cstring(&cstr) + i);
      TEST(wbuf.end  == addr_cstring(&cstr) + 70);
   }
   wbuf.end = addr_cstring(&cstr) + capacity_cstring(&cstr);

   // TEST shrink_cstring_wbuffer: EINVAL
   wbuf.next = addr_cstring(&cstr) + 70;
   TEST(EINVAL == shrink_cstring_wbuffer(wbuf.impl, 71, cast_memstream(&wbuf,)));
   TEST(wbuf.next == addr_cstring(&cstr) + 70);
   TEST(wbuf.end  == addr_cstring(&cstr) + capacity_cstring(&cstr));

   // TEST alloc_cstring_wbuffer: cleared wbuffer
   wbuf.next = addr_cstring(&cstr);
   TEST(0 == alloc_cstring_wbuffer(wbuf.impl, 200, cast_memstream(&wbuf,)));
   TEST(0 == size_cstring(&cstr));
   TEST(200 == capacity_cstring(&cstr));
   TEST(wbuf.next == addr_cstring(&cstr));
   TEST(wbuf.end  == addr_cstring(&cstr) + capacity_cstring(&cstr));

   // TEST alloc_cstring_wbuffer: non empty wbuffer
   wbuf.next += 122;
   TEST(0 == alloc_cstring_wbuffer(wbuf.impl, 300, cast_memstream(&wbuf,)));
   TEST(0 == size_cstring(&cstr));
   TEST(422 == capacity_cstring(&cstr));
   TEST(wbuf.next == addr_cstring(&cstr) + 122);
   TEST(wbuf.end  == addr_cstring(&cstr) + capacity_cstring(&cstr));

   // TEST alloc_cstring_wbuffer: ENOMEM
   char*  S = str_cstring(&cstr);
   size_t C = capacity_cstring(&cstr);
   init_testerrortimer(&s_wbuffer_errtimer, 1, ENOMEM);
   TEST(ENOMEM == alloc_cstring_wbuffer(wbuf.impl, 400, cast_memstream(&wbuf,)));
   TEST(S == str_cstring(&cstr));
   TEST(0 == size_cstring(&cstr));
   TEST(C == capacity_cstring(&cstr));
   TEST(wbuf.next == addr_cstring(&cstr) + 122);
   TEST(wbuf.end  == addr_cstring(&cstr) + C);

   // uprepare
   TEST(0 == free_cstring(&cstr));

   return 0;
ONERR:
   free_cstring(&cstr);
   return EINVAL;
}

static int test_memblock_adapter(void)
{
   memblock_t mb   = memblock_FREE;
   wbuffer_t  wbuf = wbuffer_INIT_MEMBLOCK(&mb);

   // TEST size_memblock_wbuffer: empty memblock
   TEST(0 == addr_memblock(&mb)) ;
   for (unsigned i = 100; i <= 100; --i) {
      wbuf.next = (void*)i ;
      TEST(i == size_memblock_wbuffer(wbuf.impl, cast_memstream(&wbuf,))) ;
   }

   // TEST shrink_memblock_wbuffer: empty memblock
   wbuf.next = 0;
   wbuf.end  = 0;
   TEST(0 == shrink_memblock_wbuffer(wbuf.impl, 0, cast_memstream(&wbuf,)));
   TEST(0 == wbuf.next) ;
   TEST(0 == wbuf.end) ;

   // TEST alloc_memblock_wbuffer: empty memblock
   TEST(0 == alloc_memblock_wbuffer(wbuf.impl, 100, cast_memstream(&wbuf,))) ;
   TEST(100 <= size_memblock(&mb)) ;
   TEST(wbuf.next == addr_memblock(&mb)) ;
   TEST(wbuf.end  == addr_memblock(&mb) + size_memblock(&mb)) ;

   // TEST size_memblock_wbuffer
   for (unsigned i = 100; i <= 100; --i) {
      wbuf.next = addr_memblock(&mb) + i ;
      TEST(i == size_memblock_wbuffer(wbuf.impl, cast_memstream(&wbuf,))) ;
   }

   // TEST shrink_memblock_wbuffer
   for (unsigned i = 0; i <= 70; ++i) {
      wbuf.next = addr_memblock(&mb) + 70;
      wbuf.end  = addr_memblock(&mb) + 70;
      TEST(0 == shrink_memblock_wbuffer(wbuf.impl, i, cast_memstream(&wbuf,)));
      TEST(wbuf.next == addr_memblock(&mb) + i);
      TEST(wbuf.end  == addr_memblock(&mb) + 70);
   }
   wbuf.end = addr_memblock(&mb) + size_memblock(&mb);

   // TEST shrink_memblock_wbuffer: EINVAL
   wbuf.next = addr_memblock(&mb) + 70;
   TEST(EINVAL == shrink_memblock_wbuffer(wbuf.impl, 71, cast_memstream(&wbuf,)));
   TEST(wbuf.next == addr_memblock(&mb) + 70);
   TEST(wbuf.end  == addr_memblock(&mb) + size_memblock(&mb));

   // TEST alloc_memblock_wbuffer: cleared wbuffer
   wbuf.next = addr_memblock(&mb);
   TEST(0 == alloc_memblock_wbuffer(wbuf.impl, 200, cast_memstream(&wbuf,)));
   TEST(300 <= size_memblock(&mb));
   TEST(wbuf.next == addr_memblock(&mb));
   TEST(wbuf.end  == addr_memblock(&mb) + size_memblock(&mb));

   // TEST alloc_memblock_wbuffer: non empty wbuffer
   wbuf.next += 122 ;
   TEST(0 == alloc_memblock_wbuffer(wbuf.impl, 1, cast_memstream(&wbuf,)));
   TEST(600 <= size_memblock(&mb));   // doubled in size (used/reserved is not taken into count)
   TEST(wbuf.next == addr_memblock(&mb) + 122);
   TEST(wbuf.end  == addr_memblock(&mb) + size_memblock(&mb));

   // TEST alloc_memblock_wbuffer: ENOMEM
   uint8_t* oldaddr = addr_memblock(&mb);
   size_t   oldsize = size_memblock(&mb);
   init_testerrortimer(&s_wbuffer_errtimer, 1, ENOMEM);
   TEST(ENOMEM == alloc_memblock_wbuffer(wbuf.impl, 1, cast_memstream(&wbuf,)));
   TEST(oldaddr == addr_memblock(&mb));
   TEST(oldsize == size_memblock(&mb));
   TEST(wbuf.next == addr_memblock(&mb) + 122);
   TEST(wbuf.end  == addr_memblock(&mb) + size_memblock(&mb));

   // uprepare
   TEST(0 == FREE_MM(&mb));

   return 0;
ONERR:
   FREE_MM(&mb);
   return EINVAL;
}

static int test_static_adapter(void)
{
   uint8_t     buf[100] ;
   wbuffer_t   wbuf = wbuffer_INIT_STATIC(sizeof(buf), buf) ;

   // TEST size_static_wbuffer: empty memblock
   for (unsigned i = 100; i <= 100; --i) {
      wbuf.next = buf + i ;
      TEST(i == size_static_wbuffer(wbuf.impl, cast_memstream(&wbuf,))) ;
   }

   // TEST shrink_static_wbuffer
   for (unsigned i = 0; i <= 100; ++i) {
      wbuf.next = buf+100;
      TEST(0 == shrink_static_wbuffer(wbuf.impl, i, cast_memstream(&wbuf,)));
      TEST(wbuf.next == buf+i) ;
      TEST(wbuf.end  == buf+sizeof(buf));
   }

   // TEST shrink_static_wbuffer: EINVAL
   wbuf.next = buf+10;
   TEST(EINVAL == shrink_static_wbuffer(wbuf.impl, 11, cast_memstream(&wbuf,)));
   TEST(wbuf.next == buf+10);
   TEST(wbuf.end  == buf+sizeof(buf));

   // TEST alloc_static_wbuffer
   TEST(ENOMEM == alloc_static_wbuffer(wbuf.impl, 1, cast_memstream(&wbuf,)));

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_query(void)
{
   uint8_t     buffer[256] = { 0 } ;
   cstring_t   cstr     = cstring_INIT ;
   memblock_t  memblock = memblock_FREE ;
   wbuffer_t   wbuf     = wbuffer_FREE ;

   // TEST sizefree_wbuffer: depends only wbuf.next and wbuf.end
   for (unsigned t = 0; t < 3; ++t) {
      switch (t)
      {
      case 0:  wbuf = (wbuffer_t) wbuffer_INIT_CSTRING(&cstr) ;   break ;
      case 1:  wbuf = (wbuffer_t) wbuffer_INIT_MEMBLOCK(&memblock) ; break ;
      default: wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;  break ;
      }
      for (unsigned i = 0; i < 16; ++i) {
         wbuf.next = (void*)(i + 1) ;
         wbuf.end  = (void*)i ;
         TEST((size_t)-1 == sizefree_wbuffer(&wbuf)) ;
         wbuf.end = (void*)(i+2) ;
         TEST(1 == sizefree_wbuffer(&wbuf)) ;
         wbuf.end = (void*)(5*i+2) ;
         TEST(4*i+1 == sizefree_wbuffer(&wbuf)) ;
         wbuf.next = wbuf.end ;
         TEST(0 == sizefree_wbuffer(&wbuf)) ;
      }
   }

   // TEST size_wbuffer
   for (unsigned t = 0; t < 3; ++t) {
      switch (t)
      {
      case 0:  wbuf = (wbuffer_t) wbuffer_INIT_CSTRING(&cstr) ;   break ;
      case 1:  wbuf = (wbuffer_t) wbuffer_INIT_MEMBLOCK(&memblock) ; break ;
      default: wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(buffer), buffer) ;  break ;
      }
      TEST(0 == size_wbuffer(&wbuf)) ;
      if (t < 2) {
         TEST(0 == wbuf.iimpl->alloc(wbuf.impl, 256, cast_memstream(&wbuf,))) ;
      }
      uint8_t * start = wbuf.next ;
      for (size_t i = 0; i < 256; ++i) {
         wbuf.next = start - i ;
         TEST(-i == size_wbuffer(&wbuf)) ;
         wbuf.next = start + i ;
         TEST(i == size_wbuffer(&wbuf)) ;
      }
   }

   // unprepare
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == FREE_MM(&memblock)) ;

   return 0 ;
ONERR:
   free_cstring(&cstr) ;
   FREE_MM(&memblock) ;
   return EINVAL ;
}

static int test_update(void)
{
   memblock_t mblock = memblock_FREE;
   wbuffer_t  wbuf   = wbuffer_INIT_MEMBLOCK(&mblock);
   uint8_t*   b;

   // TEST appendbyte_wbuffer: no reserved bytes
   TEST(0 == appendbyte_wbuffer(&wbuf, '0')) ;
   TEST(0 != addr_memblock(&mblock)) ;
   TEST(1 <= size_memblock(&mblock)) ;
   TEST(wbuf.next == addr_memblock(&mblock) + 1) ;
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;
   TEST('0' == addr_memblock(&mblock)[0]) ;

   // TEST appendbyte_wbuffer
   for (unsigned c = 0, size = size_memblock(&mblock); c <= 30; ++c) {
      if (sizefree_wbuffer(&wbuf) == 0) size *= 2 ;
      TEST(0 == appendbyte_wbuffer(&wbuf, (uint8_t)(c+'A'))) ;
      TEST((c+'A') == addr_memblock(&mblock)[1+c]) ;
      TEST(size      == size_memblock(&mblock)) ;
      TEST(wbuf.next == addr_memblock(&mblock) + 2 + c) ;
      TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;
   }
   TEST('0' == addr_memblock(&mblock)[0]) ;
   for (unsigned c = 0; c <= 30; ++c) {
      TEST('A'+c == addr_memblock(&mblock)[1+c]) ;
   }

   // TEST appendbyte_wbuffer: ENOMEM
   init_testerrortimer(&s_wbuffer_errtimer, 1, ENOMEM) ;
   wbuf.next = wbuf.end - 1 ;
   wbuf.end[-1] = 0 ;
   TEST(0 == appendbyte_wbuffer(&wbuf, 1)) ;
   TEST(1 == wbuf.end[-1]) ;
   memblock_t old = mblock ;
   TEST(ENOMEM == appendbyte_wbuffer(&wbuf, 1)) ;
   TEST(old.addr  == addr_memblock(&mblock)) ;
   TEST(old.size  == size_memblock(&mblock)) ;
   TEST(wbuf.next == addr_memblock(&mblock) + size_memblock(&mblock)) ;
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;

   // TEST clear_wbuffer
   old = mblock ;
   clear_wbuffer(&wbuf) ;
   TEST(old.addr  == addr_memblock(&mblock)) ;
   TEST(old.size  == size_memblock(&mblock)) ;
   TEST(wbuf.next == addr_memblock(&mblock)) ;
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;

   // TEST shrink_wbuffer
   for (unsigned i = 0; i <= 30; ++i) {
      wbuf.next = addr_memblock(&mblock)+size_memblock(&mblock);
      TEST(0 == shrink_wbuffer(&wbuf, i));
      TEST(old.addr  == addr_memblock(&mblock));
      TEST(old.size  == size_memblock(&mblock));
      TEST(wbuf.next == addr_memblock(&mblock)+i);
      TEST(wbuf.end  == addr_memblock(&mblock)+size_memblock(&mblock));
   }

   // TEST shrink_wbuffer: EINVAL
   wbuf.next = addr_memblock(&mblock)+10;
   TEST(EINVAL == shrink_wbuffer(&wbuf, 11));
   TEST(old.addr  == addr_memblock(&mblock));
   TEST(old.size  == size_memblock(&mblock));
   TEST(wbuf.next == addr_memblock(&mblock)+10);
   TEST(wbuf.end  == addr_memblock(&mblock)+size_memblock(&mblock));

   // TEST appendbytes_wbuffer: 0 bytes, wbuf.next != 0
   wbuf.next = addr_memblock(&mblock);
   TEST(0 == appendbytes_wbuffer(&wbuf, 0, &b)) ;
   TEST(old.addr  == addr_memblock(&mblock)) ;
   TEST(old.size  == size_memblock(&mblock)) ;
   TEST(b == addr_memblock(&mblock)) ;
   TEST(wbuf.next == addr_memblock(&mblock)) ;
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;

   // TEST appendbytes_wbuffer: 0 bytes, wbuf.next == 0
   TEST(0 == FREE_MM(&mblock)) ;
   wbuf = (wbuffer_t) wbuffer_INIT_MEMBLOCK(&mblock) ;
   TEST(0 == appendbytes_wbuffer(&wbuf, 0, &b)) ;
   TEST(0 == addr_memblock(&mblock)) ;
   TEST(0 == size_memblock(&mblock)) ;
   TEST(0 == b) ;
   TEST(0 == wbuf.next) ;
   TEST(0 == wbuf.end) ;

   // TEST appendbytes_wbuffer: no reserved bytes
   TEST(0 == appendbytes_wbuffer(&wbuf, 1, &b)) ;
   TEST(b == addr_memblock(&mblock)) ;
   TEST(0 != addr_memblock(&mblock)) ;
   TEST(1 <= size_memblock(&mblock)) ;
   TEST(wbuf.next == addr_memblock(&mblock) + 1) ;
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;
   *b = 0 ;

   // TEST appendbytes_wbuffer: doubles in size
   for (size_t i = 1, size = 1, offset = 1; i <= 16; ++i) {
      old = mblock ;
      bool isresize = (i > sizefree_wbuffer(&wbuf)) ;
      TEST(0 == appendbytes_wbuffer(&wbuf, i, &b)) ;
      if (isresize) {
         size *= 2 ;
         TEST(size      == size_memblock(&mblock)) ;
      } else {
         TEST(old.addr  == addr_memblock(&mblock)) ;
         TEST(old.size  == size_memblock(&mblock)) ;
      }
      TEST(b         == offset + addr_memblock(&mblock)) ;
      offset += i ;
      TEST(wbuf.next == offset + addr_memblock(&mblock)) ;
      TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;
      memset(b, (int)i, i) ;
   }
   TEST(0 == addr_memblock(&mblock)[0]) ;
   for (size_t i = 1, offset = 1; i <= 16; offset += i, ++i) {
      for (size_t c = offset; c < offset+i; ++c) {
         TEST(i == addr_memblock(&mblock)[c]) ;
      }
   }

   // TEST appendbytes_wbuffer: ENOMEM
   old = mblock ;
   b = 0 ;
   TEST(ENOMEM == appendbytes_wbuffer(&wbuf, SIZE_MAX, &b)) ;
   TEST(0 == b) ;
   init_testerrortimer(&s_wbuffer_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == appendbytes_wbuffer(&wbuf, sizefree_wbuffer(&wbuf)+1, &b)) ;
   TEST(0 == b) ;
   TEST(old.addr == addr_memblock(&mblock)) ;
   TEST(old.size == size_memblock(&mblock)) ;

   // TEST appendcopy_wbuffer: empty block
   uint8_t buffer[256];
   for (unsigned i = 0; i < 256; ++i) {
      buffer[i] = (uint8_t)i;
   }
   TEST(0 == FREE_MM(&mblock)) ;
   wbuf = (wbuffer_t) wbuffer_INIT_MEMBLOCK(&mblock) ;
   TEST(0 == appendcopy_wbuffer(&wbuf, 1, buffer)) ;
   TEST(0 != addr_memblock(&mblock)) ;
   TEST(1 <= size_memblock(&mblock)) ;
   TEST(wbuf.next == addr_memblock(&mblock) + 1) ;
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;
   TEST(0 == addr_memblock(&mblock)[0]) ;

   // TEST appendcopy_wbuffer: append to non empty block
   TEST(0 == appendcopy_wbuffer(&wbuf, 15, buffer));
   TEST(16 <= size_memblock(&mblock));
   TEST(wbuf.next == addr_memblock(&mblock) + 16);
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock));
   TEST(0 == memcmp(buffer, addr_memblock(&mblock)+1, 15));

   // TEST appendcopy_wbuffer: copy with no alloc
   memcpy(&old, &mblock, sizeof(old));
   clear_wbuffer(&wbuf);
   TEST(0 == appendcopy_wbuffer(&wbuf, size_memblock(&mblock), buffer));
   TEST(0 == memcmp(&old, &mblock, sizeof(old)));
   TEST(wbuf.next == addr_memblock(&mblock) + size_memblock(&mblock));
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock));
   TEST(0 == memcmp(buffer, addr_memblock(&mblock), size_memblock(&mblock)));

   // TEST appendcopy_wbuffer: copy with alloc
   clear_wbuffer(&wbuf);
   TEST(0 == appendcopy_wbuffer(&wbuf, 32, buffer+3));
   TEST(32 <= size_memblock(&mblock));
   TEST(wbuf.next == addr_memblock(&mblock) + 32);
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock));
   TEST(0 == memcmp(buffer+3, addr_memblock(&mblock), 32));

   // TEST appendcopy_wbuffer: ENOMEM
   old = mblock ;
   clear_wbuffer(&wbuf) ;
   TEST(0 == appendcopy_wbuffer(&wbuf, 16, buffer)) ;
   TEST(ENOMEM == appendcopy_wbuffer(&wbuf, SIZE_MAX, buffer)) ;
   init_testerrortimer(&s_wbuffer_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == appendcopy_wbuffer(&wbuf, sizefree_wbuffer(&wbuf)+1, buffer)) ;
   TEST(old.addr == addr_memblock(&mblock)) ;
   TEST(old.size == size_memblock(&mblock)) ;
   TEST(wbuf.next == addr_memblock(&mblock) + 16) ;
   TEST(wbuf.end  == addr_memblock(&mblock) + size_memblock(&mblock)) ;
   TEST(0 == memcmp(buffer, addr_memblock(&mblock), 16)) ;

   // unprepare
   TEST(0 == FREE_MM(&mblock)) ;

   return 0 ;
ONERR:
   FREE_MM(&mblock) ;
   return EINVAL ;
}

typedef struct mblock2_t   mblock2_t ;

struct mblock2_t {
   uint8_t  *  addr ;
   size_t      size ;
   size_t      used ;
   mblock2_t * next ;
} ;

static unsigned s_other_alloc  = 0;
static unsigned s_other_size   = 0;
static unsigned s_other_shrink = 0;

static int alloc_other_test(void * impl, size_t new_size, /*inout*/struct memstream_t * memstr)
{
   ++ s_other_alloc ;

   mblock2_t * current = impl ;

   while (current->addr + current->size != memstr->end) {
      current = current->next ;
      if (current == impl) {
         return EINVAL ;
      }
   }

   mblock2_t * next = current->next ;
   if (  next->used
         || next->size < new_size) {
      return ENOMEM ;
   }

   current->used = (size_t) (memstr->next - current->addr) ;

   memstr->next = next->addr ;
   memstr->end  = next->addr + next->size ;

   return 0 ;
}

static size_t size_other_test(void * impl, const struct memstream_t * memstr)
{
   ++ s_other_size ;

   mblock2_t * current = impl ;

   while (current->addr + current->size != memstr->end) {
      current = current->next ;
      if (current == impl) {
         return EINVAL ;
      }
   }

   size_t size = (size_t) (memstr->next - current->addr) ;
   mblock2_t * first = current ;
   mblock2_t * next  = first->next ;

   while (next != first) {
      size += next->used ;
      next = next->next ;
   }

   return size ;
}

static int shrink_other_test(void * impl, size_t new_size, /*inout*/struct memstream_t * memstr)
{
   ++ s_other_shrink;

   mblock2_t * next    = impl;
   mblock2_t * current = impl;

   while (current->addr + current->size != memstr->end) {
      current = current->next;
      if (current == impl) {
         return EINVAL;
      }
   }

   size_t size = new_size;
   while (next != current && size >= next->used) {
      size -= next->used;
      next = next->next;
   }

   if (next == current && (size_t)(memstr->next - current->addr) < size) {
      return EINVAL;
   }

   memstr->next = next->addr + size;
   memstr->end  = next->addr + next->size;

   for (;;) {
      next->used = 0;
      if (next == current) break;
      next = next->next;
   }

   return 0;
}

static int test_other_impl(void)
{
   wbuffer_t   wbuf       = wbuffer_FREE ;
   wbuffer_it  other_it   = { &alloc_other_test, &shrink_other_test, &size_other_test };
   uint8_t     buffer[64] = { 0 } ;
   // mb2array builds a list of 4 blocks of 16 byte of memory
   mblock2_t mb2array[4] = {
      { &buffer[ 0], 16, 0, &mb2array[1] },
      { &buffer[16], 16, 0, &mb2array[2] },
      { &buffer[32], 16, 0, &mb2array[3] },
      { &buffer[48], 16, 0, &mb2array[0] },
   } ;

   // TEST wbuffer_INIT_OTHER
   wbuf = (wbuffer_t) wbuffer_INIT_OTHER(mb2array[0].size, mb2array[0].addr, &mb2array[0], &other_it) ;
   TEST(wbuf.next == buffer) ;
   TEST(wbuf.end  == buffer+16) ;
   TEST(wbuf.impl == &mb2array[0]) ;
   TEST(wbuf.iimpl == &other_it) ;

   // == query ==

    // TEST sizefree_wbuffer
   wbuf.next = wbuf.end ;
   TEST(0 == sizefree_wbuffer(&wbuf)) ;
   wbuf.next = buffer ;
   TEST(16 == sizefree_wbuffer(&wbuf)) ;

   // TEST size_wbuffer
   s_other_size = 0 ;
   wbuf.next = wbuf.end ;
   TEST(16 == size_wbuffer(&wbuf)) ;
   TEST(1 == s_other_size) ;
   wbuf.next = buffer ;
   TEST(0 == size_wbuffer(&wbuf)) ;
   TEST(2 == s_other_size) ;
   for (unsigned i = 0; i < lengthof(mb2array); ++i) {
      mb2array[i].used = (1 + i) ;
   }
   TEST(2+3+4 == size_wbuffer(&wbuf)) ; /* first block uses wbuf.next - mb2array[0].addr */
   TEST(3 == s_other_size) ;
   for (unsigned i = 0; i < lengthof(mb2array); ++i) {
      mb2array[i].used = 0 ;
   }

   // == change ==

   // TEST appendbyte_wbuffer
   s_other_alloc = 0 ;
   for (uint8_t b = 0; b < sizeof(buffer); ++b) {
      TEST(0 == appendbyte_wbuffer(&wbuf, b)) ;
   }
   TEST(s_other_alloc  == lengthof(mb2array)-1) ;
   TEST(sizeof(buffer) == size_wbuffer(&wbuf)) ;
   TEST(wbuf.next == wbuf.end) ;
   TEST(wbuf.end  == &buffer[lengthof(buffer)]) ;
   TEST(ENOMEM == appendbyte_wbuffer(&wbuf, 0)) ;
   TEST(0 == mb2array[lengthof(mb2array)-1].used) ;
   for (unsigned i = 0; i < lengthof(mb2array)-1; ++i) {
      TEST(16 == mb2array[i].used) ;
   }
   for (uint8_t b = 0; b < sizeof(buffer); ++b) {
      TEST(b == buffer[b]) ;
   }

   // TEST clear_wbuffer
   clear_wbuffer(&wbuf) ;
   TEST(wbuf.next == mb2array[0].addr) ;
   TEST(wbuf.end  == mb2array[0].addr + mb2array[0].size) ;
   TEST(0 == size_wbuffer(&wbuf)) ;
   for (unsigned i = 0; i < lengthof(mb2array); ++i) {
      TEST(0 == mb2array[i].used) ;
   }

   // TEST shrink_wbuffer
   for (unsigned size = 0; size <= 64; ++size) {
      for (unsigned i = 0; i < lengthof(mb2array)-1; ++i) {
         mb2array[i].used = mb2array[i].size;
      }
      mb2array[lengthof(mb2array)-1].used = 0;
      wbuf.next = mb2array[lengthof(mb2array)-1].addr + mb2array[lengthof(mb2array)-1].size;
      wbuf.end  = mb2array[lengthof(mb2array)-1].addr + mb2array[lengthof(mb2array)-1].size;
      TEST(0 == shrink_wbuffer(&wbuf, size));
      unsigned nrblocks = size/mb2array[0].size;
      if (nrblocks == lengthof(mb2array)) --nrblocks;
      for (unsigned i = 0; i < nrblocks; ++i) {
         TEST(mb2array[i].used == mb2array[i].size);
      }
      TEST(0 == mb2array[nrblocks].used);
      TEST(wbuf.next == mb2array[nrblocks].addr+size-nrblocks*mb2array[0].size);
      TEST(wbuf.end  == mb2array[nrblocks].addr+mb2array[nrblocks].size);
      TEST(wbuf.impl == &mb2array[0]) ;
      TEST(wbuf.iimpl == &other_it) ;
   }

   // TEST appendbytes_wbuffer
   clear_wbuffer(&wbuf) ;
   s_other_alloc = 0 ;
   for (unsigned i = 0; i < lengthof(mb2array); ++i) {
      uint8_t * b = 0 ;
      TEST(0 == appendbytes_wbuffer(&wbuf, 16, &b)) ;
      TEST(b == &buffer[i*16]) ;
   }
   TEST(sizeof(buffer) == size_wbuffer(&wbuf)) ;
   TEST(s_other_alloc  == lengthof(mb2array)-1) ;

   // TEST appendbytes_wbuffer: ENOMEM
   TEST(ENOMEM == appendbytes_wbuffer(&wbuf, 1, (uint8_t**)0)) ;
   TEST(s_other_alloc == lengthof(mb2array)) ;

   // TEST appendcopy_wbuffer
   clear_wbuffer(&wbuf) ;
   s_other_alloc = 0 ;
   uint8_t buffer2[sizeof(buffer)] ;
   memset(buffer, 0, sizeof(buffer)) ;
   for (unsigned i = 0; i < sizeof(buffer2); ++i) {
      buffer2[i] = (uint8_t)i ;
   }
   static_assert(sizeof(buffer) == 64 && lengthof(mb2array) == 4, "16 bytes per block") ;
   TEST(0 == appendcopy_wbuffer(&wbuf, 32, buffer2)) ;
   TEST(1 == s_other_alloc) ;
   TEST(32 == size_wbuffer(&wbuf)) ;
   for (unsigned i = 2; i < lengthof(mb2array); ++i) {
      TEST(0 == appendcopy_wbuffer(&wbuf, 16, buffer2+i*16));
      TEST(i == s_other_alloc);
      TEST((i+1)*16 == size_wbuffer(&wbuf));
   }
   TEST(0 == memcmp(buffer2, buffer, sizeof(buffer)));

   // TEST appendcopy_wbuffer: ENOMEM
   s_other_alloc = 0;
   TEST(ENOMEM == appendcopy_wbuffer(&wbuf, 1, (uint8_t*)0));
   TEST(1 == s_other_alloc);

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_memory_wbuffer()
{
   if (test_variables())         goto ONERR;
   if (test_initfree())          goto ONERR;
   if (test_cstring_adapter())   goto ONERR;
   if (test_memblock_adapter())  goto ONERR;
   if (test_static_adapter())    goto ONERR;
   if (test_query())             goto ONERR;
   if (test_update())            goto ONERR;
   if (test_other_impl())        goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
