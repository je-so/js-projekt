/* title: MemoryManagerMacros
   Exports convenience macros for accessing <mm_maincontext>.

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
