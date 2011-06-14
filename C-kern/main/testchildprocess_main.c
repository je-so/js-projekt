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

enum testcase_e {
    testcase_RETURNEXITCODE = 1
   ,testcase_NEXTFREE       = 2
} ;

static void testcase_returnexitcode(int exitcode)
{
   exit(exitcode) ;
}


int main(int argc, const char * argv[])
{
   if (argc != 3) {
      printf("argc != 3\n") ;
      abort() ;
   }

   int testcase = atoi(argv[1]) ;

   switch( testcase ) {
   case testcase_RETURNEXITCODE:
      testcase_returnexitcode(atoi(argv[2])) ;
      break ;
   default:
      printf("unknown testcase (%d)\n", testcase) ;
      abort() ;
   }

   return 0 ;
}
