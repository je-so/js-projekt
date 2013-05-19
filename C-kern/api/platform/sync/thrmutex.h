/* title: InterThreadMutex

   Offers inter thread mutex which works closely with <thread_t>

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/sync/thrmutex.h
    Header file <InterThreadMutex>.

   file: C-kern/platform/Linux/sync/thrmutex.c
    Implementation file <InterThreadMutex impl>.
*/
#ifndef CKERN_PLATFORM_SYNC_THRMUTEX_HEADER
#define CKERN_PLATFORM_SYNC_THRMUTEX_HEADER

// forward
struct thread_t ;
struct slist_node_t ;

/* typedef: struct thrmutex_t
 * Export <thrmutex_t> into global namespace. */
typedef struct thrmutex_t                 thrmutex_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_sync_thrmutex
 * Test <thrmutex_t> functionality. */
int unittest_platform_sync_thrmutex(void) ;
#endif


/* struct: thrmutex_t
 * Mutual exclusion lock. Used to synchronize access to a data structure
 * shared between multiple threads. If you want to share data between processes
 * use <mutex_t>. Call <lock_threamutex> before you want to use the data
 * structure. Call <unlock_threadmutex> after you no longer need access to it. */
struct thrmutex_t {
   /* variable: last
    * Points to last entry in list of waiting threads.
    * Threads trying to lock the mutex are appended to the end of the list. */
   struct slist_node_t *   last ;
   /* variable: lockholder
    * The threads which acquired the lock and is allowed to run.
    * If this value is 0 <last> is also 0 and the no one has locked the mutex. */
   struct thread_t *       lockholder ;
   /* variable: lockflag
    * Lock flag used to protect access to data members.
    * Set and cleared with atomic operations. */
   uint8_t                 lockflag ;
} ;

// group: lifetime

/* define: thrmutex_INIT_FREEABLE
 * Static initializer. */
#define thrmutex_INIT_FREEABLE            { 0, 0, 0 }

/* define: thrmutex_INIT
 * Static initializer. Used in function <init_thrmutex>. */
#define thrmutex_INIT                     { 0, 0, 0 }

/* function: init_thrmutex
 * Sets mutex to <thrmutex_INIT>. */
void init_thrmutex(/*out*/thrmutex_t * mutex) ;

/* function: free_thrmutex
 * Checks that no one is waiting and sets mutex to <thrmutex_INIT_FREEABLE>.
 * Returns EBUSY and does nothing if anyone holds the lock. */
int free_thrmutex(thrmutex_t * mutex) ;

// group: query

/* function: isfree_thrmutex
 * Returns true if mutex equals <thrmutex_INIT_FREEABLE>. */
bool isfree_thrmutex(thrmutex_t * mutex) ;

/* function: islocked_thrmutex
 * Returns true if a thread locked the mutex.
 * If this functions returns true <lockholder_thrmutex> returns a value != 0. */
bool islocked_thrmutex(thrmutex_t * mutex) ;

/* function: iswaiting_thrmutex
 * Returns true if mutex is locked and wait list is not empty.
 * If a thread calls <lock_thrmutex> but the mutex is already locked
 * it is stored in an internal wait list. */
bool iswaiting_thrmutex(thrmutex_t * mutex) ;

/* function: lockholder_thrmutex
 * Returns the thread which locked the mutex.
 * Returns 0 if the mutex is unlocked. */
struct thread_t * lockholder_thrmutex(thrmutex_t * mutex) ;

// group: synchronize

/* function: lock_thrmutex
 * Locks the mutex. If the mutex is already locked the caller
 * is stored into an internal wait list as last entry and
 * then it is suspended.
 * If the lockholder calls <unlock_thrmutex> the first thread
 * on the wait list is resumed and returns from this function.
 * The function returns EDEADLK if the caller already locked
 * the mutex. Recursive calls to lock and unlock are not supported. */
int lock_thrmutex(thrmutex_t * mutex) ;

/* function: unlock_thrmutex
 * Unlocks the mutex. The functions returns EPERM if the lock
 * is already unlocked or locked by another thread. */
int unlock_thrmutex(thrmutex_t * mutex) ;

// group: safe-synchronize

/* function: slock_thrmutex
 * Calls <lock_thrmutex> and asserts success. */
void slock_thrmutex(thrmutex_t * mutex) ;

/* function: sunlock_thrmutex
 * Calls <sunlock_thrmutex> and asserts success. */
void sunlock_thrmutex(thrmutex_t * mutex) ;



// section: inline implementation

/* define: init_thrmutex
 * Implements <thrmutex_t.init_thrmutex>. */
#define init_thrmutex(mutex)     \
         ((void)(*(mutex) = (thrmutex_t) thrmutex_INIT))

/* define: slock_thrmutex
 * Implements <thrmutex_t.slock_thrmutex>. */
#define slock_thrmutex(mutex)    \
         (assert(!lock_thrmutex(mutex)))

/* define: sunlock_thrmutex
 * Implements <thrmutex_t.sunlock_thrmutex>. */
#define sunlock_thrmutex(mutex)  \
         (assert(!unlock_thrmutex(mutex)))


#endif
