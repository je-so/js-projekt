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
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

/* struct: process_t
 * Wraps system handle of os specific process.
 * */
struct process_t {
   pid_t  sysid ;
   int    status ;
} ;

typedef struct childprocess_exec_t        childprocess_exec_t ;

struct childprocess_exec_t {
   __extension__ struct {
      const char *   filename ;
      char       **  arguments ;
      int            devnull ;
      int            errpipe ;
   } inparam ;
   __extension__ struct {
      int err ;
      int functionid ;
   } outparam ;
} ;


// section: Functions


static int childprocess_exec(void * param)
{
   childprocess_exec_t * execparam = (childprocess_exec_t *) param ;

   execparam->outparam.functionid = 0 ;
   while( -1 == dup2(execparam->inparam.devnull, STDIN_FILENO) ) {
      if (EINTR != errno) goto ABBRUCH ;
   }
   while( -1 == dup2(execparam->inparam.devnull, STDOUT_FILENO) ) {
      if (EINTR != errno) goto ABBRUCH ;
   }
   while( -1 == dup2(execparam->inparam.devnull, STDERR_FILENO) ) {
      if (EINTR != errno) goto ABBRUCH ;
   }

   execparam->outparam.functionid = 1 ;
   // TODO: use execvpe + implement setting environment variables !
   //       access to environment must be protected with mutex
   execvp( execparam->inparam.filename, execparam->inparam.arguments ) ;

ABBRUCH:
   execparam->outparam.err = errno ;
   int err ;
   do {
      err = write( execparam->inparam.errpipe, &execparam->outparam, sizeof(execparam->outparam)) ;
   } while( -1 == err && errno == EINTR ) ;
   return 127 ;
}

int new_process(/*out*/process_t ** process, void * child_parameter, int (*child_function) (void * child_parameter))
{
   int err ;
   pid_t pid ;
   process_t * newchild ;

   newchild = (process_t*) malloc(sizeof(process_t)) ;
   if (!newchild) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(sizeof(process_t)) ;
      goto ABBRUCH ;
   }

   //TODO:MULTITHREAD: all filedescriptors opened with O_CLOEXEC
   //TODO:MULTITHREAD: system handler for freeing thread resources ?!?
   pid = fork() ;
   if (-1 == pid) {
      err = errno ;
      LOG_SYSERR("fork", err) ;
      goto ABBRUCH ;
   }

   if ( pid == 0 ) {
      // NEW CHILD PROCESS
      int returncode = (child_function ? child_function(child_parameter) : 0) ;
      exit(returncode) ;
   }

   newchild->sysid  = pid ;
   newchild->status = 0 ;

   *process = newchild ;

   return 0 ;
ABBRUCH:
   free(newchild) ;
   LOG_ABORT(err) ;
   return err ;
}

int newexec_process( process_t ** process, const char * filename, const char * const * arguments )
{
   int err ;
   process_t          * childprocess = 0 ;
   int                  pipefd[2]    = { -1, -1 } ;
   childprocess_exec_t  execparam    = { .inparam = { filename, (char**) (intptr_t) arguments, -1, -1 }, .outparam = { 0, 0 } } ;

   execparam.inparam.devnull = open("/dev/null", O_RDWR|O_CLOEXEC) ;
   if (-1 == execparam.inparam.devnull) {
      err = errno ;
      LOG_SYSERR("open(/dev/null,O_RDWR)", err) ;
      goto ABBRUCH ;
   }

   if ( pipe2(pipefd,O_CLOEXEC) ) {
      err = errno ;
      LOG_SYSERR("pipe2", err) ;
      goto ABBRUCH ;
   }

   execparam.inparam.errpipe = pipefd[1] ;

   err = new_process( &childprocess, &execparam, &childprocess_exec) ;
   if (err) goto ABBRUCH ;

   // CHECK exec error
   close( pipefd[1] ) ;
   pipefd[1] = -1 ;

   ssize_t read_bytes = 0 ;

   do {
      read_bytes = read( pipefd[0], &execparam.outparam, sizeof(execparam.outparam)) ;
   } while( -1 == read_bytes && EINTR == errno) ;

   if (-1 == read_bytes) {
      err = errno ;
      LOG_SYSERR("read", err) ;
      goto ABBRUCH ;
   } else if (read_bytes) {
      // EXEC error
      err = execparam.outparam.err ? execparam.outparam.err : EINVAL ;
      if (execparam.outparam.functionid) {
         LOG_SYSERR("execvp(filename, arguments)", err) ;
         LOG_STRING(filename) ;
         for(size_t i = 0; arguments[i]; ++i) {
            LOG_INDEX("s",arguments,i) ;
         }
      } else {
         LOG_SYSERR("dup2", err) ;
      }
      goto ABBRUCH ;
   }

   if (  close(pipefd[0])
      || close(execparam.inparam.devnull)) {
      err = errno ;
      LOG_SYSERR("close",err) ;
      goto ABBRUCH ;
   }

   *process = childprocess ;

   return 0 ;
ABBRUCH:
   if (-1 != execparam.inparam.devnull)   close(execparam.inparam.devnull) ;
   if (-1 != pipefd[1])                   close(pipefd[1]) ;
   if (-1 != pipefd[0])                   close(pipefd[0]) ;
   (void) delete_process(&childprocess) ;
   LOG_ABORT(err) ;
   return err ;
}

