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
#include "C-kern/api/platform/task/thread_stack.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/time/timevalue.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/io/writer/log/logwriter.h"
#include "C-kern/api/test/mm/testmm.h"
#include "C-kern/api/platform/task/process.h"
#include "C-kern/api/time/sysclock.h"
#endif


/* struct: startarg_t
 * Startargument of the started system thread.
 * The start function <start_thread> then calls the task stored in thread. */
typedef struct startarg_t {
   thread_t  * self;
   stack_t     signalstack;
   maincontext_e     type;
   // == only used for main thread ==
   int               argc;
   const char **     argv;
} startarg_t;


// section: thread_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_thread_errtimer
 * Simulates an error in <new_thread>. */
static test_errortimer_t   s_thread_errtimer = test_errortimer_FREE;

/* variable: s_thread_errtimer
 * Simulates an error in <start_thread>. */
static test_errortimer_t   s_start_errtimer = test_errortimer_FREE; // TODO: add tests
#endif

// group: helper

/* function: init_start_context
 * The pre start init needed before running the task. */
static int init_start_context(thread_t* thread)
{
   int err;

   thread->sys_thread = pthread_self(); // !! also assigned in new_thread
   memblock_t  signalstack = signalstack_threadstack(castPthread_threadstack(thread));
   startarg_t* startarg = (startarg_t*) signalstack.addr; // coupled with init_helper

   assert(startarg->self == thread);
   assert(pthread_self() != sys_thread_FREE);

   if (maincontext_STATIC != startarg->type) {
      if (PROCESS_testerrortimer(&s_start_errtimer, &err)) { return err; }
      if (thread->ismain) {
         err = init_maincontext(startarg->type, startarg->argc, startarg->argv);
         (void) PROCESS_testerrortimer(&s_start_errtimer, &err);
         if (err) return err;
      }
      err = init_threadcontext(&thread->threadcontext, startarg->type);
      (void) PROCESS_testerrortimer(&s_start_errtimer, &err);
      if (err) return err;
   }

   // do not access startarg after sigaltstack (startarg is stored on this stack)
   err = sigaltstack(&startarg->signalstack, (stack_t*)0);
   if (PROCESS_testerrortimer(&s_start_errtimer, &err)) { errno = err; }
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigaltstack", err);
      return err;
   }

   return 0;
}

/* function: free_start_context
 * Free resources after runnning the task. */
static int free_start_context(thread_t* thread)
{
   int err;

   err = free_threadcontext(&thread->threadcontext);
   (void) PROCESS_testerrortimer(&s_start_errtimer, &err);

   if (thread->ismain) {
      int err2 = free_maincontext();
      (void) PROCESS_testerrortimer(&s_start_errtimer, &err2);
      if (err2) err = err2;
   }

   stack_t altstack = { .ss_flags = SS_DISABLE };
   (void) sigaltstack(&altstack, 0);

   return err;
}

/* function: start_thread
 * The start function of the thread.
 * This is the same for all threads.
 * It initializes signalstack, <threadcontext_t>, and
 * executes the user supplied function. */
static void* start_thread(void* _dummy)
{  (void) _dummy;
   int err;
   thread_t* thread = self_thread();

   err = init_start_context(thread);
   if (err) goto ONERR;

   if (0 != getcontext(&thread->continuecontext)) { err = errno; }
   (void) PROCESS_testerrortimer(&s_start_errtimer, &err);
   thread = self_thread();
   if (err) {
      TRACESYSCALL_ERRLOG("getcontext", err);
      goto ONERR;
   }

   // abort_thread sets thread->returncode to ENOTRECOVERABLE
   if (0 == thread->returncode && thread->task) {
      thread->returncode = thread->task(thread->task_arg);
   }

   err = free_start_context(thread);
   if (err) goto ONERR;

   return 0;
ONERR:
   thread->returncode = err;
   thread->syserr = err;
   TRACEEXIT_ERRLOG(err);
   (void) free_start_context(thread);
   return (void*)(intptr_t)err;
}

