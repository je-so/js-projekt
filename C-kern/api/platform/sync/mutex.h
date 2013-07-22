/* title: Mutex

   Encapsulates an os specific exclusive lock.

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

   file: C-kern/api/platform/sync/mutex.h
    Header file of <Mutex>.

   file: C-kern/platform/Linux/sync/mutex.c
    Linux specific implementation <Mutex Linuximpl>.
*/
#ifndef CKERN_PLATFORM_SYNC_MUTEX_HEADER
#define CKERN_PLATFORM_SYNC_MUTEX_HEADER

/* typedef: struct mutex_t
 * Exports <mutex_t> into global namespace. */
typedef sys_mutex_t                       mutex_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_sync_mutex
 * Tests system thread functionality. */
int unittest_platform_sync_mutex(void) ;
#endif


/* struct: mutex_t
 * Implements a mutual exclusion lock.
 * This thread safe object is used to protect a critical section
 * of code against simultaneous execution by several threads.
 * Use <lock_mutex> before entering a critical section of code
 * and use <unlock_mutex> just before you leave it.
 * This mutex can also be used between different processes. */
typedef sys_mutex_t                       mutex_t ;

// group: lifetime

/* define: mutex_INIT_DEFAULT
 * Static initializer for <mutex_t> without error checking.
 *
 * Following behaviour is guaranteed:
 * 1. No deadlock detection.
 * 2. Locking it more than once without first unlocking it => DEADLOCK (waits indefinitely).
 * 3. Unlocking a mutex locked by a different thread works. It is the same as if the thread which holds the lock calls unlock.
 * 4. Unlocking an already unlocked mutex is unspecified. So never do it.
 * 5. Works only within a single process as inter thread mutex.
 */
#define mutex_INIT_DEFAULT                sys_mutex_INIT_DEFAULT

/* function: init_mutex
 * Initializer for a mutex with error checking.
 *
 * Following behaviour is guaranteed:
 * 1. No deadlock detection.
 * 2. Locking it more than once without first unlocking it returns error EDEADLK.
 * 3. Unlocking a mutex locked by a different thread is prevented and returns error EPERM.
 * 4. Unlocking an already unlocked mutex is prevented and returns error EPERM.
 * */
int init_mutex(/*out*/mutex_t * mutex) ;

/* function: free_mutex
 * Frees resources of mutex which is not in use.
 * Returns EBUSY if a thread holds a lock and nothing is freed. */
int free_mutex(mutex_t * mutex) ;

// group: change

/* function: lock_mutex
 * Locks a mutex. If another thread holds the lock the calling thread waits until the lock is released.
 * If a lock is acquired more than once a DEADLOCK results.
 * If you try to lock a freed mutex EINVAL is returned. */
int lock_mutex(mutex_t * mutex) ;

/* function: unlock_mutex
 * Unlocks a previously locked mutex.
 * Unlocking a mutex more than once is unspecified and may return success but corrupts internal counters.
 * Unlocking a mutex which another thread has locked works as if the lock holder thread would call unlock.
 * If you try to lock a freed mutex EINVAL is returned. */
int unlock_mutex(mutex_t * mutex) ;

/* function: slock_mutex
 * Same as <lock_mutex>.
 * Except that an error leads to a system abort. */
void slock_mutex(mutex_t * mutex) ;

/* function: sunlock_mutex
 * Same as <unlock_mutex>.
 * Except that an error leads to a system abort. */
void sunlock_mutex(mutex_t * mutex) ;



// section: inline implementation

/* define: slock_mutex
 * Implements <mutex_t.slock_mutex>. */
#define slock_mutex(/*mutex_t * */mutex) \
         (assert(!lock_mutex(mutex))) ;

/* define: sunlock_mutex
 * Implements <mutex_t.sunlock_mutex>. */
#define sunlock_mutex(/*mutex_t * */mutex) \
         (assert(!unlock_mutex(mutex))) ;

// group: KONFIG_SUBSYS

#define KONFIG_thread 1
#if (!((KONFIG_SUBSYS)&KONFIG_thread))
/* define: init_mutex
 * Implement <mutex_t.init_mutex> as a no op if !((KONFIG_SUBSYS)&KONFIG_thread) */
#define init_mutex(mutex)     (0)
/* define: free_mutex
 * Implement <mutex_t.free_mutex> as a no op if !((KONFIG_SUBSYS)&KONFIG_thread) */
#define free_mutex(mutex)     (0)
/* define: lock_mutex
 * Implement <mutex_t.lock_mutex> as a no op if !((KONFIG_SUBSYS)&KONFIG_thread) */
#define lock_mutex(mutex)     (0)
/* define: unlock_mutex
 * Implement <mutex_t.unlock_mutex> as a no op if !((KONFIG_SUBSYS)&KONFIG_thread) */
#define unlock_mutex(mutex)   (0)
#endif
#undef KONFIG_thread

#endif
