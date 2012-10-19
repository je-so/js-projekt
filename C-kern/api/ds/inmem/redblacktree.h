/* title: RedBlacktree-Index

   Interface to red black tree which allows
   access to a set of sorted elements in O(log n).

   Precondition:
   1. - include "C-kern/api/ds/typeadapt.h" before including this file.

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

   file: C-kern/api/ds/inmem/redblacktree.h
    Header file of <RedBlacktree-Index>.

   file: C-kern/ds/inmem/redblacktree.c
    Implementation file of <RedBlacktree-Index impl>.
*/
#ifndef CKERN_DS_INMEM_REDBLACKTREE_HEADER
#define CKERN_DS_INMEM_REDBLACKTREE_HEADER

#include "C-kern/api/ds/inmem/node/lrptree_node.h"

/* about: Red Black Tree
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

/* typedef: struct redblacktree_t
 * Export <redblacktree_t> into global namespace. */
typedef struct redblacktree_t             redblacktree_t ;

/* typedef: struct redblacktree_iterator_t
 * Export <redblacktree_iterator_t>. */
typedef struct redblacktree_iterator_t    redblacktree_iterator_t ;

/* typedef: redblacktree_node_t
 * Rename <lrptree_node_t> into <redblacktree_node_t>. */
typedef struct lrptree_node_t             redblacktree_node_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_redblacktree
 * Test implementation of <redblacktree_t>. */
int unittest_ds_inmem_redblacktree(void) ;
#endif


/* struct: redblacktree_iterator_t
 * Iterates over elements contained in <redblacktree_t>.
 * The iterator supports removing or deleting of the current node.
 * > redblacktree_t tree ;
 * > fill_tree(&tree) ;
 * > foreach (_redblacktree, &tree, node) {
 * >    if (need_to_remove(node)) {
 * >       err = remove_redblacktree(&tree, get_key_from_node(node), &node)) ;
 * >    }
 * > }
 * */
struct redblacktree_iterator_t {
   redblacktree_node_t * next ;
} ;

// group: lifetime

/* define: redblacktree_iterator_INIT_FREEABLE
 * Static initializer. */
#define redblacktree_iterator_INIT_FREEABLE     { 0 }

/* function: initfirst_redblacktreeiterator
 * Initializes an iterator for <redblacktree_t>. */
int initfirst_redblacktreeiterator(/*out*/redblacktree_iterator_t * iter, redblacktree_t * tree) ;

/* function: initlast_redblacktreeiterator
 * Initializes an iterator of <redblacktree_t>. */
int initlast_redblacktreeiterator(/*out*/redblacktree_iterator_t * iter, redblacktree_t * tree) ;

/* function: free_redblacktreeiterator
 * Frees an iterator of <redblacktree_t>. */
int free_redblacktreeiterator(redblacktree_iterator_t * iter) ;

// group: iterate

/* function: next_redblacktreeiterator
 * Returns next node of tree in ascending order.
 * The first call after <initfirst_redblacktreeiterator> returns the node with the lowest key.
 * In case no next node exists false is returned and parameter node is not changed. */
bool next_redblacktreeiterator(redblacktree_iterator_t * iter, redblacktree_t * tree, /*out*/redblacktree_node_t ** node) ;

/* function: prev_redblacktreeiterator
 * Returns next node of tree in descending order.
 * The first call after <initlast_redblacktreeiterator> returns the node with the highest key.
 * In case no previous node exists false is returned and parameter node is not changed. */
bool prev_redblacktreeiterator(redblacktree_iterator_t * iter, redblacktree_t * tree, /*out*/redblacktree_node_t ** node) ;


/* struct: redblacktree_t
 * Object which carries all information to implement a red black tree data type.
 *
 * typeadapt_t:
 * The service <typeadapt_lifetime_it.delete_object> of <typeadapt_t.lifetime> is used in <free_redblacktree> and <removenodes_redblacktree>.
 * The service <typeadapt_keycomparator_it.cmp_key_object> of <typeadapt_t.keycomparator> is used in <find_redblacktree> and <remove_redblacktree>.
 * The service <typeadapt_keycomparator_it.cmp_object> of <typeadapt_t.keycomparator> is used in <invariant_redblacktree>.
 * */
struct redblacktree_t {
   /* variable: root
    * Points to the root object which has no parent. */
   redblacktree_node_t  * root ;
   /* variable: nodeadp
    * Offers lifetime + keycomparator services to handle stored nodes. */
   typeadapt_member_t   nodeadp ;
} ;

// group: lifetime

/* define: redblacktree_INIT_FREEABLE
 * Static initializer: Makes calling <free_redblacktree> safe. */
#define redblacktree_INIT_FREEABLE                 redblacktree_INIT(0, typeadapt_member_INIT_FREEABLE)

/* define: redblacktree_INIT
 * Static initializer. You can use <redblacktree_INIT> with the returned values prvided by <getinistate_redblacktree>.
 * Parameter root is a pointer to <redblacktree_node_t> and nodeadp must be of type <typeadapt_member_t> (no pointer). */
