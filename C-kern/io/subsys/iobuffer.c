/* title: IOBuffer impl

   Implements <IOBuffer>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/io/subsys/iobuffer.h
    Header file <IOBuffer>.

   file: C-kern/io/subsys/iobuffer.c
    Implementation file <IOBuffer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/subsys/iobuffer.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/wbuffer.h"
#endif


// section: iobuffer_t

// group: lifetime

int init_iobuffer(/*out*/iobuffer_t* iobuf)
{
   int err;

   // TODO: use ALLOC_PAGECACHE
   // reimplement ALLOC_PAGECACHE as init_mempage
   // add mempage process part (bitmap index to locate free pages) to processcontext
   // add mempage thread part (free list + list of used pages) to threadcontext

   err = init_vmpage(cast_vmpage(iobuf,), 1024*1024);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_iobuffer(iobuffer_t* iobuf)
{
   int err;

   err = free_vmpage(cast_vmpage(iobuf,));
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}


// section: iobuffer_stream_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_iobufferstream_errtimer
 * Simuliert Fehler in <init_iobufferstream> und <free_iobufferstream>. */
static test_errortimer_t s_iobufferstream_errtimer = test_errortimer_FREE;
#endif

// group: helper

/* function: start_reading
 * Startet das Lesen.
 * Es werden bis zu lengthof(iostream->iotask)-1 <iotask_t> in die I/O-Queue
 * gestellt. iostream->iotask[-1] wird immer als unbenutzt markiert (state == iostate_NULL).
 *
 * Unchecked Precondition:
 * - is_not_in_use(iostream->iotask[])
 *
 * Init:
 * iostream->iotask[], iostream->ioc, iostream->nextbuffer, iostream->filesize und iostream->readpos.
 */
static void start_reading(iobuffer_stream_t* iostream, iochannel_t file, off_t filesize)
{
   static_assert(
      lengthof(iostream->iotask) == lengthof(iostream->buffer),
      "every iotask has its own buffer"
   );

   iostream->ioc = file;
   iostream->nextbuffer = 0;
   iostream->filesize = filesize;

   unsigned nrtask;
   size_t   off = 0;
   iotask_t* iot[lengthof(iostream->iotask)];

   for (unsigned i = 0; i < lengthof(iostream->iotask); ++i) {
      initreadp_iotask(&iostream->iotask[i], file, iostream->buffer[i].size, iostream->buffer[i].addr, 0, &iostream->ready);
   }

   for (nrtask = 0; filesize && nrtask < lengthof(iostream->iotask)-1; ++nrtask) {
      iot[nrtask] = &iostream->iotask[nrtask];
      size_t size = iostream->buffer[nrtask].size;
      if (size > filesize) {
         size = (size_t) filesize;
         setsize_iotask(&iostream->iotask[nrtask], size);
      }
      setoffset_iotask(&iostream->iotask[nrtask], (off_t) off);
      off += size;
      filesize -= (off_t) size;
   }

   iostream->readpos = (off_t) off;

   static_assert(lengthof(iostream->iotask) <= 255, "nrtask fits into uint8_t");
   insertiotask_iothread(&iostream->iothread, (uint8_t) nrtask, iot);
}

// group: lifetime

int init_iobufferstream(/*out*/iobuffer_stream_t* iostream, const char* path, struct directory_t* relative_to/*could be 0*/)
{
   int err;
   file_t   file = file_FREE;
   unsigned ib   = 0;
   iobuffer_t buffer[lengthof(iostream->buffer)];
   itccounter_t ready;
   off_t    filesize;

   if (! PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err)) {
      err = init_itccounter(&ready);
   }
   if (err) goto ONERR_NOFREE;

   if (! PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err)) {
      err = init_file(&file, path, accessmode_READ, relative_to);
   }
   if (err) goto ONERR;

   if (! PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err)) {
      err = size_file(file, &filesize);
   }
   if (err) goto ONERR;
   if (filesize == 0) {
      err = ENODATA;
      goto ONERR;
   }

   for (; ib < lengthof(iostream->buffer); ++ib) {
      if (! PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err)) {
         err = init_iobuffer(&buffer[ib]);
      }
      if (err) goto ONERR;
   }

   // set out

   if (! PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err)) {
      err = init_iothread(&iostream->iothread);
   }
   if (err) goto ONERR;
   iostream->ready = ready;
   static_assert(sizeof(buffer) == sizeof(iostream->buffer), "copy all bytes");
   memcpy(iostream->buffer, buffer, sizeof(buffer));

   // queues read iotask && inits other fields

   start_reading(iostream, file, filesize);

   return 0;
