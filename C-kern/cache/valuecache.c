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
#include "C-kern/api/os/virtmemory.h"
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

int initumgebung_valuecache(/*out*/valuecache_t ** valuecache, umgebung_shared_t * shared)
{
   int err ;
   valuecache_t * new_valuecache = 0 ;

   (void) shared ;

   VALIDATE_INPARAM_TEST(0 == *valuecache, ABBRUCH,) ;

   new_valuecache = malloc(sizeof(valuecache_t)) ;
   if (!new_valuecache) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(sizeof(valuecache_t)) ;
      goto ABBRUCH ;
   }

   err = init_valuecache(new_valuecache) ;
   if (err) goto ABBRUCH ;

   *valuecache = new_valuecache ;

   return 0 ;
ABBRUCH:
   free(new_valuecache) ;
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_valuecache(valuecache_t ** valuecache, umgebung_shared_t * shared)
{
   int err ;
   valuecache_t * freeobj = *valuecache ;

   (void) shared ;

   if (freeobj) {
      *valuecache = 0 ;

      err = free_valuecache(freeobj) ;

      free(freeobj) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}



#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION, ABBRUCH)

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
ABBRUCH:
   free_valuecache(&valuecache) ;
   return EINVAL ;
}

static int test_umgebunginit(void)
{
   valuecache_t   valuecache = valuecache_INIT_FREEABLE ;
   valuecache_t * cache      = 0 ;
   valuecache_t * cache2     = 0 ;
   valuecache_t * old        = valuecache_umgebung() ;

   // TEST initumgebung, double freeumgebung
   TEST(0 == initumgebung_valuecache(&cache, 0)) ;
   TEST(0 != cache) ;
   TEST(sys_pagesize_vm() == cache->pagesize_vm) ;
   TEST(0 == freeumgebung_valuecache(&cache, 0)) ;
   TEST(0 == cache) ;
   TEST(0 == freeumgebung_valuecache(&cache, 0)) ;
   TEST(0 == cache) ;

   // TEST EINVAL init
   cache = &valuecache ;
   TEST(EINVAL == initumgebung_valuecache(&cache, 0)) ;
   TEST(&valuecache == cache) ;
   cache = 0 ;

   // TEST always new object
   TEST(0 == initumgebung_valuecache(&cache, 0)) ;
   TEST(0 == initumgebung_valuecache(&cache2, 0)) ;
   TEST(0 != cache) ;
   TEST(0 != cache2) ;
   TEST(cache != cache2) ;
   TEST(0 == freeumgebung_valuecache(&cache, 0)) ;
   TEST(0 == freeumgebung_valuecache(&cache2, 0)) ;
   TEST(0 == cache) ;
   TEST(0 == cache2) ;

   // TEST valuecache_umgebung()
   TEST(old) ;
   umgebung().shared->valuecache = 0 ;
   TEST(valuecache_umgebung() == 0) ;
   umgebung().shared->valuecache = old ;
   TEST(valuecache_umgebung() == old) ;

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
ABBRUCH:
   umgebung().shared->valuecache = old ;
   old->pagesize_vm = sys_pagesize_vm() ;
   if (  cache
      && cache != &valuecache) {
      freeumgebung_valuecache(&cache, 0) ;
   }
   freeumgebung_valuecache(&cache2, 0) ;
   free_valuecache(&valuecache) ;
   return EINVAL ;
}

int unittest_cache_valuecache()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_umgebunginit())   goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

#endif
