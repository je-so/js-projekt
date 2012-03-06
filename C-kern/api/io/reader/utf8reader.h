/* title: UTF8TextReader

   This reader decodes ut8 text content into one char and administrates additional information
   as there are line and column numbers.

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
extern int unittest_io_reader_utf8reader(void) ;
#endif


/* struct: utf8reader_t
 * Manages position in text buffer.
 * It stores the current read position in memory
 * and the current text position as linnr and colnr.
 * Reading a character advances this positions. */
struct utf8reader_t {
   const uint8_t  * next ;
   size_t         size ;
   size_t         linenr ;
   size_t         colnr ;
} ;

// group: lifetime

/* define: utf8reader_INIT_FREEABLE
 * Static initializer.  */
#define utf8reader_INIT_FREEABLE   { 0, 0, 0, 0}

/* function: init_utf8reader
 * Inits *utfread* to point to text at *textaddr* with *length* bytes. */
void init_utf8reader(/*out*/utf8reader_t * utfread, size_t size, const uint8_t textaddr[size]) ;

/* function: init_utf8reader
 * Sets all internal members to 0. No resources are freed. */
void free_utf8reader(utf8reader_t * utfread) ;

// group: query

/* function: isnext_utf8reader
 * TODO */
bool isnext_utf8reader(const utf8reader_t * utfread) ;

/* function: nrcolumn_utf8reader
 * TODO */
size_t nrcolumn_utf8reader(const utf8reader_t * utfread) ;

/* function: nrline_utf8reader
 * TODO */
size_t nrline_utf8reader(const utf8reader_t * utfread) ;

/* function: unread_utf8reader
 * TODO */
const uint8_t * unread_utf8reader(const utf8reader_t * utfread) ;

/* function: unreadsize_utf8reader
 * TODO */
size_t unreadsize_utf8reader(utf8reader_t * utfread) ;

// group: memento

/* function: savetextpos_utf8reader
 * Saves the current textposition from *utfread* into *memento*.
 * You can restore it later with a call to <restoretextpos_utf8reader>. */
void savetextpos_utf8reader(const utf8reader_t * utfread, utf8reader_t * memento) ;

/* function: restoretextpos_utf8reader
 * Restores the current textposition from *memento* into *utfread*.
 * If memento was not initialized by a previous call to <savetextpos_utf8reader>
 * the behaviour is undefined. */
void restoretextpos_utf8reader(utf8reader_t * utfread, const utf8reader_t * memento) ;

// group: read

/* function: nextchar_utf8reader
 * Decodes next unicode character from input.
 *
 * Returns:
 * true  - There was another uchar read. *nxtchar* contains valid value.
 * false - No more bytes could be read. *nxtchar* is not changed. */
bool nextchar_utf8reader(utf8reader_t * utfread, uint32_t * nxtchar) ;

/* function: skipchar_utf8reader
 * Skips next character.
 *
 * Returns:
 * true  - There was another character skipped.
 * false - No more more bytes could be read. */
bool skipchar_utf8reader(utf8reader_t * utfread) ;

/* function: peekascii_utf8reader
 * Returns next ascii character.
 * The returned character is valid ascii if it is in the range [0 .. 127].
 * If it is not in this range the next character is not an ascii.
 * In this case use <nextchar_utf8reader> to read the encoded multibyte sequence.
 *
 * Returns:
 * true  - There was another byte read. *nextascii* contains byte value.
 * false - No more bytes could be read. *nextascii* is not changed. */
bool peekascii_utf8reader(const utf8reader_t * utfread, uint8_t * nextascii) ;

/* function: peekasciiatoffset_utf8reader
 * Returns ascii character at *offset* bytes from addr <unread_utf8reader>.
 * The returned character is valid ascii if it is in the range [0 .. 127].
 * If it is not in this range the character at *offset* is not an ascii.
 *
 * Returns:
 * true  - There was another byte at *offset* read. *nextascii* contains byte value.
 * false - No more bytes could be read at *offset*. *nextascii* is not changed. */
bool peekasciiatoffset_utf8reader(const utf8reader_t * utfread, size_t offset, uint8_t * nextascii) ;

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
 * The 3d parameter gives the number of contained characters to adapt the current column is accordingly.
 * Make sure that there is no newline character in the input cause the current line number is not changed.
 *
 * Unsafe Function:
 * This function assumes that the next *nrbytes* bytes contains *nrchars* characters and *NO* newline character. */
