/* title: IOChannel

   Describes an input/output channel which is implemented as file descriptor
   on POSIX systems.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/iochannel.h
    Header file <IOChannel>.

   file: C-kern/platform/Linux/io/iochannel.c
    Implementation file <IOChannel impl>.
*/
#ifndef CKERN_IO_IOCHANNEL_HEADER
#define CKERN_IO_IOCHANNEL_HEADER

/* typedef: struct iochannel_t
 * Make <iochannel_t> an alias of <sys_iochannel_t>. */
typedef sys_iochannel_t                   iochannel_t ;

/* enums: iochannel_e
 * Standard channels which are usually open at process start by convention.
 *
 * iochannel_STDIN  - The default standard input channel.
 * iochannel_STDOUT - The default standard output channel.
 * iochannel_STDERR - The default standard error (output) channel.
 * */
enum iochannel_e {
   iochannel_STDIN  = sys_iochannel_STDIN,
   iochannel_STDOUT = sys_iochannel_STDOUT,
   iochannel_STDERR = sys_iochannel_STDERR
} ;

typedef enum iochannel_e                  iochannel_e ;


// section: Functions

// group: query

/* function: nropen_iochannel
 * Returns number of opened I/O data channels.
 * The number of underlying data streams could be smaller cause oseveral I/O channel
 * could reference the same data object.
 * You can use this function at the beginning and end of any transaction to check if
 * an I/O object (file, network socket...) was not closed properly. */
int nropen_iochannel(/*out*/size_t * number_open) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_iochannel
 * Test <iochannel_t> functionality. */
int unittest_io_iochannel(void) ;
#endif


/* struct: iochannel_t
 * Describes an input/output channel like a file or network connection.
 * On POSIX systems it is a synonym for a file descriptor.
 *
 * A <sys_iochannel_t>, which is returned by <io_directory> and other functions
 * can be manipulated by this interface. Do not free a descriptor which is returned
 * by such functions else the underlying object will no more work.
 *
 * If you use <initcopy_iochannel> to initialize an <iochannel_t> from a value acquired
 * by a call to <io_socket> or any other such function, you need to call <free_iochannel>. */
typedef sys_iochannel_t    iochannel_t ;

// group: lifetime

/* define: iochannel_FREE
 * Static initializer. */
#define iochannel_FREE sys_iochannel_FREE

/* function: initcopy_iochannel
 * Makes ioc a duplicate of from_ioc.
 * Freeing ioc does not affect from_ioc.
 * Writing to ioc or reading from it does affect the content of the underlying I/O data stream. */
int initcopy_iochannel(/*out*/iochannel_t * ioc, iochannel_t from_ioc) ;

/* function: free_iochannel
 * Closes the I/O channel (file descriptor).
 * If any other iochannel_t references the same I/O data stream (file, network connection,...)
 * the stream keeps opened. Do not call this function if you got the iochannel_t from an io_XXX function.
 * Such functions return the original <iochannel_t> and do not make a copy. */
int free_iochannel(iochannel_t * ioc) ;

// group: query

/* function: isfree_iochannel
 * Returns true if ioc equals <iochannel_FREE>. */
static inline bool isfree_iochannel(const iochannel_t ioc) ;

/* function: isvalid_iochannel
 * Returns *true* if ioc is initialized and valid.
 * A return value of true implies <isfree_iochannel> returns false.
 * The function checks that ioc refers to a valid iochannel
 * which is known to the operating system.
 * It is therefore more costly than <isfree_iochannel>. */
bool isvalid_iochannel(const iochannel_t ioc);

/* function: isclosedread_iochannel
 * Returns *true* if the input channel ioc is closed from the remote side.
 * This could happen if a network (socket) connection is closed from the remote peer
 * or if a terminal is closed and no more input could be read.
 * Any error is ignored and false is returned instead.
 *
 * In case of an ioc being closed polling ioc always results in ioc being ready for read.
 * But reading returns 0 bytes.
 *
 * Files are never considered closed for reading. */
bool isclosedread_iochannel(const iochannel_t ioc);

/* function: isclosedwrite_iochannel
 * Returns *true* if the output channel ioc is closed from the remote side.
 * This could happen if a network (socket) connection is closed from the remote peer
 * or if a terminal is closed and no more output could be written.
 * Any error is ignored and false is returned instead.
 *
 * In case of an ioc being closed by the remote peer polling ioc always results
 * in ioc being ready for write and error. Writing returns EPIPE.
 *
 * Files are never considered closed for writing. */
bool isclosedwrite_iochannel(const iochannel_t ioc);

/* function: accessmode_iochannel
 * Returns <accessmode_e> for an io channel.
 * Returns <accessmode_READ>, <accessmode_WRITE> or the combination <accessmode_RDWR>.
 * In case of an error <accessmode_NONE> is returned. */
uint8_t accessmode_iochannel(const iochannel_t ioc) ;

/* function: sizeread_iochannel
 * Returns number of bytes to read.
 * For files and block devices the total size is returned.
 * If files are larger than SIZE_MAX the value SIZE_MAX is returned instead.
 * For sockets, fifos and character devices the number of readable bytes are returned.
 *
 * No error log is written in case of an error! */
int sizeread_iochannel(const iochannel_t ioc, /*out*/size_t * size);

// group: I/O

/* function: read_iochannel
 * Reads size bytes from the data stream referenced by ioc into buffer.
 * If an error occurrs or end of input is reached less then size bytes are returned.
 * Less then size bytes could be returned also if the data stream is configured
 * to operate in non blocking mode which is recommended.
 * Check value *bytes_read* to determine how many bytes were read.
 *
 * Returns:
 * 0      - Read data and bytes_read contains the number of read bytes.
 *          If bytes_read is 0 end of input (end of file) is reached.
 * EAGAIN - Data stream operates in non blocking mode and no bytes could be read. No error log is written.
 * EBADF  - ioc is closed, has an invalid value or is not open for reading. */
int read_iochannel(iochannel_t ioc, size_t size, /*out*/void * buffer/*[size]*/, /*out*/size_t * bytes_read) ;

/* function: write_iochannel
 * Writes size bytes from buffer to the data stream referenced by ioc.
 * Returns EAGAIN in case data stream is in non blocking mode and
 * no bytes could be written (due to no more system buffer space).
 * Less then size bytes could be written if the data stream is configured
 * to operate in non blocking mode which is recommended.
 * Check value *bytes_written* to determine how many bytes were written.
 *
 * Returns:
 * 0      - The first *bytes_written data bytes from buffer are written.
 * EAGAIN - Data stream operates in non blocking mode and no bytes could be written. No error log is written.
 * EPIPE  - Receiver has closed its connection or closed it during a blocking write. No error log is written.
 * EBADF  - ioc is closed, has an invalid value or is not open for writing.
 * */
int write_iochannel(iochannel_t ioc, size_t size, const void * buffer/*[size]*/, /*out*/size_t * bytes_written) ;



// section: inline implementation

/* function: isfree_iochannel
 * Implements <iochannel_t.isfree_iochannel>. */
static inline bool isfree_iochannel(iochannel_t ioc)
{
   return iochannel_FREE == ioc ;
}


#endif
