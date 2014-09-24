/* title: Resourceusage impl

   Implements <Resourceusage>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
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
   memblock_t           memmapreg     = memblock_FREE ;
   vm_mappedregions_t * mappedregions = 0 ;
   signalstate_t *      signalstate   = 0 ;

   EMPTYCACHE_PAGECACHE() ;

   err = nropen_iochannel(&fds) ;
   if (err) goto ONERR;

   pagecache_usage       = sizeallocated_pagecache(pagecache_maincontext()) ;
   pagecache_staticusage = sizestatic_pagecache(pagecache_maincontext()) ;

   mmtrans_usage = sizeallocated_mm(mm_maincontext()) ;

   err = allocatedsize_malloc(&allocated) ;
   if (err) goto ONERR;

   err = RESIZE_MM(sizeof(vm_mappedregions_t), &memmapreg) ;
   if (err) goto ONERR;

   mappedregions  = (vm_mappedregions_t*) memmapreg.addr ;
   *mappedregions = (vm_mappedregions_t) vm_mappedregions_FREE ;

   err = new_signalstate(&signalstate) ;
   if (err) goto ONERR;

   err = init_vmmappedregions(mappedregions) ;
   if (err) goto ONERR;

   err = allocatedsize_malloc(&allocated_endinit) ;
   if (err) goto ONERR;
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
   usage->signalstate          = signalstate ;
   usage->virtualmemory_usage  = mappedregions ;

   return 0 ;
ONERR:
   if (mappedregions) (void) free_vmmappedregions(mappedregions) ;
   FREE_MM(&memmapreg) ;
   delete_signalstate(&signalstate) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int free_resourceusage(resourceusage_t * usage)
{
   int err ;
   int err2 ;

   if (usage->virtualmemory_usage) {

      err = delete_signalstate(&usage->signalstate) ;

      err2 = free_vmmappedregions(usage->virtualmemory_usage) ;
      if (err2) err = err2 ;

      memblock_t mem = memblock_INIT(sizeof(vm_mappedregions_t), (uint8_t*)usage->virtualmemory_usage) ;
      usage->virtualmemory_usage = 0 ;

      err2 = FREE_MM(&mem) ;
      if (err2) err = err2 ;

      memset(usage, 0, sizeof(resourceusage_t));

      if (err) goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

int same_resourceusage(const resourceusage_t * usage)
{
   int err ;
   resourceusage_t usage2 = resourceusage_FREE ;

   err = init_resourceusage(&usage2) ;
   if (err) goto ONERR;

   err = ELEAK ;

   if (usage2.file_usage != usage->file_usage) {
      TRACE_NOARG_ERRLOG(log_flags_NONE, RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONERR;
   }

   if ((usage2.mmtrans_usage - usage->mmtrans_correction) != usage->mmtrans_usage) {
      TRACE_NOARG_ERRLOG(log_flags_NONE, RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONERR;
   }

   if ( (usage2.malloc_usage - usage->malloc_correction - usage->malloc_usage) > usage->malloc_acceptleak) {
      TRACE_NOARG_ERRLOG(log_flags_NONE, RESOURCE_USAGE_DIFFERENT, err) ;
      size_t leaked_malloc_bytes = (usage2.malloc_usage - usage->malloc_correction - usage->malloc_usage)
                              - usage->malloc_acceptleak;
      PRINTSIZE_ERRLOG(leaked_malloc_bytes);
      goto ONERR;
   }

   if ((usage2.pagecache_usage - usage->pagecache_correction) != usage->pagecache_usage) {
      TRACE_NOARG_ERRLOG(log_flags_NONE, RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONERR;
   }

   if (usage2.pagecache_staticusage != usage->pagecache_staticusage) {
      TRACE_NOARG_ERRLOG(log_flags_NONE, RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONERR;
   }

   if (compare_vmmappedregions(usage2.virtualmemory_usage, usage->virtualmemory_usage)) {
      TRACE_NOARG_ERRLOG(log_flags_NONE, RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONERR;
   }

   if (compare_signalstate(usage2.signalstate, usage->signalstate)) {
      TRACE_NOARG_ERRLOG(log_flags_NONE, RESOURCE_USAGE_DIFFERENT, err) ;
      goto ONERR;
   }

   err = free_resourceusage(&usage2) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   (void) free_resourceusage(&usage2) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   resourceusage_t   usage = resourceusage_FREE ;

   // TEST static initializer
   TEST(0 == usage.file_usage) ;
   TEST(0 == usage.mmtrans_usage) ;
   TEST(0 == usage.mmtrans_correction) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == usage.pagecache_usage) ;
   TEST(0 == usage.pagecache_correction) ;
   TEST(0 == usage.pagecache_staticusage) ;
   TEST(0 == usage.signalstate) ;
   TEST(0 == usage.virtualmemory_usage) ;
   TEST(0 == usage.malloc_acceptleak);

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
   TEST(0 != usage.signalstate) ;
   TEST(0 != usage.virtualmemory_usage) ;
   TEST(20000 > usage.malloc_correction) ;
   usage.malloc_acceptleak = 1;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == usage.file_usage) ;
   TEST(0 == usage.mmtrans_usage) ;
   TEST(0 == usage.mmtrans_correction) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == usage.pagecache_usage) ;
   TEST(0 == usage.pagecache_correction) ;
   TEST(0 == usage.pagecache_staticusage) ;
   TEST(0 == usage.signalstate) ;
   TEST(0 == usage.virtualmemory_usage) ;
   TEST(0 == usage.malloc_acceptleak) ;
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == usage.file_usage) ;
   TEST(0 == usage.mmtrans_usage) ;
   TEST(0 == usage.mmtrans_correction) ;
   TEST(0 == usage.malloc_usage) ;
   TEST(0 == usage.malloc_correction) ;
   TEST(0 == usage.pagecache_usage) ;
   TEST(0 == usage.pagecache_correction) ;
   TEST(0 == usage.pagecache_staticusage) ;
   TEST(0 == usage.signalstate) ;
   TEST(0 == usage.virtualmemory_usage) ;
   TEST(0 == usage.malloc_acceptleak) ;

   return 0 ;
ONERR:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

static int test_query(void)
{
   size_t          malloc_usage  = 1 ;
   size_t          malloc_usage2 = 0 ;
   int             fd            = -1 ;
   void *          memblock      = 0 ;
   vmpage_t        vmblock       = vmpage_FREE ;
   resourceusage_t usage         = resourceusage_FREE ;
   resourceusage_t usage2        = resourceusage_FREE ;
   bool            isoldsigmask  = false ;
   sigset_t        oldsigmask ;

   // prepare
   TEST(0 == allocatedsize_malloc(&malloc_usage)) ;

   // TEST compare the same
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: ELEAK cause of open file
   TEST(0 == init_resourceusage(&usage)) ;
   fd = dup(STDERR_FILENO) ;
   TEST(fd > 0) ;
   TEST(ELEAK == same_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage2)) ;
   TEST(usage.file_usage + 1 == usage2.file_usage) ;
   TEST(0 == free_iochannel(&fd)) ;
   TEST(0 == free_resourceusage(&usage2)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: ELEAK cause of memory
   TEST(0 == init_resourceusage(&usage)) ;
   size_t allocated[2] ;
   TEST(0 == allocatedsize_malloc(&allocated[0])) ;
   memblock = malloc(16) ;
   TEST(0 == allocatedsize_malloc(&allocated[1])) ;
   TEST(ELEAK == same_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage2)) ;
   free(memblock) ;
   memblock = 0 ;
   TEST(usage.malloc_usage + usage.malloc_correction == usage2.malloc_usage - (allocated[1]-allocated[0])) ;
   TEST(0 == free_resourceusage(&usage2)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: ELEAK cause of pagecache
   TEST(0 == init_resourceusage(&usage)) ;
   memblock_t page = memblock_FREE ;
   TEST(0 == allocpage_pagecache(pagecache_maincontext(), pagesize_4096, &page)) ;
   TEST(ELEAK == same_resourceusage(&usage)) ;
   TEST(0 == releasepage_pagecache(pagecache_maincontext(), &page)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: ELEAK cause of static memory
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == allocstatic_pagecache(pagecache_maincontext(), 128, &page)) ;
   TEST(ELEAK == same_resourceusage(&usage)) ;
   TEST(0 == freestatic_pagecache(pagecache_maincontext(), &page)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: ELEAK cause of virtual memory
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == init_vmpage(&vmblock, pagesize_vm())) ;
   TEST(ELEAK == same_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage2)) ;
   TEST(0 == free_vmpage(&vmblock)) ;
   TEST(usage.file_usage   == usage2.file_usage) ;
   TEST(usage.malloc_usage == usage2.malloc_usage - usage.malloc_correction) ;
   TEST(0 == free_resourceusage(&usage2)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // TEST same_resourceusage: ELEAK cause of changed signals
   sigset_t sigmask ;
   sigemptyset(&sigmask) ;
   sigaddset(&sigmask, SIGABRT) ;
   TEST(0 == sigprocmask(SIG_SETMASK, 0, &oldsigmask)) ;
   isoldsigmask = true ;
   TEST(0 == sigprocmask(SIG_UNBLOCK, &sigmask, 0)) ;
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == sigprocmask(SIG_BLOCK, &sigmask, 0)) ;
   TEST(ELEAK == same_resourceusage(&usage)) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK, &sigmask, 0)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // unprepare
   TEST(0 == allocatedsize_malloc(&malloc_usage2)) ;
   TEST(malloc_usage == malloc_usage2) ;

   return 0 ;
ONERR:
   free(memblock) ;
   free_iochannel(&fd) ;
   (void) free_vmpage(&vmblock) ;
   (void) free_resourceusage(&usage) ;
   if (isoldsigmask) (void) sigprocmask(SIG_SETMASK, &oldsigmask, 0) ;
   return EINVAL ;
}

static int test_update(void)
{
   resourceusage_t usage    = resourceusage_FREE ;
   void *          memblock = 0;

   // TEST acceptmallocleak_resourceusage: value is remembered in usage.malloc_acceptleak
   for (uint16_t i = 65535;; i /= 2) {
      acceptmallocleak_resourceusage(&usage, i);
      TEST(i == usage.malloc_acceptleak);
      if (!i) break;
   }

   // TEST acceptmallocleak_resourceusage: same_resourceusage uses value
   for (unsigned i = 16; i <= 1024; i *= 2) {
      size_t sizeold;
      size_t sizenew;
      TEST(0 == init_resourceusage(&usage));
      TEST(0 == allocatedsize_malloc(&sizeold));
      memblock = malloc(i);
      TEST(0 == allocatedsize_malloc(&sizenew));
      TEST(sizenew >= sizeold+i);
      TEST(sizenew <= sizeold+2*i);
      TEST(ELEAK == same_resourceusage(&usage));
      // accept leaks with one byte less
      acceptmallocleak_resourceusage(&usage, (uint16_t)(sizenew-sizeold-1));
      TEST(ELEAK == same_resourceusage(&usage));
      // accept leak exactly
      acceptmallocleak_resourceusage(&usage, (uint16_t)(sizenew-sizeold));
      TEST(0 == same_resourceusage(&usage)) ;
      // accept leaks with one byte more
      acceptmallocleak_resourceusage(&usage, (uint16_t)(sizenew-sizeold+1));
      TEST(0 == same_resourceusage(&usage)) ;
      TEST(0 == free_resourceusage(&usage)) ;
      free(memblock);
      memblock = 0;
   }

   return 0 ;
ONERR:
   free(memblock) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_test_resourceusage()
{
   resourceusage_t usage = resourceusage_FREE;

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0 ;
ONERR:
   (void) free_resourceusage(&usage);
   return EINVAL;
}

#endif
