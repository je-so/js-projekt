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

   The UTF-8 encoding is restricted to max. 4 bytes per character to be compatible with
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

/* function: maxchar_utf8
 * Returns the maximum code point which can be encoded into utf-8.
 * The minumum unicode code point is 0.
 * The returned value is 0x10FFFF. */
char32_t maxchar_utf8(void) ;

/* function: sizemax_utf8
 * Returns the maximum number of bytes of an utf-8 encoded character can use. */
uint8_t sizemax_utf8(void) ;

/* function: islegal_utf8
 * Returns true if the first byte of an utf8 byte sequence is legal. */
bool islegal_utf8(const uint8_t firstbyte) ;

/* function: isfirstbyte_utf8
 * Returns true if this byte is encoded as first byte (start) of an utf-8 character sequence.
 * This function assumes correct encoding and does not check for legal encodings
 * like <islegal_utf8>. */
bool isfirstbyte_utf8(const uint8_t firstbyte) ;

/* function: sizefromfirstbyte_utf8
 * Returns the number of bytes of an utf8 encoded character.
 * The number of bytes is calculated from firstbyte - the first byte of the encoded character.
 * The returned values are in the range 0..4. A return value between 1 and 4 is OK.
 * A value of 0 indicates that the character's first byte is not encoded correctly. */
uint8_t sizefromfirstbyte_utf8(const uint8_t firstbyte) ;

/* function: sizefromchar_utf8
 * Returns the number of bytes of an utf8 encoded character.
 * The number of bytes is calculated from firstbyte - the first byte of the encoded character.
 * The returned values are in the range 0..4. A return value between 1 and 4 is OK.
 * A value of 0 indicates that an the character's first byte is not encoded correctly. */
uint8_t sizefromchar_utf8(char32_t uchar) ;

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

// group: encode-decode

/* function: decodechar_utf8
 * Decodes utf-8 encoded bytes beginning from strstart and returns character in uchar.
 * strsize describes the size of the buffer in bytes.
 * The number of decoded bytes is returned.
 *
 * Error Indicator:
 * A return value of 0 indicates either an encoding error (EILSEQ)
 * or that the utf-8 enocded character need more bytes than strsize.
 * Nothing is done in both cases.
 *
 * Example:
 *
 * > uint8_t * str    = &strbuffer[0] ;
 * > uint8_t * strend = strbuffer + sizeof(strbuffer) ;
 * > while (str < strend) {
 * >    char32_t uchar ;
 * >    uint8_t  len = decodechar_utf8(strend-str, str, &uchar) ;
 * >    if (len == 0) {
 * >       if (sizefromfirstbyte_utf8(str[0]) > (strend-str)) {
 * >          ...not enough data for last character...
 * >          break ;
 * >       } else {
 * >          TRACECALL_ERRLOG("decodechar_utf8", EILSEQ) ;
 * >          ...move str forward until no decode error then continue...
 * >       }
 * >    }
 * >    ...add uchar to decoded string...
 * >    str += len ;
 * > } */
uint8_t decodechar_utf8(size_t strsize, const uint8_t strstart[strsize], /*out*/char32_t * uchar) ;

/* function: encodechar_utf8
 * Encodes uchar into UTF-8 enocoded string of size strsize starting at strstart.
 * The number of written bytes are returned. The maximum return value is <sizemax_utf8>.
 * A return value of 0 indicates an error. Either uchar is greater then <maxchar_utf8>
 * or strsize is not big enough. */
uint8_t encodechar_utf8(size_t strsize, /*out*/uint8_t strstart[strsize], char32_t uchar) ;

/* function: skipchar_utf8
 * Skips the next utf-8 encoded character.
 * The encoding is checked for correctness.
 * The number of skipped bytes is returned. The maximum return value is <sizemax_utf8>.
 * A return value of 0 indicates an error. If sizefromfirstbyte_utf8(strstart[0]) > strsize then strsize is too small
 * else the next character is not encoded correctly (EILSEQ). */
uint8_t skipchar_utf8(size_t strsize, const uint8_t strstart[strsize]) ;


// struct: stringstream_t
struct stringstream_t ;

// group: read-utf8

/* function: nextutf8_stringstream
 * Reads next utf-8 encoded character from strstream.
 * The character is returned as unicode character (codepoint) in uchar.
 * The next pointer of strstream is incremented with the number of decoded bytes.
 *
 * Returns:
 * 0         - UTF8 character decoded and returned in uchar and memory pointer is moved to next character.
 * ENODATA   - strstream is empty.
 * ENOTEMPTY - The string is not empty but another character could not be decoded cause there
 *             are not enough bytes left in the string.
 * EILSEQ    - The next character is encoded in a wrong way. strstream is not changed.
 *             Use <skipillegalutf8_strstream> to skip all illegal bytes. */
int nextutf8_stringstream(struct stringstream_t * strstream, /*out*/char32_t * uchar) ;

/* function: peekutf8_stringstream
 * Same as <nextutf8_stringstream> except character is not marked as unread.
 * Calling this function more than once returns always the same result. */
int peekutf8_stringstream(const struct stringstream_t * strstream, /*out*/char32_t * uchar) ;

