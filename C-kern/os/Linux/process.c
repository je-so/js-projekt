/* title: ProcessManagement Linux
   Implements <ProcessManagement>.

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

   file: C-kern/api/os/process.h
    Header file of <ProcessManagement>.

   file: C-kern/os/Linux/process.c
    Implementation file <ProcessManagement Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/process.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/os/thread.h"
#endif


typedef struct childprocess_exec_t        childprocess_exec_t ;

typedef struct process_ioredirect2_t      process_ioredirect2_t ;

enum queryoption_e {
    queryoption_NOWAIT
   ,queryoption_WAIT
   ,queryoption_WAIT_AND_FREE
} ;

typedef enum queryoption_e                queryoption_e ;

struct childprocess_exec_t {
   const char *   filename ;
   char       **  arguments ;
   sys_file_t     errpipe ;
} ;

struct process_ioredirect2_t {
   process_ioredirect_t    ioredirect ;
   sys_file_t              devnull ;
} ;


// section: Functions

// group: query

int name_process(size_t namebuffer_size, /*out*/char name[namebuffer_size], /*out*/size_t * name_size)
{
   int err ;
   char buffer[16+1] ;

   buffer[16] = 0 ;

   err = prctl(PR_GET_NAME, buffer, 0, 0, 0) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("prctl(PR_GET_NAME)", err) ;
      goto ABBRUCH ;
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
ABBRUCH:
   return err ;
}


// section: process_ioredirect2_t

// group: lifetime

#define process_ioredirect2_INIT_FREEABLE    { process_ioredirect_INIT_DEVNULL, sys_file_INIT_FREEABLE }

/* function: init_processioredirect2
 * Initializes <process_ioredirect2_t> with <process_ioredirect_t> and opens devnull.
 * The device null is only opened if ioredirection is 0 or at least one file descriptor
 * is set to sys_file_INIT_FREEABLE. */
static int init_processioredirect2(/*out*/process_ioredirect2_t * ioredirect2, process_ioredirect_t * ioredirection)
{
   int err ;
   int devnull = -1 ;

   if (  !ioredirection
      || sys_file_INIT_FREEABLE == ioredirection->infile
      || sys_file_INIT_FREEABLE == ioredirection->outfile
      || sys_file_INIT_FREEABLE == ioredirection->errfile ) {
      devnull = open("/dev/null", O_RDWR|O_CLOEXEC) ;
      if (-1 == devnull) {
         err = errno ;
         LOG_SYSERR("open(/dev/null,O_RDWR)", err) ;
         goto ABBRUCH ;
      }
   }

   // return out param ioredirect2
   if (ioredirection) {
      ioredirect2->ioredirect = *ioredirection ;
   } else {
      ioredirect2->ioredirect = (process_ioredirect_t) process_ioredirect_INIT_DEVNULL ;
   }
   ioredirect2->devnull = devnull ;

   return 0 ;
ABBRUCH:
   if (-1 != devnull) close(devnull) ;
   LOG_ABORT(err) ;
   return err ;
}

/* function: free_processioredirect2
 * Closes if devnull if necessary. */
