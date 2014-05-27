/* title: Thread Linux
   Implements <Thread>.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/task/thread.h
    Header file of <Thread>.

   file: C-kern/platform/Linux/task/thread.c
    Linux specific implementation <Thread Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/math/int/atomic.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/sync/semaphore.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/platform/task/thread_tls.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#endif


typedef struct thread_startargument_t  thread_startargument_t ;

/* struct: thread_startargument_t
 * Startargument of the started system thread.
 * The function <main_thread> then calls the main task of the thread which
 * is stored in <thread_t.main>. */
struct thread_startargument_t {
   thread_t *           self ;
   processcontext_t *   pcontext ;
   stack_t              signalstack ;
} ;


// section: thread_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_thread_errtimer
 * Simulates an error in <newgroup_thread>. */
static test_errortimer_t   s_thread_errtimer = test_errortimer_FREE ;
#endif

// group: helper

/* function: main_thread
 * The start function of the thread.
 * This is the same for all threads.
 * It initializes signalstack, <threadcontext_t>, and
 * calls the user supplied main function. */
static void * main_thread(thread_startargument_t * startarg)
{
   int err ;

   thread_t * thread = self_thread() ;

   assert(startarg->self == thread) ;

   thread->sys_thread = pthread_self() ;

   err = init_threadcontext(tcontext_maincontext(), startarg->pcontext, type_maincontext()) ;
   if (err) {
      TRACECALL_ERRLOG("init_threadcontext", err) ;
      goto ONABORT ;
   }

   if (sys_thread_FREE == pthread_self()) {
      err = EINVAL ;
      TRACE_ERRLOG(log_flags_NONE, FUNCTION_WRONG_RETURNVALUE, err, "pthread_self", STR(sys_thread_FREE)) ;
      goto ONABORT ;
   }

   // do not access startarg after sigaltstack (startarg is stored on this stack)
   err = sigaltstack(&startarg->signalstack, (stack_t*)0) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("sigaltstack", err) ;
      goto ONABORT ;
   }
   startarg = 0 ;

   if (0 != getcontext(&thread->continuecontext)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("getcontext", err) ;
      goto ONABORT ;
   }

   thread = self_thread() ;

   if (  0 == thread->returncode  // abort_thread sets returncode to ENOTRECOVERABLE
         && thread->main_task) {
      thread->returncode = thread->main_task(thread->main_arg) ;
   }

   err = free_threadcontext(sys_context_threadtls()) ;
   if (err) {
      TRACECALL_ERRLOG("free_threadcontext",err) ;
      goto ONABORT ;
   }

   return (void*)0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   abort_maincontext(err) ;
   return (void*)err ;
}

// group: lifetime

int initmain_thread(/*out*/thread_t * thread)
{
   if (!ismain_thread(thread)) return EINVAL ;
   thread->sys_thread = pthread_self() ;
   return 0 ;
}

int delete_thread(thread_t ** threadobj)
{
   int err ;
   int err2 ;
   thread_t * delobj = *threadobj ;

   if (delobj) {
      *threadobj = 0 ;

      err = join_thread(delobj) ;

      err2 = free_threadtls(genericcast_threadtls(delobj, tls_)) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int new_thread(/*out*/thread_t ** threadobj, thread_f thread_main, void * main_arg)
{
   int err ;
   int err2 = 0 ;
   pthread_attr_t    thread_attr ;
   thread_t *        thread            = 0 ;
   thread_tls_t      tls               = thread_tls_FREE ;
   bool              isThreadAttrValid = false ;

   ONERROR_testerrortimer(&s_thread_errtimer, &err, ONABORT);
   err = init_threadtls(&tls) ;
   if (err) goto ONABORT ;

   memblock_t signalstack ;
   memblock_t stack       ;
   signalstack_threadtls(&tls, &signalstack) ;
   threadstack_threadtls(&tls, &stack) ;

   thread = thread_threadtls(&tls) ;
   thread->main_task  = thread_main ;
   thread->main_arg   = main_arg ;
   thread->tls_addr   = tls.addr ;

   thread_startargument_t * startarg   = (thread_startargument_t*) signalstack.addr ;

   startarg->self        = thread ;
   startarg->pcontext    = pcontext_maincontext() ;
   startarg->signalstack = (stack_t) { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size } ;

   ONERROR_testerrortimer(&s_thread_errtimer, &err, ONABORT);
   err = pthread_attr_init(&thread_attr) ;
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_attr_init",err) ;
      goto ONABORT ;
   }
   isThreadAttrValid = true ;

   ONERROR_testerrortimer(&s_thread_errtimer, &err, ONABORT);
   err = pthread_attr_setstack(&thread_attr, stack.addr, stack.size) ;
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_attr_setstack",err) ;
      PRINTPTR_ERRLOG(stack.addr) ;
      PRINTSIZE_ERRLOG(stack.size) ;
      goto ONABORT ;
   }

   sys_thread_t sys_thread ;
   ONERROR_testerrortimer(&s_thread_errtimer, &err, ONABORT);
   static_assert( (void* (*) (typeof(startarg)))0 == (typeof(&main_thread))0, "main_thread has argument of type startarg") ;
   err = pthread_create(&sys_thread, &thread_attr, (void*(*)(void*))&main_thread, startarg) ;
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_create",err) ;
      goto ONABORT ;
   }

   thread->sys_thread = sys_thread ;

   err2 = pthread_attr_destroy(&thread_attr) ;
   isThreadAttrValid = false ;
   if (err2) {
      TRACESYSCALL_ERRLOG("pthread_attr_destroy",err) ;
      goto ONABORT ;
   }

   *threadobj = thread ;

   return 0 ;
