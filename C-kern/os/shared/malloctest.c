/* title: System-malloc Test
   Implements <prepare_memorytest_malloc> and <usedallocatedsize_malloc>.

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

   file: C-kern/api/os/memory/malloc.h
    Header file of <System-malloc>.

   file: C-kern/os/shared/malloc.c
    Implementation file of <System-malloc impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test.h"
#include "malloc.h"

void prepare_malloctest(void)
{
   // force load language module
   (void) strerror(ENOMEM) ;
   (void) strerror(EEXIST) ;

   trimmemory_malloctest() ;
}

void trimmemory_malloctest(void)
{
   malloc_trim(0) ;
}

size_t usedbytes_malloctest(void)
{
   struct mallinfo info = mallinfo() ;
   return (size_t) info.uordblks ;
}
