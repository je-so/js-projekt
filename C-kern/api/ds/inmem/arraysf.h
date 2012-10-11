/* title: ArraySF
   Array implementation which supports non-continuous index numbers (sparse distribution).
   Once an object is assigned a memory location it is never changed (fixed location).

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

   file: C-kern/api/ds/inmem/arraysf.h
    Header file <ArraySF>.

   file: C-kern/ds/inmem/arraysf.c
    Implementation file <ArraySF impl>.
*/
#ifndef CKERN_DS_INMEM_ARRAYSF_HEADER
#define CKERN_DS_INMEM_ARRAYSF_HEADER

#include "C-kern/api/ds/inmem/node/arraysf_node.h"

// forward
struct binarystack_t ;
struct typeadapt_member_t ;

/* typedef: struct arraysf_t
 * Exports <arraysf_t>. */
typedef struct arraysf_t               arraysf_t ;

/* typedef: struct arraysf_iterator_t
 * Export <arraysf_iterator_t>, iterator type to iterate of contained nodes. */
typedef struct arraysf_iterator_t      arraysf_iterator_t ;

/* enums: arraysf_e
 *
 * arraysf_6BITROOT_UNSORTED  - This root is optimized for values between 0 .. 63. The root is accessed with the
 *                              the lowest 6 bit of the index (pos % 64) or (pos & 0x3f).
 * arraysf_8BITROOT_UNSORTED  - This root is optimized for values between 0 .. 255. The root is accessed with the
 *                      the lowest 8 bit of the index  (pos % 256) or (pos & 0xff).
 * arraysf_MSBPOSROOT - This root is optimized for indizes *pos* which differ in MSBit position.
 *                      The size of the root array is 1 + 3*(bitsize(size_t)/2).
 * arraysf_8BITROOT24 - If MSBit positions are always >= 24 (and < 32) as is the case for IPv4 addresses
 *                      this root distribution must be used. The root is accessed with the
 *                      the highest 8 bit of a 32 bit number. The number should be less or equal to UINT32_MAX.
 *                      If provided numbers are higher than UINT32_MAX an iteration in ascending order is not possible.
 * */
enum arraysf_e {
    arraysf_6BITROOT_UNSORTED
   ,arraysf_8BITROOT_UNSORTED
   ,arraysf_MSBPOSROOT
   ,arraysf_8BITROOT24
} ;

typedef enum arraysf_e                 arraysf_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_arraysf
 * Unittest for <arraysf_t>. */
int unittest_ds_inmem_arraysf(void) ;
#endif


/* struct: arraysf_t
 * Trie implementation to support sparse arrays.
 * This type of array supports non-continuous index numbers (sparse distribution).
 * Once an object is assigned a memory location the address is never changed (fixed location).
 * Every internal trie node contains the bit position (from highest to lowest) of the next
 * 2 bits which are of interest. According to their value the pointer to the next node is
 * chosen from an array of 4 pointers until a leaf node is found where the value is stored.
 *
 * From the root node to the leaf node only such bit positions are compared if there exist
 * at least two stored indizes which have different values at this position.
 * Therefore leaf nodes (user type <arraysf_node_t>) has a depth of less than 16 (index_bitsize/2)
 * on a 32 bit machine or 32 on a 64 bit machine. The mean depth is log2(number of stored indizes)/2.
 *
 * If you choose arraysf_MSBPOSROOT as the type of array the maximum depth for
 * a leaf node is log2(index value)/2 (depth<=4 for index values < 256). */
struct arraysf_t {
   /* variable: length
    * The number of elements stored in this array. */
   size_t                  length ;
   /* variable: type
    * The type of the array, see <arraysf_e>. */
   arraysf_e               type ;
   /* variable: root
    * Points to top level nodes.
    * The size of the root array is determined by the <type> of the array. */
   union arraysf_unode_t   * root[] ;
} ;

// group: lifetime

/* function: new_arraysf
 * Allocates a new array object.
 * Parameter *type* determines the size of the <arraystf_t.root> array
 * and the chosen root distribution. */
int new_arraysf(/*out*/arraysf_t ** array, arraysf_e type) ;

/* function: delete_arraysf
 * Frees allocated memory.
 * If nodeadp is set to 0 no free function is called for contained nodes. */
int delete_arraysf(arraysf_t ** array, struct typeadapt_member_t * nodeadp) ;

// group: foreach-support

/* typedef: iteratortype_arraysf
 * Declaration to associate <arraysf_iterator_t> with <arraysf_t>.
 * The association is done with a typedef which looks like a function. */
