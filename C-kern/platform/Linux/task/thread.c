/* title: Thread Linux
   Implements <Thread>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/sync/semaphore.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/platform/task/thread_localstore.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/io/writer/log/logwriter.h"
#include "C-kern/api/platform/task/process.h"
#include "C-kern/api/time/timevalue.h"
#include "C-kern/api/time/sysclock.h"
#endif


/* struct: thread_startargument_t
 * Startargument of the started system thread.
 * The function <main_thread> then calls the main task of the thread which
 * is stored in <thread_t.main>. */
typedef struct thread_startargument_t {
   thread_t *  self;
   stack_t     signalstack;
} thread_startargument_t;


// section: thread_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_thread_errtimer
 * Simulates an error in <new_thread>. */
static test_errortimer_t   s_thread_errtimer = test_errortimer_FREE;
#endif

// group: helper

/* function: main_thread
 * The start function of the thread.
 * This is the same for all threads.
 * It initializes signalstack, <threadcontext_t>, and
 * calls the user supplied main function. */
static void* main_thread(thread_startargument_t * startarg)
{
   int err;

   thread_t* thread = self_thread();

   assert(startarg->self == thread);

   thread->sys_thread = pthread_self();

   err = init_threadcontext(&thread->threadcontext, maincontext_threadcontext(&thread->threadcontext)->type);
   if (err) {
      TRACECALL_ERRLOG("init_threadcontext", err);
      goto ONERR_NOABORT;
   }

   if (sys_thread_FREE == pthread_self()) {
      err = EINVAL;
      TRACE_ERRLOG(log_flags_NONE, FUNCTION_WRONG_RETURNVALUE, "pthread_self", STR(sys_thread_FREE));
      goto ONERR;
   }

   // do not access startarg after sigaltstack (startarg is stored on this stack)
   err = sigaltstack(&startarg->signalstack, (stack_t*)0);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigaltstack", err);
      goto ONERR;
   }
   startarg = 0;

   if (0 != getcontext(&thread->continuecontext)) {
      err = errno;
      TRACESYSCALL_ERRLOG("getcontext", err);
      goto ONERR;
   }

   thread = self_thread();

   if (  0 == thread->returncode  // abort_thread sets returncode to ENOTRECOVERABLE
         && thread->main_task) {
      thread->returncode = thread->main_task(thread->main_arg);
   }

   err = free_threadcontext(&thread->threadcontext);
   if (err) {
      TRACECALL_ERRLOG("free_threadcontext",err);
      goto ONERR;
   }

   return (void*)0;
ONERR:
   abort_maincontext(err);
ONERR_NOABORT:
   setreturncode_thread(self_thread(), err);
   TRACEEXIT_ERRLOG(err);
   return (void*)err;
}

// group: lifetime

int delete_thread(thread_t ** thread)
{
   int err;
   int err2;
   thread_t* delobj = *thread;

   if (delobj) {
      if (ismain_thread(delobj)) {
         err = EINVAL;
         goto ONERR;
      }

      *thread = 0;

      err = join_thread(delobj);

      thread_localstore_t* tls = castPthread_threadlocalstore(delobj);

      err2 = delete_threadlocalstore(&tls);
      if (err2) err = err2;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int new_thread(/*out*/thread_t** thread, thread_f thread_main, void * main_arg)
{
   int err;
   pthread_attr_t    thread_attr;
   thread_t *        newthread = 0;
   thread_localstore_t * tls   = 0;
   bool              isThreadAttrValid = false;
   memblock_t        stack;
   memblock_t        signalstack;

   if (! PROCESS_testerrortimer(&s_thread_errtimer, &err)) {
      err = new_threadlocalstore(&tls, &stack, &signalstack, pagesize_vm());
   }
   if (err) goto ONERR;

   newthread = thread_threadlocalstore(tls);
   newthread->main_task  = thread_main;
   newthread->main_arg   = main_arg;

   thread_startargument_t * startarg   = (thread_startargument_t*) signalstack.addr;

   startarg->self         = newthread;
   startarg->signalstack  = (stack_t) { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size };

   if (! PROCESS_testerrortimer(&s_thread_errtimer, &err)) {
      err = pthread_attr_init(&thread_attr);
   }
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_attr_init",err);
      goto ONERR;
   }
   isThreadAttrValid = true;

   err = pthread_attr_setstack(&thread_attr, stack.addr, stack.size);
   (void) PROCESS_testerrortimer(&s_thread_errtimer, &err);
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_attr_setstack",err);
      PRINTPTR_ERRLOG(stack.addr);
      PRINTSIZE_ERRLOG(stack.size);
      goto ONERR;
   }

   sys_thread_t sys_thread;
   static_assert( (void* (*) (typeof(startarg)))0 == (typeof(&main_thread))0, "main_thread has argument of type startarg");
   if (! PROCESS_testerrortimer(&s_thread_errtimer, &err)) {
      err = pthread_create(&sys_thread, &thread_attr, (void*(*)(void*))&main_thread, startarg);
   }
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_create",err);
      goto ONERR;
   }
   // variable is set after creation ==> set it in main_thread too
   newthread->sys_thread = sys_thread;

   err = pthread_attr_destroy(&thread_attr);
   isThreadAttrValid = false;
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_attr_destroy",err);
      abort_maincontext(err);
   }

   *thread = newthread;

   return 0;
ONERR:
   if (isThreadAttrValid) {
      (void) pthread_attr_destroy(&thread_attr);
   }
   (void) delete_threadlocalstore(&tls);
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: synchronize

int join_thread(thread_t* thread)
{
   int err;

   if (sys_thread_FREE != thread->sys_thread) {

      err = pthread_join(thread->sys_thread, 0);
      if (err != EDEADLK) {
         thread->sys_thread = sys_thread_FREE;
      }

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int tryjoin_thread(thread_t* thread)
{
   int err = 0;

   if (sys_thread_FREE != thread->sys_thread) {

      err = pthread_tryjoin_np(thread->sys_thread, 0);

      if (0 == err) {
         thread->sys_thread = sys_thread_FREE;
      }
   }

   return err;
}

// group: change-run-state

void suspend_thread()
{
   int err;
   sigset_t signalmask;

   err = sigemptyset(&signalmask);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigemptyset", err);
      goto ONERR;
   }

   err = sigaddset(&signalmask, SIGINT);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigaddset", err);
      goto ONERR;
   }

   do {
      err = sigwaitinfo(&signalmask, 0);
   } while (-1 == err && EINTR == errno);

   if (-1 == err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigwaitinfo", err);
      goto ONERR;
   }

   return;
ONERR:
   abort_maincontext(err);
}

int trysuspend_thread(void)
{
   int err;
   sigset_t signalmask;

   err = sigemptyset(&signalmask);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigemptyset", err);
      goto ONERR;
   }

   err = sigaddset(&signalmask, SIGINT);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigaddset", err);
      goto ONERR;
   }

   struct timespec   ts = { 0, 0 };
   err = sigtimedwait(&signalmask, 0, &ts);
   if (-1 == err) return EAGAIN;

   return 0;
ONERR:
   abort_maincontext(err);
   return err;
}

