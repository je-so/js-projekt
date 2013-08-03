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
#include "C-kern/api/context/errorcontext.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/platform/sysuser.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/test/testmm.h"
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
#define maincontext_STATICSIZE         processcontext_STATICSIZE

#ifdef KONFIG_UNITTEST
/* define: maincontext_STATICTESTSIZE
 * Defines additonal size for <s_maincontext_staticmem>.
 * This extrasize is needed during unit test time. */
#define maincontext_STATICTESTSIZE     (maincontext_STATICSIZE)
#else
#define maincontext_STATICTESTSIZE     0
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
static test_errortimer_t   s_maincontext_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: helper

static inline void compiletime_assert(void)
{
   static_assert(&((maincontext_t*)0)->pcontext == (processcontext_t*)0, "extends from processcontext_t") ;
}

/* function: initprogname_maincontext
 * Sets <maincontext_t.progname> to (strrchr(argv0, "/")+1). */
static void initprogname_maincontext(struct maincontext_t * maincontext, const char * argv0)
{
   if (argv0) {
      size_t offset = 0 ;
      size_t len    = strlen(argv0) ;

      if (len > 16) {
         offset = len - 16 ;
      }

      for (size_t i = offset; argv0[i]; ++i) {
         if (  '/' == argv0[i]
               && 0 != argv0[i+1]) {
            offset = i + 1 ;
         }
      }

      maincontext->progname = argv0 + offset ;

   } else {
      maincontext->progname = "???" ;
   }
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

int init_maincontext(maincontext_e context_type, int argc, const char ** argv)
{
   int err ;
   int is_already_initialized = (maincontext_STATIC != g_maincontext.type) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ONABORT ;
   }

   VALIDATE_INPARAM_TEST(  maincontext_STATIC  <  context_type
                           && maincontext_DEFAULT >= context_type, ONABORT, ) ;

   VALIDATE_INPARAM_TEST(argc >= 0 && (argc == 0 || argv != 0), ONABORT, ) ;

   ONERROR_testerrortimer(&s_maincontext_errtimer, ONABORT) ;

   err = init_processcontext(&g_maincontext.pcontext) ;
   if (err) goto ONABORT ;

   g_maincontext.type     = context_type ;
   g_maincontext.progname = 0 ;
   g_maincontext.argc     = argc ;
   g_maincontext.argv     = argv ;

   initprogname_maincontext(&g_maincontext, argv ? argv[0] : 0) ;

   ONERROR_testerrortimer(&s_maincontext_errtimer, ONABORT) ;

   err = init_threadcontext(tcontext_maincontext(), &g_maincontext.pcontext) ;
   if (err) goto ONABORT ;

   ONERROR_testerrortimer(&s_maincontext_errtimer, ONABORT) ;

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

static int test_initmain(void)
{
   int               fd_stderr   = -1 ;
   int               fdpipe[2]   = { -1, -1 } ;
   maincontext_t     old_context = g_maincontext ;

   // prepare
   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;
   FLUSHBUFFER_ERRLOG() ;
   TEST(0 == free_maincontext()) ;

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

   // TEST init_maincontext: maincontext_DEFAULT
   const char * argv[2] = { "1", "2" } ;
   TEST(0 == init_maincontext(maincontext_DEFAULT, 2, argv)) ;
   TEST(pcontext_maincontext() == &g_maincontext.pcontext) ;
   TEST(1 == g_maincontext.type) ;
   TEST(2 == g_maincontext.argc) ;
   TEST(argv    == g_maincontext.argv) ;
   TEST(argv[0] == g_maincontext.progname) ;
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

   if (maincontext_STATIC != old_context.type) {
      init_maincontext(old_context.type, old_context.argc, old_context.argv) ;
      PRINTF_ERRLOG("%s", buffer) ;
   }

   return 0 ;
ONABORT:
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_iochannel(&fd_stderr) ;
   free_iochannel(&fdpipe[0]);
   free_iochannel(&fdpipe[1]);
   return EINVAL ;
}

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

static int test_initerror(void)
{
   int               fd_stderr   = -1 ;
   int               fdpipe[2]   = { -1, -1 } ;
   maincontext_t     old_context = g_maincontext ;

   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;
   FLUSHBUFFER_ERRLOG() ;
   TEST(0 == free_maincontext()) ;
   TEST(maincontext_STATIC == type_maincontext()) ;

   // TEST EPROTO (call init_maincontext first)
   threadcontext_t tcontext ;
   TEST(EPROTO == init_threadcontext(&tcontext, &g_maincontext.pcontext)) ;

   // TEST error in init_maincontext in different places (called from initmain)
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

   TEST(0 == init_maincontext(old_context.type ? old_context.type : maincontext_DEFAULT, old_context.argc, old_context.argv )) ;
   TEST(0 != pcontext_maincontext()->initcount) ;

   TEST(STDERR_FILENO == dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&fd_stderr)) ;
   TEST(0 == free_iochannel(&fdpipe[0])) ;
   TEST(0 == free_iochannel(&fdpipe[1])) ;

   PRINTF_ERRLOG("%s", buffer) ;

   // TEST EALREADY
   TEST(EALREADY == init_maincontext(maincontext_DEFAULT, 0, 0)) ;

   if (maincontext_STATIC == old_context.type) {
      CLEARBUFFER_ERRLOG() ;
      free_maincontext() ;
   }

   return 0 ;
ONABORT:
   if (maincontext_STATIC == old_context.type) {
      CLEARBUFFER_ERRLOG() ;
      free_maincontext() ;
   }
   if (0 < fd_stderr) dup2(fd_stderr, STDERR_FILENO) ;
   free_iochannel(&fd_stderr) ;
   free_iochannel(&fdpipe[0]);
   free_iochannel(&fdpipe[1]);
   return EINVAL ;
}

