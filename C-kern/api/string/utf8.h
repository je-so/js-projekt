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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/string/utf8.h
    Header file <UTF-8>.

   file: C-kern/string/utf8.c
    Implementation file <UTF-8 impl>.
*/
#ifndef CKERN_STRING_UTF8_HEADER
#define CKERN_STRING_UTF8_HEADER

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


// struct: utf8

// group: query

/* function: islegal_utf8
 * Returns true if the first byte of a utf8 byte sequence is legal. */
bool islegal_utf8(const uint8_t firstbyte) ;

/* function: sizecharmax_utf8
 * Returns the maximum number of bytes of an utf-8 encoded character. */
uint8_t sizecharmax_utf8(void) ;

/* function: sizechar_utf8
 * Returns the number of bytes of the next utf8 encoded character.
 * The number of bytes can be calculated from the first byte of the next character.
 * The returned values are in the range 0..4. A return value between 1 and 4 is OK.
 * A value of 0 indicates an encoding error. */
uint8_t sizechar_utf8(const uint8_t firstbyte) ;

/* function: length_utf8
 * Returns number of UTF-8 characters encoded in string buffer.
 * The first byte of a multibyte sequence determines its size.
 * This function assumes that encodings are correct and does not check
 * the encoding of bytes following the first.
 * Illegal encoded start bytes are not counted but skipped.
 * The last multibyte sequence is counted as one character even if
 * one or more bytes are missing.
 *
 * Parameter:
 * strstart - Start of string buffer (lowest address)
 * strend   - Highest memory address of byte after last byte in the string buffer.
 *            If strend <= strstart then the string is considered the empty string.
 *            Set this value to strstart + length_of_string_in_bytes. */
size_t length_utf8(const uint8_t * strstart, const uint8_t * strend) ;

// group: decode

/* function: decode_utf8
 * Decodes utf-8 encoded bytes and returns value as <char32_t>.
 * The pointer strstart is incremented after successful decoding.
 *
 * Attention:
 * This function does not check for buffer overflow (it can not).
 * So call this function only if you know that strstart points to a buffer
 * which contains at least <sizecharmax_utf8> bytes !
 *
 * Error Indicator:
 * A return value of 0 indicates either an error (EILSEQ wrong encoding)
 * or reading of a '\0' byte. In both cases strstart is not changed.
 * If you want '\0' bytes be part of your decoded string then check content of strstart.
 * If strstart points to a 0 byte then its not an error. See example of how to handle '\0' bytes.
 * > uint8_t * str = &strbuffer[0] ;
 * > uint8_t * strend = strbuffer + sizeof(strbuffer) ;
 * > while ((strend - str) >= sizecharmax_utf8()
 * >        || (strend - str) >= sizechar_utf8(*str)) {
 * >    char32_t uchar = decode_utf8(&str) ;
 * >    if (uchar == 0 && *str == 0)
 * >       ++str ;
 * >    else
 * >       { ...decode error... ; break ; }
 * >    { ...add_uchar_to_decoded_string... }
 * > } */
char32_t decode_utf8(const uint8_t ** strstart) ;


// struct: stringstream_t
struct stringstream_t ;

// group: read-utf8

/* function: nextutf8_stringstream
 * Reads next utf-8 encoded character from <stringstream_t>.
 * The character is returned as unicode character (codepoint)
 * which is the same size as wchar_t on Linux/Unix.
 *
 * Returns:
 * 0         - UTF8 character decoded and returned in wchar and memory pointer is moved to next character.
 * ENODATA   - Stingstream is empty.
 * ENOTEMPTY - The string is not empty but another character could not be decoded cause there
 *             are not enough bytes left in the string.
 * EILSEQ    - The next character is encoded in a wrong way. The stringstream is not changed.
 *             Use <skipillegalutf8_strstream> to skip all illegal bytes. */
int nextutf8_stringstream(struct stringstream_t * strstream, /*out*/char32_t * wchar) ;

/* function: peekutf8_stringstream
 * Same as <nextutf8_stringstream> except character is not marked as unread.
 * Calling this function more than once returns always the same result. */
int peekutf8_stringstream(const struct stringstream_t * strstream, /*out*/char32_t * wchar) ;

/* function: skiputf8_stringstream
 * Skips next unread utf-8 character.
 * X bytes are skipped where x is the size of the next utf-8 character.
 * This function assumes characters are encoded correctly.
 *
 * Returns:
 * 0         - Memory pointer is moved to next character.
 * ENODATA   - Stingstream is empty.
 * ENOTEMPTY - The string is not empty but another character could not be decoded cause there
 *             are not enough bytes left in the string.
 * EILSEQ    - The next character is encoded in a wrong way. The stringstream is not changed.
 *             Use <skipillegalutf8_strstream> to skip all illegal bytes. */
int skiputf8_stringstream(struct stringstream_t * strstream) ;

/* function: skipillegalutf8_strstream
 * Skips bytes until begin of the next valid utf-8 encoding or end of stream.
 * If the last utf-8 can not be fully decoded (ENOTEMPTY) this function returns and
 * does not remove it from the buffer. This ensures that character encodings
 * split over two buffers are handled correctly. */
void skipillegalutf8_strstream(struct stringstream_t * strstream) ;

// group: find-utf8

/* function: findutf8_stringstream
 * Searches for unicode character in utf8 encoded stringstream.
 * The returned value points either the start addr of the multibyte sequence
 * in the unread buffer. THe returned value is 0 if *strstream* does not contain the multibyte sequence
 * or if wchar is bigger than 0x10FFFF and therefore invalid. */
const uint8_t * findutf8_stringstream(const struct stringstream_t * strstream, char32_t wchar) ;


// section: inline implementation

// group: utf8_t

/* function: islegal_utf8
 * Implements <utf8.islegal_utf8>. */
#define islegal_utf8(firstbyte)                    (sizechar_utf8(firstbyte) != 0)

/* function: sizecharmax_utf8
 * Implements <utf8.sizecharmax_utf8>. */
#define sizecharmax_utf8()                         ((uint8_t)4)

/* function: sizechar_utf8
 * Implements <utf8.sizechar_utf8>. */
#define sizechar_utf8(firstbyte)                   (g_utf8_bytesperchar[(uint8_t)(firstbyte)])

// group: stringstream_t

/* function: nextutf8_stringstream
 * Implements <stringstream_t.peekutf8_stringstream>. */
#define peekutf8_stringstream(strstream, wchar)    \
   (  __extension__ ({                             \
      stringstream_t _strstr2 =                    \
         stringstream_INIT(                        \
            (strstream)->next,                     \
            (strstream)->end) ;                    \
      nextutf8_stringstream(&_strstr2, wchar) ;    \
   }))

#endif
