/* title: PosixSignals Linuximpl

   Implements <PosixSignals> with help of POSIX interface.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/ioevent.h"
#include "C-kern/api/io/iopoll.h"
#include "C-kern/api/io/pipe.h"
#endif


/* typedef: sys_signalhandler_f
 * Funktionssignatur eines OS-speifischen Signalhandlers. */
typedef void (*sys_signalhandler_f) (int signr, siginfo_t* siginfo, void* ucontext);


// struct signals_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_signals_errtimer
 * Simulates an error in <signals_t.init_signals>. */
static test_errortimer_t   s_signals_errtimer = test_errortimer_FREE;
#endif

// group: default-signal-handler

/* function: dummy_signalhandler
 * Do nothing callback. Only used to return from blocking system call. */
static void dummy_signalhandler(int signr, siginfo_t* siginfo, void* ucontext)
{  (void) signr, (void) siginfo, (void)ucontext;
}

/* function: call_signalhandler
 * This signal handler is called from the OS and calls the user defined signal handler. */
static void call_signalhandler(int signr, siginfo_t* siginfo, void* ucontext)
{
   (void) ucontext;

   if (SIGSEGV ==signr) {
      signalhandler_segv_f segv_handler = self_maincontext()->signals->segv;
      if (  siginfo->si_code == SEGV_MAPERR
            || siginfo->si_code == SEGV_ACCERR) {
         if (!segv_handler) {
            signal(signr, SIG_DFL); // install default handler - rexecute instr which caused SEGV
         } else if (siginfo->si_code == SEGV_MAPERR) {
            segv_handler(siginfo->si_addr, false);
         } else {
            segv_handler(siginfo->si_addr, true);
         }
      } else {
         // ignore user send or other kernel generated SIGSEGV (user => si_code<=0)
      }
   } else {
      // signalhandler_f((unsigned)signr, siginfo->si_code == SI_QUEUE ? (uintptr_t)siginfo->si_ptr : 0);
   }
}

// group: helper

/* function: maxnr_signal
 * Returns the maximum number of valid signal handlers. */
static unsigned maxnr_signal(void)
{
   return (unsigned)SIGRTMAX; // signal nr 0 is not used
}

/* function: isinvalid_signal
 * This range of signal numbers is available for user programs.
 * Linux supports signals numbers from 1 to 31 and from SIGRTMIN to SIGRTMAX.
 * Default settings for SIGKILL/SIGCONT/SIGSTOP could not be changed. */
static inline int isinvalid_signal(unsigned sys_signr)
{
   return (32 <= sys_signr && sys_signr < (unsigned)SIGRTMIN)
            || SIGKILL == sys_signr
            || SIGSTOP == sys_signr;
}

static int isblocked_signal(unsigned sys_signr)
{
   sigset_t mask;
   int err = pthread_sigmask(SIG_SETMASK, 0, &mask);
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_sigmask", err);
      goto ONERR;
   }

   return 1 == sigismember(&mask, (int)sys_signr);
ONERR:
   TRACEEXIT_ERRLOG(err);
   return 0;
}

static inline signal_config_e getconfig_signal(unsigned sys_signr)
{
   struct sigaction sigact;
   int err = sigaction((int)sys_signr, 0, &sigact);
   if (err) {
      err = errno;
      if (!err) err = EINVAL;
      TRACESYSCALL_ERRLOG("sigaction", err);
      PRINTINT_ERRLOG(sys_signr);
      goto ONERR;
   }

   signal_config_e cfg =
            sigact.sa_handler == SIG_DFL
            ? signal_config_DEFAULT
            : sigact.sa_handler == SIG_IGN
            ? signal_config_IGNORED
            : signal_config_HANDLER
         ;

   return cfg;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return signal_config_IGNORED;
}