static inline int init_helper(
      /*out*/thread_t** thread,
      /*out*/memblock_t* stack,
      ilog_t*              log,
      thread_f            task,
      void*           task_arg,
      uint8_t     isMainThread,
      maincontext_e       type,
      int                 argc,
      const char*       argv[])
{
   int err;
   thread_stack_t* threadstack = 0;
   const size_t    static_size = extsize_threadcontext() + (isMainThread ?  extsize_maincontext() : 0);
   memblock_t      signalstack;

   if (! PROCESS_testerrortimer(&s_thread_errtimer, &err)) {
      err = new_threadstack(&threadstack, log, static_size, stack, &signalstack);
   }
   if (err) goto ONERR;

   if (! PROCESS_testerrortimer(&s_thread_errtimer, &err)) {
      err = initstatic_threadcontext(context_threadstack(threadstack), log);
   }
   if (err) goto ONERR;

   thread_t* newthread = thread_threadstack(threadstack);
   newthread->task     = task;
   newthread->task_arg = task_arg;
   newthread->ismain   = isMainThread;

   startarg_t* startarg   = (startarg_t*) signalstack.addr; // coupled with init_start_context

   startarg->self         = newthread;
   startarg->signalstack  = (stack_t) { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size };
   startarg->type         = type;
   startarg->argc         = argc;
   startarg->argv         = argv;

   // set out (stack already set)
   *thread = newthread;

   return 0;
ONERR:
   (void) delete_threadstack(&threadstack, log);
   return err;
}

static inline int free_helper(thread_t** thread, ilog_t* log)
{
   thread_t* delobj = *thread;
   if (!delobj) return 0;

   *thread = 0;

   int err = freestatic_threadcontext(&delobj->threadcontext, log);
   (void) PROCESS_testerrortimer(&s_thread_errtimer, &err);

   thread_stack_t* threadstack = castPthread_threadstack(delobj);

   int err2 = delete_threadstack(&threadstack, log);
   (void) PROCESS_testerrortimer(&s_thread_errtimer, &err2);
   if (err2) err = err2;

   return err;
}


// group: lifetime

