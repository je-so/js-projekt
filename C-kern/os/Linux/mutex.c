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
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/os/thread.h"
// #include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: mutex_t

int init_mutex(mutex_t * mutex)
{
   int err ;
   pthread_mutexattr_t  attr ;
   bool                 isAttr    = false ;
   sys_mutex_t          sys_mutex = sys_mutex_INIT_DEFAULT ;

   err = pthread_mutexattr_init(&attr) ;
   if (err) goto ABBRUCH ;
   isAttr = true ;

   err = pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK ) ;
   if (err) goto ABBRUCH ;

   err = pthread_mutex_init(&sys_mutex, &attr) ;
   if (err) goto ABBRUCH ;

   isAttr = false ;
   err    = pthread_mutexattr_destroy(&attr) ;
   if (err) goto ABBRUCH ;

   static_assert(sizeof(*mutex) == sizeof(sys_mutex), "copy is valid") ;
   static_assert((typeof(mutex))0 == (typeof(sys_mutex)*)0, "copy is valid") ;
   memcpy(mutex, (const void*)&sys_mutex, sizeof(sys_mutex)) ;

   return 0 ;
ABBRUCH:
   if (isAttr) {
      pthread_mutexattr_destroy(&attr) ;
   }
   pthread_mutex_destroy(&sys_mutex) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_destroy(mutex) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int lock_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_lock(mutex) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int unlock_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_unlock(mutex) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_sync_mutex,ABBRUCH)


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
ABBRUCH:
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
   mutex_t        mutex   = mutex_INIT_DEFAULT ;
   osthread_t   * thread1 = 0 ;
   osthread_t   * thread2 = 0 ;

   // TEST double free
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == free_mutex(&mutex)) ;
   mutex = (mutex_t) mutex_INIT_DEFAULT ;

   // TEST 2 threads parallel counting: lock, unlock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == new_osthread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
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
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
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
   TEST(0 == new_osthread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EBUSY == free_mutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;

   // TEST calling unlock from another thread is executed
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(0 == new_osthread(&thread1, thread_unlockmutex, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   // now check that free generates no error
   TEST(0 == new_osthread(&thread1, thread_freemutex, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   mutex = (mutex_t) mutex_INIT_DEFAULT ;

   // TEST calling unlock twice is *UNSPECIFIED* and CANNOT be tested !!
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(0 == unlock_mutex(&mutex)) ;

   // TEST EINVAL: calling lock, unlock after free generates error
   TEST(0 == free_mutex(&mutex)) ;
   TEST(EINVAL == lock_mutex(&mutex)) ;
   TEST(EINVAL == unlock_mutex(&mutex)) ;

   return 0 ;
ABBRUCH:
   free_mutex(&mutex) ;
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   return EINVAL ;
}

static int test_mutex_errorcheck(void)
{
   mutex_t        mutex   = mutex_INIT_DEFAULT ;
   osthread_t   * thread1 = 0 ;
   osthread_t   * thread2 = 0 ;

   // TEST double free
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == init_mutex(&mutex)) ;
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == init_mutex(&mutex)) ;

   // TEST 2 threads parallel counting: lock, unlock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == new_osthread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
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
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 == s_shared_wrong) ;

   // TEST EDEADLK: calling lock twice is prevented
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(EDEADLK == lock_mutex(&mutex)) ;
   TEST(0 == unlock_mutex(&mutex)) ;

   // TEST EBUSY: calling free on a locked mutex
   s_lockmutex_signal = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EBUSY == free_mutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;

   // TEST EPERM: calling unlock from another thread is prevented
   s_lockmutex_signal = 0 ;
   TEST(0 == new_osthread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EPERM == unlock_mutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
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
ABBRUCH:
   free_mutex(&mutex) ;
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   return EINVAL ;
}

static void sigabort(int sig)
{
   assert(sig == SIGABRT) ;
   setcontext(&s_thread_usercontext) ;
}

static int test_mutex_slock(void)
{
   mutex_t           mutex   = mutex_INIT_DEFAULT ;
   osthread_t      * thread1 = 0 ;
   osthread_t      * thread2 = 0 ;
   bool              isoldprocmask = false ;
   volatile bool     isAbort ;
   sigset_t          oldprocmask ;
   bool              isoldact = false ;
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
   TEST(0 == new_osthread(&thread1, &thread_sloop, &mutex)) ;
   TEST(0 == new_osthread(&thread2, &thread_sloop, &mutex)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == thread2->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
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
   TEST(0 == new_osthread(&thread1, &thread_lockunlockmutex, &mutex)) ;
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
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == thread1->returncode ) ;
   TEST(0 == delete_osthread(&thread1)) ;
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
      LOG_FLUSHBUFFER() ;
      ssize_t bytes = read(pipefd[0], buffer, sizeof(buffer)) ;
      TEST(bytes > 0) ;
      TEST(bytes < (int)sizeof(buffer)) ;
      LOGC_PRINTF(ERR, "%s", buffer) ;
   }

   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == close(oldstderr)) ;
   TEST(0 == close(pipefd[0])) ;
   TEST(0 == close(pipefd[1])) ;
   oldstderr = pipefd[0] = pipefd[1] = -1 ;

   return 0 ;
ABBRUCH:
   if (-1 != oldstderr) {
      dup2(oldstderr, STDERR_FILENO) ;
   }
   close(oldstderr) ;
   close(pipefd[0]) ;
   close(pipefd[1]) ;
   if (isoldprocmask)   (void) sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isoldact)        (void) sigaction(SIGABRT, &oldact, 0) ;
   free_mutex(&mutex) ;
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   return EINVAL ;
}


int unittest_os_sync_mutex()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_mutex_moveable())    goto ABBRUCH ;
   if (test_mutex_staticinit())  goto ABBRUCH ;
   if (test_mutex_errorcheck())  goto ABBRUCH ;
   if (test_mutex_slock())       goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
