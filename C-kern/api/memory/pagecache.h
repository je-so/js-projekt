/* title: PageCacheIface

   Offers object and interface for allocating pages of memory.

   Do not use the interface directly instead include <PageCacheMacros>.

   Additional Includes:
   You have to include "C-kern/api/math/int/log2.h" if you want to call
   <pagecache_t.pagesizefrombytes_pagecache>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/pagecache.h
    Header file <PageCacheIface>.

   file: C-kern/memory/pagecache.c
    Implementation file <PageCacheIface impl>.
*/
#ifndef CKERN_MEMORY_PAGECACHE_HEADER
#define CKERN_MEMORY_PAGECACHE_HEADER

// forward
struct memblock_t;

// === exported types
struct pagecache_t;
struct pagecache_it;

/* enums: pagesize_e
 * List of supported page sizes.
 *
 * pagesize_256    - Request page size of 256 bytes aligned to a 256 byte boundary.
 * pagesize_512    - Request page size of 512 bytes aligned to a 512 byte boundary.
 * pagesize_1024   - Request page size of 1024 bytes aligned to a 1024 byte boundary.
 * pagesize_2048   - Request page size of 2048 bytes aligned to a 2048 byte boundary.
 * pagesize_4096   - Request page size of 4096 bytes aligned to a 4096 byte boundary.
 * pagesize_8192   - Request page size of 8192 bytes aligned to a 8192 byte boundary.
 * pagesize_16384  - Request page size of 16384 bytes aligned to a 16384 byte boundary.
 * pagesize_32768  - Request page size of 32768 bytes aligned to a 32768 byte boundary.
 * pagesize_65536  - Request page size of 65536 bytes aligned to a 65536 byte boundary.
 * pagesize_131072 - Request page size of 131072 bytes aligned to a 131072 byte boundary.
 * pagesize_262144 - Request page size of 262144 bytes aligned to a 262144 byte boundary.
 * pagesize_524288 - Request page size of 524288 bytes aligned to a 524288 byte boundary.
 * pagesize_1MB    - Request page size of 1048576 bytes aligned to a 1MB byte boundary.
 * pagesize__NROF  - The number of named values of <pagesize_e>.
 * */
typedef enum pagesize_e {
   pagesize_256,
   pagesize_512,
   pagesize_1024,
   pagesize_2048,
   pagesize_4096,
   pagesize_8192,
   pagesize_16384,
   pagesize_32768,
   pagesize_65536,
   pagesize_131072,
   pagesize_262144,
   pagesize_524288,
   pagesize_1MB
} pagesize_e;

#define pagesize__NROF (pagesize_1MB + 1)


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_pagecache
 * Test functionality of <pagecache_t> and <pagecache_it>. */
int unittest_memory_pagecache(void);
#endif



/* struct: pagecache_t
 * Defined as <iobj_t.iobj_T>(pagecache).
 * See also <pagecache_impl_t> which is the default implementation.
 * */
typedef iobj_T(pagecache) pagecache_t;

// group: lifetime

/* define: pagecache_FREE
 * Static initializer. See <iobj_t.iobj_FREE>. */
#define pagecache_FREE \
         iobj_FREE

/* define: pagecache_INIT
 * Static initializer. See <iobj_t.iobj_INIT>. */
#define pagecache_INIT(object, iimpl) \
         iobj_INIT(object, iimpl)

// group: query

/* function: isobject_pagecache
 * Returns true if member <pagecache_t.object> of pgcache is not null. */
bool isobject_pagecache(const pagecache_t* pgcache);

/* function: pagesizeinbytes_pagecache
 * Translates enum <pagesize_e> into size in bytes.
 *
 * Unchecked Precondition:
 * o pagesize < <pagesize__NROF> */
static inline size_t pagesizeinbytes_pagecache(pagesize_e pagesize);

/* function: log2pagesizeinbytes_pagecache
 * Translates enum <pagesize_e> into log2_int(pagesizeinbytes_pagecache(pagesize))
 *
 * Unchecked Precondition:
 * o pagesize < <pagesize__NROF> */
