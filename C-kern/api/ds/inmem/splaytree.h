/* title: Splaytree

   Interface to splay tree which allows
   access to a set of sorted elements in
   O(log n) amortized time.

   See <http://en.wikipedia.org/wiki/Splay_tree> for a description.

   Precondition:
   1. - include "C-kern/api/ds/typeadapt.h" before including this file.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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

// === exported types
struct splaytree_t;
struct splaytree_iterator_t;
/* typedef: splaytree_node_t
 * Rename <lrtree_node_t> into <splaytree_node_t>. */
typedef struct lrtree_node_t splaytree_node_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_splaytree
 * Tests all functions of <splaytree_t>. */
int unittest_ds_inmem_splaytree(void);
#endif


/* struct: splaytree_iterator_t
 * Iterates over elements contained in <splaytree_t>.
 * The iterator supports removing or deleting of the current node.
 * > splaytree_t tree;
 * > fill_tree(&tree);
 * > foreach (_splaytree, node, &tree) {
 * >    if (need_to_remove(node)) {
 * >       err = remove_splaytree(&tree, node));
 * >    }
 * > }
 * */
typedef struct splaytree_iterator_t {
   splaytree_node_t    *next;
   struct splaytree_t  *tree;
   struct typeadapt_t  *typeadp;
   uint16_t             nodeoff;
} splaytree_iterator_t;

// group: lifetime

/* define: splaytree_iterator_FREE
 * Static initializer. */
#define splaytree_iterator_FREE { 0, 0, 0, 0 }

/* function: initfirst_splaytreeiterator
 * Initializes an iterator for <splaytree_t>. */
int initfirst_splaytreeiterator(/*out*/splaytree_iterator_t *iter, struct splaytree_t *tree, uint16_t nodeoffset, typeadapt_t * typeadp);

/* function: initlast_splaytreeiterator
 * Initializes an iterator of <splaytree_t>. */
int initlast_splaytreeiterator(/*out*/splaytree_iterator_t *iter, struct splaytree_t *tree, uint16_t nodeoffset, typeadapt_t * typeadp);

/* function: free_splaytreeiterator
 * Frees an iterator of <splaytree_t>. */
int free_splaytreeiterator(splaytree_iterator_t *iter);

// group: iterate

/* function: next_splaytreeiterator
 * Returns next node of tree in ascending order.
 * The first call after <initfirst_splaytreeiterator> returns the node with the lowest key.
 * In case no next node exists false is returned and parameter node is not changed. */
bool next_splaytreeiterator(splaytree_iterator_t *iter, /*out*/splaytree_node_t ** node);

/* function: prev_splaytreeiterator
 * Returns next node of tree in descending order.
 * The first call after <initlast_splaytreeiterator> returns the node with the highest key.
 * In case no previous node exists false is returned and parameter node is not changed. */
bool prev_splaytreeiterator(splaytree_iterator_t *iter, /*out*/splaytree_node_t ** node);


/* struct: splaytree_t
 * Implements a splay tree index.
 * The structure contains a root pointer.
 * No other information is maintained.
 *
 * typeadapt_t:
 * The service <typeadapt_lifetime_it.delete_object> of <typeadapt_t.lifetime> is used in <free_splaytree> and <removenodes_splaytree>.
 * The service <typeadapt_comparator_it.cmp_key_object> of <typeadapt_t.comparator> is used in <find_splaytree> and <remove_splaytree>.
 * The service <typeadapt_comparator_it.cmp_object> of <typeadapt_t.comparator> is used in <invariant_splaytree>.
 * */
typedef struct splaytree_t {
   /* variable: root
    * Points to the root object which has no parent. */
   splaytree_node_t *root;
} splaytree_t;

// group: lifetime

/* define: splaytree_FREE
 * Static initializer: After assigning you can call <free_splaytree> without harm. */
#define splaytree_FREE splaytree_INIT(0)

/* define: splaytree_INIT
 * Static initializer. You can use <splaytree_INIT> with the returned values prvided by <getinistate_splaytree>. */
#define splaytree_INIT(root) \
         { root }

/* function: init_splaytree
 * Inits an empty tree object. */
void init_splaytree(/*out*/splaytree_t *tree);

