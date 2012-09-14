/* title: MemoryManager-ImplementationObject
   Exports <mm_iot>: a pointer to object and its implementation of interface <mm_it>.
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

   file: C-kern/api/memory/mm/mm_iot.h
    Contains interfaceable object <MemoryManager-ImplementationObject>.
*/
#ifndef CKERN_MEMORY_MM_MMIOT_HEADER
#define CKERN_MEMORY_MM_MMIOT_HEADER

// forward
struct mm_t ;
struct mm_it ;

/* typedef: struct mm_iot
 * Export <mm_iot>. Memory manager implementing object. */
typedef struct mm_iot                  mm_iot ;


/* struct: mm_iot
 * A memory manager object implementing interface <mm_it>. */
struct mm_iot {
   /* variable: object
    * A pointer to the object which implements the interface <mm_it>. */
   struct mm_t    * object ;
   /* variable: iimpl
    * A pointer to the implementation of interface <mm_it>. */
   struct mm_it   * iimpl ;
} ;

// group: lifetime

/* define: mm_iot_INIT_FREEABLE
 * Static initializer. */
#define mm_iot_INIT_FREEABLE           { (struct mm_t*)0, (struct mm_it*)0 }

/* define: mm_iot_INIT
 * Static initializer. */
#define mm_iot_INIT(object, iimpl)     { (object), (iimpl) }


#endif
