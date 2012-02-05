/* title: String
   Offers type to wrap static strings or substrings into an object.

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

   file: C-kern/api/string/string.h
    Header file of <String>.

   file: C-kern/string/string.c
    Implementation file <String impl>.
*/
#ifndef CKERN_API_STRING_HEADER
#define CKERN_API_STRING_HEADER

/* typedef: struct conststring_t
 * Export <conststring_t>. */
typedef struct conststring_t           conststring_t ;

/* typedef: struct string_t
 * Export <string_t>. */
typedef struct string_t                string_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string
 * Test <escapechar_string>. */
int unittest_string(void) ;
#endif


/* struct: conststring_t
 * Same as <string_t>, except memory space is constant. */
struct conststring_t {
   /* variable: addr
    * Start address of const string memory. */
   const uint8_t  * addr ;
   /* variable: size
    * Size in bytes of string memory.
    * The number of characters is lower or equal to this value. */
   size_t         size ;
} ;

// group: lifetime

/* define: conststring_INIT_FREEABLE
 * Static initializer. Sets <conststring_t> to null. */
#define conststring_INIT_FREEABLE      { 0, 0 }

/* define: conststring_INIT
 * See <string_INIT>. */
#define conststring_INIT(strsize, straddr)   { (straddr), (strsize) }

/* function: init_conststring
 * See <init_string>. */
void init_conststring(/*out*/conststring_t * str, size_t size, const uint8_t string[size]) ;

/* function: initfl_conststring
 * See <initfl_string>. */
int initfl_conststring(/*out*/conststring_t * str, const uint8_t * first, const uint8_t * last) ;

/* function: initse_conststring
 * See <initse_string>. */
int initse_conststring(/*out*/conststring_t * str, const uint8_t * start, const uint8_t * end) ;


/* struct: string_t
 * Points to memory which contains a string.
 * This string does *not* include a the trailing '\0' byte.
 * Null bytes are allowed in the string cause <size> describes
 * the length of string and not any implicit trailing \0 byte.
 * If you want to convert this string into a C string, i.e. eiether (char *)
 * or <cstring_t> make sure that is does not include any \0 byte.
 *
 * Empty string:
 * The empty string is represented as
 * > { "", 0 }
 * The size does *not* include any trailing '\0' byte.
 *
 * Undefined string:
 * The undefined (or *null*) string is represented as
 * > { 0, 0 } */
struct string_t {
   /* variable: addr
    * Start address of non-const string memory. */
   uint8_t        * addr ;
   /* variable: size
    * Size in bytes of memory.
    * The number of characters is lower or equal to this value. */
   size_t         size ;
} ;

// group: lifetime

/* define: string_INIT_FREEABLE
 * Static initializer. Sets string_t null. */
#define string_INIT_FREEABLE           { 0, 0 }

/* define: string_INIT
 * Static initializer. Assigns static string buffer to <string_t>.
 *
 * Parameter:
 * strsize   - The length in bytes of the substring. The number of characters represented
 *             is usually less then its size (a character can be represented with multiple bytes).
 * straddr   - The start address of the string. */
#define string_INIT(strsize, straddr)  { (straddr), (strsize) }

/* function: init_string
 * Assigns static string buffer to out param str.
 * Expected parameter:
 * str     - <string_t> object to initialize
 * size    - Length of string in bytes
 * string  - Address of first character. */
void init_string(/*out*/string_t * str, size_t size, uint8_t string[size]) ;

/* function: initfl_string
 * Assigns static string buffer to out param str.
 * Expected parameter:
 * str    - <string_t> object to initialize
 * first  - Address of first character.
 * last   - Address of last character. */
int initfl_string(/*out*/string_t * str, uint8_t * first, uint8_t * last) ;

/* function: initse_string
 * Assigns static string buffer to out param str.
 * Expected parameter:
 * str    - <string_t> object to initialize
 * start  - Address of first character.
 * end    - Address of memory after last character. */
int initse_string(/*out*/string_t * str, uint8_t * start, uint8_t * end) ;

// group: cast

/* function: asconst_string
 * Casts <string_t> to <constcast_t>. */
conststring_t asconst_string(string_t * str) ;


// section: inline implementation

/* define: init_conststring
 * Implements <conststring_t.init_conststring>. */
#define init_conststring(str, size, string)     \
   do {  conststring_t * _str = (str) ;         \
         const uint8_t * _string = (string) ;   \
         init_string((string_t*)_str, (size),   \
                 (uint8_t*)(intptr_t)_string) ; \
   } while(0)

/* define: initfl_conststring
 * Implements <conststring_t.initfl_conststring>. */
#define initfl_conststring(str, first, last)    \
   ( __extension__ ({                           \
         conststring_t * _str   = (str) ;       \
         const uint8_t * _first = (first) ;     \
         const uint8_t * _last  = (last) ;      \
         initfl_string((string_t*)_str,         \
               (uint8_t*)(intptr_t)_first,      \
               (uint8_t*)(intptr_t)_last) ;     \
   }))

/* define: initse_conststring
 * Implements <conststring_t.initse_conststring>. */
#define initse_conststring(str, start, end)     \
   ( __extension__ ({                           \
         conststring_t * _str   = (str) ;       \
         const uint8_t * _start = (start) ;     \
         const uint8_t * _end   = (end) ;       \
         initse_string((string_t*)_str,         \
               (uint8_t*)(intptr_t)_start,      \
               (uint8_t*)(intptr_t)_end) ;      \
   }))

#define asconst_string(str)               \
   ( __extension__ ({                     \
         string_t * _str = (str) ;        \
         (conststring_t*) _str ;          \
   }))


#endif
