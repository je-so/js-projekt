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

   file: C-kern/api/os/thread.h
    Header file of <Thread>.

   file: C-kern/os/Linux/thread.c
    Linux specific implementation <Thread Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/os/sync/semaphore.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/errortimer.h"
#endif

typedef struct osthread_startargument_t   osthread_startargument_t ;

struct osthread_startargument_t {
   osthread_t      * osthread ;
   /* variable: isAbort
    * Indicates if not all threads could have been started successfully.
    * Ensures transactional behaviour. */
   volatile bool     isAbort ;
   /* variable: isFreeEvents
    * Indicates to a thread that it is responsible to free all event semaphores. */
   bool              isFreeEvents ;
   /* variable: isfreeable_semaphore
    * Threads signal on this event before entering the main function.
    * The first thread waits on this event to know for all other threads have entered
    * the main function. Therefore it is safe to free the two event semaphores which
    * are no longer of use. */
   semaphore_t       isfreeable_semaphore ;
   /* variable: isvalid_abortflag
    * Threads wait on startup for this event.
    * After this event has occurred variable <isAbort> contains the correct value. */
   semaphore_t       isvalid_abortflag ;
   umgebung_type_e   umgtype ;
   stack_t           signalstack ;
} ;


#ifdef KONFIG_UNITTEST
static test_errortimer_t   s_error_in_newmany_loop = test_errortimer_INIT ;
#endif


// section: osthread_stack_t

/* function: signalstacksize_osthreadstack
 * Returns the minimum size of the signal stack.
 * The signal stack is used in case of a signal or
 * exceptions which throw a signal. If for example the
 * the thread stack overflows SIGSEGV signal is thrown.
 * To handle this case the system must have an extra signal stack
 * cause signal handling needs stack space. */
static inline unsigned signalstacksize_osthreadstack(void)
{
   return MINSIGSTKSZ ;
}

/* function: threadstacksize_osthreadstack
 * Returns the minimum size of the thread stack.
 * The stack should be protected against overflow with help
 * of protected virtual memory pages. */
static inline unsigned threadstacksize_osthreadstack(void)
{
   return PTHREAD_STACK_MIN ;
}

/* function: framestacksize_osthreadstack
 * Returns the size of a stack frame for one thread.
 * The last tail protection page is not included. */
static inline size_t framestacksize_osthreadstack(void)
{
   const size_t page_size = pagesize_vm() ;
   const size_t nr_pages1 = (size_t) (signalstacksize_osthreadstack() + page_size - 1) / page_size ;
   const size_t nr_pages2 = (size_t) (threadstacksize_osthreadstack() + page_size - 1) / page_size ;

   return page_size * (2 + nr_pages1 + nr_pages2) ;
}

static osthread_stack_t getsignalstack_osthreadstack(const osthread_stack_t * stackframe)
{
   const size_t page_size = pagesize_vm() ;
   const size_t nr_pages1 = (size_t) (signalstacksize_osthreadstack() + page_size - 1) / page_size ;

   osthread_stack_t stack = {
         .addr = stackframe->addr + page_size,
         .size = page_size * nr_pages1
   } ;

   return stack ;
}

static osthread_stack_t getthreadstack_osthreadstack(const osthread_stack_t * stackframe)
{
   const size_t page_size = pagesize_vm() ;
   const size_t nr_pages1 = (size_t) (signalstacksize_osthreadstack() + page_size - 1) / page_size ;
   const size_t nr_pages2 = (size_t) (threadstacksize_osthreadstack() + page_size - 1) / page_size ;

   osthread_stack_t stack = {
         .addr = stackframe->addr + page_size * (2 + nr_pages1),
         .size = page_size * nr_pages2
   } ;

   return stack ;
}

