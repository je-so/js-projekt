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
   (C) 2011 Jörg Seebohn

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
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
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
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/*128..191 not the first byte => error*/
                                       2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                                       2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,/*192..223*/
                                       3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,/*224..239*/
                                       4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1 /*240..247*/ /*248..251*/ /*252..253*/ /*254..255 error*/
                                    } ;

// section: conststring_t

int nextcharutf8_conststring(struct conststring_t * str, unicode_t * wchar)
{
   uint8_t        firstbyte ;
   uint8_t        size ;
   uint8_t        flags ;
   uint8_t        nbyte ;
   uint32_t       uchar ;
   const uint8_t  * addr = str->addr ;

   if (!str->size) return ENODATA ;

   firstbyte = *addr++ ;
   size      = g_utf8_bytesperchar[firstbyte] ;

   if (size > str->size) return EILSEQ ;

   switch (size) {
   default:
   case 1:  if (firstbyte >= 0x80) return EILSEQ ;
            *wchar = firstbyte ;
            break ;
   case 2:  uchar = (uint32_t) ((firstbyte & 0x1F) << 6) ;
            flags = *addr++ ^ 0x80 ;
            uchar = uchar | flags ;
            if ((flags & 0xC0) || uchar < 0x80)  return EILSEQ ;
            *wchar = uchar ;
            break ;
   case 3:  uchar = (uint32_t) ((firstbyte & 0xF) << 6) ;
            flags = *addr++ ^ 0x80 ;
            uchar = (uchar | flags) << 6 ;
            nbyte = *addr++ ^ 0x80 ;
            flags = flags | nbyte ;
            uchar = (uchar | nbyte) ;
            if ((flags & 0xC0) || uchar < 0x800)  return EILSEQ ;
            *wchar = uchar ;
            break ;
   case 4:  uchar = (uint32_t) ((firstbyte & 0x7) << 6) ;
            flags = *addr++ ^ 0x80 ;
            uchar = (uchar | flags) << 6 ;
            nbyte = *addr++ ^ 0x80 ;
            flags = flags | nbyte ;
            uchar = (uchar | nbyte) << 6 ;
            nbyte = *addr++ ^ 0x80 ;
            flags = flags | nbyte ;
            uchar = (uchar | nbyte) ;
            if ((flags & 0xC0) || uchar < 0x10000 || uchar > 0x10FFFF)  return EILSEQ ;
            *wchar = uchar ;
            break ;
   }

   str->addr  = addr ;
   str->size -= size ;

   return 0 ;
}

int skipcharutf8_conststring(struct conststring_t * str)
{
   size_t   size ;
   uint8_t  firstbyte ;

   if (!str->size) return ENODATA ;

   firstbyte = *str->addr ;
   size      = g_utf8_bytesperchar[firstbyte] ;

   if (size > str->size) return EILSEQ ;

   str->addr += size ;
   str->size -= size ;

   return 0 ;
}

