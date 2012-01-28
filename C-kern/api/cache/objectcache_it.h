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

   file: C-kern/api/cache/objectcache_oit.h
    Contains interfaceable object <Objectcache-Object>.
*/
#ifndef CKERN_CACHE_INTERFACE_OBJECTCACHE_IT_HEADER
#define CKERN_CACHE_INTERFACE_OBJECTCACHE_IT_HEADER

#include "C-kern/api/memory/memblock.h"

// forward
struct objectcache_t ;

/* typedef: struct objectcache_it
 * Export interface (function table) <objectcache_it>.
 * See <objectcache_it_DECLARE> for adaption of the interface to a
 * specific imlpementation. */
typedef struct objectcache_it          objectcache_it ;

/* define: objectcache_it_DECLARE
 * Declares a function table for accessing an objectcache service.
 * Use this macro to define a interface which is structural compatible
 * with the generic interface <objectcache_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the object. Every interface function takes a pointer to this object type
 * as its first parameter.
 *
 * Parameter:
 * declared_it - The name of the structure which is declared as the functional objectcache interface.
 *               The name should have the suffix "_it".
 * object_t    - The type of the cache object which is the first parameter of all objectcache functions.
 *
 * List of declared functions:
 * lock_iobuffer    - See <objectcache_t.lockiobuffer_objectcache>
 * unlock_iobuffer  - See <objectcache_t.unlockiobuffer_objectcache>
 *
 * (start code)
 * #define objectcache_it_DECLARE(declared_it, object_t)                      \
 *    struct declared_it {                                                    \
 *       int (*lock_iobuffer)   (object_t * cache, memblock_t ** iobuffer) ;  \
 *       int (*unlock_iobuffer) (object_t * cache, memblock_t ** iobuffer) ;  \
 *    } ;
 * (end code)
 * */
#define objectcache_it_DECLARE(declared_it, object_t)                            \
   struct declared_it {                                                          \
      void (*lock_iobuffer)   (object_t * cache, /*out*/memblock_t ** iobuffer); \
      void (*unlock_iobuffer) (object_t * cache, memblock_t ** iobuffer) ;       \
   } ;


#if 0
/* struct: objectcache_it
 * The function table which describes the objectcache service. */
struct objectcache_it {
   /* function: lock_iobuffer
    * See <objectcache_t.lockiobuffer_objectcache> for an implementation. */
   void (*lock_iobuffer)   (struct objectcache_t * cache, /*out*/memblock_t ** iobuffer) ;
   /* function: unlock_iobuffer
    * See <objectcache_t.unlockiobuffer_objectcache> for an implementation. */
   void (*unlock_iobuffer) (struct objectcache_t * cache, memblock_t ** iobuffer) ;
} ;
#endif

objectcache_it_DECLARE(objectcache_it, struct objectcache_t)

#endif