static int free_processioredirect2(process_ioredirect2_t * ioredirect2)
{
   int err ;

   if (sys_file_INIT_FREEABLE != ioredirect2->devnull) {

      err = close(ioredirect2->devnull) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("close", err) ;
      }
      ioredirect2->devnull = sys_file_INIT_FREEABLE ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

/* function: redirectstdfd_processioredirect2
 * Redirects one standard channel to read/write from/to a file.
 *
 * Parameter:
 * stdfd           - The file descriptor of the standard io channel.
 *                   Set this value to one of STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
 * redirectto_file - The file descriptor of the file which now becomes the new standard io channel.
 *                   Use value sys_file_INIT_FREEABLE to redirect to devnull.
 *                   Use same value as stdfd if the standard channel should be
 *                   inherited between processes.
 * */
static int redirectstdfd_processioredirect2(const process_ioredirect2_t * ioredirect2, int stdfd, int redirectto_file)
{
   int err ;
   int fd = redirectto_file ;

   if (stdfd != fd) {
      if (sys_file_INIT_FREEABLE == fd) {
         fd = ioredirect2->devnull ;
      }
      while(-1 == dup2(fd, stdfd)) {
         if (EINTR != errno) {
            err = errno ;
            LOG_SYSERR("dup2(fd, stdfd)", err) ;
            LOG_INT(fd) ;
            LOG_INT(stdfd) ;
            goto ABBRUCH ;
         }
      }
   } else {
      // clear FD_CLOEXEC
      // ignore error in case stdfd is closed
      (void) fcntl(stdfd, F_SETFD, (long)0) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: redirectstdio_processioredirect2
 * Redirects all 3 standard io channels to read/write from/to a file.
 *
 * If the new file descriptors have the same values as the standard channels
 * then no redirection is done but they are inherited.
 *
 * Uses <redirectstdfd_processioredirect2> to implement its functionality. */
static int redirectstdio_processioredirect2(const process_ioredirect2_t * ioredirect2)
{
   int err ;
   int err2 ;

   err = redirectstdfd_processioredirect2(ioredirect2, STDIN_FILENO, ioredirect2->ioredirect.infile) ;

   err2 = redirectstdfd_processioredirect2(ioredirect2, STDOUT_FILENO, ioredirect2->ioredirect.outfile) ;
   if (err2) err = err2 ;

   err2 = redirectstdfd_processioredirect2(ioredirect2, STDERR_FILENO, ioredirect2->ioredirect.errfile) ;
   if (err2) err = err2 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
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
      PRECONDITION_INPUT(option == queryoption_WAIT_AND_FREE, ABBRUCH, LOG_INT(option)) ;
      flags = FLAGS ;
      break ;
   }

#undef FLAGS

   info.si_pid = 0 ;
   while(-1 == waitid(P_PID, (id_t) pid, &info, flags)) {
      if (EINTR != errno) {
         err = errno ;
         LOG_SYSERR("waitid",err) ;
         LOG_INT(pid) ;
         goto ABBRUCH ;
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
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int childprocess_exec(childprocess_exec_t  * execparam)
{
   int err ;
   int write_err ;

   execvp( execparam->filename, execparam->arguments ) ;
   err = errno ;

// ABBRUCH:

   do {
      write_err = write( execparam->errpipe, &err, sizeof(&err)) ;
   } while( -1 == write_err && errno == EINTR ) ;

   return err ;
}

// group: implementation

int initexec_process(process_t * process, const char * filename, const char * const * arguments, process_ioredirect_t * ioredirection /*0 => /dev/null*/)
{
   int err ;
   process_t            childprocess = process_INIT_FREEABLE ;
   int                  pipefd[2]    = { -1, -1 } ;
   childprocess_exec_t  execparam    = { filename, (char**)(intptr_t)arguments, -1 } ;

   if ( pipe2(pipefd,O_CLOEXEC) ) {
      err = errno ;
      LOG_SYSERR("pipe2", err) ;
      goto ABBRUCH ;
   }

   execparam.errpipe = pipefd[1] ;

   err = init_process( &childprocess, &childprocess_exec, &execparam, ioredirection) ;
   if (err) goto ABBRUCH ;

   // CHECK exec error
   err = close( pipefd[1] ) ;
   pipefd[1] = -1 ;
   assert(!err) ;

   ssize_t read_bytes = 0 ;

   int exec_err = 0 ;

   do {
      read_bytes = read( pipefd[0], &exec_err, sizeof(exec_err)) ;
   } while( -1 == read_bytes && EINTR == errno) ;

   if (-1 == read_bytes) {
      err = errno ;
      LOG_SYSERR("read", err) ;
      goto ABBRUCH ;
   } else if (read_bytes) {
      // EXEC error
      err = exec_err ? exec_err : ENOEXEC ;
      LOG_SYSERR("execvp(filename, arguments)", err) ;
      LOG_STRING(filename) ;
      for(size_t i = 0; arguments[i]; ++i) {
         LOG_INDEX("s",arguments,i) ;
      }
      goto ABBRUCH ;
   }

   if (close(pipefd[0])) {
      err = errno ;
      LOG_SYSERR("close",err) ;
      goto ABBRUCH ;
   }

   *process = childprocess ;

   return 0 ;
ABBRUCH:
   if (-1 != pipefd[1]) close(pipefd[1]) ;
   if (-1 != pipefd[0]) close(pipefd[0]) ;
   (void) free_process(&childprocess) ;
   LOG_ABORT(err) ;
   return err ;
}

#undef init_process
int init_process(/*out*/process_t         *  process,
                  task_callback_f            child_main,
                  struct callback_param_t *  start_arg,
                  process_ioredirect_t    *  ioredirection /*0 => /dev/null*/)
{
   int err ;
   pid_t pid ;

   // MULTITHREAD-PRECONDITION: all filedescriptors opened with O_CLOEXEC ; => test/static/close_on_exec.sh

   // TODO: MULTITHREAD: system handler for preparing fork
   pid = fork() ;
   if (-1 == pid) {
      err = errno ;
      LOG_SYSERR("fork", err) ;
      goto ABBRUCH ;
   }

   if (0 == pid) {
      // NEW CHILD PROCESS
      // TODO: MULTITHREAD: system handler for freeing thread resources (i.e. clear log, ...) ?!?
      process_ioredirect2_t ioredirect2 ;
      err = init_processioredirect2(&ioredirect2, ioredirection) ;
      if (!err) { err = redirectstdio_processioredirect2(&ioredirect2) ; }
      if (!err) { err = free_processioredirect2(&ioredirect2) ; }
      assert(!err && "[init|free|redirectstdio]_processioredirect2") ;

      int returncode = (child_main ? child_main(start_arg) : 0) ;
      exit(returncode) ;
   }

   *process = pid ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
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

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int state_process(process_t * process, /*out*/process_state_e * current_state)
{
   int err ;
   process_result_t result ;

   err = queryresult_process(*process, &result, queryoption_NOWAIT) ;
   if (err) goto ABBRUCH ;

   *current_state = result.state ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int wait_process(process_t * process, /*out*/process_result_t * result)
{
   int err ;
   pid_t pid = *process ;

   kill(pid, SIGCONT) ;

   for(;;) {
      process_result_t state ;

      err = queryresult_process(pid, &state, queryoption_WAIT) ;
      if (err) goto ABBRUCH ;

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
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}



// section: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_process,ABBRUCH)

#define init_process(process, child_main, start_arg, ioredirection) \
   /*do not forget to adapt definition in process.c test section*/                  \
   ( __extension__ ({ int _err ;                                                    \
      int (*_child_main) (typeof(start_arg)) = (child_main) ;                       \
      static_assert(sizeof(start_arg) <= sizeof(void*), "cast 2 void*") ;           \
      _err = init_process(process, (task_callback_f) _child_main,                   \
                 (struct callback_param_t*) start_arg, ioredirection ) ;            \
      _err ; }))

static int childprocess_return(int returncode)
{
   kill(getppid(), SIGUSR1) ;
   return returncode ;
}

static int childprocess_endlessloop(int dummy)
{
   (void) dummy ;
   kill(getppid(), SIGUSR1) ;
   for(;;) {
      sleepms_osthread(1000) ;
   }
   return 0 ;
}

static int childprocess_signal(int signr)
{
   kill(getpid(), signr) ;
   return 0 ;
}

static int chilprocess_execassert(int dummy)
{
   (void) dummy ;
   // flushing of log output is redirected to devnull (from caller)
   assert(0) ;
   return 0 ;
}

static int childprocess_donothing(int dummy)
{
   (void) dummy ;
   return 0 ;
}

static int childprocess_statechange(int fd)
{
   dprintf(fd,"sleep\n") ;
   kill( getpid(), SIGSTOP) ;
   dprintf(fd,"run\n") ;
   for(;;) {
      sleepms_osthread(1000) ;
   }
   return 0 ;
}

static int test_redirect(void)
{
   process_ioredirect_t  ioredirect  = { 0, 0, 0 } ;

   // TEST static init: process_ioredirect_INIT_DEVNULL
   ioredirect = (process_ioredirect_t) process_ioredirect_INIT_DEVNULL ;
   TEST(-1 == ioredirect.infile) ;
   TEST(-1 == ioredirect.outfile) ;
   TEST(-1 == ioredirect.errfile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect.infile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect.outfile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect.errfile) ;

   // TEST static init: process_ioredirect_INIT_INHERIT
   ioredirect = (process_ioredirect_t) process_ioredirect_INIT_INHERIT ;
   TEST(0 == ioredirect.infile) ;
   TEST(1 == ioredirect.outfile) ;
   TEST(2 == ioredirect.errfile) ;
   TEST(STDIN_FILENO  == ioredirect.infile) ;
   TEST(STDOUT_FILENO == ioredirect.outfile) ;
   TEST(STDERR_FILENO == ioredirect.errfile) ;
   TEST(sys_file_STDIN  == ioredirect.infile) ;
   TEST(sys_file_STDOUT == ioredirect.outfile) ;
   TEST(sys_file_STDERR == ioredirect.errfile) ;

   // TEST setXXX
   for(int i = 0; i < 100; ++i) {
      ioredirect = (process_ioredirect_t) process_ioredirect_INIT_DEVNULL ;
      TEST(sys_file_INIT_FREEABLE == ioredirect.infile) ;
      TEST(sys_file_INIT_FREEABLE == ioredirect.outfile) ;
      TEST(sys_file_INIT_FREEABLE == ioredirect.errfile) ;
      setin_processioredirect(&ioredirect, i) ;
      TEST(i == ioredirect.infile) ;
      TEST(sys_file_INIT_FREEABLE == ioredirect.outfile) ;
      TEST(sys_file_INIT_FREEABLE == ioredirect.errfile) ;
      setout_processioredirect(&ioredirect, i+1) ;
      TEST(i == ioredirect.infile) ;
      TEST(i+1 == ioredirect.outfile) ;
      TEST(sys_file_INIT_FREEABLE == ioredirect.errfile) ;
      seterr_processioredirect(&ioredirect, i+2) ;
      TEST(i == ioredirect.infile) ;
      TEST(i+1 == ioredirect.outfile) ;
      TEST(i+2 == ioredirect.errfile) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_redirect2(void)
{
   process_ioredirect2_t ioredirect2 = process_ioredirect2_INIT_FREEABLE ;
   int                   oldstdfd[3] = { [STDIN_FILENO] = -1, [STDOUT_FILENO] = -1, [STDERR_FILENO] = -1 } ;
   int                   pipefd1[2]  = { -1, -1 } ;
   int                   pipefd2[2]  = { -1, -1 } ;
   char                  buffer[10]  = { 0 } ;
   process_ioredirect_t  ioredirect ;

   // TEST static init
   TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.infile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.outfile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.errfile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.devnull) ;

   // TEST init(0), double free
   MEMSET0(&ioredirect2) ;
   ioredirect2.devnull = sys_file_INIT_FREEABLE ;
   TEST(0 == init_processioredirect2(&ioredirect2, 0)) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.infile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.outfile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.errfile) ;
   TEST(sys_file_INIT_FREEABLE != ioredirect2.devnull) ;
   TEST(0 == free_processioredirect2(&ioredirect2)) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.devnull) ;
   TEST(0 == free_processioredirect2(&ioredirect2)) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.devnull) ;

   // TEST init(inherit)
   ioredirect = (process_ioredirect_t) process_ioredirect_INIT_INHERIT ;
   ioredirect2.devnull = -2 ;
   TEST(0 == init_processioredirect2(&ioredirect2, &ioredirect)) ;
   TEST(sys_file_STDIN  == ioredirect2.ioredirect.infile) ;
   TEST(sys_file_STDOUT == ioredirect2.ioredirect.outfile) ;
   TEST(sys_file_STDERR == ioredirect2.ioredirect.errfile) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.devnull) ;
   TEST(0 == free_processioredirect2(&ioredirect2)) ;
   TEST(sys_file_INIT_FREEABLE == ioredirect2.devnull) ;

   // TEST init(only one fd is set to devnull)
   for(int i = 0; i < 3; ++i) {
      ioredirect = (process_ioredirect_t) process_ioredirect_INIT_INHERIT ;
      ioredirect2.devnull = sys_file_INIT_FREEABLE ;
      switch(i) { // set one std fd to devnull
      case 0:  setin_processioredirect(&ioredirect, sys_file_INIT_FREEABLE) ; break ;
      case 1:  setout_processioredirect(&ioredirect, sys_file_INIT_FREEABLE) ; break ;
      case 2:  seterr_processioredirect(&ioredirect, sys_file_INIT_FREEABLE) ; break ;
      }
      TEST(0 == init_processioredirect2(&ioredirect2, &ioredirect)) ;
      if (i == 0) {
         TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.infile) ;
      } else {
         TEST(sys_file_STDIN == ioredirect2.ioredirect.infile) ;
      }
      if (i == 1) {
         TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.outfile) ;
      } else {
         TEST(sys_file_STDOUT == ioredirect2.ioredirect.outfile) ;
      }
      if (i == 2) {
         TEST(sys_file_INIT_FREEABLE == ioredirect2.ioredirect.errfile) ;
      } else {
         TEST(sys_file_STDERR == ioredirect2.ioredirect.errfile) ;
      }
      TEST(sys_file_INIT_FREEABLE != ioredirect2.devnull) ;
      TEST(0 == free_processioredirect2(&ioredirect2)) ;
      TEST(sys_file_INIT_FREEABLE == ioredirect2.devnull) ;
      TEST(0 == free_processioredirect2(&ioredirect2)) ;
      TEST(sys_file_INIT_FREEABLE == ioredirect2.devnull) ;
   }

   {
      // store old stdio
      for(int stdfd = 0; stdfd < 3; ++stdfd) {
         oldstdfd[stdfd] = dup(stdfd) ;
         TEST(-1 != oldstdfd[stdfd]) ;
      }
      TEST(0 == pipe2(pipefd1, O_CLOEXEC|O_NONBLOCK)) ;
      TEST(0 == pipe2(pipefd2, O_CLOEXEC|O_NONBLOCK)) ;
   }

   // TEST redirectstdio_processioredirect2
   ioredirect = (process_ioredirect_t) process_ioredirect_INIT_DEVNULL ;
   setin_processioredirect(&ioredirect, pipefd1[0]) ;
   setout_processioredirect(&ioredirect, pipefd1[1]) ;
   seterr_processioredirect(&ioredirect, pipefd2[1]) ;
   TEST(0 == init_processioredirect2(&ioredirect2, &ioredirect)) ;
   TEST(ioredirect2.ioredirect.infile  == pipefd1[0]) ;
   TEST(ioredirect2.ioredirect.outfile == pipefd1[1]) ;
   TEST(ioredirect2.ioredirect.errfile == pipefd2[1]) ;
   TEST(ioredirect2.devnull            == sys_file_INIT_FREEABLE) ;
   TEST(0 == redirectstdio_processioredirect2(&ioredirect2)) ;
   TEST(1 == write(sys_file_STDOUT, "1", 1)) ;
   TEST(1 == write(sys_file_STDERR, "2", 1)) ;
   TEST(1 == read(pipefd1[0], buffer, sizeof(buffer))) ;
   TEST('1' == buffer[0]) ;
   TEST(1 == read(pipefd2[0], buffer, sizeof(buffer))) ;
   TEST('2' == buffer[0]) ;
   TEST(3 == write(pipefd1[1], "123", 3)) ;
   TEST(3 == read(sys_file_STDIN, &buffer, sizeof(buffer))) ;
   TEST(0 == strncmp(buffer, "123", 3)) ;
   TEST(0 == free_processioredirect2(&ioredirect2)) ;
   TEST(ioredirect2.ioredirect.infile  == pipefd1[0]) ;
   TEST(ioredirect2.ioredirect.outfile == pipefd1[1]) ;
   TEST(ioredirect2.ioredirect.errfile == pipefd2[1]) ;
   TEST(ioredirect2.devnull            == sys_file_INIT_FREEABLE) ;

   // TEST redirectstdio inherit of closed fds
   ioredirect = (process_ioredirect_t) process_ioredirect_INIT_INHERIT ;
   TEST(0 == init_processioredirect2(&ioredirect2, &ioredirect)) ;
   for(int stdfd = 0; stdfd < 3; ++stdfd) {
      TEST(0 == close(stdfd)) ;
   }
   TEST(0 == redirectstdio_processioredirect2(&ioredirect2)) ;
   TEST(0 == free_processioredirect2(&ioredirect2)) ;

   {
      // restore stdio
      for(int stdfd = 0; stdfd < 3; ++stdfd) {
         if (-1 != oldstdfd[stdfd]) {
            TEST(stdfd == dup2(oldstdfd[stdfd], stdfd)) ;
            TEST(0 == close(oldstdfd[stdfd]));
            oldstdfd[stdfd] = -1 ;
         }
      }
      TEST(0 == close(pipefd1[0])) ;
      TEST(0 == close(pipefd1[1])) ;
      pipefd1[0] = pipefd1[1] = -1 ;
      TEST(0 == close(pipefd2[0])) ;
      TEST(0 == close(pipefd2[1])) ;
      pipefd2[0] = pipefd2[1] = -1 ;
   }

   return 0 ;
ABBRUCH:
   (void) free_processioredirect2(&ioredirect2) ;
   for(int stdfd = 0; stdfd < 3; ++stdfd) {
      if (-1 != oldstdfd[stdfd]) {
         (void) dup2(oldstdfd[stdfd], stdfd) ;
         (void) close(oldstdfd[stdfd]) ;
      }
   }
   close(pipefd1[0]) ;
   close(pipefd1[1]) ;
   close(pipefd2[0]) ;
   close(pipefd2[1]) ;
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

   // TEST init, double free
   TEST(0 == init_process(&process, &childprocess_return, 0, 0)) ;
   TEST(0 <  process) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == process) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == process) ;

   for(int i = 255; i >= 0; i -= 13) {
      // TEST wait_process
      TEST(0 == init_process(&process, &childprocess_return, i, 0)) ;
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
   for(int i = 0; i < 32; ++i) {
      while( SIGUSR1 == sigtimedwait(&signalmask, 0, &ts) ) ;
      TEST(0 == init_process(&process, &childprocess_endlessloop, 0, 0)) ;
      TEST(0 <  process) ;
      TEST(SIGUSR1 == sigwaitinfo(&signalmask, 0)) ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_RUNNABLE == process_state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }

   // TEST state_process
   for(int i = 0; i < 32; ++i) {
      while( SIGUSR1 == sigtimedwait(&signalmask, 0, &ts) ) ;
      TEST(0 == init_process(&process, &childprocess_endlessloop, 0, 0)) ;
      TEST(0 <  process) ;
      TEST(SIGUSR1 == sigwaitinfo(&signalmask, 0)) ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_RUNNABLE == process_state) ;
      kill(process, SIGSTOP) ;
      for(int i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_RUNNABLE != process_state) break ;
         sleepms_osthread(1) ;
      }
      TEST(process_state_STOPPED == process_state) ;
      kill(process, SIGCONT) ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_RUNNABLE == process_state) ;
      kill(process, SIGKILL) ;
      for(int i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_RUNNABLE != process_state) break ;
         sleepms_osthread(1) ;
      }
      TEST(process_state_ABORTED == process_state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }

   // TEST ECHILD
   TEST(0 == init_process(&process, &childprocess_return, 0, 0)) ;
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
   while( SIGUSR1 == sigtimedwait(&signalmask, 0, &ts) ) ;
   isoldsignalmask = false ;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0)) ;

   return 0 ;
