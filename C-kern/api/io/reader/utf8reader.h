/* title: UTF8TextReader

   This reader decodes UTF-8 multibyte text content into a <char32_t> character
   and maintains additional information about the current line number and column.

   This reader is unsafe in that it do not check for incorrect encoding
   in same cases (skip for example).

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
#ifndef CKERN_IO_READER_UTF8READER_HEADER
#define CKERN_IO_READER_UTF8READER_HEADER

#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/string/textpos.h"
#include "C-kern/api/string/utf8.h"

/* typedef: struct utf8reader_t
 * Exports <utf8reader_t>. */
typedef struct utf8reader_t            utf8reader_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_reader_utf8reader
 * Test <utf8reader_t>. */
int unittest_io_reader_utf8reader(void) ;
#endif


/* struct: utf8reader_t
 * Extends <mmfile_t> with text reading capapbilities.
 * Only text files encoded in UTF-8 are supported.
 * The current read position is also handled by this object.
 * Reading a character advances the text position. */
struct utf8reader_t {
   const uint8_t        * next ;
   const uint8_t        * end ;
   textpos_t            pos ;
   mmfile_t             mmfile ;
} ;

// group: lifetime

/* define: utf8reader_INIT_FREEABLE
 * Static initializer.  */
#define utf8reader_INIT_FREEABLE       { 0, 0, textpos_INIT_FREEABLE, mmfile_INIT_FREEABLE }

/* function: init_utf8reader
 * Opens file at *filepath* for reading as UTF-8 encoded text.
 * The whole file is read into a single buffer. Therefore the parser must never
 * make a copy of any parsed identifier instead it can store references to the buffer. */
