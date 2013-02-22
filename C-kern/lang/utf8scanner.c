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
   scan->scanned_token = (splittoken_t) splittoken_INIT_FREEABLE ;

   return 0 ;
}

int free_utf8scanner(utf8scanner_t * scan, filereader_t * frd)
{
   int err = 0, err2 ;

   if (scan->next) {
      // release at least two buffers (even if only 1 is acquired, filereader ignores additional releases)
      err = release_filereader(frd) ;
      err2 = release_filereader(frd) ;
      if (err2) err = err2 ;
   }

   *scan = (utf8scanner_t) utf8scanner_INIT_FREEABLE ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: read

int beginscan_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd)
{
   int err ;

   if (2 == nrofstrings_splittoken(&scan->scanned_token)) {
      // second buffer is no more in use (beginscan indicates that scanned_token is no more in use)
      err = release_filereader(frd) ;
      if (err) goto ONABORT ;
   }

   // fill buffer and free acquired buffer if necessary
   setnrofstrings_splittoken(&scan->scanned_token, 0) ;
   err = readbuffer_utf8scanner(scan, frd) ;
   if (err) goto ONABORT ;

   setnrofstrings_splittoken(&scan->scanned_token, 1) ;
   setstringaddr_splittoken(&scan->scanned_token, 0, scan->next/*buffer filled*/) ;
   setstringsize_splittoken(&scan->scanned_token, 0, 0) ;

   return 0 ;
ONABORT:
   if (err != ENODATA) {
      TRACEABORT_LOG(err) ;
   }
   return err ;
}

int endscan_utf8scanner(utf8scanner_t * scan, uint16_t tokentype, uint8_t tokensubtype)
{
   int err ;

   // check that beginscan_utf8scanner was called before
   if (! nrofstrings_splittoken(&scan->scanned_token)) {
      err = EINVAL ;
      goto ONABORT ;
   }

   // set type and size of token
   const unsigned stridx    = nrofstrings_splittoken(&scan->scanned_token) - 1u ;
   const uint8_t  * straddr = stringaddr_splittoken(&scan->scanned_token, stridx) ;
   setstringsize_splittoken(&scan->scanned_token, stridx, (size_t)(scan->next - straddr)) ;
   settype_splittoken(&scan->scanned_token, tokentype, tokensubtype) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int readbuffer_utf8scanner(utf8scanner_t * scan, filereader_t * frd)
{
   int err ;

   if (scan->next < scan->end)   return 0 ;        // buffer not empty
   if (iseof_filereader(frd))    return ENODATA ;  // no more data
   if (ioerror_filereader(frd))  return ioerror_filereader(frd) ; // do not log ioerror twice

   if (0 == nrofstrings_splittoken(&scan->scanned_token)) {
      scan->next = 0 ;
      scan->end  = 0 ;   // mark buffer as released
      err = release_filereader(frd) ;
      if (err) goto ONABORT ;

   } else if (1 == nrofstrings_splittoken(&scan->scanned_token)) {
      size_t strsize = (size_t) (scan->end - stringaddr_splittoken(&scan->scanned_token, 0)) ;
      setstringsize_splittoken(&scan->scanned_token, 0, strsize) ;
   } else {
      // splittoken only supports two buffers
      err = ENOBUFS ;
      goto ONABORT ;
   }

   err = acquirenext_filereader(frd, genericcast_stringstream(scan)) ;
   if (err) goto ONABORT ;

   if (nrofstrings_splittoken(&scan->scanned_token)) {
      setstringaddr_splittoken(&scan->scanned_token, 1, scan->next) ;
      setstringsize_splittoken(&scan->scanned_token, 1, 0) ;
      setnrofstrings_splittoken(&scan->scanned_token, 2) ;
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
   utf8scanner_t scan = utf8scanner_INIT_FREEABLE ;
   filereader_t   frd = filereader_INIT_FREEABLE ;
   const size_t   B   = buffersize_filereader() ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "init", 2*B)) ;

   // TEST utf8scanner_INIT_FREEABLE
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splittoken(&scan.scanned_token)) ;

   // TEST init_utf8scanner, free_utf8scanner
   memset(&scan, 266 ,sizeof(scan)) ;
   TEST(0 == init_filereader(&frd, "init", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splittoken(&scan.scanned_token)) ;
   uint8_t oldfree = frd.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+1) ;
   scan.next = scan.end ;
   setnrofstrings_splittoken(&scan.scanned_token, 1) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+2) ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer/*both buffers are released*/) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splittoken(&scan.scanned_token)) ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splittoken(&scan.scanned_token)) ;
   TEST(0 == free_filereader(&frd)) ;

   // TEST free_utf8scanner: no buffer released if scan.next == 0 (that means all buffer already relased or never acquired)
   TEST(0 == init_filereader(&frd, "init", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   oldfree = frd.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+1) ;
   scan.next = 0 ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+1/*nothing released*/) ;
   scan.next = (const void*)1 ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer/*released!*/) ;
   TEST(0 == free_filereader(&frd)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "init")) ;

   return 0 ;
