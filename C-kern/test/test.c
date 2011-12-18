/* title: Test impl
   Implements helper functions exported in <Test>.

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

   file: C-kern/api/test.h
    Header file of <Test>.

   file: C-kern/test/test.c
    Implementation file <Test impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filedescr.h"


void logfailed_test(const char * filename, unsigned line_number)
{
   char           number[10] ;
   struct iovec   iov[4] = {
       { (void*) (intptr_t) filename, strlen(filename) }
      ,{ (void*) (intptr_t) ":", 1 }
      ,{ number, (unsigned) snprintf(number, sizeof(number), "%u", line_number) }
      ,{ (void*) (intptr_t) ": FAILED TEST\n", 14 }
   } ;

   ssize_t written = writev(filedescr_STDOUT, iov, nrelementsof(iov)) ;
   (void) written ;
}



#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_helper(void)
{
   int      fd[2]     = { sys_filedescr_INIT_FREEABLE, sys_filedescr_INIT_FREEABLE } ;
   int      oldstdout = sys_filedescr_INIT_FREEABLE;
   uint8_t  buffer[100] ;
   size_t   bytes_read ;

   // prepare
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 < (oldstdout = dup(filedescr_STDOUT))) ;
   TEST(filedescr_STDOUT == dup2(fd[1], filedescr_STDOUT)) ;

   // TEST logfailed_test
   logfailed_test("123", 45) ;
   TEST(0 == read_filedescr(fd[0], sizeof(buffer), buffer, &bytes_read)) ;
   TEST(20 == bytes_read) ;
   TEST(0 == strncmp((const char*)buffer, "123:45: FAILED TEST\n", bytes_read)) ;

   // unprepare
   TEST(filedescr_STDOUT == dup2(oldstdout, filedescr_STDOUT)) ;
   TEST(0 == free_filedescr(&oldstdout)) ;
   TEST(0 == free_filedescr(&fd[0])) ;
   TEST(0 == free_filedescr(&fd[1])) ;

   return 0 ;
ABBRUCH:
   if (isinit_filedescr(oldstdout)) {
      dup2(oldstdout, filedescr_STDOUT) ;
   }
   memset(buffer, 0, sizeof(buffer)) ;
   read_filedescr(fd[0], sizeof(buffer), buffer, 0) ;
   printf("%s", buffer) ;
   free_filedescr(&oldstdout) ;
   free_filedescr(&fd[0]) ;
   free_filedescr(&fd[1]) ;
   return EINVAL ;
}

int unittest_test_functions()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_helper())   goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