static int free_osthreadstack(osthread_stack_t * stackframe)
{
   int err ;

   void *  addr = stackframe->addr ;
   size_t  size = stackframe->size ;

   if (size) {
      *stackframe = (memoryblock_aspect_t) memoryblock_aspect_INIT_FREEABLE ;
      if (munmap(addr, size)) {
         err = errno ;
         LOG_SYSERR("munmap", err) ;
         LOG_PTR(addr) ;
         LOG_SIZE(size) ;
         goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int init_osthreadstack(osthread_stack_t * stackframe, uint32_t nr_threads)
{
   int err ;
   const size_t page_size = pagesize_vm() ;
   const size_t framesize = framestacksize_osthreadstack() ;
   osthread_stack_t stack = {
         .addr = MAP_FAILED,
         .size = page_size/*last tail protection page*/ + nr_threads * framesize
   } ;

   PRECONDITION_INPUT(nr_threads != 0, ABBRUCH, ) ;

   if (     stack.size  < (size_t)  (stack.size-page_size)
         || framesize  != (size_t) ((stack.size-page_size) / nr_threads) ) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(0) ;
      goto ABBRUCH ;
   }

   stack.addr = (uint8_t*) mmap( 0, stack.size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 ) ;
   if (MAP_FAILED == stack.addr) {
      err = errno ;
      LOG_SYSERR("mmap", err) ;
      LOG_SIZE(stack.size) ;
      goto ABBRUCH ;
   }

   /* Stack layout:
    *      size        : protection
    * ----------------------------------
    * | [page_size]    : NONE       |
    * | [signalstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * | [threadstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * ...
    * | [signalstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * | [threadstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * */

   osthread_stack_t signalstack = getsignalstack_osthreadstack(&stack) ;
   osthread_stack_t threadstack = getthreadstack_osthreadstack(&stack) ;
   for(uint32_t i = 0; i < nr_threads; ++i) {
      if (mprotect(signalstack.addr, signalstack.size, PROT_READ|PROT_WRITE)) {
         err = errno ;
         LOG_SYSERR("mprotect", err) ;
         goto ABBRUCH ;
      }
      signalstack.addr += framesize ;

      if (mprotect(threadstack.addr, threadstack.size, PROT_READ|PROT_WRITE)) {
         err = errno ;
         LOG_SYSERR("mprotect", err) ;
         goto ABBRUCH ;
      }
      threadstack.addr += framesize ;
   }

   *stackframe = stack ;

   return 0 ;
ABBRUCH:
   if (MAP_FAILED != stack.addr) {
      if (munmap(stack.addr, stack.size)) {
         LOG_SYSERR("munmap", errno) ;
         LOG_PTR(stack.addr) ;
         LOG_SIZE(stack.size) ;
      }
   }
   LOG_ABORT(err) ;
   return err ;
}


// section: osthread_t

// group: helper

/* function: startpoint_osthread
 * The start function of the thread.
 * This is the same for all threads.
 * It initializes signalstack the thread global umgebung variable
 * and calls the user supplied main function. */
static void * startpoint_osthread(void * start_arg)
{
   int            err ;
   osthread_startargument_t * startarg  = (osthread_startargument_t*)start_arg ;
   osthread_t   * osthread              = startarg->osthread ;

   err = init_umgebung(&gt_umgebung, ((osthread_startargument_t*)startarg)->umgtype) ;
   if (err) {
      LOG_CALLERR("init_umgebung",err) ;
      goto ABBRUCH ;
   }

   if (sys_thread_INIT_FREEABLE == pthread_self()) {
      err = EINVAL ;
      LOG_ERRTEXT(FUNCTION_WRONG_RETURNVALUE("pthread_self", STR(sys_thread_INIT_FREEABLE))) ;
      goto ABBRUCH ;
   }

   err = wait_semaphore(&startarg->isvalid_abortflag) ;
   if (err) {
      LOG_CALLERR("wait_semaphore",err) ;
      goto ABBRUCH ;
   }

   if (startarg->isAbort) {
      // silently ignore => undo, exit created thread
   } else {

      err = signal_semaphore(&startarg->isfreeable_semaphore, 1) ;
      if (err) {
         LOG_CALLERR("signal_semaphore",err) ;
         goto ABBRUCH ;
      }

      if (startarg->isFreeEvents) {
         for(uint32_t i = osthread->nr_threads; i ; --i) {
            err = wait_semaphore(&startarg->isfreeable_semaphore) ;
            if (err) {
               LOG_CALLERR("wait_semaphore",err) ;
               goto ABBRUCH ;
            }
         }
         err = free_semaphore(&startarg->isfreeable_semaphore) ;
         if (!err) err = free_semaphore(&startarg->isvalid_abortflag) ;
         if (err) {
            LOG_CALLERR("free_semaphore",err) ;
            goto ABBRUCH ;
         }
      }

      // do not access startarg after sigaltstack (startarg is stored on this stack)
      err = sigaltstack( &startarg->signalstack, (stack_t*)0) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("sigaltstack", err) ;
         goto ABBRUCH ;
      }

      int32_t returncode = osthread->main(osthread) ;
      if (returncode/*values != 0 indicate an error*/) {
         osthread->returncode = returncode ;
      }
   }

   err = free_umgebung(&gt_umgebung) ;
   if (err) {
      LOG_CALLERR("free_umgebung",err) ;
      goto ABBRUCH ;
   }

   return (void*)0 ;
ABBRUCH:
   LOG_FATAL(err) ;
   abort_umgebung() ;
   return (void*)err ;
}

// group: implementation

int delete_osthread(osthread_t ** threadobj)
{
   int err = 0 ;
   int err2 ;

   if (!threadobj || !*threadobj) {
      return 0 ;
   }

   osthread_t * osthread = *threadobj ;
   *threadobj = 0 ;

   err2 = join_osthread(osthread) ;
   if (err2) err = err2 ;

   err2 = free_osthreadstack(&osthread->stackframe) ;
   if (err2) err = err2 ;

   free(osthread) ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int newmany_osthread(/*out*/osthread_t ** threadobj, thread_main_f thread_main, void * thread_argument, uint32_t nr_of_threads)
{
   int err ;
   int err2 = 0 ;
   pthread_attr_t    thread_attr ;
   osthread_t      * osthread          = 0 ;
   semaphore_t       isfreeable_semaphore = semaphore_INIT_FREEABLE ;
   semaphore_t       isvalid_abortflag = semaphore_INIT_FREEABLE ;
   bool              isThreadAttrValid = false ;
   const size_t      arraysize         = nr_of_threads * sizeof(sys_thread_t) ;
   const size_t      objectsize        = sizeof(osthread_t) - sizeof(sys_thread_t) + arraysize ;
   const size_t      framesize         = framestacksize_osthreadstack() ;

   PRECONDITION_INPUT(nr_of_threads != 0, ABBRUCH,)

   if (  nr_of_threads != (arraysize / sizeof(sys_thread_t))
      || objectsize     <  arraysize ) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(0) ;
      goto ABBRUCH ;
   }

   osthread = (osthread_t*) malloc(objectsize) ;
   if (0 == osthread) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(objectsize) ;
      goto ABBRUCH ;
   }

   // init osthread_t fields
   osthread->main        = thread_main ;
   osthread->argument    = thread_argument ;
   osthread->returncode  = 0 ;
   osthread->stackframe  = (memoryblock_aspect_t) memoryblock_aspect_INIT_FREEABLE ;
   osthread->nr_threads  = nr_of_threads ;
   for(uint32_t i = 0; i < nr_of_threads; ++i) {
      osthread->sys_thread[i] = sys_thread_INIT_FREEABLE ;
   }

   err = init_osthreadstack(&osthread->stackframe, nr_of_threads) ;
   if (err) goto ABBRUCH ;

   err = init_semaphore(&isfreeable_semaphore, 0) ;
   if (err) goto ABBRUCH ;

   err = init_semaphore(&isvalid_abortflag, 0) ;
   if (err) goto ABBRUCH ;

   // !! preallocates enough resources so that init_umgebung never fails !!
   // TODO: prepare_umgebung(nr_of_threads) ;

   osthread_stack_t signalstack = getsignalstack_osthreadstack(&osthread->stackframe) ;
   osthread_stack_t threadstack = getthreadstack_osthreadstack(&osthread->stackframe) ;
   for(uint32_t i = 0; i < nr_of_threads; ++i) {

      osthread_startargument_t * startarg = (osthread_startargument_t*) signalstack.addr ;

      *startarg = (osthread_startargument_t) {
         .osthread = osthread,
         .isAbort  = false,
         .isFreeEvents = (0 == i),
         .isfreeable_semaphore = isfreeable_semaphore,
         .isvalid_abortflag    = isvalid_abortflag,
         .umgtype = (umgebung()->type ? umgebung()->type : umgebung_type_DEFAULT),
         .signalstack = (stack_t) { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size }
      } ;

      ONERROR_testerrortimer(&s_error_in_newmany_loop, UNDO_LOOP) ;
      err = pthread_attr_init(&thread_attr) ;
      if (err) {
         LOG_SYSERR("pthread_attr_init",err) ;
         goto UNDO_LOOP ;
      }
      isThreadAttrValid = true ;

      ONERROR_testerrortimer(&s_error_in_newmany_loop, UNDO_LOOP) ;
      err = pthread_attr_setstack(&thread_attr, threadstack.addr, threadstack.size) ;
      if (err) {
         LOG_SYSERR("pthread_attr_setstack",err) ;
         LOG_PTR(threadstack.addr) ;
         LOG_SIZE(threadstack.size) ;
         goto UNDO_LOOP ;
      }

      ONERROR_testerrortimer(&s_error_in_newmany_loop, UNDO_LOOP) ;
      err = pthread_create( &osthread->sys_thread[i], &thread_attr, startpoint_osthread, startarg) ;
      if (err) {
         osthread->sys_thread[i] = sys_thread_INIT_FREEABLE ;
         LOG_SYSERR("pthread_create",err) ;
         goto UNDO_LOOP ;
      }

      ONERROR_testerrortimer(&s_error_in_newmany_loop, UNDO_LOOP) ;
      err = pthread_attr_destroy(&thread_attr) ;
      isThreadAttrValid = false ;
      if (err) {
         LOG_SYSERR("pthread_attr_destroy",err) ;
         goto UNDO_LOOP ;
      }

      signalstack.addr += framesize ;
      threadstack.addr += framesize ;
      continue ;
   UNDO_LOOP:
      // !! frees preallocated resources which will never be used
      // TODO: unprepare_umgebung(nr_of_threads -i -(sys_thread_INIT_FREEABLE != osthread->sys_thread[i])) ;
      for(; i < nr_of_threads; --i) {
         startarg = (osthread_startargument_t*) signalstack.addr ;
         startarg->isAbort = true ;
         signalstack.addr -= framesize ;
         threadstack.addr -= framesize ;
      }
      break ;
   }

   err2 = signal_semaphore(&isvalid_abortflag, nr_of_threads) ;
   if (err2) {
      LOG_CALLERR("signal_semaphore", err2) ;
      goto ABBRUCH ;
   }

   if (err) {
      err2 = join_osthread(osthread) ;
      if (err2) {
         LOG_CALLERR("join_osthread", err2) ;
      }
      goto ABBRUCH ;
   }

   // startevent is freed in first created thread !

   *threadobj = osthread ;
   return 0 ;
ABBRUCH:
   if (err2) {
      LOG_FATAL(err2) ;
      abort_umgebung() ;
   }
   if (isThreadAttrValid) {
      (void) pthread_attr_destroy(&thread_attr) ;
   }
   (void) free_semaphore(&isvalid_abortflag) ;
   (void) free_semaphore(&isfreeable_semaphore) ;
   (void) delete_osthread(&osthread) ;

   LOG_ABORT(err) ;
   return err ;
}

