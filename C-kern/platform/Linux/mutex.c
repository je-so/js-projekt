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

   file: C-kern/api/platform/thread.h
    Header file of <Thread>.

   file: C-kern/platform/Linux/thread.c
    Linux specific implementation <Thread Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/platform/thread.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filedescr.h"
#endif


// section: mutex_t

int init_mutex(/*out*/mutex_t * mutex)
{
   int err ;
   pthread_mutexattr_t  attr ;
   bool                 isAttr    = false ;
   sys_mutex_t          sys_mutex = sys_mutex_INIT_DEFAULT ;

   err = pthread_mutexattr_init(&attr) ;
   if (err) goto ONABORT ;
   isAttr = true ;

   err = pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK ) ;
   if (err) goto ONABORT ;

   err = pthread_mutex_init(&sys_mutex, &attr) ;
   if (err) goto ONABORT ;

   isAttr = false ;
   err    = pthread_mutexattr_destroy(&attr) ;
   if (err) goto ONABORT ;

   static_assert(sizeof(*mutex) == sizeof(sys_mutex), "copy is valid") ;
   static_assert((typeof(mutex))0 == (typeof(sys_mutex)*)0, "copy is valid") ;
   memcpy(mutex, (const void*)&sys_mutex, sizeof(sys_mutex)) ;

   return 0 ;
ONABORT:
   if (isAttr) {
      pthread_mutexattr_destroy(&attr) ;
   }
   pthread_mutex_destroy(&sys_mutex) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_destroy(mutex) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int lock_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_lock(mutex) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int unlock_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_unlock(mutex) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// section: test

#ifdef KONFIG_UNITTEST

static int test_mutex_moveable(void)
{
   mutex_t mutex1 = mutex_INIT_DEFAULT ;
   mutex_t mutex2 = mutex_INIT_DEFAULT ;

   // TEST static init => same content (trivially true)
   TEST(0 == memcmp(&mutex1, &mutex2, sizeof(mutex_t))) ;

   // TEST init => same content
   TEST(0 == init_mutex(&mutex1)) ;
   TEST(0 == init_mutex(&mutex2)) ;
   TEST(0 == memcmp(&mutex1, &mutex2, sizeof(mutex_t))) ;
   TEST(0 == free_mutex(&mutex1)) ;
   TEST(0 == free_mutex(&mutex2)) ;

   return 0 ;
ONABORT:
   free_mutex(&mutex1) ;
   free_mutex(&mutex2) ;
   return EINVAL ;
}

static ucontext_t    s_thread_usercontext ;
static volatile int  s_shared_count = 0 ;
static volatile int  s_shared_wrong = 0 ;

static int thread_loop(mutex_t * mutex)
{
   int err = 0 ;

   for(int i = 0 ; i < 1000000; ++i) {
      int v = s_shared_wrong + 1 ;
      err = lock_mutex(mutex) ;
      if (err) break ;
      ++ s_shared_count ;
      err = unlock_mutex(mutex) ;
      if (err) break ;
      s_shared_wrong = v ;
   }

   return err ;
}

static int thread_sloop(mutex_t * mutex)
{
   int err = 0 ;

   for(int i = 0 ; i < 100000; ++i) {
      int v = s_shared_wrong + 1 ;
      slock_mutex(mutex) ;
      ++ s_shared_count ;
      sunlock_mutex(mutex) ;
      if (err) break ;
      s_shared_wrong = v ;
   }

   return err ;
}

static volatile int s_lockmutex_signal = 0 ;

static int thread_lockunlockmutex(mutex_t * mutex)
{
   int err ;
   err = lock_mutex(mutex) ;
   if (!err) {
      s_lockmutex_signal = 1 ;
      while( s_lockmutex_signal ) {
         pthread_yield() ;
      }
      err = unlock_mutex(mutex) ;
   }
   return err ;
}

static int thread_freemutex(mutex_t * mutex)
{
   int err ;
   err = free_mutex(mutex) ;
   return err ;
}

static int thread_unlockmutex(mutex_t * mutex)
{
   int err ;
   err = unlock_mutex(mutex) ;
   return err ;
}

