/* title: Typeadapt-Comparator impl

   Implements <Typeadapt-Comparator>.

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

   file: C-kern/api/ds/typeadapt/comparator.h
    Header file <Typeadapt-Comparator>.

   file: C-kern/ds/typeadapt/comparator.c
    Implementation file <Typeadapt-Comparator impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt/comparator.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: typeadapt_comparator_it

// group: query

bool isequal_typeadaptcomparator(const typeadapt_comparator_it * ladpcmp, const typeadapt_comparator_it * radpcmp)
{
   return   ladpcmp->cmp_key_object == radpcmp->cmp_key_object
            && ladpcmp->cmp_object == radpcmp->cmp_object ;
}

// group: test

#ifdef KONFIG_UNITTEST

typedef struct testobject_t            testobject_t ;
typedef struct testadapter_t           testadapter_t  ;

struct testadapter_t {
   int result ;
   const struct testobject_t  * lobject ;
   const struct testobject_t  * robject ;
   uintptr_t                  lkey ;
   const struct testobject_t  * keyobject ;
} ;

static int impl_cmpkeyobject_testadapter(testadapter_t * typeadp, uintptr_t lkey, const testobject_t * robject)
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
   return impl_cmpkeyobject_testadapter((testadapter_t*)typeadp, (uintptr_t)key, (const testobject_t*)robject) ;
}

static int impl_cmpobject_typeadapt(struct typeadapt_t * typeadp, const struct typeadapt_object_t * lobject, const struct typeadapt_object_t * robject)
{
   return impl_cmpobject_testadapter((testadapter_t*)typeadp, (const testobject_t*)lobject, (const testobject_t*)robject) ;
}

static int test_initfree(void)
{
   typeadapt_comparator_it adpcmp = typeadapt_comparator_FREE ;

   // TEST typeadapt_comparator_FREE
   TEST(0 == adpcmp.cmp_key_object) ;
   TEST(0 == adpcmp.cmp_object) ;

   // TEST typeadapt_comparator_INIT: dummy values
   for (uintptr_t i = 0; i <= 8; ++i) {
      const uintptr_t incr = 1 + ((uintptr_t)-1) / 8u ;
      adpcmp = (typeadapt_comparator_it) typeadapt_comparator_INIT((typeof(adpcmp.cmp_key_object))(i*incr), (typeof(adpcmp.cmp_object))((8-i)*incr)) ;
      TEST(adpcmp.cmp_key_object == (typeof(adpcmp.cmp_key_object)) (i*incr)) ;
      TEST(adpcmp.cmp_object == (typeof(adpcmp.cmp_object)) ((8-i)*incr)) ;
   }

   // TEST typeadapt_comparator_INIT: real example
   adpcmp = (typeadapt_comparator_it) typeadapt_comparator_INIT(&impl_cmpkeyobject_typeadapt, &impl_cmpobject_typeadapt) ;
   TEST(adpcmp.cmp_key_object == &impl_cmpkeyobject_typeadapt) ;
   TEST(adpcmp.cmp_object == &impl_cmpobject_typeadapt) ;

   // TEST isequal_typeadaptcomparator
   typeadapt_comparator_it adpcmp2 ;
   for (unsigned i = 0; i < sizeof(typeadapt_comparator_it)/sizeof(void*); ++i) {
      *(((void**)&adpcmp) +i) = (void*)i ;
      *(((void**)&adpcmp2)+i) = (void*)i ;
   }
   TEST(1 == isequal_typeadaptcomparator(&adpcmp, &adpcmp2)) ;
   TEST(1 == isequal_typeadaptcomparator(&adpcmp2, &adpcmp)) ;
   for (unsigned i = 0; i < sizeof(typeadapt_comparator_it)/sizeof(void*); ++i) {
      *(((void**)&adpcmp2)+i) = (void*)(1+i) ;
      TEST(0 == isequal_typeadaptcomparator(&adpcmp, &adpcmp2)) ;
      TEST(0 == isequal_typeadaptcomparator(&adpcmp2, &adpcmp)) ;
      *(((void**)&adpcmp2)+i) = (void*)i ;
      TEST(1 == isequal_typeadaptcomparator(&adpcmp, &adpcmp2)) ;
      TEST(1 == isequal_typeadaptcomparator(&adpcmp2, &adpcmp)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_callfunctions(void)
{
   typeadapt_comparator_it adpcmp  = typeadapt_comparator_INIT(&impl_cmpkeyobject_typeadapt, &impl_cmpobject_typeadapt) ;
   testadapter_t           testadp = { .result = 0 } ;

   // TEST: callcmpkeyobj_typeadaptcomparator
   for (int result = -100; result <= 100; result += 100) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      for (uintptr_t i = 0; i <= 8; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.result = result ;
         TEST(result == callcmpkeyobj_typeadaptcomparator(&adpcmp, (struct typeadapt_t*)&testadp, (const void*)((8-i)*incr), (const struct typeadapt_object_t*)(i*incr))) ;
         TEST(testadp.lkey    == ((8-i)*incr)) ;
         TEST(testadp.robject == (const struct testobject_t*) (i*incr)) ;
      }
   }

   // TEST: callcmpobj_typeadaptcomparator
   for (int result = -1000; result <= 1000; result += 1000) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      for (uintptr_t i = 0; i <= 8; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.result = result ;
         TEST(result == callcmpobj_typeadaptcomparator(&adpcmp, (struct typeadapt_t*)&testadp, (const struct typeadapt_object_t*)(i*incr), (const struct typeadapt_object_t*)((8-i)*incr))) ;
         TEST(testadp.lobject == (const struct testobject_t*) (i*incr)) ;
         TEST(testadp.robject == (const struct testobject_t*) ((8-i)*incr)) ;
      }
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

typeadapt_comparator_DECLARE(testadapter_it, testadapter_t, testobject_t, uintptr_t) ;

static int test_generic(void)
{
   testadapter_t  testadp = { .result = 0 } ;
   testadapter_it adpcmp  = typeadapt_comparator_FREE ;

   // TEST typeadapt_comparator_DECLARE
   static_assert(sizeof(testadapter_it) == sizeof(typeadapt_comparator_it), "structure compatible") ;
   static_assert(offsetof(testadapter_it, cmp_key_object) == offsetof(typeadapt_comparator_it, cmp_key_object), "structure compatible") ;
   static_assert(offsetof(testadapter_it, cmp_object) == offsetof(typeadapt_comparator_it, cmp_object), "structure compatible") ;

   // TEST genericcast_typeadaptcomparator
   TEST((struct typeadapt_comparator_it*)&adpcmp == genericcast_typeadaptcomparator(&adpcmp, testadapter_t, testobject_t, int)) ;

   // TEST typeadapt_comparator_FREE
   TEST(0 == adpcmp.cmp_key_object) ;
   TEST(0 == adpcmp.cmp_object) ;

   // TEST typeadapt_comparator_INIT
   adpcmp = (testadapter_it) typeadapt_comparator_INIT(&impl_cmpkeyobject_testadapter, &impl_cmpobject_testadapter) ;
   TEST(adpcmp.cmp_key_object == &impl_cmpkeyobject_testadapter) ;
   TEST(adpcmp.cmp_object     == &impl_cmpobject_testadapter) ;

   // TEST callcmpobj_typeadaptcomparator, callcmpkeyobj_typeadaptcomparator
   for (int result = -10000; result <= 10000; result += 10000) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      for (uintptr_t i = 0; i <= 4; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.result = result ;
         TEST(result == callcmpkeyobj_typeadaptcomparator(&adpcmp, &testadp, ((4-i)*incr), (const testobject_t*)(i*incr))) ;
         TEST(testadp.lkey    == ((4-i)*incr)) ;
         TEST(testadp.robject == (const testobject_t*) (i*incr)) ;
         TEST(result == callcmpobj_typeadaptcomparator(&adpcmp, &testadp, (const testobject_t*)((i+1)*incr), (const testobject_t*)((5-i)*incr))) ;
         TEST(testadp.lobject == (const testobject_t*) ((i+1)*incr)) ;
         TEST(testadp.robject == (const testobject_t*)  ((5-i)*incr)) ;
      }
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_ds_typeadapt_comparator()
{
   if (test_initfree())       goto ONERR;
   if (test_callfunctions())  goto ONERR;
   if (test_generic())        goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
