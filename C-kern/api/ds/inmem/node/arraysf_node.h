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
#ifndef CKERN_DS_ARRAYSF_NODE_HEADER
#define CKERN_DS_ARRAYSF_NODE_HEADER

/* typedef: struct arraysf_node_t
 * Exports <arraysf_node_t>. */
typedef struct arraysf_node_t          arraysf_node_t ;

/* typedef: struct arraysf_mwaybranch_t
 * Exports <arraysf_mwaybranch_t>. */
typedef struct arraysf_mwaybranch_t    arraysf_mwaybranch_t ;


/* struct: arraysf_node_t
 * Generic user node type stored by <arraysf_t>. */
struct arraysf_node_t {
   size_t   pos ;
} ;

// group: lifetime

/* define: arraysf_node_INIT
 * Static initializer with paramater index of type size_t. */
#define arraysf_node_INIT(index)       { (index) }


/* struct: arraysf_mwaybranch_t
 * Internal node to implement a *multiway* trie.
 * Currently this node type supports only a 4-way tree. */
struct arraysf_mwaybranch_t {
   // union { arraysf_mwaybranch_t * branch[4] ; arraysf_node_t * child[4] ; } ;
   /* variable: child
    * A 4-way array of child nodes. */
   arraysf_node_t    * child[4] ;
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

extern void init_arraysfmwaybranch(arraysf_mwaybranch_t * branch, unsigned shift, size_t pos1, arraysf_node_t * childnode1, size_t pos2, arraysf_node_t * childnode2) ;

extern unsigned childindex_arraysfmwaybranch(arraysf_mwaybranch_t * branch, size_t pos) ;

extern void setchild_arraysfmwaybranch(arraysf_mwaybranch_t * branch, unsigned childindex, arraysf_node_t * childnode) ;


// section: inline implementation

#define childindex_arraysfmwaybranch(branch, pos)     (0x03u & ((pos) >> (branch)->shift))

#define init_arraysfmwaybranch(branch, _shift, pos1, childnode1, pos2, childnode2)  \
      do {  memset((branch)->child, 0, sizeof((branch)->child)) ;                   \
            (branch)->child[0x03u & ((pos1) >> (shift))] = childnode1 ;             \
            (branch)->child[0x03u & ((pos2) >> (shift))] = childnode2 ;             \
            (branch)->shift = (uint8_t) _shift ;                                    \
            (branch)->used  = 2 ;                                                   \
         } while(0)

#define setchild_arraysfmwaybranch(branch, childindex, childnode)    \
      do {                                                           \
            (branch)->child[childindex] = (childnode) ;              \
         } while(0)


#endif
