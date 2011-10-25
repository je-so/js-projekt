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
#include "C-kern/api/umg/umgtype_default.h"
#include "C-kern/api/umg/umgtype_test.h"
#include "C-kern/api/writer/logwriter_locked.h"
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/test/errortimer.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/text.db/initonce)
#include "C-kern/api/os/locale.h"
#include "C-kern/api/os/X11/x11.h"
#include "C-kern/api/os/sync/signal.h"
#include "C-kern/api/os/thread.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: Implementation

#ifdef KONFIG_UNITTEST
/* variable: s_error_init
 * Simulates an error in <init_umgebung>. */
static test_errortimer_t      s_error_init = test_errortimer_INIT_FREEABLE ;
#endif

/* variable: s_umgebung_shared
 * Defines thread-local storage for global context <umgebung_t> which every modul can access.
 * */
umgebung_shared_t             s_umgebung_shared = umgebung_shared_INIT_FREEABLE ;

/* variable: gt_umgebung
 * Defines thread-local storage for global context <umgebung_t> which every modul can access.
 * */
__thread umgebung_t           gt_umgebung       = umgebung_INIT_MAINSERVICES ;

#define THREAD 1
#if ((KONFIG_SUBSYS)&THREAD)
/* variable: s_initlock
 * Lock to protect access to <s_umgebungcount>. */
static mutex_t                s_initlock        = mutex_INIT_DEFAULT ;
#endif
#undef THREAD

/* variable: s_umgebungcount
 * The number of initialized <umgebung_t>.
 * Used in <init_umgebung>*/
static size_t                 s_umgebungcount   = 0 ;

/* variable: s_initcount_no_umgebung
 * Rememberes how many resources has been initialized successfully.
 * Used in <initonce_no_umgebung> and <freeonce_no_umgebung>
 * which are called from <initmain_umgebung> and <freemain_umgebung>. */
static uint16_t               s_initcount_no_umgebung = 0 ;

/* variable: s_initcount_valid_umgebung
 * Rememberes how many times <initonce_valid_umgebung> has been called. */
static uint16_t               s_initcount_valid_umgebung  = 0 ;