/* function: free_splaytree
 * Frees all resources.
 * For every removed node the typeadapter callback <typeadapt_lifetime_it.delete_object> is called.
 * See <typeadapt_t> how to construct a typeadapter. */
int free_splaytree(splaytree_t *tree, uint16_t nodeoffset, typeadapt_t * typeadp);

// group: query

/* function: getinistate_splaytree
 * Returns the current state of <splaytree_t> for later use in <splaytree_INIT>. */
static inline void getinistate_splaytree(const splaytree_t *tree, /*out*/splaytree_node_t ** root);

/* function: isempty_splaytree
 * Returns true if tree contains no elements. */
bool isempty_splaytree(const splaytree_t *tree);

// group: foreach-support

/* typedef: iteratortype_splaytree
 * Declaration to associate <splaytree_iterator_t> with <splaytree_t>. */
typedef splaytree_iterator_t      iteratortype_splaytree;

/* typedef: iteratedtype_splaytree
 * Declaration to associate <splaytree_node_t> with <splaytree_t>. */
typedef splaytree_node_t       *  iteratedtype_splaytree;

// group: search

/* function: find_splaytree
 * Searches for a node with equal key.
 * If it exists it is returned in found_node else ESRCH is returned. */
int find_splaytree(splaytree_t *tree, const void * key, /*out*/splaytree_node_t ** found_node, uint16_t nodeoffset, typeadapt_t * typeadp);

// group: change

/* function: insert_splaytree
 * Inserts a new node into the tree only if it is unique.
 * If another node exists with the same key as *new_key* nothing is inserted and the function returns EEXIST.
 * If no other node is found new_node is inserted
 * The caller has to allocate the new node and has to transfer ownership. */
int insert_splaytree(splaytree_t *tree, splaytree_node_t * new_node, uint16_t nodeoffset, typeadapt_t * typeadp);

/* function: remove_splaytree
 * Removes a node from the tree. If the node is not part of the tree the behaviour is undefined !
 * The ownership of the removed node is transfered to the caller. */
int remove_splaytree(splaytree_t *tree, splaytree_node_t * node, uint16_t nodeoffset, typeadapt_t * typeadp);

/* function: removenodes_splaytree
 * Removes all nodes from the tree.
 * For every removed node <typeadapt_lifetime_it.delete_object> is called. */
int removenodes_splaytree(splaytree_t *tree, uint16_t nodeoffset, typeadapt_t * typeadp);

// group: test

/* function: invariant_splaytree
 * Checks that all nodes are stored in correct search order.
 * The parameter typeadp must offer compare objects functionality. */
int invariant_splaytree(splaytree_t *tree, uint16_t nodeoffset, typeadapt_t * typeadp);

// group: generic

/* function: cast_splaytree
 * Casts tree into <slaytree_t> if that is possible.
 * The generic object tree must have a root pointer to <splaytree_node_t>. */
splaytree_t * cast_splaytree(void *tree);

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
void splaytree_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, TYPENAME key_t, IDNAME nodename);



// section: inline implementation

/* define: free_splaytreeiterator
 * Implements <splaytree_iterator_t.free_splaytreeiterator> as NOP. */
#define free_splaytreeiterator(iter)   \
         ((iter)->next = 0, 0)

/* define: cast_splaytree
 * Implements <splaytree_t.cast_splaytree>. */
#define cast_splaytree(tree) \
         ( __extension__ ({                          \
            typeof(tree) _t;                         \
            _t = (tree);                             \
            static_assert(                           \
               &_t->root                             \
               == &((splaytree_t*) &_t->root)->root, \
               "ensure compatible structure");       \
            (splaytree_t*) &_t->root;                \
         }))

/* function: getinistate_splaytree
 * Implements <splaytree_t.getinistate_splaytree>. */
static inline void getinistate_splaytree(const splaytree_t *tree, /*out*/splaytree_node_t ** root)
{
         *root = tree->root;
}

/* define: init_splaytree
 * Implements <splaytree_t.init_splaytree>. */
#define init_splaytree(tree)   \
         ((void)(*(tree) = (splaytree_t) splaytree_INIT(0)))

/* define: isempty_splaytree
 * Implements <splaytree_t.isempty_splaytree>. */
#define isempty_splaytree(tree)  \
         (0 == (tree)->root)

