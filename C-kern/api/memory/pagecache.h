/* title: PageCache-Object

   Offers object and interface for allocating pages of memory.

   Do not use the interface directly instead include <PageCacheMacros>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/pagecache.h
    Header file <PageCache-Object>.

   file: C-kern/memory/pagecache.c
    Implementation file <PageCache-Object impl>.
*/
#ifndef CKERN_MEMORY_PAGECACHE_HEADER
#define CKERN_MEMORY_PAGECACHE_HEADER

// forward
struct memblock_t ;

/* typedef: struct pagecache_t
 * Export <pagecache_t> into global namespace. */
typedef struct pagecache_t                pagecache_t ;

/* typedef: struct pagecache_it
 * Export <pagecache_it> into global namespace. */
typedef struct pagecache_it               pagecache_it ;

/* enums: pagesize_e
 * List of supported page sizes.
 * pagesize_256   - Request page size of 256 byte aligned to a 256 byte boundary.
 * pagesize_1024  - Request page size of 1024 byte aligned to a 1024 byte boundary.
 * pagesize_4096  - Request page size of 4096 byte aligned to a 4096 byte boundary.
 * pagesize_16384 - Request page size of 16384 byte aligned to a 16384 byte boundary.
 * */
enum pagesize_e {
   pagesize_256,
   pagesize_1024,
   pagesize_4096,
   pagesize_16384,
   pagesize_NROFPAGESIZE
} ;

typedef enum pagesize_e                   pagesize_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_pagecache
 * Test functionality of <pagecache_t> and <pagecache_it>. */
int unittest_memory_pagecache(void) ;
#endif



/* struct: pagecache_t
 * An object exporting interface <pagecache_it>.
 * The object implements caching of memory pages.
 * See also <pagecache_impl_t> which is the default implementation. */
struct pagecache_t {
   /* variable: object
    * A pointer to the object which implements the interface <pagecache_it>.
    * The type <pagecache_t> is casted into a custom type in the implementation
    * of the interface <pagecache_it>. */
   pagecache_t *     object ;
   /* variable: iimpl
    * A pointer to the implementation of interface <pagecache_it>. */
   pagecache_it *    iimpl ;
} ;

// group: lifetime

/* define: pagecache_INIT_FREEABLE
 * Static initializer. */
#define pagecache_INIT_FREEABLE  \
         pagecache_INIT(0,0)

/* define: pagecache_INIT
 * Static initializer. */
#define pagecache_INIT(object, iimpl)  \
         { (object), (iimpl) }


/* struct: pagecache_it
 * Interface which allows to allocate and relase pages of memory. */
struct pagecache_it {
   /* function: allocpage
    * Allocates a single memory page of size pgsize.
    * The page is aligned to its own size. */
   int    (*allocpage)     (pagecache_t * pgcache, pagesize_e pgsize, /*out*/struct memblock_t * page) ;
   /* function: releasepage
    * Releases a single memory page. It is kept in the cache and only returned to
    * the operating system if a big chunk of memory is not in use.
    * After return page is set to <memblock_INIT_FREEABLE>. Calling this function with
    * page set to <memblock_INIT_FREEABLE> does nothing. */
   int    (*releasepage)   (pagecache_t * pgcache, struct memblock_t * page) ;
   /* function: sizeallocated
    * Returns the sum of the size of all allocated pages. */
   size_t (*sizeallocated) (const pagecache_t * pgcache) ;
   /* function: allocstatic
    * Allocates static memory which should live as long as pgcache.
    * These blocks can only be freed by freeing pgcache.
    * The size of memory is set in bytes with parameter bytesize.
    * The allocated memory block (aligned to <KONFIG_MEMALIGN>) is returned in memblock.
    * In case of no memory ENOMEM is returned. The size of the block if restricted to 128 bytes. */
   int    (*allocstatic)   (pagecache_t * pgcache, size_t bytesize, /*out*/struct memblock_t * memblock) ;
   /* function: freestatic
    * Frees static memory.
    * If this function is not called in the reverse order of the call sequence of <allocstatic_pagecacheimpl>
    * the value EINVAL is returned and nothing is done.
    * After return memblock is set to <memblock_INIT_FREEABLE>.
    * Calling this function with memblock set to <memblock_INIT_FREEABLE> does nothing. */
   int    (*freestatic)    (pagecache_t * pgcache, struct memblock_t * memblock) ;
   /* function: sizestatic
    * Size of memory allocated with <allocstatic>. */
   size_t (*sizestatic)    (const pagecache_t * pgcache) ;
} ;

// group: lifetime

/* define: pagecache_it_INIT_FREEABLE
 * Static initializer. */
#define pagecache_it_INIT_FREEABLE  \
         { 0, 0, 0, 0, 0, 0 }

/* define: pagecache_it_INIT_FREEABLE
 * Static initializer. Set all function pointers to the provided values.
 *
 * Parameters:
 * allocpage_f     - Function pointer to allocate memory pages. See <pagecache_it.allocpage>.
 * releasepage_f   - Function pointer to release memory pages. See <pagecache_it.releasepage>.
 * sizeallocated_f - Function pointer to query the sum of the size of all allocated memory page. See <pagecache_it.sizeallocated>.
 * allocstatic_f   - Function pointer to allocate static blocks of memory. See <pagecache_it.allocstatic>.
 * freestatic_f    - Function pointer to free static blocks of memory. See <pagecache_it.freestatic>.
 * sizestatic_f    - Function pointer to query size of static allocated memory. See <pagecache_it.sizestatic>. */
