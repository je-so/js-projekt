/* title: SyncWait-Node

   Defines structure/node which is can be linked
   into a double linked list. This node to link
   a function into a list of waiting functions.

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

   file: C-kern/api/task/syncwait_node.h
    Header file <SyncWait-Node>.
*/
#ifndef CKERN_TASK_SYNCWAIT_NODE_HEADER
#define CKERN_TASK_SYNCWAIT_NODE_HEADER

// forward
struct dlist_node_t;

/* typedef: struct syncwait_node_t
 * Export <syncwait_node_t> into global namespace. */
typedef struct syncwait_node_t syncwait_node_t;


/* struct: syncwait_node_t
 * Allows a function to be chained in a double linked list. */
struct syncwait_node_t {
   struct dlist_node_t *   next ;
   struct dlist_node_t *   prev ;
};

// group: lifetime

/* define: syncwait_node_FREE
 * Static initializer. */
#define syncwait_node_FREE \
         { 0, 0 }


#endif