static inline uint8_t log2pagesizeinbytes_pagecache(pagesize_e pagesize);

/* function: pagesizefrombytes_pagecache
 * Translates size in bytes into enum <pagesize_e>.
 *
 * Unchecked Preconditions:
 * o pagesize is a power of two
 * o pagesize <= 1024*1024 */
pagesize_e pagesizefrombytes_pagecache(size_t size_in_bytes);

// group: call

/* function: allocpage_pagecache
 * Calls pgcache->iimpl->allocpage with pgcache->object as first parameter. See <pagecache_it.allocpage>. */
int allocpage_pagecache(const pagecache_t pgcache, uint8_t pgsize, /*out*/struct memblock_t* page);

/* function: releasepage_pagecache
 * Calls pgcache->iimpl->releasepage with pgcache->object as first parameter. See <pagecache_it.releasepage>. */
int releasepage_pagecache(const pagecache_t pgcache, struct memblock_t* page);

/* function: sizeallocated_pagecache
 * Calls pgcache->iimpl->sizeallocated with pgcache->object as first parameter. See <pagecache_it.sizeallocated>. */
size_t sizeallocated_pagecache(const pagecache_t pgcache);

/* function: emptycache_pagecache
 * Calls pgcache->iimpl->emptycache with pgcache->object as first parameter. See <pagecache_it.emptycache>. */
int emptycache_pagecache(const pagecache_t pgcache);


/* struct: pagecache_it
 * Interface which allows to allocate and relase pages of memory. */
typedef struct pagecache_it {
   // group: private fields
   /* function: allocpage
    * Allocates a single memory page of size pgsize.
    * The page is aligned to its own size. pgsize must be a value from <pagesize_e>. */
   int    (*allocpage)     (struct pagecache_t* pgcache, uint8_t pgsize, /*out*/struct memblock_t* page);
   /* function: releasepage
    * Releases a single memory page. It is kept in the cache and only returned to
    * the operating system if a big chunk of memory is not in use.
    * After return page is set to <memblock_FREE>. Calling this function with
    * page set to <memblock_FREE> does nothing. */
   int    (*releasepage)   (struct pagecache_t* pgcache, struct memblock_t* page);
   /* function: sizeallocated
    * Returns the sum of the size of all allocated pages. */
   size_t (*sizeallocated) (const struct pagecache_t* pgcache);
   /* function: emptycache
    * Releases all unused memory blocks back to the operating system. */
   int    (*emptycache)    (struct pagecache_t* pgcache);
} pagecache_it;

// group: lifetime

/* define: pagecache_it_FREE
 * Static initializer. */
#define pagecache_it_FREE  \
         { 0, 0, 0, 0 }

/* define: pagecache_it_INIT
 * Static initializer. Set all function pointers to the provided values.
 *
 * Parameters:
 * allocpage_f     - Function pointer to allocate memory pages. See <pagecache_it.allocpage>.
 * releasepage_f   - Function pointer to release memory pages. See <pagecache_it.releasepage>.
 * sizeallocated_f - Function pointer to query the sum of the size of all allocated memory page. See <pagecache_it.sizeallocated>.
 * emptycache_f    - Function pointer to return unused memory blocks back to the OS. See <pagecache_it.emptycache>. */
#define pagecache_it_INIT( allocpage_f, releasepage_f, sizeallocated_f, emptycache_f)                                   \
         {  (allocpage_f), (releasepage_f), (sizeallocated_f), (emptycache_f) }

// group: generic

/* function: cast_pagecacheit
 * Casts pointer pgcacheif into pointer to interface <pagecache_it>.
 * Parameter *pgcacheif* must point to a type declared with <pagecache_IT>.
 * The other parameters must be the same as in <pagecache_IT> without the first. */
pagecache_it* cast_pagecacheit(void* pgcacheif, TYPENAME pagecache_t);

