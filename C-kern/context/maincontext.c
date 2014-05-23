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
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/platform/startup.h"
#include "C-kern/api/platform/sysuser.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/mm/mm_impl.h"
#include "C-kern/api/test/mm/testmm.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/task/thread_tls.h"
#endif


// section: maincontext_t

// group: constants

/* define: maincontext_STATICSIZE
 * Defines size for <s_maincontext_staticmem>.
 * This extrasize is needed during unit tests.
 * This value is a copy of <processcontext_STATICSIZE>. */
#define maincontext_STATICSIZE processcontext_STATICSIZE

#ifdef KONFIG_UNITTEST
/* define: maincontext_STATICTESTSIZE
 * Defines additonal size for <s_maincontext_staticmem>.
 * This extrasize is needed during unit test time. */
#define maincontext_STATICTESTSIZE (maincontext_STATICSIZE)
#else
#define maincontext_STATICTESTSIZE 0
#endif

// group: variables

/* variable: g_maincontext
 * Reserve space for the global main context. */
maincontext_t              g_maincontext  = {
   processcontext_INIT_STATIC,
   maincontext_STATIC,
   0,
   0,
   0,
   0
} ;

/* variable: s_maincontext_staticmem
 * Static memory block used in in global <maincontext_t>. */
static uint8_t             s_maincontext_staticmem[maincontext_STATICSIZE + maincontext_STATICTESTSIZE] = { 0 } ;

#ifdef KONFIG_UNITTEST
/* variable: s_maincontext_errtimer
 * Simulates an error in <init_maincontext>. */
static test_errortimer_t   s_maincontext_errtimer = test_errortimer_FREE ;
#endif

// group: helper

static inline void compiletime_assert(void)
{
   static_assert(&((maincontext_t*)0)->pcontext == (processcontext_t*)0, "extends from processcontext_t") ;
}

/* function: initprogname_maincontext
 * Sets *progname to (strrchr(argv0, "/")+1). */
static void initprogname_maincontext(const char ** progname, const char * argv0)
{
   const char * last = strrchr(argv0, '/') ;

   if (last)
      ++ last ;
   else
      last = argv0 ;

   const size_t len = strlen(last) ;
   if (len > 16) {
      *progname = last + len - 16;
   } else {
      *progname = last ;
   }
}

static int startup_maincontext(void * user)
{
   const maincontext_startparam_t * startparam = user;

   int err = init_maincontext(startparam->context_type, startparam->argc, startparam->argv);
   if (err) return err;

   int thread_err = startparam->main_thread(&g_maincontext);

   err = free_maincontext();

   return thread_err ? thread_err : err;
}

// group: lifetime