static void sigalarm(int sig)
{
   assert(sig == SIGALRM) ;
   setcontext(&s_thread_usercontext) ;
}

static int test_mutex_staticinit(void)
{
   mutex_t     mutex     = mutex_INIT_DEFAULT ;
   thread_t    * thread1 = 0 ;
   thread_t    * thread2 = 0 ;

   // TEST double free
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == free_mutex(&mutex)) ;
   mutex = (mutex_t) mutex_INIT_DEFAULT ;

   // TEST 2 threads parallel counting: lock, unlock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_thread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == new_thread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 != s_shared_wrong) ;

   // TEST sequential threads do not need lock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_thread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == new_thread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 == s_shared_wrong) ;

   // TEST DEADLOCK not prevented
   TEST(0 == free_mutex(&mutex)) ;
   mutex = (mutex_t) mutex_INIT_DEFAULT ;
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
      TEST(0 == lock_mutex(&mutex)) ;
      TEST(0 == getcontext(&s_thread_usercontext)) ;
      if (!isDeadlock) {
         isDeadlock = 1 ;
         TEST(0 == setitimer(ITIMER_REAL, &timeout, NULL)) ;
         TEST(0 == lock_mutex(&mutex)) ;
         timeout.it_value.tv_usec = 0 ;
         TEST(0 == setitimer(ITIMER_REAL, &timeout, NULL)) ;
         isDeadlock = 0 ;
      }
      TEST(isDeadlock) ;

      TEST(0 == unlock_mutex(&mutex)) ;
      TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
      TEST(0 == sigaction(SIGALRM, &oldact, 0)) ;
   }

   // TEST EBUSY: calling free on a locked mutex
   s_lockmutex_signal = 0 ;
   TEST(0 == new_thread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EBUSY == free_mutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;

   // TEST calling unlock from another thread is executed
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(0 == new_thread(&thread1, thread_unlockmutex, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   // now check that free generates no error
   TEST(0 == new_thread(&thread1, thread_freemutex, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   mutex = (mutex_t) mutex_INIT_DEFAULT ;

   // TEST calling unlock twice is *UNSPECIFIED* and CANNOT be tested !!
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(0 == unlock_mutex(&mutex)) ;

   // TEST EINVAL: calling lock, unlock after free generates error
   TEST(0 == free_mutex(&mutex)) ;
   TEST(EINVAL == lock_mutex(&mutex)) ;
   TEST(EINVAL == unlock_mutex(&mutex)) ;

   return 0 ;
ONABORT:
   free_mutex(&mutex) ;
   delete_thread(&thread1) ;
   delete_thread(&thread2) ;
   return EINVAL ;
}

static int test_mutex_errorcheck(void)
{
   mutex_t     mutex     = mutex_INIT_DEFAULT ;
   thread_t    * thread1 = 0 ;
   thread_t    * thread2 = 0 ;

   // TEST double free
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == init_mutex(&mutex)) ;
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == init_mutex(&mutex)) ;

   // TEST 2 threads parallel counting: lock, unlock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_thread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == new_thread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 != s_shared_wrong) ;

   // TEST sequential threads do not need lock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_thread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == new_thread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 == s_shared_wrong) ;

   // TEST EDEADLK: calling lock twice is prevented
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(EDEADLK == lock_mutex(&mutex)) ;
   TEST(0 == unlock_mutex(&mutex)) ;

   // TEST EBUSY: calling free on a locked mutex
   s_lockmutex_signal = 0 ;
   TEST(0 == new_thread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EBUSY == free_mutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;

   // TEST EPERM: calling unlock from another thread is prevented
   s_lockmutex_signal = 0 ;
   TEST(0 == new_thread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EPERM == unlock_mutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   // now check that free generates no error
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == init_mutex(&mutex)) ;

   // TEST EPERM: calling unlock twice is prevented
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(0 == unlock_mutex(&mutex)) ;
   TEST(EPERM == unlock_mutex(&mutex)) ;

   // TEST EINVAL: calling lock, unlock after free generates error
   TEST(0 == free_mutex(&mutex)) ;
   TEST(EINVAL == lock_mutex(&mutex)) ;
   TEST(EINVAL == unlock_mutex(&mutex)) ;

   return 0 ;
ONABORT:
   free_mutex(&mutex) ;
   delete_thread(&thread1) ;
   delete_thread(&thread2) ;
   return EINVAL ;
}

static void sigabort(int sig)
{
   assert(sig == SIGABRT) ;
   setcontext(&s_thread_usercontext) ;
}

static int test_mutex_slock(void)
{
   mutex_t           mutex     = mutex_INIT_DEFAULT ;
   thread_t          * thread1 = 0 ;
   thread_t          * thread2 = 0 ;
   bool              isoldprocmask = false ;
   volatile bool     isAbort ;
   sigset_t          oldprocmask ;
   bool              isoldact  = false ;
   struct sigaction  newact ;
   struct sigaction  oldact ;
   int               pipefd[2] = { -1, -1 } ;
   int               oldstderr = -1 ;

   TEST(0 == pipe2(pipefd, O_CLOEXEC)) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 < oldstderr) ;
   TEST(STDERR_FILENO == dup2(pipefd[1], STDERR_FILENO)) ;

   TEST(0 == sigemptyset(&newact.sa_mask)) ;
   TEST(0 == sigaddset(&newact.sa_mask,SIGABRT)) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK,&newact.sa_mask,&oldprocmask)) ;
   isoldprocmask = true ;
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags   = 0 ;
   newact.sa_handler = &sigabort ;
   TEST(0 == sigaction(SIGABRT, &newact, &oldact)) ;
   isoldact = true ;

   // TEST 2 threads parallel counting: lock, unlock
   TEST(0 == init_mutex(&mutex)) ;
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_thread(&thread1, &thread_sloop, &mutex)) ;
   TEST(0 == new_thread(&thread2, &thread_sloop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(200000 == s_shared_count) ;
   TEST(200000 != s_shared_wrong) ;

   // TEST EDEADLK: calling lock twice is prevented
   slock_mutex(&mutex) ;
   isAbort = false ;
   TEST(0 == getcontext(&s_thread_usercontext)) ;
   if (!isAbort) {
      isAbort = 1 ;
      slock_mutex(&mutex) ;
      isAbort = 0 ;
   }
   TEST(isAbort) ;
   sunlock_mutex(&mutex) ;

   // TEST EPERM: calling unlock from another thread is prevented
   s_lockmutex_signal = 0 ;
   TEST(0 == new_thread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   isAbort = false ;
   TEST(0 == getcontext(&s_thread_usercontext)) ;
   if (!isAbort) {
      isAbort = 1 ;
      sunlock_mutex(&mutex) ;
      isAbort = 0 ;
   }
   TEST(isAbort) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_thread(&thread1)) ;
   // now check that free generates no error
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == init_mutex(&mutex)) ;

   // TEST EPERM: calling unlock twice is prevented
   slock_mutex(&mutex) ;
   sunlock_mutex(&mutex) ;
   isAbort = false ;
   TEST(0 == getcontext(&s_thread_usercontext)) ;
   if (!isAbort) {
      isAbort = 1 ;
      sunlock_mutex(&mutex) ;
      isAbort = 0 ;
   }
   TEST(isAbort) ;

   // TEST EINVAL: calling lock, unlock after free generates error
   TEST(0 == free_mutex(&mutex)) ;
   isAbort = false ;
   TEST(0 == getcontext(&s_thread_usercontext)) ;
   if (!isAbort) {
      isAbort = 1 ;
      slock_mutex(&mutex) ;
      isAbort = 0 ;
   }
   TEST(isAbort) ;
   isAbort = false ;
   TEST(0 == getcontext(&s_thread_usercontext)) ;
   if (!isAbort) {
      isAbort = 1 ;
      sunlock_mutex(&mutex) ;
      isAbort = 0 ;
   }
   TEST(isAbort) ;

   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
   TEST(0 == sigaction(SIGABRT, &oldact, 0)) ;

   {
      char buffer[4096] = {0} ;
      FLUSHBUFFER_LOG() ;
      ssize_t bytes = read(pipefd[0], buffer, sizeof(buffer)) ;
      TEST(bytes > 0) ;
      TEST(bytes < (int)sizeof(buffer)) ;
      CPRINTF_LOG(ERR, "%s", buffer) ;
   }

   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == free_filedescr(&oldstderr)) ;
   TEST(0 == free_filedescr(&pipefd[0])) ;
   TEST(0 == free_filedescr(&pipefd[1])) ;

   return 0 ;
