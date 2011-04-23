/* title: Malloc-Test
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

   file: C-kern/api/test/malloctest.h
    Header file of <Malloc-Test>.

   file: C-kern/os/shared/malloctest.c
    Implementation file of <Malloc-Test impl>.
*/
#ifndef CKERN_TEST_MALLOCTEST_HEADER
#define CKERN_TEST_MALLOCTEST_HEADER

// section: Test

// group: malloc

/* function: prepare_malloctest
 * Inits test of system-malloc. The GNU C library offers
 * a query function for information about malloced memory
 * so it is not necessary to wrap the malloc function
 * with a test implementation.
 * For that reason this function does only two steps:
 * 1. Call system functions which
 *    allocate internal memory (strerror)
 *    which is never freed.
 * 2. Free all allocated but unused memory.
 *    Same as <trimmemory_malloctest>. */
extern void prepare_malloctest(void) ;

/* function: trimmemory_malloctest
 * Frees preallocated memory which is not in use.
 * Therefore unused heap memory pages are unmapped from virtual memory.
 * The function trimmemory_malloctest is useful if you want to compare
 * the layout of all virtual mapped memory pages at the beginning of
 * the test with the layout at the end of the test.
 * If a lot of memory is allocated and freed during the execution
 * of your test it is possible that additional system pages
 * will be mapped into the heap address space.
 * The call to <trimmemory_malloctest> unmaps them and
 * makes therefore the layouts comparable. */
extern void trimmemory_malloctest(void) ;

/* function: allocatedsize_malloctest
 * Returns allocated number of bytes which are not freed.
 * It is not necessary to call <trimmemory_malloctest>
 * before calling this function.
 * If you do not call <prepare_malloctest> before this function
 * then it is called for you if this is necessary.
 * At the start of your test save the number of allocated bytes
 * of system memory returned from <allocatedsize_malloctest>.
 * At the end of your test call <allocatedsize_malloctest> a second time
 * and compare the result with the value received from the first call
 * to make sure no system memory is wasted. */
extern size_t allocatedsize_malloctest(void) ;


#endif
