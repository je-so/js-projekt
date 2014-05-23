/* title: ReadWriteLock

   Implements a simple many readers and single writer lock for threads of a single process.

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

   file: C-kern/api/platform/sync/rwlock.h
    Header file <ReadWriteLock>.

   file: C-kern/platform/Linux/sync/rwlock.c
    Implementation file <ReadWriteLock Linuximpl>.
*/
#ifndef CKERN_PLATFORM_SYNC_RWLOCK_HEADER
#define CKERN_PLATFORM_SYNC_RWLOCK_HEADER

// forward
struct slist_node_t ;

/* typedef: struct rwlock_t
 * Export <rwlock_t> into global namespace. */
typedef struct rwlock_t                   rwlock_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_sync_rwlock
 * Test <rwlock_t> functionality. */
int unittest_platform_sync_rwlock(void) ;
#endif


/* struct: rwlock_t
 * Protect a data structure from access of threads of a single process.
 * Either a single writer is allowed to enter the protected area
 * or one or more readers are allowed to enter.
 * As long as no writer tries to acquire this lock no reader has to wait.
 * If a writer tries to acquire this type of lock it has to wait until
 * all readers have released the lock. A writer inserts itself in waiting list <writers>.
 * If at least a single writer waits all calls to <lockreader_rwlock> inserts the reader
 * into the the <readers> waiting list.
 *
 * The last reader which calls <unlockreader_rwlock> resumes a waiting writer.
 *
 * During this time all other parties (reader or writer) have to wait
 * until all readers and the single writer have released the lock.
 *
 * A call <unlockwriter_rwlock> wakes up first any waiting reader.
 *
 * So a writer releasing the lock wakes up all readers. The last reader releasing the lock
 * wakes up the first waiting writer.
 *
 * Implementation Notes:
 * The implementation an atomic <lockflag>.
 * Every call to <lockreader_rwlock>, <lockwriter_rwlock>, <unlockreader_rwlock> and <unlockwriter_rwlock>
 * acquires this lock. At the end they release the lock.
 *
 * If during operation of <unlockreader_rwlock> or <unlockwriter_rwlock> a writer or reader is woken up
 * the woken up thread acquires <lockflag> and releases it. This ensures that any data written by
 * the caller to <unlockwriter_rwlock> and any data written within the functions <unlockreader_rwlock>
 * or <unlockwriter_rwlock> are known to the woken up thread.
 *
 */
struct rwlock_t {
   /* variable: readers
    * Points to last entry in list of waiting readers.
    * Threads which can not lock rwlock for reading are appended to the end of the list. */
   struct {
      struct slist_node_t *   last ;
   }                       readers ;
   /* variable: writers
    * Points to last entry in list of waiting writers.
    * Threads which can not lock rwlock for writing are appended to the end of the list. */
   struct {
      struct slist_node_t *   last ;
   }                       writers ;
   /* variable: writer
    * The thread which holds <entrylock>. If <nrofreader> is greater 0
    * it is in suspended state. */
   struct thread_t *       writer  ;
   /* variable: nrofreader
    * The number of readers currently reading the protected data structure.
    * If this value is no reader has acquired this lock. */
   uint32_t                nrofreader ;
   /* variable: lockflag
    * Lock flag used to protect access to data members.
    * Set and cleared with atomic operations. */
   uint8_t                 lockflag ;
} ;

// group: lifetime

/* define: rwlock_FREE
 * Static initializer. */
#define rwlock_FREE { { 0 } , { 0 }, 0, 0, 0 }

/* define: rwlock_INIT
 * Static initializer. */
#define rwlock_INIT { { 0 } , { 0 }, 0, 0, 0 }

/* function: init_rwlock
 * Initializes data members. The same as assigning <rwlock_INIT>. */
void init_rwlock(/*out*/rwlock_t * rwlock) ;

/* function: free_rwlock
 * Frees mutex. Make sure that no readers nor writers are
 * waiting for rwlock or holding rwlock.
 * The behaviour in such a case is undefined.
 * A mutex returns EBUSY and is not freed if someone holds it. */
