/* title: Typeadapt-Lifetime impl

   Implements <Typeadapt-Lifetime>.

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

   file: C-kern/api/ds/typeadapt/lifetime.h
    Header file <Typeadapt-Lifetime>.

   file: C-kern/ds/typeadapt/lifetime.c
    Implementation file <Typeadapt-Lifetime impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt/lifetime.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: typeadapt_lifetime_it

// group: query

bool isequal_typeadaptlifetime(const typeadapt_lifetime_it * ladplife, const typeadapt_lifetime_it * radplife)
{
   return   ladplife->newcopy_object == radplife->newcopy_object
            && ladplife->delete_object == radplife->delete_object ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testadapter_t           testadapter_t  ;

struct testadapter_t {
   int err ;
   struct typeadapt_object_t        ** destobject ;
   const struct typeadapt_object_t  *  srcobject ;
   struct typeadapt_object_t        ** object ;
} ;

static int impl_newcopyobject_testadapter(testadapter_t * typeadp, /*out*/struct typeadapt_object_t ** destobject, const struct typeadapt_object_t * srcobject)
{
   typeadp->destobject = destobject ;
   typeadp->srcobject  = srcobject ;

   return typeadp->err ;
}

static int impl_deleteobject_testadapter(testadapter_t * typeadp, struct typeadapt_object_t ** object)
{
   typeadp->object = object ;

   return typeadp->err ;
}

static int adapt_newcopyobject_testadapter(struct typeadapt_t * typeadp, /*out*/struct typeadapt_object_t ** destobject, const struct typeadapt_object_t * srcobject)
{
   return impl_newcopyobject_testadapter((testadapter_t *) typeadp, destobject, srcobject) ;
}

static int adapt_deleteobject_testadapter(struct typeadapt_t * typeadp, struct typeadapt_object_t ** object)
{
   return impl_deleteobject_testadapter((testadapter_t *) typeadp, object) ;
}

