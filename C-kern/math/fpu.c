/* title: FloatingPointUnit impl
   Implements <FloatingPointUnit> in a gcc specific way.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/math/fpu.h
    Header file <FloatingPointUnit>.

   file: C-kern/math/fpu.c
    Implementation file <FloatingPointUnit impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/fpu.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/platform/sync/signal.h"
#endif


// section: fpuexcept

// group: enable

int enable_fpuexcept(fpu_except_e exception_flags)
{
   int err;

   if (-1 == feenableexcept(exception_flags)) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int disable_fpuexcept(fpu_except_e exception_flags)
{
   int err;

   if (-1 == fedisableexcept(exception_flags)) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// group: signal

int signal_fpuexcept(fpu_except_e exception_flags)
{
   int err ;

   if (feraiseexcept(exception_flags)) {
      err = errno ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int clear_fpuexcept(fpu_except_e exception_flags)
{
   int err ;

   if (feclearexcept(exception_flags)) {
      err = errno ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct test_config_t {
   bool isEnableSupported; // false ==> enable hardware exceptions are not supported
   bool isAlsoInexact;  // true ==> fpu_except_OVERFLOW signals also fpu_except_INEXACT;
                        //       && fpu_except_UNDERFLOW signals also fpu_except_INEXACT
} test_config_t;

#define test_config_INIT \
         { true, false }

static int test_fpuexcept_signalclear(test_config_t conf)
{
   fpu_except_e   oldenabled ;
   fpu_except_e   exceptflags[6] = {
      fpu_except_INVALID,
      fpu_except_DIVBYZERO,
      fpu_except_OVERFLOW|(conf.isAlsoInexact ? fpu_except_INEXACT : 0),
      fpu_except_UNDERFLOW|(conf.isAlsoInexact ? fpu_except_INEXACT : 0),
      fpu_except_INEXACT,
      fpu_except_MASK_ALL
   };

   // prepare
   oldenabled = getenabled_fpuexcept() ;
   TEST(0 == disable_fpuexcept(fpu_except_MASK_ALL)) ;

   // TEST clear_fpuexcept: signaled exception all cleared
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
   for (unsigned i = 0; i < lengthof(exceptflags); ++i) {
      TEST(0 == getsignaled_fpuexcept(exceptflags[i])) ;
   }

   // TEST signal_fpuexcept: all set
   TEST(0 == signal_fpuexcept(fpu_except_MASK_ALL)) ;
   for (unsigned i = 0; i < lengthof(exceptflags); ++i) {
      TEST(exceptflags[i] == getsignaled_fpuexcept(exceptflags[i])) ;
   }

   // TEST clear_fpuexcept: single bit
   for (unsigned i = 0, mask = fpu_except_MASK_ALL; i < lengthof(exceptflags); ++i) {
      mask = mask & ~ exceptflags[i] ;
      TEST(0 == clear_fpuexcept(exceptflags[i])) ;
      TEST(0 == getsignaled_fpuexcept(exceptflags[i])) ;
      TEST(mask == getsignaled_fpuexcept(fpu_except_MASK_ALL)) ;
      for (unsigned i2 = i+1; i2 < lengthof(exceptflags); ++i2) {
         if (exceptflags[i2] & ~mask) continue/*already cleared*/;
         TEST(exceptflags[i2] == getsignaled_fpuexcept(exceptflags[i2]));
      }
   }

   // TEST signal_fpuexcept: single bit
   for (unsigned i = 0; i < lengthof(exceptflags); ++i) {
      fpu_except_e flags = exceptflags[i];
      TEST(0 == signal_fpuexcept(flags));
      TEST(flags == getsignaled_fpuexcept(flags));
      TESTP(flags == getsignaled_fpuexcept(fpu_except_MASK_ALL), "i: %d fl:0x%x efl:0x%x get():0x%x", i, flags, exceptflags[i], getsignaled_fpuexcept(fpu_except_MASK_ALL));
      for (unsigned i2 = 0; i2 < lengthof(exceptflags); ++i2) {
         TESTP((exceptflags[i2] & flags) == getsignaled_fpuexcept(exceptflags[i2]), "i:%d i2:%d get():0x%x", i, i2, getsignaled_fpuexcept(exceptflags[i2]));
      }
      TEST(0 == clear_fpuexcept(flags));
      TEST(0 == getsignaled_fpuexcept(fpu_except_MASK_ALL));
   }

   // TEST fpu_except_INVALID: sqrt(-1)
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
   volatile long double d = time(0) ;
   for (int i = 2; i > -2; --i) {
      double d1 = sqrt(i) ;
      if (i >= 0) {
         TEST(0 == getsignaled_fpuexcept(fpu_except_INVALID)) ;
      } else {
         TEST(fpu_except_INVALID == getsignaled_fpuexcept(fpu_except_INVALID)) ;
      }
      d += d1 ;
      d *= time(0) & 0x3 ;
   }

   // TEST fpu_except_DIVBYZERO: 1.0 / 0.0
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
   d = 1 ;
   for (int i = 2; i > -1; --i) {
      double d1 = 1.0 / (double)i ;
      if (i != 0) {
         TEST(0 == getsignaled_fpuexcept(fpu_except_INVALID)) ;
      } else {
         TEST(fpu_except_DIVBYZERO == getsignaled_fpuexcept(fpu_except_DIVBYZERO)) ;
         TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
      }
      d += d1 ;
      d *= time(0) & 0x3 ;
   }

   // TEST fpu_except_OVERFLOW: DBL_MAX
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
   for (int i = 2; i > 0; --i) {
      d = LDBL_MAX ;
      d += (LDBL_MAX/i) ;
      TEST(fpu_except_OVERFLOW == getsignaled_fpuexcept(fpu_except_OVERFLOW)) ;
      TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
      TEST(isinf(d)) ;
   }

   // TEST fpu_except_UNDERFLOW: DBL_MIN / 2
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
   for (int i = 2; i > 0; --i) {
      d = LDBL_MIN ;
      d /= 1e10 * (1 + (time(0) & 0x03)) ;
      TEST(fpu_except_UNDERFLOW == getsignaled_fpuexcept(fpu_except_UNDERFLOW)) ;
      TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
   }

   // TEST fpu_except_INEXACT: 1 / 3
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
   d = 1 ;
   for (int i = 2; i > 0; --i) {
      volatile double d1 = 1.0 / (3.0 * (1 + (time(0) & 0x3))) ;
      TEST(fpu_except_INEXACT == getsignaled_fpuexcept(fpu_except_INEXACT)) ;
      TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
      d += d1 ;
      d *= (time(0) & 0x3) ;
   }

   // unprepare
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL));
   TEST(0 == disable_fpuexcept(fpu_except_MASK_ALL));
   TEST(!conf.isEnableSupported || 0 == enable_fpuexcept(oldenabled));

   return 0;
