/* title: WriteBuffer
   Implements a simple write buffer.

   Used by every function which needs to return strings
   or other information of unknown size.

   Do no forget to include "C-kern/api/ds/foreach.h" and "C-kern/api/memory/memblock.h"
   if you want to iterate the content of <wbuffer_t>.

   TODO: redefine foreach iteratedtype_wbuffer (include pointer * ) / adapt wbuffer

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

   file: C-kern/api/memory/wbuffer.h
    Header file <WriteBuffer>.

   file: C-kern/memory/wbuffer.c
    Implementation file <WriteBuffer impl>.
*/
#ifndef CKERN_MEMORY_WBUFFER_HEADER
#define CKERN_MEMORY_WBUFFER_HEADER

// forward
struct memblock_t ;

/* typedef: struct wbuffer_t
 * Exports <wbuffer_t>. */
typedef struct wbuffer_t               wbuffer_t ;

/* typedef: struct wbuffer_it
 * Exports implementation interface <wbuffer_it>.*/
typedef struct wbuffer_it              wbuffer_it ;

/* typedef: struct wbuffer_iterator_t
 * Exports wbuffer iterator <wbuffer_iterator_t>.*/
typedef struct wbuffer_iterator_t      wbuffer_iterator_t ;


/* variable: g_wbuffer_dynamic
 * The implementation of a dynamic growing <wbuffer_t>. */
extern wbuffer_it                      g_wbuffer_dynamic ;

/* variable: g_wbuffer_static
 * The implementation of a static allocated <wbuffer_t>. */
extern wbuffer_it                      g_wbuffer_static ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_wbuffer
 * Test object <wbuffer_t>. */
int unittest_memory_wbuffer(void) ;
#endif


/* struct: wbuffer_it
 * Defines implementation interface for <wbuffer_t>.
 * This allows <wbuffer_t> to adapt to different memory allocation strategies. */
struct wbuffer_it {
   /* variable: reserve
    * Function pointer which grows buffer if necessary. In case of error or a static buffer ENOMEM is returned.
    * The buffer is grown so that at least reserve_size continues bytes are reserved
    * in addtion to the already stored bytes. */
   int     (* reserve) (wbuffer_t * wbuf, size_t reserve_size) ;
   /* variable: size
    * Returns number of bytes stored in the buffer.
    * The number of allocated bytes is greater or equal than this number.
    * This return value is 0 after calling <clear_wbuffer>. */
   size_t  (* size) (const wbuffer_t * wbuf) ;
   /* variable: iterate
    * Returns the next memoryblock stored in <wbuffer_t>.
    * A return value of true indicates that the content of memblock is valid.
    * A return value of false indicates that memblock is not changed and that there is no more data.
    * The value next is also changed to point to the next data block. It remembers the state of the iterator.
    * Set *next to 0 if you want to return the first data block. */
   bool    (* iterate) (const wbuffer_t * wbuf, void ** next, /*out*/struct memblock_t * memblock) ;
   /* variable: free
    * Function pointer which frees buffer memory.
    * After successful return all fields in <wbuffer_t> are set to 0.
    * If addr is a pointer to a static buffer calling free_buffer returns always 0. */
   int     (* free) (wbuffer_t * wbuf) ;
   /* variable: clear
    * Function pointer which sets content size of wbuffer 0.
    * Memory is not necessarily deallocated. This depends on the strategy. */
   void    (* clear) (wbuffer_t * wbuf) ;
} ;


/* struct: wbuffer_iterator_t
 * Iterator for <wbuffer_t>.
 * Returns a <memblock_t> describing part of the content of <wbuffer_t>.
 * The standard implementations provided by this module store all content
 * in one memory block. */
struct wbuffer_iterator_t {
   uint8_t  * addr ;
   size_t   size ;
   void     * next ;
} ;

// group: lifetime

/* define: wbuffer_iterator_INIT_FREEABLE
 * Static initializer. */
#define wbuffer_iterator_INIT_FREEABLE    { 0, 0, 0 }

/* function: initfirst_wbufferiterator
 * Initializes an iterator for <wbuffer_t>. */
