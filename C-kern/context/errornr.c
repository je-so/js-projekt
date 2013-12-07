/* title: ErrorNumbers impl

   Implements <ErrorNumbers>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/context/errornr.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: errornr_e

// group: test

#ifdef KONFIG_UNITTEST

static int test_errornr(void)
{
   static_assert(256 == errornr_FIRSTERRORCODE, "");
   static_assert(256 == errornr_INVARIANT, "");
   static_assert(257 == errornr_LEAK, "");
   static_assert(258 == errornr_NEXTERRORCODE, "");

   return 0 ;
}

static int test_defines(void)
{
   static_assert(EINVARIANT == errornr_INVARIANT, "");
   static_assert(ELEAK      == errornr_LEAK, "");

   return 0 ;
}

static int test_errorstr(void)
{

#define CHECK(ERR,STR) TEST(0 == memcmp(str_errorcontext(error_maincontext(), ERR), STR, strlen(STR)+1))

   CHECK(EINVARIANT, "Internal invariant violated - (software bug or corrupt memory)");
   CHECK(ELEAK,      "Resource(s) leaked");

#undef CHECK

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_context_errornr()
{
   if (test_errornr())     goto ONABORT ;
   if (test_defines())     goto ONABORT ;
   if (test_errorstr())    goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
