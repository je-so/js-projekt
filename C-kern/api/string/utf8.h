/* title: UTF-8
   8-bit U(niversal Character Set) T(ransformation) F(ormat)

   This encoding of the unicode character set is backward-compatible
   with ASCII and avoids problems with endianess.

   Encoding:
   Unicode character       - UTF-8
   (codepoint)             - encoding
   0x00 .. 0x7F            - 0b0xxxxxxx
   0x80 .. 0x7FF           - 0b110xxxxx 0b10xxxxxx
   0x800 .. 0xFFFF         - 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
   0x10000 .. 0x1FFFFF     - 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   0x200000 .. 0x3FFFFFF   - 0b111110xx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   0x4000000 .. 0x7FFFFFFF - 0b1111110x 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx

   The UTF-8 encoding is constricted to max. 4 bytes per character to be compatible with
   UTF-16 (0 .. 0x10FFFF).

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
#ifndef CKERN_STRING_UTF8_HEADER
#define CKERN_STRING_UTF8_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_utf8
 * Test <escape_char>. */
int unittest_string_utf8(void) ;
#endif


// section: utf8cstring

/* function: cmpcaseascii_utf8cstring
 * Returns result of case insensitive string comparison.
 * This functions assumes only 'A'-'Z' and 'a'-'z' as case sensitive characters. */
int cmpcaseascii_utf8cstring(const uint8_t * utf8cstr, const char * cstr, size_t size) ;

/* function: skipchar_utf8cstring
 * Returns pointer to next character after utf8cstr.
 * This function assumes characters are encoded correctly.
 * If utf8cstr points to the end of string utf8cstr is returned. */
const uint8_t * skipchar_utf8cstring(const uint8_t * utf8cstr) ;

/* function: findwcharnul_utf8cstring
 * Finds character and returns position or end of string.
 * The returned value is position of the found character.
 * If the string does not contain character c the position of
 * the nul byte at the end of utf8cstr is returned. */
const uint8_t * findwcharnul_utf8cstring(const uint8_t * utf8cstr, wchar_t wchar) ;


#endif