int delete_thread(thread_t ** thread)
{
   int err;
   thread_t* delobj = *thread;
   ilog_t*   defaultlog = GETWRITER0_LOG();

   if (delobj) {
      if (ismain_thread(delobj)) {
         err = EINVAL;
         goto ONERR;
      }

      *thread = 0;

      err = join_thread(delobj);
      (void) PROCESS_testerrortimer(&s_thread_errtimer, &err);

      int err2 = free_helper(&delobj,defaultlog);
      if (err2) err = err2;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int new_thread(/*out*/thread_t** thread, thread_f task, void * task_arg)
{
   int err;
   pthread_attr_t    thread_attr;
   thread_t *        newthread = 0;
   bool              isThreadAttrValid = false;
   memblock_t        stack;
   ilog_t          * defaultlog = GETWRITER0_LOG();
   const uint8_t     normalThread = 0;

   err = init_helper(&newthread, &stack, defaultlog, task, task_arg, normalThread, type_maincontext(), 0, 0);
   if (err) goto ONERR;

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
   if (! PROCESS_testerrortimer(&s_thread_errtimer, &err)) {
      err = pthread_create(&sys_thread, &thread_attr, &start_thread, 0);
   }
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_create",err);
      goto ONERR;
   }
   newthread->sys_thread = sys_thread; // !! also assigned in start_thread

   isThreadAttrValid = false;
   err = pthread_attr_destroy(&thread_attr);
   if (err) {
      // ignore error
      TRACESYSCALL_ERRLOG("pthread_attr_destroy",err);
   }

   *thread = newthread;

   return 0;
ONERR:
   if (isThreadAttrValid) {
      (void) pthread_attr_destroy(&thread_attr);
   }
   (void) free_helper(&newthread, defaultlog);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int runmain_thread(/*out;err*/int* retcode, thread_f task, void* task_arg, ilog_t* initlog, maincontext_e type, int argc, const char* argv[])
{
   int err;
   thread_t*  thread=0;
   memblock_t stack;
   ucontext_t context_old;
   ucontext_t context_mainthread;
   const uint8_t mainThread  = 1;

   err = init_helper(&thread,&stack,initlog,task,task_arg,mainThread,type,argc,argv);
   if (err) goto ONERR;

   err = getcontext(&context_mainthread);
   if (PROCESS_testerrortimer(&s_thread_errtimer, &err)) {
      errno = err;
   }
   if (err) {
      err = errno;
      TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "getcontext", err);
      goto ONERR;
   }

   context_mainthread.uc_link  = &context_old; // ensures activation of context_old after exit of start_thread
   context_mainthread.uc_stack = (stack_t) { .ss_sp = stack.addr, .ss_flags = 0, .ss_size = stack.size };
   static_assert((void*(*)(void*))0 == (typeof(&start_thread))0, "start_thread compatible with void fct(void)");
   makecontext(&context_mainthread, (void(*)(void))&start_thread, 0, 0);

   // calls start_thread and returns after exit of this function (current context is saved in context_old)
   err = swapcontext(&context_old, &context_mainthread);
   if (PROCESS_testerrortimer(&s_thread_errtimer, &err)) {
      errno = err;
   }
   if (err) {
      err = errno;
      TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "swapcontext", err);
      goto ONERR;
   }

   err = returncode_thread(thread);
   if (retcode) *retcode = err;

   err = free_helper(&thread,initlog);
   if (err) goto ONERR;

   return 0;
ONERR:
   (void) free_helper(&thread,initlog);
   TRACE_LOG(initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
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

int suspend1_thread(struct timevalue_t* timeout)
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

   if (0 == timeout) {
      do {
         err = sigwaitinfo(&signalmask, 0);
      } while (-1 == err && EINTR == errno);
   } else {
      struct timespec ts = { .tv_sec=timeout->seconds, .tv_nsec=timeout->nanosec };
      err = sigtimedwait(&signalmask, 0, &ts);
      if (-1 == err && (EAGAIN==errno || EINTR==errno)) return EAGAIN;
   }

   if (-1 == err) {
      err = errno;
      TRACESYSCALL_ERRLOG(timeout?"sigtimedwait":"sigwaitinfo", err);
      goto ONERR;
   }

   return 0;
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

   struct timespec ts = { 0, 0 };
   err = sigtimedwait(&signalmask, 0, &ts);
   if (-1 == err) return EAGAIN;

   return 0;
ONERR:
   abort_maincontext(err);
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

static void*         s_thread_param    = 0;
static unsigned      s_thread_runcount = 0;
static unsigned      s_thread_signal   = 0;
static int           s_thread_retcode  = 0;
static sys_thread_t  s_thread_id       = 0;
static unsigned      s_thread_type     = 0;
static int           s_thread_argc     = 0;
static const char**  s_thread_argv     = 0;

static int thread_donothing(void* dummy)
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

static int thread_param(void* param)
{
   s_thread_id = pthread_self();
   s_thread_param = param;
   return s_thread_retcode;
}

static int thread_context(void* param)
{  (void) param;
   s_thread_id = pthread_self();
   s_thread_type = type_maincontext();
   s_thread_argc = self_maincontext()->argc;
   s_thread_argv = self_maincontext()->argv;
   return 0;
}

static int thread_printferrlog(void* text)
{
   PRINTF_ERRLOG("%s",(char*)text);
   return 0;
}

static int test_initfree(void)
{
   thread_t * thread = 0;
   thread_t  sthread = thread_FREE;
   threadcontext_t tcfree = threadcontext_FREE;

   // TEST thread_FREE
   TEST( 0 == memcmp(&tcfree, &sthread.threadcontext, sizeof(tcfree)));
   TEST( 0 == sthread.wait.next);
   TEST( 0 == sthread.wait.prev);
   TEST( 0 == sthread.task);
   TEST( 0 == sthread.task_arg);
   TEST( 0 == sthread.returncode);
   TEST( 0 == sthread.lockflag);
   TEST( 0 == sthread.ismain);
   TEST( sys_thread_FREE == sthread.sys_thread);

   // TEST new_thread
   TEST(0 == new_thread(&thread, &thread_donothing, (void*)3));
   TEST(thread);
   TEST(thread->wait.next  == 0);
   TEST(thread->wait.prev  == 0);
   TEST(thread->lockflag   == 0);
   TEST(sthread.ismain     == 0);
   TEST(thread->task       == &thread_donothing);
   TEST(thread->task_arg   == (void*)3);
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
   TEST(thread->wait.next  == 0);
   TEST(thread->wait.prev  == 0);
   TEST(thread->lockflag   == 0);
   TEST(sthread.ismain     == 0);
   TEST(thread->task       == (thread_f)&thread_returncode);
   TEST(thread->task_arg   == (void*)14);
   TEST(thread->returncode == 0);
   TEST(thread->sys_thread != sys_thread_FREE);
   sys_thread_t T = thread->sys_thread;
   // test thread is running
   while (0 == read_atomicint(&s_thread_runcount)) {
      yield_thread();
   }
   TEST(s_thread_id        == T);
   TEST(thread->wait.next  == 0);
   TEST(thread->wait.prev  == 0);
   TEST(sthread.ismain     == 0);
   TEST(thread->task       == (thread_f)&thread_returncode);
   TEST(thread->task_arg   == (void*)14);
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
   TEST(thread->wait.next  == 0);
   TEST(thread->wait.prev  == 0);
   TEST(sthread.ismain     == 0);
   TEST(thread->task       == (thread_f)&thread_returncode);
   TEST(thread->task_arg   == (void*)11);
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
         TEST(6 == i)
         break;
      }
      TEST(0 == thread);
      TEST(i == err);
   }
   free_testerrortimer(&s_thread_errtimer);
   write_atomicint(&s_thread_signal, 1);
   TEST(0 == delete_thread(&thread));

   // TEST delete_thread: simulated ERROR
   for (unsigned i = 1; i; ++i) {
      TEST(0 == new_thread(&thread, &thread_donothing, 0));
      init_testerrortimer(&s_thread_errtimer, i, (int)i);
      int err = delete_thread(&thread);
      // check
      TEST( 0 == thread);
      if (!err) {
         TESTP(4 == i,"i:%d",i)
         break;
      }
      TEST( (int)i == err);
   }
   free_testerrortimer(&s_thread_errtimer);

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

static int check_error_logged(pipe_t* errpipe, int oldstderr)
{
   uint8_t buffer[64];
   ssize_t len;

   len = read(errpipe->read, buffer,sizeof(buffer));
   TEST( len == sizeof(buffer));

   do {
      TEST(len == write(oldstderr, buffer, (size_t)len));
      len = read(errpipe->read, buffer,sizeof(buffer));
   } while (len > 0);
   TEST( -1 == len);
   TEST( EAGAIN == errno);

   return 0;
ONERR:
   return EINVAL;
}

static int test_mainthread(void)
{
   thread_t*   mainthread = self_thread();
   iochannel_t olderr = iochannel_FREE;
   pipe_t      pipe = pipe_FREE;
   size_t      logsize1,logsize2;
   uint8_t*    logbuffer;
   const char* argv[3] = { "1", "2", "3" };

   // prepare
   TEST( 1 == mainthread->ismain);
   TEST( 0 == switchoff_testmm());
   TEST( 0 == free_start_context(mainthread));
   TEST( maincontext_STATIC == type_maincontext());

   // TEST runmain_thread: maincontext is setup and freed
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   maincontext_e test_type[3] = { maincontext_STATIC, maincontext_DEFAULT, maincontext_CONSOLE };
   for (uintptr_t i = 0; i <= 10; ++i) {
      maincontext_e T = test_type[i%3];
      int retcode = -1;
      int C=(int)(1+(i%3));
      const char** V=&argv[2-(i%3)];
      TEST(0 == runmain_thread(&retcode, &thread_context, (void*)(i+1), GETWRITER0_LOG(), T, C, V));
      // check context type and params
      TEST(0 == retcode);
      TEST(T == s_thread_type);
      if (maincontext_STATIC==T) { C=0; V=0; } // maincontext not initialized !!
      TEST(C == s_thread_argc);
      TEST(V == s_thread_argv);
      // check no new thread created
      TEST(pthread_self() == s_thread_id);
      s_thread_id = sys_thread_FREE;
      // check no log written
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize2 == logsize1);
      // check maincontext freed
      TEST(maincontext_STATIC == type_maincontext());
   }

   // TEST runmain_thread: argument && return code
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   for (uintptr_t i = 0; i <= 10; ++i) {
      int retcode = -1;
      s_thread_retcode = (int)i;
      TEST(0 == runmain_thread(&retcode, &thread_param, (void*)(i+1), GETWRITER0_LOG(), maincontext_STATIC, 0, 0));
      // check param and return code
      TEST(i == (uintptr_t) retcode);
      TEST(i == ((uintptr_t)s_thread_param) -1);
      // check no new thread created
      TEST(pthread_self() == s_thread_id);
      s_thread_id = sys_thread_FREE;
      // check no log written
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize2 == logsize1);
   }

   // prepare
   TEST(0 == init_pipe(&pipe));
   TEST(0 <  (olderr = dup(iochannel_STDERR)));
   TEST(iochannel_STDERR == dup2(pipe.write, iochannel_STDERR));

   // TEST runmain_thread: use log
   {
      int retcode = -1;
      char text[] = { "123456789\n" };
      char buffer[sizeof(text)] = {0};
      TEST(0 == runmain_thread(&retcode, &thread_printferrlog, text, GETWRITER0_LOG(), maincontext_STATIC, 0, 0));
      TEST(0 == retcode);
      size_t len = (size_t) read(pipe.read, buffer, sizeof(buffer));
      TEST(len == strlen(text));
      TEST(0 == strcmp(text, buffer));
   }

   // TEST runmain_thread: simulated error conditions in runmain_thread
   s_thread_retcode = 0;
   s_thread_param = 0;
   for (unsigned i=1; ;++i) {
      init_testerrortimer(&s_thread_errtimer, i, (int)i);
      int err = runmain_thread(0, &thread_param, (void*)(uintptr_t)i, GETWRITER0_LOG(), maincontext_STATIC, 0, 0);
      if (i <= 3) {
         TEST( 0 == s_thread_param); // error before call
      } else {
         TEST( i == (uintptr_t)s_thread_param); // err in free resources
      }
      if (!err) {
         TEST( 7 == i);
         free_testerrortimer(&s_thread_errtimer);
         break;
      }
      TEST(err == (int)i);
      TEST(0 == check_error_logged(&pipe, olderr));
   }

   // TEST runmain_thread: simulated error conditions in start_thread
   s_thread_retcode = 0;
   s_thread_param = 0;
   for (unsigned i=1; ;++i) {
      init_testerrortimer(&s_start_errtimer, i, (int)i);
      int retcode=0;
      TEST(0 == runmain_thread(&retcode, &thread_param, (void*)(uintptr_t)i, GETWRITER0_LOG(), maincontext_DEFAULT, 0, 0));
      if (i <= 5) {
         TESTP( 0 == s_thread_param, "i:%d", i); // error before call
      } else {
         TESTP( i == (uintptr_t)s_thread_param, "i:%d", i); // err in free resources
      }
      if (!retcode) {
         TESTP( 8 == i, "i:%d", i);
         free_testerrortimer(&s_start_errtimer);
         break;
      }
      TEST(retcode == (int)i);
      TEST(0 == check_error_logged(&pipe, olderr));
   }

   // unprepare
   TEST(0 == free_pipe(&pipe));
   TEST(iochannel_STDERR == dup2(olderr, iochannel_STDERR));
   TEST(0 == free_iochannel(&olderr));

   // TEST delete_thread: EINVAL in case of main thread
   TEST(EINVAL == delete_thread(&mainthread));

   return 0;
