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

   file: C-kern/api/umg/objectcache_macros.h
    Header file of <Objectcache-Macros>.
*/
#ifndef CKERN_UMGEBUNG_OBJECTCACHE_MACROS_HEADER
#define CKERN_UMGEBUNG_OBJECTCACHE_MACROS_HEADER

#include "C-kern/api/aspect/interface/objectcache_it.h"

// section: Functions

// group: query protocol

/* define: OBJC_LOCKIOBUFFER
 * Locks the io buffer and returns a pointer to it in iobuffer.
 * See also <lockiobuffer_objectcache>.
 * > #define OBJC_LOCKIOBUFFER(iobuffer) \
 * >    (objectcache_umgebung().functable->lock_iobuffer(objectcache_umgebung().object, iobuffer))
 * */
#define OBJC_LOCKIOBUFFER(/*out*/iobuffer) \
      (objectcache_umgebung().functable->lock_iobuffer(objectcache_umgebung().object, (iobuffer)))

/* define: OBJC_UNLOCKIOBUFFER
 * Unlocks the locked io buffer and sets the pointer to NULL.
 * See also <unlockiobuffer_objectcache>.
 * > #define OBJC_UNLOCKIOBUFFER(iobuffer) \
 * >    (objectcache_umgebung().functable->unlock_iobuffer(objectcache_umgebung().object, iobuffer))
 * */
#define OBJC_UNLOCKIOBUFFER(iobuffer) \
   (objectcache_umgebung().functable->unlock_iobuffer(objectcache_umgebung().object, (iobuffer)))


#endif