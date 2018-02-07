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
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/io/writer/log/logwriter.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/platform/init.h"
#include "C-kern/api/platform/syslogin.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/task/thread.h"          // TODO: try removing after integration init into thread
#include "C-kern/api/platform/task/thread_stack.h"    // TODO: try removing after integration init into thread
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/memory/mm/mm_impl.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/test/mm/testmm.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/locale.h"
#endif


// section: maincontext_t

// forward
static logwriter_t s_maincontext_initlog;

// group: global variables

/* variable: g_maincontext
 * Reserve space for the global main context. */
maincontext_t g_maincontext = maincontext_INIT_STATIC;

// TODO: 1 merge processcontext_t with maincontext_t
// TODO: 2 use (implement it) main static memory manage instead of thread static memory
// TODO: 4 rewrite log_macros, remove errlog, use XXX_LOG(ERR, ... instead
//       add support for giving logwriter_t as argument to support log during init
//       threadstack uses init log ... (given as parameter)
//       Implement TRACE_LOG which selects TRACE0_LOG, TRACE1_LOG, ... on number of parameter !!
//       Use nrargsof to implement this feature
// TODO: 5 integrate thread_stack functionality into thread module ?
//         better to use thread_stack_t !
//       - init static logwriter in new_thread
//       - use initlog from maincontext in initmain_thread
//       6. implement init_syscontext and call it during init in maincontext_t (former processcontext_t)
//       7. rename Linux init.c into sysinit.c (or remove it of functionality could be moved into initmain_thread)

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_maincontext_errtimer
 * Simulates an error in <maincontext_t.init_maincontext>. */
static test_errortimer_t   s_maincontext_errtimer = test_errortimer_FREE;
#endif

/* variable: s_maincontext_initlog
 * Assigned to <maincontext_t.initlog>. */
static logwriter_t   s_maincontext_initlog = logwriter_FREE;
/* variable: s_maincontext_initlog
 * Assigned to <maincontext_t.initlog>. */
static uint8_t       s_maincontext_logbuffer[log_config_MINSIZE];


// group: helper

/* function: init_progname
 * Sets *progname to (strrchr(argv0, '/')+1). */
static void init_progname(/*out*/const char ** progname, const char * argv0)
{
   const char * last = strrchr(argv0, '/');

   *progname = (last ? last + 1 : argv0);
}

static inline void set_args_maincontext(/*out*/maincontext_t * maincontext, mainthread_f main_thread, int argc, const char ** argv)
{
   maincontext->main_thread = main_thread;
   maincontext->progname = "";
   maincontext->argc     = argc;
   maincontext->argv     = argv;

   if (argc) {
      init_progname(&maincontext->progname, argv[0]);
   }
}

static inline void clear_args_maincontext(/*out*/maincontext_t * maincontext)
{
   maincontext->main_thread = 0;
   maincontext->progname = 0;
   maincontext->argc = 0;
   maincontext->argv = 0;
}

static inline void initlog_maincontext(/*out*/ilog_t* initlog, logwriter_t* logwriter)
{
   if (initstatic_logwriter(logwriter, sizeof(s_maincontext_logbuffer), s_maincontext_logbuffer)) {
      #define ERRSTR "FATAL ERROR: initlog_maincontext(): call to initstatic_logwriter failed\n"
      ssize_t ignore = write(sys_iochannel_STDERR, ERRSTR, sizeof(ERRSTR)-1);
      (void) ignore;
      #undef ERRSTR
      abort();
   }
   *initlog = (ilog_t) iobj_INIT((struct log_t*)logwriter, interface_logwriter());
}

static inline void freelog_maincontext(ilog_t* initlog)
{
   if (!isfree_iobj(initlog)) {
      freestatic_logwriter((logwriter_t*)initlog->object);
      *initlog = (ilog_t) iobj_FREE;
   }
}

// group: lifetime

/* function: free_maincontext
 * Frees <g_maincontext> of type <maincontext_t>.
 *
 * Ensures that after return the basic logging functionality is working.
 *
 * Background:
 * This function calls free_threadcontext and then free_processcontext which call every
 * freethread_NAME and freeonce_NAME functions in the reverse order as defined in
 * "C-kern/resource/config/initthread" and "C-kern/resource/config/initprocess".
 * This init database files are checked against the whole project
 * with "C-kern/test/static/check_textdb.sh". */
