/* title: Process Linuximpl
   Implements <Process>.

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

   file: C-kern/api/platform/task/process.h
    Header file of <Process>.

   file: C-kern/platform/Linux/task/process.c
    Implementation file <Process Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/task/process.h"
#include "C-kern/api/err.h"
// #include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/task/thread.h"
#endif


typedef struct childprocess_exec_t        childprocess_exec_t ;

typedef struct process_stdfd2_t           process_stdfd2_t ;

enum queryoption_e {
   queryoption_NOWAIT,
   queryoption_WAIT,
   queryoption_WAIT_AND_FREE
} ;

typedef enum queryoption_e                queryoption_e ;

struct childprocess_exec_t {
   const char *   filename ;
   char      **   arguments ;
   iochannel_t    errpipe ;
} ;

struct process_stdfd2_t {
   process_stdio_t   stdfd ;
   iochannel_t       devnull ;
} ;



// section: process_stdfd2_t

// group: lifetime

#define process_stdfd2_INIT_FREEABLE      { process_stdio_INIT_DEVNULL, iochannel_INIT_FREEABLE }

/* function: init_processstdio2
 * Initializes <process_stdfd2_t> with <process_stdio_t> and opens devnull.
 * The device null is only opened if stdfd is 0 or at least one file descriptor
 * is set to <iochannel_INIT_FREEABLE>. */
