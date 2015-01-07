/* title: DoubleLinkedList-Node

   Definiert von <dlist_t> und <dlist2_t> verwalteten Knotentyp <dlist_node_t>
   Objekte, die in einer doppelt verketteten Liste gepsiehcert werden wollen,
   müssen diesen Knotentyp als Feld importieren.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

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
   dlist_node_t * next;
   /* variable: prev
    * Points to previous node in the list.
    * If this node is currently not part of any list this value is set to NULL. */
   dlist_node_t * prev;
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

// group: query

/* function: next_dlistnode
 * Returns the node coming after this node.
 * If node is the last node in the list the first is returned instead. */
dlist_node_t * next_dlistnode(const dlist_node_t * node);

/* function: prev_dlistnode
 * Returns the node coming before this node.
 * If node is the first node in the list the last is returned instead. */
dlist_node_t * prev_dlistnode(const dlist_node_t * node);

/* function: isinlist_dlistnode
 * Gibt true zurück, falls node in einer Liste steht, sonst false. */
bool isinlist_dlistnode(const dlist_node_t * node);


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

/* define: next_dlistnode
 * Implements <dlist_node_t.next_dlistnode>. */
#define next_dlistnode(node) \
         ((node)->next)

/* define: prev_dlistnode
 * Implements <dlist_node_t.prev_dlistnode>. */
#define prev_dlistnode(node) \
         ((node)->prev)

/* define: isinlist_dlistnode
 * Implements <dlist_node_t.isinlist_dlistnode>. */
#define isinlist_dlistnode(node) \
         (0 != (node)->next)

#endif
