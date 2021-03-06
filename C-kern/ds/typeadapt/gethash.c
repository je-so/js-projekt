/* title: Typeadapt-GetHash impl

   Implements <Typeadapt-GetHash>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/typeadapt/gethash.h
    Header file <Typeadapt-GetHash>.

   file: C-kern/ds/typeadapt/gethash.c
    Implementation file <Typeadapt-GetHash impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/typeadapt/gethash.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: typeadapt_gethash_it

// group: query

bool isequal_typeadaptgethash(const typeadapt_gethash_it * lgethash, const typeadapt_gethash_it * rgethash)
{
   return   lgethash->hashobject == rgethash->hashobject
            && lgethash->hashkey == rgethash->hashkey ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testadapter_t           testadapter_t ;

struct testadapter_t {
   const double * object;
   const float  * key ;
   unsigned is_hashobject ;
   unsigned is_hashkey ;
} ;

static size_t impl_hashobject_testadapter(testadapter_t * typeadp, const double * node)
{
   typeadp->object = node ;
   ++ typeadp->is_hashobject ;
   return (size_t) *node ;
}

static size_t impl_hashkey_testadapter(testadapter_t * typeadp, const float * key)
{
   typeadp->key = key ;
   ++ typeadp->is_hashkey ;
   return (size_t) *key ;
}

static size_t impl_hashobject_typeadapt(struct typeadapt_t * typeadp, const struct typeadapt_object_t * node)
{
   return impl_hashobject_testadapter((testadapter_t*)typeadp, (const double*)node) ;
}

static size_t impl_hashkey_typeadapt(struct typeadapt_t * typeadp, const void * key)
{
   return impl_hashkey_testadapter((testadapter_t*)typeadp, key) ;
}

static int test_initfree(void)
{
   typeadapt_gethash_it gethash = typeadapt_gethash_FREE ;

   // TEST typeadapt_gethash_FREE
   TEST(0 == gethash.hashobject) ;
   TEST(0 == gethash.hashkey) ;

   // TEST typeadapt_gethash_INIT
   gethash = (typeadapt_gethash_it) typeadapt_gethash_INIT((typeof(impl_hashobject_typeadapt)*)1, (typeof(impl_hashkey_typeadapt)*)2) ;
   TEST(gethash.hashobject == (typeof(impl_hashobject_typeadapt)*)1) ;
   TEST(gethash.hashkey    == (typeof(impl_hashkey_typeadapt)*)2) ;
   gethash = (typeadapt_gethash_it) typeadapt_gethash_INIT(&impl_hashobject_typeadapt, &impl_hashkey_typeadapt) ;
   TEST(gethash.hashobject == &impl_hashobject_typeadapt) ;
   TEST(gethash.hashkey    == &impl_hashkey_typeadapt) ;

   // TEST isequal_typeadaptgethash
   typeadapt_gethash_it gethash2 = typeadapt_gethash_INIT(&impl_hashobject_typeadapt, &impl_hashkey_typeadapt) ;
   TEST(1 == isequal_typeadaptgethash(&gethash, &gethash2)) ;
   TEST(1 == isequal_typeadaptgethash(&gethash2, &gethash)) ;
   gethash = (typeadapt_gethash_it) typeadapt_gethash_FREE ;
   TEST(0 == isequal_typeadaptgethash(&gethash, &gethash2)) ;
   gethash2 = (typeadapt_gethash_it) typeadapt_gethash_FREE ;
   TEST(1 == isequal_typeadaptgethash(&gethash, &gethash2)) ;
   for (unsigned i = 0; i < sizeof(typeadapt_gethash_it)/sizeof(void*); ++i) {
      ((void**)&gethash)[i] = (void*)1 ;
      TEST(0 == isequal_typeadaptgethash(&gethash, &gethash2)) ;
      TEST(0 == isequal_typeadaptgethash(&gethash2, &gethash)) ;
      ((void**)&gethash)[i] = (void*)0 ;
      TEST(1 == isequal_typeadaptgethash(&gethash, &gethash2)) ;
      TEST(1 == isequal_typeadaptgethash(&gethash2, &gethash)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_callfunctions(void)
{
   typeadapt_gethash_it gethash = typeadapt_gethash_INIT(&impl_hashobject_typeadapt, &impl_hashkey_typeadapt) ;
   testadapter_t        testadp = { 0, 0, 0, 0 } ;
   double               nodes[50] ;
   float                keys[lengthof(nodes)] ;

   // prepare
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      nodes[i] = 3*i ;
      keys[i]  = (float) (4*i) ;
   }

   // TEST callhashobject_typeadaptgethash
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(3*i == callhashobject_typeadaptgethash(&gethash, (struct typeadapt_t*)&testadp, (const struct typeadapt_object_t*)&nodes[i])) ;
      TEST(testadp.is_hashobject == 1+i) ;
      TEST(testadp.is_hashkey    == 0) ;
   }

   // TEST callhashkey_typeadaptgethash
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(4*i == callhashkey_typeadaptgethash(&gethash, (struct typeadapt_t*)&testadp, (const void*)&keys[i])) ;
      TEST(testadp.is_hashobject == lengthof(nodes)) ;
      TEST(testadp.is_hashkey    == 1+i) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

typeadapt_gethash_DECLARE(testadapter_it, testadapter_t, double, float*) ;

static int test_generic(void)
{
   testadapter_it    gethash = typeadapt_gethash_INIT(&impl_hashobject_testadapter, &impl_hashkey_testadapter) ;
   testadapter_t     testadp = { 0, 0, 0, 0 } ;
   double            nodes[50] ;
   float             keys[lengthof(nodes)] ;

   // prepare
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      nodes[i] = 5*i ;
      keys[i]  = (float) (6*i) ;
   }

   // TEST typeadapt_gethash_DECLARE
   static_assert(sizeof(testadapter_it) == sizeof(typeadapt_gethash_it), "structure compatible") ;
   static_assert(offsetof(testadapter_it, hashobject) == offsetof(typeadapt_gethash_it, hashobject), "structure compatible") ;
   static_assert(offsetof(testadapter_it, hashkey) == offsetof(typeadapt_gethash_it, hashkey), "structure compatible") ;

   // TEST cast_typeadaptgethash
   TEST((typeadapt_gethash_it*)&gethash == cast_typeadaptgethash(&gethash, testadapter_t, double, float*)) ;

   // TEST callhashobject_typeadaptgethash
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(5*i == callhashobject_typeadaptgethash(&gethash, &testadp, &nodes[i])) ;
      TEST(testadp.is_hashobject == 1+i) ;
      TEST(testadp.is_hashkey    == 0) ;
   }

   // TEST callhashkey_typeadaptgethash
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(6*i == callhashkey_typeadaptgethash(&gethash, &testadp, &keys[i])) ;
      TEST(testadp.is_hashobject == lengthof(nodes)) ;
      TEST(testadp.is_hashkey    == 1+i) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_ds_typeadapt_gethash()
{
   if (test_initfree())       goto ONERR;
   if (test_callfunctions())  goto ONERR;
   if (test_generic())        goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