const uint8_t * findcharutf8_conststring(const struct conststring_t * str, unicode_t wchar)
{
   const uint8_t  * endstr = str->addr + str->size ;
   const uint8_t  * found  ;
   uint8_t        utf8c[3] ;

   if (wchar < 0x80) {
      found = memchr(str->addr, (int)wchar, str->size) ;
   } else if (wchar < 0x800) {    //0b110xxxxx 0b10xxxxxx
      utf8c[0] = (uint8_t) (0x80 | (wchar & 0x3f)) ;
      uint32_t uc = wchar >> 6 ;
      uc   = (0xc0 | uc) ;
      for(found = memchr(str->addr, (int)uc, str->size); found ; found = memchr(found, (int)uc, (size_t)(endstr - found))) {
         if (  utf8c[0] == found[1]) {
            break ;
         }
         found += 2 ;
      }
   } else if (wchar < 0x10000) {  //0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      utf8c[1] = (uint8_t) (0x80 | (wchar & 0x3f)) ;
      uint32_t uc = wchar >> 6 ;
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (0xe0 | uc) ;
      for(found = memchr(str->addr, (int)uc, str->size); found ; found = memchr(found, (int)uc, (size_t)(endstr - found))) {
         if (     utf8c[0] == found[1]
               && utf8c[1] == found[2]) {
            break ;
         }
         found += 3 ;
      }
   } else if (wchar <= 0x10FFFF) { //0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      utf8c[2] = (uint8_t) (0x80 | (wchar & 0x3f)) ;
      uint32_t uc = wchar >> 6 ;
      utf8c[1] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (0xf0 | uc) ;
      for(found = memchr(str->addr, (int)uc, str->size); found ; found = memchr(found, (int)uc, (size_t)(endstr - found))) {
         if (     utf8c[0] == found[1]
               && utf8c[1] == found[2]
               && utf8c[2] == found[3]) {
            break ;
         }
         found += 4 ;
      }
   } else {
      goto ABBRUCH ;
   }

   return found ;
ABBRUCH:
   // IGNORE ERROR uc > 0x10FFFF
   return 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_sizecharutf8(void)
{
   const uint8_t  * const utf8str[4] = {  (const uint8_t *) "\U001fffff",
                                          (const uint8_t *) "\U0000ffff",
                                          (const uint8_t *) "\u07ff",
                                          (const uint8_t *) "\x7f"   } ;

   // TEST sizechar_utf8 of string
   for(unsigned i = 0; i < nrelementsof(utf8str); ++i) {
      TEST(4-i == sizechar_utf8(utf8str[i][0])) ;
   }

   // TEST sizechar_utf8 of all 256 first byte values
   for(unsigned i = 0; i < 256; ++i) {
      unsigned bpc = 1 ;
      /* values between [128 .. 191] and [248..255] indicate an error (returned value is 1) */
      if (192 <= i && i <= 247 ) {
         unsigned i2 = i << 1 ;
         while (i2 & 0x80) {
            i2 <<= 1 ;
            ++ bpc ;
         }
      }
      TEST(bpc == sizechar_utf8(i)) ;
      TEST(bpc == sizechar_utf8(256+i)) ;
      TEST(bpc == sizechar_utf8(1024+i)) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_conststring(void)
{
   conststring_t  str ;
   conststring_t  old ;
   uint32_t       uchar ;
   const uint8_t  * utf8str = (const uint8_t *) (
                                 "\U0010ffff"
                                 "\U00010000"
                                 "\U0000ffff"
                                 "\U00000800"
                                 "\u07ff"
                                 "\xC2\x80"
                                 "\x7f"
                                 "\x00" ) ;

   // TEST: nextcharutf8_conststring 4 byte
   init_conststring(&str, 8+6+4+2, utf8str) ;
   TEST(0 == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x10ffff) ;
   TEST(str.addr == utf8str + 4) ;
   TEST(str.size == 20 - 4) ;
   TEST(0 == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x10000) ;
   TEST(str.addr == utf8str + 8) ;
   TEST(str.size == 20 - 8) ;

   // TEST nextcharutf8_conststring 3 byte
   TEST(0 == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0xffff) ;
   TEST(str.addr == utf8str + 11) ;
   TEST(str.size == 20 - 11) ;
   TEST(0 == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x800) ;
   TEST(str.addr == utf8str + 14) ;
   TEST(str.size == 20 - 14) ;

   // TEST nextcharutf8_conststring two byte
   TEST(0 == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x7ff) ;
   TEST(str.addr == utf8str + 16) ;
   TEST(str.size == 20 - 16) ;
   TEST(0 == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x80) ;
   TEST(str.addr == utf8str + 18) ;
   TEST(str.size == 20 - 18) ;

   // TEST nextcharutf8_conststring one byte
   TEST(0 == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x7F) ;
   TEST(str.addr == utf8str + 19) ;
   TEST(str.size == 20 - 19) ;
   TEST(0 == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x00) ;
   TEST(str.addr == utf8str + 20) ;
   TEST(str.size == 20 - 20) ;

   // TEST: skipcharutf8_conststring 4 byte
   init_conststring(&str, 8+6+4+2, utf8str) ;
   TEST(0 == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 4) ;
   TEST(str.size == 20 - 4) ;
   TEST(0 == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 8) ;
   TEST(str.size == 20 - 8) ;

   // TEST skipcharutf8_conststring 3 byte
   TEST(0 == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 11) ;
   TEST(str.size == 20 - 11) ;
   TEST(0 == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 14) ;
   TEST(str.size == 20 - 14) ;

   // TEST skipcharutf8_conststring two byte
   TEST(0 == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 16) ;
   TEST(str.size == 20 - 16) ;
   TEST(0 == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 18) ;
   TEST(str.size == 20 - 18) ;

   // TEST skipcharutf8_conststring one byte
   TEST(0 == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 19) ;
   TEST(str.size == 20 - 19) ;
   TEST(0 == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 20) ;
   TEST(str.size == 20 - 20) ;

   // TEST ENODATA (empty string)
   old = str ;
   TEST(ENODATA == skipcharutf8_conststring(&str)) ;
   TEST(ENODATA == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;

   // TEST EILSEQ (wrong encodings)
      // 1 byte code > 127
   str = (conststring_t) conststring_INIT_CSTR("\x80") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
      // 2 byte code < 128
   str = (conststring_t) conststring_INIT_CSTR("\xC1\xBF") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
      // 2 byte code second byte not in [0x80 .. 0xBF]
   str = (conststring_t) conststring_INIT_CSTR("\xC2\x7F") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
   str = (conststring_t) conststring_INIT_CSTR("\xC2\xC0") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
      // 3 byte code < 0x800
   str = (conststring_t) conststring_INIT_CSTR("\xE0\x9F\xBF") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
      // 3 byte code second/3rd byte not in [0x80 .. 0xBF]
   str = (conststring_t) conststring_INIT_CSTR("\xE1\x1F\xBF") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
   str = (conststring_t) conststring_INIT_CSTR("\xE1\xBF\xFF") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
      // 4 byte code < 10000
   str = (conststring_t) conststring_INIT_CSTR("\xF0\x8F\xBF\xBF") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
      // 4 byte code > 10FFFF
   str = (conststring_t) conststring_INIT_CSTR("\xF4\x90\x80\x80") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
      // 4 byte code second/3rd/4th byte not in [0x80 .. 0xBF]
   str = (conststring_t) conststring_INIT_CSTR("\xF1\x7F\xBF\xBF") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
   str = (conststring_t) conststring_INIT_CSTR("\xF1\xBF\xFF\xBF") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
   str = (conststring_t) conststring_INIT_CSTR("\xF1\xBF\xBF\x0F") ;
   old = str ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;

   // TEST EILSEQ (not that many bytes)
   str = (conststring_t) conststring_INIT_CSTR("\U0010ffff") ;
   -- str.size ;
   old = str ;
   TEST(EILSEQ == skipcharutf8_conststring(&str)) ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
   str = (conststring_t) conststring_INIT_CSTR("\U0000ffff") ;
   -- str.size ;
   old = str ;
   TEST(EILSEQ == skipcharutf8_conststring(&str)) ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;
   str = (conststring_t) conststring_INIT_CSTR("\u07ff") ;
   -- str.size ;
   old = str ;
   TEST(EILSEQ == skipcharutf8_conststring(&str)) ;
   TEST(EILSEQ == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(old.size == str.size && old.addr == str.addr) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_findchar(void)
{
   const char     * utf8str = "\U0010fff0\U0010ffff abcd\U0000fff0\U0000ffff abcd\u07f0\u07ff abcd\x7f abcd" ;
   conststring_t  cstr      = conststring_INIT_CSTR(utf8str) ;
   const uint8_t  * found ;

   // TEST findcharutf8_conststring (find char)
   found = findcharutf8_conststring(&cstr, (uint32_t) 0x10ffff) ;
   TEST(found ==  4+(const uint8_t*)utf8str) ;
   found = findcharutf8_conststring(&cstr, (uint32_t) 0xffff) ;
   TEST(found == 16+(const uint8_t*)utf8str) ;
   found = findcharutf8_conststring(&cstr, (uint32_t) 0x7ff) ;
   TEST(found == 26+(const uint8_t*)utf8str) ;
   found = findcharutf8_conststring(&cstr, (uint32_t) 0x7f) ;
   TEST(found == 33+(const uint8_t*)utf8str) ;
   found = findcharutf8_conststring(&cstr, (uint32_t) 'a') ;
   TEST(found == 9+(const uint8_t*)utf8str) ;

   // TEST findcharutf8_conststring (end of string)
   TEST(0 == findcharutf8_conststring(&cstr, (uint32_t) 0x10fffe)) ;
   TEST(0 == findcharutf8_conststring(&cstr, (uint32_t) 0xfffe)) ;
   TEST(0 == findcharutf8_conststring(&cstr, (uint32_t) 0x7fe)) ;
   TEST(0 == findcharutf8_conststring(&cstr, (uint32_t) 0x7e)) ;

   // TEST findcharutf8_conststring (error)
   TEST(0 == findcharutf8_conststring(&cstr, (uint32_t) 0x110000)) ;

   // TEST findbyte_conststring (found)
   for(unsigned i = 0; i < 14; ++i) {
      if (i == 4) i += 4 ;
      TEST(&cstr.addr[i] == findbyte_conststring(&cstr, cstr.addr[i])) ;
   }

   // TEST findbyte_conststring (not found)
   TEST(0 == findbyte_conststring(&cstr, 0)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_string_utf8()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_sizecharutf8())   goto ABBRUCH ;
   if (test_conststring())    goto ABBRUCH ;
   if (test_findchar())       goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
