/* title: Umgebung impl
   Implementation file of generic init anf free functions.

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
    Header file of <Umgebung>.

   file: C-kern/umgebung/umgebung.c
    Implementation file <Umgebung impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/umg/services_singlethread.h"
#include "C-kern/api/umg/services_multithread.h"
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/writer/main_logwriter.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/text.db/initonce)
#include "C-kern/api/os/locale.h"
#include "C-kern/api/userinterface/X11/x11.h"
#include "C-kern/api/os/sync/signal.h"
#include "C-kern/api/os/thread.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: umgebung_t

// group: variables

/* variable: gt_umgebung
 * Defines thread-local storage for global context <umgebung_t> which every modul can access.
 * */
__thread umgebung_t           gt_umgebung       = umgebung_INIT_MAINSERVICES ;

/* variable: s_umgebung_shared
 * Defines shared part of global context <umgebung_t> which is the same for every modul. */
umgebung_shared_t             s_umgebung_shared = umgebung_shared_INIT_FREEABLE ;

#define THREAD 1
#if ((KONFIG_SUBSYS)&THREAD)
/* variable: s_initlock
 * Lock to protect access to <s_umgebungcount> in <init_umgebung> & <free_umgebung>. */
static mutex_t                s_initlock        = mutex_INIT_DEFAULT ;
#endif
#undef THREAD

#ifdef KONFIG_UNITTEST
/* variable: s_error_init
 * Simulates an error in <init_umgebung>. */
static test_errortimer_t      s_error_init      = test_errortimer_INIT_FREEABLE ;
#endif

/* variable: s_umgebungcount
 * The number of initialized <umgebung_t>.
 * Used in <init_umgebung> & <free_umgebung> to check if initonce_Functions
 * have to be called. */
static size_t                 s_umgebungcount   = 0 ;

/* variable: s_initoncecount_no_umgebung
 * Rememberes how many resources has been initialized successfully.
 * Used in <initonce_no_umgebung> and <freeonce_no_umgebung>
 * which are called from <initmain_umgebung> and <freemain_umgebung>. */
static size_t                 s_initoncecount_no_umgebung    = 0 ;

/* variable: s_initoncecount_valid_umgebung
 * Rememberes how many times <initonce_valid_umgebung> has been called. */
static size_t                 s_initoncecount_valid_umgebung = 0 ;

// group: helper

/* function: freeonce_no_umgebung
 * Frees resources after last <umgebung_t> object destroyed.
 * Uses <s_initoncecount_no_umgebung> to decide how many have to be
 * freed. Called functions has signature »freeonce_Function(void)«. */
