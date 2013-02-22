/* title: UTF8-Scanner


   Do not forget to include <FileReader> to define type <filereader_t>
   before calling some of the functions.

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

#include "C-kern/api/lang/splittoken.h"
#include "C-kern/api/string/utf8.h"

// forward
struct filereader_t ;

/* typedef: struct utf8scanner_t
 * Export <utf8scanner_t> into global namespace. */
typedef struct utf8scanner_t           utf8scanner_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_lang_utf8scanner
 * Test <utf8scanner_t> functionality. */
int unittest_lang_utf8scanner(void) ;
#endif


/* struct: utf8scanner_t
 * Handles the buffers returned from <filereader_t> and initializes a variable of type <splittoken_t>.
 *
 * Protocol:
 * The protocol of utf8scanner is to call <beginscan_utf8scanner> first and then call
 * one or more times <nextbyte_utf8scanner>, <isnext_utf8scanner>, <peekbyte_utf8scanner>
 * and then <endscan_utf8scanner> if a valid token is recognized.
 *
 * The function pair <beginscan_utf8scanner> and <endscan_utf8scanner> initilaizes the internal token
 * of type <splittoken_t> to the correct values. Use <scannedtoken_utf8scanner> to get a pointer to the
 * initialized token.
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
    * Stores the begin and length of the scanned token.
    * The token can be scattered across two buffers. */
   splittoken_t      scanned_token ;
} ;

// group: lifetime

/* define: utf8scanner_INIT_FREEABLE
 * Static initializer. */
#define utf8scanner_INIT_FREEABLE      { 0, 0, splittoken_INIT_FREEABLE }

/* function: init_utf8scanner
 * Sets all data members to 0. */
int init_utf8scanner(/*out*/utf8scanner_t * scan) ;

/* function: free_utf8scanner
 * Sets all data members to 0 and releases any acquired buffers. */
int free_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

// group: query

/* function: isnext_utf8scanner
 * Returns *true* if the buffer contains at least one more byte.
 * If the return value is *false* then you need to call <readbuffer_utf8scanner>
 * which acquires the next buffer from <filereader_t>. If <readbuffer_utf8scanner>
 * returned 0 (OK) then the next call to <isnext_utf8scanner> returns true. */
bool isnext_utf8scanner(utf8scanner_t * scan) ;

/* function: scannedtoken_utf8scanner
 * Returns the address to an internally stored <splittoken_t>.
 * The content of the returned token is only valid after calling <endscan_utf8scanner>
 * with no error. The returned token is valid as long as no other function is called
 * except query functions. */
splittoken_t * scannedtoken_utf8scanner(utf8scanner_t * scan) ;

// group: read

/* function: beginscan_utf8scanner
 * Before a new token is scanned call this function.
 * The address of the read pointer is stored as start
 * address of the token. If the buffer is empty <readbuffer_utf8scanner> is called first.
 * After successful return the buffer contains at least one more byte. */
int beginscan_utf8scanner(utf8scanner_t * scan, struct filereader_t * frd) ;

/* function: endscan_utf8scanner
 * Computes the length of the token and stores it.
 * The kength is computed from the start address of the token and the current read pointer into the buffer.
 * Also the token type and subtype is stored in the the token.
 * After calling this function the token returned from <scannedtoken_utf8scanner> is valid. */
int endscan_utf8scanner(utf8scanner_t * scan, uint16_t tokentype, uint8_t tokensubtype) ;

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


// section: inline implementation

/* function: nextbyte_utf8scanner
 * Implements <utf8scanner_t.nextbyte_utf8scanner>. */
#define isnext_utf8scanner(scan)          \
         ( __extension__ ({               \
            utf8scanner_t * _s = (scan) ; \
            (_s->next < _s->end) ;        \
         }))

/* function: nextbyte_utf8scanner
 * Implements <utf8scanner_t.nextbyte_utf8scanner>. */
#define nextbyte_utf8scanner(scan)        \
         (*(scan)->next++)

/* function: peekbyte_utf8scanner
 * Implements <utf8scanner_t.peekbyte_utf8scanner>. */
#define peekbyte_utf8scanner(scan)        \
         (*(scan)->next)

/* function: scannedtoken_utf8scanner
 * Implements <utf8scanner_t.scannedtoken_utf8scanner>. */
#define scannedtoken_utf8scanner(scan)    \
         (&(scan)->scanned_token)

#endif
