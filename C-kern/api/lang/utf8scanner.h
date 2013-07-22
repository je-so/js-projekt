/* title: UTF8-Scanner

   Exports <utf8scanner_t> which supports to break a text file
   into separate strings. The file is read with help of <filereader_t>.
   The common parts of every text scanner is implemented in this type.

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

   file: C-kern/api/lang/utf8scanner.h
    Header file <UTF8-Scanner>.

   file: C-kern/lang/utf8scanner.c
    Implementation file <UTF8-Scanner impl>.
*/
#ifndef CKERN_LANG_UTF8SCANNER_HEADER
#define CKERN_LANG_UTF8SCANNER_HEADER

#include "C-kern/api/string/splitstring.h"

// forward
struct filereader_t ;

/* typedef: struct utf8scanner_t
 * Export <utf8scanner_t> into global namespace. */
typedef struct utf8scanner_t              utf8scanner_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_lang_utf8scanner
 * Test <utf8scanner_t> functionality. */
int unittest_lang_utf8scanner(void) ;
#endif


/* struct: utf8scanner_t
 * Handles the data buffers returned from <filereader_t> and initializes a token of type <splitstring_t>.
 *
 * Protocol:
 * The token begins with the first read byte or character and can span two buffers.
 * Call <nextbyte_utf8scanner> and <nextchar_utf8scanner> to read the buffer content
 * until you have found a valid token. Call <unread_utf8scanner> if you want to remove one or
 * more of the last characters added to the token. A call to <scannedtoken_utf8scanner> returns
 * the scanned token. If the token is processed call <cleartoken_utf8scanner> to clear the token
 * and free any unsed buffers. Clearing a token sets the starting point of the new token.
 *
 * Use <filereader_t> which is given as parameter to determine if a read error has occurred.
 *
 * If the buffer is empty use <readbuffer_utf8scanner> to read the next buffer of the input data.
 * The function <nextchar_utf8scanner> calls <readbuffer_utf8scanner> automatically if the buffer is empty.
 *
 * */
struct utf8scanner_t {
   /* variable: next
    * Points to the next byte returned from <nextbyte_utf8scanner>. */
   const uint8_t *   next ;
   /* variable: end
    * As long as <next> is lower than <end> there are more bytes to read. */
   const uint8_t *   end ;
   /* variable: scanned_token
    * Stores the begin and length of a string of a recognized token.
    * The token string can be scattered across two buffers. */
   splitstring_t     scanned_token ;
} ;

// group: lifetime

/* define: utf8scanner_INIT_FREEABLE
 * Static initializer. */
#define utf8scanner_INIT_FREEABLE         { 0, 0, splitstring_INIT_FREEABLE }

/* function: init_utf8scanner
 * Sets all data members to 0. No data is read. */
int init_utf8scanner(/*out*/utf8scanner_t * scan) ;

/* function: free_utf8scanner
 * Sets all data members to 0 and releases any acquired buffers from frd. */
int free_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

// group: query

/* function: isfree_utf8scanner
 * Returns true if scan is initialized with <utf8scanner_INIT_FREEABLE>. */
bool isfree_utf8scanner(const utf8scanner_t * scan) ;

/* function: isnext_utf8scanner
 * Returns *true* if the buffer contains at least one more byte.
 * In case false is returned do not call <nextbyte_utf8scanner> or any other function
 * which accesses the buffer. Instead call <readbuffer_utf8scanner> which acquires the next
 * buffer from <filereader_t>. */
bool isnext_utf8scanner(const utf8scanner_t * scan) ;

/* function: sizeunread_utf8scanner
 * The number of bytes which are not read from the current buffer.
 * If this function returns 0 then <isnext_utf8scanner> returns false.
 * Call <readbuffer_utf8scanner> in this case. */
size_t sizeunread_utf8scanner(const utf8scanner_t * scan) ;

/* function: scannedtoken_utf8scanner
 * Returns the address to an internally stored <splitstring_t>.
 * Before the token string is returned the current reading position in the stream
 * is used to calculate the length of the token.
 * The returned string is valid as long as no other function is called except query functions.
 * If you call reading functions you need to call <scannedtoken_utf8scanner> again to adapt
 * the token string to the new length. To clear the token string call <cleartoken_utf8scanner>. */
const splitstring_t * scannedtoken_utf8scanner(utf8scanner_t * scan) ;

// group: read

/* function: nextbyte_utf8scanner
 * Reads the next byte from the buffer and increments the reading position.
 * Call this function only if <isnext_utf8scanner> returned true else the behaviour is undefined. */
