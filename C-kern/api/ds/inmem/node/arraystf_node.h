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
#ifndef CKERN_DS_ARRAYSTF_NODE_HEADER
#define CKERN_DS_ARRAYSTF_NODE_HEADER

/* typedef: struct arraystf_node_t
 * Exports <arraystf_node_t>. */
typedef struct arraystf_node_t         arraystf_node_t ;

/* typedef: struct arraystf_mwaybranch_t
 * Exports <arraystf_mwaybranch_t>. */
typedef struct arraystf_mwaybranch_t   arraystf_mwaybranch_t ;


/* struct: arraystf_node_t
 * Generic user node type stored by <arraystf_t>. */
struct arraystf_node_t {
   const uint8_t  * addr ;
   size_t         size ;
} ;

// group: lifetime

/* define: arraystf_node_INIT
 * Static initializer with parameter length in bytes and address of key. */
#define arraystf_node_INIT(length, key)   { .addr = key, .size = length }

/* define: arraystf_node_INIT_CSTR
 * Static initializer with parameter cstr referencing a "constant string". */
#define arraystf_node_INIT_CSTR(cstr)     { .addr = (const uint8_t*)(cstr), .size = (sizeof(cstr)?sizeof(cstr)-1:0) }

/* struct: arraystf_mwaybranch_t
 * Internal node to implement a *multiway* trie.
 * Currently this node type supports only a 4-way tree. */
struct arraystf_mwaybranch_t {
   // union { arraystf_mwaybranch_t * branch[4] ; arraystf_node_t * child[4] ; } ;
   /* variable: child
    * A 4-way array of child nodes. */
   arraystf_node_t   * child[4] ;
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

extern void init_arraystfmwaybranch(arraystf_mwaybranch_t * branch, size_t offset, unsigned shift, size_t data1, arraystf_node_t * childnode1, size_t data2, arraystf_node_t * childnode2) ;

extern unsigned childindex_arraystfmwaybranch(arraystf_mwaybranch_t * branch, size_t data) ;

extern void setchild_arraystfmwaybranch(arraystf_mwaybranch_t * branch, unsigned childindex, arraystf_node_t * childnode) ;


// section: inline implementation

#define childindex_arraystfmwaybranch(branch, data)      (0x03u & ((data) >> (branch)->shift))

#define init_arraystfmwaybranch(branch, _offset, _shift, data1, childnode1, data2, childnode2) \
      do {  memset((branch)->child, 0, sizeof((branch)->child)) ;                   \
            (branch)->child[0x03u & ((data1) >> (shift))] = childnode1 ;            \
            (branch)->child[0x03u & ((data2) >> (shift))] = childnode2 ;            \
            (branch)->offset = _offset ;                                            \
            (branch)->shift  = (uint8_t) _shift ;                                   \
            (branch)->used   = 2 ;                                                  \
         } while(0)

#define setchild_arraystfmwaybranch(branch, childindex, childnode)   \
      do {                                                           \
            (branch)->child[childindex] = (childnode) ;              \
         } while(0)


#endif
