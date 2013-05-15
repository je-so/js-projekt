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
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/sync/semaphore.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


typedef struct thread_startargument_t  thread_startargument_t ;

/* struct: thread_startargument_t
 * Startargument of the started system thread.
 * The function <main_thread> then calls the main task of the thread which
 * is stored in <thread_t.main>. */
struct thread_startargument_t {
   thread_f       main_task ;
   void *         main_arg ;
   uint8_t *      stackframe ;
   stack_t        signalstack ;
} ;


// section: stackframe_t

/* function: sizesignalstack_stackframe
 * Returns the minimum number of pages of the signal stack.
 * The signal stack is used in case of a signal or
 * exceptions which throw a signal. If for example the
 * the thread stack overflows SIGSEGV signal is thrown.
 * To handle this case the system must have an extra signal stack
 * cause signal handling needs stack space. */
static inline unsigned sizesignalstack_stackframe(void)
{
   const size_t page_size = pagesize_vm() ;
   return (MINSIGSTKSZ + page_size - 1) & (~(page_size-1)) ;
}

/* function: sizestack_stackframe
 * Returns the minimum number of pages of the thread stack. */
static inline unsigned sizestack_stackframe(void)
{
   const size_t page_size = pagesize_vm() ;
   return (PTHREAD_STACK_MIN + page_size - 1) & (~(page_size-1)) ;
}

/* function: size_stackframe
 * Returns the size of a stack frame for one thread.
 * The last tail protection page is not included. */
static inline size_t size_stackframe(void)
{
   return (3u << log2pagesize_vm()) + sizesignalstack_stackframe() + sizestack_stackframe() ;
}

static memblock_t signalstack_stackframe(const vmpage_t * stackframe)
{
   const size_t page_size = pagesize_vm() ;

   memblock_t stack = memblock_INIT(
                        sizesignalstack_stackframe(),
                        stackframe->addr + page_size
                     ) ;

   return stack ;
}

static memblock_t stack_stackframe(const vmpage_t * stackframe)
{
   const size_t page_size = pagesize_vm() ;

   memblock_t stack = memblock_INIT(
                        sizestack_stackframe(),
                        stackframe->addr + 2 * page_size + sizesignalstack_stackframe()
                     ) ;

   return stack ;
}

static int free_stackframe(vmpage_t * stackframe)
{
   int err ;

   err = free_vmpage(stackframe) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int init_stackframe(vmpage_t * stackframe)
{
   int err ;

   err = init2_vmpage(stackframe, size_stackframe() >> log2pagesize_vm(), accessmode_NONE) ;
   if (err) goto ONABORT ;

   /* Frame layout:
    *      size        : protection
    * ----------------------------------
    * | [page_size]    : NONE       |
    * | [signalstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * | [threadstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * */

   memblock_t signalstack = signalstack_stackframe(stackframe) ;
   err = protect_vmpage(genericcast_vmpage(&signalstack,), accessmode_RDWR) ;
   if (err) goto ONABORT ;

   memblock_t stack = stack_stackframe(stackframe) ;
   err = protect_vmpage(genericcast_vmpage(&stack,), accessmode_RDWR) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   (void) free_vmpage(stackframe) ;
   TRACEABORT_LOG(err) ;
   return err ;
}


// section: thread_t

/* variable: gt_threadcontest
 * Refers for every thread to corresponding <threadcontext_t> object.
 * Is is located on the thread stack so no heap memory is allocated. */
__thread  threadcontext_t  gt_threadcontext = threadcontext_INIT_STATIC ;

/* variable: gt_thread
 * Refers for every thread to corresponding <thread_t> object.
 * Is is located on the thread stack so no heap memory is allocated. */
__thread  thread_t         gt_thread        = { 0, 0, 0, 0, sys_thread_INIT_FREEABLE, 0, { .uc_link = 0 } } ;

/* variable: s_offset_thread
 * Contains the calculated offset from start of stack thread to <gt_thread>. */
static size_t              s_offset_thread  = 0 ;

#ifdef KONFIG_UNITTEST
/* variable: s_thread_errtimer
 * Simulates an error in <newgroup_thread>. */
static test_errortimer_t   s_thread_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: helper

/* function: main_thread
 * The start function of the thread.
 * This is the same for all threads.
 * It initializes signalstack the <threadcontext_t> variable
 * and calls the user supplied main function. */
static void * main_thread(thread_startargument_t * startarg)
{
   int err ;

   gt_thread.main_task  = startarg->main_task ;
   gt_thread.main_arg   = startarg->main_arg ;
   gt_thread.sys_thread = pthread_self() ;
   gt_thread.stackframe = startarg->stackframe ;

   err = init_threadcontext(&gt_threadcontext) ;
   if (err) {
      TRACECALLERR_LOG("init_threadcontext", err) ;
      goto ONABORT ;
   }

   if (sys_thread_INIT_FREEABLE == pthread_self()) {
      err = EINVAL ;
      TRACEERR_LOG(FUNCTION_WRONG_RETURNVALUE, "pthread_self", STR(sys_thread_INIT_FREEABLE)) ;
      goto ONABORT ;
   }

   // do not access startarg after sigaltstack (startarg is stored on this stack)
   err = sigaltstack(&startarg->signalstack, (stack_t*)0) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("sigaltstack", err) ;
      goto ONABORT ;
   }

   if (0 != getcontext(&gt_thread.continuecontext)) {
      err = errno ;
      TRACESYSERR_LOG("getcontext", err) ;
      goto ONABORT ;
   }

   if (  0 == gt_thread.returncode
         && gt_thread.main_task) {
      gt_thread.returncode = gt_thread.main_task(gt_thread.main_arg) ;
   }

   err = free_threadcontext(&gt_threadcontext) ;
   if (err) {
      TRACECALLERR_LOG("free_threadcontext",err) ;
      goto ONABORT ;
   }

   return (void*)0 ;
ONABORT:
   abort_maincontext(err) ;
   return (void*)err ;
}

