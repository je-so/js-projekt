/* title: MemoryBlock
   Declares structure describing a block of memory.

   Memory management is not offered by this type.


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

   file: C-kern/api/memory/memblock.h
    Header file of <MemoryBlock>.

   file: C-kern/memory/memblock.c
    Implementation file <MemoryBlock impl>.
*/
#ifndef CKERN_MEMORY_MEMORYBLOCK_HEADER
#define CKERN_MEMORY_MEMORYBLOCK_HEADER

/* typedef: struct memblock_t
 * Export <memblock_t>. */
typedef struct memblock_t              memblock_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
int unittest_memory_memblock(void) ;
#endif


/* struct: memblock_t
 * Describes memory block. This object does not offers its own memory
 * allocation / deallocation functionality. Other modules like memory
 * managers offer a such functionality and return allocated memory with
 * help of <meblock_t>. */
struct memblock_t {
   /* variable: addr
    * Points to start (lowest) address of memory.
    * The value (0) indicates an invalid memory block (freed state). */
   uint8_t *   addr ;
   /* variable: size
    * Size of memory in bytes <addr> points to.
    * The valid memory region is
    * > addr[ 0 .. size - 1 ] */
   size_t      size ;
} ;

// group: lifetime

/* define: memblock_INIT_FREEABLE
 * Static initializer. */
#define memblock_INIT_FREEABLE         { 0, 0 }

/* define: memblock_INIT
 * Static initializer.
 *
 * Parameters:
 * size  - Size of memory block in bytes (size_t).
 * addr  - Start address of memory block (uint8_t*). */
#define memblock_INIT(size, addr)      { (addr), (size) }

// group: query

/* function: isfree_memblock
 * Returns true if mblock equals <memblock_INIT_FREEABLE>. */
bool isfree_memblock(const memblock_t * mblock) ;

/* function: isvalid_memblock
 * Returns true if mblock->addr != 0 and block->size != 0.
 * This functions returns true for a <memblock_t> which is initialized to <memblock_INIT_FREEABLE>. */
bool isvalid_memblock(const memblock_t * mblock) ;

/* function: addr_memblock
 * Returns start (lowest) address of memory block.
 * in case of the value null the block is in a freed state. */
uint8_t * addr_memblock(const memblock_t * mblock) ;

/* function: size_memblock
 * Returns size of memory block. The size can also be 0. */
size_t size_memblock(const memblock_t * mblock) ;

// group: fill

/* function: clear_memblock
 * Sets the content of the memory block to 0. */
void clear_memblock(memblock_t * mblock) ;

// group: resize

/* function: shrinkleft_memblock
 * Shrinks the memory block by incr its addr and decr its size.
 * The start address <memblock_t.addr> is incremented by *addr_increment* and
 * the size of the block <memblock_t.size> is decremented by the same amount.
 * > ╭───────────────╮        ╭┈┈┈┈┬──────────╮
 * > │<--- size ---->│    ==> │    │<- size'->│ addr' == addr + addr_increment
 * > ├───────────────┤        ╰┈┈┈┈├──────────┤ size' == size - addr_increment
 * > └- addr         └- addr+size  └- addr'   └- addr+size == addr'+size'
 * */
int shrinkleft_memblock(memblock_t * mblock, size_t addr_increment) ;

/* function: shrinkright_memblock
 * Shrinks the memory block by decrementing only its size.
 * The start address <memblock_t.addr> is not changed but
 * the size of the block <memblock_t.size> is decremented by size_decrement.
 * > ╭───────────────╮            ╭─────────┬┈┈┈┈┈╮
 * > │<--- size ---->│    ==>     │<-size'->│<-D->│ D     == size_decrement
 * > ├───────────────┤            ╰─────────├┈┈┈┈┈┤ size' == size - D
 * > └- addr         └- addr+size └- addr   └- addr+size'
 * */
int shrinkright_memblock(memblock_t * mblock, size_t size_decrement) ;

/* function: growleft_memblock
 * Grows the memory block by decr its addr and incr its size..
 * The start address <memblock_t.addr> is decremented by *addr_decrement* and
 * the size of the block <memblock_t.size> is incremented by the same amount.
 * > ╭────────────╮            ╭┈┈┈┈┈──────────╮
 * > │<-- size -->│        ==> │<--- size' --->│ addr' == addr - addr_decrement
 * > ├────────────┤            ├┈┈┈┈┈──────────┤ size' == size + addr_decrement
 * > └- addr      └- addr+size └- addr'        └- addr'+size'
 * */
int growleft_memblock(memblock_t * mblock, size_t addr_decrement) ;

/* function: growright_memblock
 * Grows the memory block by incrementing only its size.
 * The start address <memblock_t.addr> is not changed but
 * the size of the block <memblock_t.size> is incremented by size_increment.
 * > ╭────────────╮            ╭┈┈┈┈┈──────────╮
 * > │<-- size -->│        ==> │<--- size' --->│
 * > ├────────────┤            ├┈┈┈┈┈──────────┤ size' == size + size_increment
 * > └- addr      └- addr+size └- addr         └- addr+size'
 * */
