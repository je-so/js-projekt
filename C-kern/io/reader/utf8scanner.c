/* title: UTF8-Scanner impl

   Implements <UTF8-Scanner>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/reader/utf8scanner.h
    Header file <UTF8-Scanner>.

   file: C-kern/io/reader/utf8scanner.c
    Implementation file <UTF8-Scanner impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/reader/utf8scanner.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/reader/filereader.h"
#include "C-kern/api/string/stringstream.h"
#include "C-kern/api/string/utf8.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: utf8scanner_t

// group: lifetime

int init_utf8scanner(/*out*/utf8scanner_t * scan)
{
   scan->next = 0 ;
   scan->end  = 0 ;
   scan->scanned_token = (splitstring_t) splitstring_FREE ;

   return 0 ;
}

int free_utf8scanner(utf8scanner_t * scan, filereader_t * frd)
{
   if (nrofparts_splitstring(&scan->scanned_token)) {
      // buffer is acquired
      if (2 == nrofparts_splitstring(&scan->scanned_token)) {
         // release second also
         release_filereader(frd) ;
      }
      release_filereader(frd) ;
   }

   *scan = (utf8scanner_t) utf8scanner_FREE ;

   return 0 ;
}

// group: query

bool isfree_utf8scanner(const utf8scanner_t * scan)
{
   return   scan->next == 0
            && scan->end == 0
            && isfree_splitstring(&scan->scanned_token) ;
}

const splitstring_t * scannedtoken_utf8scanner(utf8scanner_t * scan)
{
   uint8_t stridx = nrofparts_splitstring(&scan->scanned_token) ;

   if (stridx) {
      /* token is valid */
      -- stridx ;
      const uint8_t * straddr = addr_splitstring(&scan->scanned_token, stridx) ;
      setsize_splitstring(&scan->scanned_token, stridx, (size_t) (scan->next - straddr)) ;
   }

   return (const splitstring_t *) &scan->scanned_token ;
}

// group: read

int nextchar_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd, /*out*/char32_t * uchar)
{
   int err ;

   size_t size = sizeunread_utf8scanner(scan) ;
   if (! size) {
      err = readbuffer_utf8scanner(scan, frd) ;
      if (err) goto ONERR;
      size = sizeunread_utf8scanner(scan) ;
      // size > 0 is guaranteed
   }

   uint8_t chrsize ;
   if (  size >= maxsize_utf8()
         || size >= (chrsize = sizePfirst_utf8(scan->next[0])) ) {
      chrsize = decodechar_utf8(scan->next, uchar) ;
      scan->next += chrsize ;
      if (0 == chrsize) {
         ++ scan->next ;   // skip illegal (should never occur)
         err = EILSEQ ;
         goto ONERR;
      }

   } else {
      size_t nrmissing = (chrsize - size) ;
      uint8_t mbsbuf[maxsize_utf8()] ;
      memcpy(mbsbuf, scan->next, size) ;
      scan->next = scan->end ;      // consume buffer
      err = readbuffer_utf8scanner(scan, frd) ;
      if (err) {
         if (err == ENODATA) err = EILSEQ ; // should never occur
         goto ONERR;
      }
      size_t size2 = sizeunread_utf8scanner(scan) ;
      if (size2 < nrmissing) {
         scan->next = scan->end ;   // skip illegal (should never occur)
         err = EILSEQ ;
         goto ONERR;
      }
      memcpy(mbsbuf+size, scan->next, nrmissing) ;
      scan->next += nrmissing ;
      (void) decodechar_utf8(mbsbuf, uchar) ;   // always works (first byte ok)
   }

   return 0 ;
ONERR:
   if (  err != ENODATA
         && err != ENOBUFS) {
      TRACEEXIT_ERRLOG(err);
   }
   return err ;
}

int skipuntilafter_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd, char32_t uchar)
{
   int err ;

   uint8_t utf8buf[maxsize_utf8()] ;
   uint8_t utf8len   = encodechar_utf8(sizeof(utf8buf), utf8buf, uchar) ;
   uint8_t nrmissing = 0 ;

   VALIDATE_INPARAM_TEST(utf8len != 0, ONERR, ) ;

   for (;;) {
      size_t size = sizeunread_utf8scanner(scan) ;
      if (! size) {
         err = readbuffer_utf8scanner(scan, frd) ;
         if (err) goto ONERR;
         size = sizeunread_utf8scanner(scan) ;
         // size > 0 is guaranteed
      }

      if (nrmissing) {
         if (size < nrmissing) {
            // if buffer is too small then ENODATA is returned in next call readbuffer_utf8scanner
            scan->next = scan->end ;
            nrmissing  = 0 ;
            continue ;
         }
         if (0 == memcmp(&utf8buf[utf8len-nrmissing], scan->next, nrmissing)) {
            scan->next += nrmissing ;
            break ; // found
         }
         scan->next += nrmissing ;
         size       -= nrmissing ;
         nrmissing = 0 ;
      }

      const uint8_t * pos = memchr(scan->next, utf8buf[0], size) ;
      if (pos) {
         size = (size_t) (scan->end - pos) ;
         if (size < utf8len) {
            scan->next = scan->end ;
            if (0 == memcmp(&utf8buf[1], &pos[1], size-1)) {
               nrmissing = (uint8_t) (utf8len - size) ;
            }
         } else {
            scan->next = pos + utf8len ;
            if (0 == memcmp(&utf8buf[1], &pos[1], (size_t)utf8len-1)) {
               break ;  // found
            }
         }
      } else {
         // not found in buffer
         scan->next = scan->end ;
      }
   }

   return 0 ;
ONERR:
   if (  err != ENODATA
         && err != ENOBUFS) {
      TRACEEXIT_ERRLOG(err);
   }
   return err ;
}

