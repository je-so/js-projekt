/*
   C-System-Layer: C-kern/os/Linux/thread.c
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/os/virtmemory.h"
#endif

typedef struct osthread_stack_t        osthread_stack_t ;
typedef struct osthread_startparam_t   osthread_startparam_t ;
typedef struct osthread_private_t      osthread_private_t ;

struct osthread_startparam_t {
   osthread_private_t   * osthread ;
   umgebung_t           thread_umgebung ;
} ;

struct osthread_stack_t {
   void    * addr ; // addr[0 .. size-1]
   size_t  size ;
} ;

struct osthread_private_t {
   osthread_t        osthread ;
   osthread_stack_t  mapped_frame ;
   osthread_stack_t  signal_stack ;
   osthread_stack_t  thread_stack ;
} ;

static_assert( &((osthread_private_t*)0)->osthread == ((osthread_t*)0), "convertible: (osthread_private_t*) <-> (osthread_t*)") ;


static int unmap_stackmemory_osthread(osthread_private_t * osthread_private)
{
   int err ;
   void *  addr = osthread_private->mapped_frame.addr ;
   size_t  size = osthread_private->mapped_frame.size ;
   if (size) {
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

int delete_osthread(osthread_t ** threadobj)
{
   int err ;

   if (!threadobj || !*threadobj) {
      return 0 ;
   }

   osthread_private_t * osthread_private = (osthread_private_t *) *threadobj ;

   if (!osthread_private->osthread.isJoined) {
      err = EAGAIN ;
      goto ABBRUCH ;
   }

   err = unmap_stackmemory_osthread(osthread_private) ;
   if (err) goto ABBRUCH ;
   free(osthread_private) ;

   *threadobj = 0 ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int mapstacks_osthread(osthread_private_t * thread_private)
{
   int err ;
   const size_t  page_size  = (size_t) sysconf(_SC_PAGE_SIZE) ;
   const size_t  signalstack_size = page_size * ((MINSIGSTKSZ + page_size - 1) / page_size) ;
   const size_t  threadstack_size = page_size * ((PTHREAD_STACK_MIN + page_size - 1) / page_size) ;
   const size_t        stack_size = 3 * page_size + signalstack_size + threadstack_size ;

   uint8_t * stack_addr = (uint8_t*) mmap( 0, stack_size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 ) ;
   if (MAP_FAILED == stack_addr) {
      err = errno ;
      LOG_SYSERR("mmap", err) ;
      LOG_SIZE(stack_size) ;
      goto ABBRUCH ;
   }

   /* Stack layout:
    * | [page_size]: NONE |
    * | [signalstack_size]: READ,WRITE | [page_size]: NONE |
    * | [threadstack_size]: READ,WRITE | [page_size]: NONE |  */

   if (  mprotect(stack_addr + page_size, signalstack_size, PROT_READ|PROT_WRITE)
      || mprotect(stack_addr + 2 * page_size + signalstack_size, threadstack_size, PROT_READ|PROT_WRITE) ) {
      err = errno ;
      LOG_SYSERR("mprotect", err) ;
      goto ABBRUCH ;
   }

   thread_private->mapped_frame.addr = stack_addr ;
   thread_private->mapped_frame.size = stack_size ;
   thread_private->signal_stack.addr = stack_addr + page_size ;
   thread_private->signal_stack.size = signalstack_size ;
   thread_private->thread_stack.addr = stack_addr + 2 * page_size + signalstack_size ;
   thread_private->thread_stack.size = threadstack_size ;

   return 0 ;
ABBRUCH:
   if (MAP_FAILED != stack_addr) {
      if (munmap(stack_addr, stack_size)) {
         LOG_SYSERR("munmap", errno) ;
         LOG_PTR(stack_addr) ;
         LOG_SIZE(stack_size) ;
      }
   }
   LOG_ABORT(err) ;
   return err ;
}


