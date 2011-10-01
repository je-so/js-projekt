/* title: File impl
   Implements <File>.

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

   file: C-kern/api/os/file.h
    Header file of <File>.

   file: C-kern/os/Linux/file.c
    Implementation file <File impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/file.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* function: openfd_file
 * Uses Linux specific "/proc/self/fd" interface. */
int openfd_file(/*out*/size_t * number_open_fd)
{
   int err ;
   directory_stream_t procself = directory_stream_INIT_FREEABLE ;

   err = init_directorystream(&procself, "/proc/self/fd", 0) ;
   if (err) goto ABBRUCH ;

   size_t    open_fds = (size_t)0 ;
   const char  * name ;
   do {
      ++ open_fds ;
      err = readnext_directorystream(&procself, &name, 0) ;
      if (err) goto ABBRUCH ;
   } while( name ) ;

   err = free_directorystream(&procself) ;
   if (err) goto ABBRUCH ;

   /* adapt open_fds for
      1. counts one too high
      2. counts "."
      3. counts ".."
      4. counts fd opened in init_directorystream
   */
   *number_open_fd = open_fds >= 4 ? open_fds-4 : 0 ;

   return 0 ;
ABBRUCH:
   free_directorystream(&procself) ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_file,ABBRUCH)

static int test_openfd(void)
{
   size_t  openfd ;
   int     fds[128] = { -1 } ;

   // TEST std file descriptors are open
   openfd = 0 ;
   TEST(0 == openfd_file(&openfd)) ;
   TEST(3 <= openfd) ;

   // TEST increment
   for(unsigned i = 0; i < nrelementsof(fds); ++i) {
      fds[i] = open("/dev/null", O_RDONLY|O_CLOEXEC) ;
      TEST(fds[i] > 0) ;
      size_t openfd2 = 0 ;
      TEST(0 == openfd_file(&openfd2)) ;
      ++ openfd ;
      TEST(openfd == openfd2) ;
   }

   // TEST decrement
   for(unsigned i = 0; i < nrelementsof(fds); ++i) {
      TEST(0 == close(fds[i])) ;
      fds[i] = -1 ;
      size_t openfd2 = 0 ;
      TEST(0 == openfd_file(&openfd2)) ;
      -- openfd ;
      TEST(openfd == openfd2) ;
   }

   return 0 ;
ABBRUCH:
   for(unsigned i = 0; i < nrelementsof(fds); ++i) {
      if (-1 != fds[i]) {
         close(fds[i]) ;
      }
   }
   return EINVAL ;
}

int unittest_os_file()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_openfd())   goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
