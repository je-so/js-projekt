/* title: ArraySTF
   Array implementation which supports strings as indizes (st).
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

   file: C-kern/api/ds/inmem/arraystf.h
    Header file <ArraySTF>.

   file: C-kern/ds/inmem/arraystf.c
    Implementation file <ArraySTF impl>.
*/
#ifndef CKERN_DS_INMEM_ARRAYSTF_HEADER
#define CKERN_DS_INMEM_ARRAYSTF_HEADER

// forward
union arraystf_unode_t ;
struct binarystack_t ;
struct generic_object_t ;
struct typeadapter_iot ;

/* typedef: struct arraystf_t
 * Export <arraystf_t>. */
typedef struct arraystf_t              arraystf_t ;

/* typedef: struct arraystf_iterator_t
 * Export <arraystf_iterator_t>, iterator type to iterate of contained nodes. */
typedef struct arraystf_iterator_t     arraystf_iterator_t ;

/* enums: arraystf_e
 *
 * arraystf_4BITROOT_UNSORTED  - The least significant 4 bits of the string length
 *                               are used to access the root array which is of size 16.
 * arraystf_6BITROOT_UNSORTED  - The least significant 6 bits of the string length
 *                               are used to access the root array which is of size 64.
 * arraystf_MSBROOT    - This root is accessed with MSBit position of first string byte as index.
 *                       The nr elements of the root array is 8.
 * arraystf_4BITROOT   - This root is accessed with the 4 most significant bits (0xf0) of the first string byte as index.
 *                       The nr elements of the root array is 16.
 * arraystf_6BITROOT   - This root is accessed with the 6 most significant bits (0xfc) of the first string byte as index.
 *                       The nr elements of the root array is 64.
 * arraystf_8BITROOT   - This root is accessed with value of the first string byte as index.
 *                       The nr elements of the root array is 256.
 * */
enum arraystf_e {
    arraystf_4BITROOT_UNSORTED
   ,arraystf_6BITROOT_UNSORTED
   ,arraystf_MSBROOT
   ,arraystf_4BITROOT
   ,arraystf_6BITROOT
   ,arraystf_8BITROOT
} ;

typedef enum arraystf_e                arraystf_e ;


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
 * - The byte values key->size .. UINT_MAX-1 are always 0
 * - The value at offset is key->size and has 8*sizeof(size_t) bits instead of only 8
 *
 */
struct arraystf_t {
   /* variable: length
    * The number of elements stored in this array. */
   size_t                  length ;
   /* variable: type
    * The type of the array, see <arraystf_e>. */
   arraystf_e              type ;
   /* variable: root
    * Points to top level nodes.
    * The size of the root array is determined by the <type> of the array. */
   union arraystf_unode_t  * root[] ;
} ;

// group: lifetime

/* function: new_arraystf
 * Allocates a new array object.
 * Parameter *type* determines the size of the <arraystf_t.root> array
 * and the chosen root distribution. */
int new_arraystf(/*out*/arraystf_t ** array, arraystf_e type) ;

/* function: delete_arraystf
 * Frees allocated memory.
 * If typeadp is set to 0 no free function is called for contained nodes. */
int delete_arraystf(arraystf_t ** array, struct typeadapter_iot * typeadp) ;

// group: foreach-support

/* typedef: iteratortype_arraystf
 * Declaration to associate <arraystf_iterator_t> with <arraystf_t>.
 * The association is done with a typedef which looks like a function. */
typedef arraystf_iterator_t      iteratortype_arraystf ;

/* typedef: iteratedtype_arraystf
 * Function declaration to associate <generic_object_t> with <arraystf_t>.
 * The association is done with a typedef which looks like a function. */
typedef struct generic_object_t  iteratedtype_arraystf ;

// group: query

/* function: length_arraystf
 * Returns the number of elements stored in the array. */
size_t length_arraystf(arraystf_t * array) ;

/* function: at_arraystf
 * Returns node with same key as *keydata*[*size*].
 * If no element exists with this key value 0 is returned. */
struct generic_object_t * at_arraystf(const arraystf_t * array, size_t size, const uint8_t keydata[size], size_t offset_node) ;

// group: change

