/* title: Splaytree-Index
   Interface to splay tree which allows
   access to a set of sorted elements in
   O(log n) amortized time.

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

   file: C-kern/api/os/index/splaytree.h
    Header file of <Splaytree-Index>.

   file: C-kern/os/shared/index/splaytree.c
    Implementation file of <Splaytree-Index impl>.
*/
#ifndef CKERN_OS_INDEX_SPLAYTREE_HEADER
#define CKERN_OS_INDEX_SPLAYTREE_HEADER

#include "C-kern/api/aspect/treenode2.h"
#include "C-kern/api/aspect/callback.h"
#include "C-kern/api/aspect/callback/compare.h"
#include "C-kern/api/aspect/callback/free_resource.h"
#include "C-kern/api/aspect/callback/update_key.h"

/* typedef: splaytree_node_t typedef
 * Rename <treenode2_aspect_t>. */
typedef struct treenode2_aspect_t      splaytree_node_t ;

/* typedef: splaytree_t typedef
 * Export <splaytree_t> into global namespace. */
typedef struct splaytree_t             splaytree_t ;

/* typedef: splaytree_compare_nodes_f
 * Callback to check the sorting order of the tree. */
callback_compare_ADAPT(splaytree_compare_nodes, splaytree_node_t, splaytree_node_t)

/* typedef: splaytree_compare_f
 * Callback function used to determine the sorting order.
 * Same as <callback_compare_f> except for type of paramater.
 * The first parameter is of type search key and the seocnd of type node. */
 callback_compare_ADAPT(splaytree_compare, void, splaytree_node_t)

/* typedef: splaytree_update_key_f
 * Equivalent to function type <callback_update_key_f>.
 * But the object parameter is of type <splaytree_node_t>. */
callback_update_key_ADAPT(splaytree_update_key, void, splaytree_node_t)

/* typedef: splaytree_free_f
 * Equivalent to function type <callback_free_resource_f>. */
callback_free_resource_ADAPT(splaytree_free, splaytree_node_t)


// section: Functions

// group: test

/* function: invariant_splaytree
 * Checks that all nodes are stored in correct search order with calls to compare. */
extern int invariant_splaytree( splaytree_t * tree, const splaytree_compare_nodes_t * compare_callback) ;

#ifdef KONFIG_UNITTEST
/* function: unittest_os_index_splaytree
 * Tests all functions of <splaytree_t>. */
extern int unittest_os_index_splaytree(void) ;
#endif


/* struct: splaytree_t
 * Object which carries all information to implement a splay tree data type. */
struct splaytree_t {
   /* variable: root
    * Points to the root object which has no parent. */
   splaytree_node_t  * root ;
} ;

// group: lifetime

/* define: splaytree_INIT
 * Static initializer. You can use splaytree_INIT(0) instead of <init_splaytree>.
 * After assigning you can call <free_splaytree> or any other function safely. */
#define splaytree_INIT(root_node)   { root_node }

/* define: splaytree_INIT_FREEABLE
 * Static initializer: After assigning you can call <free_splaytree> without harm. */
#define splaytree_INIT_FREEABLE     splaytree_INIT(0)

/* function: init_splaytree
 * Inits an empty tree object.
 * The caller has to provide an unitialized tree object. */
extern int init_splaytree( /*out*/splaytree_t * tree ) ;

/* function: free_splaytree
 * Frees all resources. Calling it twice is safe.  */
extern int free_splaytree( splaytree_t * tree, const splaytree_free_t * free_callback ) ;

// group: search

/* function: findnode_redblacktree
 * Searches for a node with equal key.
 * If it exists it is returned in found_node else ESRCH is returned. */
extern int find_splaytree( splaytree_t * tree, const void * key, /*out*/splaytree_node_t ** found_node, const splaytree_compare_t * compare_callback ) ;

// group: change

/* function: insert_splaytree
 * Inserts a new node into the tree only if it is unique.
 * If another node exists with the same key as *new_key* nothing is inserted and the function returns EEXIST.
 * If no other node is found new_node is inserted
 * The caller has to allocate the new node and has to transfer ownership. */
extern int insert_splaytree( splaytree_t * tree, const void * new_key, splaytree_node_t * new_node, const splaytree_compare_t * compare_callback ) ;

/* function: remove_splaytree
 * Removes a node if it has equal key.
 * The removed node is not freed but a pointer to it is returned removed_node to the caller. */
extern int remove_splaytree( splaytree_t * tree, const void * key, /*out*/splaytree_node_t ** removed_node, const splaytree_compare_t * compare_callback ) ;

/* function: updatekey_splaytree
 * If a node exists in the tree with the same key as old_key then it is removed from the tree and
 * update_key_f is called with new_key and the pointer to the removed node as its parameter.
 * If update_key_f returns success the updated node is inserted.
 * If the removed node can not be inserted cause new_key exists already update_key_f is called a second time
 * to revert the changes (with old_key as first parameter) and the node is re-inserted at its old place. */
extern int updatekey_splaytree( splaytree_t * tree, const void * old_key, const void * new_key, const splaytree_update_key_t * update_key, const splaytree_compare_t * compare_callback ) ;

/* function: freenodes_splaytree
 * Removes all nodes from the tree and calls free_callback for every single one. */
extern int freenodes_splaytree( splaytree_t * tree, const splaytree_free_t * free_callback ) ;


// section: inline implementations

/* define: init_splaytree
 * Is implemented as simple static assignment - it never fails. */
#define /*int*/ init_splaytree( /*out*/tree ) \
   (__extension__ ({ *tree = (splaytree_t) splaytree_INIT(0) ; 0 ; }))

#endif
