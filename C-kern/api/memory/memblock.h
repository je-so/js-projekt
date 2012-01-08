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
extern int unittest_memory_memblock(void) ;
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
   uint8_t  * addr ;
   /* variable: size
    * Size of memory in bytes <addr> points to.
    * The valid memory region is
    * > addr[ 0 .. size - 1 ] */
   size_t   size ;
} ;

// group: lifetime

/* define: memblock_INIT_FREEABLE
 * Static initializer. */
#define memblock_INIT_FREEABLE         { 0, 0 }

// group: query

/* function: isfree_memblock
 * Returns true if *mblock* is set to <memblock_INIT_FREEABLE>. */
extern bool isfree_memblock(const memblock_t * mblock) ;

/* function: addr_memblock
 * Returns start (lowest) address of memory block. */
extern uint8_t * addr_memblock(const memblock_t * mblock) ;

/* function: size_memblock
 * Returns size of memory block. */
extern size_t size_memblock(const memblock_t * mblock) ;



// section: inline implementation

#define isfree_memblock(mblock)        (0 == (mblock)->addr)

/* define: addr_memblock
 * Implements <memblock_t.addr_memblock>. */
#define addr_memblock(mblock)          ((mblock)->addr)

/* define: size_memblock
 * Implements <memblock_t.size_memblock>. */
#define size_memblock(mblock)          ((mblock)->size)

#endif
