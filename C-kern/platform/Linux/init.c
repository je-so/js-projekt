/* title: PlatformInit Linux

   Implements <PlatformInit> in a Linux specific way.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/init.h
    Header file <PlatformInit>.

   file: C-kern/platform/Linux/init.c
    Implementation file <PlatformInit Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/init.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/task/thread_localstore.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/platform/task/process.h"
#endif


// struct: syscontext_t
struct syscontext_t;

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_syscontext_errtimer
 * Used to simulate an error in <initrun_syscontext>. */
static test_errortimer_t   s_syscontext_errtimer = test_errortimer_FREE;
#endif

// group: helper

/* function: callmain_platform
 * Calls the main threads main function.
 * An error is written to STDERR if the main thread
 * aborts. */
static void callmain_platform(void)
{
   thread_t * thread = self_thread();

   bool is_abort;
   if (  setcontinue_thread(&is_abort)
         || is_abort) {
      #define ERRSTR1 "initrun_syscontext() "
      #define ERRSTR2 ":" STR(__LINE__) "\naborted\n"
      ssize_t written = write(STDERR_FILENO, ERRSTR1, sizeof(ERRSTR1)-1)
                      + write(STDERR_FILENO, __FILE__, strlen(__FILE__))
                      + write(STDERR_FILENO, ERRSTR2, sizeof(ERRSTR2)-1);
      #undef ERRSTR1
      #undef ERRSTR2
      (void) written;
      abort();
   }

   void*    arg     = mainarg_thread(thread);
   thread_f mainthr = maintask_thread(thread);
   int      retcode = mainthr(arg);

   setreturncode_thread(thread, retcode);
}

static inline int set_scontext(/*out*/struct syscontext_t * scontext)
{
   int err;

   uint32_t pagesize = (uint32_t) sys_pagesize_vm();

   if (! pagesize
      || ! ispowerof2_int(pagesize)
      || sys_pagesize_vm() != pagesize) {
      err = EINVAL;
      goto ONERR;
   }

   // set out param
   scontext->pagesize_vm = pagesize;
   scontext->log2pagesize_vm = log2_int(pagesize);

   return 0;
ONERR:
   // TODO: call init log
   return err;
}

int initrun_syscontext(/*out;err*/struct syscontext_t * scontext, thread_f main_thread, void * main_arg)
{
   int err;
   volatile int linenr; // TODO: remove
   memblock_t threadstack;
   memblock_t signalstack;
   ucontext_t context_caller;
   ucontext_t context_mainthread;
   thread_localstore_t* tls = 0;

   if (scontext->pagesize_vm) {
      // TODO: init log
      return EALREADY;
   }

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_syscontext_errtimer, &err, ONERR);
   err = set_scontext(scontext);
   if (err) goto ONERR;

   linenr = __LINE__;    // TODO: add init log to new_threadlocalstore / remove newmain_threadlocalstore
   ONERROR_testerrortimer(&s_syscontext_errtimer, &err, ONERR);
   err = newmain_threadlocalstore(&tls, &threadstack, &signalstack);
   if (err) goto ONERR;

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_syscontext_errtimer, &err, ONERR);
   stack_t altstack = { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size };
   if (sigaltstack(&altstack, 0)) {
      err = errno;
      goto ONERR;
   }

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_syscontext_errtimer, &err, ONERR);
   if (getcontext(&context_mainthread)) {
      err = errno;
      goto ONERR;
   }

   context_mainthread.uc_link  = &context_caller;
   context_mainthread.uc_stack = (stack_t) { .ss_sp = threadstack.addr, .ss_flags = 0, .ss_size = threadstack.size };
   makecontext(&context_mainthread, &callmain_platform, 0, 0);

   thread_t * thread = thread_threadlocalstore(tls);
   initmain_thread(thread, main_thread, main_arg);

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_syscontext_errtimer, &err, ONERR);
   // start callmain_platform and returns to first context_caller after exit
   if (swapcontext(&context_caller, &context_mainthread)) {
      err = errno;
      goto ONERR;
   }

   int retcode = returncode_thread(thread);

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_syscontext_errtimer, &err, ONERR);
   altstack = (stack_t) { .ss_flags = SS_DISABLE };
   err = sigaltstack(&altstack, 0);
   if (err) {
      err = errno;
      goto ONERR;
   }

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_syscontext_errtimer, &err, ONERR);
   err = deletemain_threadlocalstore(&tls);
   if (err) goto ONERR;

   // mark system context as freed
   *scontext = (syscontext_t) syscontext_FREE;

   return retcode;
ONERR:
   altstack = (stack_t) { .ss_flags = SS_DISABLE };
   (void) sigaltstack(&altstack, 0);
   (void) deletemain_threadlocalstore(&tls);

   #define ERRSTR1 "initrun_syscontext() "
   #define ERRSTR2 ":%.4u\nError %.4u\n"
   char errstr2[sizeof(ERRSTR2)];
   snprintf(errstr2, sizeof(errstr2), ERRSTR2, (linenr&0x1FFF), (err&0x1FFF));
   ssize_t written = write(STDERR_FILENO, ERRSTR1, sizeof(ERRSTR1)-1)
                   + write(STDERR_FILENO, __FILE__, strlen(__FILE__))
                   + write(STDERR_FILENO, errstr2, strlen(errstr2));
   #undef ERRSTR1
   #undef ERRSTR2
   (void) written;

   // mark system context as freed
   *scontext = (syscontext_t) syscontext_FREE;

   return err;
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static void        * s_userarg = 0;
static int           s_retcode = 0;
static pthread_t     s_thread;

static int main_testthread(void * user)
{
   s_userarg = user;
   s_thread  = self_thread()->sys_thread;
   return s_retcode;
}

