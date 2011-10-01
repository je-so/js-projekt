/* title: Resourceusage impl
   Implements <Resourceusage>.

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

   file: C-kern/api/test/resourceusage.h
    Header file of <Resourceusage>.

   file: C-kern/test/resourceusage.c
    Implementation file <Resourceusage impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/os/file.h"
#include "C-kern/api/os/malloc.h"
#include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

int init_resourceusage(/*out*/resourceusage_t * usage)
{
   int err ;
   size_t               fds ;
   size_t               allocated ;
   size_t               alloc_correction ;
   vm_mappedregions_t * mappedregions = 0 ;

   err = openfd_file(&fds) ;
   if (err) goto ABBRUCH ;

   err = allocatedsize_malloc(&allocated) ;
   if (err) goto ABBRUCH ;

   mappedregions = MALLOC(vm_mappedregions_t, malloc, ) ;
   if (!mappedregions) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(sizeof(vm_mappedregions_t)) ;
      goto ABBRUCH ;
   }
   *mappedregions = (vm_mappedregions_t) vm_mappedregions_INIT_FREEABLE ;

   err = init_vmmappedregions(mappedregions) ;
   if (err) goto ABBRUCH ;

   err = allocatedsize_malloc(&alloc_correction) ;
   if (err) goto ABBRUCH ;

   usage->filedescriptor_usage = fds ;
   usage->malloc_usage         = allocated ;
   usage->virtualmemory_usage  = mappedregions ;
   usage->malloc_correction    = alloc_correction - allocated ;

   return 0 ;
ABBRUCH:
   if(mappedregions) {
      (void) free_vmmappedregions(mappedregions) ;
      free(mappedregions) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int free_resourceusage(resourceusage_t * usage)
{
   int err ;

   usage->filedescriptor_usage = 0 ;
   usage->malloc_usage         = 0 ;
   usage->malloc_correction    = 0 ;

   if (usage->virtualmemory_usage) {

      err = free_vmmappedregions(usage->virtualmemory_usage) ;

      free(usage->virtualmemory_usage) ;
      usage->virtualmemory_usage = 0 ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int same_resourceusage(const resourceusage_t * usage)
{
   int err ;
   resourceusage_t usage2 = resourceusage_INIT_FREEABLE ;

   err = init_resourceusage(&usage2) ;
   if (err) goto ABBRUCH ;

   if (usage2.filedescriptor_usage != usage->filedescriptor_usage) {
      err = EAGAIN ;
      LOG_ERRTEXT(RESOURCE_USAGE_DIFFERENT) ;
      goto ABBRUCH ;
   }

   if ((usage2.malloc_usage - usage->malloc_correction) != usage->malloc_usage) {
      err = EAGAIN ;
      LOG_ERRTEXT(RESOURCE_USAGE_DIFFERENT) ;
      goto ABBRUCH ;
   }

   if (compare_vmmappedregions(usage2.virtualmemory_usage, usage->virtualmemory_usage)) {
      err = EAGAIN ;
      LOG_ERRTEXT(RESOURCE_USAGE_DIFFERENT) ;
      goto ABBRUCH ;
   }

   err = free_resourceusage(&usage2) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage2) ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_test_resourceusage,ABBRUCH)

static int test_initfree(void)
{
   size_t          malloc_usage  = 1 ;
   size_t          malloc_usage2 = 0 ;
   int             fd            = -1 ;
   void          * memblock      = 0 ;
   vm_block_t      vmblock       = vm_block_INIT_FREEABLE ;
   resourceusage_t usage         = resourceusage_INIT_FREEABLE ;
   resourceusage_t usage2        = resourceusage_INIT_FREEABLE ;

   TEST(0 == allocatedsize_malloc(&malloc_usage)) ;

   // TEST static initializer
   TEST(0 == usage.filedescriptor_usage) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.virtualmemory_usage) ;
   TEST(0 == usage.malloc_correction) ;

   // TEST init, double free
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 != usage.filedescriptor_usage) ;
   TEST(0 != usage.malloc_usage) ;
   TEST(0 != usage.virtualmemory_usage) ;
   TEST(0 != usage.malloc_correction) ;
   TEST(10000 > usage.malloc_correction) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == usage.filedescriptor_usage) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.virtualmemory_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == usage.filedescriptor_usage) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.virtualmemory_usage) ;
   TEST(0 == usage.malloc_correction) ;

   // TEST compare the same
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST compare EAGAIN cause of filedescriptor
   TEST(0 == init_resourceusage(&usage)) ;
   fd = dup(STDERR_FILENO) ;
   TEST(fd > 0) ;
   TEST(EAGAIN == same_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage2)) ;
   TEST(usage.filedescriptor_usage + 1 == usage2.filedescriptor_usage) ;
   close(fd) ;
   fd = -1 ;
   TEST(0 == free_resourceusage(&usage2)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST compare EAGAIN cause of memory
   TEST(0 == init_resourceusage(&usage)) ;
   size_t allocated[2] ;
   TEST(0 == allocatedsize_malloc(&allocated[0])) ;
   memblock = malloc(16) ;
   TEST(0 == allocatedsize_malloc(&allocated[1])) ;
   TEST(EAGAIN == same_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage2)) ;
   free(memblock) ;
   memblock = 0 ;
   TEST(usage.malloc_usage + usage.malloc_correction == usage2.malloc_usage - (allocated[1]-allocated[0])) ;
   TEST(0 == free_resourceusage(&usage2)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST compare EAGAIN cause of virtual memory
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == init_vmblock(&vmblock,1)) ;
   TEST(EAGAIN == same_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage2)) ;
   TEST(0 == free_vmblock(&vmblock)) ;
   TEST(usage.filedescriptor_usage == usage2.filedescriptor_usage) ;
   TEST(usage.malloc_usage         == usage2.malloc_usage - usage.malloc_correction) ;
   TEST(0 == free_resourceusage(&usage2)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;


   TEST(0 == allocatedsize_malloc(&malloc_usage2)) ;
   TEST(malloc_usage == malloc_usage2) ;

   return 0 ;
ABBRUCH:
   free(memblock) ;
   if (-1 != fd) close(fd) ;
   (void) free_vmblock(&vmblock) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_test_resourceusage()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
