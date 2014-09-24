/* title: BlockArray

   A blocked array implements an array where not all contained
   elements are stored in a contiguous way.
   It supports non contiguous blocks of memory to store elements
   in a B-tree like hierarchy.

   It is possible that child pointers in any ptr block (including root) are NULL.
   This helps to save memory but this type of array is not optimized for a
   sparse distribution of indices.

   Once an element is assigned to an index its memory address never
   changes except if the array is resized to a smaller size which deletes the
   memory block containing the assigned element.

   >                   ╭──[ root block ]───────────╮
   >                   │ child[0] | child[1] | ... |
   >                   ╰──┬───────────┬────────────╯
   >                      │           │
   >         ▾────────────┘           └────────▾
   >         ╭──[ ptr block ]────────────╮     ╭──[ ptr block ]────────────╮
   >         │ child[0] | child[1] | ... |     │ child[0] | child[1] | ... |
   >         ╰──┬──────────┬─────────────╯     ╰───┬───────────────────────╯
   >            │          │                       NULL
   >  ▾─────────┘          └──────▾
   > ╭──[ data block ]─────────╮  ╭──[ data block ]─────────╮
   > │ elem[0] | elem[1] | ... |  │ elem[0] | elem[1] | ... |
   > ╰─────────────────────────╯  ╰─────────────────────────╯
   >

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
 * All elements are stored in memory blocks. So a block with a
 * single element wastes (blocksize-elementsize) bytes.
 * A block can store up to blocksize divided by elementsize elements.
 *
 * The index determines the memory block and the position within it
 * so elements can be accessed in a single block without searching.
 * If elements are assigned with non continues index values the non assigned
 * elements between the assigned elements are set to 0.
 * The implementation does not remember that such an element was not assigned
 * or not initialized. So you need to detect unassigned elements yourself by
 * comparing them with 0.
 *
 * If all elements fit on one page the depth of the tree is 0 and root points to a data block.
 * If all elements do not fit on a single block a B-tree like hierarchy is created.
 * In this case root (and ptr blocks also) contains pointers to ptr blocks or data blocks.
 * The depth value determines how many ptr blocks are encouontered from root to data block.
 * Any value > 0 means the root is of type ptr block and (depth-1) ptr blocks follow.
 * These pointers to child nodes will be set to NULL to save memory if a range of index values
 * is not asigned (which is not recommended). If an element is read whose path contains
 * a NULL pointer the address NULL is returned which states that this element was not
 * assigned before.
 *
 * Accessing an element in a dense hierarchy costs time
 * O(log(nr_elements)/log(nr_of_elements_per_block)).
 *
 * The size of a memory block can be set once at initialization of array,
 * it corresponds to <pagesize_e>.
 *
 * Implementation Invariant:
 * Blockarray uses only <allocpage_pagecache> and <releasepage_pagecache> to allocate and free
 * blocks of memory with size pagesize given as parameter in <init_blockarray>. */
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
    * The log2 of <elements_per_block> plus 1 or value 0.
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

/* define: blockarray_FREE
 * Static initializer. */
#define blockarray_FREE { 0, 0, 0, 0, 0, 0, 0 }

/* function: init_blockarray
 * Initializes barray to use blocks of memory of size pagesize (see <pagesize_e>).
 * Also one datablock is preallocated for elements beginning from index 0. */
int init_blockarray(/*out*/blockarray_t * barray, uint8_t pagesize, uint16_t elementsize) ;

/* function: free_blockarray
 * Frees all memory blocks. All elements become invalid
 * so make sure that all references to them have been cleared. */
int free_blockarray(blockarray_t * barray) ;

// group: query

/* function: isfree_blockarray
 * Returns true if barray equals <blockarray_FREE>. */
bool isfree_blockarray(const blockarray_t * barray) ;

// group: read

/* function: at_blockarray
 * Returns the memory address of element at position arrayindex.
 * If the element was not assigned previoulsy the returned address
 * is either NULL or points to an element initialized to 0. */
void * at_blockarray(blockarray_t * barray, size_t arrayindex) ;

// group: update

/* function: assign_blockarray
 * Assigns memory to an element at position arrayindex and returns its address in elemaddr.
 * The memory address is aligned to the size of the element. The memory is allocated
 * with help of <pagecache_maincontext> (see <pagecache_t>).
 * If is_allocate is set to false the function works like <at_blockarray>.
 * Possible errors are ENOMEM or EINVAL if something went wrong internally. */
int assign_blockarray(blockarray_t * barray, size_t arrayindex, /*out*/void ** elemaddr) ;

// group: internal

/* function: assign2_blockarray
 * Implements <at_blockarray> and <assign_blockarray>.
 * If is_allocate is set to false the function works like <at_blockarray>
 * else it works like <assign_blockarray>. */
int assign2_blockarray(blockarray_t * barray, size_t arrayindex, bool is_allocate, /*out*/void ** elemaddr) ;

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
#define at_blockarray(barray, arrayindex)    \
         ( __extension__ ({                  \
            void * elemaddr = 0 ;            \
            assign2_blockarray(              \
                  (barray), (arrayindex),    \
                  false, &elemaddr) ;        \
            elemaddr ;                       \
         }))

/* define: assign_blockarray
 * Implements <blockarray_t.assign_blockarray>. */
#define assign_blockarray(barray, arrayindex, elemaddr)  \
         (assign2_blockarray(barray, arrayindex, true, elemaddr))


/* define: blockarray_IMPLEMENT
 * Implements <blockarray_t.blockarray_IMPLEMENT>. */
#define blockarray_IMPLEMENT(_fsuffix, object_t)   \
         static inline int init##_fsuffix(/*out*/blockarray_t * barray, pagesize_e pagesize) __attribute__ ((always_inline)) ; \
         static inline int free##_fsuffix(blockarray_t * barray) __attribute__ ((always_inline)) ; \
         static inline object_t * at##_fsuffix(blockarray_t * barray, size_t arrayindex) __attribute__ ((always_inline)) ; \
         static inline int assign##_fsuffix(blockarray_t * barray, size_t arrayindex, /*out*/object_t ** elemaddr) __attribute__ ((always_inline)) ; \
         static inline int init##_fsuffix(/*out*/blockarray_t * barray, pagesize_e pagesize) { \
            return init_blockarray(barray, pagesize, sizeof(object_t)) ; \
         } \
         static inline int free##_fsuffix(blockarray_t * barray) { \
            return free_blockarray(barray) ; \
         } \
         static inline object_t * at##_fsuffix(blockarray_t * barray, size_t arrayindex) { \
            return (object_t*)at_blockarray(barray, arrayindex) ; \
         } \
         static inline int assign##_fsuffix(blockarray_t * barray, size_t arrayindex, /*out*/object_t ** elemaddr) { \
            return assign_blockarray(barray, arrayindex, (void**)elemaddr) ; \
         }


#endif
