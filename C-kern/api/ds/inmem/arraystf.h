/* title: ArraySTF

   Array implementation which supports strings as indizes (st).
   Once an object is assigned a memory location it is never changed (fixed location).
   See also <http://en.wikipedia.org/wiki/Radix_tree>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/ds/inmem/arraystf.h
    Header file <ArraySTF>.

   file: C-kern/ds/inmem/arraystf.c
    Implementation file <ArraySTF impl>.
*/
#ifndef CKERN_DS_INMEM_ARRAYSTF_HEADER
#define CKERN_DS_INMEM_ARRAYSTF_HEADER

#include "C-kern/api/ds/inmem/node/arraystf_node.h"

// forward
struct binarystack_t ;
struct typeadapt_member_t ;

/* typedef: struct arraystf_t
 * Export <arraystf_t>. */
typedef struct arraystf_t              arraystf_t ;

/* typedef: struct arraystf_iterator_t
 * Export <arraystf_iterator_t>, iterator type to iterate of contained nodes. */
typedef struct arraystf_iterator_t     arraystf_iterator_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_arraystf
 * Unittest for <arraystf_t>. */
int unittest_ds_inmem_arraystf(void) ;
#endif


/* struct: arraystf_t
 * Trie implementation to support arrays index with string keys.
 * This type of array supports strings as indizes with arbitrary length.
 *
 * Once an object is assigned a memory location the address is never changed (fixed location).
 * Every internal trie node contains a memory offset and a bit position (from highest to lowest).
 * If a key is searched for and an internal node is encountered the byte value at the memory offset
 * in the key is taken and the value of the two bits are extracted.
 * The resuslting number (0..3) is taken as index into the child array of the internal
 * node <arraystf_mwaybranch_t>.
 *
 * From the root node to the leaf node only such offset/bit combinations are compared if there exist
 * at least two stored keys which have different values at this position.
 * Therefore leaf nodes (user type <generic_object_t>) have always a depth of less than (key length in bits)/2
 * The mean depth is log2(number of stored keys)/2.
 *
 * Special Encoding:
 * Cause keys of different length make problems in a patricia trie implementation if a smaller key is
 * a prefix of a longer one.
 *
 * A C-string has a null byte at its end so no stored string can be a prefix of another longer one.
 * To allows keys of any binary content an encoding of is chosen so that every key has the same length.
 * The encoding is as follows (keylength: key->size; keycontent: key->addr[0 .. key->size-1])
 *
 * - Every key is of length SIZE_MAX (UINT32_MAX or UINT64_MAX)
 * - The byte values at offset 0 .. key->size-1 are taken from key->addr
 * - The byte values at offset key->size .. SIZE_MAX-1 are always 0
 * - The value at offset SIZE_MAX is key->size and has bitsof(size_t) bits instead of only 8 */
struct arraystf_t {
   /* variable: length
    * The number of elements stored in this array. */
   size_t                  length ;
   /* variable: toplevelsize
    * The size of array <root>. */
   uint32_t                toplevelsize:24 ;
   /* variable: rootidxshift
    * Nr of bits to shift right before root key is used to access <root>. */
   uint32_t                rootidxshift:8 ;
   /* variable: root
    * Points to top level nodes.
    * The size of the root array is determined by <toplevelsize>. */
   union arraystf_unode_t  * root[/*toplevelsize*/] ;
} ;

// group: lifetime

/* function: new_arraystf
 * Allocates a new array object.
 * Parameter *toplevelsize* determines the number of childs of root node. */
int new_arraystf(/*out*/arraystf_t ** array, uint32_t toplevelsize) ;

/* function: delete_arraystf
 * Frees allocated memory.
 * If nodeadp is set to 0 no free function is called for contained nodes. */
int delete_arraystf(arraystf_t ** array, struct typeadapt_member_t * nodeadp) ;

// group: foreach-support

/* typedef: iteratortype_arraystf
 * Declaration to associate <arraystf_iterator_t> with <arraystf_t>.
 * The association is done with a typedef which looks like a function. */
typedef arraystf_iterator_t      iteratortype_arraystf ;

/* typedef: iteratedtype_arraystf
 * Function declaration to associate <arraystf_node_t> with <arraystf_t>.
 * The association is done with a typedef which looks like a function. */
typedef struct arraystf_node_t * iteratedtype_arraystf ;

// group: query

/* function: length_arraystf
 * Returns the number of elements stored in the array. */