static int join2_osthread(osthread_t * threadobj, uint32_t thread_index)
{
   int err ;

   PRECONDITION_INPUT(thread_index < threadobj->nr_threads, ABBRUCH, LOG_UINT32(thread_index); LOG_UINT32(threadobj->nr_threads)) ;

   if (sys_thread_INIT_FREEABLE != threadobj->sys_thread[thread_index]) {

      err = pthread_join(threadobj->sys_thread[thread_index], 0) ;
      threadobj->sys_thread[thread_index] = sys_thread_INIT_FREEABLE ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int join_osthread(osthread_t * threadobj)
{
   int err = 0 ;
   int err2 ;

   for (uint32_t i = threadobj->nr_threads; i ; ) {
      --i ;
      if (sys_thread_INIT_FREEABLE != threadobj->sys_thread[i]) {
         err2 = join2_osthread(threadobj, i) ;
         if (err2) err = err2 ;
      }
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_thread,ABBRUCH)

static uint8_t  * s_sigaddr ;
static pthread_t  s_threadid ;

static void sigusr1handler(int sig)
{
   int errno_backup = errno ;
   assert(sig == SIGUSR1) ;
   s_threadid = pthread_self() ;
   s_sigaddr  = (uint8_t*) &sig ;
   errno = errno_backup ;
}

static int thread_sigaltstack(osthread_t * context)
{
   memset(&s_threadid, 0, sizeof(s_threadid)) ;
   s_sigaddr = 0 ;
   osthread_stack_t signalstack = getsignalstack_osthreadstack(&context->stackframe) ;
   TEST( ! pthread_equal(s_threadid, pthread_self()) ) ;
   TEST( ! (signalstack.addr < s_sigaddr && s_sigaddr < signalstack.addr+signalstack.size) ) ;
   TEST(0 == pthread_kill(pthread_self(), SIGUSR1)) ;
   TEST( pthread_equal(s_threadid, pthread_self()) ) ;
   TEST( (signalstack.addr < s_sigaddr && s_sigaddr < signalstack.addr+signalstack.size) ) ;
   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_thread_sigaltstack(void)
{
   int err = 1 ;
   osthread_t     * osthread = 0 ;
   uint8_t    * s_alt_stack1 = malloc(SIGSTKSZ) ;
   stack_t             newst = {
                        .ss_sp = s_alt_stack1,
                        .ss_size = SIGSTKSZ,
                        .ss_flags = 0
         } ;
   stack_t            oldst ;
   sigset_t           oldprocmask ;
   struct sigaction newact, oldact ;
   bool    isStack = false ;
   bool isProcmask = false ;
   bool   isAction = false ;

   if (!s_alt_stack1) {
      LOG_OUTOFMEMORY((2*SIGSTKSZ)) ;
      goto ABBRUCH ;
   }

   // test that thread 'thread_sigaltstack' runs under its own sigaltstack
   {
      sigemptyset(&newact.sa_mask) ;
      sigaddset(&newact.sa_mask,SIGUSR1) ;
      TEST(0==sigprocmask(SIG_UNBLOCK,&newact.sa_mask,&oldprocmask)) ;  isProcmask = true ;
      sigemptyset(&newact.sa_mask) ;
      newact.sa_flags = SA_ONSTACK ;
      newact.sa_handler = &sigusr1handler ;
      TEST(0 == sigaction(SIGUSR1, &newact, &oldact)) ; isAction = true ;
      TEST(0 == sigaltstack(&newst, &oldst)) ; isStack = true ;
      TEST(0 == new_osthread(&osthread, thread_sigaltstack, (void*)0)) ;
      TEST(osthread) ;
      TEST(0 == osthread->argument) ;
      TEST(sys_thread_INIT_FREEABLE != osthread->sys_thread[0]) ;
      TEST(0 == join_osthread(osthread)) ;
      TEST(sys_thread_INIT_FREEABLE == osthread->sys_thread[0]) ;
      TEST(0 == osthread->returncode) ;
      // signal own thread
      memset(&s_threadid, 0, sizeof(s_threadid)) ;
      s_sigaddr = 0 ;
      TEST( ! pthread_equal(s_threadid, pthread_self())) ;
      TEST( ! (s_alt_stack1 < s_sigaddr && s_sigaddr < s_alt_stack1+SIGSTKSZ) ) ;
      TEST(0 == pthread_kill(pthread_self(), SIGUSR1)) ;
      TEST( pthread_equal(s_threadid, pthread_self())) ;
      TEST( (s_alt_stack1 < s_sigaddr && s_sigaddr < s_alt_stack1+SIGSTKSZ) ) ;
   }

   err = 0 ;
ABBRUCH:
   delete_osthread(&osthread) ;
   if (isStack)      sigaltstack(&oldst, 0) ;
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isAction)     sigaction(SIGUSR1, &oldact, 0) ;
   free(s_alt_stack1) ;
   return err ;
}

static int        s_isStackoverflow = 0 ;
static ucontext_t s_thread_usercontext ;

static void sigstackoverflow(int sig)
{
   int errno_backup = errno ;
   assert(sig == SIGSEGV) ;
   s_isStackoverflow = 1 ;
   setcontext(&s_thread_usercontext) ;
   errno = errno_backup ;
}

static int thread_stackoverflow(osthread_t * context)
{
   static int count = 0 ;
   if (context->argument) {
      assert(count == 0) ;
      context->argument  = 0 ;
      s_isStackoverflow  = 0 ;
      TEST(0 == getcontext(&s_thread_usercontext)) ;
   } else {
      assert(count > 0) ;
   }
   ++ count ;
   if (!s_isStackoverflow) (void) thread_stackoverflow(context) ;
   context->argument = (void*)1 ;
   count = 0 ;
   return 0 ; // OK
ABBRUCH:
   return EINVAL ;
}

static int test_thread_stackoverflow(void)
{
   sigset_t oldprocmask ;
   struct sigaction newact, oldact ;
   osthread_t * osthread = 0 ;
   bool       isProcmask = false ;
   bool         isAction = false ;

   // test that thread 'thread_stackoverflow' recovers from stack overflow
   // and that a stack overflow is detected with signal SIGSEGV
   sigemptyset(&newact.sa_mask) ;
   sigaddset(&newact.sa_mask,SIGSEGV) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK,&newact.sa_mask,&oldprocmask)) ;
   isProcmask = true ;
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags = SA_ONSTACK ;
   newact.sa_handler = &sigstackoverflow ;
   TEST(0 == sigaction(SIGSEGV, &newact, &oldact)) ;
   isAction = true ;
   s_isStackoverflow = 0 ;
   TEST(0 == new_osthread(&osthread, &thread_stackoverflow, (void*)1)) ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(1 == s_isStackoverflow) ;
   TEST(osthread->main          == &thread_stackoverflow) ;
   TEST(osthread->argument      == (void*)1) ;
   TEST(osthread->returncode    == 0) ;
   TEST(osthread->sys_thread[0] == sys_thread_INIT_FREEABLE) ;
   TEST(0 == delete_osthread(&osthread)) ;

   // signal own thread
   s_isStackoverflow = 0 ;
   TEST(0 == getcontext(&s_thread_usercontext)) ;
   if (!s_isStackoverflow) {
      TEST(0 == pthread_kill(pthread_self(), SIGSEGV)) ;
   }
   TEST(s_isStackoverflow) ;

   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
   TEST(0 == sigaction(SIGSEGV, &oldact, 0)) ;

   return 0 ;
ABBRUCH:
   delete_osthread(&osthread) ;
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isAction)     sigaction(SIGSEGV, &oldact, 0) ;
   return EINVAL ;
}

