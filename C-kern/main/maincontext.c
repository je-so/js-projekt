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
#include "C-kern/api/io/log/logbuffer.h"
#include "C-kern/api/io/log/logcontext.h"
#include "C-kern/api/io/log/logwriter.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/platform/syslogin.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/task/thread_stack.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/config/initmain)WHERE(header-name!='')
#include "C-kern/api/io/log/logcontext.h"
#include "C-kern/api/string/clocale.h"
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
#include "C-kern/api/platform/task/process.h"
#endif



// section: maincontext_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_maincontext_errtimer
 * Simulates an error in <maincontext_t.init_maincontext>. */
static test_errortimer_t   s_maincontext_errtimer = test_errortimer_FREE;
#endif

// group: static memory

static inline uint16_t static_memory_size(void)
{
   uint16_t memorysize = sizeof(maincontext_t)
// TEXTDB:SELECT("         + sizeof("objtype")")FROM(C-kern/resource/config/initmain)WHERE(inittype=="object")
         + sizeof(logcontext_t)
         + sizeof(clocale_t)
         + sizeof(signals_t)
         + sizeof(syslogin_t)
// TEXTDB:END
     ;
   return alignpower2_int(memorysize, KONFIG_MEMALIGN);
}

/* function: alloc_static_memory
 * Allokiert statischen Speicher für alle von <maincontext_t> zu initialisierenden Objekte.
 * */
static inline int alloc_static_memory(maincontext_t** maincontext, thread_stack_t* tstack, ilog_t* initlog)
{
   int err;
   memblock_t mblock;

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = allocstatic_threadstack(tstack, initlog, static_memory_size(), &mblock);
   }
   if (err) goto ONERR;

   *maincontext = (maincontext_t*)mblock.addr;

   return 0;
ONERR:
   return err;
}

/* function: free_static_memory
 * Gibt statischen Speicher für alle im <maincontext_t> initialisierten Objekt frei.
 * */