static int freeonce_no_umgebung(void)
{
   int err = 0 ;
   int err2 ;

   switch(s_initcount_no_umgebung) {
   default: assert(0 == s_initcount_no_umgebung && "out of bounds" )  ;
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

   s_initcount_no_umgebung = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

static int initonce_no_umgebung(void)
{
   int err ;

   if (s_initcount_no_umgebung) {
      return 0 ;
   }

// TEXTDB:SELECT(\n"   err = initonce_"module"() ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ s_initcount_no_umgebung ;")FROM("C-kern/resource/text.db/initonce")WHERE(time=='before')

   err = initonce_locale() ;
   if (err) goto ABBRUCH ;
   ++ s_initcount_no_umgebung ;

   err = initonce_X11() ;
   if (err) goto ABBRUCH ;
   ++ s_initcount_no_umgebung ;

   err = initonce_signalconfig() ;
   if (err) goto ABBRUCH ;
   ++ s_initcount_no_umgebung ;
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

   switch(s_initcount_valid_umgebung) {
   default: assert(0 == s_initcount_valid_umgebung && "out of bounds" )  ;
            break ;
// TEXTDB:SELECT("   case "row-id":  err2 = freeonce_"module"(umg) ;"\n"            if (err2) err=err2 ;")FROM("C-kern/resource/text.db/initonce")WHERE(time=='after')DESCENDING
   case 1:  err2 = freeonce_osthread(umg) ;
            if (err2) err=err2 ;
// TEXTDB:END
   case 0:  break ;
   }

   s_initcount_valid_umgebung = 0 ;

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

// TEXTDB:SELECT(\n"   err = initonce_"module"(umg) ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ s_initcount_valid_umgebung ;")FROM("C-kern/resource/text.db/initonce")WHERE(time=='after')

   err = initonce_osthread(umg) ;
   if (err) goto ABBRUCH ;
   ++ s_initcount_valid_umgebung ;
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

   if (umg->free_umgebung) {

      bool is_freeonce ;

      slock_mutex(&s_initlock) ;

      -- s_umgebungcount ;

      is_freeonce = (0 == s_umgebungcount) ;

      if (is_freeonce) {
         err2 = freeonce_valid_umgebung(umg) ;
         if (err2) err = err2 ;
      }

      err2 = umg->free_umgebung(umg) ;
      if (err2) err = err2 ;

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

int init_umgebung(umgebung_t * umg, umgebung_type_e implementation_type)
{
   int err ;
   bool isinitonce_no    = false ;
   bool isinitshared     = false ;
   bool isinitumgebung   = false ;

   slock_mutex(&s_initlock) ;

   PRECONDITION_INPUT(     umgebung_type_STATIC < implementation_type
                        && implementation_type <=  umgebung_type_TEST,
                        ABBRUCH, LOG_INT(implementation_type)) ;
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

   switch(implementation_type) {
   case umgebung_type_STATIC:    assert(0) ; break ;
   case umgebung_type_DEFAULT:   err = initdefault_umgebung(umg, &s_umgebung_shared) ; break ;
   case umgebung_type_TEST:      err = inittest_umgebung(umg, &s_umgebung_shared) ; break ;
   }

   if (err) goto ABBRUCH ;
   isinitumgebung = true ;
   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   if (0 == s_umgebungcount) {
      err = initonce_valid_umgebung(umg) ;
      if (err) goto ABBRUCH ;
   }

   ++ s_umgebungcount ;

   sunlock_mutex(&s_initlock) ;

   return 0 ;
ABBRUCH:
   if (isinitumgebung)  (void) umg->free_umgebung(umg) ;
   if (isinitshared)    (void) free_umgebungshared(&s_umgebung_shared) ;
   if (isinitonce_no)   (void) freeonce_no_umgebung() ;
   sunlock_mutex(&s_initlock) ;
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
   LOG_ABORT(err) ;
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

   err = init_umgebung(umg, implementation_type) ;
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

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung,ABBRUCH)

static int test_process_init(void)
{
   const umgebung_t * umg = &umgebung() ;

   // TEST static type
   TEST( umg->type           == umgebung_type_STATIC ) ;
   TEST( umg->resource_count == 0 ) ;
   TEST( umg->free_umgebung  == 0 ) ;
   TEST( umg->shared         == 0 );
   TEST( umg->ilog.object    == &g_main_logwriterlocked );
   TEST( umg->ilog.functable == (log_it*)&g_main_logwriterlocked_interface );
   TEST( umg->objectcache    == 0 );

   // TEST EINVAL: wrong type
   TEST(0      == umgebung_type_STATIC) ;
   TEST(EINVAL == initmain_umgebung(umgebung_type_STATIC)) ;
   TEST(EINVAL == initmain_umgebung(3)) ;

   // TEST init, double free (umgebung_type_DEFAULT)
   TEST(0 == umgebung().type) ;
   TEST(0 == initmain_umgebung(umgebung_type_DEFAULT)) ;
   TEST(umgebung_type_DEFAULT == umgebung().type) ;
   TEST(0 != umgebung().resource_count) ;
   TEST(0 != umgebung().free_umgebung) ;
   TEST(&s_umgebung_shared == umgebung().shared) ;
   TEST(0 != umgebung().shared->valuecache) ;
   TEST(0 != umgebung().ilog.object) ;
   TEST(0 != umgebung().objectcache) ;
   TEST(0 != strcmp("C", current_locale())) ;
   TEST(umgebung().ilog.object    != &g_main_logwriterlocked) ;
   TEST(umgebung().ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(0 == freemain_umgebung()) ;
   TEST(0 == umgebung().type ) ;
   TEST(0 == umgebung().resource_count) ;
   TEST(0 == umgebung().free_umgebung) ;
   TEST(0 == umgebung().shared) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(umgebung().ilog.object    == &g_main_logwriterlocked) ;
   TEST(umgebung().ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(0 == umgebung().objectcache) ;
   TEST(0 == freemain_umgebung()) ;
   TEST(0 == umgebung().type ) ;
   TEST(0 == umgebung().resource_count) ;
   TEST(0 == umgebung().free_umgebung) ;
   TEST(0 == umgebung().shared) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(umgebung().ilog.object    == &g_main_logwriterlocked) ;
   TEST(umgebung().ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(0 == umgebung().objectcache) ;

   // TEST init, double free (umgebung_type_TEST)
   TEST(0 == umgebung().type) ;
   TEST(0 == initmain_umgebung(umgebung_type_TEST)) ;
   TEST(umgebung_type_TEST  == umgebung().type) ;
   TEST(0 == umgebung().resource_count) ;
   TEST(0 != umgebung().free_umgebung) ;
   TEST(&s_umgebung_shared == umgebung().shared) ;
   TEST(0 != umgebung().shared->valuecache) ;
   TEST(0 != umgebung().ilog.object) ;
   TEST(0 != umgebung().objectcache) ;
   TEST(umgebung().ilog.object    != &g_main_logwriterlocked) ;
   TEST(umgebung().ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(0 == freemain_umgebung()) ;
   TEST(0 == umgebung().type ) ;
   TEST(0 == umgebung().resource_count) ;
   TEST(0 == umgebung().free_umgebung) ;
   TEST(0 == umgebung().shared) ;
   TEST(umgebung().ilog.object    == &g_main_logwriterlocked) ;
   TEST(umgebung().ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(0 == umgebung().objectcache) ;

   // TEST static type has not changed
   TEST( umg->type           == umgebung_type_STATIC ) ;
   TEST( umg->resource_count == 0 ) ;
   TEST( umg->free_umgebung  == 0 ) ;
   TEST( umg->shared         == 0 );
   TEST( umg->ilog.object    == &g_main_logwriterlocked );
   TEST( umg->ilog.functable == (log_it*)&g_main_logwriterlocked_interface );
   TEST( umg->objectcache    == 0 );
   LOG_FLUSHBUFFER() ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_umgebung_query(void)
{

   // TEST query umgebung()
   TEST( &umgebung() == &gt_umgebung ) ;

   // TEST query log_umgebung()
   TEST( &gt_umgebung.ilog == &log_umgebung() ) ;

   // TEST query log_umgebung()
   struct objectcache_t * const oldcache1 = gt_umgebung.objectcache ;
   gt_umgebung.objectcache = (struct objectcache_t*)3 ;
   TEST( (struct objectcache_t*)3 == objectcache_umgebung() ) ;
   gt_umgebung.objectcache = oldcache1 ;
   TEST( oldcache1 == objectcache_umgebung() ) ;

   if (gt_umgebung.shared) {
      struct valuecache_t * const oldcache2 = gt_umgebung.shared->valuecache ;
      gt_umgebung.shared->valuecache = (struct valuecache_t*)3 ;
      TEST( (struct valuecache_t*)3 == valuecache_umgebung() ) ;
      gt_umgebung.shared->valuecache = oldcache2 ;
      TEST( oldcache2 == valuecache_umgebung() ) ;

      TEST( gt_umgebung.shared == &s_umgebung_shared ) ;
   }

   // TEST s_initcount_no_umgebung
   if (s_umgebungcount) {
      TEST(3 == s_initcount_no_umgebung) ;
   } else {
      TEST(0 == s_initcount_no_umgebung) ;
   }

   // TEST s_initcount_valid_umgebung
   if (s_umgebungcount) {
      TEST(1 == s_initcount_valid_umgebung) ;
   } else {
      TEST(0 == s_initcount_valid_umgebung) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_umgebung_init(void)
{
   umgebung_t umg    = umgebung_INIT_FREEABLE ;
   const bool isINIT = (umgebung_type_STATIC != umgebung().type) ;

   if (!isINIT) {
      TEST(0 == initmain_umgebung(umgebung_type_DEFAULT)) ;
   }

   // TEST static init
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == umg.shared) ;
   TEST(0 == umg.ilog.object) ;
   TEST(0 == umg.ilog.functable) ;
   TEST(0 == umg.objectcache) ;

   // TEST EINVAL: wrong type
   TEST(EINVAL == init_umgebung(&umg, umgebung_type_STATIC)) ;
   TEST(EINVAL == init_umgebung(&umg, 3)) ;

   // TEST init, double free (umgebung_type_DEFAULT)
   TEST(0 == init_umgebung(&umg, umgebung_type_DEFAULT)) ;
   TEST(umgebung_type_DEFAULT == umg.type) ;
   TEST(0 != umg.resource_count) ;
   TEST(0 != umg.free_umgebung) ;
   TEST(&s_umgebung_shared == umg.shared) ;
   TEST(0 != umg.shared->valuecache) ;
   TEST(0 != umg.ilog.object) ;
   TEST(0 != umg.objectcache) ;
   TEST(umg.ilog.object    != &g_main_logwriterlocked) ;
   TEST(umg.ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(0 == umg.type ) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == umg.shared) ;
   TEST(umg.ilog.object    == &g_main_logwriterlocked) ;
   TEST(umg.ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(0 == umg.type ) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == umg.shared) ;
   TEST(umg.ilog.object    == &g_main_logwriterlocked) ;
   TEST(umg.ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(0 == umg.objectcache) ;

   // TEST init, double free (umgebung_type_TEST)
   TEST(0 == init_umgebung(&umg, umgebung_type_TEST)) ;
   TEST(umg.type        == umgebung_type_TEST) ;
   TEST(umg.shared      == &s_umgebung_shared) ;
   TEST(umg.shared->valuecache != 0) ;
   TEST(umg.ilog.object != 0) ;
   TEST(umg.ilog.object != &g_main_logwriterlocked) ;
   TEST(umg.ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(umg.objectcache != 0) ;
   TEST(umg.resource_count == 0 ) ;
   TEST(umg.free_umgebung  != 0 ) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(umg.type        == umgebung_type_STATIC) ;
   TEST(umg.resource_count == 0 ) ;
   TEST(umg.free_umgebung  == 0 ) ;
   TEST(umg.shared      == 0) ;
   TEST(umg.ilog.object    == &g_main_logwriterlocked) ;
   TEST(umg.ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
   TEST(umg.objectcache == 0) ;

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
      TEST(0 == s_initcount_no_umgebung) ;
      TEST(0 == s_initcount_valid_umgebung) ;
      TEST(0 == s_umgebungcount) ;
      TEST(0 == memcmp(&s_umgebung_shared, &shared2, sizeof(shared2))) ;
      TEST(umgebung_type_STATIC == umgebung().type) ;
      TEST(0 == umgebung().resource_count) ;
      TEST(0 == umgebung().free_umgebung) ;
      TEST(0 == umgebung().shared) ;
      TEST(umgebung().ilog.object    == &g_main_logwriterlocked) ;
      TEST(umgebung().ilog.functable == (log_it*)&g_main_logwriterlocked_interface) ;
      TEST(0 == umgebung().objectcache) ;
   }

   LOG_FLUSHBUFFER() ;
   char buffer[2048] = { 0 };
   TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

   TEST(0 == s_umgebungcount) ;
   TEST(0 == initmain_umgebung(type)) ;
   // make printed system error messages language (English) neutral
   resetmsg_locale() ;
   TEST(1 == s_umgebungcount) ;
   TEST(0 != s_initcount_no_umgebung) ;
   TEST(0 != s_initcount_valid_umgebung) ;
   TEST(0 != memcmp(&s_umgebung_shared, &shared2, sizeof(shared2))) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == close(fd_stderr)) ;
   fd_stderr = -1 ;
   TEST(0 == close(fdpipe[0])) ;
   TEST(0 == close(fdpipe[1])) ;

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
   close(fd_stderr) ;
   close(fdpipe[0]);
   close(fdpipe[1]);
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

      if (test_umgebung_query()) goto ABBRUCH ;
      if (test_process_init())   goto ABBRUCH ;
      if (test_umgebung_init())  goto ABBRUCH ;

      char buffer[2048] = { 0 };
      TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

      const char * expect =
         // log from test_process_init
         "C-kern/umgebung/umgebung.c:243: init_umgebung(): error: Function argument violates condition (umgebung_type_STATIC < implementation_type && implementation_type <= umgebung_type_TEST)"
         "\nimplementation_type=0"
         "\nC-kern/umgebung/umgebung.c:283: init_umgebung(): error: Function aborted (err=22)"
         "\nC-kern/umgebung/umgebung.c:320: initmain_umgebung(): error: Function aborted (err=22)"
         "\nC-kern/umgebung/umgebung.c:243: init_umgebung(): error: Function argument violates condition (umgebung_type_STATIC < implementation_type && implementation_type <= umgebung_type_TEST)"
         "\nimplementation_type=3"
         "\nC-kern/umgebung/umgebung.c:283: init_umgebung(): error: Function aborted (err=22)"
         "\nC-kern/umgebung/umgebung.c:320: initmain_umgebung(): error: Function aborted (err=22)"
         // log from test_umgebung_init
         "\nC-kern/umgebung/umgebung.c:243: init_umgebung(): error: Function argument violates condition (umgebung_type_STATIC < implementation_type && implementation_type <= umgebung_type_TEST)"
         "\nimplementation_type=0"
         "\nC-kern/umgebung/umgebung.c:283: init_umgebung(): error: Function aborted (err=22)"
         "\nC-kern/umgebung/umgebung.c:243: init_umgebung(): error: Function argument violates condition (umgebung_type_STATIC < implementation_type && implementation_type <= umgebung_type_TEST)"
         "\nimplementation_type=3"
         "\nC-kern/umgebung/umgebung.c:283: init_umgebung(): error: Function aborted (err=22)"
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
      TEST(0 == close(fd_stderr)) ;
      fd_stderr = -1 ;
      TEST(0 == close(fdpipe[0]));
      TEST(0 == close(fdpipe[1]));
      fdpipe[0] = fdpipe[1] = -1 ;

   } else {
      assert(umgebung_type_STATIC != umgebung().type) ;

      if (test_initmainerror()) goto ABBRUCH ;

      TEST(0 == init_resourceusage(&usage)) ;

      if (test_umgebung_query()) goto ABBRUCH ;
      if (test_umgebung_init())  goto ABBRUCH ;
      if (test_initmainerror())  goto ABBRUCH ;

      TEST(0 == same_resourceusage(&usage)) ;
      TEST(0 == free_resourceusage(&usage)) ;
   }

   return 0 ;
ABBRUCH:
   TEST(0 == free_resourceusage(&usage)) ;
   if (-1 != fd_stderr) {
      dup2(fd_stderr, STDERR_FILENO) ;
      close(fd_stderr) ;
   }
   if (-1 != fdpipe[0]) {
      close(fdpipe[0]) ;
      close(fdpipe[1]) ;
   }
   return EINVAL ;
}

#endif