/* function: insert_arraystf
 * Inserts new node with string key into array.
 * If the provided key is already stored EEXIST is returned.
 * If you have set typeadt to a value != 0 then a copy of node is made before it is inserted.
 * The copied node is returned in *inserted_node*. If typeadt is 0 and no copy is made inserted_node
 * is set to node. */
int insert_arraystf(arraystf_t * array, struct generic_object_t * node, /*out*/struct generic_object_t ** inserted_node/*0=>copy not returned*/, struct typeadapter_iot * typeadp/*0=>no copy is made*/, size_t offset_node) ;

/* function: tryinsert_arraystf
 * Same as <insert_arraystf> but no error log in case of EEXIST.
 * If EEXIST is returned nothing was inserted but the existing node will be
 * returned in *inserted_or_existing_node* nevertheless. */
int tryinsert_arraystf(arraystf_t * array, struct generic_object_t * node, /*out;err*/struct generic_object_t ** inserted_or_existing_node, struct typeadapter_iot * typeadp/*0=>no copy is made*/, size_t offset_node) ;

/* function: remove_arraystf
 * Removes node with key *keydata*.
 * If no node exists with this key ESRCH is returned.
 * In case of success the removed node is returned in *removed_node*. */
int remove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct generic_object_t ** removed_node/*could be 0*/, size_t offset_node) ;

/* function: tryremove_arraystf
 * Same as <remove_arraystf> but no error log in case of ESRCH. */
int tryremove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct generic_object_t ** removed_node/*could be 0*/, size_t offset_node) ;

// group: generic

/* define: arraystf_IMPLEMENT
 * Generates the interface for a specific single linked list.
 * The type of the list object must be declared with help of <slist_DECLARE>
 * before this macro. It is also possible to construct "listtype_t" in another way before
 * calling this macro. In the latter case "listtype_t" must have a pointer to the object
 * declared as its first field with the name *last*.
 *
 *
 * Parameter:
 * _fctsuffix   - It is the suffix of the generated container interface functions which wraps all calls to <arraystf_t>.
 * objecttype_t - The object which is stored in this container derived from generic <arraystf_t>.
 * name_addr    - The member name (access path) in objecttype_t corresponding to the
 *                name of the first embedded member of <arraystf_node_t> - see <arraystf_node_EMBED>.
 * */
