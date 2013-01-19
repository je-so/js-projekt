/* title: CString
   Encapsulates C string datatype and hides memory management.

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

   file: C-kern/api/string/cstring.h
    Header file of <CString>.

   file: C-kern/string/cstring.c
    Implementation file <CString impl>.
*/
#ifndef CKERN_STRING_CSTRING_HEADER
#define CKERN_STRING_CSTRING_HEADER

// forward
struct string_t ;

/* typedef: struct cstring_t
 * Export <cstring_t>. */
typedef struct cstring_t               cstring_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_cstring
 * Tests <cstring_t>. */
int unittest_string_cstring(void) ;
#endif


/* struct: cstring_t
 * Dynamically growing C string with trailing '\0' byte. */
struct cstring_t
{
   /* variable: length
    * Length of string in bytes (sizeof(char)==1). The number of characters could be smaller if the encoding is UTF-8 for example.
    *
    * Invariant:
    * (0 == <chars>) || <chars>[size] == 0 */
   size_t   size ;
   /* variable: allocated_size
    * Size of allocated memory block <chars> points to.
    *
    * Invariant:
    * (0 == <chars>) || <size> + 1 <= allocated_size */
   size_t   allocated_size ;
   /* variable: chars
    * Pointer to character array with '\0' at the end of the string.
    *
    * Invariant:
    * ((0 == <chars>) && (0 == allocated_size)) || ((0 != chars) && (0 != allocated_size)) */
   uint8_t  * chars ;
} ;

// group: lifetime

/* define: cstring_INIT_FREEABLE
 * Static initializer. Makes calling <free_cstring> a no op. */
#define cstring_INIT_FREEABLE          { (size_t)0, (size_t)0, (void*)0 }

/* define: cstring_INIT
 * Static initializer. Same as calling <init_cstring> with preallocate_size set to 0. */
#define cstring_INIT                   { (size_t)0, (size_t)0, (void*)0 }

/* function: init_cstring
 * Inits <cstring_t> and preallocates memory.
 * If you set *preallocate_size* to 0 no memory is preallocated. */
int init_cstring(/*out*/cstring_t * cstr, size_t preallocate_size) ;

/* function: initfromstring_cstring
 * Inits <cstring_t> and copies data from <string_t>.
 * If parameter *copiedfrom* is empty no data is preallocated. */
int initfromstring_cstring(/*out*/cstring_t * cstr, const struct string_t * copiedfrom) ;

/* function: initmove_cstring
 * Inits dest with content of source and sets source to <cstring_INIT_FREEABLE>. */
void initmove_cstring(/*out*/cstring_t * restrict dest, cstring_t * restrict source) ;

/* function: free_cstring
 * Frees any allocated memory associated with type <cstring_t>. */
int free_cstring(cstring_t * cstr) ;

// group: query

/* function: str_cstring
 * Returns '\0' terminated string.
 * The returned value can be NULL in case cstr->chars is NULL.
 * The returned value is valid as long as *cstr* is not changed. */
char * str_cstring(cstring_t * cstr) ;

/* function: size_cstring
 * Returns the size of the string in bytes.
 * For multibyte encoded characters the length of a string in characters
 * is less than the size in bytes. */
size_t size_cstring(const cstring_t * cstr) ;

/* function: allocatedsize_cstring
 * Returns the allocated buffer size in bytes.
 * To access the buffer the start address of the buffer use <str_cstring>. */
size_t allocatedsize_cstring(const cstring_t * cstr) ;

// group: change

/* function: allocate_cstring
 * Makes sure *cstr* has at least *allocate_size* preallocated bytes.
 * The string buffer is reallocated if necessary. If it is big enough
 * nothing is done. After successful return the buffer returned from
 * <str_cstring> points to at least *allocate_size* bytes of memory. */
int allocate_cstring(cstring_t * cstr, size_t allocate_size) ;

/* function: append_cstring
 * Appends *str* with len *str_len* to cstr.
 *
 * Attention:
 * It is not allowed to use parts of the buffer of cstr as arguments for this function.
 * Cause during a possible reallocation of the buffer these arguments become invalid. */
int append_cstring(cstring_t * cstr, size_t str_len, const char str[str_len]) ;

/* function: clear_cstring
 * Sets size of string to 0.
 * This function has the same result as calling <truncate_cstring> with parameter 0.
 * No memory is deallocated. */
void clear_cstring(cstring_t * cstr) ;

/* function: printfappend_cstring
 * Appends printf formatted string to cstr. Like sprintf but the buffer is reallocated if it is too small
 * and the new content is appended to the end of the string.
 *
 * Attention:
 * It is not allowed to use parts of the buffer of cstr as arguments for this function.
 * Cause during a possible reallocation of the buffer these arguments become invalid. */
int printfappend_cstring(cstring_t * cstr, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3))) ;

/* function: resize_cstring
 * Allocates memory and sets size to *new_size*.
 * The possibly reallocated buffer can be accessed with a call to <str_cstring>.
 * If the new size is bigger than the current size the buffer
 * will contain "random" characters. */
int resize_cstring(cstring_t * cstr, size_t new_size) ;

/* function: truncate_cstring
 * Adapts size of cstr to a smaller value.
 * Use this if you you want to make the string smaller in size.
 * A trailing 0 byte is added by this call.
 * Use this function with caution cause new_size is interpreted as
 * byte offset. If a character uses more than one byte for its encoding
 * and if new_size points not to the end of a valid character sequence
 * the encoded character sequence becomes invalid. */
int truncate_cstring(cstring_t * cstr, size_t new_size) ;


// section: inline implementation

/* define: initmove_cstring
 * Implements <cstring_t.initmove_cstring>. */
#define initmove_cstring(dest, source)    \
      do {  *(dest) = *(source) ; *(source) = (cstring_t) cstring_INIT_FREEABLE ; } while(0)

/* define: allocatedsize_cstring
 * Implements <cstring_t.allocatedsize>. */
#define allocatedsize_cstring(cstr)    ((cstr)->allocated_size)

/* define: size_cstring
 * Implements <cstring_t.size_cstring>. */
#define size_cstring(cstr)             ((cstr)->size)

/* define: str_cstring
 * Implements <cstring_t.str_cstring>. */
#define str_cstring(cstr)              ((char*)(cstr)->chars)

#endif
