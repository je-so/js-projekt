/* title: UmgebungTestError impl
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

   file: C-kern/api/umg/testerror.h
    Header file of <UmgebungTestError>.

   file: C-kern/umgebung/testerror.c
    Implementation file of <UmgebungTestError impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umg/testerror.h"

#ifdef KONFIG_UNITTEST

/* variable: s_init_error
 * Is returned by initumgebung_umgebungtesterror.
 * You can change this value with <cleariniterror_umgebungtesterror> or <setiniterror_umgebungtesterror>. */
static int s_init_error = 0 ;

/* variable: s_free_error
 * Is returned by freeumgebung_umgebungtesterror. */
static int s_free_error = 0 ;

int initumgebung_umgebungtesterror()
{
   return s_init_error ;
}

int freeumgebung_umgebungtesterror()
{
   return s_free_error ;
}

void setiniterror_umgebungtesterror(int err)
{
   s_init_error = err ;
}

#endif
