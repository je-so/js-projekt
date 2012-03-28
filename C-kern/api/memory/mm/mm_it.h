/* title: MemoryManager-Interface
   Interface functions to access memory manager.
   Implemented by <mm_transient_t>.

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
#ifndef CKERN_MEMORY_MM_MM_IT_HEADER
#define CKERN_MEMORY_MM_MM_IT_HEADER

// forward
struct mm_t ;
struct memblock_t ;

/* typedef: struct mm_it
 * Export interface <mm_it>.
 * See <mm_it_DECLARE> for adaption to a specific implementation. */
typedef struct mm_it                   mm_it ;


/* struct: mm_it
 * The function table describing the interface to a memory manager.
 * If you change the interface of <mm_it> to do not forget to adapt
 * <mm_it_DECLARE> to the same signature. */
struct mm_it {
   /* function: mresize
    * See <mm_transient_t.mresize_mmtransient> for an implementation. */
   int (* mresize) (struct mm_t * mman, size_t newsize, struct memblock_t * memblock) ;
   /* function: mfree
    * See <mm_transient_t.mfree_mmtransient> for an implementation. */
   int (* mfree)   (struct mm_t * mman, struct memblock_t * memblock) ;
   /* function: sizeallocated
    * See <mm_transient_t.sizeallocated_mmtransient> for an implementation. */
   size_t (* sizeallocated) (struct mm_t * mman) ;
} ;

// group: lifetime

/* define: mm_it_INIT_FREEABLE
 * Static initializer. Sets all fields to 0. */
#define mm_it_INIT_FREEABLE            { 0, 0, 0 }

// group: generic

/* define: mm_it_DECLARE
 * Declares an interface function table for accessing a memory manager service.
 * The declared interface is structural compatible with the generic interface <mm_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the object. Every interface function takes a pointer to this object type
 * as its first parameter.
 *
 * Parameter:
 * declared_it - The name of the structure which is declared as the interface.
 *               The name should have the suffix "_it".
 * mm_t        - The type of the memory manager object which is the first parameter of all interface functions.
 *
 * See <mm_it> for a list of declared functions.
 * */
#define mm_it_DECLARE(declared_it, mm_t)                                            \
   struct declared_it {                                                             \
      int (* mresize) (mm_t * mman, size_t newsize, struct memblock_t * memblock) ; \
      int (* mfree)   (mm_t * mman, struct memblock_t * memblock) ;                 \
      size_t (* sizeallocated) (mm_t * mman) ;                                      \
   } ;

#endif
