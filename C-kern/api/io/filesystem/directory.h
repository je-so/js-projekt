/* title: Directory
   Access directory content.
   Read and write to a file system directory.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/filesystem/directory.h
    Header file of <Directory>.

   file: C-kern/platform/Linux/io/directory.c
    Linux specific implementation <Directory Linux>.
*/
#ifndef CKERN_IO_FILESYSTEM_DIRECTORY_HEADER
#define CKERN_IO_FILESYSTEM_DIRECTORY_HEADER

// forward
struct wbuffer_t;

/* typedef: struct directory_t
 * Export opaque <directory_t> to read/write a directory in the file system.
 *
 * Can also be used to represent a position in the file system.
 * Other file system operations can take an argument of type <directory_t>
 * and resolve relative paths against it.
 *
 * In case the directory is set to 0 relative paths are resolved by using the
 * current working directory as starting point. */
typedef struct directory_t directory_t;

/* enums: filetype_e
 * Encodes the type of the file the filename refers to.
 *
 * ftUnknown         - Unknown file type.
 * ftBlockDevice     - A block device special file. For example a hard disk.
 * ftCharacterDevice - A character device special file. For example a keyboard.
 * ftDirectory       - Directory type.
 * ftNamedPipe       - Named pipe type.
 * ftSymbolicLink    - A symbolic link. This value is returned from <next_directory> only if the file the symbolic link refers to does not exist.
 *                     If it exists the type of the linked file is returned.
 * ftRegularFile     - A normal data file.
 * ftSocket          - A unix system socket.
 * * */
typedef enum filetype_e {
   ftUnknown,
   ftBlockDevice,
   ftCharacterDevice,
   ftDirectory,
   ftNamedPipe,
   ftSymbolicLink,
   ftRegularFile,
   ftSocket
} filetype_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_directory
 * Test reading and writing content from and to directories. */
int unittest_io_directory(void) ;
#endif


// struct: directory_t

// group: lifetime

/* function: new_directory
 * Opens directory stream for read access.
 * If *relative_to* is NULL then the *dir_path* is relative to the current working directory else it is considered
 * relative to *relative_to*. If *dir_path* is absolute it does not matter what value *relative_to* has.
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
int new_directory(/*out*/directory_t ** dir, const char * dir_path, const directory_t * relative_to/*0 => current working directory*/) ;

/* function: newtemp_directory
 * Creates a temporary directory read / writeable by the user and opens it for reading.
 * The name of the directory is prefixed with *name_prefix* and it is created in the temporary system directory.
 * The name of the created directory is returned in parameter *dir_path*.
 * Set this parameter to 0 if you do not need the name. */
int newtemp_directory(/*out*/directory_t ** dir, const char * name_prefix) ;

/* function: delete_directory
 * Closes open directory stream. Frees allocated memory. Calling free twice is safe. Calling it without a preceding init results in undefined behaviour. */
int delete_directory(directory_t ** dir) ;

// group: query

/* function: trypath_directory
 * Checks that file_path refers to an existing file or directory.
 * *file_path* is considered relative to dir or to the current
 * working directory if dir is null.
 * If *file_path* is absolute the value of *dir* does not matter.
 *
 * Returns:
 * 0         - file_path refers to an existing file or directory.
 * ENOENT    - file_path does not refer to an existing file system entry.
 *             No error log entry is written. */
int trypath_directory(const directory_t* dir/*0 => current working directory*/, const char * const file_path);

/* function: io_directory
 * Returns <iochannel_t> (file descriptor) of the opened directory.
 * Do not free it after usage else dir can not be accessed any more. */
sys_iochannel_t io_directory(const directory_t* dir);

/* function: filesize_directory
 * Returns the filesize of a file with path »file_path«.
 * *file_path* is considered relative to dir. If dir is NULL then it is considered
 * relative to the current working directory.
 * If *file_path* is absolute the value of *dir* does not matter. */
int filesize_directory(const directory_t* dir/*0 => current working directory*/, const char* file_path, /*out*/off_t* file_size) ;

/* function: path_directory
 * Returns absolute real path of directory dir in path.
 * If dir was initializated with a path containing symlinks the returned path
 * will differ from the one used during initialization.
 * The returned path contains at least '/' as first character.
 * A trailing '\0' is appended at the end of the returned path.
 * Do not forget to call <cstring_t.adaptsize_cstring> if path wraps a <cstring_t>. */
int path_directory(const directory_t* dir, /*ret*/struct wbuffer_t* path);

// group: read

/* function: next_directory
 * Reads the next directory entry and returns its name and type (ftype could be NULL).
 *
 * Parameters:
 * name  - On success a pointer to the file name is returned. Name should never be freed.
 *         It is valid until any other function on dir is called.
 *         If there is no next entry *name* is set to NULL.
 * ftype - On success and if ftype != NULL it contains the type of file. */
int next_directory(directory_t* dir, /*out*/const char** name, /*out*/filetype_e * ftype);

/* function: gofirst_directory
 * Sets the current read position to the begin of the directory stream. The next call to <next_directory>
 * returns the first entry again. */
int gofirst_directory(directory_t* dir);

// group: write

/* function: makedirectory_directory
 * Creates directory with directory_path relative to dir.
 * If dir is 0 the directory_path is relative to the current working directory.
 * If directory_path is absolute then the parameter dir does not matter. */
int makedirectory_directory(directory_t* dir/*0 => current working directory*/, const char* directory_path);

/* function: makefile_directory
 * Creates a new file with file_path relative to dir.
 * If dir is 0 the file_path is relative to the current working directory.
 * If file_path is absolute then the parameter dir does not matter. */
int makefile_directory(directory_t * dir/*0 => current working directory*/, const char * file_path, off_t file_length);

/* function: removedirectory_directory
 * Removes the empty directory with directory_path relative to dir.
 * If dir is 0 the directory_path is relative to the current working directory.
 * If directory_path is absolute then the parameter dir does not matter. */
int removedirectory_directory(directory_t* dir/*0 => current working directory*/, const char* directory_path);

/* function: removefile_directory
 * Removes the file with file_path relative to dir.
 * If dir is 0 the file_path is relative to the current working directory.
 * If file_path is absolute then the parameter dir does not matter. */
int removefile_directory(directory_t* dir/*0 => current working directory*/, const char* file_path);


// section: inline implementation

/* define: io_directory
 * Implements <directory_t.io_directory>. */
#define io_directory(dir) \
         (dirfd((DIR*)CONST_CAST(directory_t, dir)))


#endif
