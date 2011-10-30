/* title: Umgtype-Multithread impl
   Implements <initmultithread_umgebung>, <freemultithread_umgebung>.

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

   file: C-kern/api/umg/umgtype_multithread.h
    Header file of <Umgtype-Multithread>.

   file: C-kern/umgebung/umgtype_multithread.c
    Implementation file of <Umgtype-Multithread impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umg/umgtype_multithread.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/writer/main_logwriter.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/text.db/initumgebung")WHERE(type=='multi')
#include "C-kern/api/cache/objectcachemt.h"
#include "C-kern/api/writer/logwritermt.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

#ifdef KONFIG_UNITTEST
/* variable: s_error_initres
 * Simulates an error in <init_thread_resources>. */
static test_errortimer_t   s_error_initres = test_errortimer_INIT_FREEABLE ;
#endif

// section: umgebung_type_MULTITHREAD

// group: helper

static int free_thread_resources(umgebung_t * umg)
{
   int err = 0 ;
   int err2 ;

   switch(umg->resource_count) {
   default:    assert(0 == umg->resource_count && "out of bounds") ;
               break ;
// TEXTDB:SELECT("   case "row-id":     err2 = freeumgebung_"module"("(if (parameter!="") "&umg->" else "")parameter") ;"\n"               if (err2) err = err2 ;")FROM(C-kern/resource/text.db/initumgebung)WHERE(type=='multi')DESCENDING
   case 2:     err2 = freeumgebung_logwritermt(&umg->ilog) ;
               if (err2) err = err2 ;
   case 1:     err2 = freeumgebung_objectcachemt(&umg->objectcache) ;
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

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;"\n"   err = initumgebung_"module"("(if (parameter!="") "&umg->")parameter") ;"\n"   if (err) goto ABBRUCH ;"\n"   ++umg->resource_count ;")FROM(C-kern/resource/text.db/initumgebung)WHERE(type=='multi')

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;
   err = initumgebung_objectcachemt(&umg->objectcache) ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;
   err = initumgebung_logwritermt(&umg->ilog) ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;
// TEXTDB:END

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;

   return 0 ;
ABBRUCH:
   (void) free_thread_resources(umg) ;
   LOG_ABORT(err) ;
   return err ;
}

// group: init

int freemultithread_umgebung(umgebung_t * umg)
{
   int err ;

   assert(!umg->type || umg->type == umgebung_type_MULTITHREAD) ;

   err = free_thread_resources(umg) ;

   umg->type           = 0 ;
   umg->free_umgebung  = 0 ;
   umg->shared         = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initmultithread_umgebung(umgebung_t * umg, umgebung_shared_t * shared)
{
   int err ;

   umg->type            = umgebung_type_MULTITHREAD ;
   umg->resource_count  = 0 ;
   umg->free_umgebung   = &freemultithread_umgebung ;
   umg->shared          = shared ;
   umg->ilog.object     = &g_main_logwriter ;
   umg->ilog.functable  = (log_it*) &g_main_logwriter_interface ;
   umg->objectcache     = (objectcache_oit) objectcache_oit_INIT_FREEABLE ;

   err = init_thread_resources(umg) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) freemultithread_umgebung(umg) ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_typemultithread,ABBRUCH)

static int test_initfree(void)
{
   umgebung_t           umg    = umgebung_INIT_FREEABLE ;
   umgebung_shared_t    shared = umgebung_shared_INIT_FREEABLE ;

   // TEST init, double free
   MEMSET0(&umg) ;
   TEST(0 == initmultithread_umgebung(&umg, &shared)) ;
   TEST(umg.type           == umgebung_type_MULTITHREAD) ;
   TEST(umg.resource_count == 2) ;
   TEST(umg.free_umgebung  == &freemultithread_umgebung) ;
   TEST(umg.shared         == &shared) ;
   TEST(umg.ilog.object    != 0 ) ;
   TEST(umg.ilog.object    != &g_main_logwriter) ;
   TEST(umg.ilog.functable != (log_it*)&g_main_logwriter_interface) ;
   TEST(0 != umg.objectcache.object) ;
   TEST(0 != umg.objectcache.functable) ;
   TEST(0 == freemultithread_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == umg.shared) ;
   TEST(umg.ilog.object    == &g_main_logwriter) ;
   TEST(umg.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == umg.objectcache.object) ;
   TEST(0 == umg.objectcache.functable) ;
   TEST(0 == freemultithread_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == umg.shared) ;
   TEST(umg.ilog.object    == &g_main_logwriter) ;
   TEST(umg.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == umg.objectcache.object) ;
   TEST(0 == umg.objectcache.functable) ;

   // TEST EINVAL init
   for(int i = 0; i < 3; ++i) {
      TEST(0 == init_testerrortimer(&s_error_initres, 1u+(unsigned)i, EINVAL+i)) ;
      memset(&umg, 0xff, sizeof(umg)) ;
      TEST(EINVAL+i == initmultithread_umgebung(&umg, &shared)) ;
      TEST(0 == umg.type) ;
      TEST(0 == umg.resource_count) ;
      TEST(0 == umg.free_umgebung) ;
      TEST(0 == umg.shared) ;
      TEST(umg.ilog.object    == &g_main_logwriter) ;
      TEST(umg.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
      TEST(0 == umg.objectcache.object) ;
      TEST(0 == umg.objectcache.functable) ;
   }

   return 0 ;
ABBRUCH:
   s_error_initres = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;
   return EINVAL ;
}

int unittest_umgebung_typemultithread()
{
   if (test_initfree())   goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

#endif
