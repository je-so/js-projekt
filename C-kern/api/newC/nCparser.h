/* title: NewC-Parser

   TODO: describe module interface

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
   (C) 2014 Jörg Seebohn

   file: C-kern/api/newC/nCparser.h
    Header file <NewC-Parser>.

   file: C-kern/newC/nCparser.c
    Implementation file <NewC-Parser impl>.
*/
#ifndef CKERN_NEWC_NCPARSER_HEADER
#define CKERN_NEWC_NCPARSER_HEADER

/* typedef: struct nCparser_t
 * Export <nCparser_t> into global namespace. */
typedef struct nCparser_t nCparser_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_newc_ncparser
 * Test <nCparser_t> functionality. */
int unittest_newc_ncparser(void);
#endif


/* struct: nCparser_t
 * TODO: describe type */
struct nCparser_t {
   int dummy; // TODO: remove line
};

// group: lifetime

/* define: nCparser_FREE
 * Static initializer. */
#define nCparser_FREE \
         { 0 }

/* function: init_ncparser
 * TODO: Describe Initializes object. */
int init_ncparser(/*out*/nCparser_t * obj);

/* function: free_ncparser
 * TODO: Describe Frees all associated resources. */
int free_ncparser(nCparser_t * obj);

// group: query

// group: update



// section: inline implementation

/* define: init_ncparser
 * Implements <nCparser_t.init_ncparser>. */
#define init_ncparser(obj) \
         // TODO: implement


#endif
