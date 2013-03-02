/* title: UTF8-Scanner impl

   Implements <UTF8-Scanner>.

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

   file: C-kern/api/lang/utf8scanner.h
    Header file <UTF8-Scanner>.

   file: C-kern/lang/utf8scanner.c
    Implementation file <UTF8-Scanner impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/lang/utf8scanner.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/reader/filereader.h"
#include "C-kern/api/string/stringstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: utf8scanner_t

// group: lifetime

int init_utf8scanner(/*out*/utf8scanner_t * scan)
{
   scan->next = 0 ;
   scan->end  = 0 ;
   scan->scanned_token = (splitstring_t) splitstring_INIT_FREEABLE ;

   return 0 ;
}

int free_utf8scanner(utf8scanner_t * scan, filereader_t * frd)
{
   int err = 0, err2 ;

   if (scan->end) {
      // buffer is acquired
      err = release_filereader(frd) ;
      if (2 == nrofparts_splitstring(&scan->scanned_token)) {
         // release second also
         err2 = release_filereader(frd) ;
         if (err2) err = err2 ;
      }
   }

   *scan = (utf8scanner_t) utf8scanner_INIT_FREEABLE ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

bool isfree_utf8scanner(const utf8scanner_t * scan)
{
   return   scan->next == 0
            && scan->end == 0
            && isfree_splitstring(&scan->scanned_token) ;
}

// group: read

int readbuffer_utf8scanner(utf8scanner_t * scan, filereader_t * frd)
{
   int err ;

   if (scan->next < scan->end)   return 0 ;        // buffer not empty
   if (iseof_filereader(frd))    return ENODATA ;  // no more data
   if (ioerror_filereader(frd))  return ioerror_filereader(frd) ; // do not log ioerror twice

   if (0 == nrofparts_splitstring(&scan->scanned_token)) {
      if (scan->next) {
         scan->next = 0 ;
         scan->end  = 0 ;   // mark buffer as released
         err = release_filereader(frd) ;
         if (err) goto ONABORT ;
      }

   } else if (1 == nrofparts_splitstring(&scan->scanned_token)) {
      size_t strsize = (size_t) (scan->end - addr_splitstring(&scan->scanned_token, 0)) ;
      setsize_splitstring(&scan->scanned_token, 0, strsize) ;

   } else {
      // splitstring only supports two buffers
      err = ENOBUFS ;
      goto ONABORT ;
   }

   err = acquirenext_filereader(frd, genericcast_stringstream(scan)) ;
   if (err) goto ONABORT ;

   if (nrofparts_splitstring(&scan->scanned_token)) {
      setnrofparts_splitstring(&scan->scanned_token, 2) ;
      setpart_splitstring(&scan->scanned_token, 1, 0, scan->next) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(directory_t * tempdir)
{
   utf8scanner_t scan     = utf8scanner_INIT_FREEABLE ;
   filereader_t   freader = filereader_INIT_FREEABLE ;
   const size_t   B       = buffersize_filereader() ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "init", 2*B)) ;

   // TEST utf8scanner_INIT_FREEABLE
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;

   // TEST init_utf8scanner, free_utf8scanner: two buffers acuired and released
   memset(&scan, 266 ,sizeof(scan)) ;
   TEST(0 == init_filereader(&freader, "init", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;
   uint8_t oldfree = freader.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;
   scan.next = scan.end ;
   setnrofparts_splitstring(&scan.scanned_token, 1/*simulate token points to buffer*/) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+2) ;                    // two buffers acquired
   TEST(2 == nrofparts_splitstring(&scan.scanned_token)) ; // two buffers acquired
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer/*both buffers are released*/) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST free_utf8scanner: no buffer released if scan.next == 0 (that means all buffer already relased or never acquired)
   TEST(0 == init_filereader(&freader, "init", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   oldfree = freader.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;
   scan.next = scan.end = 0 ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1/*nothing released*/) ;
   scan.next = scan.end = (const void*)1 ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer/*released!*/) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST free_utf8scanner: releases only one buffer if nrofparts_splitstring() == 1
   TEST(0 == init_filereader(&freader, "init", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   oldfree = freader.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;
   scan.next = scan.end ;
   setnrofparts_splitstring(&scan.scanned_token, 1/*simulate token points to buffer*/) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+2) ;
   setnrofparts_splitstring(&scan.scanned_token, 1/*simulate only one buffer acquired*/) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1/*only one buffer released*/) ;
   TEST(0 == free_filereader(&freader)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "init")) ;

   return 0 ;
ONABORT:
   (void) free_filereader(&freader) ;
   (void) removefile_directory(tempdir, "init") ;
   return EINVAL ;
}

