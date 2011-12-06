/* title: Services-Singlethread impl
   Implements <initsinglethread_umgebungservices>, <freesinglethread_umgebungservices>.

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

   file: C-kern/api/umg/services_singlethread.h
    Header file of <Services-Singlethread>.

   file: C-kern/umgebung/services_singlethread.c
    Implementation file <Services-Singlethread impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umg/services_singlethread.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/writer/main_logwriter.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/text.db/initumgebung")WHERE(type=='single')
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/writer/logwriter.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

#ifdef KONFIG_UNITTEST
/* variable: s_error_initres
 * Simulates an error in <init_thread_resources>. */
static test_errortimer_t   s_error_initres = test_errortimer_INIT_FREEABLE ;
#endif

// section: umgebung_services_t

// group: lifetime

int freesinglethread_umgebungservices(umgebung_services_t * svc)
{
   int err = 0 ;
   int err2 ;

   switch(svc->resource_count) {
   default:    assert(0 == svc->resource_count && "out of bounds") ;
               break ;
// TEXTDB:SELECT("   case "row-id":     err2 = freeumgebung_"module"("(if (parameter!="") "&svc->" else "")parameter") ;"\n"               if (err2) err = err2 ;")FROM(C-kern/resource/text.db/initumgebung)WHERE(type=='single')DESCENDING
   case 2:     err2 = freeumgebung_logwriter(&svc->ilog) ;
               if (err2) err = err2 ;
   case 1:     err2 = freeumgebung_objectcache(&svc->objectcache) ;
               if (err2) err = err2 ;
// TEXTDB:END
   case 0:     break ;
   }

   svc->resource_count = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int initsinglethread_umgebungservices(/*out*/umgebung_services_t * svc)
{
   int err ;

   svc->resource_count  = 0 ;
   svc->ilog.object     = &g_main_logwriter ;
   svc->ilog.functable  = (log_it*) &g_main_logwriter_interface ;
   svc->objectcache     = (objectcache_oit) objectcache_oit_INIT_FREEABLE ;

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;"\n"   err = initumgebung_"module"("(if (parameter!="") "&svc->")parameter") ;"\n"   if (err) goto ABBRUCH ;"\n"   ++svc->resource_count ;")FROM(C-kern/resource/text.db/initumgebung)WHERE(type=='single')

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;
   err = initumgebung_objectcache(&svc->objectcache) ;
   if (err) goto ABBRUCH ;
   ++svc->resource_count ;

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;
   err = initumgebung_logwriter(&svc->ilog) ;
   if (err) goto ABBRUCH ;
   ++svc->resource_count ;
// TEXTDB:END

   ONERROR_testerrortimer(&s_error_initres, ABBRUCH) ;

   return 0 ;
ABBRUCH:
   (void) freesinglethread_umgebungservices(svc) ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION, ABBRUCH)

static int test_initfree(void)
{
   umgebung_services_t  svc = umgebung_services_INIT_FREEABLE ;

   // TEST static init
   TEST(0 == svc.resource_count) ;
   TEST(0 == svc.ilog.object) ;
   TEST(0 == svc.ilog.functable) ;
   TEST(0 == svc.objectcache.object) ;
   TEST(0 == svc.objectcache.functable) ;

   // TEST init, double free
   TEST(0 == initsinglethread_umgebungservices(&svc)) ;
   TEST(svc.resource_count == 2) ;
   TEST(svc.ilog.object    != 0) ;
   TEST(svc.ilog.object    != &g_main_logwriter) ;
   TEST(svc.ilog.functable != (log_it*)&g_main_logwriter_interface) ;
   TEST(svc.objectcache.object    != 0) ;
   TEST(svc.objectcache.functable != 0) ;
   TEST(0 == freesinglethread_umgebungservices(&svc)) ;
   TEST(0 == svc.resource_count) ;
   TEST(svc.ilog.object    == &g_main_logwriter) ;
   TEST(svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == svc.objectcache.object) ;
   TEST(0 == svc.objectcache.functable) ;
   TEST(0 == freesinglethread_umgebungservices(&svc)) ;
   TEST(0 == svc.resource_count) ;
   TEST(svc.ilog.object    == &g_main_logwriter) ;
   TEST(svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == svc.objectcache.object) ;
   TEST(0 == svc.objectcache.functable) ;

   // TEST EINVAL init
   for(int i = 0; i < 3; ++i) {
      TEST(0 == init_testerrortimer(&s_error_initres, 1u+(unsigned)i, EINVAL+i)) ;
      memset(&svc, 0xff, sizeof(svc)) ;
      TEST(EINVAL+i == initsinglethread_umgebungservices(&svc)) ;
      TEST(0 == svc.resource_count) ;
      TEST(svc.ilog.object    == &g_main_logwriter) ;
      TEST(svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
      TEST(0 == svc.objectcache.object) ;
      TEST(0 == svc.objectcache.functable) ;
   }

   return 0 ;
ABBRUCH:
   s_error_initres = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;
   return EINVAL ;
}

int unittest_umgebung_services_singlethread()
{
   if (test_initfree())   goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

#endif
