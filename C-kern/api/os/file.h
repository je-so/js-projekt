/* title: File
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

   file: C-kern/api/os/file.h
    Header file of <File>.

   file: C-kern/os/Linux/file.c
    Implementation file <File impl>.
*/
#ifndef CKERN_OS_FILE_HEADER
#define CKERN_OS_FILE_HEADER

// section: Functions

// group: query

/* function: openfd_file
 * Returns number of opened file descriptors.
 * Use this function at the beginning and the end
 * of your test to check if a file or network socket
 * is not closed properly.
 * In case of error this functions returns 0. */
extern int openfd_file(/*out*/size_t * number_open_fd) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_file
 * Unittest for file interface. */
extern int unittest_os_file(void) ;
#endif

#endif
