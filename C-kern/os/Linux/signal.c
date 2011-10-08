/* title: PosixSignals Linux
   Implements <PosixSignals> with help of POSIX interface.

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

   file: C-kern/api/os/sync/signal.h
    Header file of <PosixSignals>.

   file: C-kern/os/Linux/signal.c
    Linux specific implementation <PosixSignals Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/sync/signal.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/signum.h"
#include "C-kern/api/os/thread.h"
// TEXTDB:SELECT('#include "'header'"')FROM("C-kern/resource/text.db/signalconfig")WHERE(action=='set')
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

typedef struct signalcallback_t  signalcallback_t ;

/* struct: signalcallback_t
 * Describes an overwritten signal handler. */
struct signalcallback_t {
   /* variable: isvalid
    * Indicates if this structure contains valid information. */
   bool                 isvalid ;
   /* variable: callback
    * Function pointer to new signal handler. */
   signalcallback_f     callback ;
   /* variable: oldstate
    * Contains old signal handler configuration.
    * This value is set in <initprocess_signalconfig>
    * before the signal handler is overwritten. */
   struct sigaction     oldstate ;
} ;

/* struct: signalconfig_t
 * */
struct signalconfig_t {
   /* variable: nr_signal_handlers
    * Number of stored signal handlers. */
   int               nr_signal_handlers ;
   /* variable: signalmask
    * The signal mask of the current thread. */
   sigset_t          signalmask ;
   /* variable:      signal_handlers
    * Stores setting for every signal handler. */
   struct sigaction  signal_handlers[/*nr_signal_handlers*/] ;
} ;


/* variable: s_signalhandler
 * Stores global configuration information for signal handlers.
 * See <signalcallback_t>.
 * All values in the array are set in <initprocess_signalconfig>. */
static signalcallback_t    s_signalhandler[64] = { { .isvalid = false } } ;

/* variable: s_old_signalmask
 * Stores old signal mask.
 * This value is set in <initprocess_signalconfig>
 * before the signal mask is changed. */
static sigset_t            s_old_signalmask ;

// group: helper

/* function: cbdispatcher_signalconfig
 * This signal handler is called for every signal.
 * It dispatches the handling to the configured callback. */
static void cbdispatcher_signalconfig(int signr, siginfo_t * siginfo, void * ucontext)
{
   (void) ucontext ;
   (void) siginfo ;
   if (0 < signr && signr <= (int)nrelementsof(s_signalhandler)) {
      if (s_signalhandler[signr-1].isvalid) {
         s_signalhandler[signr-1].callback((unsigned)signr) ;
      }
   }
}

// group: implementation

