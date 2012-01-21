/* title: Filedescriptor
   Exports system specific filedescriptor type as <filedescr_t>.

   A filedescriptor is an id that identifies an input/output channel
   like an opened file, a network connection or system specific devices.

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

   file: C-kern/api/io/filedescr.h
    Header file <Filedescriptor>.

   file: C-kern/platform/Linux/io/filedescr.c
    Linux specific implementation <Filedescriptor Linux>.
*/
#ifndef CKERN_IO_FILEDESCRIPTOR_HEADER
#define CKERN_IO_FILEDESCRIPTOR_HEADER

#include "C-kern/api/io/accessmode.h"

/* typedef: filedescr_t
 * Export <filedescr_t>, alias for <sys_filedescr_t>.
 * A filedescriptor is a system id that identifies an input/output channel
 * like an opened file, a network connection or system specific devices. */
typedef sys_filedescr_t                filedescr_t ;

/* enums: filedescr_e
 *
 * filedescr_STDIN  - The (descriptor) id of the standard input channel.
 * filedescr_STDOUT - The (descriptor) id of the standard output channel.
 * filedescr_STDERR - The (descriptor) id of the standard error (output) channel.
 * */
enum filedescr_e {
    filedescr_STDIN  = STDIN_FILENO
   ,filedescr_STDOUT = STDOUT_FILENO
   ,filedescr_STDERR = STDERR_FILENO
} ;

typedef enum filedescr_e               filedescr_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_filedescr
 * Unittest for file descriptor interface. */
extern int unittest_io_filedescr(void) ;
#endif


// section: filedescr_t

// group: lifetime

/* define: filedescr_INIT_FREEABLE
 * Static initializer. */
#define filedescr_INIT_FREEABLE        sys_filedescr_INIT_FREEABLE

/* function: free_filedescr
 * Closes the file descriptor.
 * This system call frees system resources. */
extern int free_filedescr(filedescr_t * fd) ;

// group: query

/* function: accessmode_filedescr
 * Returns access mode (read and or write) for a io channel.
 * Returns <accessmode_NONE> in case of an error. */
extern accessmode_e accessmode_filedescr(filedescr_t fd) ;

/* function: isinit_filedescr
 * Returns true if filedescriptor has not the value <filedescr_INIT_FREEABLE>.
 * Instead of comparing directly use this function. Necessary if
 * <filedescr_t> resp. <sys_filedescr_t> is defined as a complex type. */
extern bool isinit_filedescr(filedescr_t fd) ;

/* function: isopen_filedescr
 * Returns *true* if the filedescriptor is open.
 * If it is open then it <isinit_filedescr> returns also true.
 * <isopen_filedescr> checks that the value in fd refers to an open descriptor
 * and makes a call to the os. It is therefore more costly than <isinit_filedescr>.
 * It is possible that a valid filedescriptor is no more open
 * if the underlying object, e.g. <file_t> is closed. */
extern bool isopen_filedescr(filedescr_t fd) ;

/* function: nropen_filedescr
 * Returns number of opened file descriptors.
 * Use this function at the beginning and the end
 * of your test to check if a file or network socket
 * is not closed properly.
 * In case of error this functions returns 0. */
extern int nropen_filedescr(/*out*/size_t * number_open_fd) ;

// group: io

/* function: read_filedescr
 * Reads binary data from a input channel (e.g. file).
 * Returns buffer_size bytes in buffer. If an error occurrs or
 * end of input is reached less then buffer_size bytes are returned.
 * Less then buffer_size bytes could be returned also if *fd* is configured
 * to operate in non blocking mode.
 * Check value *bytes_read* to determine how many bytes were read.
 * Returns 0 and the value 0 in bytes_read if end of input (end of file) is reached.
 * Returns EAGAIN in case io is in non blocking mode and no bytes could be read. */
extern int read_filedescr(filedescr_t fd, size_t buffer_size, /*out*/uint8_t buffer[buffer_size], size_t * bytes_read) ;

/* function: write_filedescr
 * Writes binary data to a output channel (e.g. file).
 * Returns EAGAIN in case io is in non blocking mode and no bytes could be written
 * (due to no more system buffer space).
 * Returns EPIPE if the receiver has closed its connection or closes it during
 * a blocking write. */
extern int write_filedescr(filedescr_t fd, size_t buffer_size, const void * buffer/*[buffer_size]*/, size_t * bytes_written) ;


// section: inline implementation

/* define: isinit_filedescr
 * Implements <filedescr_t.isinit_filedescr>.
 * This function assumes that file is primitive type. */
#define isinit_filedescr(fd)           (0 <= (fd))


#endif