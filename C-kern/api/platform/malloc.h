/* title: Malloc
   Offers an interface to check for system memory allocated
   with malloc and co.

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

   file: C-kern/api/platform/malloc.h
    Header file of <Malloc>.

   file: C-kern/platform/Linux/malloc.c
    Implementation file of <Malloc impl>.
*/
#ifndef CKERN_PLATFORM_MALLOC_HEADER
#define CKERN_PLATFORM_MALLOC_HEADER


// section: Functions

// group: init

/* function: prepare_malloc
 * Inits test of system-malloc. The GNU C library offers
 * a query function for information about malloced memory
 * so it is not necessary to wrap the malloc function
 * with a test implementation.
 * For that reason this function does only two steps:
 * 1. Call system functions which
 *    allocate internal memory (strerror)
 *    which is never freed.
 * 2. Free all allocated but unused memory.
 *    Same as <trimmemory_malloc>. */
int prepare_malloc(void) ;

// group: manage

/* function: trimmemory_malloc
 * Frees preallocated memory which is not in use.
 * Therefore unused heap memory pages are unmapped from virtual memory.
 * This function is useful if you want to compare
 * the layout of all virtual mapped memory pages at the beginning of
 * the test with the layout at the end of the test.
 * If a lot of memory is allocated and freed during the execution
 * of your test it is possible that additional system pages
 * will be mapped into the heap address space.
 * The call to <trimmemory_malloc> unmaps them and
 * makes therefore the layouts comparable. */
int trimmemory_malloc(void) ;

// group: query

/* function: allocatedsize_malloc
 * Returns allocated number of bytes which are not freed.
 * It is not necessary to call <trimmemory_malloc>
 * before calling this function.
 * If you do not call <prepare_malloc> before this function
 * then it is called for you if this is necessary.
 * At the start of your test save the number of allocated bytes
 * of system memory returned from <allocatedsize_malloc>.
 * At the end of your test call it a second time
 * and compare the result with the value received from the first call
 * to make sure no system memory is wasted. */
int allocatedsize_malloc(/*out*/size_t * number_of_allocated_bytes) ;

/* function: sizeusable_malloc
 * Returns number of usable bytes in the allocated memory block addr.
 * The parameter addr must the value returned by a call to malloc.
 * The returned value is equal or greater than parameter size in used in call to malloc.
 * The value 0 is returned in case addr is 0. */
size_t sizeusable_malloc(void * addr) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_malloc
 * Unittest for query usage of malloc resources. */
int unittest_platform_malloc(void) ;
#endif



// section: inline implementation

/* define: sizeusable_malloc
 * Implements <sizeusable_malloc>. */
#define sizeusable_malloc(addr)           (malloc_usable_size(addr))


#endif
