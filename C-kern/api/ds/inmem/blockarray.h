/* title: BlockArray

   A blocked array is implementation of an array where not all
   elements are stored in a single memory block.
   It support non contiguous blocks of memory to store elements
   in a B-tree like hierarchy.

   It is possible that child pointers in any ptr block (including root) are NULL.
   This helps to save memory but this type of array is not optimized for a
   sparse distribution of array indices.

   Once an element is assigned to an index its memory address never
   changes except if the array is resized to a smaller size which deletes the
   memory block containing the assigned element.

   >                   +──[ root block ]───────────+
   >                   │ child[0] | child[1] | ... |
   >                   +──┬───────────┬────────────+
   >         ▾------------+           +------▾
   >         +──[ ptr block ]────────────+   +──[ ptr block ]────────────+
   >         │ child[0] | child[1] | ... |   │ child[0] | child[1] | ... |
   >         +──┬──────────┬─────────────+   +───┬───────────────────────+
   > ▾----------+          +-------▾             NULL
   > +──[ data block ]─────────+   +──[ data block ]─────────+
   > │ elem[0] | elem[1] | ... |   │ elem[0] | elem[1] | ... |
   > +─────────────────────────+   +─────────────────────────+
   >

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/ds/inmem/blockarray.h
    Header file <BlockArray>.

   file: C-kern/ds/inmem/blockarray.c
    Implementation file <BlockArray impl>.
*/
#ifndef CKERN_DS_INMEM_BLOCKARRAY_HEADER
#define CKERN_DS_INMEM_BLOCKARRAY_HEADER

/* typedef: struct blockarray_t
 * Export <blockarray_t> into global namespace. */
typedef struct blockarray_t               blockarray_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_blockarray
 * Test <blockarray_t> functionality. */
int unittest_ds_inmem_blockarray(void) ;
#endif


/* struct: blockarray_t
 * Stores elements and retrieves them by index of type integer.
 * All elements are stored in memory blocks. So a single element
 * wastes (blocksize-elementsize) bytes. A block can store up to
 * blocksize/elementsize elements.
 *
 * The index determines the memory block and the position within it
 * so elements can be accessed on a single block without searching.
 * If elements are assigned with non continues index values the elements on
 * the same page between the assigned elements are set to 0.
 * The implementation does not remember that such an element was not assigned
 * or initialized. So you need to detect unassigned elements yourself by comparing
 * them with 0.
 *
 * If all elements fitr on one page the depth of the tree is 0 and root points to a data block.
 * If all elements do not fit on a single block a B-tree like hierarchy is created.
 * In this case root points and ptr blocks contains pointers to ptr blocks or data blocks.
 * The depth value is > 0 and determines how many ptr blocks are encouontered from root to data block.
 * These so called child pointers will be set to NULL to save memory if the index value
 * is sparse (which is not recommended). If an element is accessed whose path encounters
 * a NULL pointer the address NULL is returned which states that this element was not
 * assigned before.
 *
 * Accessing an element in such a hierarchy costs time
 * O(log(nr_elements)/log(nr_of_elements_per_block)).
 *
 * The size of a memory block can be set once at array init,
 * it corresponds to <pagesize_e>.
 *
 * Implementation Invariant:
 * Blockarray uses only <allocpage_pagecache> and <releasepage_pagecache> the allocate and free
 * blocks of memory with the pagesize given as parameter in <init_blockarray>. */
struct blockarray_t {
   /* variable: elements_per_block
    * Number of elements stored in a single data block. */
   size_t   elements_per_block ;
   /* variable: root
    * Points to root memory block. The root contains ptrs of data elements
    * depending on depth > 0 resp. depth == 0. */
   void *   root ;
   /* variable: elementsize
    * The size of a single element */
   uint16_t elementsize ;
   /* variable: log2elements_per_block
    * The log2 of <elements_per_block> plus 1.
    * If this value is 0 the value <elements_per_block> is not a power of two.
    * This is used to speed up computation (avoiding division by <elements_per_block>). */
   uint8_t  log2elements_per_block ;
   /* variable: depth
    * The depth of the tree.
    * A depth of 0 means root points to a single data block.
    * A depth of 1 means root points to a ptr block whose child pointers point to data blocks
    * and so on. */
   uint8_t  depth ;
   /* variable: log2ptr_per_block
    * The log2 of number of pointer stored in a memory block.
    * A memory block has always a size which is a power of 2 and
    * it is assumed that a pointer also has a size which is a power of 2 (checked in unittest).
    * Therefore the number of pointers storable in a memory block is also a power of 2.
    * This optimization avoids another division. */
   uint8_t  log2ptr_per_block ;
   /* variable: pagesize
    * The size of a memory block. This value corresponds to a value from <pagesize_e>. */
   uint8_t  pagesize ;
} ;