ONERR:
   clear_fpuexcept(fpu_except_MASK_ALL);
   disable_fpuexcept(fpu_except_MASK_ALL);
   enable_fpuexcept(oldenabled);
   return EINVAL;
}

static ucontext_t    s_continue ;
static siginfo_t     s_siginfo ;
static int           s_sigcount ;
static int           s_getcontext_count ;

static void fpe_sighandler(int signr, siginfo_t * siginfo, void * ucontext)
{
   (void)signr ;
   (void)ucontext ;
   ++ s_sigcount ;
   memcpy(&s_siginfo, siginfo, sizeof(s_siginfo)) ;
   setcontext(&s_continue) ;
}

static long double generate_all_exceptions(long double dmin, long double dmax, double dneg, double dzero)
{
   volatile long double result = 0 ;
   result += dmin / (1e10 * (1 + (time(0) & 0x03))) ;
   result += dmax * 2 ;
   result += sqrt(dneg) ;
   if (1 > (1 / dzero)) result += 0.1 ;
   return result ;
}

static int test_fpuexcept_enabledisable(test_config_t conf)
{
   volatile bool     isoldsignalmask = false;
   volatile bool     isoldact1       = false;
   sigset_t          oldsignalmask;
   sigset_t          signalmask;
   struct sigaction  oldact1;
   struct sigaction  sigact1;
   fpu_except_e      oldenabled;
   volatile unsigned i;
   fpu_except_e      exceptflags[6] = {
      fpu_except_INVALID,
      fpu_except_DIVBYZERO,
      fpu_except_OVERFLOW,
      fpu_except_UNDERFLOW,
      fpu_except_INEXACT,
      fpu_except_MASK_ALL
   };

   if (!conf.isEnableSupported) {
      return 0;
   }

   // prepare: install signalhandler
   oldenabled = getenabled_fpuexcept();
   TEST(0 == disable_fpuexcept(fpu_except_MASK_ALL));
   TEST(0 == sigemptyset(&signalmask));
   TEST(0 == sigaddset(&signalmask, SIGFPE));
   TEST(0 == sigprocmask(SIG_UNBLOCK, &signalmask, &oldsignalmask));
   isoldsignalmask = true;
   sigact1.sa_sigaction = &fpe_sighandler;
   sigact1.sa_flags     = SA_SIGINFO | SA_ONSTACK;
   TEST(0 == sigemptyset(&sigact1.sa_mask));
   TEST(0 == sigaction(SIGFPE, &sigact1, &oldact1));
   isoldact1 = true;

   // TEST enable_fpuexcept, disable_fpuexcept
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL));
   TEST(0 == disable_fpuexcept(fpu_except_MASK_ALL));
   TEST(0 == getenabled_fpuexcept());
   for (i = 0; i < lengthof(exceptflags); ++i) {
      TESTP(0 == enable_fpuexcept(exceptflags[i]), "i:%d err:%d", i, enable_fpuexcept(exceptflags[i]));
      TEST(exceptflags[i] == getenabled_fpuexcept());
      TEST(0 == disable_fpuexcept(exceptflags[i]));
      TEST(0 == getenabled_fpuexcept());
   }

   // TEST enable_fpuexcept + throwing exception with fpu instruction
   for (i = 0; i < lengthof(exceptflags); ++i) {
      if (fpu_except_MASK_ALL == exceptflags[i]) continue ;
      s_getcontext_count = 0 ;
      s_sigcount         = 0 ;
      memset(&s_siginfo, 0, sizeof(s_siginfo)) ;
      TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
      TEST(0 == getcontext(&s_continue)) ;
      ++ s_getcontext_count ;
      if (1 == s_getcontext_count) {
         TEST(0 == getsignaled_fpuexcept(fpu_except_MASK_ALL)) ;
         (void) generate_all_exceptions(LDBL_MIN, LDBL_MAX, -1, 0) ;
         TEST(fpu_except_MASK_ALL == getsignaled_fpuexcept(fpu_except_MASK_ALL)) ;
         TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
         TEST(0 == getsignaled_fpuexcept(fpu_except_MASK_ALL)) ;
         TEST(0 == enable_fpuexcept(exceptflags[i])) ;
         TEST(exceptflags[i] == getenabled_fpuexcept()) ;
         (void) generate_all_exceptions(LDBL_MIN, LDBL_MAX, -1, 0) ;
      }
      TEST(0 == getenabled_fpuexcept()) ;
      TEST(2 == s_getcontext_count) ;
      TEST(1 == s_sigcount) ;
      TEST(SIGFPE == s_siginfo.si_signo)
      switch(exceptflags[i]) {
      case fpu_except_INVALID:      TEST(FPE_FLTINV == s_siginfo.si_code) ; break ;
      case fpu_except_DIVBYZERO:    TEST(FPE_FLTDIV == s_siginfo.si_code) ; break ;
      case fpu_except_OVERFLOW:     TEST(FPE_FLTOVF == s_siginfo.si_code) ; break ;
      case fpu_except_UNDERFLOW:    TEST(FPE_FLTUND == s_siginfo.si_code) ; break ;
      case fpu_except_INEXACT:      TEST(FPE_FLTRES == s_siginfo.si_code) ; break ;
      case fpu_except_MASK_ALL:     break ;
      case fpu_except_MASK_ERR:     break ;
      }
   }

   // TEST enable_fpuexcept + throwing exception with signal_fpuexcept
   for (i = 0; i < lengthof(exceptflags); ++i) {
      if (fpu_except_MASK_ALL == exceptflags[i]) continue ;
      s_getcontext_count = 0 ;
      s_sigcount         = 0 ;
      memset(&s_siginfo, 0, sizeof(s_siginfo)) ;
      TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
      TEST(0 == getcontext(&s_continue)) ;
      ++ s_getcontext_count ;
      if (1 == s_getcontext_count) {
         TEST(0 == enable_fpuexcept(exceptflags[i])) ;
         TEST(exceptflags[i] == getenabled_fpuexcept()) ;
         TEST(0 == signal_fpuexcept(exceptflags[i])) ;
      }
      TEST(0 == getenabled_fpuexcept()) ;
      TEST(2 == s_getcontext_count) ;
      TEST(1 == s_sigcount) ;
      TEST(SIGFPE == s_siginfo.si_signo)
      switch(exceptflags[i]) {
      case fpu_except_INVALID:      TEST(FPE_FLTINV == s_siginfo.si_code) ; break ;
      case fpu_except_DIVBYZERO:    TEST(FPE_FLTDIV == s_siginfo.si_code) ; break ;
      case fpu_except_OVERFLOW:     TEST(FPE_FLTOVF == s_siginfo.si_code) ; break ;
      case fpu_except_UNDERFLOW:    TEST(FPE_FLTUND == s_siginfo.si_code) ; break ;
      case fpu_except_INEXACT:      TEST(FPE_FLTRES == s_siginfo.si_code) ; break ;
      case fpu_except_MASK_ALL:     break ;
      case fpu_except_MASK_ERR:     break ;
      }
   }

   // unprepare: restore signalhandler
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL));
   TEST(0 == disable_fpuexcept(fpu_except_MASK_ALL));
   TEST(0 == enable_fpuexcept(oldenabled));
   isoldsignalmask = false;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0));
   isoldact1 = false;
   TEST(0 == sigaction(SIGFPE, &oldact1, 0));

   return 0 ;
