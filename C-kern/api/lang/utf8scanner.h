/* title: UTF8-Scanner

   Exports <utf8scanner_t> which supports to break a text file
   into separate strings. The file is read with help of <filereader_t>.
   The common parts of every text scanner is implemented in this type.

   Do not forget to include <FileReader> before calling some of the functions.

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
#include "C-kern/api/string/utf8.h"

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
 * Handles the buffers returned from <filereader_t> and initializes a token of type <splitstring_t>.
 *
 * Protocol:
 * The protocol of utf8scanner is to call <nextbyte_utf8scanner> until the beginning of a token is found.
 * Then call <settokenstart_utf8scanner> to remember the address of the last read byte as token start.
 * Call then one or more times <nextbyte_utf8scanner>, <isnext_utf8scanner>, <peekbyte_utf8scanner>
 * and then <settokenend_utf8scanner> after a valid token (lexeme, string or word) has been recognized.
 *
 * The function pair <settokenstart_utf8scanner> and <settokenend_utf8scanner> initilaizes the internal token
 * of type <splitstring_t> to the correct values. Use <scannedtoken_utf8scanner> to get a pointer to the
 * initialized string.
 *
 * If <isnext_utf8scanner> returns false during scanning call <readbuffer_utf8scanner> to acquire a new
 * buffer from <filereader_t>. If ENODATA is returned then the whole file is read. */
