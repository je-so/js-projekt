/* title: TestMemoryManager
   Offers interface for allocating & freeing transient memory.
   This is a test memory manager which checks for writing beyond the allocated memory block.
   It is used during the execution of unit tests.

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

   file: C-kern/api/memory/mm/mmtest.h
    Header file <TestMemoryManager>.

   file: C-kern/memory/mm/mmtest.c
    Implementation file <TestMemoryManager impl>.
*/
#ifndef CKERN_MEMORY_MM_MMTEST_HEADER
#define CKERN_MEMORY_MM_MMTEST_HEADER

// forward
struct memblock_t ;
struct mmtest_page_t ;

/* typedef: struct mmtest_t
 * Exports <mmtest_t>. */
typedef struct mmtest_t                mmtest_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_manager_test
 * Test transient memory manager <mmtest_t>. */
int unittest_memory_manager_test(void) ;
#endif


/* struct: mmtest_t
 * Test memory manager for allocating/freeing transient memory. */
struct mmtest_t {
   struct mmtest_page_t * mmpage ;
   size_t               sizeallocated ;
} ;

// group: context

/* function: switchteston_mmtest
 * Stores current memory manager of <threadcontext_t> and installs <mmtest_t>. */
int switchteston_mmtest(void) ;

/* function: switchtestoff_mmtest
 * Restores memory manager of <threadcontext_t>.
 * The test memory manager in use (<mmtest_t>) is freed and the memory manager in <threadcontext_t>
 * is restored to one which was in used before <switchteston_mmtest> was called. */
int switchtestoff_mmtest(void) ;

// group: lifetime

/* define: mmtest_INIT_FREEABLE
 * Static initializer. */
#define mmtest_INIT_FREEABLE           { 0, 0 }

/* function: init_mmtest
 * Initializes a new memory manager for transient memory. */
int init_mmtest(/*out*/mmtest_t * mman) ;

/* function: free_mmtest
 * Frees all memory managed by this manager.
 * Beofre freeing it make sure that every object allocated on
 * this memory heap is no more reachable or already freed. */
int free_mmtest(mmtest_t * mman) ;

/* function: initiot_mmtest
 * Calls <init_mmtest> and wraps object into interface object <mm_iot>.
 * This function is called from <switchteston_mmtest>. */
int initiot_mmtest(/*out*/mm_iot * mmtest) ;

/* function: freeiot_mmtest
 * Calls <free_mmtest> with object pointer from <mm_iot>.
 * This function is called from <switchtestoff_mmtest>. */
int freeiot_mmtest(mm_iot * mmtest) ;

// group: query

/* function: sizeallocated_mmtest
 * Returns the size in bytes of all allocated memory blocks.
 * If this value is 0 no memory is allocated on this heap. */
size_t sizeallocated_mmtest(mmtest_t * mman) ;

// group: allocate

/* function: mresize_mmtest
 * Allocates new memory or resizes already allocated memory.
 * Test implementation replacement of <mresize_mmtransient>. */
int mresize_mmtest(mmtest_t * mman, size_t newsize, struct memblock_t * memblock) ;

/* function: mfree_mmtest
 * Frees the memory of an allocated memory block.
 * Test implementation replacement of <mfree_mmtransient>. */
int mfree_mmtest(mmtest_t  * mman, struct memblock_t * memblock) ;


#endif
