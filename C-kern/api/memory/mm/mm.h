/* title: MemoryManager-Object

   Exports <mm_t>: a pointer to object and its implementation of interface <mm_it>.
   To use this object you need to include <MemoryManager-Interface>

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

   file: C-kern/api/memory/mm/mm_it.h
    Header file of <MemoryManager-Interface>.

   file: C-kern/api/memory/mm/mm.h
    Contains interfaceable object <MemoryManager-Object>.
*/
#ifndef CKERN_MEMORY_MM_MM_HEADER
#define CKERN_MEMORY_MM_MM_HEADER

// forward
struct mm_it ;

/* typedef: struct mm_t
 * Export <mm_t>. Memory manager interfaceable object. */
typedef struct mm_t                    mm_t ;


/* struct: mm_t
 * A memory manager object exporting interface <mm_it>. */
struct mm_t {
   /* variable: object
    * A pointer to the object which implements the interface <mm_it>.
    * The type <mm_t> is casted into a custom type in the implementation
    * of the interface <mm_it>. */
   struct mm_t    * object ;
   /* variable: iimpl
    * A pointer to the implementation of interface <mm_it>. */
   struct mm_it   * iimpl ;
} ;

// group: lifetime

/* define: mm_INIT_FREEABLE
 * Static initializer. */
#define mm_INIT_FREEABLE               { (struct mm_t*)0, (struct mm_it*)0 }

/* define: mm_INIT
 * Static initializer. */
#define mm_INIT(object, iimpl)         { (object), (iimpl) }


#endif
