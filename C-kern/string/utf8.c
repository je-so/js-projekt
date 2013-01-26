/* title: UTF-8 impl
   Implements <UTF-8>.

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

   file: C-kern/api/string/utf8.h
    Header file <UTF-8>.

   file: C-kern/string/utf8.c
    Implementation file <UTF-8 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/utf8.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/string/stringstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/time/timevalue.h"
#endif

// section: utf8

uint8_t g_utf8_bytesperchar[256] = {   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/*0 .. 127*/
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,/*128..191 not the first byte => error*/
                                       2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                       2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,/*192..223*/
                                       3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,/*224..239*/
                                       4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0 /*240..247*/ /*248..250, 252..253, 254..255 error*/
                                    } ;

char32_t decode_utf8(const uint8_t ** strstart)
{
   __extension__ static void * jumptable[256] = {
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0,
      &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0,
      &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0,
      &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0,
      &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2,
      &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2,
      &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3,
      &&L4, &&L4, &&L4, &&L4, &&L4, &&L4, &&L4, &&L4, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0
   } ;

   uint8_t        firstbyte ;
   uint8_t        flags ;
   uint8_t        nbyte ;
   char32_t       uchar ;
   const uint8_t  * next = *strstart ;

   firstbyte = *next++ ;
   __extension__ ({ goto *(jumptable[firstbyte]) ; }) ;

L0:
   return 0 ; /*EILSEQ*/

L1:
   if (!firstbyte) return 0 ; /*end of string*/

   *strstart = next ;
   return firstbyte ;

L2:
   uchar = ((uint32_t)(firstbyte & 0x1F) << 6) ;
   flags = *next++ ^ 0x80 ;
   uchar = uchar | flags ;
   if ((flags & 0xC0) || uchar < 0x80) return 0 ; /*EILSEQ*/

   *strstart = next ;
   return uchar ;

L3:
   uchar = ((uint32_t)(firstbyte & 0xF) << 6) ;
   flags = *next++ ^ 0x80 ;
   uchar = (uchar | flags) << 6 ;
   nbyte = *next++ ^ 0x80 ;
   flags = flags | nbyte ;
   uchar = (uchar | nbyte) ;
   if ((flags & 0xC0) || uchar < 0x800) return 0 ; /*EILSEQ*/

   *strstart = next ;
   return uchar ;

L4:
   uchar = ((uint32_t)(firstbyte & 0x7) << 6) ;
   flags = *next++ ^ 0x80 ;
   uchar = (uchar | flags) << 6 ;
   nbyte = *next++ ^ 0x80 ;
   flags = flags | nbyte ;
   uchar = (uchar | nbyte) << 6 ;
   nbyte = *next++ ^ 0x80 ;
   flags = flags | nbyte ;
   uchar = (uchar | nbyte) ;
   if ((flags & 0xC0) || uchar < 0x10000 || uchar > 0x10FFFF) return 0 ; /*EILSEQ*/

   *strstart = next ;
   return uchar ;
}


// section: stringstream_t

// group: read-utf8

int nextutf8_stringstream(struct stringstream_t * strstream, /*out*/char32_t * wchar)
{
   __extension__ static void * jumptable[256] = {
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1, &&L1,
      &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0,
      &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0,
      &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0,
      &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0,
      &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2,
      &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2, &&L2,
      &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3, &&L3,
      &&L4, &&L4, &&L4, &&L4, &&L4, &&L4, &&L4, &&L4, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0, &&L0
   } ;

   uint8_t        firstbyte ;
   uint8_t        flags ;
   uint8_t        nbyte ;
   char32_t       uchar ;
   const uint8_t  * next  = strstream->next ;
   size_t         strsize = size_stringstream(strstream) ;

   if (!strsize) return ENODATA ;
   firstbyte = *next++ ;
   if (strsize < sizechar_utf8(firstbyte)) return ENOTEMPTY ;

   __extension__ ({ goto *(jumptable[firstbyte]) ; }) ;

L0:
   return EILSEQ ;

L1:
   strstream->next = next ;
   *wchar = firstbyte ;
   return 0 ;

L2:
   uchar = ((uint32_t)(firstbyte & 0x1F) << 6) ;
   flags = *next++ ^ 0x80 ;
   uchar = uchar | flags ;
   if ((flags & 0xC0) || uchar < 0x80) return EILSEQ ;

   strstream->next = next ;
   *wchar = uchar ;
   return 0 ;

L3:
   uchar = ((uint32_t)(firstbyte & 0xF) << 6) ;
   flags = *next++ ^ 0x80 ;
   uchar = (uchar | flags) << 6 ;
   nbyte = *next++ ^ 0x80 ;
   flags = flags | nbyte ;
   uchar = (uchar | nbyte) ;
   if ((flags & 0xC0) || uchar < 0x800) return EILSEQ ;

   strstream->next = next ;
   *wchar = uchar ;
   return 0 ;

L4:
   uchar = ((uint32_t)(firstbyte & 0x7) << 6) ;
   flags = *next++ ^ 0x80 ;
   uchar = (uchar | flags) << 6 ;
   nbyte = *next++ ^ 0x80 ;
   flags = flags | nbyte ;
   uchar = (uchar | nbyte) << 6 ;
   nbyte = *next++ ^ 0x80 ;
   flags = flags | nbyte ;
   uchar = (uchar | nbyte) ;
   if ((flags & 0xC0) || uchar < 0x10000 || uchar > 0x10FFFF) return EILSEQ ;

   strstream->next = next ;
   *wchar = uchar ;
   return 0 ;
}

