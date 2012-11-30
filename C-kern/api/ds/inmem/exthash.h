/* title: Extendible-Hashing

   Offers a container which organizes stored nodes as a hash table.

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

   file: C-kern/api/ds/inmem/exthash.h
    Header file <Extendible-Hashing>.

   file: C-kern/ds/inmem/exthash.c
    Implementation file <Extendible-Hashing impl>.
*/
#ifndef CKERN_DS_INMEM_EXTHASH_HEADER
#define CKERN_DS_INMEM_EXTHASH_HEADER

#include "C-kern/api/ds/inmem/node/lrptree_node.h"

/* typedef: struct exthash_t
 * Export <exthash_t> into global namespace. */
typedef struct exthash_t               exthash_t ;

/* typedef: struct exthash_iterator_t
 * Export <exthash_iterator_t> into global namespace. */
typedef struct exthash_iterator_t      exthash_iterator_t ;

/* typedef: struct exthash_node_t
 * Rename <lrptree_node_t> into <exthash_node_t>. */
typedef struct lrptree_node_t          exthash_node_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_exthash
 * Test <exthash_t> functionality. */
int unittest_ds_inmem_exthash(void) ;
#endif


/* struct: exthash_iterator_t
 * Iterates over elements contained in <exthash_t>.
 * The iterator supports removing or deleting of the current node.
 * > exthash_t htable ;
 * > fill_tree(&htable) ;
 * > foreach (_exthash, &htable, node) {
 * >    if (need_to_remove(node)) {
 * >       err = remove_exthash(&htable, node)) ;
 * >    }
 * > }
 * */
struct exthash_iterator_t {
   exthash_node_t * next ;
   size_t         tableindex ;
} ;

// group: lifetime

/* define: exthash_iterator_INIT_FREEABLE
 * Static initializer. */
#define exthash_iterator_INIT_FREEABLE       { 0, 0 }

/* function: initfirst_exthashiterator
 * Initializes an iterator for <exthash_t>. */
int initfirst_exthashiterator(/*out*/exthash_iterator_t * iter, exthash_t * htable) ;

/* function: free_exthashiterator
 * Frees an iterator of <exthash_t>. */
int free_exthashiterator(exthash_iterator_t * iter) ;

// group: iterate

/* function: next_exthashiterator
 * Returns next node of htable not sorted in any order.
 * The first call after <initfirst_exthashiterator> returns the node with the lowest key.
 * In case no next node exists false is returned and parameter node is not changed. */
bool next_exthashiterator(exthash_iterator_t * iter, exthash_t * htable, /*out*/exthash_node_t ** node) ;


// struct: exthash_node_t

// group: lifetime

/* define: exthash_node_INIT
 * Static initializer. */
#define exthash_node_INIT              lrptree_node_INIT


/* struct: exthash_t
 * Implements a hash table which doubles in size if needed.
 * The table never shrinks.
 * A hash value is an unsigned integer (32 or 64 bit) which is computed from a key.
 * Its value modulo the hash table size is used as index into the hash table.
 * If the hash values are evenly distributed the access time is O(1).
 * In case all keys hash to the same value the worst case has time O(log n).
 *
 * Internally nodes are stored in a table which contains only one pointer.
 * The pointer is the root of a tree and all nodes stored in the same bucket
 * are organized in a tree structure.
 * The memory for nodes is allocated by the user of the container.
 *
 * To reduce the time needed for doubling the hash table the newly created table entries
 * share its content with the old entries. See also <Algorithm> in <Extendible-Hashing impl>.
 *
 * typeadapt_t:
 * The service delete_object of <typeadapt_t.lifetime> is used in <free_exthash> and <removenodes_exthash>.
 * The service cmp_key_object of <typeadapt_t.comparator> is used in <find_exthash>.
 * The service cmp_object of <typeadapt_t.comparator> is used in <invariant_exthash>, <insert_exthash>, and <remove_exthash>.
 * The service hashobject of <typeadapt_t.gethash> is used in <insert_exthash> and <remove_exthash>.
 * The service hashkey of <typeadapt_t.gethash> is used in <find_exthash>.
 * */
