/* title: StringStream

   Offers interface to read a string of bytes from memory byte-by-byte.

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

   file: C-kern/api/string/stringstream.h
    Header file <StringStream>.

   file: C-kern/string/stringstream.c
    Implementation file <StringStream impl>.
*/
#ifndef CKERN_STRING_STRINGSTREAM_HEADER
#define CKERN_STRING_STRINGSTREAM_HEADER

// forward
struct string_t ;

/* typedef: struct stringstream_t
 * Export <stringstream_t> into global namespace. */
typedef struct stringstream_t             stringstream_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_stringstream
 * Test <stringstream_t> functionality. */
int unittest_string_stringstream(void) ;
#endif


/* struct: stringstream_t
 * This objects streams a string of bytes byte-by-byte.
 * The string must be located in memory and it must be
 * of static size. Only read operations are supported. */
struct stringstream_t {
   /* variable: next
    * Points to memory address of the next byte which is read.
    * After initialization next points to the start address
    * of the string. */
   const uint8_t  * next ;
   /* variable: end
    * Points to memory address after the last byte of the string.
    * The expression (end-next) computes the number of unread bytes. */
   const uint8_t  * end ;
} ;

// group: lifetime

/* define: stringstream_INIT_FREEABLE
 * Static initializer. */
#define stringstream_INIT_FREEABLE              stringstream_INIT(0,0)

/* define: stringstream_INIT
 * Static initializer. The parameter startaddr is the start address of the string (lowest address in memory).
 * The parameter endaddr must be set to startaddr plus length of string. */
#define stringstream_INIT(startaddr, endaddr)   { startaddr, endaddr }

/* function: init_stringstream
 * Initializes stringstream with start and end address.
 * The start address must point to the lowest address of the string in memory.
 * The end address must point to the last byte of string with the highest address
 * plus one. The start address must always be lower or equal to the end address.
 * If both values are equal the stringstream is initialized with a size of zero. */
int init_stringstream(/*out*/stringstream_t * strstream, const uint8_t * startaddr, const uint8_t * endaddr) ;

/* function: initfromstring_string
 * Initializes stringstream from <string_t>. */
int initfromstring_string(/*out*/stringstream_t * strstream, const struct string_t * str) ;

/* function: free_stringstream
 * Resets stringstream to null string. */
void free_stringstream(stringstream_t * strstream) ;

// group: query

/* function: isempty_stringstream
 * Returns true if there are no more unread bytes. */
bool isempty_stringstream(const stringstream_t * strstream) ;

/* function: size_stringstream
 * Returns the number of unread bytes. */
size_t size_stringstream(const stringstream_t * strstream) ;

/* function: next_stringstream
 * Returns the address of the buffer containing all unread bytes. */
const uint8_t * next_stringstream(const stringstream_t * strstream) ;

/* function: findbyte_stringstream
 * Finds byte in string stream.
 * The returned value points to the position of the found byte.
 * The value 0 is returned if *strstream* does not contain the byte. */
const uint8_t * findbyte_stringstream(const struct string_t * strstream, uint8_t byte) ;

// group: read

/* function: nextbyte_stringstream
 * Returns next unread byte from stream.
 *
 * Attention:
 * This function is unsafe ! It does not *check* that the stream contains
 * at least one more unread byte. Call this function only if you know that
 * <isempty_stringstream> does return *false*. */
uint8_t nextbyte_stringstream(stringstream_t * strstream) ;

/* function: skipbyte_stringstream
 * Skips the next unread byte in the input stream.
 *
 * Attention:
 * This function is unsafe ! It does not *check* that the stream contains
 * at least one more unread byte. Call this function only if you know that
 * <isempty_stringstream> does return *false*. */
void skipbyte_stringstream(stringstream_t * strstream) ;

/* function: skipbytes_stringstream
 * Skips the next size unread bytes in the input stream.
 *
 * Attention:
 * This function is unsafe ! It does not *check* that the stream contains
 * at least size unread bytes. Use either <tryskipbytes_stringstream> which
 * checks the size or call <size_stringstream> before calling this function. */
void skipbytes_stringstream(stringstream_t * strstream, size_t size) ;

