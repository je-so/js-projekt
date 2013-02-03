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

/* typedef: struct mmfile_t
 * Exports <mmfile_t>. */
typedef struct mmfile_t                mmfile_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_mmfile
 * Test mapping of file into memory. */
int unittest_io_mmfile(void) ;
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
 * Add a check function to mmfile_t which checks if memory mapping is possible !
 * If not then use some »read into buffer fallback operation« in some higher component !!
 * */
struct mmfile_t {
   /* variable: addr
    * The start address of the mapped memory.
    * It is a multiple of <pagesize_vm>. */
   uint8_t  * addr ;
   /* variable: size
    * Size of the mapped memory.
    * *size* will be a multiple of <pagesize_vm> except
    * if filesize - file_offset would be < size. In this case size is
    * truncated to filesize - file_offset (see <init_mmfile>). */
   size_t   size ;
} ;

// group: lifetime

/* define: mmfile_INIT_FREEABLE
 * Static initializer for <mmfile_t>. Makes calling of <free_mmfile> a no op. */
#define mmfile_INIT_FREEABLE           { 0, 0 }

/* function: init_mmfile
 * Opens a new file and maps it to memory.
 * Parameter file_path is considered relative to relative_to instead of the current working directory
 * if relative_to is set to a value != 0 and file_path is a relative path
 *
 * Files can be mapped into memory only page by page.
 * If the file size is not a multiple of <pagesize_vm> all memory
 * of the last mapped page which is beyond the size of the underlying file
 * is filled with 0, and writes to that region are not written to the file.
 *
 * Parameter:
 * mfile       - Memory mapped file object
 * file_path   - Path of file to be read or written. A relatice path is relative to relative_to or the current working directory.
 * file_offset - The offset of the first byte in the file whcih should be mapped. Must be a multiple of <pagesize_vm>.
 *               If file_offset >= filesize then ENODATA is returned => Files with length 0 always generate ENODATA error.
 * size        - The number of bytes which should be mapped. The mapping is always done in chunks of <pagesize_vm>.
 *               size (in case it is != 0) is increased until it is a multiple of <pagesize_vm>.
 *               If file_offset + size > filesize then size is silently truncated (file_soffset + size == filesize).
 *               If size is set to 0 then this means to map the whole file starting from file_offset.
 *               If the file is bigger than could be mapped into memory ENOMEM is returned.
 * relative_to - If this parameter is != 0 and file_path is a relative path then file_path is considered relative to the path determined by this parameter.
 * mode        - Determines if the file is opened for reading or (reading and writing). Allowed values are <accessmode_READ>, <accessmode_RDWR_PRIVATE> and <accessmode_RDWR_SHARED>.
 * */
int init_mmfile(/*out*/mmfile_t * mfile, const char * file_path, off_t file_offset, size_t size, accessmode_e mode, const struct directory_t * relative_to /*0=>current_working_directory*/) ;

/* function: initfd_mmfile
 * Maps a file referenced by <sys_file_t> into memory.
 * The function does the same as <init_mmfile> except it does not open a file but takes a file descriptor to the already opened file.
 * The file must always be opened with read access and also write access in case mode contains <accessmode_WRITE>.
 *
 * Attention:
 * If the file_offset + size is bigger than the size of the underlying file accessing the memory which has no file backing
 * is undefinded. The operating system creates a bus error exception in cases where a whole memory page has no backing file object.
 * If the size is set to 0 no mapping is done at all and <mmfile_t> is initialized with a 0 (<memblock_INIT_FREEABLE>).
 * */
int initfd_mmfile(/*out*/mmfile_t * mfile, sys_file_t fd, off_t file_offset, size_t size, accessmode_e mode) ;

/* function: initsplit_mmfile
 * Split a memory mapping into two. After return destheadmfile maps the first headsize bytes. headsize must be a multiple of <pagesize_vm>.
 * desttailmfile maps the last (size_mmfile(sourcemfile)-headsize) bytes.
 * If sourcemfile is not equal to destheadmfile or desttailmfile it is initialized to <mmfile_INIT_FREEABLE>. */
