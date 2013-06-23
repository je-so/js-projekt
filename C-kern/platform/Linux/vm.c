/* title: VirtualMemory Linux
   Implements <VirtualMemory>, mapping of virtual memory pages.

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

   file: C-kern/api/memory/vm.h
    Header file of <VirtualMemory>.

   file: C-kern/platform/Linux/vm.c
    Linux specific implementation <VirtualMemory Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/err.h"
#include "C-kern/api/cache/objectcache_macros.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

/* define: PROC_SELF_MAPS
 * The Linux system file containing the currently mapped memory regions and their access permissions of this process.
 *
 * Format:
 * >  address          perms offset   dev   inode   pathname
 *
 * >  08048000-08056000 r-xp 00000000 03:0c 64593   /usr/sbin/gpm
 * >  08056000-08058000 rw-p 0000d000 03:0c 64593   /usr/sbin/gpm
 * >  08058000-0805b000 rwxp 00000000 00:00 0
 *
 * See:
 * > man 5 proc
 * for a more detailed description */
#define  PROC_SELF_MAPS    "/proc/self/maps"

struct vm_regionsarray_t
{
   vm_regionsarray_t     * next ;
   size_t                  size ;
   vm_region_t      elements[16] ;
} ;


// section: Functions

static inline void compiletime_assert(void)
{
   vmpage_t vmpage ;
   static_assert( (memblock_t*)&vmpage == genericcast_memblock(&vmpage, )
                  && sizeof(vmpage_t) == sizeof(memblock_t), "same structure") ;
}

/* function: sys_pagesize_vm
 * Uses sysconf(_SC_PAGESIZE) which conforms to POSIX.1-2001. */
uint32_t sys_pagesize_vm()
{
   long ps = sysconf(_SC_PAGESIZE) ;
   assert(256 <= ps && (unsigned long)ps <= 0x80000000) ;
   assert(ispowerof2_int((uint32_t)ps)) ;
   return (uint32_t)ps ;
}

bool ismapped_vm(const vmpage_t * vmpage, accessmode_e protection)
{
   int err ;
   vm_mappedregions_t mappedregions ;

   err = init_vmmappedregions(&mappedregions) ;
   if (err) return false ;

   bool is_mapped = ismapped_vmmappedregions(&mappedregions, vmpage, protection) ;

   (void) free_vmmappedregions(&mappedregions) ;

   return is_mapped ;
}

bool isunmapped_vm(const vmpage_t * vmpage)
{
   int err ;
   vm_mappedregions_t mappedregions ;

   err = init_vmmappedregions(&mappedregions) ;
   if (err) return false ;

   bool is_unmapped = isunmapped_vmmappedregions(&mappedregions, vmpage) ;

   (void) free_vmmappedregions(&mappedregions) ;

   return is_unmapped ;
}



// section: vm_region_t

int compare_vmregion(const vm_region_t * left, const vm_region_t * right)
{
#define RETURN_ON_UNEQUAL(name)  \
   if (left->name != right->name) { \
      return left->name > right->name ? 1 : -1 ; \
   }

   RETURN_ON_UNEQUAL(addr) ;
   RETURN_ON_UNEQUAL(endaddr) ;
   RETURN_ON_UNEQUAL(protection) ;

   return 0 ;
}


// sectin: vm_mappedregions_t

// group: helper

static int read_buffer(int fd, const size_t buffer_maxsize, uint8_t buffer[buffer_maxsize], size_t buffer_offset, /*out*/size_t * buffer_size, /*out*/size_t * line_end)
{
   int err = 0 ;
   size_t index_newline = buffer_offset ;
   for (;;) {
      const ssize_t read_size = read(fd, buffer + buffer_offset, buffer_maxsize - buffer_offset) ;
      if (!read_size) {
         if (buffer_offset) {
            err = EINVAL ;
            TRACEERR_LOG(FILE_FORMAT_MISSING_ENDOFLINE, err, PROC_SELF_MAPS) ;
            goto ONABORT ;
         }
         break ; // reached end of file
      }
      if (read_size < 0) {
         err = errno ;
         if (err == EINTR) continue ;
         TRACESYSERR_LOG("read", err) ;
         goto ONABORT ;
      }
      buffer_offset += (size_t)read_size ;
      do {
         if (buffer[index_newline] == '\n') break ;
         ++ index_newline ;
      } while (index_newline < buffer_offset) ;
      if (index_newline < buffer_offset) break ; // ! found '\n' !
   }

   *buffer_size = buffer_offset ;
   *line_end    = index_newline ;
   return 0 ;
ONABORT:
   return err ;
}

// group: lifetime

