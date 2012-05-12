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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/virtmemory.h
    Header file of <VirtualMemory>.

   file: C-kern/platform/Linux/virtmemory.c
    Linux specific implementation <VirtualMemory Linux>.
*/
#ifndef CKERN_PLATFORM_VIRTUALMEMORY_HEADER
#define CKERN_PLATFORM_VIRTUALMEMORY_HEADER

#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/io/accessmode.h"

/* typedef: struct vm_block_t
 * Exports <vm_block_t>. */
typedef struct memblock_t              vm_block_t ;

/* typedef: struct vm_region_t
 * Exports <vm_region_t>, describes a single virtual memory region. */
typedef struct vm_region_t             vm_region_t ;

/* typedef: struct vm_mappedregions_t
 * Exports <vm_mappedregions_t>, description of all mapped memory. */
typedef struct vm_mappedregions_t      vm_mappedregions_t ;

/* typedef: struct vm_regionsarray_t
 * Internal type used by <vm_mappedregions_t>. */
typedef struct vm_regionsarray_t       vm_regionsarray_t ;


// section: Functions

// group: query

/* function: pagesize_vm
 * Returns the virtual memory page size supported by the underlying system.
 * This function returns a cached value. */
size_t pagesize_vm(void) ;

/* function: sys_pagesize_vm
 * Returns the virtual memory page size supported by the underlying system.
 * This functions always calls the underlying system function. */
size_t sys_pagesize_vm(void) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_virtualmemory
 * Unittest for virtual memory module. */
int unittest_platform_virtualmemory(void) ;
#endif


/* struct: vm_block_t
 * It is a synonym for <memblock_t>.
 * The size of the mapped memory block is always
 * a multiple of <pagesize_vm>. */
struct vm_block_t ;

// group: lifetime

/* define: vm_block_INIT_FREEABLE
 * Static initializer. Sets object of type <vm_block_t> to NULL.
 * Unmapping (<free_vmblock>) such a NULL <vm_block_t> is safe. */
#define vm_block_INIT_FREEABLE         memblock_INIT_FREEABLE

/* function: init_vmblock
 * New memory is mapped into the virtual address space of the calling process.
 * The new memory has size == size_in_pages * <pagesize_vm>.
 * It is read and writeable and not shared between processes.
 * But a child process can access its content after a fork (COPY_ON_WRITE semantics). */
int init_vmblock(/*out*/vm_block_t * vmblock, size_t size_in_pages) ;

/* function: init_vmblock
 * New memory is mapped into the virtual address space of the calling process.
 * The new memory has size == size_in_pages * <pagesize_vm>.
 * It has accessible as stated in paramter *access_mode*.
 * A child process can access its content after a fork and a change is shared with the parent process
 * if <accessmode_SHARED> was specified. */
int init2_vmblock(/*out*/vm_block_t * vmblock, size_t size_in_pages, const accessmode_e access_mode) ;

/* function: free_vmblock
 * Invalidates virtual memory address range
 * > vmblock->addr[0 .. vmblock->size - 1 ]
 * After successull return every access to this memory range will generate a memory exception and
 * vmblock is set to <vm_block_t.vm_block_INIT_FREEABLE>.
 * Therefore unmapping an already unmapped memory region does nothing and returns success. */
int free_vmblock(vm_block_t * vmblock) ;

// group: change

/* function: protect_vmblock
 * Sets protection of memory (e.g. if write is possible).
 * See <accessmode_e> for a list of all supported bits.
 * <accessmode_PRIVATE> and <accessmode_SHARED> can not be changed after creation. */
int protect_vmblock(vm_block_t * vmblock, const accessmode_e access_mode) ;

/* function: tryexpand_vmblock
 * Tries to grow the upper bound of an already mapped address range.
 * The start address of virtual memory block is not changed.
 * Returns 0 on success else ENOMEM or another system specific error code.
 * In case of success the new address range is
 * > [old:vmblock->addr .. old:vmblock->addr + old:vmblock->size + increment_in_pages * <pagesize_vm>).
 * If the memory could not be expanded no error logging is done. */
int tryexpand_vmblock(vm_block_t * vmblock, size_t increment_in_pages) ;

/* function: movexpand_vmblock
 * Grows an already mapped virtual memory block.
 * If the block can not be expanded (see <tryexpand_vmblock>) it is relocated
 * to a new virtual address with sufficient space.
 * In case of success the new address range is
 * >    [ old:vmblock->addr .. old:vmblock->addr + old:vmblock->size + increment_in_pages * <pagesize_vm>)
 * > or [ NEW_ADDR .. NEW_ADDR + old:vmblock->size + increment_in_pages * <pagesize_vm>). */
int movexpand_vmblock(vm_block_t * vmblock, size_t increment_in_pages) ;

/* function: shrink_vmblock
 * Shrinks an already mapped virtual memory block (start addr keeps same).
 * If decrement_in_pages * pagesize_vm() if greater or equal to vmblock->size
 * EINVAL is returned and nothing is changed.
 * In case of success the new address range is
 * > [vmblock->addr .. vmblock->addr + vmblock->size - decrement_in_pages * <pagesize_vm>). */
