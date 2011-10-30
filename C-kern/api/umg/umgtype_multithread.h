/* title: Umgtype-Multithread
   Initializes <umgebung_t> with services which can be shared between threads
   ot used by multiple threads simultaneously.

   Exports the functions:
   - <initmultithread_umgebung>
   - <freemultithread_umgebung>

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

   file: C-kern/api/umg/umgtype_multithread.h
    Header file of <Umgtype-Multithread>.

   file: C-kern/umgebung/umgtype_multithread.c
    Implementation file of <Umgtype-Multithread impl>.
*/
#ifndef CKERN_UMGEBUNG_UMGTYPE_MULTITHREAD_HEADER
#define CKERN_UMGEBUNG_UMGTYPE_MULTITHREAD_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_typemultithread
 * Test initialization with <umgebung_type_MULTITHREAD> succeeds. */
extern int unittest_umgebung_typemultithread(void) ;
#endif

// group: init

/* function: initmultithread_umgebung
 * Is called from <init_umgebung> if type is set to umgebung_type_MULTITHREAD. */
extern int initmultithread_umgebung(/*out*/umgebung_t * umg, umgebung_shared_t * shared) ;

/* function: freemultithread_umgebung
 * Frees all resources bound to this object.
 * The <umgebung_t> must of type umgebung_type_MULTITHREAD. */
extern int freemultithread_umgebung(umgebung_t * umg) ;


// section: inline implementations

// group: KONFIG_SUBSYS

#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
/* define: initmultithread_umgebung
 * Implement <umgebung_type_MULTTHREAD.initmultithread_umgebung> as a no op if !((KONFIG_SUBSYS)&THREAD) */
#define initmultithread_umgebung(umg, shared)      (ENOSYS)
/* define: freemultithread_umgebung
 * Implement <umgebung_type_MULTTHREAD.freemultithread_umgebung> as a no op if !((KONFIG_SUBSYS)&THREAD) */
#define freemultithread_umgebung(umg)              (ENOSYS)
#endif
#undef THREAD

#endif