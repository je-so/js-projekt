/* title: Splaytree

   Interface to splay tree which allows
   access to a set of sorted elements in
   O(log n) amortized time.

   See <http://en.wikipedia.org/wiki/Splay_tree> for a description.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/splaytree.h
    Header file of <Splaytree>.

   file: C-kern/ds/inmem/splaytree.c
    Implementation file of <Splaytree impl>.
*/
#ifndef CKERN_DS_INMEM_SPLAYTREE_HEADER
#define CKERN_DS_INMEM_SPLAYTREE_HEADER

#include "C-kern/api/ds/inmem/node/lrtree_node.h"

/* typedef: struct splaytree_t
 * Export <splaytree_t> into global namespace. */
typedef struct splaytree_t             splaytree_t ;

/* typedef: struct splaytree_iterator_t
 * Export <splaytree_iterator_t>. */
typedef struct splaytree_iterator_t    splaytree_iterator_t ;

/* typedef: splaytree_node_t
 * Rename <lrtree_node_t> into <splaytree_node_t>. */
typedef struct lrtree_node_t           splaytree_node_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_splaytree
 * Tests all functions of <splaytree_t>. */
int unittest_ds_inmem_splaytree(void) ;
#endif


/* struct: splaytree_iterator_t
 * Iterates over elements contained in <splaytree_t>.
 * The iterator supports removing or deleting of the current node.
 * > splaytree_t tree ;
 * > fill_tree(&tree) ;
 * > foreach (_splaytree, &tree, node) {
 * >    if (need_to_remove(node)) {
 * >       err = remove_splaytree(&tree, node)) ;
 * >    }
 * > }
 * */
struct splaytree_iterator_t {
   splaytree_node_t * next ;
} ;

// group: lifetime

/* define: splaytree_iterator_INIT_FREEABLE
 * Static initializer. */
#define splaytree_iterator_INIT_FREEABLE   { 0 }

/* function: initfirst_splaytreeiterator
 * Initializes an iterator for <splaytree_t>. */
int initfirst_splaytreeiterator(/*out*/splaytree_iterator_t * iter, splaytree_t * tree) ;

/* function: initlast_splaytreeiterator
 * Initializes an iterator of <splaytree_t>. */
int initlast_splaytreeiterator(/*out*/splaytree_iterator_t * iter, splaytree_t * tree) ;

/* function: free_splaytreeiterator
 * Frees an iterator of <splaytree_t>. */
int free_splaytreeiterator(splaytree_iterator_t * iter) ;

// group: iterate

/* function: next_splaytreeiterator
 * Returns next node of tree in ascending order.
 * The first call after <initfirst_splaytreeiterator> returns the node with the lowest key.
 * In case no next node exists false is returned and parameter node is not changed. */
bool next_splaytreeiterator(splaytree_iterator_t * iter, splaytree_t * tree, /*out*/splaytree_node_t ** node) ;

/* function: prev_splaytreeiterator
 * Returns next node of tree in descending order.
 * The first call after <initlast_splaytreeiterator> returns the node with the highest key.
 * In case no previous node exists false is returned and parameter node is not changed. */
bool prev_splaytreeiterator(splaytree_iterator_t * iter, splaytree_t * tree, /*out*/splaytree_node_t ** node) ;


/* struct: splaytree_t
 * Implements a splay tree index.
 * The structure contains a root pointer and a member adapter <typeadapt_member_t>.
 * No other information is maintained.
 *
 * typeadapt_t:
 * The service <typeadapt_lifetime_it.delete_object> of <typeadapt_t.lifetime> is used in <free_splaytree> and <removenodes_splaytree>.
 * The service <typeadapt_comparator_it.cmp_key_object> of <typeadapt_t.comparator> is used in <find_splaytree> and <remove_splaytree>.
 * The service <typeadapt_comparator_it.cmp_object> of <typeadapt_t.comparator> is used in <invariant_splaytree>.
 * */
struct splaytree_t {
   /* variable: root
    * Points to the root object which has no parent. */
   splaytree_node_t     * root ;
   /* variable: nodeadp
    * Offers lifetime + comparator services to handle stored nodes. */
   typeadapt_member_t   nodeadp ;
} ;

// group: lifetime

/* define: splaytree_INIT_FREEABLE
 * Static initializer: After assigning you can call <free_splaytree> without harm. */
#define splaytree_INIT_FREEABLE                 splaytree_INIT(0, typeadapt_member_INIT_FREEABLE)

/* define: splaytree_INIT
 * Static initializer. You can use <splaytree_INIT> with the returned values prvided by <getinistate_splaytree>. */
#define splaytree_INIT(root, nodeadp)           { root, nodeadp }