int free_vmmappedregions(vm_mappedregions_t * mappedregions)
{
   int err = 0 ;
   vm_regionsarray_t * first = mappedregions->first_array ;

   while (first) {
      memblock_t mem = memblock_INIT(sizeof(vm_regionsarray_t), (uint8_t*) first) ;

      first = first->next ;

      int err2 = FREE_MM(&mem) ;
      if (err2) err = err2;
   }

   *mappedregions = (vm_mappedregions_t) vm_mappedregions_INIT_FREEABLE ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int init_vmmappedregions(/*out*/vm_mappedregions_t * mappedregions)
{
   int err ;
   int fd = -1 ;
   vm_regionsarray_t *  first_array = 0 ;
   vm_regionsarray_t *  last_array  = 0 ;
   size_t               total_regions_count = 0 ;
   size_t               free_region_count   = 0 ;
   vm_region_t       *  next_region = 0 ;
   memblock_t        *  iobuffer    = 0 ;

   LOCKIOBUFFER_OBJECTCACHE(&iobuffer) ;

   size_t  const buffer_maxsize = iobuffer->size ;
   uint8_t       * const buffer = iobuffer->addr ;
   fd = open(PROC_SELF_MAPS, O_RDONLY|O_CLOEXEC) ;
   if (fd < 0) {
      err = ENOSYS ;
      TRACESYSERR_LOG("open(" PROC_SELF_MAPS ")", errno) ;
      goto ONABORT ;
   }

   for (size_t buffer_offset = 0;;) {
      size_t line_start = 0 ;
      size_t buffer_size = 0 ;
      size_t line_end = 0 ;

      err = read_buffer(fd, buffer_maxsize, buffer, buffer_offset, &buffer_size, &line_end) ;
      if (err) goto ONABORT ;
      if (0 == buffer_size) break ;

      do {
         uintptr_t  start, end ;
         char  isReadable, isWriteable, isExecutable, isShared ;
         uint64_t file_offset ;
         unsigned major, minor ;
         unsigned long inode ;
         int scanned_items = sscanf((char*)(buffer+line_start),
                  "%" SCNxPTR "-%" SCNxPTR " %c%c%c%c %llx %02x:%02x %lu ",
                  &start, &end,
                  &isReadable, &isWriteable, &isExecutable, &isShared,
                  &file_offset, &major, &minor, &inode) ;
         if (scanned_items != 10) {
            err = EINVAL ;
            TRACEERR_LOG(FILE_FORMAT_WRONG, err, PROC_SELF_MAPS) ;
            goto ONABORT ;
         }

         if (!free_region_count) {
            memblock_t mem = memblock_INIT_FREEABLE ;
            err = RESIZE_MM(sizeof(vm_regionsarray_t), &mem) ;
            if (err) goto ONABORT ;
            vm_regionsarray_t * next_array = (vm_regionsarray_t*) mem.addr ;
            free_region_count = sizeof(next_array->elements) / sizeof(next_array->elements[0]) ;
            next_region       = &next_array->elements[0] ;
            next_array->next  = 0 ;
            next_array->size  = free_region_count ;

            if (first_array) {
               last_array->next = next_array ;
               last_array       = next_array ;
            } else {
               first_array = next_array ;
               last_array  = next_array ;
            }
         }

         next_region->addr         = (void *) start ;
         next_region->endaddr      = (void *) end ;
         accessmode_e   prot ;
         prot  = ((isReadable == 'r')   ? accessmode_READ   : 0) ;
         prot |= ((isWriteable == 'w')  ? accessmode_WRITE  : 0) ;
         prot |= ((isExecutable == 'x') ? accessmode_EXEC   : 0) ;
         prot |= ((isShared == 's')     ? accessmode_SHARED : accessmode_PRIVATE) ;
         next_region->protection   = prot ;

         ++ total_regions_count ;
         -- free_region_count ;
         ++ next_region ;

         for (line_start = ++ line_end; line_end < buffer_size; ++line_end) {
            if (buffer[line_end] == '\n') break ;
         }
      } while (line_end < buffer_size) ;

      if (line_start < buffer_size) {
         buffer_offset = buffer_size - line_start ;
         memmove(buffer, buffer + line_start, buffer_offset) ;
      } else {
         buffer_offset = 0 ;  // scanned whole buffer
      }
   }

   err = free_iochannel(&fd) ;
   if (err) goto ONABORT ;

   UNLOCKIOBUFFER_OBJECTCACHE(&iobuffer) ;

   mappedregions->total_count      = total_regions_count ;
   mappedregions->element_count    = 0 ;
   mappedregions->element_iterator = 0 ;
   mappedregions->array_iterator   = 0 ;
   mappedregions->first_array      = first_array ;
   if (free_region_count) {
      last_array->size            -= free_region_count ;
   }
   gofirst_vmmappedregions(mappedregions) ;

   return 0 ;
ONABORT:
   while (first_array) {
      memblock_t mem = memblock_INIT(sizeof(vm_regionsarray_t), (uint8_t*) first_array) ;
      first_array = first_array->next ;
      (void) FREE_MM(&mem) ;
   }
   UNLOCKIOBUFFER_OBJECTCACHE(&iobuffer) ;
   free_iochannel(&fd) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: query

int compare_vmmappedregions(const vm_mappedregions_t * left, const vm_mappedregions_t * right)
{
   if (left->total_count != right->total_count) {
      return left->total_count > right->total_count ? 1 : -1 ;
   }

   vm_mappedregions_t left2  = { .first_array = left->first_array } ;
   vm_mappedregions_t right2 = { .first_array = right->first_array } ;
   gofirst_vmmappedregions(&left2) ;
   gofirst_vmmappedregions(&right2) ;

   for (size_t i = left->total_count; i != 0 ; --i) {
      const vm_region_t  * lelem = next_vmmappedregions(&left2) ;
      const vm_region_t  * relem = next_vmmappedregions(&right2) ;
      int cmp = compare_vmregion(lelem, relem) ;
      if (cmp) return cmp ;
   }

   return 0 ;
}

bool ismapped_vmmappedregions(vm_mappedregions_t * mappedregions, const vmpage_t * mblock, accessmode_e protection)
{
   vm_mappedregions_t   iterator    = *mappedregions ;
   void                 * startaddr = (void*) mblock->addr ;
   void                 * endaddr   = (void*) (mblock->addr + mblock->size) ;

   gofirst_vmmappedregions(&iterator) ;

   for (const vm_region_t * vmregion; (vmregion = next_vmmappedregions(&iterator));) {
      if (  startaddr  < vmregion->endaddr
            && endaddr > vmregion->addr) {
         if (vmregion->protection != protection) {
            return false ;
         }
         if (vmregion->addr <= startaddr) {
            if (endaddr <= vmregion->endaddr) {
               return true ;
            }
            // support simple splitting (this occurs after split block, shrink lower half and expand lower half)
            startaddr = vmregion->endaddr ;
         } else {
            // iterator returns elements in ascending order
            return false ;
         }
      }
   }

   return false ;
}

bool isunmapped_vmmappedregions(vm_mappedregions_t * mappedregions, const vmpage_t * mblock)
{
   vm_mappedregions_t   iterator    = *mappedregions ;
   void                 * startaddr = (void*) mblock->addr ;
   void                 * endaddr   = (void*) (mblock->addr + mblock->size) ;

   gofirst_vmmappedregions(&iterator) ;

   for (const vm_region_t * vmregion; (vmregion = next_vmmappedregions(&iterator));) {
      if (  startaddr < vmregion->endaddr
            && endaddr > vmregion->addr) {
         return false ;
      }
   }

   return true ;
}

// group: iterate

void gofirst_vmmappedregions(vm_mappedregions_t * iterator)
{
   vm_regionsarray_t * first = iterator->first_array ;
   if (first) {
      iterator->element_count    = first->size ;
      iterator->element_iterator = &first->elements[0] ;
      iterator->array_iterator   = first->next ;
   }
}

const vm_region_t * next_vmmappedregions(vm_mappedregions_t * iterator)
{
   if (!iterator->element_count) {
      vm_regionsarray_t    * next = iterator->array_iterator ;
      if (!next) return 0 ;
      iterator->element_count     = next->size ;
      iterator->element_iterator  = &next->elements[0] ;
      iterator->array_iterator    = next->next ;
   }

   -- iterator->element_count ;
   return iterator->element_iterator ++ ;
}


// section: vmpage_t

// group: helper

/* define: SET_PROT
 * Converts <accessmode_e> into POSIX representation.
 * The parameter prot is set to the POSIX representation
 * of the value access_mode which is of type <accessmode_e>. */
#define SET_PROT(prot, access_mode) \
   static_assert(0 == accessmode_NONE, "") ;    \
   static_assert(0 == PROT_NONE, "") ;          \
   if (     accessmode_READ  == PROT_READ       \
         && accessmode_WRITE == PROT_WRITE      \
         && accessmode_EXEC  == PROT_EXEC) {    \
      prot = access_mode                        \
           & (accessmode_RDWR|accessmode_EXEC); \
   } else {                                     \
      if (accessmode_READ & access_mode) {      \
         prot = PROT_READ ;                     \
      } else {                                  \
         prot = PROT_NONE ;                     \
      }                                         \
      if (accessmode_WRITE & access_mode) {     \
         prot |= PROT_WRITE ;                   \
      }                                         \
      if (accessmode_EXEC & access_mode) {      \
         prot |= PROT_EXEC ;                    \
      }                                         \
   }

// group: lifetime

int init2_vmpage(/*out*/vmpage_t * vmpage, size_t size_in_pages, accessmode_e access_mode)
{
   int    err ;
   int    prot ;
   size_t size_in_bytes = size_in_pages << log2pagesize_vm() ;

   VALIDATE_INPARAM_TEST(0 == (access_mode & ~((unsigned)accessmode_RDWR_PRIVATE|accessmode_EXEC|accessmode_SHARED)), ONABORT,) ;
   VALIDATE_INPARAM_TEST(size_in_pages > 0, ONABORT,) ;
   VALIDATE_INPARAM_TEST((SIZE_MAX >> log2pagesize_vm()) > size_in_pages, ONABORT,) ;

   SET_PROT(prot, access_mode)

   const int shared_flags = (access_mode & accessmode_SHARED ? MAP_SHARED|MAP_ANONYMOUS : MAP_PRIVATE|MAP_ANONYMOUS) ;
   void *    mapped_pages = mmap(0, size_in_bytes, prot, shared_flags, -1, 0) ;

   if (mapped_pages == MAP_FAILED) {
      err = errno ;
      TRACESYSERR_LOG("mmap", err) ;
      PRINTSIZE_LOG(size_in_bytes) ;
      goto ONABORT ;
   }

   vmpage->addr = mapped_pages ;
   vmpage->size = size_in_bytes ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int initaligned_vmpage(/*out*/vmpage_t * vmpage, size_t powerof2_size_in_bytes)
{
   int err ;

   *vmpage = (vmpage_t) vmpage_INIT_FREEABLE ;

   VALIDATE_INPARAM_TEST(powerof2_size_in_bytes >= pagesize_vm(), ONABORT,) ;
   VALIDATE_INPARAM_TEST(ispowerof2_int(powerof2_size_in_bytes), ONABORT,) ;

   err = init_vmpage(vmpage, powerof2_size_in_bytes >> log2pagesize_vm()) ;
   if (err) goto ONABORT ;

   // align vmpage to boundary of powerof2_size_in_bytes

   if (0 != ((uintptr_t)vmpage->addr & (uintptr_t)(powerof2_size_in_bytes-1))) {
      err = movexpand_vmpage(vmpage, powerof2_size_in_bytes >> log2pagesize_vm()) ;
      if (err) goto ONABORT ;

      uintptr_t offset = (uintptr_t)vmpage->addr & (uintptr_t)(powerof2_size_in_bytes-1) ;
      size_t    hdsize = offset ? powerof2_size_in_bytes - offset : 0 ;

      vmpage_t header  = vmpage_INIT(hdsize, vmpage->addr) ;
      vmpage->addr += hdsize ;
      vmpage->size -= hdsize ;
      err = free_vmpage(&header) ;
      if (err) goto ONABORT ;

      vmpage_t trailer = vmpage_INIT(vmpage->size - powerof2_size_in_bytes, vmpage->addr + powerof2_size_in_bytes) ;
      vmpage->size  = powerof2_size_in_bytes ;
      err = free_vmpage(&trailer) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   free_vmpage(vmpage) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_vmpage(vmpage_t * vmpage)
{
   int err ;
   uint8_t * addr = vmpage->addr ;
   size_t    size = vmpage->size ;

   vmpage->addr = (uint8_t*)0 ;
   vmpage->size = 0 ;

   if (size && munmap(addr, size)) {
      err = errno ;
      TRACESYSERR_LOG("munmap", err) ;
      PRINTPTR_LOG(addr) ;
      PRINTSIZE_LOG(size) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: change

int protect_vmpage(vmpage_t * vmpage, const accessmode_e access_mode)
{
   int err ;
   int prot ;

   SET_PROT(prot, access_mode)

   if (  vmpage->size
         && mprotect(vmpage->addr, vmpage->size, prot)) {
      err = errno ;
      TRACESYSERR_LOG("mprotect", err) ;
      PRINTPTR_LOG(vmpage->addr) ;
      PRINTSIZE_LOG(vmpage->size) ;
      PRINTINT_LOG(access_mode) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int tryexpand_vmpage(vmpage_t * vmpage, size_t increment_in_pages)
{
   int err ;
   size_t    expand_in_bytes  = increment_in_pages << log2pagesize_vm() ;
   size_t    newsize_in_bytes = vmpage->size + expand_in_bytes ;
   uint8_t * new_addr ;

   if (  increment_in_pages != (expand_in_bytes >> log2pagesize_vm())
         || newsize_in_bytes <  expand_in_bytes) {
      err = EINVAL ;
      goto ONABORT ;
   }

   new_addr = mremap(vmpage->addr, vmpage->size, newsize_in_bytes, 0) ;
   if (MAP_FAILED == new_addr) {
      err = errno ;
      return err ; // no LOGGING
   }

   assert(new_addr == vmpage->addr) ;
   vmpage->size = newsize_in_bytes ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int movexpand_vmpage(vmpage_t * vmpage, size_t increment_in_pages)
{
   int err ;
   size_t    expand_in_bytes  = increment_in_pages << log2pagesize_vm() ;
   size_t    newsize_in_bytes = vmpage->size + expand_in_bytes ;
   uint8_t * new_addr ;

   if (  increment_in_pages != (expand_in_bytes >> log2pagesize_vm())
         || newsize_in_bytes <  expand_in_bytes) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   new_addr = mremap(vmpage->addr, vmpage->size, newsize_in_bytes, MREMAP_MAYMOVE) ;
   if (MAP_FAILED == new_addr) {
      err = errno ;
      TRACEOUTOFMEM_LOG(newsize_in_bytes, err) ;
      goto ONABORT ;
   }

   vmpage->addr = new_addr ;
   vmpage->size = newsize_in_bytes ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int shrink_vmpage(vmpage_t * vmpage, size_t decrement_in_pages)
{
   int err ;

   if ((vmpage->size >> log2pagesize_vm()) <= decrement_in_pages) {
      err = EINVAL ;
      goto ONABORT ;
   }

   if (decrement_in_pages) {
      size_t shrink_in_bytes  = (decrement_in_pages << log2pagesize_vm()) ;
      size_t newsize_in_bytes = vmpage->size - shrink_in_bytes ;

      err = munmap(vmpage->addr + newsize_in_bytes, shrink_in_bytes) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("munmap", err) ;
         PRINTPTR_LOG(vmpage->addr + newsize_in_bytes) ;
         PRINTSIZE_LOG(shrink_in_bytes) ;
         goto ONABORT ;
      }

      vmpage->size = newsize_in_bytes ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

static int test_functions(void)
{
   valuecache_t * vc = valuecache_maincontext() ;

   // TEST sys_pagesize_vm
   TEST(sys_pagesize_vm() >= 256) ;
   TEST(ispowerof2_int(sys_pagesize_vm())) ;

   // TEST pagesize_vm
   TEST(pagesize_vm()  == sys_pagesize_vm()) ;
   TEST(&pagesize_vm() == &vc->pagesize_vm) ;

   // TEST log2pagesize_vm
   TEST(log2pagesize_vm()  != 0) ;
   TEST(&log2pagesize_vm() == &vc->log2pagesize_vm) ;
   TEST(sys_pagesize_vm()  == 1u << log2pagesize_vm()) ;

   // TEST ismapped_vm, isunmapped_vm
   uint8_t * addr = mmap(0, 3*pagesize_vm(), PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED|MAP_ANONYMOUS, -1, 0) ;
   TEST(addr != 0) ;
   vmpage_t vmpage = vmpage_INIT(3*pagesize_vm(), addr) ;
   // protection is checked
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_SHARED|accessmode_EXEC)) ;
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE|accessmode_EXEC)) ;
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_SHARED)) ;
   // memory address range is checked
   TEST(0 == isunmapped_vm(&vmpage)) ;
   TEST(0 == munmap(addr+pagesize_vm(), pagesize_vm())) ;
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_SHARED|accessmode_EXEC)) ;
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), addr + pagesize_vm()) ;
   TEST(1 == isunmapped_vm(&vmpage)) ;
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), addr + 2*pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_SHARED|accessmode_EXEC)) ;
   TEST(0 == isunmapped_vm(&vmpage)) ;
   TEST(0 == munmap(addr+2*pagesize_vm(), pagesize_vm())) ;
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_SHARED|accessmode_EXEC)) ;
   TEST(1 == isunmapped_vm(&vmpage)) ;
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), addr) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_SHARED|accessmode_EXEC)) ;
   TEST(0 == isunmapped_vm(&vmpage)) ;
   TEST(0 == munmap(addr, pagesize_vm())) ;
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_SHARED|accessmode_EXEC)) ;
   TEST(1 == isunmapped_vm(&vmpage)) ;
   vmpage = (vmpage_t) vmpage_INIT(3*pagesize_vm(), addr) ;
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_SHARED|accessmode_EXEC)) ;
   TEST(1 == isunmapped_vm(&vmpage)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_mappedregions(void)
{
   vm_mappedregions_t mappedregions = vm_mappedregions_INIT_FREEABLE ;

   // TEST vm_mappedregions_INIT_FREEABLE
   TEST(0 == mappedregions.total_count) ;
   TEST(0 == mappedregions.element_count) ;
   TEST(0 == mappedregions.element_iterator) ;
   TEST(0 == mappedregions.array_iterator) ;
   TEST(0 == mappedregions.first_array) ;

   // TEST next_vmmappedregions: empty buffer
   TEST(0 == next_vmmappedregions(&mappedregions)) ;

   // TEST size_vmmappedregions: empty buffer
   TEST(0 == size_vmmappedregions(&mappedregions)) ;

   // TEST gofirst_vmmappedregions
   gofirst_vmmappedregions(&mappedregions) ;
   TEST(0 == mappedregions.total_count) ;
   TEST(0 == mappedregions.element_count) ;
   TEST(0 == mappedregions.element_iterator) ;
   TEST(0 == mappedregions.array_iterator) ;
   TEST(0 == mappedregions.first_array) ;

   // TEST init_vmmappedregions
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(16 == mappedregions.element_count) ;          // see vm_regionsarray_t.elements
   TEST(mappedregions.total_count >= mappedregions.element_count) ;
   TEST(mappedregions.first_array) ;
   TEST(mappedregions.first_array->next == mappedregions.array_iterator) ;
   TEST(mappedregions.element_iterator == &mappedregions.first_array->elements[0]) ;

   // TEST free_vmmappedregions
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == mappedregions.total_count) ;
   TEST(0 == mappedregions.element_count) ;
   TEST(0 == mappedregions.element_iterator) ;
   TEST(0 == mappedregions.array_iterator) ;
   TEST(0 == mappedregions.first_array) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == mappedregions.total_count) ;
   TEST(0 == mappedregions.element_count) ;
   TEST(0 == mappedregions.element_iterator) ;
   TEST(0 == mappedregions.array_iterator) ;
   TEST(0 == mappedregions.first_array) ;

   // TEST gofirst_vmmappedregions, next_vmmappedregions: simulation
   vm_regionsarray_t array[3] = {
      { .size = 1, .next = &array[1] },
      { .size = 2, .next = &array[2] },
      { .size = 3, .next = 0 },
   } ;
   vm_mappedregions_t mappedregions2 = {
      .total_count = 6,
      .first_array = &array[0]
   } ;
   for (int do_twice = 0; do_twice < 2; ++do_twice) {
      gofirst_vmmappedregions(&mappedregions2) ;
      for (int ai = 0; ai < 3; ++ai) {
         TEST(6         == mappedregions2.total_count) ;
         TEST(6         == size_vmmappedregions(&mappedregions2)) ;
         TEST(&array[0] == mappedregions2.first_array) ;
         TEST(array[ai].elements == next_vmmappedregions(&mappedregions2)) ;
         TEST((ai < 2 ? &array[ai+1] : 0) == mappedregions2.array_iterator) ;
         TEST(array[ai].size-1     == mappedregions2.element_count) ;
         TEST(array[ai].elements+1 == mappedregions2.element_iterator) ;
         for (unsigned i = 1; i < array[ai].size; ++i) {
            TEST(&array[ai].elements[i] == next_vmmappedregions(&mappedregions2)) ;
         }
         TEST(0 == mappedregions2.element_count) ;
         TEST(array[ai].elements+array[ai].size == mappedregions2.element_iterator) ;
      }
      TEST(0 == next_vmmappedregions(&mappedregions2)) ;
   }

   // TEST compare_vmmappedregions: compare not same
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(0 != compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;

   // TEST gofirst_vmmappedregions, next_vmmappedregions: current mapping of process is in ascending order !
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   gofirst_vmmappedregions(&mappedregions) ;
   const vm_region_t * first = next_vmmappedregions(&mappedregions) ;
   for (int do_twice = 0; do_twice < 2; ++do_twice) {
      const vm_region_t * next ;
      gofirst_vmmappedregions(&mappedregions) ;
      TEST(first == next_vmmappedregions(&mappedregions)) ;
      for (void * addr = first->endaddr; 0 != (next = next_vmmappedregions(&mappedregions));) {
         TEST(next->addr >= addr) ;
         TEST(next->addr <  next->endaddr) ;
         addr = next->endaddr ;
      }
      TEST(0 == next_vmmappedregions(&mappedregions)) ;
   }

   // TEST compare_vmmappedregions: compare same
   TEST(0 == init_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;

   // TEST ismapped_vmmappedregions
   uint8_t * addr = mmap(0, 3*pagesize_vm(), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) ;
   TEST(addr != 0) ;
   vmpage_t vmpage = vmpage_INIT(3*pagesize_vm(), addr) ;
   // protection is checked
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(1 == ismapped_vmmappedregions(&mappedregions, &vmpage, accessmode_RDWR_PRIVATE)) ;
   TEST(0 == ismapped_vmmappedregions(&mappedregions, &vmpage, accessmode_RDWR_SHARED)) ;
   TEST(0 == ismapped_vmmappedregions(&mappedregions, &vmpage, accessmode_RDWR)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   // memory address range is checked
   TEST(0 == munmap(addr, pagesize_vm())) ;
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(0 == ismapped_vmmappedregions(&mappedregions, &vmpage, accessmode_RDWR_PRIVATE)) ;
   vmpage = (vmpage_t) vmpage_INIT(2*pagesize_vm(), addr + pagesize_vm()) ;
   TEST(1 == ismapped_vmmappedregions(&mappedregions, &vmpage, accessmode_RDWR_PRIVATE)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == munmap(addr + 2*pagesize_vm(), pagesize_vm())) ;
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(0 == ismapped_vmmappedregions(&mappedregions, &vmpage, accessmode_RDWR_PRIVATE)) ;
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), addr + pagesize_vm()) ;
   TEST(1 == ismapped_vmmappedregions(&mappedregions, &vmpage, accessmode_RDWR_PRIVATE)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == munmap(addr + pagesize_vm(), pagesize_vm())) ;
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(0 == ismapped_vmmappedregions(&mappedregions, &vmpage, accessmode_RDWR_PRIVATE)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;

   // TEST isunmapped_vmmappedregions
   addr = mmap(0, 3*pagesize_vm(), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) ;
   TEST(addr != 0) ;
   vmpage = (vmpage_t) vmpage_INIT(3*pagesize_vm(), addr) ;
   // memory address range is checked
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(0 == isunmapped_vmmappedregions(&mappedregions, &vmpage)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == munmap(addr, pagesize_vm())) ;
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(0 == isunmapped_vmmappedregions(&mappedregions, &vmpage)) ;
   vmpage_t vmpage2 = vmpage_INIT(pagesize_vm(), addr) ;
   TEST(1 == isunmapped_vmmappedregions(&mappedregions, &vmpage2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == munmap(addr + 2*pagesize_vm(), pagesize_vm())) ;
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(0 == isunmapped_vmmappedregions(&mappedregions, &vmpage)) ;
   vmpage2 = (vmpage_t) vmpage_INIT(pagesize_vm(), addr + 2*pagesize_vm()) ;
   TEST(1 == isunmapped_vmmappedregions(&mappedregions, &vmpage2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == munmap(addr + pagesize_vm(), pagesize_vm())) ;
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(1 == isunmapped_vmmappedregions(&mappedregions, &vmpage)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;

   return 0 ;
ONABORT:
   free_vmmappedregions(&mappedregions) ;
   return EINVAL ;
}

static int test_vmpage(void)
{
   vmpage_t page = vmpage_INIT_FREEABLE ;
   vmpage_t unpage ; // unmapped page
   size_t   size_in_pages ;

   // TEST vmpage_INIT_FREEABLE
   TEST(0 == page.addr) ;
   TEST(0 == page.size) ;

   // TEST vmpage_INIT
   page = (vmpage_t) vmpage_INIT(11, (uint8_t*)10) ;
   TEST(page.addr == (uint8_t*)10) ;
   TEST(page.size == 11) ;

   // TEST isfree_vmpage: checks only size !
   page = (vmpage_t) vmpage_INIT_FREEABLE ;
   TEST(1 == isfree_vmpage(&page)) ;
   page.addr = (uint8_t*)1 ;
   TEST(1 == isfree_vmpage(&page)) ;
   page.size = 1 ;
   TEST(0 == isfree_vmpage(&page)) ;
   page.addr = 0 ;
   TEST(0 == isfree_vmpage(&page)) ;
   page.size = 0 ;
   TEST(1 == isfree_vmpage(&page)) ;

   // TEST genericcast_vmpage
   struct {
      uint8_t *   test_addr ;
      size_t      test_size ;
      uint8_t *   addr ;
      size_t      size ;
   } genericpage ;

   TEST(genericcast_vmpage(&genericpage, test_) == (vmpage_t*)&genericpage) ;
   TEST(genericcast_vmpage(&genericpage, )      == (vmpage_t*)&genericpage.addr) ;


   // TEST init_vmpage, free_vmpage
   for (size_in_pages = 1; size_in_pages < 100; ++size_in_pages) {
      size_t s = size_in_pages * pagesize_vm() ;
      TEST(0 == init_vmpage(&page, size_in_pages)) ;
      TEST(0 != page.addr) ;
      TEST(s == page.size) ;
      TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
      unpage = page ;
      TEST(0 == free_vmpage(&unpage)) ;
      TEST(0 == unpage.addr)
      TEST(0 == unpage.size) ;
      TEST(1 == isunmapped_vm(&page)) ;
      TEST(0 == free_vmpage(&unpage)) ;
      TEST(0 == unpage.addr)
      TEST(0 == unpage.size) ;
   }

   // TEST init2_vmpage, free_vmpage
   accessmode_e prot[] = { accessmode_RDWR_SHARED, accessmode_RDWR_PRIVATE, accessmode_WRITE|accessmode_PRIVATE, accessmode_READ|accessmode_SHARED, accessmode_READ|accessmode_EXEC|accessmode_PRIVATE, accessmode_RDWR_PRIVATE|accessmode_EXEC, accessmode_NONE|accessmode_PRIVATE } ;
   for (size_in_pages = 1; size_in_pages < 100; ++size_in_pages) {
      accessmode_e mode = prot[size_in_pages % lengthof(prot)] ;
      size_t s = size_in_pages * pagesize_vm() ;
      TEST(0 == init2_vmpage(&page, size_in_pages, mode)) ;
      TEST(0 != page.addr) ;
      TEST(s == page.size) ;
      TEST(1 == ismapped_vm(&page, mode)) ;
      unpage = page ;
      TEST(0 == free_vmpage(&unpage)) ;
      TEST(0 == unpage.addr)
      TEST(0 == unpage.size) ;
      TEST(1 == isunmapped_vm(&page)) ;
   }

   // TEST init_vmpage: EINVAL
   TEST(EINVAL == init_vmpage(&page, 0)) ;
   TEST(EINVAL == init_vmpage(&page, SIZE_MAX)) ;

   // TEST init2_vmpage: EINVAL
   TEST(EINVAL == init2_vmpage(&page, 0, accessmode_RDWR)) ;
   TEST(EINVAL == init2_vmpage(&page, SIZE_MAX, accessmode_RDWR)) ;
   TEST(EINVAL == init2_vmpage(&page, 1, accessmode_RDWR|accessmode_NEXTFREE_BITPOS)) ;

   // TEST initaligned_vmpage
   for (size_t size = pagesize_vm(); size <= 100*1024*1024; size *= 2) {
      TEST(0 == initaligned_vmpage(&page, size)) ;
      TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
      TEST(0 != page.addr) ;
      TEST(0 == ((uintptr_t)page.addr % size))
      TEST(size == page.size) ;
      unpage = page ;
      TEST(0 == free_vmpage(&unpage)) ;
      TEST(1 == isunmapped_vm(&page)) ;
   }

   // TEST initaligned_vmpage: EINVAL
   // too small
   TEST(EINVAL == initaligned_vmpage(&page, pagesize_vm()/2)) ;
   TEST(0 == page.addr) ;
   TEST(0 == page.size) ;
   // not a power of two
   TEST(EINVAL == initaligned_vmpage(&page, pagesize_vm()+1)) ;
   TEST(0 == page.addr) ;
   TEST(0 == page.size) ;

   // TEST map, shrink, expand, unmap in (50 pages) loop
   size_in_pages = 50 ;
   TEST(0 == init_vmpage(&page, size_in_pages)) ;
   TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
   for (size_t i = 1; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vmpage_t upperhalf = { page.addr + unmapoffset, page.size - unmapoffset } ;
      vmpage_t lowerhalf = page ;
      TEST(0 == shrink_vmpage(&lowerhalf, size_in_pages-i)) ;
      TEST(lowerhalf.addr == page.addr) ;
      TEST(lowerhalf.size == unmapoffset) ;
      TEST(1 == isunmapped_vm(&upperhalf)) ;
      TEST(1 == ismapped_vm(&lowerhalf, accessmode_RDWR_PRIVATE)) ;
      TEST(0 == tryexpand_vmpage(&lowerhalf, size_in_pages-i)) ;
      TEST(lowerhalf.addr == page.addr) ;
      TEST(lowerhalf.size == page.size) ;
   }
   TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
   unpage = page ;
   TEST(0 == free_vmpage(&unpage)) ;
   TEST(1 == isunmapped_vm(&page)) ;

   // TEST map, movexpand, unmap in (50 pages) loop
   size_in_pages = 50 ;
   TEST(0 == init_vmpage(&page, size_in_pages)) ;
   TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
   for (size_t i = 1; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vmpage_t upperhalf = { page.addr + unmapoffset, page.size - unmapoffset } ;
      vmpage_t lowerhalf = { page.addr, unmapoffset } ;
      unpage = upperhalf ;
      TEST(0 == free_vmpage(&unpage)) ;
      TEST(! unpage.addr && ! unpage.size) ;
      TEST(1 == isunmapped_vm(&upperhalf)) ;
      TEST(1 == ismapped_vm(&lowerhalf, accessmode_RDWR_PRIVATE)) ;
      TEST(0 == movexpand_vmpage(&lowerhalf, size_in_pages-i)) ;
      TEST(page.addr == lowerhalf.addr) ;
      TEST(page.size == lowerhalf.size) ;
   }
   TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
   for (size_t i = 2; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vmpage_t upperhalf = { page.addr + unmapoffset, page.size - unmapoffset } ;
      vmpage_t lowerhalf = { page.addr, unmapoffset } ;
      TEST(0 == shrink_vmpage(&lowerhalf, 1)) ;
      TEST(1 == ismapped_vm(&upperhalf, accessmode_RDWR_PRIVATE)) ;
      TEST(1 == ismapped_vm(&lowerhalf, accessmode_RDWR_PRIVATE)) ;
      unpage = (vmpage_t) { page.addr + unmapoffset - pagesize_vm(), pagesize_vm() } ;
      TEST(1 == isunmapped_vm(&unpage)) ;
      TEST(0 == movexpand_vmpage(&lowerhalf, 1)) ;
      TEST(lowerhalf.addr == page.addr) ;
      TEST(lowerhalf.size == unmapoffset) ;
   }
   TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
   unpage = page ;
   TEST(0 == free_vmpage(&unpage)) ;
   TEST(1 == isunmapped_vm(&page)) ;

   // TEST movexpand_vmpage: (move)
   size_in_pages = 50 ;
   for (size_t i = 2; i < size_in_pages; ++i) {
      TEST(0 == init_vmpage(&page, size_in_pages)) ;
      TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
      size_t   unmapoffset = i * pagesize_vm() ;
      vmpage_t upperhalf = { page.addr + unmapoffset, page.size - unmapoffset } ;
      vmpage_t lowerhalf = { page.addr, unmapoffset  } ;
      TEST(0 == shrink_vmpage(&lowerhalf, 1)) ;
      TEST(lowerhalf.addr == page.addr) ;
      TEST(lowerhalf.size == unmapoffset - pagesize_vm()) ;
      TEST(1 == ismapped_vm(&upperhalf, accessmode_RDWR_PRIVATE)) ;
      TEST(1 == ismapped_vm(&lowerhalf, accessmode_RDWR_PRIVATE)) ;
      unpage = (vmpage_t) { page.addr + unmapoffset - pagesize_vm(), pagesize_vm() } ;
      TEST(1 == isunmapped_vm(&unpage)) ;
      TEST(0 == movexpand_vmpage(&lowerhalf, 2)) ;
      TEST(lowerhalf.addr != page.addr) ;
      TEST(lowerhalf.size == unmapoffset + pagesize_vm()) ;
      TEST(0 == free_vmpage(&lowerhalf)) ;
      TEST(0 == free_vmpage(&upperhalf)) ;
      TEST(1 == isunmapped_vm(&page)) ;
   }

   // TEST tryexpand_vmpage: ENOMEM
   size_in_pages = 10 ;
   TEST(0 == init_vmpage(&page, size_in_pages)) ;
   TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
   {
      size_t   unmapoffset = 7 * pagesize_vm() ;
      vmpage_t upperhalf = { page.addr + unmapoffset,  page.size - unmapoffset } ;
      vmpage_t lowerhalf = page ;
      TEST(0 == shrink_vmpage(&lowerhalf, 3)) ;
      TEST(lowerhalf.size == unmapoffset) ;
      TEST(lowerhalf.addr == page.addr) ;
      TEST(1 == ismapped_vm(&lowerhalf, accessmode_RDWR_PRIVATE)) ;
      TEST(1 == isunmapped_vm(&upperhalf)) ;
      for (size_t i = 1; i < 7; ++i) {
         vmpage_t ext_block = { page.addr, i * pagesize_vm() } ;
         TEST(ENOMEM == tryexpand_vmpage(&ext_block, 3)) ;
         TEST(ext_block.addr == page.addr) ;
         TEST(ext_block.size == i * pagesize_vm()) ;
         TEST(1 == ismapped_vm(&ext_block, accessmode_RDWR_PRIVATE)) ;
         TEST(1 == ismapped_vm(&lowerhalf, accessmode_RDWR_PRIVATE)) ;
         TEST(1 == isunmapped_vm(&upperhalf)) ;
      }
      TEST(0 == tryexpand_vmpage(&lowerhalf, 3)) ;
      TEST(lowerhalf.size == page.size) ;
      TEST(lowerhalf.addr == page.addr) ;
   }
   TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
   unpage = page ;
   TEST(0 == free_vmpage(&unpage)) ;
   TEST(1 == isunmapped_vm(&page)) ;

   // TEST tryexpand_vmpage: expand of already mapped
   size_in_pages = 3 ;
   TEST(0 == init_vmpage(&page, size_in_pages)) ;
   TEST(1 == ismapped_vm(&page, accessmode_RDWR_PRIVATE)) ;
   for (size_t i = 0; i < size_in_pages; ++i) {
      size_t     lowersize = i * pagesize_vm() ;
      vmpage_t lowerhalf = { page.addr, lowersize } ;
      TEST(ENOMEM == tryexpand_vmpage(&lowerhalf, 1)) ;
   }

   // TEST free_vmpage: unmap of already unmapped (no error)
   unpage = page ;
   TEST(0 == free_vmpage(&unpage)) ;
   TEST(1 == isunmapped_vm(&page)) ;
   unpage = page ;
   TEST(0 == free_vmpage(&unpage)) ;
   TEST(1 == isunmapped_vm(&page)) ;

   // TEST free_vmpage: unmap empty block
   TEST(0 == unpage.addr) ;
   TEST(0 == unpage.size) ;
   TEST(0 == free_vmpage(&unpage)) ;
   TEST(0 == unpage.addr) ;
   TEST(0 == unpage.size) ;

   // TEST EINVAL
   TEST(0      == init_vmpage(&page, 1)) ;
   TEST(EINVAL == tryexpand_vmpage(&page, (size_t)-1)) ;
   TEST(EINVAL == shrink_vmpage(&page, 1)) ;
   TEST(0      == free_vmpage(&page)) ;
   TEST(EINVAL == init_vmpage(&page, (size_t)-1)) ;

   // TEST ENOMEM movexpand
   TEST(0      == init_vmpage(&page, 1)) ;
   TEST(ENOMEM == movexpand_vmpage(&page, (size_t)-1)) ;
   TEST(ENOMEM == movexpand_vmpage(&page, (((size_t)-1) / pagesize_vm()) - 10)) ;
   TEST(0      == free_vmpage(&page)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static ucontext_t    s_usercontext ;

static void sigsegfault(int _signr)
{
   (void) _signr ;
   setcontext(&s_usercontext) ;
}

static int test_protection(void)
{
   vmpage_t          vmpage   = vmpage_INIT_FREEABLE ;
   bool              isOldact = false ;
   struct sigaction  oldact ;
   volatile int      is_exception ;

   // install exception handler for SEGMENTATION FAULT
   {
      struct sigaction    newact ;
      sigemptyset(&newact.sa_mask) ;
      newact.sa_flags   = 0 ;
      newact.sa_handler = &sigsegfault ;
      TEST(0 == sigaction(SIGSEGV, &newact, &oldact)) ;
      isOldact = true ;
   }

   // TEST protection after init, expand, movexpand, shrink
   accessmode_e prot[6] = { accessmode_RDWR_PRIVATE, accessmode_WRITE|accessmode_PRIVATE, accessmode_READ|accessmode_PRIVATE, accessmode_READ|accessmode_EXEC|accessmode_PRIVATE, accessmode_RDWR|accessmode_EXEC|accessmode_PRIVATE, accessmode_NONE|accessmode_PRIVATE } ;
   for (unsigned i = 0; i < lengthof(prot); ++i) {
      // TEST init2 generates correct protection
      TEST(0 == init2_vmpage(&vmpage, 2, prot[i])) ;
      TEST(1 == ismapped_vm(&vmpage, prot[i])) ;
      TEST(0 == free_vmpage(&vmpage)) ;
      // TEST init generates RW protection
      TEST(0 == init_vmpage(&vmpage, 2)) ;
      TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
      // TEST setting protection
      TEST(0 == protect_vmpage(&vmpage, prot[i])) ;
      TEST(1 == ismapped_vm(&vmpage, prot[i])) ;
      // TEST shrink does not change flags
      TEST(0 == shrink_vmpage(&vmpage, 1)) ;
      TEST(1 == ismapped_vm(&vmpage, prot[i])) ;
      // TEST expand does not change flags
      TEST(0 == tryexpand_vmpage(&vmpage, 1)) ;
      TEST(1 == ismapped_vm(&vmpage, prot[i])) ;
      // TEST movexpand does not change flags
      TEST(0 == movexpand_vmpage(&vmpage, 10)) ;
      TEST(1 == ismapped_vm(&vmpage, prot[i])) ;
      vmpage_t old = vmpage ;
      TEST(0 == free_vmpage(&vmpage)) ;
      TEST(1 == isunmapped_vm(&old)) ;
   }

   // TEST write of readonly page is not possible
   TEST(0 == init_vmpage(&vmpage, 1)) ;
   TEST(0 == protect_vmpage(&vmpage, accessmode_READ)) ;
   is_exception = 0 ;
   TEST(0 == getcontext(&s_usercontext)) ;
   if (!is_exception) {
      is_exception = 1 ;
      vmpage.addr[0] = 0xff ;
      ++ is_exception ;
   }
   TEST(1 == is_exception) ;
   TEST(0 == free_vmpage(&vmpage)) ;

   // TEST read of not accessible page is not possible
   TEST(0 == init2_vmpage(&vmpage, 1, accessmode_NONE)) ;
   is_exception = 0 ;
   TEST(0 == getcontext(&s_usercontext)) ;
   if (!is_exception) {
      is_exception = 1 ;
      is_exception = vmpage.addr[0] ;
      is_exception = 2 ;
   }
   TEST(1 == is_exception) ;
   TEST(0 == free_vmpage(&vmpage)) ;

   // uninstall exception handler
   TEST(0 == sigaction(SIGSEGV, &oldact, 0)) ;

   return 0 ;
ONABORT:
   if (isOldact) sigaction(SIGSEGV, &oldact, 0) ;
   (void) free_vmpage(&vmpage) ;
   return EINVAL;
}

int unittest_platform_vm()
{
   vm_mappedregions_t mappedregions  = vm_mappedregions_INIT_FREEABLE ;
   vm_mappedregions_t mappedregions2 = vm_mappedregions_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_vmmappedregions(&mappedregions)) ;

   if (test_functions())      goto ONABORT ;
   if (test_mappedregions())  goto ONABORT ;
   if (test_vmpage())         goto ONABORT ;
   if (test_protection())     goto ONABORT ;

   // TEST mapping has not changed
   TEST(0 == init_vmmappedregions(&mappedregions2)) ;
   TEST(size_vmmappedregions(&mappedregions2) == size_vmmappedregions(&mappedregions)) ;
   for (size_t i = 0; i < size_vmmappedregions(&mappedregions2); ++i) {
      const vm_region_t  * next = next_vmmappedregions(&mappedregions) ;
      const vm_region_t * next2 = next_vmmappedregions(&mappedregions2) ;
      TEST(next && next2) ;
      TEST(0 == compare_vmregion(next, next2)) ;
   }
   TEST(0 == next_vmmappedregions(&mappedregions)) ;
   TEST(0 == next_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == free_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;

   return 0 ;
ONABORT:
   free_vmmappedregions(&mappedregions) ;
   free_vmmappedregions(&mappedregions2) ;
   return EINVAL ;
}

#endif
