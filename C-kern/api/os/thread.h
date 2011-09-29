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
   (C) 2010 Jörg Seebohn

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

/* typedef: osthread_mutex_t typedef
 * Export <osthread_mutex_t>. */
typedef struct osthread_mutex_t        osthread_mutex_t ;

/* typedef: thread_main_f
 * Function pointer to thread implementation. */
typedef int32_t/*errcode(0 == OK)*/ (* thread_main_f) (osthread_t * thread) ;

/* typedef: osthread_stack_t typedef
 * Export <memoryblock_aspect_t> as osthread_stack_t. */
typedef memoryblock_aspect_t           osthread_stack_t ;


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
   /* variable: main
    * Contains pointer to function which is executed by this thread. */
   thread_main_f     main ;
   /* variable: argument
    * Contains value of the single user argument. */
   void            * argument ;
   /* variable: returncode
    * Contains the value <thread_main> returns.
    * This value is only valid after <thread_main> has returned.
    * This value reflects the value of the first thread which did not return 0.
    * This value is 0 if all threads returned 0. */
   int32_t           returncode ;
   /* variable: stackframe
    * Contains the mapped memory used as stack. */
   osthread_stack_t  stackframe ;
   /* variable: nr_threads
    * Contains the number of threads handled by this object.
    * All threads share the same thread_main function and the same argument. */
   uint32_t          nr_threads ;
   /* variable: sys_thread
    * Contains system specific ID of thread. It has type <sys_thread_t>. */
   sys_thread_t      sys_thread[1/*nr_threads*/] ;
} ;

// group: lifetime

/* function: new_osthread
 * Creates and starts a new system thread.
 * The thread has to do some internal initialization after running the first time
 * and before thread_main is called.
 * If the internal preparation goes wrong <umgebung_t.abort_umgebung> is called.
 * It is unspecified if thread_main is called before new_osthread returns.
 * On Linux new_osthread returns before the newly created thread is scheduled. */
extern int new_osthread(/*out*/osthread_t ** threadobj, thread_main_f thread_main, void * thread_argument) ;

/* function: newmany_osthread
 * Creates and starts nr_of_threads new system threads.
 * See also <new_osthread>.
 * If not that many threads could be created as specified in nr_of_threads
 * already created threads silently exit themselves without any error being logged.
 * This preserves transactional all or nothing semantics. */
extern int newmany_osthread(/*out*/osthread_t ** threadobj, thread_main_f thread_main, void * thread_argument, uint32_t nr_of_threads) ;

/* function: delete_osthread
 * Calls <join_osthread> (if not already called) and deletes resources.
 * This function waits until the thread has terminated. So be careful ! */
extern int delete_osthread(osthread_t ** threadobj) ;

// group: query

/* function: argument_osthread
 * Returns the single user supplied argument argument of threadobj. */
extern void * argument_osthread(const osthread_t * threadobj) ;

/* function: returncode_osthread
 * Returns the returncode of the joined thread.
 * The returncode is only valid if <join_osthread> was called before.
 * 0 is returned in case the thread has not already been joined. */
extern int32_t returncode_osthread(const osthread_t * threadobj) ;

// group: wait

/* function: join_osthread
 * The function suspends execution of the caller until threadobj terminates.
 * If the thread has already been joined this function returns immediately. */
extern int join_osthread(osthread_t * threadobj) ;


/* struct: osthread_mutex_t
 * */
struct osthread_mutex_t {
   sys_thread_mutex_t   sys_mutex ;
} ;

// group: lifetime

/* define: osthread_mutex_INIT_DEFAULT
 * Static initializer for <osthread_mutex_t> which can be used by threads of the same process.
 *
 * Following behaviour is guaranteed:
 * 1. No deadlock detection.
 * 2. Locking it more than once without first unlocking it => DEADLOCK (waits indefinitely).
 * 3. Unlocking a mutex locked by a different thread works. It is the same as if the thread which holds the lock calls unlock.
 * 4. Unlocking an already unlocked mutex is unspecified. So never do it.
 * TODO: !!! Implement user threads and user threads compatible mutex which has *SPECIFIED* behaviour !!! */
#define osthread_mutex_INIT_DEFAULT    { .sys_mutex = sys_thread_mutex_INIT_DEFAULT }

/* function: init_osthreadmutex
 * Initializer for a mutex with error checking.
 *
 * Following behaviour is guaranteed:
 * 1. No deadlock detection.
 * 2. Locking it more than once without first unlocking it returns error EDEADLK.
 * 3. Unlocking a mutex locked by a different thread is prevented and returns error EPERM.
 * 4. Unlocking an already unlocked mutex is prevented and returns error EPERM.
 * */
extern int init_osthreadmutex(osthread_mutex_t * mutex) ;

/* function: free_osthreadmutex
 * Frees resources of mutex which is not in use.
 * Returns EBUSY if a thread holds a lock and nothing is freed. */
extern int free_osthreadmutex(osthread_mutex_t * mutex) ;

// group: change

/* function: lock_osthreadmutex
 * Locks a mutex. If another thread holds the lock the calling thread waits until the lock is released.
 * If a lock is acquired more than once a DEADLOCK results.
 * If you try to lock a freed mutex EINVAL is returned. */
extern int lock_osthreadmutex(osthread_mutex_t * mutex) ;

/* function: unlock_osthreadmutex
 * Unlocks a previously locked mutex.
 * Unlocking a mutex more than once is unspecified and may return success but corrupts internal counters.
 * Unlocking a mutex which another thread has locked works as if the lock holder thread would call unlock.
 * If you try to lock a freed mutex EINVAL is returned. */
extern int unlock_osthreadmutex(osthread_mutex_t * mutex) ;


// section: inline implementations

/* define: argument_osthread
 * Implements <osthread_t.argument_osthread>.
 * > (threadobj)->argument */
#define /*void **/ argument_osthread(/*const osthread_t * */threadobj) \
   ((threadobj)->argument)

/* define: returncode_osthread
 * Implements <osthread_t.returncode_osthread>.
 * > (threadobj)->returncode */
#define /*int32_t*/ returncode_osthread(/*const osthread_t * */threadobj) \
   ((threadobj)->returncode)

/* define: new_osthread
 * Implements <osthread_t.new_osthread>.
 * > newmany_osthread(threadobj, thread_main, thread_argument, 1) */
#define new_osthread(threadobj, thread_main, thread_argument) \
   newmany_osthread(threadobj, thread_main, thread_argument, 1)


#endif
