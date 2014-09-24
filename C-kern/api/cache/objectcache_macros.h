/* title: Objectcache-Macros
   Makes <Objectcache> service more accessible with simple defined macros.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/cache/objectcache_macros.h
    Header file of <Objectcache-Macros>.
*/
#ifndef CKERN_CACHE_OBJECTCACHE_MACROS_HEADER
#define CKERN_CACHE_OBJECTCACHE_MACROS_HEADER

#include "C-kern/api/cache/objectcache.h"


// section: Functions

// group: iobuffer

/* define: LOCKIOBUFFER_OBJECTCACHE
 * Locks the io buffer and returns a pointer to it in iobuffer.
 * See also <lockiobuffer_objectcache>. */
#define LOCKIOBUFFER_OBJECTCACHE(/*out*/iobuffer) \
         (objectcache_maincontext().iimpl->lock_iobuffer(objectcache_maincontext().object, (iobuffer)))

/* define: UNLOCKIOBUFFER_OBJECTCACHE
 * Unlocks the locked io buffer and sets the pointer to NULL.
 * See also <unlockiobuffer_objectcache>. */
#define UNLOCKIOBUFFER_OBJECTCACHE(iobuffer) \
         (objectcache_maincontext().iimpl->unlock_iobuffer(objectcache_maincontext().object, (iobuffer)))


#endif