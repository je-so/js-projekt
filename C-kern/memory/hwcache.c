/* title: HWCache impl

   Implements <HWCache>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/memory/vm.h"
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
ONERR:
   return EINVAL ;
}

static int test_prefetch(void)
{
   vmpage_t       memblock = vmpage_FREE ;
   const uint32_t blksize  = (256*1024*1024) ;
   int64_t        time_noprefetch = INT64_MAX ;
   int64_t        time_prefetch   = INT64_MAX ;
   timevalue_t    starttm, endtm ;
   uint64_t       sum, sum2 = 0 ;

   // prepare
   TEST(0 == init_vmpage(&memblock, blksize)) ;
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
      for (uint32_t * next = (uint32_t*)memblock.addr; next < endmem-8; ) {
         SUM_4_TIMES_NEXT;
         SUM_4_TIMES_NEXT;
      }
      TEST(0 == time_sysclock(sysclock_MONOTONIC, &endtm)) ;
      int64_t time_in_ms = diffms_timevalue(&endtm, &starttm) ;
      if (time_in_ms < time_noprefetch) time_noprefetch = time_in_ms ;
      if (sum2 == 0) sum2 = sum;
      TEST(sum == sum2);
   }

   for (unsigned i = 0; i < 4; ++i) {
      TEST(0 == time_sysclock(sysclock_MONOTONIC, &starttm)) ;
      sum = 0 ;
      prefetchdata_hwcache(memblock.addr) ;
      for (uint32_t * next = (uint32_t*)memblock.addr; next < endmem-8; ) {
         prefetchdata_hwcache(next+8) ;
         SUM_4_TIMES_NEXT;
         SUM_4_TIMES_NEXT;
      }
      TEST(0 == time_sysclock(sysclock_MONOTONIC, &endtm)) ;
      int64_t time_in_ms = diffms_timevalue(&endtm, &starttm) ;
      if (time_in_ms < time_prefetch) time_prefetch = time_in_ms ;
      TEST(sum == sum2);
   }

   if (time_noprefetch <= time_prefetch) {
      logwarning_unittest("prefetch is not faster");
   }

   // unprepare
   TEST(0 == free_vmpage(&memblock)) ;

   return 0 ;
ONERR:
   free_vmpage(&memblock) ;
   return EINVAL ;
}

int unittest_memory_hwcache()
{
   if (test_query())          goto ONERR;
   if (test_prefetch())       goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
