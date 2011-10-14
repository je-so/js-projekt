/* title: Threadpool
   Interface to create a pool of threads at once.

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

   file: C-kern/api/os/task/threadpool.h
    Header file of <Threadpool>.

   file: C-kern/os/shared/task/threadpool.c
    Implementation file of <Threadpool impl>.
*/
#ifndef CKERN_OS_TASK_THREADPOOL_HEADER
#define CKERN_OS_TASK_THREADPOOL_HEADER

#include "C-kern/api/os/sync/waitlist.h"

// forward
struct osthread_t ;

/* typedef: threadpool_t
 * Export <threadpool_t>. */
typedef struct threadpool_t         threadpool_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_task_threadpool
 * Tests functionality of a pool of threads. */
extern int unittest_os_task_threadpool(void) ;
#endif


/* struct: threadpool_t
 * Manages a group of threads. */
struct threadpool_t {
   /* variable: idle
    * List of idle threads. They are waiting for her next assignment.
    * If the size of waiting threads <nr_idle> equals <poolsize> all threads are in
    * idle state. */
   waitlist_t           idle ;
   /* variable: poolsize
    * The number of threads created at init time (See <init_threadpool>). */
   uint32_t             poolsize ;
   /* variable: threads
    * The group of threads which are contained in this pool. */
   struct osthread_t  * threads ;
} ;

// group: lifetime

/* define: threadpool_INIT_FREEABLE
 * Static initializer. */
#define threadpool_INIT_FREEABLE    { waitlist_INIT_FREEABLE, 0, (osthread_t*)0 }

/* function: init_threadpool
 * Creates a group of threads which wait for executing tasks. */
extern int init_threadpool(/*out*/threadpool_t * pool, uint8_t nr_of_threads) ;

/* function: free_threadpool
 * Returns EBUSY if some threads are running. */
extern int free_threadpool(threadpool_t * pool) ;

// group: query

/* function: nridle_threadpool
 * The number of idle threads which can be assigned a new task. */
extern int nridle_threadpool(const threadpool_t * pool) ;

/* function: poolsize_threadpool
 * Returns the number of threads allocated and managed by this pool. */
extern int poolsize_threadpool(const threadpool_t * pool) ;

// group: execute


// section: inline implementation

/* function: nridle_threadpool
 * Implements <threadpool_t.nridle_threadpool> inline.
 * > (nrwaiting_waitlist(&(pool)->idle)) */
#define nridle_threadpool(/*const threadpool_t * */pool)     \
   (nrwaiting_waitlist(&(pool)->idle))

/* define: poolsize_threadpool
 * Implements <threadpool_t.poolsize_threadpool> inline.
 * > ((pool)->poolsize) */
#define poolsize_threadpool(/*const threadpool_t * */pool)  \
   ((pool)->poolsize)


#endif