static inline int free_static_memory(maincontext_t** maincontext, thread_stack_t* tstack, ilog_t* initlog)
{
   int err;

   if (*maincontext) {
      memblock_t mblock = memblock_INIT(static_memory_size(), (uint8_t*)*maincontext);

      *maincontext = 0;

      err = freestatic_threadstack(tstack, initlog, &mblock);
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
static const char* get_progname(const char* argv0)
{
   const char* last = strrchr(argv0, '/');
   return (last ? last + 1 : argv0);
}

static inline void set_args_maincontext(/*out*/maincontext_t* maincontext, int argc, const char** argv)
{
   maincontext->progname = get_progname(argc ? argv[0] : "");
   maincontext->argc     = argc;
   maincontext->argv     = argv;
}

static inline void clear_args_maincontext(/*out*/maincontext_t* maincontext)
{
   maincontext->progname = 0;
   maincontext->argc = 0;
   maincontext->argv = 0;
}

static inline void initlog_maincontext(
      /*out*/logcontext_t* logcontext,
      /*out*/ilog_t*       initlog,
      /*out*/logwriter_t*  logwriter,
      size_t   size,
      uint8_t  logbuffer[size])
{
   initstatic_logcontext(logcontext);
   if (initstatic_logwriter(logwriter, logcontext, size, logbuffer) || PROCESS_testerrortimer(&s_maincontext_errtimer, 0)) {
      freestatic_logcontext(logcontext);
      #define ERRSTR "FATAL ERROR: initlog_maincontext(): call to initstatic_logwriter failed\n"
      ssize_t ignore = write(sys_iochannel_STDERR, ERRSTR, sizeof(ERRSTR)-1);
      (void) ignore;
      #undef ERRSTR
      abort();
   }
   *initlog = (ilog_t) iobj_INIT((struct log_t*)logwriter, interface_logwriter());
}

static inline void freelog_maincontext(logcontext_t* lc, ilog_t* initlog)
{
   freestatic_logcontext(lc);
   if (!isfree_iobj(initlog)) {
      freestatic_logwriter((logwriter_t*)initlog->object);
      *initlog = (ilog_t) iobj_FREE;
   }
}

// group: lifetime

int newstatic_maincontext(/*out*/maincontext_t** maincontext, struct thread_stack_t* tstack, ilog_t* initlog)
{
   int err;

   if (!PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = alloc_static_memory(maincontext, tstack, initlog);
   }
   if (err) goto ONERR;

   **maincontext = (maincontext_t) maincontext_INIT_STATIC;

   return 0;
ONERR:
   TRACE_LOG(initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

int deletestatic_maincontext(maincontext_t** maincontext, struct thread_stack_t* tstack, ilog_t* initlog)
{
   int err;

   if (*maincontext) {
      err = free_static_memory(maincontext, tstack, initlog);
      (void)PROCESS_testerrortimer(&s_maincontext_errtimer, &err);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACE_LOG(initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_FREE_RESOURCE_ERRLOG, err);
   return err;
}

int free_maincontext(maincontext_t* maincontext)
{
   int err,err2;

   err = 0;

   switch (maincontext->initcount) {
   default: assert(false && "initcount out of bounds");
            break;
// TEXTDB:SELECT(\n"   case ("row-id"):"\n"            err2 = free"(if (inittype=='once') ("once"))"_"module"("(if (parameter!="") ((if (inittype!="object") ("&"))"maincontext->"parameter))");"\n"            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);"\n"            if (err2) err = err2;"(if (inittype=='object') (\n"            maincontext->"parameter" = 0;")))FROM(C-kern/resource/config/initmain)DESCENDING

   case (6):
            err2 = freeonce_X11();
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;

   case (5):
            err2 = free_syslogin(maincontext->syslogin);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
            maincontext->syslogin = 0;

   case (4):
            err2 = free_signals(maincontext->signals);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
            maincontext->signals = 0;

   case (3):
            err2 = free_clocale(maincontext->locale);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
            maincontext->locale = 0;

   case (2):
            err2 = free_logcontext(maincontext->logcontext);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
            maincontext->logcontext = 0;

   case (1):
            err2 = free_syscontext(&maincontext->sysinfo);
            (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err2);
            if (err2) err = err2;
// TEXTDB:END
   case 0:  break;
   }

   maincontext->type = maincontext_STATIC;
   maincontext->initcount = 0;
   clear_args_maincontext(maincontext);

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_maincontext(/*out*/maincontext_t* maincontext, const maincontext_e type, int argc, const char* argv[])
{
   int err;
   memblock_t mblock = memblock_INIT(static_memory_size(), (uint8_t*)maincontext);

   if (  type <= maincontext_STATIC
      || type >= maincontext__NROF) {
      err = EINVAL;
      goto ONERR;
   }

   maincontext->type = type;
   maincontext->initcount = 0;
   set_args_maincontext(maincontext, argc, argv);
   mblock.addr += sizeof(maincontext_t);
   mblock.size -= sizeof(maincontext_t);

// TEXTDB:SELECT(\n(if (inittype=='object') ("   assert( sizeof("objtype") <= mblock.size);"\n))"   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {"\n"      err = init"(if (inittype=='once') ("once"))"_"module"("(if (parameter!=""&&inittype!="object") ("&maincontext->"parameter))(if (inittype=='object') ("("objtype"*) mblock.addr"))");"\n"   }"\n"   if (err) goto ONERR;"\n"   ++ maincontext->initcount;"(if (inittype=='object') (\n"   maincontext->"parameter" = ("objtype"*) mblock.addr;"\n"   mblock.addr += sizeof("objtype");"\n"   mblock.size -= sizeof("objtype");")))FROM(C-kern/resource/config/initmain)

   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_syscontext(&maincontext->sysinfo);
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;

   assert( sizeof(logcontext_t) <= mblock.size);
   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_logcontext((logcontext_t*) mblock.addr);
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;
   maincontext->logcontext = (logcontext_t*) mblock.addr;
   mblock.addr += sizeof(logcontext_t);
   mblock.size -= sizeof(logcontext_t);

   assert( sizeof(clocale_t) <= mblock.size);
   if (! PROCESS_testerrortimer(&s_maincontext_errtimer, &err)) {
      err = init_clocale((clocale_t*) mblock.addr);
   }
   if (err) goto ONERR;
   ++ maincontext->initcount;
   maincontext->locale = (clocale_t*) mblock.addr;
   mblock.addr += sizeof(clocale_t);
   mblock.size -= sizeof(clocale_t);

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

   (void) PROCESS_testerrortimer(&s_maincontext_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   (void) free_maincontext(maincontext);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initrun_maincontext(maincontext_e type, mainthread_f main_thread, int argc, const char** argv)
{
   int err;
   logwriter_t    lgwrt;
   uint8_t        logbuffer[log_config_MINSIZE];
   logcontext_t   logcontext = logcontext_FREE;
   ilog_t         initlog = iobj_FREE;

   // setup initlog which is used for error logging during initialization
   initlog_maincontext(&logcontext, &initlog, &lgwrt, sizeof(logbuffer), logbuffer);

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
   err = runmain_thread(&retcode, main_thread, &initlog, type, argc, argv);
   if (!err) err = retcode;

   freelog_maincontext(&logcontext, &initlog);

   return err;
ONERR:
   TRACE_LOG(&initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   freelog_maincontext(&logcontext, &initlog);
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
            && 0 == maincontext->initcount
            // shared services
            && 1 == isfree_syscontext(&maincontext->sysinfo)
            && 0 == maincontext->syslogin
            && 0 == maincontext->signals
            && 0 == maincontext->logcontext
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

/* Checks address of newly allocated standard maincontext_t. */
static int check_addr(maincontext_t* maincontext)
{
   threadcontext_t* tcontext = context_thread(self_thread());
   uint8_t* addr = tcontext->staticdata;
   addr -= extsize_maincontext();

   TEST(addr == (void*)maincontext);

   return 0;
ONERR:
   return EINVAL;
}

/* Checks address of newly allocated 2nd maincontext_t (for test purposes). */
static int check_addr2(maincontext_t* maincontext)
{
   threadcontext_t* tcontext = context_thread(self_thread());
   uint8_t* addr = tcontext->staticdata;
   addr += extsize_threadcontext();

   TEST(addr == (void*)maincontext);

   return 0;
ONERR:
   return EINVAL;
}

static int check_isinit(maincontext_t* maincontext, maincontext_e type, int argc, const char* argv[], const char* progname, size_t static_size)
{
   const uint16_t I = 6;
   const size_t   S = static_size;

   // check memory address
   TEST( maincontext == (maincontext_t*)(context_thread(self_thread())->staticdata + extsize_threadcontext())
         || maincontext == (maincontext_t*)(context_thread(self_thread())->staticdata - extsize_maincontext()));
   // check private
   TEST(maincontext->type == type);
   TEST(maincontext->initcount == I);
   // check args
   TEST(maincontext->argc == argc);
   TEST(maincontext->argv == argv);
   TEST(maincontext->progname != 0);
   TEST(strcmp(maincontext->progname, progname) == 0);
   // check services
   TEST(1 == isvalid_syscontext(&maincontext->sysinfo));
   TEST(0 != maincontext->logcontext);
   TEST(0 != maincontext->locale);
   TEST(0 != maincontext->signals);
   TEST(0 != maincontext->syslogin);
   TEST(S == sizestatic_threadstack(self_threadstack()));

   return 0;
ONERR:
   return EINVAL;
}

static int check_isinit_tc(maincontext_t* maincontext, threadcontext_t* tc)
{
   TEST(maincontext == tc->maincontext);
   TEST(0 != tc->thread_id);
   TEST(0 != tc->initcount);
   TEST(0 != tc->staticdata);

   return 0;
ONERR:
   return EINVAL;
}

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
   TEST(maincontext_STATIC == maincontext->type);
   TEST(0 == maincontext->initcount);
   // shared services
   TEST(1 == isfree_syscontext(&maincontext->sysinfo));
   TEST(0 == maincontext->syslogin);
   TEST(0 == maincontext->signals);
   TEST(0 == maincontext->logcontext);
   // program arguments
   TEST(0 == maincontext->progname);
   TEST(0 == maincontext->argc);
   TEST(0 == maincontext->argv);
   // public alternative yields the same result
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
   TEST( -1 == len && EAGAIN == errno);

   return 0;
ONERR:
   return EINVAL;
}

static int test_querymacros(void)
{
   maincontext_t* maincontext = self_maincontext();

   // TEST error_maincontext
   TEST(&logcontext_maincontext()  == &maincontext->logcontext);

   // TEST log_maincontext
   TEST(&log_maincontext()         == &tcontext_maincontext()->log);

   // TEST log_maincontext
   TEST(&objectcache_maincontext() == &tcontext_maincontext()->objectcache);

   // TEST pagecache_maincontext
   TEST(&pagecache_maincontext()   == &tcontext_maincontext()->pagecache);

   // TEST progname_maincontext
   TEST(&progname_maincontext()    == &maincontext->progname);

   // TEST self_maincontext
   TEST(self_maincontext()         == maincontext);

   // TEST syslogin_maincontext
   TEST(&syslogin_maincontext()    == &maincontext->syslogin);

   // TEST sysinfo_maincontext
   TEST(&sysinfo_maincontext()     == &maincontext->sysinfo);

   // TEST syncrunner_maincontext
   TEST(&syncrunner_maincontext()  == &tcontext_maincontext()->syncrunner);

   thread_stack_t* tls = self_threadstack();

   // TEST tcontext_maincontext
   TEST(tcontext_maincontext()     == context_threadstack(tls));

   // TEST threadid_maincontext
   TEST(&threadid_maincontext()    == &context_threadstack(tls)->thread_id);

   // TEST type_maincontext
   TEST(&type_maincontext()        == &maincontext->type);

   return 0;
ONERR:
   return EINVAL;
}

static int test_staticmemory(void)
{
   maincontext_t *maincontext = 0;
   thread_stack_t*tstack = 0;
   const size_t   S        = static_memory_size();
   uint8_t        logbuf[log_config_MINSIZE];
   logcontext_t   logctxt  = logcontext_FREE;
   logwriter_t    lgwrt    = logwriter_FREE;
   ilog_t         initlog  = iobj_INIT((struct log_t*)&lgwrt, interface_logwriter());

   // prepare
   initstatic_logcontext(&logctxt);
   TEST(0 == initstatic_logwriter(&lgwrt, &logctxt, sizeof(logbuf), logbuf));
   TEST(0 == new_threadstack(&tstack, &initlog, S, 0, 0));

   // TEST static_memory_size
   TEST(S == static_memory_size());
   TEST(S != 0);
   TEST(S <  1024);
   TEST(0 == S % KONFIG_MEMALIGN);

   // TEST alloc_static_memory
   TEST(0 == alloc_static_memory(&maincontext, tstack, &initlog))
   // check params
   TEST((uint8_t*) tstack < (uint8_t*) maincontext);
   TEST(S == sizestatic_threadstack(tstack));

   // TEST free_static_memory: double free
   for (unsigned i=0; i<2; ++i) {
      TEST(0 == free_static_memory(&maincontext, tstack, &initlog))
      // check params
      TEST(0 == maincontext);
      TEST(0 == sizestatic_threadstack(tstack));
   }

   // TEST alloc_static_memory: simulated ERROR
   init_testerrortimer(&s_maincontext_errtimer, 1, 4);
   TEST(4 == alloc_static_memory(&maincontext, tstack, &initlog))
   // check params
   TEST(0 == maincontext);
   TEST(0 == sizestatic_threadstack(tstack));

   // TEST free_static_memory: simulated ERROR
   TEST(0 == alloc_static_memory(&maincontext, tstack, &initlog))
   init_testerrortimer(&s_maincontext_errtimer, 1, 4);
   TEST(4 == free_static_memory(&maincontext, tstack, &initlog))
   // check params
   TEST(0 == maincontext);
   TEST(0 == sizestatic_threadstack(tstack));

   // reset
   TEST(0 == delete_threadstack(&tstack, &initlog));
   freestatic_logwriter(&lgwrt);
   freestatic_logcontext(&logctxt);

   return 0;
ONERR:
   delete_threadstack(&tstack, &initlog);
   freestatic_logwriter(&lgwrt);
   freestatic_logcontext(&logctxt);
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
   ilog_t  initlog = iobj_FREE;
   logwriter_t  lw = logwriter_FREE;
   logcontext_t lc = logcontext_FREE;
   uint8_t      lb[log_config_MINSIZE];
   initlog_maincontext(&lc, &initlog, &lw, log_config_MINSIZE, lb);
   TEST(0 == isfree_logcontext(&lc));
   TEST(0 != errstr_logcontext(&lc, 1));
   TEST(initlog.object == (struct log_t*)&lw);
   TEST(initlog.iimpl  == interface_logwriter());
   TEST(lw.addr == lb);
   TEST(lw.size == log_config_MINSIZE);

   // TEST freelog_maincontext
   freelog_maincontext(&lc, &initlog);
   TEST(isfree_logcontext(&lc));
   TEST(isfree_iobj(&initlog));
   TEST(isfree_logwriter(&lw));

   return 0;
ONERR:
   return EINVAL;
}

static int test_init(void)
{
   maincontext_t* maincontext=0;
   maincontext_t* old=0;
   logcontext_t   logctxt  = logcontext_FREE;
   logwriter_t    lgwrt    = logwriter_FREE;
   ilog_t         initlog  = iobj_INIT((struct log_t*)&lgwrt, interface_logwriter());
   const char *   argv[4]  = { "v/progname", "p/1", "p/2", "p/3" };
   uint8_t        logbuf[log_config_MINSIZE];
   const size_t   S = sizestatic_threadstack(self_threadstack()) + static_memory_size();

   // prepare
   initstatic_logcontext(&logctxt);
   TEST(0 == initstatic_logwriter(&lgwrt, &logctxt, sizeof(logbuf), logbuf));

   // TEST newstatic_maincontextnewstatic_maincontext
   TEST(0 == newstatic_maincontext(&maincontext, self_threadstack(), &initlog));
   TEST(0 == check_addr2(maincontext));
   TEST(0 == check_isstatic(maincontext));
   TEST(S == sizestatic_threadstack(self_threadstack()));

   // TEST deletestatic_maincontext
   for (unsigned i=0; i<2; ++i) {
      TEST( 0 == deletestatic_maincontext(&maincontext, self_threadstack(), &initlog));
      TEST( 0 == maincontext);
      TEST( S == sizestatic_threadstack(self_threadstack()) + static_memory_size());
   }

   // TEST newstatic_maincontext: simulated error
   init_testerrortimer(&s_maincontext_errtimer, 1, ENOMEM);
   TEST(ENOMEM == newstatic_maincontext(&maincontext, self_threadstack(), &initlog));
   TEST(0 == maincontext);

   // TEST deletestatic_maincontext: simulated error
   TEST(0 == newstatic_maincontext(&maincontext, self_threadstack(), &initlog));
   init_testerrortimer(&s_maincontext_errtimer, 1, EINVAL);
   TEST( EINVAL == deletestatic_maincontext(&maincontext, self_threadstack(), &initlog));
   TEST( 0 == maincontext);
   TEST( S == sizestatic_threadstack(self_threadstack()) + static_memory_size());

   // TEST deletestatic_maincontext: EMEMLEAK
   TEST(0 == newstatic_maincontext(&maincontext, self_threadstack(), &initlog));
   TEST(0 == check_addr2(maincontext));
   memblock_t mblock;
   TEST(0 == allocstatic_threadstack(self_threadstack(), &initlog, 1, &mblock)); // alloc additional mem
   // test
   old = maincontext;
   TEST( EMEMLEAK == deletestatic_maincontext(&maincontext, self_threadstack(), &initlog));
   TEST( S == sizestatic_threadstack(self_threadstack())-KONFIG_MEMALIGN);
   // check
   TEST( 0 == maincontext);
   // reset
   TEST(0 == freestatic_threadstack(self_threadstack(), &initlog, &mblock));
   maincontext = old;
   TEST(0 == deletestatic_maincontext(&maincontext, self_threadstack(), &initlog));
   TEST(S == sizestatic_threadstack(self_threadstack()) + static_memory_size());

   // prepare1
   TEST(0 == newstatic_maincontext(&maincontext, self_threadstack(), &initlog));

   // TEST init_maincontext
   TEST(0 == check_addr2(maincontext));
   TEST(0 == init_maincontext(maincontext, maincontext_DEFAULT, lengthof(argv), argv));
   TEST(0 == check_isinit(maincontext, maincontext_DEFAULT, lengthof(argv), argv, argv[0]+2, S));

   // TEST free_maincontext: double free
   for (unsigned i=0; i<2; ++i) {
      TEST(0 == free_maincontext(maincontext));
      TEST(0 == check_isstatic(maincontext));
   }

   // TEST init_maincontext: different types and arguments
   maincontext_e ttype[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   static_assert(lengthof(ttype) == maincontext__NROF-1, "test all modes except maincontext_STATIC");
   for (unsigned i=0; i < lengthof(ttype); ++i) {
      for (unsigned argc=0; argc < lengthof(argv); ++argc) {
         // test
         TEST(0 == init_maincontext(maincontext, ttype[i], (int)argc, argc?argv:0));
         // check
         TEST(0 == check_isinit(maincontext, ttype[i], (int)argc, argc?argv:0, argc?argv[0]+2:"", S));
         // reset
         TEST(0 == free_maincontext(maincontext));
         TEST(0 == check_isstatic(maincontext));
      }
   }

   // TEST free_maincontext: initcount == 0
   maincontext_t maincontext1;
   maincontext_t maincontext2;
   memset(&maincontext1, 255, sizeof(maincontext1));
   memset(&maincontext2, 255, sizeof(maincontext2));
   maincontext1.initcount = 0;
   maincontext2.initcount = 0;
   maincontext2.type = 0;
   clear_args_maincontext(&maincontext2);
   TEST(0 == free_maincontext(&maincontext1));
   TEST(0 == memcmp(&maincontext1, &maincontext2, sizeof(maincontext1)));

   // TEST init_maincontext: EINVAL
   TEST(EINVAL == init_maincontext(maincontext, maincontext_STATIC, 1, argv));
   TEST(0 == check_isstatic(maincontext));
   TEST(EINVAL == init_maincontext(maincontext, maincontext__NROF, 1, argv));
   TEST(0 == check_isstatic(maincontext));

   // TEST init_maincontext: simulated error
   for (unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_maincontext_errtimer, i, (int)(3+i));
      int err = init_maincontext(maincontext, maincontext_DEFAULT, 0, 0);
      if (!err) {
         free_testerrortimer(&s_maincontext_errtimer);
         TESTP( 8==i, "i:%d",i);
         break;
      }
      TEST(3+i == (unsigned)err);
      TEST(0 == check_isstatic(maincontext));
   }
   TEST(0 == free_maincontext(maincontext));

   // TEST free_maincontext: simulated error
   for (unsigned i=1; ; ++i) {
      TEST(0 == init_maincontext(maincontext, maincontext_DEFAULT, 1, argv));
      init_testerrortimer(&s_maincontext_errtimer, i, (int)(1+i));
      int err = free_maincontext(maincontext);
      TEST(0 == check_isstatic(maincontext));
      if (!err) {
         free_testerrortimer(&s_maincontext_errtimer);
         TESTP( 7==i, "i:%d",i);
         break;
      }
      TEST(1+i == (unsigned)err);
   }

   // reset1/reset
   TEST(0 == deletestatic_maincontext(&maincontext, self_threadstack(), &initlog));
   freestatic_logwriter(&lgwrt);
   freestatic_logcontext(&logctxt);

   return 0;
ONERR:
   if (maincontext) {
      free_maincontext(maincontext);
      deletestatic_maincontext(&maincontext, self_threadstack(), &initlog);
   }
   freestatic_logwriter(&lgwrt);
   freestatic_logcontext(&logctxt);
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
   maincontext.logcontext = (void*)1;
   TEST(0 == isstatic_maincontext(&maincontext));
   maincontext.logcontext = 0;
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

static maincontext_e s_init_type;
static int           s_init_argc;
static const char**  s_init_argv;
static const char*   s_init_progname;
static int           s_init_retcode;
static unsigned      s_init_iscalled;

static int check_initrun(maincontext_t* maincontext)
{
   const size_t static_size = extsize_maincontext() + extsize_threadcontext();
   ++ s_init_iscalled;

   TEST(0 == check_addr(maincontext));
   TEST(0 == check_isinit(maincontext, s_init_type, s_init_argc, s_init_argv, s_init_progname, static_size));
   TEST(0 == check_isinit_tc(maincontext, context_thread(self_thread())));

   return s_init_retcode;
ONERR:
   return EINVAL;
}

static int test_initrun(void)
{
   pipe_t  errpipe = pipe_FREE;
   int     oldstderr = dup(STDERR_FILENO);
   process_t child = process_FREE;

   // prepare
   TEST(0 <  oldstderr);
   TEST(0 == init_pipe(&errpipe));
   TEST(STDERR_FILENO == dup2(errpipe.write, STDERR_FILENO));

   // TEST initrun_maincontext: function called with initialized maincontext/threadcontext
   maincontext_e maintype[] = { maincontext_DEFAULT, maincontext_CONSOLE };
   const char* argv[4] = { "v/1", "v/2", "v/3", "v/4" };
   s_init_iscalled = 0;
   for (unsigned i=0; i<4; ++i) {
      s_init_type = maintype[i%lengthof(maintype)];
      s_init_argc = (int)(4 - i);
      s_init_argv = argv + i;
      s_init_progname = argv[i] + 2;
      s_init_retcode = 0;
      // test
      TEST(0 == initrun_maincontext(s_init_type, &check_initrun, s_init_argc, s_init_argv));
      TEST(i == s_init_iscalled - 1);
   }

   // TEST initrun_maincontext: return value
   s_init_iscalled = 0;
   for (unsigned i = 0; i < 10; ++i) {
      s_init_type = maintype[i%lengthof(maintype)];
      s_init_argc = 4;
      s_init_argv = argv;
      s_init_progname = argv[0] + 2;
      s_init_retcode = (int) i;
      // test
      TEST(i == (unsigned) initrun_maincontext(s_init_type, &check_initrun, s_init_argc, s_init_argv));
      TEST(i == s_init_iscalled - 1);
   }

   // TEST initrun_maincontext: EINVAL (but initlog setup ==> error log is written)
   TEST(0 == check_noerror_logged(&errpipe));
   TEST(EINVAL == initrun_maincontext(maincontext_STATIC, &check_initrun, 0, 0));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext__NROF, &check_initrun, 0, 0));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext_DEFAULT, &check_initrun, -1, 0));
   TEST(0 == check_error_logged(&errpipe, oldstderr));
   TEST(EINVAL == initrun_maincontext(maincontext_DEFAULT, &check_initrun, 1, 0));
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

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_FREE;
   pipe_t          errpipe = pipe_FREE;
   int             oldstderr = dup(STDERR_FILENO);

   // prepare
   TEST(self_thread()->ismain);
   TEST(0 <  oldstderr);
   TEST(0 == init_pipe(&errpipe));
   TEST(STDERR_FILENO == dup2(errpipe.write, STDERR_FILENO));

   TEST(0 == init_resourceusage(&usage));
   if (test_querymacros())    goto ONERR;
   if (test_staticmemory())   goto ONERR;
   if (test_helper())         goto ONERR;
   if (test_init())           goto ONERR;
   if (test_query())          goto ONERR;
   if (test_initrun())        goto ONERR;
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
   TEST(-1 == rsize && EAGAIN == errno);
   TESTP(400 < sum, "sum:%zd", sum);

   // reset
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO));
   TEST(0 == free_pipe(&errpipe));
   TEST(0 == free_iochannel(&oldstderr))

   return 0;
ONERR:
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
