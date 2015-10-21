/* title: MainContext impl
   Implementation file of global init anf free functions.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/maincontext.h
    Header file of <MainContext>.

   file: C-kern/main/maincontext.c
    Implementation file <MainContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/maincontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/platform/init.h"
#include "C-kern/api/platform/syslogin.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/task/thread_localstore.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/memory/mm/mm_impl.h"
#include "C-kern/api/test/mm/testmm.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: maincontext_t

// group: global variables

/* variable: g_maincontext
 * Reserve space for the global main context. */
maincontext_t g_maincontext = {
                  processcontext_INIT_STATIC,
                  maincontext_STATIC,
                  0,
                  0,
                  0
               };

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_maincontext_errtimer
 * Simulates an error in <maincontext_t.init_maincontext>. */
static test_errortimer_t   s_maincontext_errtimer = test_errortimer_FREE;
#endif

// group: helper

static inline void compiletime_assert(void)
{
   static_assert(&((maincontext_t*)0)->pcontext == (processcontext_t*)0, "extends from processcontext_t");
}

/* function: initprogname_maincontext
 * Sets *progname to (strrchr(argv0, "/")+1). */
static void initprogname_maincontext(const char ** progname, const char * argv0)
{
   const char * last = strrchr(argv0, '/');

   if (last)
      ++ last;
   else
      last = argv0;

   const size_t len = strlen(last);
   if (len > 16) {
      *progname = last + len - 16;
   } else {
      *progname = last;
   }
}