void resume_thread(thread_t* thread)
{
   int err;

   if (sys_thread_FREE != thread->sys_thread) {
      err = pthread_kill(thread->sys_thread, SIGINT);
      if (err) {
         if (err == ESRCH) {
            if (0 == tryjoin_thread(thread)) return; // OK
         }
         TRACESYSCALL_ERRLOG("pthread_kill", err);
         goto ONERR;
      }
   }

   return;
ONERR:
   abort_maincontext(err);
}

void interrupt_thread(thread_t* thread)
{
   int err;

   if (sys_thread_FREE != thread->sys_thread) {
      err = pthread_kill(thread->sys_thread, SIGQUIT);
      if (err) {
         if (err == ESRCH) {
            if (0 == tryjoin_thread(thread)) return; // OK
         }
         TRACESYSCALL_ERRLOG("pthread_kill", err);
         goto ONERR;
      }
   }

   return;
ONERR:
   abort_maincontext(err);
}

void sleepms_thread(uint32_t msec)
{
   struct timespec reqtime = { .tv_sec = (int32_t) (msec / 1000), .tv_nsec = (int32_t) ((msec%1000) * 1000000) };

   int err;
   err = nanosleep(&reqtime, 0);

   if (-1 == err) {
      err = errno;
      if (err != EINTR) {
         TRACESYSCALL_ERRLOG("nanosleep", err);
         goto ONERR;
      }
   }

   return;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return;
}

int exit_thread(int retcode)
{
   int err;
   thread_t * thread = self_thread();

   VALIDATE_STATE_TEST(! ismain_thread(thread), ONERR, );

   setreturncode_thread(thread, retcode);

   err = free_threadcontext(&thread->threadcontext);
   if (err) {
      TRACECALL_ERRLOG("free_threadcontext",err);
      abort_maincontext(err);
   }

   pthread_exit(0);
   assert(0);

ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: abort

void abort_thread(void)
{
   thread_t * thread = self_thread();
   setreturncode_thread(thread, ENOTRECOVERABLE);
   setcontext(&thread->continuecontext);
   assert(0);
}


// group: test

#ifdef KONFIG_UNITTEST

static unsigned      s_thread_runcount = 0;
static unsigned      s_thread_signal   = 0;
static sys_thread_t  s_thread_id       = 0;

static int thread_donothing(void * dummy)
{
   (void) dummy;
   return 0;
}

static int thread_returncode(intptr_t retcode)
{
   s_thread_id = pthread_self();
   add_atomicint(&s_thread_runcount, 1);
   while (0 == read_atomicint(&s_thread_signal)) {
      pthread_yield();
   }
   s_thread_signal = 0;
   sub_atomicint(&s_thread_runcount, 1);
   return (int) retcode;
}

static int test_initfree(void)
{
   thread_t * thread = 0;
   thread_t  sthread = thread_FREE;
   threadcontext_t tcfree = threadcontext_FREE;
   threadcontext_t tcinit = threadcontext_INIT_STATIC((thread_localstore_t*)&sthread);

   // TEST thread_FREE
   TEST( 0 == memcmp(&tcfree, &sthread.threadcontext, sizeof(tcfree)));
   TEST( 0 == sthread.nextwait);
   TEST( 0 == sthread.main_task);
   TEST( 0 == sthread.main_arg);
   TEST( 0 == sthread.returncode);
   TEST( 0 == sthread.lockflag);
   TEST( 0 == sthread.ismain);
   TEST( sys_thread_FREE == sthread.sys_thread);

   // TEST thread_INIT_STATIC
   sthread = (thread_t) thread_INIT_STATIC((thread_localstore_t*)&sthread);
   TEST( 0 == memcmp(&tcinit, &sthread.threadcontext, sizeof(tcinit)));
   TEST( 0 == sthread.nextwait);
   TEST( 0 == sthread.main_task);
   TEST( 0 == sthread.main_arg);
   TEST( 0 == sthread.returncode);
   TEST( 0 == sthread.lockflag);
   TEST( 0 == sthread.ismain);
   TEST( sys_thread_FREE == sthread.sys_thread);

   // TEST new_thread
   TEST(0 == new_thread(&thread, &thread_donothing, (void*)3));
   TEST(thread);
   TEST(thread->nextwait   == 0);
   TEST(thread->lockflag   == 0);
   TEST(sthread.ismain     == 0);
   TEST(thread->main_task  == &thread_donothing);
   TEST(thread->main_arg   == (void*)3);
   TEST(thread->returncode == 0);
   TEST(thread->sys_thread != sys_thread_FREE);

   // TEST delete_thread
   TEST(0 == delete_thread(&thread));
   TEST(0 == thread);
   TEST(0 == delete_thread(&thread));
   TEST(0 == thread);

   // TEST newgeneric_thread: thread is run
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, (intptr_t)14));
   TEST(thread);
   TEST(thread->nextwait   == 0);
   TEST(thread->lockflag   == 0);
   TEST(sthread.ismain     == 0);
   TEST(thread->main_task  == (thread_f)&thread_returncode);
   TEST(thread->main_arg   == (void*)14);
   TEST(thread->returncode == 0);
   TEST(thread->sys_thread != sys_thread_FREE);
   sys_thread_t T = thread->sys_thread;
   // test thread is running
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread();
   }
   TEST(s_thread_id        == T);
   TEST(thread->nextwait   == 0);
   TEST(sthread.ismain     == 0);
   TEST(thread->main_task  == (thread_f)&thread_returncode);
   TEST(thread->main_arg   == (void*)14);
   TEST(thread->returncode == 0);
   TEST(thread->sys_thread == T);
   write_atomicint(&s_thread_signal, 1);
   TEST(0 == delete_thread(&thread));
   TEST(0 == thread);
   TEST(0 == delete_thread(&thread));
   TEST(0 == thread);

   // TEST delete_thread: join_thread called from delete_thread
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, (intptr_t)11));
   TEST(thread);
   TEST(thread->nextwait   == 0);
   TEST(sthread.ismain     == 0);
   TEST(thread->main_task  == (thread_f)&thread_returncode);
   TEST(thread->main_arg   == (void*)11);
   TEST(thread->returncode == 0);
   TEST(thread->sys_thread != sys_thread_FREE);
   T = thread->sys_thread;
   write_atomicint(&s_thread_signal, 1);
   TEST(0 == delete_thread(&thread));
   TEST(0 == read_atomicint(&s_thread_signal)); // => delete has waited until thread has been run
   TEST(T == s_thread_id);

   // TEST new_thread: ERROR
   for (int i = 1; i; ++i) {
      init_testerrortimer(&s_thread_errtimer, (unsigned)i, i);
      int err = newgeneric_thread(&thread, &thread_returncode, (intptr_t)0);
      if (!err) {
         TEST(0 != thread);
         TEST(i == 5)
         break;
      }
      TEST(0 == thread);
      TEST(i == err);
   }
   free_testerrortimer(&s_thread_errtimer);
   write_atomicint(&s_thread_signal, 1);
   TEST(0 == delete_thread(&thread));

   // adapt LOG (stack-addr could be different in new_thread in case of ERROR)
   uint8_t *logbuffer;
   size_t   logsize;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   if (logsize) {
      char * found = (char*)logbuffer;
      while ( (found = strstr(found, "stack.addr=0x")) ) {
         for (found += 13; *found && *found != '\n'; ++found) {
            *found = 'X';
         }
      }
   }

   return 0;
