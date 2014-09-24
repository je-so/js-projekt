/* title: FileReader impl

   Implements <FileReader>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/reader/filereader.h
    Header file <FileReader>.

   file: C-kern/io/reader/filereader.c
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
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/memory/wbuffer.h"
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

// group: lifetime

static inline void initvariables_filereader(filereader_t * frd)
{
   frd->ioerror      = 0 ;
   // unreadsize set or cleared in init_filereader or initsingle_filereader
   frd->nextindex    = 0 ;
   frd->nrfreebuffer = 2 ;
   // fileoffset set or cleared in init_filereader or initsingle_filereader
   // filesize set in initfile_filreader and cleared in ONERR handler of init_filereader or initsingle_filereader
   frd->file      = (file_t) file_FREE ;
   frd->mmfile[0] = (filereader_mmfile_t) mmfile_FREE ;
   frd->mmfile[1] = (filereader_mmfile_t) mmfile_FREE ;
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

   err = initPio_mmfile(genericcast_mmfile(&mmfile[0],,), fd, 0, bufsize, accessmode_READ) ;
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
   size_t bufsize = sizebuffer_filereader() ;

   initvariables_filereader(frd) ;

   err = initfile_filereader(&frd->file, &frd->filesize, filepath, relative_to) ;
   if (err) goto ONERR;

   if (frd->filesize <= bufsize) {
      err = initsinglebuffer_filereader(frd->mmfile, frd->file, (size_t)frd->filesize) ;
      if (err) goto ONERR;
      frd->unreadsize = (size_t) frd->filesize ;
      frd->fileoffset = frd->filesize ;

   } else {
      err = initdoublebuffer_filereader(frd->mmfile, frd->file, bufsize) ;
      if (err) goto ONERR;
      frd->unreadsize = bufsize ;
      frd->fileoffset = bufsize ;
   }

   return 0 ;
ONERR:
   (void) free_filereader(frd) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int initsingle_filereader(/*out*/filereader_t * frd, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/)
{
   int err ;

   initvariables_filereader(frd) ;

   err = initfile_filereader(&frd->file, &frd->filesize, filepath, relative_to) ;
   if (err) goto ONERR;

   // check that filesize can be casted into type size_t without loss of information
   if (sizeof(frd->filesize) > sizeof(size_t) && frd->filesize >= (off_t)SIZE_MAX) {
      err = ENOMEM ;
      goto ONERR;
   }

   err = initsinglebuffer_filereader(frd->mmfile, frd->file, (size_t)frd->filesize) ;
   if (err) goto ONERR;

   frd->unreadsize = (size_t) frd->filesize ;
   frd->fileoffset = frd->filesize ;

   return 0 ;
ONERR:
   (void) free_filereader(frd) ;
   TRACEEXIT_ERRLOG(err);
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

   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

// group: query

/* function: sizebuffer_filereader
 * TODO: 1. read configuration value at runtime !
 * */
size_t sizebuffer_filereader(void)
{
   static_assert( 0 == ((filereader_SYS_BUFFER_SIZE-1) & filereader_SYS_BUFFER_SIZE), "is power of 2") ;
   size_t minsize = 2*pagesize_vm() ;  // pagesize_vm() is power of 2
   size_t    size = filereader_SYS_BUFFER_SIZE >= minsize ? filereader_SYS_BUFFER_SIZE : minsize ;
   return size ;
}

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
   if (err) goto ONERR;

   off_t  unreadsize = frd->filesize - frd->fileoffset ;
   size_t buffersize = frd->mmfile[nextindex].size ;

   if (unreadsize < buffersize) {
      buffersize = (size_t) unreadsize ;
   }

   frd->unreadsize += buffersize ;
   frd->fileoffset += buffersize ;

   return 0 ;
ONERR:
   frd->ioerror = err ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int readnext_filereader(filereader_t * frd, /*out*/struct stringstream_t * buffer)
{
   int err ;

   if (frd->ioerror) {
      // never logged twice
      return frd->ioerror ;
   }

   if (! frd->unreadsize) {
      // ENODATA is not logged
      if (frd->fileoffset == frd->filesize) return ENODATA ;
      err = ENOBUFS ;
      goto ONERR;
   }

   size_t    buffersize = frd->mmfile[frd->nextindex].size ;
   uint8_t * bufferaddr = frd->mmfile[frd->nextindex].addr ;

   if (frd->unreadsize < buffersize) {
      buffersize = frd->unreadsize ;
   }

   // set out param
   *buffer = (stringstream_t) stringstream_INIT(bufferaddr, bufferaddr + buffersize) ;

   frd->unreadsize -= buffersize ;
   -- frd->nrfreebuffer ;
   frd->nextindex   = ! frd->nextindex ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

void release_filereader(filereader_t * frd)
{
   if (frd->nrfreebuffer < 2) {
      // ignore error, error is logged in readnextblock_filereader
      // and signaled to user by next call to readnext_filereader
      (void) readnextblock_filereader(frd, frd->nrfreebuffer ? ! frd->nextindex : frd->nextindex) ;
      ++ frd->nrfreebuffer ;
   }
}

void unread_filereader(filereader_t * frd)
{
   if (frd->nrfreebuffer < 2) {
      frd->nextindex = ! frd->nextindex ;
      ++ frd->nrfreebuffer ;
      if (  frd->fileoffset == frd->filesize
            && 0 == frd->unreadsize) {
         // last buffer unread
         if (frd->mmfile[frd->nextindex].size >= frd->filesize) {
            frd->unreadsize +=  (size_t)frd->filesize ;
         } else {
            frd->unreadsize += ((size_t)frd->filesize & (frd->mmfile[frd->nextindex].size-1)) ;
         }
      } else {
         frd->unreadsize += frd->mmfile[frd->nextindex].size ;
      }
   }
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(directory_t * tempdir)
{
   filereader_t frd = filereader_FREE ;
   const size_t B   = sizebuffer_filereader() ;

   // TEST filereader_FREE
   TEST(0 == frd.ioerror) ;
   TEST(0 == frd.unreadsize) ;
   TEST(0 == frd.nextindex) ;
   TEST(0 == frd.nrfreebuffer) ;
   TEST(0 == frd.fileoffset) ;
   TEST(0 == frd.filesize) ;
   TEST(sys_iochannel_FREE == frd.file) ;
   for (unsigned i = 0; i < lengthof(frd.mmfile); ++i) {
      mmfile_t mfile = mmfile_FREE ;
      TEST(0 == frd.mmfile[i].addr) ;  // same as mmfile_FREE
      TEST(0 == frd.mmfile[i].size) ;  // same as mmfile_FREE
      TEST(0 == mfile.addr) ;
      TEST(0 == mfile.size) ;
   }

   // TEST genericcast_mmfile: filereader_t.mmfile compatible with mmfile_t
   TEST((mmfile_t*)&frd.mmfile[0].addr == genericcast_mmfile(&frd.mmfile[0],,)) ;
   TEST((mmfile_t*)&frd.mmfile[1].addr == genericcast_mmfile(&frd.mmfile[1],,)) ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "big", (size_t)3e9)) ;
   TEST(0 == makefile_directory(tempdir, "single", B)) ;
   TEST(0 == makefile_directory(tempdir, "double", 2*B)) ;

   // TEST init_filereader, initsingle_filereader, free_filereader: single buffer
   for (unsigned ti = 1; ti <= 2; ++ti) {
      switch (ti) {
      case 1:  TEST(0 == init_filereader(&frd, "single", tempdir)) ; break ;
      case 2:  TEST(0 == initsingle_filereader(&frd, "double", tempdir)) ; break ;
      default: goto ONERR;
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

   // TEST init_filereader, initsingle_filereader: ERROR
   memset(&frd, 1, sizeof(frd)) ;
   TEST(0 == isfree_filereader(&frd)) ;
   TEST(ENOENT == init_filereader(&frd, "X", tempdir)) ;
   TEST(1 == isfree_filereader(&frd)) ;
   memset(&frd, 1, sizeof(frd)) ;
   TEST(ENOENT == initsingle_filereader(&frd, "X", tempdir)) ;
   TEST(1 == isfree_filereader(&frd)) ;
   memset(&frd, 1, sizeof(frd)) ;
   TEST(ENOMEM == initsingle_filereader(&frd, "big", tempdir)) ;
   TEST(1 == isfree_filereader(&frd)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "big")) ;
   TEST(0 == removefile_directory(tempdir, "single")) ;
   TEST(0 == removefile_directory(tempdir, "double")) ;

   return 0 ;
ONERR:
   removefile_directory(tempdir, "big") ;
   removefile_directory(tempdir, "single") ;
   removefile_directory(tempdir, "double") ;
   return EINVAL ;
}

static int test_query(void)
{
   filereader_t frd = filereader_FREE ;

   // TEST sizebuffer_filereader
   TEST(sizebuffer_filereader() >= (2*pagesize_vm())) ;
   TEST(sizebuffer_filereader() >= filereader_SYS_BUFFER_SIZE) ;
   // power of 2
   TEST(0 == (sizebuffer_filereader() & (sizebuffer_filereader()-1))) ;

   // TEST ioerror_filereader
   for (int i = 15; i >= 0; --i) {
      frd.ioerror = i ;
      TEST(i == ioerror_filereader(&frd)) ;
   }

   // TEST iseof_filereader
   frd = (filereader_t) filereader_FREE ;
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
   frd = (filereader_t) filereader_FREE ;
   TEST(1 == isfree_filereader(&frd)) ;
   for (unsigned o = 0; o < sizeof(frd); ++o) {
      *(uint8_t*)&frd ^= 0xFF ;
      TEST(0 == isfree_filereader(&frd)) ;
      *(uint8_t*)&frd ^= 0xFF ;
      TEST(1 == isfree_filereader(&frd)) ;
   }

   // TEST isnext_filereader
   frd = (filereader_t) filereader_FREE ;
   TEST(0 == isnext_filereader(&frd)) ;
   frd.unreadsize = 1 ;
   TEST(1 == isnext_filereader(&frd)) ;
   memset(&frd, 255, sizeof(frd)) ;
   frd.unreadsize = 0 ;
   TEST(0 == isnext_filereader(&frd)) ;
   frd.unreadsize = (size_t)-1 ;
   TEST(1 == isnext_filereader(&frd)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_setter(void)
{
   filereader_t frd = filereader_FREE ;

   // TEST setioerror_filereader
   for (int i = 15; i >= 0; --i) {
      setioerror_filereader(&frd, i) ;
      TEST(i == frd.ioerror) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_read(directory_t * tempdir)
{
   filereader_t   frd = filereader_FREE ;
   const size_t   B   = sizebuffer_filereader() ;
   memblock_t     mem = memblock_FREE ;
   stringstream_t buffer = stringstream_FREE ;

   // prepare
   TEST(0 == RESIZE_MM(2*B+1, &mem)) ;
   for (size_t i = 0; i < 2*B+1; ++i) {
      addr_memblock(&mem)[i] = (uint8_t)(13*i) ;
   }
   TEST(0 == save_file("single", B, addr_memblock(&mem), tempdir)) ;
   TEST(0 == save_file("double", 2*B+1, addr_memblock(&mem), tempdir)) ;

   // TEST readnext_filereader, release_filereader: single buffer
   for (int ti = 1; ti <= 3; ++ti) {
      switch (ti) {
      case 1:  TEST(0 == init_filereader(&frd, "single", tempdir)) ; break ;
      case 2:  TEST(0 == initsingle_filereader(&frd, "single", tempdir)) ; break ;
      case 3:  TEST(0 == initsingle_filereader(&frd, "double", tempdir)) ; break ;
      default: goto ONERR;
      }

      // release_filereader: changes nothing if frd.nrfreebuffer == 2
      release_filereader(&frd) ;
      TEST(0 == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == frd.mmfile[0].size) ;
      TEST(frd.nextindex    == 0) ;
      TEST(frd.nrfreebuffer == 2) ;

      // readnext_filereader: reads one buffer
      TEST(0 == readnext_filereader(&frd, &buffer)) ;
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

      // readnext_filereader: ENODATA
      TEST(ENODATA == readnext_filereader(&frd, &buffer)) ;

      // release_filereader: releases single buffer
      release_filereader(&frd) ;
      TEST(1 == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == 0) ;
      TEST(frd.nextindex    == 1) ;
      TEST(frd.nrfreebuffer == 2) ;

      // readnext_filereader: ENODATA
      TEST(ENODATA == readnext_filereader(&frd, &buffer)) ;

      TEST(0 == free_filereader(&frd)) ;
   }

   // TEST readnext_filereader, release_filereader: double buffer
   TEST(0 == init_filereader(&frd, "double", tempdir)) ;
   for (size_t i = 0, offset = 0 ; i < 3; ++i) {
      // release_filereader: changes nothing if frd.nrfreebuffer == 2
      release_filereader(&frd) ;
      TEST(0 == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == (i != 2 ? B : 1)) ;
      TEST(frd.nextindex    == 0) ;
      TEST(frd.nrfreebuffer == 2) ;

      // readnext_filereader: reads one buffer
      TEST(0 == readnext_filereader(&frd, &buffer)) ;
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
         // readnext_filereader: reads first buffer
         TEST(0 == readnext_filereader(&frd, &buffer)) ;
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

      // readnext_filereader: reads second buffer
      buffer = (stringstream_t) stringstream_FREE ;
      if (i != 2) {
         TEST(ENOBUFS == readnext_filereader(&frd, &buffer)) ;
         TEST(0 == iseof_filereader(&frd)) ;
      } else {
         TEST(ENODATA == readnext_filereader(&frd, &buffer)) ;
         TEST(1 == iseof_filereader(&frd)) ;
      }
      TEST(frd.unreadsize   == 0) ;                // nothing changed
      TEST(frd.nextindex    == (i != 2 ? 0 : 1)) ; // nothing changed
      TEST(frd.nrfreebuffer == (i != 2 ? 0 : 1)) ; // nothing changed
      TEST(buffer.next == 0) ;                     // nothing changed
      TEST(buffer.end  == 0) ;                     // nothing changed

      // release_filereader: preload first buffer
      release_filereader(&frd) ;
      TEST((i == 2) == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == (i == 2 ? 0 : i == 1 ? 1 : B/2)) ; // read data into released buffer
      TEST(frd.nextindex    == (i != 2 ? 0 : 1)) ;
      TEST(frd.nrfreebuffer == (i != 2 ? 1 : 2)) ;       // released 1 buffer

      // release_filereader: preload second buffer
      release_filereader(&frd) ;
      TEST((i == 2) == iseof_filereader(&frd)) ;
      TEST(frd.unreadsize   == (i == 2 ? 0 : i == 1 ? 1 : B)) ;   // read data into released buffer
      TEST(frd.nextindex    == (i != 2 ? 0 : 1)) ;
      TEST(frd.nrfreebuffer == 2) ;       // released 2  buffers
   }
   TEST(0 == free_filereader(&frd)) ;

   // TEST readnext_filereader: ioerror is returned
   TEST(0 == init_filereader(&frd, "double", tempdir)) ;
   TEST(0 == readnext_filereader(&frd, &buffer)) ;
   for (int i = 1; i < 15; ++i) {
      setioerror_filereader(&frd, i) ;
      TEST(i == readnext_filereader(&frd, &buffer)) ;
   }
   TEST(0 == free_filereader(&frd)) ;

   // TEST unread_filereader: single buffer case
   for (int ti = 1; ti <= 2; ++ti) {
      switch (ti) {
      case 1:  TEST(0 == initsingle_filereader(&frd, "single", tempdir)) ; break ;
      case 2:  TEST(0 == initsingle_filereader(&frd, "double", tempdir)) ; break ;
      default: goto ONERR;
      }
      // call ignored if frd.nrfreebuffer == 2
      unread_filereader(&frd) ;
      TEST(frd.filesize == frd.unreadsize) ;
      TEST(0 == frd.nextindex) ;
      TEST(2 == frd.nrfreebuffer) ;
      TEST(0 == readnext_filereader(&frd, &buffer)) ;
      stringstream_t buffer2 = buffer ;
      TEST(0 == frd.unreadsize) ;
      TEST(1 == frd.nextindex) ;
      TEST(1 == frd.nrfreebuffer) ;
      // unread buffer
      unread_filereader(&frd) ;
      TEST(frd.filesize == frd.unreadsize) ;
      TEST(0 == frd.nextindex) ;
      TEST(2 == frd.nrfreebuffer) ;
      // next call to readnext_filereader returns same buffer
      TEST(0 == readnext_filereader(&frd, &buffer)) ;
      TEST(0 == frd.unreadsize) ;
      TEST(1 == frd.nextindex) ;
      TEST(1 == frd.nrfreebuffer) ;
      TEST(buffer.next == buffer2.next) ;
      TEST(buffer.end  == buffer2.end) ;
      TEST(0 == free_filereader(&frd)) ;
   }

   // TEST unread_filereader: double buffer case
   TEST(0 == init_filereader(&frd, "double", tempdir)) ;
   for (size_t i = 0; i < 5; ++i) {
      size_t I = i % 2 ;
      size_t U = (i < 3 ? B : i == 3 ? B/2 + 1 : 1) ;
      size_t U1 = (U > B/2 ? U - B/2 : 0) ;

      // unread_filereader: changes nothing if frd.nrfreebuffer == 2
      TEST(0 == iseof_filereader(&frd)) ;
      TEST(U == frd.unreadsize) ;
      TEST(I == frd.nextindex) ;
      TEST(2 == frd.nrfreebuffer) ;
      unread_filereader(&frd) ;
      TEST(0 == iseof_filereader(&frd)) ;
      TEST(U == frd.unreadsize) ;
      TEST(I == frd.nextindex) ;
      TEST(2 == frd.nrfreebuffer) ;

      // readnext_filereader: reads one buffer
      TEST(0 == readnext_filereader(&frd, &buffer)) ;
      TEST(U1 == frd.unreadsize) ;
      TEST(!I == frd.nextindex) ;
      TEST(1 == frd.nrfreebuffer) ;

      if (U1) {
         // readnext_filereader: reads second buffer
         stringstream_t buffer2 = stringstream_FREE ;
         TEST(0 == readnext_filereader(&frd, &buffer2)) ;
         TEST(I == frd.nextindex) ;
         TEST(0 == frd.nrfreebuffer) ;
         // unread_filereader: unreads second buffer
         unread_filereader(&frd) ;
         TEST(U1 == frd.unreadsize) ;
         TEST(!I == frd.nextindex) ;
         TEST(1 == frd.nrfreebuffer) ;
         // readnext_filereader returns the same second buffer
         stringstream_t buffer3 = stringstream_FREE ;
         TEST(0 == readnext_filereader(&frd, &buffer3)) ;
         TEST(I == frd.nextindex) ;
         TEST(0 == frd.nrfreebuffer) ;
         TEST(buffer3.next == buffer2.next) ;
         TEST(buffer3.end  == buffer2.end) ;
         // unread_filereader: unreads second buffer
         unread_filereader(&frd) ;
         TEST(U1 == frd.unreadsize) ;
         TEST(!I == frd.nextindex) ;
         TEST(1 == frd.nrfreebuffer) ;
      }

      // unread_filereader: unreads last read buffer
      unread_filereader(&frd) ;
      TEST(0 == iseof_filereader(&frd)) ;
      TEST(U == frd.unreadsize) ;
      TEST(I == frd.nextindex) ;
      TEST(2 == frd.nrfreebuffer) ;

      // readnext_filereader: reads same buffer
      stringstream_t buffer2 = stringstream_FREE ;
      TEST(0 == readnext_filereader(&frd, &buffer2)) ;
      TEST(U1 == frd.unreadsize) ;
      TEST(!I == frd.nextindex) ;
      TEST(1 == frd.nrfreebuffer) ;
      TEST(buffer.next == buffer2.next) ;
      TEST(buffer.end  == buffer2.end) ;

      release_filereader(&frd) ;
   }
   TEST(1 == iseof_filereader(&frd)) ;
   TEST(0 == frd.unreadsize) ;
   TEST(1 == frd.nextindex) ;
   TEST(2 == frd.nrfreebuffer) ;
   TEST(0 == free_filereader(&frd)) ;

   // unprepare
   TEST(0 == FREE_MM(&mem)) ;
   TEST(0 == removefile_directory(tempdir, "single")) ;
   TEST(0 == removefile_directory(tempdir, "double")) ;

   return 0 ;
ONERR:
   FREE_MM(&mem) ;
   removefile_directory(tempdir, "single") ;
   removefile_directory(tempdir, "double") ;
   return EINVAL ;
}

int unittest_io_reader_filereader()
{
   directory_t *tempdir = 0 ;
   cstring_t   tmppath  = cstring_INIT ;

   // prepare
   TEST(0 == newtemp_directory(&tempdir, "filereader")) ;
   TEST(0 == path_directory(tempdir, &(wbuffer_t)wbuffer_INIT_CSTRING(&tmppath))) ;

   if (test_initfree(tempdir))   goto ONERR;
   if (test_query())             goto ONERR;
   if (test_setter())            goto ONERR;
   if (test_read(tempdir))       goto ONERR;

   /* adapt log */
   uint8_t *logbuffer;
   size_t   logsize;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   while (strstr((char*)logbuffer, "/filereader.")) {
      logbuffer = (uint8_t*)strstr((char*)logbuffer, "/filereader.")+12;
      if (logbuffer[6] == '/') {
         memcpy(logbuffer, "XXXXXX", 6);
      }
   }

   // unprepare
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

   return 0 ;
ONERR:
   (void) free_cstring(&tmppath) ;
   (void) delete_directory(&tempdir) ;
   return EINVAL ;
}

#endif
