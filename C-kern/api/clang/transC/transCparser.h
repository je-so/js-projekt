/* title: TransC-Parser

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

   file: C-kern/api/clang/transC/transCparser.h
    Header file <TransC-Parser>.

   file: C-kern/clang/transC/transCparser.c
    Implementation file <TransC-Parser impl>.
*/
#ifndef CKERN_CLANG_TRANSC_TRANSCPARSER_HEADER
#define CKERN_CLANG_TRANSC_TRANSCPARSER_HEADER

/* typedef: struct transCparser_t
 * Export <transCparser_t> into global namespace. */
typedef struct transCparser_t          transCparser_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_lang_transc_transCparser
 * Test <transCparser_t> functionality. */
int unittest_lang_transc_transCparser(void) ;
#endif


/* struct: transCparser_t
 * TODO: describe type
 *
 * Data Flow:
 * >  ─────────────        ┌──────────────┐       ┌────────────────┐
 * >   Source File  ────➜  │ transCparser │ ────➜ │ Structured     │
 * >  ─────────────        └──────────────┘       │ Representation │
 * >      │                                       └────────↑───────┘
 * >      ╰────────────────────────────────────────────────╯
 * >           ( source code positions are stored in memory
 * >             to support error reporting / interpretation )
 *
 * */
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