int free_rwlock(rwlock_t * rwlock) ;

// group: query

/* function: nrofreader_rwlock
 * Returns the number of readers holding the lock. */
uint32_t nrofreader_rwlock(rwlock_t * rwlock) ;

/* function: iswriter_rwlock
 * Returns true if there is a single writer holding the lock. */
bool iswriter_rwlock(rwlock_t * rwlock) ;

// group: synchronize

/* function: lockreader_rwlock
 * Acquire <entrylock>, increment <nrofreader> and release <entrylock>.
 * After return <nrofreader> is > 0 and means a region is protected for reading.
 * A return value of EOVERFLOW indicates that <nrofreader> == UINT32_MAX and
 * no more reader is allowed to enter. */
int lockreader_rwlock(rwlock_t * rwlock) ;

/* function: lockwriter_rwlock
 * Acquire <entrylock>, sets <writer> to seld and wait until <nrofreader> == 0.
 * If <nrofreader> > 0 the calling thread is suspended and the last
 * reader leaving the protected region with <unlockreader_rwlock>
 * resumes the waiting writer.
 *
 * After the writer has been resumed it acquires <exitlock> to ensure
 * that it read a current <nrofreader> and releases <exitlock>. This also
 * prevents a race condition if a spurious resume (suprious signal) wakes
 * up the writer but the reader has not send the resume to the waiter.
 * If <nrofreader> equals 0 the call returns else the caller is suspend again. */
int lockwriter_rwlock(rwlock_t * rwlock) ;

/* function: unlockreader_rwlock
 * Acquire <exitlock>, decrement <nrofreader> and return.
 * Call this function only after calling <lockreader_rwlock>.
 * If you forget one call <nrofreader> is wrong and no writer
 * or other reader is allowed to enter. */
int unlockreader_rwlock(rwlock_t * rwlock) ;

/* function: unlockwriter_rwlock
 * Sets <writer> to 0 and unlocks <entrylock>.
 * Other writers or readers are allowed to enter (entrylock is unlocked).
 * Call this function only after calling <lockwriter_rwlock> else
 * EPERM is returned and <writer> is not changed. */
int unlockwriter_rwlock(rwlock_t * rwlock) ;

// group: safe-synchronize

/* function: slockreader_rwlock
 * Asserts that <lockreader_rwlock> has no error. */
void slockreader_rwlock(rwlock_t * rwlock) ;

/* function: slockwriter_rwlock
 * Asserts that <lockwriter_rwlock> has no error. */
void slockwriter_rwlock(rwlock_t * rwlock) ;

/* function: sunlockreader_rwlock
 * Asserts that <unlockreader_rwlock> has no error. */
void sunlockreader_rwlock(rwlock_t * rwlock) ;

/* function: sunlockwriter_rwlock
 * Asserts that <unlockwriter_rwlock> has no error. */
void sunlockwriter_rwlock(rwlock_t * rwlock) ;



// section: inline implementation

/* define: init_rwlock
 * Implements <rwlock_t.init_rwlock>. */
#define init_rwlock(rwlock)   \
         ((void)(*(rwlock) = (rwlock_t) rwlock_INIT))

/* define: slockreader_rwlock
 * Implements <rwlock_t.slockreader_rwlock>. */
#define slockreader_rwlock(rwlock) \
         (assert(!lockreader_rwlock(rwlock)))

/* define: slockwriter_rwlock
 * Implements <rwlock_t.slockwriter_rwlock>. */
#define slockwriter_rwlock(rwlock) \
         (assert(!lockwriter_rwlock(rwlock)))

/* define: sunlockreader_rwlock
 * Implements <rwlock_t.sunlockreader_rwlock>. */
#define sunlockreader_rwlock(rwlock) \
         (assert(!unlockreader_rwlock(rwlock)))

/* define: sunlockwriter_rwlock
 * Implements <rwlock_t.sunlockwriter_rwlock>. */
#define sunlockwriter_rwlock(rwlock) \
         (assert(!unlockwriter_rwlock(rwlock)))

#endif
