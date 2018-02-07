/* title: ErrorNumbers impl

   Implements <ErrorNumbers>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/err/errornr.h
    Header file <ErrorNumbers>.

   file: C-kern/err/errornr.c
    Implementation file <ErrorNumbers impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err/errornr.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: errornr_e

// group: test

#ifdef KONFIG_UNITTEST

static inline void test_errornr(void)
{
   static_assert(256 == errornr_STATE_NOTINIT, "");
   static_assert(257 == errornr_STATE_INVARIANT, "");
   static_assert(258 == errornr_STATE_RESET, "");
   static_assert(259 == errornr_RESOURCE_ALLOCATE, "");
   static_assert(260 == errornr_RESOURCE_LEAK, "");
   static_assert(261 == errornr_RESOURCE_LEAK_MEMORY, "");
   static_assert(262 == errornr_PARSER_SYNTAX, "");
   static_assert(263 == errornr_NEXTERRORCODE, "");
}

static inline void test_defines(void)
{
   static_assert(ENOTINIT   == errornr_STATE_NOTINIT, "");
   static_assert(EINVARIANT == errornr_STATE_INVARIANT, "");
   static_assert(ERESET     == errornr_STATE_RESET, "");
   static_assert(EALLOC     == errornr_RESOURCE_ALLOCATE, "");
   static_assert(ELEAK      == errornr_RESOURCE_LEAK, "");
   static_assert(EMEMLEAK   == errornr_RESOURCE_LEAK_MEMORY, "");
   static_assert(ESYNTAX    == errornr_PARSER_SYNTAX, "");
}

static int test_errorstr(void)
{

#define CHECK(ERR,STR) TEST(0 == memcmp(str_errorcontext(error_maincontext(), ERR), STR, strlen(STR)+1))

   CHECK(ENOTINIT,   "Subsystem not yet initialized");
   CHECK(EINVARIANT, "Internal invariant violated - (software bug or corrupt memory)");
   CHECK(ERESET,     "Lost context state cause of power management event");
   CHECK(EALLOC,     "Failed to allocate one or more resources");
   CHECK(ELEAK,      "Resource(s) leaked");
   CHECK(EMEMLEAK,   "Not all memory freed");
   CHECK(ESYNTAX,    "Syntax error during parsing");

#undef CHECK

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_err_errornr()
{
   if (test_errorstr())    goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