ONABORT:
   (void) free_filereader(&frd) ;
   (void) removefile_directory(tempdir, "init") ;
   return EINVAL ;
}

static int test_query(void)
{
   utf8scanner_t scan = utf8scanner_INIT_FREEABLE ;

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
   utf8scanner_t  scan = utf8scanner_INIT_FREEABLE ;
   memblock_t     mem  = memblock_INIT_FREEABLE ;
   filereader_t   frd  = filereader_INIT_FREEABLE ;
   const size_t   MS   = 2*buffersize_filereader()+123;
   const uint8_t  * addr ;

   // prepare
   TEST(0 == RESIZE_MM(MS, &mem)) ;
   for (size_t i = 0; i < MS; ++i) {
      addr_memblock(&mem)[i] = (uint8_t)(29*i) ;
   }
   TEST(0 == save_file("read", MS, addr_memblock(&mem), tempdir)) ;
   FREE_MM(&mem) ;

   // TEST beginscan_utf8scanner
   TEST(0 == init_filereader(&frd, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   settype_splittoken(scannedtoken_utf8scanner(&scan), 10, 11) ;
   setstringaddr_splittoken(scannedtoken_utf8scanner(&scan), 0, 0) ;
   setstringsize_splittoken(scannedtoken_utf8scanner(&scan), 0, 12) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   uint8_t oldfree = frd.nrfreebuffer ;
   TEST(0 == beginscan_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+1/*loads buffer*/) ;
   TEST(scan.next != 0) ;
   TEST(scan.end  == scan.next + buffersize_filereader()/2) ;
   TEST(nrofstrings_splittoken(scannedtoken_utf8scanner(&scan))   == 1) ;
   TEST(type_splittoken(scannedtoken_utf8scanner(&scan))          == 10) ;
   TEST(subtype_splittoken(scannedtoken_utf8scanner(&scan))       == 11) ;
   TEST(stringaddr_splittoken(scannedtoken_utf8scanner(&scan), 0) == scan.next) ;
   TEST(stringsize_splittoken(scannedtoken_utf8scanner(&scan), 0) == 0) ;

   // TEST beginscan_utf8scanner: frees buffer in case two are acquired
   setnrofstrings_splittoken(scannedtoken_utf8scanner(&scan), 2) ;
   TEST(0 == beginscan_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer/*freed, no buffer loaded cause buffer not empty*/) ;
   TEST(scan.next != 0) ;
   TEST(scan.end  == scan.next + buffersize_filereader()/2) ;
   TEST(nrofstrings_splittoken(scannedtoken_utf8scanner(&scan))   == 1) ;
   TEST(type_splittoken(scannedtoken_utf8scanner(&scan))          == 10) ;
   TEST(subtype_splittoken(scannedtoken_utf8scanner(&scan))       == 11) ;
   TEST(stringaddr_splittoken(scannedtoken_utf8scanner(&scan), 0) == scan.next) ;
   TEST(stringsize_splittoken(scannedtoken_utf8scanner(&scan), 0) == 0) ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(0 == free_filereader(&frd)) ;

   // TEST endscan_utf8scanner
   TEST(0 == init_filereader(&frd, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == beginscan_utf8scanner(&scan, &frd)) ;
   settype_splittoken(scannedtoken_utf8scanner(&scan), 0, 0) ;
   setstringaddr_splittoken(scannedtoken_utf8scanner(&scan), 1, 0) ;
   setstringsize_splittoken(scannedtoken_utf8scanner(&scan), 1, 0) ;
   addr = scan.next ;
   scan.next += buffersize_filereader()/4 ;
   TEST(0 == endscan_utf8scanner(&scan, 10, 11)) ;
   TEST(nrofstrings_splittoken(scannedtoken_utf8scanner(&scan))   == 1) ;
   TEST(type_splittoken(scannedtoken_utf8scanner(&scan))          == 10) ;
   TEST(subtype_splittoken(scannedtoken_utf8scanner(&scan))       == 11) ;
   TEST(stringaddr_splittoken(scannedtoken_utf8scanner(&scan), 0) == addr) ;
   TEST(stringsize_splittoken(scannedtoken_utf8scanner(&scan), 0) == buffersize_filereader()/4) ;
   setstringaddr_splittoken(scannedtoken_utf8scanner(&scan), 1, scan.next-5) ;
   setnrofstrings_splittoken(scannedtoken_utf8scanner(&scan), 2) ;
   TEST(0 == endscan_utf8scanner(&scan, 12, 13)) ;
   TEST(nrofstrings_splittoken(scannedtoken_utf8scanner(&scan))   == 2) ;
   TEST(type_splittoken(scannedtoken_utf8scanner(&scan))          == 12) ;
   TEST(subtype_splittoken(scannedtoken_utf8scanner(&scan))       == 13) ;
   TEST(stringaddr_splittoken(scannedtoken_utf8scanner(&scan), 0) == addr) ;
   TEST(stringsize_splittoken(scannedtoken_utf8scanner(&scan), 0) == buffersize_filereader()/4) ;
   TEST(stringaddr_splittoken(scannedtoken_utf8scanner(&scan), 1) == scan.next-5) ;
   TEST(stringsize_splittoken(scannedtoken_utf8scanner(&scan), 1) == 5) ;
   TEST(scan.next == addr + buffersize_filereader()/4) ;
   TEST(scan.end  == addr + buffersize_filereader()/2) ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(0 == free_filereader(&frd)) ;

   // TEST endscan_utf8scanner: EINVAL
   TEST(0 == init_filereader(&frd, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == beginscan_utf8scanner(&scan, &frd)) ;
   setnrofstrings_splittoken(scannedtoken_utf8scanner(&scan), 0) ;
   TEST(EINVAL == endscan_utf8scanner(&scan, 0, 0)) ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(0 == free_filereader(&frd)) ;

   // TEST nextbyte_utf8scanner, peekbyte_utf8scanner
   TEST(0 == init_filereader(&frd, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == beginscan_utf8scanner(&scan, &frd)) ;
   for (size_t i = 0; scan.next < scan.end; ++i) {
      const uint8_t * next = scan.next ;
      TEST(peekbyte_utf8scanner(&scan) == (uint8_t)(29*i)) ;
      TEST(scan.next == next) ;
      TEST(nextbyte_utf8scanner(&scan) == (uint8_t)(29*i)) ;
      TEST(scan.next == next+1) ;
   }

   // TEST nextbyte_utf8scanner, peekbyte_utf8scanner: does not check for end of buffer
   scan.next = -- scan.end ;
   TEST(peekbyte_utf8scanner(&scan) == *scan.end) ;
   TEST(scan.next == scan.end) ;
   TEST(nextbyte_utf8scanner(&scan) == *scan.end) ;
   TEST(scan.next == (++ scan.end)) ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(0 == free_filereader(&frd)) ;

   // TEST readbuffer_utf8scanner
   TEST(0 == init_filereader(&frd, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   oldfree = frd.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+1/*loads buffer*/) ;
   TEST(0 != scan.next) ;
   TEST(scan.end == scan.next + buffersize_filereader()/2) ;
   // does nothing if buffer not empty
   addr = scan.next ;
   TEST(0 == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+1/*loads buffer*/) ;
   TEST(scan.next == addr) ;
   TEST(scan.end  == addr + buffersize_filereader()/2) ;
   // does nothing if ioerror
   addr = scan.next ;
   scan.next = scan.end ;
   frd.ioerror = EIO ;
   TEST(EIO == readbuffer_utf8scanner(&scan, &frd)) ;
   frd.ioerror = 0 ;
   // loads next if buffer empty
   TEST(0 == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+1/*old buffer released*/) ;
   TEST(scan.next == addr + buffersize_filereader()/2/*second buffer comes after first (implemented in filereader)*/) ;
   TEST(scan.end  == addr + buffersize_filereader()) ;
   // loads next if buffer empty and does not free old if beginscan_utf8scanner called
   setstringaddr_splittoken(&scan.scanned_token, 0, scan.next+1) ;
   setstringsize_splittoken(&scan.scanned_token, 0, 0) ;
   setstringaddr_splittoken(&scan.scanned_token, 1, 0) ;
   setstringsize_splittoken(&scan.scanned_token, 1, 1) ;
   setnrofstrings_splittoken(scannedtoken_utf8scanner(&scan), 1/*simulates beginscan_utf8scanner*/) ;
   scan.next = scan.end ;
   TEST(0 == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(oldfree == frd.nrfreebuffer+2/*old buffer not released*/) ;
   TEST(scan.next == addr /*third buffer is the first again (implemented in filereader)*/) ;
   TEST(scan.end  == addr + buffersize_filereader()/2) ;
   // size of first string of scanned_token is set and addr and size of second string of scanned_token
   TEST(stringaddr_splittoken(scannedtoken_utf8scanner(&scan), 0) == addr+buffersize_filereader()/2+1) ;
   TEST(stringsize_splittoken(scannedtoken_utf8scanner(&scan), 0) == buffersize_filereader()/2-1) ;
   TEST(stringaddr_splittoken(scannedtoken_utf8scanner(&scan), 1) == scan.next) ;
   TEST(stringsize_splittoken(scannedtoken_utf8scanner(&scan), 1) == 0) ;
   TEST(nrofstrings_splittoken(scannedtoken_utf8scanner(&scan))   == 2) ;

   // readbuffer_utf8scanner: ENOBUFS
   scan.next = scan.end ;
   TEST(2 == nrofstrings_splittoken(scannedtoken_utf8scanner(&scan))) ;
   TEST(ENOBUFS == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(0 == free_filereader(&frd)) ;

   // TEST readbuffer_utf8scanner, nextbyte_utf8scanner, peekbyte_utf8scanner: read whole file
   TEST(0 == init_filereader(&frd, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   for (size_t i = 0, b = 0; i < MS; ) {
      TEST(0 == readbuffer_utf8scanner(&scan, &frd)) ;
      for (; isnext_utf8scanner(&scan); ++i) {
         TEST(peekbyte_utf8scanner(&scan) == (uint8_t)(29*i)) ;
         TEST(nextbyte_utf8scanner(&scan) == (uint8_t)(29*i)) ;
      }
      b += i == MS ? 123 : buffersize_filereader()/2 ;
      TEST(i == b) ;
   }

   // TEST beginscan_utf8scanner, readbuffer_utf8scanner: ENODATA
   TEST(ENODATA == beginscan_utf8scanner(&scan, &frd)) ;
   TEST(ENODATA == readbuffer_utf8scanner(&scan, &frd)) ;
   TEST(0 == free_utf8scanner(&scan, &frd)) ;
   TEST(0 == free_filereader(&frd)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "read")) ;

   return 0 ;
ONABORT:
   (void) free_utf8scanner(&scan, &frd) ;
   (void) free_filereader(&frd) ;
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
