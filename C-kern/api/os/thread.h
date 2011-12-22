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

   file: C-kern/api/os/thread.h
    Header file of <Thread>.

   file: C-kern/os/Linux/thread.c
    Linux specific implementation <Thread Linux>.
*/
#ifndef CKERN_OS_THREAD_HEADER
#define CKERN_OS_THREAD_HEADER

#include "C-kern/api/aspect/callback/task.h"
#include "C-kern/api/memory/memblock.h"

/* typedef: struct thread_t
 * Export <thread_t>. */
typedef struct thread_t                thread_t ;

/* typedef: thread_stack_t
 * Make <thread_stack_t> an alias for <memblock_t>. */
typedef memblock_t                     thread_stack_t ;

// globals
extern __thread  thread_t              gt_thread_self ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_thread
 * Tests <thread_t> interface. */
extern int unittest_os_thread(void) ;
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
   /* variable: lock
    * Protects access to fields of <thread_t>.
    * Use it before you read fields which are updated by another thread. */
   sys_mutex_t       lock ;
   /* variable: wlistnext
    * Points to next thread which waits on the same condition in <waitlist_t>. */
   thread_t          * wlistnext ;
   /* variable: task
    * Contains value to signal thread what to do after wakeup. */
   task_callback_t   task ;
   /* variable: sys_thread
    * Contains system specific ID of thread. It has type <sys_thread_t>. */
   sys_thread_t      sys_thread ;
   /* variable: returncode
    * Contains the value <task> returns.
    * This value is only valid after <task> has returned.
    * This value reflects the value of the first thread which did not return 0.
    * This value is 0 if all threads returned 0. */
   int               returncode ;
   /* variable: stackframe
    * Contains the mapped memory used as stack. */
   thread_stack_t    stackframe ;
   /* variable: nr_threads
    * Contains the number of threads in this group.
    * All threads share the same task function and the same argument at the beginning.
    * Use <groupnext> to iterate over the whole group. */
   uint32_t          nr_threads ;
   /* variable: groupnext
    * Points to next thread in group of throuds. */
   thread_t          * groupnext ;
} ;

// group: initonce

/* function: initonce_thread
 * Calculates some internal offsets, called from <initmain_umgebung>.
 * It is called after the <umgebung_t> object is fully operational. */
extern int initonce_thread(umgebung_t * umg) ;

/* function: freeonce_thread
 * Does nothing. Called from <freemain_umgebung>.
 * It is called before the <umgebung_t> object is freed. */
extern int freeonce_thread(umgebung_t * umg) ;

// group: lifetime

/* function: new_thread
 * Creates and starts a new system thread.
 * The thread has to do some internal initialization after running the first time
 * and before thread_main is called.
 * If the internal preparation goes wrong <umgebung_t.abort_umgebung> is called.
 * It is unspecified if thread_main is called before new_thread returns.
 * On Linux new_thread returns before the newly created thread is scheduled. */
extern int new_thread(/*out*/thread_t ** threadobj, task_callback_f thread_main, struct callback_param_t * start_arg) ;

/* function: newgroup_thread
 * Creates and starts nr_of_threads new system threads.
 * See also <new_thread>.
 * If not that many threads could be created as specified in nr_of_threads
 * already created threads silently exit themselves without any error being logged.
 * This preserves transactional all or nothing semantics. */
extern int newgroup_thread(/*out*/thread_t ** threadobj, task_callback_f thread_main, struct callback_param_t * start_arg, uint32_t nr_of_threads) ;

/* function: delete_thread
 * Calls <join_thread> (if not already called) and deletes resources.
 * This function waits until the thread has terminated. So be careful ! */
extern int delete_thread(thread_t ** threadobj) ;

// group: query

/* function: self_thread
 * Returns a pointer to the own thread object. */
extern thread_t * self_thread(void) ;

/* function: returncode_thread
 * Returns the returncode of the joined thread.
 * The returncode is only valid if <join_thread> was called before.
 * 0 is returned in case the thread has not already been joined. */
extern int returncode_thread(const thread_t * threadobj) ;