typedef arraysf_iterator_t       iteratortype_arraysf ;

/* typedef: iteratedtype_arraysf
 * Function declaration to associate <arraysf_node_t> with <arraysf_t>.
 * The association is done with a typedef which looks like a function. */
typedef struct arraysf_node_t    iteratedtype_arraysf ;

// group: query

/* function: length_arraysf
 * Returns the number of elements stored in the array. */
size_t length_arraysf(arraysf_t * array) ;

/* function: at_arraysf
 * Returns contained node at position *pos*.
 * If no element exists at position *pos* value 0 is returned. */
struct arraysf_node_t * at_arraysf(const arraysf_t * array, size_t pos) ;

// group: change

/* function: insert_arraysf
 * Inserts new node at position *pos* into array.
 * If this position is already occupied by another node EEXIST is returned.
 * If you have set nodeadp to a value != 0 then a copy of node is made before it is inserted.
 * The copied node is returned in *inserted_node*. If nodeadp is 0 and no copy is made inserted_node
 * is set to node. */
int insert_arraysf(arraysf_t * array, struct arraysf_node_t * node, /*out*/struct arraysf_node_t ** inserted_node/*0=>not returned*/, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) ;

/* function: tryinsert_arraysf
 * Same as <insert_arraysf> but no error log in case of EEXIST.
 * If EEXIST is returned nothing was inserted but the existing node will be
 * returned in *inserted_or_existing_node* nevertheless. */
int tryinsert_arraysf(arraysf_t * array, struct arraysf_node_t * node, /*out;err*/struct arraysf_node_t ** inserted_or_existing_node, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) ;

/* function: remove_arraysf
 * Removes node at position *pos*.
 * If no node exists at position *pos* ESRCH is returned.
 * In case of success the removed node is returned in *removed_node*. */
int remove_arraysf(arraysf_t * array, size_t pos, /*out*/struct arraysf_node_t ** removed_node) ;

/* function: tryremove_arraysf
 * Same as <remove_arraysf> but no error log in case of ESRCH. */
int tryremove_arraysf(arraysf_t * array, size_t pos, /*out*/struct arraysf_node_t ** removed_node) ;

// group: generic

/* define: arraysf_IMPLEMENT
 * Generates the interface for a specific single linked list.
 * The type of the list object must be declared with help of <slist_DECLARE>
 * before this macro. It is also possible to construct "listtype_t" in another way before
 * calling this macro. In the latter case "listtype_t" must have a pointer to the object
 * declared as its first field with the name *last*.
 *
 *
 * Parameter:
 * _fsuffix - It is the suffix of the generated container interface functions which wraps all calls to <arraysf_t>.
 * object_t - The object which is stored in this container derived from generic <arraysf_t>.
 * nodename - The member name (access path) in object_t corresponding to the
 *            member name of the embedded <arraysf_node_t>.
 *            Or the same value as the first parameter used in <arraysf_node_EMBED> to embed the node.
 * */
void arraysf_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, IDNAME nodename) ;


/* struct: arraysf_iterator_t
 * Iterates over elements contained in <arraysf_t>. */
struct arraysf_iterator_t {
   /* variable: stack
    * Remembers last position in tree. */
   struct binarystack_t * stack ;
   /* variable: ri
    * Index into <arraysf_t.root>. */
   unsigned             ri ;
} ;

// group: lifetime

/* define: arraysf_iterator_INIT_FREEABLE
 * Static initializer. */
#define arraysf_iterator_INIT_FREEABLE   { 0, 0 }

/* function: initfirst_arraysfiterator
 * Initializes an iterator for <arraysf_t>. */
int initfirst_arraysfiterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array) ;

/* function: free_arraysfiterator
 * Frees an iterator for <arraysf_t>. */
int free_arraysfiterator(arraysf_iterator_t * iter) ;

// group: iterate

/* function: next_arraysfiterator
 * Returns next iterated node.
 * The first call after <initfirst_arraysfiterator> returns the first array element
 * if it is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the array is empty. */
bool next_arraysfiterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/struct arraysf_node_t ** node) ;


// section: inline implementation

/* define: length_arraysf
 * Implements <arraysf_t.length_arraysf>. */
#define length_arraysf(array)          ((array)->length)

/* define: arraysf_IMPLEMENT
 * Implements <arraysf_t.arraysf_IMPLEMENT>. */
