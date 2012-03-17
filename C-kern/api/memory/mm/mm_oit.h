/* title: MemoryManager-Object
   Exports <mm_oit>: a pointer to object and its interface <mm_it>.
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

   file: C-kern/api/memory/mm/mm_oit.h
    Contains interfaceable object <MemoryManager-Object>.
*/
#ifndef CKERN_MEMORY_MM_MM_OIT_HEADER
#define CKERN_MEMORY_MM_MM_OIT_HEADER

// forward
struct mm_t ;
struct mm_it ;

/* typedef: struct mm_oit
 * Export <mm_oit> -- memory manager object implementation. */
typedef struct mm_oit                  mm_oit ;


/* struct: mm_oit
 * A memory manager object implementing interface <mm_it>. */
struct mm_oit {
   /* variable: object
    * A pointer to the object which implements the interface <mm_it>. */
   struct mm_t    * object ;
   /* variable: functable
    * A pointer to the table of functions implementing the <mm_it> interface. */
   struct mm_it   * functable ;
} ;

// group: lifetime

/* define: mm_oit_INIT_FREEABLE
 * Static initializer. */
#define mm_oit_INIT_FREEABLE    { (struct mm_t*)0, (struct mm_it*)0 }

#endif
