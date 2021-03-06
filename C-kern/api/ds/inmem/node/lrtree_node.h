/* title: LRTree-Node

   Defines node type <lrtree_node_t> which can be stored in trees
   which need only left and right pointers (see <splaytree_t>).

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/node/lrtree_node.h
    Header file <LRTree-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_LRTREE_NODE_HEADER
#define CKERN_DS_INMEM_NODE_LRTREE_NODE_HEADER

/* typedef: struct lrtree_node_t
 * Export <lrtree_node_t> into global namespace. */
typedef struct lrtree_node_t           lrtree_node_t ;


/* struct: lrtree_node_t
 * Management overhead of objects which wants to be stored in a <splaytree_t>.
 * A node of type <lrtree_node_t> can be stored in binary tree structures which
 * needs a pointer to a left and a right child.
 *
 * >    left╭──────╮right
 * >      ╭─┤ node ├─╮
 * >      │ ╰──────╯ │
 * >  ╭───∇───╮   ╭──∇────╮
 * >  │ left  │   │ right │
 * >  │ child │   │ child │
 * >  ╰┬─────┬╯   ╰┬─────┬╯
 * >  left  right left  right */
struct lrtree_node_t {
   /* variable: left
    * Points to left subtree. If there is no subtree this value is set to NULL. */
   lrtree_node_t * left ;
   /* variable: right
    * Points to right subtree. If there is no subtree this value is set to NULL. */
   lrtree_node_t * right ;
} ;

// group: lifetime

/* define: lrtree_node_INIT
 * Static initializer. */
#define lrtree_node_INIT { 0, 0 }


#endif