ONABORT:
   if (err2) {
      abort_maincontext(err2) ;
   }
   if (isThreadAttrValid) {
      (void) pthread_attr_destroy(&thread_attr) ;
   }
   (void) free_threadtls(&tls) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: synchronize

int join_thread(thread_t * threadobj)
{
   int err ;

   if (sys_thread_FREE != threadobj->sys_thread) {

      err = pthread_join(threadobj->sys_thread, 0) ;
      threadobj->sys_thread = sys_thread_FREE ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: change-run-state

void suspend_thread()
{
   int err ;
   sigset_t signalmask ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaddset(&signalmask, SIGINT) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("sigaddset", err) ;
      goto ONABORT ;
   }

   do {
      err = sigwaitinfo(&signalmask, 0) ;
   } while (-1 == err && EINTR == errno) ;

   if (-1 == err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("sigwaitinfo", err) ;
      goto ONABORT ;
   }

   return ;
ONABORT:
   abort_maincontext(err) ;
}

int trysuspend_thread(void)
{
   int err ;
   sigset_t signalmask ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaddset(&signalmask, SIGINT) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("sigaddset", err) ;
      goto ONABORT ;
   }

   struct timespec   ts = { 0, 0 } ;
   err = sigtimedwait(&signalmask, 0, &ts) ;
   if (-1 == err) return EAGAIN ;

   return 0 ;
ONABORT:
   abort_maincontext(err) ;
   return err ;
}


void resume_thread(thread_t * threadobj)
{
   int err ;

   err = pthread_kill(threadobj->sys_thread, SIGINT) ;
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_kill", err) ;
      goto ONABORT ;
   }

   return ;
ONABORT:
   abort_maincontext(err) ;
}

void sleepms_thread(uint32_t msec)
{
   struct timespec reqtime = { .tv_sec = (int32_t) (msec / 1000), .tv_nsec = (int32_t) ((msec%1000) * 1000000) } ;

   int err ;
   err = nanosleep(&reqtime, 0) ;

   if (-1 == err) {
      err = errno ;
      if (err != EINTR) {
         TRACESYSCALL_ERRLOG("nanosleep", err) ;
         goto ONABORT ;
      }
   }

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

int exit_thread(int retcode)
{
   int err ;
   thread_t * thread = self_thread() ;

   VALIDATE_STATE_TEST(! ismain_thread(thread), ONABORT, ) ;

   setreturncode_thread(thread, retcode) ;

   err = free_threadcontext(sys_context_threadtls()) ;
   if (err) {
      TRACECALL_ERRLOG("free_threadcontext",err) ;
      abort_maincontext(err) ;
   }

   pthread_exit(0) ;

ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: abort

void abort_thread(void)
{
   thread_t * thread = self_thread() ;
   setreturncode_thread(thread, ENOTRECOVERABLE) ;
   setcontext(&thread->continuecontext) ;
   assert(0) ;
}


// group: test

#ifdef KONFIG_UNITTEST

static unsigned      s_thread_runcount = 0 ;
static unsigned      s_thread_signal   = 0 ;
static sys_thread_t  s_thread_id       = 0 ;

static int thread_donothing(void * dummy)
{
   (void) dummy ;
   return 0 ;
}

static int thread_returncode(intptr_t retcode)
{
   s_thread_id = pthread_self() ;
   atomicadd_int(&s_thread_runcount, 1) ;
   while (0 == atomicread_int(&s_thread_signal)) {
      pthread_yield() ;
   }
   s_thread_signal = 0 ;
   atomicsub_int(&s_thread_runcount, 1) ;
   return (int) retcode ;
}

static int test_initfree(void)
{
   thread_t * thread = 0 ;

   // TEST thread_FREE
   thread_t sthread = thread_FREE ;
   TEST(sthread.nextwait   == 0) ;
   TEST(sthread.lockflag   == 0) ;
   TEST(sthread.main_task  == 0) ;
   TEST(sthread.main_arg   == 0) ;
   TEST(sthread.returncode == 0) ;
   TEST(sthread.sys_thread == sys_thread_FREE) ;
   TEST(sthread.tls_addr   == 0) ;

   // TEST new_thread
   TEST(0 == new_thread(&thread, &thread_donothing, (void*)3)) ;
   TEST(thread) ;
   TEST(thread->nextwait   == 0) ;
   TEST(thread->lockflag   == 0) ;
   TEST(thread->main_task  == &thread_donothing) ;
   TEST(thread->main_arg   == (void*)3) ;
   TEST(thread->returncode == 0) ;
   TEST(thread->sys_thread != sys_thread_FREE) ;
   TEST(thread->tls_addr   != 0) ;

   // TEST delete_thread
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == thread) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == thread) ;

   // TEST newgeneric_thread: thread is run
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, 14)) ;
   TEST(thread) ;
   TEST(thread->nextwait   == 0) ;
   TEST(thread->lockflag   == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)14) ;
   TEST(thread->returncode == 0) ;
   TEST(thread->sys_thread != sys_thread_FREE) ;
   TEST(thread->tls_addr   != 0) ;
   uint8_t *    tls_addr   = thread->tls_addr ;
   sys_thread_t T          = thread->sys_thread ;
   // test thread is running
   while (0 == atomicread_int(&s_thread_runcount)) {
      yield_thread() ;
   }
   TEST(s_thread_id        == T) ;
   TEST(thread) ;
   TEST(thread->nextwait   == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)14) ;
   TEST(thread->returncode == 0) ;
   TEST(thread->sys_thread == T) ;
   TEST(thread->tls_addr   == tls_addr  ) ;
   atomicwrite_int(&s_thread_signal, 1) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == thread) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == thread) ;

   // TEST delete_thread: join_thread called from delete_thread
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, 11)) ;
   TEST(thread) ;
   TEST(thread->nextwait   == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)11) ;
   TEST(thread->returncode == 0) ;
   TEST(thread->sys_thread != sys_thread_FREE) ;
   TEST(thread->tls_addr   != 0) ;
   T = thread->sys_thread ;
   atomicwrite_int(&s_thread_signal, 1) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == atomicread_int(&s_thread_signal)) ; // => delete has waited until thread has run
   TEST(T == s_thread_id) ;

   // TEST new_thread: ERROR
   for (int i = 1; i; ++i) {
      init_testerrortimer(&s_thread_errtimer, (unsigned)i, i) ;
      int err = newgeneric_thread(&thread, &thread_returncode, 0) ;
      if (!err) {
         TEST(0 != thread) ;
         TEST(i == 5)
         break ;
      }
      TEST(0 == thread) ;
      TEST(i == err) ;
   }
   free_testerrortimer(&s_thread_errtimer) ;
   atomicwrite_int(&s_thread_signal, 1) ;
   TEST(0 == delete_thread(&thread)) ;

   return 0 ;
