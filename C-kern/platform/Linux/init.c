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
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/platform/task/thread_tls.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/process.h"
#endif


// section: platform_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_platform_errtimer
 * Used to simulate an error in <platform_t.init_platform>. */
static test_errortimer_t   s_platform_errtimer = test_errortimer_FREE;
#endif

// group: helper

/* function: callmain_platform
 * Calls the main threads main function.
 * An error is written to STDERR if the main thread
 * aborts. */
static void callmain_platform(void)
{
   thread_t * thread = self_thread();

   assert(1 == ismain_thread(thread));

   bool is_abort;
   if (  setcontinue_thread(&is_abort)
         || is_abort) {
      #define ERRSTR1 "init_platform() "
      #define ERRSTR2 ":" STR(__LINE__) "\naborted\n"
      ssize_t written = write(STDERR_FILENO, ERRSTR1, sizeof(ERRSTR1)-1)
                      + write(STDERR_FILENO, __FILE__, strlen(__FILE__))
                      + write(STDERR_FILENO, ERRSTR2, sizeof(ERRSTR2)-1);
      #undef ERRSTR1
      #undef ERRSTR2
      (void) written;
      abort();
   }

   void *         userarg     = mainarg_thread(thread);
   mainthread_f   main_thread = maintask_thread(thread);
   int            retcode     = main_thread(userarg);

   setreturncode_thread(thread, retcode);
}

int init_platform(mainthread_f main_thread, void * user)
{
   volatile int err;
   volatile int linenr;
   int               retcode = 0;
   volatile int      is_exit = 0;
   thread_tls_t      tls     = thread_tls_FREE;
   memblock_t        threadstack;
   memblock_t        signalstack;
   ucontext_t        context_caller;
   ucontext_t        context_mainthread;

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_platform_errtimer, &err, ONERR);
   err = initmain_threadtls(&tls, &threadstack, &signalstack);
   if (err) goto ONERR;

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_platform_errtimer, &err, ONERR);
   stack_t altstack = { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size };
   if (sigaltstack(&altstack, 0)) {
      err = errno;
      goto ONERR;
   }

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_platform_errtimer, &err, ONERR);
   if (getcontext(&context_caller)) {
      err = errno;
      goto ONERR;
   }

   if (is_exit) {
      volatile thread_t * thread = thread_threadtls(&tls);
      retcode = returncode_thread(thread);
      goto ONERR;
   }

   is_exit = 1;

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_platform_errtimer, &err, ONERR);
   if (getcontext(&context_mainthread)) {
      err = errno;
      goto ONERR;
   }

   context_mainthread.uc_link  = &context_caller;
   context_mainthread.uc_stack = (stack_t) { .ss_sp = threadstack.addr, .ss_flags = 0, .ss_size = threadstack.size };
   makecontext(&context_mainthread, &callmain_platform, 0, 0);

   thread_t * thread = thread_threadtls(&tls);
   settask_thread(thread, main_thread, user);
#if defined(KONFIG_SUBSYS_THREAD)
   initmain_thread(thread);
#endif

   linenr = __LINE__;
   ONERROR_testerrortimer(&s_platform_errtimer, &err, ONERR);
   // start callmain_platform and returns to first getcontext then if (is_exit) {} is executed
   setcontext(&context_mainthread);
   err = errno;

ONERR:
   if (!err) linenr = __LINE__;
   SETONERROR_testerrortimer(&s_platform_errtimer, &err);
   altstack = (stack_t) { .ss_flags = SS_DISABLE };
   int err2 = sigaltstack(&altstack, 0);
   if (err2 && !err) err = errno;

   if (!err) linenr = __LINE__;
   SETONERROR_testerrortimer(&s_platform_errtimer, &err);
   err2 = freemain_threadtls(&tls);
   if (err2 && !err) err = errno;
   if (err) {
      #define ERRSTR1 "init_platform() "
      #define ERRSTR2 ":%.4u\nError %.4u\n"
      char errstr2[sizeof(ERRSTR2)];
      snprintf(errstr2, sizeof(errstr2), ERRSTR2, (linenr&0x1FFF), (err&0x1FFF));
      ssize_t written = write(STDERR_FILENO, ERRSTR1, sizeof(ERRSTR1)-1)
                      + write(STDERR_FILENO, __FILE__, strlen(__FILE__))
                      + write(STDERR_FILENO, errstr2, strlen(errstr2));
      #undef ERRSTR1
      #undef ERRSTR2
      (void) written;
      return err;
   }
   return retcode;
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
   init_platform(&mainabort_testthread, 0);
   return 0;
}

static int test_init(void)
{
   file_t      fd      = file_FREE;
   file_t      pfd[2]  = { file_FREE, file_FREE };
   process_t   process = process_FREE;

   TEST( 0 == pipe2(pfd, O_CLOEXEC)
         && -1 != (fd = dup(STDERR_FILENO))
         && -1 != dup2(pfd[1], STDERR_FILENO));

   // TEST init_platform
   s_retcode = 0;
   for (int i = 10; i >= 0; --i) {
      TEST(0 == init_platform(&main_testthread, (void*)i));
      TEST(i == (int) s_userarg);
      TEST(0 != pthread_equal(s_thread, pthread_self()));
   }

   // TEST init_platform: return code
   for (int i = 10; i >= 0; --i) {
      s_retcode = i;
      s_userarg = (void*) 1;
      TEST(i == init_platform(&main_testthread, 0));
      TEST(0 == s_userarg);
   }

   // TEST init_platform: init error
   for (unsigned i = 1; i <= 7; ++i) {
      init_testerrortimer(&s_platform_errtimer, i, 333);
      s_userarg = 0;
      TEST(333 == init_platform(&main_testthread, (void*)1));
      TEST((i > 5) == (int) s_userarg);
      uint8_t  buffer[80];
      ssize_t  len = 0;
      len = read(pfd[0], buffer, sizeof(buffer));
      TEST(61 == len);
      buffer[len] = 0;
      PRINTF_ERRLOG("%s", buffer);
   }

   TEST(-1 != dup2(fd, STDERR_FILENO));
   TEST(0 == free_file(&fd));

   // TEST init_platform: abort
   {
      process_result_t  result;
      process_stdio_t   stdfd = process_stdio_INIT_DEVNULL;
      redirecterr_processstdio(&stdfd, pfd[1]);
      TEST(0 == init_process(&process, &child_initabort, 0, &stdfd));
      TEST(0 == wait_process(&process, &result));
      TEST(process_state_ABORTED == result.state);
      TEST(0 == free_process(&process));
      uint8_t  buffer[80];
      ssize_t  len = 0;
      len = read(pfd[0], buffer, sizeof(buffer));
      TEST(56 == len);
      buffer[len] = 0;
      PRINTF_ERRLOG("%s", buffer);
   }

   TEST(0 == free_file(&pfd[0]));
   TEST(0 == free_file(&pfd[1]));

   return 0;
ONERR:
   free_process(&process);
   if (fd != file_FREE) {
      dup2(fd, STDERR_FILENO);
   }
   free_file(&fd);
   free_file(&pfd[0]);
   free_file(&pfd[1]);
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
