/* title: DefaultMemoryManager

   Offers interface for allocating & freeing blocks of memory.

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
   int todo__implement_without_malloc__ ;
} ;

// group: initthread

/* function: interface_mmimpl
 * This function is called from <init_threadcontext>.
 * Used to initialize interface of <mm_t>. */
struct mm_it * interface_mmimpl(void) ;

// group: lifetime

/* define: mmimpl_INIT_FREEABLE
 * Static initializer. */
#define mmimpl_INIT_FREEABLE              { 0 }

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

/* function: mresize_mmimpl
 * Allocates new memory or resizes already allocated memory.
 * Allocation is a special case in that it is a resize operation
 * with the size and addr of parameter <memblock_t> set to 0.
 *
 * Before calling this function function make sure that memblock is either
 * set to <memblock_INIT_FREEABLE> or to a value returned by a previous call
 * of this function.
 *
 * On successful return the field <memblock_t.size> can be set to a larger value than
 * provided in newsize to allow the memory manager for some internal optimization.
 *
 * To free a memory block it is enough to call <mfree_mmimpl> with its size field
 * set to the same value as newsize has had in the last call to <mresize_mmimpl>
 * or to the value returned by this call. The address field in memblock is not allowed
 * to be set to any other value. */
int mresize_mmimpl(mm_impl_t * mman, size_t newsize, struct memblock_t * memblock) ;

/* function: mfree_mmimpl
 * Frees the memory of an allocated memory block. After return
 * memblock is set to <memblock_INIT_FREEABLE>. This ensured that calling
 * this function twice with the same argument is a no op. */
int mfree_mmimpl(mm_impl_t * mman, struct memblock_t * memblock) ;


#endif