ONABORT:
   atomicwrite_int(&s_thread_signal, 1) ;
   delete_thread(&thread) ;
   return EINVAL ;
}

static int test_mainthread(void)
{
   thread_t thread = thread_FREE ;

   // TEST initmain_thread
   TEST(0 == initmain_thread(&thread))
   TEST(thread.nextwait   == 0) ;
   TEST(thread.lockflag   == 0) ;
   TEST(thread.main_task  == 0) ;
   TEST(thread.main_arg   == 0) ;
   TEST(thread.returncode == 0) ;
   TEST(thread.sys_thread == pthread_self()) ;
   TEST(thread.tls_addr   == 0) ;

   // TEST initmain_thread: calling twice does no harm
   TEST(0 == initmain_thread(&thread))
   TEST(thread.sys_thread == pthread_self()) ;

   // TEST initmain_thread: EINVAL
   thread.tls_addr = (void*)1 ; // no main thread
   TEST(EINVAL == initmain_thread(&thread)) ;
   TEST(thread.sys_thread == pthread_self()) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   thread_t    thread ;

   // TEST self_thread
   TEST(self_thread() == thread_threadtls(&current_threadtls(&thread))) ;

   // TEST returncode_thread
   for (int R = -10; R <= 10; ++R) {
      setreturncode_thread(&thread, R) ;
      TEST(R == returncode_thread(&thread)) ;
   }

   // TEST maintask_thread
   settask_thread(&thread, (thread_f)&thread_returncode, 0) ;
   TEST(maintask_thread(&thread) == (thread_f)&thread_returncode) ;
   settask_thread(&thread, 0, 0) ;
   TEST(maintask_thread(&thread) == 0) ;

   // TEST mainarg_thread
   for (uintptr_t A = 0; A <= 10; ++A) {
      settask_thread(&thread, 0, (void*)A) ;
      TEST(A == (uintptr_t)mainarg_thread(&thread)) ;
   }

   // TEST ismain_thread
   thread_t * mainthread = self_thread() ;
   TEST(0 == mainthread->tls_addr)
   TEST(1 == ismain_thread(mainthread)) ;
   mainthread->tls_addr = (uint8_t*)1 ;
   TEST(0 == ismain_thread(mainthread)) ;
   mainthread->tls_addr = (uint8_t*)0 ;
   TEST(1 == ismain_thread(mainthread)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_join(void)
{
   thread_t * thread = 0 ;

   // TEST join_thread
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, 12)) ;
   atomicwrite_int(&s_thread_signal, 1) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == atomicread_int(&s_thread_signal)) ;
   TEST(thread->nextwait   == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)12) ;
   TEST(thread->returncode == 12) ;
   TEST(thread->sys_thread == sys_thread_FREE) ;
   TEST(thread->tls_addr   != 0) ;

   // TEST join_thread: calling on already joined thread
   TEST(0 == join_thread(thread)) ;
   TEST(thread->nextwait   == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)12) ;
   TEST(thread->returncode == 12) ;
   TEST(thread->sys_thread == sys_thread_FREE) ;
   TEST(thread->tls_addr   != 0) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST join_thread: different returncode
   for (int i = -5; i < 5; ++i) {
      const intptr_t arg = 1111 * i ;
      TEST(0 == newgeneric_thread(&thread, thread_returncode, arg)) ;
      TEST(thread->sys_thread != sys_thread_FREE) ;
      atomicwrite_int(&s_thread_signal, 1) ;
      TEST(0 == join_thread(thread)) ;
      TEST(0 == atomicread_int(&s_thread_signal)) ;
      TEST(thread->nextwait   == 0) ;
      TEST(thread->main_task  == (thread_f)&thread_returncode) ;
      TEST(thread->main_arg   == (void*)arg) ;
      TEST(thread->returncode == arg) ;
      TEST(thread->sys_thread == sys_thread_FREE) ;
      TEST(thread->tls_addr   != 0) ;
      TEST(0 == join_thread(thread)) ;
      TEST(thread->nextwait   == 0) ;
      TEST(thread->main_task  == (thread_f)&thread_returncode) ;
      TEST(thread->main_arg   == (void*)arg) ;
      TEST(thread->returncode == arg) ;
      TEST(thread->sys_thread == sys_thread_FREE) ;
      TEST(thread->tls_addr   != 0) ;
      TEST(0 == delete_thread(&thread)) ;
   }

   // TEST join_thread: EDEADLK
   thread_t selfthread = { .sys_thread = pthread_self() } ;
   TEST(EDEADLK == join_thread(&selfthread)) ;

   // TEST join_thread: ESRCH
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, 0)) ;
   thread_t copied_thread = *thread ;
   atomicwrite_int(&s_thread_signal, 1) ;
   TEST(0 == join_thread(thread)) ;
   TEST(thread->sys_thread        == sys_thread_FREE) ;
   TEST(returncode_thread(thread) == 0) ;
   TEST(ESRCH == join_thread(&copied_thread)) ;
   TEST(0 == delete_thread(&thread)) ;

   return 0 ;
