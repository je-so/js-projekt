/* title: Thread

   Encapsulates os specific threading model.

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

   file: C-kern/api/platform/task/thread.h
    Header file of <Thread>.

   file: C-kern/platform/Linux/task/thread.c
    Linux specific implementation <Thread Linux>.
*/
#ifndef CKERN_PLATFORM_TASK_THREAD_HEADER
#define CKERN_PLATFORM_TASK_THREAD_HEADER

// forward
struct slist_node_t ;

/* typedef: struct thread_t
 * Export <thread_t>. */
typedef struct thread_t                   thread_t ;

/* typedef: thread_f
 * Defines function type executed by <thread_t>. */
typedef int                            (* thread_f) (void * main_arg) ;

/* variable: gt_thread
 * Points to own <thread_t> object (current thread self). */
extern __thread  thread_t                 gt_thread ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_thread
 * Tests <thread_t> interface. */
int unittest_platform_task_thread(void) ;
#endif


/* struct: thread_t
 * Describes a system thread.
 *
 * Attention:
 * Never forget to lock / unlock a thread object before you access
 * the fields which can be changed by other threads.
 * This ensures that you read a consistent state and that on some
 * architectures proper read and write barriers are executed.
 *
 * Use <lock_thread> and <unlock_thread> for that matter. */
struct thread_t {
   /* variable: wlistnext
    * Points to next thread which waits on the same condition in <waitlist_t>.
    * TODO: remove variable wlistnext (see <lock> for explanation) */
   struct
   slist_node_t *    wlistnext ;
   /* variable: main_task
    * Function executed after thread has been created. */
   thread_f          main_task ;
   /* variable: main_arg
    * Parameter of executed <main_task> function. */
   void *            main_arg ;
   /* variable: returncode
    * Contains the return value of <main_task>.
    * This value is only valid after <main_task> has returned. */
   int               returncode ;
   /* variable: sys_thread
    * Contains system specific ID of thread. */
   sys_thread_t      sys_thread ;
   /* variable: stackframe
    * Contains start address of the whole stack frame.
    * The stack fram contains a signal stck and a separate thread stack.
    * Both are surrounded by pages which have no access protection. */
   uint8_t *         stackframe ;
   /* variable: continuecontext
    * Contains thread machine context before <main_task> is called.
    * This context could be is used in any aborthandler.
    * The aborthandler should call <abort_thread> which sets returncode to value ENOTRECOVERABLE
    * and calls setcontext (see: man 2 setcontext) with <continuecontext> as argument. */
   ucontext_t        continuecontext ;
} ;

// group: initonce

/* function: initonce_thread
 * Calculates some internal offsets, called from <init_maincontext>.
 * It must be called after <valuecache_t> is fully operational
 * The reason is that function <pagesize_vm> needs <valuecache_t>. */
int initonce_thread(void) ;

/* function: freeonce_thread
 * Does nothing. Called from <free_maincontext>. */
int freeonce_thread(void) ;

// group: lifetime

/* function: new_thread
 * Creates and starts a new system thread.
 * On success the parameter threadobj points to the new thread object.
 * The thread has to do some internal initialization after running the first time
 * and before thread_main is called.
 * If the internal preparation goes wrong <maincontext_t.abort_maincontext> is called.
 * It is unspecified if thread_main is called before new_thread returns.
 * On Linux new_thread returns before the newly created thread is scheduled. */
int new_thread(/*out*/thread_t ** threadobj, thread_f thread_main, void * main_arg) ;

/* define: newgeneric_thread
 * Same as <new_thread> except that it accepts functions with generic argument type.
 * The function argument must be of size sizeof(void*). */
int newgeneric_thread(/*out*/thread_t ** threadobj, thread_f thread_main, void * main_arg) ;

/* function: delete_thread
 * Calls <join_thread> (if not already called) and deletes resources.
 * This function waits until the thread has terminated. So be careful ! */
int delete_thread(thread_t ** threadobj) ;

// group: query

/* function: self_thread
 * Returns a pointer to the own thread object. */
thread_t * self_thread(void) ;

/* function: returncode_thread
 * Returns the returncode of the joined thread.
 * The returncode is only valid if <join_thread> was called before.
 * 0 is returned in case the thread has not already been joined. */
int returncode_thread(const thread_t * threadobj) ;

/* function: maintask_thread
 * Returns <thread_t.main_task>.
 * This value is set to thread_main given as parameter in <new_thread>. */
thread_f maintask_thread(const thread_t * threadobj) ;

/* function: mainarg_thread
 * Reads <thread_t.main_arg> field of <thread_t> object.
 * This value is set to main_arg given as parameter in <new_thread>. */
void * mainarg_thread(const thread_t * threadobj) ;

/* function: ismain_thread
 * Returns true if the calling thread is the main thread. */
bool ismain_thread(void) ;

// group: synchronize

/* function: join_thread
 * The function suspends execution of the caller until threadobj terminates.
 * If the thread has already been joined this function returns immediately.
 * The error EDEADLK is returned if you want to join <self_thread>.
 * The error ESRCH is returned if the thread has exited already (if threadobj is not updated properly). */
int join_thread(thread_t * threadobj) ;

// group: change-run-state

/* function: settask_thread
 * Changes values returned by <maintask_thread> and <mainarg_thread>. */