ONERR:
   write_atomicint(&s_thread_signal, 1);
   delete_thread(&thread);
   return EINVAL;
}

static int test_mainthread(void)
{
   thread_t thread = thread_FREE;

   // TEST initmain_thread
   initmain_thread(&thread, &thread_donothing, (void*)2);
   TEST(thread.nextwait   == 0);
   TEST(thread.lockflag   == 0);
   TEST(thread.ismain     == 1);
   TEST(thread.main_task  == &thread_donothing);
   TEST(thread.main_arg   == (void*)2);
   TEST(thread.returncode == 0);
   TEST(thread.sys_thread == pthread_self());

   // TEST initmain_thread: calling twice does no harm
   initmain_thread(&thread, 0, 0);
   TEST(thread.nextwait   == 0);
   TEST(thread.lockflag   == 0);
   TEST(thread.ismain     == 1);
   TEST(thread.main_task  == 0);
   TEST(thread.main_arg   == 0);
   TEST(thread.returncode == 0);
   TEST(thread.sys_thread == pthread_self());

   // TEST delete_thread: EINVAL in case of main thread
   thread = *self_thread();
   thread.ismain = 1;
   thread_t* pthread = &thread;
   TEST(ismain_thread(&thread));
   TEST(EINVAL == delete_thread(&pthread));
   TEST(&thread == pthread);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   thread_t    thread;

   // TEST self_thread
   TEST(self_thread() == thread_threadlocalstore(self_threadlocalstore()));
   TEST(&self_thread()->threadcontext == sys_tcontext_syscontext());

   // TEST returncode_thread
   for (int R = -10; R <= 10; ++R) {
      setreturncode_thread(&thread, R);
      TEST(R == returncode_thread(&thread));
   }

   // TEST maintask_thread
   settask_thread(&thread, (thread_f)&thread_returncode, 0);
   TEST(maintask_thread(&thread) == (thread_f)&thread_returncode);
   settask_thread(&thread, 0, 0);
   TEST(maintask_thread(&thread) == 0);

   // TEST mainarg_thread
   for (uintptr_t A = 0; A <= 10; ++A) {
      settask_thread(&thread, 0, (void*)A);
      TEST(A == (uintptr_t)mainarg_thread(&thread));
   }

   // TEST ismain_thread
   thread_t* mainthread = self_thread();
   TEST(1 == ismain_thread(mainthread));
   mainthread->ismain = 0;
   TEST(0 == ismain_thread(mainthread));
   mainthread->ismain = 1;
   TEST(1 == ismain_thread(mainthread));

   return 0;
ONERR:
   return EINVAL;
}

static int test_join(void)
{
   thread_t * thread = 0;

   // TEST join_thread
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, (intptr_t)12));
   write_atomicint(&s_thread_signal, 1);
   TEST(0 == join_thread(thread));
   TEST(0 == read_atomicint(&s_thread_signal));
   TEST(thread->nextwait   == 0);
   TEST(thread->lockflag   == 0);
   TEST(thread->ismain     == 0);
   TEST(thread->main_task  == (thread_f)&thread_returncode);
   TEST(thread->main_arg   == (void*)12);
   TEST(thread->returncode == 12);
   TEST(thread->sys_thread == sys_thread_FREE);

   // TEST join_thread: calling on already joined thread
   TEST(0 == join_thread(thread));
   TEST(thread->nextwait   == 0);
   TEST(thread->lockflag   == 0);
   TEST(thread->ismain     == 0);
   TEST(thread->main_task  == (thread_f)&thread_returncode);
   TEST(thread->main_arg   == (void*)12);
   TEST(thread->returncode == 12);
   TEST(thread->sys_thread == sys_thread_FREE);
   TEST(0 == delete_thread(&thread));

   // TEST tryjoin_thread: EBUSY
   s_thread_signal = 0;
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, (intptr_t)13));
   TEST(EBUSY == tryjoin_thread(thread));
   TEST(sys_thread_FREE != thread->sys_thread);

   // TEST tryjoin_thread
   write_atomicint(&s_thread_signal, 1);
   while(1 == read_atomicint(&s_thread_signal)) {
      sleepms_thread(1);
   }
   for (;;) {
      int err = tryjoin_thread(thread);
      if (!err) break;
      TEST(EBUSY == err);
      TEST(sys_thread_FREE != thread->sys_thread);
   }
   TEST(thread->nextwait   == 0);
   TEST(thread->lockflag   == 0);
   TEST(thread->ismain     == 0);
   TEST(thread->main_task  == (thread_f)&thread_returncode);
   TEST(thread->main_arg   == (void*)13);
   TEST(thread->returncode == 13);
   TEST(thread->sys_thread == sys_thread_FREE);

   // TEST tryjoin_thread: calling on already joined thread
   TEST(0 == tryjoin_thread(thread));
   TEST(thread->nextwait   == 0);
   TEST(thread->lockflag   == 0);
   TEST(thread->ismain     == 0);
   TEST(thread->main_task  == (thread_f)&thread_returncode);
   TEST(thread->main_arg   == (void*)13);
   TEST(thread->returncode == 13);
   TEST(thread->sys_thread == sys_thread_FREE);
   TEST(0 == delete_thread(&thread));

   // TEST join_thread: different returncode
   for (int i = -5; i < 5; ++i) {
      const intptr_t arg = 1111 * i;
      TEST(0 == newgeneric_thread(&thread, thread_returncode, arg));
      TEST(thread->sys_thread != sys_thread_FREE);
      write_atomicint(&s_thread_signal, 1);
      for (int t = 0; t < 2; ++t) {
         TEST(0 == join_thread(thread));
         TEST(0 == read_atomicint(&s_thread_signal));
         TEST(thread->nextwait   == 0);
         TEST(thread->lockflag   == 0);
         TEST(thread->ismain     == 0);
         TEST(thread->main_task  == (thread_f)&thread_returncode);
         TEST(thread->main_arg   == (void*)arg);
         TEST(thread->returncode == arg);
         TEST(thread->sys_thread == sys_thread_FREE);
      }
      TEST(0 == delete_thread(&thread));
   }

   // TEST tryjoin_thread: different returncode
   for (int i = -5; i < 5; ++i) {
      const intptr_t arg = 123 * i;
      s_thread_runcount = 0;
      TEST(0 == newgeneric_thread(&thread, thread_returncode, arg));
      TEST(EBUSY == tryjoin_thread(thread));
      write_atomicint(&s_thread_signal, 1);
      for (int w = 0; w < 10000; ++w) {
         TEST(sys_thread_FREE != thread->sys_thread);
         int err = tryjoin_thread(thread);
         if (!err) break;
         TEST(err == EBUSY);
         sleepms_thread(1);
      }
      for (int t = 0; t < 2; ++t) {
         TEST(thread->nextwait   == 0);
         TEST(thread->lockflag   == 0);
         TEST(thread->ismain     == 0);
         TEST(thread->main_task  == (thread_f)&thread_returncode);
         TEST(thread->main_arg   == (void*)arg);
         TEST(thread->returncode == arg);
         TEST(thread->sys_thread == sys_thread_FREE);
         TEST(0 == tryjoin_thread(thread));
      }
      TEST(0 == delete_thread(&thread));
   }

   // TEST join_thread: EDEADLK
   thread_t selfthread = { .sys_thread = pthread_self() };
   TEST(EDEADLK == join_thread(&selfthread));
   TEST(pthread_self() == selfthread.sys_thread);

   // TEST tryjoin_thread: EDEADLK
   TEST(EDEADLK == tryjoin_thread(&selfthread));
   TEST(pthread_self() == selfthread.sys_thread);

   // prepare
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, (intptr_t)0));
   thread_t copied_thread1 = *thread;
   thread_t copied_thread2 = *thread;
   write_atomicint(&s_thread_signal, 1);
   TEST(0 == join_thread(thread));
   TEST(sys_thread_FREE == thread->sys_thread);
   TEST(0 == returncode_thread(thread));

   // TEST join_thread: ESRCH
   TEST(ESRCH == join_thread(&copied_thread1));
   TEST(sys_thread_FREE == copied_thread1.sys_thread);

   // TEST tryjoin_thread: EBUSY (should be ESRCH, but does not work)
   TEST(EBUSY == tryjoin_thread(&copied_thread2));
   TEST(sys_thread_FREE != copied_thread2.sys_thread);

   // unprepare
   TEST(0 == delete_thread(&thread));

   return 0;