static int free_maincontext(void)
{
   int err;
   int is_initialized = (maincontext_STATIC != g_maincontext.type);

   if (is_initialized) {
      err = free_processcontext(&g_maincontext.pcontext);
      (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err);

      // TODO: remove check ? (freestatic_threadstack also checks this value)
      if (!err && extsize_threadcontext() != sizestatic_threadstack(self_threadstack())) {
         err = ENOTEMPTY;
      }
      (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err);

      g_maincontext.type = maincontext_STATIC;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

/* function: init_maincontext
 * INitializes <processcontext_t> of <g_maincontext> and the current <threadcontext_t>.
 *
 * Background:
 * Functions init_processcontext and init_threadcontext call every
 * initonce_NAME and initthread_NAME function in the same order as defined in
 * "C-kern/resource/config/initprocess" and "C-kern/resource/config/initthread".
 * This init database files are checked against the whole project with
 * "C-kern/test/static/check_textdb.sh". */
static int init_maincontext(const maincontext_e context_type)
{
   int err;

   g_maincontext.type = context_type;

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_processcontext(&g_maincontext.pcontext);
   }
   if (err) goto ONERR;

   return 0;
ONERR:
   free_maincontext();
   TRACEEXIT_ERRLOG(err);
   return err;
}

// TODO: remove this function
void start_mainthread(void)
{
   int err;
   thread_t* thread = self_thread();
   const maincontext_e context_type = g_maincontext.startarg_type;

   if (maincontext_STATIC != context_type) {
      err = init_maincontext(context_type);
      if (err) goto ONERR;

      if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
         err = init_threadcontext(tcontext_maincontext(), context_type);
      }
      if (err) goto ONERR;
   }

   err = thread->task(thread->task_arg);
   setreturncode_thread(thread, err);

   if (maincontext_STATIC != context_type) {
      err = free_threadcontext(tcontext_maincontext());
      (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err);
      if (err) goto ONERR;

      err = free_maincontext();
      if (err) goto ONERR;
   }

   return;
ONERR:
   (void) free_maincontext();
   setreturncode_thread(thread, err);
}

int initrun_maincontext(maincontext_e type, mainthread_f main_thread, int argc, const char** argv)
{
   int err,err2;
   int is_already_initialized = (maincontext_STATIC != g_maincontext.type);

   if (is_already_initialized) {
      TRACEEXIT_ERRLOG(EALREADY);
      return EALREADY;
   }

   // setup initlog which is used for error logging during initialization
   if (isfree_iobj(&g_maincontext.initlog)) {
      initlog_maincontext(&g_maincontext.initlog, &s_maincontext_initlog);
   }

   if (  type <= maincontext_STATIC
      || type >= maincontext__NROF) {
      err = EINVAL;
      goto ONERR;
   }

   if ( argc < 0 || (argc != 0 && argv == 0)) {
      err = EINVAL;
      goto ONERR;
   }

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_syscontext(&g_maincontext.sysinfo);
   }
   if (err) goto ONERR;

   set_args_maincontext(&g_maincontext, main_thread, argc, argv);
   g_maincontext.startarg_type = type;
   err = initrun_syscontext((thread_f)main_thread, &g_maincontext);
   err2 = free_syscontext(&g_maincontext.sysinfo);
   clear_args_maincontext(&g_maincontext);
// TODO: add test that syscontet is initialized and freed check init in test_thread !!
   return err ? err : err2;
