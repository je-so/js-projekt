/* title: File-Test
   Offers an interface to check for open file descriptors.

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

   file: C-kern/api/test/filetest.h
    Header file of <File-Test>.

   file: C-kern/os/Linux/filetest.c
    Implementation file of <File-Test impl>.
*/
#ifndef CKERN_TEST_FILETEST_HEADER
#define CKERN_TEST_FILETEST_HEADER

// section: Test

// group: filedescriptor

/* function: openfd_filetest
 * Returns number of opened file descriptors.
 * Use this function at the beginning and the end
 * of your test to check if a file or network socket
 * is not closed properly. */
extern size_t openfd_filetest(void) ;


#endif