ONABORT:
   atomicwrite_int(&s_thread_signal, 1) ;
   delete_thread(&thread) ;
   return EINVAL ;
}

static memblock_t       s_sigaltstack_signalstack ;
static pthread_t        s_sigaltstack_threadid ;
static volatile int     s_sigaltstack_returncode ;

static void sigusr1handler(int sig)
{
   int errno_backup = errno ;
   if (  sig == SIGUSR1
         && 0 != pthread_equal(s_sigaltstack_threadid, pthread_self())
         && s_sigaltstack_signalstack.addr < (uint8_t*)&sig
         && (uint8_t*)&sig < s_sigaltstack_signalstack.addr+s_sigaltstack_signalstack.size) {
      s_sigaltstack_returncode = 0 ;
   } else {
      s_sigaltstack_returncode = EINVAL ;
   }
   errno = errno_backup ;
}

static int thread_sigaltstack(intptr_t dummy)
{
   (void) dummy ;
   signalstack_threadtls(genericcast_threadtls(self_thread(), tls_), &s_sigaltstack_signalstack) ;
   s_sigaltstack_threadid    = pthread_self() ;
   s_sigaltstack_returncode  = EINVAL ;
   TEST(0 == pthread_kill(pthread_self(), SIGUSR1)) ;
   TEST(0 == s_sigaltstack_returncode) ;
   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_sigaltstack(void)
{
   int err = 1 ;
   thread_t *  thread       = 0 ;
   memblock_t  altstack     = memblock_FREE ;
   stack_t     oldst ;
   sigset_t    oldprocmask ;
   struct
   sigaction   newact, oldact ;
   bool        isStack    = false ;
   bool        isProcmask = false ;
   bool        isAction   = false ;

   // prepare
   static_assert(SIGSTKSZ <= 16384, "altstack is big enough") ;
   TEST(0 == ALLOC_PAGECACHE(pagesize_16384, &altstack)) ;

   sigemptyset(&newact.sa_mask) ;
   sigaddset(&newact.sa_mask, SIGUSR1) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK, &newact.sa_mask, &oldprocmask)) ;
   isProcmask = true ;

   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags   = SA_ONSTACK ;
   newact.sa_handler = &sigusr1handler ;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact)) ;
   isAction = true ;

   // TEST sigusr1handler: signal self_thread()
   stack_t  newst = {
      .ss_sp    = altstack.addr,
      .ss_size  = altstack.size,
      .ss_flags = 0
   } ;
   TEST(0 == sigaltstack(&newst, &oldst)) ;
   isStack = true ;
   s_sigaltstack_threadid    = pthread_self() ;
   s_sigaltstack_signalstack = altstack ;
   s_sigaltstack_returncode  = EINVAL ;
   TEST(0 == pthread_kill(pthread_self(), SIGUSR1)) ;
   TEST(0 == s_sigaltstack_returncode) ;

   // TEST newgeneric_thread: test that own signal stack is used
   // thread 'thread_sigaltstack' runs under its own sigaltstack in sigusr1handler with signal SIGUSR1
   TEST(0 == newgeneric_thread(&thread, &thread_sigaltstack, 0)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;

   // unprepare
   TEST(0 == sigaltstack(&oldst, 0)) ;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
   TEST(0 == sigaction(SIGUSR1, &oldact, 0)) ;
   TEST(0 == RELEASE_PAGECACHE(&altstack)) ;

   return 0 ;
ONABORT:
   delete_thread(&thread) ;
   if (isStack)      sigaltstack(&oldst, 0) ;
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isAction)     sigaction(SIGUSR1, &oldact, 0) ;
   RELEASE_PAGECACHE(&altstack) ;
   return err ;
}

static int thread_callabort(void * dummy)
{
   (void) dummy ;
   abort_thread() ;
   return 0 ;
}

static volatile int s_callsetcontinue_isabort = 0 ;

static int thread_callsetcontinue(void * dummy)
{
   (void) dummy ;

   volatile bool is_abort = false ;

   s_callsetcontinue_isabort = 0 ;
   setreturncode_thread(self_thread(), 0) ;

   if (setcontinue_thread(&is_abort)) {
      return EINVAL ;
   }

   if (is_abort != s_callsetcontinue_isabort) {
      return EINVAL ;
   }

   if (!is_abort) {
      s_callsetcontinue_isabort = 1 ;
      abort_thread() ;
   }

   if (ENOTRECOVERABLE != returncode_thread(self_thread())) {
      return EINVAL ;
   }

   return 0 ;
}

