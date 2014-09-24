/* title: DoubleLinkedList-Node

   Defines node type <dlist_node_t> which can be stored in a
   list of type <dlist_t>. Management overhead of objects which
   wants to be stored in a <dlist_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/node/dlist_node.h
    Header file <DoubleLinkedList-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_DLIST_NODE_HEADER
#define CKERN_DS_INMEM_NODE_DLIST_NODE_HEADER

/* typedef: struct dlist_node_t
 * Export <dlist_node_t> into global namespace. */
typedef struct dlist_node_t dlist_node_t;


/* struct: dlist_node_t
 * Provides the means for linking an object to two others of the same type.
 * This kind of object is managed by the double linked list object <dlist_t>.
 * The next or previous node can be reached from this node in O(1).
 * An object which wants to be member of a list must inherit from <dlist_node_t>. */
struct dlist_node_t {
   /* variable: next
    * Points to next node in the list.
    * If this node is currently not part of any list this value is set to NULL. */
   dlist_node_t  * next;
   /* variable: prev
    * Points to previous node in the list.
    * If this node is currently not part of any list this value is set to NULL. */
   dlist_node_t  * prev;
};

// group: lifetime

/* define: dlist_node_INIT
 * Static initializer.
 * Before inserting a node into a list do not forget to initialize the next pointer with NULL.
 *
 * Note:
 * The next pointer is checked against NULL in the precondition of every insert function
 * of every list implementation. The <dlist_node_t.prev> pointer is omitted from the check.
 * This ensures that a node is not inserted in more than one list by mistake. */
#define dlist_node_INIT { 0, 0 }

// group: generic

/* function: cast_dlistnode
 * Casts node into pointer to <dlist_node_t>.
 * The pointer node must point to an object which has
 * the same data members as <dlist_node_t>. */
dlist_node_t * cast_dlistnode(void * node) ;



// section: inline implementation

/* define: cast_dlistnode
 * Implements <dlist_node_t.cast_dlistnode>. */
#define cast_dlistnode(node) \
         ( __extension__ ({                            \
            static_assert(                             \
                  &((node)->next)                      \
                  == &((dlist_node_t*) (uintptr_t)     \
                       &(node)->next)->next            \
                  && &((node)->prev)                   \
                     == &((dlist_node_t*) (uintptr_t)  \
                          &(node)->next)->prev,        \
                  "ensure compatible structure"        \
            );                                         \
            (dlist_node_t*) &(node)->next;             \
         }))

#endif
