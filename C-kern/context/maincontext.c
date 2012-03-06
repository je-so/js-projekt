/* title: MainContext impl
   Implementation file of global init anf free functions.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/maincontext.h
    Header file of <MainContext>.

   file: C-kern/context/maincontext.c
    Implementation file <MainContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/maincontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/writer/logmain.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/thread.h"
#endif


// section: maincontext_t

// group: variables

/* variable: g_maincontext
 * Reserve space for the global main context. */
#define THREAD 1
maincontext_t                 g_maincontext     = {   processcontext_INIT_FREEABLE,
#if (!((KONFIG_SUBSYS)&THREAD))
                                                      threadcontext_INIT_STATIC,
#endif
                                                      maincontext_STATIC,
                                                      0,
                                                      0
                                                   } ;
#undef THREAD

#ifdef KONFIG_UNITTEST
/* variable: s_error_init
 * Simulates an error in <init_maincontext>. */
static test_errortimer_t      s_error_init      = test_errortimer_INIT_FREEABLE ;
#endif


// group: lifetime

int free_maincontext(void)
{
   int err ;
   int err2 ;
   int     is_initialized = (maincontext_STATIC != type_maincontext()) ;

   if (is_initialized) {
      err = free_threadcontext(&thread_maincontext()) ;

      err2 = free_processcontext(&g_maincontext.pcontext) ;
      if (err2) err = err2 ;

      g_maincontext.type = maincontext_STATIC ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int init_maincontext(maincontext_e context_type, int argc, const char ** argv)
{
   int err ;
   int is_already_initialized = (maincontext_STATIC != type_maincontext()) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ABBRUCH ;
   }

   VALIDATE_INPARAM_TEST(     maincontext_STATIC  <  context_type
                           && maincontext_DEFAULT >= context_type, ABBRUCH, ) ;

   VALIDATE_INPARAM_TEST(      argc >= 0
                           && (argc == 0 || argv != 0), ABBRUCH, ) ;

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   err = init_processcontext(&g_maincontext.pcontext) ;
   if (err) goto ABBRUCH ;

   g_maincontext.type = context_type ;
   g_maincontext.argc = argc ;
   g_maincontext.argv = argv ;

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   err = init_threadcontext(&thread_maincontext()) ;
   if (err) goto ABBRUCH ;

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   return 0 ;
ABBRUCH:
   if (!is_already_initialized) {
      free_maincontext() ;
   }
   LOG_ABORT(err) ;
   return err ;
}

void abort_maincontext(int err)
{
   // TODO: add abort handler registration ...
   // TODO: add unit test for checking that resources are freed
   LOG_ERRTEXT(ABORT_FATAL(err)) ;
   LOG_FLUSHBUFFER() ;
   abort() ;
}

void assertfail_maincontext(
   const char * condition,
   const char * file,
   unsigned     line,
   const char * funcname)
{
   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ERROR_LOCATION(file, line, funcname)) ;
   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ABORT_ASSERT_FAILED(condition)) ;
   abort_maincontext(EINVAL) ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initmain(void)
{
   int               fd_stderr   = -1 ;
   int               fdpipe[2]   = { -1, -1 } ;
   maincontext_t     old_context = g_maincontext ;

   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;

   TEST(0 == free_maincontext()) ;

   // TEST static type
   TEST(0 == type_maincontext()) ;
   TEST(0 == process_maincontext().initcount) ;
   TEST(0 == process_maincontext().valuecache) ;
   TEST(0 == thread_maincontext().initcount) ;
   TEST(&g_logmain == thread_maincontext().ilog.object) ;
   TEST(&g_logmain_interface == thread_maincontext().ilog.functable) ;
   TEST(0 == thread_maincontext().objectcache.object) ;
   TEST(0 == thread_maincontext().objectcache.functable) ;

   // TEST EINVAL
   TEST(0      == maincontext_STATIC) ;
   TEST(EINVAL == init_maincontext(maincontext_STATIC, 0, 0)) ;
   TEST(EINVAL == init_maincontext(3, 0, 0)) ;
   TEST(EINVAL == init_maincontext(maincontext_DEFAULT, -1, 0)) ;

   // TEST init, double free (context_DEFAULT)
   const char * argv[2] = { "1", "2" } ;
   TEST(0 == init_maincontext(maincontext_DEFAULT, 2, argv)) ;
   TEST(1 == type_maincontext()) ;
   TEST(2       == g_maincontext.argc) ;
   TEST(argv    == g_maincontext.argv) ;
   TEST(argv[0] == progname_maincontext()) ;
   TEST(0 != process_maincontext().valuecache) ;
   TEST(0 != thread_maincontext().initcount) ;
   TEST(0 != thread_maincontext().ilog.object) ;
   TEST(0 != thread_maincontext().ilog.functable) ;
   TEST(0 != thread_maincontext().objectcache.object) ;
   TEST(0 != thread_maincontext().objectcache.functable) ;
   TEST(0 != strcmp("C", current_locale())) ;
   TEST(thread_maincontext().ilog.object    != &g_logmain) ;
   TEST(thread_maincontext().ilog.functable != &g_logmain_interface) ;
   TEST(0 == free_maincontext()) ;
   TEST(0 == type_maincontext()) ;
   TEST(0 == process_maincontext().valuecache) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(thread_maincontext().initcount      == 0) ;
   TEST(thread_maincontext().ilog.object    == &g_logmain) ;
   TEST(thread_maincontext().ilog.functable == &g_logmain_interface) ;
   TEST(thread_maincontext().objectcache.object    == 0) ;
   TEST(thread_maincontext().objectcache.functable == 0) ;
   TEST(0 == free_maincontext()) ;
   TEST(0 == type_maincontext()) ;
   TEST(0 == process_maincontext().valuecache) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(thread_maincontext().initcount      == 0) ;
   TEST(thread_maincontext().ilog.object    == &g_logmain) ;
   TEST(thread_maincontext().ilog.functable == &g_logmain_interface) ;
   TEST(thread_maincontext().objectcache.object    == 0) ;
   TEST(thread_maincontext().objectcache.functable == 0) ;

   // TEST static type has not changed
   TEST(0 == type_maincontext()) ;
   TEST(0 == process_maincontext().initcount) ;
   TEST(0 == process_maincontext().valuecache) ;
   TEST(0 == thread_maincontext().initcount) ;
   TEST(&g_logmain == thread_maincontext().ilog.object) ;
   TEST(&g_logmain_interface == thread_maincontext().ilog.functable) ;
   TEST(0 == thread_maincontext().objectcache.object) ;
   TEST(0 == thread_maincontext().objectcache.functable) ;

   LOG_FLUSHBUFFER() ;
   char buffer[4096] = { 0 };
   TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_filedescr(&fd_stderr)) ;
   TEST(0 == free_filedescr(&fdpipe[0])) ;
   TEST(0 == free_filedescr(&fdpipe[1])) ;

   if (maincontext_STATIC != old_context.type) {
      init_maincontext(old_context.type, old_context.argc, old_context.argv) ;
      LOGC_PRINTF(ERR, "%s", buffer) ;
   }

   return 0 ;
