/* title: Umgebung-Shared
   Defines top level context <umgebung_shared_t> which is shared
   between all threads.

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

   file: C-kern/api/umg/umgebung_shared.h
    Header file of <Umgebung-Shared>.

   file: C-kern/umgebung/umgebung_shared.c
    Implementation file <Umgebung-Shared impl>.
*/
#ifndef CKERN_UMGEBUNG_UMGEBUNG_SHARED_HEADER
#define CKERN_UMGEBUNG_UMGEBUNG_SHARED_HEADER

// forward
struct valuecache_t ;

/* typedef: umgebung_shared_t typedef
 * Export <umgebung_shared_t>. */
typedef struct umgebung_shared_t       umgebung_shared_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_shared
 * Test initialization process succeeds and global variables are set correctly. */
extern int unittest_umgebung_shared(void) ;
#endif


/* struct: umgebung_shared_t
 * Defines computing environment / context which is shared
 * between all threads of computation.
 * It contains e.g. services which offfer read only values or services which
 * can be implemented with use of global locks. */
struct umgebung_shared_t {
   struct valuecache_t        * valuecache ;
} ;

// group: lifetime

/* define: umgebung_shared_INIT_FREEABLE
 * Sttic initializer. */
#define umgebung_shared_INIT_FREEABLE     { 0 }

// group: lifetime

/* function: init_umgebungshared
 * Is called from <init_umgebung> if the first <umgebung_t> is created. */
extern int init_umgebungshared(umgebung_shared_t * shared) ;

/* function: free_umgebungshared
 * Frees resources associated with <umgebung_shared_t>.
 * This function is called in <free_umgebung> if the last object of type <umgebung_t>
 * is freed. */
extern int free_umgebungshared(umgebung_shared_t * shared) ;


#endif