/* title: Objectcache-MT impl
   Implements <Objectcache-MT>.

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

   file: C-kern/api/cache/objectcachemt.h
    Header file of <Objectcache-MT>.

   file: C-kern/cache/objectcachemt.c
    Implementation file <Objectcache-MT impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/cache/objectcachemt.h"
#include "C-kern/api/err.h"
#include "C-kern/api/aspect/interface/objectcache_it.h"
#include "C-kern/api/os/sync/mutex.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/os/process.h"
#endif

// group: types

/* typedef: objectcachemt_it
 * Declares multithread safe interface, see <objectcache_it>. */
objectcache_it_DECLARE(1, objectcachemt_it, objectcachemt_t)

// group: variables

/* variable: s_objectcache_interface
 * Contains single instance of interface <objectcache_mit>. */
objectcachemt_it  s_objectcachemt_interface = {
                      &lockiobuffer_objectcachemt
                     ,&unlockiobuffer_objectcachemt
                  } ;

// group: init

int initumgebung_objectcachemt(/*out*/objectcache_oit * objectcache)
{
   int err ;
   const size_t      objsize   = sizeof(objectcachemt_t) ;
   objectcachemt_t * newobject = (objectcachemt_t*) malloc(objsize) ;

   if (!newobject) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(objsize) ;
      goto ABBRUCH ;
   }

   PRECONDITION_INPUT(0 == objectcache->object, ABBRUCH, ) ;

   err = init_objectcachemt(newobject) ;
   if (err) goto ABBRUCH ;

   objectcache->object    = (objectcache_t*) newobject ;
   objectcache->functable = (objectcache_it*) &s_objectcachemt_interface ;

   return 0 ;