int delete_process(process_t ** process)
{
   int err ;
   int err2 ;
   process_t * delobject = *process ;

   if (delobject) {
      *process = 0 ;

      err = 0 ;

      if (delobject->sysid) {
         pid_t pid = delobject->sysid ;

         if (kill(pid, SIGKILL)) {
            err = errno ;
            LOG_SYSERR("kill", err) ;
            LOG_INT(pid) ;
         }

         if (ESRCH != err) {
            err2 = wait_process(delobject, 0) ;
            if (err2) err = err2 ;
         }
      }

      free(delobject) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

static int updatestatus_process(process_t * process, bool doWait)
{
   int err ;
   int status ;
   pid_t pid = process->sysid ;
   pid_t wstat ;

   while(-1 == (wstat = waitpid(pid, &status, doWait ? (WUNTRACED|WCONTINUED) : (WUNTRACED|WCONTINUED|WNOHANG) ))) {
      if (EINTR != errno) {
         err = errno ;
         LOG_SYSERR("waitpid",err) ;
         LOG_INT(pid) ;
         goto ABBRUCH ;
      }
   }

   if (pid == wstat) {
      // read update

      if (  WIFEXITED(status)
         || WIFSIGNALED(status)) {
         process->sysid = 0 ;
      }

      process->status = status ;
   }

   return 0 ;
ABBRUCH:
   return err ;
}

static inline process_state_e statefromstatus_process(process_t * process)
{
   if (!process->sysid) {
      return process_state_TERMINATED ;
   } else if (WIFSTOPPED(process->status)) {
      return process_state_STOPPED ;
   }

   return process_state_RUNABLE ;
}

int wait_process(process_t * process, /*out*/process_result_t * result)
{
   int err ;
   pid_t pid = process->sysid ;

   while(process->sysid) {

      if (process_state_STOPPED == statefromstatus_process(process)) {
         kill(pid, SIGCONT) ;
      }

      err = updatestatus_process(process, true) ;
      if (err) goto ABBRUCH ;
   }

   if (WIFEXITED(process->status)) {
      if (result) {
         result->hasTerminatedAbnormal = false ;
         result->returncode            = WEXITSTATUS(process->status) ;
      }
   } else if (WIFSIGNALED(process->status)) {
      if (result) {
         result->hasTerminatedAbnormal = true ;
         result->returncode            = WTERMSIG(process->status) ;
      }
   } else {
      LOG_ERRTEXT(CONDITION_EXPECTED("WIFEXITED || WIFSIGNALED")) ;
      LOG_ERRTEXT(CONTEXT_INFO("waitpid(pid, &process->status, 0)")) ;
      LOG_INT(pid) ;
      LOG_INT(process->status) ;
      // TODO: replace with ABORT handler
      // TODO: implement abort handler as umgebung service which calls flush on LOG BUFFER
      assert(0) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int state_process(process_t * process, process_state_e * current_state)
{
   int err ;

   if (process->sysid) {
      err = updatestatus_process(process, false) ;
      if (err) goto ABBRUCH ;
   }

   *current_state = statefromstatus_process(process) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int hasterminated_process(process_t * process)
{
   int err ;
   process_state_e current_state ;

   err = state_process(process, &current_state) ;
   if (err) goto ABBRUCH ;

   return process_state_TERMINATED == current_state ? 0 : EAGAIN ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int hasstopped_process(process_t * process)
{
   int err ;
   process_state_e current_state ;

   err = state_process(process, &current_state) ;
   if (err) goto ABBRUCH ;

   return process_state_STOPPED == current_state ? 0 : EAGAIN ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_process,ABBRUCH)

static int childprocess_return(void * param)
{
   static_assert(sizeof(void*) >= sizeof(int), "") ;
   return (int) param ;
}

static int childprocess_endlessloop(void * param)
{
   (void) param ;
   for(;;) {
      usleep(1000) ;
   }
   return 0 ;
}

static int childprocess_signal(void * param)
{
   static_assert(sizeof(void*) >= sizeof(int), "") ;
   dprintf(10,"kill\n") ;
   kill( getpid(), (int)param) ;
   return 0 ;
}

static int chilprocess_execassert(void * param)
{
   (void) param ;
   assert(0) ;
   return 0 ;
}

static int childprocess_statechange(void * param)
{
   (void) param ;
   dprintf(10,"sleep\n") ;
   kill( getpid(), SIGSTOP) ;
   dprintf(10,"run\n") ;
   return 0 ;
}

static int test_initfree(void)
{
   process_t      *  process = 0 ;
   process_result_t  process_result ;
   process_state_e   process_state ;

   // TEST init, double free
   TEST(0 == process) ;
   TEST(0 == new_process(&process, (void*)0, &childprocess_return)) ;
   TEST(0 != process) ;
   TEST(0 < process->sysid) ;
   TEST(0 == process->status) ;
   TEST(0 == delete_process(&process)) ;
   TEST(0 == process) ;
   TEST(0 == delete_process(&process)) ;
   TEST(0 == process) ;

   // TEST implementation detail
   // status = 0 => Running state => not stopped !
   TEST(0 == process) ;
   TEST(0 == new_process(&process, (void*)0, &childprocess_return)) ;
   TEST(0 != process) ;
   TEST(0 == WIFSTOPPED(process->status)) ;
   TEST(EAGAIN == hasstopped_process(process)) ;
   TEST(0 == delete_process(&process)) ;
   TEST(0 == process) ;

   // TEST normal exit => result
   for(int i = 255; i >= 0; i -= 13) {
      // wait_process
      TEST(0 == process) ;
      TEST(0 == new_process(&process, (void*)i, &childprocess_return)) ;
      TEST(0 != process) ;
      TEST(0 < process->sysid) ;
      TEST(0 == process->status) ;
      TEST(0 == wait_process(process, &process_result)) ;
      TEST(0 == process_result.hasTerminatedAbnormal) ;
      TEST(i == process_result.returncode) ;
      TEST(0 == process->sysid) ;
      TEST(0 == hasterminated_process(process)) ;
      TEST(EAGAIN == hasstopped_process(process)) ;
      TEST(0 == delete_process(&process)) ;

      // hasterminated_process
      TEST(0 == process) ;
      TEST(0 == new_process(&process, (void*)i, &childprocess_return)) ;
      for(int i2 = 0; i2 < 100; ++i2) {
         usleep(10) ;
         int err = hasterminated_process(process) ;
         if (0 == hasterminated_process(process)) break ;
         TEST(err == EAGAIN) ;
      }
      TEST(0 == hasterminated_process(process)) ;
      TEST(0 == process->sysid) ;
      process_result = (process_result_t) { .hasTerminatedAbnormal = true, .returncode = (i+1) } ;
      TEST(0 == wait_process(process, &process_result)) ;
      TEST(0 == process_result.hasTerminatedAbnormal) ;
      TEST(i == process_result.returncode) ;
      TEST(0 == delete_process(&process)) ;

      // hasstopped_process
      TEST(0 == process) ;
      TEST(0 == new_process(&process, (void*)i, &childprocess_return)) ;
      for(int i2 = 0; i2 < 100; ++i2) {
         usleep(10) ;
         int err = hasstopped_process(process) ;
         TEST(err == EAGAIN) ;
         if (0 == process->sysid) break ;
      }
      TEST(0 == process->sysid) ;
      process_result = (process_result_t) { .hasTerminatedAbnormal = true, .returncode = (i+1) } ;
      TEST(0 == wait_process(process, &process_result)) ;
      TEST(0 == process_result.hasTerminatedAbnormal) ;
      TEST(i == process_result.returncode) ;
      TEST(0 == delete_process(&process)) ;

      // run last testcase with 0
      if (0 < i && i < 13) i = 13 ;
   }

   // TEST double wait (already waited)
   TEST(0 == process) ;
   TEST(0 == new_process(&process, (void*)11, &childprocess_return)) ;
   TEST(0 != process) ;
   TEST(0 == wait_process(process, &process_result)) ;
   TEST(0 == process_result.hasTerminatedAbnormal) ;
   TEST(11== process_result.returncode) ;
   TEST(0 == process->sysid) ;
   TEST(0 == wait_process(process, &process_result)) ;
   TEST(0 == process_result.hasTerminatedAbnormal) ;
   TEST(11== process_result.returncode) ;
   TEST(0 == process->sysid) ;
   TEST(0 == hasterminated_process(process)) ;
   TEST(EAGAIN == hasstopped_process(process)) ;
   TEST(0 == delete_process(&process)) ;
   TEST(0 == process) ;

   // TEST endless loop => delete ends process
   for(int i = 0; i < 32; ++i) {
      TEST(0 == process) ;
      TEST(0 == new_process(&process, (void*)0, &childprocess_endlessloop)) ;
      TEST(0 != process) ;
      TEST(EAGAIN == hasterminated_process(process)) ;
      TEST(EAGAIN == hasstopped_process(process)) ;
      TEST(0 <  process->sysid) ;
      TEST(0 == process->status) ;
      TEST(0 == delete_process(&process)) ;
      TEST(0 == process) ;
   }

   // TEST ESRCH
   TEST(0 == process) ;
   TEST(0 == new_process(&process, (void*)0, &childprocess_return)) ;
   TEST(0 != process) ;
   TEST(0 == wait_process(process, 0)) ;
   TEST(0 == process->sysid) ;
   process->sysid = 65535 ;
   TEST(ESRCH == delete_process(&process)) ;
   TEST(0 == process) ;

   // TEST ECHILD
   TEST(0 == process) ;
   TEST(0 == new_process(&process, (void*)0, &childprocess_return)) ;
   TEST(0 != process) ;
   TEST(0 == wait_process(process, 0)) ;
   process_state = process_state_RUNABLE ;
   TEST(0 == state_process(process, &process_state)) ;
   TEST(process_state_TERMINATED == process_state) ;
   TEST(0 == process->sysid) ;
   process->sysid = 65535 ;
   TEST(ECHILD == state_process(process, &process_state)) ;
   process->sysid = 0 ;
   TEST(0 == delete_process(&process)) ;
   TEST(0 == process) ;

   return 0 ;
ABBRUCH:
   (void) delete_process(&process) ;
   return EINVAL ;
}

static int test_abnormalexit(void)
{
   process_t      *  process = 0 ;
   process_result_t  process_result ;
   int               pipefd[2] = { -1, -1 } ;

   TEST(0 == pipe2(pipefd,O_CLOEXEC)) ;
   TEST(-1 != dup2(pipefd[1], 10)) ;

   // TEST init, wait
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
      char buffer[100] = { 0 } ;
      TEST(0 == new_process(&process, (void*)snr, &childprocess_signal)) ;
      // wait until child has started
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "kill\n")) ;
      TEST(0 == wait_process(process, &process_result)) ;
      TEST(0 == hasterminated_process(process)) ;
      TEST(EAGAIN == hasstopped_process(process)) ;
      if (process_result.hasTerminatedAbnormal) {
         TEST(snr == process_result.returncode) ;
         ++ signal_count ;
      } else {
         // signal ignored
         TEST(0   == process_result.returncode) ;
      }
      TEST(0 == delete_process(&process)) ;
      TEST(0 == process) ;
   }
   TEST(signal_count > nrelementsof(test_signals)/2) ;

   // TEST delete works if process has already ended
   for(unsigned i = 0; i < 16; ++i) {
      char buffer[100] = { 0 } ;
      TEST(0 == process) ;
      TEST(0 == new_process(&process, (void*)0/*no signal is send (special value to kill)*/, &childprocess_signal)) ;
      // wait until child has started
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "kill\n")) ;
      usleep(10) ;
      TEST(0 == delete_process(&process)) ;
      TEST(0 == process) ;
   }


   TEST(0 == close(10)) ;
   TEST(0 == close(pipefd[0])) ;
   pipefd[0] = -1 ;
   TEST(0 == close(pipefd[1])) ;
   pipefd[1] = -1 ;

   return 0 ;
ABBRUCH:
   (void) delete_process(&process) ;
   close(10) ;
   if (-1 != pipefd[0]) {
      close(pipefd[0]) ;
      close(pipefd[1]) ;
   }
   return EINVAL ;
}

static int test_assert(void)
{
   process_t      *  process = 0 ;
   process_result_t  process_result ;
   int               fd_stderr = -1 ;
   int               pipefd[2] = { -1, -1 } ;

   fd_stderr = dup(STDERR_FILENO) ;
   TEST(-1 != fd_stderr) ;
   TEST(0 == pipe2(pipefd,O_CLOEXEC)) ;
   TEST(-1 != dup2(pipefd[1], STDERR_FILENO)) ;

   TEST(0 == new_process(&process, 0, &chilprocess_execassert)) ;
   TEST(0 == wait_process(process, &process_result)) ;
   TEST(0 != process_result.hasTerminatedAbnormal) ;
   TEST(SIGABRT == process_result.returncode) ;
   TEST(0 == delete_process(&process)) ;

   char buffer[100] = { 0 } ;
   TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
   const char * testassert_log = strstr(buffer, __FILE__) ;
   TEST(0 != testassert_log) ;
   LOG_STRING(testassert_log) ;

   TEST(-1 != dup2(fd_stderr, STDERR_FILENO)) ;
   TEST(0 == close(fd_stderr)) ;
   fd_stderr = -1 ;
   TEST(0 == close(pipefd[0])) ;
   pipefd[0] = -1 ;
   TEST(0 == close(pipefd[1])) ;
   pipefd[1] = -1 ;

   return 0 ;
ABBRUCH:
   (void) delete_process(&process) ;
   if (-1 != fd_stderr) {
      close(fd_stderr) ;
   }
   if (-1 != pipefd[0]) {
      close(pipefd[0]) ;
      close(pipefd[1]) ;
   }
   return EINVAL ;
}

static int test_statequery(void)
{
   process_t      *  process = 0 ;
   process_result_t  process_result ;
   int               pipefd[2] = { -1, -1 } ;

   TEST(0 == pipe2(pipefd,O_CLOEXEC)) ;
   TEST(-1 != dup2(pipefd[1], 10)) ;

   // TEST state stopped
   for(unsigned i = 0; i < 4; ++i) {
      char buffer[100] = { 0 } ;

      // use wait_process (to end process)
      TEST(0 == new_process(&process, (void*)SIGSTOP, &childprocess_signal)) ;
      // wait until child has started
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "kill\n")) ;
      for(int i2 = 0; i2 < 1000; ++i2) {
         int err = hasstopped_process(process) ;
         if (0 == err) {
            break ;
         }
         TEST(EAGAIN == err) ;
         usleep(10) ;
      }
      TEST(0 == hasstopped_process(process)) ;
      TEST(EAGAIN == hasterminated_process(process)) ;
      process_result = (process_result_t) { .hasTerminatedAbnormal = true, .returncode = 1 } ;
      TEST(0 == wait_process(process, &process_result)) ;
      TEST(0 == process_result.hasTerminatedAbnormal) ;
      TEST(0 == process_result.returncode) ;
      TEST(0 == delete_process(&process)) ;

      // use delete_process (to end process)
      TEST(0 == new_process(&process, (void*)SIGSTOP, &childprocess_signal)) ;
      // wait until child has started
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "kill\n")) ;
      for(int i2 = 0; i2 < 100; ++i2) {
         int err = hasstopped_process(process) ;
         if (0 == err) break ;
         TEST(EAGAIN == err) ;
         usleep(10) ;
      }
      TEST(0 == hasstopped_process(process)) ;
      TEST(EAGAIN == hasterminated_process(process)) ;
      TEST(0 == delete_process(&process)) ;
   }

   // TEST childprocess_statechange sleeps
   TEST(0 == process) ;
   TEST(0 == new_process(&process, (void*)0, &childprocess_statechange)) ;
   {
      // wait until child has started
      char buffer[100] = { 0 } ;
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "sleep\n")) ;
   }
   for(int i2 = 0; i2 < 1000; ++i2) {
      int err = hasstopped_process(process) ;
      if (0 == err) {
         break ;
      }
      TEST(EAGAIN == err) ;
      usleep(10) ;
   }
   TEST(0 == hasstopped_process(process)) ;
   TEST(0 == delete_process(&process)) ;

   // TEST state query returns latest state
   TEST(0 == process) ;
   TEST(0 == new_process(&process, (void*)0, &childprocess_statechange)) ;
   {
      // wait until child has started
      char buffer[100] = { 0 } ;
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "sleep\n")) ;
   }
   usleep(10000) ;
   TEST(0 == kill(process->sysid, SIGCONT)) ;
   {
      // wait until child run again
      char buffer[100] = { 0 } ;
      TEST(0 <= read(pipefd[0], buffer, sizeof(buffer)-1)) ;
      TEST(0 == strcmp(buffer, "run\n")) ;
   }
   usleep(10000) ;
   TEST(0 != process->sysid) ;
   TEST(0 == updatestatus_process(process, false)) ;
   TEST(0 == process->sysid) ; // process terminated state
   TEST(0 == delete_process(&process)) ;
   TEST(0 == process) ;


   TEST(0 == close(10)) ;
   TEST(0 == close(pipefd[0])) ;
   pipefd[0] = -1 ;
   TEST(0 == close(pipefd[1])) ;
   pipefd[1] = -1 ;

   return 0 ;
