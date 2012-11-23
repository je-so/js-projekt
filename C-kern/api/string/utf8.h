/* title: UTF-8
   8-bit U(niversal Character Set) T(ransformation) F(ormat)

   This encoding of the unicode character set is backward-compatible
   with ASCII and avoids problems with endianess.

   Encoding:
   Unicode character       - UTF-8
   (codepoint)             - (encoding)
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

#include "C-kern/api/string/unicode.h"

// forward
struct conststring_t ;

/* variable: g_utf8_bytesperchar
 * Returns the number of bytes from the first byte of an utf8 char. */
extern uint8_t g_utf8_bytesperchar[256] ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_utf8
 * Test <escape_char>. */
int unittest_string_utf8(void) ;
#endif


// group: utf8

/* function: sizechar_utf8
 * Returns the number of bytes of the next utf8 encoded character.
 * The number of bytes can be calculated from the first byte of the next character.
 * A return value between 1 and 4 is OK. Possible bigger values (5 ... 7) are per definition wrong
 * in such a case (illegal first byte) the value 1 returned so that it can be skipped. */
unsigned sizechar_utf8(const uint8_t firstbyte) ;

/* function: islegal_utf8
 * Returns true if the first byte of a utf8 byte sequence is legal. */
bool islegal_utf8(const uint8_t firstbyte) ;


// section: conststring_t

// group: utf8-stream

/* function: nextcharutf8_conststring
 * Reads next utf-8 encoded character from *str*.
 * The character is returned as unicode character (codepoint)
 * which is the same as wchar_t on Linux or Unix.
 * This function assumes characters are encoded correctly.
 *
 * Returns:
 * 0       - UTF8 character decoded and returned in wchar and memory pointer is moved to next character.
 * ENODATA - String is empty and memory pointer is not changed.
 * EILSEQ  - The next character is encoded in a wrong way or there are not enough bytes left in the string
 *           to decode from. Either way the memory pointer is not changed.
 *           Use <skipcharutf8_conststring> to move the memory pointer to the next byte or character.
 *           If it returns EILSEQ you know also that there are not enough bytes left.
 *           It my be necessary to repeat this step to skip a whole multibyte sequence. */
int nextcharutf8_conststring(struct conststring_t * str, unicode_t * wchar) ;

/* function: skipcharutf8_conststring
 * Moves internal *str* pointer to next character.
 * This function assumes characters are encoded correctly.
 *
 * Returns:
 * 0       - Memory pointer is moved to next character.
 * ENODATA - String is empty and memory pointer is not changed.
 * EILSEQ  - String contains not enough data. The last character
 *           has more encoded bytes than are available in the string.
 *           The memory pointer is not changed. */
int skipcharutf8_conststring(struct conststring_t * str) ;

// group: utf8-query

/* function: findcharutf8_conststring
 * Finds unicode character wchar in utf8 encoded string.
 * The returned value points either the start addr of the multibyte sequence.
 * Or it is null if <conststring_t> *str* does not contain the unicode character
 * or if wchar is bigger than 0x10FFFF and therefore invalid. */
const uint8_t * findcharutf8_conststring(const struct conststring_t * str, unicode_t wchar) ;

/* function: findbyte_conststring
 * Finds unicode character wchar in utf8 encoded string.
 * The returned value points either to the position of the found character.
 * Or it is null if <conststring_t> *str* does not contain the bytecode. */
const uint8_t * findbyte_conststring(const struct conststring_t * str, uint8_t byte) ;

/* function: utf8len_conststring
 * Returns number of UTF-8 characters encoded in string.
 * If an illegal start byte is encountered it is counted as 1 and skipped.
 * Even if one or more bytes of the last multibyte sequence are missing it
 * s also counted as one character. The first byte of a multibyte sequence
 * determines its size. This function does not check that the remaining bytes
 * are encoded correctly. */
size_t utf8len_conststring(const struct conststring_t * str) ;


// section: inline implementation

/* function: islegal_utf8
 * Implements <utf8.islegal_utf8>. */
#define islegal_utf8(firstbyte)           ((firstbyte) < 0x80 || sizechar_utf8(firstbyte) != 1)

/* function: sizechar_utf8
 * Implements <utf8.sizechar_utf8>. */
#define sizechar_utf8(firstbyte)          (g_utf8_bytesperchar[(uint8_t)(firstbyte)])

/* function: findbyte_conststring
 * Implements <conststring_t.findbyte_conststring>. */
#define findbyte_conststring(str, byte)   (memchr((str)->addr, (uint8_t)(byte), (str)->size))

#endif
