/* title: UTF8TextReader

   This reader decodes UTF-8 multibyte text content into a <char32_t> character
   and maintains additional information about the current line number and column.

   TODO: Adapt to instream_t

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
 * Manages position in text buffer.
 * It stores the current read position in memory
 * and the current text position as linnr and colnr.
 * Reading a character advances this positions. */
struct utf8reader_t {
   const uint8_t  * next ;
   const uint8_t  * end ;
   size_t         colnr ;
   size_t         linenr ;
} ;

// group: lifetime

/* define: utf8reader_INIT_FREEABLE
 * Static initializer.  */
#define utf8reader_INIT_FREEABLE       { 0, 0, 0, 0 }

/* function: init_utf8reader
 * Inits *utfread* to point to text at *textaddr* with *length* bytes. */
void init_utf8reader(/*out*/utf8reader_t * utfread, size_t size, const uint8_t textaddr[size]) ;

/* function: init_utf8reader
 * Sets all internal members to 0. There is no need to free any resources. */
void free_utf8reader(utf8reader_t * utfread) ;

// group: query

/* function: isnext_utf8reader
 * Returns true if there is at least one byte to read.
 * It is possible that a character is encoded into several bytes
 * and the string does contain less bytes. In this
 * case <nextchar_utf8reader> or <skipchar_utf8reader> return EILSEQ. */
bool isnext_utf8reader(const utf8reader_t * utfread) ;

/* function: nrcolumn_utf8reader
 * Returns the column nr of the current reading position.
 * At the beginning of each line this value is 0. Reading a
 * character increments this value is by one. Therefore this value
 * represents the column nr of the last read character. */
size_t nrcolumn_utf8reader(const utf8reader_t * utfread) ;

/* function: nrline_utf8reader
 * Returns the line nr of the current readng position.
 * During initialization of <utf8reader_t> this value is set to 1.
 * Eve3ry time a new line character is read this value is incremented
 * by one and the column nr is set to 0 (see <nrcolumn_utf8reader>). */
size_t nrline_utf8reader(const utf8reader_t * utfread) ;

/* function: unread_utf8reader
 * Returns a pointer to the buffer beginning with the next unread character.
 * The size of the character buffer can be determined with the return value of
 * <unreadsize_utf8reader>. */
const uint8_t * unread_utf8reader(const utf8reader_t * utfread) ;

/* function: unreadsize_utf8reader
 * Returns the size of the buffer containing all unread characters.
 * The first character in this buffer can be decoded by a call to <nextchar_utf8reader>.
 * Using this function and <unread_utf8reader> you can peek into the buffer to compare
 * it to an UTF-8/ascii string without decoding it. With <skipNbytes_utf8reader> it is possible
 * to skip all bytes of such a comparison. */
size_t unreadsize_utf8reader(utf8reader_t * utfread) ;

// group: memento

/* function: savetextpos_utf8reader
 * Saves the current textposition from *utfread* into *memento*.
 * You can restore it later with a call to <restoretextpos_utf8reader>. */
void savetextpos_utf8reader(const utf8reader_t * utfread, /*out*/utf8reader_t * memento) ;

/* function: restoretextpos_utf8reader
 * Restores the current textposition from *memento* into *utfread*.
 * If memento was not initialized by a previous call to <savetextpos_utf8reader>
 * the behaviour is undefined. */
void restoretextpos_utf8reader(utf8reader_t * utfread, const utf8reader_t * memento) ;

// group: read

/* function: nextchar_utf8reader
 * Decodes next unicode character as from input (UTF-32 encoding).
 * The character is returned as unicode character (codepoint).
 * This function assumes characters are encoded correctly.
 *
 * Returns:
 * 0       - Next UTF-8 encoded character decoded and returned in nxtchar and reading position is moved to next character.
 * ENODATA - No more bytes to read from. Reading position is not changed.
 * EILSEQ  - The next character is encoded in a wrong way or there are not enough bytes left in the input.
 *           Either way the reading position is not changed.
 *           Use <skipchar_utf8reader> or <skipNbytes_utf8reader> to move the reading position to the next byte or character.
 *           If you called <skipchar_utf8reader> and it returned EILSEQ you know also that there are not enough bytes left.
 *           It may be necessary to repeat this step to skip a whole multibyte sequence. */
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

/* function: skipNbytes_utf8reader
 * Skips next *nrbytes* bytes.
 * The third parameter gives the number of skipped characters to adapt the current column accordingly.
 * Make sure that there is no newline character in the input cause the current line number is not changed.
 * If there are no more *nrbytes* bytes nothing is done. Make sure that there are enough bytes in the
 * input stream and that you also know the number of characters before calling this function.
 *
 * Unsafe Function:
 * This function assumes that the next *nrbytes* bytes contains *nrchars* characters and *NO* newline character. */
void skipNbytes_utf8reader(utf8reader_t * utfread, size_t nrbytes, size_t nrchars) ;

/* function: skipline_utf8reader
 * Skips characters until beginning of next line.
 * If there is no next line character found nothing is changed.
 *
 * Returns:
 * 0       - All characters are skipped until beginning of next line.
 * ENODATA - Reading position is not changed cause text contains no new line character. */
int skipline_utf8reader(utf8reader_t * utfread) ;

