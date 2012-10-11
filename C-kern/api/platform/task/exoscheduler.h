/* title: Exoscheduler
   Runs all registered exothreads in a round-robin fashion.
   A newly created exothread automatically registers itself
   at the corresponding scheduler.
   TODO: The scheduler registers itself with <maincontext_t>.
   TODO: Accessing is possible with a simple call to <exothreadsched_maincontext>.

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

   file: C-kern/api/platform/task/exoscheduler.h
    Header file of <Exoscheduler>.

   file: C-kern/platform/shared/task/exoscheduler.c
    Implementation file <Exoscheduler impl>.
*/
#ifndef CKERN_PLATFORM_TASK_EXOSCHEDULER_HEADER
#define CKERN_PLATFORM_TASK_EXOSCHEDULER_HEADER

// forward
struct exothread_t ;
struct slist_node_t ;

/* typedef: exothread_scheduler_t typedef
 * Export <exoscheduler_t>. */
typedef struct exoscheduler_t      exoscheduler_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_exoscheduler
 * Tests exothread scheduler functionality. */
int unittest_platform_task_exoscheduler(void) ;
#endif


struct exoscheduler_t {
   /* variable: runlist
    * All running threads are stored this list. */
   struct { struct slist_node_t * last ; }   runlist ;
   size_t                                    runlist_size ;
} ;

// group: lifetime

/* define: exoscheduler_INIT
 * Static initializer. */
#define exoscheduler_INIT           { { 0 }, 0 }

/* function: init_exoscheduler
 * Initializes scheduler object. */
int init_exoscheduler(/*out*/exoscheduler_t * xsched) ;

/* function: free_exoscheduler
 * Frees resources associated with <exoscheduler_t>.
 * If not all xthreads has ended running they are removed from any
 * internal run or wait list but no deleted if they are made known
 * to the scheduler by calling <register_exoscheduler>. */
int free_exoscheduler(exoscheduler_t * xsched) ;

// group: query

// group: change

/* function: register_exoscheduler
 * Registers an <exothread_t> with this scheduler.
 * There is no unregister operation.
 * Every aborted or finished xthread is unregisterd from the runlist
 * automatically within the function <run_exoscheduler>. */
int register_exoscheduler(exoscheduler_t * xsched, struct exothread_t * xthread) ;

// group: execute

/* function: run_exoscheduler
 * Calls run of all registered xthreads.
 * Repeat calling <run_exoscheduler> until all xthreads have finished.
 * Every aborted or finished xthread is unregisterd from the runlist
 * automatically. */
int run_exoscheduler(exoscheduler_t * xsched) ;


#endif
