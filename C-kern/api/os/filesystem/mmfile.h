/* title: Memory-Mapped-File
   Maps a file into virtual memory. This allows accessing the content of
   a file as simple as an array.

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

   file: C-kern/api/os/filesystem/mmfile.h
    Header file of <Memory-Mapped-File>.

   file: C-kern/os/Linux/mmfile.c
    Linux specific implementation <Memory-Mapped-File Linux>.
*/
#ifndef CKERN_OS_FILESYSTEM_MEMORYMAPPEDFILE_HEADER
#define CKERN_OS_FILESYSTEM_MEMORYMAPPEDFILE_HEADER

#include "C-kern/api/aspect/constant/access_mode.h"

// forward declaration / to use it you need to include "C-kern/api/os/filessystem/directory.h"
struct directory_stream_t ;

/* enums: mmfile_openmode_e
 * Open mode like *readonly* or *read/write* (private + shared).
 *
 * mmopen_RDONLY          - Open file read only and trying to write to mapped memory generates an exception.
 * mmopen_RDWR_SHARED     - Open file read/write. Writing to memory does change the underlying file.
 *                          Every other process in the system will see the changes.
 * mmopen_RDWR_PRIVATE    - Open file read/write. However writing to memory does not change the underlying file.
 *                          Changes in memory are only visible to the calling process.
 *                          It is *unspecified* whether changes made to the underlying file by other processes are visible
 *                          after the file is mapped into memory. */
typedef access_mode_aspect_e     mmfile_openmode_e ;
#define mmfile_openmode_RDONLY         (access_mode_READ)
#define mmfile_openmode_RDWR_SHARED    (access_mode_READ|access_mode_WRITE|access_mode_SHARED)
#define mmfile_openmode_RDWR_PRIVATE   (access_mode_READ|access_mode_WRITE|access_mode_PRIVATE)
// used internally in <initcreate_mmfile>
#define mmfile_openmode_CREATE_FLAG    (access_mode_NEXTFREE_BITPOS)

/* typedef: mmfile_t typedef
 * Shortcut for <mmfile_t>. */
typedef struct mmfile_t          mmfile_t ;


// section: Functions

// group: query

extern size_t pagesize_mmfile(void) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_memorymappedfile */
extern int unittest_os_memorymappedfile(void) ;
#endif

/* struct: mmfile_t
 * Describes a memory mapped file.
 * Memory mapped files must always be readable cause the memory must be initialized before it is accessed
 * even if it only written to. */
struct mmfile_t {
   int      sys_file ;
   uint8_t  * addr ;             // aligned in pagesize
   size_t   size_pagealigned ;   // aligned in pagesize
   off_t    file_offset ;        // page aligned (parameter set by user)
   size_t   size ;               // may be not aligned if fileoffset is not multiple of pagesize or length of file is not multiple of pagesize
} ;

// group: lifetime

/* define: mmfile_INIT_FREEABLE
Static initializer for <mmfile_t>: makes calling of <free_mmfile> safe. */
#define mmfile_INIT_FREEABLE  { -1, 0, 0, 0, 0 }

/* function: init_mmfile
 * Opens or creates a new file and maps it to memory.
 * If path_relative_to is set to a value != 0 andif file_path is relative
 * file_path is considered relative to path_relative_to instead of the current working directory.
 *
 * Parameter:
 * mfile       - Memory mapped file object
 * file_path   - Path of file to be read or written. A relatice path is relative to path_relative_to or the current working directory.
 * file_offset - The offset of the first byte in the file whcih should be mapped. Must be a multiple of <pagesize_mmfile>.
 *               If file_offset >= filesize then ENODATA is returned => Files with length 0 always generate ENODATA error.
 * size        - The number of bytes which should be mapped.
 *               If file_offset + size > filesize then size is silently truncated (size = filesize - file_soffset)
 *               If size is set to 0 then it this means to map the whole file starting from file_offset.
 *               If size is set to 0 and the file is bigger then could be mapped in memory then ENOMEM is returned.
 * path_relative_to - If this paramter is != 0 and file_path is relative then file_path is relative to the path determined by this parameter.
 * mode        - Determines if the file is opened for reading or writing. See also <mmfile_openmode_e>.
 * */
extern int init_mmfile( /*out*/mmfile_t * mfile, const char * file_path, off_t file_offset, size_t size, const struct directory_stream_t * path_relative_to /*0 => current working directory*/, mmfile_openmode_e mode) ;

/* function: initcreate_mmfile
 * Creates a new file with the given size and opens it with <mmfile_openmode_RDWR_SHARED>.
 * If the file exists an error is returned. The file is always mapped from the beginning. */
extern int initcreate_mmfile( /*out*/mmfile_t * mfile, const char * file_path, size_t size, const struct directory_stream_t * path_relative_to /*0 => current working directory*/) ;

/* function: free_mmfile
 * Frees all mapped memory and closes the file. */
extern int free_mmfile(mmfile_t * mfile) ;

// query:

/* function: addr_mmfile
 * Returns the lowest address of the mapped memory.
 * The memory is always mapped in chunks of <pagesize_mmfile> (same as <pagesize_vm>).
 * */
extern uint8_t * addr_mmfile(const mmfile_t * mfile) ;

/* function: size_mmfile
 * */
extern size_t  size_mmfile(const mmfile_t * mfile) ;

/* function: fileoffset_mmfile
 * */
extern size_t  fileoffset_mmfile(const mmfile_t * mfile) ;


// section: inline implementation

/* define: addr_mmfile
 * Implements <mmfile_t.addr_mmfile>.
 *
 * Returns:
 * > ((mfile)->addr) */
#define initcreate_mmfile(mfile, file_path, size, path_relative_to) \
   init_mmfile( mfile, file_path, 0, size, path_relative_to, mmfile_openmode_CREATE_FLAG | mmfile_openmode_RDWR_SHARED )

/* define: addr_mmfile
 * Implements <mmfile_t.addr_mmfile>.
 *
 * Returns:
 * > ((mfile)->addr) */
#define /*void **/  addr_mmfile(/*mmfile_t * */mfile) \
   ((mfile)->addr)

/* define: size_mmfile
 * Implements <mmfile_t.size_mmfile>.
 *
 * Returns:
 * > ((mfile)->size) */
#define /*size_t*/ size_mmfile(/*mmfile_t * */mfile) \
   ((mfile)->size)

/* define: fileoffset_mmfile
 * Implements <mmfile_t.fileoffset_mmfile>.
 *
 * Returns:
 * > ((mfile)->file_offset) */
#define /*size_t*/ fileoffset_mmfile(/*mmfile_t * */mfile) \
   ((mfile)->file_offset)

#endif
