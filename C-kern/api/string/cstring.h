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
    * (0 == <chars>) || <chars>[length] == 0 */
   size_t   length ;
   /* variable: allocated_size
    * Size of allocated memory block <chars> points to.
    *
    * Invariant:
    * (0 == <chars>) || <length> + 1 <= allocated_size */
   size_t   allocated_size ;
   /* variable: chars
    * Pointer to character array with '\0' at the end of the string.
    *
    * Invariant:
    * ((0 == <chars>) && (0 == allocated_size)) || ((0 != chars) && (0 != allocated_size)) */
   char   * chars ;
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

/* function: initmove_cstring
 * Inits dest with content of source and sets source to <cstring_INIT_FREEABLE>. */
void initmove_cstring(/*out*/cstring_t * restrict dest, cstring_t * restrict source) ;

/* function: free_cstring
 * Frees any allocated memory associated with type <cstring_t>. */
int free_cstring(cstring_t * cstr) ;

// group: query

/* function: str_cstring
 * Returns "\0" terminated string.
 * The returned value can be NULL in case cstr->chars is NULL.
 * The returned value is valid as long as *cstr* is not changed. */
char * str_cstring(cstring_t * cstr) ;

/* function: length_cstring
 * Returns the length of the string in bytes.
 * For mbs encoded strings the length in character is less
 * than the length in bytes. */
size_t length_cstring(const cstring_t * cstr) ;

/* function: allocatedsize_cstring
 * Returns the allocated buffer size in bytes.
 * To access the buffer the start address of the buffer use <str_cstring>. */
size_t allocatedsize_cstring(const cstring_t * cstr) ;

// group: change

/* function: allocate_cstring
 * Allocates memory of at least allocate_size bytes.
 * If the already allocated string buffer size is equal or greater than allocated_size bytes
 * nothing is done. Use <str_cstring> to access a pointer to the preallocated buffer. */
int allocate_cstring(cstring_t * cstr, size_t allocate_size) ;

/* function: append_cstring
 * Appends *str* with len *str_len* to cstr.
 *
 * Attention:
 * It is not allowed to use parts of the buffer of cstr as arguments for this function.
 * Cause during a possible reallocation of the buffer these arguments become invalid. */
int append_cstring(cstring_t * cstr, size_t str_len, const char str[str_len]) ;

/* function: printfappend_cstring
 * Appends printf formatted string to cstr. Like sprintf but the buffer is reallocated if it is too small
 * and the new content is appended to the end of the string.
 *
 * Attention:
 * It is not allowed to use parts of the buffer of cstr as arguments for this function.
 * Cause during a possible reallocation of the buffer these arguments become invalid. */
int printfappend_cstring(cstring_t * cstr, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3))) ;

/* function: adaptlength_cstring
 * Adapts length of cstr. Use it if you have changed the content of the allocated buffer
 * and want to let length reflect the new position of the \0 byte.
 *
 * This function throws an assertion if no null byte is found. */
void adaptlength_cstring(cstring_t * cstr) ;

/* function: truncate_cstring
 * Adapts length of cstr to a smaller value.
 * Use this if you you want to make the string smaller in length.
 * A trailing 0 byte is added by this call.
 * Use this function with caution cause new_length is interpreted as
 * byte offset. If a character uses more than one byte for its encoding
 * and if new_length points not to the end of a valid character sequence
 * the encoded character sequence becomes invalid. */
int truncate_cstring(cstring_t * cstr, size_t new_length) ;


// section: inline implementations

/* define: initmove_cstring
 * Implements <cstring_t.initmove_cstring>. */
#define initmove_cstring(dest, source) \
   do {  *(dest) = *(source) ; *(source) = (cstring_t) cstring_INIT_FREEABLE ; } while(0)

/* define: adaptlength_cstring
 * Implements <cstring_t.adaptlength_cstring>. */
#define adaptlength_cstring(cstr) \
   do {  if ((cstr)->allocated_size) { void * pos = memchr( (cstr)->chars, 0, (cstr)->allocated_size ) ; (cstr)->length = (size_t) ((char*)pos - (cstr)->chars) ; assert(pos && (cstr)->length < (cstr)->allocated_size) ; } } while (0)

/* define: allocatedsize_cstring
 * Implements <cstring_t.allocatedsize>. */
#define allocatedsize_cstring(cstr)           ((cstr)->allocated_size)

/* define: length_cstring
 * Implements <cstring_t.length_cstring>. */
#define length_cstring(cstr)           ((cstr)->length)

/* define: str_cstring
 * Implements <cstring_t.str_cstring>. */
#define str_cstring(cstr)              ((cstr)->chars)

#endif