// group: lifetime

/* define: blockarray_INIT_FREEABLE
 * Static initializer. */
#define blockarray_INIT_FREEABLE          { 0, 0, 0, 0, 0, 0, 0 }

/* function: init_blockarray
 * Initializes barray to use blocks of memory of size pagesize.
 * Also one datablock is preallocated for elements beginning from index 0. */
int init_blockarray(/*out*/blockarray_t * barray, pagesize_e pagesize, uint16_t elementsize, const struct pagecache_t pagecache) ;

/* function: free_blockarray
 * Frees all memory blocks. All elements become invalid
 * so make sure that all references to them have been cleared. */
int free_blockarray(blockarray_t * barray, const struct pagecache_t pagecache) ;

// group: query

/* function: isfree_blockarray
 * Returns true if barray equals <blockarray_INIT_FREEABLE>. */
bool isfree_blockarray(const blockarray_t * barray) ;

// group: read

/* function: at_blockarray
 * Returns the memory address of element at position arrayindex.
 * If the element was not assigned previoulsy the returned address
 * is either NULL or points to an element initialized to 0. */
void * at_blockarray(blockarray_t * barray, size_t arrayindex) ;

// group: update

/* function: assign_blockarray
 * Assigns memory to an element at position arrayindex and returns its address.
 * The memory address is aligned to the size of the element. The memory is allocated
 * with help of pagecache (see <pagecache_t>). If pagecache is set to <pagecache_INIT_FREEABLE>
 * the function works like <at_blockarray>.
 * 0 is returned in case of an error.
 * If pagecache was set errcode is set to value ENOMEM (or EINVAL if something internal went wrong).
 * If pagecache was cleared errcode is set to value ENODATA. */
void * assign_blockarray(blockarray_t * barray, size_t arrayindex, const struct pagecache_t pagecache, /*err*/int * errcode/*0 ==> no error is returned*/) ;

// NOT IMPLEMENTED
// Deallocates all memory blocks outside of slice [lowerindex, upperindex]
// void shrink_blockarray(blockarray_t * barray, size_t lowerindex, size_t upperindex) ;

// group: generic

/* define: blockarray_IMPLEMENT
 * Generates interface of blockarray storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix     - It is the suffix of the generated blockarray interface functions, e.g. "init##_fsuffix".
 * object_t     - The type of object which can be stored and retrieved from this blockarray.
 * */
void blockarray_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t) ;



// section: inline implementation

// group: blockarray_t

/* define: at_blockarray
 * Implements <blockarray_t.at_blockarray>. */
#define at_blockarray(barray, arrayindex) \
         (assign_blockarray((barray), (arrayindex), (pagecache_t)pagecache_INIT_FREEABLE, 0))

/* define: blockarray_IMPLEMENT
 * Implements <blockarray_t.blockarray_IMPLEMENT>. */
#define blockarray_IMPLEMENT(_fsuffix, object_t)   \
         static inline int init##_fsuffix(/*out*/blockarray_t * barray, pagesize_e pagesize) __attribute__ ((always_inline)) ; \
         static inline int free##_fsuffix(blockarray_t * barray) __attribute__ ((always_inline)) ; \
         static inline object_t * at##_fsuffix(blockarray_t * barray, size_t arrayindex) __attribute__ ((always_inline)) ; \
         static inline object_t * assign##_fsuffix(blockarray_t * barray, size_t arrayindex, int * errcode) __attribute__ ((always_inline)) ; \
         static inline int init##_fsuffix(/*out*/blockarray_t * barray, pagesize_e pagesize) { \
            return init_blockarray(barray, pagesize, sizeof(object_t), pagecache_maincontext()) ; \
         } \
         static inline int free##_fsuffix(blockarray_t * barray) { \
            return free_blockarray(barray, pagecache_maincontext()) ; \
         } \
         static inline object_t * at##_fsuffix(blockarray_t * barray, size_t arrayindex) { \
            return (object_t*)at_blockarray(barray, arrayindex) ; \
         } \
         static inline object_t * assign##_fsuffix(blockarray_t * barray, size_t arrayindex, int * errcode) { \
            return (object_t*)assign_blockarray(barray, arrayindex, pagecache_maincontext(), errcode) ; \
         }


#endif
