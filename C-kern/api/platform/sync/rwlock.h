/* title: ReadWriteLock

   Implements a simple many readers and single writer lock.

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

#include "C-kern/api/platform/sync/mutex.h"

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
 * Protect a data structure.
 * Either a single writer is allowed to enter the protected area
 * or one or more readers are allowed to enter.
 * As long as no writer tries to acquire this lock no reader has to wait.
 * If a writer tries to acquire this type of lock it has to wait until
 * all readers has released the lock (<suspend_thread>).
 * The last reader which calls <unlockreader_rwlock> resumes the waiting writer (<resume_thread>).
 * During this time all other parties (reader or writer) have to wait for <entrylock>
 * until all readers and the single writer have released the lock.
 *
 * Implementation Notes:
 * The implementation uses two mutex_t namely entrylock and exitlock.
 * The entrylock must be acquired by every reader or writer
 * before it is allowed to enter the protected region.
 * If a writer calls <lockwriter_rwlock> it acquires the entrylock and
 * it holds it until it leaves the protected region.
 *
 * It acquires also the exitlock sets <writer> to itself and checks <nrofreader>.
 * It releases <exitlock> and suspends itself if <nrofreader> was != 0.
 * If a reader calls <lockreader_rwlock> it acquires <entrylock> adds 1 to nrofreader
 * and releases it immediately. A reader can only return from <lockreader_rwlock>
 * if <entrylock> is not hold by a writer. This ensures exclusive access to the rwlock.
 *
 * If a reader leaves the protected region it acquires <exitlock>,
 * decreases <nrofreader> and resumes a writer if <nrofreader> == 0.
 * Then it releases <exitlock>.
 *
 * If a writer has been resumed it acquires <exitlock>
 * checks that there is no reader and then it returns from <lockwriter_rwlock>.
 * If there are readers it suspends itself again. This is necessary cause
 * suspend and resume are implemented with signals and it is possible that a
 * resume occurs from a spurious signal received from outside of the process
 * and therefore the writer has to check the condition again. */
struct rwlock_t {
   /* variable: entrylock
    * A mutex protecting the data structure with an exclusive lock.
    * All readers aquire and release this lock before enter.
    * All writers acquire and hold this before enter. */
   mutex_t           entrylock ;
   /* variable: exitlock
    * A mutex protecting the data structure with an exclusive lock.
    * All readers aquire and release this lock before enter.
    * All writers acquire and hold this before enter. */
   mutex_t           exitlock ;
   /* variable: nrofreader
    * The number of readers currently reading the protected data structure.
    * If this value is no reader has acquired this lock. */
   uint32_t          nrofreader ;
   /* variable: writer
    * The thread which holds <xlock>. If <nrofreader> is greater 0
    * it is in suspended state. */
   struct thread_t * writer  ;
} ;

// group: lifetime

/* define: rwlock_INIT_FREEABLE
 * Static initializer. */
#define rwlock_INIT_FREEABLE              { mutex_INIT_DEFAULT, mutex_INIT_DEFAULT, 0, 0 }

/* function: init_rwlock
 * Initializes internal mutex and clears memebers. */
int init_rwlock(/*out*/rwlock_t * rwlock) ;

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

// group: update

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

// group: safe-update

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
