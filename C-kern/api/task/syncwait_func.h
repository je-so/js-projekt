/* title: SyncWait-Func

   Extends <syncfunc_t> with a <syncwait_node_t>.

   Includes:
   You need to include "C-kern/api/task/syncfunc.h" before this header.

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

   file: C-kern/api/task/syncwait_func.h
    Header file <SyncWait-Func>.
*/
#ifndef CKERN_TASK_SYNCWAIT_FUNC_HEADER
#define CKERN_TASK_SYNCWAIT_FUNC_HEADER

#include "C-kern/api/task/syncwait_node.h"

/* typedef: struct syncwait_func_t
 * Export <syncwait_func_t> into global namespace. */
typedef struct syncwait_func_t syncwait_func_t;


/* struct: syncwait_func_t
 * Allows a function to join a waiting list to wait for an event.
 * If the waiting condition is met the execution can continue.
 *
 * A waiting function can wait for another function to exit
 * in which case <waitnode> is linked with <syncfunc_t.caller>.
 *
 * If a function with a valid <syncfunc_t.caller> begins
 * to wait for another function to exit then the optional field
 * <syncfunc_t.caller> is removed and field <waitnode> does its job.
 *
 * A function with a valid <syncfunc_t.caller> begins to wait for
 * a any condition except for exiting of a function then <syncfunc_t.caller>
 * is kept and <waitnode> is added to another waiting list. */
struct syncwait_func_t {
   // group: private fields
   /* variable: waitnode
    * Connects the waiting function with other waiting nodes in a list.
    * If the list head is also of type <syncwait_node_t> the waiting function
    * can be moved in memory (memory compaction) without breaking the double linked list
    * (prev and next nodes are adapted). */
   syncwait_node_t   waitnode;
   /* variable: syncfunc
    * Describes a <syncfunc_t> with or without optional fields.
    * Never contains optional field <caller> an case this function
    * is waiting on another function to exit. */
   syncfunc_t        syncfunc;
};
