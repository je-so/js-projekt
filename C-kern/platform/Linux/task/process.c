/* title: Process Linuximpl
   Implements <Process>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/task/process.h
    Header file of <Process>.

   file: C-kern/platform/Linux/task/process.c
    Implementation file <Process Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/task/process.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/io/pipe.h"
#endif


typedef enum queryoption_e {
   queryoption_NOWAIT,
   queryoption_WAIT,
   queryoption_WAIT_AND_FREE
} queryoption_e;

typedef struct childprocess_exec_t {
   const char *   filename;
   char      **   arguments;
   iochannel_t    errpipe;
} childprocess_exec_t;


// struct: process_stdio2_t
typedef struct process_stdio2_t {
   process_stdio_t   stdfd;
   iochannel_t       devnull;
} process_stdio2_t;

// group: lifetime

#define process_stdfd2_FREE { process_stdio_INIT_DEVNULL, iochannel_FREE }

/* function: init_processstdio2
 * Initializes <process_stdio2_t> with <process_stdio_t> and opens devnull.
 * The device null is only opened if stdfd is 0 or at least one file descriptor
 * is set to <iochannel_FREE>. */
static int init_processstdio2(/*out*/process_stdio2_t* stdfd2, process_stdio_t* stdfd)
{
   int err;
   int devnull = -1;

   if (  !stdfd
      || iochannel_FREE == stdfd->std_in
      || iochannel_FREE == stdfd->std_out
      || iochannel_FREE == stdfd->std_err ) {
      devnull = open("/dev/null", O_RDWR|O_CLOEXEC);
      if (-1 == devnull) {
         err = errno;
         TRACESYSCALL_ERRLOG("open(/dev/null,O_RDWR)", err);
         goto ONERR;
      }
   }

   // return out param stdfd2
   if (stdfd) {
      stdfd2->stdfd = *stdfd;
   } else {
      stdfd2->stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL;
   }
   stdfd2->devnull = devnull;

   return 0;
ONERR:
   free_iochannel(&devnull);
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: free_processstdio2
 * Closes if devnull if necessary. */
static int free_processstdio2(process_stdio2_t* stdfd2)
{
   int err;

   err = free_iochannel(&stdfd2->devnull);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

/* function: redirectstdfd_processstdio2
 * Redirects one standard channel to read/write from/to a file.
 *
 * Parameter:
 * stdfd           - The file descriptor of the standard io channel.
 *                   Set this value to one of STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
 * redirectto_file - The file descriptor of the file which now becomes the new standard io channel.
 *                   Use value <iochannel_FREE> to redirect to devnull.
 *                   Use same value as stdfd if the standard channel should be
 *                   inherited between processes.
 * */
static int redirectstdfd_processstdio2(const process_stdio2_t* stdfd2, int stdfd, int redirectto_file)
{
   int err;
   int fd = redirectto_file;

   if (stdfd != fd) {
      if (iochannel_FREE == fd) {
         fd = stdfd2->devnull;
      }
      while (-1 == dup2(fd, stdfd)) {
         if (EINTR != errno) {
            err = errno;
            TRACESYSCALL_ERRLOG("dup2(fd, stdfd)", err);
            PRINTINT_ERRLOG(fd);
            PRINTINT_ERRLOG(stdfd);
            goto ONERR;
         }
      }
      // dup2: FD_CLOEXEC is cleared
   } else {
      // clear FD_CLOEXEC
      // ignore error in case stdfd is closed
      (void) fcntl(stdfd, F_SETFD, 0);
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: redirectstdio_processstdio2
 * Redirects all 3 standard io channels to read/write from/to a file.
 *
 * If the new file descriptors have the same values as the standard channels
 * then no redirection is done but they are inherited.
 *
 * Uses <redirectstdfd_processstdio2> to implement its functionality. */
static int redirectstdio_processstdio2(const process_stdio2_t* stdfd2)
{
   int err;
   int err2;

   err = redirectstdfd_processstdio2(stdfd2, STDIN_FILENO, stdfd2->stdfd.std_in);

   err2 = redirectstdfd_processstdio2(stdfd2, STDOUT_FILENO, stdfd2->stdfd.std_out);
   if (err2) err = err2;

   err2 = redirectstdfd_processstdio2(stdfd2, STDERR_FILENO, stdfd2->stdfd.std_err);
   if (err2) err = err2;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: process_t

// group: helper

static int queryresult_process(sys_process_t pid, /*out*/process_result_t* result, queryoption_e option)
{
   int err;
   siginfo_t info;
   int       flags;

#define FLAGS (WEXITED|WSTOPPED)

   switch (option) {
   case queryoption_NOWAIT:
      // WNOWAIT: Leave the child in a waitable state; a later wait call can
      //          be used to retrieve the child status information again.
      flags = FLAGS|WNOHANG|WNOWAIT;
      break;
   case queryoption_WAIT:
      flags = FLAGS|WNOWAIT;
      break;
   default:
      VALIDATE_INPARAM_TEST(option == queryoption_WAIT_AND_FREE, ONERR, PRINTINT_ERRLOG(option));
      flags = FLAGS;
      break;
   }

#undef FLAGS

   info.si_pid = 0;
   while (-1 == waitid(P_PID, (id_t) pid, &info, flags)) {
      if (EINTR != errno) {
         err = errno;
         TRACESYSCALL_ERRLOG("waitid",err);
         PRINTINT_ERRLOG(pid);
         goto ONERR;
      }
   }

   if (pid != info.si_pid) {
      result->state = process_state_RUNNABLE;
      return 0;
   }

   if (CLD_EXITED == info.si_code) {
      result->state = process_state_TERMINATED;
      result->returncode = info.si_status;
   } else if (    CLD_KILLED == info.si_code
               || CLD_DUMPED == info.si_code) {
      result->state = process_state_ABORTED;
      result->returncode = info.si_status;
   } else if (    CLD_STOPPED == info.si_code
               || CLD_TRAPPED == info.si_code) {
      result->state = process_state_STOPPED;
   } else {
      result->state = process_state_RUNNABLE;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

static int childprocess_exec(childprocess_exec_t * execparam)
{
   int err;
   ssize_t write_err;

   execvp( execparam->filename, execparam->arguments );
   err = errno;

// ONERR:

   do {
      write_err = write( execparam->errpipe, &err, sizeof(&err));
   } while (-1 == write_err && errno == EINTR);

   return err;
}

// group: lifetime

int initexec_process(/*out*/process_t * process, const char * filename, const char * const * arguments, process_stdio_t * stdfd/*0 => /dev/null*/)
{
   int err;
   process_t            childprocess = process_FREE;
   int                  pipefd[2]    = { -1, -1 };
   childprocess_exec_t  execparam    = { filename, (char**)(uintptr_t)arguments, -1 };

   if ( pipe2(pipefd,O_CLOEXEC) ) {
      err = errno;
      TRACESYSCALL_ERRLOG("pipe2", err);
      goto ONERR;
   }

   execparam.errpipe = pipefd[1];

   err = initgeneric_process(&childprocess, &childprocess_exec, &execparam, stdfd);
   if (err) goto ONERR;

   // CHECK exec error
   err = free_iochannel(&pipefd[1]);
   assert(!err);

   ssize_t read_bytes = 0;

   int exec_err = 0;

   do {
      read_bytes = read( pipefd[0], &exec_err, sizeof(exec_err));
   } while (-1 == read_bytes && EINTR == errno);

   if (-1 == read_bytes) {
      err = errno;
      TRACESYSCALL_ERRLOG("read", err);
      goto ONERR;
   } else if (read_bytes) {
      // EXEC error
      err = exec_err ? exec_err : ENOEXEC;
      TRACESYSCALL_ERRLOG("execvp(filename, arguments)", err);
      PRINTCSTR_ERRLOG(filename);
      for (size_t i = 0; arguments[i]; ++i) {
         PRINTARRAYFIELD_ERRLOG("s", arguments, i);
      }
      goto ONERR;
   }

   err = free_iochannel(&pipefd[0]);
   if (err) goto ONERR;

   *process = childprocess;

   return 0;
ONERR:
   free_iochannel(&pipefd[1]);
   free_iochannel(&pipefd[0]);
   (void) free_process(&childprocess);
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: fork_process
 * This function call returns twice.
 * It creates a new child process whihc returns from this with pid set to 0.
 * The calling process (parent process) gets the child's process returned in pid. */
static int fork_process(/*out*/pid_t * pid)
{
   int err;

   // MULTITHREAD-PRECONDITION: all files opened with O_CLOEXEC; => test/static/close_on_exec.sh

   // TODO: MULTITHREAD: system handler for preparing fork
   pid_t newpid = fork();
   if (-1 == newpid) {
      err = errno;
      TRACESYSCALL_ERRLOG("fork", err);
      goto ONERR;
   }

   *pid = newpid;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

static int preparechild_process(process_stdio_t * stdfd/*0 => /dev/null*/)
{
   int err;
   process_stdio2_t stdfd2 = process_stdfd2_FREE;

   // TODO: MULTITHREAD: system handler for freeing thread resources (i.e. clear log, ...) ?!?

   err = init_processstdio2(&stdfd2, stdfd);
   if (err) goto ONERR;

   err = redirectstdio_processstdio2(&stdfd2);
   if (err) goto ONERR;

   err = free_processstdio2(&stdfd2);
   if (err) goto ONERR;

   return 0;
ONERR:
   free_processstdio2(&stdfd2);
   return err;
}

int init_process(/*out*/process_t   *  process,
                  process_task_f       child_main,
                  void              *  start_arg,
                  process_stdio_t   *  stdfd/*0 => /dev/null*/)
{
   int err;
   pid_t pid = 0;

   err = fork_process(&pid);
   if (err) goto ONERR;

   if (0 == pid) {
      // NEW CHILD PROCESS
      err = preparechild_process(stdfd);
      assert(!err);
      int returncode = (child_main ? child_main(start_arg) : 0);
      exit(returncode);
   }

   *process = pid;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_process(process_t * process)
{
   int err;
   pid_t pid = *process;

   static_assert(0 == sys_process_FREE, "0 is no valid process id");

   if (pid) {

      *process = sys_process_FREE;

      kill(pid, SIGKILL);

      process_result_t result;
      err = queryresult_process(pid, &result, queryoption_WAIT_AND_FREE);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

int name_process(size_t namebuffer_size, /*out*/char name[namebuffer_size], /*out*/size_t * name_size)
{
   int err;
   char buffer[16+1];

   buffer[16] = 0;

   err = prctl(PR_GET_NAME, buffer, 0, 0, 0);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("prctl(PR_GET_NAME)", err);
      goto ONERR;
   }

   size_t size = 1 + strlen(buffer);

   if (name_size) {
      *name_size = size;
   }

   if (namebuffer_size) {
      if (namebuffer_size >= size) {
         memcpy( name, buffer, size);
      } else {
         memcpy( name, buffer, namebuffer_size-1);
         name[namebuffer_size-1] = 0;
      }
   }

   return 0;
ONERR:
   return err;
}

int state_process(process_t * process, /*out*/process_state_e * current_state)
{
   int err;
   process_result_t result;

   err = queryresult_process(*process, &result, queryoption_NOWAIT);
   if (err) goto ONERR;

   *current_state = result.state;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: change

int daemonize_process(process_stdio_t  * stdfd/*0 => /dev/null*/)
{
   int err;
   pid_t pid = 0;

   err = fork_process(&pid);
   if (err) goto ONERR;

   if (0 != pid) {
      // calling process
      exit(0);
   }

   // CHILD DAEMON PROCESS
   umask(S_IROTH|S_IWOTH|S_IXOTH);
   err = preparechild_process(stdfd);
   if (  (pid_t)-1 == setsid()
         || 0 != chdir("/")) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int wait_process(process_t * process, /*out*/process_result_t* result)
{
   int err;
   pid_t pid = *process;

   kill(pid, SIGCONT);

   for (;;) {
      process_result_t state;

      err = queryresult_process(pid, &state, queryoption_WAIT);
      if (err) goto ONERR;

      switch(state.state) {
      case process_state_RUNNABLE:
         break;
      case process_state_STOPPED:
         kill(pid, SIGCONT);
         break;
      case process_state_TERMINATED:
      case process_state_ABORTED:
         if (result) {
            *result = state;
         }
         return 0;
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}



// section: test

#ifdef KONFIG_UNITTEST

static int test_processresult(void)
{
   process_result_t result = { 0, 0 };

   // TEST isequal_processresult
   for (int r = -5; r <= 5; ++r) {
      for (int s = 0; s < process_state__NROF; ++s) {
         result.returncode = r;
         result.state      = (process_state_e) s;
         TEST(1 == isequal_processresult(&result, r, (process_state_e) s));
         TEST(0 == isequal_processresult(&result, r, (process_state_e) (s+1)));
         TEST(0 == isequal_processresult(&result, r, (process_state_e) (s-1)));
         TEST(0 == isequal_processresult(&result, r+1, (process_state_e) s));
         TEST(0 == isequal_processresult(&result, r-1, (process_state_e) s));
         TEST(0 == isequal_processresult(&result, r+3, (process_state_e) (s+3)));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int childprocess_return(intptr_t returncode)
{
   return (int) returncode;
}

static int childprocess_endlessloop(void * dummy)
{
   (void) dummy;
   kill(getppid(), SIGINT);
   for (;;) {
      sleepms_thread(1000);
   }
   return 0;
}

static void suspend_process(void)
{
   sigset_t signalmask;

   sigemptyset(&signalmask);
   sigaddset(&signalmask, SIGINT);
   sigwaitinfo(&signalmask, 0);

   return;
}

static int childprocess_signal(intptr_t signr)
{
   kill(getppid(), SIGINT);
   kill(getpid(), (int)signr);
   return 0;
}

static int chilprocess_execassert(void * dummy)
{
   (void) dummy;
   // flushing of log output is redirected to devnull (from caller)
   assert(0);
   return 0;
}

static int childprocess_donothing(void * dummy)
{
   (void) dummy;
   return 0;
}

static int childprocess_statechange(intptr_t fd)
{
   dprintf((int)fd,"sleep\n");
   kill( getppid(), SIGINT);
   kill( getpid(), SIGSTOP);
   dprintf((int)fd,"run\n");
   kill( getppid(), SIGINT);
   for (;;) {
      sleepms_thread(1000);
   }
   return 0;
}

static int test_redirect(void)
{
   process_stdio_t  stdfd  = { 0, 0, 0 };

   // TEST static init: process_stdio_INIT_DEVNULL
   stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL;
   TEST(-1 == stdfd.std_in);
   TEST(-1 == stdfd.std_out);
   TEST(-1 == stdfd.std_err);
   TEST(iochannel_FREE == stdfd.std_in);
   TEST(iochannel_FREE == stdfd.std_out);
   TEST(iochannel_FREE == stdfd.std_err);

   // TEST static init: process_stdio_INIT_INHERIT
   stdfd = (process_stdio_t) process_stdio_INIT_INHERIT;
   TEST(0 == stdfd.std_in);
   TEST(1 == stdfd.std_out);
   TEST(2 == stdfd.std_err);
   TEST(STDIN_FILENO  == stdfd.std_in);
   TEST(STDOUT_FILENO == stdfd.std_out);
   TEST(STDERR_FILENO == stdfd.std_err);
   TEST(iochannel_STDIN  == stdfd.std_in);
   TEST(iochannel_STDOUT == stdfd.std_out);
   TEST(iochannel_STDERR == stdfd.std_err);

   // TEST redirectin_processstdio, redirectout_processstdio, redirecterr_processstdio
   for (int i = 0; i < 100; ++i) {
      stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL;
      TEST(iochannel_FREE == stdfd.std_in);
      TEST(iochannel_FREE == stdfd.std_out);
      TEST(iochannel_FREE == stdfd.std_err);
      redirectin_processstdio(&stdfd, i);
      TEST(i == stdfd.std_in);
      TEST(iochannel_FREE == stdfd.std_out);
      TEST(iochannel_FREE == stdfd.std_err);
      redirectout_processstdio(&stdfd, i+1);
      TEST(i == stdfd.std_in);
      TEST(i == stdfd.std_out-1);
      TEST(iochannel_FREE == stdfd.std_err);
      redirecterr_processstdio(&stdfd, i+2);
      TEST(i == stdfd.std_in);
      TEST(i == stdfd.std_out-1);
      TEST(i == stdfd.std_err-2);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_redirect2(void)
{
   process_stdio2_t  stdfd2      = process_stdfd2_FREE;
   int               oldstdfd[3] = { [STDIN_FILENO] = -1, [STDOUT_FILENO] = -1, [STDERR_FILENO] = -1 };
   int               pipefd1[2]  = { -1, -1 };
   int               pipefd2[2]  = { -1, -1 };
   char              buffer[10]  = { 0 };
   process_stdio_t   stdfd;

   // TEST static init
   TEST(iochannel_FREE == stdfd2.stdfd.std_in);
   TEST(iochannel_FREE == stdfd2.stdfd.std_out);
   TEST(iochannel_FREE == stdfd2.stdfd.std_err);
   TEST(iochannel_FREE == stdfd2.devnull);

   // TEST init_processstdio2: stdfd== 0
   MEMSET0(&stdfd2);
   stdfd2.devnull = iochannel_FREE;
   TEST(0 == init_processstdio2(&stdfd2, 0));
   TEST(iochannel_FREE == stdfd2.stdfd.std_in);
   TEST(iochannel_FREE == stdfd2.stdfd.std_out);
   TEST(iochannel_FREE == stdfd2.stdfd.std_err);
   TEST(iochannel_FREE != stdfd2.devnull);

   // TEST free_processstdio2
   TEST(0 == free_processstdio2(&stdfd2));
   TEST(iochannel_FREE == stdfd2.devnull);
   TEST(0 == free_processstdio2(&stdfd2));
   TEST(iochannel_FREE == stdfd2.devnull);

   // TEST init_processstdio2: process_stdio_INIT_INHERIT
   stdfd = (process_stdio_t) process_stdio_INIT_INHERIT;
   stdfd2.devnull = -2;
   TEST(0 == init_processstdio2(&stdfd2, &stdfd));
   TEST(iochannel_STDIN  == stdfd2.stdfd.std_in);
   TEST(iochannel_STDOUT == stdfd2.stdfd.std_out);
   TEST(iochannel_STDERR == stdfd2.stdfd.std_err);
   TEST(iochannel_FREE == stdfd2.devnull);
   TEST(0 == free_processstdio2(&stdfd2));
   TEST(iochannel_FREE == stdfd2.devnull);

   // TEST init_processstdio2: one fd is set to devnull
   for (int i = 0; i < 3; ++i) {
      stdfd = (process_stdio_t) process_stdio_INIT_INHERIT;
      stdfd2.devnull = iochannel_FREE;
      switch(i) { // set one std fd to devnull
      case 0:  redirectin_processstdio(&stdfd, iochannel_FREE); break;
      case 1:  redirectout_processstdio(&stdfd, iochannel_FREE); break;
      case 2:  redirecterr_processstdio(&stdfd, iochannel_FREE); break;
      }
      TEST(0 == init_processstdio2(&stdfd2, &stdfd));
      if (i == 0) {
         TEST(iochannel_FREE == stdfd2.stdfd.std_in);
      } else {
         TEST(iochannel_STDIN == stdfd2.stdfd.std_in);
      }
      if (i == 1) {
         TEST(iochannel_FREE == stdfd2.stdfd.std_out);
      } else {
         TEST(iochannel_STDOUT == stdfd2.stdfd.std_out);
      }
      if (i == 2) {
         TEST(iochannel_FREE == stdfd2.stdfd.std_err);
      } else {
         TEST(iochannel_STDERR == stdfd2.stdfd.std_err);
      }
      TEST(iochannel_FREE != stdfd2.devnull);
      TEST(0 == free_processstdio2(&stdfd2));
      TEST(iochannel_FREE == stdfd2.devnull);
      TEST(0 == free_processstdio2(&stdfd2));
      TEST(iochannel_FREE == stdfd2.devnull);
   }

   // store old stdio
   oldstdfd[iochannel_STDIN]  = dup(iochannel_STDIN);
   oldstdfd[iochannel_STDOUT] = dup(iochannel_STDOUT);
   oldstdfd[iochannel_STDERR] = dup(iochannel_STDERR);
   for (unsigned i = 0; i < lengthof(oldstdfd); ++i) {
      TEST(-1 != oldstdfd[i]);
   }
   TEST(0 == pipe2(pipefd1, O_CLOEXEC|O_NONBLOCK));
   TEST(0 == pipe2(pipefd2, O_CLOEXEC|O_NONBLOCK));

   // TEST redirectstdio_processstdio2
   stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL;
   redirectin_processstdio(&stdfd, pipefd1[0]);
   redirectout_processstdio(&stdfd, pipefd1[1]);
   redirecterr_processstdio(&stdfd, pipefd2[1]);
   TEST(0 == init_processstdio2(&stdfd2, &stdfd));
   TEST(stdfd2.stdfd.std_in  == pipefd1[0]);
   TEST(stdfd2.stdfd.std_out == pipefd1[1]);
   TEST(stdfd2.stdfd.std_err == pipefd2[1]);
   TEST(stdfd2.devnull       == iochannel_FREE);
   TEST(0 == redirectstdio_processstdio2(&stdfd2));
   TEST(1 == write(iochannel_STDOUT, "1", 1));
   TEST(1 == write(iochannel_STDERR, "2", 1));
   TEST(1 == read(pipefd1[0], buffer, sizeof(buffer)));
   TEST('1' == buffer[0]);
   TEST(1 == read(pipefd2[0], buffer, sizeof(buffer)));
   TEST('2' == buffer[0]);
   TEST(3 == write(pipefd1[1], "123", 3));
   TEST(3 == read(iochannel_STDIN, &buffer, sizeof(buffer)));
   TEST(0 == strncmp(buffer, "123", 3));
   TEST(0 == free_processstdio2(&stdfd2));
   TEST(stdfd2.stdfd.std_in  == pipefd1[0]);
   TEST(stdfd2.stdfd.std_out == pipefd1[1]);
   TEST(stdfd2.stdfd.std_err == pipefd2[1]);
   TEST(stdfd2.devnull       == iochannel_FREE);

   // TEST redirectstdio_processstdio2: inherit of closed fds
   stdfd = (process_stdio_t) process_stdio_INIT_INHERIT;
   TEST(0 == init_processstdio2(&stdfd2, &stdfd));
   for (int i = 0; i < 3; ++i) {
      int fd = i;
      TEST(0  == free_iochannel(&fd));
      TEST(-1 == fd);
   }
   TEST(0 == redirectstdio_processstdio2(&stdfd2));
   TEST(0 == free_processstdio2(&stdfd2));

   // restore stdio
   TEST(iochannel_STDIN  == dup2(oldstdfd[iochannel_STDIN], iochannel_STDIN));
   TEST(iochannel_STDOUT == dup2(oldstdfd[iochannel_STDOUT], iochannel_STDOUT));
   TEST(iochannel_STDERR == dup2(oldstdfd[iochannel_STDERR], iochannel_STDERR));
   for (unsigned i = 0; i < lengthof(oldstdfd); ++i) {
      TEST(0 == free_iochannel(&oldstdfd[i]));
   }
   TEST(0 == free_iochannel(&pipefd1[0]));
   TEST(0 == free_iochannel(&pipefd1[1]));
   TEST(0 == free_iochannel(&pipefd2[0]));
   TEST(0 == free_iochannel(&pipefd2[1]));

   return 0;
ONERR:
   (void) free_processstdio2(&stdfd2);
   for (unsigned i = 0; i < lengthof(oldstdfd); ++i) {
      if (-1 != oldstdfd[i]) {
         (void) dup2(oldstdfd[i], (int) i);
         (void) free_iochannel(&oldstdfd[i]);
      }
   }
   free_iochannel(&pipefd1[0]);
   free_iochannel(&pipefd1[1]);
   free_iochannel(&pipefd2[0]);
   free_iochannel(&pipefd2[1]);
   return EINVAL;
}

static int test_initfree(void)
{
   process_t         process = process_FREE;
   process_result_t  process_result;
   process_state_e   process_state;
   struct timespec   ts = { 0, 0 };
   sigset_t          signalmask;

   // prepare
   sigemptyset(&signalmask);
   sigaddset(&signalmask, SIGINT);

   // TEST static init
   TEST(sys_process_FREE == process);
   TEST(0 == sys_process_FREE);

   // TEST init_process, free_process
   TEST(0 == initgeneric_process(&process, &childprocess_return, (intptr_t)0, 0));
   TEST(0 <  process);
   TEST(0 == free_process(&process));
   TEST(0 == process);
   TEST(0 == free_process(&process));
   TEST(0 == process);

   // TEST state_process: process_state_RUNNABLE after init
   TEST(0 == initgeneric_process(&process, &childprocess_return, (intptr_t)0, 0));
   TEST(0 <  process);
   // test
   TEST( 0 == state_process(&process, &process_state));
   TEST( process_state_RUNNABLE == process_state);
   // reset
   TEST(0 == free_process(&process));

   for (unsigned i = 0; i <= 255; i += 5, i = (i == 20 ? 240 : i)) {
      // TEST wait_process
      TEST(0 == initgeneric_process(&process, &childprocess_return, (intptr_t) i, 0));
      TEST(0 <  process);
      TEST(0 == wait_process(&process, &process_result));
      TEST(process_result.state      == process_state_TERMINATED);
      TEST(process_result.returncode == (int) i);

      // TEST state_process
      process_state = process_state_RUNNABLE;
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state == process_state_TERMINATED);

      // TEST double wait_process => returns the same result
      process_result.state = process_state_RUNNABLE;
      process_result.returncode = -1;
      TEST(0 == wait_process(&process, &process_result));
      TEST(process_result.state      == process_state_TERMINATED);
      TEST(process_result.returncode == (int) i);
      TEST(0 <  process);

      // TEST state_process
      process_state = process_state_RUNNABLE;
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state == process_state_TERMINATED);

      // reset
      TEST(0 == free_process(&process));
      TEST(0 == process);
   }

   // TEST endless loop => delete ends process
   for (unsigned i = 0; i < 16; ++i) {
      while (SIGINT == sigtimedwait(&signalmask, 0, &ts));
      TEST(0 == init_process(&process, &childprocess_endlessloop, 0, 0));
      TEST(0 <  process);
      TEST(SIGINT == sigwaitinfo(&signalmask, 0));
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_RUNNABLE == process_state);
      TEST(0 == free_process(&process));
      TEST(0 == process);
   }

   // TEST state_process
   for (unsigned i = 0; i < 16; ++i) {
      while (SIGINT == sigtimedwait(&signalmask, 0, &ts));
      TEST(0 == init_process(&process, &childprocess_endlessloop, 0, 0));
      TEST(0 <  process);
      TEST(SIGINT == sigwaitinfo(&signalmask, 0));
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_RUNNABLE == process_state);
      kill(process, SIGSTOP);
      for (unsigned i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state));
         if (process_state_RUNNABLE != process_state) break;
         sleepms_thread(1);
      }
      TEST(process_state_STOPPED == process_state);
      kill(process, SIGCONT);
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_RUNNABLE == process_state);
      kill(process, SIGKILL);
      for (unsigned i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state));
         if (process_state_RUNNABLE != process_state) break;
         sleepms_thread(1);
      }
      TEST(process_state_ABORTED == process_state);
      TEST(0 == free_process(&process));
      TEST(0 == process);
   }

   // TEST ECHILD
   TEST(0 == initgeneric_process(&process, &childprocess_return, (intptr_t)0, 0));
   TEST(0 <  process);
   TEST(0 == wait_process(&process, 0));
   TEST(0 <  process);
   {
      process_t process2 = process;
      TEST(0 == free_process(&process2));
   }
   TEST(ECHILD == state_process(&process, &process_state));
   TEST(0 <  process);
   TEST(ECHILD == wait_process(&process, 0));
   TEST(0 <  process);
   TEST(ECHILD == free_process(&process));
   TEST(0 == process);

   return 0;
ONERR:
   (void) free_process(&process);
   return EINVAL;
}

static int test_abnormalexit(void)
{
   process_t               process   = process_FREE;
   process_state_e         process_state;
   process_result_t        process_result;

   // TEST wait_process, state_process: process_state_ABORTED (or process_state_TERMINATED)
   int test_signals[] = {
       SIGHUP,    SIGINT,   SIGQUIT,  SIGILL,  SIGTRAP
      ,SIGABRT,   SIGBUS,   SIGFPE,   SIGKILL, SIGUSR1
      ,SIGSEGV,   SIGUSR2,  SIGPIPE,  SIGALRM, SIGTERM
      ,SIGSTKFLT, SIGCHLD,  SIGCONT,  SIGSTOP, SIGTSTP
      ,SIGTTIN,   SIGTTOU,  SIGURG,   SIGXCPU, SIGXFSZ
      ,SIGVTALRM, SIGPROF,  SIGWINCH, SIGIO,   SIGPWR
      ,SIGSYS,    SIGRTMIN, SIGRTMAX,
   };
   unsigned signal_count = 0;
   for (unsigned i = 0; i < lengthof(test_signals); ++i) {
      intptr_t snr = test_signals[i];
      TEST(0 == initgeneric_process(&process, &childprocess_signal, snr, 0));

      // TEST wait_process: process_state_ABORTED (or process_state_TERMINATED)
      TEST(0 == wait_process(&process, &process_result));
      if (process_state_ABORTED == process_result.state) {
         ++ signal_count;
         TEST(process_result.returncode == snr);
      } else {
         // signal ignored
         TEST(process_result.state      == process_state_TERMINATED);
         TEST(process_result.returncode == 0);
      }

      // TEST state_process: process_state_ABORTED (or process_state_TERMINATED)
      process_state = (process_state_e) -1;
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state == process_result.state);

      // reset
      TEST(0 == free_process(&process));
      TEST(0 == process);
   }
   TEST(signal_count > lengthof(test_signals)/2);

   // TEST free_process: works if process has already ended
   for (unsigned i = 0; i < 16; ++i) {
      kill( getpid(), SIGINT); // clear SIGINT
      suspend_process();       // clear SIGINT
      TEST(0 == initgeneric_process(&process, &childprocess_signal, (intptr_t)SIGKILL, 0));
      // wait until child has started
      suspend_process();
      for (int i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state));
         if (process_state_ABORTED == process_state) break;
         sleepms_thread(1);
      }
      // check process_state_ABORTED
      for (unsigned r = 0; r < 2; ++r) {
         TEST(0 == state_process(&process, &process_state));
         TEST(process_state_ABORTED == process_state);
      }
      TEST(0 == free_process(&process));
      TEST(0 == process);

      kill( getpid(), SIGINT); // clear SIGINT
      suspend_process();       // clear SIGINT
      TEST(0 == initgeneric_process(&process, &childprocess_signal, (intptr_t)SIGKILL, 0));
      suspend_process();
      // do not query state before
      TEST(0 == free_process(&process));
      TEST(0 == process);
   }

   return 0;
ONERR:
   (void) free_process(&process);
   return EINVAL;
}

static int test_assert(void)
{
   process_t            process = process_FREE;
   process_result_t     process_result;

   // TEST assert exits with signal SIGABRT
   TEST(0 == init_process(&process, &chilprocess_execassert, 0, 0));
   TEST(0 == wait_process(&process, &process_result));
   TEST(process_state_ABORTED == process_result.state);
   TEST(SIGABRT               == process_result.returncode);
   TEST(0 == free_process(&process));

   // TEST stdfd failure => assert !
   process_stdio_t stdfd = process_stdio_INIT_DEVNULL;
   {
      int pipefd1[2];  // necessary so that pipefd2[0] is not same as devnull
      int pipefd2[2];
      TEST(0 == pipe2(pipefd1,O_CLOEXEC|O_NONBLOCK));
      TEST(0 == pipe2(pipefd2,O_CLOEXEC|O_NONBLOCK));
      redirectin_processstdio(&stdfd, pipefd2[0]);
      TEST(0 == free_iochannel(&pipefd1[0]));
      TEST(0 == free_iochannel(&pipefd1[1]));
      TEST(0 == free_iochannel(&pipefd2[0]));
      TEST(0 == free_iochannel(&pipefd2[1]));
   }
   TEST(0 == init_process(&process, &childprocess_donothing, 0, &stdfd));
   TEST(0 == wait_process(&process, &process_result));
   TEST(process_state_ABORTED == process_result.state);
   TEST(SIGABRT               == process_result.returncode);
   TEST(0 == free_process(&process));

   return 0;
ONERR:
   (void) free_process(&process);
   return EINVAL;
}

static int test_statequery(void)
{
   process_t         process = process_FREE;
   pipe_t            pipefd  = pipe_FREE;
   process_state_e   process_state;
   process_result_t  process_result;

   TEST(0 == init_pipe(&pipefd));

   for (unsigned i = 0; i < 4; ++i) {

      // use wait_process (to end process)
      kill( getpid(), SIGINT); // clear SIGINT
      suspend_process();       // clear SIGINT
      TEST(0 == initgeneric_process(&process, &childprocess_signal, (intptr_t)SIGSTOP, 0));
      // wait until child has started
      suspend_process();
      for (unsigned i2 = 0; i2 < 1000; ++i2) {
         TEST(0 == state_process(&process, &process_state));
         if (process_state_STOPPED == process_state) break;
         sleepms_thread(1);
      }
      // TEST process_state_STOPPED
      TEST(process_state_STOPPED == process_state);
      process_state = process_state_TERMINATED;
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_STOPPED == process_state);
      // TEST wait_process continues stopped child
      memset(&process_result, 0xff, sizeof(process_result));
      TEST(0 == wait_process(&process, &process_result));
      TEST(process_result.state      == process_state_TERMINATED);
      TEST(process_result.returncode == 0);
      // TEST process_state_TERMINATED
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_TERMINATED  == process_state);
      TEST(0 == process_result.returncode);
      TEST(0 == free_process(&process));

      // use free_process (to end process)
      kill( getpid(), SIGINT); // clear SIGINT
      suspend_process();       // clear SIGINT
      TEST(0 == initgeneric_process(&process, &childprocess_signal, (intptr_t)SIGSTOP, 0));
      // wait until child has started
      suspend_process();
      for (unsigned i2 = 0; i2 < 1000; ++i2) {
         TEST(0 == state_process(&process, &process_state));
         if (process_state_STOPPED == process_state) break;
         sleepms_thread(1);
      }
      TEST(process_state_STOPPED == process_state);
      process_state = process_state_RUNNABLE;
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_STOPPED == process_state);
      TEST(0 == free_process(&process));
      TEST(0 == process);
   }

   // TEST state_process: returns latest state
   kill( getpid(), SIGINT); // clear SIGINT
   suspend_process();       // clear SIGINT
   TEST(0 == initgeneric_process(&process, &childprocess_statechange, (intptr_t)pipefd.write, 0));
   // wait until child has started
   suspend_process();
   {
      char buffer[50] = { 0 };
      TEST(0 <= read(pipefd.read, buffer, sizeof(buffer)-1));
      TEST(0 == strcmp(buffer, "sleep\n"));
   }
   sleepms_thread(10);
   // -- process_state_STOPPED
   for (unsigned i = 0; i < 2; ++i) {
      process_state = process_state_RUNNABLE;
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_STOPPED == process_state);
   }
   TEST(0 == kill(process, SIGCONT));
   // wait until child run again
   suspend_process();
   {
      char buffer[50] = { 0 };
      TEST(0 <= read(pipefd.read, buffer, sizeof(buffer)-1));
      TEST(0 == strcmp(buffer, "run\n"));
   }
   // -- process_state_RUNNABLE
   for (unsigned i = 0; i < 2; ++i) {
      process_state = process_state_STOPPED;
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_RUNNABLE == process_state);
   }
   TEST(0 == kill(process, SIGKILL));
   sleepms_thread(10);
   // -- process_state_ABORTED
   for (unsigned i = 0; i < 2; ++i) {
      process_state = process_state_STOPPED;
      TEST(0 == state_process(&process, &process_state));
      TEST(process_state_ABORTED == process_state);
   }

   // reset
   TEST(0 == free_process(&process));
   TEST(0 == free_pipe(&pipefd));

   return 0;
ONERR:
   (void) free_process(&process);
   (void) free_pipe(&pipefd);
   return EINVAL;
}

static int test_exec(void)
{
   process_t            process = process_FREE;
   process_result_t     process_result;
   process_stdio_t      stdfd   = process_stdio_INIT_DEVNULL;
   pipe_t               pipefd  = pipe_FREE;
   struct stat          statbuf;
   char                 numberstr[20];
   const char         * testcase1_args[] = { "bin/testchildprocess", "1", numberstr, 0 };
   const char         * testcase2_args[] = { "bin/testchildprocess", "2", 0 };
   const char         * testcase3_args[] = { "bin/testchildprocess", "3", 0 };
   char                 readbuffer[32]   = { 0 };

   // prepare
   TEST(0 == init_pipe(&pipefd));
   if (0 != stat("bin/testchildprocess", &statbuf)) {
      testcase1_args[0] = "bin/testchildprocess_Debug";
      testcase2_args[0] = "bin/testchildprocess_Debug";
      testcase3_args[0] = "bin/testchildprocess_Debug";
   }

   // TEST executing child process return value (case1)
   for (unsigned i = 0; i <= 35; i += 7) {
      snprintf(numberstr, sizeof(numberstr), "%u", i);
      TEST(0 == initexec_process(&process, testcase1_args[0], testcase1_args, 0));
      TEST(0 == wait_process(&process, &process_result));
      TEST(process_state_TERMINATED == process_result.state);
      TEST(i == (unsigned) process_result.returncode);
      TEST(0 == free_process(&process));
   }

   // TEST initexec_process: open file descriptors (case2)
   for (unsigned i = 1; i <= 3; ++i) {
      stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL;
      redirecterr_processstdio(&stdfd, pipefd.write);
      if (i > 1) redirectin_processstdio(&stdfd, STDIN_FILENO);
      if (i > 2) redirectout_processstdio(&stdfd, STDOUT_FILENO);
      TEST(0 == initexec_process(&process, testcase2_args[0], testcase2_args, &stdfd));
      TEST(0 == wait_process(&process, &process_result));
      TEST(process_result.state      == process_state_TERMINATED);
      TEST(process_result.returncode == 0);
      TEST(0 == free_process(&process));
      MEMSET0(&readbuffer);
      TEST(0 < read(pipefd.read, readbuffer, sizeof(readbuffer)));
      TEST(2 >= strlen(readbuffer));
      // either 3 or more files (X11 opens some fds which are inherited)
      TEST(3 <= atoi(readbuffer));
   }

   // TEST name_process (case 3)
   stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL;
   redirecterr_processstdio(&stdfd, pipefd.write);
   TEST(0 == initexec_process(&process, testcase3_args[0], &testcase3_args[0], &stdfd));
   TEST(0 == wait_process(&process, &process_result));
   TEST(process_state_TERMINATED == process_result.state);
   TEST(0 == process_result.returncode);
   TEST(0 == free_process(&process));
   MEMSET0(&readbuffer);
   TEST(0 < read(pipefd.read, readbuffer, sizeof(readbuffer)));
   TEST(0 == strncmp(readbuffer, testcase3_args[0]+4, 15));

   // unprepare
   TEST(0 == free_pipe(&pipefd));

   return 0;
ONERR:
   free_pipe(&pipefd);
   (void) free_process(&process);
   return EINVAL;
}

static int daemonprocess_return(void * dummy)
{
   (void) dummy;
   (void) daemonize_process(0);
   return -1;
}

static int daemonprocess_redirect(process_stdio_t * stdfd)
{
   char buffer[10] = { 0 };

   pid_t oldsid = getsid(0);

   int err = daemonize_process(stdfd);
   if (err) return err;

   if (  getpid()  != getsid(0)
         || oldsid == getsid(0)) {
      return EINVAL;
   }

   if (  5 != read(STDIN_FILENO, buffer, sizeof(buffer))
         || 5 != write(STDOUT_FILENO, buffer, 5)) {
      return EINVAL;
   }

   if (0 == getcwd(buffer, sizeof(buffer))) {
      return EINVAL;
   }

   const size_t dirsiz = strlen(buffer)+1;

   if (dirsiz != (size_t) write(STDERR_FILENO, buffer, dirsiz)) {
      return EINVAL;
   }

   return 0;
}

static int test_daemon(void)
{
   process_t            process = process_FREE;
   process_result_t     process_result;
   process_stdio_t      stdfd = process_stdio_INIT_DEVNULL;
   pipe_t               pipe[2] = { pipe_FREE, pipe_FREE };
   char                 readbuffer[20];

   // prepare
   for (unsigned i = 0; i < lengthof(pipe); ++i) {
      TEST(0 == init_pipe(&pipe[i]));
   }

   // TEST daemonize_process: return always 0 cause daemonize_process creates new child with fork
   TEST(0 == init_process(&process, &daemonprocess_return, 0, 0));
   TEST(0 == wait_process(&process, &process_result));
   TEST(process_result.state      == process_state_TERMINATED);
   TEST(process_result.returncode == 0);
   TEST(0 == free_process(&process));

   // TEST daemonize_process: redirect stdfd
   stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL;
   redirectin_processstdio(&stdfd, pipe[0].read);
   redirectout_processstdio(&stdfd, pipe[1].write);
   redirecterr_processstdio(&stdfd, pipe[1].write);
   TEST(0 == writeall_pipe(&pipe[0], 5, "12345", -1));
   TEST(0 == initgeneric_process(&process, &daemonprocess_redirect, &stdfd, 0));
   TEST(0 == wait_process(&process, &process_result));
   TEST(process_result.state      == process_state_TERMINATED);
   TEST(process_result.returncode == 0);
   TEST(0 == readall_pipe(&pipe[1], 5, readbuffer, -1));
   TEST(0 == strncmp(readbuffer, "12345", 5));
   TEST(0 == readall_pipe(&pipe[1], 2, readbuffer, -1));
   TEST(0 == strcmp(readbuffer, "/"));
   TEST(0 == free_process(&process));

   // reset
   for (unsigned i = 0; i < lengthof(pipe); ++i) {
      TEST(0 == free_pipe(&pipe[i]));
   }

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(pipe); ++i) {
      free_pipe(&pipe[i]);
   }
   (void) free_process(&process);
   return EINVAL;
}

int unittest_platform_task_process()
{
   if (test_processresult())  goto ONERR;
   if (test_redirect())       goto ONERR;
   if (test_redirect2())      goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_abnormalexit())   goto ONERR;
   if (test_assert())         goto ONERR;
   if (test_statequery())     goto ONERR;
   if (test_exec())           goto ONERR;
   if (test_daemon())         goto ONERR;

   // adapt LOG buffer ("pid=1234" replaces with "pid=?")
   uint8_t *logbuffer = 0;
   size_t   logsize   = 0;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   char buffer2[2000] = { 0 };
   TEST(logsize < sizeof(buffer2));
   logsize = 0;
   while (strstr((char*)logbuffer, "\npid=")) {
      memcpy(&buffer2[logsize], logbuffer, (size_t) (strstr((char*)logbuffer, "\npid=") - (char*)logbuffer));
      logsize += (size_t) (strstr((char*)logbuffer, "\npid=") - (char*)logbuffer);
      strcpy(&buffer2[logsize], "\npid=?");
      logsize  += strlen("\npid=?");
      logbuffer = (uint8_t*)strstr(strstr((char*)logbuffer, "\npid=")+1, "\n");
   }
   strcpy(&buffer2[logsize], (char*)logbuffer);

   CLEARBUFFER_ERRLOG();
   PRINTF_ERRLOG("%s", buffer2);

   return 0;
ONERR:
   return EINVAL;
}

#endif
