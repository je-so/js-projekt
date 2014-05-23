/* title: Patricia-Trie

   Patricia Trie Interface.

   Practical Algorithm to Retrieve Information Coded in Alphanumeric:
   The trie stores for every inserted string a node which contains a bit
   offset into the byte encoded string. The bit at this offset differentiates
   the newly inserted string from an already inserted one. If the newly inserted
   string differs in more than one bit the one with the smallest offset from the
   start of the string is chosen.

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

   file: C-kern/api/ds/inmem/patriciatrie.h
    Header file <Patricia-Trie>.

   file: C-kern/ds/inmem/patriciatrie.c
    Implementation file <Patricia-Trie impl>.
*/
#ifndef CKERN_DS_INMEM_PATRICIATRIE_HEADER
#define CKERN_DS_INMEM_PATRICIATRIE_HEADER

#include "C-kern/api/ds/inmem/node/patriciatrie_node.h"

// forward


/* typedef: struct patriciatrie_t
 * Exports <patriciatrie_t> into global namespace. */
typedef struct patriciatrie_t             patriciatrie_t ;

/* typedef: struct patriciatrie_iterator_t
 * Exports <patriciatrie_iterator_t> into global namespace. */
typedef struct patriciatrie_iterator_t    patriciatrie_iterator_t ;

/* typedef: struct patriciatrie_prefixiter_t
 * Exports <patriciatrie_prefixiter_t> into global namespace. */
typedef struct patriciatrie_prefixiter_t  patriciatrie_prefixiter_t ;



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_patriciatrie
 * Tests implementation of <patriciatrie_t>. */
int unittest_ds_inmem_patriciatrie(void) ;
#endif


/* struct: patriciatrie_t
 * Implements a trie with path compression.
 *
 * typeadapt_t:
 * The service <typeadapt_lifetime_it.delete_object> of <typeadapt_t.lifetime> is used in <free_patriciatrie> and <removenodes_patriciatrie>.
 * The service <typeadapt_getkey_it.getbinarykey> of <typeadapt_t.getbinarykey> is used in find, insert and remove functions.
 *
 * Description:
 * A patricia tree is a type of digital tree and manages nodes of type patriciatrie_node_t.
 * Every node contains a bit offset indexing the search key. If the corresponding bit of the searchkey
 * is 0 the left path is taken else the right path.
 * The bit handling is done by this implementation so it is expected that there is a binary key description
 * (<typeadapt_binarykey_t>) associated with each stored node.
 *
 * Performance:
 * If the set of stored strings is prefix-free it has guaranteed O(log n) (n = number of strings)
 * performance for insert and delete. If the strings are prefixes of each other then performance
 * could degrade to O(n).
 *
 * C-Strings:
 * If you manage C-strings and treat the trailing \0 byte as part of the key it is guaranteed that
 * a set of different strings is prefix-free.
 *
 * When to use:
 * Use patricia tries (crit-bit trees) if strings are prefix free and if they are very large.
 * The reason is that even for very large strings only O(log n) bits are compared. And if
 * strings are large (i.e. strlen >> log n) other algorithms like trees or hash tables has
 * a best effort of at least O(strlen).
 * */
struct patriciatrie_t {
   patriciatrie_node_t  * root ;
   typeadapt_member_t   nodeadp ;
} ;

// group: lifetime

/* define: patriciatrie_FREE
 * Static initializer. */
#define patriciatrie_FREE \
         patriciatrie_INIT(0, typeadapt_member_FREE)

/* define: patriciatrie_INIT
 * Static initializer. You can use <patriciatrie_INIT> with the returned values prvided by <getinistate_patriciatrie>.
 * Parameter root is a pointer to <patriciatrie_node_t> and nodeadp must be of type <typeadapt_member_t> (no pointer). */
#define patriciatrie_INIT(root, nodeadp) \
         { root, nodeadp }

/* function: init_patriciatrie
 * Inits an empty tree object.
 * The <typeadapt_member_t> is copied but the <typeadapt_t> it references is not.
 * So do not delete <typeadapt_t> as long as this object lives. */
