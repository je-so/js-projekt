/* title: ErrorNumbers

   Contains list of application specific error numbers.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/context/errornr.h
    Header file <ErrorNumbers>.

   file: C-kern/context/errornr.c
    Implementation file <ErrorNumbers impl>.
*/
#ifndef CKERN_CONTEXT_ERRORNR_HEADER
#define CKERN_CONTEXT_ERRORNR_HEADER

/* enums: errornr_e
 * Application specific error codes.
 *
 * EINVARIANT - Invariant violated. This means for example corrupt memory, a software bug.
 * ELEAK      - Resource leak. This means for example memory not freed, file descriptors not closed and so on.
 * */
enum errornr_e {
   errornr_FIRSTERRORCODE = 256,
   errornr_INVARIANT      = errornr_FIRSTERRORCODE,
   errornr_LEAK,
   errornr_NEXTERRORCODE
} ;

typedef enum errornr_e  errornr_e;

#define EINVARIANT   errornr_INVARIANT
#define ELEAK        errornr_LEAK


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_errornr
 * Test assignment of <errornr_e>. */
int unittest_context_errornr(void) ;
#endif


#endif
