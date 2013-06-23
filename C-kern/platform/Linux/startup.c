/* title: PlatformStartup Linuximpl

   Implements <PlatformStartup>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/startup.h
    Header file <PlatformStartup>.

   file: C-kern/platform/Linux/startup.c
    Implementation file <PlatformStartup Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/startup.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/platform/task/thread_tls.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/task/process.h"
#endif

typedef struct init_argument_t            init_argument_t ;

/* struct: init_argument_t
 * Argument given to <callmain_platform>. */
struct init_argument_t {
   int            argc ;
   const char **  argv ;
} ;


// section: Functions

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_platform_errtimer
 * Simulates an error in <startup_platform>. */
static test_errortimer_t   s_platform_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: startup

static void callmain_platform(void)
{
   thread_t * thread = self_thread() ;

   assert(1 == ismain_thread(thread)) ;

   bool is_abort ;
   if (  setcontinue_thread(&is_abort)
         || is_abort) {
      #define ERRSTR1 "startup_platform() at "
      #define ERRSTR2 ":" STR(__LINE__) "\naborted\n"
      ssize_t written = write(STDERR_FILENO, ERRSTR1, sizeof(ERRSTR1)-1)
                      + write(STDERR_FILENO, __FILE__, strlen(__FILE__))
                      + write(STDERR_FILENO, ERRSTR2, sizeof(ERRSTR2)-1) ;
      #undef ERRSTR1
      #undef ERRSTR2
      (void) written ;
      abort() ;
   }

   init_argument_t * initarg     = mainarg_thread(thread) ;
   mainthread_f      main_thread = (mainthread_f) maintask_thread(thread) ;
   int               retcode     = main_thread(initarg->argc, initarg->argv) ;

   setreturncode_thread(thread, retcode) ;
}