int skiputf8_stringstream(struct stringstream_t * strstream)
{
   size_t   size ;
   uint8_t  firstbyte ;
   size_t   strsize = size_stringstream(strstream) ;

   if (!strsize) return ENODATA ;

   firstbyte = *strstream->next ;
   size      = sizechar_utf8(firstbyte) ;

   if (!size) return EILSEQ ;

   if (strsize < size) return ENOTEMPTY ;

   skipbytes_stringstream(strstream, size) ;

   return 0 ;
}

void skipillegalutf8_strstream(struct stringstream_t * strstream)
{
   int err ;
   char32_t    wchar ;
   uint8_t     size ;

   while (strstream->next < strstream->end) {
      size = sizechar_utf8(*strstream->next) ;
      if (0 == size) {
         ++ strstream->next ; // skip wrong byte
         continue ;
      }

      err = peekutf8_stringstream(strstream, &wchar) ;
      if (err == 0 || err == ENOTEMPTY) break ;

      strstream->next += size ;
   }
}

// group: find-utf8

const uint8_t * findutf8_stringstream(const struct stringstream_t * strstream, char32_t wchar)
{
   const size_t   size = size_stringstream(strstream) ;
   const uint8_t  * found  ;
   uint8_t        utf8c[3] ;

   if (wchar < 0x80) {
      found = memchr(strstream->next, (int)wchar, size) ;

   } else if (wchar < 0x800) {
      //0b110xxxxx 0b10xxxxxx
      utf8c[0] = (uint8_t) (0x80 | (wchar & 0x3f)) ;
      uint32_t uc = wchar >> 6 ;
      uc   = (0xc0 | uc) ;
      for (found = memchr(strstream->next, (int)uc, size); found; found = memchr(found, (int)uc, (size_t)(strstream->end - found))) {
         if (utf8c[0] == found[1]) {
            break ;
         }
         found += 2 ;
      }

   } else if (wchar < 0x10000) {
      //0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      utf8c[1] = (uint8_t) (0x80 | (wchar & 0x3f)) ;
      uint32_t uc = wchar >> 6 ;
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (0xe0 | uc) ;
      for (found = memchr(strstream->next, (int)uc, size); found; found = memchr(found, (int)uc, (size_t)(strstream->end - found))) {
         if (  utf8c[0] == found[1]
               && utf8c[1] == found[2]) {
            break ;
         }
         found += 3 ;
      }

   } else if (wchar <= 0x10FFFF) {
      //0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      utf8c[2] = (uint8_t) (0x80 | (wchar & 0x3f)) ;
      uint32_t uc = wchar >> 6 ;
      utf8c[1] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (0xf0 | uc) ;
      for (found = memchr(strstream->next, (int)uc, size); found; found = memchr(found, (int)uc, (size_t)(strstream->end - found))) {
         if (  utf8c[0] == found[1]
               && utf8c[1] == found[2]
               && utf8c[2] == found[3]) {
            break ;
         }
         found += 4 ;
      }
   } else {
      goto ONABORT ;
   }

   return found ;
ONABORT:
   // IGNORE ERROR uc > 0x10FFFF
   return 0 ;
}