static void * osthread_startpoint(void * start_arg)
{
   int err ;
   osthread_private_t * thread_private ;

   {
      osthread_startparam_t * startparam ;
      startparam     = (osthread_startparam_t*) start_arg ;
      thread_private = startparam->osthread ;

      // move umgebung_t
      static_assert_void( ((umgebung_t*)0) == (typeof(&startparam->thread_umgebung))0 ) ;
      static_assert_void( ((umgebung_t*)0) == (typeof(&gt_umgebung))0 ) ;
      memcpy( &gt_umgebung, (const umgebung_t*)&startparam->thread_umgebung, sizeof(umgebung_t)) ;

      free(startparam) ;
   }

   stack_t new_signal_stack = {
      .ss_sp    = thread_private->signal_stack.addr,
      .ss_size  = thread_private->signal_stack.size,
      .ss_flags = 0
   } ;

   err = sigaltstack( &new_signal_stack, (stack_t*)0) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("sigaltstack", err) ;
      goto ABBRUCH ;
   }

   thread_private->osthread.isMainCalled      = true ;
   err = thread_private->osthread.thread_main(&thread_private->osthread) ;
   thread_private->osthread.thread_returncode = err ;

   err = free_umgebung(&gt_umgebung) ;
   if (err) goto ABBRUCH ;

   return (void*)0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return (void*)err ;
}

