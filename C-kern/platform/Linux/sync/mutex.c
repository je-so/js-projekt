/* title: Mutex Linuximpl
   Implements <Mutex>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/sync/mutex.h
    Header file of <Mutex>.

   file: C-kern/platform/Linux/sync/mutex.c
    Linux specific implementation <Mutex Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/sync/mutex.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/task/thread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/task/process.h"
#endif


// section: mutex_t

int init_mutex(/*out*/mutex_t * mutex)
{
   int err ;
   pthread_mutexattr_t  attr ;
   bool                 isAttr    = false ;
   sys_mutex_t          sys_mutex = sys_mutex_INIT_DEFAULT ;

   err = pthread_mutexattr_init(&attr) ;
   if (err) goto ONERR;
   isAttr = true ;

   err = pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK ) ;
   if (err) goto ONERR;

   err = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) ;
   if (err) goto ONERR;

   err = pthread_mutex_init(&sys_mutex, &attr) ;
   if (err) goto ONERR;

   isAttr = false ;
   err    = pthread_mutexattr_destroy(&attr) ;
   if (err) goto ONERR;

   static_assert(sizeof(*mutex) == sizeof(sys_mutex), "copy is valid") ;
   static_assert((typeof(mutex))0 == (typeof(sys_mutex)*)0, "copy is valid") ;
   memcpy(mutex, (const void*)&sys_mutex, sizeof(sys_mutex)) ;

   return 0 ;
