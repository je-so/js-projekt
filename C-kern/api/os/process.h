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

   file: C-kern/api/os/process.h
    Header file of <ProcessManagement>.

   file: C-kern/os/Linux/process.c
    Implementation file <ProcessManagement Linux>.
*/
#ifndef CKERN_OS_PROCESS_HEADER
#define CKERN_OS_PROCESS_HEADER

/* typedef: process_t typedef
 * Export <process_t>. */
typedef struct process_t            process_t ;

/* typedef: process_result_t typedef
 * Export <process_result_t>. */
typedef struct process_result_t     process_result_t ;

/* enums: process_state_e
 * Describes the state of a process.
 *
 * process_state_RUNABLE     - The process is in a runable state (either executing, waiting for execution or
 *                             waiting for a system call to complete)
 * process_state_STOPPED     - The process has been stopped by a STOP signal. After receiving of a CONT signal
 *                             it goes to state RUNABLE
 * process_state_TERMINATED  - The process has exited normal and returned an exit code or due to an abnormal
 *                             condition (unhandled signal/exception). */
enum process_state_e {
    process_state_RUNABLE
   ,process_state_STOPPED
   ,process_state_TERMINATED
} ;

typedef enum process_state_e        process_state_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_process
 * Unittest for pagecache module. */
extern int unittest_os_process(void) ;
#endif


/* struct: process_result_t
 * Holds result of terminated process. */
struct process_result_t
{
   /* variable: returncode
    * Either the exit number or signal number.
    * If hasTerminatedAbnormal is true returncode carries
    * the signal number which lead to abnormal process termination. */
   int   returncode ;
   /* variable: hasTerminatedAbnormal
    * Is true if process terminated due to an abnormal condition. */
   bool  hasTerminatedAbnormal ;
} ;


/* struct: process_t
 * Internal type. */
struct process_t ;

// group: lifetime

/* function: new_process
 * Creates child process which executes a function. */
extern int new_process(/*out*/process_t ** process, void * child_parameter, int (*child_function) (void * child_parameter)) ;

/* function: newexec_process
 * Creates child porcess and executes another program.
 * The parameter "filename" specifies the path to an
 * executeable binary. If it does not contain any "/"
 * the rpogram is searched for in the search path
 * specified with the PATH environment variable.
 * The parameter "arguments" is an array of pointers to C-strings
 * that represent the argument list available to the new program.
 * The first argument should point to the filename associated with the file being executed.
 * It must be terminated by a NULL pointer. */
extern int newexec_process(/*out*/process_t ** process, const char * filename, const char * const * arguments ) ;

/* function: delete_process
 * Frees resource associated with a process.
 * If a process is running it is killed.
 * So call <waitexit_process> before to ensure
 * the process has finished properly.
 * ESRCH is returned if process does no longer exist. */
extern int delete_process(process_t ** process) ;

// group: query

/* function: state_process
 * Returns 0 if current_state of process could be determined.
 * Else ECHILD if process does no longer exist. */
extern int state_process(process_t * process, process_state_e * current_state) ;

/* function: hasterminated_process
 * Returns 0 if process has terminated else EAGAIN.
 * ECHILD is returned if process does no longer exist. */
extern int hasterminated_process(process_t * process) ;

/* function: hasterminated_process
 * Returns 0 if process has stopped else EAGAIN.
 * ECHILD is returned if process does no longer exist. */
extern int hasstopped_process(process_t * process) ;

// group: wait

/* function: wait_process
 * Waits until process has terminated.
 * If the process changes into stopped state
 * it will be continued until it terminates. */
extern int wait_process(process_t * process, /*out*/process_result_t * result) ;


// section: inline implementation


#endif

