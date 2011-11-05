/* title: Services-Multithread
   Initializes <umgebung_services_t> with services which can be used
   by multiple threads simultaneously.

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

   file: C-kern/api/umg/services_multithread.h
    Header file of <Services-Multithread>.

   file: C-kern/umgebung/services_multithread.c
    Implementation file <Services-Multithread impl>.
*/
#ifndef CKERN_UMGEBUNG_SERVICES_MULTITHREAD_HEADER
#define CKERN_UMGEBUNG_SERVICES_MULTITHREAD_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_services_multithread
 * Test <initmultithread_umgebungservices> succeeds (see <umgebung_type_MULTITHREAD>). */
extern int unittest_umgebung_services_multithread(void) ;
#endif

// section: umgebung_services_t

// group: lifetime

/* function: initmultithread_umgebungservices
 * Creates all top level services in which are multi thread safe.
 * Is called from <init_umgebung> if type is set to <umgebung_type_MULTITHREAD>. */
extern int initmultithread_umgebungservices(/*out*/umgebung_services_t * svc) ;

/* function: freemultithread_umgebungservices
 * Frees all resources bound to the services.
 * Is called from <free_umgebung> if type is set to <umgebung_type_MULTITHREAD>. */
extern int freemultithread_umgebungservices(umgebung_services_t * svc) ;


// section: inline implementations

// group: KONFIG_SUBSYS

#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
/* define: initmultithread_umgebungservices
 * Implement <umgebung_services_t.initmultithread_umgebungservices> as a no op if !((KONFIG_SUBSYS)&THREAD) */
#define initmultithread_umgebungservices(svc)        (ENOSYS)
/* define: freemultithread_umgebungservices
 * Implement <umgebung_services_t.freemultithread_umgebungservices> as a no op if !((KONFIG_SUBSYS)&THREAD) */
#define freemultithread_umgebungservices(svc)        (ENOSYS)
#endif
#undef THREAD

#endif