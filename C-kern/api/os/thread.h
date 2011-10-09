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

#include "C-kern/api/aspect/memoryblock.h"

/* typedef: osthread_t typedef
 * Export <osthread_t>. */
typedef struct osthread_t              osthread_t ;

/* typedef: thread_main_f
 * Function pointer to thread implementation. */
typedef int/*errcode(0 == OK)*/     (* thread_main_f) (void * start_arg) ;

/* typedef: osthread_stack_t typedef
 * Export <memoryblock_aspect_t> as osthread_stack_t. */
typedef memoryblock_aspect_t           osthread_stack_t ;

extern __thread  osthread_t            gt_self_osthread ;

// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_thread
 * Tests system thread functionality. */
extern int unittest_os_thread(void) ;
#endif


/* struct: osthread_t
 * Describes a system thread. */
struct osthread_t {
   /* variable: wlistnext
    * Points to next thread which waits on the same condition in <waitlist_t>. */
   osthread_t      * wlistnext ;
   /* variable: command
    * Contains value to signal thread what to do after wakeup. */
   void            * command ;
   /* variable: sys_thread
    * Contains system specific ID of thread. It has type <sys_thread_t>. */
   sys_thread_t      sys_thread ;
   /* variable: main
    * Contains pointer to function which is executed by this thread. */
   thread_main_f     main ;
   /* variable: returncode
    * Contains the value <thread_main> returns.
    * This value is only valid after <thread_main> has returned.
    * This value reflects the value of the first thread which did not return 0.
    * This value is 0 if all threads returned 0. */
   int               returncode ;
   /* variable: stackframe
    * Contains the mapped memory used as stack. */
   osthread_stack_t  stackframe ;
   /* variable: nr_threads
    * Contains the number of threads in this group.
    * All threads share the same thread_main function and the same argument at the beginning.
    * Use <groupnext> to iterate over the whole group. */
   uint32_t          nr_threads ;
   /* variable: groupnext
    * Points to next thread in group of throuds. */
   osthread_t      * groupnext ;
} ;

// group: initonce

/* function: initonce_osthread
 * Calculates some internal offsets, called from <initprocess_umgebung>.
 * The <umgebung_t> object is fully operational. */
extern int initonce_osthread(void) ;


// group: lifetime

/* function: new_osthread
 * Creates and starts a new system thread.
 * The thread has to do some internal initialization after running the first time
 * and before thread_main is called.
 * If the internal preparation goes wrong <umgebung_t.abort_umgebung> is called.
 * It is unspecified if thread_main is called before new_osthread returns.
 * On Linux new_osthread returns before the newly created thread is scheduled. */
extern int new_osthread(/*out*/osthread_t ** threadobj, thread_main_f thread_main, void * thread_argument) ;

/* function: newgroup_osthread
 * Creates and starts nr_of_threads new system threads.
 * See also <new_osthread>.
 * If not that many threads could be created as specified in nr_of_threads
 * already created threads silently exit themselves without any error being logged.
 * This preserves transactional all or nothing semantics. */
extern int newgroup_osthread(/*out*/osthread_t ** threadobj, thread_main_f thread_main, void * thread_argument, uint32_t nr_of_threads) ;

/* function: delete_osthread
 * Calls <join_osthread> (if not already called) and deletes resources.
 * This function waits until the thread has terminated. So be careful ! */
extern int delete_osthread(osthread_t ** threadobj) ;

// group: query

/* function: self_osthread
 * Returns a pointer to the own thread object. */
extern osthread_t * self_osthread(void) ;

/* function: returncode_osthread
 * Returns the returncode of the joined thread.
 * The returncode is only valid if <join_osthread> was called before.
 * 0 is returned in case the thread has not already been joined. */
extern int returncode_osthread(const osthread_t * threadobj) ;

// group: change

/* function: join_osthread
 * The function suspends execution of the caller until threadobj terminates.
 * If the thread has already been joined this function returns immediately. */
extern int join_osthread(osthread_t * threadobj) ;

/* function: suspend_osthread
 * The calling thread will sleep until <resume_osthread> is called.
 * <resume_osthread> must be called from another thread.
 *
 * Linux specific:
 * Internally sigwaitinfo wirh signal SIGINT is used to sleep.
 *
 * Attention !:
 * It is possible that signals are received from outside this process therefore make sure
 * with checking of <command> or <wlistnext> or with some other mechanism that returning
 * from <suspend_osthread> emanates from a corresponding call to <resume_osthread>. */
extern void suspend_osthread(void) ;

/* function: resume_osthread
 * The thread which is refered by threadobj is woken up.
 * The status of resume is conserved if the other thread is currently not sleeping.
 * So the next call to suspend will return immediately.
 * This behaviour is needed cause a thread calling suspend could be preempted before
 * it really enters a sleep state.
 *
 * Linux specific:
 * Internally pthread_kill with signal SIGINT is used to wake up a another
 * thread from sleeping. */
extern void resume_osthread(osthread_t * threadobj) ;


// section: inline implementations

/* define: self_osthread
 * Implements <osthread_t.self_osthread>.
 * > (&gt_self_osthread) */
#define self_osthread() \
   (&gt_self_osthread)

/* define: returncode_osthread
 * Implements <osthread_t.returncode_osthread>.
 * > (threadobj)->returncode */
#define /*int*/ returncode_osthread(/*const osthread_t * */threadobj) \
   ((threadobj)->returncode)

/* define: new_osthread
 * Implements <osthread_t.new_osthread>.
 * > newgroup_osthread(threadobj, thread_main, thread_argument, 1) */
#define new_osthread(threadobj, thread_main, thread_argument) \
      newgroup_osthread(threadobj, thread_main, thread_argument, 1)

/* define: newgroup_osthread
 * Calls <osthread_t.newgroup_osthread> with adapted function pointer. */
#define newgroup_osthread(threadobj, thread_main, thread_argument, nr_of_threads) \
   /*do not forget to adapt definition in thead.c test section*/                 \
   ( __extension__ ({ int _err ;                                                 \
      int (*_thread_main) (typeof(thread_argument)) = (thread_main) ;            \
      static_assert(sizeof(thread_argument) <= sizeof(void*), "cast 2 void*") ;  \
      _err = newgroup_osthread(threadobj, (thread_main_f) _thread_main,          \
                              (void*) thread_argument, nr_of_threads) ;          \
      _err ; }))

// group: KONFIG_SUBSYS

#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
/* define: initonce_osthread
 * Implement <osthread_t.initonce_osthread> as a no op if !((KONFIG_SUBSYS)&THREAD) */
#define initonce_osthread()   (0)
#endif
#undef THREAD

#endif
