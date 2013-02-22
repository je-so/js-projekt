/* title: FileReader

   Offers a simple interface for reading and buffering the content of a file.

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

   file: C-kern/api/io/reader/filereader.h
    Header file <FileReader>.

   file: C-kern/io/filesystem/filereader.c
    Implementation file <FileReader impl>.
*/
#ifndef CKERN_IO_READER_FILEREADER_HEADER
#define CKERN_IO_READER_FILEREADER_HEADER

#include "C-kern/api/io/accessmode.h"

// forward
struct directory_t ;
struct stringstream_t ;

/* typedef: struct filereader_t
 * Export <filereader_t> into global namespace. */
typedef struct filereader_t            filereader_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_reader_filereader
 * Test <filereader_t> functionality. */
int unittest_io_reader_filereader(void) ;
#endif


/* struct: filereader_t
 * Reads file data into internal memory buffers.
 * At least two buffers are supported. If one buffer is in use the other could be filled with new data from the file.
 * The function <acquirenext_filereader> returns a buffer containing the next read data.
 * Use <releaseold_filereader> if you do not longer need it. For every called <acquirenext_filereader>
 * you need to call <releaseold_filereader>. Always the oldest acquired buffer is released. */
struct filereader_t {
   /* variable: ioerror
    * Safes status of last read access to <file>.
    * In case <ioerror> != 0 no more call is made to the underlying file. */
   int         ioerror ;
   /* variable: unreadsize
    * The size of buffered data for which <acquirenext_filereader> is not called. */
   size_t      unreadsize ;
   /* variable: nextindex
    * Index into <mmfile>.
    * It is the index of the buffer which must be returned
    * during the next call to <acquirenext_filereader>. */
   uint8_t     nextindex ;
   /* variable: nrfreebuffer
    * Number of released or unacquired buffers.
    * This value can range from 0 to 2. */
   uint8_t     nrfreebuffer ;
   /* variable: fileoffset
    * Offset into <file> where the next read operation begins. */
   off_t       fileoffset ;
   /* variable: filesize
    * The size of the io-stream <file> refers to. */
   off_t       filesize ;
   /* variable: file
    * The file from which is read. */
   sys_file_t  file ;
   /* variable: mmfile
    * The buffered input of the file. */
   struct {
      uint8_t  * addr ;
      size_t   size ;
   }           mmfile[2] ;
} ;

// group: static configuration

/* define: filereader_SYS_BUFFER_SIZE
 * The sum of the size the two allocated buffers.
 * Every buffer is allocated half the size in bytes of this value.
 * This value can be overwritten in C-kern/resource/config/modulevalues. */
#define filereader_SYS_BUFFER_SIZE     (4*4096)

// group: lifetime

/* define: filereader_INIT_FREEABLE
 * Static initializer. */
#define filereader_INIT_FREEABLE       { 0, 0, 0, 0, 0, 0, sys_file_INIT_FREEABLE, { {0, 0}, {0, 0} } }

/* function: initsb_filereader
 * Opens file for reading into a single buffer.
 * Works only on files < 2GB on 32 bit systems. */
int initsb_filereader(/*out*/filereader_t * frd, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: init_filereader
 * Opens file for reading into a double buffer.
 * Works also for files > 2GB on 32 bit systems. */
int init_filereader(/*out*/filereader_t * frd, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: free_filereader
 * Closes file and frees allocated buffers. */
int free_filereader(filereader_t * frd) ;

// group: query

/* function: buffersize_filereader
 * Returns the buffer size in bytes. See also <filereader_t.filereader_SYS_BUFFER_SIZE>.
 * The size is aligned to value (2 * pagesize_vm()). Therefore every buffer in a double buffer configuration
 * is aligned to pagesize_vm(). */
size_t buffersize_filereader(void) ;

/* function: ioerror_filereader
 * Returns the I/0 error (>0) or 0 if no error occurred.
 * If an error occurred every call to <acquirenext_filereader>
 * returns this error code (EIO, ENOMEM, ...). */
int ioerror_filereader(const filereader_t * frd) ;

/* function: iseof_filereader
 * Returns true if end of file is reached.
 * If there is no more data to read <acquirenext_filereader> will also return ENODATA. */
bool iseof_filereader(const filereader_t * frd) ;

/* function: isfree_filereader
 * Returns true in case frd == <filereader_INIT_FREEABLE>. */
bool isfree_filereader(const filereader_t * frd) ;

/* function: isnext_filereader
 * Returns true if there is a free buffer available.
 * Therefore <acquirenext_filereader> will not return ENOBUFS or ENODATA. */
bool isnext_filereader(const filereader_t * frd) ;

// group: read

/* function: acquirenext_filereader
 * Returns the buffer containing the next block of input data.
 * The current implementation supports only two acquired buffers.
 * Use <release_filereader> to release the oldest buffer.
 *
 * Returns:
 * 0       - New buffer acquired.
 * ENODATA - All data read.
 * ENOBUFS - No more buffer avialable. Call <releaseold_filereader> first before calling this function.
 * EIO     - Input/Output error (ENOMEM or other error codes are also possible). */
int acquirenext_filereader(filereader_t * frd, /*out*/struct stringstream_t * buffer) ;

/* function: release_filereader
 * Releases the oldest acquired buffer.
 * If no buffer is acquired calling release does nothing and returns no error. */
int release_filereader(filereader_t * frd) ;


// section: inline implementation

/* define: ioerror_filereader
 * Implements <filereader_t.ioerror_filereader>. */
#define ioerror_filereader(frd)        \
         ((frd)->ioerror)

/* define: iseof_filereader
 * Implements <filereader_t.iseof_filereader>. */
#define iseof_filereader(frd)          \
         ( __extension__ ({            \
            const filereader_t * _f ;  \
            _f = (frd) ;               \
            (_f->unreadsize == 0       \
            && _f->fileoffset          \
               == _f->filesize) ;      \
         }))

/* define: isnext_filereader
 * Implements <filereader_t.isnext_filereader>. */
#define isnext_filereader(frd)         \
         ( __extension__ ({            \
            const filereader_t * _f ;  \
            _f = (frd) ;               \
            (_f->unreadsize != 0) ;    \
         }))


#endif
