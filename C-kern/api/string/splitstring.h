/* title: SplitString

   Defines a string which can store two substrings.
   Used for scanning noncontiguous (double) buffers.

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

   file: C-kern/api/string/splitstring.h
    Header file <SplitString>.

   file: C-kern/string/splitstring.c
    Implementation file <SplitString impl>.
*/
#ifndef CKERN_STRING_SPLITSTRING_HEADER
#define CKERN_STRING_SPLITSTRING_HEADER

#include "C-kern/api/string/string.h"

/* typedef: struct splitstring_t
 * Export <splitstring_t> into global namespace. */
typedef struct splitstring_t              splitstring_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_splitstring
 * Test <splitstring_t> functionality. */
int unittest_string_splitstring(void) ;
#endif


/* struct: splitstring_t
 * Stores a string which is a concatenation of two other strings.
 * The type manages an array of two strings. The first string at index 0 stores
 * the first part (head) of the string and the string at index 1 (last entry) stores
 * the last part of the string (tail). */
struct splitstring_t {
   /* variable: nrofparts
    * The number of noncontinuous parts the string is composed of.
    * The maximum supported value is 2. See also array <stringpart>. */
   uint32_t    nrofparts ;
   /* variable: stringpart
    * The array of noncontinuous parts the string is composed of.
    * The maximum number of strings is 2. */
   string_t    stringpart[2] ;
} ;

// group: lifetime

/* define: splitstring_INIT_FREEABLE
 * Static initializer. */
#define splitstring_INIT_FREEABLE         { 0, { string_INIT_FREEABLE, string_INIT_FREEABLE } }

/* function: free_splitstring
 * Sets all data members to 0. */
void free_splitstring(splitstring_t * spstr) ;

// group: query

/* function: isfree_splitstring
 * Returns *true* if spstr equals <splitstring_INIT_FREEABLE>. */
bool isfree_splitstring(const splitstring_t * spstr) ;

/* function: nrofparts_splitstring
 * Returns the nr of parts the string is composed of. */
uint8_t nrofparts_splitstring(const splitstring_t * spstr) ;

/* function: addr_splitstring
 * Returns a pointer to start of the string attribute.
 * The valid range for stridx is [0 .. <nrofparts_splitstring>-1].
 * The string is not '\0' terminated. Use <sizepart_splitstring> to determine its size. */
const uint8_t * addr_splitstring(const splitstring_t * spstr, uint8_t stridx) ;

/* function: size_splitstring
 * Returns the size of the string.
 * The valid range for stridx is [0 .. <nrofparts_splitstring>-1]. */
size_t size_splitstring(const splitstring_t * spstr, uint8_t stridx) ;

// group: setter

/* function: setnrofparts_splitstring
 * Sets the nr of parts the string is composed of. */
void setnrofparts_splitstring(splitstring_t * spstr, uint8_t number_of_parts) ;

/* function: setpart_splitstring
 * Sets the start addr and length of a string part.
 * The value stridx determines which part of the string is set -- only two parts are supported.
 * For the first part set stridx to 0 for the last part set it to 1. */
void setpart_splitstring(splitstring_t * spstr, uint8_t stridx, size_t stringsize, const uint8_t stringaddr[stringsize]) ;

/* function: setaddr_splitstring
 * Sets the start addr of a string part.
 * The value stridx determines which part of the string is set -- only two parts are supported.
 * For the first part set stridx to 0 for the last part set it to 1.
 * Use <setsize_splitstring> to set the size of the string. */
void setaddr_splitstring(splitstring_t * spstr, uint8_t stridx, const uint8_t * stringaddr) ;

/* function: setsize_splitstring
 * Sets the size of a string part.
 * See <setaddr_splitstring>. */
void setsize_splitstring(splitstring_t * spstr, uint8_t stridx, size_t stringsize) ;


// section: inline implementation

/* function: free_splitstring
 * Implements <splitstring_t.free_splitstring>. */
#define free_splitstring(spstr)                 \
         ((void)(*(spstr) = (splitstring_t) splitstring_INIT_FREEABLE))

/* function: addr_splitstring
 * Implements <splitstring_t.addr_splitstring>. */
#define addr_splitstring(spstr, stridx)   \
         ((spstr)->stringpart[(stridx)].addr)

/* function: size_splitstring
 * Implements <splitstring_t.size_splitstring>. */
#define size_splitstring(spstr, stridx)   \
         ((spstr)->stringpart[(stridx)].size)

/* function: nrofparts_splitstring
 * Implements <splitstring_t.nrofparts_splitstring>. */
#define nrofparts_splitstring(spstr)          \
         ((uint8_t)((spstr)->nrofparts))

/* function: setnrofparts_splitstring
 * Implements <splitstring_t.setnrofparts_splitstring>. */
#define setnrofparts_splitstring(spstr, number_of_parts) \
         ((void)((spstr)->nrofparts = number_of_parts))

/* function: setaddr_splitstring
 * Implements <splitstring_t.setaddr_splitstring>. */
#define setaddr_splitstring(spstr, stridx, stringaddr)  \
         ((void)((spstr)->stringpart[(stridx)].addr = stringaddr))

/* function: setpart_splitstring
 * Implements <splitstring_t.setpart_splitstring>. */
#define setpart_splitstring(spstr, stridx, stringsize, stringaddr) \
         ((void)((spstr)->stringpart[(stridx)] = (string_t) string_INIT(stringsize, stringaddr)))

/* function: setsize_splitstring
 * Implements <splitstring_t.setsize_splitstring>. */
#define setsize_splitstring(spstr, stridx, stringsize)  \
         ((void)((spstr)->stringpart[(stridx)].size = stringsize))

#endif
