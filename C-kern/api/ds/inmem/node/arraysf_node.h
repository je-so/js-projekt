/* title: ArraySF-Node
   Defines node type <arraysf_node_t> which can be stored in an
   array of type <arraysf_t>. Defines also the internal node type
   <arraysf_mwaybranch_t> which is used in the implementation of <arraysf_t>.

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

   file: C-kern/api/ds/inmem/node/arraysf_node.h
    Header file <ArraySF-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_ARRAYSF_NODE_HEADER
#define CKERN_DS_INMEM_NODE_ARRAYSF_NODE_HEADER

/* typedef: struct arraysf_node_t
 * Export <arraysf_node_t>. User supplied (external) type. */
typedef struct arraysf_node_t          arraysf_node_t ;

/* typedef: struct arraysf_mwaybranch_t
 * Export <arraysf_mwaybranch_t>. Internal node type. */
typedef struct arraysf_mwaybranch_t    arraysf_mwaybranch_t ;

/* typedef: struct arraysf_unode_t
 * Export <arraysf_unode_t>. Either internal or external type. */
typedef union arraysf_unode_t          arraysf_unode_t ;


/* struct: arraysf_node_t
 * Generic node type stored by <arraysf_t>. */
struct arraysf_node_t {
   /* variable: pos
    * Index of node stored in <arraysf_t>. */
   size_t   pos ;
} ;

// group: lifetime

/* define: arraysf_node_INIT
 * Static initializer with parameter pos of type size_t. */
#define arraysf_node_INIT(pos)         { pos }

// group: query

/* function: asunode_arraysfnode
 * Casts node pointer into pointer to <arraysf_unode_t>.
 * You need to call this function to make <isbranchtype_arraysfunode> working properly. */
arraysf_unode_t * asunode_arraysfnode(arraysf_node_t * node) ;

// group: generic

/* define: arraysf_node_EMBED
 * Allows to embed members of <arraysf_node_t> into another structure.
 *
 * Parameter:
 * name_pos  - The name of the embedded <arraysf_node_t.pos> member.
 *
 * Your object must inherit or embed <arraysf_node_t> to be manageable by <arraysf_t>.
 * With macro <arraysf_node_EMBED> you can do
 * > struct object_t {
 * >    ... ;
 * >    // declares: size_t   arrayindex ;
 * >    arraysf_node_EMBED(arrayindex)
 * > }
 * >
 * > // instead of
 * > struct object_t {
 * >    ... ;
 * >    arraysf_node_t arraysfnode ;
 * > } */
#define arraysf_node_EMBED(name_pos)   size_t name_pos


/* struct: arraysf_mwaybranch_t
 * Internal node to implement a *multiway* trie.
 * Currently this node type supports only a 4-way tree. */
struct arraysf_mwaybranch_t {
   /* variable: child
    * A 4-way array of child nodes. */
   arraysf_unode_t   * child[4] ;
   /* variable: shift
    * Position of bit in array index used to branch.
    * The two bits at position shift and shift+1 are used as index into array <child>.
    * To get the correct child pointer use the following formula
    * > size_t               pos ;        // array index
    * > arraysf_mwaybranch_t * branch ;   // current branch node
    * > branch->child[(pos >> branch->shift) & 0x03] */
   uint8_t           shift ;
   /* variable: used
    * The number of entries in <child> which are *not* 0. */
   uint8_t           used ;
} ;

// group: lifetime

/* function: init_arraysfmwaybranch
 * Initializes a new branch node.
 * A branch node must point to at least two child nodes. This is the reason
 * two pointers and their corresponding index key has to be provided as parameter. */
void init_arraysfmwaybranch(arraysf_mwaybranch_t * branch, unsigned shift, size_t pos1, arraysf_unode_t * childnode1, size_t pos2, arraysf_unode_t * childnode2) ;

// group: query

/* function: asunode_arraysfmwaybranch
 * Casts object pointer into pointer to <arraysf_unode_t>.
 * You need to call this function to make <isbranchtype_arraysfunode> working properly. */
arraysf_unode_t * asunode_arraysfmwaybranch(arraysf_mwaybranch_t * branch) ;

/* function: childindex_arraysfmwaybranch
 * Determines the index into the internal array <arraysf_mwaybranch_t.childs>.
 * The index is calculated from parameter *pos* which is the index of the node. */
