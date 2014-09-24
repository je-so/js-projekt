/* title: NewC-Parser impl

   Implements <NewC-Parser>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/newC/nCparser.h
    Header file <NewC-Parser>.

   file: C-kern/newC/nCparser.c
    Implementation file <NewC-Parser impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/newC/nCparser.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: nCparser_t

// group: lifetime


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   nCparser_t obj = nCparser_FREE;

   // TEST nCparser_FREE
   TEST(0 == obj.dummy);

   return 0;
ONERR:
   return EINVAL;
}

int unittest_newc_ncparser()
{
   if (test_initfree())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
