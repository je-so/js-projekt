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
#include "C-kern/api/string/stringstream.h"
#include "C-kern/api/math/int/log2.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: utf8reader_t

// group: read

int skipline_utf8reader(utf8reader_t * utfread)
{
   const uint8_t * found = findbyte_stringstream(genericcast_stringstream(utfread), '\n') ;

   if (found) {
      utfread->next = ++ found ;
      nextline_textpos(&utfread->pos) ;

      return 0 ;
   }

   return ENODATA ;
}

int skipall_utf8reader(utf8reader_t * utfread)
{
   int err ;

   do {
      err = skipchar_utf8reader(utfread) ;
   } while (!err) ;

   return err == ENODATA ? 0 : err ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   utf8reader_t   utfread = utf8reader_INIT_FREEABLE ;

   // TEST utf8reader_INIT_FREEABLE
   TEST(0 == column_utf8reader(&utfread)) ;
   TEST(0 == line_utf8reader(&utfread)) ;
   TEST(0 == isnext_utf8reader(&utfread)) ;
   TEST(0 == unread_utf8reader(&utfread)) ;
   TEST(0 == unreadsize_utf8reader(&utfread)) ;

   // TEST init_utf8reader, free_utf8reader
   for (size_t i = 0; i <= 110; i+=11) {
      init_utf8reader(&utfread, 10+3*i, (uint8_t*)i) ;
      TEST((uint8_t*)i == unread_utf8reader(&utfread)) ;
      TEST((10+3*i)    == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == column_utf8reader(&utfread)) ;
      TEST(1 == line_utf8reader(&utfread)) ;
      TEST(1 == isnext_utf8reader(&utfread)) ;

      free_utf8reader(&utfread) ;
      TEST(0 == unread_utf8reader(&utfread)) ;
      TEST(0 == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == column_utf8reader(&utfread)) ;
      TEST(0 == line_utf8reader(&utfread)) ;
      TEST(0 == isnext_utf8reader(&utfread)) ;

      free_utf8reader(&utfread) ;
      TEST(0 == unread_utf8reader(&utfread)) ;
      TEST(0 == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == column_utf8reader(&utfread)) ;
      TEST(0 == line_utf8reader(&utfread)) ;
      TEST(0 == isnext_utf8reader(&utfread)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   utf8reader_t   utfread  ;

   // TEST column_utf8reader, line_utf8reader
   for (unsigned i = 0; i < 16; ++i) {
      utfread.pos.column = i ;
      utfread.pos.line   = 2*i + 1 ;
      TEST(1*i+0 == column_utf8reader(&utfread)) ;
      TEST(2*i+1 == line_utf8reader(&utfread)) ;
   }

   // TEST textpos_utf8reader
   for (unsigned i = 0; i < 16; ++i) {
      utfread.pos.column = 10 + i ;
      utfread.pos.line   = 11 + i ;
      textpos_t pos = textpos_utf8reader(&utfread) ;
      TEST(10 + i == column_textpos(&pos)) ;
      TEST(11 + i == line_textpos(&pos)) ;
   }

   // other query functions tested in test_read

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_read(void)
{
   utf8reader_t   utfread ;
   const uint8_t  mbs[]  = { "ab\n\U000fffffab\n\U00000fffab\nöab\näab\nü" } ;
   uint32_t       ch ;

   // TEST nextchar_utf8reader
   init_utf8reader(&utfread, 5*3+4+3+3*2, mbs) ;
   for (unsigned i = 0, col = 0; i < 5; ++i, col = 1) {
      ch = 0 ;
      TEST(0    == nextchar_utf8reader(&utfread, &ch)) ;
      TEST('a'  == ch) ;
      TEST(col+1== column_utf8reader(&utfread)) ;
      TEST(0    == nextchar_utf8reader(&utfread, &ch)) ;
      TEST('b'  == ch) ;
      TEST(col+2== column_utf8reader(&utfread));
      TEST(1+i  == line_utf8reader(&utfread));
      TEST(0    == nextchar_utf8reader(&utfread, &ch)) ;
      TEST('\n' == ch) ;
      TEST(0    == column_utf8reader(&utfread));
      TEST(2+i  == line_utf8reader(&utfread));
      TEST(0    == nextchar_utf8reader(&utfread, &ch)) ;
      switch(i) {
      case 0:  TEST(0xfffff == ch); break ;
      case 1:  TEST(0xfff == ch) ;  break ;
      case 2:  TEST(L'ö' == ch) ;   break ;
      case 3:  TEST(L'ä' == ch) ;   break ;
      default: TEST(L'ü' == ch) ;   break ;
      }
      TEST(1    == column_utf8reader(&utfread));
      TEST(2+i  == line_utf8reader(&utfread));
   }

   // TEST nextchar_utf8reader: ENODATA
   TEST(ENODATA == nextchar_utf8reader(&utfread, &ch)) ;

   // TEST skipchar_utf8reader
   init_utf8reader(&utfread, 5*3+4+3+3*2, mbs) ;
   for (unsigned i = 0, col = 0, size = 28; i < 5; ++i, col = 1) {
      TEST(0      == skipchar_utf8reader(&utfread)) ;
      TEST(--size == unreadsize_utf8reader(&utfread)) ;
      TEST(col+1  == column_utf8reader(&utfread)) ;
      TEST(0      == skipchar_utf8reader(&utfread)) ;
      TEST(--size == unreadsize_utf8reader(&utfread)) ;
      TEST(col+2  == column_utf8reader(&utfread)) ;
      TEST(0      == skipchar_utf8reader(&utfread)) ;
      TEST(--size == unreadsize_utf8reader(&utfread)) ;
      TEST(0    == column_utf8reader(&utfread));
      TEST(2+i  == line_utf8reader(&utfread));
      TEST(0    == skipchar_utf8reader(&utfread)) ;
      switch(i) {
      case 0:  size -= 4 ; break ;
      case 1:  size -= 3 ; break ;
      default: size -= 2 ; break ;
      }
      TEST(size          == unreadsize_utf8reader(&utfread)) ;
      TEST(&mbs[28-size] == unread_utf8reader(&utfread)) ;
      TEST(1    == column_utf8reader(&utfread));
      TEST(2+i  == line_utf8reader(&utfread));
   }

   // TEST skipchar_utf8reader ENODATA
   TEST(ENODATA == skipchar_utf8reader(&utfread)) ;

   // TEST nextchar_utf8reader, skipchar_utf8reader: EILSEQ + ENOTEMPTY
   utf8reader_t old ;
   init_utf8reader(&utfread, 3, (const uint8_t*)"\U0010ffff") ;
   old = utfread ;
   TEST(EILSEQ == skipchar_utf8reader(&utfread)) ;
   TEST(ENOTEMPTY == nextchar_utf8reader(&utfread, &ch)) ;
   TEST(0 == memcmp(&old, &utfread, sizeof(utfread))) ;
   init_utf8reader(&utfread, 2, (const uint8_t*)"\U0000ffff") ;
   old = utfread ;
   TEST(EILSEQ == skipchar_utf8reader(&utfread)) ;
   TEST(ENOTEMPTY == nextchar_utf8reader(&utfread, &ch)) ;
   TEST(0 == memcmp(&old, &utfread, sizeof(utfread))) ;
   init_utf8reader(&utfread, 1, (const uint8_t*)"\u07ff") ;
   old = utfread ;
   TEST(EILSEQ == skipchar_utf8reader(&utfread)) ;
   TEST(ENOTEMPTY == nextchar_utf8reader(&utfread, &ch)) ;
   TEST(0 == memcmp(&old, &utfread, sizeof(utfread))) ;

   // TEST peekascii_utf8reader, skipascii_utf8reader
   init_utf8reader(&utfread, 28, mbs) ;
   for (unsigned i = 0, col = 1, lnr = 1; i < 28; ++i, ++col) {
      TEST(0 == peekascii_utf8reader(&utfread, &ch)) ;
      TEST(mbs[i] == ch) ;
      if ('\n' == ch) {
         col = 0 ;
         ++ lnr ;
      }
      skipascii_utf8reader(&utfread) ;
      TEST(col == column_utf8reader(&utfread)) ;
      TEST(lnr == line_utf8reader(&utfread)) ;
      if (i == 27) {
      }
   }

   // TEST peekascii_utf8reader: ENODATA
   ch = 0 ;
   TEST(ENODATA == peekascii_utf8reader(&utfread, &ch)) ;
   TEST(0 == ch) ;

   // TEST peekasciiatoffset_utf8reader, skipNbytes_utf8reader
   for (unsigned i = 0; i < 28; ++i) {
      init_utf8reader(&utfread, 28, mbs) ;
      TEST(0 == peekasciiatoffset_utf8reader(&utfread, i, &ch)) ;
      TEST(mbs[i] == ch) ;
      TEST(ENODATA == peekasciiatoffset_utf8reader(&utfread, 28, &ch)) ;
      TEST(mbs[i] == ch) ;
      skipNbytes_utf8reader(&utfread, i, 3*i) ;
      TEST(&mbs[i] == unread_utf8reader(&utfread)) ;
      TEST(3*i  == column_utf8reader(&utfread)) ;
      TEST(1    == line_utf8reader(&utfread)) ;
      TEST(0    == peekascii_utf8reader(&utfread, &ch)) ;
      TEST(mbs[i]  == ch) ;
      TEST(ENODATA == peekasciiatoffset_utf8reader(&utfread, 28-i, &ch)) ;
      TEST(mbs[i]  == ch) ;
      TEST(0       == peekasciiatoffset_utf8reader(&utfread, 27-i, &ch)) ;
      TEST(mbs[27] == ch) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_skiplineall(void)
{
   utf8reader_t   utfread ;
   uint8_t        buffer[512] __attribute__ ((aligned (4))) ;

   // TEST skipline_utf8reader for every possible address alignemnt
   memset(buffer, '\n', sizeof(buffer)) ;
   memset(buffer, 0, 4) ;
   for (unsigned i = 4; i < sizeof(buffer); ++i) {
      buffer[i-1] = (uint8_t) ((uint8_t)i==10?0:i) ;
      for (unsigned al = 0; al < 4; ++al) {
         init_utf8reader(&utfread, sizeof(buffer)-al, buffer+al) ;
         utfread.pos.line   = i ;
         utfread.pos.column = 100 + i ;
         TEST(0 == skipline_utf8reader(&utfread)) ;
         TEST(utfread.next == &buffer[i+1]) ;
         TEST(utfread.end  == &buffer[sizeof(buffer)]) ;
         TEST(0   == column_utf8reader(&utfread)) ;
         TEST(i+1 == line_utf8reader(&utfread)) ;
      }
   }

   // TEST skipline_utf8reader size <= 16
   for (unsigned i = 0; i < 16; ++i) {
      memset(buffer, 0, 16) ;
      buffer[i] = '\n' ;
      init_utf8reader(&utfread, 16, buffer) ;
      utfread.pos.line   = i ;
      utfread.pos.column = 100 + i ;
      TEST(0 == skipline_utf8reader(&utfread)) ;
      TEST(utfread.next   == &buffer[i+1]) ;
      TEST(utfread.end    == &buffer[16]) ;
      TEST(0   == column_utf8reader(&utfread)) ;
      TEST(i+1 == line_utf8reader(&utfread)) ;
      // \n at the end of input
      memset(buffer, 0, 16) ;
      buffer[15] = '\n' ;
      init_utf8reader(&utfread, 16-i, buffer+i) ;
      TEST(0 == skipline_utf8reader(&utfread)) ;
      TEST(utfread.next == &buffer[16]) ;
      TEST(utfread.end  == &buffer[16]) ;
      TEST(0 == column_utf8reader(&utfread)) ;
      TEST(2 == line_utf8reader(&utfread)) ;
   }

   // TEST skipline_utf8reader ENODATA
   memset(buffer, 0, sizeof(buffer)) ;
   init_utf8reader(&utfread, sizeof(buffer), buffer) ;
   TEST(ENODATA == skipline_utf8reader(&utfread)) ;
      // nothing changed
   TEST(utfread.next == buffer) ;
   TEST(utfread.end  == buffer+sizeof(buffer)) ;
   TEST(0 == column_utf8reader(&utfread)) ;
   TEST(1 == line_utf8reader(&utfread)) ;

   // TEST skipall_utf8reader (only columns)
   unsigned   bufsize = 9 * (sizeof(buffer) / 9) ;
   const char mbs[9]  = { "\U001fffff\U0000ffff\u07ff" } ;
   for (unsigned i = 0; i < bufsize; i += 9) {
      memcpy(&buffer[i], mbs, 9) ;
   }
   for (unsigned i = 0, cols = bufsize/3; i < bufsize; ) {
      for (unsigned offset = 4; offset > 1; i += offset, --offset, --cols) {
         init_utf8reader(&utfread, bufsize-i, buffer+i) ;
         TEST(0 == skipall_utf8reader(&utfread)) ;
         TEST(utfread.next   == &buffer[bufsize]) ;
         TEST(utfread.end    == &buffer[bufsize]) ;
         TEST(cols == column_utf8reader(&utfread)) ;
         TEST(1    == line_utf8reader(&utfread)) ;
      }
   }

   // TEST skipall_utf8reader (only lines + 1 column)
   memset(buffer, '\n', sizeof(buffer)) ;
   buffer[sizeof(buffer)-2] = 'a' ;
   buffer[sizeof(buffer)-1] = 'b' ;
   init_utf8reader(&utfread, sizeof(buffer), buffer) ;
   TEST(0 == skipall_utf8reader(&utfread)) ;
   TEST(utfread.next == &buffer[sizeof(buffer)]) ;
   TEST(utfread.end  == &buffer[sizeof(buffer)]) ;
   TEST(2 == column_utf8reader(&utfread)) ;
   TEST(sizeof(buffer)-1 == line_utf8reader(&utfread)) ;

   // TEST skipline_utf8reader / skipall_utf8reader ENODATA empty string
   memset(buffer, 0, sizeof(buffer)) ;
   init_utf8reader(&utfread, 0, buffer) ;
   TEST(ENODATA == skipline_utf8reader(&utfread)) ;
   TEST(0 == skipall_utf8reader(&utfread)) ;
   init_utf8reader(&utfread, 10, buffer) ;
   TEST(0 == skipall_utf8reader(&utfread)) ;
   TEST(ENODATA == skipline_utf8reader(&utfread)) ;
   TEST(0 == skipall_utf8reader(&utfread)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_io_reader_utf8reader()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_read())        goto ONABORT ;
   if (test_skiplineall()) goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