int growright_memblock(memblock_t * mblock, size_t size_increment) ;

// group: generic

/* function: genericcast_memblock
 * Casts a pointer to generic object into pointer to <memblock_t>.
 * The object must have two members nameprefix##addr and nameprefix##size
 * of the same type as <memblock_t> and in the same order. */
memblock_t * genericcast_memblock(void * obj, IDNAME nameprefix) ;


// section: inline implementation

/* define: addr_memblock
 * Implements <memblock_t.addr_memblock>. */
#define addr_memblock(mblock)          ((mblock)->addr)

/* define: clear_memblock
 * Implements <memblock_t.clear_memblock>. */
#define clear_memblock(mblock)         (memset((mblock)->addr, 0, (mblock)->size))

/* define: genericcast_memblock
 * Implements <memblock_t.genericcast_memblock>. */
#define genericcast_memblock(obj, nameprefix)               \
         ( __extension__ ({                                 \
            typeof(obj) _obj = (obj) ;                      \
            static_assert(                                  \
               sizeof(_obj->nameprefix##addr)               \
               == sizeof(((memblock_t*)0)->addr)            \
               && offsetof(memblock_t, addr)                \
                  == 0                                      \
               && (typeof(_obj->nameprefix##addr))10        \
                  == (uint8_t*)10                           \
               && sizeof(_obj->nameprefix##size)            \
                  == sizeof(((memblock_t*)0)->size)         \
               && offsetof(memblock_t, size)                \
                  == ((uintptr_t)&_obj->nameprefix##size)   \
                     -((uintptr_t)&_obj->nameprefix##addr)  \
               && (typeof(_obj->nameprefix##size)*)10       \
                  == (size_t*)10,                           \
               "obj is compatible") ;                       \
            (memblock_t *)(&_obj->nameprefix##addr) ;       \
         }))

/* define: growleft_memblock
 * Implements <memblock_t.growleft_memblock>. */
#define growleft_memblock(mblock, addr_decrement)        \
         ( __extension__ ({ int _err ;                   \
            typeof(mblock) _mblock = (mblock) ;          \
            size_t _decr = (addr_decrement) ;            \
            if ((uintptr_t)_mblock->addr < _decr) {      \
               _err = ENOMEM ;                           \
            } else {                                     \
               _mblock->addr -= _decr ;                  \
               _mblock->size += _decr ;                  \
               _err = 0 ;                                \
            }                                            \
            _err ;                                       \
         }))

/* define: growright_memblock
 * Implements <memblock_t.growright_memblock>. */
#define growright_memblock(mblock, size_increment)       \
         ( __extension__ ({ int _err ;                   \
            typeof(mblock) _mblock = (mblock) ;          \
            size_t _size = _mblock->size                 \
                         + (size_increment) ;            \
            if ( _size < _mblock->size                   \
                 || ((uintptr_t)_mblock->addr + _size    \
                      < (uintptr_t)_mblock->addr)) {     \
               _err = ENOMEM ;                           \
            } else {                                     \
               _mblock->size = _size ;                   \
               _err = 0 ;                                \
            }                                            \
            _err ;                                       \
         }))

/* define: isfree_memblock
 * Implements <memblock_t.isfree_memblock>. */
#define isfree_memblock(mblock)                          (0 == (mblock)->addr && 0 == (mblock)->size)

/* define: isvalid_memblock
 * Implements <memblock_t.isvalid_memblock>. */
#define isvalid_memblock(mblock)                         (0 != (mblock)->size && 0 != (mblock)->addr)

/* define: shrinkleft_memblock
 * Implements <memblock_t.shrinkleft_memblock>. */
#define shrinkleft_memblock(mblock, addr_increment)      \
      ( __extension__ ({ int _err ;                      \
            typeof(mblock) _mblock = (mblock) ;          \
            size_t _incr = (addr_increment) ;            \
            if (_mblock->size < _incr) {                 \
               _err = ENOMEM ;                           \
            } else {                                     \
               _mblock->addr += _incr ;                  \
               _mblock->size -= _incr ;                  \
               _err = 0 ;                                \
            }                                            \
            _err ;                                       \
      }))

/* define: shrinkright_memblock
 * Implements <memblock_t.shrinkright_memblock>. */
#define shrinkright_memblock(mblock, size_decrement)     \
      ( __extension__ ({ int _err ;                      \
            typeof(mblock) _mblock = (mblock) ;          \
            size_t _decr = (size_decrement) ;            \
            if (_mblock->size < _decr) {                 \
               _err = ENOMEM ;                           \
            } else {                                     \
               _mblock->size -= _decr ;                  \
               _err = 0 ;                                \
            }                                            \
            _err ;                                       \
      }))

/* define: size_memblock
 * Implements <memblock_t.size_memblock>. */
#define size_memblock(mblock)          ((mblock)->size)


#endif
