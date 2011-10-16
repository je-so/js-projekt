/* title: Test-Umgebung impl
   Implements <inittest_umgebung>, <freetest_umgebung>.

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

   file: C-kern/api/umg/umgtype_test.h
    Header file of <Test-Umgebung>.

   file: C-kern/umgebung/umgtype_test.c
    Implementation file <Test-Umgebung impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umg/umgtype_test.h"
#include "C-kern/api/err.h"
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/writer/logwriter_locked.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

#undef freetest_umgebung
#undef inittest_umgebung

int freetest_umgebung(umgebung_t * umg)
{
   int err = 0 ;
   int err2 ;

   err2 = freeumgebung_logwriterlocked(&umg->log) ;
   if (err2) err = err2 ;

   err2 = freeumgebung_objectcache(&umg->objectcache) ;
   if (err2) err = err2 ;

   err2 = freeumgebung_valuecache(&umg->valuecache) ;
   if (err2) err = err2 ;

   umg->type            = umgebung_type_STATIC ;
   umg->free_umgebung   = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int inittest_umgebung(umgebung_t * umg)
{
   int err ;

   MEMSET0(umg) ;
   umg->type            = umgebung_type_TEST ;
   umg->free_umgebung   = &freetest_umgebung ;
   umg->log             = &g_main_logwriterlocked ;

   err = initumgebung_valuecache(&umg->valuecache) ;
   if (err) goto ABBRUCH ;

   err = initumgebung_objectcache(&umg->objectcache) ;
   if (err) goto ABBRUCH ;

   err = initumgebung_logwriterlocked(&umg->log) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) freetest_umgebung(umg) ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_typetest,ABBRUCH)

static int test_initfree(void)
{
   umgebung_t umg ;

   // TEST init, double free
   MEMSET0(&umg) ;
   TEST(0 == inittest_umgebung(&umg)) ;
   TEST(umgebung_type_TEST  == umg.type) ;
   TEST(0                   == umg.resource_count) ;
   TEST(&freetest_umgebung  == umg.free_umgebung) ;
   TEST(0 == freetest_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 != umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;
   TEST(0 == freetest_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(&g_main_logwriterlocked == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_umgebung_typetest()
{
   if (test_initfree()) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

#endif
