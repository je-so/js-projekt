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

   file: C-kern/api/os/virtmemory.h
    Header file of <VirtualMemory>.

   file: C-kern/os/Linux/virtmemory.c
    Linux specific implementation <VirtualMemory Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/umg/objectcache_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/math/int/power2.h"
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

/* function: sys_pagesize_vm
 * Uses sysconf(_SC_PAGESIZE) which conforms to POSIX.1-2001. */
size_t sys_pagesize_vm()
{
   size_t ps = (size_t) sysconf(_SC_PAGESIZE) ;
   return ps ;
}


int compare_vmregion( const vm_region_t * left, const vm_region_t * right )
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
   for(;;) {
      const ssize_t read_size = read( fd, buffer + buffer_offset, buffer_maxsize - buffer_offset) ;
      if (!read_size) {
         if (buffer_offset) {
            LOG_ERRTEXT(FORMAT_MISSING_ENDOFLINE(PROC_SELF_MAPS)) ;
            err = EINVAL ;
            goto ABBRUCH ;
         }
         break ; // reached end of file
      }
      if (read_size < 0) {
         err = errno ;
         if (err == EINTR) continue ;
         LOG_SYSERR("read", err) ;
         goto ABBRUCH ;
      }
      buffer_offset += (size_t)read_size ;
      do {
         if (buffer[index_newline] == '\n') break ;
         ++ index_newline ;
      } while( index_newline < buffer_offset ) ;
      if (index_newline < buffer_offset) break ; // ! found '\n' !
   }

   *buffer_size = buffer_offset ;
   *line_end    = index_newline ;
   return 0 ;
ABBRUCH:
   return err ;
}

int free_vmmappedregions( vm_mappedregions_t * mappedregions )
{
   vm_regionsarray_t    * first = mappedregions->first_array ;
   for(vm_regionsarray_t * next ; first ; first = next) {
      next = first->next ;
      free(first) ;
   }
   *mappedregions = (vm_mappedregions_t) vm_mappedregions_INIT_FREEABLE ;
   return 0 ;
}

