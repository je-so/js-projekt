/* title: Process
   Allows to creates a new process executable or
   a child process which executes a function.

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
#ifndef CKERN_PLATFORM_TASK_PROCESS_HEADER
#define CKERN_PLATFORM_TASK_PROCESS_HEADER

/* typedef: struct process_t
 * Make <process_t> an alias of <sys_process_t>. */
typedef sys_process_t                     process_t ;

/* typedef: process_task_f
 * Defines function type executed by <process_t>. */
typedef int                            (* process_task_f) (void * task_arg) ;

/* typedef: struct process_result_t
 * Export <process_result_t> into global namespace. */
typedef struct process_result_t           process_result_t ;

/* typedef: struct process_stdio_t
 * Export <process_stdio_t> into global namespace. */
typedef struct process_stdio_t            process_stdio_t ;


/* enums: process_state_e
 * Describes the state of a process.
 *
 * process_state_TERMINATED  - The process has exited normal and returned an exit code.
 * process_state_ABORTED     - The process has ended due to an abnormal condition (unhandled signal/exception).
 * process_state_RUNNABLE    - The process is in a runable state (either executing, waiting for execution or
 *                             waiting for a system call to complete)
 * process_state_STOPPED     - The process has been stopped by a STOP signal. It it receives a CONT signal
 *                             it goes to state process_state_RUNNABLE.
 * */
enum process_state_e {
   process_state_TERMINATED,
   process_state_ABORTED,
   process_state_RUNNABLE,
   process_state_STOPPED
} ;

typedef enum process_state_e              process_state_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_process
 * Unittest for pagecache module. */
int unittest_platform_task_process(void) ;
#endif


/* struct: process_result_t
 * Holds result of terminated process. */
struct process_result_t
{
   /* variable: returncode
    * Either the exit number or signal number.
    * If hasTerminatedAbnormal is true returncode carries
    * the signal number which lead to abnormal process termination. */
   int               returncode ;
   /* variable: state
    * Either process_state_TERMINATED or process_state_ABORTED. */
   process_state_e   state ;
} ;


/* struct: process_stdio_t
 * The process standard io channel redirections.
 * The process standard input, output and error channel are
 * redirected to the files given in this structure.
 * Redirection means that instead of reading from standard input
 * the process reads from <process_stdio_t.infile>.
 * And instead of writing to standard output or standard error
 * it writes to <process_stdio_t.outfile> resp. <process_stdio_t.errfile>.
 *
 * Attention:
 * Make sure that redirected files are automatically closed in case
 * another process is executed (i.e. have set their O_CLOEXEC flag). */
struct process_stdio_t {
   sys_iochannel_t   std_in ;
   sys_iochannel_t   std_out ;
   sys_iochannel_t   std_err ;
} ;

// group: lifetime

/* define: process_stdio_INIT_DEVNULL
 * Static initializer lets new process write and read from null device.
 * All written output is therefore ignored and reading returns always
 * with 0 bytes read. */
#define process_stdio_INIT_DEVNULL        { sys_iochannel_INIT_FREEABLE, sys_iochannel_INIT_FREEABLE, sys_iochannel_INIT_FREEABLE }

/* define: process_stdio_INIT_INHERIT
 * Static initializer lets new process inherit standard io channels. */
#define process_stdio_INIT_INHERIT        { sys_iochannel_STDIN, sys_iochannel_STDOUT, sys_iochannel_STDERR }

// group: update

/* function: redirectin_processstdio
 * Redirects standard input to given file.
 * Use value <iochannel_INIT_FREEABLE> to redirect standard input to device null.
 * Use value <iochannel_STDIN> to let child inherit standard error. */
void redirectin_processstdio(process_stdio_t * stdfd, sys_iochannel_t input_file) ;

/* function: redirectout_processstdio
 * Redirects standard output to given file.
 * Use value <iochannel_INIT_FREEABLE> to redirect standard output to device null.
 * Use value <iochannel_STDOUT> to let child inherit standard error. */
void redirectout_processstdio(process_stdio_t * stdfd, sys_iochannel_t output_file) ;

/* function: redirecterr_processstdio
 * Redirects standard error output to given file.
 * Use value <iochannel_INIT_FREEABLE> to redirect standard error to device null.
 * Use value <iochannel_STDERR> to let child inherit standard error. */
void redirecterr_processstdio(process_stdio_t * stdfd, sys_iochannel_t error_file) ;


/* struct: process_t
 * Represents system specific process. */
typedef sys_process_t                     process_t ;

// group: lifetime

/* define: process_INIT_FREEABLE
 * Static initializer. */
