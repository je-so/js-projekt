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
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/memory/mm/mm_it.h"
#include "C-kern/api/platform/malloc.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/platform/virtmemory.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

int init_resourceusage(/*out*/resourceusage_t * usage)
{
   int err ;
   size_t               fds ;
   size_t               sizeallocated_mmtransient ;
   size_t               allocated ;
   size_t               allocated_endinit ;
   vm_mappedregions_t * mappedregions = 0 ;
   signalconfig_t     * signalconfig  = 0 ;

   err = nropen_filedescr(&fds) ;
   if (err) goto ABBRUCH ;

   sizeallocated_mmtransient = mmtransient_maincontext().functable->sizeallocated(mmtransient_maincontext().object) ;

   err = allocatedsize_malloc(&allocated) ;
   if (err) goto ABBRUCH ;

   mappedregions = MALLOC(vm_mappedregions_t, malloc, ) ;
   if (!mappedregions) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(sizeof(vm_mappedregions_t)) ;
      goto ABBRUCH ;
   }
   *mappedregions = (vm_mappedregions_t) vm_mappedregions_INIT_FREEABLE ;

   err = new_signalconfig(&signalconfig) ;
   if (err) goto ABBRUCH ;

   err = init_vmmappedregions(mappedregions) ;
   if (err) goto ABBRUCH ;

   err = allocatedsize_malloc(&allocated_endinit) ;
   if (err) goto ABBRUCH ;

   usage->filedescriptor_usage = fds ;
   usage->sizealloc_mmtrans    = sizeallocated_mmtransient ;
   usage->malloc_usage         = allocated ;
   usage->malloc_correction    = allocated_endinit - allocated ;
   usage->signalconfig         = signalconfig ;
   usage->virtualmemory_usage  = mappedregions ;

   return 0 ;
ABBRUCH:
   if (mappedregions) {
      (void) free_vmmappedregions(mappedregions) ;
      free(mappedregions) ;
   }
   delete_signalconfig(&signalconfig) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_resourceusage(resourceusage_t * usage)
{
   int err ;
   int err2 ;

   usage->filedescriptor_usage = 0 ;
   usage->malloc_usage         = 0 ;
   usage->malloc_correction    = 0 ;

   if (usage->virtualmemory_usage) {

      err = delete_signalconfig(&usage->signalconfig) ;

      err2 = free_vmmappedregions(usage->virtualmemory_usage) ;
      if (err2) err = err2 ;

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

   err = EAGAIN ;

   if (usage2.filedescriptor_usage != usage->filedescriptor_usage) {
      LOG_ERRTEXT(RESOURCE_USAGE_DIFFERENT) ;
      goto ABBRUCH ;
   }

   if (usage2.sizealloc_mmtrans != usage->sizealloc_mmtrans) {
      LOG_ERRTEXT(RESOURCE_USAGE_DIFFERENT) ;
      goto ABBRUCH ;
   }

   if ((usage2.malloc_usage - usage->malloc_correction) != usage->malloc_usage) {
      LOG_ERRTEXT(RESOURCE_USAGE_DIFFERENT) ;
      goto ABBRUCH ;
   }

   if (compare_vmmappedregions(usage2.virtualmemory_usage, usage->virtualmemory_usage)) {
      LOG_ERRTEXT(RESOURCE_USAGE_DIFFERENT) ;
      goto ABBRUCH ;
   }

   if (compare_signalconfig(usage2.signalconfig, usage->signalconfig)) {
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


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   size_t          malloc_usage  = 1 ;
   size_t          malloc_usage2 = 0 ;
   int             fd            = -1 ;
   void          * memblock      = 0 ;
   vm_block_t      vmblock       = vm_block_INIT_FREEABLE ;
   resourceusage_t usage         = resourceusage_INIT_FREEABLE ;
   resourceusage_t usage2        = resourceusage_INIT_FREEABLE ;
   bool            isoldsigmask  = false ;
   sigset_t        oldsigmask ;

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
   TEST(0 != usage.malloc_correction) ;
   TEST(0 != usage.signalconfig) ;
   TEST(0 != usage.virtualmemory_usage) ;
   TEST(20000 > usage.malloc_correction) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == usage.filedescriptor_usage) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == usage.signalconfig) ;
   TEST(0 == usage.virtualmemory_usage) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == usage.filedescriptor_usage) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == usage.signalconfig) ;
   TEST(0 == usage.virtualmemory_usage) ;

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
   TEST(0 == free_filedescr(&fd)) ;
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

   // TEST compare EAGAIN cause of virtual memory
   sigset_t sigmask ;
   sigemptyset(&sigmask) ;
   sigaddset(&sigmask, SIGABRT) ;
   TEST(0 == sigprocmask(SIG_SETMASK, 0, &oldsigmask)) ;
   isoldsigmask = true ;
   TEST(0 == sigprocmask(SIG_UNBLOCK, &sigmask, 0)) ;
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == sigprocmask(SIG_BLOCK, &sigmask, 0)) ;
   TEST(EAGAIN == same_resourceusage(&usage)) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK, &sigmask, 0)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   TEST(0 == allocatedsize_malloc(&malloc_usage2)) ;
   TEST(malloc_usage == malloc_usage2) ;

   return 0 ;
ABBRUCH:
   free(memblock) ;
   free_filedescr(&fd) ;
   (void) free_vmblock(&vmblock) ;
   (void) free_resourceusage(&usage) ;
   if (isoldsigmask) (void) sigprocmask(SIG_SETMASK, &oldsigmask, 0) ;
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