/* function: pagecache_IT
 * Declare generic type interface type with pagecache_t as first parameter.
 * The declared interface is structural compatible with <pagecache_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the first parameter.
 *
 * See <pagecache_it> for a list of declared functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * pagecache_t   - The type of the pagecache object which is the first parameter of all interface functions.
 * */
#define pagecache_IT(pagecache_t) \
   struct {                       \
      int    (*allocpage)     (pagecache_t* pgcache, uint8_t pgsize, /*out*/struct memblock_t* page);      \
      int    (*releasepage)   (pagecache_t* pgcache, struct memblock_t* page);                             \
      size_t (*sizeallocated) (const pagecache_t* pgcache);                                                \
      int    (*emptycache)    (pagecache_t* pgcache);                                                      \
   }



// section: inline implementation

// group: pagecache_t

/* define: allocpage_pagecache
 * Implements <pagecache_t.allocpage_pagecache>. */
#define allocpage_pagecache(pgcache, pgsize, page) \
         ((pgcache).iimpl->allocpage((pgcache).object, (pgsize), (page)))

/* define: emptycache_pagecache
 * Implements <pagecache_t.emptycache_pagecache>. */
#define emptycache_pagecache(pgcache)  \
         ((pgcache).iimpl->emptycache((pgcache).object))

/* define: isobject_pagecache
 * Implements <pagecache_t.isobject_pagecache>. */
#define isobject_pagecache(pgcache) \
         (0 != (pgcache)->object)

/* define: pagesizefrombytes_pagecache
 * Implements <pagecache_t.pagesizefrombytes_pagecache>. */
#define pagesizefrombytes_pagecache(size_in_bytes) \
         ( __extension__ ({               \
            size_t _sz = (size_in_bytes); \
            _sz /= 256;                   \
            _sz += (_sz == 0);            \
            unsigned _e = log2_int(_sz);  \
            (pagesize_e) _e;              \
         }))

/* define: pagesizeinbytes_pagecache
 * Implements <pagecache_t.pagesizeinbytes_pagecache>. */
static inline size_t pagesizeinbytes_pagecache(pagesize_e pagesize)
{
         return (size_t)256 << pagesize;
}

/* define: log2pagesizeinbytes_pagecache
 * Implements <pagecache_t.log2pagesizeinbytes_pagecache>. */
static inline uint8_t log2pagesizeinbytes_pagecache(pagesize_e pagesize)
{
         return (uint8_t) (8u + pagesize);
}

/* define: releasepage_pagecache
 * Implements <pagecache_t.releasepage_pagecache>. */
#define releasepage_pagecache(pgcache, page) \
         ((pgcache).iimpl->releasepage((pgcache).object, (page)))

/* define: sizeallocated_pagecache
 * Implements <pagecache_t.sizeallocated_pagecache>. */
#define sizeallocated_pagecache(pgcache) \
         ((pgcache).iimpl->sizeallocated((pgcache).object))

// group: pagecache_it

/* define: cast_pagecacheit
 * Implements <pagecache_it.cast_pagecacheit>. */
#define cast_pagecacheit(pgcacheif, pagecache_t) \
   ( __extension__ ({                                              \
      typeof(pgcacheif) _if;                                       \
      _if = (pgcacheif);                                           \
      static_assert(                                               \
         &(_if->allocpage)                                         \
         == (int (**) (pagecache_t*,uint8_t, struct memblock_t*))  \
            &((pagecache_it*) _if)->allocpage                      \
         && &(_if->releasepage)                                    \
            == (int (**) (pagecache_t*, struct memblock_t*))       \
               &((pagecache_it*) _if)->releasepage                 \
         && &(_if->sizeallocated)                                  \
            == (size_t (**) (const pagecache_t*))                  \
               &((pagecache_it*) _if)->sizeallocated               \
         && &(_if->emptycache)                                     \
            == (int (**) (pagecache_t*))                           \
               &((pagecache_it*) _if)->emptycache,                 \
         "ensure compatible structure");                           \
      (pagecache_it*) _if;                                         \
   }))

#endif
