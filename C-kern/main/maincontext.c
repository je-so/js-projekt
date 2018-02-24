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
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/platform/syslogin.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/task/thread_stack.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/config/initmain)WHERE(header-name!='')
#include "C-kern/api/err/errorcontext.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/platform/syslogin.h"
#include "C-kern/api/platform/X11/x11.h"
// TEXTDB:END
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
#include "C-kern/api/platform/task/process.h"
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

// group: static memory

static inline uint16_t static_memory_size(void)
{
   uint16_t memorysize = 0
// TEXTDB:SELECT("         + sizeof("objtype")")FROM(C-kern/resource/config/initmain)WHERE(inittype=="object")
         + sizeof(signals_t)
         + sizeof(syslogin_t)
// TEXTDB:END
     ;
   return memorysize;
}

/* function: alloc_static_memory
 * Allokiert statischen Speicher für alle von <maincontext_t> zu initialisierenden Objekte.
 * */
static inline int alloc_static_memory(maincontext_t* maincontext, thread_stack_t* tst, /*out*/memblock_t* mblock)
{
   int err;

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = allocstatic_threadstack(tst, GETWRITER0_LOG(), static_memory_size(), mblock);
   }
   if (err) goto ONERR;

   maincontext->staticmemblock = mblock->addr;

   return 0;
ONERR:
   return err;
}

/* function: free_static_memory
 * Gibt statischen Speicher für alle im <maincontext_t> initialisierten Objekt frei.
 * */