ONABORT:
   if (-1 != oldstderr) {
      dup2(oldstderr, STDERR_FILENO) ;
   }
   free_filedescr(&oldstderr) ;
   free_filedescr(&pipefd[0]) ;
   free_filedescr(&pipefd[1]) ;
   if (isoldprocmask)   (void) sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isoldact)        (void) sigaction(SIGABRT, &oldact, 0) ;
   free_mutex(&mutex) ;
   delete_thread(&thread1) ;
   delete_thread(&thread2) ;
   return EINVAL ;
}

static void sigusr1(int sig)
{
   assert(sig == SIGUSR1) ;
   ++ s_lockmutex_signal ;
}

static int thread_lockmutex(mutex_t * mutex)
{
   int err ;
   s_lockmutex_signal = 1 ;
   err = lock_mutex(mutex) ;
   if (!err) {
      err = unlock_mutex(mutex) ;
   }
   return err ;
}

static int test_mutex_interrupt(void)
{
   mutex_t           mutex     = mutex_INIT_DEFAULT ;
   thread_t          * thread1 = 0 ;
   bool              isoldprocmask = false ;
   sigset_t          oldprocmask ;
   bool              isoldact  = false ;
   struct sigaction  newact ;
   struct sigaction  oldact ;


   TEST(0 == sigemptyset(&newact.sa_mask)) ;
   TEST(0 == sigaddset(&newact.sa_mask,SIGUSR1)) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK,&newact.sa_mask,&oldprocmask)) ;
   isoldprocmask = true ;
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags   = 0 ;
   newact.sa_handler = &sigusr1 ;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact)) ;
   isoldact = true ;

   // TEST interrupt is ignored during wait on lock
   TEST(0 == init_mutex(&mutex)) ;
   TEST(0 == lock_mutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == new_thread(&thread1, &thread_lockmutex, &mutex)) ;
   for(int i = 0; i < 1000; ++i) {
      if (s_lockmutex_signal) break ;
      sleepms_thread(1) ;
   }
   TEST(s_lockmutex_signal /*thread started*/) ;
   sleepms_thread(10) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == pthread_kill(thread1->sys_thread, SIGUSR1)) ;
   for(int i = 0; i < 1000; ++i) {
      if (s_lockmutex_signal) break ;
      sleepms_thread(1) ;
   }
   TEST(s_lockmutex_signal /*SIGUSR1 was received by thread*/) ;
   TEST(0 == unlock_mutex(&mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   // no error => lock_mutex has restarted itself
   TEST(0 == thread1->returncode) ;
   TEST(0 == delete_thread(&thread1)) ;

   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
   TEST(0 == sigaction(SIGUSR1, &oldact, 0)) ;

   return 0 ;
ONABORT:
   if (isoldprocmask)   (void) sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isoldact)        (void) sigaction(SIGABRT, &oldact, 0) ;
   free_mutex(&mutex) ;
   delete_thread(&thread1) ;
   return EINVAL ;
}

int unittest_platform_sync_mutex()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   for(int i = 0; i < 2; ++i) {
      // store current mapping
      TEST(0 == init_resourceusage(&usage)) ;

      if (test_mutex_moveable())    goto ONABORT ;
      if (test_mutex_staticinit())  goto ONABORT ;
      if (test_mutex_errorcheck())  goto ONABORT ;
      if (test_mutex_slock())       goto ONABORT ;
      if (test_mutex_interrupt())   goto ONABORT ;

      if (0 == same_resourceusage(&usage)) break ;
      TEST(0 == free_resourceusage(&usage)) ;
      CLEARBUFFER_LOG() ;
   }

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