static volatile int s_returncode_signal  = 0 ;
static volatile int s_returncode_running = 0 ;

static int thread_returncode(osthread_t * context)
{
   s_returncode_running = 1 ;
   while( !s_returncode_signal ) {
      pthread_yield() ;
   }
   return (int) context->argument ;
}

static int test_thread_init(void)
{
   osthread_t  * osthread = 0 ;

   // TEST init, double free
   s_returncode_signal = 0 ;
   TEST(0 == new_osthread(&osthread, thread_returncode, 0)) ;
   TEST(osthread) ;
   TEST(osthread->main          == &thread_returncode) ;
   TEST(osthread->argument      == 0) ;
   TEST(osthread->returncode    == 0) ;
   TEST(osthread->nr_threads    == 1) ;
   TEST(osthread->sys_thread[0] != sys_thread_INIT_FREEABLE) ;
   s_returncode_signal = 1 ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->main          == &thread_returncode) ;
   TEST(osthread->argument      == 0) ;
   TEST(osthread->returncode    == 0) ;
   TEST(osthread->nr_threads    == 1) ;
   TEST(osthread->sys_thread[0] == sys_thread_INIT_FREEABLE) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(!osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(!osthread) ;

   // TEST double join
   s_returncode_signal = 0 ;
   TEST(0 == new_osthread(&osthread, thread_returncode, (void*)11)) ;
   TEST(osthread) ;
   TEST(osthread->main          == &thread_returncode) ;
   TEST(osthread->argument      == (void*)11) ;
   TEST(osthread->returncode    == 0) ;
   TEST(osthread->nr_threads    == 1) ;
   TEST(osthread->sys_thread[0] != sys_thread_INIT_FREEABLE) ;
   s_returncode_signal = 1 ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->sys_thread[0]       == sys_thread_INIT_FREEABLE) ;
   TEST(returncode_osthread(osthread) == 11) ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->sys_thread[0]       == sys_thread_INIT_FREEABLE) ;
   TEST(returncode_osthread(osthread) == 11) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(!osthread) ;

   // TEST free does also join
   s_returncode_signal  = 1 ;
   s_returncode_running = 0 ;
   TEST(0 == new_osthread(&osthread, thread_returncode, 0)) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(1 == s_returncode_running) ; // => delete has waited until thread has run

   // TEST returncode in case of unhandled signal
   // could not be tested cause unhandled signals terminate whole process

   // TEST returncode
   for(int i = -5; i < 5; ++i) {
      const int arg = 1111 * i ;
      s_returncode_signal  = 0 ;
      s_returncode_running = 0 ;
      TEST(0 == new_osthread(&osthread, thread_returncode, (void*)arg)) ;
      TEST(osthread) ;
      TEST(osthread->argument      == (void*)arg) ;
      TEST(osthread->main          == &thread_returncode ) ;
      TEST(osthread->sys_thread[0] != sys_thread_INIT_FREEABLE) ;
      for(int yi = 0; yi < 100000 && !s_returncode_running; ++yi) {
         pthread_yield() ;
      }
      TEST(s_returncode_running) ;
      TEST(osthread->sys_thread[0] != sys_thread_INIT_FREEABLE) ;
      TEST(!osthread->returncode) ;
      s_returncode_signal = 1 ;
      TEST(0 == join_osthread(osthread)) ;
      TEST(osthread->sys_thread[0]       == sys_thread_INIT_FREEABLE) ;
      TEST(returncode_osthread(osthread) == arg) ;
      TEST(0 == delete_osthread(&osthread)) ;
      TEST(!osthread) ;
   }

   // TEST EDEADLK
   {
      osthread_t self_thread = { .nr_threads = 1, .sys_thread[0] = pthread_self() } ;
      TEST(sys_thread_INIT_FREEABLE != self_thread.sys_thread[0]) ;
      TEST(EDEADLK == join_osthread(&self_thread)) ;
   }


   // TEST ESRCH
   {
      osthread_t copied_thread ;
      s_returncode_signal = 0 ;
      TEST(0 == new_osthread(&osthread, thread_returncode, 0)) ;
      TEST(osthread) ;
      copied_thread = *osthread ;
      TEST(sys_thread_INIT_FREEABLE != osthread->sys_thread[0]) ;
      s_returncode_signal = 1 ;
      TEST(0 == join_osthread(osthread)) ;
      TEST(sys_thread_INIT_FREEABLE == osthread->sys_thread[0]) ;
      TEST(ESRCH == join_osthread(&copied_thread)) ;
      TEST(0 == delete_osthread(&osthread)) ;
   }

   return 0 ;