ONERR:
   free_file(&file);
   while (ib > 0) {
      free_iobuffer(&buffer[--ib]);
   }
   free_itccounter(&ready);
ONERR_NOFREE:
   if (err != ENODATA) {
      TRACEEXIT_ERRLOG(err);
   }
   return err;
}

int free_iobufferstream(iobuffer_stream_t* iostream)
{
   int err;
   int err2;

   err = free_iothread(&iostream->iothread);
   (void) PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err);

   err2 = free_itccounter(&iostream->ready);
   (void) PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err2);
   if (err2) err = err2;

   for (unsigned i = 0; i < lengthof(iostream->buffer); ++i) {
      err2 = free_iobuffer(&iostream->buffer[i]);
      (void) PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err2);
      if (err2) err = err2;
   }

   err2 = free_file(&iostream->ioc);
   (void) PROCESS_testerrortimer(&s_iobufferstream_errtimer, &err2);
   if (err2) err = err2;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int readnext_iobufferstream(iobuffer_stream_t* iostream, /*out*/struct memblock_t* nextbuffer)
{
   int err;

   if (iostate_NULL == iostream->iotask[iostream->nextbuffer].state) {
      return ENODATA;
   }

   while (iostate_QUEUED == iostream->iotask[iostream->nextbuffer].state) {
      err = wait_itccounter(&iostream->ready, -1);
      if (err) goto ONERR;
      (void) reset_itccounter(&iostream->ready);
   }

   if (iostate_OK != iostream->iotask[iostream->nextbuffer].state) {
      err = iostream->iotask[iostream->nextbuffer].err;
      goto ONERR;
   }

   // set out
   iostream->iotask[iostream->nextbuffer].state = iostate_NULL;
   *nextbuffer = (memblock_t) memblock_INIT(
                                 iostream->iotask[iostream->nextbuffer].bufsize,
                                 iostream->iotask[iostream->nextbuffer].bufaddr
                                 );
   // queue next I/O
   if (iostream->readpos < iostream->filesize) {
      const unsigned prev = (iostream->nextbuffer ? iostream->nextbuffer : lengthof(iostream->buffer)) - 1;
      size_t size = iostream->buffer[prev].size;
      if (size > iostream->filesize - iostream->readpos) {
         size = (size_t) (iostream->filesize - iostream->readpos);
         setsize_iotask(&iostream->iotask[prev], size);
      }
      setoffset_iotask(&iostream->iotask[prev], iostream->readpos);
      iostream->readpos += (off_t) size;
      iotask_t* iot[1] = { &iostream->iotask[prev] };
      insertiotask_iothread(&iostream->iothread, 1, iot);
   }

   // switch to next
   ++ iostream->nextbuffer;
   if (iostream->nextbuffer == lengthof(iostream->buffer)) {
      iostream->nextbuffer = 0;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}




// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   iobuffer_t iobuf = iobuffer_FREE;
   const unsigned MB = 1024*1024;

   // TEST iobuffer_FREE
   TEST(0 == iobuf.addr);
   TEST(0 == iobuf.size);

   // TEST init_iobuffer
   TEST(0 == init_iobuffer(&iobuf));
   TEST(0 != iobuf.addr);
   TEST(MB == iobuf.size);
   for (int i = 0; i < 256; ++i) {
      iobuf.addr[0] = (uint8_t) i;
      iobuf.addr[MB-1] = (uint8_t) (255-i);
      TEST(i == iobuf.addr[0]);
      TEST((255-i) == iobuf.addr[MB-1]);
   }

   // TEST free_iobuffer
   TEST(0 == free_iobuffer(&iobuf));
   TEST(0 == iobuf.addr);
   TEST(0 == iobuf.size);

   // TEST free_iobuffer: already freed
   TEST(0 == free_iobuffer(&iobuf));
   TEST(0 == iobuf.addr);
   TEST(0 == iobuf.size);

   return 0;
ONERR:
   free_iobuffer(&iobuf);
   return EINVAL;
}

