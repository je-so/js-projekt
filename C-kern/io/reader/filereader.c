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
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/memory/memstream.h"
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: filereader_t

// group: static configuration

// TEXTDB:SELECT('#undef  filereader_'name\n'#define filereader_'name'      ('value')')FROM(C-kern/resource/config/modulevalues)WHERE(module=="filereader_t")
#undef  filereader_SYS_BUFFER_SIZE
#define filereader_SYS_BUFFER_SIZE      (4*4096)
// TEXTDB:END

// group: lifetime

static inline void initvariables_filereader(filereader_t * frd)
{
   frd->ioerror      = 0;
   frd->unreadsize   = 0;
   frd->nextindex    = 0;
   frd->nrfreebuffer = 2;
   frd->fileoffset   = 0;
   // filesize set in initfile_filreader and cleared in ONERR handler of init_filereader
   frd->file    = (file_t) file_FREE;
   frd->page[0] = (filereader_page_t) filereader_page_FREE;
   frd->page[1] = (filereader_page_t) filereader_page_FREE;
}

/* function: initfile_filereader
 * Opens file for reading.
 *
 * The caller must free all out variables even in case of error ! */
static int initfile_filereader(/*out*/file_t * fd, /*out*/off_t * filesize, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/)
{
   int err;

   err = init_file(fd, filepath, accessmode_READ, relative_to);
   if (err) return err;

   err = size_file(*fd, filesize);
   if (err) return err;

   err = advisereadahead_file(*fd, 0, *filesize);
   if (err) return err;

   return 0;
}

static int initsinglebuffer_filereader(/*out*/filereader_page_t * page, size_t bufsize)
{
   int err;

   err = init_vmpage(cast_vmpage(page,), bufsize);

   return err;
}

static int initdoublebuffer_filereader(/*out*/filereader_page_t page[2], size_t bufsize)
{
   int err;

   err = initsinglebuffer_filereader(&page[0], bufsize);
   if (err) return err;

   page[0].size = bufsize/2;
   page[1].addr = page[0].addr + bufsize/2;
   page[1].size = bufsize/2;

   return 0;
}

