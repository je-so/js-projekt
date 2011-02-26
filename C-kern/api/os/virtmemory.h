/* title: VirtualMemory
   Manages mapping of virtual memory pages.

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
   (C) 2010 Jörg Seebohn

   file: C-kern/api/os/virtmemory.h
    Header file of <VirtualMemory>.

   file: C-kern/os/Linux/virtmemory.c
    Linux specific implementation of <VirtualMemory>.
*/
#ifndef CKERN_OS_VIRTUALMEMORY_HEADER
#define CKERN_OS_VIRTUALMEMORY_HEADER

/* typedef: typedef virtmemory_block_t
 * Shortcut for <virtmemory_block_t>. */
typedef struct virtmemory_block_t   virtmemory_block_t ;
/* typedef: typedef virtmemory_region_t
 * Shortcut for <virtmemory_region_t>. */
typedef struct virtmemory_region_t virtmemory_region_t ;
/* typedef: typedef virtmemory_mappedregions_t
 * Shortcut for <virtmemory_mappedregions_t>. */
typedef struct virtmemory_mappedregions_t virtmemory_mappedregions_t ;
/* typedef: typedef virtmemory_regionsarray_t
 * Internal type used to implement <virtmemory_mappedregions_t>. */
typedef struct virtmemory_regionsarray_t virtmemory_regionsarray_t ;


// section: Functions


// group: query
/* function: pagesize_virtmemory
 * Returns the virtual memory page size supported by the underlying system. */
extern size_t pagesize_virtmemory(void) ;


// group: change
/* function: map_virtmemory
 * New memory is mapped into the virtual address space of the calling process.
 * The new memory has size_in_bytes == size_in_pages * <pagesize_virtmemory>. */
extern int map_virtmemory( /*out*/virtmemory_block_t * new_mapped_block, size_t size_in_pages ) ;
/* function: extend_virtmemory
 * Tries to grow the upper bound of an already mapped address range.
 * Returns 0 on success else ENOMEM or another system specific error code.
 * In case of success the new address range is [ext_block->start .. ext_block->start + ext_block->size_in_byte + increment_in_pages * <pagesize_virtmemory>).
 * In case of error nothing is changed. You can use this function to test if an address range after ext_block is already mapped. */
extern int extend_virtmemory( virtmemory_block_t * ext_block, size_t increment_in_pages) ;
/* function: maporextend_virtmemory
 * Tries to grow the upper bound of an already mapped address range or to choose a new location.
 * Returns 0 on success else ENOMEM or another system specific error code.
 *
 * In case of success and new_mapped_block->start is != 0:
 * The new mapped address range is
 * > new_mapped_block->start[0 .. size_in_pages * pagesize_virtmemory() -1 ]
 *
 * In case of success and new_mapped_block->start is == 0:
 * The ext_block is extended into
 * > ext_block->start[0 .. ext_block->size_in_bytes -1 ]
 * Where
 * > ext_block->size_in_bytes == old (ext_block->size_in_bytes) + size_in_pages * pagesize_virtmemory() */
extern int maporextend_virtmemory( /*out*/virtmemory_block_t * new_mapped_block, size_t size_in_pages, virtmemory_block_t * ext_block) ;
/* function: unmap_virtmemory
 * Invalidates virtual memory address range
 * > mapped_block->start[0 .. mapped_block->size_in_bytes - 1 ]
 * After successull return every access to this memory range will generate a memory exception and
 * mapped_block is set to <virtmemory_block_t.virtmemory_block_INIT_FREEABLE>.
 * Therefore unmapping an already unmapped memory region does nothing and returns success. */
extern int unmap_virtmemory( virtmemory_block_t * mapped_block ) ;


// group: test
#ifdef KONFIG_UNITTEST
/* function: unittest_os_virtualmemory
 * Unittest for virtual memory module. */
extern int unittest_os_virtualmemory(void) ;
#endif

/* struct: virtmemory_block_t
 * Describes a virtual memory block mapped in the address space of the running process. */
struct virtmemory_block_t
{
   // group: variables
   /* variable: start
    * Points to lowest address of mapped memory. */
   uint8_t   * start ;
   /* variable: size_in_bytes
    * Size of mapped memory. The size is a multiple of <pagesize_virtmemory>.
    * The valid memory region is
    * > start[ 0 .. size_in_bytes - 1 ] */
   size_t      size_in_bytes ;
} ;

// group: lifetime

/* define: virtmemory_block_INIT_FREEABLE
 * Static initializer to set an object of type <virtmemory_block_t> to NULL.
 * You can unmap (<unmap_virtmemory>) such an initialized <virtmemory_block_t> object with success. */
#define virtmemory_block_INIT_FREEABLE  { NULL, 0 }


/* struct: virtmemory_region_t
 * Returns information about a mapped memory region and its access permissions. */
