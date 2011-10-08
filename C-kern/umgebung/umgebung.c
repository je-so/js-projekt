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
    Implementation file of <Umgebung impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/err.h"
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/cache/valuecache.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/text.db/initprocess)WHERE(subsystem==''||subsystem=='X11')
#include "C-kern/api/os/locale.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/os/X11/x11.h"
#include "C-kern/api/os/sync/signal.h"
// TEXTDB:END
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/text.db/initonce)
#include "C-kern/api/os/thread.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/umgebung/testerror.h"
#endif


// section: Implementation

/* variable: gt_umgebung
 * Defines thread-local storage for global context <umgebung_t> which every modul can access.
 * */
__thread umgebung_t           gt_umgebung = umgebung_INIT_MAINSERVICES ;

/* variable: s_initprocess_count
 * Rememberes how many resources has been initialized successfully.
 * Used in <init_process_resources> and <free_process_resources>
 * which are called from <initprocess_umgebung> and <freeprocess_umgebung>. */
static uint16_t               s_initprocess_count = 0 ;

static int free_process_resources(void)
{
   int err = 0 ;
   int err2 ;

   switch(s_initprocess_count) {
   default: assert(0 == s_initprocess_count && "out of bounds" )  ;
            break ;
// TEXTDB:SELECT("   case "row-id":  err2 = "free-function"() ;"\n"            if (err2) err=err2 ;")FROM("C-kern/resource/text.db/initprocess")WHERE(subsystem==''||subsystem=='X11')DESCENDING
   case 4:  err2 = freeprocess_signalconfig() ;
            if (err2) err=err2 ;
   case 3:  err2 = freeprocess_X11() ;
            if (err2) err=err2 ;
   case 2:  err2 = freeprocess_valuecache() ;
            if (err2) err=err2 ;
   case 1:  err2 = freeprocess_locale() ;
            if (err2) err=err2 ;
// TEXTDB:END
   case 0:  break ;
   }

   s_initprocess_count = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int init_process_resources(void)
{
   int err ;

   if (s_initprocess_count) {
      return 0 ;
   }

// TEXTDB:SELECT(\n"   err = "init-function"() ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ s_initprocess_count ;")FROM("C-kern/resource/text.db/initprocess")WHERE(subsystem==''||subsystem=='X11')

   err = initprocess_locale() ;
   if (err) goto ABBRUCH ;
   ++ s_initprocess_count ;

   err = initprocess_valuecache() ;
   if (err) goto ABBRUCH ;
   ++ s_initprocess_count ;

   err = initprocess_X11() ;
   if (err) goto ABBRUCH ;
   ++ s_initprocess_count ;

   err = initprocess_signalconfig() ;
   if (err) goto ABBRUCH ;
   ++ s_initprocess_count ;
// TEXTDB:END

   return 0 ;
ABBRUCH:
   (void) free_process_resources() ;
   LOG_ABORT(err) ;
   return err ;
}

static int call_initonce_functions(void)
{
   int err ;

// TEXTDB:SELECT(\n"   err = "initonce-function"() ;"\n"   if (err) goto ABBRUCH ;")FROM("C-kern/resource/text.db/initonce")

   err = initonce_osthread() ;
   if (err) goto ABBRUCH ;
// TEXTDB:END

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int free_umgebung(umgebung_t * umg)
{
   int err ;

   if (umg->free_umgebung) {
      err = umg->free_umgebung(umg) ;
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

   if (     implementation_type <= umgebung_type_STATIC
         || implementation_type >  umgebung_type_TEST) {
      err = EINVAL ;
      LOG_INT(implementation_type) ;
      goto ABBRUCH ;
   }

   switch(implementation_type) {
   case umgebung_type_STATIC:    assert(0) ; break ;
   case umgebung_type_DEFAULT:   err = initdefault_umgebung(umg) ; break ;
   case umgebung_type_TEST:      err = inittestproxy_umgebung(umg) ; break ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freeprocess_umgebung(void)
{
   int err ;
   int err2 ;
   umgebung_t       * umg = &gt_umgebung ;
   int     is_initialized = (0 != umg->type) ;

   if (is_initialized) {
      err = free_umgebung(umg) ;
      err2 = free_process_resources() ;
      if (err2) err = err2 ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initprocess_umgebung(umgebung_type_e implementation_type)
{
   int err ;
   umgebung_t           * umg = &gt_umgebung ;
   int is_already_initialized = (0 != umg->type) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ABBRUCH ;
   }

   err = init_process_resources() ;
   if (err) goto ABBRUCH ;

   err = init_umgebung(umg, implementation_type) ;
   if (err) goto ABBRUCH ;

   err = call_initonce_functions() ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) free_umgebung(umg) ;
   (void) free_process_resources() ;
   LOG_ABORT(err) ;
   return err ;
}

void abort_umgebung(void)
{
   // TODO: add abort handler registration ...
   abort() ;
   // TODO: add unit test for checking that resources are freed
}

void assertfail_umgebung(
   const char * condition,
   const char * file,
   unsigned     line,
   const char * funcname)
{
   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ERROR_LOCATION(file, line, funcname)) ;
   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ERROR_ASSERT_FAILED(condition)) ;
   abort_umgebung() ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung,ABBRUCH)

static int test_process_init(void)
{
   const umgebung_t * umg = umgebung() ;

   // TEST static type
   TEST( umg->type           == umgebung_type_STATIC ) ;
   TEST( umg->log            == &g_main_logservice );
   TEST( umg->objectcache    == 0 );
   TEST( umg->valuecache     == 0 );
   TEST( umg->free_umgebung  == 0 ) ;
   TEST( umg->resource_count == 0 ) ;

   // TEST EINVAL: wrong type
   TEST(0      == umgebung_type_STATIC) ;
   TEST(EINVAL == initprocess_umgebung(umgebung_type_STATIC)) ;
   TEST(EINVAL == initprocess_umgebung(3)) ;

   // TEST EACCES  simulated error
   setiniterror_umgebungtesterror(EACCES) ;
   TEST(EACCES == initprocess_umgebung(umgebung_type_DEFAULT)) ;
   TEST(EACCES == initprocess_umgebung(umgebung_type_TEST)) ;
   cleariniterror_umgebungtesterror() ;

   // TEST init, double free (umgebung_type_DEFAULT)
   TEST(0 == umgebung()->type) ;
   TEST(0 == initprocess_umgebung(umgebung_type_DEFAULT)) ;
   TEST(umgebung_type_DEFAULT == umgebung()->type) ;
   TEST(0 != umgebung()->resource_count) ;
   TEST(0 != umgebung()->free_umgebung) ;
   TEST(0 != umgebung()->log) ;
   TEST(0 != umgebung()->objectcache) ;
   TEST(0 != umgebung()->valuecache) ;
   TEST(0 != strcmp("C", current_locale())) ;
   TEST(&g_main_logservice  != umgebung()->log) ;
   TEST(&g_main_valuecache  == umgebung()->valuecache) ;
   TEST(0 == freeprocess_umgebung()) ;
   TEST(0 == umgebung()->type ) ;
   TEST(0 == umgebung()->resource_count) ;
   TEST(0 == umgebung()->free_umgebung) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(&g_main_logservice  == umgebung()->log) ;
   TEST(0 == umgebung()->objectcache) ;
   TEST(0 == umgebung()->valuecache) ;
   TEST(0 == freeprocess_umgebung()) ;
   TEST(0 == umgebung()->type ) ;
   TEST(0 == umgebung()->resource_count) ;
   TEST(0 == umgebung()->free_umgebung) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(&g_main_logservice  == umgebung()->log) ;
   TEST(0 == umgebung()->objectcache) ;
   TEST(0 == umgebung()->valuecache) ;

   // TEST init, double free (umgebung_type_TEST)
   TEST(0 == umgebung()->type) ;
   TEST(0 == initprocess_umgebung(umgebung_type_TEST)) ;
   TEST(umgebung_type_TEST  == umgebung()->type) ;
   TEST(0 != umgebung()->log) ;
   TEST(0 != umgebung()->objectcache) ;
   TEST(0 != umgebung()->valuecache) ;
   TEST(0 == umgebung()->resource_count) ;
   TEST(0 != umgebung()->free_umgebung) ;
   TEST(0 == freeprocess_umgebung()) ;
   TEST(0 == umgebung()->type ) ;
   TEST(0 == umgebung()->resource_count) ;
   TEST(0 == umgebung()->free_umgebung) ;
   TEST(&g_main_logservice  == umgebung()->log) ;
   TEST(0 == umgebung()->objectcache) ;
   TEST(0 == umgebung()->valuecache) ;

   // TEST static type has not changed
   TEST( umg->type           == umgebung_type_STATIC ) ;
   TEST( umg->log            == &g_main_logservice );
   TEST( umg->objectcache    == 0 );
   TEST( umg->valuecache     == 0 );
   TEST( umg->free_umgebung  == 0 ) ;
   TEST( umg->resource_count == 0 ) ;

   return 0 ;
ABBRUCH:
   cleariniterror_umgebungtesterror() ;
   return EINVAL ;
}

static int test_umgebung_query(void)
{

   // TEST query umgebung()
   TEST( umgebung() == &gt_umgebung ) ;

   // TEST query log_umgebung()
   log_config_t * const oldlog = gt_umgebung.log ;
   gt_umgebung.log = (log_config_t *) 3 ;
   TEST( (log_config_t*)3 == log_umgebung() ) ;
   gt_umgebung.log = oldlog ;
   TEST( oldlog == log_umgebung() ) ;

   // TEST query log_umgebung()
   objectcache_t * const oldcache1 = gt_umgebung.objectcache ;
   gt_umgebung.objectcache = (objectcache_t*)3 ;
   TEST( (objectcache_t*)3 == objectcache_umgebung() ) ;
   gt_umgebung.objectcache = oldcache1 ;
   TEST( oldcache1 == objectcache_umgebung() ) ;

   valuecache_t * const oldcache2 = gt_umgebung.valuecache ;
   gt_umgebung.valuecache = (valuecache_t*)3 ;
   TEST( (valuecache_t*)3 == valuecache_umgebung() ) ;
   gt_umgebung.valuecache = oldcache2 ;
   TEST( oldcache2 == valuecache_umgebung() ) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_umgebung_init(void)
{
   umgebung_t umg    = umgebung_INIT_FREEABLE ;
   const bool isINIT = (umgebung_type_STATIC != umgebung()->type) ;

   if (!isINIT) {
      TEST(0 == initprocess_umgebung(umgebung_type_DEFAULT)) ;
   }

   // TEST static init
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;

   // TEST EINVAL: wrong type
   TEST(EINVAL == init_umgebung(&umg, umgebung_type_STATIC)) ;
   TEST(EINVAL == init_umgebung(&umg, 3)) ;

   // TEST init, double free (umgebung_type_DEFAULT)
   TEST(0 == init_umgebung(&umg, umgebung_type_DEFAULT)) ;
   TEST(umgebung_type_DEFAULT == umg.type) ;
   TEST(0 != umg.resource_count) ;
   TEST(0 != umg.free_umgebung) ;
   TEST(0 != umg.log) ;
   TEST(0 != umg.objectcache) ;
   TEST(0 != umg.valuecache) ;
   TEST(&g_main_logservice != umg.log) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(0 == umg.type ) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(&g_main_logservice == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(0 == umg.type ) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(&g_main_logservice  == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;

   // TEST init, double free (umgebung_type_TEST)
   TEST(0 == init_umgebung(&umg, umgebung_type_TEST)) ;
   TEST(umg.type        == umgebung_type_TEST) ;
   TEST(umg.log         != 0) ;
   TEST(umg.log         != &g_main_logservice) ;
   TEST(umg.objectcache != 0) ;
   TEST(umg.valuecache  == &g_main_valuecache) ;
   TEST(umg.resource_count == 0 ) ;
   TEST(umg.free_umgebung  != 0 ) ;
   TEST(0 == free_umgebung(&umg)) ;
   TEST(umg.type        == umgebung_type_STATIC) ;
   TEST(umg.resource_count == 0 ) ;
   TEST(umg.free_umgebung  == 0 ) ;
   TEST(umg.log         == &g_main_logservice) ;
   TEST(umg.objectcache == 0) ;
   TEST(umg.valuecache  == 0) ;

   if (!isINIT) {
      TEST(0 == freeprocess_umgebung()) ;
   }

   return 0 ;
ABBRUCH:
   if (  !isINIT
      && (umgebung_type_STATIC != umgebung()->type)) {
      TEST(0 == freeprocess_umgebung()) ;
   }
   free_umgebung(&umg) ;
   return EINVAL ;
}

int unittest_umgebung()
{
   int fd_stderr = -1 ;
   int fdpipe[2] = { -1, -1 } ;

   if (umgebung_type_STATIC == umgebung()->type) {

      fd_stderr = dup(STDERR_FILENO) ;
      TEST(-1!= fd_stderr) ;
      TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
      TEST(-1!= dup2(fdpipe[1], STDERR_FILENO)) ;

      if (test_process_init())   goto ABBRUCH ;
      if (test_umgebung_init())  goto ABBRUCH ;

      char buffer[2048] = { 0 };
      TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

      if (strcmp( buffer, "implementation_type=0\n"
                           // log from test_process_init
                           "C-kern/umgebung/umgebung.c:176: init_umgebung(): error: "
                           "Function aborted (err=22)\n"
                           "C-kern/umgebung/umgebung.c:225: initprocess_umgebung(): error: "
                           "Function aborted (err=22)\n"
                           "implementation_type=3\n"
                           "C-kern/umgebung/umgebung.c:176: init_umgebung(): error: "
                           "Function aborted (err=22)\n"
                           "C-kern/umgebung/umgebung.c:225: initprocess_umgebung(): error: "
                           "Function aborted (err=22)\n"
                           "C-kern/umgebung/umgebung_initdefault.c:107: init_thread_resources(): error: "
                           "Function aborted (err=13)\n"
                           "C-kern/umgebung/umgebung_initdefault.c:147: initdefault_umgebung(): error: "
                           "Function aborted (err=13)\n"
                           "C-kern/umgebung/umgebung.c:176: init_umgebung(): error: "
                           "Function aborted (err=13)\n"
                           "C-kern/umgebung/umgebung.c:225: initprocess_umgebung(): error: "
                           "Function aborted (err=13)\n"
                           "C-kern/umgebung/umgebung_inittestproxy.c:90: inittestproxy_umgebung(): error: "
                           "Function aborted (err=13)\n"
                           "C-kern/umgebung/umgebung.c:176: init_umgebung(): error: "
                           "Function aborted (err=13)\n"
                           "C-kern/umgebung/umgebung.c:225: initprocess_umgebung(): error: "
                           "Function aborted (err=13)\n"
                           // log from test_umgebung_init
                           "implementation_type=0\n"
                           "C-kern/umgebung/umgebung.c:176: init_umgebung(): error: "
                           "Function aborted (err=22)\n"
                           "implementation_type=3\n"
                           "C-kern/umgebung/umgebung.c:176: init_umgebung(): error: "
                           "Function aborted (err=22)\n"
                           )) {
         printf("buffer=-----\n%s-----\n",buffer) ;
         TEST(0 == "wrong buffer content") ;
      }

      TEST(-1!= dup2(fd_stderr, STDERR_FILENO)) ;
      TEST(0 == close(fd_stderr)) ;
      fd_stderr = -1 ;
      for(int i = 0; i < 2; ++i) {
         TEST(0 == close(fdpipe[i]));
         fdpipe[i] = -1 ;
      }

   }

   if (test_umgebung_query()) goto ABBRUCH ;
   if (umgebung_type_STATIC != umgebung()->type) {
      if (test_umgebung_init())  goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
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