static int test_query(void)
{
   iobuffer_t iobuf = iobuffer_FREE;

   // TEST addr_iobuffer
   for (uintptr_t i = 1; i; i <<= 1) {
      iobuf.addr = (void*) i;
      TEST((void*)i == addr_iobuffer(&iobuf));
   }
   iobuf.addr = 0;
   TEST(0 == addr_iobuffer(&iobuf));

   // TEST size_iobuffer
   for (size_t i = 1; i; i <<= 1) {
      iobuf.size = i;
      TEST(i == size_iobuffer(&iobuf));
   }
   iobuf.size = 0;
   TEST(0 == size_iobuffer(&iobuf));

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree_stream(directory_t* tmpdir)
{
   iobuffer_stream_t iostream = iobuffer_stream_FREE;
   iobuffer_t  iobuf = iobuffer_FREE;
   file_t      file  = file_FREE;
   const unsigned SZ = 1024*1024;

   // prepare0
   TEST(0 == init_iobuffer(&iobuf));
   TEST(0 == initcreate_file(&file, "empty", tmpdir));
   TEST(0 == free_file(&file));
   TEST(0 == initcreate_file(&file, "stream", tmpdir));
   for (size_t val = 0, mb = 0; mb <= lengthof(iostream.buffer); ++mb) {
      for (size_t i = 0; i < iobuf.size/sizeof(uint32_t); ++i, ++val) {
         ((uint32_t*)iobuf.addr)[i] = val;
      }
      TEST(iobuf.size == (size_t) write(io_file(file), iobuf.addr, iobuf.size));
   }
   TEST(0 == free_file(&file));

   // TEST iobuffer_stream_FREE
   TEST(0 == iostream.iothread.thread);
   TEST( isfree_itccounter(&iostream.ready));
   for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
      TEST(0 == iostream.buffer[i].addr);
      TEST(0 == iostream.buffer[i].size);
      TEST(0 == iostream.iotask[i].state);
   }
   TEST(isfree_iochannel(iostream.ioc));

   for (size_t filesize = (lengthof(iostream.buffer)+1)*SZ; filesize; --filesize) {
      // prepare
      if (filesize % SZ == SZ - 5) {
         filesize -= SZ - 10;
      }
      const size_t nrbuffer    = filesize / SZ;
      const size_t lastbufsize = filesize % SZ;
      TEST(0 == init_file(&file, "stream", accessmode_RDWR, tmpdir));
      TEST(0 == truncate_file(file, (off_t) filesize));
      TEST(0 == free_file(&file));

      // TEST init_iobufferstream: filesize > sum(buffersize)
      TEST(0 == init_iobufferstream(&iostream, "stream", tmpdir));
      // check iostream fields which are not arrays
      TEST(0 != iostream.iothread.thread);
      TEST(! isfree_itccounter(&iostream.ready));
      TEST(! isfree_iochannel(iostream.ioc));
      TEST(iostream.nextbuffer == 0);
      TEST(iostream.filesize   == filesize);
      TEST(iostream.readpos    == (nrbuffer < lengthof(iostream.buffer)-1 ? filesize : SZ*(lengthof(iostream.buffer)-1)));
      // check iostream.buffer[]
      for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
         TEST(0 != iostream.buffer[i].addr);
         TEST(SZ == iostream.buffer[i].size);
         for (unsigned i2 = i+1; i2 < lengthof(iostream.buffer); ++i2) {
            TEST(iostream.buffer[i].addr != iostream.buffer[i2].addr);
         }
      }
      // check iostream.iotask[]
      for (unsigned i = 0; i < lengthof(iostream.iotask); ++i) {
         while (iostream.iotask[i].state == iostate_QUEUED) {
            TEST(0 == wait_itccounter(&iostream.ready, -1));
            reset_itccounter(&iostream.ready);
         }
         TEST(iostream.iotask[i].iolist_next == 0);
         TEST(iostream.iotask[i].op      == ioop_READ);
         TEST(iostream.iotask[i].bufaddr == iostream.buffer[i].addr);
         TEST(iostream.iotask[i].readycount == &iostream.ready);
         if (i == lengthof(iostream.iotask)-1 || i > nrbuffer || (i == nrbuffer && lastbufsize == 0)) {
            TEST(iostream.iotask[i].state  == iostate_NULL);
            TEST(iostream.iotask[i].offset == 0);
            TEST(iostream.iotask[i].bufsize == iostream.buffer[i].size);
         } else {
            TEST(iostream.iotask[i].bytesrw == (i == nrbuffer ? lastbufsize : SZ));
            TEST(iostream.iotask[i].state  == iostate_OK);
            TEST(iostream.iotask[i].offset == (off_t)SZ * i);
            TEST(iostream.iotask[i].bufsize == iostream.iotask[i].bytesrw);
            // check content of read buffer
            for (size_t i2 = 0, val = (size_t)iostream.iotask[i].offset/sizeof(uint32_t); i2 < iostream.iotask[i].bufsize/sizeof(uint32_t); ++i2, ++val) {
               ((uint32_t*)iostream.iotask[i].bufaddr)[i2] = val;
            }
         }
      }

      // TEST free_iobufferstream
      TEST(0 == free_iobufferstream(&iostream));
      TEST(0 == iostream.iothread.thread);
      TEST( isfree_itccounter(&iostream.ready));
      for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
         TEST(0 == iostream.buffer[i].addr);
         TEST(0 == iostream.buffer[i].size);
      }
      for (unsigned i = 0; i < lengthof(iostream.iotask); ++i) {
         TEST( iostate_OK == iostream.iotask[i].state
               || iostate_NULL == iostream.iotask[i].state);
      }
      TEST(isfree_iochannel(iostream.ioc));

      // TEST free_iobufferstream: iotask are canceled
      TEST(0 == init_iobufferstream(&iostream, "stream", tmpdir));
      TEST(0 == free_iobufferstream(&iostream));
      TEST(0 == iostream.iothread.thread);
      TEST( isfree_itccounter(&iostream.ready));
      for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
         TEST(0 == iostream.buffer[i].addr);
         TEST(0 == iostream.buffer[i].size);
      }
      for (unsigned i = 0; i < lengthof(iostream.iotask); ++i) {
         TEST( iostate_CANCELED == iostream.iotask[i].state
               || iostate_NULL == iostream.iotask[i].state);
      }
      TEST(isfree_iochannel(iostream.ioc));
   }

   // TEST free_iobufferstream: already freed
   TEST(0 == free_iobufferstream(&iostream));
   TEST(0 == iostream.iothread.thread);
   TEST( isfree_itccounter(&iostream.ready));
   for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
      TEST(0 == iostream.buffer[i].addr);
      TEST(0 == iostream.buffer[i].size);
   }
   TEST(isfree_iochannel(iostream.ioc));

   // TEST init_iobufferstream: ENOENT
   TEST(ENOENT == init_iobufferstream(&iostream, "__UNKNOWN__", tmpdir));
   TEST(0 == iostream.iothread.thread);
   TEST( isfree_itccounter(&iostream.ready));
   for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
      TEST(0 == iostream.buffer[i].addr);
      TEST(0 == iostream.buffer[i].size);
   }
   TEST(isfree_iochannel(iostream.ioc));

   // TEST init_iobufferstream: ENODATA
   TEST(ENODATA == init_iobufferstream(&iostream, "empty", tmpdir));
   TEST(0 == iostream.iothread.thread);
   TEST( isfree_itccounter(&iostream.ready));
   for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
      TEST(0 == iostream.buffer[i].addr);
      TEST(0 == iostream.buffer[i].size);
   }
   TEST(isfree_iochannel(iostream.ioc));

   // TEST init_iobufferstream: simulated ERROR
   for (int e = 1; e <= 7; ++e) {
      init_testerrortimer(&s_iobufferstream_errtimer, (unsigned)e, e);
      TEST(e == init_iobufferstream(&iostream, "stream", tmpdir));
      TEST(0 == iostream.iothread.thread);
      TEST( isfree_itccounter(&iostream.ready));
      for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
         TEST(0 == iostream.buffer[i].addr);
         TEST(0 == iostream.buffer[i].size);
      }
      TEST(isfree_iochannel(iostream.ioc));
   }

   // TEST free_iobufferstream: EBADF
   TEST(0 == init_iobufferstream(&iostream, "stream", tmpdir));
   int badfd = iostream.ioc;
   TEST(0 == free_file(&badfd));
   TEST(EBADF == free_iobufferstream(&iostream));

   // TEST free_iobufferstream: simulated ERROR
   for (int e = 1; e <= 6; ++e) {
      TEST(0 == init_iobufferstream(&iostream, "stream", tmpdir));
      init_testerrortimer(&s_iobufferstream_errtimer, (unsigned)e, e);
      TEST(e == free_iobufferstream(&iostream));
      TEST(0 == iostream.iothread.thread);
      TEST( isfree_itccounter(&iostream.ready));
      for (unsigned i = 0; i < lengthof(iostream.buffer); ++i) {
         TEST(0 == iostream.buffer[i].addr);
         TEST(0 == iostream.buffer[i].size);
      }
      TEST(isfree_iochannel(iostream.ioc));
   }

   // reset0
   TEST(0 == free_iobuffer(&iobuf));
   TEST(0 == removefile_directory(tmpdir, "stream"));
   TEST(0 == removefile_directory(tmpdir, "empty"));

   return 0;
