/* title: Semaphore

   Offers interface for accessing semaphores.
   Semaphores are used to signal events between different <thread_t> or <process_t>.
   If a process executes another program the semaphore is closed.
   To prevent this an inherit function would be need and a way to transfer the id and init from id function.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/sync/semaphore.h
    Header file of <Semaphore>.

   file: C-kern/platform/Linux/sync/semaphore.c
    Linux implementation file <Semaphore Linuximpl>.
*/
#ifndef CKERN_PLATFORM_SYNC_SEMAPHORE_HEADER
#define CKERN_PLATFORM_SYNC_SEMAPHORE_HEADER

/* typedef: struct semaphore_t
 * Export <semaphore_t>. */
typedef struct semaphore_t                semaphore_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_sync_semaphore
 * Tests system semaphore functionality. */
int unittest_platform_sync_semaphore(void) ;
#endif


/* struct: semaphore_t
 * Describes a system semaphore used between threads.
 * This thread safe object allows to wait for or to
 * send a signal to one or more waiters. */
struct semaphore_t {
   sys_semaphore_t sys_sema;
};

// group: lifetime

/* define: semaphore_FREE
 * Static initializer. */
#define semaphore_FREE { sys_semaphore_FREE }

/* function: init_semaphore
 * Initializes a semaphore. The internal signal counter is set to init_signal_count.
 * The next init_signal_count calls to <wait_semaphore> therefore succeed without
 * waiting. */
int init_semaphore(/*out*/semaphore_t * semaobj, uint16_t init_signal_count) ;

/* function: free_semaphore
 * Wakes up any waiting threads and frees the associated resources.
 * Make sure that no other thread which is not already waiting on the semaphore
 * accesses it after <free_semaphore> has been called. */
int free_semaphore(semaphore_t * semaobj) ;

// group: synchronize

/* function: signal_semaphore
 * Wakes up signal_count waiters. Or the next signal_count calls to <wait_semaphore>
 * succeed without waiting.
 * Internally this function increments a signal counter with the number signal_count.
 * Calling this function signal_count times with a value of 1 has therefore the same effect.
 * If the internal counter would overflow the signal function waits until at least
 * that many calls to <wait_semaphore> has been done so that adding signal_count
 * to the counter does no more produce an overflow.
 * On Linux a 64bit counter is used internally. */
int signal_semaphore(semaphore_t * semaobj, uint32_t signal_count) ;

/* function: wait_semaphore
 * Waits until a signal is received.
 * This functions waits until the internal counter becomes greater zero.
 * The signal counter is decremented by one and the function returns with success (0). */
int wait_semaphore(semaphore_t * semaobj) ;


#endif