ABBRUCH:
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_filedescr(&fd_stderr) ;
   free_filedescr(&fdpipe[0]);
   free_filedescr(&fdpipe[1]);
   return EINVAL ;
}

static int test_querymacros(void)
{

   // TEST query log_maincontext
   TEST( &(gt_thread_context.ilog) == &log_maincontext() ) ;

   // TEST query log_maincontext
   TEST( &(gt_thread_context.objectcache) == &objectcache_maincontext() ) ;

   // TEST query valuecache_maincontext
   struct valuecache_t * const oldcache2 = process_maincontext().valuecache ;
   process_maincontext().valuecache = (struct valuecache_t*)3 ;
   TEST((struct valuecache_t*)3 == valuecache_maincontext()) ;
   process_maincontext().valuecache = oldcache2 ;
   TEST(oldcache2 == valuecache_maincontext()) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_initerror(void)
{
   int               fd_stderr   = -1 ;
   int               fdpipe[2]   = { -1, -1 } ;
   maincontext_t     old_context = g_maincontext ;

   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;

   TEST(0 == free_maincontext()) ;
   TEST(maincontext_STATIC == type_maincontext()) ;

   // TEST EPROTO (call init_maincontext first)
   threadcontext_t tcontext ;
   TEST(EPROTO == init_threadcontext(&tcontext)) ;

   // TEST error in init_maincontext in different places (called from initmain)
   for(int i = 1; i <= 3; ++i) {
      TEST(0 == init_testerrortimer(&s_error_init, (unsigned)i, EINVAL+i)) ;
      TEST(EINVAL+i == init_maincontext(maincontext_DEFAULT, 0, 0)) ;
      TEST(0 == process_maincontext().initcount) ;
      TEST(maincontext_STATIC == type_maincontext()) ;
      TEST(0 == thread_maincontext().initcount) ;
      TEST(&g_logmain == thread_maincontext().ilog.object) ;
      TEST(&g_logmain_interface == thread_maincontext().ilog.functable) ;
      TEST(0 == thread_maincontext().objectcache.object) ;
      TEST(0 == thread_maincontext().objectcache.functable) ;
   }

   LOG_FLUSHBUFFER() ;
   char buffer[4096] = { 0 };
   TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

   TEST(0 == init_maincontext(old_context.type ? old_context.type : maincontext_DEFAULT, old_context.argc, old_context.argv )) ;
   TEST(0 != process_maincontext().initcount) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_filedescr(&fd_stderr)) ;
   TEST(0 == free_filedescr(&fdpipe[0])) ;
   TEST(0 == free_filedescr(&fdpipe[1])) ;

   LOGC_PRINTF(ERR, "%s", buffer) ;

   // TEST EALREADY
   TEST(EALREADY == init_maincontext(maincontext_DEFAULT, 0, 0)) ;

   if (maincontext_STATIC == old_context.type) {
      LOG_CLEARBUFFER() ;
      free_maincontext() ;
   }

   return 0 ;
ABBRUCH:
   if (maincontext_STATIC == old_context.type) {
      LOG_CLEARBUFFER() ;
      free_maincontext() ;
   }
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_filedescr(&fd_stderr) ;
   free_filedescr(&fdpipe[0]);
   free_filedescr(&fdpipe[1]);
   return EINVAL ;
}