static int run_maincontext(void * user)
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
   int err;
   int err2;
   int is_initialized = (maincontext_STATIC != g_maincontext.type);

   if (is_initialized) {
      err = free_threadcontext(tcontext_maincontext());

      err2 = free_processcontext(&g_maincontext.pcontext);
      if (err2) err = err2;

      if (0 != sizestatic_threadlocalstore(self_threadlocalstore())) {
         err = ENOTEMPTY;
      }

      g_maincontext.type     = maincontext_STATIC;
      g_maincontext.progname = 0;
      g_maincontext.argc     = 0;
      g_maincontext.argv     = 0;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int initstart_maincontext(const maincontext_startparam_t * startparam)
{
   int err;
   int is_already_initialized = (maincontext_STATIC != g_maincontext.type);

   if (is_already_initialized) {
      err = EALREADY;
      goto ONERR;
   }

   err = init_platform(&run_maincontext, CONST_CAST(maincontext_startparam_t,startparam));

ONERR:
   return err;
}

int init_maincontext(const maincontext_e context_type, int argc, const char ** argv)
{
   int err;
   int is_already_initialized = (maincontext_STATIC != g_maincontext.type);

   if (is_already_initialized) {
      err = EALREADY;
      goto ONERR;
   }

   VALIDATE_INPARAM_TEST( maincontext_STATIC  <  context_type
                          && maincontext_CONSOLE >= context_type, ONERR, );

   VALIDATE_INPARAM_TEST( argc >= 0 && (argc == 0 || argv != 0), ONERR, );

   // init_platform has been called !!
   VALIDATE_INPARAM_TEST( self_maincontext() == &g_maincontext, ONERR, );

   ONERROR_testerrortimer(&s_maincontext_errtimer, &err, ONERR);

   g_maincontext.type     = context_type;
   g_maincontext.progname = "";
   g_maincontext.argc     = argc;
   g_maincontext.argv     = argv;

   if (argc) {
      initprogname_maincontext(&g_maincontext.progname, argv[0]);
   }

   err = init_processcontext(&g_maincontext.pcontext);
   if (err) goto ONERR;


   ONERROR_testerrortimer(&s_maincontext_errtimer, &err, ONERR);

   err = init_threadcontext(tcontext_maincontext(), &g_maincontext.pcontext, context_type);
   if (err) goto ONERR;

   ONERROR_testerrortimer(&s_maincontext_errtimer, &err, ONERR);

   return 0;
ONERR:
   if (!is_already_initialized) {
      free_maincontext();
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

void abort_maincontext(int err)
{
   // TODO: add abort handler registration ...
   //       add unit test for checking that resources are freed
   TRACE_ERRLOG(log_flags_NONE, PROGRAM_ABORT, err);
   FLUSHBUFFER_ERRLOG();
   abort();
}

void assertfail_maincontext(
   const char * condition,
   const char * file,
   int          line,
   const char * funcname)
{
   TRACE2_ERRLOG(log_flags_LAST, ASSERT_FAILED, funcname, file, line, condition);
   abort_maincontext(EINVAL);
}

// group: test

#ifdef KONFIG_UNITTEST

static int test_querymacros(void)
{
   // TEST blockmap_maincontext
   TEST(&blockmap_maincontext()    == &pcontext_maincontext()->blockmap);

   // TEST error_maincontext
   TEST(&error_maincontext()       == &pcontext_maincontext()->error);

   // TEST log_maincontext
   TEST(&log_maincontext()         == &tcontext_maincontext()->log);

   // TEST log_maincontext
   TEST(&objectcache_maincontext() == &tcontext_maincontext()->objectcache);

   // TEST pagecache_maincontext
   TEST(&pagecache_maincontext()   == &tcontext_maincontext()->pagecache);

   // TEST pcontext_maincontext
   TEST(pcontext_maincontext()     == &g_maincontext.pcontext);

   // TEST progname_maincontext
   TEST(&progname_maincontext()    == &g_maincontext.progname);

   // TEST self_maincontext
   TEST(self_maincontext()         == &g_maincontext);

   // TEST syslogin_maincontext
   TEST(&syslogin_maincontext()    == &pcontext_maincontext()->syslogin);

   // TEST syncrunner_maincontext
   TEST(&syncrunner_maincontext()  == &tcontext_maincontext()->syncrunner);

   thread_localstore_t* tls = self_threadlocalstore();

   // TEST tcontext_maincontext
   TEST(tcontext_maincontext()     == context_threadlocalstore(tls));

   // TEST threadid_maincontext
   TEST(&threadid_maincontext()    == &context_threadlocalstore(tls)->thread_id);

   // TEST type_maincontext
   TEST(&type_maincontext()        == &g_maincontext.type);

   // TEST valuecache_maincontext
   TEST(&valuecache_maincontext()  == &pcontext_maincontext()->valuecache);

   return 0;
ONERR:
   return EINVAL;
}

static int test_initmain(void)
{
   // prepare
   if (maincontext_STATIC != g_maincontext.type) return EINVAL;

   // TEST static type
   TEST(1 == isstatic_processcontext(&g_maincontext.pcontext));
   TEST(0 == g_maincontext.type);
   TEST(0 == g_maincontext.progname);
   TEST(0 == g_maincontext.argc);
   TEST(0 == g_maincontext.argv);
   TEST(1 == isstatic_threadcontext(tcontext_maincontext()));

   // TEST EINVAL
   TEST(0      == maincontext_STATIC);
   TEST(EINVAL == init_maincontext(maincontext_STATIC, 0, 0));
   TEST(EINVAL == init_maincontext(3, 0, 0));
   TEST(EINVAL == init_maincontext(maincontext_DEFAULT, -1, 0));
   TEST(EINVAL == init_maincontext(maincontext_DEFAULT, 1, 0));
   TEST(1 == isstatic_processcontext(&g_maincontext.pcontext));
   TEST(0 == g_maincontext.type);
   TEST(0 == g_maincontext.progname);
   TEST(0 == g_maincontext.argc);
   TEST(0 == g_maincontext.argv);
   TEST(1 == isstatic_threadcontext(tcontext_maincontext()));
   TEST(0 == sizestatic_threadlocalstore(self_threadlocalstore()));

   maincontext_e mainmode[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   for (unsigned i = 0; i < lengthof(mainmode); ++i) {
      // TEST init_maincontext: maincontext_DEFAULT, maincontext_CONSOLE
      const char * argv[2] = { "1", "2" };
      TEST(0 == init_maincontext(mainmode[i], 2, argv));
      TEST(pcontext_maincontext() == &g_maincontext.pcontext);
      TEST(g_maincontext.type == mainmode[i]);
      TEST(g_maincontext.argc == 2);
      TEST(g_maincontext.argv == argv);
      TEST(g_maincontext.progname == argv[0]);
      TEST(sizestatic_threadlocalstore(self_threadlocalstore()) == extsize_processcontext() + extsize_threadcontext());
      TEST(0 != g_maincontext.pcontext.valuecache);
      TEST(0 != g_maincontext.pcontext.syslogin);
      TEST(0 != tcontext_maincontext()->initcount);
      TEST(0 != tcontext_maincontext()->log.object);
      TEST(0 != tcontext_maincontext()->log.iimpl);
      TEST(0 != tcontext_maincontext()->objectcache.object);
      TEST(0 != tcontext_maincontext()->objectcache.iimpl);
      TEST(0 != strcmp("C", current_locale()));
      TEST(tcontext_maincontext()->log.object != 0);
      TEST(tcontext_maincontext()->log.iimpl  != &g_logmain_interface);
      switch (mainmode[i]) {
      case maincontext_STATIC:
         break;
      case maincontext_DEFAULT:
         TEST(log_state_IGNORED  == GETSTATE_LOG(log_channel_USERERR));
         TEST(log_state_BUFFERED == GETSTATE_LOG(log_channel_ERR));
         break;
      case maincontext_CONSOLE:
         TEST(log_state_UNBUFFERED == GETSTATE_LOG(log_channel_USERERR));
         TEST(log_state_IGNORED    == GETSTATE_LOG(log_channel_ERR));
         break;
      }

      // TEST free_maincontext
      TEST(0 == free_maincontext());
      TEST(pcontext_maincontext() == &g_maincontext.pcontext);
      TEST(0 == sizestatic_threadlocalstore(self_threadlocalstore()));
      TEST(1 == isstatic_processcontext(&g_maincontext.pcontext));
      TEST(0 == g_maincontext.type);
      TEST(0 == g_maincontext.progname);
      TEST(0 == g_maincontext.argc);
      TEST(0 == g_maincontext.argv);
      TEST(1 == isstatic_threadcontext(tcontext_maincontext()));
      TEST(0 == strcmp("C", current_locale()));
      TEST(0 == free_maincontext());
      TEST(0 == sizestatic_threadlocalstore(self_threadlocalstore()));
      TEST(pcontext_maincontext() == &g_maincontext.pcontext);
      TEST(1 == isstatic_processcontext(&g_maincontext.pcontext));
      TEST(0 == g_maincontext.type);
      TEST(0 == g_maincontext.progname);
      TEST(0 == g_maincontext.argc);
      TEST(0 == g_maincontext.argv);
      TEST(1 == isstatic_threadcontext(tcontext_maincontext()));
      TEST(0 == strcmp("C", current_locale()));
   }

   // TEST free_maincontext: ENOTEMPTY
   g_maincontext.type = maincontext_DEFAULT;
   memblock_t mblock;
   TEST(0 == allocstatic_threadlocalstore(self_threadlocalstore(), 1, &mblock));
   TEST(ENOTEMPTY == free_maincontext());
   TEST(1 == isstatic_processcontext(&g_maincontext.pcontext));
   TEST(0 == g_maincontext.type);
   TEST(0 == g_maincontext.progname);
   TEST(0 == g_maincontext.argc);
   TEST(0 == g_maincontext.argv);
   TEST(1 == isstatic_threadcontext(tcontext_maincontext()));
   TEST(0 == freestatic_threadlocalstore(self_threadlocalstore(), &mblock));

   // unprepare
   FLUSHBUFFER_ERRLOG();

   return 0;
ONERR:
   return EINVAL;
}

static int test_initerror(void)
{
   // prepare
   if (maincontext_STATIC != g_maincontext.type) return EINVAL;

   // TEST init_maincontext: error in in different places
   for (int i = 1; i <= 3; ++i) {
      init_testerrortimer(&s_maincontext_errtimer, (unsigned)i, EINVAL+i);
      TEST(EINVAL+i == init_maincontext(maincontext_DEFAULT, 0, 0));
      TEST(0 == pcontext_maincontext()->initcount);
      TEST(maincontext_STATIC == type_maincontext());
      TEST(0 == tcontext_maincontext()->initcount);
      TEST(tcontext_maincontext()->log.object == 0);
      TEST(tcontext_maincontext()->log.iimpl  == &g_logmain_interface);
      TEST(0 == tcontext_maincontext()->objectcache.object);
      TEST(0 == tcontext_maincontext()->objectcache.iimpl);
   }

   TEST(0 == init_maincontext(maincontext_DEFAULT, 0, 0));
   TEST(0 != pcontext_maincontext()->initcount);

   // TEST EALREADY
   TEST(EALREADY == init_maincontext(maincontext_DEFAULT, 0, 0));
   FLUSHBUFFER_ERRLOG();

   TEST(0 == free_maincontext());

   return 0;
ONERR:
   CLEARBUFFER_ERRLOG();
   free_maincontext();
   return EINVAL;
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
   if (maincontext_STATIC != g_maincontext.type) return EINVAL;

   // TEST initstart_maincontext
   maincontext_e mainmode[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   for (int i = 0; i < (int)lengthof(mainmode); ++i) {
      // test correct function called
      s_startparam = (maincontext_startparam_t) { mainmode[i], 1+i, argv, &test_initstart_checkparam1 };
      TEST(0 == initstart_maincontext(&s_startparam));
      TEST(0 == s_startparam.main_thread);
      TEST(1+i == s_startparam.argc);
      TEST(argv == s_startparam.argv);

      // test free_maincontext called
      TEST(pcontext_maincontext() == &g_maincontext.pcontext);
      TEST(1 == isstatic_processcontext(&g_maincontext.pcontext));
      TEST(1 == isstatic_threadcontext(tcontext_maincontext()));
      TEST(0 == g_maincontext.type);
      TEST(0 == g_maincontext.progname);
      TEST(0 == g_maincontext.argc);
      TEST(0 == g_maincontext.argv);
   }

   // unprepare

   return 0;
ONERR:
   return EINVAL;
}

static int test_progname(void)
{

   // prepare
   if (maincontext_STATIC != g_maincontext.type) return EINVAL;

   // TEST progname_maincontext
   const char * argv[4] = { "/p1/yxz1", "/p2/yxz2/", "p3/p4/yxz3", "123456789a1234567" };

   for (unsigned i = 0; i < lengthof(argv); ++i) {
      TEST(0 == init_maincontext(maincontext_DEFAULT, 1, &argv[i]));
      TEST(1 == g_maincontext.argc);
      TEST(&argv[i] == g_maincontext.argv);
      switch(i) {
      case 0:  TEST(&argv[0][4] == progname_maincontext()); break;
      case 1:  TEST(&argv[1][9] == progname_maincontext()); break;
      case 2:  TEST(&argv[2][6] == progname_maincontext()); break;
      // only up to 16 characters
      case 3:  TEST(&argv[3][1] == progname_maincontext()); break;
      }
      TEST(0 == free_maincontext());
   }

   // unprepare

   return 0;
ONERR:
   return EINVAL;
}

static uint8_t s_static_buffer[20000];
static size_t  s_static_offset = 0;

static int static_malloc(struct mm_t * mman, size_t size, /*out*/struct memblock_t * memblock)
{
   (void) mman;
   size_t aligned_size = (size + 63) & ~(size_t)63;

   if (size > aligned_size || aligned_size > sizeof(s_static_buffer) - s_static_offset) {
      // printf("static_malloc ENOMEM\n");
      return ENOMEM;
   }

   *memblock = (memblock_t) memblock_INIT(aligned_size, s_static_buffer + s_static_offset);
   s_static_offset += aligned_size;

   return 0;
}

static int static_mresize(struct mm_t * mman, size_t newsize, struct memblock_t * memblock)
{
   if (memblock->addr != 0 || memblock->size != 0) return EINVAL;
   return static_malloc(mman, newsize, memblock);
}

static int static_mfree(struct mm_t * mman, struct memblock_t * memblock)
{
   (void) mman;
   *memblock = (memblock_t) memblock_FREE;
   return 0;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_FREE;
   maincontext_e   type = g_maincontext.type;
   int             argc = g_maincontext.argc;
   const char **   argv = g_maincontext.argv;
   threadcontext_mm_t oldmm = iobj_FREE;
   mm_it           staticmm = mm_it_FREE;
   bool            isStatic = true;
   pipe_t          errpipe  = pipe_FREE;
   int             oldstderr = -1;

   // prepare
   TEST(0 == init_pipe(&errpipe));
   oldstderr = dup(STDERR_FILENO);
   TEST(oldstderr > 0);
   TEST(STDERR_FILENO == dup2(errpipe.write, STDERR_FILENO));

   s_static_offset = 0;
   TEST(0 == switchoff_testmm());
   oldmm = mm_maincontext();
   staticmm = (mm_it) mm_it_INIT(&static_malloc, &static_mresize, &static_mfree, oldmm.iimpl->sizeallocated);
   mm_maincontext().iimpl = &staticmm;
   TEST(0 == init_resourceusage(&usage));
   mm_maincontext() = oldmm;
   isStatic = false;

   if (test_querymacros())    goto ONERR;

   TEST(0 == same_resourceusage(&usage));

   TEST(0 == free_maincontext());
   TEST(maincontext_STATIC == type_maincontext());

   if (test_querymacros())    goto ONERR;
   if (test_initmain())       goto ONERR;
   if (test_initerror())      goto ONERR;
   if (test_initstart())      goto ONERR;
   if (test_progname())       goto ONERR;

   TEST(0 == init_maincontext(type, argc, argv));
   TEST(0 == same_resourceusage(&usage));

   // unprepare
   isStatic = true;
   mm_maincontext().iimpl = &staticmm;
   TEST(0 == free_resourceusage(&usage));
   mm_maincontext() = oldmm;
   isStatic = false;

   int rsize = read(errpipe.read, s_static_buffer, sizeof(s_static_buffer)-1);
   TEST(rsize > 0);
   s_static_buffer[rsize] = 0;
   PRINTF_ERRLOG("%s", s_static_buffer);
   TEST(-1 == read(errpipe.read, s_static_buffer, sizeof(s_static_buffer)-1));
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO));
   TEST(0 == free_pipe(&errpipe));

   return 0;
ONERR:
   if (maincontext_STATIC == type_maincontext()) {
      (void) init_maincontext(type, argc, argv);
   }
   if (!isStatic) {
      mm_maincontext().iimpl = &staticmm;
   }
   (void) free_resourceusage(&usage);
   mm_maincontext() = oldmm;
   free_pipe(&errpipe);
   if (0 < oldstderr) {
      dup2(oldstderr, STDERR_FILENO);
      close(oldstderr);
   }
   return EINVAL;
}

int unittest_main_maincontext()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
