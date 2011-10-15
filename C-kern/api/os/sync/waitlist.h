/* title: Waitlist
   Allows to let threads wait for a certain condition.
   If the condition is signaled the first thread in the
   waiting list is woken up if and the thread argument
   is set to the value given in signaling function.

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

   file: C-kern/api/os/sync/waitlist.h
    Header file of <Waitlist>.

   file: C-kern/os/Linux/waitlist.c
    Linux specific implementation file <Waitlist Linux>.
*/
#ifndef CKERN_OS_SYNCHRONIZATION_WAITLIST_HEADER
#define CKERN_OS_SYNCHRONIZATION_WAITLIST_HEADER

#include "C-kern/api/aspect/callback.h"
#include "C-kern/api/aspect/callback/task.h"

// forward
struct osthread_t ;

/* typedef: waitlist_t typedef
 * Exports <waitlist_t>. */
typedef struct waitlist_t        waitlist_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_sync_waitlist
 * Tests waitlist functionality. */
extern int unittest_os_sync_waitlist(void) ;
#endif


/* struct: waitlist_t
 * Implements facility where threads can wait for a certain condition.
 * Similar to <semaphore_t>. The difference is that a thread's
 * »command« parameter is set to a specific value. Therefore a woken up
 * thread knows what to do next.
 *
 * This object is thread safe. */
struct waitlist_t {
   /* variable: last
    * The root pointer of the list of waiting threads. */
   struct osthread_t  * last ;
   /* variable: nr_waiting
    * The number of threads waiting. */
   size_t               nr_waiting ;
   /* variable: lock
    * Mutex to protect this object from concurrent access. */
   sys_mutex_t          lock ;
} ;

// group: lifetime

/* define: waitlist_INIT_FREEABLE
 * Static initializer. After initialization it is safe to call <free_waitlist>. */
#define waitlist_INIT_FREEABLE   { 0, 0, mutex_INIT_DEFAULT }

/* function: init_waitlist
 * Inits a waiting list. The waiting is protexted by a mutex. */
extern int init_waitlist(/*out*/waitlist_t * wlist) ;

/* function: free_waitlist
 * Wakes up all waiting threads and frees all resources.
 * Make sure that no more other thread is trying to call <wait_waitlist>
 * if <free_waitlist> is processing or was already called. */
extern int free_waitlist(waitlist_t * wlist) ;

// group: query

/* function: isempty_waitlist
 * Returns true if no thread is waiting else false.
 * Before testing the lock on the list is acquired.
 *
 * But in case more than one thread calls <trywakeup_waitlist>
 * you can not be sure that <trywakeup_waitlist> does not return EAGAIN
 * even if <isempty_waitlist> returns false. */
extern bool isempty_waitlist(waitlist_t * wlist) ;

/* function: nrwaiting_waitlist
 * Returns the number of threads waiting on this list.
 * Before testing the lock on the list is acquired.
 *
 * But in case more than one thread calls <trywakeup_waitlist>
 * you can not be sure that <trywakeup_waitlist> does not return EAGAIN
 * even if <nrwaiting_waitlist> returns a value greater 0. */
extern size_t nrwaiting_waitlist(waitlist_t * wlist) ;

// group: change

/* function: wait_waitlist
 * Suspends the calling thread until some other calls <trywakeup_waitlist>.
 * The waiting threads are woken up in FIFO order.
 * The calling thread enters itself as last in the waiting list and then it suspends
 * itself. See also <suspend_osthread>. */
extern int wait_waitlist(waitlist_t * wlist) ;

/* function: trywakeup_waitlist
 * Tries to wake up the first waiting thread.
 * If the list is empty EAGAIN is returned and no error is logged.
 * If the list is not empty the first waiting threads <osthread_t.command> argument is set
 * to command and it is removed from the list. The removed thread is then resumed.
 * See also <resume_osthread>. */
extern int trywakeup_waitlist(waitlist_t * wlist, task_callback_f task_main, callback_param_t * start_arg) ;


#endif
