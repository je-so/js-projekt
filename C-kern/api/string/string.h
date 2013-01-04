/* title: String

   Offers type to wrap constant strings or substrings into an object.

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
int unittest_string(void) ;
#endif


/* struct: string_t
 * Points to memory which contains a constant string.
 * This string does *not* include a trailing '\0' byte.
 * Null bytes are allowed in the string cause <size> describes
 * the length of string and not any implicit trailing \0 byte.
 * If you want to convert this string into a C string, i.e. either (char *)
 * or <cstring_t> make sure that is does not include any \0 byte.
 *
 * Empty string:
 * The empty string is represented as
 * > { "", 0 }
 * The size does *not* include any trailing '\0' byte.
 *
 * Undefined string:
 * The undefined (or *null*) string is represented as
 * > { 0, 0 }
 * */
struct string_t {
   /* variable: addr
    * Start address of non-const string memory. */
   const uint8_t  * addr ;
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

/* define: string_INIT_CSTR
 * Static initializer. Sets <string_t> to address of *cstr* and size to strlen(cstr).
 *
 * Example:
 * > const char   * buffer = "teststring" ;
 * > string_t     str      = string_t(buffer) ; */
#define string_INIT_CSTR(cstr)          { (const uint8_t*)(cstr), strlen(cstr) }

/* function: init_string
 * Assigns constant string buffer to str.
 *
 * Expected parameter:
 * str     - pointer to <string_t> object which is initialized
 * size    - Length of string in bytes
 * string  - Address of first character. */
static inline void init_string(/*out*/string_t * str, size_t size, const uint8_t string[size]) ;

/* function: initfl_string
 * Assigns static string buffer to str.
 * To initialize as empty string set last to a value smaller first.
 * If first == last the size of the string is one.
 *
 * Expected parameter:
 * str    - pointer to <string_t> object which is initialized
 * first  - Address of first character.
 * last   - Address of last character. */
int initfl_string(/*out*/string_t * str, const uint8_t * first, const uint8_t * last) ;

/* function: initse_string
 * Assigns static string buffer to str.
 *
 * Expected parameter:
 * str    - pointer to <string_t> object which is initialized
 * start  - Address of first character.
 * end    - Address of memory after last character. */
int initse_string(/*out*/string_t * str, const uint8_t * start, const uint8_t * end) ;

// group: query

/* function: isfree_string
 * Returns true if string has address and size of 0. */
bool isfree_string(const string_t * str) ;

/* function: isempty_string
 * Returns true if string has size 0. A string initialized with <string_INIT_FREEABLE>
 * is considered an empty string. Use <isfree_string> to check if it is uninitialized. */
bool isempty_string(const string_t * str) ;

// group: compare

/* function: isequalasciicase_string
 * Returns true if two strings compare equal in a case insensitive way.
 * This functions assumes only 'A'-'Z' and 'a'-'z' as case sensitive characters. */
bool isequalasciicase_string(const string_t * str, const string_t * str2) ;

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

// group: string_t

/* function: init_string
 * Implements <string_t.init_string>. */
static inline void init_string(/*out*/string_t * str, size_t size, const uint8_t string[size])
{
   str->addr = string ;
   str->size = size ;
}

/* define: isempty_string
 * Implements <string_t.isempty_string>. */
#define isempty_string(str)                     (0 == (str)->size)

/* define: isfree_string
 * Implements <string_t.isfree_string>. */
#define isfree_string(str)                      (0 == (str)->addr && 0 == (str)->size)

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