// group: buffer I/O

int cleartoken_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd)
{
   if (2 == nrofparts_splitstring(&scan->scanned_token)) {
      release_filereader(frd) ;
   }

   if (scan->next == scan->end) {
      if (nrofparts_splitstring(&scan->scanned_token)) {
         // end of buffer (empty token)
         release_filereader(frd) ;
         setnrofparts_splitstring(&scan->scanned_token, 0) ;
      }

   } else {
      // token starts at byte which will be read next
      setnrofparts_splitstring(&scan->scanned_token, 1) ;
      setstring_splitstring(&scan->scanned_token, 0, 0, scan->next) ;
   }

   return 0 ;
}

int readbuffer_utf8scanner(utf8scanner_t * scan, filereader_t * frd)
{
   int err ;

   if (scan->next < scan->end)   return 0 ;        // buffer not empty
   if (iseof_filereader(frd))    return ENODATA ;  // no more data
   if (ioerror_filereader(frd))  return ioerror_filereader(frd) ; // do not log ioerror twice
   if (2 == nrofparts_splitstring(&scan->scanned_token)) return ENOBUFS ;  // splitstring only supports two buffers

   if (nrofparts_splitstring(&scan->scanned_token)) {
      // set length of token first part (split over buffers)
      size_t strsize = (size_t) (scan->end - addr_splitstring(&scan->scanned_token, 0)) ;
      setsize_splitstring(&scan->scanned_token, 0, strsize) ;
   }

   err = readnext_filereader(frd, genericcast_stringstream(scan)) ;
   if (err) goto ONERR;

   uint8_t stridx = nrofparts_splitstring(&scan->scanned_token) ;
   setnrofparts_splitstring(&scan->scanned_token, (uint8_t) (stridx+1)) ;
   setstring_splitstring(&scan->scanned_token, stridx, 0, scan->next) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int unread_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd, uint8_t nrofchars)
{
   int err ;
   if (!nrofchars) return 0 ;

   // calc length of token
   (void) scannedtoken_utf8scanner(scan) ;

   uint8_t  firstbyte ;
   uint8_t  charsleft = nrofchars ;
   bool     isRelease = false ;
   uint8_t  stridx    = nrofparts_splitstring(&scan->scanned_token) ;
   if (!stridx) {
      err = EINVAL ;
      goto ONERR;
   }
   -- stridx ;

   size_t   size      = size_splitstring(&scan->scanned_token, stridx) ;

   do {
      do {
         if (!size) {
            if (!stridx) {
               err = EINVAL ;
               goto ONERR;
            }
            isRelease = true ;
            -- stridx ;
            size = size_splitstring(&scan->scanned_token, stridx) ;
         }
         -- size ;
         firstbyte = addr_splitstring(&scan->scanned_token, stridx)[size] ;
      } while (! isfirstbyte_utf8(firstbyte)) ;
   } while (--charsleft) ;

   if (isRelease) {
      // nrofparts_splitstring == 2 and stridx = 0
      unread_filereader(frd) ;
      scan->end = addr_splitstring(&scan->scanned_token, stridx) + size_splitstring(&scan->scanned_token, stridx) ;
   }

   scan->next = addr_splitstring(&scan->scanned_token, stridx) + size ;
   setsize_splitstring(&scan->scanned_token, stridx, size) ;
   setnrofparts_splitstring(&scan->scanned_token, (uint8_t)(stridx+1)) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(directory_t * tempdir)
{
   utf8scanner_t  scan    = utf8scanner_FREE ;
   filereader_t   freader = filereader_FREE ;
   const size_t   B       = sizebuffer_filereader() ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "init", 2*B)) ;

   // TEST utf8scanner_FREE
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;

   // TEST init_utf8scanner, free_utf8scanner: two buffers acquired and released
   memset(&scan, 255, sizeof(scan)) ;
   TEST(0 == init_filereader(&freader, "init", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;
   uint8_t oldfree = freader.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   scan.next = scan.end ; // empty buffer
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(2 == nrofparts_splitstring(&scan.scanned_token)) ;
   TEST(oldfree == freader.nrfreebuffer+2) ; // two buffers acquired
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer) ;   // both buffers released
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST free_utf8scanner: releases only one buffer if nrofparts_splitstring() == 1
   TEST(0 == init_filereader(&freader, "init", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   oldfree = freader.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;
   scan.next = scan.end ;  // empty buffer
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+2) ;
   setnrofparts_splitstring(&scan.scanned_token, 1) ; // simulate only one buffer acquired
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;          // only one buffer released
   TEST(0 == free_filereader(&freader)) ;

   // TEST free_utf8scanner: releases no buffer if nrofparts_splitstring() == 0
   TEST(0 == init_filereader(&freader, "init", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   oldfree = freader.nrfreebuffer ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;
   setnrofparts_splitstring(&scan.scanned_token, 0) ; // simulate no buffer acquired
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;          // no buffer released
   TEST(0 == free_filereader(&freader)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "init")) ;

   return 0 ;
ONERR:
   (void) free_filereader(&freader) ;
   (void) removefile_directory(tempdir, "init") ;
   return EINVAL ;
}

static int test_query(void)
{
   utf8scanner_t scan = utf8scanner_FREE ;

   // TEST isfree_utf8scanner
   TEST(1 == isfree_utf8scanner(&scan)) ;
   scan.next = (const uint8_t*)1 ;
   TEST(0 == isfree_utf8scanner(&scan)) ;
   scan.next = 0 ;
   scan.end = (const uint8_t*)1 ;
   TEST(0 == isfree_utf8scanner(&scan)) ;
   scan.end = 0 ;
   scan.scanned_token.nrofparts = 1 ;
   TEST(0 == isfree_utf8scanner(&scan)) ;
   scan.scanned_token.nrofparts = 0 ;
   TEST(1 == isfree_utf8scanner(&scan)) ;

   // TEST isnext_utf8scanner
   scan.end = scan.next + 1 ;
   TEST(1 == isnext_utf8scanner(&scan)) ;
   scan.end = scan.next = (const uint8_t*)(uintptr_t)-1 ;
   TEST(0 == isnext_utf8scanner(&scan)) ;
   scan.end = scan.next = 0 ;
   TEST(0 == isnext_utf8scanner(&scan)) ;

   // TEST sizeunread_utf8scanner
   scan.end = scan.next + 1 ;
   TEST(1 == sizeunread_utf8scanner(&scan)) ;
   scan.end = scan.next + (size_t)-1 ;
   TEST((size_t)-1 == sizeunread_utf8scanner(&scan)) ;
   scan.next = scan.end - 1 ;
   TEST(1 == sizeunread_utf8scanner(&scan)) ;
   scan.end = scan.next = 0 ;
   TEST(0 == sizeunread_utf8scanner(&scan)) ;

   // TEST scannedtoken_utf8scanner: empty token (nothing is changed)
   TEST(scannedtoken_utf8scanner(&scan) == &scan.scanned_token) ;
   TEST(1 == isfree_splitstring(&scan.scanned_token)) ;

   // TEST scannedtoken_utf8scanner: set token
   for (uint8_t i = 1; i <= 2; ++i) {
      scan.next = 200 + i + (uint8_t*)&scan ;
      scan.end  = 400 + (uint8_t*)&scan ;
      setstring_splitstring(&scan.scanned_token, 0, 100u+i, (uint8_t*)&scan) ;
      setstring_splitstring(&scan.scanned_token, 1, 100u+i, (uint8_t*)&scan) ;
      setnrofparts_splitstring(&scan.scanned_token, i) ;
      TEST(scannedtoken_utf8scanner(&scan) == &scan.scanned_token) ;
      TEST(i == nrofparts_splitstring(&scan.scanned_token)) ;
      TEST((uint8_t*)&scan == addr_splitstring(&scan.scanned_token, 0)) ;
      TEST((uint8_t*)&scan == addr_splitstring(&scan.scanned_token, 1)) ;
      TEST(200u+i == size_splitstring(&scan.scanned_token, i-1)) ;
      TEST(100u+i == size_splitstring(&scan.scanned_token, (i == 1))) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_bufferio(directory_t * tempdir)
{
   utf8scanner_t  scan    = utf8scanner_FREE ;
   filereader_t   freader = filereader_FREE ;
   const size_t   BUFSZ   = 4*sizebuffer_filereader() + 29 ;
   memblock_t     mem     = memblock_FREE ;

   // prepare
   TEST(0 == RESIZE_MM(BUFSZ, &mem)) ;
   for (size_t i = 0; i < BUFSZ; ++i) {
      addr_memblock(&mem)[i] = (uint8_t)(29*i) ;
   }
   TEST(0 == save_file("bufferio", BUFSZ, addr_memblock(&mem), tempdir)) ;

   // TEST readbuffer_utf8scanner: acquire buffer only if buffer empty
   TEST(0 == init_filereader(&freader, "bufferio", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == scan.next) ;
   TEST(0 == scan.end) ;
   uint8_t oldfree = freader.nrfreebuffer ;
   // simulate not empty
   scan.end = scan.next + 1 ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   // no buffer acquired
   TEST(0 == scan.next) ;
   TEST(scan.end == scan.next + 1) ;
   TEST(oldfree == freader.nrfreebuffer) ;
   scan.end = 0 ;

   // TEST readbuffer_utf8scanner: IO error prevents reading
   for (int i = 0; i < 10; ++i) {
      setioerror_filereader(&freader, EIO + i) ;
      TEST(EIO+i == readbuffer_utf8scanner(&scan, &freader)) ;
      TEST(0 == scan.next) ;
      TEST(0 == scan.end) ;
      TEST(oldfree == freader.nrfreebuffer) ;
   }
   setioerror_filereader(&freader, 0) ;

   // TEST readbuffer_utf8scanner: read buffer and compare content
   for (size_t i = 0; i != BUFSZ; ) {
      TEST(0 == iseof_filereader(&freader)) ;
      setstring_splitstring(&scan.scanned_token, 0, (size_t)1, (const uint8_t*)0) ;
      TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
      // acquired one buffer
      TEST(oldfree == freader.nrfreebuffer + 1) ;
      // token adapted
      TEST(1 == nrofparts_splitstring(&scan.scanned_token)) ;
      TEST(scan.next == addr_splitstring(&scan.scanned_token, 0)) ;
      TEST(0 == size_splitstring(&scan.scanned_token, 0)) ;
      // buffer valid
      TEST(0 != scan.next) ;
      TEST(scan.end > scan.next) ;
      for (const uint8_t * addr = scan.next; addr < scan.end; ++addr, ++i) {
         TEST((uint8_t)(29*i) == addr[0]) ;
      }
      // simulate cleartoken !
      setnrofparts_splitstring(&scan.scanned_token, 0) ;
      release_filereader(&freader) ;
      TEST(oldfree == freader.nrfreebuffer) ;
      // simulate empty buffer
      scan.next = scan.end ;
   }

   // TEST readbuffer_utf8scanner: ENODATA
   TEST(1 == iseof_filereader(&freader)) ;
   for (int i = 0; i < 10; ++i) {
      TEST(ENODATA == readbuffer_utf8scanner(&scan, &freader)) ;
   }
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST readbuffer_utf8scanner: token spans 2 unreleased buffers at max
   TEST(0 == init_filereader(&freader, "bufferio", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   const uint8_t * addr[4] = { 0, 0, 0, 0 } ;
   size_t bufsize = 0 ;
   for (uint8_t i = 1; i <= 2; ++i) {
      TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
      TEST(i == nrofparts_splitstring(&scan.scanned_token)) ;
      TEST(scan.next == addr_splitstring(&scan.scanned_token, i-1)) ;
      TEST(0 == size_splitstring(&scan.scanned_token, i-1)) ;
      if (addr[0]) {
         TEST(addr[0] == addr_splitstring(&scan.scanned_token, 0)) ;
         TEST(bufsize == size_splitstring(&scan.scanned_token, 0)) ;
      }
      addr[i-1] = scan.next ;
      bufsize   = (size_t) (scan.end - scan.next) ;
      // simulate empty buffer
      scan.next = scan.end ;
   }

   // TEST readbuffer_utf8scanner: ENOBUFS
   TEST(2 == nrofparts_splitstring(&scan.scanned_token)) ;
   TEST(ENOBUFS == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(addr[0] == addr_splitstring(&scan.scanned_token, 0)) ;
   TEST(bufsize == size_splitstring(&scan.scanned_token, 0)) ;
   TEST(addr[1] == addr_splitstring(&scan.scanned_token, 1)) ;
   TEST(0       == size_splitstring(&scan.scanned_token, 1)) ;
   TEST(addr[1] + bufsize == scan.next) ;
   TEST(addr[1] + bufsize == scan.end) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST cleartoken_utf8scanner
   TEST(0 == init_filereader(&freader, "bufferio", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   // cleartoken_utf8scanner does nothing in case of already cleared token
   stringstream_t dummy ;
   TEST(0 == readnext_filereader(&freader, &dummy)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(oldfree == freader.nrfreebuffer+1) ;
   // empty buffer but token not clear
   setnrofparts_splitstring(&scan.scanned_token, 1) ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(0 == nrofparts_splitstring(&scan.scanned_token)) ;
   TEST(oldfree == freader.nrfreebuffer) ;   // buffer was released
   // token with 1 part
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(1 == nrofparts_splitstring(&scan.scanned_token)) ;
   while (scan.next < scan.end) {
      setsize_splitstring(&scan.scanned_token, 0, 1) ;
      TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
      TEST(1 == nrofparts_splitstring(&scan.scanned_token)) ;
      TEST(scan.next == addr_splitstring(&scan.scanned_token, 0)) ;
      TEST(0 == size_splitstring(&scan.scanned_token, 0)) ;
      ++ scan.next ;
   }
   // token with 1 part (empty buffer)
   oldfree = freader.nrfreebuffer ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(0 == nrofparts_splitstring(&scan.scanned_token)) ;
   TEST(oldfree + 1 == freader.nrfreebuffer) ;  // released buffer
   // token with 2 part
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   scan.next = scan.end ; // empty buffer
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(2 == nrofparts_splitstring(&scan.scanned_token)) ;
   ++ scan.next ;
   oldfree = freader.nrfreebuffer ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(oldfree + 1 == freader.nrfreebuffer) ;  // cleartoken released unused buffer
   TEST(1 == nrofparts_splitstring(&scan.scanned_token)) ;
   TEST(scan.next == addr_splitstring(&scan.scanned_token, 0)) ;
   TEST(0 == size_splitstring(&scan.scanned_token, 0)) ;
   // token with 2 part (empty buffer)
   scan.next = scan.end ; // simulate empty buffer
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(2 == nrofparts_splitstring(&scan.scanned_token)) ;
   scan.next = scan.end ; // simulate empty buffer
   oldfree = freader.nrfreebuffer ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(oldfree + 2 == freader.nrfreebuffer) ;  // cleartoken released two unused buffers
   TEST(0 == nrofparts_splitstring(&scan.scanned_token)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST unread_utf8scanner
   const char * testmbs[maxsize_utf8()] = { "1", "\u07ff", "\uffff", "\U0010FFFF" } ;
   for (unsigned mbslen = 1; mbslen <= maxsize_utf8(); ++ mbslen) {
      TEST(mbslen == strlen(testmbs[mbslen-1])) ;
      size_t start_offset = sizebuffer_filereader()/2 - 128 * mbslen + 1 ;
      size_t end_offset   = start_offset ;
      for (unsigned i = 0; i < 255; ++ i, end_offset += mbslen) {
         memcpy(addr_memblock(&mem) + end_offset, testmbs[mbslen-1], mbslen) ;
      }
      TEST(0 == removefile_directory(tempdir, "bufferio")) ;
      TEST(0 == save_file("bufferio", BUFSZ, addr_memblock(&mem), tempdir)) ;
      for (unsigned i = 0; i < 256; ++i) {
         if (i < 110) i += 17 ;
         if (i > 129 && i < 255-33) i += 33 ;
         TEST(0 == init_filereader(&freader, "bufferio", tempdir)) ;
         TEST(0 == init_utf8scanner(&scan)) ;
         TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
         addr[0] = scan.next ;
         addr[1] = scan.end ;
         scan.next = scan.end ;
         TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
         addr[2] = scan.next ;
         addr[3] = scan.end ;
         scan.next += end_offset - sizebuffer_filereader()/2 ;
         oldfree = freader.nrfreebuffer ;
         // unread last i characters
         TEST(0 == unread_utf8scanner(&scan, &freader, (uint8_t)i)) ;
         TEST(addr[0] == addr_splitstring(scannedtoken_utf8scanner(&scan), 0)) ;
         if (i <= (127+(mbslen==1))) {
            // second part was big enough
            TEST(2 == nrofparts_splitstring(scannedtoken_utf8scanner(&scan))) ;
            TEST(oldfree == freader.nrfreebuffer) ; // no buffer unread
            TEST(addr[1] == addr[0] + size_splitstring(scannedtoken_utf8scanner(&scan), 0)) ;
            TEST(addr[2] == addr_splitstring(scannedtoken_utf8scanner(&scan), 1)) ;
            TEST((128-i)*mbslen+1-mbslen == size_splitstring(scannedtoken_utf8scanner(&scan), 1)) ;
         } else {
            // second part was fully unread
            TEST(1 == nrofparts_splitstring(scannedtoken_utf8scanner(&scan))) ;
            TEST(oldfree == freader.nrfreebuffer - 1) ;  // buffer unread
            TEST(sizebuffer_filereader()/2-(i-128u)*mbslen-mbslen+1 == size_splitstring(scannedtoken_utf8scanner(&scan), 0)) ;
            // now readbuffer returns unread buffer
            scan.next = scan.end ;
            TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
            TEST(addr[2] == scan.next) ;
            TEST(addr[3] == scan.end) ;
         }
         TEST(0 == free_utf8scanner(&scan, &freader)) ;
         TEST(0 == free_filereader(&freader)) ;
      }
   }

   // TEST unread_utf8scanner: EINVAL
   for (unsigned mbslen = 1; mbslen <= maxsize_utf8(); ++ mbslen) {
      TEST(mbslen == strlen(testmbs[mbslen-1])) ;
      size_t start_offset = sizebuffer_filereader()/2 - 128 * mbslen + 1 ;
      size_t end_offset   = start_offset ;
      for (unsigned i = 0; i < 254; ++ i, end_offset += mbslen) {
         memcpy(addr_memblock(&mem) + end_offset, testmbs[mbslen-1], mbslen) ;
      }
      TEST(0 == removefile_directory(tempdir, "bufferio")) ;
      TEST(0 == save_file("bufferio", BUFSZ, addr_memblock(&mem), tempdir)) ;
      TEST(0 == init_filereader(&freader, "bufferio", tempdir)) ;
      TEST(0 == init_utf8scanner(&scan)) ;
      TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
      addr[0] = scan.next ;
      addr[1] = scan.end ;
      scan.next = scan.end ;
      TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
      setstring_splitstring(&scan.scanned_token, 0, sizebuffer_filereader()/2 - start_offset, addr[0] + start_offset) ;
      addr[2] = scan.next ;
      addr[3] = scan.end ;
      oldfree = freader.nrfreebuffer ;
      scan.next += end_offset - sizebuffer_filereader()/2 ;
      // error
      TEST(EINVAL == unread_utf8scanner(&scan, &freader, 255)) ;
      // nothing changed in scan !
      TEST(scan.next == addr[2] + end_offset - sizebuffer_filereader()/2) ;
      TEST(scan.end  == addr[3]) ;
      TEST(2 == nrofparts_splitstring(scannedtoken_utf8scanner(&scan))) ;
      TEST(oldfree == freader.nrfreebuffer) ; // no buffer unread
      TEST(addr[0] == addr_splitstring(scannedtoken_utf8scanner(&scan), 0) - start_offset) ;
      TEST(addr[1] == addr_splitstring(scannedtoken_utf8scanner(&scan), 0) + size_splitstring(scannedtoken_utf8scanner(&scan), 0)) ;
      TEST(addr[2] == addr_splitstring(scannedtoken_utf8scanner(&scan), 1)) ;
      TEST(126*mbslen+1 == size_splitstring(scannedtoken_utf8scanner(&scan), 1)) ;
      TEST(0 == free_utf8scanner(&scan, &freader)) ;
      TEST(0 == free_filereader(&freader)) ;
   }

   // unprepare
   TEST(0 == removefile_directory(tempdir, "bufferio")) ;
   FREE_MM(&mem) ;

   return 0 ;
ONERR:
   (void) free_filereader(&freader) ;
   (void) removefile_directory(tempdir, "bufferio") ;
   FREE_MM(&mem) ;
   return EINVAL ;
}

static int test_read(directory_t * tempdir)
{
   utf8scanner_t  scan    = utf8scanner_FREE ;
   memblock_t     mem     = memblock_FREE ;
   filereader_t   freader = filereader_FREE ;
   const size_t   BUFSZ   = 3*sizebuffer_filereader() + 123 ;
   char32_t       uchar ;

   // prepare
   TEST(0 == RESIZE_MM(BUFSZ, &mem)) ;
   for (size_t i = 0; i < BUFSZ; ++i) {
      addr_memblock(&mem)[i] = (uint8_t)(31*i) ;
   }
   TEST(0 == save_file("read", BUFSZ, addr_memblock(&mem), tempdir)) ;

   // TEST nextbyte_utf8scanner, peekbyte_utf8scanner, skipbytes_utf8scanner
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   for (size_t i = 0, unread = sizeunread_utf8scanner(&scan); i < unread; ++i) {
      const uint8_t * next = scan.next ;
      TEST(peekbyte_utf8scanner(&scan, i) == (uint8_t)(31*i)) ;
      TEST(scan.next == next) ;
      skipbytes_utf8scanner(&scan, i) ;
      TEST(scan.next == next+i) ;
      TEST(nextbyte_utf8scanner(&scan) == (uint8_t)(31*i)) ;
      TEST(scan.next == next+i+1) ;
      scan.next = next ;
   }

   // TEST nextbyte_utf8scanner, peekbyte_utf8scanner, skipbytes_utf8scanner: does not check for end of buffer
   scan.next = -- scan.end ;
   TEST(peekbyte_utf8scanner(&scan, 0) == *scan.end) ;
   TEST(scan.next == scan.end) ;
   TEST(nextbyte_utf8scanner(&scan) == *scan.end) ;
   TEST(scan.next == (++ scan.end)) ;
   scan.next = -- scan.end ;
   skipbytes_utf8scanner(&scan, 1) ;
   TEST(scan.next == (++ scan.end)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST peekbyte_utf8scanner, skipbytes_utf8scanner: read whole file
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   for (size_t i = 0, b = 0; i < BUFSZ; ) {
      TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
      for (size_t off = 0, unread = sizeunread_utf8scanner(&scan); off < unread; ++off, ++i) {
         TEST(peekbyte_utf8scanner(&scan, off) == (uint8_t)(31*i)) ;
      }
      skipbytes_utf8scanner(&scan, sizeunread_utf8scanner(&scan)) ;
      TEST(1 == nrofparts_splitstring(scannedtoken_utf8scanner(&scan))) ;
      TEST((i-b) == size_splitstring(scannedtoken_utf8scanner(&scan), 0)) ;
      cleartoken_utf8scanner(&scan, &freader) ;
      b += i == BUFSZ ? 123 : sizebuffer_filereader()/2 ;
      TEST(i == b) ;
   }
   TEST(1 == iseof_filereader(&freader)) ;
   TEST(ENODATA == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST peekbyte_utf8scanner, nextbyte_utf8scanner: read whole file
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   for (size_t i = 0, b = 0; i < BUFSZ; ) {
      TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
      for (; isnext_utf8scanner(&scan); ++i) {
         TEST(peekbyte_utf8scanner(&scan, 0) == (uint8_t)(31*i)) ;
         TEST(nextbyte_utf8scanner(&scan) == (uint8_t)(31*i)) ;
      }
      TEST(1 == nrofparts_splitstring(scannedtoken_utf8scanner(&scan))) ;
      TEST((i-b) == size_splitstring(scannedtoken_utf8scanner(&scan), 0)) ;
      cleartoken_utf8scanner(&scan, &freader) ;
      b += i == BUFSZ ? 123 : sizebuffer_filereader()/2 ;
      TEST(i == b) ;
   }
   TEST(1 == iseof_filereader(&freader)) ;
   TEST(ENODATA == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST nextchar_utf8scanner
   for (size_t mbslen = 1; mbslen <= maxsize_utf8(); ++mbslen) {
      memset(addr_memblock(&mem), 0, 7) ;
      size_t offset = 7 ;
      for (size_t i = 0; offset < BUFSZ-maxsize_utf8(); ++i, offset += mbslen) {
         switch (mbslen) {
         case 1:  uchar = (i & 0x7f) ;      break ;
         case 2:  uchar = 0x80 + i % (0x800-0x80) ; break ;
         case 3:  uchar = (0x800 + i) ;     break ;
         case 4:  uchar = (0x10000 + i) ;   break ;
         default: TEST(0) ; break ;
         }
         uint8_t chrlen = encodechar_utf8(maxsize_utf8(), addr_memblock(&mem)+offset, uchar) ;
         TEST(chrlen == mbslen) ;
      }
      memset(addr_memblock(&mem)+offset, 0, BUFSZ-offset) ;
      TEST(0 == removefile_directory(tempdir, "read")) ;
      TEST(0 == save_file("read", BUFSZ, addr_memblock(&mem), tempdir)) ;
      // decode all characters
      TEST(0 == init_filereader(&freader, "read", tempdir)) ;
      TEST(0 == init_utf8scanner(&scan)) ;
      size_t end_offset = offset ;
      for (offset = 0; offset < 7; ++offset) {
         uchar = 1 ;
         TEST(0 == nextchar_utf8scanner(&scan, &freader, &uchar))
         TEST(0 == uchar) ;
         TEST(sizeunread_utf8scanner(&scan) == sizebuffer_filereader()/2-offset-1) ;
      }
      for (size_t i = 0; offset < end_offset; ++i, offset += mbslen) {
         char32_t chr = 0 ;
         size_t   unread = sizeunread_utf8scanner(&scan) ;
         if (unread < mbslen) {
            unread += (offset + sizebuffer_filereader()/2 > end_offset)
                    ? 123
                    : sizebuffer_filereader()/2 ;
         }
         unread -= mbslen ;
         uchar = 0 ;
         TEST(0 == nextchar_utf8scanner(&scan, &freader, &uchar))
         cleartoken_utf8scanner(&scan, &freader) ;
         switch (mbslen) {
         case 1:  chr = (i & 0x7f) ;      break ;
         case 2:  chr = 0x80 + i % (0x800-0x80) ; break ;
         case 3:  chr = (0x800 + i) ;     break ;
         case 4:  chr = (0x10000 + i) ;   break ;
         }
         TEST(unread == sizeunread_utf8scanner(&scan)) ;
         TEST(chr == uchar) ;
      }
      TEST(sizeunread_utf8scanner(&scan) == BUFSZ-end_offset) ;
      // decode 0 characters
      for (; offset < BUFSZ; ++offset) {
         TEST(0 == nextchar_utf8scanner(&scan, &freader, &uchar))
         TEST(0 == uchar) ;
         TEST(sizeunread_utf8scanner(&scan) == BUFSZ-offset-1) ;
      }
      TEST(0 == sizeunread_utf8scanner(&scan)) ;
      // ENODATA
      TEST(ENODATA == nextchar_utf8scanner(&scan, &freader, &uchar)) ;
      TEST(0 == free_utf8scanner(&scan, &freader)) ;
      TEST(0 == free_filereader(&freader)) ;
   }

   // TEST nextchar_utf8scanner: ENOBUFS
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   scan.next = scan.end ; // empty buffer
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   scan.next = scan.end ; // empty buffer
   TEST(ENOBUFS == nextchar_utf8scanner(&scan, &freader, &uchar)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST nextchar_utf8scanner: EILSEQ
   memset(addr_memblock(&mem), 0, sizebuffer_filereader()+1) ;
   for (unsigned i = 0; i <= 10; ++i) {
      addr_memblock(&mem)[i] = (uint8_t) (0xf5 + i) ;  // illegal first byte (must be skipped)
   }
   addr_memblock(&mem)[11] = (uint8_t) 240 ;    // binary constants allowed in C11
   addr_memblock(&mem)[12] = (uint8_t) __extension__ 0b10111111 ;
   addr_memblock(&mem)[13] = (uint8_t) __extension__ 0b10111111 ;
   addr_memblock(&mem)[14] = (uint8_t) 0 ; // illegal follow byte is ignored
   addr_memblock(&mem)[sizebuffer_filereader()/2-1] = (uint8_t) __extension__ 0b11011111 ;
   addr_memblock(&mem)[sizebuffer_filereader()/2]   = (uint8_t) 0 ; // illegal follow byte ignored
   addr_memblock(&mem)[sizebuffer_filereader()-1]   = 240 ;
   addr_memblock(&mem)[sizebuffer_filereader()]     = (uint8_t) __extension__ 0b10111111 ; // not enough data
   TEST(0 == removefile_directory(tempdir, "read")) ;
   TEST(0 == save_file("read", sizebuffer_filereader()+1, addr_memblock(&mem), tempdir)) ;
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   for (unsigned i = 0; i <= 10; ++i) {
      TEST(EILSEQ == nextchar_utf8scanner(&scan, &freader, &uchar)) ;
      TEST(sizebuffer_filereader()/2-1-i == sizeunread_utf8scanner(&scan)) ;
   }
   // follow byte ignored
   TEST(0 == nextchar_utf8scanner(&scan, &freader, &uchar)) ;
   TEST(sizebuffer_filereader()/2-15 == sizeunread_utf8scanner(&scan)) ;
   scan.next = scan.end -1 ;
   // follow byte ignored
   TEST(1 == sizeunread_utf8scanner(&scan)) ;
   TEST(0 == nextchar_utf8scanner(&scan, &freader, &uchar)) ;
   TEST(sizebuffer_filereader()/2-1 == sizeunread_utf8scanner(&scan)) ;
   scan.next = scan.end -1 ;
   TEST(1 == sizeunread_utf8scanner(&scan)) ;
   cleartoken_utf8scanner(&scan, &freader) ;
   // not enough data at end of file (but next buffer contains at least a byte)
   TEST(EILSEQ == nextchar_utf8scanner(&scan, &freader, &uchar)) ;
   TEST(1 == iseof_filereader(&freader)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST nextchar_utf8scanner: EILSEQ (read next buffer returns ENODATA)
   memset(addr_memblock(&mem), 0, 2*sizebuffer_filereader()-1) ;
   addr_memblock(&mem)[2*sizebuffer_filereader()-1] = 240 ;
   TEST(0 == removefile_directory(tempdir, "read")) ;
   TEST(0 == save_file("read", 2*sizebuffer_filereader(), addr_memblock(&mem), tempdir)) ;
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   for (unsigned i = 0; i < 4; ++i) {
      scan.next = scan.end ;
      TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
      cleartoken_utf8scanner(&scan, &freader) ;
   }
   scan.next = scan.end -1 ;
   TEST(1 == sizeunread_utf8scanner(&scan)) ;
   // not enough data at end of file (next buffer contains no data (ENODATA))
   TEST(EILSEQ == nextchar_utf8scanner(&scan, &freader, &uchar)) ;
   TEST(1 == iseof_filereader(&freader)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST skipuntilafter_utf8scanner
   for (size_t mbslen = 1; mbslen <= maxsize_utf8(); ++mbslen) {
      memset(addr_memblock(&mem), 0, 3) ;
      size_t offset = 3 ;
      for (size_t i = 0; offset <= BUFSZ-mbslen; ++i, offset += mbslen) {
         switch (mbslen) {
         case 1:  uchar = i & 0x7f ;      break ;
         case 2:  uchar = 0x80 + i % (0x800-0x80) ; break ;
         case 3:  uchar = 0x800 + i ;     break ;
         case 4:  uchar = 0x10000 + i ;   break ;
         default: TEST(0) ; break ;
         }
         uint8_t chrlen = encodechar_utf8(mbslen, addr_memblock(&mem)+offset, uchar) ;
         TEST(chrlen == mbslen) ;
      }
      memset(addr_memblock(&mem)+offset, 0, BUFSZ-offset) ;
      TEST(0 == removefile_directory(tempdir, "read")) ;
      TEST(0 == save_file("read", BUFSZ, addr_memblock(&mem), tempdir)) ;
      TEST(0 == init_filereader(&freader, "read", tempdir)) ;
      TEST(0 == init_utf8scanner(&scan)) ;
      size_t B = sizebuffer_filereader()/2 ;
      // skip 0 bytes at beginning
      for (offset = 1; offset <= 3; ++offset) {
         TEST(0 == skipuntilafter_utf8scanner(&scan, &freader, 0)) ;
         TEST(B-offset == sizeunread_utf8scanner(&scan)) ;
      }
      for (size_t i = 0, step = 1; true; i += step, offset += step*mbslen, step = 1 + step % 5) {
         switch (mbslen) {
         case 1:  uchar = i & 0x7f ;      break ;
         case 2:  uchar = 0x80 + i % (0x800-0x80) ; break ;
         case 3:  uchar = 0x800 + i ;     break ;
         case 4:  uchar = 0x10000 + i ;   break ;
         }
         if (offset > BUFSZ-mbslen) break ;
         TEST(0 == skipuntilafter_utf8scanner(&scan, &freader, uchar)) ;
         ++ i ;
         switch (mbslen) {
         case 1:  uchar = i & 0x7f ;      break ;
         case 2:  uchar = 0x80 + i % (0x800-0x80) ; break ;
         case 3:  uchar = 0x800 + i ;     break ;
         case 4:  uchar = 0x10000 + i ;   break ;
         }
         -- i ;
         char32_t uchar2 = 0 ;
         TEST(0 == nextchar_utf8scanner(&scan, &freader, &uchar2)) ;
         TEST(uchar2 == uchar) ;
         TEST(0 == unread_utf8scanner(&scan, &freader, 1)) ;
         TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
      }
      // ENODATA
      TEST(ENODATA == skipuntilafter_utf8scanner(&scan, &freader, uchar)) ;
      TEST(0 == free_utf8scanner(&scan, &freader)) ;
      TEST(0 == free_filereader(&freader)) ;
   }

   // TEST skipuntilafter_utf8scanner: EINVAL
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(EINVAL == skipuntilafter_utf8scanner(&scan, &freader, maxchar_utf8()+1)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST skipuntilafter_utf8scanner: ENOBUFS
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   scan.next = scan.end ; // empty buffer
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   scan.next = scan.end ; // empty buffer
   TEST(ENOBUFS == skipuntilafter_utf8scanner(&scan, &freader, 0)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST skipuntilafter_utf8scanner: illegal sequences are ignored
   size_t B = sizebuffer_filereader()/2 ;
   memset(addr_memblock(&mem), 0, BUFSZ) ;
   TEST(2 == encodechar_utf8(2, addr_memblock(&mem)+B-1, 0x80)) ;
   addr_memblock(&mem)[B] = 0 ;     // ignored
   addr_memblock(&mem)[B+1] = 0x80 ; // ignored
   TEST(3 == encodechar_utf8(3, addr_memblock(&mem)+2*B-2, 0x800)) ;
   addr_memblock(&mem)[2*B] = 0 ;      // ignored
   addr_memblock(&mem)[3*B-4] = 0x80 ; // ignored
   TEST(4 == encodechar_utf8(4, addr_memblock(&mem)+3*B-3, 0x10000)) ;
   addr_memblock(&mem)[3*B] = 0 ;   // ignored
   TEST(4 == encodechar_utf8(4, addr_memblock(&mem)+3*B+1, 0x10000)) ;
   addr_memblock(&mem)[3*B+4] = 0 ; // ignored
   addr_memblock(&mem)[4*B-1] = addr_memblock(&mem)[B-1] ;   // ENODATA
   TEST(0 == removefile_directory(tempdir, "read")) ;
   TEST(0 == save_file("read", 4*B, addr_memblock(&mem), tempdir)) ;
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(ENOBUFS == skipuntilafter_utf8scanner(&scan, &freader, maxchar_utf8())) ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(ENODATA == skipuntilafter_utf8scanner(&scan, &freader, 0x80)) ;
   TEST(iseof_filereader(&freader)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // TEST skipuntilafter_utf8scanner: EILSEQ (last buffer contains only 1 byte)
   memset(addr_memblock(&mem), 0, 2*B+1) ;
   TEST(4 == encodechar_utf8(4, addr_memblock(&mem)+2*B-1, 0x10000)) ;
   TEST(0 == removefile_directory(tempdir, "read")) ;
   TEST(0 == save_file("read", 2*B+1, addr_memblock(&mem), tempdir)) ;
   TEST(0 == init_filereader(&freader, "read", tempdir)) ;
   TEST(0 == init_utf8scanner(&scan)) ;
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   scan.next = scan.end ; // empty buffer
   TEST(0 == readbuffer_utf8scanner(&scan, &freader)) ;
   TEST(0 == cleartoken_utf8scanner(&scan, &freader)) ;
   TEST(ENODATA == skipuntilafter_utf8scanner(&scan, &freader, 0x10000)) ;
   TEST(iseof_filereader(&freader)) ;
   TEST(0 == free_utf8scanner(&scan, &freader)) ;
   TEST(0 == free_filereader(&freader)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "read")) ;
   FREE_MM(&mem) ;

   return 0 ;
ONERR:
   (void) free_utf8scanner(&scan, &freader) ;
   (void) free_filereader(&freader) ;
   (void) FREE_MM(&mem) ;
   return EINVAL ;
}

int unittest_io_reader_utf8scanner()
{
   directory_t *tempdir = 0 ;
   cstring_t   tmppath  = cstring_INIT ;

   // prepare
   TEST(0 == newtemp_directory(&tempdir, "utf8scanner")) ;
   TEST(0 == path_directory(tempdir, &(wbuffer_t)wbuffer_INIT_CSTRING(&tmppath))) ;

   if (test_initfree(tempdir))   goto ONERR;
   if (test_query())             goto ONERR;
   if (test_bufferio(tempdir))   goto ONERR;
   if (test_read(tempdir))       goto ONERR;

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
