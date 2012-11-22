/* title: ArraySTF-Node

   Defines node type <arraystf_node_t> which can be stored in an
   array of type <arraystf_t>. Defines also the internal node type
   <arraystf_mwaybranch_t> which is used in the implementation of <arraystf_t>.

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

   file: C-kern/api/ds/inmem/node/arraystf_node.h
    Header file <ArraySTF-Node>
*/
#ifndef CKERN_DS_INMEM_NODE_ARRAYSTF_NODE_HEADER
#define CKERN_DS_INMEM_NODE_ARRAYSTF_NODE_HEADER

// forward
struct conststring_t ;

/* typedef: struct arraystf_node_t
 * Export <arraystf_node_t>. User supplied (external) type. */
typedef struct arraystf_node_t         arraystf_node_t ;

/* typedef: struct arraystf_mwaybranch_t
 * Exports <arraystf_mwaybranch_t>i. Internal node type. */
typedef struct arraystf_mwaybranch_t   arraystf_mwaybranch_t ;

/* typedef: struct arraystf_unode_t
 * Export <arraystf_unode_t>. Either internal or external type. */
typedef union arraystf_unode_t         arraystf_unode_t ;


/* struct: arraystf_node_t
 * Generic user node type stored by <arraystf_t>.
 * See also <conststring_t>. */
struct arraystf_node_t {
   /* variable: addr
    * Memory start address of binary/string key. */
   const uint8_t  * addr ;
   /* variable: size
    * Length of key in memory in bytes. */
   size_t         size ;
} ;

// group: lifetime

/* define: arraystf_node_INIT
 * Static initializer with parameter length in bytes and address of key. */
#define arraystf_node_INIT(length, key)   { .addr = key, .size = length }

/* define: arraystf_node_INIT_CSTR
 * Static initializer with parameter cstr referencing a "constant string". */
#define arraystf_node_INIT_CSTR(cstr)     { .addr = (const uint8_t*)(cstr), .size = (sizeof(cstr)?sizeof(cstr)-1:0) }

// group: conversion

/* function: asconststring_arraystfnode
 * Cast <arraystf_node_t> into <conststring_t>. */
const struct conststring_t * asconststring_arraystfnode(const arraystf_node_t * node) ;

/* function: asunode_arraystfnode
 * Cast <arraystf_node_t> into <arraystf_unode_t>.
 * You need to call this function to make <isbranchtype_arraystfunode> working properly. */
arraystf_unode_t * asunode_arraystfnode(arraystf_node_t * node) ;

/* function: fromconststring_arraystfnode
 * Cast <conststring_t> into <arraystf_node_t>. */
arraystf_node_t * fromconststring_arraystfnode(struct conststring_t * str) ;

// group: generic

/* define: arraystf_node_EMBED
 * Allows to embed members of <arraystf_node_t> into another structure.
 *
 * Parameter:
 * name_addr  - The name of the embedded <arraystf_node_t.addr> member.
 * name_size  - The name of the embedded <arraystf_node_t.size> member.
 *
 * Your object must inherit or embed <arraystf_node_t> to be manageable by <arraystf_t>.
 * With macro <arraystf_node_EMBED> you can do
 * > struct object_t {
 * >    ... ;
 * >    // declares: const uint8_t * keyaddr ; size_t keysize ;
 * >    arraystf_node_EMBED(keyaddr, keysize)
 * > }
 * >
 * > // instead of
 * > struct object_t {
 * >    ... ;
 * >    arraystf_node_t arraystfnode ;
 * > } */
#define arraystf_node_EMBED(name_addr, name_size)  \
      const uint8_t *   name_addr ;                \
      size_t            name_size


/* struct: arraystf_mwaybranch_t
 * Internal node to implement a *multiway* trie.
 * Currently this node type supports only a 4-way tree. */
struct arraystf_mwaybranch_t {
   /* variable: child
    * A 4-way array of child nodes. */
   arraystf_unode_t  * child[4] ;
   /* variable: offset
    * Memory offset of first data byte of key used to branch.
    * The key value at this offset is then shifted right according to
    * value <shift> and masked to get an index into array <child>. */
   size_t            offset ;
   /* variable: shift
    * Index of bit of key data byte at position <offset> used to branch.
    * The two bits at position shift and shift+1 are used as index into array <child>.
    * To get the correct child pointer use the following formula;
    * > uint8_t               pos = key[offset] ; // array index
    * > arraystf_mwaybranch_t * branch ;           // current branch node
    * > branch->child[(pos >> branch->shift) & 0x03] */
   uint8_t           shift ;
   /* variable: used
    * The number of entries in <child> which are *not* 0. */
   uint8_t           used ;
} ;

// group: lifetime

/* function: init_arraystfmwaybranch
 * Initializes a new branch node.
 * A branch node must point to at least two child nodes. This is the reason
 * two pointers and their corresponding index key has to be provided as parameter. */
void init_arraystfmwaybranch(/*out*/arraystf_mwaybranch_t * branch, size_t offset, unsigned shift, size_t data1, arraystf_unode_t * childnode1, size_t data2, arraystf_unode_t * childnode2) ;

// group: query

/* function: asunode_arraystfmwaybranch
 * Casts object pointer into pointer to <arraystf_unode_t>.
 * You need to call this function to make <isbranchtype_arraystfunode> working properly. */