ABBRUCH:
   delete_osthread(&osthread) ;
   return EINVAL ;
}


static __thread int st_int = 123 ;
static __thread int (*st_func)(void) = &test_thread_init ;
static __thread struct test_123 {
   int i ;
   double d ;
} st_struct = { 1, 2 } ;

static int thread_returnvar1(osthread_t * context)
{
   assert(0 == context->argument) ;
   int err = (st_int != 123) ;
   st_int = 0 ;
   return err ;
}

static int thread_returnvar2(osthread_t * context)
{
   assert(0 == context->argument) ;
   int err = (st_func != &test_thread_init) ;
   st_func = 0 ;
   return err ;
}

static int thread_returnvar3(osthread_t * context)
{
   assert(0 == context->argument) ;
   int err = (st_struct.i != 1) || (st_struct.d != 2) ;
   st_struct.i = 0 ;
   st_struct.d = 0 ;
   return err ;
}

static int test_thread_localstorage(void)
{
   osthread_t       * thread1 = 0 ;
   osthread_t       * thread2 = 0 ;
   osthread_t       * thread3 = 0 ;

   // TEST TLS variables are correct initialized before thread is created
   TEST(st_int  == 123) ;
   TEST(st_func == &test_thread_init) ;
   TEST(st_struct.i == 1 && st_struct.d == 2) ;
   TEST(0 == new_osthread(&thread1, thread_returnvar1, 0)) ;
   TEST(0 == new_osthread(&thread2, thread_returnvar2, 0)) ;
   TEST(0 == new_osthread(&thread3, thread_returnvar3, 0)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == join_osthread(thread3)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread3)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(0 == delete_osthread(&thread3)) ;
   TEST(st_int  == 123) ;  // TEST TLS variables in main thread have not changed
   TEST(st_func == &test_thread_init) ;

   // TEST TLS variables are always initialized with static initializers !
   st_int  = 124 ;
   st_func = &test_thread_sigaltstack ;
   st_struct.i = 2 ;
   st_struct.d = 4 ;
   TEST(0 == new_osthread(&thread1, thread_returnvar1, 0)) ;
   TEST(0 == new_osthread(&thread2, thread_returnvar2, 0)) ;
   TEST(0 == new_osthread(&thread3, thread_returnvar3, 0)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == join_osthread(thread3)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread3)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(0 == delete_osthread(&thread3)) ;
   TEST(st_int  == 124) ;  // TEST TLS variables in main thread have not changed
   TEST(st_func == &test_thread_sigaltstack) ;
   TEST(st_struct.i == 2 && st_struct.d == 4) ;
   st_int  = 123 ;
   st_func = &test_thread_init ;
   st_struct.i = 1 ;
   st_struct.d = 2 ;

   return 0 ;
ABBRUCH:
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   delete_osthread(&thread3) ;
   return EINVAL ;
}

