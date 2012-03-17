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
                                       4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1 /*240..247*/ /*248..251*/ /*252..253*/ /*254..255 error*/
                                    } ;

// section: conststring_t

bool nextcharutf8_conststring(struct conststring_t * str, unicode_t * wchar)
{
   uint8_t  firstbyte ;
   uint8_t  size ;
   uint32_t uchar ;

   if (!str->size) return false ;

   firstbyte = *(str->addr++) ;
   size      = g_utf8_bytesperchar[firstbyte] ;

   if (size > str->size) {
      size = (uint8_t)str->size ;
   }

   str->size -= size ;

   switch (size) {
   default:
   case 1:  *wchar = firstbyte ;
            break ;
   case 2:  uchar = (uint32_t) ((firstbyte & 0x1F) << 6) ;
            *wchar = uchar | (*(str->addr++) & 0x3F) ;
            break ;
   case 3:  uchar = (uint32_t) ((firstbyte & 0xF) << 6) ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            *wchar = uchar | (*(str->addr++) & 0x3F) ;
            break ;
   case 4:  uchar = (uint32_t) ((firstbyte & 0x7) << 6) ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            *wchar = uchar | (*(str->addr++) & 0x3F) ;
            break ;
   case 5:  uchar = (uint32_t) ((firstbyte & 0x3) << 6) ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            *wchar = uchar | (*(str->addr++) & 0x3F) ;
            break ;
   case 6:  uchar = (uint32_t) ((firstbyte & 0x1) << 6) ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            uchar = (uchar | (*(str->addr++) & 0x3F)) << 6 ;
            *wchar = uchar | (*(str->addr++) & 0x3F) ;
            break ;
   }

   return true ;
}

bool skipcharutf8_conststring(struct conststring_t * str)
{
   size_t   size ;
   uint8_t  firstbyte ;

   if (!str->size) return false ;

   firstbyte = *str->addr ;
   size      = g_utf8_bytesperchar[firstbyte] ;

   if (size > str->size) {
      size = (uint8_t)str->size ;
   }

   str->addr += size ;
   str->size -= size ;

   return true ;
}


// section: utf8cstring