#define arraystf_IMPLEMENT(_fctsuffix, objecttype_t, name_addr)   \
   typedef arraystf_iterator_t   iteratortype##_fctsuffix ;       \
   typedef objecttype_t          iteratedtype##_fctsuffix ;       \
   /*declare helper functions as always inline*/                  \
   static inline size_t offsetnode##_fctsuffix(void) __attribute__ ((always_inline)) ; \
   /*declare function signatures as always inline*/               \
   static inline int new##_fctsuffix(/*out*/arraystf_t ** array, arraystf_e type) __attribute__ ((always_inline)) ; \
   static inline int delete##_fctsuffix(arraystf_t ** array, struct typeadapter_iot * typeadp) __attribute__ ((always_inline)) ; \
   static inline size_t length##_fctsuffix(arraystf_t * array) __attribute__ ((always_inline)) ; \
   static inline objecttype_t * at##_fctsuffix(const arraystf_t * array, size_t size, const uint8_t keydata[size]) __attribute__ ((always_inline)) ; \
   static inline int insert##_fctsuffix(arraystf_t * array, objecttype_t * node, /*out*/objecttype_t ** inserted_node/*0=>copy not returned*/, struct typeadapter_iot * typeadp/*0=>no copy is made*/) __attribute__ ((always_inline)) ; \
   static inline int tryinsert##_fctsuffix(arraystf_t * array, objecttype_t * node, /*out;err*/objecttype_t ** inserted_or_existing_node, struct typeadapter_iot * typeadp/*0=>no copy is made*/) __attribute__ ((always_inline)) ; \
   static inline int remove##_fctsuffix(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/objecttype_t ** removed_node/*could be 0*/) __attribute__ ((always_inline)) ; \
   static inline int tryremove##_fctsuffix(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/objecttype_t ** removed_node/*could be 0*/) __attribute__ ((always_inline)) ; \
   static inline int initfirst##_fctsuffix##iterator(/*out*/arraystf_iterator_t * iter, arraystf_t * array) __attribute__ ((always_inline)) ; \
   static inline int free##_fctsuffix##iterator(arraystf_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fctsuffix##iterator(arraystf_iterator_t * iter, arraystf_t * array, /*out*/objecttype_t ** node) __attribute__ ((always_inline)) ; \
   /*implement helper functions */ \
   static inline size_t offsetnode##_fctsuffix(void) { \
      static_assert( ((const uint8_t**) (offsetof(objecttype_t,name_addr))) == &((objecttype_t*)0)->name_addr, "member type is of type const uint8_t*") ; \
      return offsetof(objecttype_t,name_addr) ; \
   } \
   /*implement lifetime functions*/ \
   static inline int new##_fctsuffix(/*out*/arraystf_t ** array, arraystf_e type) { \
      return new_arraystf(array, type) ; \
   } \
   static inline int delete##_fctsuffix(arraystf_t ** array, struct typeadapter_iot * typeadp) { \
      return delete_arraystf(array, typeadp) ; \
   } \
   /*implement query functions*/ \
   static inline size_t length##_fctsuffix(arraystf_t * array) { \
      return length_arraystf(array) ; \
   } \
   static inline objecttype_t * at##_fctsuffix(const arraystf_t * array, size_t size, const uint8_t keydata[size]) { \
      return (objecttype_t *) at_arraystf(array, size, keydata, offsetnode##_fctsuffix()) ; \
   } \
   /*implement change functions*/ \
   static inline int insert##_fctsuffix(arraystf_t * array, objecttype_t * node, /*out*/objecttype_t ** inserted_node/*0=>copy not returned*/, struct typeadapter_iot * typeadp/*0=>no copy is made*/) { \
      return insert_arraystf(array, (struct generic_object_t*)node, (struct generic_object_t**)inserted_node, typeadp, offsetnode##_fctsuffix()) ; \
   } \
   static inline int tryinsert##_fctsuffix(arraystf_t * array, objecttype_t * node, /*out;err*/objecttype_t ** inserted_or_existing_node, struct typeadapter_iot * typeadp/*0=>no copy is made*/) { \
      return tryinsert_arraystf(array, (struct generic_object_t*)node, (struct generic_object_t**)inserted_or_existing_node, typeadp, offsetnode##_fctsuffix()) ; \
   } \
   static inline int remove##_fctsuffix(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/objecttype_t ** removed_node/*could be 0*/) { \
      return remove_arraystf(array, size, keydata, (struct generic_object_t**)removed_node, offsetnode##_fctsuffix()) ; \
   } \
   static inline int tryremove##_fctsuffix(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/objecttype_t ** removed_node/*could be 0*/) { \
      return tryremove_arraystf(array, size, keydata, (struct generic_object_t**)removed_node, offsetnode##_fctsuffix()) ; \
   } \
   /*implement iterator functions*/ \
   static inline int initfirst##_fctsuffix##iterator(/*out*/arraystf_iterator_t * iter, arraystf_t * array) { \
      return initfirst_arraystfiterator(iter, array) ; \
   } \
   static inline int free##_fctsuffix##iterator(arraystf_iterator_t * iter) { \
      return free_arraystfiterator(iter) ; \
   } \
   static inline bool next##_fctsuffix##iterator(arraystf_iterator_t * iter, arraystf_t * array, /*out*/objecttype_t ** node) { \
      return next_arraystfiterator(iter, array, (struct generic_object_t**)node) ; \
   }


/* struct: arraystf_iterator_t
 * Iterates over elements contained in <arraystf_t>. */
struct arraystf_iterator_t {
   /* variable: stack
    * Remembers last position in tree. */
   struct binarystack_t * stack ;
   /* variable: ri
    * Index into <arraystf_t.root>. */
   unsigned             ri ;
} ;

// group: lifetime

/* define: arraystf_iterator_INIT_FREEABLE
 * Static initializer. */
#define arraystf_iterator_INIT_FREEABLE   { 0, 0 }

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
bool next_arraystfiterator(arraystf_iterator_t * iter, arraystf_t * array, /*out*/struct generic_object_t ** node) ;


// section: inline implementation

/* define: length_arraystf
 * Implements <arraystf_t.length_arraystf>. */
#define length_arraystf(array)         ((array)->length)


#endif
