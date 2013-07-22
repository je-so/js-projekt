/* title: HWCache

   Allows to prefetch data into level 1 cache.

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
#ifndef CKERN_MEMORY_HWCACHE_HEADER
#define CKERN_MEMORY_HWCACHE_HEADER

/* typedef: struct hwcache_t
 * Export <hwcache_t> into global namespace. */
typedef struct hwcache_t               hwcache_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_hwcache
 * Test <hwcache_t> functionality. */
int unittest_memory_hwcache(void) ;
#endif


/* struct: hwcache_t
 * Implicit type allows to access a hardware object. */
struct hwcache_t ;

// group: query

/* function: sizedataprefetch_hwcache
 * Size of aligned memory block transfered to and from data cache.
 * The stored blocks are also called cache lines internally. A cache line stores
 * more then raw data bytes. It stores also the memory address and additional
 * flags which describe the state of the cached data.
 *
 * This value also gives the memory address alignment of such a cache line.
 *
 * Dummy Implementation:
 * The returned value is reasonable but does not adapt to the current hardware.
 * TODO: Implement it in a hardware specific way. */
uint32_t sizedataprefetch_hwcache(void) ;

// group: update

/* function: prefetchdata_hwcache
 * Prefetches data into all levels of hw cache.
 * The transfered data block is of size <sizedataprefetch_hwcache> and has the same alignment.
 * The transfered data block contains at least the first byte of the given address. */
void prefetchdata_hwcache(void * addr) ;


// section: inline implementation

/* define: prefetchdata_hwcache
 * Implements <hwcache_t.prefetchdata_hwcache>. */
#define prefetchdata_hwcache(addr)           (__builtin_prefetch((addr), 0/*read*/, 3/*put data in all cache levels*/))

/* define: sizedataprefetch_hwcache
 * Implements <hwcache_t.sizedataprefetch_hwcache>. */
#define sizedataprefetch_hwcache()           (16)

#endif