static int freeonce_no_umgebung(void)
{
   int err = 0 ;
   int err2 ;

   switch(s_initoncecount_no_umgebung) {
   default: assert(0 == s_initoncecount_no_umgebung && "out of bounds" )  ;
            break ;
// TEXTDB:SELECT("   case "row-id":  err2 = freeonce_"module"() ;"\n"            if (err2) err=err2 ;")FROM("C-kern/resource/text.db/initonce")WHERE(time=='before')DESCENDING
   case 3:  err2 = freeonce_signalconfig() ;
            if (err2) err=err2 ;
   case 2:  err2 = freeonce_X11() ;
            if (err2) err=err2 ;
   case 1:  err2 = freeonce_locale() ;
            if (err2) err=err2 ;
// TEXTDB:END
   case 0:  break ;
   }

   s_initoncecount_no_umgebung = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

/* function: initonce_no_umgebung
 * Initializes resources before first <umgebung_t> object is initialized.
 * Uses <s_initoncecount_no_umgebung> to count the number of successful calls
 * to »initonce_Function(void)« . */
static int initonce_no_umgebung(void)
{
   int err ;

   if (s_initoncecount_no_umgebung) {
      return 0 ;
   }

// TEXTDB:SELECT(\n"   err = initonce_"module"() ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ s_initoncecount_no_umgebung ;")FROM("C-kern/resource/text.db/initonce")WHERE(time=='before')

   err = initonce_locale() ;
   if (err) goto ABBRUCH ;
   ++ s_initoncecount_no_umgebung ;

   err = initonce_X11() ;
   if (err) goto ABBRUCH ;
   ++ s_initoncecount_no_umgebung ;

   err = initonce_signalconfig() ;
   if (err) goto ABBRUCH ;
   ++ s_initoncecount_no_umgebung ;
// TEXTDB:END

   return 0 ;
ABBRUCH:
   (void) freeonce_no_umgebung() ;
   LOG_ABORT(err) ;
   return err ;
}

static int freeonce_valid_umgebung(umgebung_t * umg)
{
   int err = 0 ;
   int err2 ;
   (void) umg ;

   switch(s_initoncecount_valid_umgebung) {
   default: assert(0 == s_initoncecount_valid_umgebung && "out of bounds" )  ;
            break ;
// TEXTDB:SELECT("   case "row-id":  err2 = freeonce_"module"(umg) ;"\n"            if (err2) err=err2 ;")FROM("C-kern/resource/text.db/initonce")WHERE(time=='after')DESCENDING
   case 1:  err2 = freeonce_osthread(umg) ;
            if (err2) err=err2 ;
// TEXTDB:END
   case 0:  break ;
   }

   s_initoncecount_valid_umgebung = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

static int initonce_valid_umgebung(umgebung_t * umg)
{
   int err ;
   (void) umg ;

// TEXTDB:SELECT(\n"   err = initonce_"module"(umg) ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ s_initoncecount_valid_umgebung ;")FROM("C-kern/resource/text.db/initonce")WHERE(time=='after')

   err = initonce_osthread(umg) ;
   if (err) goto ABBRUCH ;
   ++ s_initoncecount_valid_umgebung ;
// TEXTDB:END

   return 0 ;
ABBRUCH:
   (void) freeonce_valid_umgebung(umg) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_umgebung(umgebung_t * umg)
{
   int err = 0 ;
   int err2 ;

   if (umg->type) {

      bool              is_freeonce ;
      bool              is_freeservices ;
      umgebung_type_e   umg_type   = umg->type  ;
      size_t          * copy_count = umg->copy_count ;

      slock_mutex(&s_initlock) ;

      -- s_umgebungcount ;

      is_freeonce = (0 == s_umgebungcount) ;

      if (is_freeonce) {
         err2 = freeonce_valid_umgebung(umg) ;
         if (err2) err = err2 ;
      }

      umg->type       = umgebung_type_STATIC ;
      umg->copy_count = 0 ;
      umg->shared     = 0 ;

      is_freeservices = (0 == copy_count) ;

      if (!is_freeservices) {
         -- (*copy_count) ;
         if (0 == *copy_count) {
            free(copy_count) ;
            is_freeservices = true ;
         }
      }

      if (is_freeservices) {
         switch(umg_type) {
         default:
            assert(0 && "implementation_type has wrong value") ;
            break ;
         case umgebung_type_SINGLETHREAD:
            err2 = freesinglethread_umgebungservices(&umg->svc) ;
            if (err2) err = err2 ;
            break ;
         case umgebung_type_MULTITHREAD:
            err2 = freemultithread_umgebungservices(&umg->svc) ;
            if (err2) err = err2 ;
            break ;
         }
      } else {
         umg->svc = (umgebung_services_t) umgebung_services_INIT_MAINSERVICES ;
      }

      if (is_freeonce) {
         err2 = free_umgebungshared(&s_umgebung_shared) ;
         if (err2) err = err2 ;

         err2 = freeonce_no_umgebung() ;
         if (err2) err = err2 ;
      }

      sunlock_mutex(&s_initlock) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int init2_umgebung(umgebung_t * umg, umgebung_type_e implementation_type, umgebung_t * copy_from)
{
   int err ;
   bool isinitonce_no    = false ;
   bool isinitshared     = false ;
   size_t  * copy_count  = 0 ;
   int    (* free_svc) (umgebung_services_t*) = 0 ;

   slock_mutex(&s_initlock) ;

   assert(!copy_from || s_umgebungcount) ;

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   if (0 == s_umgebungcount) {
      err = initonce_no_umgebung() ;
      if (err) goto ABBRUCH ;
      isinitonce_no = true ;
      ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

      err = init_umgebungshared(&s_umgebung_shared) ;
      if (err) goto ABBRUCH ;
      isinitshared = true ;
      ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;
   }

   umg->type            = implementation_type ;
   umg->copy_count      = 0 ;
   umg->shared          = &s_umgebung_shared ;

   if (copy_from) {
      static_assert(sizeof(umg->svc) == sizeof(copy_from->svc), "memcpy invariant") ;
      umg->copy_count   = copy_from->copy_count ;
      memcpy( &umg->svc, &copy_from->svc, sizeof(umg->svc)) ;
      if (copy_from->copy_count) {
         ++ (*copy_from->copy_count) ;
      } else {
         *copy_from = (umgebung_t) umgebung_INIT_MAINSERVICES ;
      }
   } else {
      switch(implementation_type) {
      default:
         assert(0 && "implementation_type has wrong value") ;
         break ;
      case umgebung_type_SINGLETHREAD:
         err = initsinglethread_umgebungservices(&umg->svc) ;
         if (err) goto ABBRUCH ;
         free_svc = &freesinglethread_umgebungservices ;
         break ;
      case umgebung_type_MULTITHREAD:
         copy_count = (size_t*) malloc(sizeof(size_t)) ;
         if (!copy_count) {
            err = ENOMEM ;
            LOG_OUTOFMEMORY(sizeof(size_t)) ;
            goto ABBRUCH ;
         }
         err = initmultithread_umgebungservices(&umg->svc) ;
         if (err) goto ABBRUCH ;
         #if !defined(freemultithread_umgebungservices)
         free_svc = &freemultithread_umgebungservices ;
         #endif
         *copy_count     = 1 ;
         umg->copy_count = copy_count ;
         break ;
      }
   }
   if (err) goto ABBRUCH ;
   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   if (0 == s_umgebungcount) {
      err = initonce_valid_umgebung(umg) ;
      if (err) goto ABBRUCH ;
   }

   if (!copy_from || copy_from->copy_count) {
      ++ s_umgebungcount ;
   }

   sunlock_mutex(&s_initlock) ;

   return 0 ;
ABBRUCH:
   if (free_svc)        (void) free_svc(&umg->svc) ;
   if (isinitshared)    (void) free_umgebungshared(&s_umgebung_shared) ;
   if (isinitonce_no)   (void) freeonce_no_umgebung() ;
   free(copy_count) ;
   *umg = (umgebung_t) umgebung_INIT_MAINSERVICES ;
   sunlock_mutex(&s_initlock) ;
   LOG_ABORT(err) ;
   return err ;
}

int init_umgebung(umgebung_t * umg, umgebung_type_e implementation_type)
{
   int err ;

   PRECONDITION_INPUT(     umgebung_type_SINGLETHREAD <= implementation_type
                        && implementation_type <=  umgebung_type_MULTITHREAD,
                        ABBRUCH, LOG_INT(implementation_type)) ;

   err = init2_umgebung(umg, implementation_type, 0) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initcopy_umgebung(/*out*/umgebung_t * umg, umgebung_t * copy_from)
{
   int err ;

   PRECONDITION_INPUT(     umgebung_type_MULTITHREAD  == copy_from->type
                        || umgebung_type_SINGLETHREAD == copy_from->type,
                        ABBRUCH, LOG_INT(copy_from->type)) ;

   err = init2_umgebung(umg, copy_from->type, copy_from) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freemain_umgebung(void)
{
   int err ;
   umgebung_t       * umg = &gt_umgebung ;
   int     is_initialized = (0 != umg->type) ;

   if (is_initialized) {
      err = free_umgebung(umg) ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int initmain_umgebung(umgebung_type_e implementation_type)
{
   int err ;
   umgebung_t           * umg = &gt_umgebung ;
   int is_already_initialized = (0 != umg->type) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ABBRUCH ;
   }

   PRECONDITION_INPUT(     umgebung_type_SINGLETHREAD <= implementation_type
                        && implementation_type <=  umgebung_type_MULTITHREAD,
                        ABBRUCH, LOG_INT(implementation_type)) ;

   err = init2_umgebung(umg, implementation_type, 0) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

void abort_umgebung(int err)
{
   // TODO: add abort handler registration ...
   // TODO: add unit test for checking that resources are freed
   LOG_ERRTEXT(ABORT_FATAL(err)) ;
   LOG_FLUSHBUFFER() ;
   abort() ;
}

void assertfail_umgebung(
   const char * condition,
   const char * file,
   unsigned     line,
   const char * funcname)
{
   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ERROR_LOCATION(file, line, funcname)) ;
   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ABORT_ASSERT_FAILED(condition)) ;
   abort_umgebung(EINVAL) ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION, ABBRUCH)

static int test_main_init(void)
{
   const umgebung_t * umg = &umgebung() ;

   // TEST static type
   TEST( umg->type           == umgebung_type_STATIC ) ;
   TEST( umg->copy_count     == 0 ) ;
   TEST( umg->shared         == 0 );
   TEST( umg->svc.resource_count == 0 ) ;
   TEST( umg->svc.ilog.object    == &g_main_logwriter );
   TEST( umg->svc.ilog.functable == (log_it*)&g_main_logwriter_interface );
   TEST( umg->svc.objectcache.object    == 0 );
   TEST( umg->svc.objectcache.functable == 0 );

   // TEST EINVAL: wrong type
   TEST(0      == umgebung_type_STATIC) ;
   TEST(EINVAL == initmain_umgebung(umgebung_type_STATIC)) ;
   TEST(EINVAL == initmain_umgebung(3)) ;

   // TEST init, double free (umgebung_type_SINGLETHREAD)
   TEST(0 == umgebung().type) ;
   TEST(0 == initmain_umgebung(umgebung_type_SINGLETHREAD)) ;
   TEST(1 == s_umgebungcount) ;
   TEST(1 == umgebung().type) ;
   TEST(0 == umgebung().copy_count) ;
   TEST(&s_umgebung_shared == umgebung().shared) ;
   TEST(0 != umgebung().shared->valuecache) ;
   TEST(0 != umgebung().svc.resource_count) ;
   TEST(0 != umgebung().svc.ilog.object) ;
   TEST(0 != umgebung().svc.objectcache.object) ;
   TEST(0 != umgebung().svc.objectcache.functable) ;
   TEST(0 != strcmp("C", current_locale())) ;
   TEST(umgebung().svc.ilog.object    != &g_main_logwriter) ;
   TEST(umgebung().svc.ilog.functable != (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == freemain_umgebung()) ;
   TEST(0 == s_umgebungcount) ;
   TEST(0 == umgebung().type) ;
   TEST(0 == umgebung().copy_count) ;
   TEST(0 == umgebung().shared) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(umgebung().svc.resource_count == 0) ;
   TEST(umgebung().svc.ilog.object    == &g_main_logwriter) ;
   TEST(umgebung().svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(umgebung().svc.objectcache.object    == 0) ;
   TEST(umgebung().svc.objectcache.functable == 0) ;
   TEST(0 == freemain_umgebung()) ;
   TEST(0 == s_umgebungcount) ;
   TEST(0 == umgebung().type ) ;
   TEST(0 == umgebung().copy_count) ;
   TEST(0 == umgebung().shared) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(umgebung().svc.resource_count == 0) ;
   TEST(umgebung().svc.ilog.object    == &g_main_logwriter) ;
   TEST(umgebung().svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(umgebung().svc.objectcache.object    == 0) ;
   TEST(umgebung().svc.objectcache.functable == 0) ;

   // TEST init, double free (umgebung_type_MULTITHREAD)
   TEST(0 == umgebung().type) ;
   TEST(0 == initmain_umgebung(umgebung_type_MULTITHREAD)) ;
   TEST(1 == s_umgebungcount) ;
   TEST(2 == umgebung().type) ;
   TEST(0 != umgebung().copy_count) ;
   TEST(&s_umgebung_shared == umgebung().shared) ;
   TEST(0 != umgebung().shared->valuecache) ;
   TEST(0 != umgebung().svc.resource_count) ;
   TEST(0 != umgebung().svc.ilog.object) ;
   TEST(0 != umgebung().svc.objectcache.object) ;
   TEST(0 != umgebung().svc.objectcache.functable) ;
   TEST(umgebung().svc.ilog.object    != &g_main_logwriter) ;
   TEST(umgebung().svc.ilog.functable != (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == freemain_umgebung()) ;
   TEST(0 == s_umgebungcount) ;
   TEST(0 == umgebung().type ) ;
   TEST(0 == umgebung().copy_count) ;
   TEST(0 == umgebung().shared) ;
   TEST(umgebung().svc.resource_count == 0) ;
   TEST(umgebung().svc.ilog.object    == &g_main_logwriter) ;
   TEST(umgebung().svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(umgebung().svc.objectcache.object    == 0) ;
   TEST(umgebung().svc.objectcache.functable == 0) ;
   TEST(0 == freemain_umgebung()) ;
   TEST(0 == s_umgebungcount) ;
   TEST(0 == umgebung().type ) ;
   TEST(0 == umgebung().copy_count) ;
   TEST(0 == umgebung().shared) ;
   TEST(umgebung().svc.resource_count == 0) ;
   TEST(umgebung().svc.ilog.object    == &g_main_logwriter) ;
   TEST(umgebung().svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(umgebung().svc.objectcache.object    == 0) ;
   TEST(umgebung().svc.objectcache.functable == 0) ;

   // TEST static type has not changed
   TEST( umg->type           == umgebung_type_STATIC ) ;
   TEST( umg->copy_count     == 0 ) ;
   TEST( umg->shared         == 0 );
   TEST( umg->svc.resource_count == 0 ) ;
   TEST( umg->svc.ilog.object    == &g_main_logwriter );
   TEST( umg->svc.ilog.functable == (log_it*)&g_main_logwriter_interface );
   TEST( umg->svc.objectcache.object    == 0 );
   TEST( umg->svc.objectcache.functable == 0 );
   LOG_FLUSHBUFFER() ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_querymacros(void)
{

   // TEST query umgebung()
   TEST( &umgebung() == &gt_umgebung ) ;

   // TEST query log_umgebung()
   TEST( &gt_umgebung.svc.ilog == &log_umgebung() ) ;

   // TEST query log_umgebung()
   TEST( &gt_umgebung.svc.objectcache == &objectcache_umgebung() ) ;

   if (gt_umgebung.shared) {
      struct valuecache_t * const oldcache2 = gt_umgebung.shared->valuecache ;
      gt_umgebung.shared->valuecache = (struct valuecache_t*)3 ;
      TEST( (struct valuecache_t*)3 == valuecache_umgebung() ) ;
      gt_umgebung.shared->valuecache = oldcache2 ;
      TEST( oldcache2 == valuecache_umgebung() ) ;

      TEST( gt_umgebung.shared == &s_umgebung_shared ) ;
   }

   // TEST s_initoncecount_no_umgebung
   if (s_umgebungcount) {
      TEST(3 == s_initoncecount_no_umgebung) ;
   } else {
      TEST(0 == s_initoncecount_no_umgebung) ;
   }

   // TEST s_initoncecount_valid_umgebung
   if (s_umgebungcount) {
      TEST(1 == s_initoncecount_valid_umgebung) ;
   } else {
      TEST(0 == s_initoncecount_valid_umgebung) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_initfree(void)
{
   umgebung_t umg       = umgebung_INIT_FREEABLE ;
   umgebung_t umg2[20]  = { umgebung_INIT_FREEABLE } ;
   const bool isINIT    = (umgebung_type_STATIC != umgebung().type) ;
   size_t     oldcount ;

   if (!isINIT) {
      TEST(0 == initmain_umgebung(umgebung_type_SINGLETHREAD)) ;
   }

   // TEST static init
   TEST(0 == umg.type) ;
   TEST(0 == umg.copy_count) ;
   TEST(0 == umg.shared) ;
   TEST(0 == umg.svc.resource_count) ;
   TEST(0 == umg.svc.ilog.object) ;
   TEST(0 == umg.svc.ilog.functable) ;
   TEST(0 == umg.svc.objectcache.object) ;
   TEST(0 == umg.svc.objectcache.functable) ;

   // TEST EINVAL: wrong type
   TEST(EINVAL == init_umgebung(&umg, 0)) ;
   TEST(EINVAL == init_umgebung(&umg, 3)) ;

   // TEST init, double free (umgebung_type_SINGLETHREAD)
   oldcount = s_umgebungcount ;
   TEST(0 == init_umgebung(&umg, umgebung_type_SINGLETHREAD)) ;
   TEST(1+oldcount == s_umgebungcount) ;
   TEST(umg.type           == umgebung_type_SINGLETHREAD) ;
   TEST(umg.copy_count     == 0) ;
   TEST(umg.shared         == &s_umgebung_shared) ;
   TEST(umg.shared->valuecache != 0) ;
   TEST(umg.svc.resource_count != 0) ;
   TEST(umg.svc.ilog.object    != 0) ;
   TEST(umg.svc.ilog.object    != &g_main_logwriter) ;
   TEST(umg.svc.ilog.functable != (log_it*)&g_main_logwriter_interface) ;
   TEST(umg.svc.objectcache.object    != 0) ;
   TEST(umg.svc.objectcache.functable != 0) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(oldcount == s_umgebungcount) ;
   TEST(0 == umg.type ) ;
   TEST(0 == umg.copy_count) ;
   TEST(0 == umg.shared) ;
   TEST(0 == umg.svc.resource_count) ;
   TEST(umg.svc.ilog.object    == &g_main_logwriter) ;
   TEST(umg.svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == umg.svc.objectcache.object) ;
   TEST(0 == umg.svc.objectcache.functable) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(oldcount == s_umgebungcount) ;
   TEST(0 == umg.type ) ;
   TEST(0 == umg.copy_count) ;
   TEST(0 == umg.shared) ;
   TEST(0 == umg.svc.resource_count) ;
   TEST(umg.svc.ilog.object    == &g_main_logwriter) ;
   TEST(umg.svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == umg.svc.objectcache.object) ;
   TEST(0 == umg.svc.objectcache.functable) ;

   // TEST init, double free (umgebung_type_MULTITHREAD)
   TEST(0 == init_umgebung(&umg, umgebung_type_MULTITHREAD)) ;
   TEST(1+oldcount == s_umgebungcount) ;
   TEST(umg.type            == umgebung_type_MULTITHREAD) ;
   TEST(umg.copy_count     != 0) ;
   TEST(*umg.copy_count    == 1) ;
   TEST(umg.shared         == &s_umgebung_shared) ;
   TEST(umg.shared->valuecache != 0) ;
   TEST(umg.svc.resource_count != 0 ) ;
   TEST(umg.svc.ilog.object    != 0) ;
   TEST(umg.svc.ilog.object    != &g_main_logwriter) ;
   TEST(umg.svc.ilog.functable != (log_it*)&g_main_logwriter_interface) ;
   TEST(umg.svc.objectcache.object    != 0) ;
   TEST(umg.svc.objectcache.functable != 0) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(oldcount == s_umgebungcount) ;
   TEST(0 == umg.type ) ;
   TEST(0 == umg.copy_count) ;
   TEST(0 == umg.shared) ;
   TEST(0 == umg.svc.resource_count) ;
   TEST(umg.svc.ilog.object    == &g_main_logwriter) ;
   TEST(umg.svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
   TEST(0 == umg.svc.objectcache.object) ;
   TEST(0 == umg.svc.objectcache.functable) ;

   // TEST initcopy umgebung_type_SINGLETHREAD
   TEST(0 == init_umgebung(&umg, umgebung_type_SINGLETHREAD)) ;
   TEST(1+oldcount == s_umgebungcount) ;
   umgebung_t * oldumg = &umg ;
   for(unsigned i = 0; i < nrelementsof(umg2); ++i) {
      umgebung_t umgcmp = *oldumg ;
      TEST(0 == initcopy_umgebung(&umg2[i], oldumg))
      TEST(1+oldcount == s_umgebungcount) ;
      TEST(0 == memcmp(&umg2[i], &umgcmp, sizeof(umgebung_t))) ;
      // old umg was freed
      TEST(0 == oldumg->type) ;
      TEST(0 == oldumg->copy_count) ;
      TEST(0 == oldumg->shared) ;
      TEST(0 == oldumg->svc.resource_count) ;
      TEST(oldumg->svc.ilog.object    == &g_main_logwriter) ;
      TEST(oldumg->svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
      TEST(0 == oldumg->svc.objectcache.object) ;
      TEST(0 == oldumg->svc.objectcache.functable) ;
      oldumg = &umg2[i] ;
   }
   TEST(0 == free_umgebung(oldumg)) ;
   TEST(oldcount == s_umgebungcount) ;

   // TEST initcopy  umgebung_type_MULTITHREAD
   TEST(0 == init_umgebung(&umg, umgebung_type_MULTITHREAD)) ;
   TEST(1+oldcount == s_umgebungcount) ;
   for(unsigned i = 0; i < nrelementsof(umg2); ++i) {
      umgebung_t umgcompare = umg ;
      TEST(0 == initcopy_umgebung(&umg2[i], &umg))
      TEST(2+i+oldcount == s_umgebungcount) ;
      // umg keeps the same
      TEST(0 == memcmp(&umgcompare, &umg, sizeof(umg))) ;
      // copy_count is one up
      TEST(*umg.copy_count == 2+i) ;
      // umg2[i] is a copy
      TEST(0 == memcmp(&umg2[i], &umg, sizeof(umg))) ;
   }
   for(unsigned i = 0; i < nrelementsof(umg2); ++i) {
      umgebung_t umgcompare = umg ;
      TEST(0 == free_umgebung(&umg2[i]))
      TEST(nrelementsof(umg2)-i+oldcount == s_umgebungcount) ;
      // umg keeps the same
      TEST(0 == memcmp(&umgcompare, &umg, sizeof(umg))) ;
      // copy_count is one down
      TEST(*umg.copy_count == nrelementsof(umg2)-i) ;
      // umg2[i] is freed
      TEST(0 == umg2[i].type) ;
      TEST(0 == umg2[i].copy_count) ;
      TEST(0 == umg2[i].shared) ;
      TEST(0 == umg2[i].svc.resource_count) ;
      TEST(umg2[i].svc.ilog.object    == &g_main_logwriter) ;
      TEST(umg2[i].svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
      TEST(0 == umg2[i].svc.objectcache.object) ;
      TEST(0 == umg2[i].svc.objectcache.functable) ;
   }
   TEST(0 == free_umgebung(&umg)) ;
   TEST(oldcount == s_umgebungcount) ;

   // TEST initmove
   TEST(0 == init_umgebung(&umg, umgebung_type_MULTITHREAD)) ;
   TEST(1+oldcount == s_umgebungcount) ;
   oldumg = &umg ;
   for(unsigned i = 0; i < nrelementsof(umg2); ++i) {
      umgebung_t umgcompare = *oldumg ;
      TEST(0 == initmove_umgebung(&umg2[i], oldumg)) ;
      TEST(1+oldcount == s_umgebungcount) ;
      // umg2 is a copy
      TEST(umg2[i].type == umgebung_type_MULTITHREAD) ;
      TEST(0 == memcmp(&umg2[i], &umgcompare, sizeof(umgcompare))) ;
      // oldumg is cleared
      TEST(oldumg->type == umgebung_type_STATIC) ;
      umgcompare = (umgebung_t) umgebung_INIT_FREEABLE ;
      TEST(0 == memcmp(oldumg, &umgcompare, sizeof(umgcompare))) ;
      oldumg = &umg2[i] ;
   }
   TEST(0 == free_umgebung(oldumg)) ;
   TEST(oldcount == s_umgebungcount) ;

   // TEST EINVAL initcopy
   TEST(0 == umg.type) ;
   TEST(EINVAL == initcopy_umgebung(&umg2[0], &umg)) ;

   if (!isINIT) {
      TEST(0 == freemain_umgebung()) ;
   }

   return 0 ;
ABBRUCH:
   if (  !isINIT
      && (umgebung_type_STATIC != umgebung().type)) {
      TEST(0 == freemain_umgebung()) ;
   }
   free_umgebung(&umg) ;
   return EINVAL ;
}

static int test_initmainerror(void)
{
   int               fd_stderr = -1 ;
   int               fdpipe[2] = { -1, -1 } ;
   umgebung_shared_t shared2   = umgebung_shared_INIT_FREEABLE ;
   umgebung_type_e   type      = umgebung().type ;

   TEST(1 == s_umgebungcount) ;

   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;

   TEST(0 == freemain_umgebung()) ;
   TEST(0 == s_umgebungcount) ;
   TEST(umgebung_type_STATIC == umgebung().type) ;

   // TEST error in init_umgebung in different places (called from initmain)
   for(int i = 1; i <= 4; ++i) {
      TEST(0 == init_testerrortimer(&s_error_init, (unsigned)i, EINVAL+i)) ;
      TEST(EINVAL+i == initmain_umgebung(type)) ;
      TEST(0 == s_initoncecount_no_umgebung) ;
      TEST(0 == s_initoncecount_valid_umgebung) ;
      TEST(0 == s_umgebungcount) ;
      TEST(0 == memcmp(&s_umgebung_shared, &shared2, sizeof(shared2))) ;
      TEST(umgebung_type_STATIC == umgebung().type) ;
      TEST(0 == umgebung().copy_count) ;
      TEST(0 == umgebung().shared) ;
      TEST(0 == umgebung().svc.resource_count) ;
      TEST(umgebung().svc.ilog.object    == &g_main_logwriter) ;
      TEST(umgebung().svc.ilog.functable == (log_it*)&g_main_logwriter_interface) ;
      TEST(0 == umgebung().svc.objectcache.object) ;
      TEST(0 == umgebung().svc.objectcache.functable) ;
   }

   LOG_FLUSHBUFFER() ;
   char buffer[4096] = { 0 };
   TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

   TEST(0 == s_umgebungcount) ;
   TEST(0 == initmain_umgebung(type)) ;
   // make printed system error messages language (English) neutral
   resetmsg_locale() ;
   TEST(1 == s_umgebungcount) ;
   TEST(0 != s_initoncecount_no_umgebung) ;
   TEST(0 != s_initoncecount_valid_umgebung) ;
   TEST(0 != memcmp(&s_umgebung_shared, &shared2, sizeof(shared2))) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_filedescr(&fd_stderr)) ;
   TEST(0 == free_filedescr(&fdpipe[0])) ;
   TEST(0 == free_filedescr(&fdpipe[1])) ;

   LOGC_PRINTF(ERR, "%s", buffer) ;

   // TEST EALREADY
   TEST(EALREADY == initmain_umgebung(type)) ;

   return 0 ;
ABBRUCH:
   if (  0 == s_umgebungcount
      && umgebung_type_STATIC != type) {
      initmain_umgebung(type) ;
   }
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_filedescr(&fd_stderr) ;
   free_filedescr(&fdpipe[0]);
   free_filedescr(&fdpipe[1]);
   return EINVAL ;
}

int unittest_umgebung()
{
   int               fd_stderr = -1 ;
   int               fdpipe[2] = { -1, -1 } ;
   resourceusage_t   usage     = resourceusage_INIT_FREEABLE ;

   if (umgebung_type_STATIC == umgebung().type) {

      fd_stderr = dup(STDERR_FILENO) ;
      TEST(-1!= fd_stderr) ;
      TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
      TEST(-1!= dup2(fdpipe[1], STDERR_FILENO)) ;

      if (test_querymacros())    goto ABBRUCH ;
      if (test_main_init())      goto ABBRUCH ;
      if (test_initfree())       goto ABBRUCH ;

      char buffer[2048] = { 0 };
      TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

      const char * expect =
         // log from test_main_init
         "C-kern/umgebung/umgebung.c:433: initmain_umgebung(): error: Function argument violates condition (umgebung_type_SINGLETHREAD <= implementation_type && implementation_type <= umgebung_type_MULTITHREAD)"
         "\nimplementation_type=0"
         "\nC-kern/umgebung/umgebung.c:440: initmain_umgebung(): error: Function aborted (err=22)"
         "\nC-kern/umgebung/umgebung.c:433: initmain_umgebung(): error: Function argument violates condition (umgebung_type_SINGLETHREAD <= implementation_type && implementation_type <= umgebung_type_MULTITHREAD)"
         "\nimplementation_type=3"
         "\nC-kern/umgebung/umgebung.c:440: initmain_umgebung(): error: Function aborted (err=22)"
         // log from test_initfree
         "\nC-kern/umgebung/umgebung.c:375: init_umgebung(): error: Function argument violates condition (umgebung_type_SINGLETHREAD <= implementation_type && implementation_type <= umgebung_type_MULTITHREAD)"
         "\nimplementation_type=0"
         "\nC-kern/umgebung/umgebung.c:382: init_umgebung(): error: Function aborted (err=22)"
         "\nC-kern/umgebung/umgebung.c:375: init_umgebung(): error: Function argument violates condition (umgebung_type_SINGLETHREAD <= implementation_type && implementation_type <= umgebung_type_MULTITHREAD)"
         "\nimplementation_type=3"
         "\nC-kern/umgebung/umgebung.c:382: init_umgebung(): error: Function aborted (err=22)"
         "\nC-kern/umgebung/umgebung.c:392: initcopy_umgebung(): error: Function argument violates condition (umgebung_type_MULTITHREAD == copy_from->type || umgebung_type_SINGLETHREAD == copy_from->type)"
         "\ncopy_from->type=0"
         "\nC-kern/umgebung/umgebung.c:399: initcopy_umgebung(): error: Function aborted (err=22)"
         "\n"
         ;

      if (strcmp( buffer, expect)) {
         printf("buffer=-----\n%s-----\n",buffer) ;
         for(unsigned i = 0; i < strlen(buffer); ++i) {
            if (strncmp(buffer, expect, i)) {
               printf("position = %d\n", i) ;
               break ;
            }
         }
         TEST(0 == "wrong buffer content") ;
      }

      TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
      TEST(0 == free_filedescr(&fd_stderr)) ;
      TEST(0 == free_filedescr(&fdpipe[0]));
      TEST(0 == free_filedescr(&fdpipe[1]));

   } else {
      assert(umgebung_type_STATIC != umgebung().type) ;

      if (test_initmainerror()) goto ABBRUCH ;

      TEST(0 == init_resourceusage(&usage)) ;

      if (test_querymacros())    goto ABBRUCH ;
      if (test_initfree())       goto ABBRUCH ;
      if (test_initmainerror())  goto ABBRUCH ;

      TEST(0 == same_resourceusage(&usage)) ;
      TEST(0 == free_resourceusage(&usage)) ;
   }

   return 0 ;
ABBRUCH:
   TEST(0 == free_resourceusage(&usage)) ;
   if (-1 != fd_stderr) {
      dup2(fd_stderr, STDERR_FILENO) ;
      free_filedescr(&fd_stderr) ;
   }
   free_filedescr(&fdpipe[0]) ;
   free_filedescr(&fdpipe[1]) ;
   return EINVAL ;
}

#endif