size_t length_utf8(const uint8_t * strstart, const uint8_t * strend)
{
   size_t len = 0 ;
   const uint8_t * next = strstart ;
   const uint8_t * end  = ((strend + sizecharmax_utf8()) > strend) ? strend : (strend-sizecharmax_utf8()) ;

   while (next < end) {
      const unsigned sizechr = sizechar_utf8(*next) ;
      next += sizechr + (0 == sizechr) ;
      len  += (0 != sizechr) ;
   }

   while (next < strend && strstart <= next) {
      const unsigned sizechr = sizechar_utf8(*next) ;
      next += sizechr + (0 == sizechr) ;
      len  += (0 != sizechr) ;
   }

   return len ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_utf8(void)
{
   const uint8_t  * const utf8str[4] = {  (const uint8_t *) "\U001fffff",
                                          (const uint8_t *) "\U0000ffff",
                                          (const uint8_t *) "\u07ff",
                                          (const uint8_t *) "\x7f"   } ;

   // TEST sizecharmax_utf8
   TEST(4 == sizecharmax_utf8()) ;

   // TEST sizechar_utf8 of string
   for (unsigned i = 0; i < lengthof(utf8str); ++i) {
      TEST(4-i == sizechar_utf8(utf8str[i][0])) ;
   }

   // TEST sizechar_utf8 of all 256 first byte values
   for (unsigned i = 0; i < 256; ++i) {
      unsigned bpc = (i < 128) ;
      /* values between [128 .. 191] and [248..255] indicate an error (returned value is 0) */
      if (192 <= i && i <= 247) {
         bpc = 0 ;
         for (unsigned i2 = i; (i2 & 0x80); i2 <<= 1) {
            ++ bpc ;
         }
      }
      TEST(bpc == sizechar_utf8(i)) ;
      TEST(bpc == sizechar_utf8(256+i)) ;
      TEST(bpc == sizechar_utf8(1024+i)) ;
   }

   // TEST islegal_utf8 of all 256 first byte values
   for (unsigned i = 0; i < 256; ++i) {
      /* values between [128 .. 191] and [248..255] indicate an error */
      bool isOK = (i < 128 || (192 <= i && i <= 247)) ;
      TEST(isOK == islegal_utf8((uint8_t)i)) ;
   }

   // TEST length_utf8
   const char * teststrings[] = { "\U001FFFFF\U00010000", "\u0800\u0999\uFFFF", "\u00A0\u00A1\u07FE\u07FF", "\x01\x02""abcde\x07e\x7f", "\U001FFFFF\uF999\u06FEY" } ;
   size_t     testlength[]    = { 2,                      3,                    4,                          9,                          4 } ;
   for (unsigned i = 0; i < lengthof(teststrings); ++i) {
      TEST(testlength[i] == length_utf8((const uint8_t*)teststrings[i], (const uint8_t*)teststrings[i]+strlen(teststrings[i]))) ;
   }

   // TEST length_utf8: empty string
   TEST(0 == length_utf8(utf8str[0], utf8str[0])) ;
   TEST(0 == length_utf8(0, 0)) ;
   TEST(0 == length_utf8((void*)(uintptr_t)-1, (void*)(uintptr_t)-1)) ;

   // TEST length_utf8: illegal sequence && last sequence not fully contained in string
   const char * teststrings2[] = { "\xFC\x80",  "b\xC0",                 "ab\xE0",  "abc\xF0" } ;
   size_t     testlength2[]    = { 0/*EILSEQ*/, 2/*last not contained*/, 3/*ditto*/, 4/*ditto*/ } ;
   for (unsigned i = 0; i < lengthof(teststrings2); ++i) {
      TEST(testlength2[i] == length_utf8((const uint8_t*)teststrings2[i], (const uint8_t*)teststrings2[i]+strlen(teststrings2[i]))) ;
   }

   // TEST decode_utf8: whole range of first byte, ENOTEMPTY, EILSEQ
   char32_t fillchar[] = { 0x0, 0x1, 0x3F } ;
   for (char32_t i = 0; i < 256; ++i) {
      for (unsigned fci = 0; fci < lengthof(fillchar); ++fci) {
         uint8_t buffer[4]  = { (uint8_t)i, (uint8_t)(0x80 + fillchar[fci]), (uint8_t)(0x80 + fillchar[fci]), (uint8_t)(0x80 + fillchar[fci]) } ;
         const uint8_t * strstart = buffer ;
         int err = 0 ;
         char32_t expect = 0 ;
         switch (sizechar_utf8(i)) {
         case 0:
            err = EILSEQ ;
            break ;
         case 1:
            expect = i ;
            err = 0 ;
            break ;
         case 2:
            expect = ((i & 0x1f) << 6) + fillchar[fci] ;
            err = (expect < 0x80) ? EILSEQ : 0 ;
            break ;
         case 3:
            expect = ((i & 0xf) << 12) + (fillchar[fci] << 6) + fillchar[fci] ;
            err = (expect < 0x800) ? EILSEQ : 0 ;
            break ;
         case 4:
            expect = ((i & 0x7) << 18) + (fillchar[fci] << 12) + (fillchar[fci] << 6) + fillchar[fci] ;
            err = (expect < 0x10000 || expect > 0x10FFFF) ? EILSEQ : 0 ;
            break ;
         }
         char32_t uchar = decode_utf8(&strstart) ;
         if (err || !expect) {
            TEST(strstart == buffer) ;
            expect = 0 ;
         } else {
            TEST(strstart == buffer+sizechar_utf8(i)) ;
         }
         TEST(uchar == expect) ;
         if (!err) {
            for (int i2 = sizechar_utf8(i)-1; i2 > 0; --i2) {
               strstart = buffer ;
               buffer[i2] = (uint8_t)(fillchar[fci]) ;
               TEST(0 == decode_utf8(&strstart)/*illegal fill byte*/) ;
               TEST(strstart == buffer) ;
               buffer[i2] = (uint8_t)(0x40 + fillchar[fci]) ;
               TEST(0 == decode_utf8(&strstart)/*illegal fill byte*/) ;
               TEST(strstart == buffer) ;
               buffer[i2] = (uint8_t)(0xc0 + fillchar[fci]) ;
               TEST(0 == decode_utf8(&strstart)/*illegal fill byte*/) ;
               TEST(strstart == buffer) ;
               buffer[i2] = (uint8_t)(0x80 + fillchar[fci]) ;
            }
         }
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_readstrstream(void)
{
   stringstream_t strstream ;
   uint32_t       uchar       = 0 ;
   uint32_t       uchar2      = 0 ;
   const size_t   utf8strsize = 8+6+4+2 ;
   const uint8_t  * utf8str   = { (const uint8_t *)(
                                    "\U0010ffff" "\U00010000"
                                    "\U0000ffff" "\U00000800"
                                    "\u07ff"     "\xC2\x80"
                                    "\x7f"       "\x00")
                              } ;

   // TEST nextutf8_stringstream, peekutf8_stringstream: 4 byte
   TEST(0 == init_stringstream(&strstream, utf8str, utf8str+utf8strsize)) ;
   TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
   TEST(uchar2 == 0x10ffff) ;
   TEST(strstream.next == utf8str) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(uchar == 0x10ffff) ;
   TEST(strstream.next == utf8str + 4) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;
   TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
   TEST(uchar2 == 0x10000) ;
   TEST(strstream.next == utf8str + 4) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(uchar == 0x10000) ;
   TEST(strstream.next == utf8str + 8) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;

   // TEST nextutf8_stringstream, peekutf8_stringstream: 3 byte
   TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
   TEST(uchar2 == 0xffff) ;
   TEST(strstream.next == utf8str + 8) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(uchar == 0xffff) ;
   TEST(strstream.next == utf8str + 11) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;
   TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
   TEST(uchar2 == 0x800) ;
   TEST(strstream.next == utf8str + 11) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(uchar == 0x800) ;
   TEST(strstream.next == utf8str + 14) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;

   // TEST nextutf8_stringstream, peekutf8_stringstream: 2 byte
   TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
   TEST(uchar2 == 0x7ff) ;
   TEST(strstream.next == utf8str + 14) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(uchar == 0x7ff) ;
   TEST(strstream.next == utf8str + 16) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;
   TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
   TEST(uchar2 == 0x80) ;
   TEST(strstream.next == utf8str + 16) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(uchar == 0x80) ;
   TEST(strstream.next == utf8str + 18) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;

   // TEST nextutf8_stringstream, peekutf8_stringstream: 1 byte
   TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
   TEST(uchar2 == 0x7F) ;
   TEST(strstream.next == utf8str + 18) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(uchar == 0x7F) ;
   TEST(strstream.next == utf8str + 19) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;
   TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
   TEST(uchar2 == 0x00) ;
   TEST(strstream.next == utf8str + 19) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(uchar == 0x00) ;
   TEST(strstream.next == utf8str + 20) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;

   // TEST nextutf8_stringstream, peekutf8_stringstream: ENODATA
   TEST(0 == init_stringstream(&strstream, utf8str, utf8str)) ;
   TEST(ENODATA == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(ENODATA == peekutf8_stringstream(&strstream, &uchar)) ;

   // TEST nextutf8_stringstream, peekutf8_stringstream: whole range of first byte, ENOTEMPTY, EILSEQ
   char32_t fillchar[] = { 0x0, 0x1, 0x3F } ;
   for (char32_t i = 0; i < 256; ++i) {
      for (unsigned fci = 0; fci < lengthof(fillchar); ++fci) {
         uint8_t buffer[4] = { (uint8_t)i, (uint8_t)(0x80 + fillchar[fci]), (uint8_t)(0x80 + fillchar[fci]), (uint8_t)(0x80 + fillchar[fci]) } ;
         TEST(0 == init_stringstream(&strstream, buffer, buffer+4)) ;
         int err = 0 ;
         char32_t expect = 0 ;
         switch (sizechar_utf8(i)) {
         case 0:
            err = EILSEQ ;
            break ;
         case 1:
            expect = i ;
            err = 0 ;
            break ;
         case 2:
            expect = ((i & 0x1f) << 6) + fillchar[fci] ;
            err = (expect < 0x80) ? EILSEQ : 0 ;
            break ;
         case 3:
            expect = ((i & 0xf) << 12) + (fillchar[fci] << 6) + fillchar[fci] ;
            err = (expect < 0x800) ? EILSEQ : 0 ;
            break ;
         case 4:
            expect = ((i & 0x7) << 18) + (fillchar[fci] << 12) + (fillchar[fci] << 6) + fillchar[fci] ;
            err = (expect < 0x10000 || expect > 0x10FFFF) ? EILSEQ : 0 ;
            break ;
         }
         uchar = err ? expect : (expect + 1) ;
         TEST(err == peekutf8_stringstream(&strstream, &uchar)) ;
         TEST(uchar == expect) ;
         TEST(strstream.next == buffer) ;
         TEST(strstream.end  == buffer+4) ;
         uchar = err ? expect : (expect + 1) ;
         TEST(err == nextutf8_stringstream(&strstream, &uchar)) ;
         if (err) {
            TEST(strstream.next == buffer) ;
            TEST(strstream.end  == buffer+4) ;
         } else {
            TEST(strstream.next == buffer+sizechar_utf8(i)) ;
            TEST(strstream.end  == buffer+4) ;
         }
         TEST(uchar == expect) ;
         if (sizechar_utf8(i) > 1) {
            TEST(0 == init_stringstream(&strstream, buffer, buffer + sizechar_utf8(i)-1)) ;
            TEST(ENOTEMPTY == peekutf8_stringstream(&strstream, &uchar)) ;
            TEST(strstream.next == buffer) ;
            TEST(strstream.end  == buffer+sizechar_utf8(i)-1) ;
            TEST(ENOTEMPTY == nextutf8_stringstream(&strstream, &uchar)) ;
            TEST(strstream.next == buffer) ;
            TEST(strstream.end  == buffer+sizechar_utf8(i)-1) ;
         }
         if (!err) {
            for (int i2 = sizechar_utf8(i)-1; i2 > 0; --i2) {
               TEST(0 == init_stringstream(&strstream, buffer, buffer+4)) ;
               buffer[i2] = (uint8_t)(fillchar[fci]) ;
               TEST(EILSEQ == peekutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == nextutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               buffer[i2] = (uint8_t)(0x40 + fillchar[fci]) ;
               TEST(EILSEQ == peekutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == nextutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               buffer[i2] = (uint8_t)(0xc0 + fillchar[fci]) ;
               TEST(EILSEQ == peekutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == nextutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               buffer[i2] = (uint8_t)(0x80 + fillchar[fci]) ;
               TEST(strstream.next == buffer) ;
               TEST(strstream.end  == buffer+4) ;
            }
         }
      }
   }

   // TEST skiputf8_stringstream: 4 byte
   TEST(0 == init_stringstream(&strstream, utf8str, utf8str+utf8strsize)) ;
   TEST(0 == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + 4) ;
   TEST(strstream.end  == utf8str + utf8strsize) ;
   TEST(0 == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + 8) ;
   TEST(strstream.end  == utf8str + utf8strsize) ;

   // TEST skiputf8_stringstream: 3 byte
   TEST(0 == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + 11) ;
   TEST(strstream.end  == utf8str + utf8strsize) ;
   TEST(0 == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + 14) ;
   TEST(strstream.end  == utf8str + utf8strsize) ;

   // TEST skiputf8_stringstream: 2 byte
   TEST(0 == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + 16) ;
   TEST(strstream.end  == utf8str + utf8strsize) ;
   TEST(0 == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + 18) ;
   TEST(strstream.end  == utf8str + utf8strsize) ;

   // TEST skiputf8_stringstream: 1 byte
   TEST(0 == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + 19) ;
   TEST(strstream.end  == utf8str+utf8strsize) ;
   TEST(0 == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + 20) ;
   TEST(strstream.end  == utf8str + utf8strsize) ;

   // TEST skiputf8_stringstream: ENODATA
   TEST(ENODATA == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == utf8str + utf8strsize) ;
   TEST(strstream.end  == utf8str + utf8strsize) ;

   // TEST skiputf8_stringstream: whole range of first byte, ENOTEMPTY, EILSEQ
   for (char32_t i = 0; i < 256; ++i) {
      uint8_t buffer[4] = { (uint8_t)i, (uint8_t)0xff, (uint8_t)0xff, (uint8_t)0xff } ;
      TEST(0 == init_stringstream(&strstream, buffer, buffer+4)) ;
      int err = sizechar_utf8(i) ? 0 : EILSEQ ;
      TEST(err == skiputf8_stringstream(&strstream)) ;
      if (err) {
         TEST(strstream.next == buffer) ;
         TEST(strstream.end  == buffer+4) ;
      } else {
         TEST(strstream.next == buffer+sizechar_utf8(i)) ;
         TEST(strstream.end  == buffer+4) ;
      }
      if (sizechar_utf8(i) > 1) {
         TEST(0 == init_stringstream(&strstream, buffer, buffer + sizechar_utf8(i)-1)) ;
         TEST(ENOTEMPTY == skiputf8_stringstream(&strstream)) ;
         TEST(strstream.next == buffer) ;
         TEST(strstream.end  == buffer+sizechar_utf8(i)-1) ;
      }
   }

   // TEST skipillegalutf8_strstream: first byte encode illegal
   uint8_t illseq[256] ;
   TEST(sizeof(illseq) > utf8strsize) ;
   memcpy(illseq, utf8str, utf8strsize) ;
   illseq[0] = 0x80 ;
   illseq[4] = 0x80 ;
   illseq[8] = 0x80 ;
   illseq[11] = 0x80 ;
   illseq[14] = 0x80 ;
   illseq[16] = 0x80 ;
   TEST(0 == init_stringstream(&strstream, illseq, illseq+utf8strsize)) ;
   skipillegalutf8_strstream(&strstream) ;
   TEST(strstream.next == illseq+utf8strsize-2) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(0x7f == uchar) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(0 == uchar) ;
   TEST(ENODATA == nextutf8_stringstream(&strstream, &uchar)) ;

   // TEST skipillegalutf8_strstream: follow byte encoded illegal
   memcpy(illseq, utf8str, utf8strsize) ;
   illseq[1] = 0x0F ;
   illseq[5] = 0x0F ;
   illseq[9] = 0x0F ;
   illseq[12] = 0x0F ;
   illseq[15] = 0x0F ;
   illseq[17] = 0x0F ;
   TEST(0 == init_stringstream(&strstream, illseq, illseq+utf8strsize)) ;
   skipillegalutf8_strstream(&strstream) ;
   TEST(strstream.next == illseq+utf8strsize-2) ;

   // TEST skipillegalutf8_strstream: last not fully encoded byte is not skipped
   memcpy(illseq, utf8str, utf8strsize) ;
   TEST(0 == init_stringstream(&strstream, illseq, illseq+7)) ;
   illseq[1] = 0x0F ;
   skipillegalutf8_strstream(&strstream) ;
   TEST(ENOTEMPTY == nextutf8_stringstream(&strstream, &uchar)/*not skipped*/) ;
   TEST(strstream.next == illseq+4) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_findstrstream(void)
{
   const char     * utf8str = "\U0010fff0\U0010ffff abcd\U0000fff0\U0000ffff abcd\u07f0\u07ff abcd\x7f abcd" ;
   stringstream_t strstream = stringstream_INIT((const uint8_t*)utf8str, (const uint8_t*)utf8str+strlen(utf8str)) ;
   const uint8_t  * found ;

   // TEST findutf8_stringstream
   found = findutf8_stringstream(&strstream, 0x10ffffu) ;
   TEST(found ==  4+(const uint8_t*)utf8str) ;
   found = findutf8_stringstream(&strstream, 0xffffu) ;
   TEST(found == 16+(const uint8_t*)utf8str) ;
   found = findutf8_stringstream(&strstream, 0x7ffu) ;
   TEST(found == 26+(const uint8_t*)utf8str) ;
   found = findutf8_stringstream(&strstream, 0x7fu) ;
   TEST(found == 33+(const uint8_t*)utf8str) ;
   found = findutf8_stringstream(&strstream, (char32_t) 'a') ;
   TEST(found == 9+(const uint8_t*)utf8str) ;

   // TEST findutf8_stringstream: character not in stream
   TEST(0 == findutf8_stringstream(&strstream, (char32_t) 0x10fffe)) ;
   TEST(0 == findutf8_stringstream(&strstream, (char32_t) 0xfffe)) ;
   TEST(0 == findutf8_stringstream(&strstream, (char32_t) 0x7fe)) ;
   TEST(0 == findutf8_stringstream(&strstream, (char32_t) 0x7e)) ;

   // TEST findutf8_stringstream: codepoint out of range
   TEST(0 == findutf8_stringstream(&strstream, (char32_t)-1)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_speed(void)
{
   systimer_t  timer = systimer_INIT_FREEABLE ;
   uint64_t    microsec ;
   uint64_t    result[2]  = { UINT64_MAX, UINT64_MAX } ;
   union {
      uint32_t initval ;
      uint8_t  buffer[1024]  ;
   }           data       = { .buffer = "\U0010FFFF" } ;

   // prepare
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC)) ;

   for (size_t i = 1; i < sizeof(data.buffer)/4; ++i) {
      *(uint32_t*)(&data.buffer[4*i]) = data.initval ;
   }

   for (unsigned testrepeat = 0 ; testrepeat < 5; ++testrepeat) {
      TEST(0 == startinterval_systimer(timer, &(timevalue_t) { .nanosec = 1000 } )) ;
      for (unsigned decoderepeat = 0 ; decoderepeat < 5; ++decoderepeat) {
         const uint8_t * strstream = data.buffer ;
         for (unsigned nrchars = 0 ; nrchars < sizeof(data.buffer)/4; ++nrchars) {
            char32_t uchar = 0 ;
            uchar = decode_utf8(&strstream) ;
            TEST(uchar == 0x10FFFF) ;
         }
         TEST(strstream == data.buffer+sizeof(data.buffer)) ;
      }
      TEST(0 == expirationcount_systimer(timer, &microsec)) ;
      TEST(0 == stop_systimer(timer)) ;
      if (result[0] > microsec) result[0] = microsec ;
   }

   for (unsigned testrepeat = 0 ; testrepeat < 5; ++testrepeat) {
      TEST(0 == startinterval_systimer(timer, &(timevalue_t) { .nanosec = 1000 } )) ;
      for (unsigned decoderepeat = 0 ; decoderepeat < 5; ++decoderepeat) {
         stringstream_t strstream = stringstream_INIT(data.buffer, data.buffer + sizeof(data.buffer)) ;
         for (unsigned nrchars = 0 ; nrchars < sizeof(data.buffer)/4; ++nrchars) {
            char32_t uchar = 0 ;
            TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
            TEST(uchar == 0x10FFFF) ;
         }
         TEST(strstream.next == strstream.end) ;
      }
      TEST(0 == expirationcount_systimer(timer, &microsec)) ;
      TEST(0 == stop_systimer(timer)) ;
      if (result[1] > microsec) result[1] = microsec ;
   }

   if (result[1] <= result[0]) {
      CPRINTF_LOG(TEST, "** decode_utf8 is not faster ** ") ;
   }

   TEST(0 == free_systimer(&timer)) ;

   return 0 ;
ONABORT:
   free_systimer(&timer) ;
   return EINVAL ;
}

int unittest_string_utf8()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_utf8())           goto ONABORT ;
   if (test_readstrstream())  goto ONABORT ;
   if (test_findstrstream())  goto ONABORT ;
   if (test_speed())          goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