static void * calculateoffset_thread(memblock_t * start_arg)
{
   uint8_t * thread  = (uint8_t*) &gt_thread ;

   s_offset_thread = (size_t) (thread - start_arg->addr) ;
   assert(s_offset_thread < start_arg->size) ;

   return 0 ;
}

// group: initonce

int initonce_thread()
{
   /* calculate position of &gt_thread
    * relative to start of stack. */
   int err ;
   pthread_attr_t    thread_attr ;
   sys_thread_t      sys_thread        = sys_thread_INIT_FREEABLE ;
   vmpage_t          stackframe        = memblock_INIT_FREEABLE ;
   bool              isThreadAttrValid = false ;

   // init main thread_t
   gt_thread.sys_thread = pthread_self() ;

   err = init_stackframe(&stackframe) ;
   if (err) goto ONABORT ;

   memblock_t stack = stack_stackframe(&stackframe) ;

   err = pthread_attr_init(&thread_attr) ;
   if (err) {
      TRACESYSERR_LOG("pthread_attr_init",err) ;
      goto ONABORT ;
   }
   isThreadAttrValid = true ;

   err = pthread_attr_setstack(&thread_attr, stack.addr, stack.size) ;
   if (err) {
      TRACESYSERR_LOG("pthread_attr_setstack", err) ;
      goto ONABORT ;
   }

   static_assert( (void* (*) (typeof(&stack)))0 == (typeof(&calculateoffset_thread))0, "calculateoffset_thread expects &stack as argument" ) ;
   err = pthread_create(&sys_thread, &thread_attr, (void*(*)(void*))&calculateoffset_thread, &stack) ;
   if (err) {
      sys_thread = sys_thread_INIT_FREEABLE ;
      TRACESYSERR_LOG("pthread_create", err) ;
      goto ONABORT ;
   }

   err = pthread_join(sys_thread, 0) ;
   if (err) {
      TRACESYSERR_LOG("pthread_join", err) ;
      goto ONABORT ;
   }

   isThreadAttrValid = false ;
   err = pthread_attr_destroy(&thread_attr) ;
   if (err) {
      TRACESYSERR_LOG("pthread_attr_destroy", err) ;
      goto ONABORT ;
   }

   err = free_stackframe(&stackframe) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   if (sys_thread_INIT_FREEABLE != sys_thread) {
      (void) pthread_join(sys_thread, 0) ;
   }
   if (isThreadAttrValid) {
      (void) pthread_attr_destroy(&thread_attr) ;
   }
   (void) free_stackframe(&stackframe) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freeonce_thread()
{
   return 0 ;
}

// group: lifetime

int delete_thread(thread_t ** threadobj)
{
   int err ;
   int err2 ;
   thread_t * delobj = *threadobj ;

   if (delobj) {
      *threadobj = 0 ;

      vmpage_t  stackframe  = vmpage_INIT(size_stackframe(), delobj->stackframe) ;

      err = join_thread(delobj) ;

      err2 = free_stackframe(&stackframe) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int new_thread(/*out*/thread_t ** threadobj, thread_f thread_main, void * main_arg)
{
   int err ;
   int err2 = 0 ;
   pthread_attr_t    thread_attr ;
   thread_t *        thread            = 0 ;
   vmpage_t          stackframe        = vmpage_INIT_FREEABLE ;
   bool              isThreadAttrValid = false ;

   ONERROR_testerrortimer(&s_thread_errtimer, ONABORT) ;
   err = init_stackframe(&stackframe) ;
   if (err) goto ONABORT ;

   memblock_t signalstack = signalstack_stackframe(&stackframe) ;
   memblock_t stack       = stack_stackframe(&stackframe) ;

   thread = (thread_t*) (stack.addr + s_offset_thread) ;

   thread_startargument_t * startarg   = (thread_startargument_t*) signalstack.addr ;

   startarg->main_task   = thread_main ;
   startarg->main_arg    = main_arg ;
   startarg->stackframe  = stackframe.addr ;
   startarg->signalstack = (stack_t) { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size } ;

   ONERROR_testerrortimer(&s_thread_errtimer, ONABORT) ;
   err = pthread_attr_init(&thread_attr) ;
   if (err) {
      TRACESYSERR_LOG("pthread_attr_init",err) ;
      goto ONABORT ;
   }
   isThreadAttrValid = true ;

   ONERROR_testerrortimer(&s_thread_errtimer, ONABORT) ;
   err = pthread_attr_setstack(&thread_attr, stack.addr, stack.size) ;
   if (err) {
      TRACESYSERR_LOG("pthread_attr_setstack",err) ;
      PRINTPTR_LOG(stack.addr) ;
      PRINTSIZE_LOG(stack.size) ;
      goto ONABORT ;
   }

   sys_thread_t sys_thread ;
   ONERROR_testerrortimer(&s_thread_errtimer, ONABORT) ;
   static_assert( (void* (*) (typeof(startarg)))0 == (typeof(&main_thread))0, "main_thread has argument of type startarg") ;
   err = pthread_create(&sys_thread, &thread_attr, (void*(*)(void*))&main_thread, startarg) ;
   if (err) {
      TRACESYSERR_LOG("pthread_create",err) ;
      goto ONABORT ;
   }

   thread->main_task  = thread_main ;
   thread->main_arg   = main_arg ;
   thread->stackframe = stackframe.addr ;
   thread->sys_thread = sys_thread ;

   err2 = pthread_attr_destroy(&thread_attr) ;
   isThreadAttrValid = false ;
   if (err2) {
      TRACESYSERR_LOG("pthread_attr_destroy",err) ;
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
   (void) free_stackframe(&stackframe) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: synchronize

int join_thread(thread_t * threadobj)
{
   int err ;

   if (sys_thread_INIT_FREEABLE != threadobj->sys_thread) {

      err = pthread_join(threadobj->sys_thread, 0) ;
      threadobj->sys_thread = sys_thread_INIT_FREEABLE ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
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
      TRACESYSERR_LOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaddset(&signalmask, SIGINT) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("sigaddset", err) ;
      goto ONABORT ;
   }

   do {
      err = sigwaitinfo(&signalmask, 0) ;
   } while (-1 == err && EINTR == errno) ;

   if (-1 == err) {
      err = errno ;
      TRACESYSERR_LOG("sigwaitinfo", err) ;
      goto ONABORT ;
   }

   return ;
ONABORT:
   abort_maincontext(err) ;
}

void resume_thread(thread_t * threadobj)
{
   int err ;

   err = pthread_kill(threadobj->sys_thread, SIGINT) ;
   if (err) {
      TRACESYSERR_LOG("pthread_kill", err) ;
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
         TRACESYSERR_LOG("nanosleep", err) ;
         goto ONABORT ;
      }
   }

   return ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return ;
}

int exit_thread(int retcode)
{
   int err ;

   VALIDATE_STATE_TEST(! ismain_thread(), ONABORT, ) ;

   gt_thread.returncode = retcode ;

   err = free_threadcontext(&gt_threadcontext) ;
   if (err) {
      TRACECALLERR_LOG("free_threadcontext",err) ;
      abort_maincontext(err) ;
   }

   pthread_exit(0) ;

ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: abort

void abort_thread(void)
{
   gt_thread.returncode = ENOTRECOVERABLE ;
   setcontext(&gt_thread.continuecontext) ;
   assert(0) ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_stackframe(void)
{
   vmpage_t stackframe = vmpage_INIT_FREEABLE ;

   // TEST sizesignalstack_stackframe
   TEST(MINSIGSTKSZ <= sizesignalstack_stackframe()) ;
   TEST(sizesignalstack_stackframe() % pagesize_vm() == 0) ;

   // TEST sizestack_stackframe
   TEST(PTHREAD_STACK_MIN <= sizestack_stackframe()) ;
   TEST(sizestack_stackframe() % pagesize_vm() == 0) ;

   // TEST size_framestack
   TEST(size_stackframe() == pagesize_vm()*3 + sizestack_stackframe() + sizesignalstack_stackframe()) ;
   TEST(size_stackframe() % pagesize_vm() == 0) ;

   // TEST signalstack_stackframe
   for (unsigned i = 0; i < 100; ++i) {
      stackframe.addr = (uint8_t*) (pagesize_vm() * i) ;
      stackframe.size = 0 ; // not used
      memblock_t signalstack = signalstack_stackframe(&stackframe) ;
      TEST(signalstack.addr == stackframe.addr + pagesize_vm()) ;
      TEST(signalstack.size == sizesignalstack_stackframe()) ;
   }

   // TEST stack_stackframe
   for (unsigned i = 0; i < 100; ++i) {
      stackframe.addr = (uint8_t*) (pagesize_vm() * i) ;
      stackframe.size = 0 ; // not used
      memblock_t stack = stack_stackframe(&stackframe) ;
      TEST(stack.addr == stackframe.addr + 2*pagesize_vm() + sizesignalstack_stackframe()) ;
      TEST(stack.size == sizestack_stackframe()) ;
   }

   // TEST init_stackframe
   TEST(0 == init_stackframe(&stackframe)) ;
   TEST(stackframe.addr != 0) ;
   TEST(stackframe.size == size_stackframe()) ;
   {
      vmpage_t header  = vmpage_INIT(pagesize_vm(), stackframe.addr) ;
      TEST(ismapped_vm(&header, accessmode_PRIVATE)) ;
      vmpage_t header2 = vmpage_INIT(pagesize_vm(), stackframe.addr + pagesize_vm() + sizesignalstack_stackframe()) ;
      TEST(ismapped_vm(&header2, accessmode_PRIVATE)) ;
      vmpage_t tail = vmpage_INIT(pagesize_vm(), stackframe.addr + stackframe.size - pagesize_vm()) ;
      TEST(ismapped_vm(&tail, accessmode_PRIVATE)) ;
      memblock_t signalstack = signalstack_stackframe(&stackframe) ;
      memblock_t stack       = stack_stackframe(&stackframe) ;
      TEST(ismapped_vm(genericcast_vmpage(&signalstack,), accessmode_PRIVATE|accessmode_RDWR)) ;
      TEST(ismapped_vm(genericcast_vmpage(&stack,), accessmode_PRIVATE|accessmode_RDWR)) ;
   }

   // TEST free_stackframe
   vmpage_t old = stackframe ;
   TEST(0 == free_stackframe(&stackframe)) ;
   TEST(0 == stackframe.addr) ;
   TEST(0 == stackframe.size) ;
   TEST(1 == isunmapped_vm(&old)) ;
   TEST(0 == free_stackframe(&stackframe)) ;
   TEST(0 == stackframe.addr) ;
   TEST(0 == stackframe.size) ;
   TEST(1 == isunmapped_vm(&old)) ;

   return 0 ;
ONABORT:
   free_stackframe(&stackframe) ;
   return EINVAL ;
}

static volatile int s_returncode_running = 0 ;
static volatile int s_returncode_signal  = 0 ;
static volatile sys_thread_t s_returncode_threadid = 0 ;

static int thread_donothing(void * dummy)
{
   (void) dummy ;
   return 0 ;
}

static int thread_returncode(intptr_t retcode)
{
   s_returncode_threadid = pthread_self() ;
   s_returncode_running  = 1 ;
   while (s_returncode_running && !s_returncode_signal) {
      pthread_yield() ;
   }
   s_returncode_running = 0 ;
   s_returncode_signal  = 0 ;
   return (int) retcode ;
}

static int test_initfree(void)
{
   thread_t * thread = 0 ;

   // TEST new_thread
   TEST(0 == new_thread(&thread, &thread_donothing, (void*)3)) ;
   TEST(thread) ;
   TEST(thread->wlistnext  == 0) ;
   TEST(thread->main_task  == &thread_donothing) ;
   TEST(thread->main_arg   == (void*)3) ;
   TEST(thread->sys_thread != sys_thread_INIT_FREEABLE) ;
   TEST(thread->returncode == 0) ;
   TEST(thread->stackframe != 0) ;

   // TEST delete_thread
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == thread) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == thread) ;

   // TEST newgeneric_thread: thread is run
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, 14)) ;
   TEST(thread) ;
   TEST(thread->wlistnext  == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)14) ;
   TEST(thread->sys_thread != sys_thread_INIT_FREEABLE) ;
   TEST(thread->returncode == 0) ;
   TEST(thread->stackframe != 0) ;
   uint8_t *    stackframe = thread->stackframe ;
   sys_thread_t T          = thread->sys_thread ;
   // test thread is running
   for (unsigned i = 0; !s_returncode_running && i < 10000000; ++i) {
      yield_thread() ;
   }
   TEST(s_returncode_running) ;
   TEST(s_returncode_threadid == T) ;
   TEST(thread) ;
   TEST(thread->wlistnext  == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)14) ;
   TEST(thread->sys_thread == T) ;
   TEST(thread->returncode == 0) ;
   TEST(thread->stackframe == stackframe) ;
   s_returncode_running = 0 ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == thread) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == thread) ;

   // TEST delete_thread: join_thread called from delete_thread
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, 11)) ;
   TEST(thread) ;
   TEST(thread->wlistnext  == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)11) ;
   TEST(thread->sys_thread != sys_thread_INIT_FREEABLE) ;
   TEST(thread->returncode == 0) ;
   TEST(thread->stackframe != 0) ;
   T = thread->sys_thread ;
   s_returncode_signal = 1 ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == s_returncode_signal) ; // => delete has waited until thread has run
   TEST(T == s_returncode_threadid) ;

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
   s_returncode_signal  = 1 ;
   s_returncode_running = 0 ;
   TEST(0 == delete_thread(&thread)) ;

   return 0 ;
