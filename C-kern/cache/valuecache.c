/* title: Valuecache impl
   Implements <Valuecache>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: valuecache_t

// group: lifetime

int init_valuecache(/*out*/valuecache_t * valuecache)
{
   int err;

   valuecache->pagesize_vm     = (uint32_t) sys_pagesize_vm();
   valuecache->log2pagesize_vm = log2_int(valuecache->pagesize_vm);

   if (  ! ispowerof2_int(valuecache->pagesize_vm)
      || sys_pagesize_vm() != valuecache->pagesize_vm) {
      err = EINVAL;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_valuecache(valuecache_t * valuecache)
{
   valuecache->pagesize_vm     = 0 ;
   valuecache->log2pagesize_vm = 0 ;
   return 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   valuecache_t   valuecache = valuecache_FREE ;

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
ONERR:
   free_valuecache(&valuecache) ;
   return EINVAL ;
}

static int test_queryvalues(void)
{
   valuecache_t * vc = valuecache_maincontext() ;

   // TEST log2pagesize_vm
   TEST(&log2pagesize_vm() == &vc->log2pagesize_vm)

   // TEST pagesize_vm
   TEST(&pagesize_vm() == &vc->pagesize_vm) ;

   return 0 ;
ONERR:
   return EINVAL ;
}


int unittest_cache_valuecache()
{
   if (test_initfree())    goto ONERR;
   if (test_queryvalues()) goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
