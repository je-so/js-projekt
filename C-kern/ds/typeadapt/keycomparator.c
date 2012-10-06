/* title: Typeadapt-KeyComparator impl

   Implements <Typeadapt-KeyComparator>.

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

   file: C-kern/api/ds/typeadapt/keycomparator.h
    Header file <Typeadapt-KeyComparator>.

   file: C-kern/ds/typeadapt/keycomparator.c
    Implementation file <Typeadapt-KeyComparator impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt/keycomparator.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

typedef struct testobject_t            testobject_t ;
typedef struct testadapter_t           testadapter_t  ;

struct testadapter_t {
   int result ;
   const struct testobject_t  * lobject ;
   const struct testobject_t  * robject ;
   const int                  * lkey ;
   const struct testobject_t  * keyobject ;
} ;

static int impl_cmpkeyobject_testadapter(testadapter_t * typeadp, const int * lkey, const testobject_t * robject)
{
   typeadp->lkey    = lkey ;
   typeadp->robject = robject ;

   return typeadp->result ;
}

static int impl_cmpobject_testadapter(testadapter_t * typeadp, const testobject_t * lobject, const testobject_t * robject)
{
   typeadp->lobject = lobject ;
   typeadp->robject = robject ;

   return typeadp->result ;
}

static int impl_cmpkeyobject_typeadapt(struct typeadapt_t * typeadp, const void * key, const struct typeadapt_object_t * robject)
{
   return impl_cmpkeyobject_testadapter((testadapter_t*)typeadp, (const int*)key, (const testobject_t*)robject) ;
}

static int impl_cmpobject_typeadapt(struct typeadapt_t * typeadp, const struct typeadapt_object_t * lobject, const struct typeadapt_object_t * robject)
{
   return impl_cmpobject_testadapter((testadapter_t*)typeadp, (const testobject_t*)lobject, (const testobject_t*)robject) ;
}

static int test_initfree(void)
{
   typeadapt_keycomparator_it adpcmp = typeadapt_keycomparator_INIT_FREEABLE ;

   // TEST typeadapt_keycomparator_INIT_FREEABLE
   TEST(0 == adpcmp.cmp_key_object) ;
   TEST(0 == adpcmp.cmp_object) ;

   // TEST typeadapt_keycomparator_INIT: dummy values
   for (uintptr_t i = 0; i <= 8; ++i) {
      const uintptr_t incr = 1 + ((uintptr_t)-1) / 8u ;
      adpcmp = (typeadapt_keycomparator_it) typeadapt_keycomparator_INIT((typeof(adpcmp.cmp_key_object))(i*incr), (typeof(adpcmp.cmp_object))((8-i)*incr)) ;
      TEST(adpcmp.cmp_key_object == (typeof(adpcmp.cmp_key_object)) (i*incr)) ;
      TEST(adpcmp.cmp_object == (typeof(adpcmp.cmp_object)) ((8-i)*incr)) ;
   }

   // TEST typeadapt_keycomparator_INIT: real example
   adpcmp = (typeadapt_keycomparator_it) typeadapt_keycomparator_INIT(&impl_cmpkeyobject_typeadapt, &impl_cmpobject_typeadapt) ;
   TEST(adpcmp.cmp_key_object == &impl_cmpkeyobject_typeadapt) ;
   TEST(adpcmp.cmp_object == &impl_cmpobject_typeadapt) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_callfunctions(void)
{
   typeadapt_keycomparator_it adpcmp  = typeadapt_keycomparator_INIT(&impl_cmpkeyobject_typeadapt, &impl_cmpobject_typeadapt) ;
   testadapter_t              testadp = { .result = 0 } ;

   // TEST: callcmpkeyobj_typeadaptkeycomparator
   for (int result = -100; result <= 100; result += 100) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      for (uintptr_t i = 0; i <= 8; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.result = result ;
         TEST(result == callcmpkeyobj_typeadaptkeycomparator(&adpcmp, (struct typeadapt_t*)&testadp, (const void*)((8-i)*incr), (const struct typeadapt_object_t*)(i*incr))) ;
         TEST(testadp.lkey    == (const void*)  ((8-i)*incr)) ;
         TEST(testadp.robject == (const struct testobject_t*) (i*incr)) ;
      }
   }

   // TEST: callcmpobj_typeadaptkeycomparator
   for (int result = -1000; result <= 1000; result += 1000) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      for (uintptr_t i = 0; i <= 8; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.result = result ;
         TEST(result == callcmpobj_typeadaptkeycomparator(&adpcmp, (struct typeadapt_t*)&testadp, (const struct typeadapt_object_t*)(i*incr), (const struct typeadapt_object_t*)((8-i)*incr))) ;
         TEST(testadp.lobject == (const struct testobject_t*) (i*incr)) ;
         TEST(testadp.robject == (const struct testobject_t*) ((8-i)*incr)) ;
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

typeadapt_keycomparator_DECLARE(testadapter_it, testadapter_t, testobject_t, int) ;

static int test_generic(void)
{
   testadapter_t  testadp = { .result = 0 } ;
   testadapter_it adpcmp  = typeadapt_keycomparator_INIT_FREEABLE ;

   // TEST typeadapt_keycomparator_DECLARE
   static_assert(sizeof(testadapter_it) == sizeof(typeadapt_keycomparator_it), "structur compatible") ;
   static_assert(offsetof(testadapter_it, cmp_key_object) == offsetof(typeadapt_keycomparator_it, cmp_key_object), "structur compatible") ;
   static_assert(offsetof(testadapter_it, cmp_object) == offsetof(typeadapt_keycomparator_it, cmp_object), "structur compatible") ;

   // TEST asgeneric_typeadaptkeycomparator
   TEST((struct typeadapt_keycomparator_it*)&adpcmp == asgeneric_typeadaptkeycomparator(&adpcmp, testadapter_t, testobject_t, int)) ;

   // TEST typeadapt_keycomparator_INIT_FREEABLE
   TEST(0 == adpcmp.cmp_key_object) ;
   TEST(0 == adpcmp.cmp_object) ;

   // TEST typeadapt_keycomparator_INIT
   adpcmp = (testadapter_it) typeadapt_keycomparator_INIT(&impl_cmpkeyobject_testadapter, &impl_cmpobject_testadapter) ;
   TEST(adpcmp.cmp_key_object == &impl_cmpkeyobject_testadapter) ;
   TEST(adpcmp.cmp_object     == &impl_cmpobject_testadapter) ;

   // TEST callcmpobj_typeadaptkeycomparator, callcmpkeyobj_typeadaptkeycomparator
   for (int result = -10000; result <= 10000; result += 10000) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      for (uintptr_t i = 0; i <= 4; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.result = result ;
         TEST(result == callcmpkeyobj_typeadaptkeycomparator(&adpcmp, &testadp, (const int *)((4-i)*incr), (const testobject_t*)(i*incr))) ;
         TEST(testadp.lkey    == (const int*) ((4-i)*incr)) ;
         TEST(testadp.robject == (const testobject_t*) (i*incr)) ;
         TEST(result == callcmpobj_typeadaptkeycomparator(&adpcmp, &testadp, (const testobject_t*)((i+1)*incr), (const testobject_t*)((5-i)*incr))) ;
         TEST(testadp.lobject == (const testobject_t*) ((i+1)*incr)) ;
         TEST(testadp.robject == (const testobject_t*)  ((5-i)*incr)) ;
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt_keycomparator()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_callfunctions())  goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
