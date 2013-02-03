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
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


static int init_valuecache(/*out*/valuecache_t * valuecache)
{
   valuecache->pagesize_vm = sys_pagesize_vm() ;
   return 0 ;
}

static int free_valuecache(valuecache_t * valuecache)
{
   valuecache->pagesize_vm = 0 ;
   return 0 ;
}

int initonce_valuecache(/*out*/valuecache_t ** valuecache)
{
   int err ;
   valuecache_t * new_valuecache = 0 ;

   VALIDATE_INPARAM_TEST(0 == *valuecache, ONABORT,) ;

   new_valuecache = malloc(sizeof(valuecache_t)) ;
   if (!new_valuecache) {
      err = ENOMEM ;
      TRACEOUTOFMEM_LOG(sizeof(valuecache_t)) ;
      goto ONABORT ;
   }

   err = init_valuecache(new_valuecache) ;
   if (err) goto ONABORT ;

   *valuecache = new_valuecache ;

   return 0 ;
ONABORT:
   free(new_valuecache) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freeonce_valuecache(valuecache_t ** valuecache)
{
   int err ;
   valuecache_t * freeobj = *valuecache ;

   if (freeobj) {
      *valuecache = 0 ;

      err = free_valuecache(freeobj) ;

      free(freeobj) ;

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

   // TEST init, double free
   TEST(0 == init_valuecache(&valuecache)) ;
   TEST(valuecache.pagesize_vm)
   TEST(valuecache.pagesize_vm == sys_pagesize_vm()) ;
   TEST(0 == free_valuecache(&valuecache)) ;
   TEST(0 == valuecache.pagesize_vm) ;
   TEST(0 == free_valuecache(&valuecache)) ;
   TEST(0 == valuecache.pagesize_vm) ;

   return 0 ;
ONABORT:
   free_valuecache(&valuecache) ;
   return EINVAL ;
}

static int test_initonce(void)
{
   valuecache_t   valuecache = valuecache_INIT_FREEABLE ;
   valuecache_t * cache      = 0 ;
   valuecache_t * cache2     = 0 ;
   valuecache_t * old        = valuecache_maincontext() ;

   // TEST initonce, double freeonce
   TEST(0 == initonce_valuecache(&cache)) ;
   TEST(0 != cache) ;
   TEST(sys_pagesize_vm() == cache->pagesize_vm) ;
   TEST(0 == freeonce_valuecache(&cache)) ;
   TEST(0 == cache) ;
   TEST(0 == freeonce_valuecache(&cache)) ;
   TEST(0 == cache) ;

   // TEST EINVAL init
   cache = &valuecache ;
   TEST(EINVAL == initonce_valuecache(&cache)) ;
   TEST(&valuecache == cache) ;
   cache = 0 ;

   // TEST always new object
   TEST(0 == initonce_valuecache(&cache)) ;
   TEST(0 == initonce_valuecache(&cache2)) ;
   TEST(0 != cache) ;
   TEST(0 != cache2) ;
   TEST(cache != cache2) ;
   TEST(0 == freeonce_valuecache(&cache)) ;
   TEST(0 == freeonce_valuecache(&cache2)) ;
   TEST(0 == cache) ;
   TEST(0 == cache2) ;

   // TEST valuecache_maincontext()
   TEST(old) ;
   process_maincontext().valuecache = 0 ;
   TEST(valuecache_maincontext() == 0) ;
   process_maincontext().valuecache = old ;
   TEST(valuecache_maincontext() == old) ;

   // TEST pagesize_vm
   TEST(pagesize_vm() == sys_pagesize_vm()) ;
   old->pagesize_vm = 0 ;
   TEST(pagesize_vm() == 0) ;
   old->pagesize_vm = 512 ;
   TEST(pagesize_vm() == 512) ;
   old->pagesize_vm = 12345 ;
   TEST(pagesize_vm() == 12345) ;
   old->pagesize_vm = sys_pagesize_vm() ;
   TEST(pagesize_vm() == sys_pagesize_vm()) ;

   return 0 ;
ONABORT:
   process_maincontext().valuecache = old ;
   old->pagesize_vm = sys_pagesize_vm() ;
   if (     cache
         && cache != &valuecache) {
      freeonce_valuecache(&cache) ;
   }
   freeonce_valuecache(&cache2) ;
   free_valuecache(&valuecache) ;
   return EINVAL ;
}

int unittest_cache_valuecache()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_initonce())    goto ONABORT ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