/* define: splaytree_IMPLEMENT
 * Implements <splaytree_t.splaytree_IMPLEMENT>. */
#define splaytree_IMPLEMENT(_fsuffix, object_t, key_t, nodename) \
   typedef splaytree_iterator_t  iteratortype##_fsuffix; \
   typedef object_t           *  iteratedtype##_fsuffix; \
   static inline splaytree_node_t * cast2node##_fsuffix(object_t * object) { \
      static_assert(&((object_t*)0)->nodename == (splaytree_node_t*)offsetof(object_t, nodename), "correct type"); \
      return (splaytree_node_t *) ((uintptr_t)object + offsetof(object_t, nodename)); \
   } \
   static inline object_t * cast2object##_fsuffix(splaytree_node_t * node) { \
      return (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)); \
   } \
   static inline object_t * castnull2object##_fsuffix(splaytree_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) : 0; \
   } \
   static inline void init##_fsuffix(/*out*/splaytree_t *tree) { \
      init_splaytree(tree); \
   } \
   static inline int  free##_fsuffix(splaytree_t *tree, typeadapt_t * typeadp) { \
      return free_splaytree(tree, offsetof(object_t, nodename), typeadp); \
   } \
   static inline void getinistate##_fsuffix(const splaytree_t *tree, /*out*/object_t ** root) { \
      splaytree_node_t * rootnode; \
      getinistate_splaytree(tree, &rootnode); \
      *root = castnull2object##_fsuffix(rootnode); \
   } \
   static inline bool isempty##_fsuffix(const splaytree_t *tree) { \
      return isempty_splaytree(tree); \
   } \
   static inline int  find##_fsuffix(splaytree_t *tree, const key_t key, /*out*/object_t ** found_node, typeadapt_t * typeadp) { \
      int err = find_splaytree(tree, (void*)key, (splaytree_node_t**)found_node, offsetof(object_t, nodename), typeadp); \
      if (err == 0) *found_node = cast2object##_fsuffix(*(splaytree_node_t**)found_node); \
      return err; \
   } \
   static inline int  insert##_fsuffix(splaytree_t *tree, object_t * new_node, typeadapt_t * typeadp) { \
      return insert_splaytree(tree, cast2node##_fsuffix(new_node), offsetof(object_t, nodename), typeadp); \
   } \
   static inline int  remove##_fsuffix(splaytree_t *tree, object_t * node, typeadapt_t * typeadp) { \
      int err = remove_splaytree(tree, cast2node##_fsuffix(node), offsetof(object_t, nodename), typeadp); \
      return err; \
   } \
   static inline int  removenodes##_fsuffix(splaytree_t *tree, typeadapt_t * typeadp) { \
      return removenodes_splaytree(tree, offsetof(object_t, nodename), typeadp); \
   } \
   static inline int  invariant##_fsuffix(splaytree_t *tree, typeadapt_t * typeadp) { \
      return invariant_splaytree(tree, offsetof(object_t, nodename), typeadp); \
   } \
   static inline int  initfirst##_fsuffix##iterator(splaytree_iterator_t *iter, splaytree_t *tree, typeadapt_t * typeadp) { \
      return initfirst_splaytreeiterator(iter, tree, offsetof(object_t, nodename), typeadp); \
   } \
   static inline int  initlast##_fsuffix##iterator(splaytree_iterator_t *iter, splaytree_t *tree, typeadapt_t * typeadp) { \
      return initlast_splaytreeiterator(iter, tree, offsetof(object_t, nodename), typeadp); \
   } \
   static inline int  free##_fsuffix##iterator(splaytree_iterator_t *iter) { \
      return free_splaytreeiterator(iter); \
   } \
   static inline bool next##_fsuffix##iterator(splaytree_iterator_t *iter, object_t ** node) { \
      bool isNext = next_splaytreeiterator(iter, (splaytree_node_t**)node); \
      if (isNext) *node = cast2object##_fsuffix(*(splaytree_node_t**)node); \
      return isNext; \
   } \
   static inline bool prev##_fsuffix##iterator(splaytree_iterator_t *iter, object_t ** node) { \
      bool isNext = prev_splaytreeiterator(iter, (splaytree_node_t**)node); \
      if (isNext) *node = cast2object##_fsuffix(*(splaytree_node_t**)node); \
      return isNext; \
   }

#endif
