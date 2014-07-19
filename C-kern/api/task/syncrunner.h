/* title: SyncRunner

   Verwaltet ein Menge von <syncfunc_t> in
   einer Reihe von Run- und Wait-Queues.

   Jeder Thread verwendet seinen eigenen <syncrunner_t>.

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
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncrunner.h
    Header file <SyncRunner>.

   file: C-kern/task/syncrunner.c
    Implementation file <SyncRunner impl>.
*/
#ifndef CKERN_TASK_SYNCRUNNER_HEADER
#define CKERN_TASK_SYNCRUNNER_HEADER

#include "C-kern/api/task/syncqueue.h"

// forward
struct syncfunc_t;

/* typedef: struct syncrunner_t
 * Export <syncrunner_t> into global namespace. */
typedef struct syncrunner_t syncrunner_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncrunner
 * Test <syncrunner_t> functionality. */
int unittest_task_syncrunner(void);
#endif


/* struct: syncrunner_t
 * TODO: describe type
 *
 * Another very simple possibility is to use a complex of 8 run queues and 8 wait queues
 * (see <syncwait_func_t>) to store every possible combination of valid optional
 * fields. This would safe memory if a very large number of functions is used and
 * would make handling simple. But it wastes memory in case of a very a small
 * number of functions if the queues are implemented with linked pages with
 * a very large pagesize.
 *
 * TODO: make queue_t adaptable to different page sizes !
 * TODO: syncqueue_t uses 512 or 1024 bytes (check in table with different sizes !!) !
 *
 * */
struct syncrunner_t {
   /* variable: queues
    * Queues wich are used to store <syncthread_t>.
    * Every queue serves a different number of optional fields of <syncfunc_t> and <syncwait_func_t>. */
   syncqueue_t       queues[8 /*run*/+ 4*3 /*wait*/];
};

// group: lifetime

/* define: syncrunner_FREE
 * Static initializer. */
#define syncrunner_FREE \
         { { syncqueue_FREE } }

/* function: init_syncrunner
 * TODO: Describe Initializes object. */
int init_syncrunner(/*out*/syncrunner_t * obj);

/* function: free_syncrunner
 * TODO: Describe Frees all associated resources. */
int free_syncrunner(syncrunner_t * obj);

// group: query

// group: update

int addsf_syncrunner(syncrunner_t * srun, const struct syncfunc_t * sfunc);


// section: inline implementation

/* define: init_syncrunner
 * Implements <syncrunner_t.init_syncrunner>. */
#define init_syncrunner(obj) \
         // TODO: implement


#endif
