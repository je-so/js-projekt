/* title: Objectcache-Macros
   Makes <Objectcache> service more accessible with simple defined macros.

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

   file: C-kern/api/cache/objectcache_macros.h
    Header file of <Objectcache-Macros>.
*/
#ifndef CKERN_CACHE_OBJECTCACHE_MACROS_HEADER
#define CKERN_CACHE_OBJECTCACHE_MACROS_HEADER

#include "C-kern/api/cache/objectcache_it.h"


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