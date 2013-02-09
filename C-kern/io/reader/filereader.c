/* title: FileReader impl

   Implements <FileReader>.

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

   file: C-kern/api/io/filesystem/filereader.h
    Header file <FileReader>.

   file: C-kern/io/filesystem/filereader.c
    Implementation file <FileReader impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/reader/filereader.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/string/stringstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: filereader_t

// group: types

/* typedef: filereader_mmfile_t
 * Names type <filereader_t.mmfile>. */
typedef typeof(((filereader_t*)0)->mmfile[0])      filereader_mmfile_t ;

// group: static configuration

// TEXTDB:SELECT('#undef  filereader_'name\n'#define filereader_'name'      ('value')')FROM("C-kern/resource/config/modulevalues")WHERE(module=="filereader_t")
#undef  filereader_SYS_BUFFER_SIZE
#define filereader_SYS_BUFFER_SIZE      (4*4096)
// TEXTDB:END

// group: helper

/* function: buffersize_filereader
 * Returns the buffer size in bytes. See also <filereader_t.filereader_SYS_BUFFER_SIZE>.
 * The size is aligned to 2*pagesize_vm() => every buffer of the two is aligned to pagesize_vm().
 * TODO: 1. read configuration value at runtime ! */
static size_t buffersize_filereader(void)
{
   size_t sizeremain = filereader_SYS_BUFFER_SIZE % (2*pagesize_vm()) ;
   return filereader_SYS_BUFFER_SIZE + (sizeremain ? 2*pagesize_vm() : 0) - sizeremain ;
}

// group: lifetime

static inline void initvariables_filereader(filereader_t * frd)
{
   frd->ioerror      = 0 ;
   // unreadsize set or cleared in init_filereader or initsb_filereader
   frd->nextindex    = 0 ;
   frd->nrfreebuffer = 2 ;
   // fileoffset set or cleared in init_filereader or initsb_filereader
   // filesize set in initfile_filreader and cleared in ONABORT handler of init_filereader or initsb_filereader
   frd->file      = (file_t) file_INIT_FREEABLE ;
   frd->mmfile[0] = (filereader_mmfile_t) mmfile_INIT_FREEABLE ;
   frd->mmfile[1] = (filereader_mmfile_t) mmfile_INIT_FREEABLE ;
}

/* function: initfile_filereader
 * Opens file for reading.
 *
 * The caller must free all out variables even in case of error ! */
static int initfile_filereader(/*out*/file_t * fd, /*out*/off_t * filesize, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/)
{
   int err ;

   err = init_file(fd, filepath, accessmode_READ, relative_to) ;
   if (err) return err ;

   err = size_file(*fd, filesize) ;
   if (err) return err ;

   err = advisereadahead_file(*fd, 0, *filesize) ;
   if (err) return err ;

   return 0 ;
}

static int initsinglebuffer_filereader(/*out*/filereader_mmfile_t mmfile[2], file_t fd, size_t bufsize)
{
   int err ;

   err = initfd_mmfile(genericcast_mmfile(&mmfile[0],,), fd, 0, bufsize, accessmode_READ) ;
   if (err) return err ;

   return 0 ;
}

static int initdoublebuffer_filereader(/*out*/filereader_mmfile_t mmfile[2], file_t fd, size_t bufsize)
{
   int err ;

   err = initsinglebuffer_filereader(mmfile, fd, bufsize) ;
   if (err) return err ;

   err = initsplit_mmfile(genericcast_mmfile(&mmfile[0],,), genericcast_mmfile(&mmfile[1],,), bufsize/2, genericcast_mmfile(&mmfile[0],,)) ;
   if (err) return err ;

   return 0 ;
}

