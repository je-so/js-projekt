/* title: WBuffer
   Implements a simple write buffer.

   Used by every function which needs to return strings
   or other information of unknown size.

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

   file: C-kern/api/memory/wbuffer.h
    Header file <WBuffer>.

   file: C-kern/memory/wbuffer.c
    Implementation file <WBuffer impl>.
*/
#ifndef CKERN_MEMORY_WBUFFER_HEADER
#define CKERN_MEMORY_WBUFFER_HEADER

// forward
struct memblock_t ;

/* typedef: struct wbuffer_t
 * Exports <wbuffer_t>. */
typedef struct wbuffer_t               wbuffer_t ;

/* typedef: wbuffer_it
 * Exports <wbuffer_it>.*/
typedef struct wbuffer_it              wbuffer_it ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_wbuffer
 * Test object <wbuffer_t>. */
int unittest_memory_wbuffer(void) ;
#endif


/* struct: wbuffer_t
 * Wraps pointer to dynamically growing memory.
 * Supports construction of return values of unknown size a priori.
 *
 * It is also possible to wrap a static buffer into a <wbuffer_t> type.
 * In the static case ENOMEM is returned if there is no more buffer space
 * available.
 *
 * In the dynamic case the memory is grown automatically.
 * A pointer to internal memory acquired for example by a call to <appendalloc_wbuffer>
 * is therefore only valid as long as no other append operation is called.
 *
 * TODO: Implement block store, a simple chain of memory blocks.
 *       Provide adaption interface to wbuffer_t.
 * TODO: Use block store as default implementation.
 *       No need for memory copy in case of resize only adding of new block !
 *       Read must always use block iterator for reading !
 * TODO: Allow implementation to provide a static allocated block as first
 *       block in the list. Allocate additional buffer if necessary.
 *       The first static allocated memory blocked is not freed.
 * TODO: provide wbuffer_adapt2cstring for adaption to cstring_t
 *
 * */
struct wbuffer_t {
   /* variable: free_size
    * Size of free memory pointer <free> points to. */
   size_t            free_size ;
   /* variable: free
    * Points to free memory.
    * The size of the free memory is storead in <free_size>. */
   uint8_t           * free ;
   /* variable: addr
    * Points to start byte of allocated memory. */
   uint8_t           * addr ;
   const wbuffer_it  * interface ;
} ;

// group: lifetime

/* define: wbuffer_INIT
 * Static initializer for dynamic growing buffer with default memory policy. */
#define wbuffer_INIT                   { 0, 0, 0, (void*)1 }

/* define: wbuffer_INIT
 * Static initializer for freed (invalid) buffer. */
#define wbuffer_INIT_FREEABLE          { 0, 0, 0, 0 }

/* define: wbuffer_INIT_STATIC
 * Static initializer to wrap static memory into a <wbuffer_t>.
 *
 * Parameter:
 * buffer_size   - Size in bytes of static memory.
 * static_buffer - Memory address (uint8_t*) of static memory. */
#define wbuffer_INIT_STATIC(buffer_size, static_buffer) \
      { buffer_size, (static_buffer), (static_buffer), 0 }

/* define: wbuffer_INIT_SUBTYPE
 * Static initializer to subtype <wbuffer_t> with own memory management.
 *
 * Parameter:
 * buffer_size     - Size in bytes of preallocated memory.
 * prealloc_buffer - Memory address (uint8_t*) of preallocated memory.
 * subtype_it      - Interface pointer (<wbuffer_it>*) which overwrites the memory management functions. */
#define wbuffer_INIT_SUBTYPE(buffer_size, prealloc_buffer, subtype_it) \
      { buffer_size, (prealloc_buffer), (prealloc_buffer), subtype_it }

/* function: init_wbuffer
 * Inits <wbuffer_t> with a preallocated memory of preallocate_size bytes. */
int init_wbuffer(wbuffer_t * wbuf, size_t preallocate_size) ;

/* function: free_wbuffer
 * Frees all memory associated with wbuf object of type <wbuffer_t>.
 * In case of a static buffer*/
int free_wbuffer(wbuffer_t * wbuf) ;

// group: change

/* function: appendalloc_wbuffer
 * Allocates and appends buffer of buffer_size.
 * The pointer to the buffer is returned in buffer. */
int appendalloc_wbuffer(wbuffer_t * wbuf, size_t buffer_size, uint8_t ** buffer) ;

/* function: appendchar_wbuffer
 * Appends 1 character to the buffer. */
int appendchar_wbuffer(wbuffer_t * wbuf, const char c) ;

/* function: grow_wbuffer
 * Function grows buffer. In case of error ENOMEM is returned.
 * The buffer is only grown if free memory size (<sizefree_wbuffer>) is less
 * than parameter free_size.
 *
 * After successful return the follwowing is true:
 * > (sizefree_wbuffer(wbuf) >= free_size)
 *
 * If addr is a pointer to a static buffer calling grow_buffer returns always ENOMEM. */
int grow_wbuffer(wbuffer_t * wbuf, size_t free_size) ;