static int test_progname(void)
{
   int               fd_stderr   = -1 ;
   int               fdpipe[2]   = { -1, -1 } ;
   maincontext_t     old_context = g_maincontext ;

   // prepare
   fd_stderr = dup(STDERR_FILENO) ;
   TEST(0 < fd_stderr) ;
   TEST(0 == pipe2(fdpipe,O_CLOEXEC|O_NONBLOCK)) ;
   TEST(STDERR_FILENO == dup2(fdpipe[1], STDERR_FILENO)) ;
   FLUSHBUFFER_ERRLOG() ;
   TEST(0 == free_maincontext()) ;

    // TEST progname_maincontext
   const char * argv[4] = { "/p1/yxz1", "/p2/yxz2/", "p3/p4/yxz3", "123456789a1234567" } ;

   for (unsigned i = 0; i< lengthof(argv); ++i) {
      TEST(0 == init_maincontext(maincontext_DEFAULT, 1, &argv[i])) ;
      TEST(1 == g_maincontext.argc) ;
      TEST(&argv[i] == g_maincontext.argv) ;
      switch(i) {
      case 0:  TEST(&argv[0][4] == progname_maincontext()) ; break ;
      case 1:  TEST(&argv[1][4] == progname_maincontext()) ; break ;
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

   if (maincontext_STATIC != old_context.type) {
      init_maincontext(old_context.type, old_context.argc, old_context.argv) ;
      PRINTF_ERRLOG("%s", buffer) ;
   }

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
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (maincontext_STATIC == type_maincontext()) {

      if (test_querymacros())    goto ONABORT ;
      if (test_initmain())       goto ONABORT ;
      if (test_initerror())      goto ONABORT ;
      if (test_progname())       goto ONABORT ;

   } else {

      const bool istestmm = isinstalled_testmm() ;

      if (istestmm) {
         switchoff_testmm() ;
      }

      if (test_initerror())  goto ONABORT ;

      TEST(0 == init_resourceusage(&usage)) ;

      if (test_querymacros())    goto ONABORT ;
      if (test_initmain())       goto ONABORT ;
      if (test_initerror())      goto ONABORT ;
      if (test_progname())       goto ONABORT ;
      if (test_staticmem())      goto ONABORT ;

      TEST(0 == same_resourceusage(&usage)) ;
      TEST(0 == free_resourceusage(&usage)) ;

      if (istestmm) {
         switchon_testmm() ;
      }
   }

   return 0 ;
ONABORT:
   TEST(0 == free_resourceusage(&usage)) ;
   return EINVAL ;
}

#endif
