/* title: Test-Umgebung
   Initializes a test umgebung.

   Exports the functions:
   - <inittest_umgebung>
   - <freetest_umgebung>

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

   file: C-kern/api/umg/umgtype_test.h
    Header file of <Test-Umgebung>.

   file: C-kern/umgebung/umgtype_test.c
    Implementation file <Test-Umgebung impl>.
*/
#ifndef CKERN_UMGEBUNG_UMGTYPE_TEST_HEADER
#define CKERN_UMGEBUNG_UMGTYPE_TEST_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_typetest
 * Test initialization thread with default impl. succeeds. */
extern int unittest_umgebung_typetest(void) ;
#endif

// section: umgebung_type_TEST

// group: lifetime

/* function: inittest_umgebung
 * Is called from <init_umgebung> if type is set to umgebung_type_TEST. */
extern int inittest_umgebung(/*out*/umgebung_t * umg, umgebung_shared_t * shared) ;

/* function: freetest_umgebung
 * Frees all resources bound to this object.
 * The <umgebung_t> must of type umgebung_type_TEST. */
extern int freetest_umgebung(umgebung_t * umg) ;


// section: inline implementations

#if !defined(KONFIG_UNITTEST)

/* define: inittest_umgebung
 * Dummy for <umgebung_type_TEST.inittest_umgebung> in case KONFIG_UNITTEST not defined. */
#define inittest_umgebung(umg,shared) \
   (ENOSYS)

/* define: freetest_umgebung
 * Dummy for <umgebung_type_TEST.freetest_umgebung> in case KONFIG_UNITTEST not defined. */
#define freetest_umgebung(umg) \
   (ENOSYS)

#endif

#endif
