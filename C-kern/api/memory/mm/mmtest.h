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
struct test_errortimer_t ;

/* typedef: struct mmtest_t
 * Exports <mmtest_t>. */
typedef struct mmtest_t                mmtest_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_manager_mmtest
 * Test transient memory manager <mmtest_t>. */
int unittest_memory_manager_mmtest(void) ;
#endif


/* struct: mmtest_t
 * Test memory manager for allocating/freeing transient memory. */
struct mmtest_t {
   struct mmtest_page_t       * mmpage ;
   size_t                     sizeallocated ;
   struct test_errortimer_t   * simulateResizeError ;
   struct test_errortimer_t   * simulateFreeError ;
} ;

// group: context

/* function: mmcontext_mmtest
 * Returns the installed <mmtest_t> memory manager or 0.
 * If no memory amanger of type <mmtest_t> is installed by a call to <switchon_mmtest>
 * the value 0 is returned. */
mmtest_t * mmcontext_mmtest(void) ;

/* function: switchon_mmtest
 * Stores current memory manager of <threadcontext_t> and installs <mmtest_t>. */
int switchon_mmtest(void) ;

/* function: switchoff_mmtest
 * Restores memory manager of <threadcontext_t>.
 * The test memory manager in use (<mmtest_t>) is freed and the memory manager in <threadcontext_t>
 * is restored to one which was in used before <switchon_mmtest> was called. */
int switchoff_mmtest(void) ;

// group: lifetime

/* define: mmtest_INIT_FREEABLE
 * Static initializer. */
#define mmtest_INIT_FREEABLE           { 0, 0, 0, 0 }

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
 * This function is called from <switchon_mmtest>. */
int initiot_mmtest(/*out*/mm_iot * mmtest) ;

/* function: freeiot_mmtest
 * Calls <free_mmtest> with object pointer from <mm_iot>.
 * This function is called from <switchoff_mmtest>. */
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

// group: simulation

/* function: setresizeerr_mmtest
 * Sets an error timer for <mresize_mmtest>.
 * If *errtimer* is initialized with a timout of X > 0 the Xth call to
 * <mresize_mmtest> returns the error value of <process_testerrortimer>.
 * Only a reference is stored so do not delete *errtimer* until it has fired.
 * After the timer has fired the reference is set to 0. */
void setresizeerr_mmtest(mmtest_t * mman, struct test_errortimer_t * errtimer) ;

/* function: setfreeerr_mmtest
 * Sets an error timer for <mfree_mmtest>.
 * If *errtimer* is initialized with a timout of X > 0 the Xth call to
 * <mfree_mmtest> returns the error value of <process_testerrortimer>.
 * Only a reference is stored so do not delete *errtimer* until it has fired.
 * After the timer has fired the reference is set to 0. */
void setfreeerr_mmtest(mmtest_t * mman, struct test_errortimer_t * errtimer) ;


#endif
