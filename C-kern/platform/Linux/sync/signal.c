/* title: PosixSignals Linuximpl

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

   file: C-kern/platform/Linux/sync/signal.c
    Linux specific implementation <PosixSignals Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/math/int/sign.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/task/thread.h"
// TEXTDB:SELECT('#include "'header'"')FROM("C-kern/resource/config/signalhandler")WHERE(action=='set')
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

/* typedef: struct signalhandler_t
 * Exports <signalhandler_t> into global namespace. */
typedef struct signalhandler_t            signalhandler_t ;


/* struct: signalhandler_t
 * Describes an overwritten signal handler. */
struct signalhandler_t {
   /* variable: isvalid
    * Indicates if this structure contains valid information. */
   bool                 isvalid ;
   /* variable: handler
    * Function pointer to new signal handler. */
   signalhandler_f     handler ;
   /* variable: oldstate
    * Contains old signal handler configuration.
    * This value is set in <initonce_signalhandler>
    * before the signal handler is overwritten. */
   struct sigaction     oldstate ;
} ;

/* variable: s_signalhandler
 * Stores global configuration information for signal handlers.
 * See <signalhandler_t>.
 * All values in the array are set in <initonce_signalhandler>. */
static signalhandler_t     s_signalhandler[64] = { { .isvalid = false } } ;

/* variable: s_signalhandler_oldmask
 * Stores old signal mask.
 * This value is set in <initonce_signalhandler>
 * before the signal mask is changed. */
static sigset_t            s_signalhandler_oldmask ;

// group: helper

/* function: dispatcher_signalhandler
 * This signal handler is called for every signal.
 * It dispatches handling of the signal to the set signal handler. */
static void dispatcher_signalhandler(int signr, siginfo_t * siginfo, void * ucontext)
{
   (void) ucontext ;

   if (0 < signr && signr <= (int)lengthof(s_signalhandler)) {
      if (signr == SIGSEGV) {
         if (siginfo->si_code == SEGV_MAPERR) {
            ((signalhandler_segv_f)s_signalhandler[SIGSEGV-1].handler)(siginfo->si_addr, false) ;
         } else if (siginfo->si_code == SEGV_ACCERR) {
            ((signalhandler_segv_f)s_signalhandler[SIGSEGV-1].handler)(siginfo->si_addr, true) ;
         } else {
            // ignore user send SIGSEGV
         }

      } else {
         s_signalhandler[signr-1].handler((unsigned)signr, siginfo->si_code == SI_QUEUE ? (uintptr_t)siginfo->si_ptr : 0) ;
      }
   }
}

/* function: clear_signalhandler
 * Restores the default signal handler of the platform. */