static int test_query(void)
{
   utf8scanner_t scan = utf8scanner_INIT_FREEABLE ;

   // TEST isfree_utf8scanner
   TEST(isfree_utf8scanner(&scan)) ;
   for (size_t i = 0; i < sizeof(scan); ++i) {
      memset(i + (uint8_t*)&scan, 1, 1) ;
      TEST(! isfree_utf8scanner(&scan)) ;
      memset(i + (uint8_t*)&scan, 0, 1) ;
      TEST(isfree_utf8scanner(&scan)) ;
   }

   // TEST isnext_utf8scanner
   scan.end = scan.next + 1 ;
   TEST(1 == isnext_utf8scanner(&scan)) ;
   scan.end = scan.next = (const uint8_t*)(uintptr_t)-1 ;
   TEST(0 == isnext_utf8scanner(&scan)) ;
   scan.end = scan.next = 0 ;
   TEST(0 == isnext_utf8scanner(&scan)) ;

   // TEST scannedtoken_utf8scanner
   TEST(scannedtoken_utf8scanner(&scan)   == &scan.scanned_token) ;
   utf8scanner_t * nullptr = 0 ;
   TEST(scannedtoken_utf8scanner(nullptr) == (void*)offsetof(utf8scanner_t, scanned_token)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_read(directory_t * tempdir)
{
   utf8scanner_t  scan    = utf8scanner_INIT_FREEABLE ;
   memblock_t     mem     = memblock_INIT_FREEABLE ;
   filereader_t   freader = filereader_INIT_FREEABLE ;
   const size_t   MS      = 2*buffersize_filereader()+123;
   stringstream_t buffer ;
   const uint8_t  * addr ;

   // prepare
   TEST(0 == RESIZE_MM(MS, &mem)) ;
   for (size_t i = 0; i < MS; ++i) {
      addr_memblock(&mem)[i] = (uint8_t)(29*i) ;
   }
   TEST(0 == save_file("read", MS, addr_memblock(&mem), tempdir)) ;
   FREE_MM(&mem) ;

   // TEST cleartoken_utf8scanner
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   uint8_t oldfree = freader.nrfreebuffer ;
   TEST(0 == acquirenext_filereader(&freader, &buffer)) ;
   addr = (const uint8_t*) "1234" ;
   scan.next = addr + 1 ;
   scan.end  = addr + 4 ;
   setaddr_splitstring(scannedtoken_utf8scanner(&scan), 0, addr) ;
   setsize_splitstring(scannedtoken_utf8scanner(&scan), 0, 1) ;
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 0) ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == 1 + freader.nrfreebuffer) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 0) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0) == addr) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0) == 1) ;
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 1) ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == 1 + freader.nrfreebuffer) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 0) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0) == addr) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0) == 1) ;

   // TEST cleartoken_utf8scanner: frees buffer in case two are acquired
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 2) ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer/*freed, no buffer loaded cause buffer not empty*/) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 0) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0) == addr) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0) == 1) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST settokenstart_utf8scanner
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   oldfree = freader.nrfreebuffer ;
   TEST(0 == acquirenext_filereader(&freader, &buffer)) ;
   addr = (const uint8_t*) "1234" ;
   scan.next = addr + 1 ;
   scan.end  = addr + 4 ;
   setaddr_splitstring(scannedtoken_utf8scanner(&scan), 0, 0) ;
   setsize_splitstring(scannedtoken_utf8scanner(&scan), 0, 12) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 0) ;
   TEST(0 == settokenstart_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == 1 + freader.nrfreebuffer/*freed, no buffer loaded cause buffer not empty*/) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 1) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0) == addr) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0) == 0) ;
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 1) ;
   TEST(0 == settokenstart_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == 1 + freader.nrfreebuffer) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 1) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0) == addr) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0) == 0) ;

   // TEST settokenstart_utf8scanner: frees buffer in case two are acquired
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 2) ;
   setsize_splitstring(scannedtoken_utf8scanner(&scan), 0, 12) ;
   scan.next = addr + 2 ;
   TEST(0 == settokenstart_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer/*freed, no buffer loaded cause buffer not empty*/) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 1) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0) == addr + 1) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0) == 0) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST settokenend_utf8scanner
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == acquirenext_filereader(&freader, &buffer)) ;
   addr      = buffer.next ;
   scan.next = buffer.next + 1 ;
   scan.end  = buffer.end ;
   TEST(0 == settokenstart_utf8scanner(&scan, &freader))
   scan.next = addr + (scan.end - addr) / 4 ;
   // nrofparts_splitstring == 1
   settokenend_utf8scanner(&scan) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 1) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0)   == addr) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0)   == (size_t)(scan.end - addr) / 4) ;
   setpart_splitstring(scannedtoken_utf8scanner(&scan), 1, 0, scan.next-5) ;
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 2) ;
   // nrofparts_splitstring == 2
   settokenend_utf8scanner(&scan) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 2) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0)   == addr) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0)   == (size_t)(scan.end - addr) / 4) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 1)   == scan.next-5) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 1)   == 5) ;
   TEST(scan.next == addr + (scan.end - addr) / 4) ;
   // nrofparts_splitstring == 0 ==> does nothing
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 0) ;
   settokenend_utf8scanner(&scan) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 0) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0)   == addr) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0)   == (size_t)(scan.end - addr) / 4) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 1)   == scan.next-5) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 1)   == 5) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST nextbyte_utf8scanner, peekbyte_utf8scanner, skipbyte_utf8scanner
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   for (size_t i = 0; scan.next < scan.end; ++i) {
      const uint8_t * next = scan.next ;
      TEST(peekbyte_utf8scanner(&scan) == (uint8_t)(29*i)) ;
      TEST(scan.next == next) ;
      TEST(nextbyte_utf8scanner(&scan) == (uint8_t)(29*i)) ;
      TEST(scan.next == next+1) ;
      scan.next = next ;
      skipbyte_utf8scanner(&scan) ;
      TEST(scan.next == next+1) ;
   }

   // TEST nextbyte_utf8scanner, peekbyte_utf8scanner, skipbyte_utf8scanner: does not check for end of buffer
   scan.next = -- scan.end ;
   TEST(peekbyte_utf8scanner(&scan) == *scan.end) ;
   TEST(scan.next == scan.end) ;
   TEST(nextbyte_utf8scanner(&scan) == *scan.end) ;
   TEST(scan.next == (++ scan.end)) ;
   scan.next = -- scan.end ;
   skipbyte_utf8scanner(&scan) ;
   TEST(scan.next == (++ scan.end)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST readbuffer_utf8scanner
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   oldfree = freader.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1/*loads buffer*/) ;
   TEST(0 != scan.next) ;
   TEST(scan.end == scan.next + buffersize_filereader()/2) ;
   // does nothing if buffer not empty
   addr = scan.next ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1/*loads buffer*/) ;
   TEST(scan.next == addr) ;
   TEST(scan.end  == addr + buffersize_filereader()/2) ;
   // does nothing if ioerror
   addr = scan.next ;
   scan.next = scan.end ;
   freader.ioerror = EIO ;
   TEST(EIO == readbuffer_utf8scanner(&scan, &freader)) ;
   freader.ioerror = 0 ;
   // loads next if buffer empty
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1/*old buffer released*/) ;
   TEST(scan.next == addr + buffersize_filereader()/2/*second buffer comes after first (implemented in filereader)*/) ;
   TEST(scan.end  == addr + buffersize_filereader()) ;
   // loads next if buffer empty and does not free old if beginscan_utf8scanner called
   setaddr_splitstring(&scan.scanned_token, 0, scan.next+1) ;
   setsize_splitstring(&scan.scanned_token, 0, 0) ;
   setaddr_splitstring(&scan.scanned_token, 1, 0) ;
   setsize_splitstring(&scan.scanned_token, 1, 1) ;
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 1/*simulates beginscan_utf8scanner*/) ;
   scan.next = scan.end ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+2/*old buffer not released*/) ;
   TEST(scan.next == addr /*third buffer is the first again (implemented in filereader)*/) ;
   TEST(scan.end  == addr + buffersize_filereader()/2) ;
   // size of first string of scanned_token is set and addr and size of second string of scanned_token
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 0) == addr+buffersize_filereader()/2+1) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 0) == buffersize_filereader()/2-1) ;
   TEST(addr_splitstring(scannedtoken_utf8scanner(&scan), 1) == scan.next) ;
   TEST(size_splitstring(scannedtoken_utf8scanner(&scan), 1) == 0) ;
   TEST(nrofparts_splitstring(scannedtoken_utf8scanner(&scan)) == 2) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST readbuffer_utf8scanner: release_filereader is not called after initialization
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   oldfree = freader.nrfreebuffer ;
   stringstream_t dummy ;
   TEST(0 == acquirenext_filereader(&freader, &dummy)) ;
   TEST(oldfree == freader.nrfreebuffer+1/*loads buffer*/) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+2/*loads buffer without releasing the first time*/) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // readbuffer_utf8scanner: ENOBUFS (utf8scanner(splitstring) supports only 2 buffers)
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   oldfree = freader.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   scan.next = scan.end ;  // simulate all bytes read from buffer
   setnrofparts_splitstring(scannedtoken_utf8scanner(&scan), 1/*simulates token points to buffer*/) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+2/*loads 2 buffer*/) ;
   scan.next = scan.end ;  // simulate all bytes read from buffer
   TEST(2 == nrofparts_splitstring(scannedtoken_utf8scanner(&scan))) ;
   TEST(ENOBUFS == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;


   // TEST readbuffer_utf8scanner, nextbyte_utf8scanner, peekbyte_utf8scanner: read whole file
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   for (size_t i = 0, b = 0; i < MS; ) {
      TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
      for (; isnext_utf8scanner(&scan); ++i) {
         TEST(peekbyte_utf8scanner(&scan) == (uint8_t)(29*i)) ;
         TEST(nextbyte_utf8scanner(&scan) == (uint8_t)(29*i)) ;
      }
      b += i == MS ? 123 : buffersize_filereader()/2 ;
      TEST(i == b) ;
   }

   // TEST readbuffer_utf8scanner: ENODATA
   TEST(ENODATA == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "read")) ;

   return 0 ;
ONABORT:
   (void) free_utf8scanner(&scan, &freader) ;
   (void) free_filereader(&freader) ;
   (void) FREE_MM(&mem) ;
   return EINVAL ;
}

int unittest_lang_utf8scanner()
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;
   directory_t     * tempdir = 0 ;
   cstring_t         tmppath = cstring_INIT ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == newtemp_directory(&tempdir, "utf8scanner", &tmppath)) ;

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
