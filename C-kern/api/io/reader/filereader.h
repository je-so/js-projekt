/* title: FileReader

   Offers a simple interface for reading and buffering the content of a file.

   TODO: remove type and replace it after utf8scanner_t has been remove

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/reader/filereader.h
    Header file <FileReader>.

   file: C-kern/io/reader/filereader.c
    Implementation file <FileReader impl>.
*/
#ifndef CKERN_IO_READER_FILEREADER_HEADER
#define CKERN_IO_READER_FILEREADER_HEADER

// === forward
struct directory_t;
struct memstream_ro_t;

// === exported types
struct filereader_t;



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_reader_filereader
 * Test <filereader_t> functionality. */
int unittest_io_reader_filereader(void);
#endif


/* struct: filereader_page_t
 * Private type used in implementation of <filereader_t>. */
typedef struct filereader_page_t {
   uint8_t* addr ;
   size_t   size ;
} filereader_page_t;

// group: lifetime

/* define: filereader_page_FREE
 * Static initializer. */
#define filereader_page_FREE \
         {0, 0}


/* struct: filereader_t
 * Reads file data into internal memory buffers.
 * At least two buffers are supported. If one buffer is in use the other could be filled with new data from the file.
 * The function <readnext_filereader> returns a buffer containing the next read data.
 * Use <release_filereader> if you do not longer need it. For every called <readnext_filereader>
 * you need to call <release_filereader>. Always the oldest read buffer is released. */
typedef struct filereader_t {
   /* variable: ioerror
    * Safes status of last read access to <file>.
    * In case <ioerror> != 0 no more call is made to the underlying file. */
   int         ioerror;
   /* variable: unreadsize
    * The size of buffered data for which <readnext_filereader> is not called. */
   size_t      unreadsize;
   /* variable: nextindex
    * Index into <page>.
    * It is the index of the buffer which must be returned
    * during the next call to <readnext_filereader>. */
   uint8_t     nextindex;
   /* variable: nrfreebuffer
    * Number of released or unread buffers.
    * This value can range from 0 to 2. */
   uint8_t     nrfreebuffer;
   /* variable: fileoffset
    * Offset into <file> where the next read operation begins. */
   off_t       fileoffset;
   /* variable: filesize
    * The size of the io-stream <file> refers to. */
   off_t       filesize;
   /* variable: file
    * The file from which is read. */
   sys_iochannel_t  file;
   /* variable: page
    * The buffered input of the file. */
   filereader_page_t page[2];
} filereader_t;

// group: static configuration

/* define: filereader_SYS_BUFFER_SIZE
 * The sum of the size of two allocated buffers.
 * Every buffer is allocated half the size in bytes of this value.
 * This value can be overwritten in C-kern/resource/config/modulevalues. */
#define filereader_SYS_BUFFER_SIZE (4*4096)

// group: lifetime

/* define: filereader_FREE
 * Static initializer. */
#define filereader_FREE \
         { 0, 0, 0, 0, 0, 0, sys_iochannel_FREE, { filereader_page_FREE, filereader_page_FREE } }

/* function: init_filereader
 * Opens file for reading into a double buffer.
 * Works also for files > 2GB on 32 bit systems. */
int init_filereader(/*out*/filereader_t * frd, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: free_filereader
 * Closes file and frees allocated buffers. */
int free_filereader(filereader_t * frd) ;

// group: query

/* function: sizebuffer_filereader
 * Returns the buffer size in bytes. See also <filereader_t.filereader_SYS_BUFFER_SIZE>.
 * The size is aligned to value (2 * pagesize_vm()). Therefore the two buffers of the double buffer
 * configuration are aligned to pagesize_vm(). */
size_t sizebuffer_filereader(void) ;

/* function: ioerror_filereader
 * Returns the I/0 error (>0) or 0 if no error occurred.
 * If an error occurred every call to <readnext_filereader>
 * returns this error code (EIO, ENOMEM, ...). */
int ioerror_filereader(const filereader_t * frd) ;

/* function: iseof_filereader
 * Returns true if end of file is reached.
 * If there is no more data to read <readnext_filereader> will also return ENODATA. */
bool iseof_filereader(const filereader_t * frd) ;

/* function: isfree_filereader
 * Returns true in case frd == <filereader_FREE>. */
bool isfree_filereader(const filereader_t * frd) ;

// group: setter

/* function: setioerror_filereader
 * Sets ioerror of frd. After an I/O error occurred this function is called.
 * YOu can call it to simulate an I/O error. Every call to <readnext_filereader>
 * returns this error. To clear it call this function with ioerr set to 0. */
void setioerror_filereader(filereader_t * frd, int ioerr);

// group: read

/* function: readnext_filereader
 * Returns buffer containing the next block of input data.
 * If you do no longer need it use <release_filereader> to release
 * the oldest buffer. The current implementation supports only
 * two unreleased buffers.
 *
 * Returns:
 * 0       - Read new buffer.
 * ENODATA - All data read.
 * ENOBUFS - No more buffer available. Call <release_filereader> first before calling this function.
 * EIO     - Input/Output error (ENOMEM or other error codes are also possible). */
int readnext_filereader(filereader_t * frd, /*out*/struct memstream_ro_t * buffer) ;

/* function: release_filereader
 * Releases the oldest read buffer.
 * If no buffer was read this function does nothing. */
void release_filereader(filereader_t * frd) ;

/* function: unread_filereader
 * The last buffer returned by <readnext_filereader> is marked as unread.
 * The next call to <readnext_filereader> returns the same buffer.
 * If no buffer was read this function does nothing. */
void unread_filereader(filereader_t * frd) ;



// section: inline implementation

/* define: ioerror_filereader
 * Implements <filereader_t.ioerror_filereader>. */
#define ioerror_filereader(frd)        \
         ((frd)->ioerror)

/* define: iseof_filereader
 * Implements <filereader_t.iseof_filereader>. */
#define iseof_filereader(frd)          \
         ( __extension__ ({            \
            const filereader_t * _f;   \
            _f = (frd);                \
            (  _f->fileoffset          \
               == _f->filesize);       \
         }))

/* define: setioerror_filereader
 * Implements <filereader_t.setioerror_filereader>. */
#define setioerror_filereader(frd, ioerr) \
         ((void)((frd)->ioerror = (ioerr)))

#endif