struct utf8scanner_t {
   /* variable: next
    * Points to the next byte returned from <nextbyte_utf8scanner>. */
   const uint8_t     * next ;
   /* variable: end
    * As long as <next> is lower than <end> there are more bytes to read. */
   const uint8_t     * end ;
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
 * Sets all data members to 0. */
int init_utf8scanner(/*out*/utf8scanner_t * scan) ;

/* function: free_utf8scanner
 * Sets all data members to 0 and releases any acquired buffers. */
int free_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

// group: query

/* function: isfree_utf8scanner
 * Returns true if scan is initialized with <utf8scanner_INIT_FREEABLE>. */
bool isfree_utf8scanner(const utf8scanner_t * scan) ;

/* function: isnext_utf8scanner
 * Returns *true* if the buffer contains at least one more byte.
 * If the return value is *false* then you need to call <readbuffer_utf8scanner>
 * which acquires the next buffer from <filereader_t>. If <readbuffer_utf8scanner>
 * returned 0 (OK) then the next call to <isnext_utf8scanner> returns true. */
bool isnext_utf8scanner(const utf8scanner_t * scan) ;

/* function: scannedtoken_utf8scanner
 * Returns the address to an internally stored <splitstring_t>.
 * The content of the returned string is only valid after calling <endscan_utf8scanner>
 * with no error. The returned string is valid as long as no other function is called
 * except query functions. */
const splitstring_t * scannedtoken_utf8scanner(const utf8scanner_t * scan) ;

// group: read

/* function: cleartoken_utf8scanner
 * Marks the internal buffers as unused.
 * If you only want to scan data without keeping fully read buffers in memory
 * call this function. If you call <readbuffer_utf8scanner> after <isnext_utf8scanner> returned
 * false the read buffer is released and a new one is acquired. */
int cleartoken_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

/* function: settokenstart_utf8scanner
 * Before a new string is scanned call this function.
 * The address of the last read byte is stored as start
 * address of the string. */
int settokenstart_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

/* function: settokenend_utf8scanner
 * Computes the string length of the scanned token and stores it.
 * The length is computed from the difference of the start address of the string and address of the current read position.
 * After calling this function the string returned from <scannedtoken_utf8scanner> is valid.
 * If <settokenstart_utf8scanner> was not called before this function computes an undefined value. */
void settokenend_utf8scanner(utf8scanner_t * scan) ;

/* function: nextbyte_utf8scanner
 * Reads the next byte from the buffer.
 * Call this function only if <isnext_utf8scanner> returned true else the behaviour is undefined. */
uint8_t nextbyte_utf8scanner(utf8scanner_t * scan) ;

/* function: peekbyte_utf8scanner
 * Returns the next byte of the buffer without incrementing the read pointer.
 * Call this function only if <isnext_utf8scanner> returned true else the behaviour is undefined. */
uint8_t peekbyte_utf8scanner(utf8scanner_t * scan) ;

/* function: readbuffer_utf8scanner
 * Acquires the next buffer from <filereader_t> if <isnext_utf8scanner> returns false.
 *
 * Returns:
 * 0        - If <isnext_utf8scanner> returned false before this call then <acquirenext_filereader> was called
 *            and the read pointer points to the new buffer content. If <isnext_utf8scanner> returned true
 *            before this call nothing is done.
 * EIO      - If <ioerror_filereader> returns a value != 0 this value is returned or <acquirenext_filereader>
 *            returned an error.
 * ENODATA  - There is no more data. Also <iseof_filereader> returns true. */
int readbuffer_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

/* function: skipbyte_utf8scanner
 * Increments the read pointer by one without reading the byte.
 * Call this function only if <isnext_utf8scanner> returned true else the behaviour is undefined. */
void skipbyte_utf8scanner(utf8scanner_t * scan) ;


// section: inline implementation

/* function: cleartoken_utf8scanner
 * Implements <utf8scanner_t.cleartoken_utf8scanner>. */
#define cleartoken_utf8scanner(scan, frd)                               \
         ( __extension__ ({                                             \
            int _err = 0 ;                                              \
            typeof(scan) _scan = (scan) ;                               \
            if (2 == nrofparts_splitstring(&_scan->scanned_token)) {    \
               _err = release_filereader(frd) ;                         \
            }                                                           \
            setnrofparts_splitstring(&_scan->scanned_token, 0) ;        \
            _err ;                                                      \
         }))

/* function: nextbyte_utf8scanner
 * Implements <utf8scanner_t.nextbyte_utf8scanner>. */
#define isnext_utf8scanner(scan)                \
         ( __extension__ ({                     \
            const utf8scanner_t * _s = (scan) ; \
            (_s->next < _s->end) ;              \
         }))

/* function: nextbyte_utf8scanner
 * Implements <utf8scanner_t.nextbyte_utf8scanner>. */
#define nextbyte_utf8scanner(scan)              \
         (*(scan)->next++)

/* function: peekbyte_utf8scanner
 * Implements <utf8scanner_t.peekbyte_utf8scanner>. */
#define peekbyte_utf8scanner(scan)              \
         (*(scan)->next)

/* function: scannedtoken_utf8scanner
 * Implements <utf8scanner_t.scannedtoken_utf8scanner>. */
#define scannedtoken_utf8scanner(scan)          \
         ((const splitstring_t *)(&(scan)->scanned_token))

/* function: settokenend_utf8scanner
 * Implements <utf8scanner_t.settokenend_utf8scanner>. */
#define settokenend_utf8scanner(scan)                                \
         do {                                                        \
            typeof(scan) _scan = (scan) ;                            \
            if (nrofparts_splitstring(&_scan->scanned_token)) {      \
               /* settokenstart_utf8scanner was called before */     \
               /* set type and size of token */                      \
               unsigned  _stridx ;                                   \
               const uint8_t * _straddr ;                            \
               _stridx = nrofparts_splitstring(                      \
                           &_scan->scanned_token) ;                  \
               _straddr = addr_splitstring(&_scan->scanned_token,    \
                           _stridx - 1) ;                            \
               setsize_splitstring( &_scan->scanned_token,           \
                           _stridx - 1,                              \
                           (size_t) (_scan->next - _straddr)) ;      \
            }                                                        \
         } while (0)

/* function: settokenstart_utf8scanner
 * Implements <utf8scanner_t.settokenstart_utf8scanner>. */
#define settokenstart_utf8scanner(scan, frd)                            \
         ( __extension__ ({                                             \
            int _err = 0 ;                                              \
            typeof(scan) _scan = (scan) ;                               \
            if (2 == nrofparts_splitstring(&_scan->scanned_token)) {    \
               _err = release_filereader(frd) ;                         \
            }                                                           \
            setnrofparts_splitstring(&_scan->scanned_token, 1) ;        \
            setpart_splitstring( &_scan->scanned_token,                 \
                                 0, 0, _scan->next-1) ;                 \
            _err ;                                                      \
         }))

/* function: skipbyte_utf8scanner
 * Implements <utf8scanner_t.skipbyte_utf8scanner>. */
#define skipbyte_utf8scanner(scan)              \
         ((void)((scan)->next++))

#endif