static int test_abort(void)
{
   thread_t *  thread = 0 ;

   // TEST abort_thread
   TEST(0 == new_thread(&thread, &thread_callabort, 0)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(ENOTRECOVERABLE == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST setcontinue_thread
   TEST(0 == new_thread(&thread, &thread_callsetcontinue, 0)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;

   return 0 ;
ONABORT:
   delete_thread(&thread) ;
   return EINVAL ;
}

static volatile int s_stackoverflow_issignal = 0 ;

static void sigstackoverflow(int sig)
{
   (void) sig ;
   s_stackoverflow_issignal = 1 ;
   abort_thread() ;
}

static int thread_stackoverflow(void * argument)
{
   (void) argument ;
   s_stackoverflow_issignal = 0 ;

   if (!s_stackoverflow_issignal) {
      (void) thread_stackoverflow(0) ;
   }

   return 0 ;
}

static int test_stackoverflow(void)
{
   sigset_t          oldprocmask ;
   struct sigaction  newact, oldact ;
   thread_t *        thread     = 0 ;
   thread_t *        mainthread = self_thread() ;
   bool              isProcmask = false ;
   bool              isAction   = false ;

   // prepare
   sigemptyset(&newact.sa_mask) ;
   sigaddset(&newact.sa_mask, SIGSEGV) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK, &newact.sa_mask, &oldprocmask)) ;
   isProcmask = true ;

   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags = SA_ONSTACK ;
   newact.sa_handler = &sigstackoverflow ;
   TEST(0 == sigaction(SIGSEGV, &newact, &oldact)) ;
   isAction = true ;

   // TEST abort_thread: abort_thread can recover from stack overflow (detected with signal SIGSEGV)
   TEST(0 == new_thread(&thread, &thread_stackoverflow, 0)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(1 == s_stackoverflow_issignal) ;
   TEST(maintask_thread(thread)   == (thread_f)&thread_stackoverflow) ;
   TEST(mainarg_thread(thread)    == 0) ;
   TEST(returncode_thread(thread) == ENOTRECOVERABLE) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST abort_thread: own thread can do so also
   setreturncode_thread(mainthread, 0) ;
   s_stackoverflow_issignal = 0 ;
   bool is_abort = false ;
   TEST(0 == setcontinue_thread(&is_abort)) ;
   if (!is_abort) {
      TEST(0 == pthread_kill(pthread_self(), SIGSEGV)) ;
   }
   TEST(1 == s_stackoverflow_issignal) ;
   TEST(1 == is_abort) ;
   TEST(returncode_thread(mainthread) == ENOTRECOVERABLE) ;
   setreturncode_thread(mainthread, 0) ;

   // unprepare
   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
   TEST(0 == sigaction(SIGSEGV, &oldact, 0)) ;

   return 0 ;
ONABORT:
   delete_thread(&thread) ;
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isAction)     sigaction(SIGSEGV, &oldact, 0) ;
   return EINVAL ;
}


typedef struct thread_isvalidstack_t   thread_isvalidstack_t ;

struct thread_isvalidstack_t
{
   bool           isValid[30] ;
   thread_t *     thread[30] ;
   memblock_t     signalstack[30] ;
   memblock_t     threadstack[30] ;
   volatile bool  isvalid ;
} ;

static int thread_isvalidstack(thread_isvalidstack_t * startarg)
{
   stack_t  current_sigaltstack ;

   if (  0 != sigaltstack(0, &current_sigaltstack)
         || 0 != current_sigaltstack.ss_flags) {
      goto ONABORT ;
   }

   while (! startarg->isvalid) {
      yield_thread() ;
   }

   for (unsigned i = 0; i < lengthof(startarg->isValid); ++i) {
      if (  startarg->thread[i]->sys_thread == pthread_self()
            && startarg->thread[i] != self_thread()) {
         goto ONABORT ;
      }
   }

   unsigned tid ;
   for (tid = 0; tid < lengthof(startarg->isValid); ++tid) {
      if (  startarg->thread[tid] == self_thread()
            && startarg->thread[tid]->sys_thread == pthread_self()) {
         startarg->isValid[tid] = true ;
         break ;
      }
   }

   if (tid == lengthof(startarg->isValid)) {
      goto ONABORT ;
   }

   if (  startarg->signalstack[tid].addr != current_sigaltstack.ss_sp
         || startarg->signalstack[tid].size != current_sigaltstack.ss_size) {
      goto ONABORT;
   }

   if (  startarg->threadstack[tid].addr >= (uint8_t*)&startarg
         || (uint8_t*)&startarg >= startarg->threadstack[tid].addr + startarg->threadstack[tid].size) {
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_manythreads(void)
{
   thread_isvalidstack_t   startarg = { .isvalid = false } ;

   // TEST newgeneric_thread: every thread has its own stackframe + self_thread
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(0 == newgeneric_thread(&startarg.thread[i], thread_isvalidstack, &startarg)) ;
      signalstack_threadtls(genericcast_threadtls(startarg.thread[i], tls_), &startarg.signalstack[i]) ;
      threadstack_threadtls(genericcast_threadtls(startarg.thread[i], tls_), &startarg.threadstack[i]) ;
   }
   // startarg initialized
   startarg.isvalid = true ;
   // wait for exit of all threads and check returncode == OK
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(0 == join_thread(startarg.thread[i])) ;
      TEST(0 == returncode_thread(startarg.thread[i])) ;
   }
   // check all threads were executed
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(startarg.isValid[i]) ;
   }
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(0 == delete_thread(&startarg.thread[i])) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      delete_thread(&startarg.thread[i]) ;
   }
   return EINVAL ;
}

/* function: wait_for_signal
 * Waits for a blocked signal until it has been received.
 * Implemented with POSIX function <sigwaitinfo>.
 * Returns 0 if the signal with number signr has been received. */