ONERR:
   if (isAttr) {
      pthread_mutexattr_destroy(&attr) ;
   }
   pthread_mutex_destroy(&sys_mutex) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int free_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_destroy(mutex) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int lock_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_lock(mutex) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int unlock_mutex(mutex_t * mutex)
{
   int err ;

   err = pthread_mutex_unlock(mutex) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// section: test

#ifdef KONFIG_UNITTEST

static ucontext_t    s_thread_usercontext ;
static volatile int  s_shared_count = 0 ;
static volatile int  s_shared_wrong = 0 ;

static int thread_loop(mutex_t * mutex)
{
   int err = 0 ;

   for (int i = 0 ; i < 1000000; ++i) {
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

   for (int i = 0 ; i < 100000; ++i) {
      int v = s_shared_wrong + 1 ;
      slock_mutex(mutex) ;
      ++ s_shared_count ;
      sunlock_mutex(mutex) ;
      if (err) break ;
      s_shared_wrong = v ;
   }

   return err ;
}

static volatile int s_lockmutex_signal = 0;

static int thread_lockunlockmutex(mutex_t * mutex)
{
   int err;
   err = lock_mutex(mutex);
   if (!err) {
      add_atomicint(&s_lockmutex_signal, 1);
      while (0 != read_atomicint(&s_lockmutex_signal)) {
         pthread_yield();
      }
      err = unlock_mutex(mutex);
   }
   return err;
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

static int test_staticinit(void)
{
   mutex_t     mutex     = mutex_INIT_DEFAULT ;
   thread_t    * thread1 = 0 ;
   thread_t    * thread2 = 0 ;

   // TEST free_mutex: double free
   TEST(0 == free_mutex(&mutex)) ;
   TEST(0 == free_mutex(&mutex)) ;
   mutex = (mutex_t) mutex_INIT_DEFAULT ;

   // TEST 2 threads parallel counting: lock, unlock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == newgeneric_thread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == newgeneric_thread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 != s_shared_wrong) ;

   // TEST sequential threads do not need lock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == newgeneric_thread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == newgeneric_thread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
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
   clear_atomicint(&s_lockmutex_signal);
   TEST(0 == newgeneric_thread(&thread1, &thread_lockunlockmutex, &mutex));
   while (0 == read_atomicint(&s_lockmutex_signal)) {
      pthread_yield();
   }
   TEST( EBUSY == free_mutex(&mutex));
   clear_atomicint(&s_lockmutex_signal);
   TEST( 0 == join_thread(thread1));
   TEST( 0 == returncode_thread(thread1));
   TEST( 0 == delete_thread(&thread1));

   // TEST calling unlock from another thread is executed
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(0 == newgeneric_thread(&thread1, thread_unlockmutex, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == delete_thread(&thread1)) ;
   // now check that free generates no error
   TEST(0 == newgeneric_thread(&thread1, thread_freemutex, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
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
ONERR:
   free_mutex(&mutex) ;
   delete_thread(&thread1) ;
   delete_thread(&thread2) ;
   return EINVAL ;
}

static int test_errorcheck(void)
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
   TEST(0 == newgeneric_thread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == newgeneric_thread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 != s_shared_wrong) ;

   // TEST sequential threads do not need lock
   s_shared_count = 0 ;
   s_shared_wrong = 0 ;
   TEST(0 == newgeneric_thread(&thread1, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == newgeneric_thread(&thread2, &thread_loop, &mutex)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
   TEST(0 == delete_thread(&thread1)) ;
   TEST(0 == delete_thread(&thread2)) ;
   TEST(2000000 == s_shared_count) ;
   TEST(2000000 == s_shared_wrong) ;

   // TEST EDEADLK: calling lock twice is prevented
   TEST(0 == lock_mutex(&mutex)) ;
   TEST(EDEADLK == lock_mutex(&mutex)) ;
   TEST(0 == unlock_mutex(&mutex)) ;

   // TEST EBUSY: calling free on a locked mutex
   clear_atomicint(&s_lockmutex_signal);
   TEST(0 == newgeneric_thread(&thread1, &thread_lockunlockmutex, &mutex));
   while (0 == read_atomicint(&s_lockmutex_signal)) {
      pthread_yield();
   }
   TEST( EBUSY == free_mutex(&mutex));
   clear_atomicint(&s_lockmutex_signal);
   TEST( 0 == join_thread(thread1));
   TEST( 0 == returncode_thread(thread1));
   TEST( 0 == delete_thread(&thread1));

   // TEST EPERM: calling unlock from another thread is prevented
   s_lockmutex_signal = 0 ;
   TEST(0 == newgeneric_thread(&thread1, &thread_lockunlockmutex, &mutex)) ;
   while( ! s_lockmutex_signal ) {
      pthread_yield() ;
   }
   TEST(EPERM == unlock_mutex(&mutex)) ;
   s_lockmutex_signal = 0 ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == returncode_thread(thread1)) ;
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
ONERR:
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

static int test_slock(void)
{
   mutex_t           mutex     = mutex_INIT_DEFAULT ;
   thread_t          * thread1 = 0 ;
   thread_t          * thread2 = 0 ;
   volatile bool     isoldprocmask = false;
   volatile bool     isoldact  = false;
   volatile bool     isAbort;
   sigset_t          oldprocmask ;
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
   TEST(0 == newgeneric_thread(&thread1, &thread_sloop, &mutex)) ;
   TEST(0 == newgeneric_thread(&thread2, &thread_sloop, &mutex)) ;
   TEST(0 == join_thread(thread1)) ;
   TEST(0 == join_thread(thread2)) ;
   TEST(0 == returncode_thread(thread1)) ;
   TEST(0 == returncode_thread(thread2)) ;
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
   TEST(0 == newgeneric_thread(&thread1, &thread_lockunlockmutex, &mutex)) ;
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
   TEST(0 == returncode_thread(thread1)) ;
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
      FLUSHBUFFER_ERRLOG() ;
      ssize_t bytes = read(pipefd[0], buffer, sizeof(buffer)) ;
      TEST(bytes > 0) ;
      TEST(bytes < (int)sizeof(buffer)) ;
      PRINTF_ERRLOG("%s", buffer) ;
   }

   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&oldstderr)) ;
   TEST(0 == free_iochannel(&pipefd[0])) ;
   TEST(0 == free_iochannel(&pipefd[1])) ;

   return 0 ;
ONERR:
   if (-1 != oldstderr) {
      dup2(oldstderr, STDERR_FILENO) ;
   }
   free_iochannel(&oldstderr) ;
   free_iochannel(&pipefd[0]) ;
   free_iochannel(&pipefd[1]) ;
   if (isoldprocmask)   (void) sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isoldact)        (void) sigaction(SIGABRT, &oldact, 0) ;
   free_mutex(&mutex) ;
   delete_thread(&thread1) ;
   delete_thread(&thread2) ;
   return EINVAL ;
}

typedef struct processparam_t    processparam_t;

struct processparam_t {
   mutex_t              mutex   ;
   volatile uint64_t    counter ;
} ;

static int process_counter(processparam_t * param)
{
   int err ;

   for (unsigned i = 0; i < 1000000; ++i) {
      err = lock_mutex(&param->mutex) ;
      if (err) return err ;
      ++ param->counter ;
      err = unlock_mutex(&param->mutex) ;
      if (err) return err ;
   }

   return 0 ;
}

static int test_interprocess(void)
{
   processparam_t *  param      = 0 ;
   process_t         process[2] = { process_FREE, process_FREE } ;
   vmpage_t          shrdmem    = vmpage_FREE ;

   // prepare
   TEST(0 == init2_vmpage(&shrdmem, pagesize_vm(), accessmode_RDWR_SHARED)) ;
   param = (processparam_t*) shrdmem.addr ;
   TEST(0 == init_mutex(&param->mutex)) ;
   param->counter = 0 ;

   // TEST interprocess mutex
   for (unsigned i = 0; i < lengthof(process); ++i) {
      TEST(0 == initgeneric_process(&process[i], &process_counter, param, 0)) ;
   }
   for (unsigned i = 0; i < lengthof(process); ++i) {
      process_result_t result ;
      TEST(0 == wait_process(&process[i], &result)) ;
      TEST(0 == result.returncode) ;
      TEST(process_state_TERMINATED == result.state) ;
   }
   TEST(param->counter == lengthof(process)*1000000) ;

   // unprepare
   for (unsigned i = 0; i < lengthof(process); ++i) {
      TEST(0 == free_process(&process[i])) ;
   }
   TEST(0 == free_mutex(&param->mutex)) ;
   param = 0 ;
   TEST(0 == free_vmpage(&shrdmem)) ;

   return 0 ;
ONERR:
   for (unsigned i = 0; i < lengthof(process); ++i) {
      free_process(&process[i]) ;
   }
   if (param) free_mutex(&param->mutex) ;
   free_vmpage(&shrdmem) ;
   return EINVAL ;
}

static void sigusr1(int sig)
{
   assert(sig == SIGUSR1);
   ++ s_lockmutex_signal;
}

static int thread_lockmutex(mutex_t * mutex)
{
   int err ;
   s_lockmutex_signal = 1;
   err = lock_mutex(mutex) ;
   if (!err) {
      err = unlock_mutex(mutex) ;
   }
   return err ;
}

static void handler_sigcont(int sig)
{
   if (sig != SIGCONT || 4 != write(1, "cont", 4)) {
      exit(1);
   }
}

static int process_lockmutex(mutex_t * mutex)
{
   int err ;
   struct sigaction  newact;
   sigemptyset(&newact.sa_mask);
   sigaddset(&newact.sa_mask, SIGCONT);
   sigprocmask(SIG_UNBLOCK, &newact.sa_mask, 0);
   sigemptyset(&newact.sa_mask);
   newact.sa_flags   = 0 ;
   newact.sa_handler = &handler_sigcont;
   sigaction(SIGCONT, &newact, 0);
   if (1 == write(1, "l", 1)) {
      err = lock_mutex(mutex);
      // 'u' is written only to make sure lock function does not return
      // before unlock in other process is called
      if (1 != write(1, "u", 1)) {
         exit(1);
      }
      if (!err) {
         err = unlock_mutex(mutex);
      }
      if (!err) {
         exit(0);
      }
   }
   exit(1);
}

static int test_interrupt(void)
{
   mutex_t           mutex     = mutex_INIT_DEFAULT;
   thread_t        * thread1 = 0;
   bool              isoldprocmask = false;
   sigset_t          oldprocmask;
   bool              isoldact  = false;
   struct sigaction  newact;
   struct sigaction  oldact;
   process_t         process = process_FREE;
   vmpage_t          vmpage = vmpage_FREE;
   int               pfd[2] = { -1, -1 };

   TEST(0 == sigemptyset(&newact.sa_mask)) ;
   TEST(0 == sigaddset(&newact.sa_mask,SIGUSR1)) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK,&newact.sa_mask,&oldprocmask)) ;
   isoldprocmask = true ;
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags   = 0 ;
   newact.sa_handler = &sigusr1 ;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact)) ;
   isoldact = true ;

   // TEST lock_mutex: interrupt SIGUSR1 is ignored during wait on lock
   TEST(0 == init_mutex(&mutex));
   TEST(0 == lock_mutex(&mutex));
   s_lockmutex_signal = 0;
   TEST(0 == newgeneric_thread(&thread1, &thread_lockmutex, &mutex));
   for (int i = 0; i < 1000; ++i) {
      if (s_lockmutex_signal) break;
      sleepms_thread(1);
   }
   TEST(s_lockmutex_signal /*thread started*/);
   sleepms_thread(10);
   s_lockmutex_signal = 0;
   TEST(0 == pthread_kill(thread1->sys_thread, SIGUSR1));
   // wait for signal received
   for (int i = 0; i < 1000; ++i) {
      if (s_lockmutex_signal) break;
      sleepms_thread(1);
   }
   TEST(s_lockmutex_signal /*SIGUSR1 was received by thread*/);
   TEST(0 == unlock_mutex(&mutex));
   TEST(0 == join_thread(thread1));
   // no error => lock_mutex has restarted itself
   TEST(0 == returncode_thread(thread1));
   TEST(0 == delete_thread(&thread1));

   // unprepare
   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
   TEST(0 == sigaction(SIGUSR1, &oldact, 0)) ;

   // TEST lock_mutex: SIGSTOP / SIGCONT are ignored
   TEST(0 == init2_vmpage(&vmpage, sizeof(mutex_t), accessmode_SHARED|accessmode_RDWR));
   TEST(0 == pipe2(pfd, O_CLOEXEC));
   mutex_t *       smutex = (mutex_t*) vmpage.addr;
   process_stdio_t stdfd  = process_stdio_INIT_DEVNULL;
   process_result_t result;
   redirectout_processstdio(&stdfd, pfd[1]);
   TEST(0 == init_mutex(smutex));
   TEST(0 == lock_mutex(smutex));
   TEST(0 == initgeneric_process(&process, &process_lockmutex, smutex, &stdfd));
   TEST(0 == free_iochannel(&pfd[1]));
   uint8_t buffer[20];
   TEST(1 == read(pfd[0], buffer, sizeof(buffer)));
   TEST('l' == buffer[0]);
   TEST(0 == kill(process, SIGSTOP));
   process_state_e procstate;
   for (int i = 0; i < 1000; ++i) {
      TEST(0 == state_process(&process, &procstate));
      if (process_state_STOPPED == procstate) break;
      sleepms_thread(1);
   }
   TEST(process_state_STOPPED == procstate);
   TEST(0 == kill(process, SIGCONT));
   TEST(4 == read(pfd[0], buffer, sizeof(buffer)));
   // no 'u' is read from pfd[0] after "cont"
   TEST(0 == memcmp(buffer, "cont", 4));
   TEST(0 == unlock_mutex(smutex));
   TEST(0 == wait_process(&process, &result));
   TEST(process_state_TERMINATED == result.state);
   TEST(0 == result.returncode);
   TEST(0 == free_process(&process));
   TEST(0 == free_mutex(smutex));
   TEST(0 == free_iochannel(&pfd[0]));
   TEST(0 == free_vmpage(&vmpage));

   return 0;
ONERR:
   if (isoldprocmask)   (void) sigprocmask(SIG_SETMASK, &oldprocmask, 0);
   if (isoldact)        (void) sigaction(SIGABRT, &oldact, 0);
   free_mutex(&mutex);
   delete_thread(&thread1);
   free_process(&process);
   free_vmpage(&vmpage);
   return EINVAL;
}

int unittest_platform_sync_mutex()
{
   if (test_staticinit())        goto ONERR;
   if (test_errorcheck())        goto ONERR;
   if (test_slock())             goto ONERR;
   if (test_interprocess())      goto ONERR;
   if (test_interrupt())         goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}
#endif
