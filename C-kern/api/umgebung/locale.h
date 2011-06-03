/* title: UmgebungLocale
   Supports setting and getting of process locale (C runtime libraries).

   Use it to adapt to different character encodings / environments setings.

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

   file: C-kern/api/umgebung/locale.h
    Header file of <UmgebungLocale>.

   file: C-kern/umgebung/locale.c
    Implementation file <UmgebungLocale impl>.
*/
#ifndef CKERN_UMGEBUNG_LOCALE_HEADER
#define CKERN_UMGEBUNG_LOCALE_HEADER

#include "C-kern/api/os/locale.h"

// section: Functions

// group: init

/* function: initprocess_locale
 * Changes the current locale of the process to the value of the user
 * locale. The value is read from the environment variables LC_ALL or LANG.
 * The value of the first defined is used. */
extern int initprocess_locale(void) ;

/* function: freeprocess_locale
 * Resets the process locale to the standard "C" locale which
 * active per default if a new process enters its main function. */
extern int freeprocess_locale(void) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_locale
 * Test init- and freeprocess_locale succeeds. */
extern int unittest_umgebung_locale(void) ;
#endif

#endif
