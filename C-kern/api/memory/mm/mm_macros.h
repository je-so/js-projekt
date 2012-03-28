/* title: MemoryManagerMacros
   Exports convenience macros to access <mmtransient_context>.

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
#ifndef CKERN_MEMORY_MM_MM_MACROS_HEADER
#define CKERN_MEMORY_MM_MM_MACROS_HEADER

#include "C-kern/api/memory/mm/mm_it.h"


// section: Functions

// group: allocate

/* define: MM_RESIZE
 * Resizes memory block. See also <mresize_mmtransient>. */
#define  MM_RESIZE(newsize, mblock)    mmtransient_maincontext().iimpl->mresize(mmtransient_maincontext().object, (newsize), (mblock))

/* define: MM_FREE
 * Frees memory block. See also <mfree_mmtransient>. */
#define  MM_FREE(mblock)               mmtransient_maincontext().iimpl->mfree(mmtransient_maincontext().object, (mblock))


#endif
