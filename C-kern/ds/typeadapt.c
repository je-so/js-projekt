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
#include "C-kern/api/test/unittest.h"
#endif


// section: typeadapt_member_t

// group: query

bool isequal_typeadaptmember(const typeadapt_member_t * lnodeadp, const typeadapt_member_t * rnodeadp)
{
   bool isEqual = (lnodeadp->typeadp == rnodeadp->typeadp)
                  && isequal_typeadaptnodeoffset(lnodeadp->nodeoff, rnodeadp->nodeoff) ;
   return isEqual ;
}


// section: typeadapt_t

// group: query

bool isequal_typeadapt(const typeadapt_t * ltypeadp, const typeadapt_t * rtypeadp)
{
   return   isequal_typeadaptcomparator(&ltypeadp->comparator, &rtypeadp->comparator)
            && isequal_typeadaptgethash(&ltypeadp->gethash, &rtypeadp->gethash)
            && isequal_typeadaptgetkey(&ltypeadp->getkey, &rtypeadp->getkey)
            && isequal_typeadaptlifetime(&ltypeadp->lifetime, &rtypeadp->lifetime) ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testadapt_t      testadapt_t ;
typedef struct testobject_t     testobject_t ;

struct testobject_t {
   struct {
      bool  is_cmpkeyobj ;
      bool  is_cmpobj ;
   } comparator ;
   struct {
      bool  is_hashobject ;
      bool  is_hashkey ;
   } gethash ;
   struct {
      bool  is_getbinarykey ;
   } getkey ;
   double key ;
   struct {
      bool  is_newcopy ;
      bool  is_delete ;
   } lifetime ;
} ;

/* struct: testadapt_t
 * Implements adapter for type <testobject_t>. */
struct testadapt_t {
   struct {
      typeadapt_EMBED(testadapt_t, testobject_t, const double*) ;
   } ;
   int call_count ;
} ;

static int impl_cmpkeyobj_testadapt(testadapt_t * typeadp, const double * lkey, const struct testobject_t * robject)
{
   ++ * CONST_CAST(double,lkey) ;
   CONST_CAST(testobject_t,robject)->comparator.is_cmpkeyobj = true ;
   return (typeadp->call_count ++) ;
}

static int impl_cmpobj_testadapt(testadapt_t * typeadp, const struct testobject_t * lobject, const struct testobject_t * robject)
{
   CONST_CAST(testobject_t,lobject)->comparator.is_cmpobj = true ;
   CONST_CAST(testobject_t,robject)->comparator.is_cmpobj = true ;
   return (typeadp->call_count ++) ;
}

static size_t impl_hashobj_testadapt(testadapt_t * typeadp, const struct testobject_t * object)
{
   CONST_CAST(testobject_t,object)->gethash.is_hashobject = true ;
   return (size_t) (typeadp->call_count ++) ;
}

static size_t impl_hashkey_testadapt(testadapt_t * typeadp, const double * key)
{
   CONST_CAST(testobject_t,(const struct testobject_t*)key)->gethash.is_hashkey = true ;
   return (size_t) (typeadp->call_count ++) ;
}

static void impl_getbinarykey_testadapt(testadapt_t * typeadp, struct testobject_t * node, struct typeadapt_binarykey_t * binkey)
{
   node->getkey.is_getbinarykey = true ;
   binkey->addr = (void*) &node->key ;
   binkey->size = (size_t) node->key ;
   typeadp->call_count ++ ;
   return ;
}

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
   typeadapt_t typeadp = typeadapt_FREE ;

   // TEST typeadapt_FREE
   TEST(0 == typeadp.comparator.cmp_key_object) ;
   TEST(0 == typeadp.comparator.cmp_object) ;
   TEST(0 == typeadp.gethash.hashobject) ;
   TEST(0 == typeadp.gethash.hashkey) ;
   TEST(0 == typeadp.getkey.getbinarykey) ;
   TEST(0 == typeadp.lifetime.newcopy_object) ;
   TEST(0 == typeadp.lifetime.delete_object) ;

   // TEST typeadapt_INIT_LIFETIME
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFETIME((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1,
                                                   (typeof(((typeadapt_lifetime_it*)0)->delete_object))2) ;
   TEST(typeadp.comparator.cmp_object     == 0) ;
   TEST(typeadp.comparator.cmp_key_object == 0) ;
   TEST(typeadp.gethash.hashobject        == 0) ;
   TEST(typeadp.gethash.hashkey           == 0) ;
   TEST(typeadp.getkey.getbinarykey       == 0) ;
   TEST(typeadp.lifetime.newcopy_object   == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1) ;
   TEST(typeadp.lifetime.delete_object    == (typeof(((typeadapt_lifetime_it*)0)->delete_object))2) ;

   // TEST typeadapt_INIT_CMP
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_CMP((typeof(((typeadapt_comparator_it*)0)->cmp_key_object))1,
                                              (typeof(((typeadapt_comparator_it*)0)->cmp_object))2) ;
   TEST(typeadp.comparator.cmp_key_object == (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))1) ;
   TEST(typeadp.comparator.cmp_object     == (typeof(((typeadapt_comparator_it*)0)->cmp_object))2) ;
   TEST(typeadp.getkey.getbinarykey       == 0) ;
   TEST(typeadp.gethash.hashobject        == 0) ;
   TEST(typeadp.gethash.hashkey           == 0) ;
   TEST(typeadp.lifetime.newcopy_object   == 0) ;
   TEST(typeadp.lifetime.delete_object    == 0) ;

   // TEST typeadapt_INIT_LIFECMP
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFECMP((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))3,
                                                  (typeof(((typeadapt_lifetime_it*)0)->delete_object))4,
                                                  (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))5,
                                                  (typeof(((typeadapt_comparator_it*)0)->cmp_object))6) ;
   TEST(typeadp.comparator.cmp_key_object == (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))5) ;
   TEST(typeadp.comparator.cmp_object     == (typeof(((typeadapt_comparator_it*)0)->cmp_object))6) ;
   TEST(typeadp.gethash.hashobject        == 0) ;
   TEST(typeadp.gethash.hashkey           == 0) ;
   TEST(typeadp.getkey.getbinarykey       == 0) ;
   TEST(typeadp.lifetime.newcopy_object   == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))3) ;
   TEST(typeadp.lifetime.delete_object    == (typeof(((typeadapt_lifetime_it*)0)->delete_object))4) ;

   // TEST typeadapt_INIT_LIFEKEY
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFEKEY((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))7,
                                                  (typeof(((typeadapt_lifetime_it*)0)->delete_object))8,
                                                  (typeof(((typeadapt_getkey_it*)0)->getbinarykey))9) ;
   TEST(typeadp.comparator.cmp_key_object == 0) ;
   TEST(typeadp.comparator.cmp_object     == 0) ;
   TEST(typeadp.gethash.hashobject        == 0) ;
   TEST(typeadp.gethash.hashkey           == 0) ;
   TEST(typeadp.getkey.getbinarykey       == (typeof(((typeadapt_getkey_it*)0)->getbinarykey))9) ;
   TEST(typeadp.lifetime.newcopy_object   == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))7) ;
   TEST(typeadp.lifetime.delete_object    == (typeof(((typeadapt_lifetime_it*)0)->delete_object))8) ;

   // TEST typeadapt_INIT_LIFECMPKEY
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFECMPKEY((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1,
                                                     (typeof(((typeadapt_lifetime_it*)0)->delete_object))2,
                                                     (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))3,
                                                     (typeof(((typeadapt_comparator_it*)0)->cmp_object))4,
                                                     (typeof(((typeadapt_getkey_it*)0)->getbinarykey))5) ;
   TEST(typeadp.comparator.cmp_key_object == (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))3) ;
   TEST(typeadp.comparator.cmp_object     == (typeof(((typeadapt_comparator_it*)0)->cmp_object))4) ;
   TEST(typeadp.gethash.hashobject        == 0) ;
   TEST(typeadp.gethash.hashkey           == 0) ;
   TEST(typeadp.getkey.getbinarykey       == (typeof(((typeadapt_getkey_it*)0)->getbinarykey))5) ;
   TEST(typeadp.lifetime.newcopy_object   == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1) ;
   TEST(typeadp.lifetime.delete_object    == (typeof(((typeadapt_lifetime_it*)0)->delete_object))2) ;

   // TEST typeadapt_INIT_LIFECMPHASH
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFECMPHASH((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1,
                                                      (typeof(((typeadapt_lifetime_it*)0)->delete_object))2,
                                                      (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))3,
                                                      (typeof(((typeadapt_comparator_it*)0)->cmp_object))4,
                                                      (typeof(((typeadapt_gethash_it*)0)->hashobject))5,
                                                      (typeof(((typeadapt_gethash_it*)0)->hashkey))6) ;
   TEST(typeadp.comparator.cmp_key_object == (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))3) ;
   TEST(typeadp.comparator.cmp_object     == (typeof(((typeadapt_comparator_it*)0)->cmp_object))4) ;
   TEST(typeadp.gethash.hashobject        == (typeof(((typeadapt_gethash_it*)0)->hashobject))5) ;
   TEST(typeadp.gethash.hashkey           == (typeof(((typeadapt_gethash_it*)0)->hashkey))6) ;
   TEST(typeadp.getkey.getbinarykey       == 0) ;
   TEST(typeadp.lifetime.newcopy_object   == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1) ;
   TEST(typeadp.lifetime.delete_object    == (typeof(((typeadapt_lifetime_it*)0)->delete_object))2) ;

   // TEST typeadapt_INIT_LIFECMPHASHKEY
   memset(&typeadp, 1, sizeof(typeadp)) ;
   typeadp = (typeadapt_t) typeadapt_INIT_LIFECMPHASHKEY((typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1,
                                                         (typeof(((typeadapt_lifetime_it*)0)->delete_object))2,
                                                         (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))3,
                                                         (typeof(((typeadapt_comparator_it*)0)->cmp_object))4,
                                                         (typeof(((typeadapt_gethash_it*)0)->hashobject))5,
                                                         (typeof(((typeadapt_gethash_it*)0)->hashkey))6,
                                                         (typeof(((typeadapt_getkey_it*)0)->getbinarykey))7) ;
   TEST(typeadp.comparator.cmp_key_object == (typeof(((typeadapt_comparator_it*)0)->cmp_key_object))3) ;
   TEST(typeadp.comparator.cmp_object     == (typeof(((typeadapt_comparator_it*)0)->cmp_object))4) ;
   TEST(typeadp.gethash.hashobject        == (typeof(((typeadapt_gethash_it*)0)->hashobject))5) ;
   TEST(typeadp.gethash.hashkey           == (typeof(((typeadapt_gethash_it*)0)->hashkey))6) ;
   TEST(typeadp.getkey.getbinarykey       == (typeof(((typeadapt_getkey_it*)0)->getbinarykey))7) ;
   TEST(typeadp.lifetime.newcopy_object   == (typeof(((typeadapt_lifetime_it*)0)->newcopy_object))1) ;
   TEST(typeadp.lifetime.delete_object    == (typeof(((typeadapt_lifetime_it*)0)->delete_object))2) ;

   // TEST isequal_typeadapt
   typeadapt_t typeadp2 = typeadapt_FREE ;
   typeadp = (typeadapt_t) typeadapt_FREE ;
   for (unsigned i = 0; i < sizeof(typeadapt_t)/sizeof(void*); ++i) {
      ((void**)&typeadp2)[i] = (void*)1 ;
      TEST(0 == isequal_typeadapt(&typeadp, &typeadp2)) ;
      TEST(0 == isequal_typeadapt(&typeadp2, &typeadp)) ;
      ((void**)&typeadp2)[i] = (void*)0 ;
      TEST(1 == isequal_typeadapt(&typeadp, &typeadp2)) ;
      TEST(1 == isequal_typeadapt(&typeadp2, &typeadp)) ;
   }

   // TEST iscalldelete_typeadapt
   typeadp = (typeadapt_t) typeadapt_FREE ;
   typeadp.lifetime.delete_object = (typeof(typeadp.lifetime.delete_object)) 1 ;
   TEST(1 == iscalldelete_typeadapt(&typeadp)) ;
   typeadp.lifetime.delete_object = (typeof(typeadp.lifetime.delete_object)) 0 ;
   TEST(0 == iscalldelete_typeadapt(&typeadp)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static bool isequal_testobject(const testobject_t * lobj, const testobject_t * robj)
{
   return   lobj->lifetime.is_newcopy == robj->lifetime.is_newcopy
            && lobj->lifetime.is_delete == robj->lifetime.is_delete
            && lobj->comparator.is_cmpkeyobj == robj->comparator.is_cmpkeyobj
            && lobj->comparator.is_cmpobj    == robj->comparator.is_cmpobj
            && lobj->getkey.is_getbinarykey  == robj->getkey.is_getbinarykey ;
}

static int test_generic(void)
{
   testadapt_t    testadp      = {
      typeadapt_INIT_LIFECMPHASHKEY(&impl_newcopy_testadapt, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_hashobj_testadapt, &impl_hashkey_testadapt, &impl_getbinarykey_testadapt),
      0
   } ;
   testobject_t   testobj[100] = { { .lifetime = {0,0}, .comparator = {0,0}, .getkey = {0}, .key = 0 } } ;
   testobject_t   * objptr ;

   // TEST genericcast_typeadapt
   TEST((typeadapt_t*)0        == genericcast_typeadapt((testadapt_t*)0, testadapt_t, testobject_t, double*)) ;
   TEST((typeadapt_t*)&testadp == genericcast_typeadapt(&testadp, testadapt_t, testobject_t, double*)) ;

   // TEST callnewcopy_typeadapt
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      int callcount = testadp.call_count ;
      objptr = (testobject_t *)1 ;
      TEST(callcount == callnewcopy_typeadapt(&testadp, &objptr, &testobj[i])) ;
      TEST(0 == objptr) ;
      TEST(callcount + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      const testobject_t result = { .lifetime = { .is_newcopy = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].lifetime.is_newcopy = false ;
   }

   // TEST calldelete_typeadapt
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      int callcount = testadp.call_count ;
      objptr = &testobj[i] ;
      TEST(callcount == calldelete_typeadapt(&testadp, &objptr)) ;
      TEST(0 == objptr) ;
      TEST(callcount + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      const testobject_t result = { .lifetime = { .is_delete = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].lifetime.is_delete = false ;
   }

   // TEST callcmpkeyobj_typeadapt
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      int callcount = testadp.call_count ;
      double key = i ;
      TEST(callcount == callcmpkeyobj_typeadapt(&testadp, &key, &testobj[i])) ;
      TEST(i + 1  == key) ;
      TEST(callcount + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      const testobject_t result = { .comparator = { .is_cmpkeyobj = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].comparator.is_cmpkeyobj = false ;
   }

   // TEST callcmpobj_typeadapt
   for (unsigned i = 0; i < lengthof(testobj); i += 2) {
      int callcount = testadp.call_count ;
      TEST(callcount == callcmpobj_typeadapt(&testadp, &testobj[i], &testobj[i+1])) ;
      TEST(callcount + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      const testobject_t result = { .comparator = { .is_cmpobj = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].comparator.is_cmpobj = false ;
   }

   // TEST callgetbinarykey_typeadapt
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      int                   callcount = testadp.call_count + 1 ;
      typeadapt_binarykey_t binkey    = typeadapt_binarykey_FREE ;
      testobj[i].key = 1 + i ;
      callgetbinarykey_typeadapt(&testadp, &testobj[i], &binkey) ;
      TEST(binkey.addr == (void*)&testobj[i].key) ;
      TEST(binkey.size == 1 + i) ;
      TEST(callcount   == testadp.call_count) ;
   }
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      const testobject_t result = { .getkey = { .is_getbinarykey = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].getkey.is_getbinarykey = false ;
   }

   // TEST callhashobject_typeadapt
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      int callcount = testadp.call_count ;
      TEST((size_t)callcount == callhashobject_typeadapt(&testadp, &testobj[i])) ;
      TEST(callcount + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      const testobject_t result = { .gethash = { .is_hashobject = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].gethash.is_hashobject = false ;
   }

   // TEST callhashkey_typeadapt
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      int callcount = testadp.call_count ;
      TEST((size_t)callcount == callhashkey_typeadapt(&testadp, (const double*)&testobj[i])) ;
      TEST(callcount + 1 == testadp.call_count) ;
   }
   for (unsigned i = 0; i < lengthof(testobj); ++i) {
      const testobject_t result = { .gethash = { .is_hashkey = true } } ;
      TEST(true == isequal_testobject(&result, &testobj[i])) ;
      testobj[i].gethash.is_hashkey = false ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_typeadaptmember(void)
{
   typeadapt_member_t      nodeadp = typeadapt_member_FREE ;
   typeadapt_nodeoffset_t  nodeoff = typeadapt_nodeoffset_INIT(0) ;
   testadapt_t             testadp = {
                              typeadapt_INIT_LIFECMPHASHKEY(&impl_newcopy_testadapt, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_hashobj_testadapt, &impl_hashkey_testadapt, &impl_getbinarykey_testadapt),
                              0
                           } ;
   typeadapt_member_t   nodeadp8[8] = {
                           typeadapt_member_INIT(0,offsetof(testobject_t, comparator.is_cmpkeyobj)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, comparator.is_cmpobj)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, gethash.is_hashobject)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, gethash.is_hashkey)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, getkey.is_getbinarykey)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, key)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, lifetime.is_newcopy)),
                           typeadapt_member_INIT(0,offsetof(testobject_t, lifetime.is_delete))
                        } ;
   testobject_t         testobj = { .comparator = {0,0}, .key = 0, .lifetime = {0,0} } ;
   testobject_t         cmpobj ;
   typeadapt_object_t   * objptr ;
   int                  callcount ;

   // TEST typeadapt_member_FREE
   TEST(0 == nodeadp.typeadp) ;
   TEST(isequal_typeadaptnodeoffset(nodeoff, nodeadp.nodeoff)) ;

   // TEST typeadapt_member_INIT
   for (uint16_t i = 1; i; i = (uint16_t) (i << 1)) {
      nodeadp = (typeadapt_member_t) typeadapt_member_INIT((typeadapt_t*)(uintptr_t)i, (uint16_t)(i+1)) ;
      nodeoff = (typeadapt_nodeoffset_t) typeadapt_nodeoffset_INIT((uint16_t)(i+1)) ;
      TEST((typeadapt_t*)(uintptr_t)i == nodeadp.typeadp) ;
      TEST(isequal_typeadaptnodeoffset(nodeoff, nodeadp.nodeoff)) ;
   }

   // TEST isequal_typeadaptmember
   typeadapt_member_t nodeadp2 = typeadapt_member_FREE ;
   nodeadp = (typeadapt_member_t) typeadapt_member_FREE ;
   for (unsigned i = 0; i < sizeof(typeadapt_member_t)/sizeof(uint32_t); ++i) {
      memset(&((uint32_t*)&nodeadp2)[i], 255, sizeof(uint32_t)) ;
      TEST(0 == isequal_typeadaptmember(&nodeadp, &nodeadp2)) ;
      TEST(0 == isequal_typeadaptmember(&nodeadp2, &nodeadp)) ;
      memset(&((uint32_t*)&nodeadp2)[i], 0, sizeof(uint32_t)) ;
      TEST(1 == isequal_typeadaptmember(&nodeadp, &nodeadp2)) ;
      TEST(1 == isequal_typeadaptmember(&nodeadp2, &nodeadp)) ;
   }

   // TEST callnewcopy_typeadaptmember
   nodeadp = (typeadapt_member_t) typeadapt_member_INIT(genericcast_typeadapt(&testadp, testadapt_t, testobject_t, double*), 0) ;
   callcount = testadp.call_count ;
   TEST(callcount == callnewcopy_typeadaptmember(&nodeadp, &objptr, (typeadapt_object_t*)&testobj)) ;
   TEST(callcount + 1 == testadp.call_count) ;
   cmpobj = (testobject_t) { .lifetime = { .is_newcopy = true } } ;
   TEST(true == isequal_testobject(&cmpobj, &testobj)) ;
   testobj.lifetime.is_newcopy = false ;

   // TEST calldelete_typeadaptmember
   callcount = testadp.call_count ;
   objptr = (typeadapt_object_t*)&testobj ;
   TEST(callcount == calldelete_typeadaptmember(&nodeadp, &objptr)) ;
   TEST(callcount + 1 == testadp.call_count) ;
   cmpobj = (testobject_t) { .lifetime = { .is_delete = true } } ;
   TEST(true == isequal_testobject(&cmpobj, &testobj)) ;
   testobj.lifetime.is_delete = false ;

   // TEST callcmpkeyobj_typeadaptmember
   callcount = testadp.call_count ;
   testobj.key = 2 ;
   TEST(callcount == callcmpkeyobj_typeadaptmember(&nodeadp, &testobj.key, (typeadapt_object_t*)&testobj)) ;
   TEST(callcount + 1 == testadp.call_count) ;
   cmpobj = (testobject_t) { .comparator = { .is_cmpkeyobj = true }, .key = 3 } ;
   TEST(true == isequal_testobject(&cmpobj, &testobj)) ;
   testobj.comparator.is_cmpkeyobj = false ;

   // TEST callcmpobj_typeadaptmember
   callcount = testadp.call_count ;
   TEST(callcount == callcmpobj_typeadaptmember(&nodeadp, (typeadapt_object_t*)&testobj, (typeadapt_object_t*)&testobj)) ;
   TEST(callcount + 1 == testadp.call_count) ;
   cmpobj = (testobject_t) { .comparator = { .is_cmpobj = true } } ;
   TEST(true == isequal_testobject(&cmpobj, &testobj)) ;
   testobj.comparator.is_cmpobj = false ;

   // TEST callgetbinarykey_typeadaptmember
   callcount = testadp.call_count ;
   typeadapt_binarykey_t binkey = typeadapt_binarykey_FREE ;
   testobj.key = 4 ;
   callgetbinarykey_typeadaptmember(&nodeadp, (typeadapt_object_t*)&testobj, &binkey) ;
   TEST(binkey.addr == (void*)&testobj.key) ;
   TEST(binkey.size == 4) ;
   TEST(callcount + 1 == testadp.call_count) ;
   cmpobj = (testobject_t) { .getkey= { .is_getbinarykey = true } } ;
   TEST(true == isequal_testobject(&cmpobj, &testobj)) ;
   testobj.getkey.is_getbinarykey = false ;

   // TEST callhashobject_typeadapt
   callcount = testadp.call_count ;
   TEST((size_t)callcount == callhashobject_typeadapt(&testadp, &testobj)) ;
   TEST(callcount + 1 == testadp.call_count) ;
   cmpobj = (testobject_t) { .gethash = { .is_hashobject = true } } ;
   TEST(true == isequal_testobject(&cmpobj, &testobj)) ;
   testobj.gethash.is_hashobject = false ;

   // TEST callhashkey_typeadapt
   callcount = testadp.call_count ;
   TEST((size_t)callcount == callhashkey_typeadapt(&testadp, (const double*)&testobj)) ;
   TEST(callcount + 1 == testadp.call_count) ;
   cmpobj = (testobject_t) { .gethash = { .is_hashkey = true } } ;
   TEST(true == isequal_testobject(&cmpobj, &testobj)) ;
   testobj.gethash.is_hashkey = false ;

   // TEST memberasobject_typeadaptmember
   TEST((struct typeadapt_object_t*)&testobj == memberasobject_typeadaptmember(&nodeadp8[0], &testobj.comparator.is_cmpkeyobj)) ;
   TEST((struct typeadapt_object_t*)&testobj == memberasobject_typeadaptmember(&nodeadp8[1], &testobj.comparator.is_cmpobj)) ;
   TEST((struct typeadapt_object_t*)&testobj == memberasobject_typeadaptmember(&nodeadp8[2], &testobj.gethash.is_hashobject)) ;
   TEST((struct typeadapt_object_t*)&testobj == memberasobject_typeadaptmember(&nodeadp8[3], &testobj.gethash.is_hashkey)) ;
   TEST((struct typeadapt_object_t*)&testobj == memberasobject_typeadaptmember(&nodeadp8[4], &testobj.getkey.is_getbinarykey)) ;
   TEST((struct typeadapt_object_t*)&testobj == memberasobject_typeadaptmember(&nodeadp8[5], &testobj.key)) ;
   TEST((struct typeadapt_object_t*)&testobj == memberasobject_typeadaptmember(&nodeadp8[6], &testobj.lifetime.is_newcopy)) ;
   TEST((struct typeadapt_object_t*)&testobj == memberasobject_typeadaptmember(&nodeadp8[7], &testobj.lifetime.is_delete)) ;

   // TEST objectasmember_typeadaptmember
   TEST(&testobj.comparator.is_cmpkeyobj == objectasmember_typeadaptmember(&nodeadp8[0], (struct typeadapt_object_t*)&testobj)) ;
   TEST(&testobj.comparator.is_cmpobj    == objectasmember_typeadaptmember(&nodeadp8[1], (struct typeadapt_object_t*)&testobj)) ;
   TEST(&testobj.gethash.is_hashobject   == objectasmember_typeadaptmember(&nodeadp8[2], (struct typeadapt_object_t*)&testobj)) ;
   TEST(&testobj.gethash.is_hashkey      == objectasmember_typeadaptmember(&nodeadp8[3], (struct typeadapt_object_t*)&testobj)) ;
   TEST(&testobj.getkey.is_getbinarykey  == objectasmember_typeadaptmember(&nodeadp8[4], (struct typeadapt_object_t*)&testobj)) ;
   TEST(&testobj.key == objectasmember_typeadaptmember(&nodeadp8[5], (struct typeadapt_object_t*)&testobj)) ;
   TEST(&testobj.lifetime.is_newcopy == objectasmember_typeadaptmember(&nodeadp8[6], (struct typeadapt_object_t*)&testobj)) ;
   TEST(&testobj.lifetime.is_delete  == objectasmember_typeadaptmember(&nodeadp8[7], (struct typeadapt_object_t*)&testobj)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt()
{
   if (test_initfree())          goto ONABORT ;
   if (test_generic())           goto ONABORT ;
   if (test_typeadaptmember())   goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
