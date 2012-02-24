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

   file: C-kern/api/context.h
    Header file of <MainContext>.

   file: C-kern/context/context.c
    Implementation file <MainContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/context.h"
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


// section: context_t

// group: variables

/* variable: g_context
 * Reserve space for the global main context. */
context_t                     g_context         = {    processcontext_INIT_FREEABLE
#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
                                                      ,threadcontext_INIT_STATIC
#endif
#undef THREAD
                                                      ,context_STATIC
                                                   } ;

#ifdef KONFIG_UNITTEST
/* variable: s_error_init
 * Simulates an error in <initmain_context>. */
static test_errortimer_t      s_error_init      = test_errortimer_INIT_FREEABLE ;
#endif


// group: lifetime

int freemain_context(void)
{
   int err ;
   int err2 ;
   int     is_initialized = (context_STATIC != type_context()) ;

   if (is_initialized) {
      err = free_threadcontext(&thread_context()) ;

      err2 = free_processcontext(&g_context.pcontext) ;
      if (err2) err = err2 ;

      g_context.type = context_STATIC ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int initmain_context(context_e context_type)
{
   int err ;
   int is_already_initialized = (context_STATIC != type_context()) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ABBRUCH ;
   }

   VALIDATE_INPARAM_TEST(     context_STATIC  <  context_type
                           && context_DEFAULT >= context_type, ABBRUCH, ) ;

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   err = init_processcontext(&g_context.pcontext) ;
   if (err) goto ABBRUCH ;

   g_context.type = context_type ;

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   err = init_threadcontext(&thread_context()) ;
   if (err) goto ABBRUCH ;

   ONERROR_testerrortimer(&s_error_init, ABBRUCH) ;

   return 0 ;
ABBRUCH:
   if (!is_already_initialized) {
      freemain_context() ;
   }
   LOG_ABORT(err) ;
   return err ;
}

void abort_context(int err)
{
   // TODO: add abort handler registration ...
   // TODO: add unit test for checking that resources are freed
   LOG_ERRTEXT(ABORT_FATAL(err)) ;
   LOG_FLUSHBUFFER() ;
   abort() ;
}

void assertfail_context(
   const char * condition,
   const char * file,
   unsigned     line,
   const char * funcname)
{
   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ERROR_LOCATION(file, line, funcname)) ;
   LOGC_TEXTRES(ERR, TEXTRES_ERRORLOG_ABORT_ASSERT_FAILED(condition)) ;
   abort_context(EINVAL) ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initmain(void)
{
   int               fd_stderr = -1 ;
   int               fdpipe[2] = { -1, -1 } ;
   context_e         type      = type_context() ;

   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;

   TEST(0 == freemain_context()) ;

   // TEST static type
   TEST(0 == type_context()) ;
   TEST(0 == process_context().initcount) ;
   TEST(0 == process_context().valuecache) ;
   TEST(0 == thread_context().initcount) ;
   TEST(&g_logmain == thread_context().ilog.object) ;
   TEST(&g_logmain_interface == thread_context().ilog.functable) ;
   TEST(0 == thread_context().objectcache.object) ;
   TEST(0 == thread_context().objectcache.functable) ;

   // TEST EINVAL: wrong type
   TEST(0      == context_STATIC) ;
   TEST(EINVAL == initmain_context(context_STATIC)) ;
   TEST(EINVAL == initmain_context(3)) ;

   // TEST init, double free (context_DEFAULT)
   TEST(0 == initmain_context(context_DEFAULT)) ;
   TEST(1 == type_context()) ;
   TEST(0 != process_context().valuecache) ;
   TEST(0 != thread_context().initcount) ;
   TEST(0 != thread_context().ilog.object) ;
   TEST(0 != thread_context().ilog.functable) ;
   TEST(0 != thread_context().objectcache.object) ;
   TEST(0 != thread_context().objectcache.functable) ;
   TEST(0 != strcmp("C", current_locale())) ;
   TEST(thread_context().ilog.object    != &g_logmain) ;
   TEST(thread_context().ilog.functable != &g_logmain_interface) ;
   TEST(0 == freemain_context()) ;
   TEST(0 == type_context()) ;
   TEST(0 == process_context().valuecache) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(thread_context().initcount      == 0) ;
   TEST(thread_context().ilog.object    == &g_logmain) ;
   TEST(thread_context().ilog.functable == &g_logmain_interface) ;
   TEST(thread_context().objectcache.object    == 0) ;
   TEST(thread_context().objectcache.functable == 0) ;
   TEST(0 == freemain_context()) ;
   TEST(0 == type_context()) ;
   TEST(0 == process_context().valuecache) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(thread_context().initcount      == 0) ;
   TEST(thread_context().ilog.object    == &g_logmain) ;
   TEST(thread_context().ilog.functable == &g_logmain_interface) ;
   TEST(thread_context().objectcache.object    == 0) ;
   TEST(thread_context().objectcache.functable == 0) ;

   // TEST static type has not changed
   TEST(0 == type_context()) ;
   TEST(0 == process_context().initcount) ;
   TEST(0 == process_context().valuecache) ;
   TEST(0 == thread_context().initcount) ;
   TEST(&g_logmain == thread_context().ilog.object) ;
   TEST(&g_logmain_interface == thread_context().ilog.functable) ;
   TEST(0 == thread_context().objectcache.object) ;
   TEST(0 == thread_context().objectcache.functable) ;

   LOG_FLUSHBUFFER() ;
   char buffer[4096] = { 0 };
   TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_filedescr(&fd_stderr)) ;
   TEST(0 == free_filedescr(&fdpipe[0])) ;
   TEST(0 == free_filedescr(&fdpipe[1])) ;

   if (context_STATIC != type) {
      initmain_context(type) ;
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

   // TEST query log_context
   TEST( &(gt_thread_context.ilog) == &log_context() ) ;

   // TEST query log_context
   TEST( &(gt_thread_context.objectcache) == &objectcache_context() ) ;

   // TEST query valuecache_context
   struct valuecache_t * const oldcache2 = process_context().valuecache ;
   process_context().valuecache = (struct valuecache_t*)3 ;
   TEST((struct valuecache_t*)3 == valuecache_context()) ;
   process_context().valuecache = oldcache2 ;
   TEST(oldcache2 == valuecache_context()) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_initerror(void)
{
   int               fd_stderr = -1 ;
   int               fdpipe[2] = { -1, -1 } ;
   context_e         type      = type_context() ;

   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;

   TEST(0 == freemain_context()) ;
   TEST(context_STATIC == type_context()) ;

   // TEST EPROTO (call initmain_context first)
   threadcontext_t tcontext ;
   TEST(EPROTO == init_threadcontext(&tcontext)) ;

   // TEST error in initmain_context in different places (called from initmain)
   for(int i = 1; i <= 3; ++i) {
      TEST(0 == init_testerrortimer(&s_error_init, (unsigned)i, EINVAL+i)) ;
      TEST(EINVAL+i == initmain_context(context_DEFAULT)) ;
      TEST(0 == process_context().initcount) ;
      TEST(context_STATIC == type_context()) ;
      TEST(0 == thread_context().initcount) ;
      TEST(&g_logmain == thread_context().ilog.object) ;
      TEST(&g_logmain_interface == thread_context().ilog.functable) ;
      TEST(0 == thread_context().objectcache.object) ;
      TEST(0 == thread_context().objectcache.functable) ;
   }

   LOG_FLUSHBUFFER() ;
   char buffer[4096] = { 0 };
   TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

   TEST(0 == initmain_context(type ? type : context_DEFAULT)) ;
   TEST(0 != process_context().initcount) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_filedescr(&fd_stderr)) ;
   TEST(0 == free_filedescr(&fdpipe[0])) ;
   TEST(0 == free_filedescr(&fdpipe[1])) ;

   LOGC_PRINTF(ERR, "%s", buffer) ;

   // TEST EALREADY
   TEST(EALREADY == initmain_context(context_DEFAULT)) ;

   if (context_STATIC == type) {
      LOG_CLEARBUFFER() ;
      freemain_context() ;
   }

   return 0 ;
ABBRUCH:
   if (context_STATIC == type) {
      LOG_CLEARBUFFER() ;
      freemain_context() ;
   }
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_filedescr(&fd_stderr) ;
   free_filedescr(&fdpipe[0]);
   free_filedescr(&fdpipe[1]);
   return EINVAL ;
}

int unittest_context()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (context_STATIC == type_context()) {

      if (test_querymacros())    goto ABBRUCH ;
      if (test_initmain())       goto ABBRUCH ;
      if (test_initerror())      goto ABBRUCH ;

   } else {
      assert(context_STATIC != type_context()) ;

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
