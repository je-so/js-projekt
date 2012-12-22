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
#include "C-kern/api/err.h"
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/file.h"


void logfailed_test(const char * filename, unsigned line_number)
{
   char           number[10] ;
   struct iovec   iov[4] = {
       { (void*) (uintptr_t) filename, strlen(filename) }
      ,{ (void*) (uintptr_t) ":", 1 }
      ,{ number, (unsigned) snprintf(number, sizeof(number), "%u", line_number) }
      ,{ (void*) (uintptr_t) ": FAILED TEST\n", 14 }
   } ;

   ssize_t written = writev(file_STDOUT, iov, nrelementsof(iov)) ;
   (void) written ;
}

void logworking_test()
{
   (void) write_file(file_STDOUT, 3, (const uint8_t*)"OK\n", 0) ;
}

void logrun_test(const char * testname)
{
   struct iovec   iov[3] = {
       { (void*) (uintptr_t) "RUN ", 4 }
      ,{ (void*) (uintptr_t) testname, strlen(testname) }
      ,{ (void*) (uintptr_t) ": ", 2 }
   } ;

   ssize_t written = writev(file_STDOUT, iov, nrelementsof(iov)) ;
   (void) written ;
}



#ifdef KONFIG_UNITTEST

static int test_helper(void)
{
   int      fd[2]     = { file_INIT_FREEABLE, file_INIT_FREEABLE } ;
   int      oldstdout = file_INIT_FREEABLE;
   uint8_t  buffer[100] ;
   size_t   bytes_read ;

   // prepare
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 < (oldstdout = dup(file_STDOUT))) ;
   TEST(file_STDOUT == dup2(fd[1], file_STDOUT)) ;

   // TEST logfailed_test
   logfailed_test("123", 45) ;
   TEST(0 == read_file(fd[0], sizeof(buffer), buffer, &bytes_read)) ;
   TEST(20 == bytes_read) ;
   TEST(0 == strncmp((const char*)buffer, "123:45: FAILED TEST\n", bytes_read)) ;

   // TEST logworking_test
   logworking_test() ;
   TEST(0 == read_file(fd[0], sizeof(buffer), buffer, &bytes_read)) ;
   TEST(3 == bytes_read) ;
   TEST(0 == strncmp((const char*)buffer, "OK\n", bytes_read)) ;

   // TEST logrun_test
   logrun_test("test-name") ;
   TEST(0 == read_file(fd[0], sizeof(buffer), buffer, &bytes_read)) ;
   TEST(15 == bytes_read) ;
   TEST(0 == strncmp((const char*)buffer, "RUN test-name: ", bytes_read)) ;

   // unprepare
   TEST(file_STDOUT == dup2(oldstdout, file_STDOUT)) ;
   TEST(0 == free_file(&oldstdout)) ;
   TEST(0 == free_file(&fd[0])) ;
   TEST(0 == free_file(&fd[1])) ;

   return 0 ;
ONABORT:
   if (isinit_file(oldstdout)) {
      dup2(oldstdout, file_STDOUT) ;
   }
   memset(buffer, 0, sizeof(buffer)) ;
   read_file(fd[0], sizeof(buffer)-1, buffer, 0) ;
   write_file(file_STDOUT, strlen((char*)buffer), buffer, 0) ;
   free_file(&oldstdout) ;
   free_file(&fd[0]) ;
   free_file(&fd[1]) ;
   return EINVAL ;
}

int unittest_test_test()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_helper())   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
