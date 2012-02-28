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

// forward
struct generic_object_t ;
union arraysf_unode_t ;
struct binarystack_t ;
struct typeadapter_oit ;

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
 * Therefore leaf nodes (user type <generic_object_t>) has a depth of less than 16 (index_bitsize/2)
 * on a 32 bit machine or 32 on a 64 bit machine. The mean depth is log2(number of stored indizes)/2.
 *
 * If you choose arraysf_MSBPOSROOT as the type of array the maximum depth for
 * a leaf node is log2(index value)/2 (depth<=4 for index values < 256).
 *
 */
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
 * If typeadt is set to 0 no free function is called for contained nodes. */
int delete_arraysf(arraysf_t ** array, struct typeadapter_oit * typeadp) ;

// group: iterate

/* function: iteratortype_arraysf
 * Function declaration associates <arraysf_iterator_t> with <arraysf_t>.
 * The association is done with the type of the return value - there is no implementation. */
arraysf_iterator_t * iteratortype_arraysf(void) ;

/* function: iteratedtype_arraysf
 * Function declaration associates (<generic_object_t>*) with <arraysf_t>.
 * The association is done with the type of the return value - there is no implementation. */
struct generic_object_t * iteratedtype_arraysf(void) ;

// group: query

/* function: length_arraysf
 * Returns the number of elements stored in the array. */
size_t length_arraysf(arraysf_t * array) ;

/* function: at_arraysf
 * Returns contained node at position *pos*.
 * If no element exists at position *pos* value 0 is returned. */
struct generic_object_t * at_arraysf(const arraysf_t * array, size_t pos, size_t offset_node) ;

// group: change

/* function: insert_arraysf
 * Inserts new node at position *pos* into array.
 * If this position is already occupied by another node EEXIST is returned.
 * If you have set typeadt to a value != 0 then a copy of node is made before it is inserted.
 * The copied node is returned in *inserted_node*. If typeadt is 0 and no copy is made inserted_node
 * is set to node. */
int insert_arraysf(arraysf_t * array, struct generic_object_t * node, /*out*/struct generic_object_t ** inserted_node/*0=>not returned*/, struct typeadapter_oit * typeadp/*0=>no copy is made*/, size_t offset_node) ;

/* function: tryinsert_arraysf
 * Same as <insert_arraysf> but no error log in case of EEXIST.
 * If EEXIST is returned nothing was inserted but the existing node will be
 * returned in *inserted_or_existing_node* nevertheless. */
int tryinsert_arraysf(arraysf_t * array, struct generic_object_t * node, /*out;err*/struct generic_object_t ** inserted_or_existing_node, struct typeadapter_oit * typeadp/*0=>no copy is made*/, size_t offset_node) ;

/* function: remove_arraysf
 * Removes node at position *pos*.
 * If no node exists at position *pos* ESRCH is returned.
 * In case of success the removed node is returned in *removed_node*. */
int remove_arraysf(arraysf_t * array, size_t pos, /*out*/struct generic_object_t ** removed_node/*could be 0*/, size_t offset_node) ;

/* function: tryremove_arraysf
 * Same as <remove_arraysf> but no error log in case of ESRCH. */
int tryremove_arraysf(arraysf_t * array, size_t pos, /*out*/struct generic_object_t ** removed_node/*could be 0*/, size_t offset_node) ;

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
 * objecttype_t -
 * _fctsuffix   - It is the suffix of the generated <arraysf_t> interface functions.
 * name_pos     - The member name (access path) in objecttype_t corresponding to the
 *                name of the first embedded member of <arraysf_node_t> - see <arraysf_node_EMBED>.
 * */
