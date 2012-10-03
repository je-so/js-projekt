/* title: Typeadapt impl

   Implements <Typeadapt>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/typeadapt.h
    Header file <Typeadapt>.

   file: C-kern/ds/typeadapt.c
    Implementation file <Typeadapt impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

typedef struct testadapt_t      testadapt_t ;
typedef struct testobject_t     testobject_t ;

struct testobject_t {
   struct {
      bool  is_newcopy ;
      bool  is_delete ;
   } lifetime ;
} ;

/* struct: testadapt_t
 * Implements adapter for type <testobject_t>. */
struct testadapt_t {
   struct {
      typeadapt_EMBED(testadapt_t, testobject_t) ;
   } ;
   int call_count ;
} ;

static int impl_newcopy_testadapt(testadapt_t * typeadp, /*out*/struct testobject_t ** destobject, const struct testobject_t * srcobject)
{
   *destobject = 0 ;
   CONST_CAST(testobject_t,srcobject)->lifetime.is_newcopy = true ;
   return (typeadp->call_count ++) ;
}

static int impl_delete_testadapt(testadapt_t * typeadp, struct testobject_t ** object)
{
   (*object)->lifetime.is_delete = true ;
   (*object) = 0 ;
   return (typeadp->call_count ++) ;
}


static int test_initfree(void)
{
   typeadapt_t typeadp = typeadapt_INIT_FREEABLE ;

   // TEST typeadapt_INIT_FREEABLE
   TEST(0 == typeadp.lifetime.newcopy_object) ;
   TEST(0 == typeadp.lifetime.delete_object) ;

   // TEST typeadapt_INIT
   typeadp = (typeadapt_t) typeadapt_INIT( (typeof(typeadp.lifetime.newcopy_object))1,
                                           (typeof(typeadp.lifetime.delete_object))2) ;
   TEST(typeadp.lifetime.newcopy_object == (typeof(typeadp.lifetime.newcopy_object))1) ;
   TEST(typeadp.lifetime.delete_object  == (typeof(typeadp.lifetime.delete_object))2) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_generic(void)
{
   testadapt_t    testadp      = { typeadapt_INIT(&impl_newcopy_testadapt, &impl_delete_testadapt), 0 } ;
   testobject_t   testobj[100] = { { .lifetime = {0,0} } } ;
   testobject_t   * objptr ;

   // TEST asgeneric_typeadapt
   TEST((typeadapt_t*)0        == asgeneric_typeadapt((testadapt_t*)0, testadapt_t, testobject_t)) ;
   TEST((typeadapt_t*)&testadp == asgeneric_typeadapt(&testadp, testadapt_t, testobject_t)) ;

   // TEST callnewcopy_typeadapt
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int result = testadp.call_count ;
      objptr = (testobject_t *)1 ;
      TEST(result == callnewcopy_typeadapt(&testadp, &objptr, &testobj[i])) ;
      TEST(0 == objptr) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      TEST(true  == testobj[i].lifetime.is_newcopy) ;
      TEST(false == testobj[i].lifetime.is_delete) ;
      testobj[i].lifetime.is_newcopy = false ;
   }

   // TEST calldelete_typeadapt
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int result = testadp.call_count ;
      objptr = &testobj[i] ;
      TEST(result == calldelete_typeadapt(&testadp, &objptr)) ;
      TEST(0 == objptr) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      TEST(false == testobj[i].lifetime.is_newcopy) ;
      TEST(true  == testobj[i].lifetime.is_delete) ;
      testobj[i].lifetime.is_delete = false ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
