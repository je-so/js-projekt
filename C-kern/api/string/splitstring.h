/* title: SplitString

   Defines a string which can store two substrings.
   Used for scanning noncontiguous (double) buffers.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
 * Stores two contiguous sub strings.
 * The first string (at index 0) stores the first part of the combined non-contiguous string and
 * the last string (at index 1) stores the last part of the non-contiguous string. */
struct splitstring_t {
   /* variable: stringpart
    * The array of noncontinuous parts the string is composed of.
    * The maximum number of strings is 2. */
   string_t    stringpart[2] ;
   /* variable: nrofparts
    * The number of noncontinuous parts the string is composed of.
    * The maximum supported value is 2. See also array <stringpart>. */
   uint8_t     nrofparts ;
} ;

// group: lifetime

/* define: splitstring_FREE
 * Static initializer. */
#define splitstring_FREE { { string_FREE, string_FREE }, 0 }

/* function: free_splitstring
 * Sets all data members to 0. */
void free_splitstring(splitstring_t * spstr) ;

// group: query

/* function: isfree_splitstring
 * Returns *true* if spstr equals <splitstring_FREE>. */
bool isfree_splitstring(const splitstring_t * spstr) ;

/* function: nrofparts_splitstring
 * Returns the nr of parts the string is composed of.
 *
 * Returns:
 * 0 - No part is valid and therefore the empty string.
 * 1 - Only the first part is valid.
 * 2 - The first and the last part are valid. */
uint8_t nrofparts_splitstring(const splitstring_t * spstr) ;

/* function: addr_splitstring
 * Returns the start address of a part of the string at index stridx.
 * The returned value is only valid if <nrofparts_splitstring> is > stridx. */
const uint8_t * addr_splitstring(const splitstring_t * spstr, uint8_t stridx) ;

/* function: size_splitstring
 * Returns size of a part of the string at index stridx.
 * The returned value is only valid if <nrofparts_splitstring> is > stridx. */
size_t size_splitstring(const splitstring_t * spstr, uint8_t stridx) ;

// group: setter

/* function: setnrofparts_splitstring
 * Sets the nr of parts the string is composed of.
 * Use the value 0 if the string is empty. Use the value 1 if only the first part is valid.
 * Use value 2 if the first and last part are valid.
 * A value > 2 is wrong and results in undefined behaviour. */
void setnrofparts_splitstring(splitstring_t * spstr, uint8_t number_of_parts) ;

/* function: setstring_splitstring
 * Sets the stringaddr and stringsize of a part of the string at index stridx.
 * The behaviour is undefined if stridx > 2. */
void setstring_splitstring(splitstring_t * spstr, uint8_t stridx, size_t stringsize, const uint8_t stringaddr[stringsize]) ;

/* function: setsize_splitstring
 * Changes the size of the a part of the string at index stridx.
 * The behaviour is undefined if stridx > 2. */
void setsize_splitstring(splitstring_t * spstr, uint8_t stridx, size_t stringsize) ;



// section: inline implementation

/* function: free_splitstring
 * Implements <splitstring_t.free_splitstring>. */
#define free_splitstring(spstr)        \
         ((void)(*(spstr) = (splitstring_t) splitstring_FREE))

/* function: addr_splitstring
 * Implements <splitstring_t.addr_splitstring>. */
#define addr_splitstring(spstr, stridx)    \
         ((spstr)->stringpart[(stridx)].addr)

/* function: size_splitstring
 * Implements <splitstring_t.size_splitstring>. */
#define size_splitstring(spstr, stridx)    \
         ((spstr)->stringpart[(stridx)].size)

/* function: nrofparts_splitstring
 * Implements <splitstring_t.nrofparts_splitstring>. */
#define nrofparts_splitstring(spstr)   \
         ((uint8_t)((spstr)->nrofparts))

/* function: setnrofparts_splitstring
 * Implements <splitstring_t.setnrofparts_splitstring>. */
#define setnrofparts_splitstring(spstr, number_of_parts)    \
         ((void)((spstr)->nrofparts = number_of_parts))

/* function: setstring_splitstring
 * Implements <splitstring_t.setstring_splitstring>. */
#define setstring_splitstring(spstr, stridx, stringsize, stringaddr)  \
         ((void)((spstr)->stringpart[(stridx)] = (string_t) string_INIT(stringsize, stringaddr)))

/* function: setsize_splitstring
 * Implements <splitstring_t.setfirstsize_splitstring>. */
#define setsize_splitstring(spstr, stridx, stringsize) \
         ((void)((spstr)->stringpart[(stridx)].size = stringsize))

#endif
