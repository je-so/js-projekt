/* title: PseudoTerminal impl

   Implements <PseudoTerminal>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/pseudoterm.h
    Header file <PseudoTerminal>.

   file: C-kern/platform/Linux/io/pseudoterm.c
    Implementation file <PseudoTerminal impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/terminal/pseudoterm.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: pseudoterm_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_pseudoterm_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_pseudoterm_errtimer = test_errortimer_FREE;
#endif

/* variable: s_pseudoterm_lock
 * Used as lock. */
static atomicflag_t s_pseudoterm_lock = 0;

// group: helper

static int prepare_pseudoterm(int fd)
{
   int err;
   struct sigaction oldact;

   // The  behavior  of  grantpt()  is  unspecified  if  a  signal handler is
   // installed to catch SIGCHLD signals.

   err = sigaction(SIGCHLD, 0, &oldact);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("sigaction(SIGCHLD,...)", err);
      return err;
   }

   const bool issiginfo = 0 != (oldact.sa_flags & SA_SIGINFO);
   if (  (issiginfo && (sighandler_t)oldact.sa_sigaction != SIG_DFL)
         || (!issiginfo && oldact.sa_handler != SIG_DFL)) {
      // do not abort, call grantpt nevertheless
      TRACE_ERRLOG(log_flags_START|log_flags_END, STATE_WRONG_SIGHANDLER_DEFINED, EINVAL, "SIGCHLD");
   }

   err = grantpt(fd);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("grantpt(fd)", err);
      PRINTINT_ERRLOG(fd);
      return err;
   }

   err = unlockpt(fd);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("unlockpt(fd)", err);
      PRINTINT_ERRLOG(fd);
      return err;
   }

   return 0;
}

// group: lifetime

