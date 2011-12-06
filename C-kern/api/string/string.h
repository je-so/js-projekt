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

/* typedef: struct string_t
 * Export <string_t>. */
typedef struct string_t                string_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string
 * Test <escapechar_string>. */
extern int unittest_string(void) ;
#endif


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
    * Start address of the string memory. */
   const char   * addr ;
   /* variable: size
    * Size in bytes of string memory.
    * The string length (number of characters) is <= this value. */
   size_t         size ;
} ;

/* define: string_INIT_FREEABLE
 * Static initializer. Sets string_t null or undefined string. */
#define string_INIT_FREEABLE           { 0, 0 }

/* define: string_INIT
 * Static initializer. Assigns static string buffer to <string_t>.
 *
 * Parameter:
 * strsize   - The length in bytes of the substring. The number of characters represented
 *             is usually less then its size (a character can be represented with multiple bytes).
 * straddr   - The start address of the string. */
#define string_INIT(strsize, straddr)  { (straddr), (strsize) }


// section: inline implementation


#endif
