/* title: Default-Umgebung impl
   Implements <initdefault_umgebung>, <freedefault_umgebung>.

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

   file: C-kern/api/umg/umgtype_default.h
    Header file of <Default-Umgebung>.

   file: C-kern/umgebung/umgtype_default.c
    Implementation file of <Default-Umgebung impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umg/umgtype_default.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/text.db/initumgebung")
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/writer/logwriter_locked.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

#ifdef KONFIG_UNITTEST
/* variable: s_error_initres
 * Simulates an error in <init_thread_resources>. */
static test_errortimer_t   s_error_initres = test_errortimer_INIT_FREEABLE ;
#endif

// section: umgebung_type_default

// group: helper

static int free_thread_resources(umgebung_t * umg)
{
   int err = 0 ;
   int err2 ;

   switch(umg->resource_count) {
   default:    assert(0 == umg->resource_count && "out of bounds") ;
               break ;
// TEXTDB:SELECT("   case "row-id":     err2 = "free-function"("(if (parameter!="") "&umg->" else "")parameter") ;"\n"               if (err2) err = err2 ;")FROM(C-kern/resource/text.db/initumgebung)DESCENDING
   case 3:     err2 = freeumgebung_logwriterlocked(&umg->log) ;
               if (err2) err = err2 ;
   case 2:     err2 = freeumgebung_objectcache(&umg->objectcache) ;
               if (err2) err = err2 ;
   case 1:     err2 = freeumgebung_valuecache(&umg->valuecache) ;
               if (err2) err = err2 ;
// TEXTDB:END
   case 0:     break ;
   }

   umg->resource_count = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int init_thread_resources(umgebung_t * umg)
{
   int err ;

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;"\n"   err = "init-function"("(if (parameter!="") "&umg->")parameter") ;"\n"   if (err) goto ABBRUCH ;"\n"   ++umg->resource_count ;")FROM(C-kern/resource/text.db/initumgebung)

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;
   err = initumgebung_valuecache(&umg->valuecache) ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;
   err = initumgebung_objectcache(&umg->objectcache) ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;
   err = initumgebung_logwriterlocked(&umg->log) ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;
// TEXTDB:END

   return 0 ;
ABBRUCH:
   (void) free_thread_resources(umg) ;
   LOG_ABORT(err) ;
   return err ;
}

// group: implementation

int freedefault_umgebung(umgebung_t * umg)
{
   int err ;

   err = free_thread_resources(umg) ;

   umg->type           = 0 ;
   umg->free_umgebung  = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initdefault_umgebung(umgebung_t * umg)
{
   int err ;

   umg->type            = umgebung_type_DEFAULT ;
   umg->resource_count  = 0 ;
   umg->free_umgebung   = &freedefault_umgebung ;
   umg->log             = &g_main_logwriterlocked ;
   umg->objectcache     = 0 ;
   umg->valuecache      = 0 ;

   err = init_thread_resources(umg) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) freedefault_umgebung(umg) ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_typedefault,ABBRUCH)

static int test_initfree(void)
{
   umgebung_t umg = umgebung_INIT_FREEABLE ;

   // TEST init, double free
   MEMSET0(&umg) ;
   TEST(0 == initdefault_umgebung(&umg)) ;
   TEST(umgebung_type_DEFAULT == umg.type) ;
   TEST(3                     == umg.resource_count) ;
   TEST(freedefault_umgebung  == umg.free_umgebung) ;
   TEST(0 != umg.log) ;
   TEST(&g_main_logwriterlocked != umg.log) ;
   TEST(0 != umg.objectcache) ;
   TEST(0 != umg.valuecache) ;
   TEST(0 == freedefault_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(&g_main_logwriterlocked == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;
   TEST(0 == freedefault_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(&g_main_logwriterlocked == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;

   // TEST EINVAL init
   for(int i = 0; i < 3; ++i) {
      TEST(0 == init_testerrortimer(&s_error_initres, 1u+(unsigned)i, EINVAL+i)) ;
      umg = (umgebung_t) umgebung_INIT_FREEABLE ;
      TEST(EINVAL+i == initdefault_umgebung(&umg)) ;
      TEST(0 == umg.type) ;
      TEST(0 == umg.resource_count) ;
      TEST(0 == umg.free_umgebung) ;
      TEST(&g_main_logwriterlocked == umg.log) ;
      TEST(0 == umg.objectcache) ;
      TEST(0 == umg.valuecache) ;
   }

   return 0 ;
ABBRUCH:
   s_error_initres = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;
   return EINVAL ;
}

int unittest_umgebung_typedefault()
{
   if (test_initfree())   goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

#endif