void init_patriciatrie(/*out*/patriciatrie_t * tree, const typeadapt_member_t * nodeadp) ;

/* function: free_patriciatrie
 * Frees all resources. Calling it twice is safe. */
int free_patriciatrie(patriciatrie_t * tree) ;

// group: query

/* function: getinistate_patriciatrie
 * Returns the current state of <patriciatrie_t> for later use in <patriciatrie_INIT>. */
static inline void getinistate_patriciatrie(const patriciatrie_t * tree, /*out*/patriciatrie_node_t ** root, /*out*/typeadapt_member_t * nodeadp/*0=>ignored*/) ;

/* function: isempty_patriciatrie
 * Returns true if tree contains no elements. */
bool isempty_patriciatrie(const patriciatrie_t * tree) ;

// group: foreach-support

/* typedef: iteratortype_patriciatrie
 * Declaration to associate <patriciatrie_iterator_t> with <patriciatrie_t>. */
typedef patriciatrie_iterator_t      iteratortype_patriciatrie ;

/* typedef: iteratedtype_patriciatrie
 * Declaration to associate <patriciatrie_node_t> with <patriciatrie_t>. */
typedef patriciatrie_node_t       *  iteratedtype_patriciatrie ;

// group: search

/* function: find_patriciatrie
 * Searches for a node with equal key. If it exists it is returned in found_node else ESRCH is returned. */
int find_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/patriciatrie_node_t ** found_node) ;

// group: change

/* function: insert_patriciatrie
 * Inserts a new node into the tree only if it is unique.
 * If another node exists with the same key nothing is inserted and the function returns EEXIST.
 * The caller has to allocate the new node and has to transfer ownership. */
int insert_patriciatrie(patriciatrie_t * tree, patriciatrie_node_t * newnode) ;

/* function: remove_patriciatrie
 * Removes a node if its key equals searchkey.
 * The removed node is not freed but a pointer to it is returned in *removed_node* to the caller.
 * ESRCH is returned if no node with searchkey exists. */
int remove_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** removed_node) ;

/* function: removenodes_patriciatrie
 * Removes all nodes from the tree. For every removed node <typeadapt_lifetime_it.delete_object> is called. */
int removenodes_patriciatrie(patriciatrie_t * tree) ;

// group: generic

/* define: patriciatrie_IMPLEMENT
 * Generates interface of <patriciatrie_t> storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix  - The suffix name of all generated tree interface functions, e.g. "init##_fsuffix".
 * object_t  - The type of object which can be stored and retrieved from this tree.
 *             The object must contain a field of type <patriciatrie_node_t>.
 * nodename  - The access path of the field <patriciatrie_node_t> in type object_t.
 * */
void patriciatrie_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, IDNAME nodename) ;



/* struct: patriciatrie_iterator_t
 * Iterates over elements contained in <patriciatrie_t>.
 * The iterator supports removing or deleting of the current node. */
struct patriciatrie_iterator_t {
   patriciatrie_node_t * next ;
   patriciatrie_t      * tree ;
} ;

// group: lifetime

/* define: patriciatrie_iterator_FREE
 * Static initializer. */
#define patriciatrie_iterator_FREE { 0, 0 }

/* function: initfirst_patriciatrieiterator
 * Initializes an iterator for <patriciatrie_t>. */
int initfirst_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree) ;

/* function: initlast_patriciatrieiterator
 * Initializes an iterator of <patriciatrie_t>. */
int initlast_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree) ;

/* function: free_patriciatrieiterator
 * Frees an iterator of <patriciatrie_t>. */
int free_patriciatrieiterator(patriciatrie_iterator_t * iter) ;

// group: iterate

/* function: next_patriciatrieiterator
 * Returns next node of tree in ascending order.
 * The first call after <initfirst_patriciatrieiterator> returns the node with the lowest key.
 * In case no next node exists false is returned and parameter node is not changed. */
bool next_patriciatrieiterator(patriciatrie_iterator_t * iter, /*out*/patriciatrie_node_t ** node) ;