ONABORT:
   s_returncode_signal  = 1 ;
   s_returncode_running = 0 ;
   delete_thread(&thread) ;
   return EINVAL ;
}

static int test_query(void)
{
   thread_t    thread ;

   // TEST sys_context_thread
   TEST(&sys_context_thread() == &gt_threadcontext) ;

   // TEST self_thread
   TEST(self_thread() == &gt_thread) ;

   // TEST returncode_thread
   for (int R = -10; R <= 10; ++R) {
      thread.returncode = R ;
      TEST(R == returncode_thread(&thread)) ;
   }

   // TEST maintask_thread
   thread.main_task = (thread_f)&thread_returncode ;
   TEST(maintask_thread(&thread) == (thread_f)&thread_returncode) ;
   thread.main_task = 0 ;
   TEST(maintask_thread(&thread) == 0) ;

   // TEST mainarg_thread
   for (uintptr_t A = 0; A <= 10; ++A) {
      thread.main_arg = (void*)A ;
      TEST(A == (uintptr_t)mainarg_thread(&thread)) ;
   }

   // TEST ismain_thread
   TEST(0 == gt_thread.stackframe)
   TEST(1 == ismain_thread()) ;
   gt_thread.stackframe = (uint8_t*)1 ;
   TEST(0 == ismain_thread()) ;
   gt_thread.stackframe = (uint8_t*)0 ;
   TEST(1 == ismain_thread()) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_join(void)
{
   thread_t * thread = 0 ;

   // TEST join_thread
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, 12)) ;
   s_returncode_signal = 1 ;
   TEST(0 == join_thread(thread)) ;
   s_returncode_signal = 0 ;  // returned
   TEST(thread->wlistnext  == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)12) ;
   TEST(thread->sys_thread == sys_thread_INIT_FREEABLE) ;
   TEST(thread->returncode == 12) ;
   TEST(thread->stackframe != 0) ;

   // TEST join_thread: calling on already joined thread
   TEST(0 == join_thread(thread)) ;
   TEST(thread->wlistnext  == 0) ;
   TEST(thread->main_task  == (thread_f)&thread_returncode) ;
   TEST(thread->main_arg   == (void*)12) ;
   TEST(thread->sys_thread == sys_thread_INIT_FREEABLE) ;
   TEST(thread->returncode == 12) ;
   TEST(thread->stackframe != 0) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST join_thread: different returncode
   for (int i = -5; i < 5; ++i) {
      const intptr_t arg = 1111 * i ;
      TEST(0 == newgeneric_thread(&thread, thread_returncode, arg)) ;
      TEST(thread->sys_thread != sys_thread_INIT_FREEABLE) ;
      s_returncode_signal = 1 ;
      TEST(0 == join_thread(thread)) ;
      TEST(0 == s_returncode_signal) ;
      TEST(thread->wlistnext  == 0) ;
      TEST(thread->main_task  == (thread_f)&thread_returncode) ;
      TEST(thread->main_arg   == (void*)arg) ;
      TEST(thread->sys_thread == sys_thread_INIT_FREEABLE) ;
      TEST(thread->returncode == arg) ;
      TEST(thread->stackframe != 0) ;
      TEST(0 == join_thread(thread)) ;
      TEST(thread->wlistnext  == 0) ;
      TEST(thread->main_task  == (thread_f)&thread_returncode) ;
      TEST(thread->main_arg   == (void*)arg) ;
      TEST(thread->sys_thread == sys_thread_INIT_FREEABLE) ;
      TEST(thread->returncode == arg) ;
      TEST(thread->stackframe != 0) ;
      TEST(0 == delete_thread(&thread)) ;
   }

   // TEST join_thread: EDEADLK
   thread_t selfthread = { .sys_thread = pthread_self() } ;
   TEST(EDEADLK == join_thread(&selfthread)) ;

   // TEST join_thread: ESRCH
   TEST(0 == newgeneric_thread(&thread, &thread_returncode, 0)) ;
   thread_t copied_thread = *thread ;
   s_returncode_signal = 1 ;
   TEST(0 == join_thread(thread)) ;
   TEST(thread->sys_thread == sys_thread_INIT_FREEABLE) ;
   TEST(thread->returncode == 0) ;
   TEST(ESRCH == join_thread(&copied_thread)) ;
   TEST(0 == delete_thread(&thread)) ;

   return 0 ;
