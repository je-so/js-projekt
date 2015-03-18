/* title: VirtualMemory
   Manages mapping of virtual memory pages.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/memory/vm.h
    Header file of <VirtualMemory>.

   file: C-kern/platform/Linux/vm.c
    Linux specific implementation <VirtualMemory Linux>.
*/
#ifndef CKERN_MEMORY_VM_HEADER
#define CKERN_MEMORY_VM_HEADER

#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/io/accessmode.h"

/* typedef: struct vmpage_t
 * Exports <vmpage_t> into global namespace. */
typedef struct vmpage_t vmpage_t;

/* typedef: struct vm_region_t
 * Exports <vm_region_t>, describes a single virtual memory region. */
typedef struct vm_region_t vm_region_t;

/* typedef: struct vm_mappedregions_t
 * Exports <vm_mappedregions_t>, description of all mapped memory. */
typedef struct vm_mappedregions_t vm_mappedregions_t;

/* typedef: struct vm_regionsarray_t
 * Internal type used by <vm_mappedregions_t>. */
typedef struct vm_regionsarray_t vm_regionsarray_t;


// section: Functions

// group: query

/* function: log2pagesize_vm
 * Returns <log2_int> of <pagesize_vm>. */
uint8_t  log2pagesize_vm(void);

/* function: pagesize_vm
 * Returns the virtual memory page size supported by the underlying system.
 * This function returns a cached value >= 256. */
uint32_t pagesize_vm(void);

/* function: sizephysram_vm
 * Returns size of all physical memory in bytes. */
ramsize_t sizephysram_vm(void);

/* function: sizeavailableram_vm
 * Returns size of available physical memory in bytes  */
ramsize_t sizeavailableram_vm(void);

/* function: sys_pagesize_vm
 * Returns the virtual memory page size supported by the underlying system.
 * This functions always calls the underlying system function.
 * The returned value is a power of two. */
uint32_t sys_pagesize_vm(void);

/* function: ismapped_vm
 * Returns true in case vmpage is mapped with accessmode equaling protection. */
bool ismapped_vm(const vmpage_t * vmpage, accessmode_e protection);

/* function: isunmapped_vm
 * Returns true if memory at vmpage is not mapped. */
bool isunmapped_vm(const vmpage_t * vmpage);

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_vm
 * Unittest for virtual memory module. */
int unittest_platform_vm(void);
#endif


/* struct: vmpage_t
 * Type has same structure as <memblock_t>.
 * The size of the mapped memory block is always
 * a multiple of <pagesize_vm>. */
struct vmpage_t {
   /* variable: addr
    * Points to start (lowest) address of memory. */
   uint8_t *   addr;
   /* variable: size
    * Size of memory in bytes <addr> points to.
    * A value of 0 indicates a free memory page.
    * The valid memory region is
    * > addr[ 0 .. size - 1 ] */
   size_t      size;
};

// group: lifetime

/* define: vmpage_FREE
 * Static initializer. Sets object of type <vmpage_t> to NULL.
 * Unmapping (<free_vmpage>) such a NULL <vmpage_t> is safe. */
#define vmpage_FREE vmpage_INIT(0, 0)

/* define: vmpage_INIT
 * Static initializer.
 * Precondition:
 * o Make sure that size is a multiple of <pagesize_vm>. */
#define vmpage_INIT(size, addr)           { addr, size }

/* function: init_vmpage
 * Map memory of at least size_in_bytes bytes into the virtual address space of the calling process.
 * The parameter size_in_bytes is rounded up to next multiple of <pagesize_vm>.
 * The memory is read and writeable. All the changed content is private to the current process.
 * A child process can access its content after a fork but does not see any changes afterwards.
 * The parent process also see no changes (COPY_ON_WRITE semantics). */
int init_vmpage(/*out*/vmpage_t * vmpage, size_t size_in_bytes);

/* function: init2_vmpage
 * Map memory into the virtual address space of the calling process.
 * The memory size is size_in_bytes rounded up to next multiple of <pagesize_vm>.
 * It is accessible as stated in paramter *access_mode*.
 * A child process can access its content after a fork and a change is shared with the parent process
 * if <accessmode_SHARED> was specified. */
int init2_vmpage(/*out*/vmpage_t * vmpage, size_t size_in_bytes, const accessmode_e access_mode);

/* function: initaligned_vmpage
 * Map new memory into the virtual address space of the calling process.
 * The new memory has size powerof2_size_in_bytes and its description is returned in vmpage.
 * It is read and writeable and not shared between processes.
 * The address of the new memory is aligned to its own size.
 * EINVAL is returned in case powerof2_size_in_bytes < <pagesize_vm> or if it is not a power of 2. */
