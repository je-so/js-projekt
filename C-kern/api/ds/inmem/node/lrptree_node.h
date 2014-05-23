/* title: LRPTree-Node

   Defines node type <lrptree_node_t> which can be stored in trees
   which need left, right, and parent pointers (see <redblacktree_t>).

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/node/lrptree_node.h
    Header file <LRPTree-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_LRPTREE_NODE_HEADER
#define CKERN_DS_INMEM_NODE_LRPTREE_NODE_HEADER

/* typedef: struct lrptree_node_t
 * Export <lrptree_node_t> into global namespace. */
typedef struct lrptree_node_t         lrptree_node_t ;


/* struct: lrptree_node_t
 * Management overhead of objects which wants to be stored in a <redblacktree_t>.
 * A node of type <lrptree_node_t> can be stored in binary tree structures which
 * needs a pointer to left and right childs and to the parent node.
 *
 * >      left╭──────╮right
 * >      ╭───┤ node ├──╮
 * >      │   ╰∆────∆╯  │
 * >  ╭───∇───╮│    │╭──∇────╮
 * >  │ left  ├╯    ╰┤ right │
 * >  │ child │parent│ child │
 * >  ╰┬─────┬╯      ╰┬─────┬╯
 * >  left  right    left  right */
struct lrptree_node_t {
   /* variable: left
    * Points to left subtree. If there is no subtree this value is set to NULL. */
   lrptree_node_t * left ;
   /* variable: right
    * Points to right subtree. If there is no subtree this value is set to NULL. */
   lrptree_node_t * right ;
   /* variable: parent
    * Points to parent node. If this is the root node this value is set to NULL. */
   lrptree_node_t * parent ;
} ;

// group: lifetime

/* define: lrptree_node_INIT
 * Static initializer. */
#define lrptree_node_INIT { 0, 0, 0 }

#endif