static int mainabort_testthread(void * user)
{
   s_userarg = user;
   s_thread  = self_thread()->sys_thread;
   abort_thread();
   return 0;
}

static int child_initabort(void * dummy)
{
   (void) dummy;
   syscontext_t sc = syscontext_FREE;
   initrun_syscontext(&sc, &mainabort_testthread, &sc);
   return 0;
}

static int check_sc(const syscontext_t * sc)
{
   // check sc
   TEST(sc->pagesize_vm > 0);
   TEST(sc->log2pagesize_vm >= 8);
   TEST(sc->pagesize_vm == 1u << sc->log2pagesize_vm);
   TEST(sc->pagesize_vm == sys_pagesize_vm());

   return 0;
ONERR:
   return EINVAL;
}

static int maincheck_testthread(void * user)
{
   s_userarg = user;
   s_thread  = self_thread()->sys_thread;
   return check_sc(user);
}

static int test_init(void)
{
   file_t      fd  = file_FREE;
   pipe_t      pfd = pipe_FREE;
   process_t   process = process_FREE;
   syscontext_t sc = syscontext_FREE;

   // prepare
   s_userarg = 0;
   TEST( 0 == init_pipe(&pfd)
         && -1 != (fd = dup(STDERR_FILENO))
         && -1 != dup2(pfd.write, STDERR_FILENO));

   // TEST syscontext_FREE
   TEST(1 == isfree_syscontext(&sc));

   // TEST set_scontext
   TEST(0 == set_scontext(&sc));
   TEST(0 == check_sc(&sc));

   // TEST initrun_syscontext: EALREADY
   TEST(0 == check_sc(&sc));
   TEST(EALREADY == initrun_syscontext(&sc, &main_testthread, (void*)1));
   TEST(0 == s_userarg);
   sc = (syscontext_t) syscontext_FREE;

   // TEST initrun_syscontext: argument
   s_retcode = 0;
   for (int i = 10; i >= 0; --i) {
      TEST(0 == initrun_syscontext(&sc, &main_testthread, (void*)i));
      TEST(1 == isfree_syscontext(&sc));
      TEST(i == (int) s_userarg);
      TEST(0 != pthread_equal(s_thread, pthread_self()));
   }

   // TEST initrun_syscontext: return code
   for (int i = 10; i >= 0; --i) {
      s_retcode = i;
      s_userarg = (void*) 1;
      TEST(i == initrun_syscontext(&sc, &main_testthread, 0));
      TEST(1 == isfree_syscontext(&sc));
      TEST(0 == s_userarg);
      TEST(0 != pthread_equal(s_thread, pthread_self()));
   }

   // TEST initrun_syscontext: sets syscontext_t
   TEST(0 == initrun_syscontext(&sc, &maincheck_testthread, &sc));
   TEST(1 == isfree_syscontext(&sc));
   TEST(&sc == s_userarg);
   TEST(0 != pthread_equal(s_thread, pthread_self()));

   // TEST initrun_syscontext: init error
   for (unsigned i = 1; i; ++i) {
      uint8_t buffer[80];
      init_testerrortimer(&s_syscontext_errtimer, i, 333);
      s_userarg = 0;
      int err = initrun_syscontext(&sc, &main_testthread, (void*)1);
      TEST(1 == isfree_syscontext(&sc));
      TEST((i > 5) == (int) s_userarg);
      if (!err) {
         TEST(8 == i);
         TEST(-1 == read(pfd.read, buffer, sizeof(buffer)));
         free_testerrortimer(&s_syscontext_errtimer);
         break;
      }
      TEST(333 == err);
      ssize_t len = read(pfd.read, buffer, sizeof(buffer));
      TESTP(66 == len, "len=%zd", len);
      buffer[len] = 0;
      PRINTF_ERRLOG("%s", buffer);
   }

   TEST(STDERR_FILENO == dup2(fd, STDERR_FILENO));
   TEST(0 == free_file(&fd));

   // TEST initrun_syscontext: abort
   {
      process_result_t  result;
      process_stdio_t   stdfd = process_stdio_INIT_DEVNULL;
      redirecterr_processstdio(&stdfd, pfd.write);
      TEST(0 == init_process(&process, &child_initabort, 0, &stdfd));
      TEST(0 == wait_process(&process, &result));
      TEST(0 == free_process(&process));
      TEST(process_state_ABORTED == result.state);
      uint8_t  buffer[80];
      ssize_t  len = 0;
      len = read(pfd.read, buffer, sizeof(buffer));
      TEST(61 == len);
      buffer[len] = 0;
      PRINTF_ERRLOG("%s", buffer);
   }

   // unprepare
   TEST(0 == free_pipe(&pfd));

   return 0;
ONERR:
   free_process(&process);
   if (fd != file_FREE) {
      dup2(fd, STDERR_FILENO);
      free_file(&fd);
   }
   free_pipe(&pfd);
   return EINVAL;
}

static int test_query(void)
{
   syscontext_t sc = syscontext_FREE;

   // TEST isfree_syscontext: true
   TEST(1 == isfree_syscontext(&sc));

   // TEST isfree_syscontext: false
   TEST(0 == set_scontext(&sc));
   TEST(0 == isfree_syscontext(&sc));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_platform_init()
{
   bool     isold = false;
   stack_t  oldstack;

   TEST(0 == sigaltstack(0, &oldstack));
   isold = true;

   if (test_init())     goto ONERR;
   if (test_query())    goto ONERR;

   TEST(0 == sigaltstack(&oldstack, 0));

   return 0;
ONERR:
   if (isold) sigaltstack(&oldstack, 0);
   return EINVAL;
}

#endif
