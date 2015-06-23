/* title: File

   Offers an interface to handle system files.
   A file is described as system specific file descriptor <sys_iochannel_t>
   which is renamed into <file_t>.

   A file descriptor is an id that identifies an input/output channel
   like files, network connections or other system specific devices.
   Therefore I/O operations on <file_t> can be used also on other I/O objects.

   Includes:
   You need to include "C-kern/api/io/accessmode.h" (<IO-Accessmode>)
   before this file.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
struct wbuffer_t;
struct directory_t;

/* typedef: file_t
 * Export <file_t>, alias for <sys_iochannel_t>.
 * Describes a persistent binary object with a name.
 * Describes an opened file for doing reading and/or writing.
 * The file is located in a system specific filesystem. */
typedef sys_iochannel_t file_t;

/* enums: file_e
 * Standard files which are usually open at process start by convention.
 *
 * file_STDIN  - The file descriptor value of the standard input channel.
 * file_STDOUT - The file descriptor value of the standard output channel.
 * file_STDERR - The file descriptor value of the standard error (output) channel.
 * */
typedef enum file_e {
   file_STDIN  = sys_iochannel_STDIN,
   file_STDOUT = sys_iochannel_STDOUT,
   file_STDERR = sys_iochannel_STDERR
} file_e;



// section: Functions

// group: update

/* function: remove_file
 * Removes created file from filesystem. */
int remove_file(const char * filepath, struct directory_t* relative_to);

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_file
 * Unittest for file interface. */
int unittest_io_file(void);
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
int init_file(/*out*/file_t* file, const char* filepath, accessmode_e iomode, const struct directory_t* relative_to/*0 => current working dir*/);

/* function: initappend_file
 * Opens or creates a file to append only.
 * See <init_file> for a description of parameters *filepath* and *relative_to*.
 * The file can be only be written to. Every written content is appended to end of the file
 * even if more than one process is writing to the same file. */
int initappend_file(/*out*/file_t* file, const char* filepath, const struct directory_t* relative_to/*0 => current working dir*/);

/* function: initcreate_file
 * Creates a file identified by its path and name.
 * If the file exists already EEXIST is returned.
 * The filepath can be either a relative or an absolute path.
 * If filepath is relative it is considered relative to the directory relative_to.
 * If relative_to is set to NULL then it is considered relative to the current working directory. */
int initcreate_file(/*out*/file_t* file, const char* filepath, const struct directory_t* relative_to/*0 => current working dir*/);

/* function: inittempdeleted_file
 * Erzeugt eine temporäre Datei im Systemverzeichnis P_tmpdir und gibt sie in file zurück.
 * Die Datei wird sofort nach dem Erzeugen als gelöscht markiert, so daß kein Dateiname im
 * System sichtbar ist. Die Datei kann gelesen und beschrieben werden. */
int inittempdeleted_file(/*out*/file_t* file);

/* function: inittemp_file
 * Erzeugt eine temporäre Datei im Systemverzeichnis P_tmpdir und gibt sie in file zurück.
 * Die Datei kann gelesen und beschrieben werden.
 * Der '\0' terminierte Pfad der temporären Datei wird in path zurückgegeben. */
int inittemp_file(/*out*/file_t* file, /*ret*/struct wbuffer_t* path);

/* function: initmove_file
 * Moves content of sourcefile to destfile. sourcefile is also reset to <file_FREE>. */
static inline void initmove_file(/*out*/file_t* restrict destfile, file_t* restrict sourcefile);

/* function: free_file
 * Closes an opened file and frees held resources. */
int free_file(file_t* file);

// group: query

/* function: io_file
 * Returns <iochannel_t> for a file. Do not free the returned value. */
sys_iochannel_t io_file(const file_t file);

/* function: isfree_file
 * Returns true if the file was opened with <init_file>.
 * Returns false if file is in a freed (closed) state and after <free_file>
 * has been called. */
static inline bool isfree_file(const file_t file);

// group: system-query

