/* title: Semaphore
   Offers interface for accessing semaphores.
   Semaphores are used to signal events between different <osthread_t>.

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

   file: C-kern/api/os/task/semaphore.h
    Header file of <Semaphore>.

   file: C-kern/os/Linux/semaphore.c
    Linux implementation file <Semaphore Linux>.
*/
#ifndef CKERN_OS_TASK_SEMAPHORE_HEADER
#define CKERN_OS_TASK_SEMAPHORE_HEADER

/* typedef: semaphore_t typedef
 * Export <semaphore_t>. */
typedef struct semaphore_t       semaphore_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_task_semaphore
 * Tests system semaphore functionality. */
extern int unittest_os_task_semaphore(void) ;
#endif


/* struct: semaphore_t
 * Describes a system semaphore used between threads. */
struct semaphore_t {
   sys_semaphore_t   sys_sema ;
} ;

// group: lifetime

/* define: semaphore_INIT_FREEABLE
 * Static initializer. */
#define semaphore_INIT_FREEABLE  { sys_semaphore_INIT_FREEABLE }

/* function: init_semaphore
 * */
extern int init_semaphore(/*out*/semaphore_t * semaobj, uint16_t init_signal_count) ;

/* function: free_semaphore
 * */
extern int free_semaphore(semaphore_t * semaobj) ;

// group: change

/* function: signal_semaphore
 * Wakes up nr_waiters waiters.
 * Internally this function increments a signal counter with the number nr_waiters.
 * Calling this function nr_waiters times with a value of 1 has therefore the same effect.
 * If the internal counter would overflow the signal function waits until at least
 * that many calls <wait_semaphore> has been done so that adding nr_waiters
 * to the counter does no more overflow.
 * On Linux a 64bit counter is used internally. */
extern int signal_semaphore(semaphore_t * semaobj, uint32_t nr_waiters) ;

/* function: wait_semaphore
 * Waits until a signal is received.
 * This functions waits until the internal counter becomes greater zero.
 * The signal counter is decremented by one and the function returns with success (0). */
extern int wait_semaphore(semaphore_t * semaobj) ;


#endif