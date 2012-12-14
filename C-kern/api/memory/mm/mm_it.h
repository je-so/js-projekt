/* title: MemoryManager-Interface

   Interface functions to access memory manager. Implemented by <mmtransient_t>.

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
    Defines interfaceable object <MemoryManager-Object>.

   file: C-kern/memory/mm/mm_it.c
    Implements unittest <MemoryManager-Interface impl>.
*/
#ifndef CKERN_MEMORY_MM_MMIT_HEADER
#define CKERN_MEMORY_MM_MMIT_HEADER

// forward
struct mm_t ;
struct memblock_t ;

/* typedef: struct mm_it
 * Export interface <mm_it>.
 * See <mm_it_DECLARE> for adaption to a specific implementation. */
typedef struct mm_it                   mm_it ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_mm_mmit
 * Test <mm_it> functionality. */
int unittest_memory_mm_mmit(void) ;
#endif


/* struct: mm_it
 * The function table describing the interface to a memory manager.
 * If you change the interface of <mm_it> to do not forget to adapt
 * <mm_it_DECLARE> to the same signature. */
struct mm_it {
   /* function: mresize
    * See <mmtransient_t.mresize_mmtransient> for an implementation. */
   int (* mresize) (struct mm_t * mman, size_t newsize, struct memblock_t * memblock) ;
   /* function: mfree
    * See <mmtransient_t.mfree_mmtransient> for an implementation. */
   int (* mfree)   (struct mm_t * mman, struct memblock_t * memblock) ;
   /* function: sizeallocated
    * See <mmtransient_t.sizeallocated_mmtransient> for an implementation. */
   size_t (* sizeallocated) (struct mm_t * mman) ;
} ;

// group: lifetime

/* define: mm_it_INIT_FREEABLE
 * Static initializer. Set all fields to 0. */
#define mm_it_INIT_FREEABLE            { 0, 0, 0 }

/* define: mm_it_INIT_FREEABLE
 * Static initializer. Set all function pointers to the provided values.
 *
 * Parameters:
 * mresize_f       - Function pointer to allocate and resize memory. See <mm_it.mresize>.
 * mfree_f         - Function pointer to free memory blocks. See <mm_it.mfree>.
 * sizeallocated_f - Function pointer to query the size of all allocated memory nlocks. See <mm_it.sizeallocated>. */
#define mm_it_INIT(mresize_f, mfree_f, sizeallocated_f) \
            { (mresize_f), (mfree_f), (sizeallocated_f) }

// group: generic

/* function: asgeneric_mmit
 * Casts parameter mminterface into pointer to interface <mm_it>.
 * The parameter *mminterface* has to be of type pointer to type declared with <mm_it_DECLARE>.
 * The other parameters must be the same as in <mm_it_DECLARE> without the first. */
mm_it * asgeneric_mmit(void * mminterface, TYPENAME memorymanager_t) ;


/* function: mm_it_DECLARE
 * Declares an interface function table for accessing a memory manager service.
 * The declared interface is structural compatible with the generic interface <mm_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the object. Every interface function takes a pointer to this object type
 * as its first parameter.
 *
 * Parameter:
 * declared_it       - The name of the structure which is declared as the interface.
 *                     The name should have the suffix "_it".
 * memorymanager_t   - The type of the memory manager object which is the first parameter of all interface functions.
 *
 * See <mm_it> for a list of declared functions.
 * */
void mm_it_DECLARE(TYPENAME declared_it, TYPENAME memorymanager_t) ;


// section: inline implementation

/* define: asgeneric_mmit
 * Implements <mm_it.asgeneric_mmit>. */
#define asgeneric_mmit(mminterface, memorymanager_t)                                            \
   ( __extension__ ({                                                                           \
      static_assert(                                                                            \
         offsetof(typeof(*(mminterface)), mresize) == offsetof(mm_it, mresize)                  \
         && offsetof(typeof(*(mminterface)), mfree) == offsetof(mm_it, mfree)                   \
         && offsetof(typeof(*(mminterface)), sizeallocated) == offsetof(mm_it, sizeallocated),  \
         "ensure same structure") ;                                                             \
      if (0) {                                                                                  \
         int _err = (mminterface)->mresize((memorymanager_t*)0, (size_t)-1, (struct memblock_t*)0) ;  \
         _err += (mminterface)->mfree((memorymanager_t*)0, (struct memblock_t*)0) ;                   \
         size_t _err2 = (mminterface)->sizeallocated((memorymanager_t*)0) ;                           \
         _err += _err2 > INT_MAX ? INT_MAX : (int) _err2 ;                                      \
         (void) _err ;                                                                          \
      }                                                                                         \
      (mm_it*) (mminterface) ;                                                                  \
   }))


/* define: mm_it_DECLARE
 * Implements <mm_it.mm_it_DECLARE>. */
#define mm_it_DECLARE(declared_it, memorymanager_t)                                                \
   struct declared_it {                                                                            \
      int    (* mresize) (memorymanager_t * mman, size_t newsize, struct memblock_t * memblock) ;  \
      int    (* mfree)   (memorymanager_t * mman, struct memblock_t * memblock) ;                  \
      size_t (* sizeallocated) (memorymanager_t * mman) ;                                          \
   } ;


#endif