#define arraysf_IMPLEMENT(_fsuffix, object_t, nodename)     \
   typedef arraysf_iterator_t    iteratortype##_fsuffix ;   \
   typedef object_t              iteratedtype##_fsuffix ;   \
   static inline int  new##_fsuffix(/*out*/arraysf_t ** array, arraysf_e type) __attribute__ ((always_inline)) ; \
   static inline int  delete##_fsuffix(arraysf_t ** array, struct typeadapt_member_t * nodeadp) __attribute__ ((always_inline)) ; \
   static inline size_t length##_fsuffix(arraysf_t * array) __attribute__ ((always_inline)) ; \
   static inline object_t * at##_fsuffix(const arraysf_t * array, size_t pos) __attribute__ ((always_inline)) ; \
   static inline int  insert##_fsuffix(arraysf_t * array, object_t * node, /*out*/object_t ** inserted_node/*0=>not returned*/, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) __attribute__ ((always_inline)) ; \
   static inline int  tryinsert##_fsuffix(arraysf_t * array, object_t * node, /*out;err*/object_t ** inserted_or_existing_node, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) __attribute__ ((always_inline)) ; \
   static inline int  remove##_fsuffix(arraysf_t * array, size_t pos, /*out*/object_t ** removed_node) __attribute__ ((always_inline)) ; \
   static inline int  tryremove##_fsuffix(arraysf_t * array, size_t pos, /*out*/object_t ** removed_node) __attribute__ ((always_inline)) ; \
   static inline int  initfirst##_fsuffix##iterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix##iterator(arraysf_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/object_t ** node) __attribute__ ((always_inline)) ; \
   static inline arraysf_node_t * asnode##_fsuffix(object_t * object) { \
      static_assert(&((object_t*)0)->nodename == (void*)offsetof(object_t, nodename), "correct type") ; \
      return (arraysf_node_t*) ((uintptr_t)object + offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobject##_fsuffix(arraysf_node_t * node) { \
      return (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) ; \
   } \
   static inline object_t * asobjectnull##_fsuffix(arraysf_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - offsetof(object_t, nodename)) : 0 ; \
   } \
   static inline int new##_fsuffix(/*out*/arraysf_t ** array, arraysf_e type) { \
      return new_arraysf(array, type) ; \
   } \
   static inline int delete##_fsuffix(arraysf_t ** array, struct typeadapt_member_t * nodeadp) { \
      return delete_arraysf(array, nodeadp) ; \
   } \
   static inline size_t length##_fsuffix(arraysf_t * array) { \
      return length_arraysf(array) ; \
   } \
   static inline object_t * at##_fsuffix(const arraysf_t * array, size_t pos) { \
      arraysf_node_t * node = at_arraysf(array, pos) ; \
      return asobjectnull##_fsuffix(node) ; \
   } \
   static inline int insert##_fsuffix(arraysf_t * array, object_t * node, /*out*/object_t ** inserted_node/*0=>not returned*/, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) { \
      int err = insert_arraysf(array, asnode##_fsuffix(node), (struct arraysf_node_t**)inserted_node, nodeadp) ; \
      if (!err && inserted_node) *inserted_node = asobject##_fsuffix(*(struct arraysf_node_t**)inserted_node) ; \
      return err ; \
   } \
   static inline int tryinsert##_fsuffix(arraysf_t * array, object_t * node, /*out;err*/object_t ** inserted_or_existing_node, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/) { \
      int err = tryinsert_arraysf(array, asnode##_fsuffix(node), (struct arraysf_node_t**)inserted_or_existing_node, nodeadp) ; \
      *inserted_or_existing_node = asobjectnull##_fsuffix(*(struct arraysf_node_t**)inserted_or_existing_node) ; \
      return err ; \
   } \
   static inline int remove##_fsuffix(arraysf_t * array, size_t pos, /*out*/object_t ** removed_node) { \
      int err = remove_arraysf(array, pos, (struct arraysf_node_t**)removed_node) ; \
      if (!err) *removed_node = asobject##_fsuffix(*(struct arraysf_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int tryremove##_fsuffix(arraysf_t * array, size_t pos, /*out*/object_t ** removed_node) { \
      int err = tryremove_arraysf(array, pos, (struct arraysf_node_t**)removed_node) ; \
      if (!err) *removed_node = asobject##_fsuffix(*(struct arraysf_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int initfirst##_fsuffix##iterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array) { \
      return initfirst_arraysfiterator(iter, array) ; \
   } \
   static inline int free##_fsuffix##iterator(arraysf_iterator_t * iter) { \
      return free_arraysfiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/object_t ** node) { \
      bool isNext = next_arraysfiterator(iter, array, (struct arraysf_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(struct arraysf_node_t**)node) ; \
      return isNext ; \
   }

#endif