static int getconfig2_signal(unsigned sys_signr, /*out*/signal_config_t* config)
{
   struct sigaction sigact;
   int err = sigaction((int)sys_signr, 0, &sigact);
   if (err) {
      err = errno;
      if (!err) err = EINVAL;
      TRACESYSCALL_ERRLOG("sigaction", err);
      PRINTINT_ERRLOG(sys_signr);
      goto ONERR;
   }

   signal_config_e cfg =
            sigact.sa_handler == SIG_DFL
            ? signal_config_DEFAULT
            : sigact.sa_handler == SIG_IGN
            ? signal_config_IGNORED
            : signal_config_HANDLER
         ;

   config->signr = (uint8_t) sys_signr;
   config->config = (uint8_t) cfg;
   config->isblocked = (uint8_t) isblocked_signal(sys_signr);
   config->handler = (cfg == signal_config_HANDLER ? sigact.sa_handler : 0);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

static int config_signal(unsigned sys_signr, signal_config_e cfg, sys_signalhandler_f signalhandler)
{
   int err;
   struct sigaction sigact;
   sigemptyset(&sigact.sa_mask);
   err = sigemptyset(&sigact.sa_mask);
   if (err) {
      err = EINVAL;
      TRACESYSCALL_ERRLOG("sigemptyset", err);
      goto ONERR;
   }

   if (signal_config_HANDLER == cfg) {
      sigact.sa_flags     = SA_ONSTACK | SA_SIGINFO;
      sigact.sa_sigaction = signalhandler;
   } else {
      sigact.sa_flags   = 0;
      sigact.sa_handler = cfg == signal_config_DEFAULT ? SIG_DFL : SIG_IGN;
   }

   err = sigaction((int)sys_signr, &sigact, 0);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigaction", err);
      PRINTINT_ERRLOG(sys_signr);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: init

int init_signals(signals_t* sigs)
{
   int err;
   struct {
      unsigned             signr;
      signal_config_e      cfg;
      sys_signalhandler_f  handler;
   }  active_signal_table[] = {
      // used to interrupt a blocking system call
      { SIGQUIT, signal_config_HANDLER, &dummy_signalhandler},
      // used to handle an invalid memory access
      { SIGSEGV, signal_config_HANDLER, &call_signalhandler},
      // ensures that calls to write return EPIPE
      { SIGPIPE, signal_config_IGNORED, 0},
   };
   struct {
      unsigned    from_signr;
      unsigned    to_signr;
   }  blocked_signal_table[] = {
      // realtime signals are used in send_signalrt
      { (unsigned)SIGRTMIN, (unsigned)SIGRTMAX},
      // used to suspend and resume a single thread
      { (unsigned)SIGINT, (unsigned)SIGINT },
      // allows terminal_t to wait for change of window size with sigwait,sigwaitinfo, or signalfd
      { (unsigned)SIGWINCH, (unsigned)SIGWINCH },
   };

   sigs->isinit = 0;

   err = pthread_sigmask(SIG_SETMASK, 0, &sigs->sys_old_mask);
   (void) PROCESS_testerrortimer(&s_signals_errtimer, &err);
   if (err) goto ONERR_sigmask;

   static_assert( lengthof(active_signal_table) == lengthof(sigs->old_config),
                  "enough space to safe old state" );
   for (unsigned i=0; i<lengthof(sigs->old_config); ++i) {
      err = getconfig2_signal(active_signal_table[i].signr, &sigs->old_config[i]);
      (void) PROCESS_testerrortimer(&s_signals_errtimer, &err);
      if (err) goto ONERR;
   }

   sigs->isinit = 1;
   sigs->segv = 0;

   // == config bunch of used signals ==

   // == unblocked signals

   sigset_t signalmask;
   err = sigemptyset(&signalmask);
   if (err) goto ONERR_emptyset;

   for (unsigned i=0; i<lengthof(active_signal_table); ++i) {
      unsigned signr = active_signal_table[i].signr;
      if (sigaddset(&signalmask, (int)signr))   { goto ONERR_add; }
      err = config_signal(signr, active_signal_table[i].cfg, active_signal_table[i].handler);
      (void) PROCESS_testerrortimer(&s_signals_errtimer, &err);
      if (err) goto ONERR;
   }

   err = pthread_sigmask(SIG_UNBLOCK, &signalmask, 0);
   (void) PROCESS_testerrortimer(&s_signals_errtimer, &err);
   if (err) goto ONERR_sigmask;

   // == blocked signals

   err = sigemptyset(&signalmask);
   if (err) goto ONERR_emptyset;

   for (unsigned i=0; i<lengthof(blocked_signal_table); ++i) {
      for (unsigned signr = blocked_signal_table[i].from_signr; signr <= blocked_signal_table[i].to_signr; ++signr) {
         if (sigaddset(&signalmask, (int)signr))  { goto ONERR_add; }
      }
   }

   err = pthread_sigmask(SIG_BLOCK, &signalmask, 0);
   (void) PROCESS_testerrortimer(&s_signals_errtimer, &err);
   if (err) goto ONERR_sigmask;

   return 0;
ONERR_sigmask:
   TRACESYSCALL_ERRLOG("pthread_sigmask", err);
   goto ONERR;
ONERR_emptyset:
   err = EINVAL;
   TRACESYSCALL_ERRLOG("sigemptyset", err);
   goto ONERR;
ONERR_add:
   err = EINVAL;
   TRACESYSCALL_ERRLOG("sigaddset", err);
   goto ONERR;
ONERR:
   (void) free_signals(sigs);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_signals(signals_t* sigs)
{
   int err,err2;

   if (sigs->isinit) {

      sigs->isinit = 0;

      err = 0;

      for (unsigned i=0; i<lengthof(sigs->old_config); ++i) {
         err2 = config_signal(sigs->old_config[i].signr, sigs->old_config[i].config, (sys_signalhandler_f) sigs->old_config[i].handler);
         (void) PROCESS_testerrortimer(&s_signals_errtimer, &err2);
         if (err2) err = err2;
      }

      err2 = pthread_sigmask(SIG_SETMASK, &sigs->sys_old_mask, 0);
      (void) PROCESS_testerrortimer(&s_signals_errtimer, &err2);
      if (err2) {
         err = err2;
         TRACESYSCALL_ERRLOG("pthread_sigmask", err);
      }

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

signalhandler_segv_f getsegv_signals(void)
{
   return self_maincontext()->signals->segv;
}

// group: change

void clearsegv_signals(void)
{
   volatile signalhandler_segv_f* segv = &self_maincontext()->signals->segv;
   *segv = 0;
}

void setsegv_signals(signalhandler_segv_f segfault_handler)
{
   volatile signalhandler_segv_f* segv = &self_maincontext()->signals->segv;
   *segv = segfault_handler;
}


/* struct: signalstate_t
 * System specific declaration. */
struct signalstate_t {
   /* variable: nr_signal_handlers
    * Number of stored signal handlers. */
   unsigned          nr_signal_handlers;
   /* variable: signalmask
    * The signal mask of the current thread. */
   sigset_t          signalmask;
   /* variable:      signal_handlers
    * Stores setting for every signal handler. */
   struct sigaction  signal_handlers[/*nr_signal_handlers*/];
};

// group: constants

/* define: FLAGMASK
 * Used to compare sigactino.sa_flags.
 * On Raspbian/RaspberryPi2 the flg 0x4000000 could be set randomly?. */
#define FLAGMASK \
         ((int) ((  SA_NOCLDSTOP|SA_NOCLDWAIT|SA_SIGINFO|SA_ONSTACK \
                 | SA_RESTART|SA_NODEFER|SA_RESETHAND|SA_INTERRUPT )))

// group: helper

/* function: objectsize_signalstate
 * Returns size in bytes needed to store a single <signalstate_t>. */
static size_t objectsize_signalstate(void)
{
   const size_t objectsize = sizeof(signalstate_t)
                           + sizeof(struct sigaction) * (unsigned) maxnr_signal();
   return objectsize;
}

// group: lifetime

int new_signalstate(/*out*/signalstate_t ** sigstate)
{
   int err;
   const unsigned    nr_signal_handlers = maxnr_signal();
   const size_t      objectsize         = objectsize_signalstate();
   memblock_t        mem                = memblock_FREE;
   signalstate_t *   newsigstate        = 0;

   err = RESIZE_MM(objectsize, &mem);
   if (err) goto ONERR;

   newsigstate = (signalstate_t*) mem.addr;
   memset(newsigstate, 0, objectsize);
   newsigstate->nr_signal_handlers = nr_signal_handlers;

   err = pthread_sigmask(SIG_SETMASK, 0, &newsigstate->signalmask);
   if (err) {
      TRACESYSCALL_ERRLOG("pthread_sigmask", err);
      goto ONERR;
   }

   for (unsigned i = nr_signal_handlers; i > 0; --i) {
      if (isinvalid_signal(i)) continue; // not used in Linux
      err = sigaction((int)i, 0, &newsigstate->signal_handlers[i-1]);
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("sigaction(i,...)", err);
         PRINTINT_ERRLOG(i);
         goto ONERR;
      }
   }

   *sigstate = newsigstate;

   return 0;
ONERR:
   (void) delete_signalstate(&newsigstate);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int delete_signalstate(signalstate_t ** sigstate)
{
   int err;
   signalstate_t * delsigstate = *sigstate;

   if (delsigstate) {
      memblock_t mem = memblock_INIT(objectsize_signalstate(), (uint8_t*)delsigstate);
      *sigstate = 0;

      err = FREE_MM(&mem);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

int compare_signalstate(const signalstate_t * sigstate1, const signalstate_t * sigstate2)
{
   if (sigstate1 && sigstate2) {
      const int nr_diff = (int)sigstate1->nr_signal_handlers - (int)sigstate2->nr_signal_handlers;
      if (nr_diff) {
         return sign_int(nr_diff);
      }

      int cmp = memcmp(&sigstate1->signalmask, &sigstate2->signalmask, sizeof(sigstate2->signalmask));
      if (cmp) {
         return cmp;
      }

      for (unsigned i = sigstate1->nr_signal_handlers; i-- > 0; ) {
         if (0 != (FLAGMASK & (sigstate1->signal_handlers[i].sa_flags ^ sigstate2->signal_handlers[i].sa_flags))) {
            return sign_int(sigstate1->signal_handlers[i].sa_flags - sigstate2->signal_handlers[i].sa_flags);
         }
         cmp = memcmp(&sigstate1->signal_handlers[i].sa_sigaction, &sigstate2->signal_handlers[i].sa_sigaction, sizeof(sigstate2->signal_handlers[i].sa_sigaction));
         if (cmp) {
            return cmp;
         }
      }

      return 0;
   }

   return (sigstate1) ? 1 : ((sigstate2) ? -1 : 0);
}


// section: signalwait_t

// group: lifetime

int initrealtime_signalwait(/*out*/signalwait_t * signalwait, signalrt_t minrt, signalrt_t maxrt)
{
   int err;
   sigset_t signalmask;

   VALIDATE_INPARAM_TEST(minrt <= maxrt, ONERR, );
   VALIDATE_INPARAM_TEST(maxrt <= maxnr_signalrt(), ONERR, );

   err = sigemptyset(&signalmask);
   if (err) {
      err = EINVAL;
      TRACESYSCALL_ERRLOG("sigemptyset", err);
      goto ONERR;
   }

   for (unsigned signr = minrt; signr <= maxrt; ++signr) {
      if (sigaddset(&signalmask, SIGRTMIN+(int)signr)) {
         err = EINVAL;
         TRACESYSCALL_ERRLOG("sigaddset", err);
         goto ONERR;
      }
   }

   int fd = signalfd(-1, &signalmask, SFD_NONBLOCK|SFD_CLOEXEC);
   if (-1 == fd) {
      err = errno;
      TRACESYSCALL_ERRLOG("signalfd", err);
      goto ONERR;
   }

   *signalwait = fd;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: free_signalwait
 * Frees all resources associated with signalwait.
 * After return the <iochannel_t> returned from <io_signalwait> is invalid. */
int free_signalwait(signalwait_t * signalwait)
{
   int err;

   err = free_iochannel(signalwait);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}



// section: signalrt_t

// group: query

signalrt_t maxnr_signalrt(void)
{
   return (signalrt_t) (SIGRTMAX - SIGRTMIN);
}

// group: change

int send_signalrt(signalrt_t nr, uintptr_t value)
{
   int err;

   VALIDATE_INPARAM_TEST(nr <= maxnr_signalrt(), ONERR, PRINTINT_ERRLOG(nr));

   err = sigqueue(getpid(), SIGRTMIN+nr, (union sigval) { .sival_ptr = (void*)(value) } );
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigqueue", err);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int send2_signalrt(signalrt_t nr, uintptr_t value, const thread_t * thread)
{
   int err;

   VALIDATE_INPARAM_TEST(nr <= maxnr_signalrt(), ONERR, PRINTINT_ERRLOG(nr));

   err = pthread_sigqueue(thread->sys_thread, SIGRTMIN+nr, (union sigval) { .sival_ptr = (void*)(value) } );
   if (err) {
      TRACESYSCALL_ERRLOG("sigqueue", err);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int wait_signalrt(signalrt_t nr, /*out*/uintptr_t * value)
{
   int err;
   sigset_t signalmask;

   VALIDATE_INPARAM_TEST(nr <= maxnr_signalrt(), ONERR, PRINTINT_ERRLOG(nr));

   err = sigemptyset(&signalmask);
   if (err) {
      err = EINVAL;
      TRACESYSCALL_ERRLOG("sigemptyset", err);
      goto ONERR;
   }

   err = sigaddset(&signalmask, SIGRTMIN+nr);
   if (!err) err = sigaddset(&signalmask, SIGQUIT);
   if (err) {
      err = EINVAL;
      TRACESYSCALL_ERRLOG("sigaddset", err);
      PRINTINT_ERRLOG(SIGRTMIN+nr);
      goto ONERR;
   }

   for (;;) {
      siginfo_t sinfo;
      err = sigwaitinfo(&signalmask, &sinfo);
      if (-1 == err || sinfo.si_signo == SIGQUIT) {
         if (-1 == err) {
            err = errno;
            if (err == EINTR) continue;
         } else {
            err = EINTR;
         }
         TRACESYSCALL_ERRLOG("sigwaitinfo", err);
         goto ONERR;
      }
      if (value) {
         *value = sinfo.si_code == SI_QUEUE ? (uintptr_t) sinfo.si_ptr : 0;
      }
      break;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int trywait_signalrt(signalrt_t nr, /*out*/uintptr_t * value)
{
   int err;
   sigset_t          signalmask;
   struct timespec   ts = { 0, 0 };

   VALIDATE_INPARAM_TEST(nr <= maxnr_signalrt(), ONERR, PRINTINT_ERRLOG(nr));

   err = sigemptyset(&signalmask);
   if (err) {
      err = EINVAL;
      TRACESYSCALL_ERRLOG("sigemptyset", err);
      goto ONERR;
   }

   err = sigaddset(&signalmask, SIGRTMIN+nr);
   if (err) {
      err = EINVAL;
      TRACESYSCALL_ERRLOG("sigaddset", err);
      PRINTINT_ERRLOG(SIGRTMIN+nr);
      goto ONERR;
   }

   for (;;) {
      siginfo_t sinfo;
      err = sigtimedwait(&signalmask, &sinfo, &ts);
      if (-1 == err) {
         err = errno;
         if (EAGAIN == err) return err;
         if (EINTR  == err) continue;
         TRACESYSCALL_ERRLOG("sigtimedwait", err);
         goto ONERR;
      }
      if (value) {
         *value = sinfo.si_code == SI_QUEUE ? (uintptr_t) sinfo.si_ptr : 0;
      }
      break;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: test

#ifdef KONFIG_UNITTEST

static int test_enum(void)
{
   static_assert(signal_config_DEFAULT == 0, "");
   static_assert(signal_config_IGNORED == 1, "");
   static_assert(signal_config_HANDLER == 2, "");
   static_assert(signal_config__NROF   == 3, "");
   return 0;
}

void dummy_sighandler(int signr, siginfo_t * siginfo, void * ucontext)
{
   (void) signr;
   (void) siginfo;
   (void) ucontext;
}

static int test_signalstate(void)
{
   signalstate_t  *  sigstate1 = 0;
   signalstate_t  *  sigstate2 = 0;
   bool              isoldact  = false;
   bool              isoldmask = false;
   sigset_t          oldmask;
   sigset_t          signalmask;
   struct sigaction  sigact;
   int               oldsignr;
   struct sigaction  oldact;

   // TEST new_signalstate, delete_signalstate
   TEST(0 == new_signalstate(&sigstate1));
   TEST(0 != sigstate1);
   TEST(SIGRTMAX == (int) sigstate1->nr_signal_handlers);
   memset(&signalmask, 0, sizeof(signalmask));
   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &signalmask));
   TEST(0 == memcmp(&signalmask, &sigstate1->signalmask, sizeof(signalmask)));
   TEST(0 == delete_signalstate(&sigstate1));
   TEST(0 == sigstate1);
   TEST(0 == delete_signalstate(&sigstate1));
   TEST(0 == sigstate1);

   // TEST compare_signalstate: equal
   TEST(0 == new_signalstate(&sigstate1));
   TEST(0 == new_signalstate(&sigstate2));
   TEST(0 == compare_signalstate(sigstate1, sigstate2));
   TEST(0 == delete_signalstate(&sigstate2));
   TEST(0 == delete_signalstate(&sigstate1));

   // TEST compare_signalstate: compare nr_signal_handlers
   TEST(0 == new_signalstate(&sigstate1));
   TEST(0 == new_signalstate(&sigstate2));
   ++ sigstate2->nr_signal_handlers;
   TEST(-1 == compare_signalstate(sigstate1, sigstate2));
   -- sigstate2->nr_signal_handlers;
   sigstate1->nr_signal_handlers += 100;
   TEST(+1 == compare_signalstate(sigstate1, sigstate2));
   sigstate1->nr_signal_handlers -= 100;
   TEST(0 == compare_signalstate(sigstate1, sigstate2));
   TEST(0 == delete_signalstate(&sigstate2));
   TEST(0 == delete_signalstate(&sigstate1));

   // TEST compare_signalstate: compare mask
   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &oldmask));
   isoldmask = true;
   sigemptyset(&signalmask);
   TEST(0 == sigaddset(&signalmask, SIGINT));
   TEST(0 == sigaddset(&signalmask, SIGUSR1));
   TEST(0 == pthread_sigmask(SIG_UNBLOCK, &signalmask, 0));
   TEST(0 == new_signalstate(&sigstate1));
   sigemptyset(&signalmask);
   TEST(0 == sigaddset(&signalmask, SIGINT));
   TEST(0 == sigaddset(&signalmask, SIGUSR1));
   TEST(0 == pthread_sigmask(SIG_BLOCK, &signalmask, 0));
   TEST(0 == new_signalstate(&sigstate2));
   TEST(0 != compare_signalstate(sigstate1, sigstate2));
   TEST(0 == delete_signalstate(&sigstate2));
   sigemptyset(&signalmask);
   TEST(0 == sigaddset(&signalmask, SIGINT));
   TEST(0 == sigaddset(&signalmask, SIGUSR1));
   TEST(0 == pthread_sigmask(SIG_UNBLOCK, &signalmask, 0));
   TEST(0 == new_signalstate(&sigstate2));
   TEST(0 == compare_signalstate(sigstate1, sigstate2));
   TEST(0 == delete_signalstate(&sigstate2));
   TEST(0 == delete_signalstate(&sigstate1));
   TEST(0 == pthread_sigmask(SIG_SETMASK, &oldmask, 0));
   isoldmask = false;

   // TEST compare_signalstate: change handler setting
   int testsignals[] = { SIGSEGV, SIGUSR1, SIGRTMIN, SIGRTMAX };
   for (unsigned i = 0; i < lengthof(testsignals); ++i) {
      const int signr = testsignals[i];
      TEST(0 == new_signalstate(&sigstate1));
      sigact.sa_sigaction = &dummy_sighandler;
      sigact.sa_flags     = SA_SIGINFO | SA_ONSTACK;
      TEST(0 == sigemptyset(&sigact.sa_mask));
      TEST(0 == sigaction(signr, &sigact, &oldact));
      isoldact = true;
      oldsignr = signr;
      TEST(0 == new_signalstate(&sigstate2));
      TEST(&dummy_sighandler == sigstate2->signal_handlers[signr-1].sa_sigaction)
      TEST(0 != compare_signalstate(sigstate1, sigstate2));
      TEST(0 == delete_signalstate(&sigstate2));
      isoldact = false;
      TEST(0 == sigaction(signr, &oldact, 0));
      TEST(0 == new_signalstate(&sigstate2));
      TEST(0 == compare_signalstate(sigstate1, sigstate2));
      TEST(0 == delete_signalstate(&sigstate2));
      TEST(0 == delete_signalstate(&sigstate1));
   }

   return 0;
ONERR:
   if (isoldact) sigaction(oldsignr, &oldact, 0);
   if (isoldmask) (void) pthread_sigmask(SIG_SETMASK, &oldmask, 0);
   (void) delete_signalstate(&sigstate1);
   (void) delete_signalstate(&sigstate2);
   return EINVAL;
}

static int thread_call_wait(void* dummy)
{
   thread_t* mainthread = dummy;
   pipe_t   pipe = pipe_FREE;
   iopoll_t poll = iopoll_FREE;
   size_t   nr;
   ioevent_t events[1];

   TEST(0 == init_pipe(&pipe));
   TEST(0 == init_iopoll(&poll));
   TEST(0 == register_iopoll(&poll, pipe.read, &(ioevent_t)ioevent_INIT_VAL32(ioevent_READ, 555)));
   resume_thread(mainthread);
   TEST(EINTR == wait_iopoll(&poll, &nr, 1, events, 5000));
   TEST(0 == free_iopoll(&poll));
   TEST(0 == free_pipe(&pipe));

   return 0;
ONERR:
   free_iopoll(&poll);
   free_pipe(&pipe);
   return EINVAL;
}

static int test_signals_helper(void)
{
   sigset_t oldmask;
   bool     isoldmask = false;
   thread_t* thread = 0;
   signal_config_t sigconf;

   TEST(0 == pthread_sigmask(SIG_SETMASK, 0, &oldmask));
   isoldmask = true;

   // TEST dummy_signalhandler
   // check handler assigned to SIGQUIT
   TEST(0 == getconfig2_signal(SIGQUIT, &sigconf));
   TEST(signal_config_HANDLER == sigconf.config);
   TEST(&dummy_signalhandler  == (sys_signalhandler_f) sigconf.handler);
   // start thread waiting on iopoll
   trysuspend_thread();
   TEST(0 == new_thread(&thread, &thread_call_wait, self_thread()));
   suspend_thread();
   // send thread SIGQUIT
   for (;;) {
      TEST(0 == pthread_kill(thread->sys_thread, SIGQUIT) || ESRCH == pthread_kill(thread->sys_thread, SIGQUIT));
      sleepms_thread(1);
      if (0 == tryjoin_thread(thread)) break;
   }
   // check returncode 0 (==> EINTR received from within thread)
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   // TEST maxnr_signal
   TEST((unsigned)SIGRTMAX == maxnr_signal());

   // TEST isinvalid_signal
   for (unsigned signr=1; signr<=maxnr_signal(); ++signr) {
      struct sigaction old;
      struct sigaction sa = { .sa_flags = SA_SIGINFO|SA_ONSTACK, .sa_sigaction = &dummy_signalhandler };
      sigemptyset(&sa.sa_mask);
      if (isinvalid_signal(signr)) {
         TEST(-1 == sigaction((int)signr, &sa, &old));
      } else {
         TEST(0 == sigaction((int)signr, &sa, &old));
         TEST(0 == sigaction((int)signr, &old, 0));
      }
   }

   // TEST isblocked_signal, getconfig2_signal: blocked/unblocked
   for (unsigned signr=1; signr<=maxnr_signal(); ++signr) {
      if (isinvalid_signal(signr)) continue;
      sigset_t mask;
      TEST(0 == sigemptyset(&mask));
      TEST(0 == sigaddset(&mask, (int)signr));
      if (isblocked_signal(signr)) {
         TEST(0 == pthread_sigmask(SIG_UNBLOCK, &mask, 0));
         TEST(0 == isblocked_signal(signr));
         TEST(0 == getconfig2_signal(signr, &sigconf));
         TEST(0 == sigconf.isblocked);
         TEST(0 == pthread_sigmask(SIG_SETMASK, &oldmask, 0));
         TEST(1 == isblocked_signal(signr));
         TEST(0 == getconfig2_signal(signr, &sigconf));
         TEST(1 == sigconf.isblocked);
      } else {
         TEST(0 == pthread_sigmask(SIG_BLOCK, &mask, 0));
         TEST(1 == isblocked_signal(signr));
         TEST(0 == getconfig2_signal(signr, &sigconf));
         TEST(1 == sigconf.isblocked);
         TEST(0 == pthread_sigmask(SIG_SETMASK, &oldmask, 0));
         TEST(0 == isblocked_signal(signr));
         TEST(0 == getconfig2_signal(signr, &sigconf));
         TEST(0 == sigconf.isblocked);
      }
   }

   // TEST getconfig_signal
   for (unsigned signr=1; signr<=7 && signr<=maxnr_signal(); ++signr) {
      if (isinvalid_signal(signr)) continue;
      struct sigaction old;
      struct sigaction sa = { .sa_flags = SA_SIGINFO|SA_ONSTACK, .sa_sigaction = &dummy_signalhandler };
      sigemptyset(&sa.sa_mask);
      TEST(0 == sigaction((int)signr, &sa, &old));
      TEST(signal_config_HANDLER == getconfig_signal(signr));
      sa = (struct sigaction){ .sa_flags = SA_ONSTACK, .sa_handler = SIG_IGN };
      TEST(0 == sigaction((int)signr, &sa, 0));
      TEST(signal_config_IGNORED == getconfig_signal(signr));
      sa = (struct sigaction){ .sa_flags = SA_ONSTACK, .sa_handler = SIG_DFL };
      TEST(0 == sigaction((int)signr, &sa, 0));
      TEST(signal_config_DEFAULT == getconfig_signal(signr));
      TEST(0 == sigaction((int)signr, &old, 0));
   }

   // TEST getconfig2_signal
   for (unsigned signr=(unsigned)SIGRTMIN; signr<=(unsigned)SIGRTMIN+7u && signr<=maxnr_signal(); ++signr) {
      struct sigaction old;
      struct sigaction sa = { .sa_flags = SA_SIGINFO|SA_ONSTACK, .sa_sigaction = &dummy_signalhandler };
      sigemptyset(&sa.sa_mask);
      TEST(0 == sigaction((int)signr, &sa, &old));
      TEST(0 == getconfig2_signal(signr, &sigconf));
      TEST(sigconf.signr  == signr);
      TEST(sigconf.config == signal_config_HANDLER);
      TEST(sigconf.isblocked == 1);
      TEST(sigconf.handler == (void(*)(int))&dummy_signalhandler);
      sa = (struct sigaction){ .sa_flags = SA_ONSTACK, .sa_handler = SIG_IGN };
      TEST(0 == sigaction((int)signr, &sa, 0));
      TEST(0 == getconfig2_signal(signr, &sigconf));
      TEST(sigconf.signr  == signr);
      TEST(sigconf.config == signal_config_IGNORED);
      TEST(sigconf.isblocked == 1);
      TEST(sigconf.handler == 0);
      sa = (struct sigaction){ .sa_flags = SA_ONSTACK, .sa_handler = SIG_DFL };
      TEST(0 == sigaction((int)signr, &sa, 0));
      TEST(0 == getconfig2_signal(signr, &sigconf));
      TEST(sigconf.signr  == signr);
      TEST(sigconf.config == signal_config_DEFAULT);
      TEST(sigconf.isblocked == 1);
      TEST(sigconf.handler == 0);
      TEST(0 == sigaction((int)signr, &old, 0));
   }

   // TEST config_signal
   for (unsigned signr=1; signr<=7 && signr<=maxnr_signal(); ++signr) {
      struct sigaction old;
      TEST(0 == sigaction((int)signr, 0, &old));
      for (signal_config_e sc=0; sc<signal_config__NROF; ++sc) {
         TEST(0 == config_signal(signr, sc, &dummy_signalhandler));
         TEST(0 == getconfig2_signal(signr, &sigconf));
         TEST(sigconf.signr  == signr);
         TEST(sigconf.config == sc);
         TEST(sigconf.isblocked == isblocked_signal(signr));
         if (sc == signal_config_HANDLER) {
            TEST(sigconf.handler == (void(*)(int))&dummy_signalhandler);
         } else {
            TEST(sigconf.handler == 0);
         }
      }
      TEST(0 == sigaction((int)signr, &old, 0));
   }

   // reset
   TEST(0 == pthread_sigmask(SIG_SETMASK, &oldmask, 0));

   return 0;
ONERR:
   if (isoldmask) (void) pthread_sigmask(SIG_SETMASK, &oldmask, 0);
   delete_thread(&thread);
   return EINVAL;
}

static int check_signal_config(unsigned signr, bool isblocked, signal_config_e conf, sys_signalhandler_f handler)
{
   signal_config_t config;
   int err = getconfig2_signal(signr, &config);
   if (err) goto ONERR;

   TEST(signr == config.signr);
   TEST(conf  == config.config);
   TEST(isblocked == config.isblocked);
   TEST(handler == (sys_signalhandler_f) config.handler);

   return 0;
ONERR:
   return EINVAL;
}

static int test_signals_initfree(void)
{
   signals_t      signs = signals_FREE;
   signalstate_t* sigstate = 0;
   signalstate_t* sigstate2 = 0;

   // TEST signals_FREE
   TEST(0 == signs.isinit);
   TEST(0 == signs.segv);

   // TEST init_signals, free_signals: restore old signal state
   for (unsigned tc=0; tc<2; ++tc) {
      TEST(0 == new_signalstate(&sigstate));
      TEST(0 == init_signals(&signs));
      TEST(1 == signs.isinit);
      TEST(0 == free_signals(&signs));
      TEST(0 == signs.isinit);
      // check old signal state
      TEST(0 == new_signalstate(&sigstate2));
      TEST(0 == compare_signalstate(sigstate, sigstate2));
      TEST(0 == delete_signalstate(&sigstate));
      TEST(0 == delete_signalstate(&sigstate2));
      if (tc == 1) {
         for (unsigned i=1; i<maxnr_signal(); ++i) {
            if (isinvalid_signal(i)) continue;
            TEST(signal_config_DEFAULT == getconfig_signal(i));
         }
      }
      // reset config to default handlers
      if (tc == 0) {
         for (unsigned i=1; i<maxnr_signal(); ++i) {
            if (isinvalid_signal(i)) continue;
            TEST(0 == config_signal(i, signal_config_DEFAULT, 0));
         }
      }
   }

   // TEST init_signals: configurated values
   memset(&signs, 255, sizeof(signs));
   signs.isinit = 0;
   TEST( 0 == init_signals(&signs));
   // check signal_t
   TEST( 1 == signs.isinit);
   TEST( SIGQUIT == signs.old_config[0].signr);
   TEST( SIGSEGV == signs.old_config[1].signr);
   TEST( SIGPIPE == signs.old_config[2].signr);
   TEST( 0 == signs.segv);
   static_assert( 3 == lengthof(signs.old_config), "every entry checked");

   // check configured signals
   for (unsigned i=1; i<maxnr_signal(); ++i) {
      if (isinvalid_signal(i)) continue;
      bool isRealtime = (unsigned)SIGRTMIN<=i && i<=(unsigned)SIGRTMAX;
      switch(i) {
      case SIGQUIT: TEST( 0 == check_signal_config(i, false, signal_config_HANDLER, &dummy_signalhandler)); break;
      case SIGSEGV: TEST( 0 == check_signal_config(i, false, signal_config_HANDLER, &call_signalhandler)); break;
      case SIGPIPE: TEST( 0 == check_signal_config(i, false, signal_config_IGNORED, 0)); break;
      case SIGWINCH: // also blocked
      case SIGINT:  TEST( 0 == check_signal_config(i, true, signal_config_DEFAULT, 0)); break;
      default:      TEST( 0 == check_signal_config(i, isRealtime, signal_config_DEFAULT, 0)); break;
      }
   }

   // TEST free_signals: double free
   for (unsigned tc=0; tc<2; ++tc) {
      TEST( 0 == free_signals(&signs));
      // check signal_t
      TEST( 0 == signs.isinit);
      // check configured signals
      for (unsigned i=1; i<maxnr_signal(); ++i) {
         if (isinvalid_signal(i)) continue;
         TEST(signal_config_DEFAULT == getconfig_signal(i));
      }
   }

   // TEST init_signals: simulated errors
   TEST(0 == new_signalstate(&sigstate));
   for (unsigned i=1; ; ++i) {
      init_testerrortimer(&s_signals_errtimer, i, (int)i);
      int err = init_signals(&signs);
      if (!err) {
         free_testerrortimer(&s_signals_errtimer);
         TESTP(i == 10, "i:%d", i);
         TEST(1 == signs.isinit);
         TEST(0 == free_signals(&signs));
         TEST(0 == new_signalstate(&sigstate2));
         TEST(0 == compare_signalstate(sigstate, sigstate2));
         TEST(0 == delete_signalstate(&sigstate2));
         break;
      }
      // check return value
      TEST(err == (int)i);
      // check sigs
      TEST(0 == signs.isinit);
      // check signal state
      TEST(0 == new_signalstate(&sigstate2));
      TEST(0 == compare_signalstate(sigstate, sigstate2));
      TEST(0 == delete_signalstate(&sigstate2));
   }
   TEST(0 == delete_signalstate(&sigstate));

   // TEST free_signals: simulated errors
   TEST(0 == new_signalstate(&sigstate));
   for (unsigned i=1; ; ++i) {
      TEST(0 == init_signals(&signs));
      init_testerrortimer(&s_signals_errtimer, i, (int)i);
      int err = free_signals(&signs);
      // check sigs
      TEST(0 == signs.isinit);
      // check signal state
      TEST(0 == new_signalstate(&sigstate2));
      TEST(0 == compare_signalstate(sigstate, sigstate2));
      TEST(0 == delete_signalstate(&sigstate2));
      if (!err) {
         free_testerrortimer(&s_signals_errtimer);
         TESTP(i == 5, "i:%d", i);
         TEST(0 == free_signals(&signs));
         break;
      }
      // check return value
      TEST(err == (int)i);
   }
   TEST(0 == delete_signalstate(&sigstate));

   return 0;
ONERR:
   free_signals(&signs);
   delete_signalstate(&sigstate);
   delete_signalstate(&sigstate2);
   return EINVAL;
}

static int thread_receivesignal(uintptr_t rtsignr)
{
   int err;
   uintptr_t value;
   assert(rtsignr);
   assert(0 == send_signalrt(0, 0));
   err = wait_signalrt((signalrt_t)rtsignr, &value);
   assert(0 == send_signalrt(0, value));
   return err;
}

static volatile void *  s_memaddr;
static volatile bool    s_ismapped;

static void segfault_handler(void * memaddr, bool ismapped)
{
   s_memaddr  = memaddr;
   s_ismapped = ismapped;
   abort_thread();
}

static int thread_segfault_send(void * dummy)
{
   (void) dummy;
   pthread_sigqueue(self_thread()->sys_thread, SIGSEGV, (union sigval) { .sival_ptr = 0 });
   kill(getpid(), SIGSEGV);
   kill(getpid(), SIGSEGV);
   pthread_sigqueue(self_thread()->sys_thread, SIGSEGV, (union sigval) { .sival_ptr = (void*)1 });
   return 0;
}

static int thread_segfault_write(void * memaddr)
{
   *(uint8_t*)memaddr = 0;
   return 0;
}

static int test_segv(void)
{
   signalhandler_segv_f oldhandler = getsegv_signals();
   thread_t *           thread     = 0;
   void *               memaddr    = MAP_FAILED;

   // TEST getsegv_signals
   for (uintptr_t i = 0; i < 10; ++i) {
      self_maincontext()->signals->segv = (signalhandler_segv_f)i;
      TEST(i == (uintptr_t)getsegv_signals());
   }

   // TEST clearsegv_signals
   clearsegv_signals();
   TEST(0 == getsegv_signals());

   // TEST setsegv_signals
   setsegv_signals(&segfault_handler);
   TEST(&segfault_handler == self_maincontext()->signals->segv);
   setsegv_signals(0);
   TEST(0 == self_maincontext()->signals->segv);

   // TEST setsegv_signals: user send SIGSEGV are ignored
   setsegv_signals(segfault_handler);
   TEST(0 == new_thread(&thread, &thread_segfault_send, (void*)0));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   // TEST setsegv_signals: write access to read-only
   setsegv_signals(segfault_handler);
   s_memaddr  = 0;
   s_ismapped = false;
   memaddr = mmap(0, 1, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   TEST(memaddr != MAP_FAILED);
   TEST(0 == new_thread(&thread, &thread_segfault_write, memaddr));
   TEST(0 == join_thread(thread));
   TEST(ENOTRECOVERABLE == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(s_memaddr  == memaddr);
   TEST(s_ismapped == true);

   // TEST setsegv_signals: write access to unmapped memory
   s_memaddr  = 0;
   s_ismapped = true;
   TEST(0 == munmap(memaddr, 1));
   setsegv_signals(segfault_handler);
   TEST(0 == new_thread(&thread, &thread_segfault_write, memaddr));
   TEST(0 == join_thread(thread));
   TEST(ENOTRECOVERABLE == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(s_memaddr  == memaddr);
   TEST(s_ismapped == false);
   memaddr = MAP_FAILED;

   // unprepare
   setsegv_signals(oldhandler);

   return 0;
ONERR:
   setsegv_signals(oldhandler);
   (void) delete_thread(&thread);
   if (memaddr != MAP_FAILED) munmap(memaddr, 1);
   return EINVAL;
}

static int thread_callwait(thread_t* caller)
{
   int err;
   uintptr_t value;
   resume_thread(caller);
   err = wait_signalrt(0, &value);
   CLEARBUFFER_ERRLOG();
   return err;
}

static int test_signalrt(void)
{
   sigset_t          signalmask;
   thread_t *        group[3] = { 0, 0, 0 };
   struct timespec   ts       = { 0, 0 };

   // prepare
   TEST(0 == sigemptyset(&signalmask));
   for (int i = SIGRTMIN; i <= SIGRTMAX; ++i) {
      TEST(0 == sigaddset(&signalmask, i));
   }

   // TEST maxnr_signalrt: system supports at least 8 signals
   TEST(maxnr_signalrt() == SIGRTMAX - SIGRTMIN);
   TEST(maxnr_signalrt() >= 8);

   // TEST trywait_signalrt
   while (0 < sigtimedwait(&signalmask, 0, &ts));
   for (int i = 0; i <= maxnr_signalrt(); ++i) {
      uintptr_t value = 1;
      uintptr_t V     = (unsigned)i + 100u;
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0));
      TEST(0 == kill(getpid(), SIGRTMIN+i));
      TEST(0 == trywait_signalrt((signalrt_t)i, &value));
      TEST(0 == value);
      TEST(0 == sigqueue(getpid(), SIGRTMIN+i, (union sigval) { .sival_ptr = (void*)V } ));
      TEST(0 == trywait_signalrt((signalrt_t)i, &value));
      TEST(V == value);
      ++ V;
      TEST(0 == pthread_sigqueue(self_thread()->sys_thread, SIGRTMIN+i, (union sigval) { .sival_ptr = (void*)V } ));
      TEST(0 == trywait_signalrt((signalrt_t)i, &value));
      TEST(V == value);
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0));
   }

   // TEST trywait_signalrt: EINVAL
   TEST(EINVAL == trywait_signalrt((signalrt_t)(maxnr_signalrt()+1), 0));

   // TEST wait_signalrt
   for (int i = 0; i <= maxnr_signalrt(); ++i) {
      uintptr_t value = 1;
      uintptr_t V     = (unsigned)i + 100u;
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0));
      TEST(0 == kill(getpid(), SIGRTMIN+i));
      TEST(0 == wait_signalrt((signalrt_t)i, &value));
      TEST(0 == value);
      TEST(0 == sigqueue(getpid(), SIGRTMIN+i, (union sigval) { .sival_ptr = (void*)V } ));
      TEST(0 == wait_signalrt((signalrt_t)i, &value));
      TEST(V == value);
      ++ V;
      TEST(0 == pthread_sigqueue(self_thread()->sys_thread, SIGRTMIN+i, (union sigval) { .sival_ptr = (void*)V } ));
      TEST(0 == wait_signalrt((signalrt_t)i, &value));
      TEST(V == value);
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0));
   }

   // TEST wait_signalrt: EINVAL
   TEST(EINVAL == wait_signalrt((signalrt_t)(maxnr_signalrt()+1), 0));

   // TEST wait_signalrt: EINTR
   trysuspend_thread();
   TEST(0 == newgeneric_thread(&group[0], &thread_callwait, self_thread()));
   // generate interrupt
   suspend_thread();
   sleepms_thread(1);
   interrupt_thread(group[0]);
   // check EINTR
   TEST(0 == join_thread(group[0]));
   TEST(EINTR == returncode_thread(group[0]));
   // reset
   TEST(0 == delete_thread(&group[0]));

   // TEST send_signalrt: signals are queued
   for (uintptr_t i = 1; i <= maxnr_signalrt(); ++i) {
      for (uintptr_t nr = 10; nr < 20; ++nr) {
         TEST(0 == send_signalrt((signalrt_t)i, nr));
      }
      for (uintptr_t nr = 10; nr < 20; nr += 2) {
         uintptr_t value = 0;
         TEST(0 == trywait_signalrt((signalrt_t)i, &value));
         TEST(nr == value);
         TEST(0 == wait_signalrt((signalrt_t)i, &value));
         TEST(nr+1 == value);
      }
   }

   // TEST send_signalrt: one thread receives / order unspecified
   for (uintptr_t i = 1; i <= maxnr_signalrt(); ++i) {
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0));
      for (unsigned t = 0; t < lengthof(group); ++t) {
         TEST(0 == newgeneric_thread(&group[t], &thread_receivesignal, i));
      }
      // wait for start of threads
      for (unsigned t = 0; t < lengthof(group); ++t) {
         TEST(0 == wait_signalrt(0, 0));
      }
      for (unsigned t = 1; t <= lengthof(group); ++t) {
         // wake up one thread
         TEST(0 == send_signalrt((signalrt_t)i, t*i));
         // wait until woken up
         uintptr_t v = 0;
         TEST(0 == wait_signalrt(0, &v));
         TEST(v == t*i);
         for (int isdel = 0; isdel == 0; ) {
            for (unsigned t2 = 0; t2 < lengthof(group); ++t2) {
               if (group[t2] && 0 == tryjoin_thread(group[t2])) {
                  TEST(0 == returncode_thread(group[t2]));
                  TEST(0 == delete_thread(&group[t2]));
                  isdel = 1;
               }
            }
         }
         // only one woken up
         unsigned count = t;
         for (unsigned t2 = 0; t2 < lengthof(group); ++t2) {
            count += (group[t2] != 0);
         }
         TEST(count == lengthof(group));
      }
   }

   // TEST send_signalrt: EINVAL
   TEST(EINVAL == send_signalrt((signalrt_t)(maxnr_signalrt()+1), 0));

   // TEST send_signalrt: EAGAIN
   unsigned queue_size;
   for (queue_size = 0; queue_size < 1000000; ++queue_size) {
      if (0 == send_signalrt(0, queue_size)) continue;
      TEST(EAGAIN == send_signalrt(0, 0));
      TEST(EAGAIN == send2_signalrt(0, 0, self_thread()));
      break;
   }
   TEST(queue_size > 16);
   for (unsigned i = 0; i < queue_size; ++i) {
      uintptr_t value;
      TEST(0 == wait_signalrt(0, &value));
      TEST(i == value);
   }
   TEST(EAGAIN == trywait_signalrt(0, 0));

   // TEST send2_signalrt: signals are queued
   for (uintptr_t i = 1; i <= maxnr_signalrt(); ++i) {
      for (uintptr_t nr = 20; nr < 30; ++nr) {
         TEST(0 == send2_signalrt((signalrt_t)i, nr, self_thread()));
      }
      for (uintptr_t nr = 20; nr < 30; nr += 2) {
         uintptr_t value = 0;
         TEST(0 == trywait_signalrt((signalrt_t)i, &value));
         TEST(nr == value);
         TEST(0 == wait_signalrt((signalrt_t)i, &value));
         TEST(nr+1 == value);
      }
   }

   // TEST send2_signalrt: only specific thread receives
   for (uintptr_t i = 1; i <= maxnr_signalrt(); ++i) {
      TEST(EAGAIN == trywait_signalrt((signalrt_t)i, 0));
      for (unsigned t = 0; t < lengthof(group); ++t) {
         TEST(0 == newgeneric_thread(&group[t], &thread_receivesignal, i));
      }
      // wait for start of threads
      for (unsigned t = 0; t < lengthof(group); ++t) {
         TEST(0 == wait_signalrt(0, 0));
      }
      for (unsigned t = 1; t <= lengthof(group); ++t) {
         // wake up one thread
         TEST(0 == send2_signalrt((signalrt_t)i, t*i, group[t-1]));
         // wait until woken up
         uintptr_t v = 0;
         TEST(0 == wait_signalrt(0, &v));
         TEST(v == t*i);
         TEST(0 == delete_thread(&group[t-1]));
         // only one woken up
         for (unsigned t2 = t; t2 < lengthof(group); ++t2) {
            TEST(EBUSY == tryjoin_thread(group[t2]));
         }
      }
   }

   // TEST send2_signalrt: EINVAL
   TEST(EINVAL == send2_signalrt((signalrt_t)(maxnr_signalrt()+1), 0, self_thread()));

   // TEST send2_signalrt: EAGAIN
   for (queue_size = 0; queue_size < 1000000; ++queue_size) {
      if (0 == send2_signalrt(0, queue_size, self_thread())) continue;
      TEST(EAGAIN == send2_signalrt(0, 0, self_thread()));
      TEST(EAGAIN == send_signalrt(0, 0));
      break;
   }
   TEST(queue_size > 16);
   for (unsigned i = 0; i < queue_size; ++i) {
      uintptr_t value;
      TEST(0 == trywait_signalrt(0, &value));
      TEST(i == value);
   }
   TEST(EAGAIN == trywait_signalrt(0, 0));

   // unprepare
   while (0 < sigtimedwait(&signalmask, 0, &ts));

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(group);++i) {
      (void) delete_thread(&group[i]);
   }
   while (0 < sigtimedwait(&signalmask, 0, &ts));
   return EINVAL;
}

static int test_signalwait(void)
{
   signalwait_t signalwait = signalwait_FREE;

   // TEST signalwait_FREE
   TEST(1 == isfree_iochannel(signalwait));

   // TEST initrealtime_signalwait
   TEST(0 == initrealtime_signalwait(&signalwait, 0, maxnr_signalrt()));
   TEST(0 == isfree_iochannel(signalwait));

   // TEST free_signalwait
   TEST(0 == free_signalwait(&signalwait));
   TEST(1 == isfree_iochannel(signalwait));
   TEST(0 == free_signalwait(&signalwait));
   TEST(1 == isfree_iochannel(signalwait));

   // TEST initrealtime_signalwait: EINVAL
   TEST(EINVAL == initrealtime_signalwait(&signalwait, 1, 0));
   TEST(1 == isfree_iochannel(signalwait));
   TEST(EINVAL == initrealtime_signalwait(&signalwait, 0, (signalrt_t)(maxnr_signalrt()+1)));
   TEST(1 == isfree_iochannel(signalwait));

   // TEST free_signalwait: EBADF
   int fd = dup(STDIN_FILENO);
   TEST(fd > 0);
   TEST(0 == close(fd));
   signalwait = fd;
   TEST(0 == isfree_iochannel(signalwait));
   TEST(EBADF == free_signalwait(&signalwait));
   TEST(1 == isfree_iochannel(signalwait));

   // TEST io_signalwait
   for (unsigned minrt = 0; minrt <= maxnr_signalrt(); ++minrt) {
      for (unsigned maxrt = minrt; maxrt <= maxnr_signalrt(); ++ maxrt) {
         TEST(0 == initrealtime_signalwait(&signalwait, (signalrt_t)minrt, maxnr_signalrt()));

         // TEST io_signalwait
         iochannel_t ioc = io_signalwait(signalwait);
         TEST(ioc == signalwait);
         TEST(1 == isvalid_iochannel(ioc));

         // TEST io_signalwait: returns always same value
         TEST(ioc == io_signalwait(signalwait));

            // TEST io_signalwait: file descriptor generates read event
         for (unsigned signr = minrt; signr <= maxrt; ++signr) {
            for (unsigned nrqueued = 1; nrqueued <= 2; ++nrqueued) {
               struct pollfd pfd = { .fd = ioc, .events = POLLIN };
               TEST(EAGAIN == trywait_signalrt((signalrt_t)minrt, 0));
               TEST(0 == poll(&pfd, 1, 0));    // not readable
               for (unsigned i = 0; i < nrqueued; ++i) {
                  TEST(0 == send_signalrt((signalrt_t)signr, 1u+signr+i));
               }
               for (unsigned i = 0; i < nrqueued; ++i) {
                  TEST(1 == poll(&pfd, 1, 0));    // readable
                  uintptr_t v = 0;
                  TEST(0 == trywait_signalrt((signalrt_t)signr, &v));
                  TEST(v == 1u+signr+i);
               }
               TEST(EAGAIN == trywait_signalrt((signalrt_t)signr, 0));
            }
         }

         if (maxrt < maxnr_signalrt()) {
            maxrt += 5u;
            if (maxrt >= maxnr_signalrt()) maxrt = maxnr_signalrt()-1u;
         }
         TEST(0 == free_signalwait(&signalwait));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_platform_sync_signal()
{
   int err;

   if (test_enum())                    goto ONERR;
   if (test_signalstate())             goto ONERR;
   if (test_signals_helper())          goto ONERR;
   if (execasprocess_unittest(&test_signals_initfree, &err)) goto ONERR;
   if (err) goto ONERR;
   if (test_segv())                    goto ONERR;
   if (test_signalrt())                goto ONERR;
   if (test_signalwait())              goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
