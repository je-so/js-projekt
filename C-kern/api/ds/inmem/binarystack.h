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

/* typedef: struct binarystack_t
 * Export <binarystack_t> into global namespace. */
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
 *
 * This implementation tries to avoid any additional memory copy operations.
 * Therefore the caller gets a pointer to the last pushed object as result.
 * This pointer is considered valid as long the object is not popped off the stack.
 *
 * Therefore calling additonal <push_binarystack> and corresponding <pop_binarystack> keeps
 * this pointer valid. Calling <free_binarystack> does invalidate the pointer.
 *
 * Operations:
 * Push - You can push size bytes to the top of stack. This operation only allocates memory
 *        and returns a pointer to it. You need to initialize the content of the
 *        pushed object after it has been pushed.
 * Pop  - Removes size bytes from top of stack.
 *        To access the new top of stack (start address of a previously pushed object)
 *        call <top_binarystack>.
 *
 * Memory Alignment:
 * If you push an object of size X the address of the next pushed object is aligned to size X.
 * So if you need to align memory to number X make sure that every push operation pushes only objects
 * with sizes which are multiples of X. The first pushed object is page aligned (see <pagesize_vm>).
 *
 * TODO: replace <init_vmblock>, <free_vmblock> with cached vmblock implementation.
 * */
struct binarystack_t {
   /* variable: freeblocksize
    * The number of free bytes in the memory block <blockstart> points to. */
   uint32_t    freeblocksize ;
   /* variable: blocksize
    * Size of allocated memory block. */
   uint32_t    blocksize ;
   /* variable: blockstart
    * Start Address of latest allocated memory block.
    * The size of this block is stored in <blocksize>. */
   uint8_t     * blockstart ;
   /* variable: blockcache
    * Address of freed memory block else NULL.
    * Before the block is returned to the virtual memory subsystem it is cached in this value. */
   void        * blockcache ;
} ;

// group: lifetime

/* define: binarystack_INIT_FREEABLE
 * Static initializer. */
#define binarystack_INIT_FREEABLE      { 0, 0, 0, 0 }

/* function: init_binarystack
 * Initializes stack object and reserves at least preallocate_size bytes. */
int init_binarystack(/*out*/binarystack_t * stack, uint32_t preallocate_size) ;

/* function: free_binarystack
 * Frees memory resources held by stack. All pointers into the stack become invalid. */
int free_binarystack(binarystack_t * stack) ;

// group: query

/* function: isempty_binarystack
 * Returns true if stack contains no more data. */
int isempty_binarystack(const binarystack_t * stack) ;

/* function: size_binarystack
 * Returns the size of all pushed objects.
 * The runtime of the function depends on the number of allocated block.
 * So do not use this function in a inner loop. Instead use your own counter. */
size_t size_binarystack(binarystack_t * stack) ;

/* function: top_binarystack
 * Returns the start address in memory of the last pushed object.
 * The returned address is the lowest address of the allocated memory space.
 *
 * The returned pointer is never NULL. Even if the stack is empty.
 * So always check with <isempty_binarystack> if there is an element on the stack.
 *
 * Use <at_binarystack> the get the address of an object who is not top on stack. */
void * top_binarystack(binarystack_t * stack) ;

// group: change

/* function: pop_binarystack
 * Removes the last pushed object from the stack.
 * The parameter size must be set to the size of the last pushed object to remove it from the stack.
 * If it is set to the sum of x last pushed objects then these x objects are removed from the stack at once.
 * Call <top_binarystack> to get the new address of the object on top of the stack.
 *
 * Popping only part of the last pushed object shrinks the memory of it.
 * Use <top_binarystack> to get the new address of a partially shrinked object.
 * If size is bigger than <size_binarystack> the the error EINVAL is returned and nothing is done.
 * This function calls <pop2_binarystack> if one or more memory blocks have to be freed. */
int pop_binarystack(binarystack_t * stack, size_t size) ;

/* function: push_binarystack
 * Allocates memory for new object and returns pointer to its start address.
 * The returned address is the lowest address of the allocated memory space.
 * This function is implemented as a generic macro and the size
 * of the newly pushed object is calculated as
 * > size = sizeof(**lastpushed) ;
 *
 * If not enough preallocated memory is available a call to <push2_binarystack> is made
 * which allocates memory. In case of an error ENOMEM is returned.
 *
 * The memory is managed as a list of allocated blocks so the address of already
 * pushed objects never changes.
 *
 * Parameter:
 * stack      - binary stack object.
 * lastpushed - A pointer to pointer of a concrete type whose newly allocated address is returned.
 *              The allocated memory space contains random data.
 *              The size of the object is determined from its type (see above).
 * */
int push_binarystack(binarystack_t * stack, /*out*/void ** lastpushed) ;

/* function: pop2_binarystack
 * Same functionality as <pop_binarystack>.
 * This function is called from <pop_binarystack> in case one or more memory blocks must be freed.
 *
 * Attention:
 * If the memory subsystem returns an error this function returns this error.
 * But it continues the work until size bytes are popped off the stack.
 * The reason is that the error can occur any time after some blocks have already been freed.
 * So doing nothing in case of an error does not work here. */
int pop2_binarystack(binarystack_t * stack, size_t size) ;

/* function: push2_binarystack
 * Same functionality as <push_binarystack>.
 * This function is called from <push_binarystack> in case one new memory must be allocated. */
int push2_binarystack(binarystack_t * stack, uint32_t size, /*out*/uint8_t ** lastpushed) ;


// section: inline implementation

/* define: isempty_binarystack
 * Implements <binarystack_t.isempty_binarystack>. */
#define isempty_binarystack(stack)                    ((stack)->freeblocksize == (stack)->blocksize)

/* define: pop_binarystack
 * Implements <binarystack_t.pop_binarystack>. */
#define pop_binarystack(stack, size)                                 \
   ( __extension__ ({                                                \
      int _err ;                                                     \
      binarystack_t * _stack = (stack) ;                             \
      uint32_t      _size    = (size) ;                              \
      if (_stack->blocksize - _stack->freeblocksize > _size) {       \
         _stack->freeblocksize += _size ;                            \
         _err = 0 ;                                                  \
      } else {                                                       \
         _err = pop2_binarystack(_stack, _size) ;                    \
      }                                                              \
      _err ;                                                         \
   }))

/* define: push_binarystack
 * Implements <binarystack_t.push_binarystack>. */
#define push_binarystack(stack, lastpushed)                          \
   ( __extension__ ({                                                \
      int _err ;                                                     \
      binarystack_t * _stack = (stack) ;                             \
      static_assert(sizeof(**(lastpushed)) < UINT32_MAX, "cast") ;   \
      const uint32_t  _size  = (uint32_t)sizeof(**(lastpushed)) ;    \
      if (_stack->freeblocksize >= _size) {                          \
         _stack->freeblocksize -= _size ;                            \
         *(lastpushed) = (void*) &_stack->blockstart[                \
                                          _stack->freeblocksize] ;   \
         _err = 0 ;                                                  \
      } else {                                                       \
         _err = push2_binarystack(_stack, _size,                     \
                                  (uint8_t**)(lastpushed)) ;         \
      }                                                              \
      _err ;                                                         \
   }))

/* define: top_binarystack
 * Implements <binarystack_t.top_binarystack>. */
#define top_binarystack(stack)                        ((void*)(&(stack)->blockstart[(stack)->freeblocksize]))

#endif
