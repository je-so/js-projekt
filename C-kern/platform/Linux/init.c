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
#include "C-kern/api/platform/task/thread_stack.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
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

int initrun_syscontext(thread_f main_thread, void * main_arg)
{
   int err;
   memblock_t threadstack;
   memblock_t signalstack;
   ucontext_t context_old;
   ucontext_t context_mainthread;
   thread_stack_t* tst = 0;
   const size_t static_size = extsize_threadcontext() + extsize_processcontext();

   err = new_threadstack(&tst, static_size, &threadstack, &signalstack);
   (void) PROCESS_testerrortimer(&s_syscontext_errtimer, &err);
   if (err) {
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_CALL_ERRLOG, "new_threadstack", err);
      goto ONERR;
   }

   err = initstatic_threadcontext(context_threadstack(tst));
   (void) PROCESS_testerrortimer(&s_syscontext_errtimer, &err);
   if (err) {
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_CALL_ERRLOG, "initstatic_threadcontext", err);
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
   extern void start_mainthread(void);
   makecontext(&context_mainthread, &start_mainthread, 0, 0);

   thread_t * thread = thread_threadstack(tst);
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

   err = freestatic_threadcontext(context_threadstack(tst));
   (void) PROCESS_testerrortimer(&s_syscontext_errtimer, &err);
   if (err) {
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_CALL_ERRLOG, "freestatic_threadcontext", err);
      goto ONERR;
   }

   err = delete_threadstack(&tst);
   (void) PROCESS_testerrortimer(&s_syscontext_errtimer, &err);
   if (err) {
      TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, FUNCTION_CALL_ERRLOG, "delete_threadstack", err);
      goto ONERR;
   }

   return retcode;
ONERR:
   altstack = (stack_t) { .ss_flags = SS_DISABLE };
   (void) sigaltstack(&altstack, 0);
   if (tst) {
      freestatic_threadcontext(context_threadstack(tst));
   }
   (void) delete_threadstack(&tst);
   TRACE_LOG(INIT, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static void        * s_used_log = 0;
static void        * s_userarg = 0;
static int           s_retcode = 0;
static pthread_t     s_thread;

static int main_testthread(void * user)
{
   s_userarg = user;
   s_thread  = self_thread()->sys_thread;
   return s_retcode;
}

static int log_testthread(void* text)
{
   s_used_log = log_maincontext().object;
   PRINTF_ERRLOG("%.5s",(char*)text);
   FLUSHBUFFER_ERRLOG();
   return 0;
}

static int child_logstring(void* text)
{
   // make sure: static log of new thread context is used
   // (setup in initrun_syscontext) !!
   s_used_log = 0;
   int err=initrun_syscontext(&log_testthread, text);
   PRINTF_ERRLOG("%s",5+(char*)text);
   FLUSHBUFFER_ERRLOG();
   return err ? err : s_used_log != log_maincontext().object ? 0 : EINVAL;
}

static int test_init(void)
{
   pipe_t      pfd = pipe_FREE;
   iochannel_t olderr  = iochannel_FREE;
   process_t   process = process_FREE;
   char        buffer[512];

   // prepare
   g_maincontext.startarg_type = maincontext_STATIC; // ensures no init of environment
   s_userarg = 0;
   TEST(0 == init_pipe(&pfd));

   // TEST initrun_syscontext: argument
   s_retcode = 0;
   for (uintptr_t i = 0; i <= 10; ++i) {
      TEST(0 == initrun_syscontext(&main_testthread, (void*)i));
      TEST(i == (uintptr_t) s_userarg);
      TEST(0 != pthread_equal(s_thread, pthread_self()));
   }

   // TEST initrun_syscontext: return code
   s_retcode = 0;
   for (unsigned i = 0; i <= 10; ++i, ++s_retcode) {
      s_userarg = (void*) 1;
      TEST(i == (unsigned) initrun_syscontext(&main_testthread, 0));
      TEST(0 == s_userarg);
      TEST(0 != pthread_equal(s_thread, pthread_self()));
   }

   // TEST initrun_syscontext: use log
   {
      char              text[] = { "123456789\n" };
      process_result_t  result;
      process_stdio_t   stdfd = process_stdio_INIT_DEVNULL;
      redirecterr_processstdio(&stdfd, pfd.write);
      TEST(0 == init_process(&process, &child_logstring, text, &stdfd));
      TEST(0 == wait_process(&process, &result));
      TEST(0 == free_process(&process));
      TEST(process_state_TERMINATED == result.state);
      size_t len = (size_t) read(pfd.read, buffer, sizeof(buffer));
      TEST(len == strlen(text));
      buffer[len] = 0;
      TEST(0 == strcmp(text, buffer));
   }

   // TEST initrun_syscontext: simulated error
   s_retcode = 0;
   s_userarg = 0;
   TEST(0 < (olderr = dup(iochannel_STDERR)));
   TEST(iochannel_STDERR == dup2(pfd.write, iochannel_STDERR));
   for (unsigned i=1; ;++i) {
      init_testerrortimer(&s_syscontext_errtimer, i, (int)i);
      int err = initrun_syscontext(&main_testthread, (void*)(uintptr_t)i);
      if (i <= 4) {
         TEST( 0 == s_userarg); // error before call
      } else {
         TEST( i == (uintptr_t)s_userarg); // err in free resources
      }
      if (!err) {
         TEST( -1 == read(pfd.read, buffer, sizeof(buffer)));
         TEST( 9 == i);
         free_testerrortimer(&s_syscontext_errtimer);
         break;
      }
      TEST(0 < read(pfd.read, buffer, sizeof(buffer))); // written error log
   }

   // unprepare
   TEST(0 == free_pipe(&pfd));
   TEST(iochannel_STDERR == dup2(olderr, iochannel_STDERR));
   TEST(0 == free_iochannel(&olderr));

   return 0;
ONERR:
   if (!isfree_iochannel(olderr)) {
      dup2(olderr, iochannel_STDERR);
      free_iochannel(&olderr);
   }
   free_process(&process);
   free_pipe(&pfd);
   return EINVAL;
}

int unittest_platform_init()
{
   bool     isold = false;
   stack_t  oldstack;

   TEST(0 == sigaltstack(0, &oldstack));
   isold = true;

   if (test_init())     goto ONERR;

   TEST(0 == sigaltstack(&oldstack, 0));

   return 0;
ONERR:
   if (isold) sigaltstack(&oldstack, 0);
   return EINVAL;
}

#endif