int free_maincontext(void)
{
   int err ;
   int err2 ;
   int is_initialized = (maincontext_STATIC != g_maincontext.type) ;

   if (is_initialized) {
      err = free_threadcontext(tcontext_maincontext()) ;

      err2 = free_processcontext(&g_maincontext.pcontext) ;
      if (err2) err = err2 ;

      if (g_maincontext.size_staticmem) {
         err = ENOTEMPTY ;
      }

      g_maincontext.type     = maincontext_STATIC ;
      g_maincontext.progname = 0 ;
      g_maincontext.argc     = 0 ;
      g_maincontext.argv     = 0 ;
      g_maincontext.size_staticmem = 0 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

int initstart_maincontext(const maincontext_startparam_t * startparam)
{
   int err ;
   int is_already_initialized = (maincontext_STATIC != g_maincontext.type) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ONABORT ;
   }

   err = startup_platform(&startup_maincontext, CONST_CAST(maincontext_startparam_t,startparam));

ONABORT:
   return err ;
}

int init_maincontext(const maincontext_e context_type, int argc, const char ** argv)
{
   int err ;
   int is_already_initialized = (maincontext_STATIC != g_maincontext.type) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ONABORT ;
   }

   VALIDATE_INPARAM_TEST(  maincontext_STATIC  <  context_type
                           && maincontext_CONSOLE >= context_type, ONABORT, ) ;

   VALIDATE_INPARAM_TEST(argc >= 0 && (argc == 0 || argv != 0), ONABORT, ) ;

   // startup_platform has been called !!
   VALIDATE_INPARAM_TEST(  self_maincontext() == &g_maincontext, ONABORT, ) ;

   ONERROR_testerrortimer(&s_maincontext_errtimer, &err, ONABORT) ;

   g_maincontext.type     = context_type ;
   g_maincontext.progname = "" ;
   g_maincontext.argc     = argc ;
   g_maincontext.argv     = argv ;

   if (argc) {
      initprogname_maincontext(&g_maincontext.progname, argv[0]) ;
   }

   err = init_processcontext(&g_maincontext.pcontext) ;
   if (err) goto ONABORT ;


   ONERROR_testerrortimer(&s_maincontext_errtimer, &err, ONABORT) ;

   err = init_threadcontext(tcontext_maincontext(), &g_maincontext.pcontext, context_type) ;
   if (err) goto ONABORT ;

   ONERROR_testerrortimer(&s_maincontext_errtimer, &err, ONABORT) ;

   return 0 ;
ONABORT:
   if (!is_already_initialized) {
      free_maincontext() ;
   }
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

void abort_maincontext(int err)
{
   // TODO: add abort handler registration ...
   //       add unit test for checking that resources are freed
   TRACE_NOARG_ERRLOG(log_flags_END, PROGRAM_ABORT, err) ;
   FLUSHBUFFER_ERRLOG() ;
   abort() ;
}

void assertfail_maincontext(
   const char * condition,
   const char * file,
   int          line,
   const char * funcname)
{
   TRACE2_ERRLOG(log_flags_END, ASSERT_FAILED, funcname, file, line, EINVAL, condition) ;
   abort_maincontext(EINVAL) ;
}

// group: static-memory

void * allocstatic_maincontext(uint8_t size)
{
   int err ;

   if (sizeof(s_maincontext_staticmem) - g_maincontext.size_staticmem < size) {
      err = ENOMEM ;
      TRACEOUTOFMEM_ERRLOG(size, err) ;
      goto ONABORT ;
   }

   uint16_t offset = g_maincontext.size_staticmem ;

   g_maincontext.size_staticmem = (uint16_t) (g_maincontext.size_staticmem + size) ;

   return &s_maincontext_staticmem[offset] ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return 0 ;
}

int freestatic_maincontext(uint8_t size)
{
   int err ;

   VALIDATE_INPARAM_TEST(g_maincontext.size_staticmem >= size, ONABORT,) ;

   g_maincontext.size_staticmem = (uint16_t) (g_maincontext.size_staticmem - size) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_querymacros(void)
{
   // TEST blockmap_maincontext
   TEST(&blockmap_maincontext()    == &pcontext_maincontext()->blockmap) ;

   // TEST error_maincontext
   TEST(&error_maincontext()       == &pcontext_maincontext()->error) ;

   // TEST log_maincontext
   TEST(&log_maincontext()         == &tcontext_maincontext()->log) ;

   // TEST log_maincontext
   TEST(&objectcache_maincontext() == &tcontext_maincontext()->objectcache) ;

   // TEST pagecache_maincontext
   TEST(&pagecache_maincontext()   == &tcontext_maincontext()->pagecache) ;

   // TEST pcontext_maincontext
   TEST(pcontext_maincontext()     == &g_maincontext.pcontext) ;

   // TEST progname_maincontext
   TEST(&progname_maincontext()    == &g_maincontext.progname) ;

   // TEST self_maincontext
   TEST(self_maincontext()         == &g_maincontext) ;

   // TEST sizestatic_maincontext
   TEST(&sizestatic_maincontext()  == &g_maincontext.size_staticmem) ;

   // TEST sysuser_maincontext
   TEST(&sysuser_maincontext()     == &pcontext_maincontext()->sysuser) ;

   // TEST syncrun_maincontext
   TEST(&syncrun_maincontext()     == &tcontext_maincontext()->syncrun) ;

   thread_tls_t tls ;
   tls = current_threadtls(&tls) ;

   // TEST tcontext_maincontext
   TEST(tcontext_maincontext()     == context_threadtls(&tls)) ;

   // TEST threadid_maincontext
   TEST(&threadid_maincontext()    == &context_threadtls(&tls)->thread_id) ;

   // TEST type_maincontext
   TEST(&type_maincontext()        == &g_maincontext.type) ;

   // TEST valuecache_maincontext
   TEST(&valuecache_maincontext()  == &pcontext_maincontext()->valuecache) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initmain(void)
{
   int fd_stderr = -1 ;
   int fdpipe[2] = { -1, -1 } ;

   // prepare
   if (maincontext_STATIC != g_maincontext.type) return EINVAL ;
   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;
   FLUSHBUFFER_ERRLOG() ;

   // TEST static type
   TEST(1 == isstatic_processcontext(&g_maincontext.pcontext)) ;
   TEST(0 == g_maincontext.type) ;
   TEST(0 == g_maincontext.progname) ;
   TEST(0 == g_maincontext.argc) ;
   TEST(0 == g_maincontext.argv) ;
   TEST(0 == g_maincontext.size_staticmem) ;
   TEST(1 == isstatic_threadcontext(tcontext_maincontext())) ;

   // TEST EINVAL
   TEST(0      == maincontext_STATIC) ;
   TEST(EINVAL == init_maincontext(maincontext_STATIC, 0, 0)) ;
   TEST(EINVAL == init_maincontext(3, 0, 0)) ;
   TEST(EINVAL == init_maincontext(maincontext_DEFAULT, -1, 0)) ;
   TEST(EINVAL == init_maincontext(maincontext_DEFAULT, 1, 0)) ;
   TEST(1 == isstatic_processcontext(&g_maincontext.pcontext)) ;
   TEST(0 == g_maincontext.type) ;
   TEST(0 == g_maincontext.progname) ;
   TEST(0 == g_maincontext.argc) ;
   TEST(0 == g_maincontext.argv) ;
   TEST(0 == g_maincontext.size_staticmem) ;
   TEST(1 == isstatic_threadcontext(tcontext_maincontext())) ;

   maincontext_e mainmode[] = { maincontext_DEFAULT, maincontext_CONSOLE } ;
   for (unsigned i = 0 ; i < lengthof(mainmode); ++i) {
      // TEST init_maincontext: maincontext_DEFAULT, maincontext_CONSOLE
      const char * argv[2] = { "1", "2" } ;
      TEST(0 == init_maincontext(mainmode[i], 2, argv)) ;
      TEST(pcontext_maincontext() == &g_maincontext.pcontext) ;
      TEST(g_maincontext.type == mainmode[i]) ;
      TEST(g_maincontext.argc == 2) ;
      TEST(g_maincontext.argv == argv) ;
      TEST(g_maincontext.progname == argv[0]) ;
      TEST(0 != g_maincontext.pcontext.valuecache) ;
      TEST(0 != g_maincontext.pcontext.sysuser) ;
      TEST(0 != tcontext_maincontext()->initcount) ;
      TEST(0 != tcontext_maincontext()->log.object) ;
      TEST(0 != tcontext_maincontext()->log.iimpl) ;
      TEST(0 != tcontext_maincontext()->objectcache.object) ;
      TEST(0 != tcontext_maincontext()->objectcache.iimpl) ;
      TEST(0 != strcmp("C", current_locale())) ;
      TEST(tcontext_maincontext()->log.object != 0) ;
      TEST(tcontext_maincontext()->log.iimpl  != &g_logmain_interface) ;
      switch (mainmode[i]) {
      case maincontext_STATIC:
         break ;
      case maincontext_DEFAULT:
         TEST(log_state_IGNORED  == GETSTATE_LOG(log_channel_USERERR)) ;
         TEST(log_state_BUFFERED == GETSTATE_LOG(log_channel_ERR)) ;
         break ;
      case maincontext_CONSOLE:
         TEST(log_state_UNBUFFERED == GETSTATE_LOG(log_channel_USERERR)) ;
         TEST(log_state_IGNORED    == GETSTATE_LOG(log_channel_ERR)) ;
         break ;
      }

      // TEST free_maincontext
      TEST(0 == free_maincontext()) ;
      TEST(pcontext_maincontext() == &g_maincontext.pcontext) ;
      TEST(1 == isstatic_processcontext(&g_maincontext.pcontext)) ;
      TEST(0 == g_maincontext.type) ;
      TEST(0 == g_maincontext.progname) ;
      TEST(0 == g_maincontext.argc) ;
      TEST(0 == g_maincontext.argv) ;
      TEST(0 == g_maincontext.size_staticmem) ;
      TEST(1 == isstatic_threadcontext(tcontext_maincontext())) ;
      TEST(0 == strcmp("C", current_locale())) ;
      TEST(0 == free_maincontext()) ;
      TEST(pcontext_maincontext() == &g_maincontext.pcontext) ;
      TEST(1 == isstatic_processcontext(&g_maincontext.pcontext)) ;
      TEST(0 == g_maincontext.type) ;
      TEST(0 == g_maincontext.progname) ;
      TEST(0 == g_maincontext.argc) ;
      TEST(0 == g_maincontext.argv) ;
      TEST(0 == g_maincontext.size_staticmem) ;
      TEST(1 == isstatic_threadcontext(tcontext_maincontext())) ;
      TEST(0 == strcmp("C", current_locale())) ;
   }

   // TEST free_maincontext: ENOTEMPTY
   g_maincontext.type = maincontext_DEFAULT ;
   g_maincontext.size_staticmem = 1 ;
   TEST(ENOTEMPTY == free_maincontext()) ;
   TEST(1 == isstatic_processcontext(&g_maincontext.pcontext)) ;
   TEST(0 == g_maincontext.type) ;
   TEST(0 == g_maincontext.progname) ;
   TEST(0 == g_maincontext.argc) ;
   TEST(0 == g_maincontext.argv) ;
   TEST(0 == g_maincontext.size_staticmem) ;
   TEST(1 == isstatic_threadcontext(tcontext_maincontext())) ;

   // unprepare
   FLUSHBUFFER_ERRLOG() ;
   char buffer[4096] = { 0 };
   TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&fd_stderr)) ;
   TEST(0 == free_iochannel(&fdpipe[0])) ;
   TEST(0 == free_iochannel(&fdpipe[1])) ;

   return 0 ;
ONABORT:
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_iochannel(&fd_stderr) ;
   free_iochannel(&fdpipe[0]);
   free_iochannel(&fdpipe[1]);
   return EINVAL ;
}

static int test_initerror(void)
{
   int fd_stderr = -1 ;
   int fdpipe[2] = { -1, -1 } ;

   // prepare
   if (maincontext_STATIC != g_maincontext.type) return EINVAL ;
   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;
   FLUSHBUFFER_ERRLOG() ;

   // TEST init_maincontext: error in in different places
   for (int i = 1; i <= 3; ++i) {
      init_testerrortimer(&s_maincontext_errtimer, (unsigned)i, EINVAL+i) ;
      TEST(EINVAL+i == init_maincontext(maincontext_DEFAULT, 0, 0)) ;
      TEST(0 == pcontext_maincontext()->initcount) ;
      TEST(maincontext_STATIC == type_maincontext()) ;
      TEST(0 == tcontext_maincontext()->initcount) ;
      TEST(tcontext_maincontext()->log.object == 0) ;
      TEST(tcontext_maincontext()->log.iimpl  == &g_logmain_interface) ;
      TEST(0 == tcontext_maincontext()->objectcache.object) ;
      TEST(0 == tcontext_maincontext()->objectcache.iimpl) ;
   }

   FLUSHBUFFER_ERRLOG() ;
   char buffer[4096] = { 0 };
   TEST(0 < read(fdpipe[0], buffer, sizeof(buffer))) ;

   TEST(0 == init_maincontext(maincontext_DEFAULT, 0, 0)) ;
   TEST(0 != pcontext_maincontext()->initcount) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&fd_stderr)) ;
   TEST(0 == free_iochannel(&fdpipe[0])) ;
   TEST(0 == free_iochannel(&fdpipe[1])) ;

   PRINTF_ERRLOG("%s", buffer) ;

   // TEST EALREADY
   TEST(EALREADY == init_maincontext(maincontext_DEFAULT, 0, 0)) ;
   CLEARBUFFER_ERRLOG() ;
   TEST(0 == free_maincontext()) ;

   return 0 ;