/* function: popbytes_wbuffer
 * Removes the last size appended bytes from <wbuffer_t>.
 * If the buffer contains less bytes then only the contained bytes
 * are removed until the buffer is empty. */
void popbytes_wbuffer(wbuffer_t * wbuf, size_t size) ;

/* function: reset_wbuffer
 * Removes all appended content from wbuffer.
 * Only allocated memory is not freed. */
void reset_wbuffer(wbuffer_t * wbuf) ;

// group: query

/* function: sizefree_wbuffer
 * Returns the number of allocated bytes which are not in use. */
size_t sizefree_wbuffer(const wbuffer_t * wbuf) ;

/* function: sizecontent_wbuffer
 * Returns the number of bytes appended / written to the buffer. */
size_t sizecontent_wbuffer(const wbuffer_t * wbuf) ;

/* function: content_wbuffer
 * Returns address of content.
 * A value of 0 indicates no content.
 * The size of content can be queried by a call to <sizecontent_wbuffer>.
 *
 * Special case:
 * If <sizecontent_wbuffer> returns a value != 0 but <content_wbuffer> a value of 0
 * you need to access the content with <contentiterator_wbuffer>.
 * The default implementation supports only <content_wbuffer> but highly optimized
 * versions may be using an internal list of more than one preallocated memory block. */
uint8_t * content_wbuffer(const wbuffer_t * wbuf) ;

/* function: contentiterator_wbuffer
 * There is *no* need to implement this function.
 * Cause the default imeplementation supports only one linear buffer and therefore
 * calling <content_wbuffer> always works.
 *
 * Returns:
 * 0       - Parameter *content* is set to valid memory block description which contains the index-th content block.
 * ENODATA - Parameter *index* is too high and no data is returned. */
int contentiterator_wbuffer(const wbuffer_t * wbuf, size_t index, /*out*/struct memblock_t * content);

// group: helper

/* function: appendalloc2_wbuffer
 * Same as <appendalloc_wbuffer> but implemented not inline. */
int appendalloc2_wbuffer(wbuffer_t * wbuf, size_t buffer_size, uint8_t ** buffer) ;

/* function: appendchar2_wbuffer
 * Same as <appendchar_wbuffer> but implemented not inline. */
int appendchar2_wbuffer(wbuffer_t * wbuf, const char c) ;


/* struct: wbuffer_it
 * Defines interface for <wbuffer_t>.
 * This allows subtyping - for example <cstring_t> uses this to offer a <wbuffer_t> view. */
struct wbuffer_it {
   /* variable: free_wbuffer
    * Function pointer which frees buffer memory.
    * After successful return all fields in <wbuffer_t> are set to 0.
    * If addr is a pointer to a static buffer calling free_buffer returns always 0. */
   int     (* free_wbuffer) (wbuffer_t * wbuf) ;

   /* variable: grow_wbuffer
    * Function pointer which grows buffer. In case of error ENOMEM is returned.
    * The buffer is only grown if (addr_plus_size-free) is less than free_size.
    *
    * After successful return the follwowing is true:
    * > (addr_plus_size - free) >= free_size
    *
    * If addr is a pointer to a static buffer calling grow_buffer returns always ENOMEM. */
   int     (* grow_wbuffer) (wbuffer_t * wbuf, size_t free_size) ;
} ;


// section: inline implementation

/* define: appendalloc_wbuffer
 * Implements <wbuffer_t.appendalloc_wbuffer>. */
#define appendalloc_wbuffer(wbuf, buffer_size, buffer)            \
   ( __extension__ ({   int _err ;                                   \
      if (sizefree_wbuffer(wbuf) >= (size_t)(buffer_size)) {         \
         ((wbuf)->free_size) -= (size_t)(buffer_size) ;              \
         *(buffer) = ((wbuf)->free) ;                                \
         ((wbuf)->free)      += (size_t)(buffer_size) ;              \
         _err = 0 ;                                                  \
      } else {                                                       \
         _err = appendalloc2_wbuffer(wbuf, buffer_size, buffer) ;    \
      }                                                              \
      _err ; }))

/* define: appendchar_wbuffer
 * Implements <wbuffer_t.appendchar_wbuffer>. */
#define appendchar_wbuffer(wbuf, c)             \
   ( __extension__ ({   int _err ;              \
      if (sizefree_wbuffer(wbuf)) {             \
         -- ((wbuf)->free_size) ;               \
         *((wbuf)->free++) = (uint8_t)c ;       \
         _err = 0 ;                             \
      } else {                                  \
         _err = appendchar2_wbuffer(wbuf, c) ;  \
      }                                         \
      _err ; }))

/* define: content_wbuffer
 * Implements <wbuffer_t.content_wbuffer>. */
#define content_wbuffer(wbuf)          ((wbuf)->addr)

/* define: sizefree_wbuffer
 * Implements <wbuffer_t.sizefree_wbuffer>. */
#define sizefree_wbuffer(wbuf)         ((wbuf)->free_size)

/* define: sizecontent_wbuffer
 * Implements <wbuffer_t.sizecontent_wbuffer>. */
#define sizecontent_wbuffer(wbuf)         ((size_t)((wbuf)->free - (wbuf)->addr))

#endif
