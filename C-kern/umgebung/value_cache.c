/* title: ValueCache impl
   Implements <ValueCache>.

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

   file: C-kern/api/umgebung/value_cache.h
    Header file of <ValueCache>.

   file: C-kern/umgebung/value_cache.c
    Implementation file <ValueCache impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung/value_cache.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/os/virtmemory.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


value_cache_t    g_main_valuecache = { 0 } ;


int initprocess_valuecache(void)
{
   g_main_valuecache.pagesize_vm = sys_pagesize_vm() ;
   return 0 ;
}

int freeprocess_valuecache(void)
{
   // singleton object is allocated static => do not need to free it
   return 0 ;
}

int initumgebung_valuecache(value_cache_t ** valuecache)
{
   int err ;

   if (*valuecache) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   *valuecache = &g_main_valuecache ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_valuecache(value_cache_t ** valuecache)
{
   if (*valuecache) {
      *valuecache = 0 ;
   }

   return 0 ;
}



#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_valuecache,ABBRUCH)

static int test_processinit(void)
{

   // TEST init
   MEMSET0(&g_main_valuecache) ;
   TEST(0 == initprocess_valuecache()) ;
   TEST(g_main_valuecache.pagesize_vm)
   TEST(g_main_valuecache.pagesize_vm == sys_pagesize_vm()) ;

   // TEST double free (does nothing)
   TEST(0 == freeprocess_valuecache()) ;
   TEST(0 == freeprocess_valuecache()) ;
   TEST(g_main_valuecache.pagesize_vm == sys_pagesize_vm()) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_umgebunginit(void)
{
   value_cache_t * cache          = 0 ;
   value_cache_t * cache2         = 0 ;

   // TEST init, double free
   MEMSET0(&g_main_valuecache) ;
   TEST(0 == initumgebung_valuecache(&cache)) ;
   TEST(0 == g_main_valuecache.pagesize_vm) ;
   TEST(cache == &g_main_valuecache) ;
   TEST(0 == freeumgebung_valuecache(&cache)) ;
   TEST(0 == cache) ;
   TEST(0 == freeumgebung_valuecache(&cache)) ;
   TEST(0 == cache) ;

   // TEST EINVAL init
   TEST(0 == initumgebung_valuecache(&cache)) ;
   TEST(cache == &g_main_valuecache) ;
   TEST(EINVAL == initumgebung_valuecache(&cache)) ;
   TEST(0 == freeumgebung_valuecache(&cache)) ;
   TEST(0 == cache) ;

   // TEST always same value
   TEST(0 == initumgebung_valuecache(&cache)) ;
   TEST(0 == initumgebung_valuecache(&cache2)) ;
   TEST(0 == g_main_valuecache.pagesize_vm) ;
   TEST(cache  == &g_main_valuecache) ;
   TEST(cache2 == &g_main_valuecache) ;
   TEST(0 == freeumgebung_valuecache(&cache)) ;
   TEST(0 == cache) ;
   TEST(0 == freeumgebung_valuecache(&cache2)) ;
   TEST(0 == cache2) ;

   // TEST valuecache_umgebung()
   ((umgebung_t*)(intptr_t)umgebung())->valuecache = 0 ;
   TEST(valuecache_umgebung() == 0) ;
   ((umgebung_t*)(intptr_t)umgebung())->valuecache = &g_main_valuecache ;
   TEST(valuecache_umgebung() == &g_main_valuecache) ;

   // TEST cached functions
   g_main_valuecache.pagesize_vm = 0 ;
   TEST(pagesize_vm() == 0) ;
   g_main_valuecache.pagesize_vm = 512 ;
   TEST(pagesize_vm() == 512) ;
   g_main_valuecache.pagesize_vm = 12345 ;
   TEST(pagesize_vm() == 12345) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_umgebung_valuecache()
{
   value_cache_t * old_umgebung = umgebung()->valuecache ;
   value_cache_t   old_value    = g_main_valuecache ;

   if (test_processinit())    goto ABBRUCH ;
   if (test_umgebunginit())   goto ABBRUCH ;

   ((umgebung_t*)(intptr_t)umgebung())->valuecache = old_umgebung ;
   g_main_valuecache = old_value ;
   return 0 ;
ABBRUCH:
   ((umgebung_t*)(intptr_t)umgebung())->valuecache = old_umgebung ;
   g_main_valuecache = old_value ;
   return EINVAL ;
}

#endif
