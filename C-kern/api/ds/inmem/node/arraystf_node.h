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
struct string_t;

// === exported types
struct arraystf_node_t;
struct arraystf_mwaybranch_t;
union  arraystf_unode_t;


/* struct: arraystf_node_t
 * Generic user node type stored by <arraystf_t>.
 * See also <string_t>. */
typedef struct arraystf_node_t {
   /* variable: addr
    * Memory start address of binary/string key. */
   const uint8_t* addr;
   /* variable: size
    * Length of key in memory in bytes. */
   size_t         size;
} arraystf_node_t;

// group: lifetime

/* define: arraystf_node_INIT
 * Static initializer with parameter length in bytes and address of key. */
#define arraystf_node_INIT(length, key)   { .addr = key, .size = length }

/* define: arraystf_node_INIT_CSTR
 * Static initializer with parameter cstr referencing a "constant string". */
#define arraystf_node_INIT_CSTR(cstr)     { .addr = (const uint8_t*)(cstr), .size = (sizeof(cstr)?sizeof(cstr)-1:0) }

// group: conversion

/* function: cast_arraystfnode
 * Cast <string_t> into <arraystf_node_t>. */
static inline arraystf_node_t * cast_arraystfnode(struct string_t * str);

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
 * >    ...;
 * >    // declares: const uint8_t * keyaddr; size_t keysize;
 * >    arraystf_node_EMBED(keyaddr, keysize)
 * > }
 * >
 * > // instead of
 * > struct object_t {
 * >    ...;
 * >    arraystf_node_t arraystfnode;
 * > } */
#define arraystf_node_EMBED(name_addr, name_size) \
         const uint8_t * name_addr;               \
         size_t          name_size


/* struct: arraystf_mwaybranch_t
 * Internal node to implement a *multiway* trie.
 * Currently this node type supports only a 4-way tree. */
typedef struct arraystf_mwaybranch_t {
   /* variable: child
    * A 4-way array of child nodes. */
   union arraystf_unode_t * child[4];
   /* variable: offset
    * Memory offset of first data byte of key used to branch.
    * The key value at this offset is then shifted right according to
    * value <shift> and masked to get an index into array <child>. */
   size_t   offset;
   /* variable: shift
    * Index of bit of key data byte at position <offset> used to branch.
    * The two bits at position shift and shift+1 are used as index into array <child>.
    * To get the correct child pointer use the following formula;
    * > uint8_t               pos = key[offset]; // array index
    * > arraystf_mwaybranch_t * branch;           // current branch node
    * > branch->child[(pos >> branch->shift) & 0x03] */
   uint8_t  shift;
   /* variable: used
    * The number of entries in <child> which are *not* 0. */
   uint8_t  used;
} arraystf_mwaybranch_t;

// group: lifetime

/* function: init_arraystfmwaybranch
 * Initializes a new branch node.
 * A branch node must point to at least two child nodes. This is the reason
 * two pointers and their corresponding index key has to be provided as parameter. */
static inline void init_arraystfmwaybranch(/*out*/arraystf_mwaybranch_t * branch, size_t offset, unsigned shift, size_t data1, union arraystf_unode_t * childnode1, size_t data2, union arraystf_unode_t * childnode2);

// group: query

/* function: childindex_arraystfmwaybranch
 * Determines the index into the internal array <arraystf_mwaybranch_t.childs>.
 * The index is calculated from parameter *data* which is the key value at offset <arraystf_mwaybranch_t.offset>. */
unsigned childindex_arraystfmwaybranch(arraystf_mwaybranch_t * branch, size_t data);

// group: change

/* function: setchild_arraystfmwaybranch
 * Changes entries in arry <arraystf_mwaybranch_t.child>. */
static inline void setchild_arraystfmwaybranch(arraystf_mwaybranch_t * branch, unsigned childindex, union arraystf_unode_t * childnode);


/* union: arraystf_unode_t
 * Either <arraystf_node_t> or <arraystf_mwaybranch_t>.
 * The least significant bit in the pointer to this node discriminates between the two. */
typedef union arraystf_unode_t {
   arraystf_node_t         node;
   arraystf_mwaybranch_t   branch;
} arraystf_unode_t;

// group: query