void skipNbytes_utf8reader(utf8reader_t * utfread, size_t nrbytes, size_t nrchars) ;

/* function: skipchar_utf8reader
 * Skips characters until beginning of next line or end of input.
 *
 * Returns:
 * true  - There was at least one character skipped.
 * false - Already reached »end of input«, no characters were skipped. */
bool skipline_utf8reader(utf8reader_t * utfread) ;


// section: inline implementation

/* define: free_utf8reader
 * Implements <utf8reader_t.free_utf8reader>. */
#define free_utf8reader(utfread)       \
   do {     (utfread)->next    = 0 ;   \
            (utfread)->size    = 0 ;   \
            (utfread)->linenr  = 0 ;   \
            (utfread)->colnr   = 0 ;   \
   } while(0)

/* define: init_utf8reader
 * Implements <utf8reader_t.init_utf8reader>. */
#define init_utf8reader(utfread, _size, textaddr)  \
   do {     (utfread)->next    = (textaddr) ;      \
            (utfread)->size    = (_size) ;         \
            (utfread)->linenr  = 1 ;               \
            (utfread)->colnr   = 0 ;               \
   } while(0)

/* define: isnext_utf8reader
 * Implements <utf8reader_t.isnext_utf8reader>. */
#define isnext_utf8reader(utfread)                       \
            (0 != (utfread)->size)

/* define: nextbyte_utf8reader
 * Implements <utf8reader_t.nextbyte_utf8reader>. */
#define nextbyte_utf8reader(utfread, nextbyte)           \
   ( __extension__ ({                                    \
            typeof(utfread) _urd1 = (utfread) ;          \
            bool _isnext = isnext_utf8reader(_urd1) ;    \
            if (_isnext) {                               \
               -- _urd1->size ;                          \
               ++ _urd1->colnr ;                         \
               uint8_t _chr = *(_urd1->next ++) ;        \
               *(nextbyte)  = _chr ;                     \
               if ('\n' == _chr) {                       \
                  ++ _urd1->linenr ;                     \
                  _urd1->colnr = 0 ;                     \
               }                                         \
            }                                            \
            _isnext ;                                    \
   }))

/* define: nextchar_utf8reader
 * Implements <utf8reader_t.nextchar_utf8reader>. */
#define nextchar_utf8reader(utfread, nxtchar)            \
   ( __extension__ ({                                    \
            typeof(utfread) _urd1 = (utfread) ;          \
            bool _isnext = nextcharutf8_conststring(     \
                     (conststring_t*)_urd1, (nxtchar)) ; \
            if (_isnext) {                               \
               ++ _urd1->colnr ;                         \
               if ('\n' == *(nxtchar)) {                 \
                  ++ _urd1->linenr ;                     \
                  _urd1->colnr = 0 ;                     \
               }                                         \
            }                                            \
            _isnext ;                                    \
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
            typeof(utfread) _urd1 = (utfread) ;          \
            bool _isnext = isnext_utf8reader(_urd1) ;    \
            if (_isnext) {                               \
               *(nextascii) = *_urd1->next ;             \
            }                                            \
            _isnext ;                                    \
   }))

/* define: peekasciiatoffset_utf8reader
 * Implements <utf8reader_t.peekasciiatoffset_utf8reader>. */
#define peekasciiatoffset_utf8reader(utfread, offset, nextascii) \
   ( __extension__ ({                                    \
            typeof(utfread) _urd1 = (utfread) ;          \
            size_t          _off1 = (offset) ;           \
            bool _isnext = (_urd1->size > _off1) ;       \
            if (_isnext) {                               \
               *(nextascii) = _urd1->next[_off1] ;       \
            }                                            \
            _isnext ;                                    \
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
               -- _urd1->size ;                          \
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
            uint32_t _chr2 ;                             \
            bool _isnext2 =                              \
               nextchar_utf8reader(utfread, &_chr2) ;    \
            _isnext2 ;                                   \
   }))

/* define: skipNbytes_utf8reader
 * Implements <utf8reader_t.skipNbytes_utf8reader>. */
#define skipNbytes_utf8reader(utfread, nrbytes, nrchars) \
   do {                                                  \
            typeof(utfread) _urd1 = (utfread) ;          \
            size_t          _nrb1 = (nrbytes) ;          \
            bool _isnext = (_urd1->size >= _nrb1) ;      \
            if (_isnext) {                               \
               _urd1->next  += _nrb1 ;                   \
               _urd1->size  -= _nrb1 ;                   \
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
            ((utfread)->size)


#endif
