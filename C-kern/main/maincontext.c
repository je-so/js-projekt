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
#include "C-kern/api/platform/syslogin.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/task/thread.h"

#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/memory/mm/mm_impl.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/platform/task/process.h"
#include "C-kern/api/test/mm/testmm.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/task/thread_stack.h"
#endif



// section: maincontext_t

// group: global variables

/* variable: g_maincontext
 * Reserve space for the global main context. */
maincontext_t g_maincontext = maincontext_INIT_STATIC;

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_maincontext_errtimer
 * Simulates an error in <maincontext_t.init_maincontext>. */
static test_errortimer_t   s_maincontext_errtimer = test_errortimer_FREE;
#endif


// group: helper

/* function: init_progname
 * Sets *progname to (strrchr(argv0, '/')+1). */
static void init_progname(/*out*/const char ** progname, const char * argv0)
{
   const char * last = strrchr(argv0, '/');

   *progname = (last ? last + 1 : argv0);
}

static inline void set_args_maincontext(/*out*/maincontext_t* maincontext, int argc, const char** argv)
{
   maincontext->progname = "";
   maincontext->argc     = argc;
   maincontext->argv     = argv;

   if (argc) {
      init_progname(&maincontext->progname, argv[0]);
   }
}

static inline void clear_args_maincontext(/*out*/maincontext_t* maincontext)
{
   maincontext->progname = 0;
   maincontext->argc = 0;
   maincontext->argv = 0;
}

