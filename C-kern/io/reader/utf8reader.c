/* title: UTF8TextReader impl

   Implements <UTF8TextReader>.

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

   file: C-kern/api/io/reader/utf8reader.h
    Header file <UTF8TextReader>.

   file: C-kern/io/reader/utf8reader.c
    Implementation file <UTF8TextReader impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/reader/utf8reader.h"
#include "C-kern/api/err.h"
#include "C-kern/api/string/string.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: utf8reader_t

static inline void compiletime_assert(void)
{
   conststring_t     cnststr ;
   utf8reader_t      utfread ;
   struct {
      const uint8_t  * addr ;
      size_t         size ;
   }                 dummy_size ;

   // TEST conststring_t has only members: addr & size
   static_assert(sizeof(dummy_size)   == sizeof(cnststr), "considered all members in conststring_t") ;

   // TEST utf8reader_t.next & utf8reader_t.size compatible with conststring_t.addr & conststring_t.size
   static_assert(offsetof(utf8reader_t, next) == offsetof(conststring_t, addr), "utf8reader_t compatible with conststring_t") ;
   static_assert(sizeof(utfread.next) == sizeof(cnststr.addr), "utf8reader_t compatible with conststring_t") ;
   static_assert(offsetof(utf8reader_t, size) == offsetof(conststring_t, size), "utf8reader_t compatible with conststring_t") ;
   static_assert(sizeof(utfread.size) == sizeof(cnststr.size), "utf8reader_t compatible with conststring_t") ;
}

// group: read

bool skipline_utf8reader(utf8reader_t * utfread)
{
   uint32_t uchar ;
   size_t         size   = utfread->size ;
   const uint8_t  * next = utfread->next ;
   union {
      uint32_t word ;
      uint8_t  bytes[4] ;
   }              data ;

   if (size > 16) {

      switch (((uintptr_t)next) % 4) {
      case 1:     --size ; if ('\n' == *next++) break ;
      case 2:     --size ; if ('\n' == *next++) break ;
      case 3:     --size ; if ('\n' == *next++) break ;
      default:    for (; size >= 4; size -= 4, next += 4) {
                     /* if data.bytes[X] is '\n' it is set to 0 else to a value != 0 */
                     data.word = 0x0A0A0A0A ^ *(const uint32_t*)next ;
                     /* if data.bytes[X] is 0 it will be set to 0x80 else to 0 */
                     data.word = ((data.word - 0x01010101) & ~data.word) & 0x80808080 ;
                     if (data.word) {
                        --size ; ++ next ; if (data.bytes[0]) goto FOUND_NEWLINE ;
                        --size ; ++ next ; if (data.bytes[1]) goto FOUND_NEWLINE ;
                        --size ; ++ next ; if (data.bytes[2]) goto FOUND_NEWLINE ;
                        --size ; ++ next ; goto FOUND_NEWLINE ;
                     }
                  }

                  goto SIMPLE_LOOP ;
      }

      FOUND_NEWLINE:

      utfread->next = next ;
      utfread->size = size ;
      ++ utfread->linenr ;
      utfread->colnr = 0 ;

   } else {

      if (!size) return false ;

      SIMPLE_LOOP:

      while (nextchar_utf8reader(utfread, &uchar)) {
         if ('\n' == uchar) {
            break ;
         }
      }

   }

   return true ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   utf8reader_t   utfread = utf8reader_INIT_FREEABLE ;

   // TEST static init: utf8reader_INIT_FREEABLE
   TEST(0 == nrcolumn_utf8reader(&utfread)) ;
   TEST(0 == nrline_utf8reader(&utfread)) ;
   TEST(0 == isnext_utf8reader(&utfread)) ;
   TEST(0 == unread_utf8reader(&utfread)) ;
   TEST(0 == unreadsize_utf8reader(&utfread)) ;

   // TEST init_utf8reader, double free_utf8reader
   for(size_t i = 0; i <= 110; i+=11) {
      init_utf8reader(&utfread, 10+3*i, (uint8_t*)i) ;
      TEST((uint8_t*)i == unread_utf8reader(&utfread)) ;
      TEST((10+3*i)    == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == nrcolumn_utf8reader(&utfread)) ;
      TEST(1 == nrline_utf8reader(&utfread)) ;
      TEST(1 == isnext_utf8reader(&utfread)) ;

      free_utf8reader(&utfread) ;
      TEST(0 == unread_utf8reader(&utfread)) ;
      TEST(0 == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == nrcolumn_utf8reader(&utfread)) ;
      TEST(0 == nrline_utf8reader(&utfread)) ;
      TEST(0 == isnext_utf8reader(&utfread)) ;

      free_utf8reader(&utfread) ;
      TEST(0 == unread_utf8reader(&utfread)) ;
      TEST(0 == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == nrcolumn_utf8reader(&utfread)) ;
      TEST(0 == nrline_utf8reader(&utfread)) ;
      TEST(0 == isnext_utf8reader(&utfread)) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_memento(void)
{
   utf8reader_t   utfread  ;
   utf8reader_t   utfread2 ;

   // TEST savetextpos_utf8reader, restoretextpos_utf8reader
   for(unsigned i = 0; i < 256; ++i) {
      utfread.colnr  = 10 + i ;
      utfread.linenr = 11 + i ;
      utfread.next   = (uint8_t*) 12 + i ;
      utfread.size   = 13 + i ;
      savetextpos_utf8reader(&utfread, &utfread2) ;
      utfread = (utf8reader_t) utf8reader_INIT_FREEABLE ;
      restoretextpos_utf8reader(&utfread, &utfread2) ;
      TEST(10 + i == nrcolumn_utf8reader(&utfread)) ;
      TEST(11 + i == nrline_utf8reader(&utfread)) ;
      TEST(12 + i == (uintptr_t) unread_utf8reader(&utfread)) ;
      TEST(13 + i == unreadsize_utf8reader(&utfread)) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_read(void)
{
   utf8reader_t   utfread ;
   const uint8_t  mbs[]  = { "ab\n\U000fffffab\n\U00000fffab\nöab\näab\nü" } ;

   // TEST: nextchar_utf8reader
   init_utf8reader(&utfread, 5*3+4+3+3*2, mbs) ;
   for(unsigned i = 0, col = 0; i < 5; ++i, col = 1) {
      uint32_t ch = 0 ;
      TEST(true == nextchar_utf8reader(&utfread, &ch)) ;
      TEST('a'  == ch) ;
      TEST(col+1== nrcolumn_utf8reader(&utfread)) ;
      TEST(true == nextchar_utf8reader(&utfread, &ch)) ;
      TEST('b'  == ch) ;
      TEST(col+2== nrcolumn_utf8reader(&utfread));
      TEST(1+i  == nrline_utf8reader(&utfread));
      TEST(true == nextchar_utf8reader(&utfread, &ch)) ;
      TEST('\n' == ch) ;
      TEST(0    == nrcolumn_utf8reader(&utfread));
      TEST(2+i  == nrline_utf8reader(&utfread));
      TEST(true == nextchar_utf8reader(&utfread, &ch)) ;
      switch(i) {
      case 0:  TEST(0xfffff == ch); break ;
      case 1:  TEST(0xfff == ch) ;  break ;
      case 2:  TEST(L'ö' == ch) ;   break ;
      case 3:  TEST(L'ä' == ch) ;   break ;
      default: TEST(L'ü' == ch) ;
               TEST(false == nextchar_utf8reader(&utfread, &ch)) ;
               break ;
      }
      TEST(1    == nrcolumn_utf8reader(&utfread));
      TEST(2+i  == nrline_utf8reader(&utfread));
   }

   // TEST: skipchar_utf8reader
   init_utf8reader(&utfread, 5*3+4+3+3*2, mbs) ;
   for(unsigned i = 0, col = 0, size = 28; i < 5; ++i, col = 1) {
      TEST(true == skipchar_utf8reader(&utfread)) ;
      TEST(--size == unreadsize_utf8reader(&utfread)) ;
      TEST(col+1== nrcolumn_utf8reader(&utfread)) ;
      TEST(true == skipchar_utf8reader(&utfread)) ;
      TEST(--size == unreadsize_utf8reader(&utfread)) ;
      TEST(col+2== nrcolumn_utf8reader(&utfread)) ;
      TEST(true == skipchar_utf8reader(&utfread)) ;
      TEST(--size == unreadsize_utf8reader(&utfread)) ;
      TEST(0    == nrcolumn_utf8reader(&utfread));
      TEST(2+i  == nrline_utf8reader(&utfread));
      TEST(true == skipchar_utf8reader(&utfread)) ;
      switch(i) {
      case 0:  size -= 4 ; break ;
      case 1:  size -= 3 ; break ;
      default: size -= 2 ; break ;
      }
      TEST(size          == unreadsize_utf8reader(&utfread)) ;
      TEST(&mbs[28-size] == unread_utf8reader(&utfread)) ;
      TEST(1    == nrcolumn_utf8reader(&utfread));
      TEST(2+i  == nrline_utf8reader(&utfread));
   }

   // TEST: peekascii_utf8reader, skipascii_utf8reader
   init_utf8reader(&utfread, 28, mbs) ;
   for(unsigned i = 0, col = 1, lnr = 1; i < 28; ++i, ++col) {
      uint8_t ch = 0 ;
      TEST(true == peekascii_utf8reader(&utfread, &ch)) ;
      TEST(mbs[i] == ch) ;
      if ('\n' == ch) {
         col = 0 ;
         ++ lnr ;
      }
      skipascii_utf8reader(&utfread) ;
      TEST(col == nrcolumn_utf8reader(&utfread)) ;
      TEST(lnr == nrline_utf8reader(&utfread)) ;
      if (i == 27) {
         ch = 0 ;
         TEST(false == peekascii_utf8reader(&utfread, &ch)) ;
         TEST(0 == ch) ;
      }
   }

   // TEST: peekasciiatoffset_utf8reader, skipNbytes_utf8reader
   for(unsigned i = 0; i < 28; ++i) {
      uint8_t ch = 0 ;
      init_utf8reader(&utfread, 28, mbs) ;
      TEST(true == peekasciiatoffset_utf8reader(&utfread, i, &ch)) ;
      TEST(mbs[i] == ch) ;
      ch = 0 ;
      TEST(false == peekasciiatoffset_utf8reader(&utfread, 28, &ch)) ;
      TEST(0 == ch) ;
      skipNbytes_utf8reader(&utfread, i, 3*i) ;
      TEST(&mbs[i] == unread_utf8reader(&utfread)) ;
      TEST(3*i  == nrcolumn_utf8reader(&utfread)) ;
      TEST(1    == nrline_utf8reader(&utfread)) ;
      TEST(true == peekascii_utf8reader(&utfread, &ch)) ;
      TEST(mbs[i] == ch) ;
      ch = 0 ;
      TEST(false == peekasciiatoffset_utf8reader(&utfread, 28-i, &ch)) ;
      TEST(0     == ch) ;
      TEST(true  == peekasciiatoffset_utf8reader(&utfread, 27-i, &ch)) ;
      TEST(mbs[27] == ch) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_skipline(void)
{
   utf8reader_t   utfread ;
   uint8_t        buffer[512] __attribute__ ((aligned (4))) ;

   // TEST skipline_utf8reader for every possible address alignemnt
   memset(buffer, '\n', sizeof(buffer)) ;
   memset(buffer, 0, 4) ;
   for(unsigned i = 4; i < sizeof(buffer); ++i) {
      buffer[i-1] = (uint8_t) ((uint8_t)i==10?0:i) ;
      for(unsigned al = 0; al < 4; ++al) {
         init_utf8reader(&utfread, sizeof(buffer)-al, buffer+al) ;
         utfread.linenr = i ;
         utfread.colnr  = 100 + i ;
         TEST(true == skipline_utf8reader(&utfread)) ;
         TEST(utfread.size   == sizeof(buffer)-i-1) ;
         TEST(utfread.next   == &buffer[i+1]) ;
         TEST(utfread.linenr == i + 1) ;
         TEST(utfread.colnr  == 0) ;
      }
   }

   // TEST skipline_utf8reader size <= 16
   for(unsigned i = 0; i < 16; ++i) {
      memset(buffer, 0, 16) ;
      buffer[i] = '\n' ;
      init_utf8reader(&utfread, 16, buffer) ;
      utfread.linenr = i ;
      utfread.colnr  = 100 + i ;
      TEST(true == skipline_utf8reader(&utfread)) ;
      TEST(utfread.size   == 16-i-1) ;
      TEST(utfread.next   == &buffer[i+1]) ;
      TEST(utfread.linenr == i + 1) ;
      TEST(utfread.colnr  == 0) ;
      // \n at the end of input
      memset(buffer, 0, 16) ;
      buffer[15] = '\n' ;
      init_utf8reader(&utfread, 16-i, buffer+i) ;
      TEST(true == skipline_utf8reader(&utfread)) ;
      TEST(utfread.size   == 0) ;
      TEST(utfread.next   == &buffer[16]) ;
      TEST(utfread.linenr == 2) ;
      TEST(utfread.colnr  == 0) ;
   }

   // TEST skipline_utf8reader skips until end of input
   unsigned   bufsize = 9 * (sizeof(buffer) / 9) ;
   const char mbs[9]  = { "\U001fffff\U0000ffff\u07ff" } ;
   for(unsigned i = 0; i < bufsize; i += 9) {
      memcpy(&buffer[i], mbs, 9) ;
   }
   for(unsigned i = 0, cols = bufsize/3; i < bufsize; ) {
      for(unsigned offset = 4; offset > 1; i += offset, --offset, --cols) {
         init_utf8reader(&utfread, bufsize-i, buffer+i) ;
         TEST(true == skipline_utf8reader(&utfread)) ;
         TEST(utfread.size   == 0) ;
         TEST(utfread.next   == &buffer[bufsize]) ;
         TEST(utfread.linenr == 1) ;
         TEST(utfread.colnr  == cols) ;
      }
   }

   // TEST skipline_utf8reader returns false
   memset(buffer, 0, sizeof(buffer)) ;
   init_utf8reader(&utfread, 0, buffer) ;
   TEST(false == skipline_utf8reader(&utfread)) ;
   init_utf8reader(&utfread, 10, buffer) ;
   TEST(true  == skipline_utf8reader(&utfread)) ;
   TEST(false == skipline_utf8reader(&utfread)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_io_reader_utf8reader()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ABBRUCH ;
   if (test_memento())     goto ABBRUCH ;
   if (test_read())        goto ABBRUCH ;
   if (test_skipline())    goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