int unittest_context_maincontext()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (maincontext_STATIC == type_maincontext()) {

      if (test_querymacros())    goto ABBRUCH ;
      if (test_initmain())       goto ABBRUCH ;
      if (test_initerror())      goto ABBRUCH ;

   } else {
      assert(maincontext_STATIC != type_maincontext()) ;

      if (test_initerror())  goto ABBRUCH ;

      TEST(0 == init_resourceusage(&usage)) ;
      {  // TODO remove in case malloc is no more in use (init_resourceusage)
         resourceusage_t   usage2[10] ;
         for(unsigned i = 0; i < nrelementsof(usage2); ++i) {
            TEST(0 == init_resourceusage(&usage2[i])) ;
         }
         for(unsigned i = 0; i < nrelementsof(usage2); ++i) {
            TEST(0 == free_resourceusage(&usage2[i])) ;
         }
         TEST(0 == free_resourceusage(&usage)) ;
         TEST(0 == init_resourceusage(&usage)) ;
      }

      if (test_querymacros())    goto ABBRUCH ;
      if (test_initmain())       goto ABBRUCH ;
      if (test_initerror())      goto ABBRUCH ;

      TEST(0 == same_resourceusage(&usage)) ;
      TEST(0 == free_resourceusage(&usage)) ;
   }

   // make printed system error messages language (English) neutral
   resetmsg_locale() ;

   return 0 ;
ABBRUCH:
   TEST(0 == free_resourceusage(&usage)) ;
   return EINVAL ;
}

#endif