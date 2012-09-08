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
#include "C-kern/api/cache/objectcache_it.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/virtmemory.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/process.h"
#include "C-kern/api/io/filedescr.h"
#endif

// group: variables

/* variable: s_objectcache_interface
 * Contains single instance of interface <objectcache_it>. */
objectcache_it    s_objectcache_interface = {
                     &lockiobuffer_objectcache,
                     &unlockiobuffer_objectcache,
                  } ;

// group: init

int initthread_objectcache(/*out*/objectcache_iot * objectcache)
{
   int err ;
   const size_t    objsize   = sizeof(objectcache_t) ;
   objectcache_t * newobject = (objectcache_t*) malloc(objsize) ;

   if (!newobject) {
      err = ENOMEM ;
      TRACEOUTOFMEM_LOG(objsize) ;
      goto ONABORT ;
   }

   VALIDATE_INPARAM_TEST(0 == objectcache->object, ONABORT, ) ;

   err = init_objectcache(newobject) ;
   if (err) goto ONABORT ;

   objectcache->object = newobject;
   objectcache->iimpl  = &s_objectcache_interface ;

   return 0 ;
ONABORT:
   free(newobject) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freethread_objectcache(objectcache_iot * objectcache)
{
   int err ;
   objectcache_t * delobject = objectcache->object ;

   if (delobject) {
      assert(&s_objectcache_interface == objectcache->iimpl) ;

      objectcache->object = 0 ;
      objectcache->iimpl  = 0 ;

      err = free_objectcache(delobject) ;

      free(delobject) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int init_objectcache(/*out*/objectcache_t * cache )
{
   int err ;
   vm_block_t      iobuffer  = vm_block_INIT_FREEABLE ;

   err = init_vmblock( &iobuffer, (4096-1+sys_pagesize_vm()) / sys_pagesize_vm()) ;
   if (err) goto ONABORT ;

   static_assert( sizeof(*cache) == sizeof(iobuffer), "only one cached object" ) ;
   cache->iobuffer = iobuffer ;

   return 0 ;
ONABORT:
   (void) free_vmblock(&iobuffer) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_objectcache(objectcache_t * cache)
{
   int err ;

   err = free_vmblock(&cache->iobuffer) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int move_objectcache(objectcache_t * destination, objectcache_t * source)
{
   int err ;

   // move vm_rootbuffer
   if (source != destination) {
      err = free_vmblock(&destination->iobuffer) ;
      if (err) goto ONABORT ;

      MEMCOPY( &destination->iobuffer, (const typeof(source->iobuffer) *)&source->iobuffer ) ;
      source->iobuffer = (vm_block_t) vm_block_INIT_FREEABLE ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int lockiobuffer2_objectcache(objectcache_t * objectcache, /*out*/memblock_t ** iobuffer)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 == *iobuffer, ONABORT, ) ;

   *iobuffer = &objectcache->iobuffer ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int unlockiobuffer2_objectcache(objectcache_t * objectcache, memblock_t ** iobuffer)
{
   int err ;

   if (*iobuffer) {
      VALIDATE_INPARAM_TEST(&objectcache->iobuffer == *iobuffer, ONABORT, ) ;
      *iobuffer = 0 ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

void lockiobuffer_objectcache(objectcache_t * objectcache, /*out*/memblock_t ** iobuffer)
{
   int err ;

   err = lockiobuffer2_objectcache(objectcache, iobuffer) ;

   assert(!err && "lockiobuffer2_objectcache") ;
}

void unlockiobuffer_objectcache(objectcache_t * objectcache, memblock_t ** iobuffer)
{
   int err ;

   err = unlockiobuffer2_objectcache(objectcache, iobuffer) ;

   assert(!err && "unlockiobuffer2_objectcache") ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   objectcache_t   cache  = objectcache_INIT_FREEABLE ;
   objectcache_t   cache2 = objectcache_INIT_FREEABLE ;

   // TEST static init
   TEST(0 == cache.iobuffer.addr ) ;
   TEST(0 == cache.iobuffer.size ) ;

   // TEST init, double free
   TEST(0 == init_objectcache(&cache)) ;
   TEST(0 != cache.iobuffer.addr ) ;
   TEST(0 != cache.iobuffer.size ) ;
   TEST(0 == free_objectcache(&cache)) ;
   TEST(0 == cache.iobuffer.addr ) ;
   TEST(0 == cache.iobuffer.size ) ;
   TEST(0 == free_objectcache(&cache)) ;
   TEST(0 == cache.iobuffer.addr ) ;
   TEST(0 == cache.iobuffer.size ) ;

   // TEST move
   TEST(0 == init_objectcache(&cache)) ;
   TEST(0 == init_objectcache(&cache2)) ;
   void * start = cache.iobuffer.addr ;
   TEST(cache.iobuffer.addr  != 0) ;
   TEST(cache.iobuffer.size  == pagesize_vm()) ;
   TEST(cache2.iobuffer.addr != 0) ;
   TEST(cache2.iobuffer.size == pagesize_vm()) ;
   TEST(cache2.iobuffer.addr != start ) ;
   TEST(0 == move_objectcache(&cache2, &cache)) ;
   TEST(cache.iobuffer.addr  == 0) ;
   TEST(cache.iobuffer.size  == 0) ;
   TEST(cache2.iobuffer.addr == start ) ;
   TEST(cache2.iobuffer.size == pagesize_vm()) ;
   TEST(0 == free_objectcache(&cache)) ;
   TEST(0 == free_objectcache(&cache2)) ;
   TEST(cache.iobuffer.addr  == 0) ;
   TEST(cache.iobuffer.size  == 0) ;
   TEST(cache2.iobuffer.addr == 0) ;
   TEST(cache2.iobuffer.size == 0) ;

   // Test move to same address dos nothing
   TEST(0 == init_objectcache(&cache)) ;
   start = cache.iobuffer.addr ;
   TEST(cache.iobuffer.addr != 0) ;
   TEST(cache.iobuffer.size == pagesize_vm()) ;
   TEST(0 == move_objectcache(&cache, &cache)) ;
   TEST(cache.iobuffer.addr == start) ;
   TEST(cache.iobuffer.size == pagesize_vm()) ;
   TEST(0 == free_objectcache(&cache)) ;
   TEST(cache.iobuffer.addr == 0) ;
   TEST(cache.iobuffer.size == 0) ;

   return 0 ;
ONABORT:
   (void) free_objectcache(&cache) ;
   (void) free_objectcache(&cache2) ;
   return EINVAL ;
}

static int test_initthread(void)
{
   objectcache_iot   cache = objectcache_iot_INIT_FREEABLE;

   // TEST static init
   TEST(0 == cache.object) ;
   TEST(0 == cache.iimpl) ;

   // TEST exported interface
   TEST(s_objectcache_interface.lock_iobuffer   == &lockiobuffer_objectcache) ;
   TEST(s_objectcache_interface.unlock_iobuffer == &unlockiobuffer_objectcache) ;

   // TEST initthread and double free
   TEST(0 == initthread_objectcache( &cache )) ;
   TEST(cache.object != 0) ;
   TEST(cache.iimpl  == &s_objectcache_interface) ;
   TEST(0 == freethread_objectcache( &cache )) ;
   TEST(0 == cache.object) ;
   TEST(0 == cache.iimpl) ;
   TEST(0 == freethread_objectcache( &cache )) ;
   TEST(0 == cache.object) ;
   TEST(0 == cache.iimpl) ;

   // TEST EINVAL initthread
   cache.object = (objectcache_t*) 1 ;
   TEST(EINVAL == initthread_objectcache( &cache )) ;

   return 0 ;
ONABORT:
   (void) freethread_objectcache(&cache) ;
   return EINVAL ;
}

static int child_lockassert(objectcache_t * cache)
{
   CLEARBUFFER_LOG() ;
   if (cache) {
      vm_block_t * iobuffer  = (vm_block_t *) 1 ;
      lockiobuffer_objectcache(cache, &iobuffer) ;
   }
   return 0 ;
}

static int child_unlockassert(objectcache_t * cache)
{
   CLEARBUFFER_LOG() ;
   if (cache) {
      vm_block_t * iobuffer  = (vm_block_t *) 1 ;
      unlockiobuffer_objectcache(cache, &iobuffer) ;
   }
   return 0 ;
}

static int test_iobuffer(void)
{
   objectcache_t    cache     = objectcache_INIT_FREEABLE ;
   process_t        process   = process_INIT_FREEABLE ;
   vm_block_t     * iobuffer  = 0 ;
   int              pipefd[2] = { -1, -1 } ;
   process_result_t result ;
   char             buffer[512] ;

   // TEST lock / unlock
   TEST(0 == init_objectcache( &cache )) ;
   TEST(0 == iobuffer) ;
   TEST(0 == lockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(&cache.iobuffer == iobuffer) ;
   TEST(0 == unlockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == free_objectcache( &cache )) ;

   // TEST unlock twice
   TEST(0 == init_objectcache( &cache )) ;
   TEST(0 == iobuffer) ;
   TEST(0 == lockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 != iobuffer) ;
   TEST(0 == unlockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == unlockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == free_objectcache( &cache )) ;

   // TEST EINVAL lock
   TEST(0 == init_objectcache( &cache )) ;
   TEST(0 == iobuffer) ;
   TEST(0 == lockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 != iobuffer) ;
   TEST(EINVAL == lockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 == unlockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == free_objectcache( &cache )) ;

   // TEST EINVAL unlock
   TEST(0 == init_objectcache( &cache )) ;
   TEST(0 == iobuffer) ;
   TEST(0 == lockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 != iobuffer) ;
   TEST(0 == unlockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   iobuffer = (vm_block_t*) (uintptr_t) &iobuffer ;
   TEST(EINVAL == unlockiobuffer2_objectcache(&cache, &iobuffer)) ;
   TEST(0 == free_objectcache( &cache )) ;

   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK)) ;
   process_ioredirect_t ioredirect = process_ioredirect_INIT_DEVNULL ;
   setstderr_processioredirect(&ioredirect, pipefd[1]) ;

   // TEST assertion lockiobuffer_objectcache
   TEST(0 == init_objectcache( &cache )) ;
   TEST(0 == init_process(&process, child_lockassert, &cache, &ioredirect)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(process_state_ABORTED == result.state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == free_objectcache( &cache )) ;
   MEMSET0(&buffer) ;
   ssize_t read_bytes = read(pipefd[0], buffer, sizeof(buffer)-1) ;
   TEST(read_bytes > 50) ;
   PRINTF_LOG("%s", buffer) ;

   // TEST assertion unlockiobuffer_objectcache
   TEST(0 == init_objectcache( &cache )) ;
   TEST(0 == init_process(&process, child_unlockassert, &cache, &ioredirect)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(process_state_ABORTED == result.state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == free_objectcache( &cache )) ;
   MEMSET0(&buffer) ;
   read_bytes = read(pipefd[0], buffer, sizeof(buffer)-1) ;
   TEST(read_bytes > 50) ;
   PRINTF_LOG("%s", buffer) ;

   TEST(0 == free_filedescr(&pipefd[0])) ;
   TEST(0 == free_filedescr(&pipefd[1])) ;

   return 0 ;
ONABORT:
   (void) free_filedescr(&pipefd[0]) ;
   (void) free_filedescr(&pipefd[1]) ;
   (void) free_objectcache(&cache) ;
   return EINVAL ;
}

int unittest_cache_objectcache()
{
   resourceusage_t    usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_initthread())     goto ONABORT ;
   if (test_iobuffer())       goto ONABORT ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
