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

/* define: conststring_INIT_CSTR
 * Static initializer. Sets <conststring_t> to address C string *cstr* and size to strlen(cstr).
 * Example:
 * > const char    * buffer = "teststring" ;
 * > conststring_t str      = conststring_t(buffer) ; */
#define conststring_INIT_CSTR(cstr)          { (const uint8_t*)(cstr), strlen(cstr) }

/* function: init_conststring
 * See <init_string>. */
void init_conststring(/*out*/conststring_t * str, size_t size, const uint8_t string[size]) ;

/* function: initfl_conststring
 * See <initfl_string>. */
int initfl_conststring(/*out*/conststring_t * str, const uint8_t * first, const uint8_t * last) ;

/* function: initse_conststring
 * See <initse_string>. */
int initse_conststring(/*out*/conststring_t * str, const uint8_t * start, const uint8_t * end) ;

// group: compare

/* function: isequalasciicase_conststring
 * Returns true if two strings compare equal under in a case insensitive way.
 * This functions assumes only 'A'-'Z' and 'a'-'z' as case sensitive characters. */
bool isequalasciicase_conststring(const conststring_t * str, const conststring_t * str2) ;


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

// group: query

/* function: isempty_string
 * Returns true if string has length 0. */
bool isempty_string(const string_t * str) ;

// group: cast

/* function: asconst_string
 * Casts <string_t> to <constcast_t>. */
conststring_t asconst_string(string_t * str) ;

// group: change

/* function: skipbytes_string
 * Increments the start address of the string and decrements its length by size bytes.
 * Call this function only if you know that (<string_t.size> >= size) is valid.
 * The content of the string is not changed. */
void skipbytes_string(string_t * str, size_t size) ;

/* function: skipbyte_string
 * Increments the start address of the string by one and decrements its size.
 * Call this function only if you know that <isempty_string> does return *false*.
 * The content of the string is not changed. */
void skipbyte_string(string_t * str) ;

/* function: tryskipbytes_string
 * Tries to increments the start address of the string and decrements its length by size bytes.
 * The content of the string is not changed.
 *
 * Returns:
 * 0      - The address was successul incremented and the length decrementd by size bytes.
 * EINVAL - The condition (<string_t.size> >= size) is false. *str* is not changed and no error is logged. */
int tryskipbytes_string(string_t * str, size_t size) ;


// section: inline implementation

/* define: asconst_string
 * Implements <string_t.asconst_string>. */
#define asconst_string(str)               \
   ( __extension__ ({                     \
         string_t * _str = (str) ;        \
         (conststring_t*) _str ;          \
   }))

/* define: init_conststring
 * Implements <conststring_t.init_conststring>. */
#define init_conststring(str, size, string)                 \
   do {  conststring_t * _str = (str) ;                     \
         uint8_t * _string = CONST_CAST(uint8_t, string) ;  \
         init_string((string_t*)_str, (size), _string) ;    \
   } while(0)

/* define: initfl_conststring
 * Implements <conststring_t.initfl_conststring>. */
#define initfl_conststring(str, first, last)             \
   ( __extension__ ({                                    \
         conststring_t * _str = (str) ;                  \
         uint8_t * _first = CONST_CAST(uint8_t, first) ; \
         uint8_t * _last  = CONST_CAST(uint8_t, last) ;  \
         initfl_string((string_t*)_str, _first, _last) ; \
   }))

/* define: initse_conststring
 * Implements <conststring_t.initse_conststring>. */
#define initse_conststring(str, start, end)              \
   ( __extension__ ({                                    \
         conststring_t * _str = (str) ;                  \
         uint8_t * _start = CONST_CAST(uint8_t, start) ; \
         uint8_t * _end   = CONST_CAST(uint8_t, end) ;   \
         initse_string((string_t*)_str, _start, _end) ;  \
   }))

/* define: isempty_string
 * Implements <string_t.isempty_string>. */
#define isempty_string(str)                     (0 == (str)->size)

/* define: skipbytes_string
 * Implements <string_t.skipbytes_string>. */
#define skipbytes_string(str, _size)            \
   do {                                         \
      size_t _size2 = (_size) ;                 \
      (str)->addr += _size2 ;                   \
      (str)->size -= _size2 ;                   \
   } while(0)

/* define: tryskipbytes_string
 * Implements <string_t.tryskipbytes_string>. */
#define tryskipbytes_string(str, _size)         \
   ( __extension__ ({                           \
      size_t _size2 = (_size) ;                 \
      int    _err = ((str)->size < _size2) ;    \
      if (!_err) {                              \
         (str)->addr += _size2 ;                \
         (str)->size -= _size2 ;                \
      }                                         \
      (_err ? EINVAL : 0) ;                     \
   }))

/* define: skipbyte_string
 * Implements <string_t.skipbyte_string>. */
#define skipbyte_string(str)                    \
   do {                                         \
      ++ (str)->addr ;                          \
      -- (str)->size ;                          \
   } while(0)

#endif
