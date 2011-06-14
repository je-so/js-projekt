/* title: Memoryblock-Aspect
   Structure describing a block of memory.

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

   file: C-kern/api/aspect/memoryblock.h
    Header file of <Memoryblock-Aspect>.
*/
#ifndef CKERN_ASPECT_MEMORYBLOCK_HEADER
#define CKERN_ASPECT_MEMORYBLOCK_HEADER

/* typedef: memoryblock_aspect_t typedef
 * Shortcut for <memoryblock_aspect_t >. */
typedef struct memoryblock_aspect_t    memoryblock_aspect_t ;


/* struct: memoryblock_aspect_t
 * Describes the start address and size of a memory block. */
struct memoryblock_aspect_t {
   /* variable: addr
    * Points to lowest address of memory.
    * If addr is (0) then the memory block is in a freed state
    * and it is not allowed to access it. */
   uint8_t   * addr ;
   /* variable: size
    * Size of memory in bytes.
    * The valid memory region is
    * > addr[ 0 .. size - 1 ] */
   size_t      size ;
} ;

// group: lifetime

/* define: memoryblock_aspect_INIT_FREEABLE
 * Static initializer. */
#define memoryblock_aspect_INIT_FREEABLE            { 0, 0 }


#endif