void settask_thread(thread_t * thread, thread_f main, void * main_arg) ;

/* function: suspend_thread
 * The calling thread will sleep until <resume_thread> is called.
 * <resume_thread> must be called from another thread.
 *
 * Linux specific:
 * Internally sigwaitinfo wirh signal SIGINT is used to sleep.
 *
 * Attention !:
 * It is possible that signals are received which are generated from outside this process
 * therefore make sure with some other mechanism that returning
 * from <suspend_thread> emanates from a corresponding call to <resume_thread>. */
void suspend_thread(void) ;

/* function: resume_thread
 * The thread which is refered by threadobj is woken up.
 * The status of resume is conserved if the other thread is currently not sleeping.
 * So the next call to suspend will return immediately.
 * This behaviour is needed cause a thread calling suspend could be preempted before
 * it really enters a sleep state.
 *
 * Linux specific:
 * Internally pthread_kill with signal SIGINT is used to wake up a another
 * thread from sleeping. */
void resume_thread(thread_t * threadobj) ;

/* function: sleepms_thread
 * Makes calling thread to sleep msec milli-seconds. */
void sleepms_thread(uint32_t msec) ;

/* function: yield_thread
 * Schedules another thread on this Processor. */
void yield_thread(void) ;

/* function: exit_thread
 * Ends the calling thread and sets retcode as its return code.
 * If the caller is the main thread the value EPROTO is returned and
 * nothing is done. The main thread must calle <free_maincontext> and exit(retcode).
 * No cleanup handlers are executed. */
int exit_thread(int retcode) ;

// group: abort

/* function: yield_thread
 * Aborts the calling thread.
 * This functions sets returncode (see <returncode_thread>) to value ENOTRECOVERABLE
 * and continues execution at the place which was marked with <setcontinue_thread> previously.
 * No cleanup handlers are executed. */
void abort_thread(void) ;

/* function: setcontinue_thread
 * Stores the current execution context and returns 0 on success.
 * Parameter is_abort is set to false if <setcontinue_thread> is called
 * to store the execution context where execution should continue after
 * <abort_thread> has been called.
 * Parameter is_abort is set to true if <setcontinue_thread> returns as
 * a reaction to a previous call to <abort_thread>.
 * For any started thread <setcontinue_thread> is called before thread_main
 * (parameter in <new_thread>) is called. The main thread which calls <init_maincontext>
 * must call <setcontinue_thread> explicitly. */
int setcontinue_thread(bool * is_abort) ;



// section: inline implementation

/* define: ismain_thread
 * Implements <thread_t.ismain_thread>. */
#define ismain_thread()                   (gt_thread.stackframe == 0)

/* define: newgeneric_thread
 * Implements <thread_t.newgeneric_thread>. */
#define newgeneric_thread(threadobj, thread_main, main_arg)       \
         ( __extension__ ({                                       \
            int (*_thread_main) (typeof(main_arg)) ;              \
            _thread_main = (thread_main) ;                        \
            static_assert( sizeof(main_arg) == sizeof(void*),     \
                           "same as void*") ;                     \
            new_thread( threadobj, (thread_f)_thread_main,        \
                        (void*)main_arg) ;                        \
         }))

/* define: returncode_thread
 * Implements <thread_t.returncode_thread>.
 * > (threadobj)->returncode */
#define returncode_thread(threadobj)      ((threadobj)->returncode)

/* define: self_thread
 * Implements <thread_t.self_thread>. */
#define self_thread()                     (&gt_thread)

/* define: maintask_thread
 * Implements <thread_t.maintask_thread>. */
#define maintask_thread(threadobj)        ((threadobj)->main_task)

/* define: mainarg_thread
 * Implements <thread_t.mainarg_thread>. */
#define mainarg_thread(threadobj)         ((threadobj)->main_arg)

/* define: setcontinue_thread
 * Implements <thread_t.setcontinue_thread>. */
#define setcontinue_thread(is_abort)         \
         ( __extension__ ({                  \
            gt_thread.returncode = 0 ;       \
            int _err = getcontext(           \
               &gt_thread.continuecontext) ; \
            if (_err) {                      \
               _err = errno ;                \
               TRACESYSERR_LOG(              \
                     "getcontext", _err) ;   \
            }                                \
            if (gt_thread.returncode) {      \
               *(is_abort) = true ;          \
            } else {                         \
               *(is_abort) = false ;         \
            }                                \
            _err ;                           \
         }))

/* define: settask_thread
 * Implements <thread_t.settask_thread>. */
#define settask_thread(thread, _main, _main_arg)   \
         ( __extension__ ({                        \
            typeof(thread) _thread = (thread) ;    \
            _thread->main_task = (_main) ;         \
            (void) (                               \
            _thread->main_arg  = (_main_arg)) ;    \
         }))

/* define: yield_thread
 * Implements <thread_t.yield_thread>. */
#define yield_thread()                    (pthread_yield())

#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
/* define: initonce_thread
 * Implement <thread_t.initonce_thread> as noop if !((KONFIG_SUBSYS)&THREAD) */
#define initonce_thread()                 (0)
/* define: freeonce_thread
 * Implement <thread_t.freeonce_thread> as noop if !((KONFIG_SUBSYS)&THREAD) */
#define freeonce_thread()                 (0)
#endif
#undef THREAD

#endif