static int test_initfree(void)
{
   typeadapt_lifetime_it   adplife = typeadapt_lifetime_INIT_FREEABLE ;

   // TEST typeadapt_lifetime_INIT_FREEABLE
   TEST(0 == adplife.newcopy_object) ;
   TEST(0 == adplife.delete_object) ;

   // TEST typeadapt_lifetime_INIT: dummy values
   for (uintptr_t i = 0; i <= 8; ++i) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      adplife = (typeadapt_lifetime_it) typeadapt_lifetime_INIT((typeof(adplife.newcopy_object))(i*incr), (typeof(adplife.delete_object))((8-i)*incr)) ;
      TEST(adplife.newcopy_object == (typeof(adplife.newcopy_object)) (i*incr)) ;
      TEST(adplife.delete_object  == (typeof(adplife.delete_object)) ((8-i)*incr)) ;
   }

   // TEST typeadapt_lifetime_INIT: real example
   adplife = (typeadapt_lifetime_it) typeadapt_lifetime_INIT(&adapt_newcopyobject_testadapter, &adapt_deleteobject_testadapter) ;
   TEST(adplife.newcopy_object == &adapt_newcopyobject_testadapter) ;
   TEST(adplife.delete_object  == &adapt_deleteobject_testadapter) ;

   // TEST isequal_typeadaptlifetime
   typeadapt_lifetime_it adplife2 ;
   for (unsigned i = 0; i < sizeof(typeadapt_lifetime_it)/sizeof(void*); ++i) {
      *(((void**)&adplife) +i) = (void*)i ;
      *(((void**)&adplife2)+i) = (void*)i ;
   }
   TEST(1 == isequal_typeadaptlifetime(&adplife, &adplife2)) ;
   TEST(1 == isequal_typeadaptlifetime(&adplife2, &adplife)) ;
   for (unsigned i = 0; i < sizeof(typeadapt_lifetime_it)/sizeof(void*); ++i) {
      *(((void**)&adplife2)+i) = (void*)(1+i) ;
      TEST(0 == isequal_typeadaptlifetime(&adplife, &adplife2)) ;
      TEST(0 == isequal_typeadaptlifetime(&adplife2, &adplife)) ;
      *(((void**)&adplife2)+i) = (void*)i ;
      TEST(1 == isequal_typeadaptlifetime(&adplife, &adplife2)) ;
      TEST(1 == isequal_typeadaptlifetime(&adplife2, &adplife)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_callfunctions(void)
{
   typeadapt_lifetime_it   adplife = typeadapt_lifetime_INIT(&adapt_newcopyobject_testadapter, &adapt_deleteobject_testadapter) ;
   testadapter_t           testadp = { .err = 0 } ;

   // TEST: callnewcopy_typeadaptlifetime
   for (int terr = -1000; terr <= 1000; terr += 1000) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      for (uintptr_t i = 0; i <= 8; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.err = terr ;
         TEST(terr == callnewcopy_typeadaptlifetime(&adplife, (struct typeadapt_t*)&testadp, (struct typeadapt_object_t**)(i*incr), (struct typeadapt_object_t*)((8-i)*incr))) ;
         TEST(testadp.destobject == (struct typeadapt_object_t**) (i*incr)) ;
         TEST(testadp.srcobject  == (struct typeadapt_object_t*)  ((8-i)*incr)) ;
      }
   }

   // TEST: calldelete_typeadaptlifetime
   for (int terr = -100; terr <= 100; terr += 100) {
      const uintptr_t incr = ((uintptr_t)-1) / 8u ;
      for (uintptr_t i = 0; i <= 8; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.err = terr ;
         TEST(terr == calldelete_typeadaptlifetime(&adplife, (struct typeadapt_t*)&testadp, (struct typeadapt_object_t**)(i*incr))) ;
         TEST(testadp.object == (struct typeadapt_object_t**) (i*incr)) ;
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

typeadapt_lifetime_DECLARE(testadapter_it, testadapter_t, struct typeadapt_object_t) ;

static int test_generic(void)
{
   testadapter_t  testadp = { .err = 0 } ;
   testadapter_it adplife = typeadapt_lifetime_INIT_FREEABLE ;

   // TEST typeadapt_lifetime_DECLARE
   static_assert(sizeof(testadapter_it) == sizeof(typeadapt_lifetime_it), "structur compatible") ;
   static_assert(offsetof(testadapter_it, newcopy_object) == offsetof(typeadapt_lifetime_it, newcopy_object), "structur compatible") ;
   static_assert(offsetof(testadapter_it, delete_object)  == offsetof(typeadapt_lifetime_it, delete_object), "structur compatible") ;

   // TEST asgeneric_typeadaptlifetime
   TEST((struct typeadapt_lifetime_it*)&adplife == asgeneric_typeadaptlifetime(&adplife, testadapter_t, struct typeadapt_object_t)) ;

   // TEST typeadapt_lifetime_INIT_FREEABLE
   TEST(0 == adplife.newcopy_object) ;
   TEST(0 == adplife.delete_object) ;

   // TEST typeadapt_lifetime_INIT
   adplife = (testadapter_it) typeadapt_lifetime_INIT(&impl_newcopyobject_testadapter, &impl_deleteobject_testadapter) ;
   TEST(adplife.newcopy_object == &impl_newcopyobject_testadapter) ;
   TEST(adplife.delete_object  == &impl_deleteobject_testadapter) ;

   // TEST: callnewcopy_typeadaptlifetime, calldelete_typeadaptlifetime
   for (int terr = -10000; terr <= 10000; terr += 10000) {
      const uintptr_t incr = ((uintptr_t)-1) / 4u ;
      for (uintptr_t i = 0; i <= 4; ++i) {
         memset(&testadp, (int)i+1, sizeof(testadp)) ;
         testadp.err = terr ;
         TEST(terr == callnewcopy_typeadaptlifetime(&adplife, &testadp, (struct typeadapt_object_t**)(i*incr), (struct typeadapt_object_t*)((4-i)*incr))) ;
         TEST(testadp.destobject == (struct typeadapt_object_t**) (i*incr)) ;
         TEST(testadp.srcobject  == (struct typeadapt_object_t*)  ((4-i)*incr)) ;
         TEST(testadp.object     != (struct typeadapt_object_t**) (i*incr)) ;
         TEST(terr == calldelete_typeadaptlifetime(&adplife, &testadp, (struct typeadapt_object_t**)(i*incr))) ;
         TEST(testadp.destobject == (struct typeadapt_object_t**) (i*incr)) ;
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt_lifetime()
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