size_t length_arraystf(arraystf_t * array) ;

/* function: at_arraystf
 * Returns node with same key as *keydata*[*size*].
 * If no element exists with this key value 0 is returned. */
struct arraystf_node_t * at_arraystf(const arraystf_t * array, size_t size, const uint8_t keydata[size]) ;

// group: change

/* function: insert_arraystf
 * Inserts new node with string key into array.
 * If the provided key is already stored EEXIST is returned.
 * If you have set nodeadp to a value != 0 then a copy of node is made before it is inserted.
 * The copied node is returned in *inserted_node*. If nodeadp is 0 and no copy is made inserted_node
 * is set to node. */
int insert_arraystf(arraystf_t * array, struct arraystf_node_t * node, /*out*/struct arraystf_node_t ** inserted_node/*0=>copy not returned*/, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) ;

/* function: tryinsert_arraystf
 * Same as <insert_arraystf> but no error log in case of EEXIST.
 * If EEXIST is returned nothing was inserted but the existing node will be
 * returned in *inserted_or_existing_node* nevertheless. */
int tryinsert_arraystf(arraystf_t * array, struct arraystf_node_t * node, /*out;err*/struct arraystf_node_t ** inserted_or_existing_node, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) ;

/* function: remove_arraystf
 * Removes node with key *keydata*.
 * If no node exists with this key ESRCH is returned.
 * In case of success the removed node is returned in *removed_node*. */
int remove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct arraystf_node_t ** removed_node) ;

/* function: tryremove_arraystf
 * Same as <remove_arraystf> but no error log in case of ESRCH. */
int tryremove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct arraystf_node_t ** removed_node) ;

// group: generic

/* define: arraystf_IMPLEMENT
 * Adapts interface of <arraystf_t> to object type object_t.
 * All generated functions are the same as for <arraystf_t> except the type <arraystf_node_t> is replaced with object_t.
 * The conversion from object_t to arraystf_node_t and vice versa is done before _arraystf functions are called and after
 * return the out parameters are converted.
 *
 * Parameter:
 * _fsuffix - It is the suffix of the generated container interface functions which wraps all calls to <arraystf_t>.
 * object_t - The object which is stored in this container derived from generic <arraystf_t>.
 * nodename - The member name (access path) in object_t corresponding to the
 *            member name of the embedded <arraystf_node_t>.
 *            Or the same value as the first parameter used in <arraystf_node_EMBED> to embed the node.
 * */
void arraystf_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, IDNAME nodename) ;


/* struct: arraystf_iterator_t
 * Iterates over elements contained in <arraystf_t>. */
struct arraystf_iterator_t {
   /* variable: stack
    * Remembers last position in tree. */
   struct binarystack_t *  stack ;
   /* variable: array
    * Remembers iterated container. */
   arraystf_t           *  array ;
   /* variable: ri
    * Index into <arraystf_t.root>. */
   unsigned                ri ;
} ;

// group: lifetime

/* define: arraystf_iterator_FREE
 * Static initializer. */
#define arraystf_iterator_FREE { 0, 0, 0 }

/* function: initfirst_arraystfiterator
 * Initializes an iterator for an <arraystf_t>. */
int initfirst_arraystfiterator(/*out*/arraystf_iterator_t * iter, arraystf_t * array) ;

/* function: free_arraystfiterator
 * Frees an iterator for an <arraystf_t>. */
int free_arraystfiterator(arraystf_iterator_t * iter) ;

// group: iterate

/* function: next_arraystfiterator
 * Returns next iterated node.
 * The first call after <initfirst_arraystfiterator> returns the first array element
 * if it is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the array is empty. */
bool next_arraystfiterator(arraystf_iterator_t * iter, /*out*/struct arraystf_node_t ** node) ;


// section: inline implementation

/* define: length_arraystf
 * Implements <arraystf_t.length_arraystf>. */
#define length_arraystf(array)         ((array)->length)

/* define: arraystf_IMPLEMENT
 * Implements <arraystf_t.arraystf_IMPLEMENT>. */