int initfirst_wbufferiterator(/*out*/wbuffer_iterator_t * iter, wbuffer_t * wbuf) ;

/* function: free_wbufferiterator
 * Frees <wbuffer_t> iterator. */
int free_wbufferiterator(wbuffer_iterator_t * iter) ;

// group: iterate

/* function: next_wbufferiterator
 * Returns all contained memory blocks.
 * The first call after <initfirst_wbufferiterator> returns the
 * first memory block if wbuf is not empty.
 *
 * Returns:
 * true  - memblock contains a pointer to the next valid <memblock_t>.
 * false - There is no next <memblock_t>. The last element was already returned or wbuf is empty. */
bool next_wbufferiterator(wbuffer_iterator_t * iter, wbuffer_t * wbuf, /*out*/struct memblock_t ** memblock) ;


/* struct: wbuffer_t
 * Wraps pointer to dynamically growing or static memory.
 * Supports construction of return values of unknown size.
 *
 * In the dynamic case the memory is grown automatically.
 *
 * In the static case ENOMEM is returned if memory is exhausted.
 * */
struct wbuffer_t {
   /* variable: next
    * Pointer to next free memory location of the reserved memory. */
   uint8_t           * next ;
   /* variable: end
    * Points to memory address after last reserved memory block.
    * end-next gives the size of the reserved memory. */
   uint8_t           * end ;
   /* variable: addr
    * Points to first byte (lowest address) of allocated memory.
    * This may be redefined in your own memory allocation strategy. */
   void              * addr ;
   /* variable: iimpl
    * Pointer to interface implementing memeory allocation strategy. */
   const wbuffer_it  * iimpl ;
} ;

// group: lifetime

/* define: wbuffer_INIT_DYNAMIC
 * Static initializer for dynamic growing buffer with default memory policy. */
#define wbuffer_INIT_DYNAMIC                       \
         { 0, 0, 0, &g_wbuffer_dynamic }

/* define: wbuffer_INIT_STATIC
 * Static initializer to wrap static memory into a <wbuffer_t>.
 * Reserving memory beyond the size of static memory always results in ENOMEM.
 *
 * Parameter:
 * buffer_size   - Size in bytes of static memory.
 * static_buffer - Memory address of static memory. */
#define wbuffer_INIT_STATIC(buffer_size, buffer)   \
         { (buffer), (buffer) + (buffer_size), (buffer), &g_wbuffer_static }

/* define: wbuffer_INIT_IMPL
 * Static initializer to adapt <wbuffer_t> ot own memory allocation strategy.
 *
 * Parameter:
 * buffer_size  - Size in bytes of already reserved memory.
 * buffer       - Start address of already reserved memory.
 * addr         - Pointer to data used by allocation strategy.
 * iimpl_it     - Pointer to interface of type <wbuffer_it> which implements memory allocation strategy. */
#define wbuffer_INIT_IMPL(buffer_size, buffer, addr, iimpl_it)  \
         { (buffer), (buffer) + (buffer_size), (addr), (iimpl_it) }

/* define: wbuffer_INIT_FREEABLE
 * Static initializer for freed (invalid) buffer. */
#define wbuffer_INIT_FREEABLE                      \
         { 0, 0, 0, 0 }

/* function: init_wbuffer
 * Inits <wbuffer_t> with a preallocated memory of preallocate_size bytes. */
int init_wbuffer(/*out*/wbuffer_t * wbuf, size_t preallocate_size) ;

/* function: free_wbuffer
 * Frees all memory associated with wbuf object of type <wbuffer_t>.
 * In case of a static buffer*/
int free_wbuffer(wbuffer_t * wbuf) ;

// group: query

/* function: isreserved_wbuffer
 * Returns true if at least one byte is reserved. */
bool isreserved_wbuffer(const wbuffer_t * wbuf) ;

/* function: reservedsize_wbuffer
 * Returns the number of allocated bytes which are not in use. */
size_t reservedsize_wbuffer(const wbuffer_t * wbuf) ;

/* function: size_wbuffer
 * Returns the number of all bytes appended / written to the buffer. */
