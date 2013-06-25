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
#include "C-kern/api/io/iochannel.h"


void logfailed_test(const char * filename, unsigned line_number)
{
   char           number[10] ;
   struct iovec   iov[4] = {
      { (void*) (uintptr_t) filename, strlen(filename) },
      { (void*) (uintptr_t) ":", 1 },
      { number, (unsigned) snprintf(number, sizeof(number), "%u", line_number) },
      { (void*) (uintptr_t) ": FAILED TEST\n", 14 },
   } ;

   ssize_t written = writev(iochannel_STDOUT, iov, lengthof(iov)) ;
   (void) written ;
}

void logworking_test()
{
   (void) write_iochannel(iochannel_STDOUT, 3, "OK\n", 0) ;
}

void logrun_test(const char * testname)
{
   struct iovec   iov[3] = {
       { (void*) (uintptr_t) "RUN ", 4 }
      ,{ (void*) (uintptr_t) testname, strlen(testname) }
      ,{ (void*) (uintptr_t) ": ", 2 }
   } ;

   ssize_t written = writev(iochannel_STDOUT, iov, lengthof(iov)) ;
   (void) written ;
}

void logformat_test(const char * format, ...)
{
   va_list args ;
   va_start(args, format) ;
   int bytes = vdprintf(iochannel_STDOUT, format, args) ;
   (void) bytes ;
   va_end(args) ;
}



#ifdef KONFIG_UNITTEST

static int test_helper(void)
{
   int      fd[2]     = { iochannel_INIT_FREEABLE, iochannel_INIT_FREEABLE } ;
   int      oldstdout = iochannel_INIT_FREEABLE;
   uint8_t  buffer[100] ;
   size_t   bytes_read ;

   // prepare
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK)) ;
   oldstdout = dup(iochannel_STDOUT) ;
   TEST(0 < oldstdout) ;
   TEST(iochannel_STDOUT == dup2(fd[1], iochannel_STDOUT)) ;

   // TEST logfailed_test
   logfailed_test("123", 45) ;
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read)) ;
   TEST(20 == bytes_read) ;
   TEST(0 == strncmp((const char*)buffer, "123:45: FAILED TEST\n", bytes_read)) ;

   // TEST logworking_test
   logworking_test() ;
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read)) ;
   TEST(3 == bytes_read) ;
   TEST(0 == strncmp((const char*)buffer, "OK\n", bytes_read)) ;

   // TEST logrun_test
   logrun_test("test-name") ;
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read)) ;
   TEST(15 == bytes_read) ;
   TEST(0 == strncmp((const char*)buffer, "RUN test-name: ", bytes_read)) ;

   // TEST logformat_test
   logformat_test("Hello %d,%d\n", 1, 2) ;
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read)) ;
   TEST(10 == bytes_read) ;
   TEST(0 == strncmp((const char*)buffer, "Hello 1,2\n", bytes_read)) ;

   // unprepare
   TEST(iochannel_STDOUT == dup2(oldstdout, iochannel_STDOUT)) ;
   TEST(0 == free_iochannel(&oldstdout)) ;
   TEST(0 == free_iochannel(&fd[0])) ;
   TEST(0 == free_iochannel(&fd[1])) ;

   return 0 ;
ONABORT:
   if (!isfree_iochannel(oldstdout)) {
      dup2(oldstdout, iochannel_STDOUT) ;
   }
   memset(buffer, 0, sizeof(buffer)) ;
   read_iochannel(fd[0], sizeof(buffer)-1, buffer, 0) ;
   write_iochannel(iochannel_STDOUT, strlen((char*)buffer), buffer, 0) ;
   free_iochannel(&oldstdout) ;
   free_iochannel(&fd[0]) ;
   free_iochannel(&fd[1]) ;
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
