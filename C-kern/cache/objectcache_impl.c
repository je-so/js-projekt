/* title: Objectcache impl

   Implements <Objectcache>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/cache/objectcache_impl.h
    Header file of <Objectcache>.

   file: C-kern/cache/objectcache_impl.c
    Implementation file <Objectcache impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/cache/objectcache_impl.h"
#include "C-kern/api/err.h"
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/platform/task/process.h"
#endif

// section: objectcacheimpl

// group: static variables

objectcache_it_DECLARE(objectcache_impl_it, objectcache_impl_t) ;

/* variable: s_objectcacheimpl_interface
 * Contains single instance of interface <objectcache_it>. */
static objectcache_impl_it  s_objectcacheimpl_interface = {
   &lockiobuffer_objectcacheimpl,
   &unlockiobuffer_objectcacheimpl
};

// group: initthread

objectcache_it* interface_objectcacheimpl(void)
{
   return cast_objectcacheit(&s_objectcacheimpl_interface, objectcache_impl_t);
}

// group: lifetime

int init_objectcacheimpl(/*out*/objectcache_impl_t * cache)
{
   int err ;
   memblock_t  iobuffer = memblock_FREE ;

   err = ALLOC_PAGECACHE(pagesize_4096, &iobuffer) ;
   if (err) goto ONERR;

   static_assert(sizeof(*cache) == sizeof(iobuffer), "only one cached object") ;
   *cast_memblock(&cache->iobuffer, ) = iobuffer;

   return 0;
ONERR:
   (void) RELEASE_PAGECACHE(&iobuffer);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_objectcacheimpl(objectcache_impl_t * cache)
{
   int err ;

   err = RELEASE_PAGECACHE(cast_memblock(&cache->iobuffer, )) ;

   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

// group: access

static int lockiobuffer2_objectcacheimpl(objectcache_impl_t * objectcache, /*out*/memblock_t ** iobuffer)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 == *iobuffer, ONERR, ) ;

   *iobuffer = cast_memblock(&objectcache->iobuffer,) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

static int unlockiobuffer2_objectcacheimpl(objectcache_impl_t * objectcache, memblock_t ** iobuffer)
{
   int err ;

   if (*iobuffer) {
      VALIDATE_INPARAM_TEST(cast_memblock(&objectcache->iobuffer,) == *iobuffer, ONERR, ) ;
      *iobuffer = 0 ;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

void lockiobuffer_objectcacheimpl(objectcache_impl_t * objectcache, /*out*/memblock_t ** iobuffer)
{
   int err ;

   err = lockiobuffer2_objectcacheimpl(objectcache, iobuffer) ;

   assert(!err && "lockiobuffer2_objectcacheimpl") ;
}

void unlockiobuffer_objectcacheimpl(objectcache_impl_t * objectcache, memblock_t ** iobuffer)
{
   int err ;

   err = unlockiobuffer2_objectcacheimpl(objectcache, iobuffer) ;

   assert(!err && "unlockiobuffer2_objectcacheimpl") ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   objectcache_impl_t   cache = objectcache_impl_FREE ;

   // TEST objectcache_impl_FREE
   TEST(0 == cache.iobuffer.addr) ;
   TEST(0 == cache.iobuffer.size) ;

   // TEST init_objectcacheimpl, free_objectcacheimpl
   TEST(0 == init_objectcacheimpl(&cache)) ;
   TEST(0 != cache.iobuffer.addr) ;
   TEST(4096 == cache.iobuffer.size) ;
   TEST(0 == free_objectcacheimpl(&cache)) ;
   TEST(0 == cache.iobuffer.addr) ;
   TEST(0 == cache.iobuffer.size) ;
   TEST(0 == free_objectcacheimpl(&cache)) ;
   TEST(0 == cache.iobuffer.addr) ;
   TEST(0 == cache.iobuffer.size) ;

   return 0 ;
ONERR:
   (void) free_objectcacheimpl(&cache) ;
   return EINVAL ;
}

static int test_initthread(void)
{
   // TEST cast_objectcacheit
   TEST((objectcache_it*)&s_objectcacheimpl_interface == cast_objectcacheit(&s_objectcacheimpl_interface, objectcache_impl_t)) ;

   // TEST s_objectcacheimpl_interface
   TEST(s_objectcacheimpl_interface.lock_iobuffer   == &lockiobuffer_objectcacheimpl) ;
   TEST(s_objectcacheimpl_interface.unlock_iobuffer == &unlockiobuffer_objectcacheimpl) ;

   // TEST interface_objectcacheimpl
   TEST(interface_objectcacheimpl() == (objectcache_it*)&s_objectcacheimpl_interface) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int child_lockassert(objectcache_impl_t * cache)
{
   CLEARBUFFER_ERRLOG() ;
   if (cache) {
      memblock_t * iobuffer = (memblock_t*) 1 ;
      lockiobuffer_objectcacheimpl(cache, &iobuffer) ;
   }
   return 0 ;
}

static int child_unlockassert(objectcache_impl_t * cache)
{
   CLEARBUFFER_ERRLOG() ;
   if (cache) {
      memblock_t * iobuffer = (memblock_t*) 1 ;
      unlockiobuffer_objectcacheimpl(cache, &iobuffer) ;
   }
   return 0 ;
}

static int test_iobuffer(void)
{
   objectcache_impl_t   cache     = objectcache_impl_FREE ;
   process_t            process   = process_FREE ;
   memblock_t *         iobuffer  = 0 ;
   int                  pipefd[2] = { -1, -1 } ;
   process_result_t     result ;
   char                 buffer[1024] ;

   // TEST lock / unlock
   TEST(0 == init_objectcacheimpl(&cache)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == lockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(&cache.iobuffer == (void*)iobuffer) ;
   TEST(0 == unlockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == free_objectcacheimpl(&cache)) ;

   // TEST unlock twice
   TEST(0 == init_objectcacheimpl(&cache)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == lockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 != iobuffer) ;
   TEST(0 == unlockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == unlockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == free_objectcacheimpl(&cache)) ;

   // TEST EINVAL lock
   TEST(0 == init_objectcacheimpl(&cache)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == lockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 != iobuffer) ;
   TEST(EINVAL == lockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 == unlockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == free_objectcacheimpl(&cache)) ;

   // TEST EINVAL unlock
   TEST(0 == init_objectcacheimpl(&cache)) ;
   TEST(0 == iobuffer) ;
   TEST(0 == lockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 != iobuffer) ;
   TEST(0 == unlockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 == iobuffer) ;
   iobuffer = (void*) 1 ;
   TEST(EINVAL == unlockiobuffer2_objectcacheimpl(&cache, &iobuffer)) ;
   TEST(0 == free_objectcacheimpl(&cache)) ;

   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK)) ;
   process_stdio_t stdfd = process_stdio_INIT_DEVNULL ;
   redirecterr_processstdio(&stdfd, pipefd[1]) ;

   // TEST assertion lockiobuffer_objectcacheimpl
   TEST(0 == init_objectcacheimpl(&cache)) ;
   TEST(0 == initgeneric_process(&process, child_lockassert, &cache, &stdfd)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(process_state_ABORTED == result.state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == free_objectcacheimpl(&cache)) ;
   MEMSET0(&buffer) ;
   ssize_t read_bytes = read(pipefd[0], buffer, sizeof(buffer)-1) ;
   TEST(read_bytes > 50) ;
   PRINTF_ERRLOG("%s", buffer) ;

   // TEST assertion unlockiobuffer_objectcacheimpl
   TEST(0 == init_objectcacheimpl(&cache)) ;
   TEST(0 == initgeneric_process(&process, child_unlockassert, &cache, &stdfd)) ;
   TEST(0 == wait_process(&process, &result)) ;
   TEST(process_state_ABORTED == result.state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == free_objectcacheimpl(&cache)) ;
   MEMSET0(&buffer) ;
   read_bytes = read(pipefd[0], buffer, sizeof(buffer)-1) ;
   TEST(read_bytes > 50) ;
   PRINTF_ERRLOG("%s", buffer) ;

   TEST(0 == free_file(&pipefd[0])) ;
   TEST(0 == free_file(&pipefd[1])) ;

   return 0 ;
ONERR:
   (void) free_file(&pipefd[0]) ;
   (void) free_file(&pipefd[1]) ;
   (void) free_objectcacheimpl(&cache) ;
   return EINVAL ;
}

int unittest_cache_objectcacheimpl()
{
   if (test_initfree())       goto ONERR;
   if (test_initthread())     goto ONERR;
   if (test_iobuffer())       goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