ABBRUCH:
   (void) delete_process(&process) ;
   close(10) ;
   if (-1 != pipefd[0]) {
      close(pipefd[0]) ;
      close(pipefd[1]) ;
   }
   return EINVAL ;
}

static int test_exec(void)
{
   process_t      *  process = 0 ;
   process_result_t  process_result ;
   struct stat       statbuf ;
   char              numberstr[20] ;
   const char      * testcase1_args[] = { "testchildprocess", "1", numberstr, 0 } ;

   if (0 != stat("bin/testchildprocess", &statbuf)) {
      testcase1_args[0] = "testchildprocess_Debug" ;
   }

   // TEST executing child process
   for(int i = 0; i <= 35; i += 7) {
      snprintf(numberstr, sizeof(numberstr), "%d", i) ;
      TEST(0 == newexec_process(&process, testcase1_args[0], testcase1_args )) ;
      TEST(0 == wait_process(process, &process_result)) ;
      TEST(0 == process_result.hasTerminatedAbnormal) ;
      TEST(i == process_result.returncode) ;
      TEST(0 == delete_process(&process)) ;
   }

   return 0 ;
ABBRUCH:
   (void) delete_process(&process) ;
   return EINVAL ;
}

int unittest_os_process()
{
   char            * oldpath = getenv("PATH") ? strdup(getenv("PATH")) : 0 ;
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;

   TEST(0 == getenv("PATH") || oldpath) ;
   setenv("PATH","./bin/", 1) ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_abnormalexit())   goto ABBRUCH ;
   if (test_assert())         goto ABBRUCH ;
   if (test_statequery())     goto ABBRUCH ;
   if (test_exec())           goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   setenv("PATH", oldpath, 1) ;
   free(oldpath) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   free(oldpath) ;
   return EINVAL ;
}

#endif