int init_filereader(/*out*/filereader_t * frd, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/)
{
   int err;
   size_t bufsize = sizebuffer_filereader();

   initvariables_filereader(frd);

   err = initfile_filereader(&frd->file, &frd->filesize, filepath, relative_to);
   if (err) goto ONERR;

   if (frd->filesize <= bufsize) {
      err = initsinglebuffer_filereader(frd->page, (size_t)frd->filesize);
      if (err) goto ONERR;

   } else {
      err = initdoublebuffer_filereader(frd->page, bufsize);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   (void) free_filereader(frd);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_filereader(filereader_t * frd)
{
   int err;
   int err2;

   frd->ioerror    = 0;
   frd->unreadsize = 0;
   frd->nextindex  = 0;
   frd->nrfreebuffer = 0;
   frd->fileoffset = 0;
   frd->filesize   = 0;

   err = free_file(&frd->file);

   err2 = free_vmpage(cast_vmpage(&frd->page[0],));
   if (err2) err = err2;

   err2 = free_vmpage(cast_vmpage(&frd->page[1],));
   if (err2) err = err2;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

/* function: sizebuffer_filereader
 * TODO: 1. read configuration value at runtime !
 * */
size_t sizebuffer_filereader(void)
{
   static_assert( 0 == ((filereader_SYS_BUFFER_SIZE-1) & filereader_SYS_BUFFER_SIZE), "is power of 2");
   size_t minsize = 2*pagesize_vm();  // pagesize_vm() is power of 2
   size_t    size = filereader_SYS_BUFFER_SIZE >= minsize ? filereader_SYS_BUFFER_SIZE : minsize;
   return size;
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
            && frd->page[0].addr == 0
            && frd->page[0].size == 0
            && frd->page[1].addr == 0
            && frd->page[1].size == 0;
}

// group: read

int readnext_filereader(filereader_t * frd, /*out*/struct memstream_ro_t * buffer)
{
   int err;

   if (frd->ioerror) {
      // never logged twice
      return frd->ioerror;
   }

   off_t unreadsize = frd->filesize - frd->fileoffset;

   if (0 == unreadsize) {
      // ENODATA is not logged
      return ENODATA;
   }

   if (! frd->nrfreebuffer) {
      err = ENOBUFS;
      goto ONERR;
   }

   uint8_t * bufferaddr = frd->page[frd->nextindex].addr;
   size_t    buffersize = frd->page[frd->nextindex].size;
   if (unreadsize < buffersize) {
      buffersize = (size_t) unreadsize;
   }

   if (! frd->unreadsize) {
      err = readall_iochannel(frd->file, buffersize, bufferaddr, -1);
      if (err) {
         frd->ioerror = err;
         goto ONERR;
      }
      frd->unreadsize = buffersize;
   }

   frd->unreadsize -= buffersize;
   frd->nextindex = ! frd->nextindex;
   -- frd->nrfreebuffer;
   frd->fileoffset += buffersize;

   // set out param
   init_memstream(buffer, bufferaddr, bufferaddr + buffersize);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

void release_filereader(filereader_t * frd)
{
   if (frd->nrfreebuffer < 2) {
      ++ frd->nrfreebuffer;
   }
}

void unread_filereader(filereader_t * frd)
{
   if (frd->nrfreebuffer < 2) {
      size_t buffer_size;
      frd->nextindex = ! frd->nextindex;
      ++ frd->nrfreebuffer;
      if (frd->fileoffset == frd->filesize) {
         // last buffer unread
         if (frd->page[frd->nextindex].size >= frd->filesize) {
            buffer_size = (size_t)frd->filesize;
         } else {
            buffer_size = ((size_t)frd->filesize & (frd->page[frd->nextindex].size-1));
         }
      } else {
         buffer_size = frd->page[frd->nextindex].size;
      }
      frd->unreadsize += buffer_size;
      frd->fileoffset -= buffer_size;
   }
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(directory_t * tempdir)
{
   filereader_t frd = filereader_FREE;
   const size_t B   = sizebuffer_filereader();

   // TEST filereader_FREE
   TEST(0 == frd.ioerror);
   TEST(0 == frd.unreadsize);
   TEST(0 == frd.nextindex);
   TEST(0 == frd.nrfreebuffer);
   TEST(0 == frd.fileoffset);
   TEST(0 == frd.filesize);
   TEST(sys_iochannel_FREE == frd.file);
   for (unsigned i = 0; i < lengthof(frd.page); ++i) {
      TEST(0 == frd.page[i].addr);
      TEST(0 == frd.page[i].size);
   }

   // TEST cast_vmpage: filereader_t.page compatible with vmpage_t
   TEST((vmpage_t*)&frd.page[0].addr == cast_vmpage(&frd.page[0],));

   // prepare
   TEST(0 == makefile_directory(tempdir, "single", B));
   TEST(0 == makefile_directory(tempdir, "double", 2*B));

   // TEST init_filereader: file size <= sizebuffer_filereader
   TEST(0 == init_filereader(&frd, "single", tempdir));
   TEST(frd.page[0].addr != 0);
   TEST(frd.page[0].size == B);
   TEST(frd.page[1].addr == 0);
   TEST(frd.page[1].size == 0);
   TEST(frd.ioerror    == 0);
   TEST(frd.unreadsize == 0);
   TEST(frd.nextindex  == 0);
   TEST(frd.nrfreebuffer == 2);
   TEST(frd.fileoffset == 0);
   TEST(frd.filesize   == B);
   TEST(!isfree_file(frd.file));

   // TEST free_filereader: double free
   TEST(0 == free_filereader(&frd));
   TEST(1 == isfree_filereader(&frd));
   TEST(0 == free_filereader(&frd));
   TEST(1 == isfree_filereader(&frd));

   // TEST init_filereader: file size > sizebuffer_filereader
   TEST(0 == init_filereader(&frd, "double", tempdir));
   TEST(frd.page[0].addr != 0);
   TEST(frd.page[0].size == B/2);
   TEST(frd.page[1].addr == frd.page[0].addr + B/2);
   TEST(frd.page[1].size == B/2);
   TEST(frd.ioerror    == 0);
   TEST(frd.unreadsize == 0);
   TEST(frd.nextindex  == 0);
   TEST(frd.nrfreebuffer == 2);
   TEST(frd.fileoffset == 0);
   TEST(frd.filesize   == 2*B);
   TEST(!isfree_file(frd.file));

   // TEST free_filereader
   TEST(0 == free_filereader(&frd));
   TEST(1 == isfree_filereader(&frd));
   TEST(0 == free_filereader(&frd));
   TEST(1 == isfree_filereader(&frd));

   // TEST init_filereader: ERROR
   memset(&frd, 1, sizeof(frd));
   TEST(0 == isfree_filereader(&frd));
   TEST(ENOENT == init_filereader(&frd, "X", tempdir));
   TEST(1 == isfree_filereader(&frd));

   // unprepare
   TEST(0 == removefile_directory(tempdir, "single"));
   TEST(0 == removefile_directory(tempdir, "double"));

   return 0;
ONERR:
   removefile_directory(tempdir, "single");
   removefile_directory(tempdir, "double");
   return EINVAL;
}

static int test_query(void)
{
   filereader_t frd = filereader_FREE;

   // TEST sizebuffer_filereader
   TEST(sizebuffer_filereader() >= (2*pagesize_vm()));
   TEST(sizebuffer_filereader() >= filereader_SYS_BUFFER_SIZE);
   // power of 2
   TEST(0 == (sizebuffer_filereader() & (sizebuffer_filereader()-1)));

   // TEST ioerror_filereader
   for (int i = 15; i >= 0; --i) {
      frd.ioerror = i;
      TEST(i == ioerror_filereader(&frd));
   }

   // TEST iseof_filereader
   frd = (filereader_t) filereader_FREE;
   for (size_t s = 0; s < 2; ++s) {
      frd.fileoffset = 0;
      frd.filesize   = 0;
      frd.unreadsize = s;
      TEST(1 == iseof_filereader(&frd));
      frd.fileoffset = s + 1;
      TEST(0 == iseof_filereader(&frd));
      frd.filesize   = s + 1;
      TEST(1 == iseof_filereader(&frd));
      frd.filesize = s + 2;
      TEST(0 == iseof_filereader(&frd));
      frd.fileoffset = s + 2;
      TEST(1 == iseof_filereader(&frd));
   }

   // TEST isfree_filereader
   frd = (filereader_t) filereader_FREE;
   TEST(1 == isfree_filereader(&frd));
   for (unsigned o = 0; o < sizeof(frd); ++o) {
      *(uint8_t*)&frd ^= 0xFF;
      TEST(0 == isfree_filereader(&frd));
      *(uint8_t*)&frd ^= 0xFF;
      TEST(1 == isfree_filereader(&frd));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_setter(void)
{
   filereader_t frd = filereader_FREE;

   // TEST setioerror_filereader
   for (int i = 15; i >= 0; --i) {
      setioerror_filereader(&frd, i);
      TEST(i == frd.ioerror);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_read(directory_t * tempdir)
{
   filereader_t   frd = filereader_FREE;
   const size_t   B   = sizebuffer_filereader();
   memblock_t     mem = memblock_FREE;
   memstream_ro_t buffer = memstream_FREE;

   // prepare
   TEST(0 == RESIZE_MM(2*B+1, &mem));
   for (size_t i = 0; i < 2*B+1; ++i) {
      addr_memblock(&mem)[i] = (uint8_t)(13*i);
   }
   TEST(0 == save_file("single", B, addr_memblock(&mem), tempdir));
   TEST(0 == save_file("double", 2*B+1, addr_memblock(&mem), tempdir));

   // prepare: single buffer
   TEST(0 == init_filereader(&frd, "single", tempdir));

   // TEST release_filereader: changes nothing if frd.nrfreebuffer == 2
   release_filereader(&frd);
   TEST(0 == frd.unreadsize);
   TEST(0 == frd.nextindex);
   TEST(2 == frd.nrfreebuffer);
   TEST(0 == frd.fileoffset);

   for (int ti = 0; ti < 2; ++ti) {

      // TEST unread_filereader
      unread_filereader(&frd);
      if (0 == ti) {
         // call ignored if frd.nrfreebuffer == 2
         TEST(0 == frd.unreadsize);
      } else {
         // next call to readnext_filereader returns same buffer
         TEST(B == frd.unreadsize);
      }
      TEST(0 == frd.nextindex);
      TEST(2 == frd.nrfreebuffer);
      TEST(0 == frd.fileoffset);

      // TEST readnext_filereader: reads one buffer
      TEST(0 == readnext_filereader(&frd, &buffer));
      // check buffer
      TEST(B == size_memstream(&buffer));
      TEST(buffer.next == frd.page[0].addr);
      TEST(buffer.end  == frd.page[0].addr + frd.page[0].size);
      // check frd
      TEST(0 == frd.unreadsize); // all read
      TEST(1 == frd.nextindex); // nextindex incremented
      TEST(1 == frd.nrfreebuffer); // acquired 1 buffer
      TEST(B == frd.fileoffset);
      // check content
      for (size_t i = 0; isnext_memstream(&buffer); ++i) {
         uint8_t byte = nextbyte_memstream(&buffer);
         TEST(byte == (uint8_t)(13*i));
      }

   }

   // TEST readnext_filereader: ENODATA
   TEST(ENODATA == readnext_filereader(&frd, &buffer));

   // TEST release_filereader: releases single buffer
   release_filereader(&frd);
   TEST(0 == frd.unreadsize);
   TEST(1 == frd.nextindex);
   TEST(2 == frd.nrfreebuffer);
   TEST(B == frd.fileoffset);

   // TEST readnext_filereader: ENODATA
   TEST(ENODATA == readnext_filereader(&frd, &buffer));

   // unprepare
   TEST(0 == free_filereader(&frd));

   // prepare: double buffer
   TEST(0 == init_filereader(&frd, "double", tempdir));

   for (size_t i = 0, offset = 0; i < 3; ++i) {

      // TODO: add unread functionality !!!

      // TEST release_filereader: frd.nrfreebuffer == 2 ==> does nothing
      release_filereader(&frd);
      TEST( 0 == frd.unreadsize);
      TEST( 0 == frd.nextindex);
      TEST( 2 == frd.nrfreebuffer);
      TEST( offset == frd.fileoffset);

      // TEST unread_filereader: frd.nrfreebuffer == 2 ==> does nothing
      unread_filereader(&frd);
      TEST( 0 == frd.unreadsize);
      TEST( 0 == frd.nextindex);
      TEST( 2 == frd.nrfreebuffer);
      TEST( offset == frd.fileoffset);

      const size_t S = (i != 2 ? B/2 : 1);

      for (int U = 1; U >= 0; --U) {   // test unread

         for (unsigned n = 0, u = (U == 1 || S == 1)?0:S; n <= 1; ++n, u -= u?S:0) {
            // TEST readnext_filereader: reads one buffer
            TEST( 0 == readnext_filereader(&frd, &buffer));
            // check buffer
            TEST( buffer.next == frd.page[n].addr);
            TEST( buffer.end  == frd.page[n].addr + S);
            // check frd
            TEST( u == frd.unreadsize);
            TEST( !n == frd.nextindex);
            TEST( !n == frd.nrfreebuffer);
            TEST( offset + S == frd.fileoffset);
            // check content
            for (; isnext_memstream(&buffer); ++offset) {
               uint8_t byte = nextbyte_memstream(&buffer);
               TEST(byte == (uint8_t)(13*offset));
            }
            if (S == 1) break;
         }

         // TEST readnext_filereader: more than 2 buffers or ENODATA
         const unsigned N = frd.nextindex;
         buffer = (memstream_ro_t) memstream_FREE;
         TEST( (N?ENODATA:ENOBUFS) == readnext_filereader(&frd, &buffer));
         TEST( N == iseof_filereader(&frd));
         // check buffer
         TEST( 0 == buffer.next);
         TEST( 0 == buffer.end);
         // check frd
         TEST( 0 == frd.unreadsize);
         TEST( N == frd.nextindex);
         TEST( N == frd.nrfreebuffer);
         TEST( offset == frd.fileoffset);

         if (U) {
            for (unsigned n = N+1, u = S; n <= 2; ++n, u += S) {
               // TEST unread_filereader: unreads second buffer
               unread_filereader(&frd);
               // check frd
               TEST( u == frd.unreadsize);
               TEST( (n==1) == frd.nextindex);
               TEST( n == frd.nrfreebuffer);
               offset -= S;
               TEST( offset == frd.fileoffset);
            }
         }
      }

      const int N = frd.nextindex;
      for (int n = N+1; n <= 2; ++n) {
         // TEST release_filereader: mark last buffer as unused
         release_filereader(&frd);
         TEST( 0 == frd.unreadsize);
         TEST( (N==1) == frd.nextindex);
         TEST( n == frd.nrfreebuffer);
         TEST( offset == frd.fileoffset);
      }
   }

   // unprepare
   TEST(0 == free_filereader(&frd));

   // TEST readnext_filereader: ioerror is returned
   TEST(0 == init_filereader(&frd, "double", tempdir));
   TEST( 0 == readnext_filereader(&frd, &buffer));
   for (unsigned i = 1; i < 15; ++i) {
      setioerror_filereader(&frd, (int) i);
      const unsigned err = (unsigned) readnext_filereader(&frd, &buffer);
      TEST( err == i);
   }
   TEST(0 == free_filereader(&frd));

   // unprepare
   TEST(0 == FREE_MM(&mem));
   TEST(0 == removefile_directory(tempdir, "single"));
   TEST(0 == removefile_directory(tempdir, "double"));

   return 0;
ONERR:
   FREE_MM(&mem);
   removefile_directory(tempdir, "single");
   removefile_directory(tempdir, "double");
   return EINVAL;
}

int unittest_io_reader_filereader()
{
   directory_t* tempdir = 0;
   char         tmppath[128];

   // prepare
   TEST(0 == newtemp_directory(&tempdir, "filereader", &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(tmppath), (uint8_t*)tmppath)));

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
   TEST(0 == removedirectory_directory(0, tmppath));
   TEST(0 == delete_directory(&tempdir));

   return 0;
ONERR:
   (void) delete_directory(&tempdir);
   return EINVAL;
}

#endif