static inline void initlog_maincontext(/*out*/ilog_t* initlog, logwriter_t* logwriter, size_t size, uint8_t logbuffer[size])
{
   if (initstatic_logwriter(logwriter, size, logbuffer) || PROCESS_testerrortimer(&s_maincontext_errtimer, 0)) {
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
int free_maincontext(void)
{
   int err;
   int is_initialized = (maincontext_STATIC != g_maincontext.type);

   if (is_initialized) {
      err = free_processcontext(&g_maincontext.pcontext);
      (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err);

      int err2 = free_syscontext(&g_maincontext.sysinfo);
      (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
      if (err2) err = err2;

      clear_args_maincontext(&g_maincontext);

      g_maincontext.type = maincontext_STATIC;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

/* function: init_maincontext
 * Initializes <maincontext_t> with its contained <processcontext_t>.
 *
 * Background:
 * Functions init_processcontext and init_threadcontext call every
 * initonce_NAME and initthread_NAME function in the same order as defined in
 * "C-kern/resource/config/initprocess" and "C-kern/resource/config/initthread".
 * This init database files are checked against the whole project with
 * "C-kern/test/static/check_textdb.sh". */
int init_maincontext(const maincontext_e context_type, int argc, const char* argv[])
{
   int err;

   g_maincontext.type = context_type;

   set_args_maincontext(&g_maincontext, argc, argv);

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_syscontext(&g_maincontext.sysinfo);
   }
   if (err) goto ONERR;

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_processcontext(&g_maincontext.pcontext);
   }
   if (err) goto ONERR;

   return 0;
ONERR:
   free_syscontext(&g_maincontext.sysinfo);
   free_maincontext();
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initrun_maincontext(maincontext_e type, mainthread_f main_thread, int argc, const char** argv)
{
   int err;
   int is_already_initialized = (maincontext_STATIC != g_maincontext.type);
   logwriter_t lgwrt;
   uint8_t     logbuffer[log_config_MINSIZE];
   ilog_t      initlog = iobj_FREE;

   if (is_already_initialized) {
      TRACEEXIT_ERRLOG(EALREADY);
      return EALREADY;
   }

   // setup initlog which is used for error logging during initialization
   initlog_maincontext(&initlog, &lgwrt, sizeof(logbuffer), logbuffer);

   if (  type <= maincontext_STATIC
      || type >= maincontext__NROF) {
      err = EINVAL;
      goto ONERR;
   }

   if ( argc < 0 || (argc != 0 && argv == 0)) {
      err = EINVAL;
      goto ONERR;
   }

   g_maincontext.startarg_type = type;
   g_maincontext.startarg_argc = argc;
   g_maincontext.startarg_argv = argv;

   int retcode;
   err = runmain_thread(&retcode, (thread_f)main_thread, &g_maincontext, &initlog, type, argc, argv);
   if (!err) err = retcode;

   freelog_maincontext(&initlog);

   return err;
ONERR:
   TRACE_LOG(&initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   freelog_maincontext(&initlog);
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
   TEST(0 == maincontext->progname);
   TEST(0 == maincontext->argc);
   TEST(0 == maincontext->argv);

   return 0;
ONERR:
   return EINVAL;
}

static int check_isstatic_maincontext(maincontext_t* maincontext)
{
   // check env
   const size_t S=extsize_threadcontext();
   TEST(self_maincontext() == &g_maincontext);
   TEST(S == sizestatic_threadstack(self_threadstack())); // main thread already runs with threadstack
   TEST(tcontext_maincontext()->staticdata == (void*) tcontext_maincontext()->log.object);
   TEST(0 == strcmp("C", current_locale()));

   // check maincontext
   TEST(1 == isstatic_processcontext(&maincontext->pcontext));
   TEST(1 == isfree_syscontext(&maincontext->sysinfo));

   TEST(1 == isstatic_threadcontext(tcontext_maincontext()));
   TEST(0 == check_isfree_args(maincontext));

   return 0;
ONERR:
   return EINVAL;
}

static int check_noerror_logged(pipe_t* errpipe)
{
   uint8_t c;

   TEST( -1 == read(errpipe->read, &c, 1));
   TEST( EAGAIN == errno);

   return 0;
ONERR:
   return EINVAL;
}

static int check_error_logged(pipe_t* errpipe, int oldstderr)
{
   uint8_t buffer[64];
   ssize_t len;

   len = read(errpipe->read, buffer,sizeof(buffer));
   TEST( len == sizeof(buffer));

   do {
      if (isvalid_iochannel(oldstderr)) {
         TEST(len == write(oldstderr, buffer, (size_t)len));
      }
      len = read(errpipe->read, buffer,sizeof(buffer));
   } while (len > 0);
   TEST( -1 == len);
   TEST( EAGAIN == errno);

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
      set_args_maincontext(&maincontext, (int) (1+i), &argv[i]);
      TEST(1+i      == (unsigned) maincontext.argc);
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
   set_args_maincontext(&maincontext, 0, argv);
   TEST(0 != maincontext.progname && 0 == strcmp("", maincontext.progname));
   TEST(0 == maincontext.argc);
   TEST(argv == maincontext.argv);

   // TEST clear_args_maincontext
   // prepare
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
   uint8_t     lb[log_config_MINSIZE];
   initlog_maincontext(&initlog, &lw, log_config_MINSIZE, lb);
   TEST(initlog.object == (struct log_t*)&lw);
   TEST(initlog.iimpl  == interface_logwriter());
   TEST(lw.addr == lb);
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
   logwriter_t lgwrt = logwriter_FREE;
   ilog_t      initlog = iobj_INIT((struct log_t*)&lgwrt, interface_logwriter());
   uint8_t     logbuf[log_config_MINSIZE];
   const char *argv[4] = { "progname", 0, 0, 0 };

   // prepare
   TEST(0 == check_isstatic_maincontext(&g_maincontext));
   TEST(0 == initstatic_logwriter(&lgwrt, sizeof(logbuf), logbuf));

   // TEST maincontext_INIT_STATIC
   {
      maincontext_t maincontext = maincontext_INIT_STATIC;
      TEST(0 == check_isstatic_maincontext(&maincontext));
   }

   maincontext_e mainmode[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   static_assert(lengthof(mainmode) == maincontext__NROF-1, "test all modes except maincontext_STATIC");
   for (unsigned i = 0; i < lengthof(mainmode); ++i) {
      for (unsigned argc=0; argc < lengthof(argv); ++argc) {

         // TEST init_maincontext: different init types
         TEST(0 == init_maincontext(mainmode[i], (int)argc, argc?argv:0));
         TEST(sizestatic_threadstack(self_threadstack()) == extsize_processcontext() + extsize_threadcontext());
         TEST(g_maincontext.type == mainmode[i]);
         TEST(g_maincontext.argc == (int)argc);
         TEST(g_maincontext.argv == (argc?argv:0));
         TEST(g_maincontext.progname != 0);
         TEST(strcmp(g_maincontext.progname, argc?"progname":"") == 0);
         TEST(0 != g_maincontext.pcontext.syslogin);
         TEST(0 == tcontext_maincontext()->initcount); // not initialized
         TEST(0 != strcmp("C", current_locale()));

         // TEST free_maincontext: free / double free
         for (int tc = 0; tc < 2; ++tc) {
            TEST(0 == free_maincontext());
            TEST(0 == check_isstatic_maincontext(&g_maincontext));
         }
      }
   }

   // TEST free_maincontext: EMEMLEAK
   TEST(0 == init_maincontext(maincontext_DEFAULT, 1, argv));
   memblock_t mblock;
   TEST(0 == allocstatic_threadstack(self_threadstack(), &initlog, 1, &mblock)); // alloc additional mem
   // test
   uint8_t* staticmemblock = pcontext_maincontext()->staticmemblock;
   TEST( EMEMLEAK == free_maincontext());
   // check
   TEST( 0 == pcontext_maincontext()->staticmemblock); // TODO: !! merge pcontext with maincontext !!
   // reset
   TEST(0 == freestatic_threadstack(self_threadstack(), &initlog, &mblock));
   pcontext_maincontext()->staticmemblock = staticmemblock;
   pcontext_maincontext()->initcount = 1;
   TEST(0 == free_processcontext(pcontext_maincontext()));
   TEST(0 == check_isstatic_maincontext(&g_maincontext));

   // TEST init_maincontext: simulated error
   for (unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_maincontext_errtimer, i, (int)(3+i));
      int err = init_maincontext(maincontext_DEFAULT, 0, 0);
      if (!err) {
         free_testerrortimer(&s_maincontext_errtimer);
         TESTP( 3==i, "i:%d",i);
         break;
      }
      TEST(3+i == (unsigned)err);
      TEST(0 == check_isstatic_maincontext(&g_maincontext));
   }
   TEST(0 == free_maincontext());

   // TEST free_maincontext: simulated error
   for (unsigned i = 1; ; ++i) {
      TEST(0 == init_maincontext(maincontext_DEFAULT, 1, argv));
      init_testerrortimer(&s_maincontext_errtimer, i, (int)(1+i));
      int err = free_maincontext();
      if (!err) {
         free_testerrortimer(&s_maincontext_errtimer);
         TESTP( 3==i, "i:%d",i);
         break;
      }
      TEST(1+i == (unsigned)err);
      TEST(0 == check_isstatic_maincontext(&g_maincontext));
   }

   // check no logs written
   {
      uint8_t *lb;
      size_t   logsize;
      GETBUFFER_LOG(&initlog, log_channel_ERR, &lb, &logsize);
      TEST( 0 == logsize);
   }

   // unprepare
   TEST(0 == free_syscontext(&g_maincontext.sysinfo));
   freestatic_logwriter(&lgwrt);

   return 0;
ONERR:
   free_maincontext();
   freestatic_logwriter(&lgwrt);
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
   if (strcmp("", maincontext->progname)) return EINVAL;
   if (maincontext->argc != 0) return EINVAL;
   if (maincontext->argv != 0) return EINVAL;

   s_is_called = 1;

   return (int) s_i;
}

static int test_ealready(maincontext_t* maincontext)
{
   (void) maincontext;
   int err = initrun_maincontext(maincontext_DEFAULT, &test_ealready, 0, 0);
   return err;
}

static int test_initrun(void)
{
   pipe_t  errpipe = pipe_FREE;
   int     oldstderr = dup(STDERR_FILENO);
   process_t child = process_FREE;

   // prepare
   TEST(0 == check_isstatic_maincontext(&g_maincontext));
   TEST(0 <  oldstderr);
   TEST(0 == init_pipe(&errpipe));
   TEST(STDERR_FILENO == dup2(errpipe.write, STDERR_FILENO));

   // TEST initrun_maincontext: function called with initialized maincontext
   for (uintptr_t i = 0; i < lengthof(s_mainmode); ++i) {
      s_i = i;
      s_argv[i] = "./.";
      s_is_called = 0;
      // test
      TEST(0 == initrun_maincontext(s_mainmode[i], &test_init_param, (int) (1+i), &s_argv[i]));
      TEST(1 == s_is_called);

      // check free_syscontext, free_maincontext called
      TEST(0 == check_isstatic_maincontext(&g_maincontext));
   }

   // TEST initrun_maincontext: return value
   for (uintptr_t i = 0; i < 10; ++i) {
      s_is_called = 0;
      // test
      s_i = i; // argument to test_init_returncode
      TEST(i == (unsigned) initrun_maincontext(s_mainmode[i%lengthof(s_mainmode)], &test_init_returncode, 0, 0));
      TEST(1 == s_is_called);

      // check free_syscontext, free_maincontext called
      TEST(0 == check_isstatic_maincontext(&g_maincontext));
   }

   // TEST initrun_maincontext: EINVAL (but initlog setup ==> error log is written)
   TEST(0 == check_noerror_logged(&errpipe));
   TEST(EINVAL == initrun_maincontext(maincontext_STATIC, &test_init_returncode, 0, 0));
   TEST(0 == check_isstatic_maincontext(&g_maincontext));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext__NROF, &test_init_returncode, 0, 0));
   TEST(0 == check_isstatic_maincontext(&g_maincontext));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext_DEFAULT, &test_init_returncode, -1, 0));
   TEST(0 == check_isstatic_maincontext(&g_maincontext));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext_DEFAULT, &test_init_returncode, 1, 0));
   TEST(0 == check_isstatic_maincontext(&g_maincontext));
   TEST(0 == check_error_logged(&errpipe, oldstderr));

   // TEST initrun_maincontext: EALREADY
   TEST(EALREADY == initrun_maincontext(maincontext_DEFAULT, &test_ealready, 0, 0));
   TEST(0 == check_error_logged(&errpipe, oldstderr));

   // reset
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO));
   TEST(0 == free_pipe(&errpipe));
   TEST(0 == free_iochannel(&oldstderr))

   return 0;