static int init_processstdio2(/*out*/process_stdfd2_t * stdfd2, process_stdio_t * stdfd)
{
   int err ;
   int devnull = -1 ;

   if (  !stdfd
      || iochannel_INIT_FREEABLE == stdfd->std_in
      || iochannel_INIT_FREEABLE == stdfd->std_out
      || iochannel_INIT_FREEABLE == stdfd->std_err ) {
      devnull = open("/dev/null", O_RDWR|O_CLOEXEC) ;
      if (-1 == devnull) {
         err = errno ;
         TRACESYSCALL_ERRLOG("open(/dev/null,O_RDWR)", err) ;
         goto ONABORT ;
      }
   }

   // return out param stdfd2
   if (stdfd) {
      stdfd2->stdfd = *stdfd ;
   } else {
      stdfd2->stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL ;
   }
   stdfd2->devnull = devnull ;

   return 0 ;
ONABORT:
   free_iochannel(&devnull) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

/* function: free_processstdio2
 * Closes if devnull if necessary. */
static int free_processstdio2(process_stdfd2_t * stdfd2)
{
   int err ;

   err = free_iochannel(&stdfd2->devnull) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

/* function: redirectstdfd_processstdio2
 * Redirects one standard channel to read/write from/to a file.
 *
 * Parameter:
 * stdfd           - The file descriptor of the standard io channel.
 *                   Set this value to one of STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
 * redirectto_file - The file descriptor of the file which now becomes the new standard io channel.
 *                   Use value <iochannel_INIT_FREEABLE> to redirect to devnull.
 *                   Use same value as stdfd if the standard channel should be
 *                   inherited between processes.
 * */
static int redirectstdfd_processstdio2(const process_stdfd2_t * stdfd2, int stdfd, int redirectto_file)
{
   int err ;
   int fd = redirectto_file ;

   if (stdfd != fd) {
      if (iochannel_INIT_FREEABLE == fd) {
         fd = stdfd2->devnull ;
      }
      while (-1 == dup2(fd, stdfd)) {
         if (EINTR != errno) {
            err = errno ;
            TRACESYSCALL_ERRLOG("dup2(fd, stdfd)", err) ;
            PRINTINT_ERRLOG(fd) ;
            PRINTINT_ERRLOG(stdfd) ;
            goto ONABORT ;
         }
      }
      // dup2: FD_CLOEXEC is cleared
   } else {
      // clear FD_CLOEXEC
      // ignore error in case stdfd is closed
      (void) fcntl(stdfd, F_SETFD, (long)0) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

/* function: redirectstdio_processstdio2
 * Redirects all 3 standard io channels to read/write from/to a file.
 *
 * If the new file descriptors have the same values as the standard channels
 * then no redirection is done but they are inherited.
 *
 * Uses <redirectstdfd_processstdio2> to implement its functionality. */
static int redirectstdio_processstdio2(const process_stdfd2_t * stdfd2)
{
   int err ;
   int err2 ;

   err = redirectstdfd_processstdio2(stdfd2, STDIN_FILENO, stdfd2->stdfd.std_in) ;

   err2 = redirectstdfd_processstdio2(stdfd2, STDOUT_FILENO, stdfd2->stdfd.std_out) ;
   if (err2) err = err2 ;

   err2 = redirectstdfd_processstdio2(stdfd2, STDERR_FILENO, stdfd2->stdfd.std_err) ;
   if (err2) err = err2 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// section: process_t

// group: helper

static int queryresult_process(sys_process_t pid, /*out*/process_result_t * result, queryoption_e option)
{
   int err ;
   siginfo_t info ;
   int       flags ;

#define FLAGS (WEXITED|WSTOPPED)

   switch(option) {
   case queryoption_NOWAIT:
      flags = FLAGS|WNOHANG|WNOWAIT ;
      break ;
   case queryoption_WAIT:
      flags = FLAGS|WNOWAIT ;
      break ;
   case queryoption_WAIT_AND_FREE:
      flags = FLAGS ;
      break ;
   default:
      VALIDATE_INPARAM_TEST(option == queryoption_WAIT_AND_FREE, ONABORT, PRINTINT_ERRLOG(option)) ;
      flags = FLAGS ;
      break ;
   }

#undef FLAGS

   info.si_pid = 0 ;
   while (-1 == waitid(P_PID, (id_t) pid, &info, flags)) {
      if (EINTR != errno) {
         err = errno ;
         TRACESYSCALL_ERRLOG("waitid",err) ;
         PRINTINT_ERRLOG(pid) ;
         goto ONABORT ;
      }
   }


   if (pid != info.si_pid) {
      result->state = process_state_RUNNABLE ;
      return 0 ;
   }

   if (CLD_EXITED == info.si_code) {
      result->state = process_state_TERMINATED ;
      result->returncode = info.si_status ;
   } else if (    CLD_KILLED == info.si_code
               || CLD_DUMPED == info.si_code) {
      result->state = process_state_ABORTED ;
      result->returncode = info.si_status ;
   } else if (    CLD_STOPPED == info.si_code
               || CLD_TRAPPED == info.si_code) {
      result->state = process_state_STOPPED ;
   } else {
      result->state = process_state_RUNNABLE ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int childprocess_exec(childprocess_exec_t  * execparam)
{
   int err ;
   int write_err ;

   execvp( execparam->filename, execparam->arguments ) ;
   err = errno ;

// ONABORT:

   do {
      write_err = write( execparam->errpipe, &err, sizeof(&err)) ;
   } while (-1 == write_err && errno == EINTR) ;

   return err ;
}

// group: lifetime

int initexec_process(/*out*/process_t * process, const char * filename, const char * const * arguments, process_stdio_t * stdfd/*0 => /dev/null*/)
{
   int err ;
   process_t            childprocess = process_INIT_FREEABLE ;
   int                  pipefd[2]    = { -1, -1 } ;
   childprocess_exec_t  execparam    = { filename, (char**)(uintptr_t)arguments, -1 } ;

   if ( pipe2(pipefd,O_CLOEXEC) ) {
      err = errno ;
      TRACESYSCALL_ERRLOG("pipe2", err) ;
      goto ONABORT ;
   }

   execparam.errpipe = pipefd[1] ;

   err = initgeneric_process(&childprocess, &childprocess_exec, &execparam, stdfd) ;
   if (err) goto ONABORT ;

   // CHECK exec error
   err = free_iochannel(&pipefd[1]) ;
   assert(!err) ;

   ssize_t read_bytes = 0 ;

   int exec_err = 0 ;

   do {
      read_bytes = read( pipefd[0], &exec_err, sizeof(exec_err)) ;
   } while (-1 == read_bytes && EINTR == errno) ;

   if (-1 == read_bytes) {
      err = errno ;
      TRACESYSCALL_ERRLOG("read", err) ;
      goto ONABORT ;
   } else if (read_bytes) {
      // EXEC error
      err = exec_err ? exec_err : ENOEXEC ;
      TRACESYSCALL_ERRLOG("execvp(filename, arguments)", err) ;
      PRINTCSTR_ERRLOG(filename) ;
      for (size_t i = 0; arguments[i]; ++i) {
         PRINTARRAYFIELD_ERRLOG("s",arguments,i) ;
      }
      goto ONABORT ;
   }

   err = free_iochannel(&pipefd[0]) ;
   if (err) goto ONABORT ;

   *process = childprocess ;

   return 0 ;
ONABORT:
   free_iochannel(&pipefd[1]) ;
   free_iochannel(&pipefd[0]) ;
   (void) free_process(&childprocess) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

/* function: fork_process
 * This function call returns twice.
 * It creates a new child process whihc returns from this with pid set to 0.
 * The calling process (parent process) gets the child's process returned in pid. */
static int fork_process(/*out*/pid_t * pid)
{
   int err ;

   // MULTITHREAD-PRECONDITION: all files opened with O_CLOEXEC ; => test/static/close_on_exec.sh

   // TODO: MULTITHREAD: system handler for preparing fork
   pid_t newpid = fork() ;
   if (-1 == newpid) {
      err = errno ;
      TRACESYSCALL_ERRLOG("fork", err) ;
      goto ONABORT ;
   }

   *pid = newpid ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int preparechild_process(process_stdio_t * stdfd/*0 => /dev/null*/)
{
   int err ;
   process_stdfd2_t stdfd2 = process_stdfd2_INIT_FREEABLE ;

   // TODO: MULTITHREAD: system handler for freeing thread resources (i.e. clear log, ...) ?!?

   err = init_processstdio2(&stdfd2, stdfd) ;
   if (err) goto ONABORT ;

   err = redirectstdio_processstdio2(&stdfd2) ;
   if (err) goto ONABORT ;

   err = free_processstdio2(&stdfd2) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   free_processstdio2(&stdfd2) ;
   return err ;
}

int init_process(/*out*/process_t   *  process,
                  process_task_f       child_main,
                  void              *  start_arg,
                  process_stdio_t   *  stdfd/*0 => /dev/null*/)
{
   int err ;
   pid_t pid = 0 ;

   err = fork_process(&pid) ;
   if (err) goto ONABORT ;

   if (0 == pid) {
      // NEW CHILD PROCESS
      err = preparechild_process(stdfd) ;
      assert(!err) ;
      int returncode = (child_main ? child_main(start_arg) : 0) ;
      exit(returncode) ;
   }

   *process = pid ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int initdaemon_process(/*out*/process_t * process,
                       process_task_f     child_main,
                       void             * start_arg,
                       process_stdio_t  * stdfd/*0 => /dev/null*/)
{
   int err ;
   pid_t pid = 0 ;

   err = fork_process(&pid) ;
   if (err) goto ONABORT ;

   if (0 == pid) {
      // NEW CHILD DAEMON PROCESS
      err = preparechild_process(stdfd) ;
      if (setsid() == (pid_t)-1) {
         err = errno ;
      }
      umask(S_IROTH|S_IWOTH|S_IXOTH) ;
      if (0 != chdir("/")) {
         err = errno ;
      }
      assert(!err) ;
      int returncode = (child_main ? child_main(start_arg) : 0) ;
      exit(returncode) ;
   }

   *process = pid ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_process(process_t * process)
{
   int err ;
   pid_t pid = *process ;

   static_assert(0 == sys_process_INIT_FREEABLE, "0 is no valid process id") ;

   if (pid) {

      *process = sys_process_INIT_FREEABLE ;

      kill(pid, SIGKILL) ;

      process_result_t result ;
      err = queryresult_process(pid, &result, queryoption_WAIT_AND_FREE) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

int name_process(size_t namebuffer_size, /*out*/char name[namebuffer_size], /*out*/size_t * name_size)
{
   int err ;
   char buffer[16+1] ;

   buffer[16] = 0 ;

   err = prctl(PR_GET_NAME, buffer, 0, 0, 0) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("prctl(PR_GET_NAME)", err) ;
      goto ONABORT ;
   }

   size_t size = 1 + strlen(buffer) ;

   if (name_size) {
      *name_size = size ;
   }

   if (namebuffer_size) {
      if (namebuffer_size >= size) {
         memcpy( name, buffer, size) ;
      } else {
         memcpy( name, buffer, namebuffer_size-1) ;
         name[namebuffer_size-1] = 0 ;
      }
   }

   return 0 ;
ONABORT:
   return err ;
}

int state_process(process_t * process, /*out*/process_state_e * current_state)
{
   int err ;
   process_result_t result ;

   err = queryresult_process(*process, &result, queryoption_NOWAIT) ;
   if (err) goto ONABORT ;

   *current_state = result.state ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: change

int wait_process(process_t * process, /*out*/process_result_t * result)
{
   int err ;
   pid_t pid = *process ;

   kill(pid, SIGCONT) ;

   for (;;) {
      process_result_t state ;

      err = queryresult_process(pid, &state, queryoption_WAIT) ;
      if (err) goto ONABORT ;

      switch(state.state) {
      case process_state_RUNNABLE:
         break ;
      case process_state_STOPPED:
         kill(pid, SIGCONT) ;
         break ;
      case process_state_TERMINATED:
      case process_state_ABORTED:
         if (result) {
            *result = state ;
         }
         return 0 ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}



// section: test

#ifdef KONFIG_UNITTEST

static int childprocess_return(intptr_t returncode)
{
   kill(getppid(), SIGUSR1) ;
   return (int) returncode ;
}

static int childprocess_endlessloop(void * dummy)
{
   (void) dummy ;
   kill(getppid(), SIGUSR1) ;
   for (;;) {
      sleepms_thread(1000) ;
   }
   return 0 ;
}

static int childprocess_signal(intptr_t signr)
{
   kill(getpid(), (int)signr) ;
   return 0 ;
}

static int chilprocess_execassert(void * dummy)
{
   (void) dummy ;
   // flushing of log output is redirected to devnull (from caller)
   assert(0) ;
   return 0 ;
}

static int childprocess_donothing(void * dummy)
{
   (void) dummy ;
   return 0 ;
}

static int childprocess_statechange(int fd)
{
   dprintf(fd,"sleep\n") ;
   kill( getpid(), SIGSTOP) ;
   dprintf(fd,"run\n") ;
   for (;;) {
      sleepms_thread(1000) ;
   }
   return 0 ;
}

static int childprocess_daemon(void * dummy)
{
   char buffer[20];
   char dir[10] = { 0 } ;

   (void) dummy ;

   if (  5 != read(STDIN_FILENO, buffer, 5)
         || 5 != write(STDOUT_FILENO, buffer, 5)) {
      return EINVAL ;
   }

   if (0 == getcwd(dir, sizeof(dir))) {
      return EINVAL ;
   }

   snprintf(buffer, sizeof(buffer), "%d:%s", getsid(0), dir) ;

   if ((int)strlen(buffer)+1 != write(STDERR_FILENO, buffer, strlen(buffer)+1)) {
      return EINVAL ;
   }

   return 0 ;
}

static int test_redirect(void)
{
   process_stdio_t  stdfd  = { 0, 0, 0 } ;

   // TEST static init: process_stdio_INIT_DEVNULL
   stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL ;
   TEST(-1 == stdfd.std_in) ;
   TEST(-1 == stdfd.std_out) ;
   TEST(-1 == stdfd.std_err) ;
   TEST(iochannel_INIT_FREEABLE == stdfd.std_in) ;
   TEST(iochannel_INIT_FREEABLE == stdfd.std_out) ;
   TEST(iochannel_INIT_FREEABLE == stdfd.std_err) ;

   // TEST static init: process_stdio_INIT_INHERIT
   stdfd = (process_stdio_t) process_stdio_INIT_INHERIT ;
   TEST(0 == stdfd.std_in) ;
   TEST(1 == stdfd.std_out) ;
   TEST(2 == stdfd.std_err) ;
   TEST(STDIN_FILENO  == stdfd.std_in) ;
   TEST(STDOUT_FILENO == stdfd.std_out) ;
   TEST(STDERR_FILENO == stdfd.std_err) ;
   TEST(iochannel_STDIN  == stdfd.std_in) ;
   TEST(iochannel_STDOUT == stdfd.std_out) ;
   TEST(iochannel_STDERR == stdfd.std_err) ;

   // TEST redirectin_processstdio, redirectout_processstdio, redirecterr_processstdio
   for (int i = 0; i < 100; ++i) {
      stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL ;
      TEST(iochannel_INIT_FREEABLE == stdfd.std_in) ;
      TEST(iochannel_INIT_FREEABLE == stdfd.std_out) ;
      TEST(iochannel_INIT_FREEABLE == stdfd.std_err) ;
      redirectin_processstdio(&stdfd, i) ;
      TEST(i == stdfd.std_in) ;
      TEST(iochannel_INIT_FREEABLE == stdfd.std_out) ;
      TEST(iochannel_INIT_FREEABLE == stdfd.std_err) ;
      redirectout_processstdio(&stdfd, i+1) ;
      TEST(i == stdfd.std_in) ;
      TEST(i == stdfd.std_out-1) ;
      TEST(iochannel_INIT_FREEABLE == stdfd.std_err) ;
      redirecterr_processstdio(&stdfd, i+2) ;
      TEST(i == stdfd.std_in) ;
      TEST(i == stdfd.std_out-1) ;
      TEST(i == stdfd.std_err-2) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_redirect2(void)
{
   process_stdfd2_t  stdfd2      = process_stdfd2_INIT_FREEABLE ;
   int               oldstdfd[3] = { [STDIN_FILENO] = -1, [STDOUT_FILENO] = -1, [STDERR_FILENO] = -1 } ;
   int               pipefd1[2]  = { -1, -1 } ;
   int               pipefd2[2]  = { -1, -1 } ;
   char              buffer[10]  = { 0 } ;
   process_stdio_t   stdfd ;

   // TEST static init
   TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_in) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_out) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_err) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.devnull) ;

   // TEST init_processstdio2: stdfd== 0
   MEMSET0(&stdfd2) ;
   stdfd2.devnull = iochannel_INIT_FREEABLE ;
   TEST(0 == init_processstdio2(&stdfd2, 0)) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_in) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_out) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_err) ;
   TEST(iochannel_INIT_FREEABLE != stdfd2.devnull) ;

   // TEST free_processstdio2
   TEST(0 == free_processstdio2(&stdfd2)) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.devnull) ;
   TEST(0 == free_processstdio2(&stdfd2)) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.devnull) ;

   // TEST init_processstdio2: process_stdio_INIT_INHERIT
   stdfd = (process_stdio_t) process_stdio_INIT_INHERIT ;
   stdfd2.devnull = -2 ;
   TEST(0 == init_processstdio2(&stdfd2, &stdfd)) ;
   TEST(iochannel_STDIN  == stdfd2.stdfd.std_in) ;
   TEST(iochannel_STDOUT == stdfd2.stdfd.std_out) ;
   TEST(iochannel_STDERR == stdfd2.stdfd.std_err) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.devnull) ;
   TEST(0 == free_processstdio2(&stdfd2)) ;
   TEST(iochannel_INIT_FREEABLE == stdfd2.devnull) ;

   // TEST init_processstdio2: one fd is set to devnull
   for (int i = 0; i < 3; ++i) {
      stdfd = (process_stdio_t) process_stdio_INIT_INHERIT ;
      stdfd2.devnull = iochannel_INIT_FREEABLE ;
      switch(i) { // set one std fd to devnull
      case 0:  redirectin_processstdio(&stdfd, iochannel_INIT_FREEABLE) ; break ;
      case 1:  redirectout_processstdio(&stdfd, iochannel_INIT_FREEABLE) ; break ;
      case 2:  redirecterr_processstdio(&stdfd, iochannel_INIT_FREEABLE) ; break ;
      }
      TEST(0 == init_processstdio2(&stdfd2, &stdfd)) ;
      if (i == 0) {
         TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_in) ;
      } else {
         TEST(iochannel_STDIN == stdfd2.stdfd.std_in) ;
      }
      if (i == 1) {
         TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_out) ;
      } else {
         TEST(iochannel_STDOUT == stdfd2.stdfd.std_out) ;
      }
      if (i == 2) {
         TEST(iochannel_INIT_FREEABLE == stdfd2.stdfd.std_err) ;
      } else {
         TEST(iochannel_STDERR == stdfd2.stdfd.std_err) ;
      }
      TEST(iochannel_INIT_FREEABLE != stdfd2.devnull) ;
      TEST(0 == free_processstdio2(&stdfd2)) ;
      TEST(iochannel_INIT_FREEABLE == stdfd2.devnull) ;
      TEST(0 == free_processstdio2(&stdfd2)) ;
      TEST(iochannel_INIT_FREEABLE == stdfd2.devnull) ;
   }

   // store old stdio
   for (int i = 0; i < 3; ++i) {
      oldstdfd[i] = dup(i) ;
      TEST(-1 != oldstdfd[i]) ;
   }
   TEST(0 == pipe2(pipefd1, O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 == pipe2(pipefd2, O_CLOEXEC|O_NONBLOCK)) ;

   // TEST redirectstdio_processstdio2
   stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL ;
   redirectin_processstdio(&stdfd, pipefd1[0]) ;
   redirectout_processstdio(&stdfd, pipefd1[1]) ;
   redirecterr_processstdio(&stdfd, pipefd2[1]) ;
   TEST(0 == init_processstdio2(&stdfd2, &stdfd)) ;
   TEST(stdfd2.stdfd.std_in  == pipefd1[0]) ;
   TEST(stdfd2.stdfd.std_out == pipefd1[1]) ;
   TEST(stdfd2.stdfd.std_err == pipefd2[1]) ;
   TEST(stdfd2.devnull       == iochannel_INIT_FREEABLE) ;
   TEST(0 == redirectstdio_processstdio2(&stdfd2)) ;
   TEST(1 == write(iochannel_STDOUT, "1", 1)) ;
   TEST(1 == write(iochannel_STDERR, "2", 1)) ;
   TEST(1 == read(pipefd1[0], buffer, sizeof(buffer))) ;
   TEST('1' == buffer[0]) ;
   TEST(1 == read(pipefd2[0], buffer, sizeof(buffer))) ;
   TEST('2' == buffer[0]) ;
   TEST(3 == write(pipefd1[1], "123", 3)) ;
   TEST(3 == read(iochannel_STDIN, &buffer, sizeof(buffer))) ;
   TEST(0 == strncmp(buffer, "123", 3)) ;
   TEST(0 == free_processstdio2(&stdfd2)) ;
   TEST(stdfd2.stdfd.std_in  == pipefd1[0]) ;
   TEST(stdfd2.stdfd.std_out == pipefd1[1]) ;
   TEST(stdfd2.stdfd.std_err == pipefd2[1]) ;
   TEST(stdfd2.devnull       == iochannel_INIT_FREEABLE) ;

   // TEST redirectstdio_processstdio2: inherit of closed fds
   stdfd = (process_stdio_t) process_stdio_INIT_INHERIT ;
   TEST(0 == init_processstdio2(&stdfd2, &stdfd)) ;
   for (int i = 0; i < 3; ++i) {
      int fd = i ;
      TEST(0  == free_iochannel(&fd)) ;
      TEST(-1 == fd) ;
   }
   TEST(0 == redirectstdio_processstdio2(&stdfd2)) ;
   TEST(0 == free_processstdio2(&stdfd2)) ;

   // restore stdio
   for (int i = 0; i < 3; ++i) {
      TEST(i == dup2(oldstdfd[i], i)) ;
      TEST(0 == free_iochannel(&oldstdfd[i]));
   }
   TEST(0 == free_iochannel(&pipefd1[0])) ;
   TEST(0 == free_iochannel(&pipefd1[1])) ;
   TEST(0 == free_iochannel(&pipefd2[0])) ;
   TEST(0 == free_iochannel(&pipefd2[1])) ;

   return 0 ;
ONABORT:
   (void) free_processstdio2(&stdfd2) ;
   for (int i = 0; i < 3; ++i) {
      if (-1 != oldstdfd[i]) {
         (void) dup2(oldstdfd[i], i) ;
         (void) free_iochannel(&oldstdfd[i]) ;
      }
   }
   free_iochannel(&pipefd1[0]) ;
   free_iochannel(&pipefd1[1]) ;
   free_iochannel(&pipefd2[0]) ;
   free_iochannel(&pipefd2[1]) ;
   return EINVAL ;
}

static int test_initfree(void)
{
   process_t         process = process_INIT_FREEABLE ;
   process_result_t  process_result ;
   process_state_e   process_state ;
   struct timespec   ts              = { 0, 0 } ;
   bool              isoldsignalmask = false ;
   sigset_t          oldsignalmask ;
   sigset_t          signalmask ;

   // install signalhandler
   TEST(0 == sigemptyset(&signalmask)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == sigprocmask(SIG_BLOCK, &signalmask, &oldsignalmask)) ;
   isoldsignalmask = true ;

   // TEST static init
   TEST(sys_process_INIT_FREEABLE == process) ;
   TEST(0 == sys_process_INIT_FREEABLE) ;

   // TEST init_process, free_process
   TEST(0 == initgeneric_process(&process, &childprocess_return, 0, 0)) ;
   TEST(0 <  process) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == process) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == process) ;

   for (int i = 255; i >= 0; i -= 13) {
      // TEST wait_process
      TEST(0 == initgeneric_process(&process, &childprocess_return, i, 0)) ;
      TEST(0 <  process) ;
      TEST(0 == wait_process(&process, &process_result)) ;
      TEST(process_result.state      == process_state_TERMINATED) ;
      TEST(process_result.returncode == i) ;

      // TEST state_process
      process_state = process_state_RUNNABLE ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state == process_state_TERMINATED) ;

      // TEST double wait_process => returns the same result
      process_result.state = process_state_RUNNABLE ;
      process_result.returncode = -1 ;
      TEST(0 == wait_process(&process, &process_result)) ;
      TEST(process_result.state      == process_state_TERMINATED) ;
      TEST(process_result.returncode == i) ;
      TEST(0 <  process) ;

      // TEST state_process
      process_state = process_state_RUNNABLE ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state == process_state_TERMINATED) ;

      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;

      // run last testcase with 0
      if (0 < i && i < 13) i = 13 ;
   }

   // TEST endless loop => delete ends process
   for (int i = 0; i < 32; ++i) {
      while (SIGUSR1 == sigtimedwait(&signalmask, 0, &ts)) ;
      TEST(0 == init_process(&process, &childprocess_endlessloop, 0, 0)) ;
      TEST(0 <  process) ;
      TEST(SIGUSR1 == sigwaitinfo(&signalmask, 0)) ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_RUNNABLE == process_state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }

   // TEST state_process
   for (int i = 0; i < 32; ++i) {
      while (SIGUSR1 == sigtimedwait(&signalmask, 0, &ts)) ;
      TEST(0 == init_process(&process, &childprocess_endlessloop, 0, 0)) ;
      TEST(0 <  process) ;
      TEST(SIGUSR1 == sigwaitinfo(&signalmask, 0)) ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_RUNNABLE == process_state) ;
      kill(process, SIGSTOP) ;
      for (int i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_RUNNABLE != process_state) break ;
         sleepms_thread(1) ;
      }
      TEST(process_state_STOPPED == process_state) ;
      kill(process, SIGCONT) ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_RUNNABLE == process_state) ;
      kill(process, SIGKILL) ;
      for (int i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_RUNNABLE != process_state) break ;
         sleepms_thread(1) ;
      }
      TEST(process_state_ABORTED == process_state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }

   // TEST ECHILD
   TEST(0 == initgeneric_process(&process, &childprocess_return, 0, 0)) ;
   TEST(0 <  process) ;
   TEST(0 == wait_process(&process, 0)) ;
   TEST(0 <  process) ;
   {
      process_t process2 = process ;
      TEST(0 == free_process(&process2)) ;
   }
   TEST(ECHILD == state_process(&process, &process_state)) ;
   TEST(0 <  process) ;
   TEST(ECHILD == wait_process(&process, 0)) ;
   TEST(0 <  process) ;
   TEST(ECHILD == free_process(&process)) ;
   TEST(0 == process) ;

   // restore signalhandler
   while (SIGUSR1 == sigtimedwait(&signalmask, 0, &ts)) ;
   isoldsignalmask = false ;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0)) ;

   return 0 ;