/* function: accessmode_file
 * Returns access mode (read and or write) for an I/O channel.
 * Returns <accessmode_NONE> in case of an error. */
accessmode_e accessmode_file(const file_t file);

/* function: isvalid_file
 * Returns *true* if file is valid.
 * A return value of true implies <isfree_file> returns false.
 * <isvalid_file> checks that file refers to a valid file
 * which is known to the operating system.
 * It is therefore more costly than <isfree_file>. */
bool isvalid_file(const file_t file);

/* function: path_file
 * Gibt den absoluten und mit einem '\0' Byte beendeten Pfad der Datei file in path zurück. */
int path_file(const file_t file, /*ret*/struct wbuffer_t* path);

/* function: size_file
 * Returns the size in bytes of the file. */
int size_file(const file_t file, /*out*/off_t * file_size);

// group: I/O

/* function: read_file
 * Reads size bytes from file referenced by file into buffer.
 * See <read_iochannel>. */
int read_file(file_t file, size_t size, /*out*/void * buffer/*[size]*/, size_t * bytes_read);

/* function: write_file
 * Writes size bytes from buffer to file referenced by file.
 * See <write_iochannel>. */
int write_file(file_t file, size_t size, const void * buffer/*[size]*/, size_t * bytes_written);

// TODO: implement seek_file, seekrelative_file

/* function: advisereadahead_file
 * Expects data to be accessed sequentially and in the near future.
 * The operating system is advised to read ahead the data beginning at offset and extending for length bytes
 * right now. The value 0 for length means: until the end of the file. */
int advisereadahead_file(file_t file, off_t offset, off_t length);

/* function: advisedontneed_file
 * Expects data not to be accessed in the near future.
 * The operating system is advised to free the page cache for the file beginning at offset and extending
 * for length bytes. The value 0 for length means: until the end of the file. */
int advisedontneed_file(file_t file, off_t offset, off_t length);

// group: allocation

/* function: truncate_file
 * Truncates file to file_size bytes.
 * Data beyond file_size is lost. If file_size is bigger than the value returned by <size_file>
 * the file is either extended with 0 bytes or EPERM is returned. This call only changes the length
 * but does not allocate data blocks on the file system. */
int truncate_file(file_t file, off_t file_size);

/* function: allocate_file
 * Reserviert 0 Byte gefüllte Datenblöcke für eine Datei im Voraus.
 * Der Bereich, für den Datenblöcke reserviert werden, startet mit Fileoffset offset
 * und erstreckt sich über len Bytes.
 * Die Reservierung ist schneller als explizit 0 Bytes zu schreiben und stellt zudem sicher,
 * daß eine Schreiboperation nicht wegen mangelndem Plattenplatz abbricht.
 * Dieser Aufruf erhöht die Dateilänge, falls offset+len größer als die aktuelle Dateilänge ist.
 *
 * Return:
 * 0      - OK, Datenblöcke wurden für den Bereich [offset..offset+len) reserviert.
 * ENOSPC - Auf der Platte ist nicht genügend Platz vrohanden. */
int allocate_file(file_t file, off_t offset, off_t len);


// section: inline implementation

// group: file_t

/* function: initmove_file
 * Implements <file_t.initmove_file>. */
static inline void initmove_file(file_t* restrict destfile, file_t* restrict sourcefile)
{
   *destfile   = *sourcefile;
   *sourcefile = file_FREE;
}

/* define: io_file
 * Implements <file_t.io_file>. */
#define io_file(file) \
         (file)

/* function: isfree_file
 * Implements <file_t.isfree_file>.
 * This function assumes that file is a primitive type. */
static inline bool isfree_file(file_t file)
{
   return file_FREE == file;
}

/* function: read_file
 * Implements <file_t.read_file>. */
#define read_file(file, size, buffer, bytes_read)  \
         (read_iochannel(file, size, buffer, bytes_read))

/* function: write_file
 * Implements <file_t.write_file>. */
#define write_file(file, size, buffer, bytes_written) \
         (write_iochannel(file, size, buffer, bytes_written))

#endif