ONERR:
   clear_fpuexcept(fpu_except_MASK_ALL) ;
   disable_fpuexcept(fpu_except_MASK_ALL) ;
   enable_fpuexcept(oldenabled) ;
   if (isoldsignalmask) sigprocmask(SIG_SETMASK, &oldsignalmask, 0) ;
   if (isoldact1)       sigaction(SIGFPE, &oldact1, 0) ;
   return EINVAL ;
}

static bool s_isEnableSupported;

static int thread_testenabled(fpu_except_e * flag)
{
   if (         0 == wait_signalrt(4, 0)
         && (!s_isEnableSupported || (fpu_except_MASK_ALL & ~*flag) == getenabled_fpuexcept())
         && *flag == getsignaled_fpuexcept(fpu_except_MASK_ALL)) {
      disable_fpuexcept(*flag);
      return 0;
   }

   return EINVAL ;
}

static int test_fpuexcept_thread(test_config_t conf)
{
   thread_t     * thread1 = 0;
   fpu_except_e   oldenabled;
   fpu_except_e   exceptflags[6] = {
      0,
      fpu_except_INVALID,
      fpu_except_DIVBYZERO,
      fpu_except_OVERFLOW|(conf.isAlsoInexact ? fpu_except_INEXACT : 0),
      fpu_except_UNDERFLOW|(conf.isAlsoInexact ? fpu_except_INEXACT : 0),
      fpu_except_INEXACT
   };

   // prepare
   s_isEnableSupported = conf.isEnableSupported;
   oldenabled = getenabled_fpuexcept();
   TEST(0 == disable_fpuexcept(fpu_except_MASK_ALL));

   // TEST thread inherits fpu settings and does not change settings in main thread
   TEST(EAGAIN == trywait_signalrt(4, 0)) ;
   for (unsigned i = 0; i < lengthof(exceptflags); ++i) {
      const fpu_except_e E = (fpu_except_MASK_ALL & ~exceptflags[i]);
      TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL));
      TEST(!conf.isEnableSupported || 0 == enable_fpuexcept(E));
      TEST(0 == signal_fpuexcept(exceptflags[i]));
      TESTP(exceptflags[i] == getsignaled_fpuexcept(fpu_except_MASK_ALL), "i:%d F:0x%x get():0x%x", i, exceptflags[i], getsignaled_fpuexcept(fpu_except_MASK_ALL));
      TEST(0 == newgeneric_thread(&thread1, &thread_testenabled, &exceptflags[i]));
      TEST(0 == send_signalrt(4, 0));
      TEST(0 == join_thread(thread1));
      TESTP(0 == returncode_thread(thread1), "i:%d", i);
      TEST(0 == delete_thread(&thread1));
      TEST(!conf.isEnableSupported || E == getenabled_fpuexcept());
      TEST(0 == disable_fpuexcept(E));
   }
   TEST(EAGAIN == trywait_signalrt(4, 0));

   // TEST changes in main thread are also local
   TEST(EAGAIN == trywait_signalrt(4, 0)) ;
   for (unsigned i = 0; i < lengthof(exceptflags); ++i) {
      const fpu_except_e E = (fpu_except_MASK_ALL & ~exceptflags[i]);
      TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL));
      TEST(!conf.isEnableSupported || 0 == enable_fpuexcept(E));
      TEST(0 == signal_fpuexcept(exceptflags[i]));
      TEST(0 == newgeneric_thread(&thread1, &thread_testenabled, &exceptflags[i]));
      TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL));
      TEST(!conf.isEnableSupported || 0 == enable_fpuexcept(fpu_except_MASK_ALL));
      TEST(0 == send_signalrt(4, 0));
      TEST(0 == join_thread(thread1));
      TEST(0 == returncode_thread(thread1));
      TEST(0 == delete_thread(&thread1));
      TEST(0 == disable_fpuexcept(fpu_except_MASK_ALL));
   }
   TEST(EAGAIN == trywait_signalrt(4, 0));

   // unprepare
   TEST(0 == clear_fpuexcept(fpu_except_MASK_ALL)) ;
   TEST(0 == disable_fpuexcept(fpu_except_MASK_ALL)) ;
   TEST(!conf.isEnableSupported || 0 == enable_fpuexcept(oldenabled));

   return 0 ;