struct exthash_t {
   /* variable: hashtable
    * Pointer to table of size pow(2,level). See also <level>.
    * The directory contains a pointer to the stored node and no buckets.
    * The nodes are organized as a tree and the hashtable entry points to the root of the tree.
    * If a pointer is set to 0 the tree is empty. If it set to the value (intptr_t)-1
    * it shares the same tree as the entry of the next smaller level. */
   exthash_node_t       ** hashtable ;
   /* variable: nr_nodes
    * The number of stored nodes in the hash table. */
   size_t               nr_nodes ;
   /* variable: nodeadp
    * Offers lifetime + keycomparator + gethash services to handle stored nodes. */
   typeadapt_member_t   nodeadp ;
   /* variable: level
    * Determines the hash table size as pow(2,level). */
   uint8_t              level ;
   /* variable: maxlevel
    * Determines the max size of the hash table.
    * Once the <level> value has reached <maxlevel> the table does no more grow. */
   uint8_t              maxlevel ;
} ;

// group: lifetime

/* define: exthash_INIT_FREEABLE
 * Static initializer. Makes calling <free_exthash> safe. */
#define exthash_INIT_FREEABLE          { 0, 0, typeadapt_member_INIT_FREEABLE, 0, 0}

/* function: init_exthash
 * Allocates a hash table of at least size 1.
 * The parameter initial_size and max_size should be a power of two.
 * If not the next smaller power of two is chosen. */
int init_exthash(/*out*/exthash_t * htable, size_t initial_size, size_t max_size, const typeadapt_member_t * nodeadp) ;

/* function: free_exthash
 * Calls <removenodes_exthash> and frees the hash table memory. */
int free_exthash(exthash_t * htable) ;

// group: query

/* function: isempty_exthash
 * Returns true if the table contains no element. */
bool isempty_exthash(const exthash_t * htable) ;

/* function: nrelements_exthash
 * Returns the nr of elements stored in the hash table. */
size_t nrelements_exthash(const exthash_t * htable) ;

// group: foreach-support

/* typedef: iteratortype_exthash
 * Declaration to associate <exthash_iterator_t> with <exthash_t>. */
typedef exthash_iterator_t      iteratortype_exthash ;

/* typedef: iteratedtype_exthash
 * Declaration to associate <exthash_node_t> with <exthash_t>. */
typedef exthash_node_t          iteratedtype_exthash ;

// group: search

/* function: find_exthash
 * Searches for a node with equal key.
 * If it exists it is returned in found_node else ESRCH is returned. */
int find_exthash(exthash_t * htable, const void * key, /*out*/exthash_node_t ** found_node) ;

// group: change

/* function: insert_exthash
 * Inserts a new node into the hash table if it is unique.
 * If another node exists with the same key as *new_node* nothing is inserted and the function returns EEXIST.
 * The caller has to allocate the new node and has to transfer ownership. */
int insert_exthash(exthash_t * htable, exthash_node_t * new_node) ;

/* function: remove_exthash
 * Removes a node from the hash table. If the node is not part of the table the behaviour is undefined !
 * The ownership of the removed node is transfered back to the caller. */
int remove_exthash(exthash_t * htable, exthash_node_t * node) ;

/* function: removenodes_exthash
 * Removes all nodes from the hash table.
 * For every removed node <typeadapt_lifetime_it.delete_object> is called. */
int removenodes_exthash(exthash_t * htable) ;

// group: generic

/* define: exthash_IMPLEMENT
 * Adapts interface of <exthash_t> to nodes of type object_t.
 * The generated wrapper functions has the suffix _fsuffix which is provided as first parameter.
 * They are defined as static inline.
 *
 * Parameter:
 * _fsuffix  - The suffix name of all generated tree interface functions, e.g. "init##_fsuffix".
 * object_t  - The type of object which can be stored and retrieved from this table.
 *             The object must contain a field of type <exthash_node_t>.
 * key_t     - The type of key the objects are sorted by.
 * nodename  - The access path of the field <exthash_node_t> in type object_t. */
void exthash_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, TYPENAME key_t, IDNAME nodename) ;

// group: test

/* function: invariant_exthash
 * Checks that every bucket points to a correct red black tree. */
int invariant_exthash(const exthash_t * htable) ;


// section: inline implementation

/* define: free_exthashiterator
 * Implements <exthash_iterator_t.free_exthashiterator>. */
#define free_exthashiterator(iter)     ((iter)->next = 0, 0)

/* define: isempty_exthash
 * Implements <exthash_t.isempty_exthash>. */