static int wait_for_signal(int signr)
{
   int err ;
   sigset_t signalmask ;

   err = sigemptyset(&signalmask) ;
   if (err) return EINVAL ;

   err = sigaddset(&signalmask, signr) ;
   if (err) return EINVAL ;

   do {
      err = sigwaitinfo(&signalmask, 0) ;
   } while (-1 == err && EINTR == errno) ;

   return err == -1 ? errno : err == signr ? 0 : EINVAL ;
}

/* function: wait_for_signal
 * Polls for a blocked signal.
 * Implemented with POSIX function <sigtimedwait>.
 * Returns 0 if the signal with number signr has been received. */
static int poll_for_signal(int signr)
{
   int err ;
   sigset_t signalmask ;

   err = sigemptyset(&signalmask) ;
   if (err) return EINVAL ;

   err = sigaddset(&signalmask, signr) ;
   if (err) return EINVAL ;

   struct timespec   ts = { 0, 0 } ;
   err = sigtimedwait(&signalmask, 0, &ts) ;

   return err == -1 ? errno : err == signr ? 0 : EINVAL ;
}

static int thread_sendsignal2thread(thread_t * receiver)
{
   int err ;
   err = pthread_kill(receiver->sys_thread, SIGUSR1) ;
   assert(0 == err) ;
   return err ;
}

static int thread_sendsignal2process(int dummy)
{
   int err ;
   (void) dummy ;
   err = kill(getpid(), SIGUSR1) ;
   return err ;
}

static int thread_receivesignal(int dummy)
{
   int err ;
   (void) dummy ;
   err = wait_for_signal(SIGUSR1) ;
   return err ;
}

static int thread_receivesignal2(int dummy)
{
   int err ;
   (void) dummy ;
   err = wait_for_signal(SIGUSR2) ;
   return err ;
}


static int thread_receivesignalrt(int dummy)
{
   int err ;
   (void) dummy ;
   err = wait_for_signal(SIGRTMIN) ;
   return err ;
}

static int test_signal(void)
{
   thread_t *        thread1         = 0 ;
   thread_t *        thread2         = 0 ;
   struct timespec   ts              = { 0, 0 } ;
   bool              isoldsignalmask = false ;
   sigset_t          oldsignalmask ;
   sigset_t          signalmask ;

   // prepare
   TEST(0 == sigemptyset(&signalmask)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR2)) ;
   TEST(0 == sigaddset(&signalmask, SIGRTMIN)) ;
   TEST(0 == sigprocmask(SIG_BLOCK, &signalmask, &oldsignalmask)) ;
   isoldsignalmask = true ;

   // TEST pthread_kill: main thread receives from 1st thread
   TEST(0 == newgeneric_thread(&thread1, thread_sendsignal2thread, self_thread())) ;
   TEST(0 == wait_for_signal(SIGUSR1)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == delete_thread(&thread1)) ;

   // TEST pthread_kill: 2nd thread receives from 1st thread
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ; // empty signal queue
   TEST(0 == newgeneric_thread(&thread2, thread_receivesignal, 0)) ;
   TEST(0 == newgeneric_thread(&thread1, thread_sendsignal2thread, thread2)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;

   // TEST pthread_kill: main thread can not receive from 1st thread if it sends to 2nd thread
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ; // empty signal queue
   TEST(0 == newgeneric_thread(&thread2, thread_receivesignal2, 0)) ;
   TEST(0 == newgeneric_thread(&thread1, thread_sendsignal2thread, thread2)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts)) ;
   TEST(EAGAIN == errno) ;
   TEST(0 == pthread_kill(thread2->sys_thread, SIGUSR2)) ; // wake up thread2
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;

   // TEST kill(): send signal to process => main thread receives !
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ; // empty signal queue
   TEST(0 == newgeneric_thread(&thread1, thread_sendsignal2process, 0)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == wait_for_signal(SIGUSR1)) ;
   TEST(0 == delete_thread(&thread1)) ;

   // TEST kill(): send signal to process => second thread receives !
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ; // empty signal queue
   TEST(0 == newgeneric_thread(&thread1, thread_sendsignal2process, 0)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == newgeneric_thread(&thread2, thread_receivesignal, 0)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts)) ; // consumed by second thread
   TEST(EAGAIN == errno) ;

   // TEST kill: SIGUSR1 is not stored into queue
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ; // empty signal queue
   TEST(0 == kill(getpid(), SIGUSR1)) ;
   TEST(0 == kill(getpid(), SIGUSR1)) ;
   TEST(0 == kill(getpid(), SIGUSR1)) ;
   TEST(0 == newgeneric_thread(&thread1, &thread_receivesignal, 0)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts)) ; // only one signal
   TEST(EAGAIN == errno) ;

   // TEST kill: SIGRTMIN (POSIX realtime signals) is stored into internal queue
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ; // empty signal queue
   TEST(0 == kill(getpid(), SIGRTMIN)) ;
   TEST(0 == kill(getpid(), SIGRTMIN)) ;
   TEST(0 == kill(getpid(), SIGRTMIN)) ;
   TEST(0 == newgeneric_thread(&thread1, &thread_receivesignalrt, 0)) ;
   TEST(0 == newgeneric_thread(&thread2, &thread_receivesignalrt, 0)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(SIGRTMIN == sigtimedwait(&signalmask, 0, &ts)) ; // 3rd queued signal
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts)) ;       // no 4rd
   TEST(EAGAIN == errno) ;

   // unprepare
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ; // empty signal queue
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0)) ;

   return 0 ;
ONABORT:
   delete_thread(&thread1) ;
   delete_thread(&thread2) ;
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ; // empty signal queue
   if (isoldsignalmask) sigprocmask(SIG_SETMASK, &oldsignalmask, 0) ;
   return EINVAL ;
}