struct virtmemory_region_t
{
   /* variable: start
    * Start address of mapping. */
   void * start ;
   /* variable: end
    * End address of mapping. It points to the address after the last mapped byte.
    * Therefore the length in pages can be calculated as:
    * > (end - start) / pagesize_virtmemory() */
   void * end ;
   /* variable: isReadable
    * True if memory can be read. */
   bool isReadable ;
   /* variable: isWriteable
    * True if memory can be written to. */
   bool isWriteable ;
   /* variable: isExecutable
    * True if memory contains executable machine code. */
   bool isExecutable ;
   /* variable: isShared
    * True if memory is shared with another process. */
   bool isShared ;
} ;

// group: query
/* function: compare_virtmemory_region
 * Returns 0 if left and right regions compare equal. */
extern int compare_virtmemory_region( const virtmemory_region_t * left, const virtmemory_region_t * right ) ;


/* struct: virtmemory_mappedregions_t
 * Buffer which stores a snapshot of all mapped memory regions.
 * Use <init_virtmemory_mappedregions> to store a snapshot of the current mapping.
 * Do not forget to free (<free_virtmemory_mappedregions>) after using.
 * To access individual mapping descriptions of type <virtmemory_region_t> use
 * <next_virtmemory_mappedregions>. With <gofirst_virtmemory_mappedregions> you can reset
 * the internal iterator and scan from the beginning again. */
struct virtmemory_mappedregions_t
{
   // group: private variables
   /* var: total_count
    * Number of stored elements of type <virtmemory_region_t>. */
   size_t               total_count ;
   /* var: element_count
    * Number of elements <element_iterator> can access in sequence.
    * Used to implement internal iterator. */
   size_t               element_count ;
   /* var: element_iterator
    * Points to an array of <virtmemory_region_t> of size <element_count>.
    * Used to implement internal iterator. */
   virtmemory_region_t        * element_iterator ;
   /* var: array_iterator
    * Points to next array of <virtmemory_region_t> which comes after array <element_iterator> points to.
    * Used to implement internal iterator. */
   virtmemory_regionsarray_t  * array_iterator ;
   /* var: first_array
    * Points to first array of <virtmemory_region_t>.
    * The memory is organized as a list of arrays of elements of type <virtmemory_region_t>.
    * Used to implement internal iterator and to free memory. */
   virtmemory_regionsarray_t  * first_array ;
} ;

// group: lifetime
/* define: virtmemory_mappedregions_INIT_FREEABLE
 * Static initializer: Makes calling <free_virtmemory_mappedregions> safe. */
#define virtmemory_mappedregions_INIT_FREEABLE  { 0, 0, 0, 0, 0 }

/* function: init_virtmemory_mappedregions
 * Returns in mappedregions the descriptions of all current virtual memory mappings.
 * The first skip_offset descriptions are ignored and up to max_size are returned.
 * The number of valid entries in result_array is returned in result_size.
 * result_size is set to 0 if there are no more than skip_offset mappings.
 * See <virtmemory_mapping_t> for a description of the returned information. */
extern int init_virtmemory_mappedregions( /*out*/virtmemory_mappedregions_t * mappedregions ) ;

/* function: free_virtmemory_mappedregions
 * Returns the size of the array needed to store all <virtmemory_mapping_t> with a single call of <querymapping_virtmemory>.
 * It is possible that this value changes before the next call to <querymapping_virtmemory> if memory is allocated
 * during a library call. */
extern int free_virtmemory_mappedregions( virtmemory_mappedregions_t * mappedregions ) ;

// group: query
/* function: size_virtmemory_mappedregions
 * Returns the total number of contained <virtmemory_region_t>. */
extern size_t size_virtmemory_mappedregions( const virtmemory_mappedregions_t * mappedregions ) ;

/* function: compare_virtmemory_region
 * Returns 0 if left and right regions compare equal. */
extern int compare_virtmemory_mappedregions( const virtmemory_mappedregions_t * left, const virtmemory_mappedregions_t * right ) ;

// group: iterate
/* function: gofirst_virtmemory_mappedregions
 * Resets iterator to the first element.
 * The next call to <next_virtmemory_mappedregions> returns the first contained element.
 * See generic <ArrayList Iterator> for a more in deep description. */
extern void gofirst_virtmemory_mappedregions( virtmemory_mappedregions_t * iterator ) ;

/* function: next_virtmemory_mappedregions
 * Returns the next <virtmemory_region_t> in the set of all stored elements.
 * NULL is returned if there is no next element.
 * See generic <ArrayList Iterator> for a more in deep description. */
extern const virtmemory_region_t * next_virtmemory_mappedregions( virtmemory_mappedregions_t * iterator ) ;


// section: inline implementation


/* define: inline pagesize_virtmemory
 * Uses sysconf(_SC_PAGESIZE) which conforms to POSIX.1-2001. */
#define /*size_t*/ pagesize_virtmemory() \
   ((size_t)(sysconf(_SC_PAGESIZE)))

/* define: size_virtmemory_mappedregions
 * Returns <virtmemory_mappedregions_t->total_count>.
 * Inline implementation of <virtmemory_mappedregions_t.size_virtmemory_mappedregions>. */
#define /*size_t*/ size_virtmemory_mappedregions( /*const virtmemory_mappedregions_t * */mappedregions ) \
   ((mappedregions)->total_count)

#endif
