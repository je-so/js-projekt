/* title: HWCache impl

   Implements <HWCache>.

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

   file: C-kern/api/memory/hwcache.h
    Header file <HWCache>.

   file: C-kern/memory/hwcache.c
    Implementation file <HWCache impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/hwcache.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/virtmemory.h"
#include "C-kern/api/time/sysclock.h"
#include "C-kern/api/time/timevalue.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

static int test_query(void)
{
   const uint32_t sizeprefetch = sizedataprefetch_hwcache() ;

   // TEST sizedataprefetch_hwcache
   TEST(sizedataprefetch_hwcache() == sizeprefetch) ;
   TEST(sizedataprefetch_hwcache() == 16) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_prefetch(void)
{
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   const uint32_t nrpages  = (256*1024*1024) / pagesize_vm() ;
   int64_t        time_noprefetch = INT64_MAX ;
   int64_t        time_prefetch   = INT64_MAX ;
   timevalue_t    starttm, endtm ;
   uint64_t       sum, sum2 = 0 ;

   // prepare
   TEST(0 == init_vmblock(&memblock, nrpages)) ;
   memset(memblock.addr, 0x03, memblock.size) ;
   uint32_t * const endmem = (uint32_t*) (memblock.addr + memblock.size) ;

#define SUM_4_TIMES_NEXT      \
         sum += *(next++) ;   \
         sum += *(next++) ;   \
         sum += *(next++) ;   \
         sum += *(next++)     \

   for (unsigned i = 0; i < 4; ++i) {
      TEST(0 == time_sysclock(sysclock_MONOTONIC, &starttm)) ;
      sum = 0 ;
      for (uint32_t * next = (uint32_t*)memblock.addr; next < endmem; ) {
         SUM_4_TIMES_NEXT ;
      }
      TEST(0 == time_sysclock(sysclock_MONOTONIC, &endtm)) ;
      int64_t time_in_ms = diffms_timevalue(&endtm, &starttm) ;
      if (time_in_ms < time_noprefetch) time_noprefetch = time_in_ms ;
      if (sum2 == 0) sum2 = sum ;
      TEST(sum == sum2) ;
   }

   for (unsigned i = 0; i < 4; ++i) {
      TEST(0 == time_sysclock(sysclock_MONOTONIC, &starttm)) ;
      sum = 0 ;
      prefetchdata_hwcache(memblock.addr) ;
      for (uint32_t * next = (uint32_t*)memblock.addr; next < endmem-4; ) {
         prefetchdata_hwcache(next+4) ;
         SUM_4_TIMES_NEXT ;
      }
      uint32_t * next = endmem-4 ;
      SUM_4_TIMES_NEXT ;
      TEST(0 == time_sysclock(sysclock_MONOTONIC, &endtm)) ;
      int64_t time_in_ms = diffms_timevalue(&endtm, &starttm) ;
      if (time_in_ms < time_prefetch) time_prefetch = time_in_ms ;
      TEST(sum == sum2) ;
   }

   if (time_noprefetch <= time_prefetch) {
      CPRINTF_LOG(TEST, "** prefetch is not faster ** ") ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   free_vmblock(&memblock) ;
   return EINVAL ;
}

int unittest_memory_hwcache()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_query())          goto ONABORT ;
   if (test_prefetch())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