#define redblacktree_INIT(root, nodeadp)           { root, nodeadp }

/* function: init_redblacktree
 * Inits an empty tree object.
 * The <typeadapt_member_t> is copied but the <typeadapt_t> it references is not.
 * So do not delete <typeadapt_t> as long as this object lives. */
int init_redblacktree(/*out*/redblacktree_t * tree, const typeadapt_member_t * nodeadp) ;

/* function: free_redblacktree
 * Frees all resources. Calling it twice is safe.  */
int free_redblacktree(redblacktree_t * tree) ;

// group: query

/* function: getinistate_redblacktree
 * Returns the current state of <redblacktree_t> for later use in <redblacktree_INIT>. */
static inline void getinistate_redblacktree(const redblacktree_t * tree, /*out*/redblacktree_node_t ** root, /*out*/typeadapt_member_t * nodeadp/*0=>ignored*/) ;

/* function: isempty_redblacktree
 * Returns true if tree contains no elements. */
bool isempty_redblacktree(const redblacktree_t * tree) ;

// group: foreach-support

/* typedef: iteratortype_redblacktree
 * Declaration to associate <redblacktree_iterator_t> with <redblacktree_t>. */
typedef redblacktree_iterator_t      iteratortype_redblacktree ;

/* typedef: iteratedtype_redblacktree
 * Declaration to associate <redblacktree_node_t> with <redblacktree_t>. */
typedef redblacktree_node_t          iteratedtype_redblacktree ;

// group: search

/* function: find_redblacktree
 * Searches for a node with equal key.
 * If it exists it is returned in found_node else ESRCH is returned. */
int find_redblacktree(redblacktree_t * tree, const void * key, /*out*/redblacktree_node_t ** found_node) ;

// group: change

/* function: insert_redblacktree
 * Inserts a new node into the tree only if it is unique.
 * If another node exists with the same key as *new_key* nothing is inserted and the function returns EEXIST.
 * If no other node is found new_node is inserted.
 * The caller has to allocate the new node and has to transfer ownership. */
int insert_redblacktree(redblacktree_t * tree, const void * new_key, redblacktree_node_t * new_node) ;

/* function: remove_redblacktree
 * Removes a node if its key equals parameter *key*.
 * The removed node is not freed but a pointer to it is returned in *removed_node* to the caller. */
int remove_redblacktree(redblacktree_t * tree, const void * key, /*out*/redblacktree_node_t ** removed_node) ;

/* function: removenodes_redblacktree
 * Removes all nodes from the tree.
 * For every removed node <typeadapt_lifetime_it.delete_object> is called. */
int removenodes_redblacktree(redblacktree_t * tree) ;

// group: test

/* function: invariant_redblacktree
 * Checks that this tree meets 5 conditions of red-black trees. */
int invariant_redblacktree(redblacktree_t * tree) ;

// group: generic

/* define: redblacktree_IMPLEMENT
 * Generates interface of <redblacktree_t> storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix  - The suffix name of all generated tree interface functions, e.g. "init##_fsuffix".
 * object_t  - The type of object which can be stored and retrieved from this tree.
 *             The object must contain a field of type <redblacktree_node_t>.
 * key_t     - The type of key the objects are sorted by.
 * nodename  - The access path of the field <redblacktree_node_t> in type object_t.
 * */
void redblacktree_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, TYPENAME key_t, IDNAME nodename) ;


// section: inline implementation

/* define: free_redblacktreeiterator
 * Implements <redblacktree_iterator_t.free_redblacktreeiterator> as NOP. */
#define free_redblacktreeiterator(iter)      ((iter)->next = 0, 0)

/* function: getinistate_redblacktree
 * Implements <redblacktree_t.getinistate_redblacktree>. */
static inline void getinistate_redblacktree(const redblacktree_t * tree, /*out*/redblacktree_node_t ** root, /*out*/typeadapt_member_t * nodeadp)
{
   *root = tree->root ;
   if (0 != nodeadp) *nodeadp = tree->nodeadp ;
}

/* define: init_redblacktree
 * Implements <redblacktree_t.init_redblacktree>. */
#define init_redblacktree(tree,nodeadp)      ((void)(*(tree) = (redblacktree_t) redblacktree_INIT(0, *(nodeadp))))

/* define: isempty_redblacktree
 * Implements <redblacktree_t.isempty_redblacktree>. */
#define isempty_redblacktree(tree)           (0 == (tree)->root)

/* define: redblacktree_IMPLEMENT
 * Implements <redblacktree_t.redblacktree_IMPLEMENT>. */
