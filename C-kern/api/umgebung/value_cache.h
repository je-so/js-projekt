/* title: ValueCache
   Offers a simple cache mechanism for objects
   needed in submodules either often or which are costly
   to construct or deconstruct.

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

   file: C-kern/api/umgebung/value_cache.h
    Header file of <ValueCache>.

   file: C-kern/umgebung/value_cache.c
    Implementation file <ValueCache impl>.
*/
#ifndef CKERN_UMGEBUNG_VALUECACHE_HEADER
#define CKERN_UMGEBUNG_VALUECACHE_HEADER

#include "C-kern/api/aspect/valuecache.h"

// value_cache_t already defined in umgebung.h;

extern value_cache_t g_main_valuecache ;

// section: Functions

// group: init

/* function: initprocess_valuecache
 * Creates an internal value cache singleton object. */
extern int initprocess_valuecache(void) ;

/* function: freeprocess_locale
 * Frees internally created value cache singleton object. */
extern int freeprocess_valuecache(void) ;

/* function: initumgebung_valuecache
 * Sets valuecache pointer to a singleton object. */
extern int initumgebung_valuecache(/*out*/value_cache_t ** valuecache) ;

/* function: freeumgebung_valuecache
 * Resets the pointer to null. Singleton is never freed. */
extern int freeumgebung_valuecache(value_cache_t ** valuecache) ;


#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_valuecache
 * Test allocation and free works. */
extern int unittest_umgebung_valuecache(void) ;
#endif



#endif
