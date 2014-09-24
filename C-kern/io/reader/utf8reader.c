/* title: UTF8TextReader impl

   Implements <UTF8TextReader>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/memstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: utf8reader_t

// group: lifetime

int init_utf8reader(/*out*/utf8reader_t * utfread, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/)
{
   int err ;

   err = init_mmfile(&utfread->mmfile, filepath, 0, 0, accessmode_READ, relative_to) ;
   if (err) goto ONERR;

   utfread->pos  = (textpos_t) textpos_INIT ;
   utfread->next = addr_mmfile(&utfread->mmfile) ;
   utfread->end  = utfread->next + size_mmfile(&utfread->mmfile) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

/* define: free_utf8reader
 * Implements <utf8reader_t.free_utf8reader>. */
int free_utf8reader(utf8reader_t * utfread)
{
   int err ;
   utfread->next = 0 ;
   utfread->end  = 0 ;
   free_textpos(&utfread->pos) ;

   err = free_mmfile(&utfread->mmfile) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}


// group: read

int skipline_utf8reader(utf8reader_t * utfread)
{
   const uint8_t * found = findbyte_memstream(cast_memstreamro(utfread,), '\n');

   if (found) {
      utfread->next = ++ found ;
      incrline_textpos(&utfread->pos) ;

      return 0 ;
   }

   utfread->next = utfread->end ;
   return ENODATA ;
}

// group: read+match

int matchbytes_utf8reader(utf8reader_t * utfread, size_t colnr, size_t nrbytes, const uint8_t bytes[nrbytes], /*err*/size_t * matchedsize)
{

   if (  unreadsize_utf8reader(utfread) < nrbytes
         || 0 != strncmp((const char*)unread_utf8reader(utfread), (const char*)bytes, nrbytes)) {
      size_t    i = 0 ;
      size_t maxi = nrbytes < unreadsize_utf8reader(utfread) ? nrbytes : unreadsize_utf8reader(utfread) ;
      for (i = 0; i < maxi; ++i) {
         if (unread_utf8reader(utfread)[i] != bytes[i]) break ;
      }

      if (matchedsize) *matchedsize = i ;
      skipbytes_utf8reader(utfread, i, 0) ;
      return (0 == unreadsize_utf8reader(utfread)) ? ENODATA : EMSGSIZE ;
   }

   // matchedsize is no out value
   skipbytes_utf8reader(utfread, nrbytes, colnr) ;
   return 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(directory_t * tempdir)
{
   utf8reader_t   utfread = utf8reader_FREE ;

   // TEST utf8reader_FREE
   TEST(0 == column_utf8reader(&utfread)) ;
   TEST(0 == line_utf8reader(&utfread)) ;
   TEST(0 == isnext_utf8reader(&utfread)) ;
   TEST(0 == unread_utf8reader(&utfread)) ;
   TEST(0 == unreadsize_utf8reader(&utfread)) ;
   TEST(1 == isfree_mmfile(&utfread.mmfile)) ;

   // TEST init_utf8reader, free_utf8reader
   for (size_t i = 0; i <= 110; i+=11) {
      TEST(0 == makefile_directory(tempdir, "grow", 10+3*i)) ;

      TEST(0 == init_utf8reader(&utfread, "grow", tempdir)) ;
      TEST(0 != unread_utf8reader(&utfread)) ;
      TEST((10+3*i) == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == column_utf8reader(&utfread)) ;
      TEST(1 == line_utf8reader(&utfread)) ;
      TEST(1 == isnext_utf8reader(&utfread)) ;

      free_utf8reader(&utfread) ;
      TEST(0 == unread_utf8reader(&utfread)) ;
      TEST(0 == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == column_utf8reader(&utfread)) ;
      TEST(0 == line_utf8reader(&utfread)) ;
      TEST(0 == isnext_utf8reader(&utfread)) ;
      TEST(1 == isfree_mmfile(&utfread.mmfile)) ;

      free_utf8reader(&utfread) ;
      TEST(0 == unread_utf8reader(&utfread)) ;
      TEST(0 == unreadsize_utf8reader(&utfread)) ;
      TEST(0 == column_utf8reader(&utfread)) ;
      TEST(0 == line_utf8reader(&utfread)) ;
      TEST(0 == isnext_utf8reader(&utfread)) ;
      TEST(1 == isfree_mmfile(&utfread.mmfile)) ;

      TEST(0 == removefile_directory(tempdir, "grow")) ;
   }

   // TEST init_utf8reader: empty file
   TEST(0 == makefile_directory(tempdir, "grow", 0)) ;
   TEST(0 == init_utf8reader(&utfread, "grow", tempdir)) ;
   TEST(0 == unread_utf8reader(&utfread)) ;
   TEST(0 == unreadsize_utf8reader(&utfread)) ;
   TEST(0 == column_utf8reader(&utfread)) ;
   TEST(1 == line_utf8reader(&utfread)) ;
   TEST(0 == isnext_utf8reader(&utfread)) ;
   TEST(1 == isfree_mmfile(&utfread.mmfile)) ;
   TEST(0 == free_utf8reader(&utfread)) ;
   TEST(0 == removefile_directory(tempdir, "grow")) ;

   return 0 ;
ONERR:
   removefile_directory(tempdir, "grow") ;
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
      textpos_t pos = *textpos_utf8reader(&utfread) ;
      TEST(10 + i == column_textpos(&pos)) ;
      TEST(11 + i == line_textpos(&pos)) ;
   }

   // other query functions tested in test_read

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_read(directory_t * tempdir)
{
   utf8reader_t   utfread = utf8reader_FREE ;
   const uint8_t  mbs[]   = { "ab\n\U000fffffab\n\U00000fffab\nöab\näab\nü" } ;
   const size_t   mbssize = 5*3 + 4 + 3 + 3*2 ;
   uint32_t       ch ;

   // prepare
   TEST(0 == save_file("text", mbssize, mbs, tempdir)) ;

   // TEST nextbyte_utf8reader
   TEST(0 == init_utf8reader(&utfread, "text", tempdir)) ;
   for (unsigned i = 0, l = 1, c = 0; i < mbssize; ++i) {
      uint8_t b ;
      TEST(0 == nextbyte_utf8reader(&utfread, &b)) ;
      TEST(b == mbs[i]) ;
      ++ c ;
      if (b == '\n') {
         ++ l ;
         c = 0 ;
      }
      TEST(c == column_utf8reader(&utfread)) ;
      TEST(l == line_utf8reader(&utfread)) ;
   }

   // TEST nextbyte_utf8reader: ENODATA
   TEST(ENODATA == nextbyte_utf8reader(&utfread, (uint8_t*)0)) ;
   TEST(0 == free_utf8reader(&utfread)) ;

   // TEST nextchar_utf8reader
   TEST(0 == init_utf8reader(&utfread, "text", tempdir)) ;
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
   TEST(0 == free_utf8reader(&utfread)) ;

   // TEST skipchar_utf8reader
   TEST(0 == init_utf8reader(&utfread, "text", tempdir)) ;
   for (unsigned i = 0, col = 0, size = mbssize; i < 5; ++i, col = 1) {
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
      TEST(size == unreadsize_utf8reader(&utfread)) ;
      TEST(1    == column_utf8reader(&utfread));
      TEST(2+i  == line_utf8reader(&utfread));
   }

   // TEST skipchar_utf8reader: ENODATA
   TEST(ENODATA == skipchar_utf8reader(&utfread)) ;
   TEST(0 == free_utf8reader(&utfread)) ;

   // TEST nextchar_utf8reader, skipchar_utf8reader: EILSEQ + ENOTEMPTY
   utf8reader_t old ;
   TEST(0 == save_file("illseq", 3, (const uint8_t*)"\U0010ffff", tempdir)) ;
   TEST(0 == init_utf8reader(&utfread, "illseq", tempdir)) ;
   old = utfread ;
   TEST(EILSEQ == skipchar_utf8reader(&utfread)) ;
   TEST(ENOTEMPTY == nextchar_utf8reader(&utfread, &ch)) ;
   TEST(0 == memcmp(&old, &utfread, sizeof(utfread))) ;
   TEST(0 == remove_file("illseq", tempdir)) ;
   TEST(0 == save_file("illseq", 2, (const uint8_t*)"\U0000ffff", tempdir)) ;
   TEST(0 == free_utf8reader(&utfread)) ;
   TEST(0 == init_utf8reader(&utfread, "illseq", tempdir)) ;
   old = utfread ;
   TEST(EILSEQ == skipchar_utf8reader(&utfread)) ;
   TEST(ENOTEMPTY == nextchar_utf8reader(&utfread, &ch)) ;
   TEST(0 == memcmp(&old, &utfread, sizeof(utfread))) ;
   TEST(0 == remove_file("illseq", tempdir)) ;
   TEST(0 == save_file("illseq", 1, (const uint8_t*)"\u07ff", tempdir)) ;
   TEST(0 == free_utf8reader(&utfread)) ;
   TEST(0 == init_utf8reader(&utfread, "illseq", tempdir)) ;
   old = utfread ;
   TEST(EILSEQ == skipchar_utf8reader(&utfread)) ;
   TEST(ENOTEMPTY == nextchar_utf8reader(&utfread, &ch)) ;
   TEST(0 == memcmp(&old, &utfread, sizeof(utfread))) ;
   TEST(0 == free_utf8reader(&utfread)) ;

   // TEST peekascii_utf8reader, skipascii_utf8reader
   TEST(0 == init_utf8reader(&utfread, "text", tempdir)) ;
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
   TEST(0 == free_utf8reader(&utfread)) ;

   // TEST peekasciiatoffset_utf8reader, skipbytes_utf8reader
   for (unsigned i = 0; i < mbssize; ++i) {
      TEST(0 == init_utf8reader(&utfread, "text", tempdir)) ;
      TEST(0 == peekasciiatoffset_utf8reader(&utfread, i, &ch)) ;
      TEST(mbs[i]  == ch) ;
      TEST(ENODATA == peekasciiatoffset_utf8reader(&utfread, mbssize, &ch)) ;
      TEST(mbs[i]  == ch) ;
      skipbytes_utf8reader(&utfread, i, 3*i) ;
      TEST(mbssize-i == unreadsize_utf8reader(&utfread)) ;
      TEST(3*i == column_utf8reader(&utfread)) ;
      TEST(1  == line_utf8reader(&utfread)) ;
      TEST(0  == peekascii_utf8reader(&utfread, &ch)) ;
      TEST(ch == mbs[i]) ;
      TEST(ENODATA == peekasciiatoffset_utf8reader(&utfread, mbssize-i, &ch)) ;
      TEST(ch == mbs[i]) ;
      TEST(0  == peekasciiatoffset_utf8reader(&utfread, mbssize-1-i, &ch)) ;
      TEST(ch == mbs[mbssize-1]) ;
      TEST(0  == free_utf8reader(&utfread)) ;
   }

   // TEST skipbytes_utf8reader: skip all bytes
   TEST(0 == init_utf8reader(&utfread, "text", tempdir)) ;
   skipbytes_utf8reader(&utfread, mbssize, 9) ;
   TEST(9 == column_utf8reader(&utfread)) ;
   TEST(0 == unreadsize_utf8reader(&utfread)) ;
   TEST(0 == free_utf8reader(&utfread)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "text")) ;
   TEST(0 == removefile_directory(tempdir, "illseq")) ;

   return 0 ;
ONERR:
   free_utf8reader(&utfread) ;
   removefile_directory(tempdir, "text") ;
   removefile_directory(tempdir, "illseq") ;
   return EINVAL ;
}

static int test_skipline(directory_t * tempdir)
{
   utf8reader_t   utfread = utf8reader_FREE ;
   uint8_t        buffer[512] ;

   // TEST skipline_utf8reader
   memset(buffer, '\n', sizeof(buffer)) ;
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      if (i) buffer[i-1] = (uint8_t)((i&0xff) == '\n' ? 0: i) ;
      TEST(0 == save_file("newline", sizeof(buffer), buffer, tempdir)) ;
      TEST(0 == init_utf8reader(&utfread, "newline", tempdir)) ;
      utfread.pos.line   = i ;
      utfread.pos.column = 100 + i ;
      TEST(0 == skipline_utf8reader(&utfread)) ;
      TEST(sizeof(buffer)-1-i == unreadsize_utf8reader(&utfread)) ;
      TEST(0   == column_utf8reader(&utfread)) ;
      TEST(i+1 == line_utf8reader(&utfread)) ;
      TEST(0 == free_utf8reader(&utfread)) ;
      TEST(0 == remove_file("newline", tempdir)) ;
   }

   // TEST skipline_utf8reader: ENODATA
   memset(buffer, 0, sizeof(buffer)) ;
   TEST(0 == save_file("newline", sizeof(buffer), buffer, tempdir)) ;
   TEST(0 == init_utf8reader(&utfread, "newline", tempdir)) ;
   TEST(ENODATA == skipline_utf8reader(&utfread)) ;
      // all bytes consumed
   TEST(utfread.next == utfread.end) ;
   TEST(0 == column_utf8reader(&utfread)) ;
   TEST(1 == line_utf8reader(&utfread)) ;
   TEST(0 == remove_file("newline", tempdir)) ;
   TEST(0 == free_utf8reader(&utfread)) ;

   return 0 ;
ONERR:
   remove_file("newline", tempdir) ;
   free_utf8reader(&utfread) ;
   return EINVAL ;
}

