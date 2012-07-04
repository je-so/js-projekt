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
int unittest_ds_inmem_binarystack(void) ;
#endif


/* struct: binarystack_t
 * Stores binary data in LIFO order.
 * This implementation tries to avoid any additional memory copy operations.
 * Therefore the notion of a pointer to the last pushed object is introduced.
 * This pointer is only valid as long as no other operations like push or pop
 * are called.
 *
 * Operations:
 * Push - You can push size bytes to the top of stack. This operation only allocates memory
 *        and returns a pointer to it. You need to initialize the content of the
 *        pushed object after it has been pushed.
 * Pop  - Removes size bytes from top of stack.
 *        To access any previously pushed object call <lastpushed_binarystack>
 *        with the size of the object.
 *
 * Memory Alignment:
 * If you set the size of the a pushed object to X bytes the address of the next
 * pushed object is aligned to these X bytes. So if you need to access memory of
 * X bytes alignment make sure that all object sizes are a multiple of X.
 * The first pushed object has correct alignment required by the system.
 * */
struct binarystack_t {
   size_t      size ;
   memblock_t  stackmem ;
} ;

// group: lifetime

/* define: binarystack_INIT_FREEABLE
 * Static initializer. */
#define binarystack_INIT_FREEABLE      { 0, memblock_INIT_FREEABLE }

/* function: init_binarystack
 * Initializes stack object and preallocates at least initial_size bytes. */
int init_binarystack(/*out*/binarystack_t * stack, size_t initial_size) ;

/* function: free_binarystack
 * Frees memory resources held by stack. */
int free_binarystack(binarystack_t * stack) ;

// group: query

/* function: isempty_binarystack
 * Returns true if stack contains no more data. */
int isempty_binarystack(const binarystack_t * stack) ;

/* function: size_binarystack
 * Returns number of bytes pushed on stack.
 * If this number is 0 <isempty_binarystack> returns true. */
size_t size_binarystack(const binarystack_t * stack) ;

/* function: lastpushed_binarystack
 * Returns the memory address of the last pushed binary object.
 * The returned pointer points to memory of at least *size_lastpushed* bytes.
 * If the returned pointer is 0 the stack size is smaller than *size_lastpushed*. */
void * lastpushed_binarystack(binarystack_t * stack, size_t size_lastpushed) ;

// group: operations

/* function: push_binarystack
 * Allocates a new object of *size* bytes and increments the size of the stack.
 * The pointer to newly allocated object is returned in *lastpushed*.
 * If the stack has no more preallocated memory a reallocation is done.
 * Therefore any previously returned pointers become invalid. If the reallocation
 * failes ENOMEM is returned. */
int push_binarystack(binarystack_t * stack, size_t size, /*out*/void ** lastpushed) ;

/* function: pushgeneric_binarystack
 * Wrapper macro which calls <push_binarystack> with size set to sizeof(**lastpushed).
 * The parameter *lastpushed* must be a pointer to pointer of a concrete type
 * whose newly allocated address is returned. */
int pushgeneric_binarystack(binarystack_t * stack, /*out*/void ** lastpushed) ;

/* function: pop_binarystackř
 * Removes the last *size* bytes from the stack.
 * If the size of the stack is less than *size* nothing is done and the error code EINVAL is returned. */
int pop_binarystack(binarystack_t * stack, size_t size) ;


// section: inline implementation

/* define: isempty_binarystack
 * Implements <binarystack_t.isempty_binarystack>. */
#define isempty_binarystack(stack)     (0 == size_binarystack(stack))

/* define: pushgeneric_binarystack
 * Implements <binarystack_t.pushgeneric_binarystack>. */
#define pushgeneric_binarystack(stack, lastpushed) \
                                       (push_binarystack((stack), sizeof(**(lastpushed)), (void**) (lastpushed)))

/* define: size_binarystack
 * Implements <binarystack_t.size_binarystack>. */
#define size_binarystack(stack)        ((stack)->size)


#endif