/* function: skipall_utf8reader
 * Skips all characters until end of input.
 * Use this function in conjunction with <skipline_utf8reader>.
 * If <skipline_utf8reader> returns ENODATA call <skipall_utf8reader> if
 * you want to consume all characters and compute the column number correctly.
 * Use <skipNbytes_utf8reader> if you do not want to compute the column number
 * correctly but only want to signal the fact that there are no more bytes to read.
 * If the function returns EILSEQ see <skipchar_utf8reader> for the meaning of it. */
int skipall_utf8reader(utf8reader_t * utfread) ;


// section: inline implementation

/* define: free_utf8reader
 * Implements <utf8reader_t.free_utf8reader>. */
#define free_utf8reader(utfread)       \
   do {     (utfread)->next   = 0 ;    \
            (utfread)->end    = 0 ;    \
            (utfread)->colnr  = 0 ;    \
            (utfread)->linenr = 0 ;    \
   } while(0)

/* define: init_utf8reader
 * Implements <utf8reader_t.init_utf8reader>. */
#define init_utf8reader(utfread, _size, textaddr)  \
   do {     (utfread)->next   = (textaddr) ;       \
            (utfread)->end    = (utfread)->next    \
                                + (_size) ;        \
            (utfread)->colnr  = 0 ;                \
            (utfread)->linenr = 1 ;                \
   } while(0)

/* define: isnext_utf8reader
 * Implements <utf8reader_t.isnext_utf8reader>. */
#define isnext_utf8reader(utfread)                       \
            ((utfread)->next < (utfread)->end)

/* define: nextchar_utf8reader
 * Implements <utf8reader_t.nextchar_utf8reader>. */
#define nextchar_utf8reader(utfread, nxtchar)            \
   ( __extension__ ({                                    \
            typeof(utfread) _rd1 = (utfread) ;           \
            int _err = nextutf8_stringstream(            \
                  genericcast_stringstream(_rd1),        \
                  (nxtchar)) ;                           \
            if (0 == _err) {                             \
               ++ _rd1->colnr ;                          \
               if ('\n' == *(nxtchar)) {                 \
                  ++ _rd1->linenr ;                      \
                  _rd1->colnr = 0 ;                      \
               }                                         \
            }                                            \
            _err ;                                       \
   }))

/* define: nrcolumn_utf8reader
 * Implements <utf8reader_t.nrcolumn_utf8reader>. */
#define nrcolumn_utf8reader(utfread)                     \
            ((utfread)->colnr)

/* define: nrline_utf8reader
 * Implements <utf8reader_t.nrline_utf8reader>. */
#define nrline_utf8reader(utfread)                       \
            ((utfread)->linenr)

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


/* define: restoretextpos_utf8reader
 * Implements <utf8reader_t.restoretextpos_utf8reader>. */
#define restoretextpos_utf8reader(utfread, memento)      \
   ((void) (*(utfread) = *(memento)))

/* define: savetextpos_utf8reader
 * Implements <utf8reader_t.savetextpos_utf8reader>. */
#define savetextpos_utf8reader(utfread, memento)         \
   ((void) (*(memento) = *(utfread)))

/* define: skipascii_utf8reader
 * Implements <utf8reader_t.skipascii_utf8reader>. */
#define skipascii_utf8reader(utfread)                    \
   do {                                                  \
            typeof(utfread) _urd1 = (utfread) ;          \
            bool _isnext = isnext_utf8reader(_urd1) ;    \
            if (_isnext) {                               \
               ++ _urd1->colnr ;                         \
               if ('\n' == *(_urd1->next ++)) {          \
                  ++ _urd1->linenr ;                     \
                  _urd1->colnr = 0 ;                     \
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
                  ++ _rd1->linenr ;                      \
                  _rd1->colnr = 0 ;                      \
                  ++ _rd1->next ;                        \
               } else {                                  \
                  uint8_t _sz ;                          \
                  _sz = sizechar_utf8(firstbyte) ;       \
                  if (_sz > (_rd1->end - _rd1->next)) {  \
                     _err = EILSEQ ;                     \
                  } else {                               \
                     _rd1->next += _sz + (_sz==0) ;      \
                     ++ _rd1->colnr ;                    \
                  }                                      \
               }                                         \
            } else {                                     \
               _err = ENODATA ;                          \
            }                                            \
            _err ;                                       \
   }))

/* define: skipNbytes_utf8reader
 * Implements <utf8reader_t.skipNbytes_utf8reader>. */
#define skipNbytes_utf8reader(utfread, nrbytes, nrchars) \
   do {                                                  \
            typeof(utfread) _urd1 = (utfread) ;          \
            size_t _nrb1 = (nrbytes) ;                   \
            size_t _size = unreadsize_utf8reader(_urd1); \
            if (_size > _nrb1)  {                        \
               _urd1->next  += _nrb1 ;                   \
               _urd1->colnr += (nrchars) ;               \
            }                                            \
   } while(0)


/* define: unread_utf8reader
 * Implements <utf8reader_t.unread_utf8reader>. */
#define unread_utf8reader(utfread)                       \
            ((utfread)->next)

/* define: unreadsize_utf8reader
 * Implements <utf8reader_t.unreadsize_utf8reader>. */
#define unreadsize_utf8reader(utfread)                   \
            ((size_t)((utfread)->end - (utfread)->next))


#endif