/* function: task_thread
 * Reads <thread_t.task> field of <thread_t> structure. */
extern task_callback_t task_thread(const thread_t * threadobj) ;

// group: change lock

/* function: lock_thread
 * Locks thread object before fields can be accessed.
 *
 * Attention:
 * Never forget to lock / unlock a thread object before you access
 * the fields which can be changed by other threads. This ensures
 * that you read a consistent state and that on some architectures
 * proper read and write barriers are executed. */
extern void lock_thread(thread_t * threadobj) ;

/* function: unlock_thread
 * Unlocks thread object after access to fields is finished. */
extern void unlock_thread(thread_t * threadobj) ;

// group: change

/* function: join_thread
 * The function suspends execution of the caller until threadobj terminates.
 * If the thread has already been joined this function returns immediately. */
extern int join_thread(thread_t * threadobj) ;

/* function: suspend_thread
 * The calling thread will sleep until <resume_thread> is called.
 * <resume_thread> must be called from another thread.
 *
 * Linux specific:
 * Internally sigwaitinfo wirh signal SIGINT is used to sleep.
 *
 * Attention !:
 * It is possible that signals are received from outside this process therefore make sure
 * with checking of <task> or <wlistnext> or with some other mechanism that returning
 * from <suspend_thread> emanates from a corresponding call to <resume_thread>. */
extern void suspend_thread(void) ;

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
extern void resume_thread(thread_t * threadobj) ;

/* function: sleepms_thread
 * Makes calling thread to sleep msec milli-seconds. */
extern void sleepms_thread(uint32_t msec) ;


// section: inline implementation

/* define: task_thread
 * Implements <thread_t.task_thread>.
 * > ((threadobj)->task) */
#define task_thread(threadobj)         ((threadobj)->task)

/* define: lock_thread
 * Implements <thread_t.lock_thread>.
 * Do not forget to include C-kern/api/os/sync/mutex.h before using <lock_thread>. */
#define lock_thread(threadobj)         slock_mutex(&(threadobj)->lock)

/* define: new_thread
 * Implements <thread_t.new_thread>.
 * > newgroup_thread(threadobj, thread_main, start_arg, 1) */
#define new_thread(threadobj, thread_main, start_arg) \
      newgroup_thread(threadobj, thread_main, start_arg, 1)

/* define: newgroup_thread
 * Calls <thread_t.newgroup_thread> with adapted function pointer. */
#define newgroup_thread(threadobj, thread_main, start_arg, nr_of_threads) \
   /*do not forget to adapt definition in thead.c test section*/                    \
   ( __extension__ ({ int _err ;                                                    \
      int (*_thread_main) (typeof(start_arg)) = (thread_main) ;                     \
      static_assert(sizeof(start_arg) <= sizeof(void*), "cast 2 void*") ;           \
      _err = newgroup_thread(threadobj, (task_callback_f) _thread_main,             \
                              (struct callback_param_t*) start_arg, nr_of_threads) ;\
      _err ; }))

/* define: returncode_thread
 * Implements <thread_t.returncode_thread>.
 * > (threadobj)->returncode */
#define /*int*/ returncode_thread(threadobj) \
   ((threadobj)->returncode)

/* define: self_thread
 * Implements <thread_t.self_thread>.
 * > (&gt_thread_self) */
#define self_thread()                  (&gt_thread_self)

/* define: unlock_thread
 * Implements <thread_t.unlock_thread>.
 * Do not forget to include "C-kern/api/os/sync/mutex.h" before using <unlock_thread>. */
#define unlock_thread(threadobj)       sunlock_mutex(&(threadobj)->lock)

// group: KONFIG_SUBSYS

#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
/* define: initonce_thread
 * Implement <thread_t.initonce_thread> as a no op if !((KONFIG_SUBSYS)&THREAD) */
#define initonce_thread(umg)   (0)
/* define: freeonce_thread
 * Implement <thread_t.freeonce_thread> as a no op if !((KONFIG_SUBSYS)&THREAD) */
#define freeonce_thread(umg)   (0)
#endif
#undef THREAD

#endif