/* function: cast2branch_arraystfunode
 * Gets pointer to value <arraystf_mwaybranch_t>. */
static inline arraystf_mwaybranch_t * cast2branch_arraystfunode(arraystf_unode_t * node);

/* function: cast2node_arraystfunode
 * Gets pointer to value <arraystf_node_t>. */
static inline arraystf_node_t * cast2node_arraystfunode(arraystf_unode_t * node);

/* function: isbranchtype_arraystfunode
 * Returns true in case node is pointer to <arraystf_mwaybranch_t>. */
static inline int isbranchtype_arraystfunode(const arraystf_unode_t * node);

// group: conversion

/* function: castPnode_arraystfunode
 * Cast <arraystf_node_t> into <arraystf_unode_t>.
 * You need to call this function to make <isbranchtype_arraystfunode> working properly. */
static inline arraystf_unode_t * castPnode_arraystfunode(arraystf_node_t * node);

/* function: castPbranch_arraystfunode
 * Casts pointer to <arraystf_mwaybranch_t> into pointer to <arraystf_unode_t>.
 * You need to call this function to make <isbranchtype_arraystfunode> working properly. */
static inline arraystf_unode_t * castPbranch_arraystfunode(arraystf_mwaybranch_t * branch);


// section: inline implementation

// group: arraystf_node_t

/* define: cast_arraystfnode
 * Implements <arraystf_node_t.cast_arraystfnode>. */
static inline arraystf_node_t * cast_arraystfnode(struct string_t * str)
{
         return (arraystf_node_t*) str;
}

// group: arraystf_mwaybranch_t

/* define: childindex_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.childindex_arraystfmwaybranch>. */
#define childindex_arraystfmwaybranch(branch, data) \
         (0x03u & ((data) >> (branch)->shift))

/* define: init_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.init_arraystfmwaybranch>. */
static inline void init_arraystfmwaybranch(/*out*/arraystf_mwaybranch_t * branch, size_t offset, unsigned shift, size_t data1, arraystf_unode_t * childnode1, size_t data2, arraystf_unode_t * childnode2)
{
         memset(branch->child, 0, sizeof(branch->child));
         branch->child[0x03u & (data1 >> shift)] = childnode1;
         branch->child[0x03u & (data2 >> shift)] = childnode2;
         branch->offset = offset;
         branch->shift  = (uint8_t) shift;
         branch->used   = 2;
}

/* define: setchild_arraystfmwaybranch
 * Implements <arraystf_mwaybranch_t.setchild_arraystfmwaybranch>. */
static inline void setchild_arraystfmwaybranch(arraystf_mwaybranch_t * branch, unsigned childindex, arraystf_unode_t * childnode)
{
         branch->child[childindex] = childnode;
}

// group: arraystf_unode_t

/* define: cast2branch_arraystfunode
 * Implements <arraystf_unode_t.cast2branch_arraystfunode>. */
static inline arraystf_mwaybranch_t * cast2branch_arraystfunode(arraystf_unode_t * node)
{                                   \
         return (arraystf_mwaybranch_t*) (0x01 ^ (uintptr_t) node);
}

/* define: castPbranch_arraystfunode
 * Implements <arraystf_unode_t.castPbranch_arraystfunode>. */
static inline arraystf_unode_t * castPbranch_arraystfunode(arraystf_mwaybranch_t * branch)
{
         return (arraystf_unode_t*) (0x01 ^ (uintptr_t) branch);
}

/* define: isbranchtype_arraystfunode
 * Implements <arraystf_unode_t.isbranchtype_arraystfunode>. */
static inline int isbranchtype_arraystfunode(const arraystf_unode_t * node)
{
         return 0x01 & ((uintptr_t) node);
}

/* define: cast2node_arraystfunode
 * Implements <arraystf_unode_t.cast2node_arraystfunode>. */
static inline arraystf_node_t * cast2node_arraystfunode(arraystf_unode_t * node)
{
         return (arraystf_node_t*) node;
}

/* define: castPnode_arraystfunode
 * Implements <arraystf_unode_t.castPnode_arraystfunode>. */
static inline arraystf_unode_t * castPnode_arraystfunode(arraystf_node_t * node)
{
         return (arraystf_unode_t*) node;
}

#endif