int init_utf8reader(/*out*/utf8reader_t * utfread, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: free_utf8reader
 * Sets all internal members to 0. There is no need to free any resources. */
int free_utf8reader(utf8reader_t * utfread) ;

// group: query

/* function: column_utf8reader
 * Returns the column nr of the current reading position.
 * At the beginning of each line this value is 0. Reading a
 * character increments this value is by one. Therefore this value
 * represents the column nr of the last read character. */
size_t column_utf8reader(const utf8reader_t * utfread) ;

/* function: line_utf8reader
 * Returns the line nr of the current readng position.
 * During initialization of <utf8reader_t> this value is set to 1.
 * Eve3ry time a new line character is read this value is incremented
 * by one and the column nr is set to 0 (see <column_utf8reader>). */
size_t line_utf8reader(const utf8reader_t * utfread) ;

/* function: textpos_utf8reader
 * Returns the current textposition from *utfread*.
 * You can restore it later with a call to <restoretextpos_utf8reader>. */
const textpos_t * textpos_utf8reader(const utf8reader_t * utfread) ;

/* function: isnext_utf8reader
 * Returns true if there is at least one byte to read.
 * It is possible that a character is encoded into several bytes
 * and the string does contain less bytes. In this
 * case <nextchar_utf8reader> or <skipchar_utf8reader> return EILSEQ. */
bool isnext_utf8reader(const utf8reader_t * utfread) ;

/* function: unread_utf8reader
 * Returns a pointer to the buffer beginning with the next unread character.
 * The size of the character buffer can be determined with the return value of
 * <unreadsize_utf8reader>. */
const uint8_t * unread_utf8reader(const utf8reader_t * utfread) ;

/* function: unreadsize_utf8reader
 * Returns the size of the buffer containing all unread characters.
 * The first character in this buffer can be decoded by a call to <nextchar_utf8reader>.
 * Using this function and <unread_utf8reader> you can peek into the buffer to compare
 * it to an UTF-8/ascii string without decoding it. With <skipbytes_utf8reader> it is possible
 * to skip all bytes of such a comparison. */
size_t unreadsize_utf8reader(utf8reader_t * utfread) ;

// group: read

/* function: nextbyte_utf8reader
 * Reads next byte and increments column by one.
 * The value is returned in nextbyte.
 *
 * See <nextutf8_stringstream> for a list of error codes.
 * Use <skipchar_utf8reader> or <skipbytes_utf8reader> to move the
 * reading position in case of returned error EILSEQ. */
int nextbyte_utf8reader(utf8reader_t * utfread, uint8_t * nextbyte) ;

/* function: nextchar_utf8reader
 * Decodes next unicode character from UTF-8 into UTF-32 encoding.
 * The returned value in nxtchar corresponds to a unicode codepoint.
 *
 * See <nextutf8_stringstream> for a list of error codes.
 * Use <skipchar_utf8reader> or <skipbytes_utf8reader> to move the
 * reading position in case of returned error EILSEQ. */
int nextchar_utf8reader(utf8reader_t * utfread, char32_t * nxtchar) ;

/* function: skipchar_utf8reader
 * Skips next character.
 * This function assumes characters are encoded correctly.
 *
 * Returns:
 * 0       - Reading position is moved to next character.
 * ENODATA - String is empty and reading position is not changed.
 * EILSEQ  - String contains not enough data. The last character
 *           has more encoded bytes than are available in the string.
 *           The reading position is not changed. */
int skipchar_utf8reader(utf8reader_t * utfread) ;

/* function: peekascii_utf8reader
 * Returns next ascii character.
 * The returned character is valid ascii if it is in the range [0 .. 127].
 * If it is not in this range the next character is not an ascii.
 * In this case use <nextchar_utf8reader> to read the encoded multibyte sequence.
 *
 * Returns:
 * 0       - There was another byte read and *nextascii* contains its value. Reading position is not moved.
 * ENODATA - No more byte could be read. *nextascii* is not changed. */
int peekascii_utf8reader(const utf8reader_t * utfread, uint8_t * nextascii) ;

/* function: peekasciiatoffset_utf8reader
 * Returns next ascii character with an offset if *offset* bytes.
 * Calling this function with an offset parameter set to 0 is the same as calling <peekascii_utf8reader>.
 * The returned character is valid ascii if it is in the range [0 .. 127].
 * If it is not in this range the next character at *offset* is not an encoded ascii value.
 *
 * Returns:
 * 0       - There was another byte read and *nextascii* contains its value. Reading position is not moved.
 * ENODATA - No more byte could be read at *offset*. *nextascii* is not changed. */
int peekasciiatoffset_utf8reader(const utf8reader_t * utfread, size_t offset, uint8_t * nextascii) ;

/* function: skipascii_utf8reader
 * Skips next ascii character.
 *
 * Unsafe Function:
 * This function assumes that the next character is of ascii encoding (range 0..127 of utf-8).
 * Therefore call this function only if you know that <peekascii_utf8reader> returned a valid
 * ascii character ! */
void skipascii_utf8reader(utf8reader_t * utfread) ;

/* function: skipbytes_utf8reader
 * Skips next *nrbytes* bytes.
 * The third parameter gives the number of skipped characters to adapt the current column accordingly.
 * Make sure that there is no newline character in the input cause the current line number is not changed.
 * If there are no more *nrbytes* bytes nothing is done. Make sure that there are enough bytes in the
 * input stream and that you also know the number of characters before calling this function.
 *
 * Unsafe Function:
 * This function assumes that the next *nrbytes* bytes contains *nrchars* characters and *NO* newline character. */
void skipbytes_utf8reader(utf8reader_t * utfread, size_t nrbytes, size_t nrchars) ;

/* function: skipline_utf8reader
 * Skips characters until beginning of next line.
 * If there is no next line character found nothing is changed.
 *
 * Returns:
 * 0       - All characters are skipped until beginning of next line.
 * ENODATA - All characters are skipped until end of input but stream contained no new line character. */
int skipline_utf8reader(utf8reader_t * utfread) ;

// group: read+match

/* function: matchbytes_utf8reader
 * Matches a string of bytes. In case of success the column number is incremented by colnr
 * and rhe reading position is advanced by nrbytes.
 *
 * In case of an error *matchedsize contains the number of matched bytes before the error
 * and the reading position is advanced by the same amount. The column number is not changed.
 *
 * Returns:
 * 0        - All nrbytes have been matched.
 * EMSGSIZE - The next byte after *matchedsize did not match. TODO: replace EMSGSIZE with own error EPARTMATCH
 * ENODATA  - Encountered end of stream during match. */
int matchbytes_utf8reader(utf8reader_t * utfread, size_t colnr, size_t nrbytes, const uint8_t bytes[nrbytes], /*err*/size_t * matchedsize) ;


// section: inline implementation

/* define: isnext_utf8reader
 * Implements <utf8reader_t.isnext_utf8reader>. */
#define isnext_utf8reader(utfread)                       \
            ((utfread)->next < (utfread)->end)

/* define: nextchar_utf8reader
 * Implements <utf8reader_t.nextchar_utf8reader>. */
#define nextchar_utf8reader(utfread, nxtchar)            \
         ( __extension__ ({                              \
            typeof(utfread) _rd1 = (utfread) ;           \
            int _err2 = nextutf8_stringstream(           \
                  genericcast_stringstream(_rd1),        \
                  (nxtchar)) ;                           \
            if (0 == _err2) {                            \
               incrcolumn_textpos(&_rd1->pos) ;          \
               if ('\n' == *(nxtchar)) {                 \
                  incrline_textpos(&_rd1->pos) ;         \
               }                                         \
            }                                            \
            _err2 ;                                      \
         }))

/* define: column_utf8reader
 * Implements <utf8reader_t.column_utf8reader>. */
#define column_utf8reader(utfread)                       \
         (column_textpos(&(utfread)->pos))

/* define: line_utf8reader
 * Implements <utf8reader_t.line_utf8reader>. */
#define line_utf8reader(utfread)                         \
         (line_textpos(&(utfread)->pos))

/* define: nextbyte_utf8reader
 * Implements <utf8reader_t.nextbyte_utf8reader>. */
#define nextbyte_utf8reader(utfread, nextbyte)           \
   ( __extension__ ({                                    \
            int             _err ;                       \
            typeof(utfread) _rd  = (utfread) ;           \
            if (isnext_utf8reader(_rd)) {                \
               incrcolumn_textpos(&_rd->pos) ;           \
               const uint8_t _b = *(_rd->next ++) ;      \
               *(nextbyte) = _b ;                        \
               if ('\n' == _b) {                         \
                  incrline_textpos(&_rd->pos) ;          \
               }                                         \
               _err = 0 ;                                \
            } else {                                     \
               _err = ENODATA ;                          \
            }                                            \
            _err ;                                       \
   }))

/* define: peekascii_utf8reader
 * Implements <utf8reader_t.peekascii_utf8reader>. */
#define peekascii_utf8reader(utfread, nextascii)         \
   ( __extension__ ({                                    \
            int             _err2 ;                      \
            typeof(utfread) _rd2  = (utfread) ;          \
            if (isnext_utf8reader(_rd2)) {               \
               *(nextascii) = *_rd2->next ;              \
               _err2 = 0 ;                               \
            } else {                                     \
               _err2 = ENODATA ;                         \
            }                                            \
            _err2 ;                                      \
   }))

/* define: peekasciiatoffset_utf8reader
 * Implements <utf8reader_t.peekasciiatoffset_utf8reader>. */
#define peekasciiatoffset_utf8reader(utfread, offset, nextascii) \
   ( __extension__ ({                                    \
            typeof(utfread) _rd2  = (utfread) ;          \
            int    _err2 ;                               \
            size_t _off2 = (offset) ;                    \
            size_t _size = unreadsize_utf8reader(_rd2) ; \
            if (_size > _off2) {                         \
               *(nextascii) = _rd2->next[_off2] ;        \
               _err2 = 0 ;                               \
            } else {                                     \
               _err2 = ENODATA ;                         \
            }                                            \
            _err2 ;                                      \
   }))


/* define: skipascii_utf8reader
 * Implements <utf8reader_t.skipascii_utf8reader>. */
#define skipascii_utf8reader(utfread)                    \
   do {                                                  \
            typeof(utfread) _rd1 = (utfread) ;           \
            bool _isnext = isnext_utf8reader(_rd1) ;     \
            if (_isnext) {                               \
               incrcolumn_textpos(&_rd1->pos) ;          \
               if ('\n' == *(_rd1->next ++)) {           \
                  incrline_textpos(&_rd1->pos) ;         \
               }                                         \
            }                                            \
   } while(0)

/* define: skipchar_utf8reader
 * Implements <utf8reader_t.skipchar_utf8reader>. */
#define skipchar_utf8reader(utfread)                     \
   ( __extension__ ({                                    \
            int             _err = 0 ;                   \
            typeof(utfread) _rd1 = (utfread) ;           \
            if (isnext_utf8reader(_rd1)) {               \
               uint8_t firstbyte = *(_rd1->next) ;       \
               if ('\n' == firstbyte) {                  \
                  incrline_textpos(&_rd1->pos) ;         \
                  ++ _rd1->next ;                        \
               } else {                                  \
                  uint8_t _sz ;                          \
                  _sz = sizefromfirstbyte_utf8(          \
                                      firstbyte) ;       \
                  if (_sz > (_rd1->end - _rd1->next)) {  \
                     _err = EILSEQ ;                     \
                  } else {                               \
                     _rd1->next += _sz + (_sz==0) ;      \
                     incrcolumn_textpos(&_rd1->pos) ;    \
                  }                                      \
               }                                         \
            } else {                                     \
               _err = ENODATA ;                          \
            }                                            \
            _err ;                                       \
   }))

/* define: skipbytes_utf8reader
 * Implements <utf8reader_t.skipbytes_utf8reader>. */
#define skipbytes_utf8reader(utfread, nrbytes, nrchars)  \
   do {                                                  \
            typeof(utfread) _rd1 = (utfread) ;           \
            size_t _nrb1 = (nrbytes) ;                   \
            size_t _size = unreadsize_utf8reader(_rd1) ; \
            if (_size >= _nrb1)  {                       \
               _rd1->next += _nrb1 ;                     \
               addcolumn_textpos(&_rd1->pos,(nrchars)) ; \
            }                                            \
   } while(0)


/* define: textpos_utf8reader
 * Implements <utf8reader_t.textpos_utf8reader>. */
#define textpos_utf8reader(utfread)                      \
            (&(utfread)->pos)

/* define: unread_utf8reader
 * Implements <utf8reader_t.unread_utf8reader>. */
#define unread_utf8reader(utfread)                       \
            ((utfread)->next)

/* define: unreadsize_utf8reader
 * Implements <utf8reader_t.unreadsize_utf8reader>. */
#define unreadsize_utf8reader(utfread)                   \
            ((size_t)((utfread)->end - (utfread)->next))


#endif