/* function: prev_patriciatrieiterator
 * Returns next node of tree in descending order.
 * The first call after <initlast_patriciatrieiterator> returns the node with the highest key.
 * In case no previous node exists false is returned and parameter node is not changed. */
bool prev_patriciatrieiterator(patriciatrie_iterator_t * iter, /*out*/patriciatrie_node_t ** node) ;


/* struct: patriciatrie_prefixiter_t
 * Iterates over elements contained in <patriciatrie_t>.
 * The iterator supports removing or deleting of the current node. */
struct patriciatrie_prefixiter_t {
   patriciatrie_node_t* next ;
   patriciatrie_t     * tree ;
   size_t               prefix_bits ;
} ;

// group: lifetime

/* define: patriciatrie_prefixiter_FREE
 * Static initializer. */
#define patriciatrie_prefixiter_FREE \
         { 0, 0, 0 }

/* function: initfirst_patriciatrieprefixiter
 * Initializes an iterator for <patriciatrie_t> for nodes with prefix *prefixkey*. */
int initfirst_patriciatrieprefixiter(/*out*/patriciatrie_prefixiter_t * iter, patriciatrie_t * tree, size_t keylength, const uint8_t prefixkey[keylength]) ;

/* function: free_patriciatrieprefixiter
 * Frees an prefix iterator of <patriciatrie_t>. */
int free_patriciatrieprefixiter(patriciatrie_iterator_t * iter) ;

// group: iterate

/* function: next_patriciatrieprefixiter
 * Returns next node with a certain prefix in ascending order.
 * The first call after <initfirst_patriciatrieprefixiter> returns the node with the lowest key.
 * In case no next node exists false is returned and parameter node is not changed. */
bool next_patriciatrieprefixiter(patriciatrie_prefixiter_t * iter, /*out*/patriciatrie_node_t ** node) ;


// section: inline implementation

/* define: free_patriciatrieiterator
 * Implements <patriciatrie_iterator_t.free_patriciatrieiterator> as NOP. */
#define free_patriciatrieiterator(iter)      \
         ((iter)->next = 0, 0)

/* define: free_patriciatrieprefixiter
 * Implements <patriciatrie_prefixiter_t.free_patriciatrieprefixiter> as NOP. */
#define free_patriciatrieprefixiter(iter)    \
         ((iter)->next = 0, 0)

/* function: getinistate_patriciatrie
 * Implements <patriciatrie_t.getinistate_patriciatrie>. */
static inline void getinistate_patriciatrie(const patriciatrie_t * tree, /*out*/patriciatrie_node_t ** root, /*out*/typeadapt_member_t * nodeadp)
{
   *root = tree->root ;
   if (0 != nodeadp) *nodeadp = tree->nodeadp ;
}

/* define: init_patriciatrie
 * Implements <patriciatrie_t.init_patriciatrie>. */
#define init_patriciatrie(tree,nodeadp)      ((void)(*(tree) = (patriciatrie_t) patriciatrie_INIT(0, *(nodeadp))))

/* define: isempty_patriciatrie
 * Implements <patriciatrie_t.isempty_patriciatrie>. */
#define isempty_patriciatrie(tree)           (0 == (tree)->root)

/* define: patriciatrie_IMPLEMENT
 * Implements <patriciatrie_t.patriciatrie_IMPLEMENT>. */
