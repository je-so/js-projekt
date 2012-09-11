/* title: TransientMemoryManager
   Offers interface for allocating & freeing transient memory.

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

   file: C-kern/api/memory/mm/mmtransient.h
    Header file <TransientMemoryManager>.

   file: C-kern/memory/mm/mmtransient.c
    Implementation file <TransientMemoryManager impl>.
*/
#ifndef CKERN_MEMORY_MM_MMTRANSIENT_HEADER
#define CKERN_MEMORY_MM_MMTRANSIENT_HEADER

// forward
struct memblock_t ;

/* typedef: struct mmtransient_t
 * Exports <mmtransient_t>. */
typedef struct mmtransient_t           mmtransient_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_manager_transient
 * Test transient memory manager <mmtransient_t>. */
int unittest_memory_manager_transient(void) ;
#endif


/* struct: mmtransient_t
 * Memory manager for allocating/freeing transient memory. */
struct mmtransient_t {
   int todo__implement_without_malloc__ ;
} ;

// group: init

/* function: initthread_mmtransient
 * Calls <init_mmtransient> and wraps object into interface object <mm_iot>.
 * This function is called from <init_threadcontext>. */
int initthread_mmtransient(/*out*/mm_iot * mm_transient) ;

/* function: freethread_mmtransient
 * Calls <free_mmtransient> with object pointer from <mm_iot>.
 * This function is called from <free_threadcontext>. */
int freethread_mmtransient(mm_iot * mm_transient) ;

// group: lifetime

/* define: mmtransient_INIT_FREEABLE
 * Static initializer. */
#define mmtransient_INIT_FREEABLE      { 0 }

/* function: init_mmtransient
 * Initializes a new memory manager for transient memory. */
int init_mmtransient(/*out*/mmtransient_t * mman) ;

/* function: free_mmtransient
 * Frees all memory managed by this manager.
 * Beofre freeing it make sure that every object allocated on
 * this memory heap is no more reachable or already freed. */
int free_mmtransient(mmtransient_t * mman) ;

// group: query

/* function: sizeallocated_mmtransient
 * Returns the size in bytes of all allocated memory blocks.
 * If this value is 0 no memory is allocated on this heap. */
size_t sizeallocated_mmtransient(mmtransient_t * mman) ;

// group: allocate

/* function: mresize_mmtransient
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
 * To free a memory block it is enough to call <mfree_mmtransient> with its size field
 * set to the same value as newsize has had in the last call to <mresize_mmtransient>
 * or to the value returned by this call. The address field in memblock is not allowed
 * to be set to any other value. */
int mresize_mmtransient(mmtransient_t * mman, size_t newsize, struct memblock_t * memblock) ;

/* function: mfree_mmtransient
 * Frees the memory of an allocated memory block. After return
 * memblock is set to <memblock_INIT_FREEABLE>. This ensured that calling
 * this function twice with the same argument is a no op. */
int mfree_mmtransient(mmtransient_t * mman, struct memblock_t * memblock) ;


#endif
