/* title: Test-MemoryManager

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

   file: C-kern/api/test/testmm.h
    Header file <Test-MemoryManager>.

   file: C-kern/test/testmm.c
    Implementation file <Test-MemoryManager impl>.
*/
#ifndef CKERN_TEST_TESTMM_HEADER
#define CKERN_TEST_TESTMM_HEADER

// forward
struct memblock_t ;
struct testmm_page_t ;
struct test_errortimer_t ;

/* typedef: struct testmm_t
 * Exports <testmm_t>. */
typedef struct testmm_t                testmm_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_testmm
 * Test <testmm_t> - a test memory manager for transient memory. */
int unittest_test_testmm(void) ;
#endif


/* struct: testmm_t
 * Test memory manager for allocating/freeing transient memory.
 *
 * - Call <switchon_testmm> to install it.
 * - Call <switchoff_testmm> to restore normal memory manager. */
struct testmm_t {
   struct testmm_page_t       * mmpage ;
   size_t                     sizeallocated ;
   struct test_errortimer_t   * simulateResizeError ;
   struct test_errortimer_t   * simulateFreeError ;
} ;

// group: context

/* function: mmcontext_testmm
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

/* define: testmm_INIT_FREEABLE
 * Static initializer. */
#define testmm_INIT_FREEABLE           { 0, 0, 0, 0 }

/* function: init_testmm
 * Initializes a new memory manager for transient memory. */
int init_testmm(/*out*/testmm_t * mman) ;

/* function: free_testmm
 * Frees all memory managed by this manager.
 * Beofre freeing it make sure that every object allocated on
 * this memory heap is no more reachable or already freed. */
int free_testmm(testmm_t * mman) ;

/* function: initiot_testmm
 * Calls <init_testmm> and wraps object into interface object <mm_iot>.
 * This function is called from <switchon_testmm>. */
int initiot_testmm(/*out*/mm_iot * testmm) ;

/* function: freeiot_testmm
 * Calls <free_testmm> with object pointer from <mm_iot>.
 * This function is called from <switchoff_testmm>. */
int freeiot_testmm(mm_iot * testmm) ;

// group: query

/* function: sizeallocated_testmm
 * Returns the size in bytes of all allocated memory blocks.
 * If this value is 0 no memory is allocated on this heap. */
size_t sizeallocated_testmm(testmm_t * mman) ;

// group: allocate

/* function: mresize_testmm
 * Allocates new memory or resizes already allocated memory.
 * Test implementation replacement of <mresize_mmtransient>. */
int mresize_testmm(testmm_t * mman, size_t newsize, struct memblock_t * memblock) ;

/* function: mfree_testmm
 * Frees the memory of an allocated memory block.
 * Test implementation replacement of <mfree_mmtransient>. */
int mfree_testmm(testmm_t  * mman, struct memblock_t * memblock) ;

// group: simulation

/* function: setresizeerr_testmm
 * Sets an error timer for <mresize_testmm>.
 * If *errtimer* is initialized with a timout of X > 0 the Xth call to
 * <mresize_testmm> returns the error value of <process_testerrortimer>.
 * Only a reference is stored so do not delete *errtimer* until it has fired.
 * After the timer has fired the reference is set to 0. */
void setresizeerr_testmm(testmm_t * mman, struct test_errortimer_t * errtimer) ;

/* function: setfreeerr_testmm
 * Sets an error timer for <mfree_testmm>.
 * If *errtimer* is initialized with a timout of X > 0 the Xth call to
 * <mfree_testmm> returns the error value of <process_testerrortimer>.
 * Only a reference is stored so do not delete *errtimer* until it has fired.
 * After the timer has fired the reference is set to 0. */
void setfreeerr_testmm(testmm_t * mman, struct test_errortimer_t * errtimer) ;


// section: inline implementation

/* define: isinstalled_testmm
 * Implements <testmm_t.isinstalled_testmm>. */
#define isinstalled_testmm()           (0 != mmcontext_testmm())

#endif