static inline int free_static_memory(maincontext_t* maincontext, thread_stack_t* tst)
{
   int err;

   if (maincontext->staticmemblock) {
      memblock_t mblock = memblock_INIT(static_memory_size(), maincontext->staticmemblock);

      maincontext->staticmemblock = 0;

      err = freestatic_threadstack(tst, GETWRITER0_LOG(), &mblock);
      (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

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

int free_shared_services(maincontext_t* maincontext)
{
   int err;
   int err2;

   err = 0;

   switch (maincontext->initcount) {
   default: assert(false && "initcount out of bounds");
            break;
// TEXTDB:SELECT(\n"   case ("row-id"+1):"\n"            err2 = free"(if (inittype=='once') ("once"))"_"module"("(if (parameter!="") ((if (inittype!="object") ("&"))"maincontext->"parameter))");"\n"            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);"\n"            if (err2) err = err2;"(if (inittype=='object') (\n"            maincontext->"parameter" = 0;")))FROM(C-kern/resource/config/initmain)DESCENDING

   case (6+1):
            err2 = freeonce_X11();
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;

   case (5+1):
            err2 = free_syslogin(maincontext->syslogin);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
            maincontext->syslogin = 0;

   case (4+1):
            err2 = free_signals(maincontext->signals);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
            maincontext->signals = 0;

   case (3+1):
            err2 = freeonce_locale();
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;

   case (2+1):
            err2 = freeonce_errorcontext(&maincontext->error);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;

   case (1+1):
            err2 = free_syscontext(&maincontext->sysinfo);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
// TEXTDB:END
   case 1:  err2 = free_static_memory(maincontext, self_threadstack());
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
   case 0:  break;
   }

   maincontext->initcount = 0;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_shared_services(maincontext_t* maincontext)
{
   int err;

   maincontext->initcount = 0;

   memblock_t mblock;
   err = alloc_static_memory(maincontext, self_threadstack(), &mblock);
   if (err) goto ONERR;
   ++ maincontext->initcount;

// TEXTDB:SELECT(\n(if (inittype=='object') ("   assert( sizeof("objtype") <= mblock.size);"\n))"   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {"\n"      err = init"(if (inittype=='once') ("once"))"_"module"("(if (parameter!=""&&inittype!="object") ("&maincontext->"parameter))(if (inittype=='object') ("("objtype"*) mblock.addr"))");"\n"   }"\n"   if (err) goto ONERR;"\n"   ++ maincontext->initcount;"(if (inittype=='object') (\n"   maincontext->"parameter" = ("objtype"*) mblock.addr;"\n"   mblock.addr += sizeof("objtype");"\n"   mblock.size -= sizeof("objtype");")))FROM(C-kern/resource/config/initmain)

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_syscontext(&maincontext->sysinfo);
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = initonce_errorcontext(&maincontext->error);
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = initonce_locale();
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;

   assert( sizeof(signals_t) <= mblock.size);
   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_signals((signals_t*) mblock.addr);
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;
   maincontext->signals = (signals_t*) mblock.addr;
   mblock.addr += sizeof(signals_t);
   mblock.size -= sizeof(signals_t);

   assert( sizeof(syslogin_t) <= mblock.size);
   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_syslogin((syslogin_t*) mblock.addr);
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;
   maincontext->syslogin = (syslogin_t*) mblock.addr;
   mblock.addr += sizeof(syslogin_t);
   mblock.size -= sizeof(syslogin_t);

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = initonce_X11();
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;
// TEXTDB:END

   assert(mblock.size == 0);
   if (PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) goto ONERR;

   return 0;
ONERR:
   (void) free_shared_services(maincontext);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_maincontext(void)
{
   int err;

   if (maincontext_STATIC != g_maincontext.type) {
      err = free_shared_services(&g_maincontext);

      clear_args_maincontext(&g_maincontext);

      g_maincontext.type = maincontext_STATIC;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_maincontext(const maincontext_e context_type, int argc, const char* argv[])
{
   int err;

   g_maincontext.type = context_type;

   set_args_maincontext(&g_maincontext, argc, argv);

   err = init_shared_services(&g_maincontext);
   if (err) goto ONERR;

   return 0;
ONERR:
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

// group: query

bool isstatic_maincontext(const maincontext_t* maincontext)
{
   return   maincontext_STATIC == maincontext->type
            && 0 == maincontext->staticmemblock
            && 0 == maincontext->initcount
            // shared services
            && 1 == isfree_syscontext(&maincontext->sysinfo)
            && 0 == maincontext->syslogin
            && 0 == maincontext->signals
            && g_errorcontext_stroffset == maincontext->error.stroffset && g_errorcontext_strdata == maincontext->error.strdata
            // program arguments
            && 0 == maincontext->progname
            && 0 == maincontext->argc
            && 0 == maincontext->argv;
}

size_t extsize_maincontext(void)
{
   return static_memory_size();
}


// group: test

#ifdef KONFIG_UNITTEST

static int check_isfree_args(maincontext_t* maincontext)
{
   TEST(0 == maincontext->progname);
   TEST(0 == maincontext->argc);
   TEST(0 == maincontext->argv);

   return 0;
ONERR:
   return EINVAL;
}

static int check_isstatic(maincontext_t* maincontext)
{
   // check env
   TEST(self_maincontext() == &g_maincontext);
   TEST(1 == isstatic_threadcontext(tcontext_maincontext()));
   TEST(0 == strcmp("C", current_locale()));

   // check maincontext
   TEST(1 == isstatic_maincontext(maincontext));

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
   TEST(&error_maincontext()       == &self_maincontext()->error);

   // TEST log_maincontext
   TEST(&log_maincontext()         == &tcontext_maincontext()->log);

   // TEST log_maincontext
   TEST(&objectcache_maincontext() == &tcontext_maincontext()->objectcache);

   // TEST pagecache_maincontext
   TEST(&pagecache_maincontext()   == &tcontext_maincontext()->pagecache);

   // TEST progname_maincontext
   TEST(&progname_maincontext()    == &g_maincontext.progname);

   // TEST self_maincontext
   TEST(self_maincontext()         == &g_maincontext);

   // TEST syslogin_maincontext
   TEST(&syslogin_maincontext()    == &self_maincontext()->syslogin);

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

static int test_staticmemory(void)
{
   maincontext_t  maincontext = maincontext_INIT_STATIC;
   thread_stack_t*tls = 0;
   const size_t   S        = static_memory_size();
   memblock_t     mblock   = memblock_FREE;
   uint8_t        logbuf[log_config_MINSIZE];
   logwriter_t    lgwrt    = logwriter_FREE;
   ilog_t         initlog  = iobj_INIT((struct log_t*)&lgwrt, interface_logwriter());

   // prepare
   TEST(0 == initstatic_logwriter(&lgwrt, sizeof(logbuf), logbuf));
   TEST(0 == new_threadstack(&tls, &initlog, S, 0, 0));

   // TEST static_memory_size
   TEST(S == static_memory_size());
   TEST(S != 0);
   TEST(S <  1024);
   TEST(0 == S % KONFIG_MEMALIGN);

   // TEST alloc_static_memory
   TEST(0 == alloc_static_memory(&maincontext, tls, &mblock))
   // check params
   TEST(mblock.addr == maincontext.staticmemblock);
   TEST(mblock.size == sizestatic_threadstack(tls));
   TEST(mblock.addr >  (uint8_t*) tls);
   TEST(mblock.size == S);

   // TEST free_static_memory: double free
   for (unsigned i=0; i<2; ++i) {
      TEST(0 == free_static_memory(&maincontext, tls))
      // check params
      TEST(0 == maincontext.staticmemblock);
      TEST(0 == sizestatic_threadstack(tls));
   }

   // TEST alloc_static_memory: simulated ERROR
   mblock = (memblock_t) memblock_FREE;
   init_testerrortimer(&s_maincontext_errtimer, 1, 4);
   TEST(4 == alloc_static_memory(&maincontext, tls, &mblock))
   // check params
   TEST(0 == maincontext.staticmemblock);
   TEST(0 == sizestatic_threadstack(tls));
   TEST( isfree_memblock(&mblock));

   // TEST free_static_memory: simulated ERROR
   TEST(0 == alloc_static_memory(&maincontext, tls, &mblock))
   init_testerrortimer(&s_maincontext_errtimer, 1, 4);
   TEST(4 == free_static_memory(&maincontext, tls))
   // check params
   TEST(0 == maincontext.staticmemblock);
   TEST(0 == sizestatic_threadstack(tls));

   // reset
   TEST(0 == delete_threadstack(&tls, &initlog));
   freestatic_logwriter(&lgwrt);

   return 0;
ONERR:
   delete_threadstack(&tls, &initlog);
   freestatic_logwriter(&lgwrt);
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
   maincontext_t     maincontext = maincontext_INIT_STATIC;
   const uint16_t    I        = 7;
   const size_t      S        = sizestatic_threadstack(self_threadstack());
   logwriter_t       lgwrt    = logwriter_FREE;
   ilog_t            initlog  = iobj_INIT((struct log_t*)&lgwrt, interface_logwriter());
   const char *      argv[4]  = { "progname", 0, 0, 0 };
   uint8_t           logbuf[log_config_MINSIZE];

   // prepare
   TEST(0 == check_isstatic(&g_maincontext));
   TEST(0 == initstatic_logwriter(&lgwrt, sizeof(logbuf), logbuf));

   // TEST maincontext_INIT_STATIC
   TEST(1 == isstatic_maincontext(&maincontext));

   // TEST init_maincontext
   TEST(0 == init_maincontext(maincontext_DEFAULT, lengthof(argv), argv));
   TEST(1 == g_maincontext.type);
   TEST(0 != g_maincontext.staticmemblock);
   TEST(4 == g_maincontext.argc);
   TEST(argv == g_maincontext.argv);
   TEST(0 == strcmp("progname", g_maincontext.progname));
   TEST(I == g_maincontext.initcount);
   TEST(0 != g_maincontext.syslogin);
   TEST(0 != g_maincontext.signals);
   TEST(0 != g_maincontext.error.stroffset);
   TEST(0 != g_maincontext.error.strdata);
   TEST(S == sizestatic_threadstack(self_threadstack()) - static_memory_size());

   // TEST free_maincontext: double free
   for (unsigned tc=0; tc<2; ++tc) {
      TEST(0 == free_maincontext());
      TEST(0 == check_isstatic(&g_maincontext));
   }

   // TEST init_maincontext: different types and arguments
   maincontext_e mainmode[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   static_assert(lengthof(mainmode) == maincontext__NROF-1, "test all modes except maincontext_STATIC");
   for (unsigned i=0; i < lengthof(mainmode); ++i) {
      for (unsigned argc=0; argc < lengthof(argv); ++argc) {
         // test
         TEST(0 == init_maincontext(mainmode[i], (int)argc, argc?argv:0));
         // check
         TEST(g_maincontext.type == mainmode[i]);
         TEST(g_maincontext.staticmemblock != 0);
         TEST(g_maincontext.initcount == I);
         TEST(g_maincontext.argc == (int)argc);
         TEST(g_maincontext.argv == (argc?argv:0));
         TEST(g_maincontext.progname != 0);
         TEST(strcmp(g_maincontext.progname, argc?"progname":"") == 0);
         // check services
         TEST(0 != g_maincontext.syslogin);
         TEST(0 != g_maincontext.signals);
         TEST(0 != strcmp("C", current_locale()));
         TEST(sizestatic_threadstack(self_threadstack()) == extsize_maincontext() + extsize_threadcontext());
         // check threadcontext
         TEST(0 == tcontext_maincontext()->initcount); // not initialized

         // reset
         TEST(0 == free_maincontext());
         TEST(0 == check_isstatic(&g_maincontext));
      }
   }

   // TEST free_maincontext: EMEMLEAK
   TEST(0 == init_maincontext(maincontext_DEFAULT, 1, argv));
   memblock_t mblock;
   TEST(0 == allocstatic_threadstack(self_threadstack(), &initlog, 1, &mblock)); // alloc additional mem
   // test
   uint8_t* staticmemblock = self_maincontext()->staticmemblock;
   TEST( EMEMLEAK == free_maincontext());
   // check
   TEST( 0 == self_maincontext()->staticmemblock);
   // reset
   TEST(0 == freestatic_threadstack(self_threadstack(), &initlog, &mblock));
   self_maincontext()->staticmemblock = staticmemblock;
   TEST(0 == free_static_memory(self_maincontext(), self_threadstack()));
   TEST(0 == check_isstatic(&g_maincontext));

   // TEST free_shared_services: initcount == 0
   maincontext_t maincontext2;
   memset(&maincontext, 255, sizeof(maincontext));
   memset(&maincontext2, 255, sizeof(maincontext2));
   maincontext.initcount = 0;
   maincontext2.initcount = 0;
   TEST(0 == free_shared_services(&maincontext));
   TEST(0 == memcmp(&maincontext, &maincontext2, sizeof(maincontext)));

   // TEST init_maincontext: simulated error
   for (unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_maincontext_errtimer, i, (int)(3+i));
      int err = init_maincontext(maincontext_DEFAULT, 0, 0);
      if (!err) {
         free_testerrortimer(&s_maincontext_errtimer);
         TESTP( 9==i, "i:%d",i);
         break;
      }
      TEST(3+i == (unsigned)err);
      TEST(0 == check_isstatic(&g_maincontext));
   }
   TEST(0 == free_maincontext());

   // TEST free_maincontext: simulated error
   for (unsigned i = 1; ; ++i) {
      TEST(0 == init_maincontext(maincontext_DEFAULT, 1, argv));
      init_testerrortimer(&s_maincontext_errtimer, i, (int)(1+i));
      int err = free_maincontext();
      if (!err) {
         free_testerrortimer(&s_maincontext_errtimer);
         TESTP( 9==i, "i:%d",i);
         break;
      }
      TEST(1+i == (unsigned)err);
      TEST(0 == check_isstatic(&g_maincontext));
   }

   // check no logs written
   {
      uint8_t *lb;
      size_t   logsize;
      GETBUFFER_LOG(&initlog, log_channel_ERR, &lb, &logsize);
      TEST( 0 == logsize);
   }

   // unprepare
   TEST(0 == check_isstatic(&g_maincontext));
   freestatic_logwriter(&lgwrt);

   return 0;
ONERR:
   free_maincontext();
   freestatic_logwriter(&lgwrt);
   return EINVAL;
}

static int test_query(void)
{
   maincontext_t maincontext = maincontext_INIT_STATIC;

   // TEST isstatic_maincontext
   TEST(1 == isstatic_maincontext(&maincontext));
   // private
   maincontext.type = (maincontext_e)1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.type = maincontext_STATIC;
   maincontext.staticmemblock = (void*)1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.staticmemblock = 0;
   maincontext.initcount = 1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.initcount = 0;
   // services
   TEST(1 == isstatic_maincontext(&maincontext));
   maincontext.sysinfo.pagesize_vm = 1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.sysinfo.pagesize_vm = 0;
   maincontext.syslogin = (void*)1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.syslogin = 0;
   maincontext.signals = (void*)1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.signals = 0;
   maincontext.error.stroffset = 0;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.error.stroffset = g_errorcontext_stroffset;
   maincontext.error.strdata = 0;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.error.strdata = g_errorcontext_strdata;
   // arguments
   TEST(1 == isstatic_maincontext(&maincontext));
   maincontext.progname = (void*)1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.progname = (void*)0;
   maincontext.argc     = 1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.argc = 0;
   maincontext.argv = (void*)1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.argv = (void*)0;
   TEST(1 == isstatic_maincontext(&maincontext));

   // TEST extsize_maincontext
   for (unsigned i=0; i<2; ++i) {
      TEST(extsize_maincontext() != 0);
      TEST(extsize_maincontext() == static_memory_size());
   }

   return 0;
ONERR:
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
   TEST(0 == check_isstatic(&g_maincontext));
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
      TEST(0 == check_isstatic(&g_maincontext));
   }

   // TEST initrun_maincontext: return value
   for (uintptr_t i = 0; i < 10; ++i) {
      s_is_called = 0;
      // test
      s_i = i; // argument to test_init_returncode
      TEST(i == (unsigned) initrun_maincontext(s_mainmode[i%lengthof(s_mainmode)], &test_init_returncode, 0, 0));
      TEST(1 == s_is_called);

      // check free_syscontext, free_maincontext called
      TEST(0 == check_isstatic(&g_maincontext));
   }

   // TEST initrun_maincontext: EINVAL (but initlog setup ==> error log is written)
   TEST(0 == check_noerror_logged(&errpipe));
   TEST(EINVAL == initrun_maincontext(maincontext_STATIC, &test_init_returncode, 0, 0));
   TEST(0 == check_isstatic(&g_maincontext));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext__NROF, &test_init_returncode, 0, 0));
   TEST(0 == check_isstatic(&g_maincontext));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext_DEFAULT, &test_init_returncode, -1, 0));
   TEST(0 == check_isstatic(&g_maincontext));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext_DEFAULT, &test_init_returncode, 1, 0));
   TEST(0 == check_isstatic(&g_maincontext));
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
   if (test_staticmemory())   goto ONERR;
   if (test_helper())         goto ONERR;
   if (test_init())           goto ONERR;
   if (test_query())          goto ONERR;
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

   // test initialized context

   TEST(0 == init_resourceusage(&usage));
   if (test_querymacros())    goto ONERR;
   if (test_staticmemory())   goto ONERR;
   if (test_helper())         goto ONERR;
   if (test_query())          goto ONERR;
   TEST( 0 == same_resourceusage(&usage));
   TEST( 0 == free_resourceusage(&usage));

   // prepare
   memcpy(&oldmc, &g_maincontext, sizeof(oldmc));
   TEST(0 <  oldstderr);
   TEST(0 == init_pipe(&errpipe));
   TEST(STDERR_FILENO == dup2(errpipe.write, STDERR_FILENO));

   // test uninitialized context

   freeonce_locale();
   free_signals(g_maincontext.signals);
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
