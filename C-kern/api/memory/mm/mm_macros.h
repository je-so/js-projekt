/* title: MemoryManagerMacros
   Exports convenience macros for accessing <mm_maincontext>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/memory/mm/mm_macros.h
    Header file <MemoryManagerMacros>.
*/
#ifndef CKERN_MEMORY_MM_MMMACROS_HEADER
#define CKERN_MEMORY_MM_MMMACROS_HEADER

#include "C-kern/api/memory/mm/mm.h"


// section: Functions

// group: query

/* define: SIZEALLOCATED_MM
 * REturns number of allocated bytes. See also <sizeallocated_mmimpl>. */
#define  SIZEALLOCATED_MM()            sizeallocated_mm(mm_maincontext())

// group: allocate

/* define: ALLOC_MM
 * Allocates a new memory block. See also <malloc_mmimpl>. */
#define  ALLOC_MM(size, mblock)        malloc_mm(mm_maincontext(), size, mblock)

/* define: RESIZE_MM
 * Resizes memory block. See also <mresize_mmimpl>. */
#define  RESIZE_MM(newsize, mblock)    mresize_mm(mm_maincontext(), newsize, mblock)

/* define: FREE_MM
 * Frees memory block. See also <mfree_mmimpl>. */
#define  FREE_MM(mblock)               mfree_mm(mm_maincontext(), mblock)


#endif
