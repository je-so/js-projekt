/* title: InterfaceableObject impl

   Implements <InterfaceableObject>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/context/iobj.h
    Header file <InterfaceableObject>.

   file: C-kern/context/iobj.c
    Implementation file <InterfaceableObject impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/context/iobj.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: iobj_t

// group: test

#ifdef KONFIG_UNITTEST

/* function: test_initfree
 * Tests lifetime functions of <iobj_t>. */
static int test_initfree(void)
{
   iobj_t iobj = iobj_FREE;

   // TEST iobj_FREE
   TEST(iobj.object == 0);
   TEST(iobj.iimpl  == 0);

   // TEST iobj_INIT
   iobj = (iobj_t) iobj_INIT((iobj_t*)1, (iobj_it*)2);
   TEST(iobj.object == (iobj_t*)1);
   TEST(iobj.iimpl  == (iobj_it*)2);

   // TEST init_iobj
   init_iobj(&iobj, (iobj_t*)3, (iobj_it*)4);
   TEST(iobj.object == (iobj_t*)3);
   TEST(iobj.iimpl  == (iobj_it*)4);

   // TEST free_iobj
   free_iobj(&iobj);
   TEST(iobj.object == 0);
   TEST(iobj.iimpl  == 0);

   return 0;
ONERR:
   return EINVAL;
}

/* function: test_generic
 * Tests generic functions of <iobj_t>. */
static int test_generic(void)
{
   struct testiob2_t {
      struct testiobj_t*  object;
      struct testiobj_it* iimpl;
      int _extra_field; // extra fields at end do not harm
   } testiobj2;

   // TEST iobj_T
   typedef iobj_T(testiobj) testiobj_t;
   testiobj_t testiobj1;

   // TEST cast_iobj
   TEST(cast_iobj(&testiobj1, testiobj) == &testiobj1);
   TEST(cast_iobj(&testiobj2, testiobj) == (testiobj_t*) &testiobj2);

   // TEST init_iobj
   init_iobj(&testiobj1, (struct testiobj_t*)1, (struct testiobj_it*)2);
   init_iobj(&testiobj2, (struct testiobj_t*)3, (struct testiobj_it*)4);
   TEST(testiobj1.object == (struct testiobj_t*)1);
   TEST(testiobj1.iimpl  == (struct testiobj_it*)2);
   TEST(testiobj2.object == (struct testiobj_t*)3);
   TEST(testiobj2.iimpl  == (struct testiobj_it*)4);

   // TEST initcopy_iobj
   struct testiob2_t testiobj3 = testiobj2;
   initcopy_iobj(&testiobj2, &testiobj1);
   TEST(testiobj2.object == (struct testiobj_t*)1);
   TEST(testiobj2.iimpl  == (struct testiobj_it*)2);
   initcopy_iobj(&testiobj1, &testiobj3);
   TEST(testiobj1.object == (struct testiobj_t*)3);
   TEST(testiobj1.iimpl  == (struct testiobj_it*)4);

   // TEST free_iobj
   free_iobj(&testiobj1);
   free_iobj(&testiobj2);
   TEST(testiobj1.object == 0);
   TEST(testiobj1.iimpl  == 0);
   TEST(testiobj2.object == 0);
   TEST(testiobj2.iimpl  == 0);

   return 0;
ONERR:
   return EINVAL;
}

int unittest_context_iobj()
{
   if (test_initfree())       goto ONERR;
   if (test_generic())        goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