int new_osthread(/*out*/osthread_t ** threadobj, thread_main_f thread_main, void * thread_argument)
{
   int err ;
   pthread_attr_t          thread_attr ;
   osthread_startparam_t  * startparam = 0 ;
   osthread_private_t * thread_private = 0 ;
   bool              isThreadAttrValid = false ;

   thread_private = MALLOC(osthread_private_t,malloc,) ;
   if (0 == thread_private) {
      LOG_OUTOFMEMORY(sizeof(osthread_private_t)) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   startparam = MALLOC(osthread_startparam_t,malloc,) ;
   if (0 == startparam) {
      LOG_OUTOFMEMORY(sizeof(osthread_startparam_t)) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   MEMSET0( thread_private ) ;
   thread_private->osthread.thread_main     = thread_main ;
   thread_private->osthread.thread_argument = thread_argument ;

   startparam->osthread = thread_private ;
   err = init_umgebung(&startparam->thread_umgebung, umgebung()->type ? umgebung()->type : umgebung_type_DEFAULT) ;
   if (err) goto ABBRUCH ;

   err = mapstacks_osthread(thread_private) ;
   if (err) goto ABBRUCH ;

   err = pthread_attr_init(&thread_attr) ;
   if (err) {
      LOG_SYSERR("pthread_attr_init",err) ;
      goto ABBRUCH ;
   }
   isThreadAttrValid = true ;

   err = pthread_attr_setstack(&thread_attr, thread_private->thread_stack.addr, thread_private->thread_stack.size) ;
   if (err) {
      LOG_SYSERR("pthread_attr_setstack",err) ;
      LOG_PTR(thread_private->thread_stack.addr) ;
      LOG_SIZE(thread_private->thread_stack.size) ;
      goto ABBRUCH ;
   }
   err = pthread_create( &thread_private->osthread.sys_thread, &thread_attr, osthread_startpoint, startparam) ;
   if (err) {
      LOG_SYSERR("pthread_create",err) ;
      goto ABBRUCH ;
   }
   err = pthread_attr_destroy(&thread_attr) ;
   if (err) {
      LOG_SYSERR("pthread_attr_destroy",err) ;
      // ignore error
   }

   *threadobj = &thread_private->osthread ;
   return 0 ;
ABBRUCH:
   if (isThreadAttrValid) {
      int err2 = pthread_attr_destroy(&thread_attr) ;
      if (err2) {
         LOG_SYSERR("pthread_attr_destroy", err2) ;
         // ignore error
      }
   }
   free(startparam) ;
   (void) delete_osthread((osthread_t**)&thread_private) ;

   LOG_ABORT(err) ;
   return err ;
}

int join_osthread(osthread_t * threadobj)
{
   int err ;
   if (!threadobj->isJoined) {
      void * result = 0 ;
      err = pthread_join(threadobj->sys_thread, &result) ;
      if (err) goto ABBRUCH ;
      assert(result == 0) ;
      threadobj->isJoined  = true ;
   }
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int init_osthreadmutex(osthread_mutex_t * mutex)
{
   int err ;
   bool                isAttr = false ;
   pthread_mutexattr_t   attr ;

   err = pthread_mutexattr_init(&attr) ;
   if (err) goto ABBRUCH ;
   isAttr = true ;

   err = pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK ) ;
   if (err) goto ABBRUCH ;

   err = pthread_mutex_init(&mutex->sys_mutex, &attr) ;
   if (err) goto ABBRUCH ;

   isAttr = false ;
   err    = pthread_mutexattr_destroy(&attr) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   if (isAttr) pthread_mutexattr_destroy(&attr) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_osthreadmutex(osthread_mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_destroy(&mutex->sys_mutex) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int lock_osthreadmutex(osthread_mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_lock(&mutex->sys_mutex) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int unlock_osthreadmutex(osthread_mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_unlock(&mutex->sys_mutex) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_thread,ABBRUCH)

static uint8_t   * s_sigaddr ;
static pthread_t s_threadid ;

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
   osthread_private_t * thread_private = (osthread_private_t *) context ;
   memset(&s_threadid, 0, sizeof(s_threadid)) ;
   s_sigaddr = 0 ;
   uint8_t * stack   = thread_private->signal_stack.addr ;
   size_t stack_size = thread_private->signal_stack.size ;
   TEST( ! pthread_equal(s_threadid, pthread_self()) ) ;
   TEST( ! (stack < s_sigaddr && s_sigaddr < stack+stack_size) ) ;
   TEST(0 == pthread_kill(pthread_self(), SIGUSR1)) ;
   TEST( pthread_equal(s_threadid, pthread_self()) ) ;
   TEST( (stack < s_sigaddr && s_sigaddr < stack+stack_size) ) ;
   return 0 ; //OK
ABBRUCH:
   return 1 ; //ERR
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
      TEST(0 == osthread->thread_argument) ;
      TEST(!osthread->isJoined) ;
      TEST(0 == join_osthread(osthread)) ;
      TEST(osthread->isJoined) ;
      TEST(osthread->isMainCalled) ;
      TEST(0 == osthread->thread_returncode) ;
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

static int s_isStackoverflow = 0 ;
ucontext_t s_thread_usercontext ;

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
   if (context->thread_argument) {
      assert(count == 0) ;
      context->thread_argument = 0 ;
      s_isStackoverflow        = 0 ;
      TEST(0 == getcontext(&s_thread_usercontext)) ;
   } else {
      assert(count > 0) ;
   }
   ++ count ;
   if (!s_isStackoverflow) (void) thread_stackoverflow(context) ;
   context->thread_argument = (void*)1 ;
   count = 0 ;
   return 0 ; //OK
ABBRUCH:
   return 1 ; //ERR
}

static int test_thread_stackoverflow(void)
{
   int err = 1 ;
   sigset_t oldprocmask ;
   struct sigaction newact, oldact ;
   osthread_t * osthread = 0 ;
   bool       isProcmask = false ;
   bool         isAction = false ;

   // test that thread 'thread_stackoverflow' recovers from stack overflow
   // and that a stack overflow is detected with signal SIGSEGV
   {
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
      TEST(osthread->thread_main       == &thread_stackoverflow) ;
      TEST(osthread->thread_argument   == (void*)1) ;
      TEST(osthread->thread_returncode == 0) ;
      TEST(osthread->isJoined) ;
      TEST(osthread->isMainCalled) ;
      // signal own thread
      s_isStackoverflow = 0 ;
      TEST(0 == getcontext(&s_thread_usercontext)) ;
      if (!s_isStackoverflow) {
         TEST(0 == pthread_kill(pthread_self(), SIGSEGV)) ;
      }
      TEST(s_isStackoverflow) ;
   }

   err = 0 ;
ABBRUCH:
   delete_osthread(&osthread) ;
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isAction)     sigaction(SIGSEGV, &oldact, 0) ;
   return err ;
}

static volatile int s_returncode_signal = 0 ;

static int thread_returncode(osthread_t * context)
{
   while( !s_returncode_signal ) {
      pthread_yield() ;
   }
   return (int) context->thread_argument ;
}

static int test_thread_init(void)
{
   int err = 1 ;
   osthread_t  * osthread = 0 ;

   // TEST init, double free
   s_returncode_signal = 1 ;
   TEST(0 == new_osthread(&osthread, thread_returncode, 0)) ;
   TEST(osthread) ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(!osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(!osthread) ;

   // TEST returncode
   for(int i = -5; i < 5; ++i) {
      const int arg = 1111 * i ;
      s_returncode_signal = 0 ;
      TEST(0 == new_osthread(&osthread, thread_returncode, (void*)arg)) ;
      TEST(osthread) ;
      TEST(arg                == (int)osthread->thread_argument) ;
      TEST(&thread_returncode == osthread->thread_main) ;
      for(int yi = 0; yi < 100000 && !osthread->isMainCalled; ++yi) {
         pthread_yield() ;
      }
      TEST(osthread->isMainCalled) ;
      TEST(!osthread->isJoined) ;
      s_returncode_signal = 1 ;
      TEST(0 == join_osthread(osthread)) ;
      TEST(osthread->isMainCalled) ;
      TEST(osthread->isJoined) ;
      TEST(0 == delete_osthread(&osthread)) ;
      TEST(!osthread) ;
   }

   // TEST EDEADLK
   {
      osthread_t self_thread ;
      self_thread.isJoined   = false ;
      self_thread.sys_thread = pthread_self() ;
      TEST(EDEADLK == join_osthread(&self_thread)) ;
   }

   // TEST ESRCH
   s_returncode_signal = 1 ;
   TEST(0 == new_osthread(&osthread, thread_returncode, 0)) ;
   TEST(osthread) ;
   TEST(0 == join_osthread(osthread)) ;
   osthread->isJoined = false ;
   TEST(ESRCH == join_osthread(osthread)) ;

   // TEST EAGAIN
   TEST(!osthread->isJoined) ;
   TEST(EAGAIN == delete_osthread(&osthread)) ;
   osthread->isJoined = true ;
   TEST(0 == delete_osthread(&osthread)) ;

   err = 0 ;
ABBRUCH:
   delete_osthread(&osthread) ;
   return err ;
}


static volatile int s_shared_count = 0 ;
static volatile int s_shared_wrong = 0 ;

static int thread_loop(osthread_t * context)
{
   int err = 0 ;
   osthread_mutex_t * mutex = (osthread_mutex_t *) context->thread_argument ;

   for(int i = 0 ; i < 1000000; ++i) {
      int v = s_shared_wrong + 1 ;
      err = lock_osthreadmutex(mutex) ;
      if (err) break ;
      ++ s_shared_count ;
      err = unlock_osthreadmutex(mutex) ;
      if (err) break ;
      s_shared_wrong = v ;
   }

   return err ;
}

static volatile int s_lockmutex_signal = 0 ;

static int thread_lockunlockmutex(osthread_t * context)
{
   int err ;
   osthread_mutex_t * mutex = (osthread_mutex_t *) context->thread_argument ;
   err = lock_osthreadmutex(mutex) ;
   if (!err) {
      s_lockmutex_signal = 1 ;
      while( s_lockmutex_signal ) {
         pthread_yield() ;
      }
      err = unlock_osthreadmutex(mutex) ;
   }
   return err ;
}

static int thread_freemutex(osthread_t * context)
{
   int err ;
   osthread_mutex_t * mutex = (osthread_mutex_t *) context->thread_argument ;
   err = free_osthreadmutex(mutex) ;
   return err ;
}

static int thread_unlockmutex(osthread_t * context)
{
   int err ;
   osthread_mutex_t * mutex = (osthread_mutex_t *) context->thread_argument ;
   err = unlock_osthreadmutex(mutex) ;
   return err ;
}

static void sigalarm(int sig)
{
   assert(sig == SIGALRM) ;
   setcontext(&s_thread_usercontext) ;
}

static int test_thread_mutex_staticinit(void)
{
   osthread_mutex_t     mutex = osthread_mutex_INIT_DEFAULT ;
   osthread_t       * thread1 = 0 ;
   osthread_t       * thread2 = 0 ;

   // TEST double free
   TEST(0 == free_osthreadmutex(&mutex)) ;
   TEST(0 == free_osthreadmutex(&mutex)) ;
   mutex = (osthread_mutex_t) osthread_mutex_INIT_DEFAULT ;

   // TEST 2 threads parallel counting: lock, unlock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == new_osthread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == thread2->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 != s_shared_wrong) ;

   // TEST sequential threads do not need lock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == new_osthread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == thread2->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 == s_shared_wrong) ;

   // TEST DEADLOCK not prevented
   TEST(0 == free_osthreadmutex(&mutex)) ;
   mutex = (osthread_mutex_t) osthread_mutex_INIT_DEFAULT ;
   {
      volatile int     isDeadlock = 0 ;
      struct itimerval    timeout = { .it_interval = { 0, 0 }, .it_value = { .tv_sec = 0, .tv_usec = 1000000 / 10 } } ;
      sigset_t         oldprocmask ;
      struct sigaction newact, oldact ;
      sigemptyset(&newact.sa_mask) ;
      sigaddset(&newact.sa_mask,SIGALRM) ;
      TEST(0 == sigprocmask(SIG_UNBLOCK,&newact.sa_mask,&oldprocmask)) ;
      sigemptyset(&newact.sa_mask) ;
      newact.sa_flags   = 0 ;
      newact.sa_handler = &sigalarm ;
      TEST(0 == sigaction(SIGALRM, &newact, &oldact)) ;
      TEST(0 == lock_osthreadmutex(&mutex)) ;
      TEST(0 == getcontext(&s_thread_usercontext)) ;
      if (!isDeadlock) {
         isDeadlock = 1 ;
         TEST(0 == setitimer(ITIMER_REAL, &timeout, NULL)) ;
         TEST(0 == lock_osthreadmutex(&mutex)) ;
         timeout.it_value.tv_usec = 0 ;
         TEST(0 == setitimer(ITIMER_REAL, &timeout, NULL)) ;
         isDeadlock = 0 ;
      }
      TEST(isDeadlock) ;

      TEST(0 == unlock_osthreadmutex(&mutex)) ;
      TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
      TEST(0 == sigaction(SIGALRM, &oldact, 0)) ;
   }

   // TEST EBUSY: calling free on a locked mutex
   s_lockmutex_signal = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EBUSY == free_osthreadmutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;

   // TEST calling unlock from another thread is executed
   TEST(0 == lock_osthreadmutex(&mutex)) ;
   TEST(0 == new_osthread(&thread1, thread_unlockmutex, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   // now check that free generates no error
   TEST(0 == new_osthread(&thread1, thread_freemutex, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   mutex = (osthread_mutex_t) osthread_mutex_INIT_DEFAULT ;

   // TEST calling unlock twice is *UNSPECIFIED* and CANNOT be tested !!
   TEST(0 == lock_osthreadmutex(&mutex)) ;
   TEST(0 == unlock_osthreadmutex(&mutex)) ;

   // TEST EINVAL: calling lock, unlock after free generates error
   TEST(0 == free_osthreadmutex(&mutex)) ;
   TEST(EINVAL == lock_osthreadmutex(&mutex)) ;
   TEST(EINVAL == unlock_osthreadmutex(&mutex)) ;

   return 0 ;
ABBRUCH:
   free_osthreadmutex(&mutex) ;
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   return 1 ;
}

static int test_thread_mutex_errorcheck(void)
{
   osthread_mutex_t     mutex = osthread_mutex_INIT_DEFAULT ;
   osthread_t       * thread1 = 0 ;
   osthread_t       * thread2 = 0 ;

   // TEST double free
   TEST(0 == free_osthreadmutex(&mutex)) ;
   TEST(0 == init_osthreadmutex(&mutex)) ;
   TEST(0 == free_osthreadmutex(&mutex)) ;
   TEST(0 == free_osthreadmutex(&mutex)) ;
   TEST(0 == init_osthreadmutex(&mutex)) ;

   // TEST 2 threads parallel counting: lock, unlock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == new_osthread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == thread2->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 != s_shared_wrong) ;

   // TEST sequential threads do not need lock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == new_osthread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == thread2->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 == s_shared_wrong) ;

   // TEST EDEADLK: calling lock twice is prevented
   TEST(0 == lock_osthreadmutex(&mutex)) ;
   TEST(EDEADLK == lock_osthreadmutex(&mutex)) ;
   TEST(0 == unlock_osthreadmutex(&mutex)) ;

   // TEST EBUSY: calling free on a locked mutex
   s_lockmutex_signal = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EBUSY == free_osthreadmutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;

   // TEST EPERM: calling unlock from another thread is prevented
   s_lockmutex_signal = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EPERM == unlock_osthreadmutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   // now check that free generates no error
   TEST(0 == free_osthreadmutex(&mutex)) ;
   TEST(0 == init_osthreadmutex(&mutex)) ;

   // TEST EPERM: calling unlock twice is prevented
   TEST(0 == lock_osthreadmutex(&mutex)) ;
   TEST(0 == unlock_osthreadmutex(&mutex)) ;
   TEST(EPERM == unlock_osthreadmutex(&mutex)) ;

   // TEST EINVAL: calling lock, unlock after free generates error
   TEST(0 == free_osthreadmutex(&mutex)) ;
   TEST(EINVAL == lock_osthreadmutex(&mutex)) ;
   TEST(EINVAL == unlock_osthreadmutex(&mutex)) ;

   return 0 ;
ABBRUCH:
   free_osthreadmutex(&mutex) ;
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   return 1 ;
}

static __thread int st_int = 123 ;
static __thread int (*st_func)(void) = &test_thread_init ;
static __thread struct test_123 {
   int i ;
   double d ;
} st_struct = { 1, 2 } ;

static int thread_returnvar1(osthread_t * context)
{
   assert(0 == context->thread_argument) ;
   int err = (st_int != 123) ;
   st_int = 0 ;
   return err ;
}

static int thread_returnvar2(osthread_t * context)
{
   assert(0 == context->thread_argument) ;
   int err = (st_func != &test_thread_init) ;
   st_func = 0 ;
   return err ;
}

static int thread_returnvar3(osthread_t * context)
{
   assert(0 == context->thread_argument) ;
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
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == thread2->thread_returncode ) ;
   TEST(0 == thread3->thread_returncode ) ;
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
   TEST(0 == thread1->thread_returncode ) ;
   TEST(0 == thread2->thread_returncode ) ;
   TEST(0 == thread3->thread_returncode ) ;
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
   return 1 ;
}

int unittest_os_thread()
{
   vm_mappedregions_t mappedregions  = vm_mappedregions_INIT_FREEABLE ;
   vm_mappedregions_t mappedregions2 = vm_mappedregions_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_vmmappedregions(&mappedregions)) ;

   if (test_thread_init())          goto ABBRUCH ;
   if (test_thread_sigaltstack())   goto ABBRUCH ;
   if (test_thread_stackoverflow()) goto ABBRUCH ;
   if (test_thread_mutex_staticinit())   goto ABBRUCH ;
   if (test_thread_mutex_errorcheck())   goto ABBRUCH ;
   if (test_thread_localstorage())   goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == init_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == free_vmmappedregions(&mappedregions2)) ;

   return 0 ;
ABBRUCH:
   free_vmmappedregions(&mappedregions) ;
   free_vmmappedregions(&mappedregions2) ;
   return 1 ;
}
#endif