// == test_suspendresume ==

static int thread_suspend(intptr_t signr)
{
   int err ;
   err = send_signalrt((signalrt_t)signr, 0) ;
   if (!err) {
      suspend_thread() ;
      err = send_signalrt((signalrt_t)(signr+1), 0) ;
   }
   return err ;
}

static int thread_resume(thread_t * receiver)
{
   resume_thread(receiver) ;
   return 0 ;
}

static int thread_waitsuspend(intptr_t signr)
{
   int err ;
   err = wait_signalrt((signalrt_t)signr, 0) ;
   if (!err) {
      suspend_thread() ;
   }
   return err ;
}

static int test_suspendresume(void)
{
   thread_t * thread1 = 0 ;
   thread_t * thread2 = 0 ;

   // TEST resume_thread: uses SIGINT (not queued, only single instance)
   TEST(EAGAIN == poll_for_signal(SIGINT)) ;
   resume_thread(self_thread()) ;
   TEST(0 == poll_for_signal(SIGINT)) ;
   TEST(EAGAIN == poll_for_signal(SIGINT)) ;

   // TEST trysuspend_thread
   for (unsigned i = 0; i < 100; ++i) {
      TEST(EAGAIN == trysuspend_thread()) ;
      resume_thread(self_thread()) ;
      TEST(0 == trysuspend_thread()) ;
      TEST(EAGAIN == trysuspend_thread()) ;
   }

   // TEST suspend_thread: thread suspends
   TEST(EAGAIN == trywait_signalrt(0, 0)) ;
   TEST(EAGAIN == trywait_signalrt(1, 0)) ;
   TEST(0 == newgeneric_thread(&thread1, thread_suspend, 0)) ;
   TEST(0 == wait_signalrt(0, 0)) ;
   TEST(EAGAIN == trywait_signalrt(1, 0)) ;
   // now suspended

   // TEST resume_thread: main thread resumes suspended thread
   resume_thread(thread1) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == trywait_signalrt(1, 0)) ;
   TEST(0 == delete_thread(&thread1)) ;

   // TEST resume_thread: other threads resume suspended thread
   TEST(EAGAIN == trywait_signalrt(0, 0)) ;
   TEST(EAGAIN == trywait_signalrt(1, 0)) ;
   TEST(0 == newgeneric_thread(&thread1, &thread_suspend, 0)) ;
   TEST(0 == wait_signalrt(0, 0)) ;
   TEST(EAGAIN == trywait_signalrt(1, 0)) ;
   TEST(0 == newgeneric_thread(&thread2, &thread_resume, thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == trywait_signalrt(1, 0)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;

   // TEST resume_thread: main thread resumes threads before they called suspend_thread
   //                     test that resume_thread is preserved !
   TEST(EAGAIN == trywait_signalrt(0, 0)) ;
   TEST(EAGAIN == trywait_signalrt(1, 0)) ;
   TEST(0 == newgeneric_thread(&thread1, thread_waitsuspend, 0)) ;
   TEST(0 == newgeneric_thread(&thread2, thread_waitsuspend, 0)) ;
   resume_thread(thread1) ;
   resume_thread(thread2) ;
   TEST(0 == send_signalrt(0, 0)) ; // start threads
   TEST(0 == send_signalrt(0, 0)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;

   // TEST resume_thread: main resumes itself
   for (unsigned i = 0; i < 100; ++i) {
      resume_thread(self_thread()) ;
      suspend_thread() ;   // consumes SIGINT
      TEST(EAGAIN == poll_for_signal(SIGINT)) ;
   }

   return 0 ;
ONABORT:
   delete_thread(&thread1) ;
   delete_thread(&thread2) ;
   return EINVAL ;
}

int test_sleep(void)
{
   struct timeval tv ;
   struct timeval tv2 ;
   int32_t msec ;

   // TEST 250 msec
   TEST(0 == gettimeofday(&tv, 0)) ;
   sleepms_thread(250) ;
   TEST(0 == gettimeofday(&tv2, 0)) ;

   msec = 1000 * (tv2.tv_sec - tv.tv_sec) + (tv2.tv_usec - tv.tv_usec) / 1000 ;
   TEST(msec > 200) ;
   TEST(msec < 300) ;

   // TEST 100 msec
   TEST(0 == gettimeofday(&tv, 0)) ;
   sleepms_thread(100) ;
   TEST(0 == gettimeofday(&tv2, 0)) ;

   msec = 1000 * (tv2.tv_sec - tv.tv_sec) + (tv2.tv_usec - tv.tv_usec) / 1000 ;
   TEST(msec > 80) ;
   TEST(msec < 120) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

// == test_yield ==

static uint32_t s_countyield_counter   = 0 ;
static uint32_t s_countnoyield_counter = 0 ;
static int      s_countyield_exit      = 0 ;

static int thread_countyield(void * dummy)
{
   (void) dummy ;

   atomicwrite_int(&s_countyield_counter, 0) ;

   while (  s_countyield_counter < 10000000
            && ! atomicread_int(&s_countyield_exit)) {
      yield_thread() ;
      atomicadd_int(&s_countyield_counter, 1) ;
   }

   if (! atomicread_int(&s_countyield_exit)) {
      atomicwrite_int(&s_countyield_counter, 0) ;
   }

   return 0 ;
}

static int thread_countnoyield(void * dummy)
{
   (void) dummy ;

   atomicwrite_int(&s_countnoyield_counter, 0) ;

   while (  s_countnoyield_counter < 10000000
            && ! atomicread_int(&s_countyield_exit)) {
      // give other thread a chance to run
      if (s_countnoyield_counter < 3) yield_thread() ;
      atomicadd_int(&s_countnoyield_counter , 1) ;
   }

   atomicwrite_int(&s_countnoyield_counter, 0) ;

   return 0 ;
}

static int test_yield(void)
{
   thread_t * thread_yield   = 0 ;
   thread_t * thread_noyield = 0 ;

   // TEST yield_thread
   s_countyield_exit = 0 ;
   TEST(0 == new_thread(&thread_yield, &thread_countyield, 0)) ;
   TEST(0 == new_thread(&thread_noyield, &thread_countnoyield, 0)) ;
   TEST(0 == join_thread(thread_noyield)) ;
   atomicwrite_int(&s_countyield_exit, 1) ;
   TEST(0 == join_thread(thread_yield)) ;
   // no yield ready
   TEST(0 == atomicread_int(&s_countnoyield_counter)) ;
   // yield not ready
   TEST(0 != atomicread_int(&s_countyield_counter)) ;
   TEST(s_countyield_counter < 1000000) ;
   TEST(0 == delete_thread(&thread_noyield)) ;
   TEST(0 == delete_thread(&thread_yield)) ;

   return 0 ;
ONABORT:
   s_countyield_exit = 1 ;
   delete_thread(&thread_noyield) ;
   delete_thread(&thread_yield) ;
   return EINVAL ;
}

// == test_exit ==

static int thread_callexit(intptr_t retval)
{
   exit_thread((int)retval) ;
   while (1) {
      sleepms_thread(1000) ;
   }
   return 0 ;
}

static int test_exit(void)
{
   thread_t * thread[20] = { 0 } ;

   // TEST exit_thread: called from created thread
   for (int i = 0; i < (int)lengthof(thread); ++i) {
      TEST(0 == newgeneric_thread(&thread[i], &thread_callexit, (intptr_t)i)) ;
   }
   for (int i = 0; i < (int)lengthof(thread); ++i) {
      TEST(0 == join_thread(thread[i])) ;
      TEST(i == returncode_thread(thread[i])) ;
      TEST(0 == delete_thread(&thread[i])) ;
   }

   // TEST exit_thread: EPROTO
   TEST(ismain_thread(self_thread())) ;
   TEST(EPROTO == exit_thread(0)) ;

   return 0 ;
ONABORT:
   for (int i = 0; i < (int)lengthof(thread); ++i) {
      delete_thread(&thread[i]) ;
   }
   return EINVAL ;
}

static int thread_lockflag(int * runcount)
{
   while (0 == atomicread_int(&self_thread()->lockflag)) {
      yield_thread() ;
   }
   atomicadd_int(runcount, 1) ;
   lockflag_thread(self_thread()) ;
   atomicsub_int(runcount, 1) ;
   return 0 ;
}

static int test_update(void)
{
   thread_t    thread   = { .lockflag = 0 } ;
   thread_t *  thread2  = 0 ;
   int         runcount = 0 ;

   // TEST settask_thread
   settask_thread(&thread, &thread_donothing, (void*)10) ;
   TEST(maintask_thread(&thread) == &thread_donothing) ;
   TEST(mainarg_thread(&thread)  == (void*)10) ;
   settask_thread(&thread, 0, 0) ;
   TEST(maintask_thread(&thread) == 0) ;
   TEST(mainarg_thread(&thread)  == 0) ;

   // TEST setreturncode_thread
   setreturncode_thread(&thread, 1) ;
   TEST(1 == returncode_thread(&thread)) ;
   setreturncode_thread(&thread, 0) ;
   TEST(0 == returncode_thread(&thread)) ;

   // TEST lockflag_thread
   lockflag_thread(&thread) ;
   TEST(0 != thread.lockflag) ;

   // TEST unlockflag_thread
   unlockflag_thread(&thread) ;
   TEST(0 == thread.lockflag) ;

   // TEST lockflag_thread: waits
   TEST(0 == newgeneric_thread(&thread2, thread_lockflag, &runcount)) ;
   TEST(0 == thread2->lockflag) ;
   lockflag_thread(thread2) ;
   TEST(0 != thread2->lockflag) ;
   while (0 == atomicread_int(&runcount)) {
      yield_thread() ;
   }
   // waits until unlock
   for (int i = 0; i < 5; ++i) {
      yield_thread() ;
      TEST(1 == atomicread_int(&runcount)) ;
   }
   unlockflag_thread(thread2) ;
   TEST(0 == join_thread(thread2)) ;

   // TEST unlockflag_thread: works from another thread
   TEST(0 != atomicread_int(&thread2->lockflag)) ;
   unlockflag_thread(thread2) ;
   TEST(0 == thread2->lockflag) ;
   TEST(0 == delete_thread(&thread2)) ;

   return 0 ;
ONABORT:
   delete_thread(&thread2) ;
   return EINVAL ;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_FREE ;

   if (test_exit())                    goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())                goto ONABORT ;
   if (test_mainthread())              goto ONABORT ;
   if (test_query())                   goto ONABORT ;
   if (test_join())                    goto ONABORT ;
   if (test_sigaltstack())             goto ONABORT ;
   if (test_abort())                   goto ONABORT ;
   if (test_stackoverflow())           goto ONABORT ;
   if (test_manythreads())             goto ONABORT ;
   if (test_signal())                  goto ONABORT ;
   if (test_suspendresume())           goto ONABORT ;
   if (test_sleep())                   goto ONABORT ;
   if (test_yield())                   goto ONABORT ;
   if (test_exit())                    goto ONABORT ;
   if (test_update())                  goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_platform_task_thread()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONABORT:
   return EINVAL;
}

#endif