const uint8_t * findunicode_utf8cstring(const uint8_t * utf8cstr, unicode_t wchar)
{
   const char  * pos = (const char*) utf8cstr ;
   uint32_t    uc    = wchar ;
   uint8_t     utf8c[3] ;

   if (uc < 0x80) {
      pos = strchrnul(pos, (int)uc) ;
   } else if (uc < 0x800) {    //0b110xxxxx 0b10xxxxxx
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (uint8_t) (0xc0 | (uc )) ;
      for(pos = strchrnul(pos, (int)uc); 0 != (*pos) ; pos = strchrnul(pos, (int)uc)) {
         if (  (char)utf8c[0] == *(++pos)) {
            pos -= 1 ;
            break ;
         }
      }
   } else if (uc < 0x10000) {  //0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      utf8c[1] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (uint8_t) (0xe0 | uc) ;
      for(pos = strchrnul(pos, (int)uc); 0 != (*pos) ; pos = strchrnul(pos, (int)uc)) {
         if (     (char)utf8c[0] == *(++pos)
               && (char)utf8c[1] == *(++pos)) {
            pos -= 2 ;
            break ;
         }
      }
   } else if (uc < 0x200000) { //0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      utf8c[2] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      utf8c[1] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (uint8_t) (0xf0 | uc) ;
      for(pos = strchrnul(pos, (int)uc); 0 != (*pos) ; pos = strchrnul(pos, (int)uc)) {
         if (     (char)utf8c[0] == *(++pos)
               && (char)utf8c[1] == *(++pos)
               && (char)utf8c[2] == *(++pos)) {
            pos -= 3 ;
            break ;
         }
      }
   } else {
      // ignore error uc >= 0x200000
      pos += strlen(pos) ;
   }

   return (const uint8_t*) pos ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_utf8(void)
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
      /* values between [128 .. 191] and [254..255] indicate an error (returned value is 1) */
      if (  0xFD >= i
         && 0xC0 <= i) {
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
   uint32_t       uchar ;
   const uint8_t  * utf8str = (const uint8_t *) (
                                 "\U001fffff"
                                 "\U00010000"
                                 "\U0000ffff"
                                 "\U00000800"
                                 "\u07ff"
                                 "\xC2\x80"
                                 "\x7f"
                                 "\x00" ) ;

   // TEST: nextcharutf8_conststring 4 byte
   init_conststring(&str, 8+6+4+2, utf8str) ;
   TEST(true == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x1fffff) ;
   TEST(str.addr == utf8str + 4) ;
   TEST(str.size == 20 - 4) ;
   TEST(true == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x10000) ;
   TEST(str.addr == utf8str + 8) ;
   TEST(str.size == 20 - 8) ;

   // TEST nextcharutf8_conststring 3 byte
   TEST(true == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0xffff) ;
   TEST(str.addr == utf8str + 11) ;
   TEST(str.size == 20 - 11) ;
   TEST(true == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x800) ;
   TEST(str.addr == utf8str + 14) ;
   TEST(str.size == 20 - 14) ;

   // TEST nextcharutf8_conststring two byte
   TEST(true == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x7ff) ;
   TEST(str.addr == utf8str + 16) ;
   TEST(str.size == 20 - 16) ;
   TEST(true == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x80) ;
   TEST(str.addr == utf8str + 18) ;
   TEST(str.size == 20 - 18) ;

   // TEST nextcharutf8_conststring one byte
   TEST(true == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x7F) ;
   TEST(str.addr == utf8str + 19) ;
   TEST(str.size == 20 - 19) ;
   TEST(true == nextcharutf8_conststring(&str, &uchar)) ;
   TEST(uchar    == 0x00) ;
   TEST(str.addr == utf8str + 20) ;
   TEST(str.size == 20 - 20) ;

   // TEST nextcharutf8_conststring empty string
   TEST(false == nextcharutf8_conststring(&str, &uchar)) ;

   // TEST: skipcharutf8_conststring 4 byte
   init_conststring(&str, 8+6+4+2, utf8str) ;
   TEST(true == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 4) ;
   TEST(str.size == 20 - 4) ;
   TEST(true == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 8) ;
   TEST(str.size == 20 - 8) ;

   // TEST skipcharutf8_conststring 3 byte
   TEST(true == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 11) ;
   TEST(str.size == 20 - 11) ;
   TEST(true == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 14) ;
   TEST(str.size == 20 - 14) ;

   // TEST skipcharutf8_conststring two byte
   TEST(true == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 16) ;
   TEST(str.size == 20 - 16) ;
   TEST(true == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 18) ;
   TEST(str.size == 20 - 18) ;

   // TEST skipcharutf8_conststring one byte
   TEST(true == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 19) ;
   TEST(str.size == 20 - 19) ;
   TEST(true == skipcharutf8_conststring(&str)) ;
   TEST(str.addr == utf8str + 20) ;
   TEST(str.size == 20 - 20) ;

   // TEST skipcharutf8_conststring empty string
   TEST(false == skipcharutf8_conststring(&str)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_utf8cstring(void)
{
   const uint8_t  * const utf8cstr = (const uint8_t *) "\U001fffff abcd\U0000ffff abcd\u07ff abcd\x7f abcd" ;
   const uint8_t  * cstr ;

   // TEST findunicode_utf8cstring (find char)
   cstr = findunicode_utf8cstring(utf8cstr, L'\U001fffff') ;
   TEST(cstr == utf8cstr) ;
   cstr = findunicode_utf8cstring(utf8cstr, L'\U0000ffff') ;
   TEST(cstr == 9+utf8cstr) ;
   cstr = findunicode_utf8cstring(utf8cstr, L'\u07ff') ;
   TEST(cstr == 17+utf8cstr) ;
   cstr = findunicode_utf8cstring(utf8cstr, L'\x7f') ;
   TEST(cstr == 24+utf8cstr) ;
   cstr = findunicode_utf8cstring(utf8cstr, L'\x7e') ;

   // TEST findunicode_utf8cstring (end of string)
   cstr = utf8cstr + strlen((const char*)utf8cstr) ;
   TEST(cstr == findunicode_utf8cstring(utf8cstr, L'\U001ffffe')) ;
   TEST(cstr == findunicode_utf8cstring(utf8cstr, L'\U0000fffe')) ;
   TEST(cstr == findunicode_utf8cstring(utf8cstr, L'\u07fe')) ;
   TEST(cstr == findunicode_utf8cstring(utf8cstr, L'\x7e')) ;

   // TEST findunicode_utf8cstring (error)
   cstr = utf8cstr + strlen((const char*)utf8cstr) ;
   TEST(cstr == findunicode_utf8cstring(utf8cstr, L'\U0fffffff')) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_string_utf8()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_utf8())           goto ABBRUCH ;
   if (test_conststring())    goto ABBRUCH ;
   if (test_utf8cstring())    goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