size_t size_wbuffer(const wbuffer_t * wbuf) ;

/* function: firstmemblock_wbuffer
 * Returns the first appended data block. The size of the block could be less
 * than <size_wbuffer>. Use <foreach> to iterate over all contained data blocks.
 * A return value of 0 indicates a non empty wbuf and datablock is set to a valid value.
 * A return value of ENODATA indicates an empty wbuf and datablock is not changed. */
int firstmemblock_wbuffer(const wbuffer_t * wbuf, /*out*/struct memblock_t * datablock) ;

// group: foreach-support

/* typedef: iteratortype_wbuffer
 * Declaration to associate <wbuffer_iterator_t> with <wbuffer_t>. */
typedef wbuffer_iterator_t    iteratortype_wbuffer ;

/* typedef: iteratedtype_wbuffer
 * Declaration to associate <memblock_t> with <wbuffer_t>. */
typedef struct memblock_t     iteratedtype_wbuffer ;

// group: change

/* function: appendbytes_wbuffer
 * Reserves buffer_size bytes and appends them uninitialized.
 * A pointer to the uninitialized memory block is returned in buffer.
 * If you have called <reserve_wbuffer> before with reserve_size set to a value
 * greater or equal to buffer_size the returned pointer in buffer is the same.
 * The returned buffer pointer is valid as long as no other function is called on wbuf. */
int appendbytes_wbuffer(wbuffer_t * wbuf, size_t buffer_size, uint8_t ** buffer) ;

/* function: appendbyte_wbuffer
 * Appends 1 byte to the buffer. */
int appendbyte_wbuffer(wbuffer_t * wbuf, const uint8_t c) ;

/* function: reserve_wbuffer
 * Reserves reserve_size bytes. In case of error ENOMEM is returned.
 * The buffer is only grown if <reservedsize_wbuffer> returns a value
 * less than parameter reserve_size. A pointer to the uninitialized
 * reserved memory block is returned. If you want to append the data after
 * having it initialized call <appendbytes_wbuffer> with buffer_size set to reserve_size.
 * The returned buffer pointer is valid as long as no other function is called on wbuf.
 * The following holds after successful return:
 * > reservedsize_wbuffer(wbuf) >= reserve_size  */
int reserve_wbuffer(wbuffer_t * wbuf, size_t reserve_size, uint8_t ** buffer) ;

/* function: clear_wbuffer
 * Removes all appended content from wbuffer.
 * The memory is not necessarily freed but it is marked as free.
 * Use this function if an error has occurred but the out parameter of type <wbuffer_t>
 * has been filled with content partially. */
void clear_wbuffer(wbuffer_t * wbuf) ;


// section: inline implementation

// group: wbuffer_t

/* define: appendbyte_wbuffer
 * Implements <wbuffer_t.appendbyte_wbuffer>. */
#define appendbyte_wbuffer(wbuf, c)                      \
         ( __extension__ ({                              \
            int         _err = 0 ;                       \
            wbuffer_t * _wb  = (wbuf) ;                  \
            if (  isreserved_wbuffer(_wb)                \
                  || (0 == (_err = _wb->iimpl->          \
                      reserve(_wb, 1)))) {               \
               *(_wb->next++) = c ;                      \
            }                                            \
            _err ;                                       \
         }))

/* define: appendbytes_wbuffer
 * Implements <wbuffer_t.appendbytes_wbuffer>. */
#define appendbytes_wbuffer(wbuf, buffer_size, buffer)   \
         ( __extension__ ({                              \
            int         _err = 0 ;                       \
            wbuffer_t * _wb = (wbuf) ;                   \
            size_t      _bs = (buffer_size) ;            \
            if (  _bs <= reservedsize_wbuffer(_wb)       \
                  || (0 == (_err = _wb->iimpl->          \
                      reserve(_wb, _bs)))) {             \
               *(buffer)  = _wb->next ;                  \
               _wb->next += _bs ;                        \
            }                                            \
            _err ;                                       \
         }))

/* define: clear_wbuffer
 * Implements <wbuffer_t.clear_wbuffer>. */
