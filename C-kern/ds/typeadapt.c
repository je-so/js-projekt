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


// section: typeadapt_t

// group: query

bool isequal_typeadapt(const typeadapt_t * ltypeadp, const typeadapt_t * rtypeadp)
{
   return   isequal_typeadaptlifetime(&ltypeadp->lifetime, &rtypeadp->lifetime)
            && isequal_typeadaptkeycomparator(&ltypeadp->keycomparator, &rtypeadp->keycomparator)
            && isequal_typeadaptgetbinarykey(&ltypeadp->getbinarykey, &rtypeadp->getbinarykey) ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testadapt_t      testadapt_t ;
typedef struct testobject_t     testobject_t ;

struct testobject_t {
   struct {
      bool  is_newcopy ;
      bool  is_delete ;
   } lifetime ;
   struct {
      bool  is_cmpkeyobj ;
      bool  is_cmpobj ;
   } keycomparator ;
   struct {
      bool  is_getbinarykey ;
   } getbinarykey ;
   double key ;
} ;

/* struct: testadapt_t
 * Implements adapter for type <testobject_t>. */
struct testadapt_t {
   struct {
      typeadapt_EMBED(testadapt_t, testobject_t, double) ;
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

static int impl_cmpkeyobj_testadapt(testadapt_t * typeadp, const double * lkey, const struct testobject_t * robject)
{
   ++ * CONST_CAST(double,lkey) ;
   CONST_CAST(testobject_t,robject)->keycomparator.is_cmpkeyobj = true ;
   return (typeadp->call_count ++) ;
}

static int impl_cmpobj_testadapt(testadapt_t * typeadp, const struct testobject_t * lobject, const struct testobject_t * robject)
{
   CONST_CAST(testobject_t,lobject)->keycomparator.is_cmpobj = true ;
   CONST_CAST(testobject_t,robject)->keycomparator.is_cmpobj = true ;
   return (typeadp->call_count ++) ;
}

static void impl_getbinarykey_testadapt(testadapt_t * typeadp, struct testobject_t * node, struct typeadapt_binarykey_t * binkey)
{
   node->getbinarykey.is_getbinarykey = true ;
   binkey->addr = (void*) &node->key ;
   binkey->size = (size_t) node->key ;
   typeadp->call_count ++ ;
   return ;
}

static int test_initfree(void)
{
   typeadapt_t typeadp = typeadapt_INIT_FREEABLE ;

   // TEST typeadapt_INIT_FREEABLE
   TEST(0 == typeadp.lifetime.newcopy_object) ;
   TEST(0 == typeadp.lifetime.delete_object) ;
   TEST(0 == typeadp.keycomparator.cmp_object) ;
   TEST(0 == typeadp.keycomparator.cmp_key_object) ;
   TEST(0 == typeadp.getbinarykey.getbinarykey) ;

   // TEST typeadapt_INIT_LIFETIME
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFETIME((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1,
                                                   (typeof(((typeadapt_lifetime_it*)0)->delete_object))2) ;
   TEST(typeadp.lifetime.newcopy_object      == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1) ;
   TEST(typeadp.lifetime.delete_object       == (typeof(((typeadapt_lifetime_it*)0)->delete_object))2) ;
   TEST(typeadp.keycomparator.cmp_object     == 0) ;
   TEST(typeadp.keycomparator.cmp_key_object == 0) ;
   TEST(typeadp.getbinarykey.getbinarykey    == 0) ;

   // TEST typeadapt_INIT_KEYCMP
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_KEYCMP((typeof(((typeadapt_keycomparator_it*)0)->cmp_key_object))1,
                                                 (typeof(((typeadapt_keycomparator_it*)0)->cmp_object))2) ;
   TEST(typeadp.lifetime.newcopy_object      == 0) ;
   TEST(typeadp.lifetime.delete_object       == 0) ;
   TEST(typeadp.keycomparator.cmp_key_object == (typeof(((typeadapt_keycomparator_it*)0)->cmp_key_object))1) ;
   TEST(typeadp.keycomparator.cmp_object     == (typeof(((typeadapt_keycomparator_it*)0)->cmp_object))2) ;
   TEST(typeadp.getbinarykey.getbinarykey    == 0) ;

   // TEST typeadapt_INIT_LIFEKEYCMP
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFEKEYCMP((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))3,
                                                     (typeof(((typeadapt_lifetime_it*)0)->delete_object))4,
                                                     (typeof(((typeadapt_keycomparator_it*)0)->cmp_key_object))5,
                                                     (typeof(((typeadapt_keycomparator_it*)0)->cmp_object))6) ;
   TEST(typeadp.lifetime.newcopy_object      == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))3) ;
   TEST(typeadp.lifetime.delete_object       == (typeof(((typeadapt_lifetime_it*)0)->delete_object))4) ;
   TEST(typeadp.keycomparator.cmp_key_object == (typeof(((typeadapt_keycomparator_it*)0)->cmp_key_object))5) ;
   TEST(typeadp.keycomparator.cmp_object     == (typeof(((typeadapt_keycomparator_it*)0)->cmp_object))6) ;
   TEST(typeadp.getbinarykey.getbinarykey    == 0) ;

   // TEST typeadapt_INIT_LIFEGETKEY
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFEGETKEY((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))7,
                                                     (typeof(((typeadapt_lifetime_it*)0)->delete_object))8,
                                                     (typeof(((typeadapt_getbinarykey_it*)0)->getbinarykey))9) ;
   TEST(typeadp.lifetime.newcopy_object      == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))7) ;
   TEST(typeadp.lifetime.delete_object       == (typeof(((typeadapt_lifetime_it*)0)->delete_object))8) ;
   TEST(typeadp.keycomparator.cmp_key_object == 0) ;
   TEST(typeadp.keycomparator.cmp_object     == 0) ;
   TEST(typeadp.getbinarykey.getbinarykey    == (typeof(((typeadapt_getbinarykey_it*)0)->getbinarykey))9) ;

   // TEST typeadapt_INIT_LIFEKEYCMPGET
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFEKEYCMPGET((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1,
                                                     (typeof(((typeadapt_lifetime_it*)0)->delete_object))2,
                                                     (typeof(((typeadapt_keycomparator_it*)0)->cmp_key_object))3,
                                                     (typeof(((typeadapt_keycomparator_it*)0)->cmp_object))4,
                                                     (typeof(((typeadapt_getbinarykey_it*)0)->getbinarykey))5) ;
   TEST(typeadp.lifetime.newcopy_object      == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1) ;
   TEST(typeadp.lifetime.delete_object       == (typeof(((typeadapt_lifetime_it*)0)->delete_object))2) ;
   TEST(typeadp.keycomparator.cmp_key_object == (typeof(((typeadapt_keycomparator_it*)0)->cmp_key_object))3) ;
   TEST(typeadp.keycomparator.cmp_object     == (typeof(((typeadapt_keycomparator_it*)0)->cmp_object))4) ;
   TEST(typeadp.getbinarykey.getbinarykey    == (typeof(((typeadapt_getbinarykey_it*)0)->getbinarykey))5) ;

   // TEST isequal_typeadapt
   typeadapt_t typeadp2 ;
   for (unsigned i = 0; i < sizeof(typeadapt_t)/sizeof(void*); ++i) {
      *(((void**)&typeadp) +i) = (void*)i ;
      *(((void**)&typeadp2)+i) = (void*)i ;
   }
   TEST(1 == isequal_typeadapt(&typeadp, &typeadp2)) ;
   TEST(1 == isequal_typeadapt(&typeadp2, &typeadp)) ;
   for (unsigned i = 0; i < sizeof(typeadapt_t)/sizeof(void*); ++i) {
      *(((void**)&typeadp2)+i) = (void*)(1+i) ;
      TEST(0 == isequal_typeadapt(&typeadp, &typeadp2)) ;
      TEST(0 == isequal_typeadapt(&typeadp2, &typeadp)) ;
      *(((void**)&typeadp2)+i) = (void*)i ;
      TEST(1 == isequal_typeadapt(&typeadp, &typeadp2)) ;
      TEST(1 == isequal_typeadapt(&typeadp2, &typeadp)) ;
   }

   // TEST islifetimedelete_typeadapt
   typeadp = (typeadapt_t) typeadapt_INIT_FREEABLE ;
   TEST(0 == islifetimedelete_typeadapt(&typeadp)) ;
   typeadp.lifetime.delete_object = (typeof(typeadp.lifetime.delete_object)) 1 ;
   TEST(1 == islifetimedelete_typeadapt(&typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_FREEABLE ;
   TEST(0 == islifetimedelete_typeadapt(&typeadp)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static bool isequal_testobject(const testobject_t * lobj, const testobject_t * robj)
{
   return   lobj->lifetime.is_newcopy == robj->lifetime.is_newcopy
            && lobj->lifetime.is_delete == robj->lifetime.is_delete
            && lobj->keycomparator.is_cmpkeyobj == robj->keycomparator.is_cmpkeyobj
            && lobj->keycomparator.is_cmpobj == robj->keycomparator.is_cmpobj
            && lobj->getbinarykey.is_getbinarykey == robj->getbinarykey.is_getbinarykey ;
}

static int test_generic(void)
{
   testadapt_t    testadp      = {
      typeadapt_INIT_LIFEKEYCMPGET(&impl_newcopy_testadapt, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_getbinarykey_testadapt),
      0
   } ;
   testobject_t   testobj[100] = { { .lifetime = {0,0}, .keycomparator = {0,0}, .getbinarykey = {0}, .key = 0 } } ;
   testobject_t   * objptr ;

   // TEST asgeneric_typeadapt
   TEST((typeadapt_t*)0        == asgeneric_typeadapt((testadapt_t*)0, testadapt_t, testobject_t, double)) ;
   TEST((typeadapt_t*)&testadp == asgeneric_typeadapt(&testadp, testadapt_t, testobject_t, double)) ;

   // TEST callnewcopy_typeadapt
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int result = testadp.call_count ;
      objptr = (testobject_t *)1 ;
      TEST(result == callnewcopy_typeadapt(&testadp, &objptr, &testobj[i])) ;
      TEST(0 == objptr) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .lifetime = { .is_newcopy = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
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
      const testobject_t result = { .lifetime = { .is_delete = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].lifetime.is_delete = false ;
   }

   // TEST callcmpkeyobj_typeadapt
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int result = testadp.call_count ;
      double key = i ;
      TEST(result == callcmpkeyobj_typeadapt(&testadp, &key, &testobj[i])) ;
      TEST(i + 1  == key) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .keycomparator = { .is_cmpkeyobj = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].keycomparator.is_cmpkeyobj = false ;
   }

   // TEST callcmpobj_typeadapt
   for (unsigned i = 0; i < nrelementsof(testobj); i += 2) {
      int result = testadp.call_count ;
      TEST(result == callcmpobj_typeadapt(&testadp, &testobj[i], &testobj[i+1])) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .keycomparator = { .is_cmpobj = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].keycomparator.is_cmpobj = false ;
   }

   // TEST callgetbinarykey_typeadapt
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int               call_count = testadp.call_count + 1 ;
      typeadapt_binarykey_t binkey = typeadapt_binarykey_INIT_FREEABLE ;
      testobj[i].key = 1 + i ;
      callgetbinarykey_typeadapt(&testadp, &testobj[i], &binkey) ;
      TEST(binkey.addr == (void*)&testobj[i].key) ;
      TEST(binkey.size == 1 + i) ;
      TEST(call_count  == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .getbinarykey= { .is_getbinarykey = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].getbinarykey.is_getbinarykey = false ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_typeadaptmember(void)
{
   typeadapt_member_t   nodeadp = typeadapt_member_INIT_FREEABLE ;
   typeadapt_typeinfo_t tinfo   = typeadapt_typeinfo_INIT(0) ;
   testadapt_t          testadp = {
                           typeadapt_INIT_LIFEKEYCMPGET(&impl_newcopy_testadapt, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_getbinarykey_testadapt),
                           0
                        } ;
   typeadapt_member_t   nodeadp5[5] = {
                           typeadapt_member_INIT(0,offsetof(testobject_t, lifetime.is_newcopy)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, lifetime.is_delete)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, keycomparator.is_cmpkeyobj)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, keycomparator.is_cmpobj)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, key))
                        } ;
   testobject_t         testobj[100] = { { .lifetime = {0,0}, .keycomparator = {0,0}, .key = 0 } } ;
   testobject_t         * objptr ;

   // TEST typeadapt_member_INIT_FREEABLE
   TEST(0 == nodeadp.typeadp) ;
   TEST(isequal_typeadapttypeinfo(&tinfo, &nodeadp.typeinfo)) ;

   // TEST typeadapt_member_INIT
   for(unsigned i = 1; i; i <<= 1) {
      nodeadp = (typeadapt_member_t) typeadapt_member_INIT((typeadapt_t*)i, i+1) ;
      tinfo   = (typeadapt_typeinfo_t) typeadapt_typeinfo_INIT(i+1) ;
      TEST((typeadapt_t*)i == nodeadp.typeadp) ;
      TEST(isequal_typeadapttypeinfo(&tinfo, &nodeadp.typeinfo)) ;
   }

   // TEST isequal_typeadaptmember
   for(unsigned i = 1; i; i <<= 1) {
      nodeadp = (typeadapt_member_t) typeadapt_member_INIT((typeadapt_t*)i, i+1) ;
      typeadapt_member_t nodeadp2 = { .typeadp = (typeadapt_t*)i, { i+1 } } ;
      TEST(isequal_typeadaptmember(&nodeadp2, &nodeadp)) ;
      TEST(isequal_typeadaptmember(&nodeadp, &nodeadp2)) ;
      nodeadp2 = (typeadapt_member_t) { .typeadp = (typeadapt_t*)(i+1), { i+1 } } ;
      TEST(!isequal_typeadaptmember(&nodeadp2, &nodeadp)) ;
      TEST(!isequal_typeadaptmember(&nodeadp, &nodeadp2)) ;
      nodeadp2 = (typeadapt_member_t) { .typeadp = (typeadapt_t*)i, { i } } ;
      TEST(!isequal_typeadaptmember(&nodeadp2, &nodeadp)) ;
      TEST(!isequal_typeadaptmember(&nodeadp, &nodeadp2)) ;
   }

   // TEST callnewcopy_typeadaptmember
   nodeadp = (typeadapt_member_t) typeadapt_member_INIT(asgeneric_typeadapt(&testadp,testadapt_t, testobject_t, double), 0) ;
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int result = testadp.call_count ;
      objptr = (testobject_t *)1 ;
      TEST(result == callnewcopy_typeadaptmember(&nodeadp, (typeadapt_object_t**)&objptr, (typeadapt_object_t*)&testobj[i])) ;
      TEST(0 == objptr) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .lifetime = { .is_newcopy = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].lifetime.is_newcopy = false ;
   }

   // TEST calldelete_typeadaptmember
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int result = testadp.call_count ;
      objptr = &testobj[i] ;
      TEST(result == calldelete_typeadaptmember(&nodeadp, (typeadapt_object_t**)&objptr)) ;
      TEST(0 == objptr) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .lifetime = { .is_delete = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].lifetime.is_delete = false ;
   }

   // TEST callcmpkeyobj_typeadaptmember
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int result = testadp.call_count ;
      double key = i ;
      TEST(result == callcmpkeyobj_typeadaptmember(&nodeadp, (void*)&key, (typeadapt_object_t*)&testobj[i])) ;
      TEST(i + 1  == key) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .keycomparator = { .is_cmpkeyobj = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].keycomparator.is_cmpkeyobj = false ;
   }

   // TEST callcmpobj_typeadaptmember
   for (unsigned i = 0; i < nrelementsof(testobj); i += 2) {
      int result = testadp.call_count ;
      TEST(result == callcmpobj_typeadaptmember(&nodeadp, (typeadapt_object_t*)&testobj[i], (typeadapt_object_t*)&testobj[i+1])) ;
      TEST(result + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .keycomparator = { .is_cmpobj = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].keycomparator.is_cmpobj = false ;
   }

   // TEST callgetbinarykey_typeadaptmember
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      int               call_count = testadp.call_count + 1 ;
      typeadapt_binarykey_t binkey = typeadapt_binarykey_INIT_FREEABLE ;
      testobj[i].key = 1 + i ;
      callgetbinarykey_typeadaptmember(&nodeadp, (typeadapt_object_t*)&testobj[i], &binkey) ;
      TEST(binkey.addr == (void*)&testobj[i].key) ;
      TEST(binkey.size == 1 + i) ;
      TEST(call_count  == testadp.call_count) ;
   }
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      const testobject_t result = { .getbinarykey= { .is_getbinarykey = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].getbinarykey.is_getbinarykey = false ;
   }

   // TEST memberasobject_typeadaptmember
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      TEST((struct typeadapt_object_t*)&testobj[i] == memberasobject_typeadaptmember(&nodeadp5[0], &testobj[i].lifetime.is_newcopy)) ;
      TEST((struct typeadapt_object_t*)&testobj[i] == memberasobject_typeadaptmember(&nodeadp5[1], &testobj[i].lifetime.is_delete)) ;
      TEST((struct typeadapt_object_t*)&testobj[i] == memberasobject_typeadaptmember(&nodeadp5[2], &testobj[i].keycomparator.is_cmpkeyobj)) ;
      TEST((struct typeadapt_object_t*)&testobj[i] == memberasobject_typeadaptmember(&nodeadp5[3], &testobj[i].keycomparator.is_cmpobj)) ;
      TEST((struct typeadapt_object_t*)&testobj[i] == memberasobject_typeadaptmember(&nodeadp5[4], &testobj[i].key)) ;
   }

   // TEST objectasmember_typeadaptmember
   for (unsigned i = 0; i < nrelementsof(testobj); ++i) {
      TEST(&testobj[i].lifetime.is_newcopy == objectasmember_typeadaptmember(&nodeadp5[0], (struct typeadapt_object_t*)&testobj[i])) ;
      TEST(&testobj[i].lifetime.is_delete  == objectasmember_typeadaptmember(&nodeadp5[1], (struct typeadapt_object_t*)&testobj[i])) ;
      TEST(&testobj[i].keycomparator.is_cmpkeyobj == objectasmember_typeadaptmember(&nodeadp5[2], (struct typeadapt_object_t*)&testobj[i])) ;
      TEST(&testobj[i].keycomparator.is_cmpobj    == objectasmember_typeadaptmember(&nodeadp5[3], (struct typeadapt_object_t*)&testobj[i])) ;
      TEST(&testobj[i].key == objectasmember_typeadaptmember(&nodeadp5[4], (struct typeadapt_object_t*)&testobj[i])) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())          goto ONABORT ;
   if (test_generic())           goto ONABORT ;
   if (test_typeadaptmember())   goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
