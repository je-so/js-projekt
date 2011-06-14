/* title: Valuecache-Aspect
   Describes set of cached precomputed values.

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

   file: C-kern/api/aspect/valuecache.h
    Header file of <Valuecache-Aspect>.
*/
#ifndef CKERN_ASPECT_VALUECACHE_HEADER
#define CKERN_ASPECT_VALUECACHE_HEADER

/* typedef: valuecache_aspect_t typedef
 * Exports <valuecache_aspect_t>. */
typedef struct valuecache_aspect_t     valuecache_aspect_t ;


/* struct: valuecache_aspect_t
 * Caches values which have to be computed only once. */
struct valuecache_aspect_t {
   /* variable: pagesize_vm
    * */
   size_t         pagesize_vm ;
} ;

#endif