ONABORT:
   while (SIGUSR1 == sigtimedwait(&signalmask, 0, &ts)) ;
   if (isoldsignalmask) sigprocmask(SIG_SETMASK, &oldsignalmask, 0) ;
   (void) free_process(&process) ;
   return EINVAL ;
}

static int test_abnormalexit(void)
{
   process_t               process   = process_INIT_FREEABLE ;
   process_state_e         process_state ;
   process_result_t        process_result ;

   // TEST init, wait process_state_ABORTED
   int test_signals[] = {
       SIGHUP,    SIGINT,   SIGQUIT,  SIGILL,  SIGTRAP
      ,SIGABRT,   SIGBUS,   SIGFPE,   SIGKILL, SIGUSR1
      ,SIGSEGV,   SIGUSR2,  SIGPIPE,  SIGALRM, SIGTERM
      ,SIGSTKFLT, SIGCHLD,  SIGCONT,  SIGSTOP, SIGTSTP
      ,SIGTTIN,   SIGTTOU,  SIGURG,   SIGXCPU, SIGXFSZ
      ,SIGVTALRM, SIGPROF,  SIGWINCH, SIGIO,   SIGPWR
      ,SIGSYS,    SIGRTMIN, SIGRTMAX,
   } ;
   unsigned signal_count = 0 ;
   for (unsigned i = 0; i < lengthof(test_signals); ++i) {
      int snr = test_signals[i] ;
      TEST(0 == initgeneric_process(&process, &childprocess_signal, snr, 0)) ;
      TEST(0 == wait_process(&process, &process_result)) ;
      if (process_state_ABORTED == process_result.state) {
         TEST(snr == process_result.returncode) ;
         ++ signal_count ;
      } else {
         TEST(process_state_TERMINATED == process_result.state) ;
         // signal ignored
         TEST(0   == process_result.returncode) ;
      }
      // TEST state_process returns always process_state_ABORTED or process_state_TERMINATED
      process_state = (process_state_e) -1 ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state == process_result.state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }
   TEST(signal_count > lengthof(test_signals)/2) ;

   // TEST free works if process has already ended
   for (unsigned i = 0; i < 16; ++i) {
      TEST(0 == process) ;
      TEST(0 == initgeneric_process(&process, &childprocess_signal, SIGKILL, 0)) ;
      // wait until child has started
      for (int i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_ABORTED == process_state) break ;
         sleepms_thread(1) ;
      }
      // TEST process_state_ABORTED
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_ABORTED == process_state) ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_ABORTED == process_state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;

      TEST(0 == initgeneric_process(&process, &childprocess_signal, SIGKILL, 0)) ;
      sleepms_thread(10) ;
      // do not query state before
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }

   return 0 ;