static int test_thread_stack(void)
{
   osthread_stack_t  stack = memoryblock_aspect_INIT_FREEABLE ;

   // TEST query signalstacksize
   TEST(MINSIGSTKSZ == signalstacksize_osthreadstack()) ;

   // TEST query stacksize
   TEST(PTHREAD_STACK_MIN == threadstacksize_osthreadstack()) ;

   // TEST query nextstackoffset_osthreadstack
   {
      size_t nr_pages1 = (signalstacksize_osthreadstack() + pagesize_vm() -1) / pagesize_vm() ;
      size_t nr_pages2 = (threadstacksize_osthreadstack() + pagesize_vm() -1) / pagesize_vm() ;
      TEST(framestacksize_osthreadstack() == pagesize_vm() * (2 + nr_pages1 + nr_pages2)) ;
   }

   // TEST init, double free
   TEST(0 == stack.addr) ;
   TEST(0 == stack.size) ;
   TEST(0 == init_osthreadstack(&stack, 1)) ;
   TEST(0 != stack.addr) ;
   TEST(0 != stack.size) ;
   TEST(0 == free_osthreadstack(&stack)) ;
   TEST(0 == stack.addr) ;
   TEST(0 == stack.size) ;
   TEST(0 == free_osthreadstack(&stack)) ;
   TEST(0 == stack.addr) ;
   TEST(0 == stack.size) ;

   for(uint32_t i = 1; i < 64; ++i) {
      size_t nr_pages1 = (signalstacksize_osthreadstack() + pagesize_vm() -1) / pagesize_vm() ;
      size_t nr_pages2 = (threadstacksize_osthreadstack() + pagesize_vm() -1) / pagesize_vm() ;
      // TEST init array
      TEST(0 == init_osthreadstack(&stack, i)) ;
      TEST(stack.addr != 0) ;
      TEST(stack.size == pagesize_vm() + i * framestacksize_osthreadstack()) ;
      // TEST getsignalstack
      TEST(getsignalstack_osthreadstack(&stack).addr == &stack.addr[pagesize_vm()]) ;
      TEST(getsignalstack_osthreadstack(&stack).size == pagesize_vm() * nr_pages1) ;
      // TEST getthreadstack
      TEST(getthreadstack_osthreadstack(&stack).addr == &stack.addr[pagesize_vm() * (2+nr_pages1)]) ;
      TEST(getthreadstack_osthreadstack(&stack).size == pagesize_vm() * nr_pages2) ;
      // Test stack protection
      for(uint32_t o = 0; o < i; ++o) {
         size_t  offset = o * framestacksize_osthreadstack() ;
         uint8_t * addr = &stack.addr[offset] ;
         addr[pagesize_vm()] = 0 ;
         addr[pagesize_vm() * (1+nr_pages1)-1] = 0 ;
         addr[pagesize_vm() * (2+nr_pages1)] = 0 ;
         addr[pagesize_vm() * (2+nr_pages1+nr_pages2)-1] = 0 ;
      }
      TEST(0 == free_osthreadstack(&stack)) ;
      TEST(0 == stack.addr) ;
      TEST(0 == stack.size) ;
   }

   // EINVAL
   TEST(EINVAL == init_osthreadstack(&stack, 0)) ;

   // ENOMEM (variable overflow)
   if (sizeof(size_t) <= sizeof(uint32_t)) {
      TEST(ENOMEM == init_osthreadstack(&stack, 0x0FFFFFFF)) ;
   }

   return 0 ;
ABBRUCH:
   free_osthreadstack(&stack) ;
   return EINVAL ;
}