ABBRUCH:
   while( SIGUSR1 == sigtimedwait(&signalmask, 0, &ts) ) ;
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
   for(unsigned i = 0; i < nrelementsof(test_signals); ++i) {
      int snr = test_signals[i] ;
      TEST(0 == init_process(&process, &childprocess_signal, snr, 0)) ;
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
   TEST(signal_count > nrelementsof(test_signals)/2) ;

   // TEST free works if process has already ended
   for(unsigned i = 0; i < 16; ++i) {
      TEST(0 == process) ;
      TEST(0 == init_process(&process, &childprocess_signal, SIGKILL, 0)) ;
      // wait until child has started
      for(int i2 = 0; i2 < 10000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_ABORTED == process_state) break ;
         sleepms_osthread(1) ;
      }
      // TEST process_state_ABORTED
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_ABORTED == process_state) ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_ABORTED == process_state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;

      TEST(0 == init_process(&process, &childprocess_signal, SIGKILL, 0)) ;
      sleepms_osthread(10) ;
      // do not query state before
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }

   return 0 ;
ABBRUCH:
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

   // TEST ioredirection failure => assert !
   process_ioredirect_t ioredirect = process_ioredirect_INIT_DEVNULL ;
   {
      int pipefd1[2] ;
      int pipefd2[2] ;
      TEST(0 == pipe2(pipefd1,O_CLOEXEC|O_NONBLOCK)) ;
      TEST(0 == pipe2(pipefd2,O_CLOEXEC|O_NONBLOCK)) ;
      setin_processioredirect(&ioredirect, pipefd2[0]) ;
      TEST(0 == close(pipefd1[0])) ;
      TEST(0 == close(pipefd1[1])) ;
      TEST(0 == close(pipefd2[0])) ;
      TEST(0 == close(pipefd2[1])) ;
   }
   TEST(0 == init_process(&process, &childprocess_donothing, 0, &ioredirect)) ;
   TEST(0 == wait_process(&process, &process_result)) ;
   TEST(process_state_ABORTED == process_result.state) ;
   TEST(SIGABRT               == process_result.returncode) ;
   TEST(0 == free_process(&process)) ;

   return 0 ;