arraystf_unode_t * asunode_arraystfmwaybranch(arraystf_mwaybranch_t * branch) ;

/* function: childindex_arraystfmwaybranch
 * Determines the index into the internal array <arraystf_mwaybranch_t.childs>.
 * The index is calculated from parameter *data* which is the key value at the <arraystf_mwaybranch_t.offset>. */
unsigned childindex_arraystfmwaybranch(arraystf_mwaybranch_t * branch, size_t data) ;

// group: change

/* function: setchild_arraystfmwaybranch
 * Changes entries in arry <arraystf_mwaybranch_t.child>. */
void setchild_arraystfmwaybranch(arraystf_mwaybranch_t * branch, unsigned childindex, arraystf_node_t * childnode) ;


/* union: arraystf_unode_t
 * Either <arraystf_node_t> or <arraystf_mwaybranch_t>.
 * The least significant bit in the pointer to this node discriminates between the two. */
union arraystf_unode_t {
   arraystf_node_t         node ;
   arraystf_mwaybranch_t   branch ;
} ;

// group: query

/* function: asbranch_arraystfunode
 * Casts pointer to <arraystf_unode_t> into pointer to <arraystf_mwaybranch_t>. */
arraystf_mwaybranch_t * asbranch_arraystfunode(arraystf_unode_t * node) ;

/* function: asnode_arraystfunode
 * Casts object pointer into pointer to <arraystf_node_t>. */
arraystf_node_t * asnode_arraystfunode(arraystf_unode_t * node) ;

/* function: isbranchtype_arraystfunode
 * Returns true in case node is pointer to <arraystf_mwaybranch_t>. */
int isbranchtype_arraystfunode(const arraystf_unode_t * node) ;


// section: inline implementation

/* define: asconststring_arraystfnode
 * Implements <arraystf_node_t.asconststring_arraystfnode>. */
#define asconststring_arraystfnode(node)                             \
      (  __extension__ ({                                            \
            const arraystf_node_t * _node1 = (node) ;                \
            (const struct conststring_t*) _node1 ;                   \
         }))

/* define: asunode_arraystfnode
 * Implements <arraystf_node_t.asunode_arraystfnode>. */
#define asunode_arraystfnode(node)                                   \
      ( __extension__ ({                                             \
            struct arraystf_node_t * _node1 = (node) ;               \
            (arraystf_unode_t*) _node1 ;                             \
      }))

/* define: fromconststring_arraystfnode
 * Implements <arraystf_node_t.fromconststring_arraystfnode>. */
#define fromconststring_arraystfnode(str)                            \
      (  __extension__ ({                                            \
            struct conststring_t * _str1 = (str) ;                   \
            (arraystf_node_t*) (_str1) ;                             \
         }))

/* define: asunode_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.asunode_arraystfmwaybranch>. */
#define asunode_arraystfmwaybranch(branch)                           \
      (  __extension__ ({                                            \
            arraystf_mwaybranch_t * _branch = (branch) ;             \
            (arraystf_unode_t*) (0x01 ^ (uintptr_t)(_branch)) ;      \
         }))

/* define: asbranch_arraystfunode
 * Implements <arraystf_unode_t.asbranch_arraystfunode>. */
#define asbranch_arraystfunode(node)                                 \
      (  __extension__ ({                                            \
            arraystf_unode_t * _node1 = (node) ;                     \
            (arraystf_mwaybranch_t*) (0x01 ^ (uintptr_t)(_node1));   \
         }))

/* define: arraystf
 * Implements <arraystf_unode_t.asnode_arraystfunode>. */
#define asnode_arraystfunode(node)                                   \
      (  __extension__ ({                                            \
            arraystf_unode_t * _node3 = (node) ;                     \
            (arraystf_node_t*) _node3 ;                              \
         }))

/* define: childindex_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.childindex_arraystfmwaybranch>. */
#define childindex_arraystfmwaybranch(branch, data)      (0x03u & ((data) >> (branch)->shift))

/* define: init_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.init_arraystfmwaybranch>. */
#define init_arraystfmwaybranch(branch, _offset, _shift, data1, childnode1, data2, childnode2) \
      do {  memset((branch)->child, 0, sizeof((branch)->child)) ;                   \
            (branch)->child[0x03u & ((data1) >> (shift))] = childnode1 ;            \
            (branch)->child[0x03u & ((data2) >> (shift))] = childnode2 ;            \
            (branch)->offset = _offset ;                                            \
            (branch)->shift  = (uint8_t) _shift ;                                   \
            (branch)->used   = 2 ;                                                  \
         } while(0)

/* define: isbranchtype_arraystfunode
 * Implements <arraystf_unode_t.isbranchtype_arraystfunode>. */
#define isbranchtype_arraystfunode(node)                             \
      (  __extension__ ({                                            \
            const arraystf_unode_t * _node4 = (node) ;               \
            ((uintptr_t)(_node4) & 0x01) ;                           \
         }))

/* define: setchild_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.setchild_arraystfmwaybranch>. */
#define setchild_arraystfmwaybranch(branch, childindex, childnode)   \
      do {                                                           \
            (branch)->child[childindex] = (childnode) ;              \
         } while(0)


#endif
