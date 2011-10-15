/* title: RedBlacktree-Index
   Interface to red black tree which allows
   access to a set of sorted elements in O(log n).

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

   file: C-kern/api/os/index/redblacktree.h
    Header file of <RedBlacktree-Index>.

   file: C-kern/os/shared/index/redblacktree.c
    Implementation file of <RedBlacktree-Index impl>.
*/
#ifndef CKERN_OS_INDEX_REFBLACKTREE_HEADER
#define CKERN_OS_INDEX_REFBLACKTREE_HEADER

#include "C-kern/api/aspect/callback.h"
#include "C-kern/api/aspect/treenode3.h"
#include "C-kern/api/aspect/callback/compare.h"
#include "C-kern/api/aspect/callback/free.h"
#include "C-kern/api/aspect/callback/updatekey.h"

/* about:Red black tree
 *
 * See:
 * <http://en.wikipedia.org/wiki/Red_black_tree> for a description of the algorithm.
 *
 * Properties:
 *    1. Every node is colored red or black.
 *    2. Every leaf is a NIL node, and is colored black.
 *    3. If a node is red, then both its children are black.
 *    4. Every simple path from a node to a descendant leaf contains the same number
 *       of black nodes.
 *    5. The root is always black.
 *
 * Height of tree:
 *    The number of black nodes on a path from root to leaf is known
 *    as the black-height of a tree.
 *
 * 1. The above properties guarantee that any path from the root to a leaf
 *    is no more than twice as long as any other path.
 * 2. A tree of height 2n contains at least N=pow(2,n)-1 nodes =>
 *    A search needs at max 2*log2(N) steps.
 *    The implementation of delete & insert traverses the tree
 *    in worst case twice and therefore needs less than 2*2*log2(N) steps.
 * 3. All operations lie in O(log n).
 *    To see why this is true, consider a tree with a black height of n.
 *    The shortest path from root to leaf is n (B-B-...-B).
 *    The longest path from root to leaf is 2n-1 (B-R-B-...-R-B).
 *    It is not possible to insert more black nodes as this would violate property 4.
 *    Since red nodes must have black children (property 3),
 *    having two red nodes in a row is not allowed.
 *    Cause of property 5 the root has always the color black
 *    and therefore the largest path we can construct consists
 *    of an alternation of red-black nodes with the first node colored black.
 *    For every black node we can insert a read one after it except for the last
 *    one. =>
 *    Length of shortest path of black height n:  = n (property 4)
 *    Length of longest path of black height n:   = n + n - 1 = 2n -1
 */

/* typedef: redblacktree_node_t typedef
 * Rename <treenode3_aspect_t>. */
typedef struct treenode3_aspect_t   redblacktree_node_t ;

/* typedef: redblacktree_t typedef
 * Export <redblacktree_t> into global namespace. */
typedef struct redblacktree_t       redblacktree_t ;

/* typedef: redblacktree_compare_nodes_f
 * Callback to check the sorting order of the tree. */
compare_callback_ADAPT(redblacktree_compare_nodes, callback_param_t, redblacktree_node_t, redblacktree_node_t)

/* typedef: redblacktree_compare_f
 * Same as <compare_callback_f> except for type of paramater.
 * The first parameter is of type search key and the seocnd of type node. */
compare_callback_ADAPT(redblacktree_compare, callback_param_t, void, redblacktree_node_t)

/* typedef: redblacktree_update_key
 * Equivalent to function type <updatekey_callback_f>.
 * But the object parameter is of type <redblacktree_node_t>. */
updatekey_callback_ADAPT(redblacktree_update_key, callback_param_t, void, redblacktree_node_t)

/* typedef: redblacktree_free_f
 * Equivalent to function type <free_callback_f>. */
free_callback_ADAPT(redblacktree_free, callback_param_t, redblacktree_node_t)


// group: test

/* function: invariant_redblacktree
 * Checks that this tree meets 5 conditions of red-black trees. */
extern int invariant_redblacktree( redblacktree_t * tree, const redblacktree_compare_nodes_t * compare_callback ) ;

#ifdef KONFIG_UNITTEST
/* function: unittest_os_index_redblacktree
 * Tests interface of <redblacktree_t>. */
extern int unittest_os_index_redblacktree(void) ;
#endif


/* struct: redblacktree_t
 * Object which carries all information to implement a red black tree data type. */
struct redblacktree_t {
   /* variable: root
    * Points to the root object which has no parent. */
   redblacktree_node_t   * root ;
} ;

// group: lifetime

/* define: redblacktree_INIT_FREEABLE
 * Static initializer: Makes calling <free_redblacktree> safe. */
#define redblacktree_INIT_FREEABLE     { (redblacktree_node_t*)0 }

/* function: init_redblacktree
 * Inits an empty tree object.
 * The caller has to provide an unitialized tree object. */
extern int init_redblacktree( /*out*/redblacktree_t * tree ) ;

/* function: free_redblacktree
 * Frees all resources. Calling it twice is safe.  */
extern int free_redblacktree( redblacktree_t * tree, const redblacktree_free_t * free_callback ) ;

// group: search

/* function: findnode_redblacktree
 * Searches for a node with equal key.
 * If it exists it is returned in found_node else ESRCH is returned. */
extern int find_redblacktree( redblacktree_t * tree, const void * key, /*out*/redblacktree_node_t ** found_node, const redblacktree_compare_t * compare_callback ) ;

// group: change

/* function: insertnode_redblacktree
 * Inserts a new node into the tree only if it is unique.
 * If another node exists with the same key as *new_key* nothing is inserted and the function returns EEXIST.
 * If no other node is found new_node is inserted
 * The caller has to allocate the new node and has to transfer ownership. */
extern int insert_redblacktree( redblacktree_t * tree, const void * new_key, redblacktree_node_t * new_node, const redblacktree_compare_t * compare_callback ) ;

/* function: removenode_redblacktree
 * Removes a node if it has equal key.
 * The removed node is not freed but a pointer to it is returned removed_node to the caller. */
extern int remove_redblacktree( redblacktree_t * tree, const void * key, /*out*/redblacktree_node_t ** removed_node, const redblacktree_compare_t * compare_callback ) ;

/* function: updatekey_redblacktree
 * If a node exists in the tree with the same key as old_key then it is removed from the tree and
 * update_key_f is called with new_key and the pointer to the removed node as its parameter.
 * If update_key_f returns success the updated node is inserted.
 * If the removed node can not be inserted cause new_key exists already update_key_f is called a second time
 * to revert the changes (with old_key as first parameter) and the node is re-inserted at its old place. */
extern int updatekey_redblacktree( redblacktree_t * tree, const void * old_key, const void * new_key, const redblacktree_update_key_t * update_key, const redblacktree_compare_t * compare_callback ) ;

/* function: freenodes_redblacktree
 * Removes all nodes from the tree and calls free_callback for every single one. */
extern int freenodes_redblacktree( redblacktree_t * tree, const redblacktree_free_t * free_callback ) ;


#endif
