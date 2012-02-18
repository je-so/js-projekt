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
struct arraystf_node_t ;
struct binarystack_t ;

/* typedef: struct arraystf_t
 * Export <arraystf_t>. */
typedef struct arraystf_t              arraystf_t ;

/* typedef: struct arraystf_imp_it
 * Exports interface <arraystf_imp_it>, implementation memory policy. */
typedef struct arraystf_imp_it         arraystf_imp_it ;

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
 * Therefore leaf nodes (type <arraystf_node_t>) have always a depth of less than (key length in bits)/2
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
 * - Every key is of length ~(size_t)0 (UINT32_MAX or UINT64_MAX)
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
   /* variable: impit
    * Pointer to memory policy interface used in implementation. */
   struct arraystf_imp_it  * impit ;
   /* variable: root
    * Points to top level nodes.
    * The size of the root array is determined by the <type> of the array. */
   struct arraystf_node_t  * root[] ;
} ;

// group: lifetime

/* function: new_arraystf
 * Allocates a new array object.
 * Parameter *type* determines the size of the <arraystf_t.root> array.
 * Set parameter *impit* to 0 if nodes should not be copied and freed in functions <insert_arraystf>, <remove_arraystf> and
 * <delete_arraystf>. All memory is allocated with calls to malloc and free from standard library.
 * You can obtain this standard implementation with a call to <defaultimpit_arraystf>.
 * Let this parameter point to an initialized interface if you want to use your own memory allocator.
 * At least the function pointers <arraystf_imp_it.malloc> and <arraystf_imp_it.free> must be set to a value != 0.
 *
 * The content of parameter *impit* is not copied so make sure that impit is valid as long as
 * the *array* object is alive. */
int new_arraystf(/*out*/arraystf_t ** array, arraystf_e type, arraystf_imp_it * impit/*0 => uses default_arraystfimpit()*/) ;

/* function: delete_arraystf
 * Frees allocated memory.
 * The policy interface <arraystf_imp_it> defines the <arraystf_imp_it.freenode> function pointer
 * which is called for every stored <arraystf_node_t>. If you have set this callback to 0 no
 * it is not called. */
int delete_arraystf(arraystf_t ** array) ;

// group: query

/* function: impolicy_arraystf
 * Returns the interface of the memory policy. */
arraystf_imp_it * impolicy_arraystf(arraystf_t * array) ;

/* function: length_arraystf
 * Returns the number of elements stored in the array. */
size_t length_arraystf(arraystf_t * array) ;

/* function: at_arraystf
 * Returns element at position *pos*.
 * If no element exists at position *pos* value 0 is returned. */
struct arraystf_node_t * at_arraystf(const arraystf_t * array, size_t size, const uint8_t keydata[size]) ;

// group: iterate

/* function: iteratortype_arraystf
 * Declaration associates <arraystf_iterator_t> with <arraystf_t>.
 * The association is done with the type of the return value - there is no implementation. */
arraystf_iterator_t * iteratortype_arraystf(void) ;

/* function: iteratedtype_arraystf
 * Declaration associates (<arraystf_node_t>*) with <arraystf_t>.
 * The association is done with the type of the return value - there is no implementation. */
struct arraystf_node_t * iteratedtype_arraystf(void) ;

// group: change

/* function: insert_arraystf
 * Inserts new node at position *pos* into array.
 * If this position is already occupied by another node EEXIST is returned.
 * The policy interface <arraystf_imp_it> defines <arraystf_imp_it.copynode> function pointer
 * which is called to make a copy from parameter *node* before inserting.
 * If you have set this callback to 0 no copy is made.
 * In out parameter *inserted_node* the inserted node is returned. If no cop< is made the
 * returned value is identical to parameter *node*. You can set this parameter to 0 if you
 * do not need the value. */
int insert_arraystf(arraystf_t * array, struct arraystf_node_t * node, /*out*/struct arraystf_node_t ** inserted_node/*0 => not returned*/) ;

/* function: tryinsert_arraystf
 * Same as <insert_arraystf> but no error log in case of EEXIST.
 * If EEXIST is returned nothing was inserted but the existing node will be
 * returned in *inserted_or_existing_node* nevertheless. */
int tryinsert_arraystf(arraystf_t * array, struct arraystf_node_t * node, /*out;err*/struct arraystf_node_t ** inserted_or_existing_node) ;