ONERR:
   free_testerrortimer(&s_maincontext_errtimer);
   free_process(&child);
   if (0 < oldstderr) {
      dup2(oldstderr, STDERR_FILENO);
      close(oldstderr);
   }
   free_pipe(&errpipe);
   return EINVAL;
}

static int test_static(void* dummy)
{
   TEST(0 == dummy);
   TEST(maincontext_STATIC == type_maincontext());

   if (test_querymacros())    goto ONERR;
   if (test_helper())         goto ONERR;
   if (test_init())           goto ONERR;
   if (test_initrun())        goto ONERR;

   return 0;
ONERR:
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
   TEST(0 == init_resourceusage(&usage));
   memcpy(&oldmc, &g_maincontext, sizeof(oldmc));
   g_maincontext = (maincontext_t) maincontext_INIT_STATIC;
   int err = EINVAL;
   TEST( 0 == runmain_thread(&err, &test_static, 0, GETWRITER0_LOG(), maincontext_STATIC, 0, 0));
   memcpy(&g_maincontext, &oldmc, sizeof(oldmc));
   TEST( 0 == err);
   TEST( 0 == same_resourceusage(&usage));
   TEST( 0 == free_resourceusage(&usage));

   // get error log from pipe
   uint8_t readbuffer[64];
   ssize_t sum=0;
   ssize_t rsize;
   while (0 < (rsize = read(errpipe.read, readbuffer, sizeof(readbuffer)))) {
      PRINTF_ERRLOG("%.*s", (int)rsize, readbuffer);
      sum += rsize;
   }
   TEST(-1 == rsize);
   TEST(1000 < sum);
   TEST(EAGAIN == errno);

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