ONERR:
   if (!isfree_iochannel(olderr)) {
      dup2(olderr, iochannel_STDERR);
      free_iochannel(&olderr);
   }
   free_pipe(&pipe);
   return EINVAL;
}

static int test_query(void)
{
   thread_t    thread;

   // TEST context_thread
   TEST(&thread.threadcontext == context_thread(&thread));
   TEST((threadcontext_t*)0   == context_thread((thread_t*)0));

   // TEST self_thread
   TEST(self_thread() == thread_threadstack(self_threadstack()));
   TEST(&self_thread()->threadcontext == context_syscontext());

   // TEST returncode_thread
   for (unsigned R = 0; R <= 20; ++R) {
      setreturncode_thread(&thread, (int)R - 10);
      TEST( (int)R - 10 == returncode_thread(&thread));
   }

   // TEST task_thread
   settask_thread(&thread, (thread_f)&thread_returncode, 0);
   TEST(task_thread(&thread) == (thread_f)&thread_returncode);
   settask_thread(&thread, 0, 0);
   TEST(task_thread(&thread) == 0);

   // TEST taskarg_thread
   for (uintptr_t A = 0; A <= 10; ++A) {
      settask_thread(&thread, 0, (void*)A);
      TEST(A == (uintptr_t)taskarg_thread(&thread));
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
   TEST(thread->wait.next  == 0);
   TEST(thread->wait.prev  == 0);
   TEST(thread->lockflag   == 0);
   TEST(thread->ismain     == 0);
   TEST(thread->task       == (thread_f)&thread_returncode);
   TEST(thread->task_arg   == (void*)12);
   TEST(thread->returncode == 12);
   TEST(thread->sys_thread == sys_thread_FREE);

   // TEST join_thread: calling on already joined thread
   TEST(0 == join_thread(thread));
   TEST(thread->wait.next  == 0);
   TEST(thread->wait.prev  == 0);
   TEST(thread->lockflag   == 0);
   TEST(thread->ismain     == 0);
   TEST(thread->task       == (thread_f)&thread_returncode);
   TEST(thread->task_arg   == (void*)12);
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
   TEST(thread->wait.next  == 0);
   TEST(thread->wait.prev  == 0);
   TEST(thread->lockflag   == 0);
   TEST(thread->ismain     == 0);
   TEST(thread->task       == (thread_f)&thread_returncode);
   TEST(thread->task_arg   == (void*)13);
   TEST(thread->returncode == 13);
   TEST(thread->sys_thread == sys_thread_FREE);

   // TEST tryjoin_thread: calling on already joined thread
   TEST(0 == tryjoin_thread(thread));
   TEST(thread->wait.next  == 0);
   TEST(thread->wait.prev  == 0);
   TEST(thread->lockflag   == 0);
   TEST(thread->ismain     == 0);
   TEST(thread->task       == (thread_f)&thread_returncode);
   TEST(thread->task_arg   == (void*)13);
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
         TEST(thread->wait.next  == 0);
         TEST(thread->wait.prev  == 0);
         TEST(thread->lockflag   == 0);
         TEST(thread->ismain     == 0);
         TEST(thread->task       == (thread_f)&thread_returncode);
         TEST(thread->task_arg   == (void*)arg);
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
         TEST(thread->wait.next  == 0);
         TEST(thread->wait.prev  == 0);
         TEST(thread->lockflag   == 0);
         TEST(thread->ismain     == 0);
         TEST(thread->task       == (thread_f)&thread_returncode);
         TEST(thread->task_arg   == (void*)arg);
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
   s_sigaltstack_signalstack = signalstack_threadstack(castPthread_threadstack(self_thread()));
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

static int thread_callsetcontinue(void* dummy)
{
   (void) dummy;
   thread_t* thread = self_thread();

   s_callsetcontinue_isabort = 0;
   setreturncode_thread(thread, 0);
   if (setcontinue_thread(thread)) {
      return EINVAL;
   }

   if ((returncode_thread(thread) != 0) != s_callsetcontinue_isabort) {
      return EINVAL;
   }
   if (0 == returncode_thread(thread)) {
      s_callsetcontinue_isabort = 1;
      abort_thread();
   }
   if (ENOTRECOVERABLE != returncode_thread(thread)) {
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
   TEST(task_thread(thread)   == (thread_f)&thread_stackoverflow);
   TEST(taskarg_thread(thread)    == 0);
   TEST(returncode_thread(thread) == ENOTRECOVERABLE);
   TEST(0 == delete_thread(&thread));

   // TEST abort_thread: own thread can do so also
   setreturncode_thread(mainthread, 0);
   s_stackoverflow_issignal = 0;
   TEST(0 == setcontinue_thread(mainthread));
   if (0 == returncode_thread(mainthread)) {
      TEST(0 == pthread_kill(pthread_self(), SIGSEGV));
   }
   TEST(1 == s_stackoverflow_issignal);
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
      startarg.signalstack[i] = signalstack_threadstack(castPthread_threadstack(startarg.thread[i]));
      startarg.threadstack[i] = threadstack_threadstack(castPthread_threadstack(startarg.thread[i]));
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

static int thread_suspend0(thread_t* caller)
{
   resume_thread(caller);
   suspend_thread();
   return 0;
}

static int thread_suspend1(thread_t* caller)
{
   resume_thread(caller);
   timevalue_t timeout=timevalue_INIT(100,0);
   suspend_thread(&timeout);
   return 0;
}

static int thread_flagsuspend0(int* flag)
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

static int thread_waitsuspend0(intptr_t signr)
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
   timevalue_t starttime;
   timevalue_t endtime;
   timevalue_t timeout;
   int64_t     msec;

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

   // TEST suspend1_thread: measure timeout time
   TEST(0 == time_sysclock(sysclock_MONOTONIC, &starttime));
   timeout = (timevalue_t) {0,40000000};
   TEST(EAGAIN == suspend_thread(&timeout));
   TEST(0 == time_sysclock(sysclock_MONOTONIC, &endtime));
   // check
   msec = diffms_timevalue(&endtime, &starttime);
   TESTP(30 < msec && msec < 50, "msec:%"PRId64, msec);

   // TEST suspend0_thread: thread suspends
   trysuspend_thread();
   TEST(0 == newgeneric_thread(&thread1, &thread_suspend0, self_thread()));
   // wait for thread
   while (EAGAIN == poll_for_signal(SIGINT)) {
      yield_thread();
   }
   // check thread suspended
   for (int i = 0; i < 5; ++i) {
      sleepms_thread(1);
      TEST(EBUSY == tryjoin_thread(thread1));
   }

   // TEST suspend0_thread: EINTR does not wakeup thread
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

   // TEST suspend1_thread: thread suspends
   trysuspend_thread();
   TEST(0 == newgeneric_thread(&thread1, &thread_suspend1, self_thread()));
   // wait for thread
   while (EAGAIN == poll_for_signal(SIGINT)) {
      yield_thread();
   }
   // check thread suspended
   for (int i = 0; i < 5; ++i) {
      sleepms_thread(1);
      TEST(EBUSY == tryjoin_thread(thread1));
   }

   // TEST suspend1_thread: EINTR wakes up thread
   interrupt_thread(thread1);
   // check thread is in suspend
   TEST(0 == join_thread(thread1));
   TEST(0 == returncode_thread(thread1));

   // TEST resume_thread: already joined thread is ignored
   resume_thread(thread1);
   TEST(0 == delete_thread(&thread1));

   // TEST suspend0_thread: EINTR does not clear already queued resume
   trysuspend_thread();
   int flag = 0;
   TEST(0 == newgeneric_thread(&thread1, &thread_flagsuspend0, &flag));
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
   TEST(0 == newgeneric_thread(&thread1, &thread_suspend0, self_thread()));
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
   TEST(0 == newgeneric_thread(&thread1, &thread_waitsuspend0, (intptr_t)0));
   TEST(0 == newgeneric_thread(&thread2, &thread_waitsuspend0, (intptr_t)0));
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

   ssize_t nrbytes = read(arg->fd, buffer, 4);
   err = nrbytes < 0 ? errno : 0;

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
const  uint32_t s_countyield_max       = 1000000;

static int thread_countyield(void * dummy)
{
   (void) dummy;

   write_atomicint(&s_countyield_counter, 0);

   while (  s_countyield_counter < s_countyield_max
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

   while (  s_countnoyield_counter < s_countyield_max
            && ! read_atomicint(&s_countyield_exit)) {
      // give other thread a chance to run
      if (s_countnoyield_counter < 3) yield_thread();
      add_atomicint(&s_countnoyield_counter, 1);
   }

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
   // test that noyield is ready
   TEST(s_countyield_max == read_atomicint(&s_countnoyield_counter));
   // test that yield is not ready
   TEST(0 != read_atomicint(&s_countyield_counter));
   TEST(s_countyield_max/2 > s_countyield_counter);
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
   for (uintptr_t i = 0; i < lengthof(thread); ++i) {
      TEST(0 == newgeneric_thread(&thread[i], &thread_callexit, (intptr_t)i));
   }
   for (unsigned i = 0; i < lengthof(thread); ++i) {
      TEST(0 == join_thread(thread[i]));
      TEST( (int)i == returncode_thread(thread[i]));
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
   lock_thread(self_thread());
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
   TEST(task_thread(&thread)    == &thread_donothing);
   TEST(taskarg_thread(&thread) == (void*)10);
   settask_thread(&thread, 0, 0);
   TEST(task_thread(&thread)    == 0);
   TEST(taskarg_thread(&thread) == 0);

   // TEST settaskarg_thread
   settask_thread(&thread, &thread_donothing, 0);
   settaskarg_thread(&thread, (void*)11);
   TEST(task_thread(&thread)    == &thread_donothing);
   TEST(taskarg_thread(&thread) == (void*)11);
   settaskarg_thread(&thread, 0);
   TEST(task_thread(&thread)    == &thread_donothing);
   TEST(taskarg_thread(&thread) == 0);

   // TEST setreturncode_thread
   setreturncode_thread(&thread, 1);
   TEST(1 == returncode_thread(&thread));
   setreturncode_thread(&thread, 0);
   TEST(0 == returncode_thread(&thread));

   // TEST lock_thread
   lock_thread(&thread);
   TEST(0 != thread.lockflag);

   // TEST unlock_thread
   unlock_thread(&thread);
   TEST(0 == thread.lockflag);

   // TEST lock_thread: waits
   TEST(0 == newgeneric_thread(&thread2, thread_lockflag, &runcount));
   TEST(0 == thread2->lockflag);
   lock_thread(thread2);
   TEST(0 != thread2->lockflag);
   while (0 == read_atomicint(&runcount)) {
      yield_thread();
   }
   // waits until unlock
   for (int i = 0; i < 5; ++i) {
      yield_thread();
      TEST(1 == read_atomicint(&runcount));
   }
   unlock_thread(thread2);
   TEST(0 == join_thread(thread2));

   // TEST unlock_thread: works from another thread
   TEST(0 != read_atomicint(&thread2->lockflag));
   unlock_thread(thread2);
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

static int test_outofmem(void)
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
   if (test_outofmem())       goto ONERR;

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

   TEST(0 == execasprocess_unittest(&test_mainthread, &err));
   TEST(0 == err);
   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));
   TEST(0 == err);

   return 0;
ONERR:
   return EINVAL;
}

#endif