/* function: remove_arraystf
 * Removes node with key *keydata*.
 * If no node exists with key keydata ESRCH is returned.
 * The policy interface <arraystf_imp_it> defines <arraystf_imp_it.freenode> function pointer
 * which is called to free the node after removing.
 * If you have set this callback to 0 no callback is made. */
int remove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct arraystf_node_t ** removed_node/*could be 0*/) ;

/* function: tryremove_arraystf
 * Same as <remove_arraystf> but no error log in case of ESRCH. */
int tryremove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct arraystf_node_t ** removed_node/*could be 0*/) ;


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
#define arraystf_iterator_INIT_FREEABLE   { 0 }

/* function: init_arraystfiterator
 * Initializes an iterator for an <arraystf_t>. */
int init_arraystfiterator(/*out*/arraystf_iterator_t * iter, arraystf_t * array) ;

/* function: free_arraystfiterator
 * Frees an iterator for an <arraystf_t>. */
int free_arraystfiterator(arraystf_iterator_t * iter) ;

// group: iterate

/* function: next_arraystfiterator
 * Returns next iterated node.
 * The first call after <init_arraystfiterator> returns the first array element
 * if it is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the array is empty. */
bool next_arraystfiterator(arraystf_iterator_t * iter, arraystf_t * array, /*out*/struct arraystf_node_t ** node) ;


/* struct: arraystf_imp_it
 * Interface to adapt <arraystf_t> to different memory policies.
 * There are two kinds of memory functions.
 * The functions <copynode> and <freenode> are called to handle the parameter
 * with <arraystf_node_t> given in the call <insert_arraystf> and <remove_arraystf>.
 * The functions <malloc> and <free> are called to handle allocation of objects of
 * type <arraystf_t> and <arraystf_mwaybranch_t>. */
struct arraystf_imp_it {
   /* function: copynode
    * Creates new node and intializes it with values copies from *node*.
    * The newly created node is returned *copied_node*. */
   int   (* copynode) (const arraystf_imp_it * impit, const struct arraystf_node_t * node, /*out*/struct arraystf_node_t ** copied_node) ;
   /* function: freenode
    * Frees a node returned from a call to function <copynode>. */
   int   (* freenode) (const arraystf_imp_it * impit, struct arraystf_node_t * node) ;
   /* function: malloc
    * Allocates memory for internal nodes and the array objects itself.
    * Use the parameter *size* to discriminate between the two cases. */
   int   (* malloc)   (const arraystf_imp_it * impit, size_t size, /*out*/void ** memblock/*[size]*/) ;
   /* function: malloc
    * Frees memory allocated with <malloc> for internal nodes.
    *
    * Parameter:
    * size     - The same value as in the call to <malloc>
    * memblock - The value returned by a previous call to <malloc>. */
   int   (* free)     (const arraystf_imp_it * impit, size_t size, void * memblock/*[size]*/) ;
} ;

// group: lifetime

/* function: defaultimpit_arraystf
 * Returns the default memory policy of this object.
 * This is a static object. So do not free it nor change its content. */
arraystf_imp_it * default_arraystfimp(void) ;

/* function: new_arraystfimp
 * Allocates implementaion object of interface <arraystf_imp_it>.
 * The implementation copies and frees nodes of type <arraystf_node_t> and handles also
 * <arraystf_imp_it.malloc> and <arraystf_imp_it.free> requests.
 *
 * Parameter:
 * impit      - Returns pointer to newly allocated <arraystf_imp_it>.
 * objectsize - Memory size in bytes of object which contains <arraystf_node_t>.
 * nodeoffset - Offset in bytes from startaddr of allocated object to contained <arraystf_node_t>.
 *
 * Example:
 * > struct usernode_t {
 * >    arraystf_node_t node ;
 * >    const char *    userdata ;
 * > } ;
 * > int err ;
 * > arraystf_imp_it * impit = 0 ;
 * > err = new_arraystfimp(&impit, sizeof(struct usernode_t), offsetof(struct usernode_t, node)) ; */
int new_arraystfimp(/*out*/arraystf_imp_it ** impit, size_t objectsize, size_t nodeoffset) ;

/* function: delete_arraystfimp
 * Frees implementation object returned from <new_arraystfimp>. */
int delete_arraystfimp(arraystf_imp_it ** impit) ;


// section: inline implementation

/* define: length_arraystf
 * Implements <arraystf_t.length_arraystf>. */
#define length_arraystf(array)      ((array)->length)

/* define: impolicy_arraystf
 * Implements <arraystf_t.impolicy_arraystf>. */
#define impolicy_arraystf(array)        ((array)->impit)

#endif
