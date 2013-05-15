/* title: Valuecache impl
   Implements <Valuecache>.

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

   file: C-kern/api/cache/valuecache.h
    Header file of <Valuecache>.

   file: C-kern/cache/valuecache.c
    Implementation file <Valuecache impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


static int init_valuecache(/*out*/valuecache_t * valuecache)
{
   valuecache->pagesize_vm     = sys_pagesize_vm() ;
   valuecache->log2pagesize_vm = log2_int(valuecache->pagesize_vm) ;
   return 0 ;
}

static int free_valuecache(valuecache_t * valuecache)
{
   valuecache->pagesize_vm     = 0 ;
   valuecache->log2pagesize_vm = 0 ;
   return 0 ;
}

int initonce_valuecache(/*out*/valuecache_t ** valuecache)
{
   int err ;
   valuecache_t * new_valuecache ;

   new_valuecache = allocstatic_processcontext(&process_maincontext(), sizeof(valuecache_t)) ;
   if (!new_valuecache) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   err = init_valuecache(new_valuecache) ;
   if (err) goto ONABORT ;

   *valuecache = new_valuecache ;

   return 0 ;
ONABORT:
   if (new_valuecache) {
      freestatic_processcontext(&process_maincontext(), sizeof(valuecache_t)) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int freeonce_valuecache(valuecache_t ** valuecache)
{
   int err ;
   valuecache_t * delobj = *valuecache ;

   if (delobj) {
      *valuecache = 0 ;

      err = free_valuecache(delobj) ;

      int err2 = freestatic_processcontext(&process_maincontext(), sizeof(valuecache_t)) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   valuecache_t   valuecache = valuecache_INIT_FREEABLE ;

   // TEST static init
   TEST(0 == valuecache.pagesize_vm) ;

   // TEST init_valuecache
   TEST(0 == init_valuecache(&valuecache)) ;
   TEST(valuecache.pagesize_vm) ;
   TEST(valuecache.log2pagesize_vm) ;
   TEST(valuecache.pagesize_vm == sys_pagesize_vm()) ;
   TEST(valuecache.pagesize_vm == 1u << valuecache.log2pagesize_vm) ;

   // TEST free_valuecache
   TEST(0 == free_valuecache(&valuecache)) ;
   TEST(0 == valuecache.pagesize_vm) ;
   TEST(0 == valuecache.log2pagesize_vm) ;
   TEST(0 == free_valuecache(&valuecache)) ;
   TEST(0 == valuecache.pagesize_vm) ;
   TEST(0 == valuecache.log2pagesize_vm) ;

   return 0 ;
ONABORT:
   free_valuecache(&valuecache) ;
   return EINVAL ;
}

static int test_initonce(void)
{
   valuecache_t * cache      = 0 ;
   size_t         oldsize ;

   // prepare
   oldsize = sizestatic_processcontext(&process_maincontext()) ;

   // TEST initonce_valuecache
   TEST(0 == initonce_valuecache(&cache)) ;
   TEST(0 != cache) ;
   TEST(cache->pagesize_vm == sys_pagesize_vm()) ;
   TEST(cache->pagesize_vm == 1u << cache->log2pagesize_vm) ;
   TEST(sizestatic_processcontext(&process_maincontext()) == oldsize + sizeof(valuecache_t)) ;

   // TEST freeonce_valuecache
   TEST(0 == freeonce_valuecache(&cache)) ;
   TEST(0 == cache) ;
   TEST(sizestatic_processcontext(&process_maincontext()) == oldsize) ;
   TEST(0 == freeonce_valuecache(&cache)) ;
   TEST(0 == cache) ;
   TEST(sizestatic_processcontext(&process_maincontext()) == oldsize) ;

   // TEST initonce_valuecache: ENOMEM
   while (0 != allocstatic_processcontext(&process_maincontext(), sizeof(valuecache_t))) {
   }
   valuecache_t * dummy = 0 ;
   TEST(ENOMEM == initonce_valuecache(&dummy)) ;
   while (sizestatic_processcontext(&process_maincontext()) > oldsize) {
      freestatic_processcontext(&process_maincontext(), 1) ;
   }
   TEST(sizestatic_processcontext(&process_maincontext()) == oldsize) ;

   return 0 ;
ONABORT:
   while (sizestatic_processcontext(&process_maincontext()) > oldsize) {
      freestatic_processcontext(&process_maincontext(), 1) ;
   }
   freeonce_valuecache(&cache) ;
   return EINVAL ;
}

static int test_queryvalues(void)
{
   valuecache_t * vc    = valuecache_maincontext() ;
   valuecache_t   oldvc = *vc ;

   // TEST log2pagesize_vm
   TEST(log2pagesize_vm() == oldvc.log2pagesize_vm) ;
   TEST(pagesize_vm()     == 1u << log2pagesize_vm()) ;
   vc->log2pagesize_vm = 0 ;
   TEST(log2pagesize_vm() == 0) ;
   vc->log2pagesize_vm = 7 ;
   TEST(log2pagesize_vm() == 7) ;
   vc->log2pagesize_vm = oldvc.log2pagesize_vm ;
   TEST(pagesize_vm()     == 1u << log2pagesize_vm()) ;

   // TEST pagesize_vm
   TEST(pagesize_vm() == oldvc.pagesize_vm) ;
   TEST(pagesize_vm() == sys_pagesize_vm()) ;
   vc->pagesize_vm = 0 ;
   TEST(pagesize_vm() == 0) ;
   vc->pagesize_vm = 512 ;
   TEST(pagesize_vm() == 512) ;
   vc->pagesize_vm = oldvc.pagesize_vm ;
   TEST(pagesize_vm() == sys_pagesize_vm()) ;

   return 0 ;
ONABORT:
   *vc = oldvc ;
   return EINVAL ;
}


int unittest_cache_valuecache()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_initonce())    goto ONABORT ;
   if (test_queryvalues()) goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
