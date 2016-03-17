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
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/io/writer/log/logwriter.h"
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
 * Calls the main thread's main function.
 * The whole process is aborted if the main thread aborts. */
static void callmain_platform(void)
{
   thread_t * thread = self_thread();

   volatile bool is_abort;
   if (  setcontinue_thread(&is_abort)
         || is_abort) {
      // threadcontext_t is already set up (use default log)
      int err = returncode_thread(thread);
      TRACE_ERRLOG(log_flags_NONE, THREAD_MAIN_ABORT, context_threadlocalstore(castPthread_threadlocalstore(thread))->thread_id);
      TRACEEXIT_ERRLOG(err);
      abort_maincontext(err);
   }

   void*    arg     = mainarg_thread(thread);
   thread_f mainthr = maintask_thread(thread);
   int      retcode = mainthr(arg);

   setreturncode_thread(thread, retcode);
}

static inline int init_syscontext(/*out*/struct syscontext_t * scontext)
{
   int err;

   size_t pagesize = sys_pagesize_vm();

   if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      pagesize = 0;
   } else if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      pagesize = 1023;
   } else if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      pagesize >>= 1;
   }

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
   TRACE_LOG(INIT, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

int initrun_syscontext(/*out;err*/struct syscontext_t * scontext, thread_f main_thread, void * main_arg)
{
   int err;
   memblock_t threadstack;
   memblock_t signalstack;
   ucontext_t context_old;
   ucontext_t context_mainthread;
   thread_localstore_t* tls = 0;

   err = init_syscontext(scontext);
   if (err) goto ONERR;

   err = new_threadlocalstore(&tls, &threadstack, &signalstack, scontext->pagesize_vm);
   (void) PROCESS_testerrortimer(&s_syscontext_errtimer, &err);
   if (err) {
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_CALL_ERRLOG, "new_threadlocalstore", err);
      goto ONERR;
   }

   stack_t altstack = { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size };
   err = sigaltstack(&altstack, 0);
   if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      errno = err;
   }
   if (err) {
      err = errno;
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "sigaltstack", err);
      goto ONERR;
   }

   err = getcontext(&context_mainthread);
   if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      errno = err;
   }
   if (err) {
      err = errno;
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "getcontext", err);
      goto ONERR;
   }

   context_mainthread.uc_link  = &context_old;
   context_mainthread.uc_stack = (stack_t) { .ss_sp = threadstack.addr, .ss_flags = 0, .ss_size = threadstack.size };
   makecontext(&context_mainthread, &callmain_platform, 0, 0);

   thread_t * thread = thread_threadlocalstore(tls);
   initmain_thread(thread, main_thread, main_arg);

   // start callmain_platform and returns to context_old after exit
   err = swapcontext(&context_old, &context_mainthread);
   if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      errno = err;
   }
   if (err) {
      err = errno;
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "swapcontext", err);
      goto ONERR;
   }

   int retcode = returncode_thread(thread);

   altstack = (stack_t) { .ss_flags = SS_DISABLE };
   err = sigaltstack(&altstack, 0);
   if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      errno = err;
   }
   if (err) {
      err = errno;
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "sigaltstack", err);
      goto ONERR;
   }

   err = delete_threadlocalstore(&tls);
   (void) PROCESS_testerrortimer(&s_syscontext_errtimer, &err);
   if (err) {
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_CALL_ERRLOG, "delete_threadlocalstore", err);
      goto ONERR;
   }

   // mark system context as freed
   *scontext = (syscontext_t) syscontext_FREE;

   return retcode;
ONERR:
   altstack = (stack_t) { .ss_flags = SS_DISABLE };
   (void) sigaltstack(&altstack, 0);
   (void) delete_threadlocalstore(&tls);

   // mark system context as freed
   *scontext = (syscontext_t) syscontext_FREE;

   TRACE_LOG(INIT, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
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
   // make sure: static log of new thread context is used
   // (setup in inittun_syscontext) !!
   memset(tcontext_maincontext(), 0, sizeof(threadcontext_t));
   // test abort
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
   uint8_t     buffer[512];

   // prepare
   s_userarg = 0;
   TEST( 0 == init_pipe(&pfd)
         && -1 != (fd = dup(STDERR_FILENO))
         && -1 != dup2(pfd.write, STDERR_FILENO));

   // TEST syscontext_FREE
   TEST(1 == isfree_syscontext(&sc));

   // TEST init_syscontext
   TEST(0 == init_syscontext(&sc));
   TEST(0 == check_sc(&sc));

   // TEST init_syscontext: EINVAL (pagesize == 0, pagesize not power of 2, pagesize truncated)
   sc = (syscontext_t) syscontext_FREE;
   for (unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_syscontext_errtimer, i, ENOSYS);
      int err = init_syscontext(&sc);
      if (!err) {
         TESTP(i == 4, "i=%d", i);
         break;
      }
      TEST(EINVAL == err);
      TEST(1 == isfree_syscontext(&sc));
   }
   free_testerrortimer(&s_syscontext_errtimer);

   // TEST initrun_syscontext: argument
   s_retcode = 0;
   for (uintptr_t i = 0; i <= 10; ++i) {
      TEST(0 == initrun_syscontext(&sc, &main_testthread, (void*)i));
      TEST(1 == isfree_syscontext(&sc));
      TEST(i == (uintptr_t) s_userarg);
      TEST(0 != pthread_equal(s_thread, pthread_self()));
   }

   // TEST initrun_syscontext: return code
   s_retcode = 0;
   for (unsigned i = 0; i <= 10; ++i, ++s_retcode) {
      s_userarg = (void*) 1;
      TEST(i == (unsigned) initrun_syscontext(&sc, &main_testthread, 0));
      TEST(1 == isfree_syscontext(&sc));
      TEST(0 == s_userarg);
      TEST(0 != pthread_equal(s_thread, pthread_self()));
   }

   // TEST initrun_syscontext: sets syscontext_t
   TEST(0 == initrun_syscontext(&sc, &maincheck_testthread, &sc));
   TEST(1 == isfree_syscontext(&sc));
   TEST(&sc == s_userarg);
   TEST(0 != pthread_equal(s_thread, pthread_self()));

   // TEST initrun_syscontext: simulated ERROR
   s_retcode = 0;
   for (unsigned i = 1; ; ++i) {
      init_testerrortimer(&s_syscontext_errtimer, i, 333);
      s_userarg = 0;
      int err = initrun_syscontext(&sc, &main_testthread, (void*)1);
      TEST(1 == isfree_syscontext(&sc));
      TEST((i > 6) == (intptr_t) s_userarg);
      if (!err) {
         TEST(10 == i);
         TEST(-1 == read(pfd.read, buffer, sizeof(buffer)));
         free_testerrortimer(&s_syscontext_errtimer);
         break;
      }
      TESTP((i < 4 ? EINVAL : 333) == err, "i:%d err:%d", i, err);
      ssize_t len = read(pfd.read, buffer, sizeof(buffer));
      TESTP(165 < len, "len=%zd", len);
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
      ssize_t len = read(pfd.read, buffer, sizeof(buffer));
      TEST(300 < len);
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
   TEST(0 == init_syscontext(&sc));
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
