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

   file: C-kern/api/platform/sync/signal.h
    Header file of <PosixSignals>.

   file: C-kern/platform/Linux/signal.c
    Linux specific implementation <PosixSignals Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/sign.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/task/thread.h"
// TEXTDB:SELECT('#include "'header'"')FROM("C-kern/resource/config/signalconfig")WHERE(action=='set')
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
    * This value is set in <initonce_signalconfig>
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
 * All values in the array are set in <initonce_signalconfig>. */
static signalcallback_t    s_signalhandler[64] = { { .isvalid = false } } ;

/* variable: s_old_signalmask
 * Stores old signal mask.
 * This value is set in <initonce_signalconfig>
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
   if (0 < signr && signr <= (int)lengthof(s_signalhandler)) {
      if (s_signalhandler[signr-1].isvalid) {
         s_signalhandler[signr-1].callback((unsigned)signr) ;
      }
   }
}

// group: implementation

static int clearcallback_signalconfig(unsigned signr)
{
   int err ;

   VALIDATE_INPARAM_TEST(signr <= lengthof(s_signalhandler), ONABORT, PRINTINT_LOG(signr)) ;

   if (s_signalhandler[signr-1].isvalid) {
      s_signalhandler[signr-1].isvalid  = false ;
      s_signalhandler[signr-1].callback = 0 ;
      err = sigaction((int)signr, &s_signalhandler[signr-1].oldstate, 0) ;
      if (err) {
         TRACESYSERR_LOG("sigaction", err) ;
         PRINTINT_LOG(signr) ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static inline int setcallback_signalconfig(unsigned signr, signalcallback_f callback)
{
   int err ;
   struct sigaction  sighandler ;

   VALIDATE_INPARAM_TEST(signr > 0, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(signr <= lengthof(s_signalhandler), ONABORT, PRINTINT_LOG(signr)) ;

   err = clearcallback_signalconfig(signr) ;
   if (err) goto ONABORT ;

   sighandler.sa_flags     = SA_ONSTACK | SA_SIGINFO ;
   sighandler.sa_sigaction = &cbdispatcher_signalconfig ;
   err = sigemptyset(&sighandler.sa_mask) ;
   if (err) {
      err = EINVAL ;
      TRACESYSERR_LOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaction((int)signr, &sighandler, &s_signalhandler[signr-1].oldstate);
   if (err) {
      TRACESYSERR_LOG("sigaction", err) ;
      PRINTINT_LOG(signr) ;
      goto ONABORT ;
   }
   s_signalhandler[signr-1].callback = callback ;
   s_signalhandler[signr-1].isvalid  = true ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int setignore_signalconfig(unsigned signr)
{
   int err ;
   struct sigaction  sighandler ;

   VALIDATE_INPARAM_TEST(signr > 0, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(signr <= lengthof(s_signalhandler), ONABORT, PRINTINT_LOG(signr)) ;

   err = clearcallback_signalconfig(signr) ;
   if (err) goto ONABORT ;

   sighandler.sa_flags     = SA_ONSTACK ;
   sighandler.sa_handler   = SIG_IGN ;
   err = sigemptyset(&sighandler.sa_mask) ;
   if (err) {
      err = EINVAL ;
      TRACESYSERR_LOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaction((int)signr, &sighandler, &s_signalhandler[signr-1].oldstate) ;
   if (err) {
      TRACESYSERR_LOG("sigaction", err) ;
      PRINTINT_LOG(signr) ;
      goto ONABORT ;
   }
   s_signalhandler[signr-1].callback = 0 /*ignore*/ ;
   s_signalhandler[signr-1].isvalid  = true ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int initonce_signalconfig()
{
   int err ;
   sigset_t signalmask ;
   int      signr ;
   bool     isoldmask = false ;

   err = sigemptyset(&signalmask) ;
   if (err) goto ONABORT_emptyset ;

#define add(_SIGNR)  signr = (_SIGNR) ; if (sigaddset(&signalmask, signr)) goto ONABORT_add ;

// TEXTDB:SELECT( (if (description!="") ("   // " description \n) ) "   add("signal") ;")FROM("C-kern/resource/config/signalconfig")WHERE(action=='block')
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
   if (err) goto ONABORT_sigmask ;
   isoldmask = true ;

   err = sigemptyset(&signalmask) ;
   if (err) goto ONABORT_emptyset ;

// TEXTDB:SELECT("   // "description\n"   add("signal") ;")FROM("C-kern/resource/config/signalconfig")WHERE(action=='unblock'||action=='set')
// TEXTDB:END
   err = pthread_sigmask(SIG_UNBLOCK, &signalmask, 0) ;
   if (err) goto ONABORT_sigmask ;
#undef add

#define set(_SIGNR, _CALLBACK) \
   static_assert(0 < _SIGNR && _SIGNR <= lengthof(s_signalhandler),     \
   "s_signalhandler must be big enough" ) ;                             \
   err = setcallback_signalconfig(_SIGNR, _CALLBACK) ;                  \
   if (err) goto ONABORT ;

// TEXTDB:SELECT("   // "description\n"   set("signal", "callback") ;")FROM("C-kern/resource/config/signalconfig")WHERE(action=='set')
// TEXTDB:END
#undef set

#define ignore(_SIGNR) \
   static_assert(0 < _SIGNR && _SIGNR <= lengthof(s_signalhandler),     \
   "s_signalhandler must be big enough" ) ;                             \
   err = setignore_signalconfig(_SIGNR) ;                               \
   if (err) goto ONABORT ;

// TEXTDB:SELECT("   // "description\n"   ignore("signal") ;")FROM("C-kern/resource/config/signalconfig")WHERE(action=='ignore')
   // ensures that calls to write return EPIPE
   ignore(SIGPIPE) ;
// TEXTDB:END
#undef ignore

   return 0 ;
ONABORT_sigmask:
   TRACESYSERR_LOG("pthread_sigmask", err) ;
   goto ONABORT ;
ONABORT_emptyset:
   err = EINVAL ;
   TRACESYSERR_LOG("sigemptyset", err) ;
   goto ONABORT ;
ONABORT_add:
   err = EINVAL ;
   TRACESYSERR_LOG("sigaddset", err) ;
   PRINTINT_LOG(signr) ;
   goto ONABORT ;
ONABORT:
   if (isoldmask) freeonce_signalconfig() ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freeonce_signalconfig()
{
   int err ;
   int signr ;

   for (unsigned i = 0; i < lengthof(s_signalhandler); ++i) {
      if (s_signalhandler[i].isvalid) {
         s_signalhandler[i].isvalid = false ;
         signr = (int)i + 1 ;
         err = sigaction(signr, &s_signalhandler[i].oldstate, 0) ;
         if (err) {
            TRACESYSERR_LOG("sigaction", err) ;
            PRINTINT_LOG(signr) ;
            goto ONABORT ;
         }
      }
   }

   err = pthread_sigmask(SIG_SETMASK, &s_old_signalmask, 0) ;
   if (err) {
      TRACESYSERR_LOG("pthread_sigmask", err) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

static int nrhandlers_signalconfig(void)
{
   return SIGRTMAX ;
}

static size_t objectsize_signalconfig(void)
{
   const size_t objectsize = sizeof(signalconfig_t)
                           + sizeof(struct sigaction) * (unsigned) nrhandlers_signalconfig() ;
   return objectsize ;
}

int new_signalconfig(/*out*/signalconfig_t ** sigconfig)
{
   int err ;
   const int         nr_signal_handlers = nrhandlers_signalconfig() ;
   const size_t      objectsize         = objectsize_signalconfig() ;
   memblock_t        mem                = memblock_INIT_FREEABLE ;
   signalconfig_t *  newsigconfig       = 0 ;

   err = RESIZE_MM(objectsize, &mem) ;
   if (err) goto ONABORT ;

   newsigconfig = (signalconfig_t*) mem.addr ;
   memset(newsigconfig, 0, objectsize) ;
   newsigconfig->nr_signal_handlers = nr_signal_handlers ;

   err = pthread_sigmask(SIG_SETMASK, 0, &newsigconfig->signalmask) ;
   if (err) {
      TRACESYSERR_LOG("pthread_sigmask", err) ;
      goto ONABORT ;
   }

   for (int i = nr_signal_handlers; i > 0; --i) {
      if (32 <= i && i < SIGRTMIN) continue ; // not used in Linux
      err = sigaction(i, 0, &newsigconfig->signal_handlers[i-1]) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("sigaction(i,...)", err) ;
         PRINTINT_LOG(i) ;
         goto ONABORT ;
      }
   }

   *sigconfig = newsigconfig ;

   return 0 ;
ONABORT:
   (void) delete_signalconfig(&newsigconfig) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int delete_signalconfig(signalconfig_t ** sigconfig)
{
   int err ;
   signalconfig_t * delsigconfig = *sigconfig ;

   if (delsigconfig) {
      memblock_t mem = memblock_INIT(objectsize_signalconfig(), (uint8_t*)delsigconfig) ;
      *sigconfig = 0 ;

      err = FREE_MM(&mem) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int compare_signalconfig(const signalconfig_t * sigconfig1, const signalconfig_t * sigconfig2)
{
   if (sigconfig1 && sigconfig2) {
      int nr_diff = sigconfig1->nr_signal_handlers - sigconfig2->nr_signal_handlers ;
      nr_diff = sign_int(nr_diff) ;
      if (nr_diff) {
         return nr_diff ;
      }

      int cmp = memcmp(&sigconfig1->signalmask, &sigconfig2->signalmask, sizeof(sigconfig2->signalmask)) ;
      if (cmp) {
         return cmp ;
      }

      for (int i = sigconfig1->nr_signal_handlers; i > 0; ) {
         --i ;
         if (sigconfig1->signal_handlers[i].sa_flags != sigconfig2->signal_handlers[i].sa_flags) {
            return sign_int(sigconfig1->signal_handlers[i].sa_flags - sigconfig2->signal_handlers[i].sa_flags) ;
         }
         cmp = memcmp(&sigconfig1->signal_handlers[i].sa_sigaction, &sigconfig2->signal_handlers[i].sa_sigaction, sizeof(sigconfig2->signal_handlers[i].sa_sigaction)) ;
         if (cmp) {
            return cmp ;
         }
      }

      return 0 ;
   }

   return (sigconfig1) ? 1 : ((sigconfig2) ? -1 : 0) ;
}

// section: rtsignal_t

// TODO: send2_rtsignal
/*
int send2_rtsignal(rtsignal_t nr, thread_t * thread)
{

}
*/

int send_rtsignal(rtsignal_t nr)
{
   int err ;

   VALIDATE_INPARAM_TEST(nr <= maxnr_rtsignal(), ONABORT, PRINTINT_LOG(nr)) ;

   err = sigqueue(getpid(), SIGRTMIN+nr, (union sigval) { 0 } ) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("sigqueue", err) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int wait_rtsignal(rtsignal_t nr, uint32_t nr_signals)
{
   int err ;
   sigset_t signalmask ;

   VALIDATE_INPARAM_TEST(nr <= maxnr_rtsignal(), ONABORT, PRINTINT_LOG(nr)) ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = EINVAL ;
      TRACESYSERR_LOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaddset(&signalmask, SIGRTMIN+nr) ;
   if (err) {
      err = EINVAL ;
      TRACESYSERR_LOG("sigaddset", err) ;
      PRINTINT_LOG(SIGRTMIN+nr) ;
      goto ONABORT ;
   }

   for (uint32_t i = nr_signals; i; --i) {
      do {
         err = sigwaitinfo(&signalmask, 0) ;
      } while(-1 == err && EINTR == errno) ;

      if (-1 == err) {
         err = errno ;
         TRACESYSERR_LOG("sigwaitinfo", err) ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int trywait_rtsignal(rtsignal_t nr)
{
   int err ;
   sigset_t          signalmask ;
   struct timespec   ts = { 0, 0 } ;

   VALIDATE_INPARAM_TEST(nr <= maxnr_rtsignal(), ONABORT, PRINTINT_LOG(nr)) ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = EINVAL ;
      TRACESYSERR_LOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaddset(&signalmask, SIGRTMIN+nr) ;
   if (err) {
      err = EINVAL ;
      TRACESYSERR_LOG("sigaddset", err) ;
      PRINTINT_LOG(SIGRTMIN+nr) ;
      goto ONABORT ;
   }

   for (;;) {
      err = sigtimedwait(&signalmask, 0, &ts) ;
      if (-1 != err) break ;
      err = errno ;
      if (EAGAIN == err) {
         return err ;
      }
      if (EINTR != err) {
         TRACESYSERR_LOG("sigtimedwait", err) ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// section: test

#ifdef KONFIG_UNITTEST

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
ONABORT:
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

   for (unsigned i = 0; i < lengthof(testsignals); ++i) {
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
ONABORT:
   if (isoldmask) (void) pthread_sigmask(SIG_SETMASK, &oldmask, 0) ;
   return EINVAL ;
}

static int test_initonce(void)
{
   sigset_t            old_signalmask ;
   signalcallback_t    signalhandler[lengthof(s_signalhandler)] ;

   static_assert(sizeof(old_signalmask) == sizeof(s_old_signalmask), "must be of same type") ;
   static_assert(sizeof(signalhandler) == sizeof(s_signalhandler), "must be of same type") ;

   memcpy(&old_signalmask, &s_old_signalmask, sizeof(old_signalmask)) ;
   memcpy(signalhandler, s_signalhandler, sizeof(signalhandler)) ;
   memset( &s_signalhandler, 0, sizeof(s_signalhandler)) ;

   TEST(0 == initonce_signalconfig()) ;
   TEST(0 == freeonce_signalconfig()) ;

   memcpy(&s_old_signalmask, &old_signalmask, sizeof(s_old_signalmask)) ;
   memcpy(s_signalhandler, signalhandler, sizeof(s_signalhandler)) ;

   return 0 ;
ONABORT:
   memcpy(&s_old_signalmask, &old_signalmask, sizeof(old_signalmask)) ;
   memcpy(s_signalhandler, signalhandler, sizeof(s_signalhandler)) ;
   return EINVAL ;
}

static int thread_receivesignal(unsigned rtsignr)
{
   int err ;
   assert(rtsignr) ;
   assert(self_thread()->task_arg) ;
   err = wait_rtsignal((rtsignal_t)rtsignr, 1) ;
   self_thread()->task_arg = 0 ;
   assert(0 == send_rtsignal(0)) ;
   return err ;
}

static int test_rtsignal(void)
{
   sigset_t          oldmask ;
   sigset_t          signalmask ;
   thread_t          * thread  = 0 ;
   bool              isoldmask = false ;
   struct timespec   ts        = { 0, 0 } ;

   // TEST system supports at least 16 signals
   TEST(15 <= (SIGRTMAX - SIGRTMIN)) ;
   TEST(15 == maxnr_rtsignal()) ;

   TEST(0 == sigprocmask(SIG_SETMASK, 0, &oldmask)) ;
   isoldmask = true ;
   TEST(0 == sigemptyset(&signalmask)) ;
   for (unsigned i = 0; i <= maxnr_rtsignal(); ++i) {
      TEST(0 == sigaddset(&signalmask, SIGRTMIN + (int)i)) ;
   }
   TEST(0 == sigprocmask(SIG_BLOCK, &signalmask, 0)) ;

   // TEST wait (consume all queued signals)
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   // generate signals in queue
   for (int i = 0; i <= maxnr_rtsignal(); ++i) {
      for (int nr = 0; nr <= i; ++nr) {
         TEST(0 == kill(getpid(), SIGRTMIN+i)) ;
      }
   }
   // consume signals
   for (unsigned i = 0; i <= maxnr_rtsignal(); ++i) {
      TEST(0 == wait_rtsignal((rtsignal_t)i, (1+i))) ;
   }
   // all signals consumed
   for (unsigned i = 0; i <= maxnr_rtsignal(); ++i) {
      TEST(EAGAIN == trywait_rtsignal((rtsignal_t)i)) ;
   }

   // TEST wait (consume not all signals)
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   // generate signals in queue
   for (unsigned i = 0; i <= maxnr_rtsignal(); ++i) {
      for (int nr = 0; nr < 6; ++nr) {
         TEST(0 == kill(getpid(), SIGRTMIN+(int)i)) ;
      }
   }
   // consume signals
   for (unsigned i = 0; i <= maxnr_rtsignal(); ++i) {
      TEST(0 == wait_rtsignal((rtsignal_t)i, 5)) ;
   }
   // all signals consumed except for one
   for (unsigned i = 0; i <= maxnr_rtsignal(); ++i) {
      TEST(0 == trywait_rtsignal((rtsignal_t)i)) ;
      TEST(EAGAIN == trywait_rtsignal((rtsignal_t)i)) ;
   }

   // TEST send_rtsignal (order unspecified)
   for (unsigned i = 1; i <= maxnr_rtsignal(); ++i) {
      TEST(0 == newgeneric_thread(&thread, thread_receivesignal, i, 3)) ;
      thread_t * group[3] = { thread, thread->groupnext, thread->groupnext->groupnext } ;
      for (int t = 0; t < 3; ++t) {
         TEST((void*)i == group[t]->task_arg) ;
      }
      for (int t = 0; t < 3; ++t) {
         // wake up one thread
         TEST(0 == send_rtsignal((rtsignal_t)i)) ;
         // wait until woken up
         TEST(0 == wait_rtsignal(0, 1)) ;
         for (int t2 = 0; t2 < 3; ++t2) {
            if (group[t2] && 0 == group[t2]->task_arg) {
               group[t2] = 0 ;
               break ;
            }
         }
         // only one woken up
         int count = t ;
         for (int t2 = 0; t2 < 3; ++t2) {
            if (group[t2]) {
               ++ count ;
               TEST((void*)i == group[t2]->task_arg) ;
            }
         }
         TEST(2 == count) ;
      }
      TEST(0 == delete_thread(&thread)) ;
   }

   // TEST EINVAL
   TEST(EINVAL == send_rtsignal(16)) ;
   TEST(EINVAL == trywait_rtsignal(16)) ;
   TEST(EINVAL == wait_rtsignal(16,1)) ;
   TEST(EINVAL == wait_rtsignal(255,1)) ;

   // TEST EAGAIN
   unsigned queue_size ;
   for (queue_size = 0; queue_size < 1000000; ++queue_size) {
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
ONABORT:
   (void) delete_thread(&thread) ;
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   if (isoldmask) (void) sigprocmask(SIG_SETMASK, &oldmask, 0) ;
   return EINVAL ;
}


int unittest_platform_sync_signal()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   for (int i = 0; i < 2; ++i) {
      TEST(0 == init_resourceusage(&usage)) ;

      if (test_initfree())       goto ONABORT ;
      if (test_helper())         goto ONABORT ;
      if (test_initonce())       goto ONABORT ;
      if (test_rtsignal())       goto ONABORT ;

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