ONERR:
   write_atomicint(&s_thread_signal, 1);
   delete_thread(&thread);
   return EINVAL;
}

static memblock_t       s_sigaltstack_signalstack;
static pthread_t        s_sigaltstack_threadid;
static volatile int     s_sigaltstack_returncode;

static void handler_sigusr1(int sig)
{
   int errno_backup = errno;
   if (  sig == SIGUSR1
         && 0 != pthread_equal(s_sigaltstack_threadid, pthread_self())
         && s_sigaltstack_signalstack.addr < (uint8_t*)&sig
         && (uint8_t*)&sig < s_sigaltstack_signalstack.addr+s_sigaltstack_signalstack.size) {
      s_sigaltstack_returncode = 0;
   } else {
      s_sigaltstack_returncode = EINVAL;
   }
   errno = errno_backup;
}

static int thread_sigaltstack(intptr_t dummy)
{
   (void) dummy;
   signalstack_threadlocalstore(castPthread_threadlocalstore(self_thread()), &s_sigaltstack_signalstack);
   s_sigaltstack_threadid    = pthread_self();
   s_sigaltstack_returncode  = EINVAL;
   TEST(0 == pthread_kill(pthread_self(), SIGUSR1));
   TEST(0 == s_sigaltstack_returncode);
   return 0;
ONERR:
   return EINVAL;
}

static int test_sigaltstack(void)
{
   int err = 1;
   thread_t *  thread       = 0;
   memblock_t  altstack     = memblock_FREE;
   stack_t     oldst;
   sigset_t    oldprocmask;
   struct
   sigaction   newact, oldact;
   bool        isStack    = false;
   bool        isProcmask = false;
   bool        isAction   = false;

   // prepare
   static_assert(SIGSTKSZ <= 16384, "altstack is big enough");
   TEST(0 == ALLOC_PAGECACHE(pagesize_16384, &altstack));

   sigemptyset(&newact.sa_mask);
   sigaddset(&newact.sa_mask, SIGUSR1);
   TEST(0 == sigprocmask(SIG_UNBLOCK, &newact.sa_mask, &oldprocmask));
   isProcmask = true;

   sigemptyset(&newact.sa_mask);
   newact.sa_flags   = SA_ONSTACK;
   newact.sa_handler = &handler_sigusr1;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact));
   isAction = true;

   // TEST sigusr1handler: signal self_thread()
   stack_t  newst = {
      .ss_sp    = altstack.addr,
      .ss_size  = altstack.size,
      .ss_flags = 0
   };
   TEST(0 == sigaltstack(&newst, &oldst));
   isStack = true;
   s_sigaltstack_threadid    = pthread_self();
   s_sigaltstack_signalstack = altstack;
   s_sigaltstack_returncode  = EINVAL;
   TEST(0 == pthread_kill(pthread_self(), SIGUSR1));
   TEST(0 == s_sigaltstack_returncode);

   // TEST newgeneric_thread: test that own signal stack is used
   // thread 'thread_sigaltstack' runs under its own sigaltstack in sigusr1handler with signal SIGUSR1
   TEST(0 == newgeneric_thread(&thread, &thread_sigaltstack, (intptr_t)0));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   // unprepare
   TEST(0 == sigaltstack(&oldst, 0));
   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0));
   TEST(0 == sigaction(SIGUSR1, &oldact, 0));
   TEST(0 == RELEASE_PAGECACHE(&altstack));

   return 0;
ONERR:
   delete_thread(&thread);
   if (isStack)      sigaltstack(&oldst, 0);
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0);
   if (isAction)     sigaction(SIGUSR1, &oldact, 0);
   RELEASE_PAGECACHE(&altstack);
   return err;
}

static int thread_callabort(void * dummy)
{
   (void) dummy;
   abort_thread();
   return 0;
}

static volatile int s_callsetcontinue_isabort = 0;

static int thread_callsetcontinue(void * dummy)
{
   (void) dummy;

   volatile bool is_abort = false;

   s_callsetcontinue_isabort = 0;
   setreturncode_thread(self_thread(), 0);

   if (setcontinue_thread(&is_abort)) {
      return EINVAL;
   }

   if (is_abort != s_callsetcontinue_isabort) {
      return EINVAL;
   }

   if (!is_abort) {
      s_callsetcontinue_isabort = 1;
      abort_thread();
   }

   if (ENOTRECOVERABLE != returncode_thread(self_thread())) {
      return EINVAL;
   }

   return 0;
}

