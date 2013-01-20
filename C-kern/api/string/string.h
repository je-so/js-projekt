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

// forward
struct stringstream_t ;

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
 * or <cstring_t> make sure that it does not contain any \0 byte.
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

/* function: initsubstr_string
 * Copies content of srcstr to str. The two string objects are not allowed to overlap.
 * Only references are copied. The characters in memory are not copied. */
static inline void initcopy_string(/*out*/string_t * str, const string_t * restrict srcstr) ;

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

/* function: initsubstr_string
 * Initializes str with substring of fromstr.
 * Same as calling <initcopy_string>(str,fromstr) and then calling <substr_string>(str,start_offset,size). */
int initsubstr_string(/*out*/string_t * str, const string_t * restrict fromstr, size_t start_offset, size_t size) ;

/* function: initfromstringstream_string
 * Initializes str with content of <stringstream_t>.
 * The string points to the unread string buffer of <stringstream_t>. */
void initfromstringstream_string(/*out*/string_t * str, const struct stringstream_t * strstream) ;

/* function: free_string
 * Sets string to <string_INIT_FREEABLE>. */
void free_string(string_t * str) ;

// group: query

/* function: isfree_string
 * Returns true if string has address and size of 0. */
bool isfree_string(const string_t * str) ;

/* function: isempty_string
 * Returns true if string has size 0. A string initialized with <string_INIT_FREEABLE>
 * is considered an empty string. Use <isfree_string> to check if it is uninitialized. */
bool isempty_string(const string_t * str) ;

/* function: addr_string
 * Returns the start address of the string in memory.
 * The start address is the lowest address in memory.
 * To access the second byte of the string use
 * > string_t str = string_INIT(...) ;
 * > uint8_t  seconde_byte = addr_string(&str)[1] ;
 * */
const uint8_t * addr_string(const string_t * str) ;

/* function: size_string
 * Returns size in bytes of string. The size is not the same as the number of contained characters.
 * A character can occupy more than one byte in memory so the size of a string is the upper limit
 * of the number of contained characters. */
size_t size_string(const string_t * str) ;

/* function: findbyte_string
 * Finds byte in string.
 * The returned value points to the position of the found byte.
 * The value 0 is returned if *str* does not contain the byte. */
const uint8_t * findbyte_string(const string_t * str, uint8_t byte) ;

// group: compare

/* function: isequalasciicase_string
 * Returns true if two strings compare equal in a case insensitive way.
 * This functions assumes only 'A'-'Z' and 'a'-'z' as case sensitive characters. */
bool isequalasciicase_string(const string_t * str, const string_t * str2) ;

// group: change

/* function: substr_string
 * The string is made a substring of itself. Call <initsubstr_string> if you want to keep the original.
 * If the precondition fails EINVAL is returned.
 *
 * Parameter:
 * start_offset - The start address of substring relative to the previous start address (see <addr_string>).
 *                This value must be smaller or equal than <size_string>(str).
 * size         - The size (length) in number of bytes of the substring.
 *                This value must be smaller or equal than (<size_string>(str)-start_offset). */
int substr_string(string_t * str, size_t start_offset, size_t size) ;

/* function: shrinkleft_string
 * Shrinks size of string by skipping bytes from the start.
 * Increments the start address of the string and decrements its size by nr_bytes_removed_from_string_start.
 * The parameter nr_bytes_removed_from_string_start must be smaller or equal to <size_string>(str).
 * If the precondition fails EINVAL is returned. */
int shrinkleft_string(string_t * str, size_t nr_bytes_removed_from_string_start) ;

/* function: keeplast_string
 * Shrinks size of string by decrementing its size.
 * The parameter nr_bytes_removed_from_string_end must be smaller or equal to <size_string>(str).
 * If the precondition fails EINVAL is returned.
 * After successfull return the size is decremented by nr_bytes_removed_from_string_end.
 * The start address of the string keeps the same. */
int shrinkright_string(string_t * str, size_t nr_bytes_removed_from_string_end) ;

/* function: skipbyte_string
 * Increments the start address of the string by one and decrements its size.
 *
 * Attention:
 * Call this function only if you know that <isempty_string> does return *false*. */
void skipbyte_string(string_t * str) ;


// group: generic

/* function: genericcast_string
 * Casts a pointer to generic obj type into pointer to <string_t>. */
const string_t * genericcast_string(const void * obj) ;


// section: inline implementation

// group: string_t

/* define: addr_string
 * Implements <string_t.addr_string>. */
#define addr_string(str)                        ((str)->addr)

/* function: findbyte_string
 * Implements <string_t.findbyte_string>. */
#define findbyte_string(str, byte)              ((const uint8_t*)memchr((str)->addr, (uint8_t)(byte), (str)->size))

/* function: free_string
 * Implements <string_t.free_string>. */
#define free_string(str)                        ((void)((*str) = (string_t)string_INIT_FREEABLE))

/* function: genericcast_string
 * Implements <string_t.genericcast_string>. */
#define genericcast_string(obj)                 \
   ( __extension__ ({                           \
      typeof(obj) _obj = (obj) ;                \
      static_assert(                            \
         sizeof(_obj->addr)                     \
         == sizeof(((string_t*)0)->addr)        \
         && 0 == offsetof(string_t, addr),      \
         "addr member is compatible") ;         \
      static_assert(                            \
         sizeof(_obj->size)                     \
         == sizeof(((string_t*)0)->size)        \
         && offsetof(string_t, size)            \
            == ((uintptr_t)&_obj->size)         \
               -((uintptr_t)&_obj->addr),       \
         "size member is compatible") ;         \
      if (0) {                                  \
         volatile uint8_t _err ;                \
         volatile size_t _size = _obj->size ;   \
         _err = _obj->addr[_size] ;             \
         (void) _err ;                          \
      }                                         \
      (const string_t*)(&_obj->addr) ;          \
   }))

/* function: init_string
 * Implements <string_t.init_string>. */
static inline void init_string(/*out*/string_t * str, size_t size, const uint8_t string[size])
{
   str->addr = string ;
   str->size = size ;
}

/* function: initcopy_string
 * Implements <string_t.initcopy_string>. */
static inline void initcopy_string(/*out*/string_t * str, const string_t * restrict srcstr)
{
   *str = *srcstr ;
}

/* define: isempty_string
 * Implements <string_t.isempty_string>. */
#define isempty_string(str)                     (0 == (str)->size)

/* define: isfree_string
 * Implements <string_t.isfree_string>. */
#define isfree_string(str)                      (0 == (str)->addr && 0 == (str)->size)

/* define: size_string
 * Implements <string_t.size_string>. */
#define size_string(str)                      ((str)->size)

/* define: skipbyte_string
 * Implements <string_t.skipbyte_string>. */
#define skipbyte_string(str)                    \
   do {                                         \
      ++ (str)->addr ;                          \
      -- (str)->size ;                          \
   } while(0)

#endif