static int clear_signalhandler(unsigned signr)
{
   int err ;

   VALIDATE_INPARAM_TEST(signr > 0, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(signr <= lengthof(s_signalhandler), ONABORT, PRINTINT_ERRLOG(signr)) ;

   if (s_signalhandler[signr-1].isvalid) {
      s_signalhandler[signr-1].isvalid = false ;
      s_signalhandler[signr-1].handler = 0 ;
      err = sigaction((int)signr, &s_signalhandler[signr-1].oldstate, 0) ;
      if (err) {
         TRACESYSCALL_ERRLOG("sigaction", err) ;
         PRINTINT_ERRLOG(signr) ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static inline signalhandler_f get_signalhandler(unsigned signr)
{
   return   (0 < signr && signr <= lengthof(s_signalhandler))
            ? s_signalhandler[signr-1].handler
            : 0 ;
}

/* function: set_signalhandler
 * Overwrites a previously set signal handler with sighandler. */
static int set_signalhandler(unsigned signr, signalhandler_f sighandler)
{
   int err ;
   struct sigaction  sigact ;

   VALIDATE_INPARAM_TEST(signr > 0, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(signr <= lengthof(s_signalhandler), ONABORT, PRINTINT_ERRLOG(signr)) ;

   err = clear_signalhandler(signr) ;
   if (err) goto ONABORT ;

   if (sighandler) {
      sigact.sa_flags     = SA_ONSTACK | SA_SIGINFO ;
      sigact.sa_sigaction = &dispatcher_signalhandler ;
      err = sigemptyset(&sigact.sa_mask) ;
      if (err) {
         err = EINVAL ;
         TRACESYSCALL_ERRLOG("sigemptyset", err) ;
         goto ONABORT ;
      }

      err = sigaction((int)signr, &sigact, &s_signalhandler[signr-1].oldstate);
      if (err) {
         TRACESYSCALL_ERRLOG("sigaction", err) ;
         PRINTINT_ERRLOG(signr) ;
         goto ONABORT ;
      }
      s_signalhandler[signr-1].handler = sighandler ;
      s_signalhandler[signr-1].isvalid = true ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

/* function: setignore_signalhandler
 * Sets signal handler which consumes and ignores signal signr. */
static int setignore_signalhandler(unsigned signr)
{
   int err ;
   struct sigaction  sighandler ;

   VALIDATE_INPARAM_TEST(signr > 0, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(signr <= lengthof(s_signalhandler), ONABORT, PRINTINT_ERRLOG(signr)) ;

   err = clear_signalhandler(signr) ;
   if (err) goto ONABORT ;

   sighandler.sa_flags     = SA_ONSTACK ;
   sighandler.sa_handler   = SIG_IGN ;
   err = sigemptyset(&sighandler.sa_mask) ;
   if (err) {
      err = EINVAL ;
      TRACESYSCALL_ERRLOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaction((int)signr, &sighandler, &s_signalhandler[signr-1].oldstate) ;
   if (err) {
      TRACESYSCALL_ERRLOG("sigaction", err) ;
      PRINTINT_ERRLOG(signr) ;
      goto ONABORT ;
   }
   s_signalhandler[signr-1].handler = 0 /*ignored*/ ;
   s_signalhandler[signr-1].isvalid = true ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: init

int initonce_signalhandler()
{
   int err ;
   sigset_t signalmask ;
   int      signr ;
   bool     isoldmask = false ;

   err = sigemptyset(&signalmask) ;
   if (err) goto ONABORT_emptyset ;

#define addrange(_MINSIGNR, _MAXSIGNR)  \
         for (signr = _MINSIGNR; signr <= _MAXSIGNR; ++signr) {   \
            if (sigaddset(&signalmask, signr)) goto ONABORT_add ; \
         }

// TEXTDB:SELECT( (if (description!="") ("   // " description \n) ) "   addrange("signal") ;")FROM("C-kern/resource/config/signalhandler")WHERE(action=='block2')
   // SIGRTMIN ... SIGRTMAX are blocked and used in send_signalrt
   addrange(SIGRTMIN,SIGRTMAX) ;
// TEXTDB:END
#undef  addall

#define add(_SIGNR)  \
         signr = (_SIGNR) ;   \
         if (sigaddset(&signalmask, signr)) goto ONABORT_add ;

// TEXTDB:SELECT( (if (description!="") ("   // " description \n) ) "   add("signal") ;")FROM("C-kern/resource/config/signalhandler")WHERE(action=='block')
   // used to suspend and resume a single thread
   add(SIGINT) ;
// TEXTDB:END

   err = pthread_sigmask(SIG_BLOCK, &signalmask, &s_signalhandler_oldmask) ;
   if (err) goto ONABORT_sigmask ;
   isoldmask = true ;

   err = sigemptyset(&signalmask) ;
   if (err) goto ONABORT_emptyset ;

// TEXTDB:SELECT("   // "description\n"   add("signal") ;")FROM("C-kern/resource/config/signalhandler")WHERE(action=='unblock'||action=='set')
// TEXTDB:END
   err = pthread_sigmask(SIG_UNBLOCK, &signalmask, 0) ;
   if (err) goto ONABORT_sigmask ;
#undef add

#define set(_SIGNR, _HANDLER) \
   static_assert(0 < _SIGNR && _SIGNR <= lengthof(s_signalhandler),  \
   "s_signalhandler must be big enough" ) ;                          \
   err = set_signalhandler(_SIGNR, _HANDLER) ;                       \
   if (err) goto ONABORT ;

// TEXTDB:SELECT("   // "description\n"   set("signal", "handler") ;")FROM("C-kern/resource/config/signalhandler")WHERE(action=='set')
// TEXTDB:END
#undef set

#define ignore(_SIGNR) \
   static_assert(0 < _SIGNR && _SIGNR <= lengthof(s_signalhandler),  \
   "s_signalhandler must be big enough" ) ;                          \
   err = setignore_signalhandler(_SIGNR) ;                           \
   if (err) goto ONABORT ;

// TEXTDB:SELECT("   // "description\n"   ignore("signal") ;")FROM("C-kern/resource/config/signalhandler")WHERE(action=='ignore')
   // ensures that calls to write return EPIPE
   ignore(SIGPIPE) ;
// TEXTDB:END
#undef ignore

   return 0 ;
ONABORT_sigmask:
   TRACESYSCALL_ERRLOG("pthread_sigmask", err) ;
   goto ONABORT ;
ONABORT_emptyset:
   err = EINVAL ;
   TRACESYSCALL_ERRLOG("sigemptyset", err) ;
   goto ONABORT ;
ONABORT_add:
   err = EINVAL ;
   TRACESYSCALL_ERRLOG("sigaddset", err) ;
   PRINTINT_ERRLOG(signr) ;
   goto ONABORT ;
ONABORT:
   if (isoldmask) freeonce_signalhandler() ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int freeonce_signalhandler()
{
   int err ;
   int signr ;

   for (unsigned i = 0; i < lengthof(s_signalhandler); ++i) {
      if (s_signalhandler[i].isvalid) {
         s_signalhandler[i].isvalid = false ;
         signr = (int)i + 1 ;
         err = sigaction(signr, &s_signalhandler[i].oldstate, 0) ;
         if (err) {
            TRACESYSCALL_ERRLOG("sigaction", err) ;
            PRINTINT_ERRLOG(signr) ;
            goto ONABORT ;
         }
      }
   }

   err = pthread_sigmask(SIG_SETMASK, &s_signalhandler_oldmask, 0) ;
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_sigmask", err) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

signalhandler_segv_f getsegv_signalhandler(void)
{
   return (signalhandler_segv_f) get_signalhandler(SIGSEGV) ;
}

// group: change

int setsegv_signalhandler(signalhandler_segv_f segfault_handler)
{
   int err ;

   err = set_signalhandler(SIGSEGV, (signalhandler_f) segfault_handler) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


/* struct: signalstate_t
 * System specific declaration. */
struct signalstate_t {
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

// group: helper

/* function: nrhandlers_signalstate
 * Returns the number of valid signal handlers.
 * Linux supports signals numbers from 1 to 31 and from SIGRTMIN to SIGRTMAX. */
static int nrhandlers_signalstate(void)
{
   return SIGRTMAX ; // signal nr 0 is not used
}

/* function: objectsize_signalstate
 * Returns size in bytes needed to store a single <signalstate_t>. */
static size_t objectsize_signalstate(void)
{
   const size_t objectsize = sizeof(signalstate_t)
                           + sizeof(struct sigaction) * (unsigned) nrhandlers_signalstate() ;
   return objectsize ;
}

// group: lifetime

int new_signalstate(/*out*/signalstate_t ** sigstate)
{
   int err ;
   const int         nr_signal_handlers = nrhandlers_signalstate() ;
   const size_t      objectsize         = objectsize_signalstate() ;
   memblock_t        mem                = memblock_FREE ;
   signalstate_t *   newsigstate        = 0 ;

   err = RESIZE_MM(objectsize, &mem) ;
   if (err) goto ONABORT ;

   newsigstate = (signalstate_t*) mem.addr ;
   memset(newsigstate, 0, objectsize) ;
   newsigstate->nr_signal_handlers = nr_signal_handlers ;

   err = pthread_sigmask(SIG_SETMASK, 0, &newsigstate->signalmask) ;
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_sigmask", err) ;
      goto ONABORT ;
   }

   for (int i = nr_signal_handlers; i > 0; --i) {
      if (32 <= i && i < SIGRTMIN) continue ; // not used in Linux
      err = sigaction(i, 0, &newsigstate->signal_handlers[i-1]) ;
      if (err) {
         err = errno ;
         TRACESYSCALL_ERRLOG("sigaction(i,...)", err) ;
         PRINTINT_ERRLOG(i) ;
         goto ONABORT ;
      }
   }

   *sigstate = newsigstate ;

   return 0 ;
ONABORT:
   (void) delete_signalstate(&newsigstate) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int delete_signalstate(signalstate_t ** sigstate)
{
   int err ;
   signalstate_t * delsigstate = *sigstate ;

   if (delsigstate) {
      memblock_t mem = memblock_INIT(objectsize_signalstate(), (uint8_t*)delsigstate) ;
      *sigstate = 0 ;

      err = FREE_MM(&mem) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

int compare_signalstate(const signalstate_t * sigstate1, const signalstate_t * sigstate2)
{
   if (sigstate1 && sigstate2) {
      int nr_diff = sigstate1->nr_signal_handlers - sigstate2->nr_signal_handlers ;
      nr_diff = sign_int(nr_diff) ;
      if (nr_diff) {
         return nr_diff ;
      }

      int cmp = memcmp(&sigstate1->signalmask, &sigstate2->signalmask, sizeof(sigstate2->signalmask)) ;
      if (cmp) {
         return cmp ;
      }

      for (int i = sigstate1->nr_signal_handlers; i > 0; ) {
         --i ;
         if (sigstate1->signal_handlers[i].sa_flags != sigstate2->signal_handlers[i].sa_flags) {
            return sign_int(sigstate1->signal_handlers[i].sa_flags - sigstate2->signal_handlers[i].sa_flags) ;
         }
         cmp = memcmp(&sigstate1->signal_handlers[i].sa_sigaction, &sigstate2->signal_handlers[i].sa_sigaction, sizeof(sigstate2->signal_handlers[i].sa_sigaction)) ;
         if (cmp) {
            return cmp ;
         }
      }

      return 0 ;
   }

   return (sigstate1) ? 1 : ((sigstate2) ? -1 : 0) ;
}


// section: signalwait_t

// group: lifetime

int initrealtime_signalwait(/*out*/signalwait_t * signalwait, signalrt_t minrt, signalrt_t maxrt)
{
   int err ;
   sigset_t signalmask ;

   VALIDATE_INPARAM_TEST(minrt <= maxrt, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(maxrt <= maxnr_signalrt(), ONABORT, ) ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = EINVAL ;
      TRACESYSCALL_ERRLOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   for (int signr = minrt; signr <= maxrt; ++signr) {
      if (sigaddset(&signalmask, SIGRTMIN+signr)) {
         err = EINVAL ;
         TRACESYSCALL_ERRLOG("sigaddset", err) ;
         goto ONABORT ;
      }
   }

   int fd = signalfd(-1, &signalmask, SFD_NONBLOCK|SFD_CLOEXEC) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSCALL_ERRLOG("signalfd", err) ;
      goto ONABORT ;
   }

   *signalwait = fd ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

/* function: free_signalwait
 * Frees all resources associated with signalwait.
 * After return the <iochannel_t> returned from <io_signalwait> is invalid. */
int free_signalwait(signalwait_t * signalwait)
{
   int err ;

   err = free_iochannel(signalwait) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}



// section: signalrt_t

// group: query

signalrt_t maxnr_signalrt(void)
{
   return (signalrt_t) (SIGRTMAX - SIGRTMIN) ;
}

// group: change

int send_signalrt(signalrt_t nr, uintptr_t value)
{
   int err ;

   VALIDATE_INPARAM_TEST(nr <= maxnr_signalrt(), ONABORT, PRINTINT_ERRLOG(nr)) ;

   err = sigqueue(getpid(), SIGRTMIN+nr, (union sigval) { .sival_ptr = (void*)(value) } ) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("sigqueue", err) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int send2_signalrt(signalrt_t nr, uintptr_t value, const thread_t * thread)
{
   int err ;

   VALIDATE_INPARAM_TEST(nr <= maxnr_signalrt(), ONABORT, PRINTINT_ERRLOG(nr)) ;

   err = pthread_sigqueue(thread->sys_thread, SIGRTMIN+nr, (union sigval) { .sival_ptr = (void*)(value) } ) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("sigqueue", err) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int wait_signalrt(signalrt_t nr, /*out*/uintptr_t * value)
{
   int err ;
   sigset_t signalmask ;

   VALIDATE_INPARAM_TEST(nr <= maxnr_signalrt(), ONABORT, PRINTINT_ERRLOG(nr)) ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = EINVAL ;
      TRACESYSCALL_ERRLOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaddset(&signalmask, SIGRTMIN+nr) ;
   if (err) {
      err = EINVAL ;
      TRACESYSCALL_ERRLOG("sigaddset", err) ;
      PRINTINT_ERRLOG(SIGRTMIN+nr) ;
      goto ONABORT ;
   }

   for (;;) {
      siginfo_t sinfo ;
      err = sigwaitinfo(&signalmask, &sinfo) ;
      if (-1 == err) {
         err = errno ;
         if (EINTR == err) continue ;
         TRACESYSCALL_ERRLOG("sigwaitinfo", err) ;
         goto ONABORT ;
      }
      if (value) {
         *value = sinfo.si_code == SI_QUEUE ? (uintptr_t) sinfo.si_ptr : 0 ;
      }
      break ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int trywait_signalrt(signalrt_t nr, /*out*/uintptr_t * value)
{
   int err ;
   sigset_t          signalmask ;
   struct timespec   ts = { 0, 0 } ;

   VALIDATE_INPARAM_TEST(nr <= maxnr_signalrt(), ONABORT, PRINTINT_ERRLOG(nr)) ;

   err = sigemptyset(&signalmask) ;
   if (err) {
      err = EINVAL ;
      TRACESYSCALL_ERRLOG("sigemptyset", err) ;
      goto ONABORT ;
   }

   err = sigaddset(&signalmask, SIGRTMIN+nr) ;
   if (err) {
      err = EINVAL ;
      TRACESYSCALL_ERRLOG("sigaddset", err) ;
      PRINTINT_ERRLOG(SIGRTMIN+nr) ;
      goto ONABORT ;
   }

   for (;;) {
      siginfo_t sinfo ;
      err = sigtimedwait(&signalmask, &sinfo, &ts) ;
      if (-1 == err) {
         err = errno ;
         if (EAGAIN == err)   return err ;
         if (EINTR  == err)   continue ;
         TRACESYSCALL_ERRLOG("sigtimedwait", err) ;
         goto ONABORT ;
      }
      if (value) {
         *value = sinfo.si_code == SI_QUEUE ? (uintptr_t) sinfo.si_ptr : 0 ;
      }
      break ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
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

static int test_signalstate(void)
{
   signalstate_t  *  sigstate1 = 0 ;
   signalstate_t  *  sigstate2 = 0 ;
   bool              isoldact  = false ;
   bool              isoldmask = false ;
   sigset_t          oldmask ;
   sigset_t          signalmask ;
   struct sigaction  sigact ;
   int               oldsignr ;
   struct sigaction  oldact ;

   // TEST new_signalstate, delete_signalstate
   TEST(0 == new_signalstate(&sigstate1)) ;
   TEST(0 != sigstate1) ;
   TEST(SIGRTMAX == sigstate1->nr_signal_handlers) ;
   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &signalmask)) ;
   TEST(0 == memcmp(&signalmask, &sigstate1->signalmask, sizeof(&signalmask))) ;
   TEST(0 == delete_signalstate(&sigstate1)) ;
   TEST(0 == sigstate1) ;
   TEST(0 == delete_signalstate(&sigstate1)) ;
   TEST(0 == sigstate1) ;

   // TEST compare_signalstate: equal
   TEST(0 == new_signalstate(&sigstate1)) ;
   TEST(0 == new_signalstate(&sigstate2)) ;
   TEST(0 == compare_signalstate(sigstate1, sigstate2)) ;
   TEST(0 == delete_signalstate(&sigstate2)) ;
   TEST(0 == delete_signalstate(&sigstate1)) ;

   // TEST compare_signalstate: compare nr_signal_handlers
   TEST(0 == new_signalstate(&sigstate1)) ;
   TEST(0 == new_signalstate(&sigstate2)) ;
   ++ sigstate2->nr_signal_handlers ;
   TEST(-1 == compare_signalstate(sigstate1, sigstate2)) ;
   -- sigstate2->nr_signal_handlers ;
   sigstate1->nr_signal_handlers += 100 ;
   TEST(+1 == compare_signalstate(sigstate1, sigstate2)) ;
   sigstate1->nr_signal_handlers -= 100 ;
   TEST(0 == compare_signalstate(sigstate1, sigstate2)) ;
   TEST(0 == delete_signalstate(&sigstate2)) ;
   TEST(0 == delete_signalstate(&sigstate1)) ;

   // TEST compare_signalstate: compare mask
   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &oldmask)) ;
   isoldmask = true ;
   sigemptyset(&signalmask) ;
   TEST(0 == sigaddset(&signalmask, SIGINT)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == pthread_sigmask(SIG_UNBLOCK, &signalmask, 0)) ;
   TEST(0 == new_signalstate(&sigstate1)) ;
   sigemptyset(&signalmask) ;
   TEST(0 == sigaddset(&signalmask, SIGINT)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == pthread_sigmask(SIG_BLOCK, &signalmask, 0)) ;
   TEST(0 == new_signalstate(&sigstate2)) ;
   TEST(0 != compare_signalstate(sigstate1, sigstate2)) ;
   TEST(0 == delete_signalstate(&sigstate2)) ;
   sigemptyset(&signalmask) ;
   TEST(0 == sigaddset(&signalmask, SIGINT)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == pthread_sigmask(SIG_UNBLOCK, &signalmask, 0)) ;
   TEST(0 == new_signalstate(&sigstate2)) ;
   TEST(0 == compare_signalstate(sigstate1, sigstate2)) ;
   TEST(0 == delete_signalstate(&sigstate2)) ;
   TEST(0 == delete_signalstate(&sigstate1)) ;
   TEST(0 == pthread_sigmask(SIG_SETMASK, &oldmask, 0)) ;
   isoldmask = false ;

   // TEST compare_signalstate: change handler setting
   int testsignals[] = { SIGSEGV, SIGUSR1, SIGRTMIN, SIGRTMAX } ;
   for (unsigned i = 0; i < lengthof(testsignals); ++i) {
      int signr = testsignals[i] ;
      TEST(0 == new_signalstate(&sigstate1)) ;
      sigact.sa_sigaction = &dummy_sighandler ;
      sigact.sa_flags     = SA_SIGINFO | SA_ONSTACK ;
      TEST(0 == sigemptyset(&sigact.sa_mask)) ;
      TEST(0 == sigaction(signr, &sigact, &oldact)) ;
      isoldact = true ;
      oldsignr = signr ;
      TEST(0 == new_signalstate(&sigstate2)) ;
      TEST(&dummy_sighandler == sigstate2->signal_handlers[signr-1].sa_sigaction)
      TEST(0 != compare_signalstate(sigstate1, sigstate2)) ;
      TEST(0 == delete_signalstate(&sigstate2)) ;
      isoldact = false ;
      TEST(0 == sigaction(signr, &oldact, 0)) ;
      TEST(0 == new_signalstate(&sigstate2)) ;
      TEST(0 == compare_signalstate(sigstate1, sigstate2)) ;
      TEST(0 == delete_signalstate(&sigstate2)) ;
      TEST(0 == delete_signalstate(&sigstate1)) ;
   }

   return 0 ;
ONABORT:
   if (isoldact) sigaction(oldsignr, &oldact, 0) ;
   if (isoldmask) (void) pthread_sigmask(SIG_SETMASK, &oldmask, 0) ;
   (void) delete_signalstate(&sigstate1) ;
   (void) delete_signalstate(&sigstate2) ;
   return EINVAL ;
}

static volatile unsigned   s_signr ;
static volatile uintptr_t  s_sigvalue ;

static void test_handler(unsigned signr, uintptr_t sigvalue)
{
   s_signr    = signr ;
   s_sigvalue = sigvalue ;
}

static int test_signalhandler_helper(void)
{
   int         testsignals[] = { SIGQUIT, SIGUSR1, SIGUSR2, SIGRTMIN, SIGRTMAX } ;
   sigset_t    oldmask ;
   bool        isoldmask     = false ;

   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &oldmask)) ;
   isoldmask = true ;

   for (unsigned i = 0; i < lengthof(testsignals); ++i) {
      int               signr   = testsignals[i] ;
      signalhandler_t   handler = s_signalhandler[signr-1] ;
      sigset_t          signalmask ;
      struct sigaction  oldstate ;
      struct sigaction  oldstate2 ;

      // prepare
      TEST(0 == sigemptyset(&signalmask)) ;
      TEST(0 == sigaddset(&signalmask, signr)) ;
      TEST(0 == pthread_sigmask(SIG_UNBLOCK, &signalmask, 0)) ;
      TEST(0 == sigaction(signr, 0, &oldstate)) ;

      // TEST set_signalhandler: test_handler is called
      s_signalhandler[signr-1].isvalid = false ;
      TEST(0 == set_signalhandler((unsigned)signr, &test_handler)) ;
      TEST(1 == s_signalhandler[signr-1].isvalid) ;
      TEST(&test_handler == s_signalhandler[signr-1].handler) ;
      TEST(oldstate.sa_handler == s_signalhandler[signr-1].oldstate.sa_handler) ;
      TEST(oldstate.sa_flags   == s_signalhandler[signr-1].oldstate.sa_flags) ;
      s_signr    = 0 ;
      s_sigvalue = 1 ;
      pthread_kill(pthread_self(), signr) ;
      TEST(s_signr    == (unsigned)signr) ;
      TEST(s_sigvalue == 0) ;
      s_signr    = 0 ;
      pthread_sigqueue(pthread_self(), signr, (union sigval) { .sival_ptr = (void*)(100+i) } ) ;
      TEST(s_signr    == (unsigned)signr) ;
      TEST(s_sigvalue == 100+i) ;

      // TEST get_signalhandler
      TEST(&test_handler == get_signalhandler((unsigned)signr)) ;

      // TEST set_signalhandler: restore previous if handler == 0
      TEST(0 == set_signalhandler((unsigned)signr, 0)) ;
      TEST(0 == s_signalhandler[signr-1].isvalid) ;
      TEST(0 == s_signalhandler[signr-1].handler) ;
      s_signalhandler[signr-1] = handler ;
      TEST(0 == sigaction(signr, 0, &oldstate2)) ;
      TEST(oldstate.sa_handler == oldstate2.sa_handler) ;
      TEST(oldstate.sa_flags   == oldstate2.sa_flags) ;

      // TEST get_signalhandler: 0 value
      TEST(0 == get_signalhandler((unsigned)signr)) ;

      // TEST setignore_signalhandler
      s_signalhandler[signr-1].isvalid = false ;
      TEST(0 == setignore_signalhandler((unsigned)signr)) ;
      TEST(1 == s_signalhandler[signr-1].isvalid) ;
      TEST(0 == s_signalhandler[signr-1].handler) ;
      s_signr = 0 ;
      pthread_kill(pthread_self(), signr) ;
      TEST(0 == s_signr) ;

      // TEST clear_signalhandler: restore previous
      TEST(0 == clear_signalhandler((unsigned)signr)) ;
      TEST(0 == s_signalhandler[signr-1].isvalid) ;
      TEST(0 == s_signalhandler[signr-1].handler) ;
      s_signalhandler[signr-1] = handler ;
      TEST(0 == sigaction(signr, 0, &oldstate2)) ;
      TEST(oldstate.sa_handler == oldstate2.sa_handler) ;
      TEST(oldstate.sa_flags   == oldstate2.sa_flags) ;

      // TEST get_signalhandler: 0 value
      TEST(0 == get_signalhandler((unsigned)signr)) ;

      // unprepare
      TEST(0 == pthread_sigmask(SIG_SETMASK, &oldmask, 0)) ;
   }

   // TEST clear_signalhandler: EINVAL
   TEST(EINVAL == clear_signalhandler(0)) ;
   TEST(EINVAL == clear_signalhandler(lengthof(s_signalhandler)+1)) ;

   // TEST get_signalhandler: (invalid signr)
   TEST(0 == get_signalhandler(0)) ;
   TEST(0 == get_signalhandler(lengthof(s_signalhandler)+1)) ;

   // TEST set_signalhandler: EINVAL
   TEST(EINVAL == set_signalhandler(0, &test_handler)) ;
   TEST(EINVAL == set_signalhandler(lengthof(s_signalhandler)+1, &test_handler)) ;

   // TEST setignore_signalhandler: EINVAL
   TEST(EINVAL == setignore_signalhandler(0)) ;
   TEST(EINVAL == setignore_signalhandler(lengthof(s_signalhandler)+1)) ;

   return 0 ;
ONABORT:
   if (isoldmask) (void) pthread_sigmask(SIG_SETMASK, &oldmask, 0) ;
   return EINVAL ;
}

static int test_signalhandler_initonce(void)
{
   sigset_t          old_signalmask ;
   signalhandler_t   signalhandler[lengthof(s_signalhandler)] ;

   static_assert(sizeof(old_signalmask) == sizeof(s_signalhandler_oldmask), "must be of same type") ;
   static_assert(sizeof(signalhandler) == sizeof(s_signalhandler), "must be of same type") ;

   memcpy(&old_signalmask, &s_signalhandler_oldmask, sizeof(old_signalmask)) ;
   memcpy(signalhandler, s_signalhandler, sizeof(signalhandler)) ;
   memset(&s_signalhandler, 0, sizeof(s_signalhandler)) ;

   TEST(0 == initonce_signalhandler()) ;
   TEST(0 == freeonce_signalhandler()) ;

   memcpy(&s_signalhandler_oldmask, &old_signalmask, sizeof(s_signalhandler_oldmask)) ;
   memcpy(s_signalhandler, signalhandler, sizeof(s_signalhandler)) ;

   return 0 ;
ONABORT:
   memcpy(&s_signalhandler_oldmask, &old_signalmask, sizeof(old_signalmask)) ;
   memcpy(s_signalhandler, signalhandler, sizeof(s_signalhandler)) ;
   return EINVAL ;
}

static int thread_receivesignal(uintptr_t rtsignr)
{
   int err ;
   uintptr_t value ;
   assert(rtsignr) ;
   assert(mainarg_thread(self_thread())) ;
   err = wait_signalrt((signalrt_t)rtsignr, &value) ;
   settask_thread(self_thread(), 0, 0) ;
   assert(0 == send_signalrt(0, value)) ;
   return err ;
}

static volatile void *  s_memaddr ;
static volatile bool    s_ismapped ;

static void segfault_handler(void * memaddr, bool ismapped)
{
   s_memaddr  = memaddr ;
   s_ismapped = ismapped ;
   abort_thread() ;
}

static int thread_segfault_queue(void * dummy)
{
   (void) dummy ;
   pthread_sigqueue(self_thread()->sys_thread, SIGSEGV, (union sigval) { .sival_ptr = 0 }) ;
   return 0 ;
}

static int thread_segfault(void * memaddr)
{
   *(uint8_t*)memaddr = 0 ;
   return 0 ;
}

static int test_signalhandler(void)
{
   signalhandler_segv_f oldhandler = getsegv_signalhandler() ;
   thread_t *           thread     = 0 ;
   void *               memaddr    = MAP_FAILED ;

   // TEST getsegv_signalhandler
   TEST(0 == clear_signalhandler(SIGSEGV)) ;
   TEST(0 == getsegv_signalhandler()) ;
   for (uintptr_t i = 0; i < 10; ++i) {
      s_signalhandler[SIGSEGV-1].handler = (signalhandler_f)i ;
      TEST(i == (uintptr_t)getsegv_signalhandler()) ;
   }
   s_signalhandler[SIGSEGV-1].handler = 0 ;

   // TEST setsegv_signalhandler
   TEST(0 == setsegv_signalhandler(&segfault_handler)) ;
   TEST(1 == s_signalhandler[SIGSEGV-1].isvalid) ;
   TEST(&segfault_handler == (signalhandler_segv_f) s_signalhandler[SIGSEGV-1].handler) ;
   TEST(0 == setsegv_signalhandler(0)) ;
   TEST(0 == s_signalhandler[SIGSEGV-1].isvalid) ;
   TEST(0 == (signalhandler_segv_f) s_signalhandler[SIGSEGV-1].handler) ;

   // TEST setsegv_signalhandler: user send SIGSEGV are ignored
   TEST(0 == setsegv_signalhandler(segfault_handler)) ;
   TEST(0 == new_thread(&thread, &thread_segfault_queue, (void*)0)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST setsegv_signalhandler: write access to read-only
   s_memaddr  = 0 ;
   s_ismapped = false ;
   memaddr = mmap(0, 1, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) ;
   TEST(memaddr != MAP_FAILED) ;
   TEST(0 == setsegv_signalhandler(segfault_handler)) ;
   TEST(0 == new_thread(&thread, &thread_segfault, memaddr)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(ENOTRECOVERABLE == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(s_memaddr  == memaddr) ;
   TEST(s_ismapped == true) ;

   // TEST setsegv_signalhandler: write access to unmapped memory
   s_memaddr  = 0 ;
   s_ismapped = true ;
   TEST(0 == munmap(memaddr, 1)) ;
   TEST(0 == setsegv_signalhandler(segfault_handler)) ;
   TEST(0 == new_thread(&thread, &thread_segfault, memaddr)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(ENOTRECOVERABLE == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(s_memaddr  == memaddr) ;
   TEST(s_ismapped == false) ;
   memaddr = MAP_FAILED ;

   // unprepare
   TEST(0 == setsegv_signalhandler(oldhandler)) ;

  return 0 ;
ONABORT:
   setsegv_signalhandler(oldhandler) ;
   (void) delete_thread(&thread) ;
   if (memaddr != MAP_FAILED) munmap(memaddr, 1) ;
   return EINVAL ;
}

static int test_signalrt(void)
{
   sigset_t          signalmask ;
   thread_t *        group[3]  = { 0, 0, 0 } ;
   struct timespec   ts        = { 0, 0 } ;

   // prepare
   TEST(0 == sigemptyset(&signalmask)) ;
   for (int i = SIGRTMIN; i <= SIGRTMAX; ++i) {
      TEST(0 == sigaddset(&signalmask, i)) ;
   }

   // TEST maxnr_signalrt: system supports at least 8 signals
   TEST(maxnr_signalrt() == SIGRTMAX - SIGRTMIN) ;
   TEST(maxnr_signalrt() >= 8) ;

   // TEST trywait_signalrt
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ;
   for (int i = 0; i <= maxnr_signalrt(); ++i) {
      uintptr_t value = 1 ;
      uintptr_t V     = (unsigned)i + 100u ;
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0)) ;
      TEST(0 == kill(getpid(), SIGRTMIN+i)) ;
      TEST(0 == trywait_signalrt((signalrt_t)i, &value)) ;
      TEST(0 == value) ;
      TEST(0 == sigqueue(getpid(), SIGRTMIN+i, (union sigval) { .sival_ptr = (void*)V } )) ;
      TEST(0 == trywait_signalrt((signalrt_t)i, &value)) ;
      TEST(V == value) ;
      ++ V ;
      TEST(0 == pthread_sigqueue(self_thread()->sys_thread, SIGRTMIN+i, (union sigval) { .sival_ptr = (void*)V } )) ;
      TEST(0 == trywait_signalrt((signalrt_t)i, &value)) ;
      TEST(V == value) ;
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0)) ;
   }

   // TEST trywait_signalrt: EINVAL
   TEST(EINVAL == trywait_signalrt((signalrt_t)(maxnr_signalrt()+1), 0)) ;

   // TEST wait_signalrt
   for (int i = 0; i <= maxnr_signalrt(); ++i) {
      uintptr_t value = 1 ;
      uintptr_t V     = (unsigned)i + 100u ;
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0)) ;
      TEST(0 == kill(getpid(), SIGRTMIN+i)) ;
      TEST(0 == wait_signalrt((signalrt_t)i, &value)) ;
      TEST(0 == value) ;
      TEST(0 == sigqueue(getpid(), SIGRTMIN+i, (union sigval) { .sival_ptr = (void*)V } )) ;
      TEST(0 == wait_signalrt((signalrt_t)i, &value)) ;
      TEST(V == value) ;
      ++ V ;
      TEST(0 == pthread_sigqueue(self_thread()->sys_thread, SIGRTMIN+i, (union sigval) { .sival_ptr = (void*)V } )) ;
      TEST(0 == wait_signalrt((signalrt_t)i, &value)) ;
      TEST(V == value) ;
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0)) ;
   }

   // TEST wait_signalrt: EINVAL
   TEST(EINVAL == wait_signalrt((signalrt_t)(maxnr_signalrt()+1), 0)) ;

   // TEST send_signalrt: signals are queued
   for (uintptr_t i = 1; i <= maxnr_signalrt(); ++i) {
      for (uintptr_t nr = 10; nr < 20; ++nr) {
         TEST(0 == send_signalrt((signalrt_t)i, nr)) ;
      }
      for (uintptr_t nr = 10; nr < 20; nr += 2) {
         uintptr_t value = 0 ;
         TEST(0 == trywait_signalrt((signalrt_t)i, &value)) ;
         TEST(nr == value) ;
         TEST(0 == wait_signalrt((signalrt_t)i, &value)) ;
         TEST(nr+1 == value) ;
      }
   }

   // TEST send_signalrt: one thread receives / order unspecified
   for (uintptr_t i = 1; i <= maxnr_signalrt(); ++i) {
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0)) ;
      for (unsigned t = 0; t < lengthof(group); ++t) {
         TEST(0 == newgeneric_thread(&group[t], thread_receivesignal, i)) ;
      }
      for (unsigned t = 0; t < lengthof(group); ++t) {
         TEST(i == (uintptr_t) mainarg_thread(group[t])) ;
      }
      for (unsigned t = 1; t <= lengthof(group); ++t) {
         // wake up one thread
         TEST(0 == send_signalrt((signalrt_t)i, t*i)) ;
         // wait until woken up
         uintptr_t v ;
         TEST(0 == wait_signalrt(0, &v)) ;
         TEST(v == t*i) ;
         for (unsigned t2 = 0; t2 < lengthof(group); ++t2) {
            if (group[t2] && 0 == mainarg_thread(group[t2])) {
               TEST(0 == delete_thread(&group[t2])) ;
               break ;
            }
         }
         // only one woken up
         unsigned count = t ;
         for (unsigned t2 = 0; t2 < lengthof(group); ++t2) {
            count += (group[t2] != 0) ;
         }
         TEST(count == lengthof(group)) ;
      }
   }

   // TEST send_signalrt: EINVAL
   TEST(EINVAL == send_signalrt((signalrt_t)(maxnr_signalrt()+1), 0)) ;

   // TEST send_signalrt: EAGAIN
   unsigned queue_size ;
   for (queue_size = 0; queue_size < 1000000; ++queue_size) {
      if (0 == send_signalrt(0, queue_size)) continue ;
      TEST(EAGAIN == send_signalrt(0, 0)) ;
      TEST(EAGAIN == send2_signalrt(0, 0, self_thread())) ;
      break ;
   }
   TEST(queue_size > 16) ;
   for (unsigned i = 0; i < queue_size; ++i) {
      uintptr_t value ;
      TEST(0 == wait_signalrt(0, &value)) ;
      TEST(i == value) ;
   }
   TEST(EAGAIN == trywait_signalrt(0, 0)) ;

   // TEST send2_signalrt: signals are queued
   for (uintptr_t i = 1; i <= maxnr_signalrt(); ++i) {
      for (uintptr_t nr = 20; nr < 30; ++nr) {
         TEST(0 == send2_signalrt((signalrt_t)i, nr, self_thread())) ;
      }
      for (uintptr_t nr = 20; nr < 30; nr += 2) {
         uintptr_t value = 0 ;
         TEST(0 == trywait_signalrt((signalrt_t)i, &value)) ;
         TEST(nr == value) ;
         TEST(0 == wait_signalrt((signalrt_t)i, &value)) ;
         TEST(nr+1 == value) ;
      }
   }

   // TEST send2_signalrt: only specific thread receives
   for (uintptr_t i = 1; i <= maxnr_signalrt(); ++i) {
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0)) ;
      for (unsigned t = 0; t < lengthof(group); ++t) {
         TEST(0 == newgeneric_thread(&group[t], thread_receivesignal, i)) ;
      }
      for (unsigned t = 0; t < lengthof(group); ++t) {
         TEST(i == (uintptr_t) mainarg_thread(group[t])) ;
      }
      for (unsigned t = 1; t <= lengthof(group); ++t) {
         // wake up one thread
         TEST(0 == send2_signalrt((signalrt_t)i, t*i, group[t-1])) ;
         // wait until woken up
         uintptr_t v ;
         TEST(0 == wait_signalrt(0, &v)) ;
         TEST(v == t*i) ;
         TEST(0 == mainarg_thread(group[t-1])) ;
         TEST(0 == delete_thread(&group[t-1])) ;
         // only one woken up
         for (unsigned t2 = t; t2 < lengthof(group); ++t2) {
            TEST(i == (uintptr_t) mainarg_thread(group[t2])) ;
         }
      }
   }

   // TEST send2_signalrt: EINVAL
   TEST(EINVAL == send2_signalrt((signalrt_t)(maxnr_signalrt()+1), 0, self_thread())) ;

   // TEST send2_signalrt: EAGAIN
   for (queue_size = 0; queue_size < 1000000; ++queue_size) {
      if (0 == send2_signalrt(0, queue_size, self_thread())) continue ;
      TEST(EAGAIN == send2_signalrt(0, 0, self_thread())) ;
      TEST(EAGAIN == send_signalrt(0, 0)) ;
      break ;
   }
   TEST(queue_size > 16) ;
   for (unsigned i = 0; i < queue_size; ++i) {
      uintptr_t value ;
      TEST(0 == trywait_signalrt(0, &value)) ;
      TEST(i == value) ;
   }
   TEST(EAGAIN == trywait_signalrt(0, 0)) ;

   // unprepare
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ;

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < lengthof(group);++i) {
      (void) delete_thread(&group[i]) ;
   }
   while (0 < sigtimedwait(&signalmask, 0, &ts)) ;
   return EINVAL ;
}

static int test_signalwait(void)
{
   signalwait_t signalwait = signalwait_FREE ;

   // TEST signalwait_FREE
   TEST(1 == isfree_iochannel(signalwait)) ;

   // TEST initrealtime_signalwait
   TEST(0 == initrealtime_signalwait(&signalwait, 0, maxnr_signalrt())) ;
   TEST(0 == isfree_iochannel(signalwait)) ;

   // TEST free_signalwait
   TEST(0 == free_signalwait(&signalwait)) ;
   TEST(1 == isfree_iochannel(signalwait)) ;
   TEST(0 == free_signalwait(&signalwait)) ;
   TEST(1 == isfree_iochannel(signalwait)) ;

   // TEST initrealtime_signalwait: EINVAL
   TEST(EINVAL == initrealtime_signalwait(&signalwait, 1, 0)) ;
   TEST(1 == isfree_iochannel(signalwait)) ;
   TEST(EINVAL == initrealtime_signalwait(&signalwait, 0, (signalrt_t)(maxnr_signalrt()+1))) ;
   TEST(1 == isfree_iochannel(signalwait)) ;

   // TEST free_signalwait: EBADF
   int fd = dup(STDIN_FILENO) ;
   TEST(fd > 0) ;
   TEST(0 == close(fd)) ;
   signalwait = fd ;
   TEST(0 == isfree_iochannel(signalwait)) ;
   TEST(EBADF == free_signalwait(&signalwait)) ;
   TEST(1 == isfree_iochannel(signalwait)) ;

   // TEST io_signalwait
   for (unsigned minrt = 0; minrt <= maxnr_signalrt(); ++minrt) {
      for (unsigned maxrt = minrt; maxrt <= maxnr_signalrt(); ++ maxrt) {
         TEST(0 == initrealtime_signalwait(&signalwait, (signalrt_t)minrt, maxnr_signalrt())) ;

         // TEST io_signalwait
         iochannel_t ioc = io_signalwait(signalwait) ;
         TEST(ioc == signalwait) ;
         TEST(1 == isvalid_iochannel(ioc)) ;

         // TEST io_signalwait: returns always same value
         TEST(ioc == io_signalwait(signalwait)) ;

            // TEST io_signalwait: file descriptor generates read event
         for (unsigned signr = minrt; signr <= maxrt; ++signr) {
            for (unsigned nrqueued = 1; nrqueued <= 2; ++nrqueued) {
               struct pollfd pfd = { .fd = ioc, .events = POLLIN } ;
               TEST(EAGAIN == trywait_signalrt((signalrt_t)minrt, 0)) ;
               TEST(0 == poll(&pfd, 1, 0)) ;    // not readable
               for (unsigned i = 0; i < nrqueued; ++i) {
                  TEST(0 == send_signalrt((signalrt_t)signr, 1u+signr+i)) ;
               }
               for (unsigned i = 0; i < nrqueued; ++i) {
                  TEST(1 == poll(&pfd, 1, 0)) ;    // readable
                  uintptr_t v = 0 ;
                  TEST(0 == trywait_signalrt((signalrt_t)signr, &v)) ;
                  TEST(v == 1u+signr+i) ;
               }
               TEST(EAGAIN == trywait_signalrt((signalrt_t)signr, 0)) ;
            }
         }

         if (maxrt < maxnr_signalrt()) {
            maxrt += 5u ;
            if (maxrt >= maxnr_signalrt()) maxrt = maxnr_signalrt()-1u ;
         }
         TEST(0 == free_signalwait(&signalwait)) ;
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_platform_sync_signal()
{
   if (test_signalstate())             goto ONABORT ;
   if (test_signalhandler_helper())    goto ONABORT ;
   if (test_signalhandler_initonce())  goto ONABORT ;
   if (test_signalhandler())           goto ONABORT ;
   if (test_signalrt())                goto ONABORT ;
   if (test_signalwait())              goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