int shrink_vmblock(vm_block_t * vmblock, size_t decrement_in_pages) ;


/* struct: vm_region_t
 * Returns information about a mapped memory region and its access permissions. */
struct vm_region_t
{
   /* variable: addr
    * Start address or lowest address of mapping. */
   void           *  addr ;
   /* variable: endaddr
    * End address of mapping. It points to the address after the last mapped byte.
    * Therefore the length in pages can be calculated as:
    * > (endaddr - addr) / pagesize_vm() */
   void           *  endaddr ;
   /* variable: protection
    * Gives protection (access rights) of the memory block.
    * See <accessmode_e> for a list of supported bits. */
   accessmode_e      protection ;
} ;

// group: query

/* function: compare_vmregion
 * Returns 0 if left and right regions compare equal. */
int compare_vmregion(const vm_region_t * left, const vm_region_t * right) ;


/* struct: vm_mappedregions_t
 * Buffer which stores a snapshot of all mapped memory regions.
 * Use <init_vmmappedregions> to store a snapshot of the current mapping.
 * Do not forget to call <free_vmmappedregions> after using.
 * To access individual mapping descriptions of type <vm_region_t> use
 * <next_vmmappedregions>. With <gofirst_vmmappedregions> you can reset
 * the internal iterator and scan from the beginning again. */
struct vm_mappedregions_t
{
   // group: private variables
   /* var: total_count
    * Number of stored elements of type <vm_region_t>. */
   size_t               total_count ;
   /* var: element_count
    * Number of elements <element_iterator> can access in sequence.
    * Used to implement internal iterator. */
   size_t               element_count ;
   /* var: element_iterator
    * Points to an array of <vm_region_t> of size <element_count>.
    * Used to implement internal iterator. */
   vm_region_t        * element_iterator ;
   /* var: array_iterator
    * Points to next array of <vm_region_t> which comes after array <element_iterator> points to.
    * Used to implement internal iterator. */
   vm_regionsarray_t  * array_iterator ;
   /* var: first_array
    * Points to first array of <vm_region_t>.
    * The memory is organized as a list of arrays of elements of type <vm_region_t>.
    * Used to implement internal iterator and to free memory. */
   vm_regionsarray_t  * first_array ;
} ;

// group: lifetime

/* define: vm_mappedregions_INIT_FREEABLE
 * Static initializer: Makes calling <free_vmmappedregions> safe. */
#define vm_mappedregions_INIT_FREEABLE  { 0, 0, 0, 0, 0 }

/* function: init_vmmappedregions
 * Returns in mappedregions the descriptions of all current virtual memory mappings.
 * If changes are made to the mapping of virtual memory pages after the call to init
 * mappedregions still holds the old mapping description.
 * See <vm_region_t> for a description of the returned information. */
int init_vmmappedregions(/*out*/vm_mappedregions_t * mappedregions) ;

/* function: free_vmmappedregions
 * Free internal memory cache used to store all <vm_region_t> objects. */
int free_vmmappedregions(vm_mappedregions_t * mappedregions) ;

// group: query

/* function: size_vmmappedregions
 * Returns the total number of contained <vm_region_t>. */
size_t size_vmmappedregions(const vm_mappedregions_t * mappedregions) ;

/* function: compare_vmregion
 * Returns 0 if all regions stored in left and right container compare equal. */
int compare_vmmappedregions(const vm_mappedregions_t * left, const vm_mappedregions_t * right) ;

/* function: iscontained_vmmappedregions
 * Returns true if <vm_mappedregions_t> contains a memory region with correct protection.
 * In either case *mblock* is not fully contained or the *protection* is different false
 * is returned. */
bool iscontained_vmmappedregions(vm_mappedregions_t * mappedregions, const memblock_t * mblock, accessmode_e protection) ;

// group: iterate

/* function: gofirst_vmmappedregions
 * Resets iterator to the first element.
 * The next call to <next_vmmappedregions> will return the first contained element.
 * See generic <ArrayList Iterator> for a more in deep description. */
void gofirst_vmmappedregions(vm_mappedregions_t * iterator) ;

/* function: next_vmmappedregions
 * Returns the next <vm_region_t> in the set of all stored elements.
 * NULL is returned if there is no next element.
 * See generic <ArrayList Iterator> for a more in deep description. */
const vm_region_t * next_vmmappedregions(vm_mappedregions_t * iterator) ;


// section: inline implementation

/* define: init_vmblock
 * Implements <vm_block_t.init_vmblock> with help of <vm_block_t.init2_vmblock>. */
#define init_vmblock(vmblock, size_in_pages) \
   (init2_vmblock( vmblock, size_in_pages, accessmode_RDWR|accessmode_PRIVATE ))

/* define: pagesize_vm
 * Uses cached value from <valuecache_context>. */
#define pagesize_vm() \
   (valuecache_maincontext()->pagesize_vm)

/* define: size_vmmappedregions
 * Returns <vm_mappedregions_t->total_count>.
 * Inline implementation of <vm_mappedregions_t.size_vmmappedregions>. */
#define size_vmmappedregions(mappedregions) \
   ((mappedregions)->total_count)

#endif