#define arraysf_IMPLEMENT(objecttype_t, _fctsuffix, name_pos)  \
   /*declare helper functions as always inline*/               \
   static inline size_t offsetnode##_fctsuffix(void) __attribute__ ((always_inline)) ; \
   /*declare function signatures as always inline*/            \
   static inline int new##_fctsuffix(/*out*/arraysf_t ** array, arraysf_e type) __attribute__ ((always_inline)) ; \
   static inline int delete##_fctsuffix(arraysf_t ** array, struct typeadapter_oit * typeadp) __attribute__ ((always_inline)) ; \
                 arraysf_iterator_t * iteratortype##_fctsuffix(void) ; \
                 objecttype_t       * iteratedtype##_fctsuffix(void) ; \
   static inline size_t length##_fctsuffix(arraysf_t * array) __attribute__ ((always_inline)) ; \
   static inline objecttype_t * at##_fctsuffix(const arraysf_t * array, size_t pos) __attribute__ ((always_inline)) ; \
   static inline int insert##_fctsuffix(arraysf_t * array, objecttype_t * node, /*out*/objecttype_t ** inserted_node/*0=>not returned*/, struct typeadapter_oit * typeadp/*0=>no copy is made*/) __attribute__ ((always_inline)) ; \
   static inline int tryinsert##_fctsuffix(arraysf_t * array, objecttype_t * node, /*out;err*/objecttype_t ** inserted_or_existing_node, struct typeadapter_oit * typeadp/*0=>no copy is made*/) __attribute__ ((always_inline)) ; \
   static inline int remove##_fctsuffix(arraysf_t * array, size_t pos, /*out*/objecttype_t ** removed_node/*could be 0*/) __attribute__ ((always_inline)) ; \
   static inline int tryremove##_fctsuffix(arraysf_t * array, size_t pos, /*out*/objecttype_t ** removed_node/*could be 0*/) __attribute__ ((always_inline)) ; \
   static inline int init##_fctsuffix##iterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array) __attribute__ ((always_inline)) ; \
   static inline int free##_fctsuffix##iterator(arraysf_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fctsuffix##iterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/objecttype_t ** node) __attribute__ ((always_inline)) ; \
   /*implement helper functions */ \
   static inline size_t offsetnode##_fctsuffix(void) { \
      static_assert( ((size_t *) (offsetof(objecttype_t,name_pos))) == &((objecttype_t*)0)->name_pos, "member type is of type size_t") ; \
      return offsetof(objecttype_t,name_pos) ; \
   } \
   /*implement lifetime functions*/ \
   static inline int new##_fctsuffix(/*out*/arraysf_t ** array, arraysf_e type) { \
      return new_arraysf(array, type) ; \
   } \
   static inline int delete##_fctsuffix(arraysf_t ** array, struct typeadapter_oit * typeadp) { \
      return delete_arraysf(array, typeadp) ; \
   } \
   /*implement query functions*/ \
   static inline size_t length##_fctsuffix(arraysf_t * array) { \
      return length_arraysf(array) ; \
   } \
   static inline objecttype_t * at##_fctsuffix(const arraysf_t * array, size_t pos) { \
      return (objecttype_t *) at_arraysf(array, pos, offsetnode##_fctsuffix()) ; \
   } \
   /*implement change functions*/ \
   static inline int insert##_fctsuffix(arraysf_t * array, objecttype_t * node, /*out*/objecttype_t ** inserted_node/*0=>not returned*/, struct typeadapter_oit * typeadp/*0=>no copy is made*/) { \
      return insert_arraysf(array, (struct generic_object_t*)node, (struct generic_object_t**)inserted_node, typeadp, offsetnode##_fctsuffix()) ; \
   } \
   static inline int tryinsert##_fctsuffix(arraysf_t * array, objecttype_t * node, /*out;err*/objecttype_t ** inserted_or_existing_node, struct typeadapter_oit * typeadp/*0=>no copy is made*/) { \
      return tryinsert_arraysf(array, (struct generic_object_t*)node, (struct generic_object_t**)inserted_or_existing_node, typeadp, offsetnode##_fctsuffix()) ; \
   } \
   static inline int remove##_fctsuffix(arraysf_t * array, size_t pos, /*out*/objecttype_t ** removed_node/*could be 0*/) { \
      return remove_arraysf(array, pos, (struct generic_object_t**)removed_node, offsetnode##_fctsuffix()) ; \
   } \
   static inline int tryremove##_fctsuffix(arraysf_t * array, size_t pos, /*out*/objecttype_t ** removed_node/*could be 0*/) { \
      return tryremove_arraysf(array, pos, (struct generic_object_t**)removed_node, offsetnode##_fctsuffix()) ; \
   } \
   /*implement iterator functions*/ \
   static inline int init##_fctsuffix##iterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array) { \
      return init_arraysfiterator(iter, array) ; \
   } \
   static inline int free##_fctsuffix##iterator(arraysf_iterator_t * iter) { \
      return free_arraysfiterator(iter) ; \
   } \
   static inline bool next##_fctsuffix##iterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/objecttype_t ** node) { \
      return next_arraysfiterator(iter, array, (struct generic_object_t**)node) ; \
   }


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

/* function: init_arraysfiterator
 * Initializes an iterator for <arraysf_t>. */
int init_arraysfiterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array) ;

/* function: free_arraysfiterator
 * Frees an iterator for <arraysf_t>. */
int free_arraysfiterator(arraysf_iterator_t * iter) ;

// group: iterate

/* function: next_arraysfiterator
 * Returns next iterated node.
 * The first call after <init_arraysfiterator> returns the first array element
 * if it is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the array is empty. */
bool next_arraysfiterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/struct generic_object_t ** node) ;


// section: inline implementation

/* define: length_arraysf
 * Implements <arraysf_t.length_arraysf>. */
#define length_arraysf(array)          ((array)->length)


#endif
