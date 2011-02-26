/* title: VirtualMemory Implementation
   Implementation of managing virtual memory pages.

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
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/generic/integer.h"
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

struct virtmemory_regionsarray_t
{
   virtmemory_regionsarray_t  * next ;
   size_t                     size ;
   virtmemory_region_t        elements[16] ;
} ;

static __thread  virtmemory_block_t st_rootbuffer = virtmemory_block_INIT_FREEABLE ;

int compare_virtmemory_region( const virtmemory_region_t * left, const virtmemory_region_t * right )
{
#define RETURN_ON_UNEQUAL(name)  \
   if (left->name != right->name) { \
      return left->name > right->name ? 1 : -1 ; \
   }

   RETURN_ON_UNEQUAL(start) ;
   RETURN_ON_UNEQUAL(end) ;
   RETURN_ON_UNEQUAL(isReadable) ;
   RETURN_ON_UNEQUAL(isWriteable) ;
   RETURN_ON_UNEQUAL(isExecutable) ;
   RETURN_ON_UNEQUAL(isShared) ;

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
            LOG_TEXT(FORMAT_MISSING_ENDOFLINE(PROC_SELF_MAPS)) ;
            err = EINVAL ;
            goto ABBRUCH ;
         }
         break ; // reached end of file
      }
      if (read_size < 0) {
         err = errno ;
         if (err == EINTR) continue ;
         LOG_SYSERRNO("read") ;
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

int free_virtmemory_mappedregions( virtmemory_mappedregions_t * mappedregions )
{
   virtmemory_regionsarray_t    * first = mappedregions->first_array ;
   for(virtmemory_regionsarray_t * next ; first ; first = next) {
      next = first->next ;
      free(first) ;
   }
   *mappedregions = (virtmemory_mappedregions_t) virtmemory_mappedregions_INIT_FREEABLE ;
   return 0 ;
}

int init_virtmemory_mappedregions( /*out*/virtmemory_mappedregions_t * mappedregions )
{
   int err ;
   int fd = -1 ;
   virtmemory_regionsarray_t  * first_array = 0 ;
   virtmemory_regionsarray_t  * last_array= 0 ;
   size_t                     total_regions_count = 0 ;
   size_t                     free_region_count = 0 ;
   virtmemory_region_t        * next_region = 0 ;

   if (!st_rootbuffer.start) {
      err = map_virtmemory(&st_rootbuffer, 1) ;
      if (err) goto ABBRUCH ;
   }

   const size_t  buffer_maxsize = st_rootbuffer.size_in_bytes ;
   uint8_t       * const buffer = st_rootbuffer.start ;
   fd = open( PROC_SELF_MAPS, O_RDONLY|O_CLOEXEC ) ;
   if (fd < 0) {
      LOG_SYSERRNO("open") ;
      LOG_STRING(PROC_SELF_MAPS) ;
      err = ENOSYS ;
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
            LOG_TEXT(FORMAT_WRONG(PROC_SELF_MAPS)) ;
            err = EINVAL ;
            goto ABBRUCH ;
         }

         if (!free_region_count) {
            virtmemory_regionsarray_t * next_array ;
            next_array = (virtmemory_regionsarray_t*) malloc(sizeof(virtmemory_regionsarray_t)) ;
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

         next_region->start = (void *) start ;
         next_region->end   = (void *) end ;
         next_region->isReadable   = (isReadable == 'r') ;
         next_region->isWriteable  = (isWriteable == 'w') ;
         next_region->isExecutable = (isExecutable == 'x') ;
         next_region->isShared     = (isShared == 's') ;

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

   close(fd) ;

   mappedregions->total_count      = total_regions_count ;
   mappedregions->element_count    = 0 ;
   mappedregions->element_iterator = 0 ;
   mappedregions->array_iterator   = 0 ;
   mappedregions->first_array      = first_array ;
   if (free_region_count) {
      last_array->size            -= free_region_count ;
   }
   gofirst_virtmemory_mappedregions(mappedregions) ;
   return 0 ;
ABBRUCH:
   while (first_array) {
      virtmemory_regionsarray_t * next = first_array->next ;
      free(first_array) ;
      first_array = next ;
   }

   close(fd) ;
   LOG_ABORT(err) ;
   return err ;
}

int compare_virtmemory_mappedregions( const virtmemory_mappedregions_t * left, const virtmemory_mappedregions_t * right )
{
   if (left->total_count != right->total_count) {
      return left->total_count  > right->total_count ? 1 : -1 ;
   }

   virtmemory_mappedregions_t left2  = { .first_array = left->first_array } ;
   virtmemory_mappedregions_t right2 = { .first_array = right->first_array } ;
   gofirst_virtmemory_mappedregions(&left2) ;
   gofirst_virtmemory_mappedregions(&right2) ;

   for(size_t i = left->total_count; i != 0 ; --i) {
      const virtmemory_region_t  * lelem = next_virtmemory_mappedregions(&left2) ;
      const virtmemory_region_t  * relem = next_virtmemory_mappedregions(&right2) ;
      int cmp = compare_virtmemory_region(lelem, relem) ;
      if (cmp) return cmp ;
   }

   return 0 ;
}

void gofirst_virtmemory_mappedregions( virtmemory_mappedregions_t * iterator )
{
   virtmemory_regionsarray_t   * first = iterator->first_array ;
   if (first) {
      iterator->element_count    = first->size ;
      iterator->element_iterator = &first->elements[0] ;
      iterator->array_iterator   = first->next ;
   }
}

const virtmemory_region_t * next_virtmemory_mappedregions( virtmemory_mappedregions_t * iterator )
{
   if (!iterator->element_count) {
      virtmemory_regionsarray_t  * next = iterator->array_iterator ;
      if (!next) return 0 ;
      iterator->element_count     = next->size ;
      iterator->element_iterator  = &next->elements[0] ;
      iterator->array_iterator    = next->next ;
   }

   -- iterator->element_count ;
   return iterator->element_iterator ++ ;
}

int map_virtmemory( /*out*/virtmemory_block_t * new_mapped_block, size_t size_in_pages )
{
   int    err ;
   size_t pagesize        = pagesize_virtmemory() ;
   size_t length_in_bytes = pagesize * size_in_pages ;

   if (length_in_bytes / pagesize != size_in_pages) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   void * mapped_pages = mmap( 0, length_in_bytes, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) ;
   if (mapped_pages == MAP_FAILED) {
      err = errno ;
      LOG_SYSERRNO("mmap") ;
      LOG_SIZE(length_in_bytes) ;
      goto ABBRUCH ;
   }

   new_mapped_block->start         = mapped_pages ;
   new_mapped_block->size_in_bytes = length_in_bytes ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int maporextend_virtmemory( /*out*/virtmemory_block_t * new_mapped_block, size_t size_in_pages, virtmemory_block_t * ext_block)
{
   int     err ;
   size_t  pagesize         = pagesize_virtmemory() ;
   size_t  length_in_bytes  = pagesize * size_in_pages ;
   uint8_t *  extended_addr = ext_block->start + ext_block->size_in_bytes ;

   if (  length_in_bytes / pagesize != size_in_pages
         || extended_addr < ext_block->start ) {
      err = EINVAL ;
      goto ABBRUCH ;
   }
   void * mapped_pages = mmap( extended_addr, length_in_bytes, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) ;
   if (mapped_pages == MAP_FAILED) {
      err = errno ;
      LOG_SYSERRNO("mmap") ;
      LOG_PTR(extended_addr) ;
      LOG_SIZE(length_in_bytes) ;
      goto ABBRUCH ;
   }

   if (mapped_pages != extended_addr) {
      new_mapped_block->start         = mapped_pages ;
      new_mapped_block->size_in_bytes = length_in_bytes ;
   } else {
      new_mapped_block->start         = 0 ;
      new_mapped_block->size_in_bytes = 0 ;
      ext_block->size_in_bytes       += length_in_bytes ;
   }
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int extend_virtmemory( virtmemory_block_t * ext_block, size_t increment_in_pages )
{
   int err ;
   virtmemory_block_t new_mapped_block ;

   err = maporextend_virtmemory( &new_mapped_block, increment_in_pages, ext_block) ;
   if (err) goto ABBRUCH ;

   if (new_mapped_block.size_in_bytes) {
      (void) unmap_virtmemory(&new_mapped_block) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int unmap_virtmemory( virtmemory_block_t * mapped_block )
{
   int err ;

   if (     mapped_block->size_in_bytes
         && munmap( mapped_block->start, mapped_block->size_in_bytes )) {
      err = errno ;
      LOG_SYSERRNO("munmap") ;
      LOG_PTR(mapped_block->start) ;
      LOG_SIZE(mapped_block->size_in_bytes) ;
      goto ABBRUCH ;
   }

   mapped_block->start         = (uint8_t*)0 ;
   mapped_block->size_in_bytes = 0 ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}



#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_virtualmemory,ABBRUCH)

/* Returns:
 * 0 - All size_in_pages memory pages starting from address start are unmapped
 * 1 - All size_in_pages memory pages starting from address start are mapped
 * 2 - Wrong page permission (not read/writeable)
 * 3 - A part is mapped another is not
 * 4 - mapping query error
 * */
static int iscontained_in_mapping(const virtmemory_block_t * mapped_block)
{
   int result = 0 ;
   virtmemory_mappedregions_t mappedregions = virtmemory_mappedregions_INIT_FREEABLE ;
   void   * mapped_start = mapped_block->start ;
   void   * mapped_end   = (void*) (mapped_block->size_in_bytes + mapped_block->start) ;

   if (init_virtmemory_mappedregions(&mappedregions)) {
      result = 4 ;
   }

   for(const virtmemory_region_t * next; (next = next_virtmemory_mappedregions(&mappedregions)); ) {
      if (  mapped_start < next->end
            && mapped_end > next->start ) {
         // overlapping
         if (  next->isExecutable
               || next->isShared
               || !next->isReadable
               || !next->isWriteable
               ) {
            result = 2 ;
            break ;
         }
         if ( mapped_start < next->start ) {
            result = 3 ;
            break ;
         } else if ( mapped_end <= next->end ) {
            result = 1 ;
            break ;
         } else {
            result = 3 ;
            mapped_start = next->end ;
         }
      }
   }

   free_virtmemory_mappedregions(&mappedregions) ;
   return result ;
}

#if 0
// Reads all virtmemory_mapping_t and prints it to standard output
static void print_mapping(void)
{
   size_t result_size = 0 ;
   size_t skip_offset = 0 ;
   do {
      virtmemory_mapping_t mapping[10] ;
      if (mapping_virtmemory(10, mapping, &result_size, skip_offset)) break ;
      for(size_t i = 0 ; i < result_size; ++i) {
         printf("%08x-%08x %c%c%c%c\n",   (intptr_t)mapping[i].start, (intptr_t)mapping[i].end,
                                          mapping[i].isReadable?'r':'-',
                                          mapping[i].isWriteable?'w':'-',
                                          mapping[i].isExecutable?'x':'-',
                                          mapping[i].isShared?'s':'p') ;
      }
      skip_offset += result_size ;
   } while( result_size ) ;
}
#endif


static int test_mappedregions(void)
{
   int err = 1 ;
   virtmemory_mappedregions_t mappedregions  = virtmemory_mappedregions_INIT_FREEABLE ;

   // TEST query empty buffer
   TEST(0 == next_virtmemory_mappedregions(&mappedregions)) ;
   TEST(0 == size_virtmemory_mappedregions(&mappedregions)) ;
   gofirst_virtmemory_mappedregions(&mappedregions) ;

   // TEST init, double free
   TEST(0 == init_virtmemory_mappedregions(&mappedregions)) ;
   TEST(16 == mappedregions.element_count) ;
   TEST(mappedregions.total_count >= mappedregions.element_count) ;
   TEST(mappedregions.first_array) ;
   TEST(mappedregions.first_array->next == mappedregions.array_iterator) ;
   TEST(mappedregions.element_iterator == &mappedregions.first_array->elements[0]) ;
   TEST(0 == free_virtmemory_mappedregions(&mappedregions)) ;
   TEST(!mappedregions.first_array && !mappedregions.element_iterator && !mappedregions.element_count) ;
   TEST(!mappedregions.array_iterator && !mappedregions.total_count) ;
   TEST(0 == free_virtmemory_mappedregions(&mappedregions)) ;
   TEST(!mappedregions.first_array && !mappedregions.element_iterator && !mappedregions.element_count) ;
   TEST(!mappedregions.array_iterator && !mappedregions.total_count) ;

   // TEST iterator
   virtmemory_regionsarray_t array[3] = {
      { .size = 1, .next = &array[1] },
      { .size = 2, .next = &array[2] },
      { .size = 3, .next = 0 },
   } ;
   virtmemory_mappedregions_t mappedregions2 = {
      .total_count = 6,
      .first_array = &array[0]
   } ;
   for(int do_twice = 0; do_twice < 2; ++do_twice) {
      gofirst_virtmemory_mappedregions(&mappedregions2) ;
      for(int ai = 0; ai < 3; ++ai) {
         TEST( 6         == mappedregions2.total_count ) ;
         TEST( 6         == size_virtmemory_mappedregions(&mappedregions2)) ;
         TEST( &array[0] == mappedregions2.first_array ) ;
         TEST( array[ai].elements == next_virtmemory_mappedregions(&mappedregions2)) ;
         TEST( (ai < 2 ? &array[ai+1] : 0) == mappedregions2.array_iterator ) ;
         TEST( array[ai].size-1     == mappedregions2.element_count  ) ;
         TEST( array[ai].elements+1 == mappedregions2.element_iterator ) ;
         for(unsigned i = 1; i < array[ai].size; ++i) {
            TEST( &array[ai].elements[i] == next_virtmemory_mappedregions(&mappedregions2)) ;
         }
         TEST( 0 == mappedregions2.element_count  ) ;
         TEST( array[ai].elements+array[ai].size == mappedregions2.element_iterator ) ;
      }
      TEST( 0 == next_virtmemory_mappedregions(&mappedregions2)) ;
   }

   err = 0 ;
ABBRUCH:
   free_virtmemory_mappedregions(&mappedregions) ;
   return err ;
}

int unittest_os_virtualmemory()
{
   int err = 1 ;
   virtmemory_block_t   mapped_block ;
   virtmemory_block_t   unmapped_block ;
   size_t               size_in_pages ;
   virtmemory_mappedregions_t mappedregions  = virtmemory_mappedregions_INIT_FREEABLE ;
   virtmemory_mappedregions_t mappedregions2 = virtmemory_mappedregions_INIT_FREEABLE ;

   TEST(0 == test_mappedregions()) ;

   // TEST query
   TEST(pagesize_virtmemory() >= 1) ;
   TEST(makepowerof2(pagesize_virtmemory()) == pagesize_virtmemory()) ;
   TEST(ispowerof2(pagesize_virtmemory())) ;

   // store current mapping
   TEST(0 == init_virtmemory_mappedregions(&mappedregions)) ;

   // TEST init, double free
   TEST(0 == init_virtmemory_mappedregions(&mappedregions2)) ;
   TEST(16 == mappedregions2.element_count) ;
   TEST(mappedregions2.total_count >= mappedregions2.element_count) ;
   TEST(mappedregions2.first_array) ;
   TEST(mappedregions2.first_array->next == mappedregions2.array_iterator) ;
   TEST(mappedregions2.element_iterator == &mappedregions2.first_array->elements[0]) ;
   TEST(0 == free_virtmemory_mappedregions(&mappedregions2)) ;
   TEST(!mappedregions2.first_array && !mappedregions2.element_iterator && !mappedregions2.element_count) ;
   TEST(!mappedregions2.array_iterator && !mappedregions2.total_count) ;
   TEST(0 == free_virtmemory_mappedregions(&mappedregions2)) ;
   TEST(!mappedregions2.first_array && !mappedregions2.element_iterator && !mappedregions2.element_count) ;
   TEST(!mappedregions2.array_iterator && !mappedregions2.total_count) ;

   // TEST map, unmap
   size_in_pages = 1 ;
   TEST(0 == map_virtmemory(&mapped_block, size_in_pages)) ;
   TEST(mapped_block.start && mapped_block.size_in_bytes == size_in_pages * pagesize_virtmemory()) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == unmap_virtmemory(&unmapped_block)) ;
   TEST(unmapped_block.start == 0 && unmapped_block.size_in_bytes == 0) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map, extend, unmap
   size_in_pages = 10 ;
   TEST(0 == map_virtmemory(&mapped_block, size_in_pages)) ;
   TEST(mapped_block.start && mapped_block.size_in_bytes == size_in_pages * pagesize_virtmemory()) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   {  virtmemory_block_t upperhalf = { mapped_block.start + mapped_block.size_in_bytes/2, mapped_block.size_in_bytes/2 } ;
      virtmemory_block_t lowerhalf = { mapped_block.start, mapped_block.size_in_bytes/2 } ;
      unmapped_block = upperhalf ;
      TEST(0 == unmap_virtmemory(&unmapped_block)) ;
      TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      TEST(0 == extend_virtmemory( &lowerhalf, size_in_pages/2)) ;
      TEST(lowerhalf.start         == mapped_block.start) ;
      TEST(lowerhalf.size_in_bytes == mapped_block.size_in_bytes) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == unmap_virtmemory(&unmapped_block)) ;
   TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block.start = mapped_block.start ;
   TEST(0 == extend_virtmemory( &unmapped_block, size_in_pages)) ;
   TEST(unmapped_block.start         == mapped_block.start) ;
   TEST(unmapped_block.size_in_bytes == mapped_block.size_in_bytes) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   TEST(0 == unmap_virtmemory(&unmapped_block)) ;
   TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map, extend, unmap in (50 pages) loop
   size_in_pages = 50 ;
   TEST(0 == map_virtmemory(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for(size_t i = 0; i < size_in_pages; ++i) {
      size_t           unmapoffset = i * pagesize_virtmemory() ;
      virtmemory_block_t upperhalf = { mapped_block.start + unmapoffset, mapped_block.size_in_bytes - unmapoffset } ;
      virtmemory_block_t lowerhalf = { mapped_block.start, unmapoffset } ;
      unmapped_block = upperhalf ;
      TEST(0 == unmap_virtmemory(&unmapped_block)) ;
      TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      if (unmapoffset) {
         TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      }
      TEST(0 == extend_virtmemory(&lowerhalf, size_in_pages-i)) ;
      TEST(mapped_block.start         == lowerhalf.start) ;
      TEST(mapped_block.size_in_bytes == lowerhalf.size_in_bytes) ;
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for(size_t i = size_in_pages-1; i < size_in_pages; --i) {
      const size_t           unmapoffset = i * pagesize_virtmemory() ;
      const virtmemory_block_t upperhalf = { mapped_block.start + unmapoffset, pagesize_virtmemory() } ;
      const virtmemory_block_t lowerhalf = { mapped_block.start, unmapoffset } ;
      unmapped_block = upperhalf ;
      TEST(0 == unmap_virtmemory(&unmapped_block)) ;
      TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      if (i) {
         TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      }
   }
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map, tryextend, unmap
   size_in_pages = 10 ;
   TEST(0 == map_virtmemory(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   {
      size_t           unmapoffset = 7 * pagesize_virtmemory() ;
      virtmemory_block_t upperhalf = { mapped_block.start + unmapoffset,  mapped_block.size_in_bytes - unmapoffset } ;
      virtmemory_block_t lowerhalf = { mapped_block.start, unmapoffset } ;
      unmapped_block = upperhalf ;
      TEST(0 == unmap_virtmemory(&unmapped_block)) ;
      TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
      TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
      TEST(0 == iscontained_in_mapping(&upperhalf)) ;
      for(size_t i = 0; i <= 7; ++i) {
         virtmemory_block_t ext_block = { mapped_block.start, i * pagesize_virtmemory() } ;
         upperhalf = lowerhalf ;
         TEST(0 == maporextend_virtmemory(&upperhalf, 3, &ext_block)) ;
         TEST(ext_block.start == mapped_block.start) ;
         if (i < 7) {
            TEST(ext_block.size_in_bytes == i * pagesize_virtmemory() ) ;
            TEST(upperhalf.start < lowerhalf.start || upperhalf.start >= lowerhalf.start + lowerhalf.size_in_bytes) ;
            TEST(1 == iscontained_in_mapping(&upperhalf)) ;
            unmapped_block = upperhalf ;
            TEST(0 == unmap_virtmemory(&unmapped_block)) ;
            TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
            TEST(1 == iscontained_in_mapping(&lowerhalf)) ;
            TEST(0 == iscontained_in_mapping(&upperhalf)) ;
         } else {
            TEST(ext_block.size_in_bytes == mapped_block.size_in_bytes) ;
            TEST(0 == upperhalf.start ) ;
            TEST(0 == upperhalf.size_in_bytes ) ;
         }
      }
   }
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == unmap_virtmemory(&unmapped_block)) ;
   TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST map of already mapped
   size_in_pages = 3 ;
   TEST(0 == map_virtmemory(&mapped_block, size_in_pages)) ;
   TEST(1 == iscontained_in_mapping(&mapped_block)) ;
   for(size_t i = 0; i < size_in_pages; ++i) {
      size_t             lowersize = i * pagesize_virtmemory() ;
      virtmemory_block_t lowerhalf = { mapped_block.start, lowersize } ;
      TEST(ENOMEM == extend_virtmemory(&lowerhalf, 1)) ;
   }

   // TEST unmap of already unmapped (no error)
   unmapped_block = mapped_block ;
   TEST(0 == unmap_virtmemory(&unmapped_block)) ;
   TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;
   unmapped_block = mapped_block ;
   TEST(0 == unmap_virtmemory(&unmapped_block)) ;
   TEST(0 == iscontained_in_mapping(&mapped_block)) ;

   // TEST unmap empty block
   TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;
   TEST(0 == unmap_virtmemory(&unmapped_block)) ;
   TEST(0 == unmapped_block.start && 0 == unmapped_block.size_in_bytes) ;

   // TEST mapping has not changed
   TEST(0 == init_virtmemory_mappedregions(&mappedregions2)) ;
   TEST(size_virtmemory_mappedregions(&mappedregions2) == size_virtmemory_mappedregions(&mappedregions)) ;
   for(size_t i = 0; i < size_virtmemory_mappedregions(&mappedregions2); ++i) {
      const virtmemory_region_t  * next = next_virtmemory_mappedregions(&mappedregions) ;
      const virtmemory_region_t * next2 = next_virtmemory_mappedregions(&mappedregions2) ;
      TEST(next && next2) ;
      TEST(0 == compare_virtmemory_region( next, next2 )) ;
   }
   TEST(0 == next_virtmemory_mappedregions(&mappedregions)) ;
   TEST(0 == next_virtmemory_mappedregions(&mappedregions2)) ;
   TEST(0 == compare_virtmemory_mappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_virtmemory_mappedregions(&mappedregions)) ;
   TEST(0 == free_virtmemory_mappedregions(&mappedregions2)) ;
   TEST(0 == compare_virtmemory_mappedregions(&mappedregions, &mappedregions2)) ;

   err = 0 ;
ABBRUCH:
   free_virtmemory_mappedregions(&mappedregions) ;
   free_virtmemory_mappedregions(&mappedregions2) ;
   return err ;
}

#endif