#define redblacktree_IMPLEMENT(_fsuffix, object_t, key_t, nodename)  \
   typedef redblacktree_iterator_t  iteratortype##_fsuffix ;         \
   typedef object_t                 iteratedtype##_fsuffix ;         \
   static inline int  initfirst##_fsuffix##iterator(redblacktree_iterator_t * iter, redblacktree_t * tree) __attribute__ ((always_inline)) ;   \
   static inline int  initlast##_fsuffix##iterator(redblacktree_iterator_t * iter, redblacktree_t * tree) __attribute__ ((always_inline)) ;    \
   static inline int  free##_fsuffix##iterator(redblacktree_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(redblacktree_iterator_t * iter, redblacktree_t * tree, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline bool prev##_fsuffix##iterator(redblacktree_iterator_t * iter, redblacktree_t * tree, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline void init##_fsuffix(/*out*/redblacktree_t * tree, const typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix(redblacktree_t * tree) __attribute__ ((always_inline)) ; \
   static inline void getinistate##_fsuffix(const redblacktree_t * tree, /*out*/object_t ** root, /*out*/typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline bool isempty##_fsuffix(const redblacktree_t * tree) __attribute__ ((always_inline)) ; \
   static inline int  find##_fsuffix(redblacktree_t * tree, const key_t key, /*out*/object_t ** found_node) __attribute__ ((always_inline)) ; \
   static inline int  insert##_fsuffix(redblacktree_t * tree, const key_t new_key, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline int  remove##_fsuffix(redblacktree_t * tree, const key_t key, /*out*/object_t ** removed_node) __attribute__ ((always_inline)) ; \
   static inline int  removenodes##_fsuffix(redblacktree_t * tree) __attribute__ ((always_inline)) ; \
   static inline int  invariant##_fsuffix(redblacktree_t * tree) __attribute__ ((always_inline)) ; \
   static inline redblacktree_node_t * asnode##_fsuffix(object_t * object) { \
      static_assert(&((object_t*)0)->nodename == (redblacktree_node_t*)offsetof(object_t, nodename), "correct type") ; \
      return (redblacktree_node_t *) ((uintptr_t)object + offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobject##_fsuffix(redblacktree_node_t * node) { \
      return (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobjectnull##_fsuffix(redblacktree_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) : 0 ; \
   } \
   static inline void init##_fsuffix(/*out*/redblacktree_t * tree, const typeadapt_member_t * nodeadp) { \
      init_redblacktree(tree, nodeadp) ; \
   } \
   static inline int  free##_fsuffix(redblacktree_t * tree) { \
      return free_redblacktree(tree) ; \
   } \
   static inline void getinistate##_fsuffix(const redblacktree_t * tree, /*out*/object_t ** root, /*out*/typeadapt_member_t * nodeadp) { \
      redblacktree_node_t * rootnode ; \
      getinistate_redblacktree(tree, &rootnode, nodeadp) ; \
      *root = asobjectnull##_fsuffix(rootnode) ; \
   } \
   static inline bool isempty##_fsuffix(const redblacktree_t * tree) { \
      return isempty_redblacktree(tree) ; \
   } \
   static inline int  find##_fsuffix(redblacktree_t * tree, const key_t key, /*out*/object_t ** found_node) { \
      int err = find_redblacktree(tree, (void*)key, (redblacktree_node_t**)found_node) ; \
      if (err == 0) *found_node = asobject##_fsuffix(*(redblacktree_node_t**)found_node) ; \
      return err ; \
   } \
   static inline int  insert##_fsuffix(redblacktree_t * tree, const key_t new_key, object_t * new_node) { \
      return insert_redblacktree(tree, (void*)new_key, asnode##_fsuffix(new_node)) ; \
   } \
   static inline int  remove##_fsuffix(redblacktree_t * tree, const key_t key, /*out*/object_t ** removed_node) { \
      int err = remove_redblacktree(tree, (void*)key, (redblacktree_node_t**)removed_node) ; \
      if (err == 0) *removed_node = asobject##_fsuffix(*(redblacktree_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int  removenodes##_fsuffix(redblacktree_t * tree) { \
      return removenodes_redblacktree(tree) ; \
   } \
   static inline int  invariant##_fsuffix(redblacktree_t * tree) { \
      return invariant_redblacktree(tree) ; \
   } \
   static inline int  initfirst##_fsuffix##iterator(redblacktree_iterator_t * iter, redblacktree_t * tree) { \
      return initfirst_redblacktreeiterator(iter, tree) ; \
   } \
   static inline int  initlast##_fsuffix##iterator(redblacktree_iterator_t * iter, redblacktree_t * tree) { \
      return initlast_redblacktreeiterator(iter, tree) ; \
   } \
   static inline int  free##_fsuffix##iterator(redblacktree_iterator_t * iter) { \
      return free_redblacktreeiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(redblacktree_iterator_t * iter, redblacktree_t * tree, object_t ** node) { \
      bool isNext = next_redblacktreeiterator(iter, tree, (redblacktree_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(redblacktree_node_t**)node) ; \
      return isNext ; \
   } \
   static inline bool prev##_fsuffix##iterator(redblacktree_iterator_t * iter, redblacktree_t * tree, object_t ** node) { \
      bool isNext = prev_redblacktreeiterator(iter, tree, (redblacktree_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(redblacktree_node_t**)node) ; \
      return isNext ; \
   }

#endif