ONABORT:
   CLEARBUFFER_ERRLOG() ;
   free_maincontext() ;
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_iochannel(&fd_stderr) ;
   free_iochannel(&fdpipe[0]);
   free_iochannel(&fdpipe[1]);
   return EINVAL ;
}

maincontext_startparam_t s_startparam;

static int test_initstart_checkparam1(maincontext_t * maincontext)
{
   if (s_startparam.main_thread != &test_initstart_checkparam1) return EINVAL;
   s_startparam.main_thread = 0;

   if (maincontext != &g_maincontext) return EINVAL;
   if (maincontext->type != s_startparam.context_type) return EINVAL;
   if (maincontext->argc != s_startparam.argc) return EINVAL;
   if (maincontext->argv != s_startparam.argv) return EINVAL;
   if (0 != strcmp(maincontext->argv[0], "1")) return EINVAL;
   if (0 != strcmp(maincontext->argv[1], "2")) return EINVAL;

   return 0;
}

static int test_initstart(void)
{
   const char* argv[2] = { "1", "2" };

   // prepare
   if (maincontext_STATIC != g_maincontext.type) return EINVAL ;

   // TEST initstart_maincontext
   maincontext_e mainmode[] = { maincontext_DEFAULT, maincontext_CONSOLE } ;
   for (int i = 0 ; i < (int)lengthof(mainmode); ++i) {
      // test correct function called
      s_startparam = (maincontext_startparam_t) { mainmode[i], 1+i, argv, &test_initstart_checkparam1 };
      TEST(0 == initstart_maincontext(&s_startparam));
      TEST(0 == s_startparam.main_thread);
      TEST(1+i == s_startparam.argc);
      TEST(argv == s_startparam.argv);

      // test free_maincontext called
      TEST(pcontext_maincontext() == &g_maincontext.pcontext) ;
      TEST(1 == isstatic_processcontext(&g_maincontext.pcontext)) ;
      TEST(1 == isstatic_threadcontext(tcontext_maincontext())) ;
      TEST(0 == g_maincontext.type) ;
      TEST(0 == g_maincontext.progname) ;
      TEST(0 == g_maincontext.argc) ;
      TEST(0 == g_maincontext.argv) ;
      TEST(0 == g_maincontext.size_staticmem) ;
   }

   // unprepare

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_progname(void)
{
   int fd_stderr = -1 ;
   int fdpipe[2] = { -1, -1 } ;

   // prepare
   if (maincontext_STATIC != g_maincontext.type) return EINVAL ;
   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC|O_NONBLOCK)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;
   FLUSHBUFFER_ERRLOG() ;

    // TEST progname_maincontext
   const char * argv[4] = { "/p1/yxz1", "/p2/yxz2/", "p3/p4/yxz3", "123456789a1234567" } ;

   for (unsigned i = 0; i < lengthof(argv); ++i) {
      TEST(0 == init_maincontext(maincontext_DEFAULT, 1, &argv[i])) ;
      TEST(1 == g_maincontext.argc) ;
      TEST(&argv[i] == g_maincontext.argv) ;
      switch(i) {
      case 0:  TEST(&argv[0][4] == progname_maincontext()) ; break ;
      case 1:  TEST(&argv[1][9] == progname_maincontext()) ; break ;
      case 2:  TEST(&argv[2][6] == progname_maincontext()) ; break ;
      // only up to 16 characters
      case 3:  TEST(&argv[3][1] == progname_maincontext()) ; break ;
      }
      TEST(0 == free_maincontext()) ;
   }

   // unprepare
   FLUSHBUFFER_ERRLOG() ;
   char buffer[4096] = { 0 };
   ssize_t bytes = read(fdpipe[0], buffer, sizeof(buffer)) ;
   TEST(0 < bytes || (errno == EAGAIN && -1 == bytes)) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&fd_stderr)) ;
   TEST(0 == free_iochannel(&fdpipe[0])) ;
   TEST(0 == free_iochannel(&fdpipe[1])) ;

   return 0 ;
