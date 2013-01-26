/* title: File-Util

   Defines additional functions like <load_file> and <save_file>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/filesystem/fileutil.h
    Header file <File-Util>.

   file: C-kern/io/filesystem/fileutil.c
    Implementation file <File-Util impl>.
*/
#ifndef CKERN_IO_FILESYSTEM_FILEUTIL_HEADER
#define CKERN_IO_FILESYSTEM_FILEUTIL_HEADER

// forward
struct directory_t ;
struct wbuffer_t ;


// section: Functions

/* function: load_file
 * Loads content of file and appends it to result.
 *
 * Returns:
 * 0      - File content loaded and appended to result.
 * ENOENT - File does not exist. Nothing was done.
 * EIO    - Loading error occurred - result was cleared. */
int load_file(const char * filepath, /*ret*/struct wbuffer_t * result, struct directory_t * relative_to) ;

/* function: save_file
 * Creates file and writes file_content into it.
 *
 * Returns:
 * 0      - File created and file_size bytes of file_content written into it.
 * EEXIST - File already exists. Nothing was done.
 * EIO    - File created and deleted after a write error occurred. */
int save_file(const char * filepath, size_t file_size, const void * file_content/*[filer_size]*/, struct directory_t * relative_to) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_fileutil
 * Test additional functionality implemented for <file_t>. */
int unittest_io_fileutil(void) ;
#endif


#endif