ONABORT:
   (void) free_process(&process) ;
   return EINVAL ;
}

static int test_assert(void)
{
   process_t            process = process_INIT_FREEABLE ;
   process_result_t     process_result ;

   // TEST assert exits with signal SIGABRT
   TEST(0 == init_process(&process, &chilprocess_execassert, 0, 0)) ;
   TEST(0 == wait_process(&process, &process_result)) ;
   TEST(process_state_ABORTED == process_result.state) ;
   TEST(SIGABRT               == process_result.returncode) ;
   TEST(0 == free_process(&process)) ;

   // TEST stdfd failure => assert !
   process_stdio_t stdfd = process_stdio_INIT_DEVNULL ;
   {
      int pipefd1[2] ;  // necessary so that pipefd2[0] is not same as devnull
      int pipefd2[2] ;
      TEST(0 == pipe2(pipefd1,O_CLOEXEC|O_NONBLOCK)) ;
      TEST(0 == pipe2(pipefd2,O_CLOEXEC|O_NONBLOCK)) ;
      redirectin_processstdio(&stdfd, pipefd2[0]) ;
      TEST(0 == free_iochannel(&pipefd1[0])) ;
      TEST(0 == free_iochannel(&pipefd1[1])) ;
      TEST(0 == free_iochannel(&pipefd2[0])) ;
      TEST(0 == free_iochannel(&pipefd2[1])) ;
   }
   TEST(0 == init_process(&process, &childprocess_donothing, 0, &stdfd)) ;
   TEST(0 == wait_process(&process, &process_result)) ;
   TEST(process_state_ABORTED == process_result.state) ;
   TEST(SIGABRT               == process_result.returncode) ;
   TEST(0 == free_process(&process)) ;

   return 0 ;
ONABORT:
   (void) free_process(&process) ;
   return EINVAL ;
}