int init_pseudoterm(/*out*/pseudoterm_t* pty)
{
   int err;
   int fd = open("/dev/ptmx", O_RDWR|O_NOCTTY|O_CLOEXEC|O_NONBLOCK);

   if (-1 == fd) {
      err = errno;
      goto ONERR;
   }

   ONERROR_testerrortimer(&s_pseudoterm_errtimer, &err, ONERR);
   err = prepare_pseudoterm(fd);
   if (err) goto ONERR;

   // set out param
   pty->master_device = fd;

   return 0;
ONERR:
   if (-1 != fd) close(fd);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_pseudoterm(pseudoterm_t* pty)
{
   int err;

   err = free_iochannel(&pty->master_device);
   if (err) goto ONERR;
   ONERROR_testerrortimer(&s_pseudoterm_errtimer, &err, ONERR);

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

int pathname_pseudoterm(const pseudoterm_t* pty, size_t len, /*out*/uint8_t name[len], /*err;out*/size_t* namesize)
{
   int err;

   // acquire lock
   while (0 != set_atomicflag(&s_pseudoterm_lock)) {
      sched_yield();
   }

   // function ptsname not thread safe
   const char* ptyname = ptsname(pty->master_device);

   if (!ptyname) {
      err = errno;

   } else {
      size_t namelen = strlen(ptyname);
      if (namelen >= len) {
         err = ENOBUFS;
      } else {
         memcpy(name, ptyname, namelen);
         name[namelen] = 0;
         err = 0;
      }

      if (namesize) *namesize = namelen + 1/*\0 byte*/;
   }

   // release lock
   clear_atomicflag(&s_pseudoterm_lock);

   if (err) goto ONERR;

   return 0;
ONERR:
   if (err != ENOBUFS) {
      TRACEEXIT_ERRLOG(err);
   }
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static void sigaction_sigchld(int signr, siginfo_t* info, void* ucontext)
{
   (void) signr;
   (void) info;
   (void) ucontext;
}

static void sighandler_sigchld(int signr)
{
   (void) signr;
}

static int test_helper(void)
{
   int  fd = -1;
   int  fd2 = -1;
   bool isoldact = false;
   struct sigaction oldact;
   struct sigaction newact;
   const char* name;
   uint8_t*    logbuffer;
   size_t      logsize1, logsize2;
   char        buffer[16];

   // TEST prepare_pseudoterm: normal operation
   for (int flag = 0; flag <= SA_SIGINFO; flag += SA_SIGINFO) {
      // prepare
      newact.sa_flags = flag;
      if (flag)
         newact.sa_sigaction = (void(*)(int, siginfo_t*, void*)) SIG_DFL;
      else
         newact.sa_handler = SIG_DFL;
      sigemptyset(&newact.sa_mask);
      TEST(0 == sigaction(SIGCHLD, &newact, &oldact));
      isoldact = true;
      fd = posix_openpt(O_RDWR|O_NOCTTY);
      TEST(fd > 0);

      // test

      // check open is not possible
      name = ptsname(fd);
      TEST(-1 == open(name, O_RDWR|O_NOCTTY|O_CLOEXEC));
      TEST(EIO == errno);

      GETBUFFER_ERRLOG(&logbuffer, &logsize1);
      TEST(0 == prepare_pseudoterm(fd));
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2);

      // check open is possible now
      fd2 = open(name, O_RDWR|O_NOCTTY|O_CLOEXEC);
      TEST(fd2 > 0);
      // check I/O
      TEST(3 == write(fd2, "xyc", 3));
      TEST(3 == read(fd, buffer, sizeof(buffer)));
      TEST(0 == memcmp("xyc", buffer, 3));
      TEST(4 == write(fd, "asd\n", 4));
      TEST(4 == read(fd2, buffer, sizeof(buffer)));
      TEST(0 == memcmp("asd\n", buffer, 4));

      // unprepare
      TEST(0 == close(fd2));
      TEST(0 == close(fd));
      isoldact = true;
      TEST(0 == sigaction(SIGCHLD, &oldact, 0));
   }

   // TEST prepare_pseudoterm: SIGCHLD handler installed
   for (int flag = 0; flag <= SA_SIGINFO; flag += SA_SIGINFO) {
      // prepare
      newact.sa_flags = flag;
      if (flag)
         newact.sa_sigaction = &sigaction_sigchld;
      else
         newact.sa_handler = &sighandler_sigchld;
      sigemptyset(&newact.sa_mask);
      TEST(0 == sigaction(SIGCHLD, &newact, &oldact));
      isoldact = true;
      fd = posix_openpt(O_RDWR|O_NOCTTY);
      TEST(fd > 0);

      // test

      // check open is not possible
      name = ptsname(fd);
      TEST(-1 == open(name, O_RDWR|O_NOCTTY|O_CLOEXEC));
      TEST(EIO == errno);

      GETBUFFER_ERRLOG(&logbuffer, &logsize1);
      TEST(0 == prepare_pseudoterm(fd));
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize2 > logsize1); // state error (SIGCHLD) written into ERRLOG

      // check open is possible now
      fd2 = open(name, O_RDWR|O_NOCTTY|O_CLOEXEC);
      TEST(fd2 > 0);
      // check I/O
      TEST(3 == write(fd2, "xyc", 3));
      TEST(3 == read(fd, buffer, sizeof(buffer)));
      TEST(0 == memcmp("xyc", buffer, 3));
      TEST(4 == write(fd, "asd\n", 4));
      TEST(4 == read(fd2, buffer, sizeof(buffer)));
      TEST(0 == memcmp("asd\n", buffer, 4));

      // unprepare
      TEST(0 == close(fd));
      TEST(0 == close(fd2));
      isoldact = true;
      TEST(0 == sigaction(SIGCHLD, &oldact, 0));
   }

   // TEST prepare_pseudoterm: EBADF
   TEST(EBADF == prepare_pseudoterm(sys_iochannel_FREE));

   return 0;
ONERR:
   if (fd != -1) close(fd);
   if (fd2 != -1) close(fd2);
   if (isoldact) sigaction(SIGCHLD, &oldact, 0);
   return EINVAL;
}

static int test_initfree(void)
{
   pseudoterm_t pty  = pseudoterm_FREE;
   uint8_t      buffer[16] = { 0 };
   const char*  name = 0;
   struct termios tconf;
   int fd = -1;
   int flags;

   // TEST pseudoterm_FREE
   TEST(sys_iochannel_FREE == pty.master_device);

   // TEST init_pseudoterm: EINVAL
   init_testerrortimer(&s_pseudoterm_errtimer, 1, EINVAL);
   TEST(EINVAL == init_pseudoterm(&pty));
   TEST(sys_iochannel_FREE == pty.master_device);

   // TEST init_pseudoterm
   TEST(0 == init_pseudoterm(&pty));
   TEST(0 <  pty.master_device);
   TEST(1 == isatty(pty.master_device));
   // check O_CLOEXEC, O_NONBLOCK
   TEST(FD_CLOEXEC == fcntl(pty.master_device, F_GETFD));
   flags = fcntl(pty.master_device, F_GETFL);
   TEST(-1 != flags);
   TEST(O_NONBLOCK == (flags&O_NONBLOCK));
   // check read/write with closed slave
   TEST(-1 == read(pty.master_device, buffer, sizeof(buffer)));
   TEST(EAGAIN == errno);
   TEST(1 == write(pty.master_device, "m", 1)); // saved in buffer
   // check open of slave is possible
   name = ptsname(pty.master_device);
   TEST(0 != name);
   fd = open(name, O_RDWR|O_CLOEXEC);
   TEST(0 < fd);
   TEST(1 == isatty(fd));
   TEST(0 == tcgetattr(fd, &tconf));
   TEST(0 == tcsetattr(fd, TCSANOW, &tconf));
   // check data transfer between master/slave
   TEST(1 == write(pty.master_device, "\n", 1));
   TEST(2 == read(fd, buffer, sizeof(buffer)));
   TEST(0 == memcmp(buffer, "m\n", 2));
   TEST(2 == write(fd, "s\n", 2));
   TEST(6 == read(pty.master_device, buffer, sizeof(buffer)));
   TEST(0 == memcmp(buffer, "m\r\ns\r\n", 6));
   // close slave
   TEST(0 == close(fd));
   struct pollfd pfd = { .fd = pty.master_device, .events = POLLIN };
   TEST(1 == poll(&pfd, 1, 0));
   TEST(0 != (pfd.revents&POLLHUP));
   for (int i = 0; i < 2; ++i) {
      TEST(-1 == read(pty.master_device, buffer, sizeof(buffer)));
      TEST(EIO == errno);
   }

   // TEST free_pseudoterm: (+ double free)
   for (int i = 0; i < 2; ++i) {
      TEST(0 == free_pseudoterm(&pty));
      TEST(sys_iochannel_FREE == pty.master_device);
      // check open of slave is *no more* possible
      errno = 0;
      TEST(-1 == open(name, O_RDWR|O_CLOEXEC));
      TEST(ENOENT == errno); // device removed
   }

   // TEST free_pseudoterm: EINVAL
   TEST(0 == init_pseudoterm(&pty));
   init_testerrortimer(&s_pseudoterm_errtimer, 1, EINVAL);
   TEST(EINVAL == free_pseudoterm(&pty));
   TEST(sys_iochannel_FREE == pty.master_device);

   return 0;
ONERR:
   if (fd != -1) close(fd);
   free_pseudoterm(&pty);
   return EINVAL;
}

typedef struct pathname_param_t {
   pseudoterm_t* pty;
   uint8_t*      expect;
   size_t        S;
   int           started;
} pathname_param_t;

static int thread_pathname(pathname_param_t* param)
{
   uint8_t ptypath[128];
   size_t  namesize;

   add_atomicint(&param->started, 1);
   TEST(0 == pathname_pseudoterm(param->pty, sizeof(ptypath), ptypath, &namesize));
   add_atomicint(&param->started, 1);
   TEST(param->S == namesize);
   TEST(0 == memcmp(ptypath, param->expect, param->S));

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   pseudoterm_t pty = pseudoterm_FREE;
   pseudoterm_t pty2 = pseudoterm_FREE;
   uint8_t      ptypath[128];
   uint8_t      expect[128];
   size_t       namesize;
   size_t       S;
   const char*  name;
   uint8_t*     logbuffer;
   size_t       logsize1, logsize2;
   thread_t*    thr;

   // TEST io_pseudoterm
   for (int i = 0; i < 256; ++i) {
      pty.master_device = i;
      TEST(i == io_pseudoterm(&pty));
   }
   pty.master_device = sys_iochannel_FREE;
   TEST(sys_iochannel_FREE == io_pseudoterm(&pty));

   // TEST pathname_pseudoterm: ENOTTY
   namesize = 0;
   TEST(ENOTTY == pathname_pseudoterm(&pty, sizeof(ptypath), ptypath, &namesize));
   TEST(0 == s_pseudoterm_lock);
   TEST(0 == namesize);

   // prepare
   TEST(0 == init_pseudoterm(&pty));
   name = ptsname(io_pseudoterm(&pty));
   TEST(0 != name)
   S = strlen(name) + 1;
   TEST(3 < S && S <= sizeof(expect));
   memcpy(expect, name, S);

   // TEST pathname_pseudoterm
   for (int issize = 0; issize <= 1; ++issize) {
      memset(ptypath, 255, sizeof(ptypath));
      namesize = 0;
      TEST(0 == pathname_pseudoterm(&pty, sizeof(ptypath), ptypath, issize ? &namesize : 0));
      TEST(namesize == (issize ? S : 0));
      TEST(0 == s_pseudoterm_lock);
      TEST(0 == memcmp(ptypath, expect, S));
      for (size_t s = S; s < sizeof(ptypath); ++s) {
         TEST(255 == ptypath[s]);
      }
   }

   // TEST pathname_pseudoterm: interthread locking
   pathname_param_t param = { .pty = &pty, .expect = expect, .S = S, .started = 0 };
   TEST(0 == set_atomicflag(&s_pseudoterm_lock));
   TEST(0 == newgeneric_thread(&thr, &thread_pathname, &param));
   for (int i = 0; i < 10000; ++i) {
      sched_yield();
      if (read_atomicint(&param.started)) break;
   }
   sleepms_thread(50);
   TEST(1 == read_atomicint(&param.started));
   clear_atomicflag(&s_pseudoterm_lock);
   TEST(0 == join_thread(thr));
   TEST(2 == param.started);
   TEST(0 == returncode_thread(thr));
   TEST(0 == delete_thread(&thr));

   // TEST pathname_pseudoterm: ENOBUFS
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   for (size_t s = 0; s <= S-1; ++s) {
      namesize = 0;
      TEST(ENOBUFS == pathname_pseudoterm(&pty, s, ptypath, &namesize));
      TEST(0 == s_pseudoterm_lock);
      TEST(S == namesize);
   }
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize1 == logsize2); // no log written

   // TEST pathname_pseudoterm: different path
   TEST(0 == init_pseudoterm(&pty2));
   TEST(0 == pathname_pseudoterm(&pty2, sizeof(ptypath), ptypath, &namesize));
   TEST(0 == s_pseudoterm_lock);
   TEST(S <= namesize);
   TEST(0 == memcmp(ptypath, expect, S-3));
   TEST(0 != memcmp(ptypath, expect, S));

   // unprepare
   TEST(0 == free_pseudoterm(&pty));
   TEST(0 == free_pseudoterm(&pty2));

   return 0;
ONERR:
   clear_atomicflag(&s_pseudoterm_lock);
   delete_thread(&thr);
   free_pseudoterm(&pty);
   free_pseudoterm(&pty2);
   return EINVAL;
}

int unittest_io_terminal_pseudoterm()
{
   uint8_t    termpath1[128];
   uint8_t    termpath2[128];

   // get path to controlling terminal
   TEST(0 == ttyname_r(sys_iochannel_STDIN, (char*) termpath1, sizeof(termpath1)));

   if (test_helper())   goto ONERR;
   if (test_initfree()) goto ONERR;
   if (test_query())    goto ONERR;

   // check controlling terminal has not changed
   TEST(getsid(0) == tcgetsid(sys_iochannel_STDIN));
   TEST(0 == ttyname_r(sys_iochannel_STDIN, (char*) termpath2, sizeof(termpath2)));
   TEST(0 == strcmp((char*)termpath1, (char*)termpath2));

   return 0;
ONERR:
   return EINVAL;
}

#endif