#define arraystf_IMPLEMENT(_fsuffix, object_t, nodename)    \
   typedef arraystf_iterator_t   iteratortype##_fsuffix ;   \
   typedef object_t           *  iteratedtype##_fsuffix ;   \
   static inline int  new##_fsuffix(/*out*/arraystf_t ** array, uint32_t toplevelsize) __attribute__ ((always_inline)) ; \
   static inline int  delete##_fsuffix(arraystf_t ** array, struct typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline size_t length##_fsuffix(arraystf_t * array) __attribute__ ((always_inline)) ; \
   static inline object_t * at##_fsuffix(const arraystf_t * array, size_t size, const uint8_t keydata[size]) __attribute__ ((always_inline)) ; \
   static inline int  insert##_fsuffix(arraystf_t * array, object_t * node, /*out*/object_t ** inserted_node/*0=>copy not returned*/, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) __attribute__ ((always_inline)) ; \
   static inline int  tryinsert##_fsuffix(arraystf_t * array, object_t * node, /*out;err*/object_t ** inserted_or_existing_node, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) __attribute__ ((always_inline)) ; \
   static inline int  remove##_fsuffix(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/object_t ** removed_node) __attribute__ ((always_inline)) ; \
   static inline int  tryremove##_fsuffix(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/object_t ** removed_node) __attribute__ ((always_inline)) ; \
   static inline int  initfirst##_fsuffix##iterator(/*out*/arraystf_iterator_t * iter, arraystf_t * array) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix##iterator(arraystf_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(arraystf_iterator_t * iter, /*out*/object_t ** node) __attribute__ ((always_inline)) ; \
   static inline arraystf_node_t * asnode##_fsuffix(object_t * object) { \
      static_assert(&((object_t*)0)->nodename == (void*)offsetof(object_t, nodename), "correct type") ; \
      return (arraystf_node_t*) ((uintptr_t)object + offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobject##_fsuffix(arraystf_node_t * node) { \
      return (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobjectnull##_fsuffix(arraystf_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) : 0 ; \
   } \
   static inline int new##_fsuffix(/*out*/arraystf_t ** array, uint32_t toplevelsize) { \
      return new_arraystf(array, toplevelsize) ; \
   } \
   static inline int delete##_fsuffix(arraystf_t ** array, struct typeadapt_member_t * nodeadp) { \
      return delete_arraystf(array, nodeadp) ; \
   } \
   static inline size_t length##_fsuffix(arraystf_t * array) { \
      return length_arraystf(array) ; \
   } \
   static inline object_t * at##_fsuffix(const arraystf_t * array, size_t size, const uint8_t keydata[size]) { \
      arraystf_node_t * node = at_arraystf(array, size, keydata) ; \
      return asobjectnull##_fsuffix(node) ; \
   } \
   static inline int insert##_fsuffix(arraystf_t * array, object_t * node, /*out*/object_t ** inserted_node/*0=>copy not returned*/, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) { \
      int err = insert_arraystf(array, asnode##_fsuffix(node), (struct arraystf_node_t**)inserted_node, nodeadp) ; \
      if (!err && inserted_node) *inserted_node = asobject##_fsuffix(*(struct arraystf_node_t**)inserted_node) ; \
      return err ; \
   } \
   static inline int tryinsert##_fsuffix(arraystf_t * array, object_t * node, /*out;err*/object_t ** inserted_or_existing_node, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) { \
      int err = tryinsert_arraystf(array, asnode##_fsuffix(node), (struct arraystf_node_t**)inserted_or_existing_node, nodeadp) ; \
      *inserted_or_existing_node = asobjectnull##_fsuffix(*(struct arraystf_node_t**)inserted_or_existing_node) ; \
      return err ; \
   } \
   static inline int remove##_fsuffix(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/object_t ** removed_node) { \
      int err = remove_arraystf(array, size, keydata, (struct arraystf_node_t**)removed_node) ; \
      if (!err) *removed_node = asobject##_fsuffix(*(struct arraystf_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int tryremove##_fsuffix(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/object_t ** removed_node) { \
      int err = tryremove_arraystf(array, size, keydata, (struct arraystf_node_t**)removed_node) ; \
      if (!err) *removed_node = asobject##_fsuffix(*(struct arraystf_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int initfirst##_fsuffix##iterator(/*out*/arraystf_iterator_t * iter, arraystf_t * array) { \
      return initfirst_arraystfiterator(iter, array) ; \
   } \
   static inline int free##_fsuffix##iterator(arraystf_iterator_t * iter) { \
      return free_arraystfiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(arraystf_iterator_t * iter, /*out*/object_t ** node) { \
      bool isNext = next_arraystfiterator(iter, (struct arraystf_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(struct arraystf_node_t**)node) ; \
      return isNext ; \
   }

#endif
