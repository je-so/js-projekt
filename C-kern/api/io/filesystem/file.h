/* title: File

   Offers an interface to handle system files.
   A file is described as system specific file descriptor <sys_iochannel_t>
   which is renamed into <file_t>.

   A file descriptor is an id that identifies an input/output channel
   like files, network connections or other system specific devices.
   Therefore I/O operations on <file_t> can be used also on other I/O objects.

   Includes:
   You need to include "C-kern/api/io/accessmode.h" (<IO-Accessmode>)
   and "C-kern/api/io/iochannel.h" (<IOChannel>) before htis file.

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

   file: C-kern/platform/Linux/io/file.c
    Linux specific implementation <File Linux>.
*/
#ifndef CKERN_IO_FILESYSTEM_FILE_HEADER
#define CKERN_IO_FILESYSTEM_FILE_HEADER

// forward
struct directory_t ;

/* typedef: file_t
 * Export <file_t>, alias for <sys_iochannel_t>.
 * Describes a persistent binary object with a name.
 * Describes an opened file for doing reading and/or writing.
 * The file is located in a system specific filesystem. */
typedef sys_iochannel_t                   file_t ;

/* enums: file_e
 * Standard files which are usually open at process start by convention.
 *
 * file_STDIN  - The file descriptor value of the standard input channel.
 * file_STDOUT - The file descriptor value of the standard output channel.
 * file_STDERR - The file descriptor value of the standard error (output) channel.
 * */
enum file_e {
   file_STDIN  = sys_iochannel_STDIN,
   file_STDOUT = sys_iochannel_STDOUT,
   file_STDERR = sys_iochannel_STDERR
} ;

typedef enum file_e                       file_e ;


// section: Functions

// group: update

/* function: remove_file
 * Removes created file from filesystem. */
int remove_file(const char * filepath, struct directory_t * relative_to) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_file
 * Unittest for file interface. */
int unittest_io_file(void) ;
#endif


// struct: file_t

// group: lifetime

/* define: file_FREE
 * Static initializer. */
#define file_FREE sys_iochannel_FREE

/* function: init_file
 * Opens a file identified by its path and name.
 * The filepath can be either a relative or an absolute path.
 * If filepath is relative it is considered relative to the directory relative_to.
 * If relative_to is set to NULL then it is considered relative to the current working directory. */
int init_file(/*out*/file_t * fileobj, const char* filepath, accessmode_e iomode, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: initappend_file
 * Opens or creates a file to append only.
 * See <init_file> for a description of parameters *filepath* and *relative_to*.
 * The file can be only be written to. Every written content is appended to end of the file
 * even if more than one process is writing to the same file. */
int initappend_file(/*out*/file_t * fileobj, const char* filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: initcreate_file
 * Creates a file identified by its path and name.
 * If the file exists already EEXIST is returned.
 * The filepath can be either a relative or an absolute path.
 * If filepath is relative it is considered relative to the directory relative_to.
 * If relative_to is set to NULL then it is considered relative to the current working directory. */
int initcreate_file(/*out*/file_t * fileobj, const char* filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: initmove_file
 * Moves content of sourcefile to destfile. sourcefile is also reset to <file_FREE>. */
static inline void initmove_file(/*out*/file_t * restrict destfile, file_t * restrict sourcefile) ;

/* function: free_file
 * Closes an opened file and frees held resources. */
int free_file(file_t * fileobj) ;

// group: query

/* function: accessmode_file
 * Returns access mode (read and or write) for a io channel.
 * Returns <accessmode_NONE> in case of an error. */
accessmode_e accessmode_file(const file_t fileobj) ;

/* function: io_file
 * Returns <iochannel_t> for a file. Do not free the returned value. */
sys_iochannel_t io_file(const file_t fileobj) ;

/* function: isfree_file
 * Returns true if the file was opened with <init_file>.
 * Returns false if file is in a freed (closed) state and after <free_file>
 * has been called. */
static inline bool isfree_file(const file_t fileobj) ;

/* function: isvalid_file
 * Returns *true* if fileobj is valid.
 * A return value of true implies <isfree_file> returns false.
 * <isvalid_file> checks that fileobj refers to a valid file
 * which is known to the operating system.
 * It is therefore more costly than <isfree_file>. */
bool isvalid_file(const file_t fileobj) ;

/* function: size_file
 * Returns the size in bytes of the file. */
int size_file(const file_t fileobj, /*out*/off_t * file_size) ;

// group: I/O

/* function: read_file
 * Reads size bytes from file referenced by fileobj into buffer.
 * See <read_iochannel>. */
int read_file(file_t fileobj, size_t size, /*out*/void * buffer/*[size]*/, size_t * bytes_read) ;

/* function: write_file
 * Writes size bytes from buffer to file referenced by fileobj.
 * See <write_iochannel>. */
int write_file(file_t fileobj, size_t size, const void * buffer/*[size]*/, size_t * bytes_written) ;

/* function: advisereadahead_file
 * Expects data to be accessed sequentially and in the near future.
 * The operating system is advised to read ahead the data beginning at offset and extending for length bytes
 * right now. The value 0 for length means: until the end of the file. */
int advisereadahead_file(file_t fileobj, off_t offset, off_t length) ;

/* function: advisedontneed_file
 * Expects data not to be accessed in the near future.
 * The operating system is advised to free the page cache for the file beginning at offset and extending
 * for length bytes. The value 0 for length means: until the end of the file. */
int advisedontneed_file(file_t fileobj, off_t offset, off_t length) ;

// group: allocation

/* function: truncate_file
 * Truncates file to file_size bytes.
 * Data beyond file_size is lost. If file_size is bigger than the value <size_file> returns
 * the file is either extended with 0 bytes or EPERM is returned. This call only changes the length
 * but does not allocate data blocks on the file system. */
int truncate_file(file_t fileobj, off_t file_size) ;

/* function: allocate_file
 * Preallocates blocks on disk filled with 0 bytes.
 * The preallocation is faster than filling a file with 0 bytes and it ensures that
 * a writer does not run out of disk space.
 * This call can only grow the file size.
 * Returns ENOSPC if not enough space is available on the disk. */
int allocate_file(file_t fileobj, off_t file_size) ;


// section: inline implementation

// group: file_t

/* function: initmove_file
 * Implements <file_t.initmove_file>. */
static inline void initmove_file(file_t * restrict destfile, file_t * restrict sourcefile)
{
   *destfile   = *sourcefile ;
   *sourcefile = file_FREE ;
}

/* define: io_file
 * Implements <file_t.io_file>. */
#define io_file(fileobj)                  (fileobj)

/* function: isfree_file
 * Implements <file_t.isfree_file>.
 * This function assumes that file is a primitive type. */
static inline bool isfree_file(file_t fileobj)
{
   return file_FREE == fileobj ;
}

/* function: read_file
 * Implements <file_t.read_file>. */
#define read_file(fileobj, size, buffer, bytes_read)  \
         (read_iochannel(fileobj, size, buffer, bytes_read))

/* function: write_file
 * Implements <file_t.write_file>. */
#define write_file(fileobj, size, buffer, bytes_written) \
         (write_iochannel(fileobj, size, buffer, bytes_written))

#endif