ONERR:
   TRACE_LOG(INIT, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
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

static int check_isfree_args(maincontext_t* maincontext)
{

   // check maincontext
   TEST(0 == maincontext->type);
   TEST(0 == maincontext->main_thread);
   TEST(0 == maincontext->progname);
   TEST(0 == maincontext->argc);
   TEST(0 == maincontext->argv);

   return 0;
ONERR:
   return EINVAL;
}

static int check_isstatic_maincontext(maincontext_t* maincontext, int issyscontext, int isinitlog)
{
   // check env
   const size_t S=extsize_threadcontext();
   TEST(self_maincontext() == &g_maincontext);
   TEST(S == sizestatic_threadstack(self_threadstack())); // main thread already runs with threadstack
   TEST(tcontext_maincontext()->staticdata == (void*) tcontext_maincontext()->log.object);
   TEST(0 == strcmp("C", current_locale()));

   // check maincontext
   TEST(1 == isstatic_processcontext(&maincontext->pcontext));
   TEST(issyscontext == ! isfree_syscontext(&maincontext->sysinfo));

   TEST(1 == isstatic_threadcontext(tcontext_maincontext()));
   TEST(0 == check_isfree_args(maincontext));
   if (isinitlog) {
      TEST(&s_maincontext_initlog == (logwriter_t*) maincontext->initlog.object);
      TEST(interface_logwriter()  == maincontext->initlog.iimpl);
   } else {
      TEST(1 == isfree_iobj(&maincontext->initlog));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_querymacros(void)
{
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

   // TEST sysinfo_maincontext
   TEST(&sysinfo_maincontext()     == &g_maincontext.sysinfo);

   // TEST syncrunner_maincontext
   TEST(&syncrunner_maincontext()  == &tcontext_maincontext()->syncrunner);

   thread_stack_t* tls = self_threadstack();

   // TEST tcontext_maincontext
   TEST(tcontext_maincontext()     == context_threadstack(tls));

   // TEST threadid_maincontext
   TEST(&threadid_maincontext()    == &context_threadstack(tls)->thread_id);

   // TEST type_maincontext
   TEST(&type_maincontext()        == &g_maincontext.type);

   return 0;
ONERR:
   return EINVAL;
}

static int test_helper(void)
{
   maincontext_t maincontext = maincontext_INIT_STATIC;
   const char * argv[4] = { "/p1/yxz1", "/p2/yxz2/", "p3/p4/yxz3", "0x123456789abcdef" };

   // TEST set_args_maincontext
   for (uintptr_t i = 0; i < lengthof(argv); ++i) {
      maincontext = (maincontext_t) maincontext_INIT_STATIC;
      set_args_maincontext(&maincontext, (mainthread_f)i, (int) (1+i), &argv[i]);
      TEST(1*i == (uintptr_t) maincontext.main_thread);
      TEST(1+i == (unsigned) maincontext.argc);
      TEST(&argv[i] == maincontext.argv);
      switch(i) {
      case 0:  TEST(&argv[0][4] == maincontext.progname); break;
      case 1:  TEST(&argv[1][9] == maincontext.progname); break;
      case 2:  TEST(&argv[2][6] == maincontext.progname); break;
      case 3:  TEST(&argv[3][0] == maincontext.progname); break;
      }
   }

   // TEST set_args_maincontext: argc == 0
   maincontext = (maincontext_t) maincontext_INIT_STATIC;
   set_args_maincontext(&maincontext, 0, 0, argv);
   TEST(0 == maincontext.main_thread);
   TEST(0 != maincontext.progname && 0 == strcmp("", maincontext.progname));
   TEST(0 == maincontext.argc);
   TEST(argv == maincontext.argv);

   // TEST clear_args_maincontext
   // prepare
   maincontext.main_thread = (mainthread_f) &test_helper;
   maincontext.progname = "";
   maincontext.argc = 1;
   maincontext.argv = argv;
   // test
   clear_args_maincontext(&maincontext);
   // check maincontext
   TEST(0 == check_isfree_args(&maincontext));

   // TEST initlog_maincontext
   ilog_t initlog = iobj_FREE;
   logwriter_t lw = logwriter_FREE;
   initlog_maincontext(&initlog, &lw);
   TEST(initlog.object == (struct log_t*)&lw);
   TEST(initlog.iimpl  == interface_logwriter());
   TEST(lw.addr == s_maincontext_logbuffer);
   TEST(lw.size == log_config_MINSIZE);

   // TEST freelog_maincontext
   freelog_maincontext(&initlog);
   TEST(isfree_iobj(&initlog));
   TEST(isfree_logwriter(&lw));

   return 0;
ONERR:
   return EINVAL;
}

static int test_init(void)
{
   // prepare
   // TODO: move init_syscontext into init_maincontext !!
   TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 0));
   TEST(0 == init_syscontext(&g_maincontext.sysinfo));
   TEST(0 == check_isstatic_maincontext(&g_maincontext, 1, 0));

   // TEST maincontext_INIT_STATIC
   {
      maincontext_t maincontext = maincontext_INIT_STATIC;
      TEST(0 == check_isstatic_maincontext(&maincontext, 0, 0));
   }

   maincontext_e mainmode[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   static_assert(lengthof(mainmode) == maincontext__NROF-1, "test all modes except maincontext_STATIC");
   for (unsigned i = 0; i < lengthof(mainmode); ++i) {

      // TEST init_maincontext: different init types
      TEST(0 == init_maincontext(mainmode[i]));
      TEST(sizestatic_threadstack(self_threadstack()) == extsize_processcontext() + extsize_threadcontext());
      TEST(g_maincontext.type == mainmode[i]);
      TEST(g_maincontext.main_thread == 0);
      TEST(g_maincontext.argc == 0);
      TEST(g_maincontext.argv == 0);
      TEST(g_maincontext.progname == 0);
      TEST(0 != g_maincontext.pcontext.syslogin);
      TEST(0 == tcontext_maincontext()->initcount); // not initialized
      TEST(0 != strcmp("C", current_locale()));

      // TEST free_maincontext: free / double free
      for (int tc = 0; tc < 2; ++tc) {
         TEST(0 == free_maincontext());
         TEST(0 == check_isstatic_maincontext(&g_maincontext, 1, 0));
      }
   }

   // TEST free_maincontext: ENOTEMPTY
   // prealloc static mem to generate ENOTEMPTY
   memblock_t mblock;
   const size_t oldsize=sizestatic_threadstack(self_threadstack());
   TEST(0 == allocstatic_threadstack(self_threadstack(), 1, &mblock));
   TEST(0 == init_maincontext(maincontext_DEFAULT));
   // test
   TEST(ENOTEMPTY == free_maincontext());
   TEST(0 == freestatic_threadstack(self_threadstack(), &mblock));
   TEST(oldsize == sizestatic_threadstack(self_threadstack()));
   // check g_maincontext
   TEST(0 == check_isstatic_maincontext(&g_maincontext, 1, 0));

   // TEST init_maincontext: simulated error
   for (unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_maincontext_errtimer, i, (int)(3+i));
      int err = init_maincontext(maincontext_DEFAULT);
      if (!err) {
         free_testerrortimer(&s_maincontext_errtimer);
         TESTP(i == 2, "i:%d",i);
         break;
      }
      TEST(3+i == (unsigned)err);
      TEST(0 == check_isstatic_maincontext(&g_maincontext, 1, 0));
   }
   TEST(0 == free_maincontext());

   // TEST free_maincontext: simulated error
   for (unsigned i = 1; ; ++i) {
      TEST(0 == init_maincontext(maincontext_DEFAULT));
      init_testerrortimer(&s_maincontext_errtimer, i, (int)(1+i));
      int err = free_maincontext();
      if (!err) {
         free_testerrortimer(&s_maincontext_errtimer);
         TESTP(i == 3, "i:%d",i);
         break;
      }
      TEST(1+i == (unsigned)err);
      TEST(0 == check_isstatic_maincontext(&g_maincontext, 1, 0));
   }

   // unprepare
   TEST(0 == free_syscontext(&g_maincontext.sysinfo));

   return 0;
ONERR:
   free_maincontext();
   return EINVAL;
}

static maincontext_e s_mainmode[] = { maincontext_DEFAULT, maincontext_CONSOLE };
static const char *  s_argv[lengthof(s_mainmode)] = { 0 };
static uintptr_t     s_i;
static int           s_is_called;

static int test_init_param(maincontext_t * maincontext)
{
   if (maincontext != &g_maincontext) return EINVAL;
   if (maincontext->type != s_mainmode[s_i]) return EINVAL;
   if (maincontext->main_thread != &test_init_param) return EINVAL;
   if (maincontext->progname != &s_argv[s_i][2]) return EINVAL;
   if (maincontext->argc != (int) (1 + s_i)) return EINVAL;
   if (maincontext->argv != &s_argv[s_i]) return EINVAL;
   if (isfree_syscontext(&maincontext->sysinfo)) return EINVAL;
   if (! isvalid_syscontext(&maincontext->sysinfo)) return EINVAL;

   s_is_called = 1;

   return 0;
}

static int test_init_returncode(maincontext_t * maincontext)
{
   if (maincontext != &g_maincontext) return EINVAL;
   if (maincontext->type != s_mainmode[s_i % lengthof(s_mainmode) ]) return EINVAL;
   if (maincontext->main_thread != &test_init_returncode) return EINVAL;
   if (strcmp("", maincontext->progname)) return EINVAL;
   if (maincontext->argc != 0) return EINVAL;
   if (maincontext->argv != 0) return EINVAL;

   s_is_called = 1;

   return (int) s_i;
}

static int test_ealready(maincontext_t * maincontext)
{
   int err = initrun_maincontext(maincontext_DEFAULT, maincontext->main_thread, 0, 0);
   return err;
}

static int test_initrun(void)
{
   // prepare
   TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 0));

   // TEST initrun_maincontext: function called with initialized maincontext
   for (uintptr_t i = 0; i < lengthof(s_mainmode); ++i) {
      s_i = i;
      s_argv[i] = "./.";
      s_is_called = 0;
      // test
      TEST(0 == initrun_maincontext(s_mainmode[i], &test_init_param, (int) (1+i), &s_argv[i]));
      TEST(1 == s_is_called);

      // check free_syscontext, free_maincontext called
      TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 1));
   }

   // TEST initrun_maincontext: return value
   for (uintptr_t i = 0; i < 10; ++i) {
      s_is_called = 0;
      // test
      s_i = i; // argument to test_init_returncode
      TEST(i == (unsigned) initrun_maincontext(s_mainmode[i%lengthof(s_mainmode)], &test_init_returncode, 0, 0));
      TEST(1 == s_is_called);

      // check free_syscontext, free_maincontext called
      TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 1));
   }

   // TEST initrun_maincontext: initlog is set up
   for (uintptr_t i = 0; i < lengthof(s_mainmode); ++i) {
      TEST(0 == isfree_logwriter(&s_maincontext_initlog));
      flushbuffer_logwriter(&s_maincontext_initlog, log_channel_ERR);
      freelog_maincontext(&g_maincontext.initlog);
      TEST(1 == isfree_iobj(&g_maincontext.initlog));
      TEST(1 == isfree_logwriter(&s_maincontext_initlog));
      g_maincontext.initlog = (ilog_t) iobj_FREE;
      s_i = i;
      s_argv[i] = "./.";
      s_is_called = 0;
      // test
      TEST(0 == initrun_maincontext(s_mainmode[i], &test_init_param, (int) (1+i), &s_argv[i]));
      TEST(1 == s_is_called);

      // check initlog
      TEST( g_maincontext.initlog.object == (struct log_t*)&s_maincontext_initlog);
      TEST( g_maincontext.initlog.iimpl  == interface_logwriter());
      // check s_maincontext_initlog
      TEST(s_maincontext_initlog.addr == s_maincontext_logbuffer);
      TEST(s_maincontext_initlog.size == sizeof(s_maincontext_logbuffer));

      // check free_syscontext, free_maincontext called
      TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 1));
   }

   // TEST initrun_maincontext: EINVAL (but initlog setup ==> error log is written)
   freelog_maincontext(&g_maincontext.initlog);
   TEST(EINVAL == initrun_maincontext(maincontext_STATIC, &test_init_returncode, 0, 0));
   TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 1));
   freelog_maincontext(&g_maincontext.initlog);
   TEST(EINVAL == initrun_maincontext(maincontext__NROF, &test_init_returncode, 0, 0));
   TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 1));
   freelog_maincontext(&g_maincontext.initlog);
   TEST(EINVAL == initrun_maincontext(maincontext_DEFAULT, &test_init_returncode, -1, 0));
   TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 1));
   freelog_maincontext(&g_maincontext.initlog);
   TEST(EINVAL == initrun_maincontext(maincontext_DEFAULT, &test_init_returncode, 1, 0));
   TEST(0 == check_isstatic_maincontext(&g_maincontext, 0, 1));

   // TEST initrun_maincontext: EALREADY
   TEST(EALREADY == initrun_maincontext(maincontext_DEFAULT, &test_ealready, 0, 0));

   // TEST initrun_maincontext: simulated error
   s_i = 0; // argument to test_init_returncode
   for (unsigned i=1; ; ++i) {
      s_is_called = 0;
      init_testerrortimer(&s_maincontext_errtimer, i, (int)i);
      // test
      int err = initrun_maincontext(s_mainmode[s_i], &test_init_returncode, 0, 0);
      // check calling of test_init_returncode
      TESTP( (i >= 4) == s_is_called, "i=%d", i);
      // check free_syscontext, free_maincontext, and clear_args_maincontext called
      TEST( 0 == check_isstatic_maincontext(&g_maincontext, 0, 1));
      if (!err) {
         TESTP(i==7,"i:%d",i);
         free_testerrortimer(&s_maincontext_errtimer);
         break;
      }
      TEST( i == (unsigned)err);
   }

   // unprepare

   return 0;