typedef struct thread_isvalidstack_t   thread_isvalidstack_t ;
struct thread_isvalidstack_t
{
   bool  isSignalstackValid[30] ;
   bool  isThreadstackValid[30] ;
   osthread_stack_t  signalstack[30] ;
   osthread_stack_t  threadstack[30] ;
   mutex_t           lock ;
} ;

static int32_t thread_isvalidstack(osthread_t * osthread)
{
   int err ;
   thread_isvalidstack_t * startarg = (thread_isvalidstack_t*) argument_osthread(osthread) ;
   stack_t                 current_sigaltstack ;

   if (  0 != sigaltstack(0, &current_sigaltstack)
      || 0 != current_sigaltstack.ss_flags) {
      goto ABBRUCH ;
   }

   err = lock_mutex(&startarg->lock) ;
   if (err) goto ABBRUCH ;
   err = unlock_mutex(&startarg->lock) ;
   if (err) goto ABBRUCH ;

   for(unsigned i = 0; i < nrelementsof(startarg->isSignalstackValid); ++i) {
      if (  (void*)startarg->signalstack[i].addr == current_sigaltstack.ss_sp
         &&        startarg->signalstack[i].size == current_sigaltstack.ss_size ) {
         startarg->isSignalstackValid[i] = true ;
         break ;
      }
   }

   for(unsigned i = 0; i < nrelementsof(startarg->isSignalstackValid); ++i) {
      if (  startarg->threadstack[i].addr < (uint8_t*)&startarg
         && (uint8_t*)&startarg < startarg->threadstack[i].addr + startarg->threadstack[i].size ) {
         startarg->isThreadstackValid[i] = true ;
         break ;
      }
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_thread_array(void)
{
   osthread_t            * osthread = 0 ;
   thread_isvalidstack_t   startarg = { .lock = mutex_INIT_DEFAULT } ;

   // TEST init, double free
   s_returncode_signal = 0 ;
   TEST(0 == newmany_osthread(&osthread, thread_returncode, 0, 23)) ;
   TEST(osthread) ;
   TEST(osthread->main          == &thread_returncode) ;
   TEST(osthread->argument      == 0) ;
   TEST(osthread->returncode    == 0) ;
   TEST(osthread->nr_threads    == 23) ;
   for(unsigned i = 0; i < osthread->nr_threads; ++i) {
      TEST(sys_thread_INIT_FREEABLE != osthread->sys_thread[i]) ;
   }
   s_returncode_signal = 1 ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->main          == &thread_returncode) ;
   TEST(osthread->argument      == 0) ;
   TEST(osthread->returncode    == 0) ;
   TEST(osthread->nr_threads    == 23) ;
   for(unsigned i = 0; i < osthread->nr_threads; ++i) {
      TEST(sys_thread_INIT_FREEABLE == osthread->sys_thread[i]) ;
   }
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(0 == osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(0 == osthread) ;

   // Test return values (== 0)
   s_returncode_signal = 1 ;
   TEST(0 == newmany_osthread(&osthread, thread_returncode, 0, 53)) ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->returncode    == 0) ;
   TEST(osthread->nr_threads    == 53) ;
   for(unsigned i = 0; i < osthread->nr_threads; ++i) {
      TEST(sys_thread_INIT_FREEABLE == osthread->sys_thread[i]) ;
   }
   TEST(0 == delete_osthread(&osthread)) ;

   // Test return values (!= 0)
   s_returncode_signal = 1 ;
   TEST(0 == newmany_osthread(&osthread, thread_returncode, (void*)0x0FABC, 87)) ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->returncode    == 0x0FABC) ;
   TEST(osthread->nr_threads    == 87) ;
   for(unsigned i = 0; i < osthread->nr_threads; ++i) {
      TEST(sys_thread_INIT_FREEABLE == osthread->sys_thread[i]) ;
   }
   TEST(0 == delete_osthread(&osthread)) ;

   // Test every thread has its own stackframe
   TEST(0 == lock_mutex(&startarg.lock)) ;
   TEST(0 == newmany_osthread(&osthread, thread_isvalidstack, &startarg, nrelementsof(startarg.isSignalstackValid))) ;

   osthread_stack_t signalstack = getsignalstack_osthreadstack(&osthread->stackframe) ;
   osthread_stack_t threadstack = getthreadstack_osthreadstack(&osthread->stackframe) ;
   size_t           framesize   = framestacksize_osthreadstack() ;
   for(unsigned i = 0; i < nrelementsof(startarg.isSignalstackValid); ++i) {
      startarg.isSignalstackValid[i] = false ;
      startarg.isThreadstackValid[i] = false ;
      startarg.signalstack[i] = signalstack ;
      startarg.threadstack[i] = threadstack ;
      signalstack.addr += framesize ;
      threadstack.addr += framesize ;
   }

   TEST(0 == unlock_mutex(&startarg.lock)) ;
   TEST(0 == delete_osthread(&osthread)) ;

   for(unsigned i = 0; i < nrelementsof(startarg.isSignalstackValid); ++i) {
      TEST(startarg.isSignalstackValid[i]) ;
      TEST(startarg.isThreadstackValid[i]) ;
   }

   // Test error in newmany => executing UNDO_LOOP
   for(int i = 7; i < 27; i += 5) {
      TEST(0 == init_testerrortimer(&s_error_in_newmany_loop, (unsigned)i, 99 + i)) ;
      s_returncode_signal = 1 ;
      TEST((99 + i) == newmany_osthread(&osthread, thread_returncode, 0, 33)) ;
   }

   TEST(0 == free_mutex(&startarg.lock)) ;

   return 0 ;
ABBRUCH:
   (void) unlock_mutex(&startarg.lock) ;
   (void) free_mutex(&startarg.lock) ;
   delete_osthread(&osthread) ;
   return EINVAL ;
}

int unittest_os_thread()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_thread_array())            goto ABBRUCH ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_thread_stack())            goto ABBRUCH ;
   if (test_thread_init())             goto ABBRUCH ;
   if (test_thread_sigaltstack())      goto ABBRUCH ;
   if (test_thread_stackoverflow())    goto ABBRUCH ;
   if (test_thread_localstorage())     goto ABBRUCH ;
   if (test_thread_array())            goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
