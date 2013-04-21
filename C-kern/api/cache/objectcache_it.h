/* title: Objectcache-Interface

   Interface functions to access cached objects.
   Implemented by <objectcache_t>.

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

   file: C-kern/api/cache/objectcache_it.h
    Header file of <Objectcache-Interface>.

   file: C-kern/api/cache/objectcache.h
    Contains implementation object <Objectcache-Object>.
*/
#ifndef CKERN_CACHE_INTERFACE_OBJECTCACHE_IT_HEADER
#define CKERN_CACHE_INTERFACE_OBJECTCACHE_IT_HEADER

// forward
struct objectcache_t ;
struct memblock_t ;

/* typedef: struct objectcache_it
 * Export interface (function table) <objectcache_it>.
 * See <objectcache_it_DECLARE> for adaption of the interface to a
 * specific implementation. */
typedef struct objectcache_it          objectcache_it ;


/* struct: objectcache_it
 * The function table which describes the objectcache service. */
struct objectcache_it {
   /* function: lock_iobuffer
    * See <objectcache_impl_t.lockiobuffer_objectcacheimpl> for an implementation. */
   void (*lock_iobuffer)   (struct objectcache_t * cache, /*out*/struct memblock_t ** iobuffer) ;
   /* function: unlock_iobuffer
    * See <objectcache_impl_t.unlockiobuffer_objectcacheimpl> for an implementation. */
   void (*unlock_iobuffer) (struct objectcache_t * cache, struct memblock_t ** iobuffer) ;
} ;

// group: generic

/* define: genericcast_objectcacheit
 * Casts first parameter into pointer to <objectcache_it>.
 * The first parameter has to point to a type declared with <objectcache_it_DECLARE>.
 * The second must be the same as in <objectcache_it_DECLARE>. */
objectcache_it * genericcast_objectcacheit(void * cache, TYPENAME objectcache_t) ;

/* define: objectcache_it_DECLARE
 * Declares a function table for accessing an objectcache service.
 * Use this macro to define an interface which is structural compatible
 * with the generic interface <objectcache_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the first parameter.
 *
 * See <objectcache_it> for a list of declared functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the functional objectcache interface.
 *                 The name should have the suffix "_it".
 * objectcache_t - The type of the cache object which is the first parameter of all objectcache functions.
 * */
void objectcache_it_DECLARE(TYPENAME declared_it, TYPENAME objectcache_t) ;



// section: inline implementation

/* define: genericcast_objectcacheit
 * Implements <objectcache_it.genericcast_objectcacheit>. */
#define genericcast_objectcacheit(cache, objectcache_t)        \
   ( __extension__ ({                                          \
      static_assert(                                           \
         offsetof(objectcache_it, lock_iobuffer)               \
         == offsetof(typeof(*(cache)), lock_iobuffer)          \
         && offsetof(objectcache_it, unlock_iobuffer)          \
            == offsetof(typeof(*(cache)), unlock_iobuffer),    \
         "ensure same structure") ;                            \
      if (0) {                                                 \
         (cache)->lock_iobuffer((objectcache_t*)0,             \
                     (struct memblock_t**)0) ;                 \
         (cache)->unlock_iobuffer((objectcache_t*)0,           \
                     (struct memblock_t**)0) ;                 \
      }                                                        \
      (objectcache_it*) (cache) ;                              \
   }))

/* define: objectcache_it_DECLARE
 * Implements <objectcache_it.objectcache_it_DECLARE>. */
#define objectcache_it_DECLARE(declared_it, objectcache_t)  \
   typedef struct declared_it          declared_it ;        \
   struct declared_it {                                     \
      void (*lock_iobuffer)   (objectcache_t * cache, /*out*/struct memblock_t ** iobuffer); \
      void (*unlock_iobuffer) (objectcache_t * cache, struct memblock_t ** iobuffer) ;       \
   }

#endif
