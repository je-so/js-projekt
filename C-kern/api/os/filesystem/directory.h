/* title: Directory Functions
   Access directory content.

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

   file: C-kern/api/os/filesystem/directory.h
    Header file of <Directory Functions>.

   file: C-kern/os/Linux/directory.c
    Linux specific implementations of <Directory Functions>.
*/
#ifndef CKERN_OS_FILESYSTEM_DIRECTORY_HEADER
#define CKERN_OS_FILESYSTEM_DIRECTORY_HEADER

/* typedef: typedef directory_stream_t
 * shortcut for <directory_stream_t> */
typedef struct directory_stream_t directory_stream_t ;

/* enums: filetype_e
 * Encodes the type of the file the filename refers to.
 *
 * ftUnknown         - Unknown file type.
 * ftBlockDevice     - A block device special file. For example a hard disk.
 * ftCharacterDevice - A character device special file. For example a keyboard.
 * ftDirectory       - Directory type.
 * ftNamedPipe       - Named pipe type.
 * ftSymbolicLink    - A symbolic link. This value is returned from <readnext_directorystream> only if the file the symbolic link refers to does not exist.
 *                     If it exists the type of the linked file is returned.
 * ftRegularFile     - A normal data file.
 * ftSocket          - A unix system socket.
 * * */
enum filetype_e {
   ftUnknown
   ,ftBlockDevice
   ,ftCharacterDevice
   ,ftDirectory
   ,ftNamedPipe
   ,ftSymbolicLink
   ,ftRegularFile
   ,ftSocket
} ;
typedef enum filetype_e filetype_e ;


// section: Functions

// group: lifetime

/* define: directory_stream_INIT_FREEABLE
Static initializer for <directory_stream_t>: makes calling of <free_directorystream> safe. */
#define directory_stream_INIT_FREEABLE (directory_stream_t) { .sysentry = (void*)0, .path = (void*)0 }
/* function: init_directorystream
 * Opens directory stream for read access.
 * If *working_dir* is NULL then the *dir_path* is relative to the current working directory else it is considered
 * relative to *working_dir*. If *dir_path* is absolute it does not matter what value *working_dir* has.
 *
 * Current working directory:
 * To open the current working directory *dir_path* can be set to either "." or "".
 *
 * Returns:
 * 0       - Directory stream is open and *dir* is valid (in case of error dir is not changed).
 * EACCES  - Permission denied.
 * EMFILE  - Too many file descriptors in use by process.
 * ENFILE  - Too many files are currently open in the system.
 * ENOENT  - Directory does not exist, or name is an empty string.
 * ENOMEM  - Insufficient memory to complete the operation.
 * ENOTDIR - name is not a directory. */
extern int init_directorystream(/*out*/directory_stream_t * dir, const char * dir_path, const directory_stream_t * working_dir) ;
/* function: inittemp_directorystream
 * Creates a temporary directory read / writeable by the user and opens it for reading.
 * The name of the directory is prefixed with *name_prefix* and it is created in the temporary system directory. */
extern int inittemp_directorystream(/*out*/directory_stream_t * dir, const char * name_prefix ) ;
/* function: free_directorystream
 * Closes open directory stream. Frees allocated memory. Calling free twice is safe. Calling it without a preceding init results in undefined behaviour. */
extern int free_directorystream(directory_stream_t * dir) ;

// group: query

/* function: isvalid_directory */
extern int isvalid_directory( const char * const checked_path, const char * const basedir ) ;

/* function: filesize_directory
 * Returns the filesize of a file with path »file_path«.
 * If *working_dir* is NULL then the *dir_path* is relative to the current working directory else it is considered
 * relative to *working_dir*. If *dir_path* is absolute it does not matter what value *working_dir* has. */
extern int filesize_directory( const char * file_path, const directory_stream_t * working_dir, /*out*/ off_t * file_size ) ;

// group: read
/* function: readnext_directorystream
 * Reads the next directory entry and returns its name and type (ftype could be NULL).
 *
 * Parameters:
 * name  - On success a pointer to the file name is returned. Name should never be freed.
 *         It is valid until any other function on dir is called.
 *         If there is no next entry *name* is set to NULL.
 * ftype - On success and if ftype != NULL it contains the type of file. */
extern int readnext_directorystream(directory_stream_t * dir, /*out*/const char ** name, /*out*/filetype_e * ftype) ;
/* function: returntobegin_directorystream
 * Sets the current read position to the begin of the directory stream. The next call to <readnext_directorystream>
 * returns the first entry again. */
extern int returntobegin_directorystream(directory_stream_t * dir) ;

// group: write
/* function: makedirectory_directorystream */
extern int makedirectory_directorystream(directory_stream_t * dir, const char * directory_name) ;
/* function: makefile_directorystream */
extern int makefile_directorystream(directory_stream_t * dir, const char * file_name) ;
/* function: remove_directorystream
 * Removes the opened directory (stream) from the file system. */
extern int remove_directorystream(directory_stream_t * dir) ;
/* function: removedirectory_directorystream */
extern int removedirectory_directorystream(directory_stream_t * dir, const char * directory_name) ;
/* function: removefile_directorystream */
extern int removefile_directorystream(directory_stream_t * dir, const char * file_name) ;


// group: test
#ifdef KONFIG_UNITTEST
/* function: unittest_os_directory */
extern int unittest_os_directory(void) ;
#endif

// section: Objects
/* struct: directory_stream_t */
struct directory_stream_t
{
   sys_directory_t         sys_dir ;
   sys_directory_entry_t * sysentry ;
   /* variable: path_len
    * Length of path name of opened directory. Length is measured in bytes (without trailing 0 byte) and is stored in <directory_stream_t::path>.
    * Initialized after call to <init_directorystream> or <maketemp_directorystream>. */
   size_t                  path_len ;
   /* variable: path_size
    * size in bytes allocated for <directory_stream_t::path> variable (including 0 byte).
    * path_size is chosen in such a way that a filename with maximum length can be appended after &path[path_len]. */
   size_t                  path_size ;
   /* variable: path
    * The path name of the opened directory. It always ends in a '/'. */
   char *                  path ;
} ;

#endif