ONERR:
   send_signalrt(4, 0);
   delete_thread(&thread1);
   clear_fpuexcept(fpu_except_MASK_ALL);
   disable_fpuexcept(fpu_except_MASK_ALL);
   enable_fpuexcept(oldenabled);
   return EINVAL;
}

int unittest_math_fpu()
{
   test_config_t conf = test_config_INIT;

   // check if supported
   if (0 == getenabled_fpuexcept()) {
      clear_fpuexcept(fpu_except_MASK_ALL);
      if (-1 != feenableexcept(fpu_except_MASK_ALL)) {
         disable_fpuexcept(fpu_except_MASK_ALL);
      } else {
         if (! isrepeat_unittest()) {
            logwarning_unittest("fpu hardware exceptions not supported");
         }
         conf.isEnableSupported = false;
      }

      clear_fpuexcept(fpu_except_MASK_ALL);
      signal_fpuexcept(fpu_except_OVERFLOW);
      if ((fpu_except_OVERFLOW|fpu_except_INEXACT) == getsignaled_fpuexcept(fpu_except_MASK_ALL)) {
         conf.isAlsoInexact = true;
      }
      clear_fpuexcept(fpu_except_MASK_ALL);
   }

   if (test_fpuexcept_signalclear(conf))     goto ONERR;
   if (test_fpuexcept_enabledisable(conf))   goto ONERR;
   if (test_fpuexcept_thread(conf))          goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
