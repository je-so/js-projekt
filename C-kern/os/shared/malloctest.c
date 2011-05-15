/* title: Malloc-Test impl
   Implements <prepare_malloctest> and <allocatedsize_malloctest>.

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

   file: C-kern/api/test/malloctest.h
    Header file of <Malloc-Test>.

   file: C-kern/os/shared/malloctest.c
    Implementation file of <Malloc-Test impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/malloctest.h"
#include "C-kern/api/errlog.h"
#include <malloc.h>

/* variable: s_isprepared_malloctest
 * Remembers if <prepare_malloctest> was called already. */
static bool s_isprepared_malloctest = false ;

/* function: prepare_malloctest impl
 * Calls functions to force allocating of system memory.
 * No own malloc version is initialized:
 * see <allocatedsize_malloctest> for a why. */
void prepare_malloctest()
{
   s_isprepared_malloctest = true ;

   // force load language module
   (void) strerror(ENOMEM) ;
   (void) strerror(EEXIST) ;
   // force some overhead
   void * dummy = malloc((size_t)-1) ;
   assert(! dummy) ;

   trimmemory_malloctest() ;
}

/* function: trimmemory_malloctest impl
 * Uses GNU malloc_trim extension.
 * This function may be missing on some platforms.
 * Currently it is only working on Linux platforms. */
void trimmemory_malloctest()
{
   malloc_trim(0) ;
}

/* function: allocatedsize_malloctest impl
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
size_t allocatedsize_malloctest()
{
   if (!s_isprepared_malloctest) {
      prepare_malloctest() ;
   }

   int err ;
   int fd     = -1 ;
   int pfd[2] = { -1, -1 } ;

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

   dup2(fd, STDERR_FILENO) ;
   close(fd) ;
   close(pfd[0]) ;
   close(pfd[1]) ;

   return used_bytes ;
ABBRUCH:
   if (pfd[0] != -1) {
      close(pfd[0]) ;
      close(pfd[1]) ;
   }
   if (fd != -1) {
      dup2(fd, STDERR_FILENO) ;
      close(fd) ;
   }
   LOG_ABORT(err) ;
   return 0 ;
}
