/* title: ArraySF-Node
   Defines node type <arraysf_node_t> which can be stored in an
   array of type <arraysf_t>. Defines also the internal node type
   <arraysf_mwaybranch_t> which is used in the implementation of <arraysf_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/ds/inmem/node/arraysf_node.h
    Header file <ArraySF-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_ARRAYSF_NODE_HEADER
#define CKERN_DS_INMEM_NODE_ARRAYSF_NODE_HEADER

/* typedef: struct arraysf_node_t
 * Export <arraysf_node_t>. User supplied (external) type. */
typedef struct arraysf_node_t arraysf_node_t;

/* typedef: struct arraysf_mwaybranch_t
 * Export <arraysf_mwaybranch_t>. Internal node type. */
typedef struct arraysf_mwaybranch_t arraysf_mwaybranch_t;

/* typedef: struct arraysf_unode_t
 * Export <arraysf_unode_t>. Either internal or external type. */
typedef union arraysf_unode_t arraysf_unode_t;


/* struct: arraysf_node_t
 * Generic node type stored by <arraysf_t>. */
struct arraysf_node_t {
   /* variable: pos
    * Index of node stored in <arraysf_t>. */
   size_t   pos;
};

// group: lifetime

/* define: arraysf_node_INIT
 * Static initializer with parameter pos of type size_t. */
#define arraysf_node_INIT(pos)         { pos }

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
 * >    ...;
 * >    // declares: size_t   arrayindex;
 * >    arraysf_node_EMBED(arrayindex)
 * > }
 * >
 * > // instead of
 * > struct object_t {
 * >    ...;
 * >    arraysf_node_t arraysfnode;
 * > } */
#define arraysf_node_EMBED(name_pos)   size_t name_pos


/* struct: arraysf_mwaybranch_t
 * Internal node to implement a *multiway* trie.
 * Currently this node type supports only a 4-way tree. */
struct arraysf_mwaybranch_t {
   /* variable: child
    * A 4-way array of child nodes. */
   arraysf_unode_t   * child[4];
   /* variable: shift
    * Position of bit in array index used to branch.
    * The two bits at position shift and shift+1 are used as index into array <child>.
    * To get the correct child pointer use the following formula
    * > size_t               pos;        // array index
    * > arraysf_mwaybranch_t * branch;   // current branch node
    * > branch->child[(pos >> branch->shift) & 0x03] */
   uint8_t           shift;
   /* variable: used
    * The number of entries in <child> which are *not* 0. */
   uint8_t           used;
};

// group: lifetime

/* function: init_arraysfmwaybranch
 * Initializes a new branch node.
 * A branch node must point to at least two child nodes. This is the reason
 * two pointers and their corresponding index key has to be provided as parameter. */
static inline void init_arraysfmwaybranch(/*out*/arraysf_mwaybranch_t * branch, unsigned shift, size_t pos1, arraysf_unode_t * childnode1, size_t pos2, arraysf_unode_t * childnode2);

// group: query

/* function: childindex_arraysfmwaybranch
 * Determines the index into the internal array <arraysf_mwaybranch_t.childs>.
 * The index is calculated from parameter *pos* which is the index of the node. */
unsigned childindex_arraysfmwaybranch(arraysf_mwaybranch_t * branch, size_t pos);

// group: change

/* function: setchild_arraysfmwaybranch
 * Changes entries in arry <arraysf_mwaybranch_t.child>. */
static inline void setchild_arraysfmwaybranch(arraysf_mwaybranch_t * branch, unsigned childindex, arraysf_unode_t * childnode);


/* union: arraysf_unode_t
 * Either <arraysf_node_t> or <arraysf_mwaybranch_t>.
 * The least significant bit in the pointer to this node discriminates between the two. */
union arraysf_unode_t {
   arraysf_node_t       node;
   arraysf_mwaybranch_t branch;
};

// group: query

/* function: cast2branch_arraysfunode
 * Cast node to pointer to <arraysf_mwaybranch_t>.
 *
 * Unchecked Precondition:
 * 0 != isbranchtype_arraysfunode(node) */
