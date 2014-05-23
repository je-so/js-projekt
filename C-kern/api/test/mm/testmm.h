/* title: Test-MemoryManager

   Offers interface for allocating & freeing blocks of memory.
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

   file: C-kern/api/test/mm/testmm.h
    Header file <Test-MemoryManager>.

   file: C-kern/test/mm/testmm.c
    Implementation file <Test-MemoryManager impl>.
*/
#ifndef CKERN_TEST_MM_TESTMM_HEADER
#define CKERN_TEST_MM_TESTMM_HEADER

// forward
struct memblock_t ;
struct mm_t ;
struct testmm_page_t ;
struct test_errortimer_t ;

/* typedef: struct testmm_t
 * Exports <testmm_t>. */
typedef struct testmm_t                   testmm_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_mm_testmm
 * Test <testmm_t> - memory manager for tests. */
int unittest_test_mm_testmm(void) ;
#endif


/* struct: testmm_t
 * Test memory manager for allocating/freeing blocks of memory.
 *
 * - Call <switchon_testmm> to install it.
 * - Call <switchoff_testmm> to restore normal memory manager. */
struct testmm_t {
   struct testmm_page_t       * mmpage ;
   size_t                     sizeallocated ;
} ;

// group: context

/* function: isinstalled_testmm
 * Returns true if test memory manager is installed. */
bool isinstalled_testmm(void) ;

/* function: mmcontext_testmm
 * Returns the installed <testmm_t> memory manager or 0.
 * If no memory manager of type <testmm_t> was installed with a previous call to <switchon_testmm>
 * the value 0 is returned. */
testmm_t * mmcontext_testmm(void) ;

/* function: switchon_testmm
 * Stores current memory manager of <threadcontext_t> and installs <testmm_t>. */
int switchon_testmm(void) ;

/* function: switchoff_testmm
 * Restores memory manager of <threadcontext_t>.
 * The test memory manager in use (<testmm_t>) is freed and the memory manager in <threadcontext_t>
 * is restored to one which was in used before <switchon_testmm> was called. */
int switchoff_testmm(void) ;

// group: lifetime

/* define: testmm_FREE
 * Static initializer. */
#define testmm_FREE { 0, 0 }

/* function: init_testmm
 * Initializes a new test memory manager. */
int init_testmm(/*out*/testmm_t * mman) ;

/* function: free_testmm
 * Frees all memory managed by this manager.
 * Before freeing it make sure that every object allocated on
 * this memory heap is no more reachable or already freed. */
int free_testmm(testmm_t * mman) ;

/* function: initasmm_testmm
 * Calls <init_testmm> and wraps object into interface object <mm_t>.
 * This function is called from <switchon_testmm>. */
int initasmm_testmm(/*out*/struct mm_t * testmm) ;

/* function: freeasmm_testmm
 * Calls <free_testmm> with object pointer from <mm_t>.
 * This function is called from <switchoff_testmm>. */
int freeasmm_testmm(struct mm_t * testmm) ;

// group: query

/* function: sizeallocated_testmm
 * Returns the size in bytes of all allocated memory blocks.
 * If this value is 0 no memory is allocated on this heap. */
size_t sizeallocated_testmm(testmm_t * mman) ;

// group: allocate

/* function: malloc_testmm
 * Allocates a new memory block.
 * Test implementation replacement of <malloc_mmimpl>. */
int malloc_testmm(testmm_t * mman, size_t size, /*out*/struct memblock_t * memblock) ;

/* function: mresize_testmm
 * Allocates new or resizes already allocated memory block.
 * Test implementation replacement of <mresize_mmimpl>. */
int mresize_testmm(testmm_t * mman, size_t newsize, struct memblock_t * memblock) ;

/* function: mfree_testmm
 * Frees the memory of an allocated memory block.
 * Test implementation replacement of <mfree_mmimpl>. */
int mfree_testmm(testmm_t  * mman, struct memblock_t * memblock) ;



// section: inline implementation

/* define: isinstalled_testmm
 * Implements <testmm_t.isinstalled_testmm>. */
#define isinstalled_testmm()           (0 != mmcontext_testmm())

#endif
