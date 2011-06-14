/* title: ObjectCache impl
   Implements <ObjectCache>.

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

   file: C-kern/api/umgebung/object_cache.h
    Header file of <ObjectCache>.

   file: C-kern/umgebung/object_cache.c
    Implementation file <ObjectCache impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung/object_cache.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/os/virtmemory.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

// module internal declarations
static int new_objectcache   ( /*out*/object_cache_t ** cache ) ;
static int delete_objectcache( object_cache_t ** cache ) ;


int initumgebung_objectcache(object_cache_t ** objectcache)
{
   int err ;

   err = new_objectcache(objectcache) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_objectcache(object_cache_t ** objectcache)
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
static int new_objectcache( /*out*/object_cache_t ** cache )
{
   int err ;
   size_t memsize = sizeof(object_cache_t)
                  + sizeof(*((object_cache_t*)0)->vm_rootbuffer) ;

   if (*cache) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   object_cache_t * new_cache = (object_cache_t *) malloc(memsize) ;
   if (!new_cache) {
      LOG_OUTOFMEMORY(memsize) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   new_cache->vm_rootbuffer  = (vm_block_t*) (((uint8_t*)(&new_cache[1])) + 0) ;
   *new_cache->vm_rootbuffer = (vm_block_t) vm_block_INIT_FREEABLE ;
   // only one cached object
   static_assert_void( sizeof(new_cache->vm_rootbuffer) == sizeof(object_cache_t) ) ;

   *cache = new_cache ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: delete_objectcache
 * Frees cached objects and allocated storage. */
static int delete_objectcache( object_cache_t ** cache )
{
   int err = 0 ;

   object_cache_t * delobject = *cache ;
   if (delobject) {
      *cache = NULL ;

      err = free_vmblock(delobject->vm_rootbuffer) ;

      free(delobject) ;
   }

   if (err) goto ABBRUCH ;
   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}


int move_objectcache( object_cache_t * destination, object_cache_t * source )
{
   int err ;

   // move vm_rootbuffer
   if (source != destination) {
      err = free_vmblock(destination->vm_rootbuffer) ;
      if (err) goto ABBRUCH ;

      MEMCOPY( destination->vm_rootbuffer, (const typeof(*source->vm_rootbuffer)*)source->vm_rootbuffer ) ;
      *source->vm_rootbuffer = (vm_block_t) vm_block_INIT_FREEABLE ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_objectcache,ABBRUCH)

int unittest_umgebung_objectcache()
{
   object_cache_t            * cache = 0 ;
   object_cache_t           * cache2 = 0 ;
   vm_mappedregions_t mappedregions  = vm_mappedregions_INIT_FREEABLE ;
   vm_mappedregions_t mappedregions2 = vm_mappedregions_INIT_FREEABLE ;

   TEST(0 == init_vmmappedregions(&mappedregions)) ;

   // TEST init, double free
   TEST(0 == new_objectcache(&cache)) ;
   TEST(cache) ;
   TEST(cache->vm_rootbuffer) ;
   TEST(! cache->vm_rootbuffer->addr ) ;
   TEST(! cache->vm_rootbuffer->size ) ;
   TEST(0 == delete_objectcache(&cache)) ;
   TEST(! cache)
   TEST(0 == delete_objectcache(&cache)) ;
   TEST(! cache)

   // TEST initumgebung_objectcache and double free
   umgebung_t tempumg ;
   MEMSET0(&tempumg) ;
   TEST(0 == initumgebung_objectcache( &tempumg.objectcache )) ;
   TEST( tempumg.objectcache ) ;
   TEST(0 == freeumgebung_objectcache( &tempumg.objectcache )) ;
   TEST( ! tempumg.objectcache ) ;
   TEST(0 == freeumgebung_objectcache( &tempumg.objectcache )) ;
   TEST( ! tempumg.objectcache ) ;

   // TEST move cache -> cache2
   TEST(0 == new_objectcache(&cache)) ;
   TEST(0 == new_objectcache(&cache2)) ;
   TEST(cache) ;
   TEST(cache2) ;
   TEST(! cache2->vm_rootbuffer->addr ) ;
   TEST(! cache2->vm_rootbuffer->size ) ;
   TEST(0 == init_vmblock( cache->vm_rootbuffer, 1 )) ;
   TEST(cache->vm_rootbuffer->addr != 0 ) ;
   TEST(cache->vm_rootbuffer->size == pagesize_vm()) ;
   void * start = cache->vm_rootbuffer->addr ;
   TEST(0 == move_objectcache(cache2, cache)) ;
   TEST(cache2->vm_rootbuffer->addr == start) ;
   TEST(cache2->vm_rootbuffer->size == pagesize_vm()) ;
   TEST(! cache->vm_rootbuffer->addr ) ;
   TEST(! cache->vm_rootbuffer->size ) ;

   // Test move cache2 -> cach2 does nothing !
   TEST(cache2->vm_rootbuffer->addr == start) ;
   TEST(cache2->vm_rootbuffer->size == pagesize_vm()) ;
   TEST(0 == move_objectcache(cache2, cache2)) ;
   TEST(cache2->vm_rootbuffer->addr == start) ;
   TEST(cache2->vm_rootbuffer->size == pagesize_vm()) ;


   // TEST free of vm_rootbuffer
   TEST(0 == delete_objectcache(&cache)) ;
   TEST(! cache)
   TEST(0 == delete_objectcache(&cache2)) ;
   TEST(! cache2)
   TEST(0 == init_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == free_vmmappedregions(&mappedregions2)) ;

   return 0 ;
ABBRUCH:
   free_vmmappedregions(&mappedregions) ;
   free_vmmappedregions(&mappedregions2) ;
   delete_objectcache(&cache) ;
   delete_objectcache(&cache2) ;
   return 1 ;
}

#endif