/* function: skiputf8_stringstream
 * Skips next unread utf-8 character from strstream.
 * The next pointer of strstream is incremented with the size of the next character.
 *
 * Returns:
 * 0         - Memory pointer is moved to next character.
 * ENODATA   - strstream is empty.
 * ENOTEMPTY - The string is not empty but another character could not be decoded cause there
 *             are not enough bytes left in the string.
 * EILSEQ    - The next character is encoded in a wrong way. strstream is not changed.
 *             Use <skipillegalutf8_strstream> to skip all illegal bytes. */
int skiputf8_stringstream(struct stringstream_t * strstream) ;

/* function: skipillegalutf8_strstream
 * Skips bytes until begin of the next valid utf-8 encoding or end of stream.
 * If the last utf-8 can not be fully decoded (ENOTEMPTY) this function returns and
 * does not remove it from the buffer. This ensures that character encodings
 * split over two buffers can be handled correctly. */
void skipillegalutf8_strstream(struct stringstream_t * strstream) ;

// group: find-utf8

/* function: findutf8_stringstream
 * Searches for unicode character in utf8 encoded stringstream.
 * The returned value points to the start addr of the multibyte sequence
 * in the unread buffer. A return value of 0 inidcates that *strstream* does not contain the multibyte sequence
 * or that uchar is bigger than <maxchar_utf8> and therefore invalid. */
const uint8_t * findutf8_stringstream(const struct stringstream_t * strstream, char32_t uchar) ;



// section: inline implementation

// group: utf8_t

/* function: maxchar_utf8
 * Implements <utf8.maxchar_utf8>. */
#define maxchar_utf8()                    ((char32_t)0x10ffff)

/* function: isfirstbyte_utf8
 * Implements <utf8.isfirstbyte_utf8>. */
#define isfirstbyte_utf8(firstbyte)       (0x80 != ((firstbyte)&0xc0))

/* function: islegal_utf8
 * Implements <utf8.islegal_utf8>. */
#define islegal_utf8(firstbyte)           (sizefromfirstbyte_utf8(firstbyte) != 0)

/* function: sizemax_utf8
 * Implements <utf8.sizemax_utf8>. */
#define sizemax_utf8()                    ((uint8_t)4)

/* function: sizefromfirstbyte_utf8
 * Implements <utf8.sizefromfirstbyte_utf8>. */
#define sizefromfirstbyte_utf8(firstbyte) (g_utf8_bytesperchar[(uint8_t)(firstbyte)])

/* function: sizefromchar_utf8
 * Implements <utf8.sizefromchar_utf8>. */
#define sizefromchar_utf8(uchar)    \
         ( __extension__ ({         \
            char32_t _ch = (uchar); \
            (1 + (_ch > 0x7f)       \
               + (_ch > 0x7ff)      \
               + (_ch > 0xffff)) ;  \
         }))

// group: stringstream_t

/* function: nextutf8_stringstream
 * Implements <stringstream_t.nextutf8_stringstream>. */
#define nextutf8_stringstream(strstream, uchar)    \
         ( __extension__ ({                        \
            stringstream_t * _str = (strstream) ;  \
            size_t   _strsize = size_stringstream( \
                                       _str) ;     \
            uint8_t  _len     = decodechar_utf8(   \
                                 _strsize,         \
                                 _str->next,       \
                                 (uchar)) ;        \
                                                   \
            _str->next += _len ;                   \
                                                   \
            int _err = 0 ;                         \
            if (!_len) {                           \
               if (!_strsize) {                    \
                  /* all data decoded */           \
                  _err = ENODATA ;                 \
               } else if ( _strsize <              \
                        sizefromfirstbyte_utf8(    \
                                 _str->next[0])) { \
                  /* not enough data */            \
                  _err = ENOTEMPTY ;               \
               } else {                            \
                  /* wrong encoding */             \
                  _err = EILSEQ ;                  \
               }                                   \
            }                                      \
                                                   \
            _err ;                                 \
         }))

/* function: peekutf8_stringstream
 * Implements <stringstream_t.peekutf8_stringstream>. */
#define peekutf8_stringstream(strstream, uchar)    \
         (  __extension__ ({                       \
            stringstream_t * _strstr =             \
                              (strstream) ;        \
            nextutf8_stringstream(                 \
               &(stringstream_t)                   \
                  stringstream_INIT(               \
                     _strstr->next,                \
                     _strstr->end), uchar) ;       \
         }))

/* function: skiputf8_stringstream
 * Implements <stringstream_t.skiputf8_stringstream>. */
#define skiputf8_stringstream(strstream)           \
         ( __extension__ ({                        \
            stringstream_t * _str = (strstream) ;  \
            size_t   _strsize = size_stringstream( \
                                       _str) ;     \
            uint8_t  _len     = skipchar_utf8(     \
                                 _strsize,         \
                                 _str->next) ;     \
                                                   \
            _str->next += _len ;                   \
                                                   \
            int _err = 0 ;                         \
            if (!_len) {                           \
               if (!_strsize) {                    \
                  /* all data decoded */           \
                  _err = ENODATA ;                 \
               } else if ( _strsize <              \
                        sizefromfirstbyte_utf8(    \
                                 _str->next[0])) { \
                  /* not enough data */            \
                  _err = ENOTEMPTY ;               \
               } else {                            \
                  /* wrong encoding */             \
                  _err = EILSEQ ;                  \
               }                                   \
            }                                      \
                                                   \
            _err ;                                 \
         }))


#endif