ONABORT:
   s_returncode_signal  = 1 ;
   s_returncode_running = 0 ;
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
   vmpage_t stackframe = vmpage_INIT(size_stackframe(), self_thread()->stackframe) ;
   s_sigaltstack_signalstack = signalstack_stackframe(&stackframe) ;
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
   memblock_t  altstack     = memblock_INIT_FREEABLE ;
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
   TEST(0 == thread->returncode) ;
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
   self_thread()->returncode = 0 ;

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

   if (self_thread()->returncode != ENOTRECOVERABLE) {
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
   TEST(thread->returncode == ENOTRECOVERABLE) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST setcontinue_thread
   TEST(0 == new_thread(&thread, &thread_callsetcontinue, 0)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == thread->returncode) ;
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
   thread_t          * thread = 0 ;
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
   TEST(thread->main_task  == (thread_f)&thread_stackoverflow) ;
   TEST(thread->main_arg   == 0) ;
   TEST(thread->returncode == ENOTRECOVERABLE) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST abort_thread: own thread can do so also
   s_stackoverflow_issignal = 0 ;
   bool is_abort = false ;
   TEST(0 == setcontinue_thread(&is_abort)) ;
   if (!s_stackoverflow_issignal) {
      TEST(0 == pthread_kill(pthread_self(), SIGSEGV)) ;
   }
   TEST(1 == s_stackoverflow_issignal) ;
   TEST(1 == is_abort) ;
   TEST(gt_thread.main_task  == 0) ;
   TEST(gt_thread.main_arg   == 0) ;
   TEST(gt_thread.returncode == ENOTRECOVERABLE) ;
   gt_thread.returncode = 0 ;

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


typedef struct test_123_t test_123_t ;

struct test_123_t {
   int i ;
   double d ;
} ;

static __thread int        s_returnvar_int         = 123 ;
static __thread int     (* s_returnvar_fct) (void) = &test_initfree ;
static __thread test_123_t s_returnvar_struct      = { 1, 2 } ;

static int thread_returnvar1(void * start_arg)
{
   (void) start_arg ;
   int err = (s_returnvar_int != 123) ;
   s_returnvar_int = 0 ;
   return err ;
}

static int thread_returnvar2(void * start_arg)
{
   (void) start_arg ;
   int err = (s_returnvar_fct != &test_initfree) ;
   s_returnvar_fct = 0 ;
   return err ;
}

static int thread_returnvar3(void * start_arg)
{
   (void) start_arg ;
   int err = (s_returnvar_struct.i != 1) || (s_returnvar_struct.d != 2) ;
   s_returnvar_struct.i = 0 ;
   s_returnvar_struct.d = 0 ;
   return err ;
}

static int test_threadlocalstorage(void)
{
   thread_t * thread1 = 0 ;
   thread_t * thread2 = 0 ;
   thread_t * thread3 = 0 ;

   // TEST new_thread: TLS variables are correct initialized before thread is created
   TEST(s_returnvar_int      == 123) ;
   TEST(s_returnvar_fct      == &test_initfree) ;
   TEST(s_returnvar_struct.i == 1)
   TEST(s_returnvar_struct.d == 2) ;
   TEST(0 == new_thread(&thread1, &thread_returnvar1, 0)) ;
   TEST(0 == new_thread(&thread2, &thread_returnvar2, 0)) ;
   TEST(0 == new_thread(&thread3, &thread_returnvar3, 0)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == join_thread(thread3)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == returncode_thread(thread3)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(0 == delete_thread(&thread3)) ;
   // TLS variables in main thread have not changed
   TEST(s_returnvar_int      == 123) ;
   TEST(s_returnvar_fct      == &test_initfree) ;
   TEST(s_returnvar_struct.i == 1)
   TEST(s_returnvar_struct.d == 2) ;

   // TEST new_thread: TLS variables are always initialized with static initializers !
   s_returnvar_int      = 124 ;
   s_returnvar_fct      = &test_sigaltstack ;
   s_returnvar_struct.i = 2 ;
   s_returnvar_struct.d = 4 ;
   TEST(0 == new_thread(&thread1, thread_returnvar1, 0)) ;
   TEST(0 == new_thread(&thread2, thread_returnvar2, 0)) ;
   TEST(0 == new_thread(&thread3, thread_returnvar3, 0)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == join_thread(thread3)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == returncode_thread(thread3)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(0 == delete_thread(&thread3)) ;
   // TLS variables in main thread have not changed
   TEST(s_returnvar_int      == 124) ;
   TEST(s_returnvar_fct      == &test_sigaltstack) ;
   TEST(s_returnvar_struct.i == 2)
   TEST(s_returnvar_struct.d == 4) ;
   s_returnvar_int      = 123 ;
   s_returnvar_fct      = &test_initfree ;
   s_returnvar_struct.i = 1 ;
   s_returnvar_struct.d = 2 ;

   return 0 ;
ONABORT:
   delete_thread(&thread1) ;
   delete_thread(&thread2) ;
   delete_thread(&thread3) ;
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

      vmpage_t     stackframe = vmpage_INIT(size_stackframe(), startarg.thread[i]->stackframe) ;
      startarg.signalstack[i] = signalstack_stackframe(&stackframe) ;
      startarg.threadstack[i] = stack_stackframe(&stackframe) ;
   }
   // startarg initialized
   startarg.isvalid = true ;
   // wait for exit of all threads and check returncode == OK
   for (unsigned i = 0; i < lengthof(startarg.isValid); ++i) {
      TEST(0 == join_thread(startarg.thread[i])) ;
      TEST(0 == startarg.thread[i]->returncode) ;
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
   err = send_rtsignal((rtsignal_t)signr) ;
   if (!err) {
      suspend_thread() ;
      err = send_rtsignal((rtsignal_t)(signr+1)) ;
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
   err = wait_rtsignal((rtsignal_t)signr, 1) ;
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

   // TEST suspend_thread: thread suspends
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == newgeneric_thread(&thread1, thread_suspend, 0)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   // now suspended

   // TEST resume_thread: main thread resumes suspended thread
   resume_thread(thread1) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == trywait_rtsignal(1)) ;
   TEST(0 == delete_thread(&thread1)) ;

   // TEST resume_thread: other threads resume suspended thread
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == newgeneric_thread(&thread1, &thread_suspend, 0)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == newgeneric_thread(&thread2, &thread_resume, thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == trywait_rtsignal(1)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;

   // TEST resume_thread: main thread resumes threads before they called suspend_thread
   //                     test that resume_thread is preserved !
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == newgeneric_thread(&thread1, thread_waitsuspend, 0)) ;
   TEST(0 == newgeneric_thread(&thread2, thread_waitsuspend, 0)) ;
   resume_thread(thread1) ;
   resume_thread(thread2) ;
   TEST(0 == send_rtsignal(0)) ; // start threads
   TEST(0 == send_rtsignal(0)) ;
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

static volatile uint32_t s_countyield_counter   = 0 ;
static volatile uint32_t s_countnoyield_counter = 0 ;
static volatile int      s_countyield_exit      = 0 ;

static int thread_countyield(void * dummy)
{
   (void) dummy ;

   s_countyield_counter = 0 ;

   while (  s_countyield_counter < 10000000
            && !s_countyield_exit) {
      yield_thread() ;
      ++ s_countyield_counter ;
   }

   if (! s_countyield_exit) {
      s_countyield_counter = 0 ;
   }

   return 0 ;
}

static int thread_countnoyield(void * dummy)
{
   (void) dummy ;

   s_countnoyield_counter = 0 ;

   while (  s_countnoyield_counter < 10000000
            && !s_countyield_exit) {
      // give other thread a chance to run
      if (s_countnoyield_counter < 3) yield_thread() ;
      ++ s_countnoyield_counter ;
   }

   s_countnoyield_counter = 0 ;

   return 0 ;
}

static int test_yield(void)
{
   thread_t * thread_yield   = 0 ;
   thread_t * thread_noyield = 0 ;

   // TEST yield_thread
   TEST(0 == new_thread(&thread_yield, &thread_countyield, 0)) ;
   TEST(0 == new_thread(&thread_noyield, &thread_countnoyield, 0)) ;
   TEST(0 == join_thread(thread_noyield)) ;
   s_countyield_exit = 1 ;
   TEST(0 == join_thread(thread_yield)) ;
   // no yield ready
   TEST(0 == s_countnoyield_counter) ;
   // yield not ready
   TEST(0 != s_countyield_counter) ;
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
      TEST(i == thread[i]->returncode) ;
      TEST(0 == delete_thread(&thread[i])) ;
   }

   // TEST exit_thread: EPROTO
   TEST(ismain_thread()) ;
   TEST(EPROTO == exit_thread(0)) ;

   return 0 ;
ONABORT:
   for (int i = 0; i < (int)lengthof(thread); ++i) {
      delete_thread(&thread[i]) ;
   }
   return EINVAL ;
}

static int test_initonce(void)
{
   size_t S = s_offset_thread ;

   // TEST initonce_thread: already called
   TEST(S != 0) ;

   // TEST self_thread() called initonce
   TEST(self_thread()->wlistnext  == 0) ;
   TEST(self_thread()->main_arg   == 0) ;
   TEST(self_thread()->main_task  == 0) ;
   TEST(self_thread()->returncode == 0) ;
   TEST(self_thread()->sys_thread == pthread_self()) ;
   TEST(self_thread()->stackframe == 0) ;

   // TEST initonce_thread
   TEST(0 == initonce_thread()) ;
   TEST(S == s_offset_thread) ;

   // TEST freeonce_thread: (does nothing)
   TEST(0 == freeonce_thread()) ;
   TEST(S == s_offset_thread) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_platform_task_thread()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_exit())                    goto ONABORT ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_stackframe())              goto ONABORT ;
   if (test_initfree())                goto ONABORT ;
   if (test_query())                   goto ONABORT ;
   if (test_join())                    goto ONABORT ;
   if (test_sigaltstack())             goto ONABORT ;
   if (test_abort())                   goto ONABORT ;
   if (test_stackoverflow())           goto ONABORT ;
   if (test_threadlocalstorage())      goto ONABORT ;
   if (test_manythreads())             goto ONABORT ;
   if (test_signal())                  goto ONABORT ;
   if (test_suspendresume())           goto ONABORT ;
   if (test_sleep())                   goto ONABORT ;
   if (test_yield())                   goto ONABORT ;
   if (test_exit())                    goto ONABORT ;
   if (test_initonce())                goto ONABORT ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
