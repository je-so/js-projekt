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
struct arraysf_node_t ;
struct binarystack_t ;

/* typedef: struct arraysf_t
 * Exports <arraysf_t>. */
typedef struct arraysf_t               arraysf_t ;

/* typedef: struct arraysf_imp_it
 * Exports interface <arraysf_imp_it>, implementation memory policy. */
typedef struct arraysf_imp_it          arraysf_imp_it ;

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
 * Therefore leaf nodes (type <arraysf_node_t>) has a depth of less than 16 (index_bitsize/2)
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
   /* variable: impit
    * Pointer to memory policy interface used in implementation. */
   struct arraysf_imp_it   * impit ;
   /* variable: root
    * Points to top level nodes.
    * The size of the root array is determined by the <type> of the array. */
   struct arraysf_node_t   * root[] ;
} ;

// group: lifetime

/* function: new_arraysf
 * Allocates a new array object.
 * Parameter *type* determines the size of the <arraysf_t.root> array.
 * Set parameter *impit* to 0 if nodes should not be copied and freed in functions <insert_arraysf>, <remove_arraysf> and
 * <delete_arraysf>. All memory is allocated with calls to malloc and free from standard library.
 * You can obtain this standard implementation with a call to <defaultimpit_arraysf>.
 * Let this parameter point to an initialized interface if you want to use your own memory allocator.
 * At least the function pointers <arraysf_imp_it.malloc> and <arraysf_imp_it.free> must be set to a value != 0.
 *
 * The content of parameter *impit* is not copied so make sure that impit is valid as long as
 * the *array* object is alive. */
int new_arraysf(/*out*/arraysf_t ** array, arraysf_e type, arraysf_imp_it * impit/*0 => uses default_arraysfimpit()*/) ;

/* function: delete_arraysf
 * Frees allocated memory.
 * The policy interface <arraysf_imp_it> defines the <arraysf_imp_it.freenode> function pointer
 * which is called for every stored <arraysf_node_t>. If you have set this callback to 0 no
 * it is not called. */
int delete_arraysf(arraysf_t ** array) ;

// group: iterate

/* function: iteratortype_arraysf
 * Function declaration associates <arraysf_iterator_t> with <arraysf_t>.
 * The association is done with the type of the return value - there is no implementation. */
arraysf_iterator_t * iteratortype_arraysf(void) ;

/* function: iteratedtype_arraysf
 * Function declaration associates (<arraysf_node_t>*) with <arraysf_t>.
 * The association is done with the type of the return value - there is no implementation. */
struct arraysf_node_t * iteratedtype_arraysf(void) ;

// group: query

/* function: impolicy_arraysf
 * Returns the interface of the memory policy. */
arraysf_imp_it * impolicy_arraysf(arraysf_t * array) ;

/* function: length_arraysf
 * Returns the number of elements stored in the array. */
size_t length_arraysf(arraysf_t * array) ;

/* function: at_arraysf
 * Returns element at position *pos*.
 * If no element exists at position *pos* value 0 is returned. */
struct arraysf_node_t * at_arraysf(const arraysf_t * array, size_t pos) ;

// group: change

/* function: insert_arraysf
 * Inserts new node at position *pos* into array.
 * If this position is already occupied by another node EEXIST is returned.
 * The policy interface <arraysf_imp_it> defines <arraysf_imp_it.copynode> function pointer
 * which is called to make a copy from parameter *node* before inserting.
 * If you have set this callback to 0 no copy is made.
 * In out parameter *inserted_node* the inserted node is returned. If no cop< is made the
 * returned value is identical to parameter *node*. You can set this parameter to 0 if you
 * do not need the value. */
int insert_arraysf(arraysf_t * array, struct arraysf_node_t * node, /*out*/struct arraysf_node_t ** inserted_node/*0 => not returned*/) ;

/* function: tryinsert_arraysf
 * Same as <insert_arraysf> but no error log in case of EEXIST.
 * If EEXIST is returned nothing was inserted but the existing node will be
 * returned in *inserted_or_existing_node* nevertheless. */
int tryinsert_arraysf(arraysf_t * array, struct arraysf_node_t * node, /*out;err*/struct arraysf_node_t ** inserted_or_existing_node) ;

