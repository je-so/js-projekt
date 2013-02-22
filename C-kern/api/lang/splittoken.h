/* title: SplitToken

   Defines a token which can store two string attributes.
   Used for scanning two noncontiguous buffers.

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

   file: C-kern/api/lang/splittoken.h
    Header file <SplitToken>.

   file: C-kern/lang/splittoken.c
    Implementation file <SplitToken impl>.
*/
#ifndef CKERN_LANG_SPLITTOKEN_HEADER
#define CKERN_LANG_SPLITTOKEN_HEADER

#include "C-kern/api/string/string.h"

/* typedef: struct splittoken_t
 * Export <splittoken_t> into global namespace. */
typedef struct splittoken_t            splittoken_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_lang_splittoken
 * Test <splittoken_t> functionality. */
int unittest_lang_splittoken(void) ;
#endif


/* struct: splittoken_t
 * Stores a description of a tokenized (scanned) lexem.
 *
 * The token consists of a type and subtype id and a string attribute
 * which stores a pointer to the lexem value and its size.
 *
 * In case the scanner supports more than a single buffer the sstring can cross buffer boundaries.
 * Therefore an array of strings is managed. The first string at index 0 stores
 * the start of the lexem and the string at index 1 (last entry) stores the tail of the lexem. */
struct splittoken_t {
   /* variable: tokentype
    * The type id or class or category of the token. */
   uint16_t    tokentype ;
   /* variable: tokensubtype
    * The subtype id or subclass or category of the token. */
   uint8_t     tokensubtype ;
   /* variable: nrofstrings
    * The number of noncontinuous strings the lexem is composed of.
    * The maximum supported value is 2. See also array <stringpart>. */
   uint8_t     nrofstrings ;
   /* variable: stringpart
    * The array of noncontinuous strings the lexem is composed of.
    * The maximum number of strings is 2. */
   string_t    stringpart[2] ;
} ;

// group: lifetime

/* define: splittoken_INIT_FREEABLE
 * Static initializer. */
#define splittoken_INIT_FREEABLE       { 0, 0, 0, {{0, 0}, {0, 0}} }

/* function: free_splittoken
 * Sets all data members to 0. */
void free_splittoken(splittoken_t * sptok) ;

// group: query

/* function: isfree_splittoken
 * Returns *true* if sptok equals <splittoken_INIT_FREEABLE>. */
bool isfree_splittoken(const splittoken_t * sptok) ;

/* function: type_splittoken
 * Returns the type id (class or category) of the token. */
uint16_t type_splittoken(const splittoken_t * sptok) ;

/* function: subtype_splittoken
 * Returns the subtype id (subclass or subcategory) of the token. */
uint16_t subtype_splittoken(const splittoken_t * sptok) ;

/* function: nrofstrings_splittoken
 * Returns the nr of strings the lexem is composed of. */
uint8_t nrofstrings_splittoken(const splittoken_t * sptok) ;

/* function: stringaddr_splittoken
 * Returns a pointer to start of the string attribute.
 * The valid range for stridx is [0 .. <nrofstrings_splittoken>-1].
 * The string is not '\0' terminated. Use <sizepart_splittoken> to determine its size. */
const uint8_t * stringaddr_splittoken(const splittoken_t * sptok, uint8_t stridx) ;

/* function: stringsize_splittoken
 * Returns the size of the string.
 * The valid range for stridx is [0 .. <nrofstrings_splittoken>-1]. */
size_t stringsize_splittoken(const splittoken_t * sptok, uint8_t stridx) ;

// group: setter

/* function: settype_splittoken
 * Sets the type and subtype of the token. */
void settype_splittoken(splittoken_t * sptok, uint16_t type, uint16_t subtype) ;

/* function: setnrofstrings_splittoken
 * Sets the nr of strings the lexem is composed of. */
void setnrofstrings_splittoken(splittoken_t * sptok, uint8_t number_of_strings) ;

/* function: setstringaddr_splittoken
 * Sets the start addr of the string describing the lexem.
 * The value stridx determines which part of the string is set -- only two parts are supported.
 * For the first part set stridx to 0 for the last part set it to 1.
 * Use <setstringsize_splittoken> to set the size of the string. */
void setstringaddr_splittoken(splittoken_t * sptok, uint8_t stridx, const uint8_t * stringaddr) ;

/* function: setstringsize_splittoken
 * Sets the size of the string describing the lexem.
 * See <setstringaddr_splittoken>. */
void setstringsize_splittoken(splittoken_t * sptok, uint8_t stridx, size_t stringsize) ;


// section: inline implementation

/* function: free_splittoken
 * Implements <splittoken_t.free_splittoken>. */
#define free_splittoken(sptok)         \
         ((void)(*(sptok) = (splittoken_t) splittoken_INIT_FREEABLE))

/* function: stringaddr_splittoken
 * Implements <splittoken_t.stringaddr_splittoken>. */
#define stringaddr_splittoken(sptok, stridx)     \
         ((sptok)->stringpart[(stridx)].addr)

/* function: stringsize_splittoken
 * Implements <splittoken_t.stringsize_splittoken>. */
#define stringsize_splittoken(sptok, stridx)     \
         ((sptok)->stringpart[(stridx)].size)

/* function: nrofstrings_splittoken
 * Implements <splittoken_t.nrofstrings_splittoken>. */
#define nrofstrings_splittoken(sptok)    \
         ((sptok)->nrofstrings)

/* function: setnrofstrings_splittoken
 * Implements <splittoken_t.setnrofstrings_splittoken>. */
#define setnrofstrings_splittoken(sptok, number_of_strings) \
         ((void)((sptok)->nrofstrings = number_of_strings))

/* function: setstringaddr_splittoken
 * Implements <splittoken_t.setstringaddr_splittoken>. */
#define setstringaddr_splittoken(sptok, stridx, stringaddr)  \
         ((void)((sptok)->stringpart[(stridx)].addr = stringaddr))

/* function: setstringsize_splittoken
 * Implements <splittoken_t.setstringsize_splittoken>. */
#define setstringsize_splittoken(sptok, stridx, stringsize)  \
         ((void)((sptok)->stringpart[(stridx)].size = stringsize))

/* function: settype_splittoken
 * Implements <splittoken_t.settype_splittoken>. */
#define settype_splittoken(sptok, type, subtype)         \
         do {                                            \
               typeof(sptok) _sptok = sptok ;            \
               _sptok->tokentype    = type ;             \
               _sptok->tokensubtype = subtype ;          \
         } while (0)

/* function: subtype_splittoken
 * Implements <splittoken_t.subtype_splittoken>. */
#define subtype_splittoken(sptok)      \
         ((sptok)->tokensubtype)

/* function: type_splittoken
 * Implements <splittoken_t.type_splittoken>. */
#define type_splittoken(sptok)         \
         ((sptok)->tokentype)

#endif