#define clear_wbuffer(wbuf)                              \
         ( __extension__ ({                              \
            wbuffer_t  * _wb = (wbuf) ;                  \
            _wb->iimpl->clear(_wb) ;                     \
         }))

/* define: firstmemblock_wbuffer
 * Implements <wbuffer_t.firstmemblock_wbuffer>. */
#define firstmemblock_wbuffer(wbuf, datablock)           \
         ( __extension__ ({                              \
            wbuffer_t  * _wb = (wbuf) ;                  \
            void       * _n  = 0 ;                       \
            (_wb->iimpl->iterate(_wb, &_n, datablock)    \
            ? 0                                          \
            : ENODATA) ;                                 \
         }))

/* define: free_wbuffer
 * Implements <wbuffer_t.free_wbuffer>. */
#define free_wbuffer(wbuf)                               \
         ( __extension__ ({                              \
            wbuffer_t  * _wb = (wbuf) ;                  \
            (  _wb->iimpl                                \
               ? _wb->iimpl->free(_wb)                   \
               : 0                                       \
            ) ;                                          \
         }))

/* define: isreserved_wbuffer
 * Implements <wbuffer_t.isreserved_wbuffer>. */
#define isreserved_wbuffer(wbuf)                         \
         ( __extension__ ({                              \
            const wbuffer_t  * _wb2 = (wbuf) ;           \
            (_wb2->next < _wb2->end) ;                   \
         }))

/* define: reserve_wbuffer
 * Implements <wbuffer_t.reserve_wbuffer>. */
#define reserve_wbuffer(wbuf, reserve_size, buffer)      \
         ( __extension__ ({                              \
            int         _err = 0 ;                       \
            size_t      _rs  = (reserve_size) ;          \
            wbuffer_t  * _wb = (wbuf) ;                  \
            if (  _rs <= reservedsize_wbuffer(_wb)       \
                  || (0 == (_err = _wb->iimpl->          \
                      reserve(_wb, _rs)))) {             \
               (*buffer) = _wb->next ;                   \
            }                                            \
            _err ;                                       \
         }))

/* define: reservedsize_wbuffer
 * Implements <wbuffer_t.reservedsize_wbuffer>. */
#define reservedsize_wbuffer(wbuf)                       \
         ( __extension__ ({                              \
            const wbuffer_t * _wb2 = (wbuf) ;            \
            (size_t) (_wb2->end - _wb2->next) ;          \
         }))

/* define: size_wbuffer
 * Implements <wbuffer_t.size_wbuffer>. */
#define size_wbuffer(wbuf)                               \
         ( __extension__ ({                              \
            const wbuffer_t  * _wb = (wbuf) ;            \
            _wb->iimpl->size(_wb) ;                      \
         }))

// group: wbuffer_iterator_t

/* define: free_wbufferiterator
 * Implements <wbuffer_iterator_t.free_wbufferiterator>. */
#define free_wbufferiterator(iter)                    \
         (*(iter) = (wbuffer_iterator_t)wbuffer_iterator_INIT_FREEABLE, 0)

/* define: initfirst_wbufferiterator
 * Implements <wbuffer_iterator_t.initfirst_wbufferiterator>. */
#define initfirst_wbufferiterator(iter, wbuf)         \
         (*(iter) = (wbuffer_iterator_t)wbuffer_iterator_INIT_FREEABLE, 0)

/* define: next_wbufferiterator
 * Implements <wbuffer_iterator_t.next_wbufferiterator>. */
#define next_wbufferiterator(iter, wbuf, memblock)    \
         ( __extension__ ({                           \
            wbuffer_iterator_t * _it = (iter) ;       \
            wbuffer_t  * _wb = (wbuf) ;               \
            memblock_t * _mb =                        \
               genericcast_memblock(_it, ) ;          \
   /* TODO: remove line, change after foreach is adapted !*/ \
            *(memblock) = _mb ;                       \
            (_wb->iimpl->iterate(_wb,                 \
                                 &_it->next, _mb)) ;  \
         }))

#endif