static inline arraysf_mwaybranch_t * cast2branch_arraysfunode(arraysf_unode_t * node);

/* function: cast2node_arraysfunode
 * Cast node to pointer to <arraysf_node_t>.
 *
 * Unchecked Precondition:
 * 0 != isbranchtype_arraysfunode(node) */
static inline arraysf_node_t * cast2node_arraysfunode(arraysf_unode_t * node);

/* function: isbranchtype_arraysfunode
 * Returns true in case node is pointer to <arraysf_mwaybranch_t>. */
static inline int isbranchtype_arraysfunode(const arraysf_unode_t * node);

// group: conversion

/* function: castPnode_arraysfunode
 * Casts pointer to <arraysf_node_t> into pointer to <arraysf_unode_t>.
 * You need to call this function to make <isbranchtype_arraysfunode> working properly. */
static inline arraysf_unode_t * castPnode_arraysfunode(arraysf_node_t * node);

/* function: castPbranch_arraysfunode
 * Casts pointer to <arraysf_mwaybranch_t> into pointer to <arraysf_unode_t>.
 * You need to call this function to make <isbranchtype_arraysfunode> working properly. */
static inline arraysf_unode_t * castPbranch_arraysfunode(arraysf_mwaybranch_t * branch);


// section: inline implementation

// group: arraysf_mwaybranch_t

/* define: childindex_arraysfmwaybranch
 * Implements <arraysf_mwaybranch_t.childindex_arraysfmwaybranch>. */
#define childindex_arraysfmwaybranch(branch, pos) \
         (0x03u & ((pos) >> (branch)->shift))

/* define: init_arraysfmwaybranch
 * Implements <arraysf_mwaybranch_t.init_arraysfmwaybranch>. */
static inline void init_arraysfmwaybranch(/*out*/arraysf_mwaybranch_t * branch, unsigned shift, size_t pos1, arraysf_unode_t * childnode1, size_t pos2, arraysf_unode_t * childnode2)
{
         memset(branch->child, 0, sizeof(branch->child));
         branch->child[0x03u & (pos1 >> shift)] = childnode1;
         branch->child[0x03u & (pos2 >> shift)] = childnode2;
         branch->shift = (uint8_t) shift;
         branch->used  = 2;
}

/* define: setchild_arraysfmwaybranch
 * Implements <arraysf_mwaybranch_t.setchild_arraysfmwaybranch>. */
static inline void setchild_arraysfmwaybranch(arraysf_mwaybranch_t * branch, unsigned childindex, arraysf_unode_t * childnode)
{
         branch->child[childindex] = childnode;
}

// group: arraysf_unode_t

/* define: cast2branch_arraysfunode
 * Implements <arraysf_unode_t.cast2branch_arraysfunode>. */
static inline arraysf_mwaybranch_t * cast2branch_arraysfunode(arraysf_unode_t * node)
{
         return (arraysf_mwaybranch_t*) (0x01 ^ (uintptr_t) node);
}

static inline arraysf_unode_t * castPbranch_arraysfunode(arraysf_mwaybranch_t * branch)
{
         return (arraysf_unode_t*) (0x01 ^ (uintptr_t) branch);
}

/* define: isbranchtype_arraysfunode
 * Implements <arraysf_unode_t.isbranchtype_arraysfunode>. */
static inline int isbranchtype_arraysfunode(const arraysf_unode_t * node)
{
         return (0x01 & (uintptr_t) node);
}

/* define: cast2node_arraysfunode
 * Implements <arraysf_unode_t.cast2node_arraysfunode>. */
static inline arraysf_node_t * cast2node_arraysfunode(arraysf_unode_t * node)
{
         return (arraysf_node_t*) node;
}

/* define: castPnode_arraysfunode
 * Implements <arraysf_unode_t.castPnode_arraysfunode>. */
static inline arraysf_unode_t * castPnode_arraysfunode(arraysf_node_t * node)
{
         return (arraysf_unode_t*) node;
}

#endif