static int test_statequery(void)
{
   process_t               process = process_INIT_FREEABLE ;
   int                     pipefd[2] = { -1, -1 } ;
   process_state_e         process_state ;
   process_result_t        process_result ;

   TEST(0 == pipe2(pipefd,O_CLOEXEC)) ;

   for (unsigned i = 0; i < 4; ++i) {

      // use wait_process (to end process)
      TEST(0 == initgeneric_process(&process, &childprocess_signal, SIGSTOP, 0)) ;
      // wait until child has started
      for (int i2 = 0; i2 < 1000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_STOPPED == process_state) break ;
         sleepms_thread(1) ;
      }
      // TEST process_state_STOPPED
      TEST(process_state_STOPPED == process_state) ;
      process_state = process_state_TERMINATED ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_STOPPED == process_state) ;
      // TEST wait_process continues stopped child
      memset(&process_result, 0xff, sizeof(process_result)) ;
      TEST(0 == wait_process(&process, &process_result)) ;
      TEST(process_result.state      == process_state_TERMINATED) ;
      TEST(process_result.returncode == 0) ;
      // TEST process_state_TERMINATED
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_TERMINATED  == process_state) ;
      TEST(0 == process_result.returncode) ;
      TEST(0 == free_process(&process)) ;

      // use free_process (to end process)
      TEST(0 == initgeneric_process(&process, &childprocess_signal, SIGSTOP, 0)) ;
      // wait until child has started
      for (int i2 = 0; i2 < 1000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_STOPPED == process_state) break ;
         sleepms_thread(1) ;
      }
      TEST(process_state_STOPPED == process_state) ;
      process_state = process_state_RUNNABLE ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_STOPPED == process_state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }

   // TEST state_process: returns latest state
   TEST(0 == initgeneric_process(&process, &childprocess_statechange, pipefd[1], 0)) ;
   {
      // wait until child has started
      char buffer[100] = { 0 } ;
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "sleep\n")) ;
   }
   sleepms_thread(10) ;
   // -- process_state_STOPPED
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_STOPPED == process_state) ;
   process_state = process_state_RUNNABLE ;
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_STOPPED == process_state) ;
   TEST(0 == kill(process, SIGCONT)) ;
   {
      // wait until child run again
      char buffer[100] = { 0 } ;
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "run\n")) ;
   }
   // -- process_state_RUNNABLE
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_RUNNABLE == process_state) ;
   process_state = process_state_STOPPED ;
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_RUNNABLE == process_state) ;
   TEST(0 == kill(process, SIGKILL)) ;
   sleepms_thread(10) ;
   // -- process_state_ABORTED
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_ABORTED == process_state) ;
   process_state = process_state_STOPPED ;
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_ABORTED == process_state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == process) ;

   TEST(0 == free_iochannel(&pipefd[0])) ;
   TEST(0 == free_iochannel(&pipefd[1])) ;

   return 0 ;
