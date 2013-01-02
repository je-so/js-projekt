/* title: Adapter-InStream-MemoryMappedFile

   Adapts <mmfile_t> to <instream_it> interface.
   Can be used with <instream_t> to build a working
   input stream.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/io/adapter/instream_mmfile.h
    Header file <Adapter-InStream-MemoryMappedFile>.

   file: C-kern/io/adapter/instream_mmfile.c
    Implementation file <Adapter-InStream-MemoryMappedFile impl>.
*/
#ifndef CKERN_IO_ADAPTER_INSTREAM_MMFILE_HEADER
#define CKERN_IO_ADAPTER_INSTREAM_MMFILE_HEADER

#include "C-kern/api/io/instream.h"
#include "C-kern/api/io/adapter/konfig_buffersize.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/mmfile.h"

/* typedef: struct instream_mmfile_t
 * Export <instream_mmfile_t> into global namespace. */
typedef struct instream_mmfile_t             instream_mmfile_t ;

/* typedef: instream_mmfile_it
 * Declare <instream_mmfile_it> as subtype of <instream_it>. */
instream_it_DECLARE(instream_mmfile_it, instream_mmfile_t) ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_adapter_instream_mmfile
 * Test <instream_mmfile_t> functionality. */
int unittest_io_adapter_instream_mmfile(void) ;
#endif


/* struct: instream_mmfile_t
 * Implements <instream_it> with help of <mmfile_t>. */
struct instream_mmfile_t {
   /* variable: buffer
    * Read buffer which is implemented by <mmfile_t>. */
   mmfile_t buffer ;
   /* variable: inputsize
    * The length of the input file. This value is read only once in <init_instreammmfile>.
    * If the length of the file is truncated afterwards reading beyond the file length results in a
    * segmentation fault ! */
   off_t    inputsize ;
   /* variable: inputoffset
    * The offset into <inputstream> which is unread.
    * The next call to <readnext_instreammmfile> returns data
    * beginning from (inputoffset - keepsize). */
   off_t    inputoffset ;
   /* variable: bufferoffset
    * The offset of the unread data in <buffer>. The offset is always smaller than the size of <buffer>. */
   size_t   bufferoffset ;
   /* variable: inputstream
    * The file from which is read. */
   file_t   inputstream ;
} ;

// group: lifetime

/* define: instream_mmfile_INIT_FREEABLE
 * Static initializer. */
#define instream_mmfile_INIT_FREEABLE        { mmfile_INIT_FREEABLE, 0, 0, 0, file_INIT_FREEABLE }

/* function: init_instreammmfile
 * Opens a file for input to be streamed.
 * The inistialized object plays the role of <instream_impl_t> and <instream_mmfile_it> plays the role of <instream_it>.
 * See <init_file> for a description of parameter filepath and relative_to. */
int init_instreammmfile(/*out*/instream_mmfile_t * obj, /*out*/const instream_mmfile_it ** iinstream, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: free_instreammmfile
 * Frees all associated resources and closes the input file. */
int free_instreammmfile(instream_mmfile_t * obj) ;

// group: query

/* function: isinit_instreammmfile
 * Returns true if <instream_mmfile_t> is not equal <instream_mmfile_INIT_FREEABLE>. */
bool isinit_instreammmfile(const instream_mmfile_t * obj) ;

// group: instream_it implementation

int readnext_instreammmfile(instream_mmfile_t * obj, /*inout*/struct memblock_t * datablock, /*out*/uint8_t ** keepaddr, size_t keepsize) ;

#endif
