/* title: ProcessManagement
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

   file: C-kern/api/platform/process.h
    Header file of <ProcessManagement>.

   file: C-kern/platform/Linux/process.c
    Implementation file <ProcessManagement Linux>.
*/
#ifndef CKERN_PLATFORM_PROCESS_HEADER
#define CKERN_PLATFORM_PROCESS_HEADER

#include "C-kern/api/io/filedescr.h"

/* typedef: process_t
 * Represents op. system specific process. */
typedef sys_process_t                  process_t ;

/* typedef: struct process_task_f
 * Defines function type executed by <process_t>. */
typedef int                         (* process_task_f) (void * task_arg) ;

/* typedef: process_result_t typedef
 * Export <process_result_t>. */
typedef struct process_result_t        process_result_t ;

/* struct: process_ioredirect_t
 * Exports <process_ioredirect_t>. */
typedef struct process_ioredirect_t    process_ioredirect_t ;


/* enums: process_state_e
 * Describes the state of a process.
 *
 * process_state_RUNNABLE    - The process is in a runable state (either executing, waiting for execution or
 *                             waiting for a system call to complete)
 * process_state_STOPPED     - The process has been stopped by a STOP signal. After receiving of a CONT signal
 *                             it goes to state RUNABLE
 * process_state_TERMINATED  - The process has exited normal and returned an exit code.
 * process_state_ABORTED     - The process has ended due to an abnormal condition (unhandled signal/exception). */
enum process_state_e {
    process_state_RUNNABLE
   ,process_state_STOPPED
   ,process_state_TERMINATED
   ,process_state_ABORTED
} ;

typedef enum process_state_e        process_state_e ;


// section: Functions

// group: query

/* function: name_process
 * Returns the system name of this process in the provided buffer.
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
 *                   If this pointer is not NULL nothing is returned. */
int name_process(size_t namebuffer_size, /*out*/char name[namebuffer_size], /*out*/size_t * name_size) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_process
 * Unittest for pagecache module. */
int unittest_platform_process(void) ;
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


/* struct: process_ioredirect_t
 * The process standard file redirections.
 * The process standard input, output and error channel are
 * redirected to the files given in this structure.
 * Redirection means that instead of reading from standard input
 * the process reads from <process_ioredirect_t.infile>.
 * And instead of writing to standard output or standard error
 * it writes to <process_ioredirect_t.outfile> resp. <process_ioredirect_t.errfile>.
 *
 * Attention:
 * Make sure that redirected files are automatically closed in case
 * another process is executed (i.e. have set their O_CLOEXEC flag). */
struct process_ioredirect_t {
   filedescr_t  std_in ;
   filedescr_t  std_out ;
   filedescr_t  std_err ;
} ;

// group: lifetime

/* define: process_ioredirect_INIT_DEVNULL
 * Static initializer lets new process write and read from null device.
 * All written output is therefore ignored and reading returns always
 * with 0 bytes read. */
#define process_ioredirect_INIT_DEVNULL      { filedescr_INIT_FREEABLE, filedescr_INIT_FREEABLE, filedescr_INIT_FREEABLE }

/* define: process_ioredirect_INIT_INHERIT
 * Static initializer lets new process inherit standard io channels. */
#define process_ioredirect_INIT_INHERIT      { filedescr_STDIN, filedescr_STDOUT, filedescr_STDERR }

/* function: setstdin_processioredirect
 * Redirects standard input to given file.
 * Use value <filedescr_INIT_FREEABLE> to redirect standard input to device null. */
void setstdin_processioredirect(process_ioredirect_t * ioredirect, filedescr_t input_file) ;

/* function: setstdout_processioredirect
 * Redirects standard output to given file.
 * Use value <filedescr_INIT_FREEABLE> to redirect standard output to device null. */
void setstdout_processioredirect(process_ioredirect_t * ioredirect, filedescr_t output_file) ;

/* function: setstderr_processioredirect
 * Redirects standard error output to given file.
 * Use value <filedescr_INIT_FREEABLE> to redirect standard error to device null. */
void setstderr_processioredirect(process_ioredirect_t * ioredirect, filedescr_t error_file) ;


// section: process_t

// group: lifetime

/* define: process_INIT_FREEABLE
 * Static initializer. */
#define process_INIT_FREEABLE       { sys_process_INIT_FREEABLE }

/* function: init_process
 * Creates child process which executes a function. */
int init_process(/*out*/process_t * process, process_task_f child_main, void * start_arg, process_ioredirect_t * ioredirection /*0 => /dev/null*/) ;

/* function: initexec_process
 * Executes another program with same environment.
 * The parameter "filename" specifies the path to an executeable binary.
 * If it does not contain any "/" the program is searched for in the search path
 * specified with the PATH environment variable.
 * The parameter "arguments" is an array of pointers to C-strings
 * that represent the argument list available to the new program.
 * The first argument should point to the filename associated with the file being executed.
 * It must be terminated by a NULL pointer. */
int initexec_process(/*out*/process_t * process, const char * filename, const char * const * arguments, process_ioredirect_t * ioredirection /*0 => /dev/null*/) ;

/* function: free_process
 * Frees resource associated with a process.
 * If a process is running it is killed.
 * So call <wait_process> before to ensure the process has finished properly.
 * ECHILD is returned if process does no longer exist. */
int free_process(process_t * process) ;

// group: query

/* function: state_process
 * Returns current state of process.
 *
 * Returns:
 * 0      - Out parameter current_state is set to current state.
 * ECHILD - Process does no longer exist. */
int state_process(process_t * process, /*out*/process_state_e * current_state) ;

// group: change

/* function: wait_process
 * Waits until process has terminated.
 * If the process changes into stopped state it will be continued until it terminates.
 * Calling the function more than once always returns the same result. */
int wait_process(process_t * process, /*out*/process_result_t * result) ;


// section: inline implementation

/* define: init_process
 * Calls <process_t.init_process> with adapted function pointer. */
#define init_process(process, child_main, start_arg, ioredirection)                 \
   /*do not forget to adapt definition in process.c test section*/                  \
   ( __extension__ ({ int _err ;                                                    \
      int (*_child_main) (typeof(start_arg)) = (child_main) ;                       \
      static_assert(sizeof(start_arg) <= sizeof(void*), "cast 2 void*") ;           \
      _err = init_process(process, (process_task_f) _child_main,                    \
                                 (void*)start_arg, ioredirection ) ;                \
      _err ; }))

/* define: setstdin_processioredirect
 * Implements <process_ioredirect_t.setstdin_processioredirect>. */
#define setstdin_processioredirect(ioredirect, input_file) \
   ((void)((ioredirect)->std_in = input_file))

/* function: setstdout_processioredirect
 * Implements <process_ioredirect_t.setstdout_processioredirect>. */
#define setstdout_processioredirect(ioredirect, output_file) \
   ((void)((ioredirect)->std_out = output_file))

/* function: setstderr_processioredirect
 * Implements <process_ioredirect_t.setstderr_processioredirect>. */
#define setstderr_processioredirect(ioredirect, error_file) \
   ((void)((ioredirect)->std_err = error_file))

#endif

