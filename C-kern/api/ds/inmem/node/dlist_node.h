/* title: DoubleLinkedList-Node

   Defines node type <dlist_node_t> which can be stored in a
   list of type <dlist_t>. Management overhead of objects which
   wants to be stored in a <dlist_t>.

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

   file: C-kern/api/ds/inmem/node/dlist_node.h
    Header file <DoubleLinkedList-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_DLIST_NODE_HEADER
#define CKERN_DS_INMEM_NODE_DLIST_NODE_HEADER

/* typedef: struct dlist_node_t
 * Export <dlist_node_t> into global namespace. */
typedef struct dlist_node_t            dlist_node_t ;


/* struct: dlist_node_t
 * Provides the means for linking an object to two others of the same type.
 * This kind of object is managed by the double linked list object <dlist_t>.
 * The next or previous node can be reached from this node in O(1).
 * An object which wants to be member of a list must inherit from <dlist_node_t>. */
struct dlist_node_t {
   /* variable: next
    * Points to next node in the list.
    * If this node is currently not part of any list this value is set to NULL. */
   dlist_node_t  * next ;
   /* variable: prev
    * Points to previous node in the list.
    * If this node is currently not part of any list this value is set to NULL. */
   dlist_node_t  * prev ;
} ;

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

/* function: genericcast_dlistnode
 * Casts node into pointer to <dlist_node_t>.
 * The pointer node must point to an object which has
 * the same data members as <dlist_node_t>. */
dlist_node_t * genericcast_dlistnode(void * node) ;



// section: inline implementation

/* define: genericcast_dlistnode
 * Implements <dlist_node_t.genericcast_dlistnode>. */
#define genericcast_dlistnode(node)                         \
         ( __extension__ ({                                 \
            static_assert(                                  \
                  sizeof((node)->next)                      \
                  == sizeof(((dlist_node_t*)0)->next)       \
                  && sizeof((node)->prev)                   \
                  == sizeof(((dlist_node_t*)0)->prev)       \
                  && offsetof(dlist_node_t, next)           \
                     < offsetof(dlist_node_t, prev)         \
                  && offsetof(typeof(*(node)), next)        \
                     < offsetof(typeof(*(node)), prev)      \
                  && offsetof(typeof(*(node)), prev)        \
                     - offsetof(typeof(*(node)), next)      \
                     == offsetof(dlist_node_t, prev)        \
                     - offsetof(dlist_node_t, next),        \
                  "ensure compatible structure"             \
            ) ;                                             \
            static_assert(                                  \
                  (typeof((node)->next))0                   \
                     == (dlist_node_t*)0                    \
                  && (typeof((node)->prev))0                \
                     == (dlist_node_t*)0,                   \
                  "ensure same type") ;                     \
            (dlist_node_t*) &(node)->next ;                 \
         }))

#endif
