/* title: Adapter-InStream-MemoryMappedFile impl

   Implements <Adapter-InStream-MemoryMappedFile>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/adapter/instream_mmfile.h"
#include "C-kern/api/platform/virtmemory.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: instream_mmfile_t

// group: variables

static const instream_mmfile_it s_iinstream = { &readnext_instreammmfile } ;

// group: lifetime

static inline size_t buffersize_instreammmfile(void)
{
   size_t align_remain = KONFIG_BUFFERSIZE_INSTREAM_READNEXT % (2*pagesize_vm()) ;
   return (align_remain ? KONFIG_BUFFERSIZE_INSTREAM_READNEXT + (2*pagesize_vm()) - align_remain : KONFIG_BUFFERSIZE_INSTREAM_READNEXT) ;
}

int init_instreammmfile(/*out*/instream_mmfile_t * obj, /*out*/const instream_mmfile_it ** iinstream, const char * filepath, const struct directory_t * relative_to)
{
   int err ;
   file_t   fd    = file_INIT_FREEABLE ;
   mmfile_t mfile = mmfile_INIT_FREEABLE ;
   size_t   bufsize = 2 * buffersize_instreammmfile() ;
   off_t    inputsize ;

   err = init_file(&fd, filepath, accessmode_READ, relative_to) ;
   if (err) goto ONABORT ;

   err = size_file(fd, &inputsize) ;
   if (err) goto ONABORT ;

   if (inputsize <= bufsize) {
      bufsize = (size_t) inputsize ;
   }

   err = initfd_mmfile(&mfile, fd, 0, bufsize, accessmode_READ) ;
   if (err) goto ONABORT ;

   initmove_mmfile(&obj->buffer, &mfile) ;
   obj->inputsize    = inputsize ;
   obj->inputoffset  = 0 ;
   obj->bufferoffset = 0 ;
   initmove_file(&obj->inputstream, &fd) ;

   *iinstream = &s_iinstream ;

   return 0 ;
ONABORT:
   (void) free_file(&fd) ;
   (void) free_mmfile(&mfile) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_instreammmfile(instream_mmfile_t * obj)
{
   int err ;
   int err2 ;

   err = free_mmfile(&obj->buffer) ;
   obj->inputsize    = 0 ;
   obj->inputoffset  = 0 ;
   obj->bufferoffset = 0 ;
   err2 = free_file(&obj->inputstream) ;
   if (err2) err = err2 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

bool isinit_instreammmfile(const instream_mmfile_t * obj)
{
   return   isinit_file(obj->inputstream)
            || isinit_mmfile(&obj->buffer) ;
}

// group: instream_it implementation

int readnext_instreammmfile(instream_mmfile_t * obj, /*inout*/struct memblock_t * datablock, /*out*/uint8_t ** keepaddr, size_t keepsize)
{
   int err ;
   size_t   keepsize_alignoffset ;

   if (obj->inputoffset >= obj->inputsize) {
      if (!keepsize) {
         *datablock = (memblock_t) memblock_INIT_FREEABLE ;
      } // else keep datablock
      keepsize_alignoffset = datablock->size - keepsize ;

   } else {
      size_t keepsize_aligned ;
      size_t keepsize_modulo ;
      size_t readnextsize ;
      keepsize_modulo      = keepsize % pagesize_vm() ;
      keepsize_alignoffset = (keepsize_modulo ? pagesize_vm() : 0) - keepsize_modulo ;
      keepsize_aligned     = keepsize + keepsize_alignoffset ;
      readnextsize         = size_mmfile(&obj->buffer)/2 ;

      if (keepsize_aligned > readnextsize || (off_t)keepsize_aligned > obj->inputoffset) {
         err = EINVAL ;
         goto ONABORT ;
      }

      off_t    readoffset  = obj->inputoffset - (off_t)keepsize_aligned ;
      off_t    unreadsize  = obj->inputsize - readoffset ;
      obj->bufferoffset   -= keepsize_aligned ;

      if (size_mmfile(&obj->buffer) - obj->bufferoffset >= unreadsize) {
         readnextsize = (size_t)unreadsize ;

      } else if (keepsize_aligned == readnextsize) {
         // grow buffer or reposition ?
         readnextsize = size_mmfile(&obj->buffer) ;
         obj->bufferoffset = 0 ;

         if (readnextsize >= unreadsize) {
            // reposition is enough
            readnextsize = (size_t)unreadsize ;
            err = seek_mmfile(&obj->buffer, obj->inputstream, readoffset, accessmode_READ) ;
            if (err) goto ONABORT ;

         } else {
            // double buffer size
            size_t newsize = 2 * readnextsize ;
            if (newsize <= size_mmfile(&obj->buffer)) {
               err = ENOMEM ;
               goto ONABORT ;
            }
            // grow buffer
            err = free_mmfile(&obj->buffer) ;
            if (err) goto ONABORT ;
            err = initfd_mmfile(&obj->buffer, obj->inputstream, readoffset, newsize, accessmode_READ) ;
            if (err) goto ONABORT ;
         }

      } else if (obj->bufferoffset >= readnextsize) {
         // reposition is enough
         obj->bufferoffset = 0 ;
         err = seek_mmfile(&obj->buffer, obj->inputstream, readoffset, accessmode_READ) ;
         if (err) goto ONABORT ;

         if (readnextsize > unreadsize) {
            readnextsize = (size_t)unreadsize ;
         }
      }

      // set datablock
      *datablock = (memblock_t) memblock_INIT(readnextsize, addr_mmfile(&obj->buffer) + obj->bufferoffset) ;

      // prepare for next call
      obj->inputoffset   = readoffset + readnextsize ;
      obj->bufferoffset += readnextsize ;
   }

   // set keepaddr
   *keepaddr = datablock->addr + keepsize_alignoffset ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(directory_t * tempdir)
{
   instream_mmfile_t          obj = instream_mmfile_INIT_FREEABLE ;
   const instream_mmfile_it   * iinstream = 0 ;
   const size_t               S = 2*buffersize_instreammmfile() ;
   const size_t               D = 4*buffersize_instreammmfile() ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "singlebuffer", S)) ;
   TEST(0 == makefile_directory(tempdir, "doublebuffer", D)) ;

   // TEST instream_mmfile_INIT_FREEABLE
   TEST(0 == isinit_mmfile(&obj.buffer)) ;
   TEST(0 == obj.inputsize) ;
   TEST(0 == obj.inputoffset) ;
   TEST(0 == obj.bufferoffset) ;
   TEST(0 == isinit_file(obj.inputstream)) ;

   // TEST init_instreammmfile, free_instreammmfile: single buffer enough
   memset(&obj, -1, sizeof(obj)) ;
   TEST(0 == init_instreammmfile(&obj, &iinstream, "singlebuffer", tempdir)) ;
   TEST(iinstream == &s_iinstream) ;
   TEST(iinstream->readnext == &readnext_instreammmfile) ;
   TEST(1 == isinit_mmfile(&obj.buffer)) ;
   TEST(S == size_mmfile(&obj.buffer)) ;
   TEST(S == obj.inputsize) ;
   TEST(0 == obj.inputoffset) ;
   TEST(0 == obj.bufferoffset) ;
   TEST(1 == isinit_file(obj.inputstream)) ;
   obj.inputoffset  = (size_t)-1 ;   // artificial value used to test for setting to 0
   obj.bufferoffset = 1 ;            // artificial value used to test for setting to 0
   TEST(0 == free_instreammmfile(&obj)) ;
   TEST(0 == isinit_mmfile(&obj.buffer)) ;
   TEST(0 == obj.inputsize) ;
   TEST(0 == obj.inputoffset) ;
   TEST(0 == obj.bufferoffset) ;
   TEST(0 == isinit_file(obj.inputstream)) ;
   TEST(0 == free_instreammmfile(&obj)) ;
   TEST(0 == isinit_mmfile(&obj.buffer)) ;
   TEST(0 == obj.inputsize) ;
   TEST(0 == obj.inputoffset) ;
   TEST(0 == obj.bufferoffset) ;
   TEST(0 == isinit_file(obj.inputstream)) ;

   // TEST init_instreammmfile, free_instreammmfile: double buffer needed
   memset(&obj, -1, sizeof(obj)) ;
   iinstream = 0 ;
   TEST(0 == init_instreammmfile(&obj, &iinstream, "doublebuffer", tempdir)) ;
   TEST(iinstream == &s_iinstream) ;
   TEST(iinstream->readnext == &readnext_instreammmfile) ;
   TEST(1 == isinit_mmfile(&obj.buffer)) ;
   TEST(S == size_mmfile(&obj.buffer)) ;
   TEST(D == obj.inputsize) ;
   TEST(0 == obj.inputoffset) ;
   TEST(0 == obj.bufferoffset) ;
   TEST(1 == isinit_file(obj.inputstream)) ;
   TEST(0 == free_instreammmfile(&obj)) ;
   TEST(0 == isinit_mmfile(&obj.buffer)) ;
   TEST(0 == obj.inputsize) ;
   TEST(0 == obj.inputoffset) ;
   TEST(0 == obj.bufferoffset) ;
   TEST(0 == isinit_file(obj.inputstream)) ;
   TEST(0 == free_instreammmfile(&obj)) ;

   // TEST isinit_instreammmfile
   obj = (instream_mmfile_t) instream_mmfile_INIT_FREEABLE ;
   TEST(0 == isinit_instreammmfile(&obj)) ;
   obj.inputstream = file_STDIN ;
   TEST(1 == isinit_instreammmfile(&obj)) ;
   obj.inputstream = file_INIT_FREEABLE ;
   obj.buffer.addr = (void*)1 ;
   TEST(1 == isinit_instreammmfile(&obj)) ;
   obj.buffer.addr = (void*)0 ;
   obj.buffer.size = 1 ;
   TEST(1 == isinit_instreammmfile(&obj)) ;
   obj.buffer.size = 0 ;
   TEST(0 == isinit_instreammmfile(&obj)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "singlebuffer")) ;
   TEST(0 == removefile_directory(tempdir, "doublebuffer")) ;

   return 0 ;
ONABORT:
   removefile_directory(tempdir, "singlebuffer") ;
   removefile_directory(tempdir, "doublebuffer") ;
   return EINVAL ;
}

