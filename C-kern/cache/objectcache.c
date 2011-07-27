/* title: Objectcache impl
   Implements <Objectcache>.

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

   file: C-kern/api/cache/objectcache.h
    Header file of <Objectcache>.

   file: C-kern/cache/objectcache.c
    Implementation file <Objectcache impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/os/virtmemory.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


int initumgebung_objectcache(objectcache_t ** objectcache)
{
   int err ;

   err = new_objectcache(objectcache) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_objectcache(objectcache_t ** objectcache)
{
   int err ;

   err = delete_objectcache(objectcache) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

/* function: new_objectcache
 * Allocates storage and initialized cached objects
 * as INIT_FREEABLE. */
int new_objectcache( /*out*/objectcache_t ** cache )
{
   int err ;
   vm_block_t      iobuffer  = vm_block_INIT_FREEABLE ;
   objectcache_t * new_cache = 0 ;
   const size_t    memsize   = sizeof(objectcache_t) ;

   if (*cache) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   err = init_vmblock( &iobuffer, (4096-1+sys_pagesize_vm()) / sys_pagesize_vm()) ;
   if (err) goto ABBRUCH ;

   new_cache = (objectcache_t *) malloc(memsize) ;
   if (!new_cache) {
      LOG_OUTOFMEMORY(memsize) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   static_assert( sizeof(*new_cache) == sizeof(iobuffer), "only one cached object" ) ;
   new_cache->iobuffer = iobuffer ;

   *cache = new_cache ;

   return 0 ;
ABBRUCH:
   free(new_cache) ;
   (void) free_vmblock(&iobuffer) ;
   LOG_ABORT(err) ;
   return err ;
}

/* function: delete_objectcache
 * Frees cached objects and allocated storage. */
int delete_objectcache( objectcache_t ** cache )
{
   int err ;

   objectcache_t * delobject = *cache ;
   if (delobject) {
      *cache = 0 ;

      err = free_vmblock(&delobject->iobuffer) ;

      free(delobject) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}


int move_objectcache( objectcache_t * destination, objectcache_t * source )
{
   int err ;

   // move vm_rootbuffer
   if (source != destination) {
      err = free_vmblock(&destination->iobuffer) ;
      if (err) goto ABBRUCH ;

      MEMCOPY( &destination->iobuffer, (const typeof(source->iobuffer) *)&source->iobuffer ) ;
      source->iobuffer = (vm_block_t) vm_block_INIT_FREEABLE ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_cache_objectcache,ABBRUCH)

static int test_initfree(void)
{
   objectcache_t    * cache  = 0 ;
   objectcache_t    * cache2 = 0 ;

   // TEST init, double free
   TEST(0 == new_objectcache(&cache)) ;
   TEST(cache) ;
   TEST(0 != cache->iobuffer.addr ) ;
   TEST(0 != cache->iobuffer.size ) ;
   TEST(0 == delete_objectcache(&cache)) ;
   TEST(! cache)
   TEST(0 == delete_objectcache(&cache)) ;
   TEST(! cache)

   // TEST move
   TEST(0 == new_objectcache(&cache)) ;
   TEST(0 == new_objectcache(&cache2)) ;
   TEST(cache) ;
   TEST(cache2) ;
   void * start = cache->iobuffer.addr ;
   TEST(cache->iobuffer.addr  != 0 ) ;
   TEST(cache->iobuffer.size  == pagesize_vm()) ;
   TEST(cache2->iobuffer.addr != 0 ) ;
   TEST(cache2->iobuffer.size == pagesize_vm()) ;
   TEST(cache2->iobuffer.addr != start ) ;
   TEST(0 == move_objectcache(cache2, cache)) ;
   TEST(cache->iobuffer.addr  == 0 ) ;
   TEST(cache->iobuffer.size  == 0) ;
   TEST(cache2->iobuffer.addr == start ) ;
   TEST(cache2->iobuffer.size == pagesize_vm()) ;
   TEST(0 == delete_objectcache(&cache)) ;
   TEST(0 == delete_objectcache(&cache2)) ;
   TEST(0 == cache)
   TEST(0 == cache2)

   // Test move to same address dos nothing
   TEST(0 == new_objectcache(&cache)) ;
   start = cache->iobuffer.addr ;
   TEST(cache->iobuffer.addr != 0) ;
   TEST(cache->iobuffer.size == pagesize_vm()) ;
   TEST(0 == move_objectcache(cache, cache)) ;
   TEST(cache->iobuffer.addr == start) ;
   TEST(cache->iobuffer.size == pagesize_vm()) ;
   TEST(0 == delete_objectcache(&cache)) ;
   TEST(0 == cache)

   return 0 ;
ABBRUCH:
   (void) delete_objectcache(&cache) ;
   (void) delete_objectcache(&cache2) ;
   return EINVAL ;
}

static int test_initumgebung(void)
{
   umgebung_t tempumg ;

   // TEST initumgebung_objectcache and double free
   MEMSET0(&tempumg) ;
   TEST(0 == initumgebung_objectcache( &tempumg.objectcache )) ;
   TEST(0 != tempumg.objectcache ) ;
   TEST(0 == freeumgebung_objectcache( &tempumg.objectcache )) ;
   TEST(0 == tempumg.objectcache ) ;
   TEST(0 == freeumgebung_objectcache( &tempumg.objectcache )) ;
   TEST(0 == tempumg.objectcache ) ;

   return 0 ;
ABBRUCH:
   (void) delete_objectcache(&tempumg.objectcache) ;
   return EINVAL ;
}

int unittest_cache_objectcache()
{
   resourceusage_t    usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_initumgebung())   goto ABBRUCH ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