#define patriciatrie_IMPLEMENT(_fsuffix, object_t, nodename)   \
   typedef patriciatrie_iterator_t  iteratortype##_fsuffix ;   \
   typedef object_t              *  iteratedtype##_fsuffix ;   \
   static inline int  initfirst##_fsuffix##iterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree) __attribute__ ((always_inline)) ;   \
   static inline int  initlast##_fsuffix##iterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree) __attribute__ ((always_inline)) ;    \
   static inline int  free##_fsuffix##iterator(patriciatrie_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(patriciatrie_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline bool prev##_fsuffix##iterator(patriciatrie_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline void init##_fsuffix(/*out*/patriciatrie_t * tree, const typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix(patriciatrie_t * tree) __attribute__ ((always_inline)) ; \
   static inline void getinistate##_fsuffix(const patriciatrie_t * tree, /*out*/object_t ** root, /*out*/typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline bool isempty##_fsuffix(const patriciatrie_t * tree) __attribute__ ((always_inline)) ; \
   static inline int  find##_fsuffix(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/object_t ** found_node) __attribute__ ((always_inline)) ; \
   static inline int  insert##_fsuffix(patriciatrie_t * tree, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline int  remove##_fsuffix(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/object_t ** removed_node) __attribute__ ((always_inline)) ; \
   static inline int  removenodes##_fsuffix(patriciatrie_t * tree) __attribute__ ((always_inline)) ; \
   static inline patriciatrie_node_t * asnode##_fsuffix(object_t * object) { \
      static_assert(&((object_t*)0)->nodename == (patriciatrie_node_t*)offsetof(object_t, nodename), "correct type") ; \
      return (patriciatrie_node_t *) ((uintptr_t)object + offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobject##_fsuffix(patriciatrie_node_t * node) { \
      return (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobjectnull##_fsuffix(patriciatrie_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) : 0 ; \
   } \
   static inline void init##_fsuffix(/*out*/patriciatrie_t * tree, const typeadapt_member_t * nodeadp) { \
      init_patriciatrie(tree, nodeadp) ; \
   } \
   static inline int  free##_fsuffix(patriciatrie_t * tree) { \
      return free_patriciatrie(tree) ; \
   } \
   static inline void getinistate##_fsuffix(const patriciatrie_t * tree, /*out*/object_t ** root, /*out*/typeadapt_member_t * nodeadp) { \
      patriciatrie_node_t * rootnode ; \
      getinistate_patriciatrie(tree, &rootnode, nodeadp) ; \
      *root = asobjectnull##_fsuffix(rootnode) ; \
   } \
   static inline bool isempty##_fsuffix(const patriciatrie_t * tree) { \
      return isempty_patriciatrie(tree) ; \
   } \
   static inline int  find##_fsuffix(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/object_t ** found_node) { \
      int err = find_patriciatrie(tree, keylength, searchkey, (patriciatrie_node_t**)found_node) ; \
      if (err == 0) *found_node = asobject##_fsuffix(*(patriciatrie_node_t**)found_node) ; \
      return err ; \
   } \
   static inline int  insert##_fsuffix(patriciatrie_t * tree, object_t * new_node) { \
      return insert_patriciatrie(tree, asnode##_fsuffix(new_node)) ; \
   } \
   static inline int  remove##_fsuffix(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/object_t ** removed_node) { \
      int err = remove_patriciatrie(tree, keylength, searchkey, (patriciatrie_node_t**)removed_node) ; \
      if (err == 0) *removed_node = asobject##_fsuffix(*(patriciatrie_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int  removenodes##_fsuffix(patriciatrie_t * tree) { \
      return removenodes_patriciatrie(tree) ; \
   } \
   static inline int  initfirst##_fsuffix##iterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree) { \
      return initfirst_patriciatrieiterator(iter, tree) ; \
   } \
   static inline int  initlast##_fsuffix##iterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree) { \
      return initlast_patriciatrieiterator(iter, tree) ; \
   } \
   static inline int  free##_fsuffix##iterator(patriciatrie_iterator_t * iter) { \
      return free_patriciatrieiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(patriciatrie_iterator_t * iter, object_t ** node) { \
      bool isNext = next_patriciatrieiterator(iter, (patriciatrie_node_t**)node) ;  \
      if (isNext) *node = asobject##_fsuffix(*(patriciatrie_node_t**)node) ;        \
      return isNext ; \
   } \
   static inline bool prev##_fsuffix##iterator(patriciatrie_iterator_t * iter, object_t ** node) { \
      bool isNext = prev_patriciatrieiterator(iter, (patriciatrie_node_t**)node) ;  \
      if (isNext) *node = asobject##_fsuffix(*(patriciatrie_node_t**)node) ;        \
      return isNext ; \
   }


#endif