static int test_abort(void)
{
   thread_t *  thread = 0;

   // TEST abort_thread
   TEST(0 == new_thread(&thread, &thread_callabort, 0));
   TEST(0 == join_thread(thread));
   TEST(ENOTRECOVERABLE == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   // TEST setcontinue_thread
   TEST(0 == new_thread(&thread, &thread_callsetcontinue, 0));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   return 0;
ONERR:
   delete_thread(&thread);
   return EINVAL;
}

static volatile int s_stackoverflow_issignal = 0;

static void sigstackoverflow(int sig)
{
   (void) sig;
   s_stackoverflow_issignal = 1;
   abort_thread();
}

static int thread_stackoverflow(void * argument)
{
   (void) argument;
   s_stackoverflow_issignal = 0;

   if (!s_stackoverflow_issignal) {
      (void) thread_stackoverflow(0);
   }

   return 0;
}

static int test_stackoverflow(void)
{
   sigset_t           oldprocmask;
   struct sigaction   newact, oldact;
   thread_t *         thread     = 0;
   thread_t* volatile mainthread = self_thread();
   volatile bool      isProcmask = false;
   volatile bool      isAction   = false;

   // prepare
   sigemptyset(&newact.sa_mask);
   sigaddset(&newact.sa_mask, SIGSEGV);
   TEST(0 == sigprocmask(SIG_UNBLOCK, &newact.sa_mask, &oldprocmask));
   isProcmask = true;

   sigemptyset(&newact.sa_mask);
   newact.sa_flags = SA_ONSTACK;
   newact.sa_handler = &sigstackoverflow;
   TEST(0 == sigaction(SIGSEGV, &newact, &oldact));
   isAction = true;

   // TEST abort_thread: abort_thread can recover from stack overflow (detected with signal SIGSEGV)
   TEST(0 == new_thread(&thread, &thread_stackoverflow, 0));
   TEST(0 == join_thread(thread));
   TEST(1 == s_stackoverflow_issignal);
   TEST(maintask_thread(thread)   == (thread_f)&thread_stackoverflow);
   TEST(mainarg_thread(thread)    == 0);
   TEST(returncode_thread(thread) == ENOTRECOVERABLE);
   TEST(0 == delete_thread(&thread));

   // TEST abort_thread: own thread can do so also
   setreturncode_thread(mainthread, 0);
   s_stackoverflow_issignal = 0;
   bool is_abort = false;
   TEST(0 == setcontinue_thread(&is_abort));
   if (!is_abort) {
      TEST(0 == pthread_kill(pthread_self(), SIGSEGV));
   }
   TEST(1 == s_stackoverflow_issignal);
   TEST(1 == is_abort);
   TEST(returncode_thread(mainthread) == ENOTRECOVERABLE);
   setreturncode_thread(mainthread, 0);

   // unprepare
   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0));
   TEST(0 == sigaction(SIGSEGV, &oldact, 0));

   return 0;
ONERR:
   delete_thread(&thread);
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0);
   if (isAction)     sigaction(SIGSEGV, &oldact, 0);
   return EINVAL;
}


typedef struct thread_isvalidstack_t   thread_isvalidstack_t;

struct thread_isvalidstack_t
{
   bool           isValid[30];
   thread_t *     thread[30];
   memblock_t     signalstack[30];
   memblock_t     threadstack[30];
   volatile bool  isvalid;
};

static int thread_isvalidstack(thread_isvalidstack_t * startarg)
{
   stack_t  current_sigaltstack;

   if (  0 != sigaltstack(0, &current_sigaltstack)
         || 0 != current_sigaltstack.ss_flags) {
      goto ONERR;
   }

   while (! startarg->isvalid) {
      yield_thread();
   }

   for (unsigned i = 0; i < lengthof(startarg->isValid); ++i) {
      if (  startarg->thread[i]->sys_thread == pthread_self()
            && startarg->thread[i] != self_thread()) {
         goto ONERR;
      }
   }

   unsigned tid;
   for (tid = 0; tid < lengthof(startarg->isValid); ++tid) {
      if (  startarg->thread[tid] == self_thread()
            && startarg->thread[tid]->sys_thread == pthread_self()) {
         startarg->isValid[tid] = true;
         break;
      }
   }

   if (tid == lengthof(startarg->isValid)) {
      goto ONERR;
   }

   if (  startarg->signalstack[tid].addr != current_sigaltstack.ss_sp
         || startarg->signalstack[tid].size != current_sigaltstack.ss_size) {
      goto ONERR;
   }

   if (  startarg->threadstack[tid].addr >= (uint8_t*)&startarg
         || (uint8_t*)&startarg >= startarg->threadstack[tid].addr + startarg->threadstack[tid].size) {
      goto ONERR;
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_manythreads(void)
{
   thread_isvalidstack_t   startarg = { .isvalid = false };

   // TEST newgeneric_thread: every thread has its own stackframe + self_thread
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(0 == newgeneric_thread(&startarg.thread[i], thread_isvalidstack, &startarg));
      signalstack_threadlocalstore(castPthread_threadlocalstore(startarg.thread[i]), &startarg.signalstack[i]);
      threadstack_threadlocalstore(castPthread_threadlocalstore(startarg.thread[i]), &startarg.threadstack[i]);
   }
   // startarg initialized
   startarg.isvalid = true;
   // wait for exit of all threads and check returncode == OK
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(0 == join_thread(startarg.thread[i]));
      TEST(0 == returncode_thread(startarg.thread[i]));
   }
   // check all threads were executed
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(startarg.isValid[i]);
   }
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(0 == delete_thread(&startarg.thread[i]));
   }

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      delete_thread(&startarg.thread[i]);
   }
   return EINVAL;
}

/* function: wait_for_signal
 * Waits for a blocked signal until it has been received.
 * Implemented with POSIX function <sigwaitinfo>.
 * Returns 0 if the signal with number signr has been received. */
static int wait_for_signal(int signr)
{
   int err;
   sigset_t signalmask;

   err = sigemptyset(&signalmask);
   if (err) return EINVAL;

   err = sigaddset(&signalmask, signr);
   if (err) return EINVAL;

   do {
      err = sigwaitinfo(&signalmask, 0);
   } while (-1 == err && EINTR == errno);

   return err == -1 ? errno : err == signr ? 0 : EINVAL;
}

/* function: wait_for_signal
 * Polls for a blocked signal.
 * Implemented with POSIX function <sigtimedwait>.
 * Returns 0 if the signal with number signr has been received. */
static int poll_for_signal(int signr)
{
   int err;
   sigset_t signalmask;

   err = sigemptyset(&signalmask);
   if (err) return EINVAL;

   err = sigaddset(&signalmask, signr);
   if (err) return EINVAL;

   struct timespec   ts = { 0, 0 };
   err = sigtimedwait(&signalmask, 0, &ts);

   return err == -1 ? errno : err == signr ? 0 : EINVAL;
}

static int thread_sendsignal2thread(thread_t* receiver)
{
   int err;
   err = pthread_kill(receiver->sys_thread, SIGUSR1);
   assert(0 == err);
   return err;
}

static int thread_sendsignal2process(void* dummy)
{
   int err;
   (void) dummy;
   err = kill(getpid(), SIGUSR1);
   return err;
}

static int thread_receivesignal(void* dummy)
{
   int err;
   (void) dummy;
   err = wait_for_signal(SIGUSR1);
   return err;
}

static int thread_receivesignal2(void* dummy)
{
   int err;
   (void) dummy;
   err = wait_for_signal(SIGUSR2);
   return err;
}


static int thread_receivesignalrt(void* dummy)
{
   int err;
   (void) dummy;
   err = wait_for_signal(SIGRTMIN);
   return err;
}