/* function: init_splaytree
 * Inits an empty tree object.
 * The <typeadapt_member_t> is copied but the <typeadapt_t> it references is not.
 * So do not delete <typeadapt_t> as long as this object lives. */
void init_splaytree(/*out*/splaytree_t * tree, const typeadapt_member_t * nodeadp) ;

/* function: free_splaytree
 * Frees all resources.
 * For every removed node the typeadapter callback <typeadapt_lifetime_it.delete_object> is called.
 * See <typeadapt_member_t> how to construct typeadapter for node member. */
int free_splaytree(splaytree_t * tree) ;

// group: query

/* function: getinistate_splaytree
 * Returns the current state of <splaytree_t> for later use in <splaytree_INIT>. */
static inline void getinistate_splaytree(const splaytree_t * tree, /*out*/splaytree_node_t ** root, /*out*/typeadapt_member_t * nodeadp/*0=>ignored*/) ;

/* function: isempty_splaytree
 * Returns true if tree contains no elements. */
bool isempty_splaytree(const splaytree_t * tree) ;

// group: foreach-support

/* typedef: iteratortype_splaytree
 * Declaration to associate <splaytree_iterator_t> with <splaytree_t>. */
typedef splaytree_iterator_t      iteratortype_splaytree ;

/* typedef: iteratedtype_splaytree
 * Declaration to associate <splaytree_node_t> with <splaytree_t>. */
typedef splaytree_node_t       *  iteratedtype_splaytree ;

// group: search

/* function: find_splaytree
 * Searches for a node with equal key.
 * If it exists it is returned in found_node else ESRCH is returned. */
int find_splaytree(splaytree_t * tree, const void * key, /*out*/splaytree_node_t ** found_node) ;

// group: change

/* function: insert_splaytree
 * Inserts a new node into the tree only if it is unique.
 * If another node exists with the same key as *new_key* nothing is inserted and the function returns EEXIST.
 * If no other node is found new_node is inserted
 * The caller has to allocate the new node and has to transfer ownership. */
int insert_splaytree(splaytree_t * tree, splaytree_node_t * new_node) ;

/* function: remove_splaytree
 * Removes a node from the tree. If the node is not part of the tree the behaviour is undefined !
 * The ownership of the removed node is transfered to the caller. */
int remove_splaytree(splaytree_t * tree, splaytree_node_t * node) ;

/* function: removenodes_splaytree
 * Removes all nodes from the tree.
 * For every removed node <typeadapt_lifetime_it.delete_object> is called. */
int removenodes_splaytree(splaytree_t * tree) ;

// group: test

/* function: invariant_splaytree
 * Checks that all nodes are stored in correct search order.
 * The parameter nodeadp must offer compare objects functionality. */
int invariant_splaytree(splaytree_t * tree) ;

// group: generic

/* define: splaytree_IMPLEMENT
 * Generates interface of <splaytree_t> storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix  - The suffix name of all generated tree interface functions, e.g. "init##_fsuffix".
 * object_t  - The type of object which can be stored and retrieved from this tree.
 *             The object must contain a field of type <splaytree_node_t>.
 * key_t     - The type of key the objects are sorted by.
 * nodename  - The access path of the field <splaytree_node_t> in type object_t.
 * */
void splaytree_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, TYPENAME key_t, IDNAME nodename) ;


// section: inline implementation

/* define: free_splaytreeiterator
 * Implements <splaytree_iterator_t.free_splaytreeiterator> as NOP. */
#define free_splaytreeiterator(iter)         ((iter)->next = 0, 0)

/* function: getinistate_splaytree
 * Implements <splaytree_t.getinistate_splaytree>. */
static inline void getinistate_splaytree(const splaytree_t * tree, /*out*/splaytree_node_t ** root, /*out*/typeadapt_member_t * nodeadp)
{
   *root = tree->root ;
   if (0 != nodeadp) *nodeadp = tree->nodeadp ;
}

/* define: init_splaytree
 * Implements <splaytree_t.init_splaytree>. */
#define init_splaytree(tree,nodeadp)         ((void)(*(tree) = (splaytree_t) splaytree_INIT(0, *(nodeadp))))

/* define: isempty_splaytree
 * Implements <splaytree_t.isempty_splaytree>. */
#define isempty_splaytree(tree)              (0 == (tree)->root)

/* define: splaytree_IMPLEMENT
 * Implements <splaytree_t.splaytree_IMPLEMENT>. */