ONABORT:
   (void) free_process(&process) ;
   free_iochannel(&pipefd[0]) ;
   free_iochannel(&pipefd[1]) ;
   return EINVAL ;
}

static int test_exec(void)
{
   process_t            process = process_INIT_FREEABLE ;
   process_result_t     process_result ;
   process_stdio_t      stdfd   = process_stdio_INIT_DEVNULL ;
   int                  fd[2]   = { -1, -1 } ;
   struct stat          statbuf ;
   char                 numberstr[20] ;
   const char         * testcase1_args[] = { "bin/testchildprocess", "1", numberstr, 0 } ;
   const char         * testcase2_args[] = { "bin/testchildprocess", "2", 0 } ;
   const char         * testcase3_args[] = { "bin/testchildprocess", "3", 0 } ;
   char                 readbuffer[32]   = { 0 } ;

   // prepare
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK)) ;
   if (0 != stat("bin/testchildprocess", &statbuf)) {
      testcase1_args[0] = "bin/testchildprocess_Debug" ;
      testcase2_args[0] = "bin/testchildprocess_Debug" ;
      testcase3_args[0] = "bin/testchildprocess_Debug" ;
   }

   // TEST executing child process return value (case1)
   for (int i = 0; i <= 35; i += 7) {
      snprintf(numberstr, sizeof(numberstr), "%d", i) ;
      TEST(0 == initexec_process(&process, testcase1_args[0], testcase1_args, 0)) ;
      TEST(0 == wait_process(&process, &process_result)) ;
      TEST(process_state_TERMINATED == process_result.state) ;
      TEST(i == process_result.returncode) ;
      TEST(0 == free_process(&process)) ;
   }

   // TEST initexec_process: open file descriptors (case2)
   for (int i = 1; i <= 3; ++i) {
      stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL ;
      redirecterr_processstdio(&stdfd, fd[1]) ;
      if (i > 1) redirectin_processstdio(&stdfd, STDIN_FILENO) ;
      if (i > 2) redirectout_processstdio(&stdfd, STDOUT_FILENO) ;
      TEST(0 == initexec_process(&process, testcase2_args[0], testcase2_args, &stdfd)) ;
      TEST(0 == wait_process(&process, &process_result)) ;
      TEST(process_result.state      == process_state_TERMINATED) ;
      TEST(process_result.returncode == 0) ;
      TEST(0 == free_process(&process)) ;
      MEMSET0(&readbuffer) ;
      TEST(0 < read(fd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(2 >= strlen(readbuffer)) ;
      // either 3 or more files (X11+GLX opens some fds which are inherited)
      TEST(3 <= atoi(readbuffer)) ;
   }

   // TEST name_process (case 3)
   stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL ;
   redirecterr_processstdio(&stdfd, fd[1]) ;
   TEST(0 == initexec_process(&process, testcase3_args[0], &testcase3_args[0], &stdfd)) ;
   TEST(0 == wait_process(&process, &process_result)) ;
   TEST(process_state_TERMINATED == process_result.state) ;
   TEST(0 == process_result.returncode) ;
   TEST(0 == free_process(&process)) ;
   MEMSET0(&readbuffer) ;
   TEST(0 < read(fd[0], readbuffer, sizeof(readbuffer))) ;
   TEST(0 == strncmp(readbuffer, testcase3_args[0]+4, 15)) ;

   // unprepare
   TEST(0 == free_iochannel(&fd[0])) ;
   TEST(0 == free_iochannel(&fd[1])) ;

   return 0 ;
ONABORT:
   free_iochannel(&fd[0]) ;
   free_iochannel(&fd[1]) ;
   (void) free_process(&process) ;
   return EINVAL ;
}

static int test_daemon(void)
{
   process_t            process = process_INIT_FREEABLE ;
   process_result_t     process_result ;
   process_stdio_t      stdfd   = process_stdio_INIT_DEVNULL ;
   int                  fd[2]   = { -1, -1 } ;
   struct timespec      ts      = { 0, 0 } ;
   char                 readbuffer[20] ;
   sigset_t             oldsignalmask ;
   sigset_t             signalmask ;
   bool                 isoldsignalmask = false ;

   // prepare
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 == sigemptyset(&signalmask)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == sigprocmask(SIG_BLOCK, &signalmask, &oldsignalmask)) ;
   isoldsignalmask = true ;

   // TEST initdaemon_process: return value
   for (int i = 255; i >= 0; i -= 50) {
      // TEST wait_process
      TEST(0 == initdaemon_process(&process, (process_task_f)&childprocess_return, (void*)i, 0)) ;
      TEST(0 == wait_process(&process, &process_result)) ;
      TEST(process_result.state      == process_state_TERMINATED) ;
      TEST(process_result.returncode == i) ;
      TEST(0 == free_process(&process)) ;
   }

   // TEST initdaemon_process: stdfd return ssid
   stdfd = (process_stdio_t) process_stdio_INIT_DEVNULL ;
   redirectin_processstdio(&stdfd, fd[0]) ;
   redirectout_processstdio(&stdfd, fd[1]) ;
   redirecterr_processstdio(&stdfd, fd[1]) ;
   TEST(5 == write(fd[1], "12345", 5)) ;
   TEST(0 == initdaemon_process(&process, &childprocess_daemon, (void*)0, &stdfd)) ;
   TEST(0 == wait_process(&process, &process_result)) ;
   TEST(process_result.state      == process_state_TERMINATED) ;
   TEST(process_result.returncode == 0) ;
   TEST(5 == read(fd[0], readbuffer, 5)) ;
   TEST(0 == strncmp(readbuffer, "12345", 5)) ;
   TEST(0 < read(fd[0], readbuffer, sizeof(readbuffer))) ;
   TEST(atoi(readbuffer) == process) ;
   TEST(0 != strchr(readbuffer, ':')) ;
   TEST(0 == strcmp(strchr(readbuffer, ':')+1, "/")) ;
   TEST(0 == free_process(&process)) ;

   // unprepare
   TEST(0 == free_iochannel(&fd[0])) ;
   TEST(0 == free_iochannel(&fd[1])) ;
   while (SIGUSR1 == sigtimedwait(&signalmask, 0, &ts)) ;
   isoldsignalmask = false ;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0)) ;

   return 0 ;
