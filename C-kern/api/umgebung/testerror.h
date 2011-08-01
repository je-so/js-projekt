/* title: UmgebungTestError
   Allows to simulate an error during initialization.

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

   file: C-kern/api/umgebung/testerror.h
    Header file of <UmgebungTestError>.

   file: C-kern/umgebung/testerror.c
    Implementation file of <UmgebungTestError impl>.
*/
#ifndef CKERN_UMGEBUNG_TESTERROR_HEADER
#define CKERN_UMGEBUNG_TESTERROR_HEADER

// section: Functions

// group: initumgebung

/* function: initumgebung_umgebungtesterror
 * */
extern int initumgebung_umgebungtesterror(void) ;

/* function: freeumgebung_umgebungtesterror
 * */
extern int freeumgebung_umgebungtesterror(void) ;

// group: control returned error

/* function: cleariniterror_umgebungtesterror
 * Clears the returned erroe code. */
extern void cleariniterror_umgebungtesterror(void) ;

/* function: setiniterror_umgebungtesterror
 * Sets the error code returned by <initumgebung_umgebungtesterror>. */
extern void setiniterror_umgebungtesterror(int err) ;


// section: inline implementations

/* define: cleariniterror_umgebungtesterror inline
 * Implements <cleariniterror_umgebungtesterror>.
 * > setiniterror_umgebungtesterror(0) */
#define cleariniterror_umgebungtesterror() \
   setiniterror_umgebungtesterror(0)

#if !defined(KONFIG_UNITTEST)

/* define: freeumgebung_umgebungtesterror inline
 * Dummy for <freeumgebung_umgebungtesterror> in case KONFIG_UNITTEST not defined. */
#define freeumgebung_umgebungtesterror() \
   (0)

/* define: initumgebung_umgebungtesterror inline
 * Dummy for <initumgebung_umgebungtesterror> in case KONFIG_UNITTEST not defined. */
#define initumgebung_umgebungtesterror() \
   (0)

#endif

#endif
