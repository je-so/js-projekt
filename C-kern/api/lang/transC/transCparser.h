/* title: TransactionalC

   Offers interface to parse a transactional C source code file
   and represent its content as syntax tree.

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

   file: C-kern/api/lang/transC/transCparser.h
    Header file <TransactionalC>.

   file: C-kern/lang/transC/transCparser.c
    Implementation file <TransactionalC impl>.
*/
#ifndef CKERN_LANG_TRANSC_TRANSCPARSER_HEADER
#define CKERN_LANG_TRANSC_TRANSCPARSER_HEADER

/* typedef: struct transCparser_t
 * Export <transCparser_t> into global namespace. */
typedef struct transCparser_t          transCparser_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_lang_transc_transcparser
 * Test <transCparser_t> functionality. */
int unittest_lang_transc_transcparser(void) ;
#endif


/* struct: transCparser_t
 * TODO: describe type */
struct transCparser_t {
   int dummy ;
} ;

// group: lifetime

/* define: transCparser_INIT_FREEABLE
 * Static initializer. */
#define transCparser_INIT_FREEABLE        { 0 }

/* function: init_transcparser
 * TODO: Describe Initializes object. */
int init_transcparser(/*out*/transCparser_t * tcparser) ;

/* function: free_transcparser
 * TODO: Describe Frees all associated resources. */
int free_transcparser(transCparser_t * tcparser) ;

// group: query

// group: update


#endif
