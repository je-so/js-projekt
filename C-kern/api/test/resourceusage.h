/* title: Resourceusage
   Test that all resource are properly released.

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

   file: C-kern/api/test/resourceusage.h
    Header file of <Resourceusage>.

   file: C-kern/test/resourceusage.c
    Implementation file <Resourceusage impl>.
*/
#ifndef CKERN_TEST_RESOURCEUSAGE_HEADER
#define CKERN_TEST_RESOURCEUSAGE_HEADER

// forward
struct signalstate_t;
struct vm_mappedregions_t;


/* typedef: struct resourceusage_t
 * Exports <resourceusage_t>. */
typedef struct resourceusage_t            resourceusage_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_resourceusage
 * Unittest for query usage & releasing all resources. */
int unittest_test_resourceusage(void);
#endif


/* struct: resourceusage_t
 * Stores the number of resources currently in use. */
struct resourceusage_t {
   /* variable: file_usage
    * Number of open files. */
   size_t                        file_usage;
   /* variable: mmtrans_usage
    * Number of memory bytes allocated by <mm_impl_t>. */
   size_t                        mmtrans_usage;
   /* variable: mmtrans_correction
    * Number of bytes <resourceusage_t> uses itself. */
   size_t                        mmtrans_correction;
   /* variable: malloc_usage
    * Number of memory bytes allocated by malloc. */
   size_t                        malloc_usage;
   /* variable: malloc_correction
    * Number of bytes <resourceusage_t> uses itself. */
   size_t                        malloc_correction;
   /* variable: pagecache_usage
    * Sum of size of all cache memory pages. */
   size_t                        pagecache_usage;
   /* variable: pagecache_correction
    * Size of pages <resourceusage_t> uses itself. */
   size_t                        pagecache_correction;
   /* variable: pagecache_staticusage
    * Size of static memory allocated in <pagecache_t>. */
   size_t                        pagecache_staticusage;
   /* variable: signalstate
    * Stores configuration state of signal subsystem. */
   struct signalstate_t *        signalstate;
   /* variable: virtualmemory_usage
    * Layout of virtual memory. */
   struct vm_mappedregions_t *   virtualmemory_usage;
   /* variable: malloc_acceptleak
    * Holds maximum number of bytes of accepted malloc memory leak. */
   uint16_t                      malloc_acceptleak;
};

// group: lifetime

/* define: resourceusage_FREE
 * Static initializer. */
#define resourceusage_FREE { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

/* function: init_resourceusage
 * Stores the number of resources currently in use. */
int init_resourceusage(/*out*/resourceusage_t * usage);

/* function: free_resourceusage
 * Frees any memory which may be used to store the usage information. */
int free_resourceusage(resourceusage_t * usage);

// group: query

/* function: same_resourceusage
 * Returns 0 if the numbers of resources equals the numbers stored in usage.
 * If more or less resources are in use the error ELEAK is returned. */
int same_resourceusage(const resourceusage_t * usage);

// group: update

void acceptmallocleak_resourceusage(const resourceusage_t * usage, uint16_t malloc_leak_in_bytes);


// section: inline implementation

// group: resourceusage_t

/* define: acceptmallocleak_resourceusage
 * Implements <resourceusage_t.acceptmallocleak_resourceusage>. */
#define acceptmallocleak_resourceusage(usage, malloc_leak_in_bytes) \
         do { (usage)->malloc_acceptleak = (malloc_leak_in_bytes); } while (0)

#endif
