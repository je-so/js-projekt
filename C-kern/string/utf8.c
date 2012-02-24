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
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


const uint8_t * skipchar_utf8cstring(const uint8_t * utf8cstr)
{
   const uint8_t  * nextchar = utf8cstr ;
   uint8_t        c          = *nextchar ;

   if (c) {
      do {
         ++ nextchar ;
         c = *nextchar ;
      } while(0x80 == (c&0xc0)) ;
   }

   return nextchar ;
}

const uint8_t * findwcharnul_utf8cstring(const uint8_t * utf8cstr, wchar_t wchar)
{
   const char  * pos = (const char*) utf8cstr ;
   uint32_t    uc    = (uint32_t) wchar ;
   uint8_t     utf8c[3] ;

   static_assert( sizeof(wchar_t) == sizeof(uint32_t), ) ;

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

int cmpcaseascii_utf8cstring(const uint8_t * utf8cstr, const char * cstr, size_t size)
{

   for(size_t i = 0; i < size; ++ i) {
      uint8_t c2 = ((const uint8_t*)cstr)[i] ;
      uint8_t x  = (c2 ^ utf8cstr[i]) ;
      if (x) {
         static_assert(0x20 == ('a' - 'A'), ) ;
         if (0x20 == x) {
            x |= c2 ;
            if ('a' <= x && x <= 'z') {
               continue ;
            }
         }
         return (int) utf8cstr[i] - c2 ;
      }
   }

   return 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_utf8cstring(void)
{
   const uint8_t  * const utf8cstr = (const uint8_t *) "\U001fffff abcd\U0000ffff abcd\u07ff abcd\x7f abcd" ;
   const uint8_t  * cstr ;
   size_t         i ;

   // TEST skipchar_utf8cstring
   for(i = 0, cstr = utf8cstr; *cstr; ++ i) {
      cstr = skipchar_utf8cstring(cstr) ;
      TEST(0 == strncmp( (const char*)cstr, " abcd", 5)) ;
      cstr += 5 ;
   }
   TEST(4 == i);

   // TEST findwcharnul_utf8cstring (find char)
   cstr = findwcharnul_utf8cstring(utf8cstr, L'\U001fffff') ;
   TEST(cstr == utf8cstr) ;
   cstr = findwcharnul_utf8cstring(utf8cstr, L'\U0000ffff') ;
   TEST(cstr == 9+utf8cstr) ;
   cstr = findwcharnul_utf8cstring(utf8cstr, L'\u07ff') ;
   TEST(cstr == 17+utf8cstr) ;
   cstr = findwcharnul_utf8cstring(utf8cstr, L'\x7f') ;
   TEST(cstr == 24+utf8cstr) ;
   cstr = findwcharnul_utf8cstring(utf8cstr, L'\x7e') ;

   // TEST findwcharnul_utf8cstring (end of string)
   cstr = utf8cstr + strlen((const char*)utf8cstr) ;
   TEST(cstr == findwcharnul_utf8cstring(utf8cstr, L'\U001ffffe')) ;
   TEST(cstr == findwcharnul_utf8cstring(utf8cstr, L'\U0000fffe')) ;
   TEST(cstr == findwcharnul_utf8cstring(utf8cstr, L'\u07fe')) ;
   TEST(cstr == findwcharnul_utf8cstring(utf8cstr, L'\x7e')) ;

   // TEST findwcharnul_utf8cstring (error)
   cstr = utf8cstr + strlen((const char*)utf8cstr) ;
   TEST(cstr == findwcharnul_utf8cstring(utf8cstr, L'\U0fffffff')) ;

   // TEST cmpcaseascii_utf8cstring (equal: all ascii alphabet. chars)
   const char * utf8cstr2 = "\U001fffff ABCD\U0000ffff ABCD\u07ff abcd\x7f ABCD" ;
   TEST(strlen((const char*)utf8cstr) == strlen(utf8cstr2)) ;
   TEST(0 == cmpcaseascii_utf8cstring(utf8cstr, utf8cstr2, strlen(utf8cstr2))) ;
   utf8cstr2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@^¬^°üöä" ;
   TEST(0 == cmpcaseascii_utf8cstring((const uint8_t*)"ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@^¬^°üöä", utf8cstr2, strlen(utf8cstr2))) ;
   TEST(0 == cmpcaseascii_utf8cstring((const uint8_t*)"abcdefghijklmnopqrstuvwxyz1234567890!@^¬^°üöä", utf8cstr2, strlen(utf8cstr2))) ;
   TEST(0 == cmpcaseascii_utf8cstring((const uint8_t*)utf8cstr2, "abcdefghijklmnopqrstuvwxyz1234567890!@^¬^°üöä", strlen(utf8cstr2))) ;

   // TEST cmpcaseascii_utf8cstring (öäü: German Umlaut not equal)
   utf8cstr2 = "Aüöä" ;
   TEST(0 != cmpcaseascii_utf8cstring((const uint8_t*)utf8cstr2, "AÜöä", strlen(utf8cstr2))) ;
   TEST(0 != cmpcaseascii_utf8cstring((const uint8_t*)utf8cstr2, "AüÖä", strlen(utf8cstr2))) ;
   TEST(0 != cmpcaseascii_utf8cstring((const uint8_t*)utf8cstr2, "AüöÄ", strlen(utf8cstr2))) ;

   // TEST cmpcaseascii_utf8cstring (unequal)
   TEST(-1 == cmpcaseascii_utf8cstring((const uint8_t*)"1", "2", 1)) ;
   TEST(+1 == cmpcaseascii_utf8cstring((const uint8_t*)"2", "1", 1)) ;
   TEST(-2 == cmpcaseascii_utf8cstring((const uint8_t*)"1", "3", 1)) ;
   TEST(+2 == cmpcaseascii_utf8cstring((const uint8_t*)"3", "1", 1)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_string_utf8()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_utf8cstring())  goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
