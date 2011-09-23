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


/* typedef: osthread_t typedef
 * Export <osthread_t>. */
typedef struct osthread_t              osthread_t ;

/* typedef: osthread_mutex_t typedef
 * Export <osthread_mutex_t>. */
typedef struct osthread_mutex_t        osthread_mutex_t ;

/* typedef: thread_main_f
 * Function pointer to thread implementation. */
typedef int32_t/*errcode(0 == OK)*/ (* thread_main_f) (osthread_t * thread) ;


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
   /* variable: sys_thread
    * Contains system specific ID of thread. It has type <sys_thread_t>. */
   sys_thread_t   sys_thread ;
   /* variable: thread_main
    * Contains pointer to function which is executed by this thread. */
   thread_main_f  thread_main ;
   /* variable: thread_argument
    * Contains value of the single user argument. */
   void         * thread_argument ;
   /* variable: thread_returncode
    * Contains the value <thread_main> returns.
    * This value is only valid after <thread_main> has returned. */
   int32_t        thread_returncode ;
   bool           isJoined ;
} ;

// group: lifetime

/* function: new_osthread
 * Creates and starts a new system thread.
 * After successful return threadobj->isRunnning is not necessarily true.
 * It will be set to true some instructions before a call to thread_main is made.
 * The thread has to do some internal initialization after running the first time
 * and before thread_main is called.
 * If the internal preparation goes wrong it exits itself and never calls thread_main and
 * threadobj->isRunning will be never set to true. */
extern int new_osthread(/*out*/osthread_t ** threadobj, thread_main_f thread_main, void * thread_argument) ;

/* function: delete_osthread
 * Before deleting a thread object <join_osthread> must be called.
 * EAGAIN is returned if you have not joined it before. */
extern int delete_osthread(osthread_t ** threadobj) ;

// group: query

/* function: returncode_osthread
 * Returns the returncode of the joined thread.
 * The returncode is only valid if <join_osthread> was called before.
 * 0 is returned in case the thread has not already been joined. */
extern int32_t returncode_osthread(const osthread_t * threadobj) ;

// group: wait

/* function: join_osthread
 * The function suspends execution of the caller until threadobj terminates,
 * unless threadobj has already terminated. */
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

/* define: returncode_osthread
 * Implements <exothread_t.returncode_osthread>.
 * > (threadobj)->thread_returncode */
#define /*int32_t*/ returncode_osthread(/*const osthread_t * */threadobj) \
   ((threadobj)->thread_returncode)


#endif