ABBRUCH:
   free(newobject) ;
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_objectcachemt(objectcache_oit * objectcache)
{
   int err ;
   objectcachemt_t * delobject = (objectcachemt_t *) objectcache->object ;

   if (delobject) {
      assert((objectcache_it*) &s_objectcachemt_interface == objectcache->functable) ;

      objectcache->object    = 0 ;
      objectcache->functable = 0 ;

      err = free_objectcachemt(delobject) ;

      free(delobject) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int init_objectcachemt(/*out*/objectcachemt_t * cache)
{
   int err ;

   cache->objectcache = (objectcache_t) objectcache_INIT_FREEABLE ;
   err = init_objectcache(&cache->objectcache) ;
   if (err) goto ABBRUCH ;

   err = init_mutex(&cache->lock) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) free_objectcache(&cache->objectcache) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_objectcachemt(objectcachemt_t * cache )
{
   int err ;
   int err2 ;

   err = free_mutex(&cache->lock) ;

   err2 = free_objectcache(&cache->objectcache) ;
   if (err2) err = err2 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

void lockiobuffer_objectcachemt(objectcachemt_t * objectcache, /*out*/memblock_t ** iobuffer)
{
   slock_mutex(&objectcache->lock) ;
   lockiobuffer_objectcache(&objectcache->objectcache, iobuffer) ;
}

void unlockiobuffer_objectcachemt(objectcachemt_t * objectcache, memblock_t ** iobuffer)
{
   unlockiobuffer_objectcache(&objectcache->objectcache, iobuffer) ;
   sunlock_mutex(&objectcache->lock) ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_cache_objectcachemt,ABBRUCH)

static int test_initfree(void)
{
   objectcachemt_t   cache  = objectcachemt_INIT_FREEABLE ;

   // TEST static init
   TEST(0 == lock_mutex(&cache.lock)) ;
   TEST(0 == unlock_mutex(&cache.lock)) ;
   TEST(0 == cache.objectcache.iobuffer.addr ) ;
   TEST(0 == cache.objectcache.iobuffer.size ) ;

   // TEST init, double free
   TEST(0 == init_objectcachemt(&cache)) ;
   TEST(0 != cache.objectcache.iobuffer.addr ) ;
   TEST(0 != cache.objectcache.iobuffer.size ) ;
   TEST(0 == lock_mutex(&cache.lock)) ;
   TEST(0 == unlock_mutex(&cache.lock)) ;
   TEST(0 == free_objectcachemt(&cache)) ;
   TEST(0 == cache.objectcache.iobuffer.addr ) ;
   TEST(0 == cache.objectcache.iobuffer.size ) ;
   TEST(EINVAL == lock_mutex(&cache.lock)) ;
   TEST(0 == free_objectcachemt(&cache)) ;
   TEST(0 == cache.objectcache.iobuffer.addr ) ;
   TEST(0 == cache.objectcache.iobuffer.size ) ;

   // TEST EDEADLOCK lock
   TEST(0 == init_objectcachemt(&cache)) ;
   TEST(0 == lock_mutex(&cache.lock)) ;
   TEST(EDEADLOCK == lock_mutex(&cache.lock)) ;
   TEST(0 == unlock_mutex(&cache.lock)) ;
   TEST(0 == free_objectcachemt(&cache)) ;

   return 0 ;
ABBRUCH:
   (void) free_objectcachemt(&cache) ;
   return EINVAL ;
}

static int test_initumgebung(void)
{
   objectcache_oit   cache = objectcache_oit_INIT_FREEABLE;

   // TEST static init
   TEST(0 == cache.object) ;
   TEST(0 == cache.functable) ;

   // TEST exported interface
   TEST(s_objectcachemt_interface.lock_iobuffer   == &lockiobuffer_objectcachemt) ;
   TEST(s_objectcachemt_interface.unlock_iobuffer == &unlockiobuffer_objectcachemt) ;

   // TEST initumgebung and double free
   TEST(0 == initumgebung_objectcachemt( &cache )) ;
   TEST(cache.object    != 0) ;
   TEST(cache.functable == (objectcache_it*) &s_objectcachemt_interface) ;
   TEST(0 == freeumgebung_objectcachemt( &cache )) ;
   TEST(0 == cache.object) ;
   TEST(0 == cache.functable) ;
   TEST(0 == freeumgebung_objectcachemt( &cache )) ;
   TEST(0 == cache.object) ;
   TEST(0 == cache.functable) ;

   // TEST EINVAL initumgebung
   cache.object = (objectcache_t*) 1 ;
   TEST(EINVAL == initumgebung_objectcachemt( &cache )) ;

   return 0 ;
ABBRUCH:
   (void) freeumgebung_objectcachemt(&cache) ;
   return EINVAL ;
}

typedef struct childparam_t   childparam_t ;

struct childparam_t {
   objectcachemt_t   * cache ;
   memblock_t        * iobuffer ;
} ;

static int child_lockassert(childparam_t * start_arg)
{
   LOG_CLEARBUFFER() ;
   if (start_arg) {
      int err = lock_mutex(&start_arg->cache->lock) ;
      if (!err && 0 != start_arg->iobuffer) {
         err = unlock_mutex(&start_arg->cache->lock) ;
      }
      if (!err) {
         lockiobuffer_objectcachemt(start_arg->cache, &start_arg->iobuffer) ;
      }
   }
   return 0 ;
}

static int child_unlockassert(childparam_t * start_arg)
{
   LOG_CLEARBUFFER() ;
   if (start_arg) {
      unlockiobuffer_objectcachemt(start_arg->cache, &start_arg->iobuffer) ;
   }
   return 0 ;
}

static int test_iobuffer(void)
{
   objectcachemt_t  cache     = objectcachemt_INIT_FREEABLE ;
   process_t        process   = process_INIT_FREEABLE ;
   int              pipefd[2] = { -1, -1 } ;
   process_result_t result ;
   childparam_t     start_arg ;
   char             buffer[512] ;

   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK)) ;
   process_ioredirect_t ioredirect = process_ioredirect_INIT_DEVNULL ;
   setstderr_processioredirect(&ioredirect, pipefd[1]) ;

   // TEST assertion lockiobuffer 1
   TEST(0 == init_objectcachemt( &cache )) ;
   start_arg.cache    = &cache ;
   start_arg.iobuffer = (memblock_t*) 1 ;
   TEST(0 == init_process(&process, child_lockassert, &start_arg, &ioredirect)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(process_state_ABORTED == result.state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == free_objectcachemt( &cache )) ;
   MEMSET0(&buffer) ;
   ssize_t read_bytes = read(pipefd[0], buffer, sizeof(buffer)-1) ;
   TEST(read_bytes > 50) ;
   LOG_PRINTF("%s", buffer) ;

   // TEST assertion lockiobuffer 2
   TEST(0 == init_objectcachemt( &cache )) ;
   start_arg.cache    = &cache ;
   start_arg.iobuffer = (memblock_t*) 0 ;
   TEST(0 == init_process(&process, child_lockassert, &start_arg, &ioredirect)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(process_state_ABORTED == result.state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == free_objectcachemt( &cache )) ;
   MEMSET0(&buffer) ;
   read_bytes = read(pipefd[0], buffer, sizeof(buffer)-1) ;
   TEST(read_bytes > 50) ;
   LOG_PRINTF("%s", buffer) ;

   // TEST assertion unlockiobuffer 1
   TEST(0 == init_objectcachemt( &cache )) ;
   start_arg.cache    = &cache ;
   start_arg.iobuffer = (memblock_t*) 1 ;
   TEST(0 == init_process(&process, child_unlockassert, &start_arg, &ioredirect)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(process_state_ABORTED == result.state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == free_objectcachemt( &cache )) ;
   MEMSET0(&buffer) ;
   read_bytes = read(pipefd[0], buffer, sizeof(buffer)-1) ;
   TEST(read_bytes > 50) ;
   LOG_PRINTF("%s", buffer) ;

   // TEST assertion unlockiobuffer 2
   TEST(0 == init_objectcachemt( &cache )) ;
   start_arg.cache    = &cache ;
   start_arg.iobuffer = (memblock_t*) 0 ;
   TEST(0 == lock_mutex(&cache.lock)) ;
   TEST(0 == init_process(&process, child_unlockassert, &start_arg, &ioredirect)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(0 == unlock_mutex(&cache.lock)) ;
   TEST(process_state_ABORTED == result.state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == free_objectcachemt( &cache )) ;
   MEMSET0(&buffer) ;
   read_bytes = read(pipefd[0], buffer, sizeof(buffer)-1) ;
   TEST(read_bytes > 50) ;
   LOG_PRINTF("%s", buffer) ;

   TEST(0 == close(pipefd[0])) ;
   TEST(0 == close(pipefd[1])) ;
   pipefd[0] = pipefd[1] = -1 ;

   return 0 ;
ABBRUCH:
   if (-1 != pipefd[0]) close(pipefd[0]) ;
   if (-1 != pipefd[1]) close(pipefd[1]) ;
   (void) free_objectcachemt(&cache) ;
   return EINVAL ;
}

int unittest_cache_objectcachemt()
{
   resourceusage_t    usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_initumgebung())   goto ABBRUCH ;
   if (test_iobuffer())       goto ABBRUCH ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
