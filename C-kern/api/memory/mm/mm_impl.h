/* title: DefaultMemoryManager

   Default implementation of <mm_it>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/memory/mm/mm_impl.h
    Header file <DefaultMemoryManager>.

   file: C-kern/memory/mm/mm_impl.c
    Implementation file <DefaultMemoryManager impl>.
*/
#ifndef CKERN_MEMORY_MM_MMIMPL_HEADER
#define CKERN_MEMORY_MM_MMIMPL_HEADER

// forward
struct memblock_t ;

/* typedef: struct mm_impl_t
 * Exports <mm_impl_t>. */
typedef struct mm_impl_t                  mm_impl_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_mm_mmimpl
 * Test default memory manager <mm_impl_t>. */
int unittest_memory_mm_mmimpl(void) ;
#endif


/* struct: mm_impl_t
 * Default memory manager for allocating/freeing blocks of memory. */
struct mm_impl_t {
   size_t size_allocated ;
} ;

// group: initthread

/* function: interface_mmimpl
 * This function is called from <threadcontext_t.init_threadcontext>.
 * Used to initialize interface of <mm_t>. */
struct mm_it * interface_mmimpl(void) ;

// group: lifetime

/* define: mmimpl_FREE
 * Static initializer. */
#define mmimpl_FREE { 0 }

/* function: init_mmimpl
 * Initializes a new memory manager. */
int init_mmimpl(/*out*/mm_impl_t * mman) ;

/* function: free_mmimpl
 * Frees all memory managed by this manager.
 * Beofre freeing it make sure that every object allocated on
 * this memory heap is no more reachable or already freed. */
int free_mmimpl(mm_impl_t * mman) ;

// group: query

/* function: sizeallocated_mmimpl
 * Returns the size in bytes of all allocated memory blocks.
 * If this value is 0 no memory is allocated on this heap. */
size_t sizeallocated_mmimpl(mm_impl_t * mman) ;

// group: allocate

/* function: malloc_mmimpl
 * Allocates new memory block.
 *
 * On successful return the field <memblock_t.size> can be set to a larger value than
 * provided in size to allow the memory manager for some internal optimization.
 * To free a memory block it is enough to call <mfree_mmimpl> with the returned memblock.
 * It is allowed to set <memblock_t.size> to the same value as provided in argument size.
 *
 * */
int malloc_mmimpl(mm_impl_t * mman, size_t size, /*out*/struct memblock_t * memblock) ;

/* function: mresize_mmimpl
 * Allocates new memory or resizes already allocated memory.
 * Allocation is a special case in that it is a resize operation
 * with the size and addr of parameter <memblock_t> set to 0.
 *
 * Before calling this function make sure that memblock is either
 * set to <memblock_FREE> or to a value returned by a previous call
 * to <malloc_mmimpl> or this function.
 *
 * On successful return the field <memblock_t.size> can be set to a larger value than
 * provided in newsize to allow the memory manager for some internal optimization.
 * To free a memory block it is enough to call <mfree_mmimpl> with the returned memblock.
 * It is allowed to set <memblock_t.size> to the same value as provided in argument newsize.
 *
 */
int mresize_mmimpl(mm_impl_t * mman, size_t newsize, struct memblock_t * memblock) ;

/* function: mfree_mmimpl
 * Frees the memory of an allocated memory block. After return
 * memblock is set to <memblock_FREE>. This ensured that calling
 * this function twice with the same argument is a no op. */
int mfree_mmimpl(mm_impl_t * mman, struct memblock_t * memblock) ;


#endif