ONERR:
   free_iobufferstream(&iostream);
   free_file(&file);
   removefile_directory(tmpdir, "stream");
   removefile_directory(tmpdir, "empty");
   free_iobuffer(&iobuf);
   return EINVAL;
}

static int test_read_stream(directory_t* tmpdir)
{
   iobuffer_stream_t iostream = iobuffer_stream_FREE;
   iobuffer_t  iobuf = iobuffer_FREE;
   file_t      file  = file_FREE;
   memblock_t  mblock = memblock_FREE;
   off_t       filesize;

   // prepare0
   TEST(0 == init_iobuffer(&iobuf));
   TEST(0 == initcreate_file(&file, "stream", tmpdir));
   for (size_t val = 0, mb = 0; mb < 25; ++mb) {
      for (size_t i = 0; i < iobuf.size/sizeof(uint32_t); ++i, ++val) {
         ((uint32_t*)iobuf.addr)[i] = val;
      }
      TEST(iobuf.size == (size_t) write(io_file(file), iobuf.addr, iobuf.size));
   }
   TEST(0 == size_file(file, &filesize));
   TEST(0 == free_file(&file));

   while (filesize > 0) {
      // prepare
      TEST(0 == init_iobufferstream(&iostream, "stream", tmpdir));

      // TEST readnext_iobufferstream: read files of different sizes
      unsigned bi = 0;
      unsigned pbi = lengthof(iostream.buffer)-1;
      for (off_t off = 0; off < filesize; off += (off_t) iobuf.size) {
         bool isLoad = (iostream.readpos < filesize);
         TEST(0 == readnext_iobufferstream(&iostream, &mblock));
         // check mblock
         TEST(mblock.addr == iostream.buffer[bi].addr);
         if (filesize - off < (off_t) iobuf.size) {
            TEST(mblock.size == (size_t) (filesize - off));
         } else {
            TEST(mblock.size == iostream.buffer[bi].size);
         }
         // check content
         for (size_t i = 0, val = (size_t)off/sizeof(uint32_t); i < iobuf.size/sizeof(uint32_t); ++i, ++val) {
            ((uint32_t*)mblock.addr)[i] = val;
         }
         // check iostream.iotask[pbi]
         if (isLoad) {
            TEST(iostream.iotask[pbi].state != iostate_NULL);
         }else {
            TEST(iostream.iotask[pbi].state == iostate_NULL);
         }
         // check iostream.iotask[bi]
         TEST(iostream.iotask[bi].bytesrw == mblock.size);
         TEST(iostream.iotask[bi].state   == iostate_NULL);
         TEST(iostream.iotask[bi].op      == ioop_READ);
         TEST(iostream.iotask[bi].ioc     == iostream.ioc);
         TEST(iostream.iotask[bi].offset  == off);
         TEST(iostream.iotask[bi].bufsize == mblock.size);
         TEST(iostream.iotask[bi].bufaddr == mblock.addr);
         // check iostream
         pbi = bi;
         bi = (bi + 1) % lengthof(iostream.buffer);
         TEST(iostream.nextbuffer == bi);
         TEST(iostream.filesize   == filesize);
         TEST(iostream.readpos    == (off+3*(off_t)iobuf.size < filesize ? off+3*(off_t)iobuf.size : filesize));
      }

      // reset
      TEST(0 == free_iobufferstream(&iostream));

      // switch to other filesize
      if (filesize > iobuf.size*lengthof(iostream.buffer)) {
         filesize = iobuf.size * lengthof(iostream.buffer);
      } else {
         filesize -= 1+iobuf.size;
      }
      if (filesize > 0) {
         TEST(0 == init_file(&file, "stream", accessmode_RDWR, tmpdir));
         TEST(0 == truncate_file(file, filesize));
         TEST(0 == free_file(&file));
      }
   }

   // reset0
   TEST(0 == free_iobuffer(&iobuf));
   TEST(0 == removefile_directory(tmpdir, "stream"));

   return 0;
ONERR:
   free_iobufferstream(&iostream);
   free_file(&file);
   removefile_directory(tmpdir, "stream");
   free_iobuffer(&iobuf);
   return EINVAL;
}

int unittest_io_subsys_iobuffer()
{
   directory_t* dir = 0;
   uint8_t      dirpath[256];

   // prepare
   TEST(0 == newtemp_directory(&dir, "iobuffer", &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(dirpath), dirpath)));

   // iobuffer_t
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   // iobuffer_stream_t
   if (test_initfree_stream(dir))   goto ONERR;
   if (test_read_stream(dir))       goto ONERR;

   // adapt log (change temp name of directory to XXXXXX)
   size_t   logsize;
   uint8_t* logbuffer;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   for (char* s = (char*)logbuffer; 0 != (s = strstr(s, "/__UNKNOWN__")); ++s) {
      memcpy(s-6, "XXXXXX", 6);
   }

   // reset
   TEST(0 == delete_directory(&dir));
   TEST(0 == removedirectory_directory(0, (const char*)dirpath));

   return 0;
ONERR:
   if (dir) {
      delete_directory(&dir);
      removedirectory_directory(0, (const char*)dirpath);
   }
   return EINVAL;
}

#endif
