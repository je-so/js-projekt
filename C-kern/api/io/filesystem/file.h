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

   file: C-kern/api/io/filesystem/file.h
    Header file <File>.

   file: C-kern/os/Linux/io/file.c
    Linux specific implementation <File Linux>.
*/
#ifndef CKERN_IO_FILESYSTEM_FILE_HEADER
#define CKERN_IO_FILESYSTEM_FILE_HEADER

#include "C-kern/api/io/filedescr.h"

// forward
struct directory_t ;

/* typedef: file_t
 * Export <file_t>, alias for <sys_filedescr_t>.
 * Describes a persistent binary object with a name.
 * Describes an opened file for doing reading and/or writing.
 * The file is located in a system specific filesystem. */
typedef sys_filedescr_t                file_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_file
 * Unittest for file interface. */
extern int unittest_io_file(void) ;
#endif


// section: file_t

// group: lifetime

/* define: file_INIT_FREEABLE
 * Static initializer. */
#define file_INIT_FREEABLE             sys_filedescr_INIT_FREEABLE

/* function: init_file
 * Opens a file identified by its path and name.
 * The filepath can be either a relative or an absolute path.
 * If filepath is relative it is considered relative to the directory relative_to.
 * If relative_to is set to NULL then it is considered relative to the current working directory. */
extern int init_file(/*out*/file_t * fileobj, const char* filepath, accessmode_e iomode, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: initcreat_file
 * Creates a file identified by its path and name.
 * If the file exists already EEXIST is returned.
 * The filepath can be either a relative or an absolute path.
 * If filepath is relative it is considered relative to the directory relative_to.
 * If relative_to is set to NULL then it is considered relative to the current working directory. */
extern int initcreat_file(/*out*/file_t * fileobj, const char* filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: free_file
 * Closes an opened file and frees held resources. */
extern int free_file(file_t * fileobj) ;

// group: query

/* function: convertfd_file
 * Returns the filedescriptor of an open file.
 * Returns filedescr_INIT_FREEABLE in case file is closed. */
extern filedescr_t fd_file(const file_t * fileobj) ;

/* function: isinit_file
 * Returns true if the file was opened with <init_file>.
 * Returns false if file is in a freed (closed) state and after <free_file>
 * has been called. */
extern bool isinit_file(const file_t * fileobj) ;

// group: io

/* function: read_file
 * Reads binary data from a file. */
extern int read_file(file_t * fileobj, size_t buffer_size, /*out*/uint8_t buffer[buffer_size], size_t * bytes_read) ;

/* function: write_file
 * Writes binary data to a file. */
extern int write_file(file_t * fileobj, size_t buffer_size, const uint8_t buffer[buffer_size], size_t * bytes_written) ;


// section: inline implementation

/* define: fd_file
 * Implements <file_t.fd_file>.
 * This function assumes that <file_t> and <filedescr_t> are interchangeable. */
#define fd_file(fileobj)               (*(fileobj))

/* define: isinit_file
 * Implements <file_t.isinit_file>.
 * This function assumes that file is primitive type. */
#define isinit_file(fileobj)           (file_INIT_FREEABLE != *(fileobj))

/* define: read_file
 * Implements <file_t.read_file>. */
#define read_file(fileobj, buffer_size, buffer, bytes_read) \
   read_filedescr(fd_file(fileobj), buffer_size, buffer, bytes_read)

/* define: write_file
 * Implements <file_t.write_file>. */
#define write_file(fileobj, buffer_size, buffer, bytes_written) \
   write_filedescr(fd_file(fileobj), buffer_size, buffer, bytes_written)

#endif