/* function: tryskipbytes_stringstream
 * Same as <skipbytes_stringstream> but checks the number of unread bytes before.
 *
 * Returns:
 * 0      - The next size bytes are skipped successfully.
 * EINVAL - Nothing was done cause <streamstream_t> contains less than *size* unread bytes. */
int tryskipbytes_stringstream(stringstream_t * strstream, size_t size) ;

// group: generic

/* function: genericcast_stringstreamstream
 * Converts pointer to strstream of any type to pointer to <stringstream_t>.
 * The conversion checks that *strstream* contains the members next and
 * end in that order. */
stringstream_t * genericcast_stringstream(void * strstream) ;


// section: inline implementation

/* define: findbyte_stringstream
 * Implements <stringstream_t.findbyte_stringstream>. */
#define findbyte_stringstream(strstream, byte)                       \
   ( __extension__({                                                 \
      typeof(strstream) _strstream = (strstream) ;                   \
      memchr(next_stringstream(_strstream), (uint8_t)(byte),         \
             size_stringstream(_strstream)) ;                        \
   }))

/* define: free_stringstream
 * Implements <stringstream_t.free_stringstream>. */
#define free_stringstream(strstream)               ((void)(*(strstream) = (stringstream_t) stringstream_INIT_FREEABLE))

/* define: genericcast_stringstream
 * Implements <stringstream_t.genericcast_stringstream>. */
#define genericcast_stringstream(strstream)                          \
   ( __extension__({                                                 \
      typeof(strstream) _obj = (strstream) ;                         \
      static_assert(                                                 \
         ((uint8_t*)&_obj->end) - ((uint8_t*)&_obj->next)            \
         == offsetof(stringstream_t, end)                            \
         && sizeof(_obj->next) == sizeof(void*)                      \
         && sizeof(_obj->end) == sizeof(void*),                      \
         "member next and member end in that order") ;               \
      if (0) {                                                       \
         volatile uint8_t _err1 ;                                    \
         volatile uint8_t _err2 ;                                    \
         _err1 = _obj->next[0] ;                                     \
         _err2 = _obj->end[0] ;                                      \
         (void) (_err1 + _err2) ;                                    \
      }                                                              \
      (stringstream_t*)(&_obj->next) ;                               \
   }))

/* define: isempty_stringstream
 * Implements <stringstream_t.isempty_stringstream>. */
#define isempty_stringstream(strstream)                              \
   ( __extension__({                                                 \
      typeof(strstream) _strstream = (strstream) ;                   \
      (_strstream->next >= _strstream->end) ;                        \
   }))

/* define: nextbyte_stringstream
 * Implements <stringstream_t.nextbyte_stringstream>. */
#define nextbyte_stringstream(strstream)                             \
      (*((strstream)->next ++))                                      \

/* define: skipbyte_stringstream
 * Implements <stringstream_t.skipbyte_stringstream>. */
#define skipbyte_stringstream(strstream)                             \
   do {                                                              \
      ++ (strstream)->next ;                                         \
   } while(0)

/* define: skipbytes_stringstream
 * Implements <stringstream_t.skipbytes_stringstream>. */
#define skipbytes_stringstream(strstream, size)                      \
   do {                                                              \
      (strstream)->next += (size) ;                                  \
   } while(0)

/* define: tryskipbytes_stringstream
 * Implements <stringstream_t.tryskipbytes_stringstream>. */
#define tryskipbytes_stringstream(strstream, size)                   \
   ( __extension__ ({                                                \
      typeof(strstream) _strstream = (strstream) ;                   \
      size_t _size = (size) ;                                        \
      int    _err  = (size_t) (_strstream->end                       \
                               - _strstream->next)                   \
                     < _size ;                                       \
      if (!_err) {                                                   \
         (_strstream)->next += _size ;                               \
      }                                                              \
      (_err ? EINVAL : 0) ;                                          \
   }))

/* define: size_stringstream
 * Implements <stringstream_t.size_stringstream>. */
#define size_stringstream(strstream)               ((size_t)((strstream)->end - (strstream)->next))

/* define: next_stringstream
 * Implements <stringstream_t.next_stringstream>. */
#define next_stringstream(strstream)               ((strstream)->next)

#endif
