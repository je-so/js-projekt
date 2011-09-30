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
#ifndef CKERN_OS_SYNCHRONIZATION_MUTEX_HEADER
#define CKERN_OS_SYNCHRONIZATION_MUTEX_HEADER

/* typedef: mutex_t typedef
 * Export <mutex_t>. */
typedef struct mutex_t           mutex_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_sync_mutex
 * Tests system thread functionality. */
extern int unittest_os_sync_mutex(void) ;
#endif


/* struct: mutex_t
 * Describes a mutual exclusion lock.
 * This thread safe object is used to protect a critical section
 * of code against simultaneous execution by several threads.
 * Use <lock_mutex> before entering a critical section of code
 * and use <unlock_mutex> just before you leave it.
 * This mutex can only be used in the same process. */
struct mutex_t {
   sys_mutex_t    sys_mutex ;
} ;

// group: lifetime

/* define: mutex_INIT_DEFAULT
 * Static initializer for <mutex_t> without error checking.
 *
 * Following behaviour is guaranteed:
 * 1. No deadlock detection.
 * 2. Locking it more than once without first unlocking it => DEADLOCK (waits indefinitely).
 * 3. Unlocking a mutex locked by a different thread works. It is the same as if the thread which holds the lock calls unlock.
 * 4. Unlocking an already unlocked mutex is unspecified. So never do it.
 * TODO: !!! Implement user threads and user threads compatible mutex which has *SPECIFIED* behaviour !!! */
#define mutex_INIT_DEFAULT       { .sys_mutex = sys_mutex_INIT_DEFAULT }

/* function: init_mutex
 * Initializer for a mutex with error checking.
 *
 * Following behaviour is guaranteed:
 * 1. No deadlock detection.
 * 2. Locking it more than once without first unlocking it returns error EDEADLK.
 * 3. Unlocking a mutex locked by a different thread is prevented and returns error EPERM.
 * 4. Unlocking an already unlocked mutex is prevented and returns error EPERM.
 * */
extern int init_mutex(mutex_t * mutex) ;

/* function: free_mutex
 * Frees resources of mutex which is not in use.
 * Returns EBUSY if a thread holds a lock and nothing is freed. */
extern int free_mutex(mutex_t * mutex) ;

// group: change

/* function: lock_mutex
 * Locks a mutex. If another thread holds the lock the calling thread waits until the lock is released.
 * If a lock is acquired more than once a DEADLOCK results.
 * If you try to lock a freed mutex EINVAL is returned. */
extern int lock_mutex(mutex_t * mutex) ;

/* function: unlock_mutex
 * Unlocks a previously locked mutex.
 * Unlocking a mutex more than once is unspecified and may return success but corrupts internal counters.
 * Unlocking a mutex which another thread has locked works as if the lock holder thread would call unlock.
 * If you try to lock a freed mutex EINVAL is returned. */
extern int unlock_mutex(mutex_t * mutex) ;


#endif
