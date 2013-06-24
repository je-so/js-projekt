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
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/malloc.h"
#include "C-kern/api/platform/sync/signal.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

int init_resourceusage(/*out*/resourceusage_t * usage)
{
   int err ;
   size_t               fds ;
   size_t               pagecache_usage ;
   size_t               pagecache_endinit ;
   size_t               pagecache_staticusage ;
   size_t               mmtrans_usage ;
   size_t               mmtrans_endinit ;
   size_t               allocated ;
   size_t               allocated_endinit ;
   memblock_t           memmapreg     = memblock_INIT_FREEABLE ;
   vm_mappedregions_t * mappedregions = 0 ;
   signalconfig_t     * signalconfig  = 0 ;

   EMPTYCACHE_PAGECACHE() ;

   err = nropen_iochannel(&fds) ;
   if (err) goto ONABORT ;

   pagecache_usage       = sizeallocated_pagecache(pagecache_maincontext()) ;
   pagecache_staticusage = sizestatic_pagecache(pagecache_maincontext()) ;

   mmtrans_usage = sizeallocated_mm(mm_maincontext()) ;

   err = allocatedsize_malloc(&allocated) ;
   if (err) goto ONABORT ;

   err = RESIZE_MM(sizeof(vm_mappedregions_t), &memmapreg) ;
   if (err) goto ONABORT ;

   mappedregions  = (vm_mappedregions_t*) memmapreg.addr ;
   *mappedregions = (vm_mappedregions_t) vm_mappedregions_INIT_FREEABLE ;

   err = new_signalconfig(&signalconfig) ;
   if (err) goto ONABORT ;

   err = init_vmmappedregions(mappedregions) ;
   if (err) goto ONABORT ;

   err = allocatedsize_malloc(&allocated_endinit) ;
   if (err) goto ONABORT ;
   mmtrans_endinit   = sizeallocated_mm(mm_maincontext()) ;
   pagecache_endinit = sizeallocated_pagecache(pagecache_maincontext()) ;

   usage->file_usage           = fds ;
   usage->mmtrans_usage        = mmtrans_usage ;
   usage->mmtrans_correction   = mmtrans_endinit - mmtrans_usage ;
   usage->malloc_usage         = allocated ;
   usage->malloc_correction    = allocated_endinit - allocated ;
   usage->pagecache_usage      = pagecache_usage ;
   usage->pagecache_correction = pagecache_endinit - pagecache_usage ;
   usage->pagecache_staticusage= pagecache_staticusage ;
   usage->signalconfig         = signalconfig ;
   usage->virtualmemory_usage  = mappedregions ;

   return 0 ;
ONABORT:
   if (mappedregions) (void) free_vmmappedregions(mappedregions) ;
   FREE_MM(&memmapreg) ;
   delete_signalconfig(&signalconfig) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_resourceusage(resourceusage_t * usage)
{
   int err ;
   int err2 ;

   usage->file_usage        = 0 ;
   usage->mmtrans_usage     = 0 ;
   usage->mmtrans_correction= 0 ;
   usage->malloc_usage      = 0 ;
   usage->malloc_correction = 0 ;
   usage->pagecache_usage   = 0 ;
   usage->pagecache_correction = 0 ;
   usage->pagecache_staticusage= 0 ;

   if (usage->virtualmemory_usage) {

      err = delete_signalconfig(&usage->signalconfig) ;

      err2 = free_vmmappedregions(usage->virtualmemory_usage) ;
      if (err2) err = err2 ;

      memblock_t mem = memblock_INIT(sizeof(vm_mappedregions_t), (uint8_t*)usage->virtualmemory_usage) ;
      usage->virtualmemory_usage = 0 ;

      err2 = FREE_MM(&mem) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

int same_resourceusage(const resourceusage_t * usage)
{
   int err ;
   resourceusage_t usage2 = resourceusage_INIT_FREEABLE ;

   err = init_resourceusage(&usage2) ;
   if (err) goto ONABORT ;

   err = EAGAIN ;

   if (usage2.file_usage != usage->file_usage) {
      TRACE_NOARG_ERRLOG(RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONABORT ;
   }

   if ((usage2.mmtrans_usage - usage->mmtrans_correction) != usage->mmtrans_usage) {
      TRACE_NOARG_ERRLOG(RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONABORT ;
   }

   if ((usage2.malloc_usage - usage->malloc_correction) != usage->malloc_usage) {
      TRACE_NOARG_ERRLOG(RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONABORT ;
   }

   if ((usage2.pagecache_usage - usage->pagecache_correction) != usage->pagecache_usage) {
      TRACE_NOARG_ERRLOG(RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONABORT ;
   }

   if (usage2.pagecache_staticusage != usage->pagecache_staticusage) {
      TRACE_NOARG_ERRLOG(RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONABORT ;
   }

   if (compare_vmmappedregions(usage2.virtualmemory_usage, usage->virtualmemory_usage)) {
      TRACE_NOARG_ERRLOG(RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONABORT ;
   }

   if (compare_signalconfig(usage2.signalconfig, usage->signalconfig)) {
      TRACE_NOARG_ERRLOG(RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONABORT ;
   }

   err = free_resourceusage(&usage2) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage2) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   // TEST static initializer
   TEST(0 == usage.file_usage) ;
   TEST(0 == usage.mmtrans_usage) ;
   TEST(0 == usage.mmtrans_correction) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == usage.pagecache_usage) ;
   TEST(0 == usage.pagecache_correction) ;
   TEST(0 == usage.pagecache_staticusage) ;
   TEST(0 == usage.signalconfig) ;
   TEST(0 == usage.virtualmemory_usage) ;

   // TEST init_resourceusage, free_resourceusage
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 != usage.file_usage) ;
   TEST(0 != usage.mmtrans_usage) ;
   TEST(0 != usage.mmtrans_correction) ;
   TEST(0 != usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 != usage.pagecache_usage) ;
   TEST(0 == usage.pagecache_correction) ; // change to != 0 if testmm uses pagecache !!
   TEST(0 != usage.pagecache_staticusage) ;
   TEST(0 != usage.signalconfig) ;
   TEST(0 != usage.virtualmemory_usage) ;
   TEST(20000 > usage.malloc_correction) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == usage.file_usage) ;
   TEST(0 == usage.mmtrans_usage) ;
   TEST(0 == usage.mmtrans_correction) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == usage.pagecache_usage) ;
   TEST(0 == usage.pagecache_correction) ;
   TEST(0 == usage.pagecache_staticusage) ;
   TEST(0 == usage.signalconfig) ;
   TEST(0 == usage.virtualmemory_usage) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == usage.file_usage) ;
   TEST(0 == usage.mmtrans_usage) ;
   TEST(0 == usage.mmtrans_correction) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == usage.pagecache_usage) ;
   TEST(0 == usage.pagecache_correction) ;
   TEST(0 == usage.pagecache_staticusage) ;
   TEST(0 == usage.signalconfig) ;
   TEST(0 == usage.virtualmemory_usage) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

static int test_query(void)
{
   size_t          malloc_usage  = 1 ;
   size_t          malloc_usage2 = 0 ;
   int             fd            = -1 ;
   void *          memblock      = 0 ;
   vmpage_t        vmblock       = vmpage_INIT_FREEABLE ;
   resourceusage_t usage         = resourceusage_INIT_FREEABLE ;
   resourceusage_t usage2        = resourceusage_INIT_FREEABLE ;
   bool            isoldsigmask  = false ;
   sigset_t        oldsigmask ;

   // prepare
   TEST(0 == allocatedsize_malloc(&malloc_usage)) ;

   // TEST compare the same
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: EAGAIN cause of open file
   TEST(0 == init_resourceusage(&usage)) ;
   fd = dup(STDERR_FILENO) ;
   TEST(fd > 0) ;
   TEST(EAGAIN == same_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage2)) ;
   TEST(usage.file_usage + 1 == usage2.file_usage) ;
   TEST(0 == free_iochannel(&fd)) ;
   TEST(0 == free_resourceusage(&usage2)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: EAGAIN cause of memory
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

   // TEST same_resourceusage: EAGAIN cause of pagecache
   TEST(0 == init_resourceusage(&usage)) ;
   memblock_t page = memblock_INIT_FREEABLE ;
   TEST(0 == allocpage_pagecache(pagecache_maincontext(), pagesize_4096, &page)) ;
   TEST(EAGAIN == same_resourceusage(&usage)) ;
   TEST(0 == releasepage_pagecache(pagecache_maincontext(), &page)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: EAGAIN cause of static memory
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == allocstatic_pagecache(pagecache_maincontext(), 128, &page)) ;
   TEST(EAGAIN == same_resourceusage(&usage)) ;
   TEST(0 == freestatic_pagecache(pagecache_maincontext(), &page)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: EAGAIN cause of virtual memory
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == init_vmpage(&vmblock, pagesize_vm())) ;
   TEST(EAGAIN == same_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage2)) ;
   TEST(0 == free_vmpage(&vmblock)) ;
   TEST(usage.file_usage   == usage2.file_usage) ;
   TEST(usage.malloc_usage == usage2.malloc_usage - usage.malloc_correction) ;
   TEST(0 == free_resourceusage(&usage2)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: EAGAIN cause of changed signals
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

   // unprepare
   TEST(0 == allocatedsize_malloc(&malloc_usage2)) ;
   TEST(malloc_usage == malloc_usage2) ;

   return 0 ;
ONABORT:
   free(memblock) ;
   free_iochannel(&fd) ;
   (void) free_vmpage(&vmblock) ;
   (void) free_resourceusage(&usage) ;
   if (isoldsigmask) (void) sigprocmask(SIG_SETMASK, &oldsigmask, 0) ;
   return EINVAL ;
}

int unittest_test_resourceusage()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_query())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
