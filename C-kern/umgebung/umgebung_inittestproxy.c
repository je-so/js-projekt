/* title: Umgebung Testproxy
   Implements <inittestproxy_umgebung>, <freetestproxy_umgebung>.

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

   file: C-kern/api/umgebung.h
    Header file of <Umgebung Interface>.

   file: C-kern/umgebung/umgebung_inittestproxy.c
    Implementation file <Umgebung Testproxy>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/umgebung/log.h"
#include "C-kern/api/umgebung/object_cache.h"
#include "C-kern/api/umgebung/value_cache.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


int freetestproxy_umgebung(umgebung_t * umg)
{
   int err = 0 ;
   int err2 ;

   err2 = freeumgebung_log(&umg->log) ;
   if (err2) err = err2 ;
   err2 = freeumgebung_objectcache(&umg->objectcache) ;
   if (err2) err = err2 ;
   err2 = freeumgebung_valuecache(&umg->valuecache) ;
   if (err2) err = err2 ;

   *umg = (umgebung_t) umgebung_INIT_MAINSERVICES ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int inittestproxy_umgebung(umgebung_t * umg)
{
   int err ;

   MEMSET0(umg) ;
   umg->type            = umgebung_type_TEST ;
   umg->free_umgebung   = &freetestproxy_umgebung ;

   err = initumgebung_valuecache(&umg->valuecache) ;
   if (err) goto ABBRUCH ;

   err = initumgebung_objectcache(&umg->objectcache) ;
   if (err) goto ABBRUCH ;

   err = initumgebung_log(&umg->log) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) freeumgebung_log(&umg->log) ;
   (void) freeumgebung_objectcache(&umg->objectcache) ;
   (void) freeumgebung_valuecache(&umg->valuecache) ;
   LOG_ABORT(err) ;
   return err ;
}

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_testproxy,ABBRUCH)

static int test_init(void)
{
   umgebung_t umg ;

   // TEST init, double free
   MEMSET0(&umg) ;
   TEST(0 == inittestproxy_umgebung(&umg)) ;
   TEST(umgebung_type_TEST       == umg.type) ;
   TEST(0                        == umg.resource_count) ;
   TEST(&freetestproxy_umgebung  == umg.free_umgebung) ;
   TEST(0 == freetestproxy_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 != umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;
   TEST(0 == freetestproxy_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(&g_main_logservice == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_umgebung_testproxy()
{
   if (test_init()) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

#endif