int init_filereader(/*out*/filereader_t * frd, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/)
{
   int err ;
   size_t bufsize = buffersize_filereader() ;

   initvariables_filereader(frd) ;

   err = initfile_filereader(&frd->file, &frd->filesize, filepath, relative_to) ;
   if (err) goto ONABORT ;

   if (frd->filesize <= bufsize) {
      err = initsinglebuffer_filereader(frd->mmfile, frd->file, (size_t)frd->filesize) ;
      if (err) goto ONABORT ;
      frd->unreadsize = (size_t) frd->filesize ;
      frd->fileoffset = frd->filesize ;
   } else {
      err = initdoublebuffer_filereader(frd->mmfile, frd->file, bufsize) ;
      if (err) goto ONABORT ;
      frd->unreadsize = bufsize ;
      frd->fileoffset = bufsize ;
   }

   return 0 ;
ONABORT:
   (void) free_filereader(frd) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int initsb_filereader(/*out*/filereader_t * frd, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/)
{
   int err ;

   initvariables_filereader(frd) ;

   err = initfile_filereader(&frd->file, &frd->filesize, filepath, relative_to) ;
   if (err) goto ONABORT ;

   // check that filesize can be casted into type size_t without loss of information
   if (sizeof(frd->filesize) > sizeof(size_t) && frd->filesize >= (off_t)SIZE_MAX) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   err = initsinglebuffer_filereader(frd->mmfile, frd->file, (size_t)frd->filesize) ;
   if (err) goto ONABORT ;

   frd->unreadsize = (size_t) frd->filesize ;
   frd->fileoffset = frd->filesize ;

   return 0 ;
ONABORT:
   (void) free_filereader(frd) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_filereader(filereader_t * frd)
{
   int err, err2 ;

   frd->ioerror    = 0 ;
   frd->unreadsize = 0 ;
   frd->nextindex  = 0 ;
   frd->nrfreebuffer = 0 ;
   frd->fileoffset = 0 ;
   frd->filesize   = 0 ;

   err = free_file(&frd->file) ;

   err2 = free_mmfile(genericcast_mmfile(&frd->mmfile[0],,)) ;
   if (err2) err = err2 ;

   err2 = free_mmfile(genericcast_mmfile(&frd->mmfile[1],,)) ;
   if (err2) err = err2 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

bool isfree_filereader(const filereader_t * frd)
{
   return   0 == frd->ioerror
            && 0 == frd->unreadsize
            && 0 == frd->nextindex
            && 0 == frd->nrfreebuffer
            && 0 == frd->fileoffset
            && 0 == frd->filesize
            && isfree_file(frd->file)
            && isfree_mmfile(genericcast_mmfile(&frd->mmfile[0],,const))
            && isfree_mmfile(genericcast_mmfile(&frd->mmfile[1],,const)) ;
}

// group: read

static int readnextblock_filereader(filereader_t * frd, int nextindex)
{
   int err ;

   if (frd->fileoffset == frd->filesize) {
      // ignore error
      return ENODATA ;
   }

   err = seek_mmfile(genericcast_mmfile(&frd->mmfile[nextindex],,), frd->file, frd->fileoffset, accessmode_READ) ;
   if (err) goto ONABORT ;

   off_t  unreadsize = frd->filesize - frd->fileoffset ;
   size_t buffersize = size_mmfile(genericcast_mmfile(&frd->mmfile[nextindex],,)) ;

   if (unreadsize < buffersize) {
      buffersize = (size_t) unreadsize ;
   }

   frd->unreadsize += buffersize ;
   frd->fileoffset += buffersize ;

   return 0 ;
ONABORT:
   frd->ioerror = err ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int acquirenext_filereader(filereader_t * frd, /*out*/struct stringstream_t * buffer)
{
   int err ;

   if (frd->ioerror) {
      // never logged twice
      return frd->ioerror ;
   }

   if (! frd->unreadsize) {
      // ENODATA no logged
      if (frd->fileoffset == frd->filesize) return ENODATA ;
      err = ENOBUFS ;
      goto ONABORT ;
   }

   size_t    buffersize = size_mmfile(genericcast_mmfile(&frd->mmfile[frd->nextindex],,)) ;
   uint8_t * bufferaddr = addr_mmfile(genericcast_mmfile(&frd->mmfile[frd->nextindex],,)) ;

   if (frd->unreadsize < buffersize) {
      buffersize = frd->unreadsize ;
   }

   // set out param
   *buffer = (stringstream_t) stringstream_INIT(bufferaddr, bufferaddr + buffersize) ;

   frd->unreadsize -= buffersize ;
   -- frd->nrfreebuffer ;
   frd->nextindex   = ! frd->nextindex ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int release_filereader(filereader_t * frd)
{
   if (frd->nrfreebuffer < 2) {
      // ignore error, error is logged in readnextblock_filereader
      // and signaled to user by next call to acquirenext_filereader
      (void) readnextblock_filereader(frd, frd->nrfreebuffer ? ! frd->nextindex : frd->nextindex) ;
      ++ frd->nrfreebuffer ;
   }

   return 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(directory_t * tempdir)
{
   filereader_t frd = filereader_INIT_FREEABLE ;
   const size_t B   = buffersize_filereader() ;

   // TEST iobuffer_INIT_FREEABLE
   TEST(0 == frd.ioerror) ;
   TEST(0 == frd.unreadsize) ;
   TEST(0 == frd.nextindex) ;
   TEST(0 == frd.nrfreebuffer) ;
   TEST(0 == frd.fileoffset) ;
   TEST(0 == frd.filesize) ;
   TEST(sys_file_INIT_FREEABLE == frd.file) ;
   for (unsigned i = 0; i < lengthof(frd.mmfile); ++i) {
      TEST(0 == frd.mmfile[i].addr) ;
      TEST(0 == frd.mmfile[i].size) ;
   }

   // TEST genericcast_mmfile: filereader_t.mmfile compatible with mmfile_t
   TEST((mmfile_t*)&frd.mmfile[0].addr == genericcast_mmfile(&frd.mmfile[0],,)) ;
   TEST((mmfile_t*)&frd.mmfile[1].addr == genericcast_mmfile(&frd.mmfile[1],,)) ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "big", (size_t)3e9)) ;
   TEST(0 == makefile_directory(tempdir, "single", B)) ;
   TEST(0 == makefile_directory(tempdir, "double", 2*B)) ;

   // TEST init_filereader, initsb_filereader, free_filereader: single buffer
   for (unsigned ti = 1; ti <= 2; ++ti) {
      switch (ti) {
      case 1:  TEST(0 == init_filereader(&frd, "single", tempdir)) ; break ;
      case 2:  TEST(0 == initsb_filereader(&frd, "double", tempdir)) ; break ;
      default: goto ONABORT ;
      }
      TEST(addr_mmfile(genericcast_mmfile(&frd.mmfile[0],,)) != 0) ;
      TEST(size_mmfile(genericcast_mmfile(&frd.mmfile[0],,)) == ti*B) ;
      TEST(addr_mmfile(genericcast_mmfile(&frd.mmfile[1],,)) == 0) ;
      TEST(size_mmfile(genericcast_mmfile(&frd.mmfile[1],,)) == 0) ;
      TEST(frd.ioerror    == 0) ;
      TEST(frd.unreadsize == ti*B) ;
      TEST(frd.nextindex  == 0) ;
      TEST(frd.nrfreebuffer == 2) ;
      TEST(frd.fileoffset == ti*B) ;
      TEST(frd.filesize   == ti*B) ;
      TEST(!isfree_file(frd.file)) ;
      TEST(0 == free_filereader(&frd)) ;
      TEST(1 == isfree_filereader(&frd)) ;
      TEST(0 == free_filereader(&frd)) ;
      TEST(1 == isfree_filereader(&frd)) ;
   }

   // TEST init_filereader, free_filereader
   TEST(0 == init_filereader(&frd, "double", tempdir)) ;
   TEST(addr_mmfile(genericcast_mmfile(&frd.mmfile[0],,)) != 0) ;
   TEST(size_mmfile(genericcast_mmfile(&frd.mmfile[0],,)) == B/2) ;
   TEST(addr_mmfile(genericcast_mmfile(&frd.mmfile[1],,)) == addr_mmfile(genericcast_mmfile(&frd.mmfile[0],,)) + B/2) ;
   TEST(size_mmfile(genericcast_mmfile(&frd.mmfile[1],,)) == B/2) ;
   TEST(frd.ioerror    == 0) ;
   TEST(frd.unreadsize == B) ;
   TEST(frd.nextindex  == 0) ;
   TEST(frd.nrfreebuffer == 2) ;
   TEST(frd.fileoffset == B) ;
   TEST(frd.filesize   == 2*B) ;
   TEST(!isfree_file(frd.file)) ;
   TEST(0 == free_filereader(&frd)) ;
   TEST(1 == isfree_filereader(&frd)) ;
   TEST(0 == free_filereader(&frd)) ;
   TEST(1 == isfree_filereader(&frd)) ;

   // TEST init_filereader, initsb_filereader: ERROR
   memset(&frd, 1, sizeof(frd)) ;
   TEST(0 == isfree_filereader(&frd)) ;
   TEST(ENOENT == init_filereader(&frd, "X", tempdir)) ;
   TEST(1 == isfree_filereader(&frd)) ;
   memset(&frd, 1, sizeof(frd)) ;
   TEST(ENOENT == initsb_filereader(&frd, "X", tempdir)) ;
   TEST(1 == isfree_filereader(&frd)) ;
   memset(&frd, 1, sizeof(frd)) ;
   TEST(ENOMEM == initsb_filereader(&frd, "big", tempdir)) ;
   TEST(1 == isfree_filereader(&frd)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "big")) ;
   TEST(0 == removefile_directory(tempdir, "single")) ;
   TEST(0 == removefile_directory(tempdir, "double")) ;

   return 0 ;
ONABORT:
   removefile_directory(tempdir, "big") ;
   removefile_directory(tempdir, "single") ;
   removefile_directory(tempdir, "double") ;
   return EINVAL ;
}


static int test_query(void)
{
   filereader_t frd = filereader_INIT_FREEABLE ;

   // TEST ioerror_filereader
   for (int i = 15; i >= 0; --i) {
      frd.ioerror = i ;
      TEST(i == ioerror_filereader(&frd)) ;
   }

   // TEST iseof_filereader
   frd = (filereader_t) filereader_INIT_FREEABLE ;
   TEST(1 == iseof_filereader(&frd)) ;
   frd.unreadsize = 1 ;
   TEST(0 == iseof_filereader(&frd)) ;
   frd.unreadsize = 0 ;
   TEST(1 == iseof_filereader(&frd)) ;
   frd.fileoffset = 1 ;
   TEST(0 == iseof_filereader(&frd)) ;
   frd.filesize   = 1 ;
   TEST(1 == iseof_filereader(&frd)) ;
   frd.filesize = 2 ;
   TEST(0 == iseof_filereader(&frd)) ;
   frd.fileoffset = 2 ;
   TEST(1 == iseof_filereader(&frd)) ;
   frd.unreadsize = (size_t)-1 ;
   TEST(0 == iseof_filereader(&frd)) ;

   // TEST isfree_filereader
   frd = (filereader_t) filereader_INIT_FREEABLE ;
   TEST(1 == isfree_filereader(&frd)) ;
   for (unsigned o = 0; o < sizeof(frd); ++o) {
      *(uint8_t*)&frd ^= 0xFF ;
      TEST(0 == isfree_filereader(&frd)) ;
      *(uint8_t*)&frd ^= 0xFF ;
      TEST(1 == isfree_filereader(&frd)) ;
   }

   // TEST isnext_filereader
   frd = (filereader_t) filereader_INIT_FREEABLE ;
   TEST(0 == isnext_filereader(&frd)) ;
   frd.unreadsize = 1 ;
   TEST(1 == isnext_filereader(&frd)) ;
   memset(&frd, 255, sizeof(frd)) ;
   frd.unreadsize = 0 ;
   TEST(0 == isnext_filereader(&frd)) ;
   frd.unreadsize = (size_t)-1 ;
   TEST(1 == isnext_filereader(&frd)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_read(directory_t * tempdir)
{
   filereader_t frd = filereader_INIT_FREEABLE ;
   const size_t B   = buffersize_filereader() ;
   memblock_t   mem = memblock_INIT_FREEABLE ;

   // prepare
   TEST(0 == RESIZE_MM(2*B+1, &mem)) ;
   for (size_t i = 0; i < 2*B+1; ++i) {
      addr_memblock(&mem)[i] = (uint8_t)(13*i) ;
   }
   TEST(0 == save_file("single", B, addr_memblock(&mem), tempdir)) ;
   TEST(0 == save_file("double", 2*B+1, addr_memblock(&mem), tempdir)) ;

   // TEST acquirenext_filereader, release_filereader: single buffer
   for (int ti = 1; ti <= 3; ++ti) {
      switch (ti) {
      case 1:  TEST(0 == init_filereader(&frd, "single", tempdir)) ; break ;
      case 2:  TEST(0 == initsb_filereader(&frd, "single", tempdir)) ; break ;
      case 3:  TEST(0 == initsb_filereader(&frd, "double", tempdir)) ; break ;
      default: goto ONABORT ;
      }

      stringstream_t buffer = stringstream_INIT_FREEABLE ;

      // release_filereader: changes nothing if frd.nrfreebuffer == 2
      TEST(0 == release_filereader(&frd)) ;
      TEST(0 == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == frd.mmfile[0].size) ;
      TEST(frd.nextindex    == 0) ;
      TEST(frd.nrfreebuffer == 2) ;

      // acquirenext_filereader: reads one buffer
      TEST(0 == acquirenext_filereader(&frd, &buffer)) ;
      TEST(1 == iseof_filereader(&frd)) ; // only 1 block
      TEST(frd.unreadsize   == 0) ; // all read
      TEST(frd.nextindex    == 1) ; // nextindex incremented
      TEST(frd.nrfreebuffer == 1) ; // acquired 1 buffer
      TEST(buffer.next == frd.mmfile[0].addr) ;
      TEST(buffer.end  == frd.mmfile[0].addr + frd.mmfile[0].size) ;
      TEST(size_stringstream(&buffer) == (ti == 3 ? 2*B+1 : B)) ;
      // check content
      for (size_t i = 0; isnext_stringstream(&buffer); ++i) {
         uint8_t byte = nextbyte_stringstream(&buffer) ;
         TEST(byte == (uint8_t)(13*i)) ;
      }

      // acquirenext_filereader: ENODATA
      TEST(ENODATA == acquirenext_filereader(&frd, &buffer)) ;

      // release_filereader: releases single buffer
      TEST(0 == release_filereader(&frd)) ;
      TEST(1 == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == 0) ;
      TEST(frd.nextindex    == 1) ;
      TEST(frd.nrfreebuffer == 2) ;

      // acquirenext_filereader: ENODATA
      TEST(ENODATA == acquirenext_filereader(&frd, &buffer)) ;

      TEST(0 == free_filereader(&frd)) ;
   }

   // TEST acquirenext_filereader, release_filereader: double buffer
   TEST(0 == init_filereader(&frd, "double", tempdir)) ;
   for (size_t i = 0, offset = 0 ; i < 3; ++i) {
      // release_filereader: changes nothing if frd.nrfreebuffer == 2
      TEST(0 == release_filereader(&frd)) ;
      TEST(0 == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == (i != 2 ? B : 1)) ;
      TEST(frd.nextindex    == 0) ;
      TEST(frd.nrfreebuffer == 2) ;

      stringstream_t buffer = stringstream_INIT_FREEABLE ;

      // acquirenext_filereader: reads one buffer
      TEST(0 == acquirenext_filereader(&frd, &buffer)) ;
      if (i != 2) {
         TEST(0 == iseof_filereader(&frd)) ;
         TEST(frd.unreadsize == B/2) ; // read half of buffered data
      } else {
         TEST(1 == iseof_filereader(&frd)) ; // read last byte
         TEST(frd.unreadsize == 0) ;         // read last byte
      }
      TEST(frd.nextindex    == 1) ;                   // nextindex incremented
      TEST(frd.nrfreebuffer == 1) ;                   // acquired 1 buffer
      TEST(buffer.next == frd.mmfile[0].addr) ;
      TEST(buffer.end  == frd.mmfile[0].addr + (i != 2 ? B/2 : 1)) ;
      // check content
      for (; isnext_stringstream(&buffer); ++offset) {
         uint8_t byte = nextbyte_stringstream(&buffer) ;
         TEST(byte == (uint8_t)(13*offset)) ;
      }

      if (i != 2) {
         // acquirenext_filereader: reads first buffer
         TEST(0 == acquirenext_filereader(&frd, &buffer)) ;
         TEST(0 == iseof_filereader(&frd)) ;
         TEST(frd.unreadsize   == 0) ;       // all read
         TEST(frd.nextindex    == 0) ;       // nextindex incremented
         TEST(frd.nrfreebuffer == 0) ;       // acquired 1 buffer
         TEST(buffer.next == frd.mmfile[1].addr) ;
         TEST(buffer.end  == frd.mmfile[1].addr + frd.mmfile[0].size) ;
         TEST(size_stringstream(&buffer) == B/2) ;
         // check content
         for (; isnext_stringstream(&buffer); ++offset) {
            uint8_t byte = nextbyte_stringstream(&buffer) ;
            TEST(byte == (uint8_t)(13*offset)) ;
         }
      }

      // acquirenext_filereader: reads second buffer
      buffer = (stringstream_t) stringstream_INIT_FREEABLE ;
      if (i != 2) {
         TEST(ENOBUFS == acquirenext_filereader(&frd, &buffer)) ;
         TEST(0 == iseof_filereader(&frd)) ;
      } else {
         TEST(ENODATA == acquirenext_filereader(&frd, &buffer)) ;
         TEST(1 == iseof_filereader(&frd)) ;
      }
      TEST(frd.unreadsize   == 0) ;                // nothing changed
      TEST(frd.nextindex    == (i != 2 ? 0 : 1)) ; // nothing changed
      TEST(frd.nrfreebuffer == (i != 2 ? 0 : 1)) ; // nothing changed
      TEST(buffer.next == 0) ;                     // nothing changed
      TEST(buffer.end  == 0) ;                     // nothing changed

      // release_filereader: preload first buffer
      TEST(0 == release_filereader(&frd)) ;
      TEST((i == 2) == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == (i == 2 ? 0 : i == 1 ? 1 : B/2)) ; // read data into released buffer
      TEST(frd.nextindex    == (i != 2 ? 0 : 1)) ;
      TEST(frd.nrfreebuffer == (i != 2 ? 1 : 2)) ;       // released 1 buffer

      // release_filereader: preload second buffer
      TEST(0 == release_filereader(&frd)) ;
      TEST((i == 2) == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == (i == 2 ? 0 : i == 1 ? 1 : B)) ;   // read data into released buffer
      TEST(frd.nextindex    == (i != 2 ? 0 : 1)) ;
      TEST(frd.nrfreebuffer == 2) ;       // released 2  buffers
   }
   TEST(0 == free_filereader(&frd)) ;

   // unprepare
   TEST(0 == FREE_MM(&mem)) ;
   TEST(0 == removefile_directory(tempdir, "single")) ;
   TEST(0 == removefile_directory(tempdir, "double")) ;

   return 0 ;
ONABORT:
   FREE_MM(&mem) ;
   removefile_directory(tempdir, "single") ;
   removefile_directory(tempdir, "double") ;
   return EINVAL ;
}

int unittest_io_reader_filereader()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;
   directory_t     * tempdir = 0 ;
   cstring_t         tmppath = cstring_INIT ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == newtemp_directory(&tempdir, "filereader", &tmppath)) ;

   if (test_initfree(tempdir))   goto ONABORT ;
   if (test_query())             goto ONABORT ;
   if (test_read(tempdir))       goto ONABORT ;

   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_cstring(&tmppath) ;
   (void) delete_directory(&tempdir) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
