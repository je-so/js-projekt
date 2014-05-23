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
#include "C-kern/api/test/unittest.h"
#endif


// section: valuecache_t

// group: lifetime

int init_valuecache(/*out*/valuecache_t * valuecache)
{
   valuecache->pagesize_vm     = sys_pagesize_vm() ;
   valuecache->log2pagesize_vm = log2_int(valuecache->pagesize_vm) ;
   return 0 ;
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
ONABORT:
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
ONABORT:
   return EINVAL ;
}


int unittest_cache_valuecache()
{
   if (test_initfree())    goto ONABORT ;
   if (test_queryvalues()) goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