#define process_INIT_FREEABLE             sys_process_INIT_FREEABLE

/* function: init_process
 * Creates child process which executes a function.
 * Setting pointer stdfd to 0 has the same effect as providing a pointer to <process_stdio_t>
 * initialized with <process_stdio_INIT_DEVNULL>. */
int init_process(/*out*/process_t * process, process_task_f child_main, void * start_arg, process_stdio_t * stdfd) ;

/* function: initgeneric_process
 * Same as <init_process> except that it accepts functions with generic argument type.
 * The argument must have same size as sizeof(void*). */
int initgeneric_process(/*out*/process_t * process, process_task_f child_main, void * start_arg, process_stdio_t * stdfd) ;

/* function: initexec_process
 * Executes another program with same environment.
 * The parameter "filename" specifies the path to an executeable binary.
 * If it does not contain any "/" the program is searched for in the search path
 * specified with the PATH environment variable.
 * The parameter "arguments" is an array of pointers to C-strings
 * that represent the argument list available to the new program.
 * The first argument should point to the filename associated with the file being executed.
 * It must be terminated by a NULL pointer. */
int initexec_process(/*out*/process_t * process, const char * filename, const char * const * arguments, process_stdio_t * stdfd /*0 => /dev/null*/) ;

/* function: free_process
 * Frees resource associated with a process.
 * If a process is running it is killed.
 * So call <wait_process> before to ensure the process has finished properly.
 * ECHILD is returned if process does no longer exist. */
int free_process(process_t * process) ;

// group: query

/* function: name_process
 * Returns the system name of the calling process in the provided buffer.
 * The returned string is always null-terminated even if the provided buffer
 * is smaller then the name.
 *
 * Parameter:
 * namebuffer_size - The size in bytes of the name buffer.
 *                   If namebuffer_size is smaller than the size of the process name
 *                   the returned value is truncated. A value of 0 means nothing is returned.
 * name            - The name of the process is returned in this buffer.
 *                   The last character is always a '\0' byte.
 *                   Points to character buffer of size namebuffer_size.
 * name_size       - Is set to the size of the name including the trailing \0 byte.
 *                   If this pointer is NULL nothing is returned. */
int name_process(size_t namebuffer_size, /*out*/char name[namebuffer_size], /*out*/size_t * name_size) ;

/* function: state_process
 * Returns current state of process.
 *
 * Returns:
 * 0      - Out parameter current_state is set to current state.
 * ECHILD - Process does no longer exist. */
int state_process(process_t * process, /*out*/process_state_e * current_state) ;

// group: change

/* function: daemonize_process
 * Prepares this process to be a daemon process.
 * The working directory is set to '/', umask is set to S_IRWXO, the process becomes
 * the session leader and all 3 stdio channels are redirected to /dev/null.
 * This call works only if there are no other running threads besides the calling thread.
 * The reason is that after return only the calling thread is running and all other
 * threads have no chance to free their resources. After return the process id has been changed. */
int daemonize_process(process_stdio_t  * stdfd/*0 => /dev/null*/) ;

/* function: wait_process
 * Waits until process has terminated.
 * If the process changes into stopped state it will be continued until it terminates.
 * Calling the function more than once always returns the same result. */
int wait_process(process_t * process, /*out*/process_result_t * result) ;



// section: inline implementation

// group: process_stdio_t

/* define: redirectin_processstdio
 * Implements <process_stdio_t.redirectin_processstdio>. */
#define redirectin_processstdio(stdfd, input_file) \
         ((void)((stdfd)->std_in = input_file))

/* define: redirectout_processstdio
 * Implements <process_stdio_t.redirectout_processstdio>. */
#define redirectout_processstdio(stdfd, output_file) \
         ((void)((stdfd)->std_out = output_file))

/* define: redirecterr_processstdio
 * Implements <process_stdio_t.redirecterr_processstdio>. */
#define redirecterr_processstdio(stdfd, error_file) \
         ((void)((stdfd)->std_err = error_file))

// group: process_t

/* define: initgeneric_process
 * Implements <process_t.initgeneric_process>. */
#define initgeneric_process(process, child_main, start_arg, stdfd)   \
         ( __extension__ ({                                          \
               int (*_child_main) (typeof(start_arg)) ;              \
               _child_main = (child_main) ;                          \
               static_assert( sizeof(start_arg) == sizeof(void*),    \
                              "same as void*" ) ;                    \
               init_process(  process, (process_task_f) _child_main, \
                              (void*)start_arg, stdfd ) ;            \
         }))

#endif