static int test_match(directory_t * tempdir)
{
   utf8reader_t   utfread = utf8reader_FREE ;
   uint8_t        buffer[256] ;
   uint8_t        buffer2[10] ;
   size_t         matchedsize ;

   // prepare
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t)i ;
   }
   TEST(0 == save_file("match", sizeof(buffer), buffer, tempdir)) ;

   // TEST matchbytes_utf8reader
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      matchedsize = 512 ;
      TEST(0 == init_utf8reader(&utfread, "match", tempdir)) ;
      TEST(0 == matchbytes_utf8reader(&utfread, 2*i, i, buffer, &matchedsize)) ;
      TEST(512 == matchedsize/*not changed*/) ;
      TEST(2*i == column_utf8reader(&utfread)) ;
      TEST(0 == matchbytes_utf8reader(&utfread, 3*i, sizeof(buffer)-i, buffer+i, &matchedsize)) ;
      TEST(5*i == column_utf8reader(&utfread)) ;
      TEST(512 == matchedsize/*not changed*/) ;
      TEST(ENODATA == matchbytes_utf8reader(&utfread, 1, 1, buffer, &matchedsize)) ;
      TEST(0 == matchedsize/*err*/) ;
      TEST(0 == free_utf8reader(&utfread)) ;
   }

   // TEST matchbytes_utf8reader: EMSGSIZE
   matchedsize = 0 ;
   TEST(0 == init_utf8reader(&utfread, "match", tempdir)) ;
   TEST(0 == matchbytes_utf8reader(&utfread, 0, sizeof(buffer)-sizeof(buffer2), buffer, &matchedsize)) ;
   memcpy(buffer2, buffer+sizeof(buffer)-sizeof(buffer2), sizeof(buffer2)) ;
   ++ buffer2[sizeof(buffer2)-1] ;
   TEST(EMSGSIZE == matchbytes_utf8reader(&utfread, 1, sizeof(buffer2), buffer2, &matchedsize)) ;
   TEST(1 == unreadsize_utf8reader(&utfread)) ;
   TEST(0 == column_utf8reader(&utfread)) ;
   TEST(matchedsize == sizeof(buffer2)-1) ;
   TEST(0 == free_utf8reader(&utfread)) ;

   // TEST matchbytes_utf8reader: ENODATA
   matchedsize = 0 ;
   TEST(0 == init_utf8reader(&utfread, "match", tempdir)) ;
   TEST(0 == matchbytes_utf8reader(&utfread, 0, sizeof(buffer)-sizeof(buffer2)+1, buffer, &matchedsize)) ;
   memcpy(buffer2, buffer+sizeof(buffer)-sizeof(buffer2)+1, sizeof(buffer2)-1) ;
   TEST(ENODATA == matchbytes_utf8reader(&utfread, 1, sizeof(buffer2), buffer2, &matchedsize)) ;
   TEST(0 == column_utf8reader(&utfread)) ;
   TEST(0 == unreadsize_utf8reader(&utfread)) ;
   TEST(matchedsize == sizeof(buffer2)-1) ;
   TEST(0 == free_utf8reader(&utfread)) ;

   // unprepare
   TEST(0 == remove_file("match", tempdir)) ;

   return 0 ;
ONERR:
   remove_file("match", tempdir) ;
   free_utf8reader(&utfread) ;
   return EINVAL ;
}

int unittest_io_reader_utf8reader()
{
   directory_t *tempdir = 0 ;
   cstring_t   tmppath  = cstring_INIT ;

   // prepare
   TEST(0 == newtemp_directory(&tempdir, "utf8reader")) ;
   TEST(0 == path_directory(tempdir, &(wbuffer_t)wbuffer_INIT_CSTRING(&tmppath))) ;

   if (test_initfree(tempdir))   goto ONERR;
   if (test_query())             goto ONERR;
   if (test_read(tempdir))       goto ONERR;
   if (test_skipline(tempdir))   goto ONERR;
   if (test_match(tempdir))      goto ONERR;

   // unprepare
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

   return 0 ;
ONERR:
   removedirectory_directory(0, str_cstring(&tmppath)) ;
   free_cstring(&tmppath) ;
   delete_directory(&tempdir) ;
   return EINVAL ;
}

#endif