ABBRUCH:
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

   for(unsigned i = 0; i < 4; ++i) {

      // use wait_process (to end process)
      TEST(0 == init_process(&process, &childprocess_signal, SIGSTOP, 0)) ;
      // wait until child has started
      for(int i2 = 0; i2 < 1000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_STOPPED == process_state) break ;
         sleepms_osthread(1) ;
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
      TEST(0 == init_process(&process, &childprocess_signal, SIGSTOP, 0)) ;
      // wait until child has started
      for(int i2 = 0; i2 < 1000; ++i2) {
         TEST(0 == state_process(&process, &process_state)) ;
         if (process_state_STOPPED == process_state) break ;
         sleepms_osthread(1) ;
      }
      TEST(process_state_STOPPED == process_state) ;
      process_state = process_state_RUNNABLE ;
      TEST(0 == state_process(&process, &process_state)) ;
      TEST(process_state_STOPPED == process_state) ;
      TEST(0 == free_process(&process)) ;
      TEST(0 == process) ;
   }

   // TEST state query returns latest state
   TEST(0 == init_process(&process, &childprocess_statechange, pipefd[1], 0)) ;
   {
      // wait until child has started
      char buffer[100] = { 0 } ;
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "sleep\n")) ;
   }
   sleepms_osthread(10) ;
   // TEST process_state_STOPPED
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
   // TEST process_state_RUNNABLE
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_RUNNABLE == process_state) ;
   process_state = process_state_STOPPED ;
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_RUNNABLE == process_state) ;
   TEST(0 == kill(process, SIGKILL)) ;
   sleepms_osthread(10) ;
   // TEST process_state_ABORTED
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_ABORTED == process_state) ;
   process_state = process_state_STOPPED ;
   TEST(0 == state_process(&process, &process_state)) ;
   TEST(process_state_ABORTED == process_state) ;
   TEST(0 == free_process(&process)) ;
   TEST(0 == process) ;

   TEST(0 == close(pipefd[0])) ;
   TEST(0 == close(pipefd[1])) ;
   pipefd[0] = pipefd[1] = -1 ;

   return 0 ;
