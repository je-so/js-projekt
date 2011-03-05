/* title: Memory-Mapped File
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
   (C) 2010 Jörg Seebohn

   file: C-kern/api/os/filesystem/mmfile.h
    Header file of <Memory-Mapped File>.

   file: C-kern/os/Linux/mmfile.c
    Linux specific implementation of <Memory-Mapped File>.
*/
#ifndef CKERN_OS_FILESYSTEM_MEMORYMAPPEDFILE_HEADER
#define CKERN_OS_FILESYSTEM_MEMORYMAPPEDFILE_HEADER

// forward declaration / to use it you need to include "C-kern/api/os/filessystem/directory.h"
struct directory_stream_t ;

/* typedef: typedef mmfile_t
 * Shortcut for <mmfile_t>. */
typedef struct mmfile_t mmfile_t ;

/* enums: mmfile_openmode_e
 * Open mode like *readonly* or *read/write* (private + shared).
 *
 * mmopen_RDONLY       - Open file read only and trying to write to mapped memory generates an exception.
 * mmopen_RDWR_SHARED  - Open file read/write. Writing to memory does change the underlying file.
 *                       Every other process in the system will see the changes.
 * mmopen_RDWR_PRIVATE - Open file read/write. However writing to memory does not change the underlying file.
 *                       Changes in memory are only visible to the calling process.
 *                       It is *unspecified* whether changes made to the underlying file by other processes are visible
 *                       after the file is mapped into memory.
 * */
enum mmfile_openmode_e {
   mmfile_openmode_RDONLY       = 0,
   mmfile_openmode_RDWR_SHARED  = 1,
   mmfile_openmode_RDWR_PRIVATE = 2
} ;
typedef enum mmfile_openmode_e mmfile_openmode_e ;

// section: functions

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
 * even if it only written to.
 * */
struct mmfile_t {
   int      sys_file ;
   uint8_t  * memory_start ;              // aligned in pagesize
   size_t   size_in_bytes_pagealigned ;   // aligned in pagesize
   off_t    file_offset ;                 // page aligned (parameter set by user)
   size_t   size_in_bytes ;               // may be not aligned if fileoffset is not multiple of pagesize or length of file is not multiple of pagesize
} ;

// group: lifetime

/* define: mmfile_INIT_FREEABLE
Static initializer for <mmfile_t>: makes calling of <free_mmfile> safe. */
#define mmfile_INIT_FREEABLE  { -1, 0, 0, 0, 0 }

/* function: init_mmfile
 * */
extern int init_mmfile( /*out*/mmfile_t * mfile,  const char * file_path,  off_t file_offset, size_t size_in_bytes, const struct directory_stream_t * path_relative_to /*0 => current working directory*/, mmfile_openmode_e mode) ;

/* function: initcreat_mmfile
 * Creates a new file, maps memory for writing. It returns an error if the file already exists.
 * The file always opened with mode <mmopen_RDWR_SHARED>. */
extern int initcreat_mmfile( /*out*/mmfile_t * mfile,  const char * file_path,  size_t file_size_in_bytes, const struct directory_stream_t * path_relative_to /*0 => current working directory*/) ;

/* function: free_mmfile
 * */
extern int free_mmfile(mmfile_t * mfile) ;

// query:

/* function: memstart_mmfile
 * Returns the lowest address of the mapped memory.
 * The memory is always mapped in chunks of <pagesize_mmfile> (same as <pagesize_virtmemory>).
 * */
extern uint8_t *  memstart_mmfile(const mmfile_t * mfile) ;

/* function: sizeinbytes_mmfile
 * */
extern size_t  sizeinbytes_mmfile(const mmfile_t * mfile) ;

/* function: fileoffset_mmfile
 * */
extern size_t  fileoffset_mmfile(const mmfile_t * mfile) ;


// section: inline implementation

/* define: memstart_mmfile
 * Implements <mmfile_t.memstart_mmfile>.
 *
 * Returns:
 * > ((mfile)->memory_start) */
#define /*void **/  memstart_mmfile(/*mmfile_t * */mfile) \
   ((mfile)->memory_start)

/* define: sizeinbytes_mmfile
 * Implements <mmfile_t.sizeinbytes_mmfile>.
 *
 * Returns:
 * > ((mfile)->size_in_bytes) */
#define /*size_t*/ sizeinbytes_mmfile(/*mmfile_t * */mfile) \
   ((mfile)->size_in_bytes)

/* define: fileoffset_mmfile
 * Implements <mmfile_t.fileoffset_mmfile>.
 *
 * Returns:
 * > ((mfile)->file_offset) */
#define /*size_t*/ fileoffset_mmfile(/*mmfile_t * */mfile) \
   ((mfile)->file_offset)

#endif