static int test_readnext(directory_t * tempdir)
{
   instream_mmfile_t          obj   = instream_mmfile_INIT_FREEABLE ;
   mmfile_t                   mfile = mmfile_INIT_FREEABLE ;
   uint8_t                    * keepaddr ;
   size_t                     keepsize = 0 ;
   const instream_mmfile_it   * iinstream = 0 ;
   const char                 * filename[3] = { "singlebuffer", "doublebuffer", "specialcases" } ;
   const size_t               filesize[3]   = { 2*buffersize_instreammmfile()-sizeof(uint32_t), 16*buffersize_instreammmfile()-sizeof(uint32_t), 2*buffersize_instreammmfile()+pagesize_vm()-sizeof(uint32_t) } ;
   memblock_t                 datablock = memblock_INIT_FREEABLE ;

   // prepare
   TEST(0 == (pagesize_vm() % sizeof(uint32_t))) ;
   for (unsigned i = 0; i < nrelementsof(filename); ++i) {
      TEST(0 == makefile_directory(tempdir, filename[i], filesize[i])) ;
      TEST(0 == init_mmfile(&mfile, filename[i], 0, 0, accessmode_RDWR_SHARED, tempdir)) ;
      for (size_t b = 0; b < filesize[i]/sizeof(uint32_t); ++b) {
         ((uint32_t*)addr_mmfile(&mfile))[b] = b ;
      }
      TEST(0 == free_mmfile(&mfile)) ;
   }

   // TEST readnext_instreammmfile
   for (unsigned i = 0; i < nrelementsof(filename); ++i) {
      TEST(0 == init_instreammmfile(&obj, &iinstream, filename[i], tempdir)) ;
      size_t readoffset = 0 ;
      // check streaming blocks
      while (readoffset < filesize[i]) {
         TEST(0 == (readoffset % pagesize_vm())) ;
         keepaddr = 0 ;
         TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
         TEST(keepaddr == addr_memblock(&datablock)) ;
         TEST(addr_memblock(&datablock) != 0) ;
         if (size_memblock(&datablock) != buffersize_instreammmfile()) { /*last block*/
            TEST(size_memblock(&datablock) == filesize[i] - readoffset) ;
            TEST(addr_memblock(&datablock) == addr_memblock(&obj.buffer) + (readoffset ? buffersize_instreammmfile() : 0)) ;
         } else {
            TEST(addr_memblock(&datablock) == addr_memblock(&obj.buffer)) /* bufferoffset is reset to 0*/ ;
         }
         for (size_t b = 0; b < size_memblock(&datablock)/sizeof(uint32_t); ++b) {
            TEST(((uint32_t*)addr_memblock(&datablock))[b] == b + (readoffset/sizeof(uint32_t))) ;
         }
         readoffset += size_memblock(&datablock) ;
      }
      // check end of input
      TEST(readoffset == filesize[i]) ;
      TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
      TEST(0 == keepaddr) ;
      TEST(isfree_memblock(&datablock)) ;
      TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
      TEST(isfree_memblock(&datablock)) ;
      TEST(0 == free_instreammmfile(&obj)) ;
   }

   // TEST readnext_instreammmfile: if (keepsize == blocksize) { "buffer doubles in size" }
   for (unsigned i = 0; i < nrelementsof(filename); ++i) {
      TEST(0 == init_instreammmfile(&obj, &iinstream, filename[i], tempdir)) ;
      keepsize = 0 ;
      size_t bufsize = buffersize_instreammmfile() ;
      // check streaming blocks
      while (keepsize < filesize[i]) {
         keepaddr = 0 ;
         TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, keepsize)) ;
         TEST(keepaddr == addr_memblock(&datablock)) ;
         TEST(addr_memblock(&datablock) != 0) ;
         TEST(addr_memblock(&datablock) == addr_mmfile(&obj.buffer)) ;
         if (bufsize != size_memblock(&datablock)) {
            TEST(size_memblock(&datablock) == filesize[i]) ;
         }
         for (size_t b = 0; b < size_memblock(&datablock)/sizeof(uint32_t); ++b) {
            TEST(((uint32_t*)addr_memblock(&datablock))[b] == b) ;
         }
         keepsize = size_memblock(&datablock) ;
         bufsize *= 2 ;
      }
      // check end of input
      TEST(keepsize == filesize[i]) ;
      size_t   oldsize   = size_memblock(&datablock) ;
      uint8_t  * oldaddr = addr_memblock(&datablock) ;
      TEST(oldaddr == addr_mmfile(&obj.buffer)) ;
      for (; keepsize; --keepsize) {
         TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, keepsize)) ;
         TEST(oldsize  == size_memblock(&datablock)) ;
         TEST(oldaddr  == addr_memblock(&datablock)) ;
         TEST(keepaddr == oldaddr + oldsize - keepsize) ;
         if (keepsize > pagesize_vm()) keepsize -= pagesize_vm() ;
      }
      for (size_t b = 0; b < size_memblock(&datablock)/sizeof(uint32_t); ++b) {
         TEST(((uint32_t*)addr_memblock(&datablock))[b] == b) ;
      }
      TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
      TEST(0 == keepaddr) ;
      TEST(isfree_memblock(&datablock)) ;
      TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
      TEST(isfree_memblock(&datablock)) ;
      TEST(0 == free_instreammmfile(&obj)) ;
   }

   // TEST readnext_instreammmfile: keepsize < buffersize_instreammmfile()
   for (unsigned i = 0; i < nrelementsof(filename); ++i) {
      TEST(0 == init_instreammmfile(&obj, &iinstream, filename[i], tempdir)) ;
      TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
      keepsize = pagesize_vm()-1 ;
      bool   isoffset   = true ;
      size_t readoffset = size_memblock(&datablock) ;
      while (readoffset < filesize[i]) {
         readoffset -= pagesize_vm() ;
         TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, keepsize)) ;
         TEST(keepaddr == addr_memblock(&datablock)+1) ;
         TEST(addr_memblock(&datablock) == (isoffset ? buffersize_instreammmfile() - pagesize_vm() : 0) + addr_mmfile(&obj.buffer)) ;
         if (size_memblock(&datablock) != buffersize_instreammmfile()) {
            TEST(size_memblock(&datablock) == filesize[i] - readoffset) ;
         }
         for (size_t b = 0; b < size_memblock(&datablock)/sizeof(uint32_t); ++b) {
            TEST(((uint32_t*)addr_memblock(&datablock))[b] == b + (readoffset/sizeof(uint32_t))) ;
         }
         isoffset = !isoffset ;
         readoffset += size_memblock(&datablock) ;
      }
      // check end of input
      TEST(readoffset == filesize[i]) ;
      TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
      TEST(0 == keepaddr) ;
      TEST(isfree_memblock(&datablock)) ;
      TEST(0 == free_instreammmfile(&obj)) ;
   }

   // TEST readnext_instreammmfile: buffer would grow but unreadsize fits so resize is not necessary
   TEST(0 == init_instreammmfile(&obj, &iinstream, filename[nrelementsof(filename)-1], tempdir)) ;
   TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
   keepsize = pagesize_vm() ;
   TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, keepsize)) ;
   keepsize = buffersize_instreammmfile() ;
   TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, keepsize)) ;
   for (size_t b = 0; b < size_memblock(&datablock)/sizeof(uint32_t); ++b) {
      TEST(((uint32_t*)addr_memblock(&datablock))[b] == b + (pagesize_vm()/sizeof(uint32_t))) ;
   }
   TEST(2*buffersize_instreammmfile() == size_memblock(&datablock)+sizeof(uint32_t)) ;
   TEST(2*buffersize_instreammmfile() == size_mmfile(&obj.buffer)) /*buffer not grown*/ ;
   TEST(0 == free_instreammmfile(&obj)) ;

   // TEST readnext_instreammmfile: reposition considers unread size and shrinks datablock
   TEST(0 == init_instreammmfile(&obj, &iinstream, filename[nrelementsof(filename)-1], tempdir)) ;
   TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
   TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, pagesize_vm())) ;
   TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
   for (size_t b = 0; b < size_memblock(&datablock)/sizeof(uint32_t); ++b) {
      TEST(((uint32_t*)addr_memblock(&datablock))[b] == b + ((buffersize_instreammmfile()+pagesize_vm())/sizeof(uint32_t))) ;
   }
   TEST(buffersize_instreammmfile() == size_memblock(&datablock)+sizeof(uint32_t)) /*shrunk by sizeof(uint32_t)*/ ;
   TEST(0 == free_instreammmfile(&obj)) ;

   // TEST readnext_instreammmfile: EINVAL
   TEST(0 == init_instreammmfile(&obj, &iinstream, filename[1], tempdir)) ;
   datablock = (memblock_t) memblock_INIT_FREEABLE ;
   TEST(EINVAL == readnext_instreammmfile(&obj, &datablock, &keepaddr, 1)/*keepsize_aligned > obj.inputoffset*/) ;
   TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
   TEST(0 == readnext_instreammmfile(&obj, &datablock, &keepaddr, 0)) ;
   TEST(EINVAL == readnext_instreammmfile(&obj, &datablock, &keepaddr, buffersize_instreammmfile()+1)/*keepsize_aligned > readnextsize*/) ;
   TEST(0 == free_instreammmfile(&obj)) ;

   // unprepare
   for (unsigned i = 0; i < nrelementsof(filename); ++i) {
      TEST(0 == removefile_directory(tempdir, filename[i])) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < nrelementsof(filename); ++i) {
      (void) removefile_directory(tempdir, filename[i]) ;
   }
   free_mmfile(&mfile) ;
   free_instreammmfile(&obj) ;
   return EINVAL ;
}

int unittest_io_adapter_instream_mmfile()
{
   directory_t   * tempdir = 0 ;
   cstring_t       tmppath = cstring_INIT ;
   resourceusage_t usage   = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == newtemp_directory(&tempdir, "instreammmfile", &tmppath)) ;

   if (test_initfree(tempdir))   goto ONABORT ;
   if (test_readnext(tempdir))   goto ONABORT ;

   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   if (tempdir) {
      (void) delete_directory(&tempdir) ;
      (void) removedirectory_directory(0, str_cstring(&tmppath)) ;
   }
   (void) free_cstring(&tmppath) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
