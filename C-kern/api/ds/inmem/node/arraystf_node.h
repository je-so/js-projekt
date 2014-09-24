/* title: ArraySTF-Node

   Defines node type <arraystf_node_t> which can be stored in an
   array of type <arraystf_t>. Defines also the internal node type
   <arraystf_mwaybranch_t> which is used in the implementation of <arraystf_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/ds/inmem/node/arraystf_node.h
    Header file <ArraySTF-Node>
*/
#ifndef CKERN_DS_INMEM_NODE_ARRAYSTF_NODE_HEADER
#define CKERN_DS_INMEM_NODE_ARRAYSTF_NODE_HEADER

// forward
struct string_t ;

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
 * See also <string_t>. */
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

/* function: stringcast_arraystfnode
 * Cast <string_t> into <arraystf_node_t>. */
arraystf_node_t * stringcast_arraystfnode(struct string_t * str) ;

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

/* function: childindex_arraystfmwaybranch
 * Determines the index into the internal array <arraystf_mwaybranch_t.childs>.
 * The index is calculated from parameter *data* which is the key value at offset <arraystf_mwaybranch_t.offset>. */
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

/* function: branch_arraystfunode
 * Gets pointer to value <arraystf_mwaybranch_t>. */
arraystf_mwaybranch_t * branch_arraystfunode(arraystf_unode_t * node) ;

/* function: node_arraystfunode
 * Gets pointer to value <arraystf_node_t>. */
arraystf_node_t * node_arraystfunode(arraystf_unode_t * node) ;

/* function: isbranchtype_arraystfunode
 * Returns true in case node is pointer to <arraystf_mwaybranch_t>. */
int isbranchtype_arraystfunode(const arraystf_unode_t * node) ;

// group: conversion

/* function: nodecast_arraystfunode
 * Cast <arraystf_node_t> into <arraystf_unode_t>.
 * You need to call this function to make <isbranchtype_arraystfunode> working properly. */
static inline arraystf_unode_t * nodecast_arraystfunode(arraystf_node_t * node) ;

/* function: branchcast_arraystfunode
 * Casts pointer to <arraystf_mwaybranch_t> into pointer to <arraystf_unode_t>.
 * You need to call this function to make <isbranchtype_arraystfunode> working properly. */
static inline arraystf_unode_t * branchcast_arraystfunode(arraystf_mwaybranch_t * branch) ;


// section: inline implementation

// group: arraystf_node_t

/* define: stringcast_arraystfnode
 * Implements <arraystf_node_t.stringcast_arraystfnode>. */
#define stringcast_arraystfnode(str)                                 \
      (  __extension__ ({                                            \
            struct string_t * _str1 = (str) ;                        \
            (arraystf_node_t*) (_str1) ;                             \
         }))

// group: arraystf_mwaybranch_t

/* define: childindex_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.childindex_arraystfmwaybranch>. */
#define childindex_arraystfmwaybranch(branch, data)      (0x03u & ((data) >> (branch)->shift))

/* define: init_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.init_arraystfmwaybranch>. */
#define init_arraystfmwaybranch(branch, _offset, _shift, data1, childnode1, data2, childnode2) \
      do {  memset((branch)->child, 0, sizeof((branch)->child)) ;                   \
            (branch)->child[0x03u & ((data1) >> (_shift))] = childnode1 ;           \
            (branch)->child[0x03u & ((data2) >> (_shift))] = childnode2 ;           \
            (branch)->offset = _offset ;                                            \
            (branch)->shift  = (uint8_t) _shift ;                                   \
            (branch)->used   = 2 ;                                                  \
         } while(0)

/* define: setchild_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.setchild_arraystfmwaybranch>. */
#define setchild_arraystfmwaybranch(branch, childindex, childnode)   \
      do {                                                           \
            (branch)->child[childindex] = (childnode) ;              \
         } while(0)

// group: arraystf_unode_t

/* define: branch_arraystfunode
 * Implements <arraystf_unode_t.branch_arraystfunode>. */
#define branch_arraystfunode(node)                                   \
      (  __extension__ ({                                            \
            arraystf_unode_t * _node1 = (node) ;                     \
            (arraystf_mwaybranch_t*) (0x01 ^ (uintptr_t)(_node1));   \
         }))

/* function: branchcast_arraystfunode
 * Implements <arraystf_unode_t.branchcast_arraystfunode>. */
static inline arraystf_unode_t * branchcast_arraystfunode(arraystf_mwaybranch_t * branch)
{
   return (arraystf_unode_t*) (0x01 ^ (uintptr_t)branch) ;
}

/* define: isbranchtype_arraystfunode
 * Implements <arraystf_unode_t.isbranchtype_arraystfunode>. */
#define isbranchtype_arraystfunode(node)                             \
      (  __extension__ ({                                            \
            const arraystf_unode_t * _node3 = (node) ;               \
            ((uintptr_t)(_node3) & 0x01) ;                           \
         }))

/* define: node_arraystfunode
 * Implements <arraystf_unode_t.node_arraystfunode>. */
#define node_arraystfunode(node)                                     \
      (  __extension__ ({                                            \
            arraystf_unode_t * _node2 = (node) ;                     \
            (arraystf_node_t*) _node2 ;                              \
         }))

/* function: nodecast_arraystfunode
 * Implements <arraystf_unode_t.nodecast_arraystfunode>. */
static inline arraystf_unode_t * nodecast_arraystfunode(arraystf_node_t * node)
{
   return (arraystf_unode_t*) node ;
}

#endif
