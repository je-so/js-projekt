/* title: Services-Singlethread
   Initializes <umgebung_services_t> with services which can not be shared between threads.

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

   file: C-kern/api/umg/services_singlethread.h
    Header file of <Services-Singlethread>.

   file: C-kern/umgebung/services_singlethread.c
    Implementation file <Services-Singlethread impl>.
*/
#ifndef CKERN_UMGEBUNG_SERVICES_SINGLETHREAD_HEADER
#define CKERN_UMGEBUNG_SERVICES_SINGLETHREAD_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_services_singlethread
 * Test <initsinglethread_umgebungservices> succeeds (see <umgebung_type_SINGLETHREAD>). */
extern int unittest_umgebung_services_singlethread(void) ;
#endif


// section: umgebung_services_t

// group: lifetime

/* function: initsinglethread_umgebungservices
 * Creates all top level services in which are *not* multi thread safe.
 * Is called from <init_umgebung> if type is set to <umgebung_type_SINGLETHREAD>. */
extern int initsinglethread_umgebungservices(/*out*/umgebung_services_t * svc) ;

/* function: freesinglethread_umgebungservices
 * Frees all resources bound to this object.
 * Is called from <free_umgebung> if type is set to <umgebung_type_SINGLETHREAD>. */
extern int freesinglethread_umgebungservices(umgebung_services_t * svc) ;

#endif