#define splaytree_IMPLEMENT(_fsuffix, object_t, key_t, nodename)      \
   typedef splaytree_iterator_t  iteratortype##_fsuffix ;   \
   typedef object_t           *  iteratedtype##_fsuffix ;   \
   static inline int  initfirst##_fsuffix##iterator(splaytree_iterator_t * iter, splaytree_t * tree) __attribute__ ((always_inline)) ;   \
   static inline int  initlast##_fsuffix##iterator(splaytree_iterator_t * iter, splaytree_t * tree) __attribute__ ((always_inline)) ;    \
   static inline int  free##_fsuffix##iterator(splaytree_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(splaytree_iterator_t * iter, splaytree_t * tree, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline bool prev##_fsuffix##iterator(splaytree_iterator_t * iter, splaytree_t * tree, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline void init##_fsuffix(/*out*/splaytree_t * tree, const typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix(splaytree_t * tree) __attribute__ ((always_inline)) ; \
   static inline void getinistate##_fsuffix(const splaytree_t * tree, /*out*/object_t ** root, /*out*/typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline bool isempty##_fsuffix(const splaytree_t * tree) __attribute__ ((always_inline)) ; \
   static inline int  find##_fsuffix(splaytree_t * tree, const key_t key, /*out*/object_t ** found_node) __attribute__ ((always_inline)) ; \
   static inline int  insert##_fsuffix(splaytree_t * tree, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline int  remove##_fsuffix(splaytree_t * tree, object_t * node) __attribute__ ((always_inline)) ; \
   static inline int  removenodes##_fsuffix(splaytree_t * tree) __attribute__ ((always_inline)) ; \
   static inline int  invariant##_fsuffix(splaytree_t * tree) __attribute__ ((always_inline)) ; \
   static inline splaytree_node_t * asnode##_fsuffix(object_t * object) { \
      static_assert(&((object_t*)0)->nodename == (splaytree_node_t*)offsetof(object_t, nodename), "correct type") ; \
      return (splaytree_node_t *) ((uintptr_t)object + offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobject##_fsuffix(splaytree_node_t * node) { \
      return (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobjectnull##_fsuffix(splaytree_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) : 0 ; \
   } \
   static inline void init##_fsuffix(/*out*/splaytree_t * tree, const typeadapt_member_t * nodeadp) { \
      init_splaytree(tree, nodeadp) ; \
   } \
   static inline int  free##_fsuffix(splaytree_t * tree) { \
      return free_splaytree(tree) ; \
   } \
   static inline void getinistate##_fsuffix(const splaytree_t * tree, /*out*/object_t ** root, /*out*/typeadapt_member_t * nodeadp) { \
      splaytree_node_t * rootnode ; \
      getinistate_splaytree(tree, &rootnode, nodeadp) ; \
      *root = asobjectnull##_fsuffix(rootnode) ; \
   } \
   static inline bool isempty##_fsuffix(const splaytree_t * tree) { \
      return isempty_splaytree(tree) ; \
   } \
   static inline int  find##_fsuffix(splaytree_t * tree, const key_t key, /*out*/object_t ** found_node) { \
      int err = find_splaytree(tree, (void*)key, (splaytree_node_t**)found_node) ; \
      if (err == 0) *found_node = asobject##_fsuffix(*(splaytree_node_t**)found_node) ; \
      return err ; \
   } \
   static inline int  insert##_fsuffix(splaytree_t * tree, object_t * new_node) { \
      return insert_splaytree(tree, asnode##_fsuffix(new_node)) ; \
   } \
   static inline int  remove##_fsuffix(splaytree_t * tree, object_t * node) { \
      int err = remove_splaytree(tree, asnode##_fsuffix(node)) ; \
      return err ; \
   } \
   static inline int  removenodes##_fsuffix(splaytree_t * tree) { \
      return removenodes_splaytree(tree) ; \
   } \
   static inline int  invariant##_fsuffix(splaytree_t * tree) { \
      return invariant_splaytree(tree) ; \
   } \
   static inline int  initfirst##_fsuffix##iterator(splaytree_iterator_t * iter, splaytree_t * tree) { \
      return initfirst_splaytreeiterator(iter, tree) ; \
   } \
   static inline int  initlast##_fsuffix##iterator(splaytree_iterator_t * iter, splaytree_t * tree) { \
      return initlast_splaytreeiterator(iter, tree) ; \
   } \
   static inline int  free##_fsuffix##iterator(splaytree_iterator_t * iter) { \
      return free_splaytreeiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(splaytree_iterator_t * iter, splaytree_t * tree, object_t ** node) { \
      bool isNext = next_splaytreeiterator(iter, tree, (splaytree_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(splaytree_node_t**)node) ; \
      return isNext ; \
   } \
   static inline bool prev##_fsuffix##iterator(splaytree_iterator_t * iter, splaytree_t * tree, object_t ** node) { \
      bool isNext = prev_splaytreeiterator(iter, tree, (splaytree_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(splaytree_node_t**)node) ; \
      return isNext ; \
   }

#endif