int startup_platform(int argc, const char ** argv, mainthread_f main_thread)
{
   volatile int err ;
   volatile int linenr ;
   int               retcode = 0 ;
   volatile int      is_exit = 0 ;
   init_argument_t   initarg = { argc, argv } ;
   thread_tls_t      tls     = thread_tls_INIT_FREEABLE ;
   memblock_t        threadstack ;
   memblock_t        signalstack ;
   ucontext_t        context_caller ;
   ucontext_t        context_mainthread ;

   linenr = __LINE__ ;
   ONERROR_testerrortimer(&s_platform_errtimer, ONABORT) ;
   err = initstartup_threadtls(&tls, &threadstack, &signalstack) ;
   if (err) goto ONABORT ;

   linenr = __LINE__ ;
   ONERROR_testerrortimer(&s_platform_errtimer, ONABORT) ;
   stack_t altstack = { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size } ;
   if (sigaltstack(&altstack, 0)) {
      err = errno ;
      goto ONABORT ;
   }

   linenr = __LINE__ ;
   ONERROR_testerrortimer(&s_platform_errtimer, ONABORT) ;
   if (getcontext(&context_caller)) {
      err = errno ;
      goto ONABORT ;
   }

   if (is_exit) {
      volatile thread_t * thread = thread_threadtls(&tls) ;
      retcode = returncode_thread(thread) ;
      goto ONABORT ;
   }

   is_exit = 1 ;

   linenr = __LINE__ ;
   ONERROR_testerrortimer(&s_platform_errtimer, ONABORT) ;
   if (getcontext(&context_mainthread)) {
      err = errno ;
      goto ONABORT ;
   }

   context_mainthread.uc_link  = &context_caller ;
   context_mainthread.uc_stack = (stack_t) { .ss_sp = threadstack.addr, .ss_flags = 0, .ss_size = threadstack.size } ;
   makecontext(&context_mainthread, &callmain_platform, 0, 0) ;

   thread_t * thread = thread_threadtls(&tls) ;
   settask_thread(thread, (thread_f)main_thread, &initarg) ;
#define KONFIG_thread 1
#if ((KONFIG_SUBSYS&KONFIG_thread) == KONFIG_thread)
   initstartup_thread(thread) ;
#endif
#undef KONFIG_thread

   linenr = __LINE__ ;
   ONERROR_testerrortimer(&s_platform_errtimer, ONABORT) ;
   // start callmain_platform and returns to first getcontext then if (is_exit) {} is executed
   setcontext(&context_mainthread) ;
   err = errno ;

ONABORT:
   if (!err) linenr = __LINE__ ;
   SETONERROR_testerrortimer(&s_platform_errtimer, &err) ;
   altstack = (stack_t) { .ss_flags = SS_DISABLE } ;
   int err2 = sigaltstack(&altstack, 0) ;
   if (err2 && !err) err = errno ;

   if (!err) linenr = __LINE__ ;
   SETONERROR_testerrortimer(&s_platform_errtimer, &err) ;
   err2 = freestartup_threadtls(&tls) ;
   if (err2 && !err) err = errno ;
   if (err) {
      #define ERRSTR1 "startup_platform() at "
      #define ERRSTR2 ":%.4u\nError %.4u\n"
      char errstr2[sizeof(ERRSTR2)] ;
      snprintf(errstr2, sizeof(errstr2), ERRSTR2, (linenr&0x1FFF), (err&0x1FFF)) ;
      ssize_t written = write(STDERR_FILENO, ERRSTR1, sizeof(ERRSTR1)-1)
                      + write(STDERR_FILENO, __FILE__, strlen(__FILE__))
                      + write(STDERR_FILENO, errstr2, strlen(errstr2)) ;
      #undef ERRSTR1
      #undef ERRSTR2
      (void) written ;
      return err ;
   }
   return retcode ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int           s_argc = 0 ;
static const char ** s_argv = 0 ;
static int           s_retcode = 0 ;
static pthread_t     s_thread ;

static int main_testthread(int argc, const char ** argv)
{
   s_argc = argc ;
   s_argv = argv ;
   s_thread = self_thread()->sys_thread ;
   return s_retcode ;
}

static int mainabort_testthread(int argc, const char ** argv)
{
   s_argc = argc ;
   s_argv = argv ;
   s_thread = self_thread()->sys_thread ;
   abort_thread() ;
   return 0 ;
}

static int child_startupabort(void * dummy)
{
   (void) dummy ;
   startup_platform(0, 0, &mainabort_testthread) ;
   return 0 ;
}

static int test_startup(void)
{
   file_t      fd      = file_INIT_FREEABLE ;
   file_t      pfd[2]  = { file_INIT_FREEABLE, file_INIT_FREEABLE } ;
   process_t   process = process_INIT_FREEABLE ;

   TEST( 0 == pipe2(pfd, O_CLOEXEC)
         && -1 != (fd = dup(STDERR_FILENO))
         && -1 != dup2(pfd[1], STDERR_FILENO)) ;

   // TEST startup_platform
   s_retcode = 0 ;
   for (int i = 10; i >= 0; --i) {
      TEST(0 == startup_platform(i, (const char**)(i+1), &main_testthread)) ;
      TEST(s_argc == i) ;
      TEST(s_argv == (const char**)(i+1)) ;
      TEST(s_argv == (const char**)(i+1)) ;
      TEST(0 != pthread_equal(s_thread, pthread_self())) ;
   }

   // TEST startup_platform: return code
   for (int i = 10; i >= 0; --i) {
      s_retcode = i ;
      s_argc    = 1 ;
      s_argv    = (const char**)1 ;
      TEST(i == startup_platform(0, 0, &main_testthread)) ;
      TEST(0 == s_argc) ;
      TEST(0 == s_argv) ;
   }

   // TEST startup_platform: startup error
   for (unsigned i = 1; i <= 7; ++i) {
      init_testerrortimer(&s_platform_errtimer, i, 333) ;
      s_argc = 0 ;
      s_argv = 0 ;
      TEST(333 == startup_platform(1, (const char**)1, &main_testthread))
      if (i <= 5) {
         TEST(0 == s_argc) ;
         TEST(0 == s_argv) ;
      } else {
         TEST(1 == s_argc) ;
         TEST(1 == (uintptr_t)s_argv) ;
      }
      uint8_t  buffer[80] ;
      ssize_t  len = 0 ;
      len = read(pfd[0], buffer, sizeof(buffer)) ;
      TEST(70 == len) ;
      buffer[len] = 0 ;
      PRINTF_LOG("%s", buffer) ;
   }

   TEST(-1 != dup2(fd, STDERR_FILENO)) ;
   TEST(0 == free_file(&fd)) ;

   // TEST startup_platform: abort
   {
      process_result_t  result ;
      process_stdio_t   stdfd = process_stdio_INIT_DEVNULL ;
      redirecterr_processstdio(&stdfd, pfd[1]) ;
      TEST(0 == init_process(&process, &child_startupabort, 0, &stdfd)) ;
      TEST(0 == wait_process(&process, &result)) ;
      TEST(process_state_ABORTED == result.state) ;
      TEST(0 == free_process(&process)) ;
      uint8_t  buffer[80] ;
      ssize_t  len = 0 ;
      len = read(pfd[0], buffer, sizeof(buffer)) ;
      TEST(65 == len) ;
      buffer[len] = 0 ;
      PRINTF_LOG("%s", buffer) ;
   }

   TEST(0 == free_file(&pfd[0])) ;
   TEST(0 == free_file(&pfd[1])) ;

   return 0 ;
ONABORT:
   free_process(&process) ;
   if (fd != file_INIT_FREEABLE) {
      dup2(fd, STDERR_FILENO) ;
   }
   free_file(&fd) ;
   free_file(&pfd[0]) ;
   free_file(&pfd[1]) ;
   return EINVAL ;
}

int unittest_platform_startup()
{
   resourceusage_t   usage    = resourceusage_INIT_FREEABLE ;
   bool              isold    = false ;
   stack_t           oldstack ;

   TEST(0 == sigaltstack(0, &oldstack)) ;
   isold = true ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_startup())     goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   TEST(0 == sigaltstack(&oldstack, 0)) ;

   return 0 ;
ONABORT:
   if (isold) sigaltstack(&oldstack, 0) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