int initaligned_vmpage(/*out*/vmpage_t * vmpage, size_t powerof2_size_in_bytes);

/* function: free_vmpage
 * Invalidates virtual memory address range
 * > vmpage->addr[0 .. vmpage->size - 1 ]
 * After successull return every access to this memory range will generate a memory exception and
 * vmpage is set to <vmpage_FREE>.
 * Therefore unmapping an already unmapped memory region does nothing and returns success. */
int free_vmpage(vmpage_t * vmpage);

// group: query

/* function: isfree_vmpage
 * Returns true if vmpage equals <vmpage_FREE>. */
bool isfree_vmpage(const vmpage_t* vmpage);

// group: change

/* function: protect_vmpage
 * Sets protection of memory (e.g. if write is possible).
 * See <accessmode_e> for a list of all supported bits.
 * <accessmode_PRIVATE> and <accessmode_SHARED> can not be changed after creation. */
int protect_vmpage(const vmpage_t* vmpage, const accessmode_e access_mode);

/* function: tryexpand_vmpage
 * Tries to grow the upper bound of an already mapped address range.
 * The new memory size is size_in_bytes rounded up to next multiple of <pagesize_vm>.
 * If size_in_bytes is lower than vmpage->size EINVAL is returned and nothing is changed.
 * The start address of virtual memory block is not changed.
 * Returns 0 on success else ENOMEM or another system specific error code.
 * In case of success the new address range is
 * >    [ old:vmpage->addr .. old:vmpage->addr + (size_in_bytes rounded up to multiple of pagesize_vm()))
 * If the memory could not be expanded no error logging is done. */
int tryexpand_vmpage(vmpage_t * vmpage, size_t size_in_bytes);

/* function: movexpand_vmpage
 * Grows an already mapped virtual memory block.
 * The new memory size is size_in_bytes rounded up to next multiple of <pagesize_vm>.
 * If size_in_bytes is lower than vmpage->size EINVAL is returned and nothing is changed.
 * If the block can not be expanded (see <tryexpand_vmpage>) it is relocated
 * to a new virtual address with sufficient address space.
 * In case of success the new address range is
 * >    [ old:vmpage->addr .. old:vmpage->addr + (size_in_bytes rounded up to multiple of pagesize_vm()))
 * > or [ NEW_ADDR .. NEW_ADDR + (size_in_bytes rounded up to multiple of pagesize_vm())). */
int movexpand_vmpage(vmpage_t * vmpage, size_t size_in_bytes);

/* function: shrink_vmpage
 * Shrinks the size of an already mapped virtual memory block. The start address keeps same.
 * The new memory size is size_in_bytes rounded up to next multiple of <pagesize_vm>.
 * If size_in_bytes is greater than vmpage->size EINVAL is returned and nothing is changed.
 * In case of success the new address range is
 * > [vmpage->addr .. vmpage->addr + (size_in_bytes rounded up to multiple of pagesize_vm())). */
int shrink_vmpage(vmpage_t * vmpage, size_t size_in_bytes);

// generic

/* function: cast_vmpage
 * Casts a pointer to an compatible object into pointer to <vmpage_t>.
 * The object must have two members nameprefix##addr and nameprefix##size
 * of the same type as <vmpage_t> and in the same order. */
vmpage_t * cast_vmpage(void * obj, IDNAME nameprefix);


/* struct: vm_region_t
 * Returns information about a mapped memory region and its access permissions. */
struct vm_region_t
{
   /* variable: addr
    * Start address or lowest address of mapping. */
   void           *  addr;
   /* variable: endaddr
    * End address of mapping. It points to the address after the last mapped byte.
    * Therefore the length in pages can be calculated as:
    * > (endaddr - addr) / pagesize_vm() */
   void           *  endaddr;
   /* variable: protection
    * Gives protection (access rights) of the memory block.
    * See <accessmode_e> for a list of supported bits. */
   accessmode_e      protection;
};

// group: query

/* function: compare_vmregion
 * Returns 0 if left and right regions compare equal. */
