/* title: Malloc impl
   Implements <Malloc>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/platform/malloc.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filedescr.h"
#include <malloc.h>
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* variable: s_isprepared_malloc
 * Remembers if <prepare_malloc> was called already. */
static bool s_isprepared_malloc = false ;

/* function: prepare_malloc
 * Calls functions to force allocating of system memory.
 * No own malloc version is initialized:
 * see <allocatedsize_malloc> for a why. */
int prepare_malloc()
{
   int err ;

   s_isprepared_malloc = true ;

   // force load language module
   (void) strerror(ENOMEM) ;
   (void) strerror(EEXIST) ;
   // force some overhead
   void * dummy = malloc((size_t)-1) ;
   assert(! dummy) ;

   err = trimmemory_malloc() ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: trimmemory_malloc
 * Uses GNU malloc_trim extension.
 * This function may be missing on some platforms.
 * Currently it is only working on Linux platforms. */
int trimmemory_malloc()
{
   malloc_trim(0) ;
   return 0 ;
}

/* function: allocatedsize_malloc impl
 * Uses GNU malloc_stats extension.
 * This functions returns internal collected statistics
 * about memory usage so implementing a thin wrapper
 * for malloc is not necessary.
 * This function may be missing on some platforms.
 * Currently it is only working on Linux platforms.
 *
 * What malloc_stats does:
 * The GNU C lib function malloc_stats writes textual information
 * to standard err in the following form:
 * > Arena 0:
 * > system bytes     =     135168
 * > in use bytes     =      15000
 * > Total (incl. mmap):
 * > system bytes     =     135168
 * > in use bytes     =      15000
 * > max mmap regions =          0
 * > max mmap bytes   =          0
 *
 * How is it implementated:
 * This function redirects standard error file descriptor
 * to an internal pipe and reads the content of the pipe
 * into a buffer.
 * The it scans backwards until the third last line is
 * reached ("in use bytes") and then returns the converted
 * the number at the end of the line as result.
 * */
int allocatedsize_malloc(size_t * number_of_allocated_bytes)
{
   int err ;
   int fd     = -1 ;
   int pfd[2] = { -1, -1 } ;

   if (!s_isprepared_malloc) {
      err = prepare_malloc() ;
      if (err) goto ABBRUCH ;
   }

   if (pipe2(pfd, O_CLOEXEC)) {
      err = errno ;
      LOG_SYSERR("pipe2", err) ;
      goto ABBRUCH ;
   }

   fd = dup(STDERR_FILENO) ;
   if (fd == -1) {
      err = errno ;
      LOG_SYSERR("dup", err) ;
      goto ABBRUCH ;
   }

   if (-1 == dup2(pfd[1], STDERR_FILENO)) {
      err = errno ;
      LOG_SYSERR("dup2", err) ;
      goto ABBRUCH ;
   }

   malloc_stats() ;

   uint8_t  buffer[256] ;
   ssize_t  len    = 0 ;
   if (128 == (len = read(pfd[0], buffer, 128))) {
      while( 128 == (len = read(pfd[0], &buffer[128], 128))) {
         memcpy( buffer, &buffer[128], 128) ;
      }
      if (len >= 0) {
         len += 128 ;
      }
   }

   if (len < 0) {
      err = errno ;
      LOG_SYSERR("read", err) ;
      goto ABBRUCH ;
   }

   for(int i = 3; i && len > 0; -- len) {
      if (buffer[len-1] == '\n') {
         --i ;
      }
   }

   while(   len > 0
         && buffer[len-1] >= '0'
         && buffer[len-1] <= '9'  ) {
      -- len ;
   }

   size_t used_bytes = 0 ;
   if (     len > 0
         && buffer[len] >= '0'
         && buffer[len] <= '9'  ) {
      sscanf( (char*)&buffer[len], "%" SCNuSIZE, &used_bytes ) ;
   }

   if (-1 == dup2(fd, STDERR_FILENO)) {
      err = errno ;
      LOG_SYSERR("dup2",err) ;
      goto ABBRUCH ;
   }

   err = free_filedescr(&fd) ;
   if (err) goto ABBRUCH ;
   err = free_filedescr(&pfd[0]) ;
   if (err) goto ABBRUCH ;
   err = free_filedescr(&pfd[1]) ;
   if (err) goto ABBRUCH ;

   *number_of_allocated_bytes = used_bytes ;

   return 0;
ABBRUCH:
   free_filedescr(&pfd[0]) ;
   free_filedescr(&pfd[1]) ;
   if (fd != -1) {
      dup2(fd, STDERR_FILENO) ;
      free_filedescr(&fd) ;
   }
   LOG_ABORT(err) ;
   return 0 ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_allocatedsize(void)
{
   size_t   allocated ;
   void   * memblocks[256] = { 0 } ;

   // TEST  allocated > 0
   allocated = 0 ;
   TEST(0 == allocatedsize_malloc(&allocated)) ;
   TEST(1 <= allocated) ;

   // TEST increment
   for(unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      memblocks[i] = malloc(16) ;
      TEST(0 != memblocks[i]) ;
      size_t   allocated2 ;
      TEST(0 == allocatedsize_malloc(&allocated2)) ;
      TEST(allocated + 16 <= allocated2) ;
      TEST(allocated + 32 >= allocated2) ;
      allocated = allocated2 ;
   }


   // TEST decrement
   for(unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      free(memblocks[i]) ;
      memblocks[i] = 0 ;
      size_t   allocated2 ;
      TEST(0 == allocatedsize_malloc(&allocated2)) ;
      TEST(allocated2 + 16 <= allocated) ;
      TEST(allocated2 + 32 >= allocated) ;
      allocated = allocated2 ;
   }

   return 0 ;
ABBRUCH:
   for(unsigned i = 0; i < nrelementsof(memblocks); ++i) {
      if (memblocks[i]) {
         free(memblocks[i]) ;
      }
   }
   return EINVAL ;
}

int unittest_platform_malloc()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_allocatedsize())  goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