ABBRUCH:
   (void) free_process(&process) ;
   close(pipefd[0]) ;
   close(pipefd[1]) ;
   return EINVAL ;
}

static int test_exec(void)
{
   process_t            process = process_INIT_FREEABLE ;
   process_result_t     process_result ;
   process_ioredirect_t ioredirect = process_ioredirect_INIT_DEVNULL ;
   int                  fd[2]      = { -1, -1 } ;
   struct stat          statbuf ;
   char                 numberstr[20] ;
   const char         * testcase1_args[] = { "bin/testchildprocess", "1", numberstr, 0 } ;
   const char         * testcase2_args[] = { "bin/testchildprocess", "2", 0 } ;
   const char         * testcase3_args[] = { "bin/testchildprocess", "3", 0 } ;
   char                 readbuffer[32]   = { 0 } ;

   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK)) ;

   if (0 != stat("bin/testchildprocess", &statbuf)) {
      testcase1_args[0] = "bin/testchildprocess_Debug" ;
      testcase2_args[0] = "bin/testchildprocess_Debug" ;
      testcase3_args[0] = "bin/testchildprocess_Debug" ;
   }

   // TEST executing child process return value (case1)
   for(int i = 0; i <= 35; i += 7) {
      snprintf(numberstr, sizeof(numberstr), "%d", i) ;
      TEST(0 == initexec_process(&process, testcase1_args[0], testcase1_args, 0)) ;
      TEST(0 == wait_process(&process, &process_result)) ;
      TEST(process_state_TERMINATED == process_result.state) ;
      TEST(i == process_result.returncode) ;
      TEST(0 == free_process(&process)) ;
   }

   // TEST open file descriptors (case2)
   for(int i = 1; i <= 3; ++i) {
      ioredirect = (process_ioredirect_t) process_ioredirect_INIT_DEVNULL ;
      seterr_processioredirect(&ioredirect, fd[1]) ;
      if (i > 1) setin_processioredirect(&ioredirect, STDIN_FILENO) ;
      if (i > 2) setout_processioredirect(&ioredirect, STDOUT_FILENO) ;
      TEST(0 == initexec_process(&process, testcase2_args[0], testcase2_args, &ioredirect)) ;
      TEST(0 == wait_process(&process, &process_result)) ;
      TEST(process_result.state      == process_state_TERMINATED) ;
      TEST(process_result.returncode == 0) ;
      TEST(0 == free_process(&process)) ;
      MEMSET0(&readbuffer) ;
      TEST(0 < read(fd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(0 == strcmp(readbuffer, "3")) ;
   }

   // TEST name_process (case 3)
   ioredirect = (process_ioredirect_t) process_ioredirect_INIT_DEVNULL ;
   seterr_processioredirect(&ioredirect, fd[1]) ;
   TEST(0 == initexec_process(&process, testcase3_args[0], &testcase3_args[0], &ioredirect)) ;
   TEST(0 == wait_process(&process, &process_result)) ;
   TEST(process_state_TERMINATED == process_result.state) ;
   TEST(0 == process_result.returncode) ;
   TEST(0 == free_process(&process)) ;
   MEMSET0(&readbuffer) ;
   TEST(0 < read(fd[0], readbuffer, sizeof(readbuffer))) ;
   TEST(0 == strncmp(readbuffer, testcase3_args[0]+4, 15)) ;


   TEST(0 == close(fd[0])) ;
   TEST(0 == close(fd[1])) ;
   fd[0] = fd[1] = -1 ;

   return 0 ;
ABBRUCH:
   close(fd[0]) ;
   close(fd[1]) ;
   (void) free_process(&process) ;
   return EINVAL ;
}

int unittest_os_process()
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_redirect())       goto ABBRUCH ;
   if (test_redirect2())      goto ABBRUCH ;
   if (test_initfree())       goto ABBRUCH ;
   if (test_abnormalexit())   goto ABBRUCH ;
   if (test_assert())         goto ABBRUCH ;
   if (test_statequery())     goto ABBRUCH ;
   if (test_exec())           goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // adapt LOG buffer ("pid=1234" replaces with "pid=?")
   char * buffer = 0 ;
   size_t size   = 0 ;
   LOG_GETBUFFER(&buffer, &size) ;
   char buffer2[1000] = { 0 } ;
   assert(size < sizeof(buffer2)) ;
   size = 0 ;
   while( strstr(buffer, "\npid=") ) {
      memcpy( &buffer2[size], buffer, (size_t) (strstr(buffer, "\npid=") - buffer)) ;
      size += (size_t) (strstr(buffer, "\npid=") - buffer) ;
      strcpy( &buffer2[size], "\npid=?") ;
      size += strlen("\npid=?") ;
      buffer = 1 + strstr(buffer, "\npid=") ;
      buffer = strstr(buffer, "\n") ;
   }
   strcpy( &buffer2[size], buffer) ;

   LOG_CLEARBUFFER() ;
   LOG_PRINTF("%s", buffer2) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