static int test_signal(void)
{
   thread_t *        thread1         = 0;
   thread_t *        thread2         = 0;
   struct timespec   ts              = { 0, 0 };
   bool              isoldsignalmask = false;
   sigset_t          oldsignalmask;
   sigset_t          signalmask;

   // prepare
   TEST(0 == sigemptyset(&signalmask));
   TEST(0 == sigaddset(&signalmask, SIGUSR1));
   TEST(0 == sigaddset(&signalmask, SIGUSR2));
   TEST(0 == sigaddset(&signalmask, SIGRTMIN));
   TEST(0 == sigprocmask(SIG_BLOCK, &signalmask, &oldsignalmask));
   isoldsignalmask = true;

   // TEST pthread_kill: main thread receives from 1st thread
   TEST(0 == newgeneric_thread(&thread1, thread_sendsignal2thread, self_thread()));
   TEST(0 == wait_for_signal(SIGUSR1));
   TEST(0 == join_thread(thread1));
   TEST(0 == returncode_thread(thread1));
   TEST(0 == delete_thread(&thread1));

   // TEST pthread_kill: 2nd thread receives from 1st thread
   while (0 < sigtimedwait(&signalmask, 0, &ts)); // empty signal queue
   TEST(0 == new_thread(&thread2, &thread_receivesignal, 0));
   TEST(0 == newgeneric_thread(&thread1, &thread_sendsignal2thread, thread2));
   TEST(0 == join_thread(thread1));
   TEST(0 == join_thread(thread2));
   TEST(0 == returncode_thread(thread1));
   TEST(0 == returncode_thread(thread2));
   TEST(0 == delete_thread(&thread1));
   TEST(0 == delete_thread(&thread2));

   // TEST pthread_kill: main thread can not receive from 1st thread if it sends to 2nd thread
   while (0 < sigtimedwait(&signalmask, 0, &ts)); // empty signal queue
   TEST(0 == new_thread(&thread2, &thread_receivesignal2, 0));
   TEST(0 == newgeneric_thread(&thread1, &thread_sendsignal2thread, thread2));
   TEST(0 == join_thread(thread1));
   TEST(0 == returncode_thread(thread1));
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts));
   TEST(EAGAIN == errno);
   TEST(0 == pthread_kill(thread2->sys_thread, SIGUSR2)); // wake up thread2
   TEST(0 == join_thread(thread2));
   TEST(0 == returncode_thread(thread2));
   TEST(0 == delete_thread(&thread1));
   TEST(0 == delete_thread(&thread2));

   // TEST kill(): send signal to process => main thread receives !
   while (0 < sigtimedwait(&signalmask, 0, &ts)); // empty signal queue
   TEST(0 == new_thread(&thread1, &thread_sendsignal2process, 0));
   TEST(0 == join_thread(thread1));
   TEST(0 == returncode_thread(thread1));
   TEST(0 == wait_for_signal(SIGUSR1));
   TEST(0 == delete_thread(&thread1));

   // TEST kill(): send signal to process => second thread receives !
   while (0 < sigtimedwait(&signalmask, 0, &ts)); // empty signal queue
   TEST(0 == new_thread(&thread1, &thread_sendsignal2process, 0));
   TEST(0 == join_thread(thread1));
   TEST(0 == returncode_thread(thread1));
   TEST(0 == new_thread(&thread2, &thread_receivesignal, 0));
   TEST(0 == join_thread(thread2));
   TEST(0 == returncode_thread(thread2));
   TEST(0 == delete_thread(&thread1));
   TEST(0 == delete_thread(&thread2));
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts)); // consumed by second thread
   TEST(EAGAIN == errno);

   // TEST kill: SIGUSR1 is not stored into queue
   while (0 < sigtimedwait(&signalmask, 0, &ts)); // empty signal queue
   TEST(0 == kill(getpid(), SIGUSR1));
   TEST(0 == kill(getpid(), SIGUSR1));
   TEST(0 == kill(getpid(), SIGUSR1));
   TEST(0 == new_thread(&thread1, &thread_receivesignal, 0));
   TEST(0 == join_thread(thread1));
   TEST(0 == returncode_thread(thread1));
   TEST(0 == delete_thread(&thread1));
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts)); // only one signal
   TEST(EAGAIN == errno);

   // TEST kill: SIGRTMIN (POSIX realtime signals) is stored into internal queue
   while (0 < sigtimedwait(&signalmask, 0, &ts)); // empty signal queue
   TEST(0 == kill(getpid(), SIGRTMIN));
   TEST(0 == kill(getpid(), SIGRTMIN));
   TEST(0 == kill(getpid(), SIGRTMIN));
   TEST(0 == new_thread(&thread1, &thread_receivesignalrt, 0));
   TEST(0 == new_thread(&thread2, &thread_receivesignalrt, 0));
   TEST(0 == join_thread(thread1));
   TEST(0 == join_thread(thread2));
   TEST(0 == returncode_thread(thread1));
   TEST(0 == returncode_thread(thread2));
   TEST(0 == delete_thread(&thread1));
   TEST(0 == delete_thread(&thread2));
   TEST(SIGRTMIN == sigtimedwait(&signalmask, 0, &ts)); // 3rd queued signal
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts));       // no 4rd
   TEST(EAGAIN == errno);

   // unprepare
   while (0 < sigtimedwait(&signalmask, 0, &ts)); // empty signal queue
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0));

   return 0;
ONERR:
   delete_thread(&thread1);
   delete_thread(&thread2);
   while (0 < sigtimedwait(&signalmask, 0, &ts)); // empty signal queue
   if (isoldsignalmask) sigprocmask(SIG_SETMASK, &oldsignalmask, 0);
   return EINVAL;
}

// == test_suspendresume ==

static int thread_suspend(thread_t* caller)
{
   resume_thread(caller);
   suspend_thread();
   return 0;
}

static int thread_flagsuspend(int* flag)
{
   // signal caller
   write_atomicint(flag, 1);
   // wait
   while (read_atomicint(flag)) {
      yield_thread();
   }
   yield_thread();
   suspend_thread();
   return 0;
}


static int thread_resume(thread_t* caller)
{
   resume_thread(caller);
   return 0;
}

static int thread_waitsuspend(intptr_t signr)
{
   int err;
   err = wait_signalrt((signalrt_t)signr, 0);
   if (!err) {
      suspend_thread();
   }
   return err;
}