int initsplit_mmfile(/*out*/mmfile_t * destheadmfile, /*out*/mmfile_t * desttailmfile, size_t headsize, mmfile_t * sourcemfile) ;

/* function: initmove_mmfile
 * Moves content of sourcemfile to destmfile. sourcemfile is also reset to <mmfile_INIT_FREEABLE>. */
void initmove_mmfile(/*out*/mmfile_t * restrict destmfile, mmfile_t * restrict sourcemfile) ;

/* function: free_mmfile
 * Frees all mapped memory and closes the file. */
int free_mmfile(mmfile_t * mfile) ;

// group: query

/* function: isinit_mmfile
 * Returns true if mfile != <mmfile_INIT_FREEABLE>. */
bool isinit_mmfile(const mmfile_t * mfile) ;

/* function: addr_mmfile
 * Returns the lowest address of the mapped memory.
 * The memory is always mapped in chunks of <pagesize_vm>.
 * The memory which can be accessed is at least [<addr_mmfile> .. <addr_mmfile>+<size_mmfile>]. */
uint8_t * addr_mmfile(const mmfile_t * mfile) ;

/* function: size_mmfile
 * Returns the size of the mapped memory.
 * The memory which corresponds to the underlying file is exactly
 * [<addr_mmfile> .. <addr_mmfile>+<size_mmfile>]. */
size_t  size_mmfile(const mmfile_t * mfile) ;

/* function: alignedsize_mmfile
 * Returns the size of the mapped memory.
 * This value is a multiple of <pagesize_vm> and it is >= <size_mmfile>.
 * The mapped memory region is [<addr_mmfile> .. <addr_mmfile>+<alignedsize_mmfile>]
 * but only [<addr_mmfile> .. <addr_mmfile>+<size_mmfile>] corresponds to the underlying file. */
size_t  alignedsize_mmfile(const mmfile_t * mfile) ;

// group: change

/* function: seek_mmfile
 * Use it to change offset of the underlying file mapped into memory.
 * The file_offset must be a multiple of <pagesize_vm> -- see also <initfd_mmfile>.
 *
 * Attention:
 * If size_file(fd) - file_offset (see <file_t.size_file>) is smaller than
 * the value returned by <size_mmfile> accessing outside the memory address range can result
 * in a SIGBUS (see <initfd_mmfile>). The solution is to handle the size on a higher level
 * after a change of the file offset.
 * */
int seek_mmfile(mmfile_t * mfile, sys_file_t fd, off_t file_offset, accessmode_e mode) ;


// section: inline implementation

/* define: addr_mmfile
 * Implements <mmfile_t.addr_mmfile>. */
#define addr_mmfile(mfile)             ((mfile)->addr)

/* define: alignedsize_mmfile
 * Implements <mmfile_t.alignedsize_mmfile>. */
#define alignedsize_mmfile(mfile)      ((size_mmfile(mfile) + (pagesize_vm()-1)) & ~(pagesize_vm()-1))

/* define: initmove_mmfile
 * Implements <mmfile_t.initmove_mmfile>. */
#define initmove_mmfile(destmfile, sourcemfile)             \
   do {                                                     \
      mmfile_t * _sourcemfile = (sourcemfile) ;             \
      *(destmfile) = *(_sourcemfile) ;                      \
      *(_sourcemfile) = (mmfile_t) mmfile_INIT_FREEABLE ;   \
   } while (0)

/* define: isinit_mmfile
 * Implements <mmfile_t.isinit_mmfile>. */
#define isinit_mmfile(mfile)           (0 != addr_mmfile(mfile) || 0 != size_mmfile(mfile))

/* define: size_mmfile
 * Implements <mmfile_t.size_mmfile>. */
#define size_mmfile(mfile)             ((mfile)->size)


#endif
