/* title: BinaryStack
   Interface to a stack which supports objects of differing size.

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

   file: C-kern/api/ds/inmem/binarystack.h
    Header file <BinaryStack>.

   file: C-kern/ds/inmem/binarystack.c
    Implementation file <BinaryStack impl>.
*/
#ifndef CKERN_DS_INMEM_BINARYSTACK_HEADER
#define CKERN_DS_INMEM_BINARYSTACK_HEADER

#include "C-kern/api/memory/memblock.h"

/* typedef: struct binarystack_t
 * Exports <binarystack_t>. */
typedef struct binarystack_t           binarystack_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_binarystack
 * Test <binarystack_t> functionality. */
extern int unittest_ds_inmem_binarystack(void) ;
#endif


/* struct: binarystack_t
 * */
struct binarystack_t {
   size_t      size ;
   memblock_t  stackmem ;
} ;

// group: lifetime

/* define: binarystack_INIT_FREEABLE
 * */
#define binarystack_INIT_FREEABLE      { 0, memblock_INIT_FREEABLE }

/* function: init_binarystack
 * */
extern int init_binarystack(/*out*/binarystack_t * stack, size_t initial_size) ;

/* function: free_binarystack
 * */
extern int free_binarystack(binarystack_t * stack) ;

// group: query

extern int isempty_binarystack(const binarystack_t * stack) ;

// group: access

/* function: push_binarystack
 * Writes binary data of size bytes onto stack and grows stack.
 * The top of stack is moved *size* bytes to make room for the new data
 * and then the *data* is copied to the newly-created memory.
 * The pointer *data* must point to a memory location of at least *size* bytes size. */
extern int push_binarystack(binarystack_t * stack, size_t size, const void * data/*[size]*/) ;

/* function: pop_binarystackř
 * Read size bytes of data from top of stack and shrinks it.
 * The top *size* bytes of the stack are copied to *data*. Then the stack is shrinked
 * by that size. If parameter *data* is 0 no data is copied but only skrining is done.  */
extern int pop_binarystack(binarystack_t * stack, size_t size, void * data/*[size]*/) ;


// section: inline implementation

/* define: isempty_binarystack
 * Implements <binarystack_t.isempty_binarystack>. */
#define isempty_binarystack(stack)     (0 == (stack)->size)

#endif