static int test_suspendresume(void)
{
   thread_t * thread1 = 0;
   thread_t * thread2 = 0;

   // TEST resume_thread: uses SIGINT (not queued, only single instance)
   TEST(EAGAIN == poll_for_signal(SIGINT));
   resume_thread(self_thread());
   TEST(0 == poll_for_signal(SIGINT));
   TEST(EAGAIN == poll_for_signal(SIGINT));

   // TEST trysuspend_thread
   for (unsigned i = 0; i < 100; ++i) {
      TEST(EAGAIN == trysuspend_thread());
      resume_thread(self_thread());
      TEST(0 == trysuspend_thread());
      TEST(EAGAIN == trysuspend_thread());
   }

   // TEST suspend_thread: thread suspends
   trysuspend_thread();
   TEST(0 == newgeneric_thread(&thread1, &thread_suspend, self_thread()));
   // wait for thread
   while (EAGAIN == poll_for_signal(SIGINT)) {
      yield_thread();
   }
   // check thread suspended
   for (int i = 0; i < 5; ++i) {
      sleepms_thread(1);
      TEST(EBUSY == tryjoin_thread(thread1));
   }

   // TEST suspend_thread: EINTR does not wakeup thread
   for (int i = 0; i < 5; ++i) {
      interrupt_thread(thread1);
      sleepms_thread(1);
      // check thread is in suspend
      TEST(EBUSY == tryjoin_thread(thread1));
   }

   // TEST resume_thread: main thread resumes suspended thread
   resume_thread(thread1);
   TEST(0 == join_thread(thread1));
   TEST(0 == returncode_thread(thread1));

   // TEST resume_thread: already joined thread is ignored
   resume_thread(thread1);
   TEST(0 == delete_thread(&thread1));

   // TEST suspend_thread: EINTR does not clear already queued resume
   trysuspend_thread();
   int flag = 0;
   TEST(0 == newgeneric_thread(&thread1, &thread_flagsuspend, &flag));
   // wait for start of thread
   while (0 == read_atomicint(&flag)) {
      yield_thread();
   }
   // queue resume signal
   resume_thread(thread1);
   // start thread
   write_atomicint(&flag, 0);
   // send interrupt until exit
   while (thread1->sys_thread != sys_thread_FREE) {
      interrupt_thread(thread1);
   }
   // reset
   delete_thread(&thread1);

   // TEST resume_thread: already exited thread is ignored (join is called)
   trysuspend_thread();
   TEST(0 == newgeneric_thread(&thread1, &thread_resume, self_thread()));
   // wait until thread ended
   suspend_thread();
   sleepms_thread(10);
   // test
   resume_thread(thread1);
   // check marked thread as joined
   TEST(sys_thread_FREE == thread1->sys_thread);
   // reset
   TEST(0 == delete_thread(&thread1));

   // TEST resume_thread: other threads resume suspended thread
   trysuspend_thread();
   TEST(0 == newgeneric_thread(&thread1, &thread_suspend, self_thread()));
   // wait for thread
   suspend_thread();
   TEST(0 == newgeneric_thread(&thread2, &thread_resume, thread1));
   TEST(0 == join_thread(thread2));
   TEST(0 == join_thread(thread1));
   TEST(0 == returncode_thread(thread1));
   TEST(0 == returncode_thread(thread2));
   TEST(0 == delete_thread(&thread1));
   TEST(0 == delete_thread(&thread2));

   // TEST resume_thread: main thread resumes threads before they called suspend_thread
   //                     test that resume_thread is preserved !
   TEST(EAGAIN == trywait_signalrt(0, 0));
   TEST(EAGAIN == trywait_signalrt(1, 0));
   TEST(0 == newgeneric_thread(&thread1, &thread_waitsuspend, (intptr_t)0));
   TEST(0 == newgeneric_thread(&thread2, &thread_waitsuspend, (intptr_t)0));
   resume_thread(thread1);
   resume_thread(thread2);
   TEST(0 == send_signalrt(0, 0)); // start threads
   TEST(0 == send_signalrt(0, 0));
   TEST(0 == join_thread(thread1));
   TEST(0 == join_thread(thread2));
   TEST(0 == returncode_thread(thread1));
   TEST(0 == returncode_thread(thread2));
   TEST(0 == delete_thread(&thread1));
   TEST(0 == delete_thread(&thread2));

   // TEST resume_thread: main resumes itself
   for (unsigned i = 0; i < 100; ++i) {
      resume_thread(self_thread());
      suspend_thread();   // consumes SIGINT
      TEST(EAGAIN == poll_for_signal(SIGINT));
   }

   return 0;
ONERR:
   delete_thread(&thread1);
   delete_thread(&thread2);
   return EINVAL;
}

typedef struct {
   int       fd;
   thread_t* resume;
} reapipe_arg_t;

static int thread_readpipe(reapipe_arg_t* arg)
{
   int err;
   uint8_t buffer[4];

   resume_thread(arg->resume);

   err = read(arg->fd, buffer, 4);
   err = err < 0 ? errno : 0;

   return err;
}

static int test_interrupt(void)
{
   thread_t * thread = 0;
   int        fd[2] = { -1, -1 };

   // prepare0
   TEST(0 == pipe2(fd, O_CLOEXEC));

   // TEST interrupt_thread
   reapipe_arg_t readarg = { fd[0], self_thread() };
   trysuspend_thread();
   TEST(0 == newgeneric_thread(&thread, &thread_readpipe, &readarg))
   // wait for start of thread
   suspend_thread();
   sleepms_thread(1);
   // generate interrupt
   interrupt_thread(thread);
   // check EINTR
   TEST(0 == join_thread(thread));
   TEST(EINTR == returncode_thread(thread));
   // reset
   TEST(0 == delete_thread(&thread));

   // TEST interrupt_thread: already exited thread is ignored (join is called)
   trysuspend_thread();
   TEST(0 == newgeneric_thread(&thread, &thread_readpipe, &readarg))
   // wait for start of thread
   suspend_thread();
   sleepms_thread(1);
   // generate interrupt
   interrupt_thread(thread);
   sleepms_thread(1);
   // wait until thread has ended
   while (0 == pthread_kill(thread->sys_thread, 0)) {
      yield_thread();
   }
   // test
   TEST(sys_thread_FREE != thread->sys_thread);
   interrupt_thread(thread);
   // check tryjoin called
   TEST(sys_thread_FREE == thread->sys_thread);
   TEST(EINTR == returncode_thread(thread));

   // TEST interrupt_thread: already joined thread is ignored
   interrupt_thread(thread);
   // reset
   TEST(0 == delete_thread(&thread));

   // reset0
   for (unsigned i = 0; i < lengthof(fd); ++i) {
      TEST(0 == close(fd[i]));
      fd[i] = -1;
   }

   return 0;
ONERR:
   close(fd[0]);
   close(fd[1]);
   delete_thread(&thread);
   return EINVAL;
}

static int test_sleep(void)
{
   timevalue_t tv;
   timevalue_t tv2;
   int64_t msec;

   // TEST sleepms_thread: 250 msec
   TEST(0 == time_sysclock(sysclock_MONOTONIC, &tv));
   sleepms_thread(250);
   TEST(0 == time_sysclock(sysclock_MONOTONIC, &tv2));
   // check
   msec = diffms_timevalue(&tv2, &tv);
   TESTP(200 < msec && msec < 300, "msec:%"PRId64, msec);

   // TEST sleepms_thread: 100 msec
   TEST(0 == time_sysclock(sysclock_MONOTONIC, &tv));
   sleepms_thread(100);
   TEST(0 == time_sysclock(sysclock_MONOTONIC, &tv2));
   // check
   msec = diffms_timevalue(&tv2, &tv);
   TESTP(80 < msec && msec < 120, "msec:%"PRId64, msec);

   return 0;
ONERR:
   return EINVAL;
}

// == test_yield ==

static uint32_t s_countyield_counter   = 0;
static uint32_t s_countnoyield_counter = 0;
static int      s_countyield_exit      = 0;

