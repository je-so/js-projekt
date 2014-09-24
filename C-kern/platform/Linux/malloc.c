/* title: Malloc impl
   Implements <Malloc>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/malloc.h
    Header file of <Malloc>.

   file: C-kern/platform/Linux/malloc.c
    Implementation file of <Malloc impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/platform/malloc.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#endif


// section: malloc

// group: static variables

/* variable: s_isprepared_malloc
 * Remembers if <prepare_malloc> was called already. */
static bool s_isprepared_malloc = false ;

// group: init

/* function: prepare_malloc
 * Calls functions to force allocating of system memory.
 * No own malloc version is initialized:
 * See <allocatedsize_malloc> for why. */
int prepare_malloc()
{
   int err ;

   s_isprepared_malloc = true ;

   // force some overhead
   void * dummy = malloc(10*1024*1024) ;
   free(dummy) ;

   err = trimmemory_malloc() ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

// group: manage

/* function: trimmemory_malloc
 * Uses GNU malloc_trim extension.
 * This function may be missing on some platforms.
 * Currently it is only working on Linux platforms. */
int trimmemory_malloc()
{
   malloc_trim(0) ;
   return 0 ;
}

// group: query

/* function: allocatedsize_malloc impl
 * Uses GNU malloc_stats extension.
 * This functions returns internal collected statistics
 * about memory usage so implementing a thin wrapper
 * for malloc is not necessary.
 * This function may be missing on some platforms.
 * Currently it is only tested on Linux platforms.
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
 * How it is implemented:
 * This function redirects standard error file descriptor
 * to a pipe and reads the content of the pipe into a buffer.
 * It scans backwards until the third last line is
 * reached ("in use bytes") and then returns the converted
 * the number at the end of the line as result. */
int allocatedsize_malloc(/*out*/size_t * number_of_allocated_bytes)
{
   int err ;
   int fd     = -1 ;
   int pfd[2] = { -1, -1 } ;

   if (!s_isprepared_malloc) {
      err = prepare_malloc() ;
      if (err) goto ONERR;
   }

   if (pipe2(pfd, O_CLOEXEC|O_NONBLOCK)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("pipe2", err) ;
      goto ONERR;
   }

   fd = dup(STDERR_FILENO) ;
   if (fd == -1) {
      err = errno ;
      TRACESYSCALL_ERRLOG("dup", err) ;
      goto ONERR;
   }

   if (-1 == dup2(pfd[1], STDERR_FILENO)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("dup2", err) ;
      goto ONERR;
   }

   malloc_stats() ;

   uint8_t  buffer[256/*must be even*/] ;

   ssize_t slen = read(pfd[0], buffer, sizeof(buffer)) ;
   if (slen < 0) {
      err = errno ;
      TRACESYSCALL_ERRLOG("read", err) ;
      goto ONERR;
   }

   size_t len = (size_t)slen ;

   while (sizeof(buffer) == len) {
      len = sizeof(buffer)/2 ;
      memcpy(buffer, &buffer[sizeof(buffer)/2], sizeof(buffer)/2) ;
      slen = read(pfd[0], &buffer[sizeof(buffer)/2], sizeof(buffer)/2) ;
      if (slen < 0) {
         err = errno ;
         if (err == EWOULDBLOCK || err == EAGAIN) {
            break ;
         }
         TRACESYSCALL_ERRLOG("read", err) ;
         goto ONERR;
      }
      len += (size_t) slen ;
   }

   // remove last two lines
   for (unsigned i = 3; len > 0; --len) {
      i -= (buffer[len] == '\n') ;
      if (!i) break ;
   }

   while (len > 0
          && buffer[len-1] >= '0'
          && buffer[len-1] <= '9') {
      -- len ;
   }

   size_t used_bytes = 0 ;
   if (  len > 0
         && buffer[len] >= '0'
         && buffer[len] <= '9'  ) {
      sscanf((char*)&buffer[len], "%" SCNuSIZE, &used_bytes) ;
   }

   if (-1 == dup2(fd, STDERR_FILENO)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("dup2",err) ;
      goto ONERR;
   }

   close(fd) ;
   close(pfd[0]) ;
   close(pfd[1]) ;

   *number_of_allocated_bytes = used_bytes ;

   return 0;
ONERR:
   if (pfd[0] != -1) close(pfd[0]) ;
   if (pfd[1] != -1) close(pfd[1]) ;
   if (fd != -1) {
      dup2(fd, STDERR_FILENO) ;
      close(fd) ;
   }
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_allocatedsize(void)
{
   size_t   allocated ;
   void *   memblocks[256] = { 0 } ;
   int      fd[4096] ;
   unsigned fdcount = 0 ;

   // TEST allocatedsize_malloc: allocated > 0
   allocated = 0 ;
   TEST(0 == allocatedsize_malloc(&allocated)) ;
   TEST(1 <= allocated) ;

   // TEST allocatedsize_malloc: increment
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      memblocks[i] = malloc(16) ;
      TEST(0 != memblocks[i]) ;
      size_t   allocated2 ;
      TEST(0 == allocatedsize_malloc(&allocated2)) ;
      TEST(allocated + 16 <= allocated2) ;
      TEST(allocated + 32 >= allocated2) ;
      allocated = allocated2 ;
   }

   // TEST allocatedsize_malloc: decrement
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      free(memblocks[i]) ;
      memblocks[i] = 0 ;
      size_t   allocated2 ;
      TEST(0 == allocatedsize_malloc(&allocated2)) ;
      TEST(allocated2 + 16 <= allocated) ;
      TEST(allocated2 + 32 >= allocated) ;
      allocated = allocated2 ;
   }

   // TEST allocatedsize_malloc: EMFILE
   for (; fdcount < lengthof(fd); ++fdcount) {
      fd[fdcount] = dup(STDOUT_FILENO) ;
      if (fd[fdcount] == -1) break ;
   }
   allocated = 1 ;
   TEST(EMFILE == allocatedsize_malloc(&allocated)) ;
   TEST(1 == allocated) ;
   while (fdcount > 0) {
      -- fdcount ;
      TEST(0 == close(fd[fdcount])) ;
   }

   return 0 ;
ONERR:
   while (fdcount > 0) {
      -- fdcount ;
      close(fd[fdcount]) ;
   }
   for (unsigned i = 0; i < lengthof(memblocks); ++i) {
      if (memblocks[i]) {
         free(memblocks[i]) ;
      }
   }
   return EINVAL ;
}

