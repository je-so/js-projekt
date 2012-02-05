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

   file: C-kern/api/io/filesystem/mmfile.h
    Header file of <Memory-Mapped-File>.

   file: C-kern/platform/Linux/io/mmfile.c
    Linux specific implementation <Memory-Mapped-File Linux>.
*/
#ifndef CKERN_IO_FILESYSTEM_MEMORYMAPPEDFILE_HEADER
#define CKERN_IO_FILESYSTEM_MEMORYMAPPEDFILE_HEADER

#include "C-kern/api/io/accessmode.h"

// forward declaration
struct directory_t ;

/* enums: mmfile_openmode_e
 * Open mode like *readonly* or *read/write* (private + shared).
 *
 * mmfile_openmode_RDONLY          - Open file read only and trying to write to mapped memory generates an exception.
 * mmfile_openmode_RDWR_SHARED     - Open file read/write. Writing to memory does change the underlying file.
 *                                   Every other process in the system will see the changes.
 * mmfile_openmode_RDWR_PRIVATE    - Open file read/write. However writing to memory does not change the underlying file.
 *                                   Changes in memory are only visible to the calling process.
 *                                   It is *unspecified* whether changes made to the underlying file by other processes are visible
 *                                   after the file is mapped into memory. */
typedef accessmode_e                   mmfile_openmode_e ;
#define mmfile_openmode_RDONLY         (accessmode_READ)
#define mmfile_openmode_RDWR_SHARED    (accessmode_READ|accessmode_WRITE|accessmode_SHARED)
#define mmfile_openmode_RDWR_PRIVATE   (accessmode_READ|accessmode_WRITE|accessmode_PRIVATE)


/* typedef: struct mmfile_t
 * Exports <mmfile_t>. */
typedef struct mmfile_t                mmfile_t ;


// section: Functions

// group: query

/* function: pagesize_mmfile
 * Returns size of a memory page (block).
 * Files can be mapped into memory only page by page.
 * If the file size is not a multiple of <pagesize_mmfile> all memory
 * of the last mapped page which is beyond the size of the underlying file
 * is filled with 0, and writes to that region are not written to the file. */
extern size_t pagesize_mmfile(void) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_mmfile
 * Test mapping of file into memory. */
extern int unittest_io_mmfile(void) ;
#endif


/* struct: mmfile_t
 * Describes a memory mapped file.
 * Memory mapped files must always be readable cause the memory must be
 * initialized before it can be accessed. Even if you only want to write to it.
 *
 * TODO: Recovery:
 * In case a read error occurs a SIGBUS is thrown under Linux.
 * Register special recovery handler for mmfiles => abort + read error !
 *
 * TODO: memory mapping fails:
 * Add a check function to filedescriptor which checks if memory mapping is possible !
 * If not then use some »read into buffer fallback operation« in some higher component !!
 *
 *
 * */
struct mmfile_t {
   /* variable: addr
    * The start address of the mapped memory.
    * It is a multiple of <pagesize_mmfile>. */
   uint8_t  * addr ;
   /* variable: size
    * Size of the mapped memory.
    * *size* will be a multiple of <pagesize_mmfile> except
    * if filesize - file_offset would be < size. In this case size is
    * truncated to filesize - file_offset (see <init_mmfile>). */
   size_t   size ;
} ;

// group: lifetime

/* define: mmfile_INIT_FREEABLE
 * Static initializer for <mmfile_t>. Makes calling of <free_mmfile> a no op. */
#define mmfile_INIT_FREEABLE  { 0, 0 }

/* function: init_mmfile
 * Opens or creates a new file and maps it to memory.
 * If relative_to is set to a value != 0 and if file_path is relative
 * file_path is considered relative to relative_to instead of the current working directory.
 *
 * Parameter:
 * mfile       - Memory mapped file object
 * file_path   - Path of file to be read or written. A relatice path is relative to relative_to or the current working directory.
 * file_offset - The offset of the first byte in the file whcih should be mapped. Must be a multiple of <pagesize_mmfile>.
 *               If file_offset >= filesize then ENODATA is returned => Files with length 0 always generate ENODATA error.
 * size        - The number of bytes which should be mapped. The mapping is always done in chunks of <pagesize_mmfile>.
 *               size (in case it is != 0) is increased until it is a multiple of <pagesize_mmfile>.
 *               If file_offset + size > filesize then size is silently truncated (file_soffset + size == filesize).
 *               If size is set to 0 then this means to map the whole file starting from file_offset.
 *               If the file is bigger than could be mapped in memory ENOMEM is returned.
 * relative_to - If this paramter is != 0 and file_path is relative then file_path is relative to the path determined by this parameter.
 * mode        - Determines if the file is opened for reading or writing. See also <mmfile_openmode_e>.
 * */
extern int init_mmfile(/*out*/mmfile_t * mfile, const char * file_path, off_t file_offset, size_t size, mmfile_openmode_e mode, const struct directory_t * relative_to /*0=>current_working_directory*/) ;

/* function: initcreate_mmfile
 * Creates a new file with the given size and opens it with <mmfile_openmode_RDWR_SHARED>.
 * If the file exists EEXIST is returned. The file is always mapped from the beginning. */
extern int initcreate_mmfile(/*out*/mmfile_t * mfile, const char * file_path, size_t size, const struct directory_t * relative_to /*0=>current working directory*/) ;

/* function: free_mmfile
 * Frees all mapped memory and closes the file. */
extern int free_mmfile(mmfile_t * mfile) ;

// group: query

/* function: addr_mmfile
 * Returns the lowest address of the mapped memory.
 * The memory is always mapped in chunks of <pagesize_mmfile> (same as <pagesize_vm>).
 * The memory which can be accessed is at least [<addr_mmfile> .. <addr_mmfile>+<size_mmfile>]. */
extern uint8_t * addr_mmfile(const mmfile_t * mfile) ;

/* function: size_mmfile
 * Returns the size of the mapped memory.
 * The memory which corresponds to the underlying file is exactly
 * [<addr_mmfile> .. <addr_mmfile>+<size_mmfile>]. */
extern size_t  size_mmfile(const mmfile_t * mfile) ;

/* function: alignedsize_mmfile
 * Returns the size of the mapped memory.
 * This value is a multiple of <pagesize_mmfile> and it is >= <size_mmfile>.
 * The mapped memory region is [<addr_mmfile> .. <addr_mmfile>+<alignedsize_mmfile>]
 * but only [<addr_mmfile> .. <addr_mmfile>+<size_mmfile>] corresponds to the underlying file. */
extern size_t  alignedsize_mmfile(const mmfile_t * mfile) ;


// section: inline implementation

/* define: addr_mmfile
 * Implements <mmfile_t.addr_mmfile>. */
#define addr_mmfile(mfile)             ((mfile)->addr)

/* define: alignedsize_mmfile
 * Implements <mmfile_t.alignedsize_mmfile>. */
#define alignedsize_mmfile(mfile)      ((size_mmfile(mfile) + (pagesize_mmfile()-1)) & ~(pagesize_mmfile()-1))

/* define: size_mmfile
 * Implements <mmfile_t.size_mmfile>. */
#define size_mmfile(mfile)             ((mfile)->size)


#endif