static int thread_countyield(void * dummy)
{
   (void) dummy;

   write_atomicint(&s_countyield_counter, 0);

   while (  s_countyield_counter < 10000000
            && ! read_atomicint(&s_countyield_exit)) {
      yield_thread();
      add_atomicint(&s_countyield_counter, 1);
   }

   if (! read_atomicint(&s_countyield_exit)) {
      write_atomicint(&s_countyield_counter, 0);
   }

   return 0;
}

static int thread_countnoyield(void * dummy)
{
   (void) dummy;

   write_atomicint(&s_countnoyield_counter, 0);

   while (  s_countnoyield_counter < 10000000
            && ! read_atomicint(&s_countyield_exit)) {
      // give other thread a chance to run
      if (s_countnoyield_counter < 3) yield_thread();
      add_atomicint(&s_countnoyield_counter , 1);
   }

   write_atomicint(&s_countnoyield_counter, 0);

   return 0;
}

static int test_yield(void)
{
   thread_t * thread_yield   = 0;
   thread_t * thread_noyield = 0;

   // TEST yield_thread
   s_countyield_exit = 0;
   TEST(0 == new_thread(&thread_yield, &thread_countyield, 0));
   TEST(0 == new_thread(&thread_noyield, &thread_countnoyield, 0));
   TEST(0 == join_thread(thread_noyield));
   write_atomicint(&s_countyield_exit, 1);
   TEST(0 == join_thread(thread_yield));
   // no yield ready
   TEST(0 == read_atomicint(&s_countnoyield_counter));
   // yield not ready
   TEST(0 != read_atomicint(&s_countyield_counter));
   TEST(s_countyield_counter < 1000000);
   TEST(0 == delete_thread(&thread_noyield));
   TEST(0 == delete_thread(&thread_yield));

   return 0;
ONERR:
   s_countyield_exit = 1;
   delete_thread(&thread_noyield);
   delete_thread(&thread_yield);
   return EINVAL;
}

// == test_exit ==

static int thread_callexit(intptr_t retval)
{
   exit_thread((int)retval);
   while (1) {
      sleepms_thread(1000);
   }
   return 0;
}

static int test_exit(void)
{
   thread_t * thread[20] = { 0 };

   // TEST exit_thread: called from created thread
   for (int i = 0; i < (int)lengthof(thread); ++i) {
      TEST(0 == newgeneric_thread(&thread[i], &thread_callexit, (intptr_t)i));
   }
   for (int i = 0; i < (int)lengthof(thread); ++i) {
      TEST(0 == join_thread(thread[i]));
      TEST(i == returncode_thread(thread[i]));
      TEST(0 == delete_thread(&thread[i]));
   }

   // TEST exit_thread: EPROTO
   TEST(ismain_thread(self_thread()));
   TEST(EPROTO == exit_thread(0));

   return 0;
ONERR:
   for (int i = 0; i < (int)lengthof(thread); ++i) {
      delete_thread(&thread[i]);
   }
   return EINVAL;
}

static int thread_lockflag(int * runcount)
{
   while (0 == read_atomicint(&self_thread()->lockflag)) {
      yield_thread();
   }
   add_atomicint(runcount, 1);
   lockflag_thread(self_thread());
   sub_atomicint(runcount, 1);
   return 0;
}

static int test_update(void)
{
   thread_t    thread   = { .lockflag = 0 };
   thread_t *  thread2  = 0;
   int         runcount = 0;

   // TEST settask_thread
   settask_thread(&thread, &thread_donothing, (void*)10);
   TEST(maintask_thread(&thread) == &thread_donothing);
   TEST(mainarg_thread(&thread)  == (void*)10);
   settask_thread(&thread, 0, 0);
   TEST(maintask_thread(&thread) == 0);
   TEST(mainarg_thread(&thread)  == 0);

   // TEST setreturncode_thread
   setreturncode_thread(&thread, 1);
   TEST(1 == returncode_thread(&thread));
   setreturncode_thread(&thread, 0);
   TEST(0 == returncode_thread(&thread));

   // TEST lockflag_thread
   lockflag_thread(&thread);
   TEST(0 != thread.lockflag);

   // TEST unlockflag_thread
   unlockflag_thread(&thread);
   TEST(0 == thread.lockflag);

   // TEST lockflag_thread: waits
   TEST(0 == newgeneric_thread(&thread2, thread_lockflag, &runcount));
   TEST(0 == thread2->lockflag);
   lockflag_thread(thread2);
   TEST(0 != thread2->lockflag);
   while (0 == read_atomicint(&runcount)) {
      yield_thread();
   }
   // waits until unlock
   for (int i = 0; i < 5; ++i) {
      yield_thread();
      TEST(1 == read_atomicint(&runcount));
   }
   unlockflag_thread(thread2);
   TEST(0 == join_thread(thread2));

   // TEST unlockflag_thread: works from another thread
   TEST(0 != read_atomicint(&thread2->lockflag));
   unlockflag_thread(thread2);
   TEST(0 == thread2->lockflag);
   TEST(0 == delete_thread(&thread2));

   return 0;
ONERR:
   delete_thread(&thread2);
   return EINVAL;
}

static int child_outofmemory(void* dummy)
{
   (void) dummy;
   vmpage_t    freepage;
   thread_t*   thread = 0;

   // prepare
   TEST(0 == init_vmpage(&freepage, 1024*1024));
   for (size_t size = ~(((size_t)-1)/2); (size /= 2) >= 1024*1024; ) {
      vmpage_t page;
      while (0 == init_vmpage(&page, size)) {
         // exhaust virtual memory
      }
      CLEARBUFFER_ERRLOG();
   }
   TEST(0 == free_vmpage(&freepage));

   // TEST new_thread: init_threadcontext fails with ENOMEM
   TEST(0 == new_thread(&thread, &thread_donothing, 0));
   TEST(0 == join_thread(thread));
   TEST(ENOMEM == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   return 0;
ONERR:
   return EINVAL;
}

static int test_outofres(void)
{
   process_t        child = process_FREE;
   process_result_t result;

   TEST(0 == init_process(&child, &child_outofmemory, 0, 0));
   TEST(0 == wait_process(&child, &result));
   TEST( isequal_processresult(&result, 0, process_state_TERMINATED));
   TEST(0 == free_process(&child));

   return 0;
ONERR:
   free_process(&child);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_FREE;

   if (test_exit())           goto ONERR;

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree())       goto ONERR;
   if (test_mainthread())     goto ONERR;
   if (test_query())          goto ONERR;
   if (test_join())           goto ONERR;
   if (test_sigaltstack())    goto ONERR;
   if (test_abort())          goto ONERR;
   if (test_stackoverflow())  goto ONERR;
   if (test_manythreads())    goto ONERR;
   if (test_signal())         goto ONERR;
   if (test_suspendresume())  goto ONERR;
   if (test_interrupt())      goto ONERR;
   if (test_sleep())          goto ONERR;
   if (test_yield())          goto ONERR;
   if (test_exit())           goto ONERR;
   if (test_update())         goto ONERR;
   if (test_outofres())       goto ONERR;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   free_resourceusage(&usage);
   return EINVAL;
}

int unittest_platform_task_thread()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