int compare_vmregion(const vm_region_t * left, const vm_region_t * right);


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
   size_t               total_count;
   /* var: element_count
    * Number of elements <element_iterator> can access in sequence.
    * Used to implement internal iterator. */
   size_t               element_count;
   /* var: element_iterator
    * Points to an array of <vm_region_t> of size <element_count>.
    * Used to implement internal iterator. */
   vm_region_t        * element_iterator;
   /* var: array_iterator
    * Points to next array of <vm_region_t> which comes after array <element_iterator> points to.
    * Used to implement internal iterator. */
   vm_regionsarray_t  * array_iterator;
   /* var: first_array
    * Points to first array of <vm_region_t>.
    * The memory is organized as a list of arrays of elements of type <vm_region_t>.
    * Used to implement internal iterator and to free memory. */
   vm_regionsarray_t  * first_array;
};

// group: lifetime

/* define: vm_mappedregions_FREE
 * Static initializer: Makes calling <free_vmmappedregions> safe. */
#define vm_mappedregions_FREE { 0, 0, 0, 0, 0 }

/* function: init_vmmappedregions
 * Returns in mappedregions the descriptions of all current virtual memory mappings.
 * If changes are made to the mapping of virtual memory pages after the call to init
 * mappedregions still holds the old mapping description.
 * See <vm_region_t> for a description of the returned information. */
int init_vmmappedregions(/*out*/vm_mappedregions_t * mappedregions);

/* function: free_vmmappedregions
 * Free internal memory cache used to store all <vm_region_t> objects. */
int free_vmmappedregions(vm_mappedregions_t * mappedregions);

// group: query

/* function: size_vmmappedregions
 * Returns the total number of contained <vm_region_t>. */
size_t size_vmmappedregions(const vm_mappedregions_t * mappedregions);

/* function: compare_vmmappedregions
 * Returns 0 if all regions stored in left and right container compare equal. */
int compare_vmmappedregions(const vm_mappedregions_t * left, const vm_mappedregions_t * right);

/* function: ismapped_vmmappedregions
 * Returns true if mappedregions contains a memory region with correct protection.
 * if *mblock* is not fully contained or the *protection* is different false is returned. */
bool ismapped_vmmappedregions(vm_mappedregions_t * mappedregions, const vmpage_t * mblock, accessmode_e protection);

/* function: isunmapped_vmmappedregions
 * Returns true if mappedregions contains no memory region which overlaps with mblock. */
bool isunmapped_vmmappedregions(vm_mappedregions_t * mappedregions, const vmpage_t * mblock);

// group: iterate

/* function: gofirst_vmmappedregions
 * Resets iterator to the first element.
 * The next call to <next_vmmappedregions> will return the first contained element.
 * See generic <ArrayList Iterator> for a more in deep description. */
void gofirst_vmmappedregions(vm_mappedregions_t * iterator);

/* function: next_vmmappedregions
 * Returns the next <vm_region_t> in the set of all stored elements.
 * NULL is returned if there is no next element.
 * See generic <ArrayList Iterator> for a more in deep description. */
const vm_region_t * next_vmmappedregions(vm_mappedregions_t * iterator);


// section: inline implementation

/* define: cast_vmpage
 * Implements <vmpage_t.cast_vmpage>. */
#define cast_vmpage(obj, nameprefix) \
         ( __extension__ ({                         \
            typeof(obj) _o = (obj);                 \
            static_assert(                          \
               &(_o->nameprefix##addr)              \
               == &((vmpage_t*) (uintptr_t)         \
                    (_o->nameprefix##addr))->addr   \
               && &(_o->nameprefix##size)           \
                  == &((vmpage_t*) (uintptr_t)      \
                     (_o->nameprefix##addr))->size, \
               "fields are compatible"              \
            );                                      \
            (vmpage_t*) (&_o->nameprefix##addr);    \
         }))

/* define: init_vmpage
 * Implements <vmpage_t.init_vmpage>. */
#define init_vmpage(vmpage, size_in_bytes) \
         (init2_vmpage((vmpage), (size_in_bytes), accessmode_RDWR|accessmode_PRIVATE))

/* define: isfree_vmpage
 * Implements <vmpage_t.isfree_vmpage>>. */
#define isfree_vmpage(vmpage)  \
         (0 == (vmpage)->addr && 0 == (vmpage)->size)

/* define: log2pagesize_vm
 * Uses cached value from <valuecache_maincontext>. */
#define log2pagesize_vm()                       (valuecache_maincontext()->log2pagesize_vm)

/* define: pagesize_vm
 * Uses cached value from <valuecache_maincontext>. */
#define pagesize_vm()                           (valuecache_maincontext()->pagesize_vm)

/* define: size_vmmappedregions
 * Returns <vm_mappedregions_t->total_count>.
 * Inline implementation of <vm_mappedregions_t.size_vmmappedregions>. */
#define size_vmmappedregions(mappedregions)     ((mappedregions)->total_count)

#endif