#define pagecache_it_INIT(allocpage_f, releasepage_f, sizeallocated_f, allocstatic_f, sizestatic_f, freestatic_f) \
         { (allocpage_f), (releasepage_f), (sizeallocated_f), (allocstatic_f), (sizestatic_f), (freestatic_f) }

// group: generic

/* function: genericcast_pagecacheit
 * Casts pointer pgcacheif into pointer to interface <pagecache_it>.
 * Parameter *pgcacheif* must point to a type declared with <pagecache_it_DECLARE>.
 * The other parameters must be the same as in <pagecache_it_DECLARE> without the first. */
pagecache_it * genericcast_pagecacheit(void * pgcacheif, TYPENAME pagecache_t) ;

/* function: pagecache_it_DECLARE
 * Declares an interface function table for accessing pagecache.
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
void pagecache_it_DECLARE(TYPENAME declared_it, TYPENAME pagecache_t) ;



// section: inline implementation

/* define: genericcast_pagecacheit
 * Implements <pagecache_it.genericcast_pagecacheit>. */
#define genericcast_pagecacheit(pgcacheif, pagecache_t)     \
   ( __extension__ ({                                       \
      static_assert(                                        \
         offsetof(typeof(*(pgcacheif)), allocpage)          \
         == offsetof(pagecache_it, allocpage)               \
         && offsetof(typeof(*(pgcacheif)), releasepage)     \
            == offsetof(pagecache_it, releasepage)          \
         && offsetof(typeof(*(pgcacheif)), sizeallocated)   \
            == offsetof(pagecache_it, sizeallocated)        \
         && offsetof(typeof(*(pgcacheif)), allocstatic)     \
            == offsetof(pagecache_it, allocstatic)          \
         && offsetof(typeof(*(pgcacheif)), freestatic)      \
            == offsetof(pagecache_it, freestatic)           \
         && offsetof(typeof(*(pgcacheif)), sizestatic)      \
            == offsetof(pagecache_it, sizestatic)           \
         && sizeof(int)                                     \
            == sizeof((pgcacheif)->allocpage(               \
               (pagecache_t*)0, pagesize_16384,             \
               (struct memblock_t*)0))                      \
         && sizeof(int)                                     \
            == sizeof((pgcacheif)->releasepage(             \
               (pagecache_t*)0, (struct memblock_t*)0))     \
         && sizeof(size_t)                                  \
            == sizeof((pgcacheif)->sizeallocated(           \
                        (const pagecache_t*)0))             \
         && sizeof(int)                                     \
            == sizeof((pgcacheif)->allocstatic(             \
                     (pagecache_t*)0, (size_t)0,            \
                     (struct memblock_t*)0))                \
         && sizeof(int)                                     \
            == sizeof((pgcacheif)->freestatic(              \
                     (pagecache_t*)0,                       \
                     (struct memblock_t*)0))                \
         && sizeof(size_t)                                  \
            == sizeof((pgcacheif)->sizestatic(              \
                        (const pagecache_t*)0)),            \
         "ensure same structure") ;                         \
      if (0) {                                              \
         int      _err ;                                    \
         size_t   _err2 ;                                   \
         _err = (pgcacheif)->allocpage(                     \
               (pagecache_t*)0, pagesize_16384,             \
               (struct memblock_t*)0) ;                     \
         _err += (pgcacheif)->releasepage(                  \
               (pagecache_t*)0, (struct memblock_t*)0) ;    \
         _err2 = (pgcacheif)->sizeallocated(                \
                        (const pagecache_t*)0) ;            \
         _err += (pgcacheif)->allocstatic(                  \
               (pagecache_t*)0, (size_t)128,                \
               (struct memblock_t *)0) ;                    \
         _err += (pgcacheif)->freestatic(                   \
               (pagecache_t*)0,                             \
               (struct memblock_t *)0) ;                    \
         _err2 += (pgcacheif)->sizestatic(                  \
                        (const pagecache_t*)0) ;            \
         _err2 += (size_t)_err ;                            \
         (void) _err2 ;                                     \
      }                                                     \
      (pagecache_it*) (pgcacheif) ;                         \
   }))

/* define: pagecache_it_DECLARE
 * Implements <pagecache_it.pagecache_it_DECLARE>. */
#define pagecache_it_DECLARE(declared_it, pagecache_t)      \
   typedef struct declared_it       declared_it ;           \
   struct declared_it {                                     \
      int    (*allocpage)     (pagecache_t * pgcache, pagesize_e pgsize, /*out*/struct memblock_t * page) ;    \
      int    (*releasepage)   (pagecache_t * pgcache, struct memblock_t * page) ;                              \
      size_t (*sizeallocated) (const pagecache_t * pgcache) ;                                                  \
      int    (*allocstatic)   (pagecache_t * pgcache, size_t bytesize, /*out*/struct memblock_t * memblock) ;  \
      int    (*freestatic)    (pagecache_t * pgcache, struct memblock_t * memblock) ;                          \
      size_t (*sizestatic)    (const pagecache_t * pgcache) ;                                                  \
   } ;


#endif