#define isempty_exthash(htable)        (0 == ((htable)->nr_nodes))

/* define: nrelements_exthash
 * Implements <exthash_t.nrelements_exthash>. */
#define nrelements_exthash(htable)     ((htable)->nr_nodes)

/* define: exthash_IMPLEMENT
 * Implements <exthash_t.exthash_IMPLEMENT>. */
#define exthash_IMPLEMENT(_fsuffix, object_t, key_t, nodename)  \
   typedef exthash_iterator_t  iteratortype##_fsuffix ;         \
   typedef object_t            iteratedtype##_fsuffix ;         \
   static inline int  initfirst##_fsuffix##iterator(exthash_iterator_t * iter, exthash_t * htable) __attribute__ ((always_inline)) ;   \
   static inline int  free##_fsuffix##iterator(exthash_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(exthash_iterator_t * iter, exthash_t * htable, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline int  init##_fsuffix(/*out*/exthash_t * htable, size_t initial_size, size_t max_size, const typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix(exthash_t * htable) __attribute__ ((always_inline)) ; \
   static inline bool isempty##_fsuffix(const exthash_t * htable) __attribute__ ((always_inline)) ; \
   static inline size_t nrelements##_fsuffix(const exthash_t * htable) __attribute__ ((always_inline)) ; \
   static inline int  find##_fsuffix(exthash_t * htable, const key_t key, /*out*/object_t ** found_node) __attribute__ ((always_inline)) ; \
   static inline int  insert##_fsuffix(exthash_t * htable, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline int  remove##_fsuffix(exthash_t * htable, object_t * node) __attribute__ ((always_inline)) ; \
   static inline int  removenodes##_fsuffix(exthash_t * htable) __attribute__ ((always_inline)) ; \
   static inline int  invariant##_fsuffix(exthash_t * htable) __attribute__ ((always_inline)) ; \
   static inline exthash_node_t * asnode##_fsuffix(object_t * object) { \
      static_assert(&((object_t*)0)->nodename == (exthash_node_t*)offsetof(object_t, nodename), "correct type") ; \
      return (exthash_node_t *) ((uintptr_t)object + offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobject##_fsuffix(exthash_node_t * node) { \
      return (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) ; \
   } \
   static inline int init##_fsuffix(/*out*/exthash_t * htable, size_t initial_size, size_t max_size, const typeadapt_member_t * nodeadp) { \
      return init_exthash(htable, initial_size, max_size, nodeadp) ; \
   } \
   static inline int  free##_fsuffix(exthash_t * htable) { \
      return free_exthash(htable) ; \
   } \
   static inline bool isempty##_fsuffix(const exthash_t * htable) { \
      return isempty_exthash(htable) ; \
   } \
   static inline size_t nrelements##_fsuffix(const exthash_t * htable) { \
      return nrelements_exthash(htable) ; \
   } \
   static inline int  find##_fsuffix(exthash_t * htable, const key_t key, /*out*/object_t ** found_node) { \
      int err = find_exthash(htable, (void*)key, (exthash_node_t**)found_node) ; \
      if (err == 0) *found_node = asobject##_fsuffix(*(exthash_node_t**)found_node) ; \
      return err ; \
   } \
   static inline int  insert##_fsuffix(exthash_t * htable, object_t * new_node) { \
      return insert_exthash(htable, asnode##_fsuffix(new_node)) ; \
   } \
   static inline int  remove##_fsuffix(exthash_t * htable, object_t * node) { \
      int err = remove_exthash(htable, asnode##_fsuffix(node)) ; \
      return err ; \
   } \
   static inline int  removenodes##_fsuffix(exthash_t * htable) { \
      return removenodes_exthash(htable) ; \
   } \
   static inline int  invariant##_fsuffix(exthash_t * htable) { \
      return invariant_exthash(htable) ; \
   } \
   static inline int  initfirst##_fsuffix##iterator(exthash_iterator_t * iter, exthash_t * htable) { \
      return initfirst_exthashiterator(iter, htable) ; \
   } \
   static inline int  free##_fsuffix##iterator(exthash_iterator_t * iter) { \
      return free_exthashiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(exthash_iterator_t * iter, exthash_t * htable, object_t ** node) { \
      bool isNext = next_exthashiterator(iter, htable, (exthash_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(exthash_node_t**)node) ; \
      return isNext ; \
   }

#endif