unsigned childindex_arraysfmwaybranch(arraysf_mwaybranch_t * branch, size_t pos) ;

// group: change

/* function: setchild_arraysfmwaybranch
 * Changes entries in arry <arraysf_mwaybranch_t.child>. */
void setchild_arraysfmwaybranch(arraysf_mwaybranch_t * branch, unsigned childindex, arraysf_node_t * childnode) ;


/* union: arraysf_unode_t
 * Either <arraysf_node_t> or <arraysf_mwaybranch_t>.
 * The least significant bit in the pointer to this node discriminates between the two. */
union arraysf_unode_t {
   arraysf_node_t       node ;
   arraysf_mwaybranch_t branch ;
} ;

// group: query

/* function: asbranch_arraysfunode
 * Casts pointer to <arraysf_unode_t> into pointer to <arraysf_mwaybranch_t>. */
arraysf_mwaybranch_t * asbranch_arraysfunode(arraysf_unode_t * node) ;

/* function: asnode_arraysfunode
 * Casts object pointer into pointer to <arraysf_node_t>. */
arraysf_node_t * asnode_arraysfunode(arraysf_unode_t * node) ;

/* function: isbranchtype_arraysfunode
 * Returns true in case node is pointer to <arraysf_mwaybranch_t>. */
int isbranchtype_arraysfunode(const arraysf_unode_t * node) ;


// section: inline implementation

/* define: asunode_arraysfnode
 * Implements <arraysf_node_t.asunode_arraysfnode>. */
#define asunode_arraysfnode(node)                     \
      ( __extension__ ({                              \
            struct arraysf_node_t * _node1 = (node) ; \
            (arraysf_unode_t*) (_node1) ;             \
      }))

/* define: asunode_arraysfmwaybranch
 * Implements <arraysf_mwaybranch_t.asunode_arraysfmwaybranch>. */
#define asunode_arraysfmwaybranch(branch)                         \
      (  __extension__ ({                                         \
            arraysf_mwaybranch_t * _branch = (branch) ;           \
            (arraysf_unode_t*) (0x01 ^ (uintptr_t)(_branch)) ;    \
         }))

/* define: asbranch_arraysfunode
 * Implements <arraysf_unode_t.asbranch_arraysfunode>. */
#define asbranch_arraysfunode(node)                               \
      ( __extension__ ({                                          \
            arraysf_unode_t * _node2 = (node) ;                   \
            (arraysf_mwaybranch_t*)(0x01 ^ (uintptr_t)(_node2)) ; \
      }))

/* define: asnode_arraysfunode
 * Implements <arraysf_unode_t.asnode_arraysfunode>. */
#define asnode_arraysfunode(node)                                 \
      ( __extension__ ({                                          \
            arraysf_unode_t * _node3 = (node) ;                   \
            (arraysf_node_t*) _node3 ;                            \
      }))

/* define: childindex_arraysfmwaybranch
 * Implements <arraysf_mwaybranch_t.childindex_arraysfmwaybranch>. */
#define childindex_arraysfmwaybranch(branch, pos)     (0x03u & ((pos) >> (branch)->shift))

/* define: init_arraysfmwaybranch
 * Implements <arraysf_mwaybranch_t.init_arraysfmwaybranch>. */
#define init_arraysfmwaybranch(branch, _shift, pos1, childnode1, pos2, childnode2)  \
      do {  memset((branch)->child, 0, sizeof((branch)->child)) ;                   \
            (branch)->child[0x03u & ((pos1) >> (shift))] = childnode1 ;             \
            (branch)->child[0x03u & ((pos2) >> (shift))] = childnode2 ;             \
            (branch)->shift = (uint8_t) _shift ;                                    \
            (branch)->used  = 2 ;                                                   \
         } while(0)

/* define: isbranchtype_arraysfunode
 * Implements <arraysf_unode_t.isbranchtype_arraysfunode>. */
#define isbranchtype_arraysfunode(node)                     \
      ( __extension__ ({                                    \
            const arraysf_unode_t * _node4 = (node) ;       \
            ((uintptr_t)(_node4) & 0x01) ;                  \
      }))

/* define: setchild_arraysfmwaybranch
 * Implements <arraysf_mwaybranch_t.setchild_arraysfmwaybranch>. */
#define setchild_arraysfmwaybranch(branch, childindex, childnode)    \
      do {                                                           \
            (branch)->child[childindex] = (childnode) ;              \
         } while(0)


#endif