uint8_t nextbyte_utf8scanner(utf8scanner_t * scan) ;

/* function: peekbyte_utf8scanner
 * Returns any byte from the buffer without changing the read pointer.
 * The parameter offset must be smaller than <sizeunread_utf8scanner> else the behaviour is undefined. */
uint8_t peekbyte_utf8scanner(utf8scanner_t * scan, size_t offset) ;

/* function: skipbytes_utf8scanner
 * Increments the read pointer by nrbytes without reading the bytes.
 *
 * (Unchecked) Preconditions:
 * o Make sure nrbytes <= <sizeunread_utf8scanner> else the behaviour is undefined.
 * o Skip only whole characters (if an utf-8 character is encoded in 4 bytes
 *   then skip the whole 4 bytes).
 * o Always check that all skipped characters are encoded correctly.
 *   Else later functions could  generate undefined behaviour (security breaks in case of characters encoded not correctly and being
 *   therefore not filtered). */
void skipbytes_utf8scanner(utf8scanner_t * scan, size_t nrbytes) ;

/* function: nextchar_utf8scanner
 * Decodes the next utf8 character and increments the reading position.
 * This function differs from other reading function in that it calls <readbuffer_utf8scanner>
 * if the buffer is empty. It also handles the case where a multibyte character sequence is split
 * across two buffers.
 * Returns error EILSEQ in case of an illegal characater sequence. The sequence is skipped.
 * For any other returned error values see <readbuffer_utf8scanner>. */
int nextchar_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd, /*out*/char32_t * uchar) ;

// group: buffer I/O

/* function: cleartoken_utf8scanner
 * Clears the current token string.
 * All buffers are released which are no longer referenced by the cleared token. */
int cleartoken_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

/* function: readbuffer_utf8scanner
 * Acquires the next buffer from <filereader_t> if <isnext_utf8scanner> returns false.
 *
 * Returns:
 * 0        - If <isnext_utf8scanner> returned false before this call then <readnext_filereader> was called
 *            and the read pointer points to the new buffer content. If <isnext_utf8scanner> returned true
 *            before this call nothing was done.
 * EIO ...  - The same error as returned from <ioerror_filereader> (see also <readnext_filereader>).
 * ENODATA  - There is no more data. Also <iseof_filereader> returns true.
 * ENOBUFS  - The scanned token spans already two buffers and no more than 2 buffers per token are supported.
 *            In other words: the token is too long. */
int readbuffer_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

/* function: unread_utf8scanner
 * Decrements the reading position until the last nrofchars characters are unread.
 * For the last nrofchars characters their size in bytes is summed up into
 * the value nrofbytes and the reading position is decremented by nrofbytes.
 * This works only if the token (returned from <scannedtoken_utf8scanner>) contains at
 * least nrofbytes bytes. In case the token is shorter EINVAL is returned and nothing is done.
 * After successful return the scanned token's length is decremented by nrofbytes which corresponds to
 * nrofchars characters. */
int unread_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd, uint8_t nrofchars) ;



// section: inline implementation

/* function: isnext_utf8scanner
 * Implements <utf8scanner_t.isnext_utf8scanner>. */
#define isnext_utf8scanner(scan)          \
         ( __extension__ ({               \
            const utf8scanner_t * _s ;    \
            _s = (scan) ;                 \
            (_s->next < _s->end) ;        \
         }))

/* function: nextbyte_utf8scanner
 * Implements <utf8scanner_t.nextbyte_utf8scanner>. */
#define nextbyte_utf8scanner(scan)        \
         (*(scan)->next++)

/* function: peekbyte_utf8scanner
 * Implements <utf8scanner_t.peekbyte_utf8scanner>. */
#define peekbyte_utf8scanner(scan, offset) \
         ((scan)->next[(offset)])

/* function: skipbytes_utf8scanner
 * Implements <utf8scanner_t.skipbytes_utf8scanner>. */
#define skipbytes_utf8scanner(scan, nrbytes) \
         ((void)((scan)->next += (nrbytes)))

/* function: sizeunread_utf8scanner
 * Implements <utf8scanner_t.sizeunread_utf8scanner>. */
#define sizeunread_utf8scanner(scan)      \
         ( __extension__ ({               \
            const utf8scanner_t * _s ;    \
            _s = (scan) ;                 \
            (size_t) ( _s->end            \
                  - _s->next) ;           \
         }))

#endif
