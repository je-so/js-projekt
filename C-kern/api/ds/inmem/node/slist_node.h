/* title: SingleLinkedList-Node
   Storage overhead needed by objects which
   wants to be stored in a <slist_t>.

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

   file: C-kern/api/ds/inmem/node/slist_node.h
    Header file of <SingleLinkedList-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_SLIST_NODE_HEADER
#define CKERN_DS_INMEM_NODE_SLIST_NODE_HEADER

/* typedef: struct slist_node_t
 * Exports <slist_node_t>. */
typedef struct slist_node_t            slist_node_t ;


/* struct: slist_node_t
 * Provides the means for linking an object to another of the same type.
 * This kind of object is managed by the single linked list object <slist_t>.
 * The next node can be reached from this node in O(1).
 * An object which wants to be member of a list must inherit from <slist_node_t>. */
struct slist_node_t {
   /* variable: next
    * Points to next node in the list.
    * If this node is currently not part of any list this value is set to NULL. */
   struct generic_object_t  * next ;
} ;

// group: lifetime

/* define: slist_node_INIT
 * Static initializer.
 * Before inserting a node into a list do not forget to initialize the next pointer with NULL.
 *
 * Note:
 * The next pointer is checked against NULL in the precondition of every insert function
 * of every list implementation.
 * This ensures that a node is not inserted in more than one list by mistake. */
#define slist_node_INIT     { 0 }

// group: generic

/* define: slist_node_EMBED
 * Allows to embed members of <slist_node_t> into another structure.
 *
 * Parameter:
 * objecttype_t  - The type of object the list node should link.
 * name_nextptr  - The name of the embedded <slist_node_t.next> member.
 *
 * Your object must inherit or embed <slist_node_t> to be manageable by <slist_t>.
 * With macro <slist_node_EMBED> you can do
 * > struct object_t {
 * >    ... ;
 * >    // declares: struct object_t * next ;
 * >    slist_node_EMBED(struct object_t, next)
 * > }
 * >
 * > // instead of
 * > struct object_t {
 * >    ... ;
 * >    slist_node_t listnode ;
 * > } */
#define slist_node_EMBED(objecttype_t, name_nextptr) \
   objecttype_t * name_nextptr ;


// struct: generic_object_t

// group: query

/* function: asslistnode_genericobject
 * Casts object pointer into pointer to <slist_node_t>. */
slist_node_t * asslistnode_genericobject(struct generic_object_t * object, size_t offset_node) ;


// section: inline implementation

/* define: asslistnode_genericobject
 * Implements <generic_object_t.asslistnode_genericobject>. */
#define asslistnode_genericobject(node, offset_node)              \
      (  __extension__ ({                                         \
            struct generic_object_t * _node1 = (node) ;           \
            (slist_node_t*) ((offset_node)+(uint8_t*)(_node1)) ;  \
         }))


#endif