/* function: remove_arraysf
 * Removes node at position *pos*.
 * If no node exists at position *pos* ESRCH is returned.
 * The policy interface <arraysf_imp_it> defines function <arraysf_imp_it.freenode>
 * which is called to free the node after removing.
 * If you have set this callback to 0 the removed node is not freed.
 * In case parameter removed_node is not 0 the removed node is returned or 0.
 * If the removed node was freed with the callback 0 is returned. */
int remove_arraysf(arraysf_t * array, size_t pos, /*out*/struct arraysf_node_t ** removed_node/*could be 0*/) ;

/* function: tryremove_arraysf
 * Same as <remove_arraysf> but no error log in case of ESRCH. */
int tryremove_arraysf(arraysf_t * array, size_t pos, /*out*/struct arraysf_node_t ** removed_node/*could be 0*/) ;


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
#define arraysf_iterator_INIT_FREEABLE   { 0 }

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
bool next_arraysfiterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/struct arraysf_node_t ** node) ;


/* struct: arraysf_imp_it
 * Interface to adapt <arraysf_t> to different memory policies.
 * There are two kinds of memory functions.
 * The functions <copynode> and <freenode> are called to handle the parameter
 * with <arraysf_node_t> given in the call <insert_arraysf> and <remove_arraysf>.
 * The functions <malloc> and <free> are called to handle allocation of objects of
 * type <arraysf_t> and <arraysf_mwaybranch_t>. */
struct arraysf_imp_it {
   /* function: copynode
    * Creates new node and intializes it with values copies from *node*.
    * The newly created node is returned *copied_node*. */
   int   (* copynode) (const arraysf_imp_it * impit, const struct arraysf_node_t * node, /*out*/struct arraysf_node_t ** copied_node) ;
   /* function: freenode
    * Frees a node returned from a call to function <copynode>. */
   int   (* freenode) (const arraysf_imp_it * impit, struct arraysf_node_t * node) ;
   /* function: malloc
    * Allocates memory for internal nodes and the array objects itself.
    * Use the parameter *size* to discriminate between the two cases. */
   int   (* malloc)   (const arraysf_imp_it * impit, size_t size, /*out*/void ** memblock/*[size]*/) ;
   /* function: malloc
    * Frees memory allocated with <malloc> for internal nodes.
    *
    * Parameter:
    * size     - The same value as in the call to <malloc>
    * memblock - The value returned by a previous call to <malloc>. */
   int   (* free)     (const arraysf_imp_it * impit, size_t size, void * memblock/*[size]*/) ;
} ;

// group: lifetime

/* function: defaultimpit_arraysf
 * Returns the default memory policy of this object.
 * This is a static object. So do not free it nor change its content. */
arraysf_imp_it * default_arraysfimp(void) ;

/* function: new_arraysfimp
 * Allocates implementaion object of interface <arraysf_imp_it>.
 * The implementation copies and frees nodes of type <arraysf_node_t> and handles also
 * <arraysf_imp_it.malloc> and <arraysf_imp_it.free> requests.
 *
 * Parameter:
 * impit      - Returns pointer to newly allocated <arraysf_imp_it>.
 * objectsize - Memory size in bytes of object which contains <arraysf_node_t>.
 * nodeoffset - Offset in bytes from startaddr of allocated object to contained <arraysf_node_t>.
 *
 * Example:
 * > struct usernode_t {
 * >    arraysf_node_t node ;
 * >    const char *   userdata ;
 * > } ;
 * > int err ;
 * > arraysf_imp_it * impit = 0 ;
 * > err = new_arraysfimp(&impit, sizeof(struct usernode_t), offsetof(struct usernode_t, node)) ; */
int new_arraysfimp(/*out*/arraysf_imp_it ** impit, size_t objectsize, size_t nodeoffset) ;

/* function: delete_arraysfimp
 * Frees implementation object returned from <new_arraysfimp>. */
int delete_arraysfimp(arraysf_imp_it ** impit) ;


// section: inline implementation

/* define: length_arraysf
 * Implements <arraysf_t.length_arraysf>. */
#define length_arraysf(array)      ((array)->length)

/* define: impolicy_arraysf
 * Implements <arraysf_t.impolicy_arraysf>. */
#define impolicy_arraysf(array)        ((array)->impit)

#endif