static int test_usablesize(void)
{
   void *   addr[1024] = { 0 } ;

   // TEST sizeusable_malloc: return 0 in case addr == 0
   TEST(0 == sizeusable_malloc(0)) ;

   // TEST sizeusable_malloc: small blocks ; return >= size
   for (unsigned i = 0; i < lengthof(addr); ++i) {
      addr[i] = malloc(1+i) ;
      TEST(0 != addr[i]) ;
   }
   for (unsigned i = 0; i < lengthof(addr); ++i) {
      TEST(1+i <= sizeusable_malloc(addr[i])) ;
   }
   for (unsigned i = 0; i < lengthof(addr); ++i) {
      free(addr[i]) ;
      addr[i] = 0 ;
   }

   // TEST sizeusable_malloc: big blocks ; return >= size
   for (unsigned i = 0; i < lengthof(addr); ++i) {
      addr[0] = malloc(65536*(1+i)) ;
      TEST(0 != addr[0]) ;
      TEST(16384*(1+i) <= sizeusable_malloc(addr[0])) ;
      free(addr[0]) ;
      addr[0] = 0 ;
   }

   return 0 ;
ONERR:
   for (unsigned i = 0; i < lengthof(addr); ++i) {
      if (addr[i]) free(addr[i]) ;
      addr[i] = 0 ;
   }
   return EINVAL ;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_FREE ;

   for (int i = 0; i < 3; ++i) {
      if (test_allocatedsize())  goto ONERR;
      if (test_usablesize())     goto ONERR;
   }
   CLEARBUFFER_ERRLOG() ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_allocatedsize())  goto ONERR;
   if (test_usablesize())     goto ONERR;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONERR:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_platform_malloc()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
