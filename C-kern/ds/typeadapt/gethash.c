/* title: Typeadapt-GetHash impl

   Implements <Typeadapt-GetHash>.

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

   file: C-kern/api/ds/typeadapt/gethash.h
    Header file <Typeadapt-GetHash>.

   file: C-kern/ds/typeadapt/gethash.c
    Implementation file <Typeadapt-GetHash impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/typeadapt/gethash.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
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
   typeadapt_gethash_it gethash = typeadapt_gethash_INIT_FREEABLE ;

   // TEST typeadapt_gethash_INIT_FREEABLE
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
   gethash = (typeadapt_gethash_it) typeadapt_gethash_INIT_FREEABLE ;
   TEST(0 == isequal_typeadaptgethash(&gethash, &gethash2)) ;
   gethash2 = (typeadapt_gethash_it) typeadapt_gethash_INIT_FREEABLE ;
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
ONABORT:
   return EINVAL ;
}

static int test_callfunctions(void)
{
   typeadapt_gethash_it gethash = typeadapt_gethash_INIT(&impl_hashobject_typeadapt, &impl_hashkey_typeadapt) ;
   testadapter_t        testadp = { 0, 0, 0, 0 } ;
   double               nodes[50] ;
   float                keys[nrelementsof(nodes)] ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i] = 3*i ;
      keys[i]  = (float) (4*i) ;
   }

   // TEST callhashobject_typeadaptgethash
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(3*i == callhashobject_typeadaptgethash(&gethash, (struct typeadapt_t*)&testadp, (const struct typeadapt_object_t*)&nodes[i])) ;
      TEST(testadp.is_hashobject == 1+i) ;
      TEST(testadp.is_hashkey    == 0) ;
   }

   // TEST callhashkey_typeadaptgethash
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(4*i == callhashkey_typeadaptgethash(&gethash, (struct typeadapt_t*)&testadp, (const void*)&keys[i])) ;
      TEST(testadp.is_hashobject == nrelementsof(nodes)) ;
      TEST(testadp.is_hashkey    == 1+i) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

typeadapt_gethash_DECLARE(testadapter_it, testadapter_t, double, float*) ;

static int test_generic(void)
{
   testadapter_it    gethash = typeadapt_gethash_INIT(&impl_hashobject_testadapter, &impl_hashkey_testadapter) ;
   testadapter_t     testadp = { 0, 0, 0, 0 } ;
   double            nodes[50] ;
   float             keys[nrelementsof(nodes)] ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i] = 5*i ;
      keys[i]  = (float) (6*i) ;
   }

   // TEST typeadapt_gethash_DECLARE
   static_assert(sizeof(testadapter_it) == sizeof(typeadapt_gethash_it), "structure compatible") ;
   static_assert(offsetof(testadapter_it, hashobject) == offsetof(typeadapt_gethash_it, hashobject), "structure compatible") ;
   static_assert(offsetof(testadapter_it, hashkey) == offsetof(typeadapt_gethash_it, hashkey), "structure compatible") ;

   // TEST genericcast_typeadaptgethash
   TEST((typeadapt_gethash_it*)&gethash == genericcast_typeadaptgethash(&gethash, testadapter_t, double, float*)) ;

   // TEST callhashobject_typeadaptgethash
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(5*i == callhashobject_typeadaptgethash(&gethash, &testadp, &nodes[i])) ;
      TEST(testadp.is_hashobject == 1+i) ;
      TEST(testadp.is_hashkey    == 0) ;
   }

   // TEST callhashkey_typeadaptgethash
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(6*i == callhashkey_typeadaptgethash(&gethash, &testadp, &keys[i])) ;
      TEST(testadp.is_hashobject == nrelementsof(nodes)) ;
      TEST(testadp.is_hashkey    == 1+i) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt_gethash()
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
