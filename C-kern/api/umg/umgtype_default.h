/* title: Default-Umgebung
   Initializes the default umgebung.

   Exports the functions:
   - <initdefault_umgebung>
   - <freedefault_umgebung>

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

   file: C-kern/api/umg/umgtype_default.h
    Header file of <Default-Umgebung>.

   file: C-kern/umgebung/umgtype_default.c
    Implementation file of <Default-Umgebung impl>.
*/
#ifndef CKERN_UMGEBUNG_UMGTYPE_DEFAULT_HEADER
#define CKERN_UMGEBUNG_UMGTYPE_DEFAULT_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_typedefault
 * Test initialization thread with default impl. succeeds. */
extern int unittest_umgebung_typedefault(void) ;
#endif

// section: umgebung_type_DEFAULT

// group: lifetime

/* function: initdefault_umgebung
 * Is called from <init_umgebung> if type is set to umgebung_type_DEFAULT. */
extern int initdefault_umgebung(/*out*/umgebung_t * umg) ;

/* function: freedefault_umgebung
 * Frees all resources bound to this object.
 * The <umgebung_t> must of type umgebung_type_DEFAULT. */
extern int freedefault_umgebung(umgebung_t * umg) ;

#endif