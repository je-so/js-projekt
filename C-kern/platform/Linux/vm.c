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
#include "C-kern/api/io/filesystem/file.h"
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


static int read_buffer(int fd, const size_t buffer_maxsize, uint8_t buffer[buffer_maxsize], size_t buffer_offset, /*out*/size_t * buffer_size, /*out*/size_t * line_end)
{
   int err = 0 ;
   size_t index_newline = buffer_offset ;
   for (;;) {
      const ssize_t read_size = read(fd, buffer + buffer_offset, buffer_maxsize - buffer_offset) ;
      if (!read_size) {
         if (buffer_offset) {
            TRACEERR_LOG(FILE_FORMAT_MISSING_ENDOFLINE,PROC_SELF_MAPS) ;
            err = EINVAL ;
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
   vmpage_t          *  iobuffer    = 0 ;

   OBJC_LOCKIOBUFFER(&iobuffer) ;

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
            TRACEERR_LOG(FILE_FORMAT_WRONG,PROC_SELF_MAPS) ;
            err = EINVAL ;
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

   err = free_file(&fd) ;
   if (err) goto ONABORT ;

   OBJC_UNLOCKIOBUFFER(&iobuffer) ;

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
   OBJC_UNLOCKIOBUFFER(&iobuffer) ;
   free_file(&fd) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

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

bool iscontained_vmmappedregions(vm_mappedregions_t * mappedregions, const vmpage_t * mblock, accessmode_e protection)
{
   vm_mappedregions_t   iterator    = *mappedregions ;
   void                 * startaddr = (void*) mblock->addr ;
   void                 * endaddr   = (void*) (mblock->addr + mblock->size) ;

   gofirst_vmmappedregions(&iterator) ;

   for (const vm_region_t * vmregion; (vmregion = next_vmmappedregions(&iterator));) {
      if (  vmregion->protection == protection
         && startaddr < vmregion->endaddr
         && endaddr   > vmregion->addr) {
         if (vmregion->addr <= startaddr) {
            if (endaddr <= vmregion->endaddr) {
               return true ;
            }
            startaddr = vmregion->endaddr ;
         } else if (endaddr <= vmregion->endaddr) {
            endaddr = vmregion->addr ;
         }
      }
   }

   return false ;
}

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

#define SET_PROT(prot, access_mode) \
   static_assert(0 == accessmode_NONE, "") ;    \
   static_assert(0 == PROT_NONE, "") ;          \
   if (     accessmode_READ  == PROT_READ       \
         && accessmode_WRITE == PROT_WRITE      \
         && accessmode_EXEC  == PROT_EXEC) {    \
      prot  = access_mode ;                     \
      prot &= (accessmode_RDWR|accessmode_EXEC);\
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

int init2_vmpage(/*out*/vmpage_t * vmpage, size_t size_in_pages, accessmode_e access_mode)
{
   int    err ;
   int    prot ;
   size_t pagesize        = pagesize_vm() ;
   size_t length_in_bytes = pagesize * size_in_pages ;

   if (length_in_bytes / pagesize != size_in_pages) {
      err = EINVAL ;
      goto ONABORT ;
   }

   SET_PROT(prot, access_mode)

#define shared_flags       ((access_mode & accessmode_SHARED) ? MAP_SHARED|MAP_ANONYMOUS : MAP_PRIVATE|MAP_ANONYMOUS)
   void * mapped_pages = mmap(0, length_in_bytes, prot, shared_flags, -1, 0) ;
#undef shared_flags
   if (mapped_pages == MAP_FAILED) {
      err = errno ;
      TRACESYSERR_LOG("mmap", err) ;
      PRINTSIZE_LOG(length_in_bytes) ;
      goto ONABORT ;
   }

   vmpage->addr = mapped_pages ;
   vmpage->size = length_in_bytes ;

   return 0 ;
ONABORT:
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
   size_t    pagesize         = pagesize_vm() ;
   size_t    expand_in_bytes  = pagesize * increment_in_pages ;
   size_t    newsize_in_bytes = vmpage->size + expand_in_bytes ;
   uint8_t * new_addr ;

   if (  increment_in_pages != (expand_in_bytes / pagesize)
         || newsize_in_bytes   <  expand_in_bytes) {
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
   size_t    pagesize         = pagesize_vm() ;
   size_t    expand_in_bytes  = pagesize * increment_in_pages ;
   size_t    newsize_in_bytes = vmpage->size + expand_in_bytes ;
   uint8_t * new_addr ;

   if (  increment_in_pages  != (expand_in_bytes / pagesize)
         || newsize_in_bytes <   expand_in_bytes) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   new_addr = mremap(vmpage->addr, vmpage->size, newsize_in_bytes, MREMAP_MAYMOVE) ;
   if (MAP_FAILED == new_addr) {
      err = errno ;
      TRACEOUTOFMEM_LOG(newsize_in_bytes) ;
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
   size_t    pagesize         = pagesize_vm() ;
   size_t    shrink_in_bytes  = pagesize * decrement_in_pages ;
   size_t    newsize_in_bytes = vmpage->size - shrink_in_bytes ;

   if (  decrement_in_pages != (shrink_in_bytes / pagesize)
         || vmpage->size    <=  shrink_in_bytes) {
      err = EINVAL ;
      goto ONABORT ;
   }

   if (shrink_in_bytes) {
      err = munmap(vmpage->addr + newsize_in_bytes, shrink_in_bytes) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("munmap", err) ;
         PRINTPTR_LOG(vmpage->addr + newsize_in_bytes) ;
         PRINTSIZE_LOG(shrink_in_bytes) ;
         goto ONABORT ;
      }
   }

   vmpage->size = newsize_in_bytes ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

/* function: iscontained_in_mapping
 * Returns:
 * true  - mapped_block is contained in <vm_mappedregions_t> with protection (accessmode_RDWR|accessmode_PRIVATE).
 * false - *mapped_block* is either not contained or has the wrong accessmode.
 * */
static bool iscontained_in_mapping(const vmpage_t * mapped_block)
{
   vm_mappedregions_t mappedregions = vm_mappedregions_INIT_FREEABLE ;

   if (init_vmmappedregions(&mappedregions)) {
      return false ;
   }

   bool ismapped = iscontained_vmmappedregions(&mappedregions, mapped_block, (accessmode_RDWR | accessmode_PRIVATE)) ;

   if (free_vmmappedregions(&mappedregions)) {
      return false ;
   }

   return ismapped ;
}

static int query_region(/*out*/vm_region_t * region, const vmpage_t * vmpage)
{
   int err ;
   vm_mappedregions_t mappedregions = vm_mappedregions_INIT_FREEABLE ;
   void              * mapped_start = vmpage->addr ;
   void              * mapped_end   = (void*) (vmpage->size + vmpage->addr) ;

   err = init_vmmappedregions(&mappedregions) ;
   if (err) goto ONABORT ;

   err = EINVAL ;

   for (const vm_region_t * next; (next = next_vmmappedregions(&mappedregions));) {
      if (  mapped_start  >= next->addr
            && mapped_end <= next->endaddr) {
         // vmpage is contained in region
         *region = *next ;
         err = 0 ;
         break ;
      }
   }

ONABORT:
   free_vmmappedregions(&mappedregions) ;
   return err ;
}

static int compare_protection(const vmpage_t * vmpage, accessmode_e prot)
{
   int err ;
   vm_region_t region ;

   err = query_region(&region, vmpage) ;
   if (err) goto ONABORT ;

   if (region.protection != (prot | accessmode_PRIVATE)) {
      err = EINVAL ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return err ;
}

static int test_mappedregions(void)
{
   int err = 1 ;
   vm_mappedregions_t mappedregions  = vm_mappedregions_INIT_FREEABLE ;

   // TEST query empty buffer
   TEST(0 == next_vmmappedregions(&mappedregions)) ;
   TEST(0 == size_vmmappedregions(&mappedregions)) ;
   gofirst_vmmappedregions(&mappedregions) ;

   // TEST init, double free
   TEST(0 == init_vmmappedregions(&mappedregions)) ;
   TEST(16 == mappedregions.element_count) ;          // see vm_regionsarray_t.elements
   TEST(mappedregions.total_count >= mappedregions.element_count) ;
   TEST(mappedregions.first_array) ;
   TEST(mappedregions.first_array->next == mappedregions.array_iterator) ;
   TEST(mappedregions.element_iterator == &mappedregions.first_array->elements[0]) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(!mappedregions.first_array && !mappedregions.element_iterator && !mappedregions.element_count) ;
   TEST(!mappedregions.array_iterator && !mappedregions.total_count) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(!mappedregions.first_array && !mappedregions.element_iterator && !mappedregions.element_count) ;
   TEST(!mappedregions.array_iterator && !mappedregions.total_count) ;

   // TEST iterator
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

   err = 0 ;
ONABORT:
   free_vmmappedregions(&mappedregions) ;
   return err ;
}

static int test_vmpage(void)
{
   vmpage_t mapped_block = vmpage_INIT_FREEABLE ;
   vmpage_t unmapped_block ;
   size_t   size_in_pages ;

   // TEST vmpage_INIT_FREEABLE
   TEST(0 == mapped_block.addr) ;
   TEST(0 == mapped_block.size) ;

   // TEST vmpage_INIT
   mapped_block = (vmpage_t) vmpage_INIT(11, (uint8_t*)10) ;
   TEST(mapped_block.addr == (uint8_t*)10) ;
   TEST(mapped_block.size == 11) ;

   // TEST pagesize_vm
   TEST(pagesize_vm() >= 256) ;
   TEST(ispowerof2_int(pagesize_vm())) ;

   // TEST isfree_vmpage
   mapped_block = (vmpage_t) vmpage_INIT_FREEABLE ;
   TEST(1 == isfree_vmpage(&mapped_block)) ;
   mapped_block.addr = (uint8_t*)1 ;
   TEST(1 == isfree_vmpage(&mapped_block)) ;
   mapped_block.size = 1 ;
   TEST(0 == isfree_vmpage(&mapped_block)) ;
   mapped_block.addr = 0 ;
   TEST(0 == isfree_vmpage(&mapped_block)) ;
   mapped_block.size = 0 ;
   TEST(1 == isfree_vmpage(&mapped_block)) ;

   // TEST genericcast_vmpage
   struct {
      uint8_t *   test_addr ;
      size_t      test_size ;
   } genericpage ;
   TEST(genericcast_vmpage(&genericpage, test_) == (vmpage_t*)&genericpage) ;

   // TEST map, unmap
   size_in_pages = 1 ;
   TEST(0 == init_vmpage(&mapped_block, size_in_pages)) ;
   TEST(mapped_block.addr && mapped_block.size == size_in_pages * pagesize_vm()) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmpage(&unmapped_block)) ;
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map, shrink, expand, unmap in (50 pages) loop
   size_in_pages = 50 ;
   TEST(0 == init_vmpage(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for (size_t i = 1; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vmpage_t upperhalf = { mapped_block.addr + unmapoffset, mapped_block.size - unmapoffset } ;
      vmpage_t lowerhalf = mapped_block ;
      TEST(0 == shrink_vmpage(&lowerhalf, size_in_pages-i)) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(lowerhalf.size == unmapoffset) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      TEST(0 == tryexpand_vmpage(&lowerhalf, size_in_pages-i)) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(lowerhalf.size == mapped_block.size) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmpage(&unmapped_block)) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map, movexpand, unmap in (50 pages) loop
   size_in_pages = 50 ;
   TEST(0 == init_vmpage(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for (size_t i = 1; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vmpage_t upperhalf = { mapped_block.addr + unmapoffset, mapped_block.size - unmapoffset } ;
      vmpage_t lowerhalf = { mapped_block.addr, unmapoffset } ;
      unmapped_block = upperhalf ;
      TEST(0 == free_vmpage(&unmapped_block)) ;
      TEST(! unmapped_block.addr && ! unmapped_block.size) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      TEST(0 == movexpand_vmpage(&lowerhalf, size_in_pages-i)) ;
      TEST(mapped_block.addr == lowerhalf.addr) ;
      TEST(mapped_block.size == lowerhalf.size) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for (size_t i = 2; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vmpage_t upperhalf = { mapped_block.addr + unmapoffset, mapped_block.size - unmapoffset } ;
      vmpage_t lowerhalf = { mapped_block.addr, unmapoffset } ;
      TEST(0 == shrink_vmpage(&lowerhalf, 1)) ;
      TEST(1 == iscontained_in_mapping(&upperhalf)) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      unmapped_block = (vmpage_t) { mapped_block.addr + unmapoffset - pagesize_vm(), pagesize_vm() } ;
      TEST(0 == iscontained_in_mapping(&unmapped_block)) ;
      TEST(0 == movexpand_vmpage(&lowerhalf, 1)) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(lowerhalf.size == unmapoffset) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmpage(&unmapped_block)) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST movexpand (move)
   size_in_pages = 50 ;
   for (size_t i = 2; i < size_in_pages; ++i) {
      TEST(0 == init_vmpage(&mapped_block, size_in_pages)) ;
      TEST(1 == iscontained_in_mapping(&mapped_block)) ;
      size_t   unmapoffset = i * pagesize_vm() ;
      vmpage_t upperhalf = { mapped_block.addr + unmapoffset, mapped_block.size - unmapoffset } ;
      vmpage_t lowerhalf = { mapped_block.addr, unmapoffset  } ;
      TEST(0 == shrink_vmpage(&lowerhalf, 1)) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(lowerhalf.size == unmapoffset - pagesize_vm()) ;
      TEST(1 == iscontained_in_mapping(&upperhalf)) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      unmapped_block = (vmpage_t) { mapped_block.addr + unmapoffset - pagesize_vm(), pagesize_vm() } ;
      TEST(0 == iscontained_in_mapping(&unmapped_block)) ;
      TEST(0 == movexpand_vmpage(&lowerhalf, 2)) ;
      TEST(lowerhalf.addr != mapped_block.addr) ;
      TEST(lowerhalf.size == unmapoffset + pagesize_vm()) ;
      TEST(0 == free_vmpage(&lowerhalf)) ;
      TEST(0 == free_vmpage(&upperhalf)) ;
      TEST(0 == iscontained_in_mapping(&mapped_block)) ;
   }

   // TEST ENOMEM tryexpand
   size_in_pages = 10 ;
   TEST(0 == init_vmpage(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   {
      size_t   unmapoffset = 7 * pagesize_vm() ;
      vmpage_t upperhalf = { mapped_block.addr + unmapoffset,  mapped_block.size - unmapoffset } ;
      vmpage_t lowerhalf = mapped_block ;
      TEST(0 == shrink_vmpage(&lowerhalf, 3)) ;
      TEST(lowerhalf.size == unmapoffset) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      for (size_t i = 1; i < 7; ++i) {
         vmpage_t ext_block = { mapped_block.addr, i * pagesize_vm() } ;
         TEST(ENOMEM == tryexpand_vmpage(&ext_block, 3)) ;
         TEST(ext_block.addr == mapped_block.addr) ;
         TEST(ext_block.size == i * pagesize_vm()) ;
         TEST(1 == iscontained_in_mapping(&ext_block)) ;
         TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
         TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      }
      TEST(0 == tryexpand_vmpage(&lowerhalf, 3)) ;
      TEST(lowerhalf.size == mapped_block.size) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmpage(&unmapped_block)) ;
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map of already mapped
   size_in_pages = 3 ;
   TEST(0 == init_vmpage(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for (size_t i = 0; i < size_in_pages; ++i) {
      size_t     lowersize = i * pagesize_vm() ;
      vmpage_t lowerhalf = { mapped_block.addr, lowersize } ;
      TEST(ENOMEM == tryexpand_vmpage(&lowerhalf, 1)) ;
   }

   // TEST unmap of already unmapped (no error)
   unmapped_block = mapped_block ;
   TEST(0 == free_vmpage(&unmapped_block)) ;
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmpage(&unmapped_block)) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST unmap empty block
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;
   TEST(0 == free_vmpage(&unmapped_block)) ;
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;

   // TEST EINVAL
   TEST(0      == init_vmpage(&mapped_block, 1)) ;
   TEST(EINVAL == tryexpand_vmpage(&mapped_block, (size_t)-1)) ;
   TEST(EINVAL == shrink_vmpage(&mapped_block, 1)) ;
   TEST(0      == free_vmpage(&mapped_block)) ;
   TEST(EINVAL == init_vmpage(&mapped_block, (size_t)-1)) ;

   // TEST ENOMEM movexpand
   TEST(0      == init_vmpage(&mapped_block, 1)) ;
   TEST(ENOMEM == movexpand_vmpage(&mapped_block, (size_t)-1)) ;
   TEST(ENOMEM == movexpand_vmpage(&mapped_block, (((size_t)-1) / pagesize_vm()) - 10)) ;
   TEST(0      == free_vmpage(&mapped_block)) ;

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
   accessmode_e prot[6] = { accessmode_RDWR, accessmode_WRITE, accessmode_READ, accessmode_READ|accessmode_EXEC, accessmode_RDWR|accessmode_EXEC, accessmode_NONE } ;
   for (unsigned i = 0; i < lengthof(prot); ++i) {
      // TEST init2 generates correct protection
      TEST(0 == init2_vmpage(&vmpage, 2, prot[i])) ;
      TEST(0 == compare_protection(&vmpage, prot[i])) ;
      TEST(0 == free_vmpage(&vmpage)) ;
      // TEST init generates RW protection
      TEST(0 == init_vmpage(&vmpage, 2)) ;
      TEST(0 == compare_protection(&vmpage, accessmode_RDWR)) ;
      // TEST setting protection
      TEST(0 == protect_vmpage(&vmpage, prot[i])) ;
      TEST(0 == compare_protection(&vmpage, prot[i])) ;
      // TEST shrink does not change flags
      TEST(0 == shrink_vmpage(&vmpage, 1)) ;
      TEST(0 == compare_protection(&vmpage, prot[i])) ;
      // TEST expand does not change flags
      TEST(0 == tryexpand_vmpage(&vmpage, 1)) ;
      TEST(0 == compare_protection(&vmpage, prot[i])) ;
      // TEST movexpand does not change flags
      TEST(0 == movexpand_vmpage(&vmpage, 10)) ;
      TEST(0 == compare_protection(&vmpage, prot[i])) ;
      TEST(0 == free_vmpage(&vmpage)) ;
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
