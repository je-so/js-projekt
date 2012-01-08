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

/* typedef: struct arraysf_t
 * Exports <arraysf_t>. */
typedef struct arraysf_t               arraysf_t ;

/* typedef: struct arraysf_freecb_t
 * Exports <arraysf_freecb_t>. */
typedef struct arraysf_freecb_t        arraysf_freecb_t ;

/* typedef: struct arraysf_imp_it
 * Exports interface <arraysf_imp_it>, implementation memory policy. */
typedef struct arraysf_imp_it          arraysf_imp_it ;

/* typedef: struct arraysf_itercb_t
 * Exports <arraysf_itercb_t>, a callback processing iterated array nodes. */
typedef struct arraysf_itercb_t        arraysf_itercb_t ;

/* typedef: arraysf_itercb_f
 * Signature of iterator callback function used in <arraysf_itercb_t>.
 * A returned value not equal 0 is considered an error and aborts the iteration. */
typedef int                         (* arraysf_itercb_f) (arraysf_itercb_t * itercb, struct arraysf_node_t * node) ;

/* enums: arraysf_e
 *
 * arraysf_MSBPOSROOT - This root is optimized for indizes *pos* which differ in MSBit position.
 *                      You can access the correct root entry with index: log2_int(pos)/2 .
 * arraysf_8BITROOT0  - This root is optimized for values between 0 .. 255. The root is accessed with the
 *                      the lowest 8 bit of the number (pos % 256) or (pos & 0xff).
 * arraysf_8BITROOT24 - If MSBit positions are always >= 24 (and < 32) as is the case for IPv4 addresses
 *                      this root distribution must be used. The root is accessed with the
 *                      the highest 8 bit of a 32 bit number. The number should be less or equal than UINT32_MAX.
 * */
enum arraysf_e {
    arraysf_MSBPOSROOT
   ,arraysf_8BITROOT0
   ,arraysf_8BITROOT24
} ;

typedef enum arraysf_e                 arraysf_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_arraysf
 * Unittest for <arraysf_t>. */
extern int unittest_ds_inmem_arraysf(void) ;
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
   /* variable: nr_elements
    * The number of elements stored in this array. */
   size_t                  nr_elements ;
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
extern int new_arraysf(/*out*/arraysf_t ** array, arraysf_e type, arraysf_imp_it * impit/*0 => uses default_arraysfimpit()*/) ;

/* function: delete_arraysf
 * Frees allocated memory.
 * The policy interface <arraysf_imp_it> defines the <arraysf_imp_it.freenode> function pointer
 * which is called for every stored <arraysf_node_t>. If you have set this callback to 0 no
 * it is not called. */
extern int delete_arraysf(arraysf_t ** array) ;

// group: query

/* function: impolicy_arraysf
 * Returns the interface of the memory policy. */
extern arraysf_imp_it * impolicy_arraysf(arraysf_t * array) ;

/* function: nrelements_arraysf
 * Returns the number of elements stored in the array. */
extern size_t nrelements_arraysf(arraysf_t * array) ;

/* function: at_arraysf
 * Returns element at position *pos*.
 * If no element exists at position *pos* value 0 is returned. */
extern struct arraysf_node_t * at_arraysf(const arraysf_t * array, size_t pos) ;

/* function: iterate_arraysf
 * Iterates over nodes and calls *itercb* for every contained node.
 * If the iterator callback <arraysf_itercb_t.fct> returns an error the iteration is aborted
 * with the same error code returned. */
extern int iterate_arraysf(const arraysf_t * array, arraysf_itercb_t * itercb) ;

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
extern int insert_arraysf(arraysf_t * array, size_t pos, struct arraysf_node_t * node, /*out*/struct arraysf_node_t ** inserted_node/*0 => not returned*/) ;

/* function: tryinsert_arraysf
 * Same as <insert_arraysf> but no error log in case of EEXIST.
 * If EEXIST is returned nothing was inserted but the existing node will be
 * returned in *inserted_or_existing_node* nevertheless. */
extern int tryinsert_arraysf(arraysf_t * array, size_t pos, struct arraysf_node_t * node, /*out;err*/struct arraysf_node_t ** inserted_or_existing_node) ;

/* function: remove_arraysf
 * Removes node at position *pos*.
 * If no node exists at position *pos* ESRCH is returned.
 * The policy interface <arraysf_imp_it> defines <arraysf_imp_it.freenode> function pointer
 * which is called to free the node after removing.
 * If you have set this callback to 0 no callback is made. */
extern int remove_arraysf(arraysf_t * array, size_t pos) ;

/* function: tryremove_arraysf
 * Same as <remove_arraysf> but no error log in case of ESRCH. */
extern int tryremove_arraysf(arraysf_t * array, size_t pos) ;


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
extern arraysf_imp_it * default_arraysfimp(void) ;

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
extern int new_arraysfimp(/*out*/arraysf_imp_it ** impit, size_t objectsize, size_t nodeoffset) ;

/* function: delete_arraysfimp
 * Frees implementation object returned from <new_arraysfimp>. */
extern int delete_arraysfimp(arraysf_imp_it ** impit) ;

/* struct: arraysf_itercb_t
 * Callback object to process iterated nodes. */
struct arraysf_itercb_t {
   arraysf_itercb_f  fct ;
} ;


// section: inline implementation

/* define: nrelements_arraysf
 * Implements <arraysf_t.nrelements_arraysf>. */
#define nrelements_arraysf(array)      ((array)->nr_elements)

/* define: impolicy_arraysf
 * Implements <arraysf_t.impolicy_arraysf>. */
#define impolicy_arraysf(array)        ((array)->impit)

#endif