ONABORT:
   free_iochannel(&fd[0]) ;
   free_iochannel(&fd[1]) ;
   while (SIGUSR1 == sigtimedwait(&signalmask, 0, &ts)) ;
   if (isoldsignalmask) sigprocmask(SIG_SETMASK, &oldsignalmask, 0) ;
   (void) free_process(&process) ;
   return EINVAL ;
}

int unittest_platform_task_process()
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_redirect())       goto ONABORT ;
   if (test_redirect2())      goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_abnormalexit())   goto ONABORT ;
   if (test_assert())         goto ONABORT ;
   if (test_statequery())     goto ONABORT ;
   if (test_exec())           goto ONABORT ;
   if (test_daemon())         goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // adapt LOG buffer ("pid=1234" replaces with "pid=?")
   char * buffer = 0 ;
   size_t size   = 0 ;
   GETBUFFER_LOG(&buffer, &size) ;
   char buffer2[2000] = { 0 } ;
   assert(size < sizeof(buffer2)) ;
   size = 0 ;
   while (strstr(buffer, "\npid=")) {
      memcpy( &buffer2[size], buffer, (size_t) (strstr(buffer, "\npid=") - buffer)) ;
      size += (size_t) (strstr(buffer, "\npid=") - buffer) ;
      strcpy( &buffer2[size], "\npid=?") ;
      size += strlen("\npid=?") ;
      buffer = 1 + strstr(buffer, "\npid=") ;
      buffer = strstr(buffer, "\n") ;
   }
   strcpy( &buffer2[size], buffer) ;

   CLEARBUFFER_LOG() ;
   PRINTF_ERRLOG("%s", buffer2) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