ONERR:
   free_testerrortimer(&s_maincontext_errtimer);
   return EINVAL;
}

static volatile int test_static_result;

static int test_static(void* dummy)
{
   TEST(0 == dummy);
   TEST(maincontext_STATIC == type_maincontext());

   if (test_querymacros())    goto ONERR;
   if (test_helper())         goto ONERR;
   if (test_init())           goto ONERR;
   if (test_initrun())        goto ONERR;

   test_static_result = 0;
   return 0;
ONERR:
   test_static_result = EINVAL;
   return EINVAL;
}

static int childprocess_unittest(void)
{
   maincontext_t   oldmc;
   resourceusage_t usage = resourceusage_FREE;
   pipe_t          errpipe = pipe_FREE;
   int             oldstderr = dup(STDERR_FILENO);

   // prepare
   memcpy(&oldmc, &g_maincontext, sizeof(oldmc));
   TEST(0 <  oldstderr);
   TEST(0 == init_pipe(&errpipe));
   TEST(STDERR_FILENO == dup2(errpipe.write, STDERR_FILENO));

   // test initialized context

   TEST(0 == init_resourceusage(&usage));
   if (test_querymacros())    goto ONERR;
   if (test_helper())         goto ONERR;
   TEST( 0 == same_resourceusage(&usage));
   TEST( 0 == free_resourceusage(&usage));

   // test uninitialized context

   freeonce_locale();
   freeonce_signalhandler();
   test_static_result = EINVAL;
   TEST(0 == init_resourceusage(&usage));
   memcpy(&oldmc, &g_maincontext, sizeof(oldmc));
   g_maincontext = (maincontext_t) maincontext_INIT_STATIC;
   TEST( 0 == initrun_syscontext(&test_static, (void*) 0));
   memcpy(&g_maincontext, &oldmc, sizeof(oldmc));
   TEST( 0 == test_static_result);
   TEST( 0 == same_resourceusage(&usage));
   TEST( 0 == free_resourceusage(&usage));

   // get error log from pipe
   static uint8_t s_static_buffer[20000];
   ssize_t rsize = read(errpipe.read, s_static_buffer, sizeof(s_static_buffer)-1);
   TEST(0 < rsize);
   s_static_buffer[rsize] = 0;
   PRINTF_ERRLOG("%s", s_static_buffer);
   TEST(-1 == read(errpipe.read, s_static_buffer, sizeof(s_static_buffer)-1));

   // reset
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO));
   TEST(0 == free_pipe(&errpipe));
   TEST(0 == free_iochannel(&oldstderr))

   return 0;
ONERR:
   memcpy(&g_maincontext, &oldmc, sizeof(oldmc));
   (void) free_resourceusage(&usage);
   if (0 < oldstderr) {
      dup2(oldstderr, STDERR_FILENO);
      close(oldstderr);
   }
   free_pipe(&errpipe);
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