ONABORT:
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_iochannel(&fd_stderr) ;
   free_iochannel(&fdpipe[0]);
   free_iochannel(&fdpipe[1]);
   return EINVAL ;
}

static int test_staticmem(void)
{
   const uint16_t oldsize = g_maincontext.size_staticmem ;

   // TEST allocstatic_maincontext: all block sizes
   for (unsigned i = 0; i <= sizeof(s_maincontext_staticmem); ++i) {
      uint8_t i8 = (uint8_t)i ;
      g_maincontext.size_staticmem = 0 ;
      TEST(allocstatic_maincontext(i8)  == s_maincontext_staticmem) ;
      TEST(g_maincontext.size_staticmem == i) ;
   }

   // TEST freestatic_maincontext: all block sizes
   for (unsigned i = 0; i <= sizeof(s_maincontext_staticmem); ++i) {
      uint8_t i8 = (uint8_t)i ;
      g_maincontext.size_staticmem = 255 ;
      TEST(freestatic_maincontext(i8)   == 0) ;
      TEST(g_maincontext.size_staticmem == 255-i) ;
   }

   // TEST allocstatic_maincontext: all bytes
   g_maincontext.size_staticmem = 0 ;
   for (unsigned i = 0; i < sizeof(s_maincontext_staticmem); ++i) {
      TEST(allocstatic_maincontext(1)   == s_maincontext_staticmem+i) ;
      TEST(g_maincontext.size_staticmem == 1+i) ;
   }

   // TEST allocstatic_maincontext: ENOMEM
   TEST(allocstatic_maincontext(0) == s_maincontext_staticmem+sizeof(s_maincontext_staticmem)) ;
   TEST(allocstatic_maincontext(1) == 0) ;

   // TEST freestatic_maincontext: all bytes
   for (unsigned i = sizeof(s_maincontext_staticmem); i > 0; --i) {
      TEST(freestatic_maincontext(1)    == 0) ;
      TEST(g_maincontext.size_staticmem == i-1) ;
   }

   // TEST freestatic_maincontext: EINVAL
   TEST(freestatic_maincontext(1)    == EINVAL) ;
   TEST(g_maincontext.size_staticmem == 0) ;

   // unprepare
   g_maincontext.size_staticmem = oldsize ;

   return 0 ;
ONABORT:
   g_maincontext.size_staticmem = oldsize ;
   return EINVAL ;
}

int unittest_context_maincontext()
{

   if (maincontext_STATIC == type_maincontext()) {

      if (test_querymacros())    goto ONABORT ;
      if (test_initmain())       goto ONABORT ;
      if (test_initerror())      goto ONABORT ;
      if (test_initstart())      goto ONABORT ;
      if (test_progname())       goto ONABORT ;

   } else {

      if (test_querymacros())    goto ONABORT ;
      if (test_staticmem())      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