static int clearcallback_signalconfig(unsigned signr)
{
   int err ;

   PRECONDITION_INPUT(signr <= nrelementsof(s_signalhandler), ABBRUCH, LOG_INT(signr)) ;

   if (s_signalhandler[signr-1].isvalid) {
      s_signalhandler[signr-1].isvalid  = false ;
      s_signalhandler[signr-1].callback = 0 ;
      err = sigaction((int)signr, &s_signalhandler[signr-1].oldstate, 0) ;
      if (err) {
         LOG_SYSERR("sigaction", err) ;
         LOG_INT(signr) ;
         goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int setcallback_signalconfig(unsigned signr, signalcallback_f callback)
{
   int err ;
   struct sigaction  sighandler ;

   PRECONDITION_INPUT(signr <= nrelementsof(s_signalhandler), ABBRUCH, LOG_INT(signr)) ;

   err = clearcallback_signalconfig(signr) ;
   if (err) goto ABBRUCH ;

   sighandler.sa_flags     = SA_ONSTACK | SA_SIGINFO ;
   sighandler.sa_sigaction = &cbdispatcher_signalconfig ;
   err = sigemptyset(&sighandler.sa_mask) ;
   if (err) {
      err = EINVAL ;
      LOG_SYSERR("sigemptyset", err) ;
      goto ABBRUCH ;
   }

   err = sigaction((int)signr, &sighandler, &s_signalhandler[signr-1].oldstate);
   if (err) {
      LOG_SYSERR("sigaction", err) ;
      LOG_INT(signr) ;
      goto ABBRUCH ;
   }
   s_signalhandler[signr-1].callback = callback ;
   s_signalhandler[signr-1].isvalid  = true ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initprocess_signalconfig()
{
   int err ;
   sigset_t signalmask ;
   int      signr ;
   bool     isoldmask = false ;

   err = sigemptyset(&signalmask) ;
   if (err) goto ABBRUCH_emptyset ;

#define add(_SIGNR)  signr = (_SIGNR) ; if (sigaddset(&signalmask, signr)) goto ABBRUCH_add ;

// TEXTDB:SELECT( (if (description!="") ("   // " description \n) ) "   add("signal") ;")FROM("C-kern/resource/text.db/signalconfig")WHERE(action=='block')
   // used to suspend and resume a single thread
   add(SIGINT) ;
   // SIGRTMIN ... SIGRTMIN+15 used in send_rtsignal
   add(SIGRTMIN) ;
   add(SIGRTMIN+1) ;
   add(SIGRTMIN+2) ;
   add(SIGRTMIN+3) ;
   add(SIGRTMIN+4) ;
   add(SIGRTMIN+5) ;
   add(SIGRTMIN+6) ;
   add(SIGRTMIN+7) ;
   add(SIGRTMIN+8) ;
   add(SIGRTMIN+9) ;
   add(SIGRTMIN+10) ;
   add(SIGRTMIN+11) ;
   add(SIGRTMIN+12) ;
   add(SIGRTMIN+13) ;
   add(SIGRTMIN+14) ;
   add(SIGRTMIN+15) ;
// TEXTDB:END

   err = pthread_sigmask(SIG_BLOCK, &signalmask, &s_old_signalmask) ;
   if (err) goto ABBRUCH_sigmask ;
   isoldmask = true ;

   err = sigemptyset(&signalmask) ;
   if (err) goto ABBRUCH_emptyset ;

// TEXTDB:SELECT("   // "description\n"   add("signal") ;")FROM("C-kern/resource/text.db/signalconfig")WHERE(action=='unblock'||action=='set')
// TEXTDB:END
   err = pthread_sigmask(SIG_UNBLOCK, &signalmask, 0) ;
   if (err) goto ABBRUCH_sigmask ;
#undef add

#define set(_SIGNR, _CALLBACK) \
   static_assert(0 < _SIGNR && _SIGNR <= nrelementsof(s_signalhandler), \
   "s_signalhandler must be big enough" ) ;                             \
   err = setcallback_signalconfig(_SIGNR, _CALLBACK) ;                  \
   if (err) goto ABBRUCH ;

// TEXTDB:SELECT("   // "description\n"   set("signal", "callback") ;")FROM("C-kern/resource/text.db/signalconfig")WHERE(action=='set')
// TEXTDB:END

   return 0 ;
ABBRUCH_sigmask:
   LOG_SYSERR("pthread_sigmask", err) ;
   goto ABBRUCH ;
ABBRUCH_emptyset:
   err = EINVAL ;
   LOG_SYSERR("sigemptyset", err) ;
   goto ABBRUCH ;
ABBRUCH_add:
   err = EINVAL ;
   LOG_SYSERR("sigaddset", err) ;
   LOG_INT(signr) ;
   goto ABBRUCH ;
ABBRUCH:
   if (isoldmask) freeprocess_signalconfig() ;
   LOG_ABORT(err) ;
   return err ;
}

int freeprocess_signalconfig()
{
   int err ;
   int signr ;

   for(unsigned i = 0; i < nrelementsof(s_signalhandler); ++i) {
      if (s_signalhandler[i].isvalid) {
         s_signalhandler[i].isvalid = false ;
         signr = (int)i + 1 ;
         err = sigaction(signr, &s_signalhandler[i].oldstate, 0) ;
         if (err) goto ABBRUCH_sigaction ;
      }
   }

   err = pthread_sigmask(SIG_SETMASK, &s_old_signalmask, 0) ;
   if (err) goto ABBRUCH_sigmask ;

   return 0 ;
ABBRUCH_sigaction:
   LOG_SYSERR("sigaction", err) ;
   LOG_INT(signr) ;
   goto ABBRUCH ;
ABBRUCH_sigmask:
   LOG_SYSERR("pthread_sigmask", err) ;
   goto ABBRUCH ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}


int new_signalconfig(/*out*/signalconfig_t ** sigconfig)
{
   int err ;
   const int         nr_signal_handlers = SIGRTMAX ;
   const size_t      objectsize         = sizeof(signalconfig_t)
                                        + sizeof(struct sigaction) * (unsigned) nr_signal_handlers ;
   signalconfig_t  * newsigconfig       = 0 ;

   newsigconfig = (signalconfig_t*) malloc(objectsize) ;
   if (!newsigconfig) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(objectsize) ;
      goto ABBRUCH ;
   }

   memset(newsigconfig, 0, objectsize) ;
   newsigconfig->nr_signal_handlers = nr_signal_handlers ;

   err = pthread_sigmask(SIG_SETMASK, 0, &newsigconfig->signalmask) ;
   if (err) {
      LOG_SYSERR("pthread_sigmask", err) ;
      goto ABBRUCH ;
   }

   for(int i = nr_signal_handlers; i > 0; --i) {
      if (32 <= i && i < SIGRTMIN) continue ;
      err = sigaction(i, 0, &newsigconfig->signal_handlers[i-1]) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("sigaction(i,...)", err) ;
         LOG_INT(i) ;
         goto ABBRUCH ;
      }
   }

   *sigconfig = newsigconfig ;
   return 0 ;
ABBRUCH:
   (void) delete_signalconfig(&newsigconfig) ;
   LOG_ABORT(err) ;
   return err ;
}

int delete_signalconfig(signalconfig_t ** sigconfig)
{
   signalconfig_t * delsigconfig = *sigconfig ;

   if (delsigconfig) {
      *sigconfig = 0 ;
      free(delsigconfig) ;
   }

   return 0 ;
}

int compare_signalconfig(const signalconfig_t * sigconfig1, const signalconfig_t * sigconfig2)
{
   if (sigconfig1 && sigconfig2) {
      int nr_diff = sigconfig1->nr_signal_handlers - sigconfig2->nr_signal_handlers ;
      nr_diff = signum(nr_diff) ;
      if (nr_diff) return nr_diff ;

      int cmp = memcmp(&sigconfig1->signalmask, &sigconfig2->signalmask, sizeof(sigconfig2->signalmask)) ;
      if (cmp) return cmp ;

      for(int i = sigconfig1->nr_signal_handlers; i > 0; ) {
         --i ;
         cmp = memcmp(&sigconfig1->signal_handlers[i].sa_sigaction, &sigconfig2->signal_handlers[i].sa_sigaction, sizeof(sigconfig2->signal_handlers[i].sa_sigaction)) ;
         if (cmp) return cmp ;
      }

      return 0 ;
   }

   return (sigconfig1) ? 1 : ((sigconfig2) ? -1 : 0) ;
}

// section: rtsignal_t

// TODO send2_rtsignal
/*
int send2_rtsignal(rtsignal_t nr, osthread_t * thread)
{

}
*/

int send_rtsignal(rtsignal_t nr)
{
   int err ;

   PRECONDITION_INPUT(nr < 16, ABBRUCH, LOG_INT(nr)) ;

   err = sigqueue(getpid(), SIGRTMIN+nr, (union sigval) { 0 } ) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("sigqueue", err) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int wait_rtsignal(rtsignal_t nr, uint32_t nr_signals)
{
   int err ;
   sigset_t signalmask ;

   PRECONDITION_INPUT(nr < 16, ABBRUCH, LOG_INT(nr)) ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = EINVAL ;
      LOG_SYSERR("sigemptyset", err) ;
      goto ABBRUCH ;
   }

   err = sigaddset(&signalmask, SIGRTMIN+nr) ;
   if (err) {
      err = EINVAL ;
      LOG_SYSERR("sigaddset", err) ;
      LOG_INT(SIGRTMIN+nr) ;
      goto ABBRUCH ;
   }

   for(uint32_t i = nr_signals; i; --i) {
      do {
         err = sigwaitinfo(&signalmask, 0) ;
      } while(-1 == err && EINTR == errno) ;

      if (-1 == err) {
         err = errno ;
         LOG_SYSERR("sigwaitinfo", err) ;
         goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int trywait_rtsignal(rtsignal_t nr)
{
   int err ;
   sigset_t          signalmask ;
   struct timespec   ts = { 0, 0 } ;

   PRECONDITION_INPUT(nr < 16, ABBRUCH, LOG_INT(nr)) ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = EINVAL ;
      LOG_SYSERR("sigemptyset", err) ;
      goto ABBRUCH ;
   }

   err = sigaddset(&signalmask, SIGRTMIN+nr) ;
   if (err) {
      err = EINVAL ;
      LOG_SYSERR("sigaddset", err) ;
      LOG_INT(SIGRTMIN+nr) ;
      goto ABBRUCH ;
   }

   for(;;) {
      err = sigtimedwait(&signalmask, 0, &ts) ;
      if (-1 != err) break ;
      err = errno ;
      if (EAGAIN == err) {
         return err ;
      }
      if (EINTR != err) {
         LOG_SYSERR("sigtimedwait", err) ;
         goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// section: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_sync_signal,ABBRUCH)

void dummy_sighandler(int signr, siginfo_t * siginfo, void * ucontext)
{
   (void)signr ;
   (void)siginfo ;
   (void)ucontext ;
}

static int test_initfree(void)
{
   signalconfig_t  * sigconfig1 = 0 ;
   signalconfig_t  * sigconfig2 = 0 ;
   bool              isoldact1  = false ;
   bool              isoldact2  = false ;
   bool              isoldmask  = false ;
   sigset_t          oldmask ;
   sigset_t          signalmask ;
   struct sigaction  sigact1 ;
   struct sigaction  oldact1 ;
   struct sigaction  sigact2 ;
   struct sigaction  oldact2 ;

   // TEST static init
   TEST(0 == sigconfig1) ;
   TEST(0 == sigconfig2) ;

   // TEST init, double free
   TEST(0 == new_signalconfig(&sigconfig1)) ;
   TEST(0 != sigconfig1) ;
   TEST(SIGRTMAX == sigconfig1->nr_signal_handlers) ;
   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &signalmask)) ;
   TEST(0 == memcmp(&signalmask, &sigconfig1->signalmask, sizeof(&signalmask))) ;
   TEST(0 == delete_signalconfig(&sigconfig1)) ;
   TEST(0 == sigconfig1) ;
   TEST(0 == delete_signalconfig(&sigconfig1)) ;
   TEST(0 == sigconfig1) ;

   // TEST compare equal
   TEST(0 == new_signalconfig(&sigconfig1)) ;
   TEST(0 == new_signalconfig(&sigconfig2)) ;
   TEST(0 == compare_signalconfig(sigconfig1, sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig1)) ;

   // TEST compare nr_signal_handlers
   TEST(0 == new_signalconfig(&sigconfig1)) ;
   TEST(0 == new_signalconfig(&sigconfig2)) ;
   ++ sigconfig2->nr_signal_handlers ;
   TEST(-1 == compare_signalconfig(sigconfig1, sigconfig2)) ;
   sigconfig1->nr_signal_handlers += 100 ;
   -- sigconfig2->nr_signal_handlers ;
   TEST(+1 == compare_signalconfig(sigconfig1, sigconfig2)) ;
   sigconfig1->nr_signal_handlers -= 100 ;
   TEST(0 == compare_signalconfig(sigconfig1, sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig1)) ;

   // TEST compare + change mask
   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &oldmask)) ;
   isoldmask = true ;
   sigemptyset(&signalmask) ;
   TEST(0 == sigaddset(&signalmask, SIGINT)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == pthread_sigmask(SIG_UNBLOCK, &signalmask, 0)) ;
   TEST(0 == new_signalconfig(&sigconfig1)) ;
   sigemptyset(&signalmask) ;
   TEST(0 == sigaddset(&signalmask, SIGINT)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == pthread_sigmask(SIG_BLOCK, &signalmask, 0)) ;
   TEST(0 == new_signalconfig(&sigconfig2)) ;
   TEST(0 != compare_signalconfig(sigconfig1, sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig2)) ;
   sigemptyset(&signalmask) ;
   TEST(0 == sigaddset(&signalmask, SIGINT)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == pthread_sigmask(SIG_UNBLOCK, &signalmask, 0)) ;
   TEST(0 == new_signalconfig(&sigconfig2)) ;
   TEST(0 == compare_signalconfig(sigconfig1, sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig1)) ;
   TEST(0 == pthread_sigmask(SIG_SETMASK, &oldmask, 0)) ;
   isoldmask = false ;

   // TEST compare + change handler setting
   TEST(0 == new_signalconfig(&sigconfig1)) ;
   sigact1.sa_sigaction = &dummy_sighandler ;
   sigact1.sa_flags     = SA_SIGINFO | SA_ONSTACK ;
   TEST(0 == sigemptyset(&sigact1.sa_mask)) ;
   TEST(0 == sigaction(SIGUSR1, &sigact1, &oldact1)) ;
   isoldact1 = true ;
   sigact2.sa_sigaction = &dummy_sighandler ;
   sigact2.sa_flags     = SA_SIGINFO | SA_ONSTACK ;
   TEST(0 == sigemptyset(&sigact2.sa_mask)) ;
   TEST(0 == sigaction(SIGSEGV, &sigact2, &oldact2)) ;
   isoldact2 = true ;
   TEST(0 == new_signalconfig(&sigconfig2)) ;
   TEST(&dummy_sighandler == sigconfig2->signal_handlers[SIGUSR1-1].sa_sigaction)
   TEST(&dummy_sighandler == sigconfig2->signal_handlers[SIGSEGV-1].sa_sigaction)
   TEST(0 != compare_signalconfig(sigconfig1, sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig2)) ;
   isoldact1 = false ;
   TEST(0 == sigaction(SIGUSR1, &oldact1, 0)) ;
   isoldact2 = false ;
   TEST(0 == sigaction(SIGSEGV, &oldact2, 0)) ;
   TEST(0 == new_signalconfig(&sigconfig2)) ;
   TEST(0 == compare_signalconfig(sigconfig1, sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig2)) ;
   TEST(0 == delete_signalconfig(&sigconfig1)) ;

   return 0 ;
ABBRUCH:
   if (isoldact1) sigaction(SIGUSR1, &oldact1, 0) ;
   if (isoldact2) sigaction(SIGSEGV, &oldact2, 0) ;
   if (isoldmask) (void) pthread_sigmask(SIG_SETMASK, &oldmask, 0) ;
   (void) delete_signalconfig(&sigconfig1) ;
   (void) delete_signalconfig(&sigconfig2) ;
   return EINVAL ;
}

static unsigned s_signr ;

static void test_callback(unsigned signr)
{
   s_signr = signr ;
}

static int test_helper(void)
{
   bool     isoldmask = false ;
   sigset_t oldmask ;
   sigset_t signalmask ;
   int      testsignals[] = { SIGQUIT, SIGUSR1, SIGUSR2 } ;

   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &oldmask)) ;
   isoldmask = true ;

   for(unsigned i = 0; i < nrelementsof(testsignals); ++i) {
      int              signr   = testsignals[i] ;
      signalcallback_t handler = s_signalhandler[signr-1] ;
      TEST(0 == sigemptyset(&signalmask)) ;
      TEST(0 == sigaddset(&signalmask, signr)) ;
      TEST(0 == pthread_sigmask(SIG_UNBLOCK, &signalmask, 0)) ;
      s_signalhandler[signr-1].isvalid = false ;
      TEST(0 == setcallback_signalconfig((unsigned)signr, &test_callback)) ;
      TEST(s_signalhandler[signr-1].isvalid) ;
      TEST(&test_callback == s_signalhandler[signr-1].callback) ;
      s_signr = 0 ;
      pthread_kill(pthread_self(), signr) ;
      TEST(0 == clearcallback_signalconfig((unsigned)signr)) ;
      TEST(! s_signalhandler[signr-1].isvalid) ;
      TEST(0 == s_signalhandler[signr-1].callback) ;
      TEST(signr == (int)s_signr) ;
      s_signalhandler[signr-1] = handler ;
   }

   return 0 ;
ABBRUCH:
   if (isoldmask) (void) pthread_sigmask(SIG_SETMASK, &oldmask, 0) ;
   return EINVAL ;
}

static int test_initprocess(void)
{
   sigset_t            old_signalmask ;
   signalcallback_t    signalhandler[nrelementsof(s_signalhandler)] ;

   memcpy(&old_signalmask, &s_old_signalmask, sizeof(old_signalmask)) ;
   memcpy(signalhandler, s_signalhandler, sizeof(s_signalhandler)) ;

   TEST(0 == initprocess_signalconfig()) ;
   TEST(0 == freeprocess_signalconfig()) ;

   memcpy(&s_old_signalmask, &old_signalmask, sizeof(old_signalmask)) ;
   memcpy(s_signalhandler, signalhandler, sizeof(s_signalhandler)) ;

   return 0 ;
ABBRUCH:
   memcpy(&s_old_signalmask, &old_signalmask, sizeof(old_signalmask)) ;
   memcpy(s_signalhandler, signalhandler, sizeof(s_signalhandler)) ;
   return EINVAL ;
}

static int thread_receivesignal(int rtsignr)
{
   int err ;
   assert(rtsignr) ;
   assert(self_osthread()->command) ;
   err = wait_rtsignal((rtsignal_t)rtsignr, 1) ;
   self_osthread()->command = 0 ;
   assert(0 == send_rtsignal(0)) ;
   return err ;
}

static int test_rtsignal(void)
{
   sigset_t          oldmask ;
   sigset_t          signalmask ;
   osthread_t      * thread    = 0 ;
   bool              isoldmask = false ;
   struct timespec   ts        = { 0, 0 } ;

   // TEST system supports at least 15 signals
   TEST(15 <= (SIGRTMAX - SIGRTMIN)) ;

   TEST(0 == sigprocmask(SIG_SETMASK, 0, &oldmask)) ;
   isoldmask = true ;
   TEST(0 == sigemptyset(&signalmask)) ;
   for(int i = 0; i < 16; ++i) {
      TEST(0 == sigaddset(&signalmask, SIGRTMIN + i)) ;
   }
   TEST(0 == sigprocmask(SIG_BLOCK, &signalmask, 0)) ;

   // TEST wait (consume all queued signals)
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   // generate signals in queue
   for(int i = 0; i < 16; ++i) {
      for(int nr = 0; nr < 1+i; ++nr) {
         TEST(0 == kill(getpid(), SIGRTMIN+i)) ;
      }
   }
   // consume signals
   for(int i = 0; i < 15; ++i) {
      TEST(0 == wait_rtsignal((rtsignal_t)i, (unsigned) (1+i))) ;
   }
   // all signals consumed
   for(int i = 0; i < 15; ++i) {
      TEST(EAGAIN == trywait_rtsignal((rtsignal_t)i)) ;
   }

   // TEST wait (consume not all signals)
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   // generate signals in queue
   for(int i = 0; i < 16; ++i) {
      for(int nr = 0; nr < 6; ++nr) {
         TEST(0 == kill(getpid(), SIGRTMIN+i)) ;
      }
   }
   // consume signals
   for(int i = 0; i < 15; ++i) {
      TEST(0 == wait_rtsignal((rtsignal_t)i, 5)) ;
   }
   // all signals consumed except for one
   for(int i = 0; i < 15; ++i) {
      TEST(0 == trywait_rtsignal((rtsignal_t)i)) ;
      TEST(EAGAIN == trywait_rtsignal((rtsignal_t)i)) ;
   }

   // TEST send_rtsignal (order unspecified)
   for(int i = 1; i < 15; ++i) {
      TEST(0 == newgroup_osthread(&thread, thread_receivesignal, i, 3)) ;
      osthread_t * group[3] = { thread, thread->groupnext, thread->groupnext->groupnext } ;
      for(int t = 0; t < 3; ++t) {
         TEST((void*)i == group[t]->command) ;
      }
      for(int t = 0; t < 3; ++t) {
         // wake up one thread
         TEST(0 == send_rtsignal((rtsignal_t)i)) ;
         // wait until woken up
         TEST(0 == wait_rtsignal(0, 1)) ;
         for(int t2 = 0; t2 < 3; ++t2) {
            if (group[t2] && 0 == group[t2]->command) {
               group[t2] = 0 ;
               break ;
            }
         }
         // only one woken up
         int count = t ;
         for(int t2 = 0; t2 < 3; ++t2) {
            if (group[t2]) {
               ++ count ;
               TEST((void*)i == group[t2]->command) ;
            }
         }
         TEST(2 == count) ;
      }
      TEST(0 == delete_osthread(&thread)) ;
   }

   // TEST EINVAL
   TEST(EINVAL == wait_rtsignal(16,1)) ;
   TEST(EINVAL == wait_rtsignal(255,1)) ;

   // TEST EAGAIN
   unsigned queue_size ;
   for(queue_size = 0; queue_size < 1000000; ++queue_size) {
      if (0 == send_rtsignal(0)) continue ;
      TEST(EAGAIN == send_rtsignal(0)) ;
      break ;
   }
   TEST(0 == wait_rtsignal(0, queue_size)) ;
   TEST(EAGAIN == trywait_rtsignal(0)) ;

   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   isoldmask = false ;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldmask, 0)) ;

   return 0 ;
ABBRUCH:
   (void) delete_osthread(&thread) ;
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   if (isoldmask) (void) sigprocmask(SIG_SETMASK, &oldmask, 0) ;
   return EINVAL ;
}


int unittest_os_sync_signal()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_helper())         goto ABBRUCH ;
   if (test_initprocess())    goto ABBRUCH ;
   if (test_rtsignal())       goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
