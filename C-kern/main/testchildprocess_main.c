/* title: Testchildprocess
   Test process which is executed as
   child process by some unittests.

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

   file: C-kern/main/testchildprocess_main.c
    Implementation file of <Testchildprocess>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/process.h"
#include "C-kern/api/io/filedescr.h"

enum testcase_e {
    testcase_RETURNEXITCODE    = 1
   ,testcase_OPENFILES         = 2
   ,testcase_WRITEPROCESSNAME  = 3
} ;

static void testcase_returnexitcode(int exitcode)
{
   exit(exitcode) ;
}

static void testcase_writeopenfd(void)
{
   int err ;
   size_t nrfiles ;
   err = nropen_filedescr(&nrfiles) ;
   if (!err) {
      dprintf(STDERR_FILENO, "%d", (int)nrfiles) ;
   }
   exit(err) ;
}

static void testcase_writename(void)
{
   int err ;
   char name[32] ;
   err = name_process(sizeof(name), name, 0) ;
   if (!err) {
      dprintf(STDERR_FILENO, "%s", name) ;
   }
   exit(err) ;
}

static void check_argc(int argc, int should_be)
{
   if (argc != should_be) {
      printf("argc(%d) != %d\n", argc, should_be) ;
      abort() ;
   }
}

int main(int argc, const char * argv[])
{
   if (argc < 2) {
      printf("argc < 2\n") ;
      abort() ;
   }

   int testcase = atoi(argv[1]) ;

   switch( testcase ) {
   case testcase_RETURNEXITCODE:
      check_argc(argc, 3) ;
      testcase_returnexitcode(atoi(argv[2])) ;
      break ;
   case testcase_OPENFILES:
      check_argc(argc, 2) ;
      testcase_writeopenfd() ;
      break ;
   case testcase_WRITEPROCESSNAME:
      check_argc(argc, 2) ;
      testcase_writename() ;
      break ;
   }

   abort() ;
   return 127 ;
}