int init_vmmappedregions( /*out*/vm_mappedregions_t * mappedregions )
{
   int err ;
   int fd = -1 ;
   vm_regionsarray_t * first_array = 0 ;
   vm_regionsarray_t * last_array  = 0 ;
   size_t      total_regions_count = 0 ;
   size_t        free_region_count = 0 ;
   vm_region_t       * next_region = 0 ;
   vm_block_t         * iobuffer   = 0 ;


   OBJC_LOCKIOBUFFER(&iobuffer) ;

   const size_t  buffer_maxsize = iobuffer->size ;
   uint8_t       * const buffer = iobuffer->addr ;
   fd = open( PROC_SELF_MAPS, O_RDONLY|O_CLOEXEC ) ;
   if (fd < 0) {
      err = ENOSYS ;
      LOG_SYSERR("open(" PROC_SELF_MAPS ")", errno) ;
      goto ABBRUCH ;
   }


   for(size_t buffer_offset = 0;;) {
      size_t line_start = 0 ;
      size_t buffer_size = 0 ;
      size_t line_end = 0 ;

      err = read_buffer( fd, buffer_maxsize, buffer, buffer_offset, &buffer_size, &line_end) ;
      if (err) goto ABBRUCH ;
      if (0 == buffer_size) break ;

      do {
         uintptr_t  start, end ;
         char  isReadable, isWriteable, isExecutable, isShared ;
         uint64_t file_offset ;
         unsigned major, minor ;
         unsigned long inode ;
         int scanned_items = sscanf( (char*)(buffer+line_start),
                  "%" SCNxPTR "-%" SCNxPTR " %c%c%c%c %llx %02x:%02x %lu ",
                  &start, &end,
                  &isReadable, &isWriteable, &isExecutable, &isShared,
                  &file_offset, &major, &minor, &inode ) ;
         if (scanned_items != 10) {
            LOG_ERRTEXT(FORMAT_WRONG(PROC_SELF_MAPS)) ;
            err = EINVAL ;
            goto ABBRUCH ;
         }

         if (!free_region_count) {
            vm_regionsarray_t * next_array ;
            next_array = MALLOC(vm_regionsarray_t,malloc,) ;
            if (!next_array) {
               err = ENOMEM ;
               goto ABBRUCH ;
            }
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

         for(line_start = ++ line_end; line_end < buffer_size; ++line_end) {
            if (buffer[line_end] == '\n') break ;
         }
      } while( line_end < buffer_size ) ;

      if (line_start < buffer_size) {
         buffer_offset = buffer_size - line_start ;
         memmove( buffer, buffer + line_start, buffer_offset) ;
      } else {
         buffer_offset = 0 ;  // scanned whole buffer
      }
   }

   err = free_filedescr(&fd) ;
   if (err) goto ABBRUCH ;

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
ABBRUCH:
   while( first_array ) {
      vm_regionsarray_t * next = first_array->next ;
      free(first_array) ;
      first_array = next ;
   }

   OBJC_UNLOCKIOBUFFER(&iobuffer) ;

   free_filedescr(&fd) ;
   LOG_ABORT(err) ;
   return err ;
}

int compare_vmmappedregions( const vm_mappedregions_t * left, const vm_mappedregions_t * right )
{
   if (left->total_count != right->total_count) {
      return left->total_count  > right->total_count ? 1 : -1 ;
   }

   vm_mappedregions_t left2  = { .first_array = left->first_array } ;
   vm_mappedregions_t right2 = { .first_array = right->first_array } ;
   gofirst_vmmappedregions(&left2) ;
   gofirst_vmmappedregions(&right2) ;

   for(size_t i = left->total_count; i != 0 ; --i) {
      const vm_region_t  * lelem = next_vmmappedregions(&left2) ;
      const vm_region_t  * relem = next_vmmappedregions(&right2) ;
      int cmp = compare_vmregion(lelem, relem) ;
      if (cmp) return cmp ;
   }

   return 0 ;
}

void gofirst_vmmappedregions( vm_mappedregions_t * iterator )
{
   vm_regionsarray_t   * first = iterator->first_array ;
   if (first) {
      iterator->element_count    = first->size ;
      iterator->element_iterator = &first->elements[0] ;
      iterator->array_iterator   = first->next ;
   }
}

const vm_region_t * next_vmmappedregions( vm_mappedregions_t * iterator )
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
         && accessmode_EXEC  == PROT_EXEC ) {   \
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

int init2_vmblock( /*out*/vm_block_t * vmblock, size_t size_in_pages, accessmode_e access_mode )
{
   int    err ;
   int    prot ;
   size_t pagesize        = pagesize_vm() ;
   size_t length_in_bytes = pagesize * size_in_pages ;

   if (length_in_bytes / pagesize != size_in_pages) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   SET_PROT(prot, access_mode)

#define shared_flags       ((access_mode & accessmode_SHARED) ? MAP_SHARED|MAP_ANONYMOUS : MAP_PRIVATE|MAP_ANONYMOUS)
   void * mapped_pages = mmap( 0, length_in_bytes, prot, shared_flags, -1, 0) ;
#undef shared_flags
   if (mapped_pages == MAP_FAILED) {
      err = errno ;
      LOG_SYSERR("mmap", err) ;
      LOG_SIZE(length_in_bytes) ;
      goto ABBRUCH ;
   }

   vmblock->addr = mapped_pages ;
   vmblock->size = length_in_bytes ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int free_vmblock( vm_block_t * vmblock )
{
   int err ;

   if (     vmblock->size
         && munmap( vmblock->addr, vmblock->size )) {
      err = errno ;
      LOG_SYSERR("munmap", err) ;
      LOG_PTR(vmblock->addr) ;
      LOG_SIZE(vmblock->size) ;
      goto ABBRUCH ;
   }

   vmblock->addr = (uint8_t*)0 ;
   vmblock->size = 0 ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int protect_vmblock( vm_block_t * vmblock, const accessmode_e access_mode )
{
   int err ;
   int prot ;

   SET_PROT(prot, access_mode)

   if (     vmblock->size
         && mprotect( vmblock->addr, vmblock->size, prot)) {
      err = errno ;
      LOG_SYSERR("mprotect", err) ;
      LOG_PTR(vmblock->addr) ;
      LOG_SIZE(vmblock->size) ;
      LOG_INT(access_mode) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int tryexpand_vmblock( vm_block_t * vmblock, size_t increment_in_pages )
{
   int err ;
   size_t    pagesize         = pagesize_vm() ;
   size_t    expand_in_bytes  = pagesize * increment_in_pages ;
   size_t    newsize_in_bytes = vmblock->size + expand_in_bytes ;
   uint8_t * new_addr ;

   if (     increment_in_pages != (expand_in_bytes / pagesize)
         || newsize_in_bytes   <  expand_in_bytes ) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   new_addr = mremap( vmblock->addr, vmblock->size, newsize_in_bytes, 0) ;
   if (MAP_FAILED == new_addr) {
      err = errno ;
      return err ; // no LOGGING
   }

   assert(new_addr == vmblock->addr) ;
   vmblock->size = newsize_in_bytes ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int movexpand_vmblock( vm_block_t * vmblock, size_t increment_in_pages )
{
   int err ;
   size_t    pagesize         = pagesize_vm() ;
   size_t    expand_in_bytes  = pagesize * increment_in_pages ;
   size_t    newsize_in_bytes = vmblock->size + expand_in_bytes ;
   uint8_t * new_addr ;

   if (     increment_in_pages != (expand_in_bytes / pagesize)
         || newsize_in_bytes   <   expand_in_bytes ) {
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   new_addr = mremap( vmblock->addr, vmblock->size, newsize_in_bytes, MREMAP_MAYMOVE) ;
   if (MAP_FAILED == new_addr) {
      err = errno ;
      LOG_OUTOFMEMORY(newsize_in_bytes) ;
      goto ABBRUCH ;
   }

   vmblock->addr = new_addr ;
   vmblock->size = newsize_in_bytes ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int shrink_vmblock( vm_block_t * vmblock, size_t decrement_in_pages )
{
   int err ;
   size_t    pagesize         = pagesize_vm() ;
   size_t    shrink_in_bytes  = pagesize * decrement_in_pages ;
   size_t    newsize_in_bytes = vmblock->size - shrink_in_bytes ;

   if (     decrement_in_pages != (shrink_in_bytes / pagesize)
         || vmblock->size      <=  shrink_in_bytes ) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (shrink_in_bytes) {
      err = munmap( vmblock->addr + newsize_in_bytes, shrink_in_bytes) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("munmap", err) ;
         LOG_PTR(vmblock->addr + newsize_in_bytes) ;
         LOG_SIZE(shrink_in_bytes) ;
         goto ABBRUCH ;
      }
   }

   vmblock->size = newsize_in_bytes ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

/* Returns:
 * 0 - All size_in_pages memory pages starting from address start are unmapped
 * 1 - All size_in_pages memory pages starting from address start are mapped
 * 2 - Wrong page permission (not read/writeable)
 * 3 - A part is mapped another is not
 * 4 - mapping query error
 * */
static int iscontained_in_mapping(const vm_block_t * mapped_block)
{
   int result = 0 ;
   vm_mappedregions_t mappedregions = vm_mappedregions_INIT_FREEABLE ;
   void              * mapped_start = mapped_block->addr ;
   void              * mapped_end   = (void*) (mapped_block->size + mapped_block->addr) ;

   if (init_vmmappedregions(&mappedregions)) {
      result = 4 ;
   }

   for(const vm_region_t * next; (next = next_vmmappedregions(&mappedregions)); ) {
      if (     mapped_start < next->endaddr
            && mapped_end   > next->addr ) {
         // overlapping
         if (next->protection != (accessmode_RDWR | accessmode_PRIVATE)) {
            printf("(%p)->protection=%d\n", (void*)(intptr_t)next, next->protection) ;
            result = 2 ;
            break ;
         }
         if ( mapped_start < next->addr ) {
            result = 3 ;
            break ;
         } else if ( mapped_end <= next->endaddr ) {
            result = 1 ;
            break ;
         } else {
            result = 3 ;
            mapped_start = next->endaddr ;
         }
      }
   }

   free_vmmappedregions(&mappedregions) ;
   return result ;
}

static int query_region(/*out*/vm_region_t * region, const vm_block_t * vmblock)
{
   int err ;
   vm_mappedregions_t mappedregions = vm_mappedregions_INIT_FREEABLE ;
   void              * mapped_start = vmblock->addr ;
   void              * mapped_end   = (void*) (vmblock->size + vmblock->addr) ;

   err = init_vmmappedregions(&mappedregions) ;
   if (err) goto ABBRUCH ;

   err = EINVAL ;

   for(const vm_region_t * next; (next = next_vmmappedregions(&mappedregions)); ) {
      if (     mapped_start >= next->addr
            && mapped_end   <= next->endaddr ) {
         // vmblock is contained in region
         *region = *next ;
         err = 0 ;
         break ;
      }
   }

ABBRUCH:
   free_vmmappedregions(&mappedregions) ;
   return err ;
}

static int compare_protection(const vm_block_t * vmblock, accessmode_e prot)
{
   int err ;
   vm_region_t region ;

   err = query_region(&region, vmblock) ;
   if (err) goto ABBRUCH ;

   if ( region.protection  != (prot | accessmode_PRIVATE) ) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
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
   for(int do_twice = 0; do_twice < 2; ++do_twice) {
      gofirst_vmmappedregions(&mappedregions2) ;
      for(int ai = 0; ai < 3; ++ai) {
         TEST( 6         == mappedregions2.total_count ) ;
         TEST( 6         == size_vmmappedregions(&mappedregions2)) ;
         TEST( &array[0] == mappedregions2.first_array ) ;
         TEST( array[ai].elements == next_vmmappedregions(&mappedregions2)) ;
         TEST( (ai < 2 ? &array[ai+1] : 0) == mappedregions2.array_iterator ) ;
         TEST( array[ai].size-1     == mappedregions2.element_count  ) ;
         TEST( array[ai].elements+1 == mappedregions2.element_iterator ) ;
         for(unsigned i = 1; i < array[ai].size; ++i) {
            TEST( &array[ai].elements[i] == next_vmmappedregions(&mappedregions2)) ;
         }
         TEST( 0 == mappedregions2.element_count  ) ;
         TEST( array[ai].elements+array[ai].size == mappedregions2.element_iterator ) ;
      }
      TEST( 0 == next_vmmappedregions(&mappedregions2)) ;
   }

   err = 0 ;
ABBRUCH:
   free_vmmappedregions(&mappedregions) ;
   return err ;
}

static int test_mapping(void)
{
   vm_block_t   mapped_block ;
   vm_block_t   unmapped_block ;
   size_t       size_in_pages ;

   // TEST query
   TEST(pagesize_vm() >= 1) ;
   TEST(pagesize_vm() == makepowerof2_int(pagesize_vm())) ;
   TEST(ispowerof2_int(pagesize_vm())) ;

   // TEST map, unmap
   size_in_pages = 1 ;
   TEST(0 == init_vmblock(&mapped_block, size_in_pages)) ;
   TEST(mapped_block.addr && mapped_block.size == size_in_pages * pagesize_vm()) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmblock(&unmapped_block)) ;
   TEST(! unmapped_block.addr && ! unmapped_block.size ) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map, shrink, expand, unmap in (50 pages) loop
   size_in_pages = 50 ;
   TEST(0 == init_vmblock(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for(size_t i = 1; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vm_block_t upperhalf = { mapped_block.addr + unmapoffset, mapped_block.size - unmapoffset } ;
      vm_block_t lowerhalf = mapped_block ;
      TEST(0 == shrink_vmblock(&lowerhalf, size_in_pages-i)) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(lowerhalf.size == unmapoffset) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      TEST(0 == tryexpand_vmblock(&lowerhalf, size_in_pages-i)) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(lowerhalf.size == mapped_block.size) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmblock(&unmapped_block)) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map, movexpand, unmap in (50 pages) loop
   size_in_pages = 50 ;
   TEST(0 == init_vmblock(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for(size_t i = 1; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vm_block_t upperhalf = { mapped_block.addr + unmapoffset, mapped_block.size - unmapoffset } ;
      vm_block_t lowerhalf = { mapped_block.addr, unmapoffset } ;
      unmapped_block = upperhalf ;
      TEST(0 == free_vmblock(&unmapped_block)) ;
      TEST(! unmapped_block.addr && ! unmapped_block.size) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      TEST(0 == movexpand_vmblock(&lowerhalf, size_in_pages-i)) ;
      TEST(mapped_block.addr == lowerhalf.addr) ;
      TEST(mapped_block.size == lowerhalf.size) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for(size_t i = 2; i < size_in_pages; ++i) {
      size_t   unmapoffset = i * pagesize_vm() ;
      vm_block_t upperhalf = { mapped_block.addr + unmapoffset, mapped_block.size - unmapoffset } ;
      vm_block_t lowerhalf = { mapped_block.addr, unmapoffset } ;
      TEST(0 == shrink_vmblock(&lowerhalf, 1)) ;
      TEST(1 == iscontained_in_mapping(&upperhalf)) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      unmapped_block = (vm_block_t) { mapped_block.addr + unmapoffset - pagesize_vm(), pagesize_vm() } ;
      TEST(0 == iscontained_in_mapping(&unmapped_block)) ;
      TEST(0 == movexpand_vmblock(&lowerhalf, 1)) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(lowerhalf.size == unmapoffset) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmblock(&unmapped_block)) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST movexpand (move)
   size_in_pages = 50 ;
   for(size_t i = 2; i < size_in_pages; ++i) {
      TEST(0 == init_vmblock(&mapped_block, size_in_pages)) ;
      TEST(1 == iscontained_in_mapping(&mapped_block)) ;
      size_t   unmapoffset = i * pagesize_vm() ;
      vm_block_t upperhalf = { mapped_block.addr + unmapoffset, mapped_block.size - unmapoffset } ;
      vm_block_t lowerhalf = { mapped_block.addr, unmapoffset  } ;
      TEST(0 == shrink_vmblock(&lowerhalf, 1)) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(lowerhalf.size == unmapoffset - pagesize_vm()) ;
      TEST(1 == iscontained_in_mapping(&upperhalf)) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      unmapped_block = (vm_block_t) { mapped_block.addr + unmapoffset - pagesize_vm(), pagesize_vm() } ;
      TEST(0 == iscontained_in_mapping(&unmapped_block)) ;
      TEST(0 == movexpand_vmblock(&lowerhalf, 2)) ;
      TEST(lowerhalf.addr != mapped_block.addr) ;
      TEST(lowerhalf.size == unmapoffset + pagesize_vm()) ;
      TEST(0 == free_vmblock(&lowerhalf)) ;
      TEST(0 == free_vmblock(&upperhalf)) ;
      TEST(0 == iscontained_in_mapping(&mapped_block)) ;
   }

   // TEST ENOMEM tryexpand
   size_in_pages = 10 ;
   TEST(0 == init_vmblock(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   {
      size_t   unmapoffset = 7 * pagesize_vm() ;
      vm_block_t upperhalf = { mapped_block.addr + unmapoffset,  mapped_block.size - unmapoffset } ;
      vm_block_t lowerhalf = mapped_block ;
      TEST(0 == shrink_vmblock(&lowerhalf, 3)) ;
      TEST(lowerhalf.size == unmapoffset) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      for(size_t i = 1; i < 7; ++i) {
         vm_block_t ext_block = { mapped_block.addr, i * pagesize_vm() } ;
         TEST(ENOMEM == tryexpand_vmblock(&ext_block, 3)) ;
         TEST(ext_block.addr == mapped_block.addr) ;
         TEST(ext_block.size == i * pagesize_vm()) ;
         TEST(1 == iscontained_in_mapping(&ext_block)) ;
         TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
         TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      }
      TEST(0 == tryexpand_vmblock(&lowerhalf, 3)) ;
      TEST(lowerhalf.size == mapped_block.size) ;
      TEST(lowerhalf.addr == mapped_block.addr) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmblock(&unmapped_block)) ;
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map of already mapped
   size_in_pages = 3 ;
   TEST(0 == init_vmblock(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for(size_t i = 0; i < size_in_pages; ++i) {
      size_t     lowersize = i * pagesize_vm() ;
      vm_block_t lowerhalf = { mapped_block.addr, lowersize } ;
      TEST(ENOMEM == tryexpand_vmblock(&lowerhalf, 1)) ;
   }

   // TEST unmap of already unmapped (no error)
   unmapped_block = mapped_block ;
   TEST(0 == free_vmblock(&unmapped_block)) ;
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == free_vmblock(&unmapped_block)) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST unmap empty block
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;
   TEST(0 == free_vmblock(&unmapped_block)) ;
   TEST(! unmapped_block.addr && ! unmapped_block.size) ;

   // TEST EINVAL
   TEST(0      == init_vmblock(&mapped_block, 1)) ;
   TEST(EINVAL == tryexpand_vmblock(&mapped_block, (size_t)-1)) ;
   TEST(EINVAL == shrink_vmblock(&mapped_block, 1)) ;
   TEST(0      == free_vmblock(&mapped_block)) ;
   TEST(EINVAL == init_vmblock(&mapped_block, (size_t)-1)) ;

   // TEST ENOMEM movexpand
   TEST(0      == init_vmblock(&mapped_block, 1)) ;
   TEST(ENOMEM == movexpand_vmblock(&mapped_block, (size_t)-1)) ;
   TEST(ENOMEM == movexpand_vmblock(&mapped_block, (((size_t)-1) / pagesize_vm()) - 10)) ;
   TEST(0      == free_vmblock(&mapped_block)) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

static ucontext_t    s_usercontext ;

static void sigsegfault(int _signr)
{
   (void) _signr ;
   setcontext(&s_usercontext) ;
}

static int test_protection(void)
{
   vm_block_t           vmblock  = vm_block_INIT_FREEABLE ;
   bool                 isOldact = false ;
   struct sigaction     oldact ;
   volatile int         is_exception ;

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
   for(unsigned i = 0; i < nrelementsof(prot); ++i) {
      // TEST init2 generates correct protection
      TEST(0 == init2_vmblock(&vmblock, 2, prot[i])) ;
      TEST(0 == compare_protection(&vmblock, prot[i])) ;
      TEST(0 == free_vmblock(&vmblock)) ;
      // TEST init generates RW protection
      TEST(0 == init_vmblock(&vmblock, 2)) ;
      TEST(0 == compare_protection(&vmblock, accessmode_RDWR)) ;
      // TEST setting protection
      TEST(0 == protect_vmblock(&vmblock, prot[i])) ;
      TEST(0 == compare_protection(&vmblock, prot[i])) ;
      // TEST shrink does not change flags
      TEST(0 == shrink_vmblock(&vmblock, 1)) ;
      TEST(0 == compare_protection(&vmblock, prot[i])) ;
      // TEST expand does not change flags
      TEST(0 == tryexpand_vmblock(&vmblock, 1)) ;
      TEST(0 == compare_protection(&vmblock, prot[i])) ;
      // TEST movexpand does not change flags
      TEST(0 == movexpand_vmblock(&vmblock, 10)) ;
      TEST(0 == compare_protection(&vmblock, prot[i])) ;
      TEST(0 == free_vmblock(&vmblock)) ;
   }

   // TEST write of readonly page is not possible
   TEST(0 == init_vmblock(&vmblock, 1)) ;
   TEST(0 == protect_vmblock(&vmblock, accessmode_READ)) ;
   is_exception = 0 ;
   TEST(0 == getcontext(&s_usercontext)) ;
   if (!is_exception) {
      is_exception = 1 ;
      vmblock.addr[0] = 0xff ;
      is_exception = 2 ;
   }
   TEST(1 == is_exception) ;
   TEST(0 == free_vmblock(&vmblock)) ;

   // TEST read of not accessible page is not possible
   TEST(0 == init2_vmblock(&vmblock, 1, accessmode_NONE)) ;
   is_exception = 0 ;
   TEST(0 == getcontext(&s_usercontext)) ;
   if (!is_exception) {
      is_exception = 1 ;
      is_exception = vmblock.addr[0] ;
      is_exception = 2 ;
   }
   TEST(1 == is_exception) ;
   TEST(0 == free_vmblock(&vmblock)) ;

   // uninstall exception handler
   TEST(0 == sigaction(SIGSEGV, &oldact, 0)) ;

   return 0 ;
ABBRUCH:
   if (isOldact) sigaction(SIGSEGV, &oldact, 0) ;
   (void) free_vmblock(&vmblock) ;
   return 1 ;
}

int unittest_os_virtualmemory()
{
   vm_mappedregions_t mappedregions  = vm_mappedregions_INIT_FREEABLE ;
   vm_mappedregions_t mappedregions2 = vm_mappedregions_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_vmmappedregions(&mappedregions)) ;

   if (test_mappedregions())  goto ABBRUCH ;
   if (test_mapping())        goto ABBRUCH ;
   if (test_protection())     goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == init_vmmappedregions(&mappedregions2)) ;
   TEST(size_vmmappedregions(&mappedregions2) == size_vmmappedregions(&mappedregions)) ;
   for(size_t i = 0; i < size_vmmappedregions(&mappedregions2); ++i) {
      const vm_region_t  * next = next_vmmappedregions(&mappedregions) ;
      const vm_region_t * next2 = next_vmmappedregions(&mappedregions2) ;
      TEST(next && next2) ;
      TEST(0 == compare_vmregion( next, next2 )) ;
   }
   TEST(0 == next_vmmappedregions(&mappedregions)) ;
   TEST(0 == next_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == free_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;

   return 0 ;
ABBRUCH:
   free_vmmappedregions(&mappedregions) ;
   free_vmmappedregions(&mappedregions2) ;
   return 1 ;
}

#endif
