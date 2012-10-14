/* title: Typeadapt-GetBinaryKey impl

   Implements <Typeadapt-GetBinaryKey>.

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

   file: C-kern/api/ds/typeadapt/getbinarykey.h
    Header file <Typeadapt-GetBinaryKey>.

   file: C-kern/ds/typeadapt/getbinarykey.c
    Implementation file <Typeadapt-GetBinaryKey impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/typeadapt/getbinarykey.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/string/string.h"
#endif


// section: typeadapt_getbinarykey_it

// group: query

bool isequal_typeadaptgetbinarykey(const typeadapt_getbinarykey_it * ladpbinkey, const typeadapt_getbinarykey_it * radpbinkey)
{
   return ladpbinkey->getbinarykey == radpbinkey->getbinarykey ;
}

// group: test

#ifdef KONFIG_UNITTEST

typedef struct testnode_t              testnode_t ;
typedef struct testadapter_t           testadapter_t ;

struct testnode_t {
   const uint8_t  * addr ;
   size_t         size ;
   int is_getbinarykey ;
} ;

struct testadapter_t {
   int callcount ;
} ;

static void impl_getbinarykey_testadapter(testadapter_t * typeadp, testnode_t * node, /*out*/typeadapt_binarykey_t * binkey)
{
   ++ typeadp->callcount ;
   *binkey = (typeadapt_binarykey_t) typeadapt_binarykey_INIT(node->size, node->addr) ;
   ++ node->is_getbinarykey ;
}

static void impl_getbinarykey_typeadapt(struct typeadapt_t * typeadp, struct typeadapt_object_t * node, /*out*/typeadapt_binarykey_t * binkey)
{
   impl_getbinarykey_testadapter((testadapter_t*)typeadp, (testnode_t*)node, binkey) ;
}

static int test_binarykey(void)
{
   struct {
      /*without const */uint8_t  * addr ;
      size_t   size ;
   }                       anonym = { 0, 0 } ;
   conststring_t           str1   = conststring_INIT(5, (const uint8_t*)"12345") ;
   string_t                str2   = string_INIT(5, 0) ;
   typeadapt_binarykey_t   binkey = typeadapt_binarykey_INIT_FREEABLE ;
   typeadapt_binarykey_t   * ptrkey = 0 ;

   // TEST typeadapt_binarykey_INIT_FREEABLE
   TEST(0 == binkey.addr) ;
   TEST(0 == binkey.size) ;

   // TEST typeadapt_binarykey_INIT
   binkey = (typeadapt_binarykey_t) typeadapt_binarykey_INIT(1, (const uint8_t*)2) ;
   TEST(binkey.addr == (void*)2) ;
   TEST(binkey.size == 1) ;
   binkey = (typeadapt_binarykey_t) typeadapt_binarykey_INIT((size_t)-1, (const uint8_t*)-2) ;
   TEST(binkey.addr == (void*)-2) ;
   TEST(binkey.size == (size_t)-1) ;

   // TEST asgeneric_typeadaptbinarykey: with type conststring_t
   ptrkey = asgeneric_typeadaptbinarykey(&str1) ;
   TEST(ptrkey == (void*)&str1) ;

   // TEST asgeneric_typeadaptbinarykey: with type string_t
   ptrkey = asgeneric_typeadaptbinarykey(&str2) ;
   TEST(ptrkey == (void*)&str2) ;

   // TEST asgeneric_typeadaptbinarykey: with anonymous type
   ptrkey = asgeneric_typeadaptbinarykey(&anonym) ;
   TEST(ptrkey == (void*)&anonym) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}


static int test_initfree(void)
{
   typeadapt_getbinarykey_it adpbinkey  = typeadapt_getbinarykey_INIT_FREEABLE ;
   typeadapt_getbinarykey_it adpbinkey2 ;

   // TEST typeadapt_getbinarykey_INIT_FREEABLE
   TEST(0 == adpbinkey.getbinarykey) ;

   // TEST typeadapt_getbinarykey_INIT
   adpbinkey = (typeadapt_getbinarykey_it) typeadapt_getbinarykey_INIT((typeof(&impl_getbinarykey_typeadapt))1) ;
   TEST((typeof(&impl_getbinarykey_typeadapt))1 == adpbinkey.getbinarykey) ;
   adpbinkey = (typeadapt_getbinarykey_it) typeadapt_getbinarykey_INIT(&impl_getbinarykey_typeadapt) ;
   TEST(&impl_getbinarykey_typeadapt == adpbinkey.getbinarykey) ;

   // TEST isequal_typeadaptgetbinarykey
   adpbinkey2 = (typeadapt_getbinarykey_it) typeadapt_getbinarykey_INIT(&impl_getbinarykey_typeadapt) ;
   TEST(1 == isequal_typeadaptgetbinarykey(&adpbinkey, &adpbinkey2)) ;
   TEST(1 == isequal_typeadaptgetbinarykey(&adpbinkey2, &adpbinkey)) ;
   adpbinkey  = (typeadapt_getbinarykey_it) typeadapt_getbinarykey_INIT_FREEABLE ;
   TEST(0 == isequal_typeadaptgetbinarykey(&adpbinkey, &adpbinkey2)) ;
   TEST(0 == isequal_typeadaptgetbinarykey(&adpbinkey2, &adpbinkey)) ;
   adpbinkey2 = (typeadapt_getbinarykey_it) typeadapt_getbinarykey_INIT_FREEABLE ;
   TEST(1 == isequal_typeadaptgetbinarykey(&adpbinkey, &adpbinkey2)) ;
   TEST(1 == isequal_typeadaptgetbinarykey(&adpbinkey2, &adpbinkey)) ;
   for (unsigned i = 0; i < sizeof(typeadapt_getbinarykey_it)/sizeof(void*); ++i) {
      *(((void**)&adpbinkey)+i) = (void*)(1+i) ;
      TEST(0 == isequal_typeadaptgetbinarykey(&adpbinkey, &adpbinkey2)) ;
      TEST(0 == isequal_typeadaptgetbinarykey(&adpbinkey2, &adpbinkey)) ;
      *(((void**)&adpbinkey)+i) = (void*)i ;
      TEST(1 == isequal_typeadaptgetbinarykey(&adpbinkey, &adpbinkey2)) ;
      TEST(1 == isequal_typeadaptgetbinarykey(&adpbinkey2, &adpbinkey)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_callfunctions(void)
{
   typeadapt_getbinarykey_it  adpbinkey = typeadapt_getbinarykey_INIT(&impl_getbinarykey_typeadapt) ;
   testadapter_t              testadp   = { .callcount = 0 } ;
   testnode_t                 nodes[100] ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].addr = (const void*) (1 + i) ;
      nodes[i].size = - (size_t) i ;
      nodes[i].is_getbinarykey = 0 ;
   }

   // TEST callgetbinarykey_typeadaptgetbinarykey
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      int callcount = testadp.callcount + 1 ;
      typeadapt_binarykey_t binkey = typeadapt_binarykey_INIT_FREEABLE ;
      callgetbinarykey_typeadaptgetbinarykey(&adpbinkey, (struct typeadapt_t*)&testadp, (struct typeadapt_object_t*)&nodes[i], &binkey) ;
      TEST(testadp.callcount == callcount) ;
      TEST(binkey.addr == nodes[i].addr) ;
      TEST(binkey.size == nodes[i].size) ;
      TEST(1 == nodes[i].is_getbinarykey) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

typeadapt_getbinarykey_DECLARE(testadapter_it, testadapter_t, testnode_t) ;

static int test_generic(void)
{
   testadapter_t  testadp   = { .callcount = 0 } ;
   testadapter_it adpbinkey = typeadapt_getbinarykey_INIT_FREEABLE ;
   testnode_t     nodes[100] ;

   // prepare
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].addr = (const void*) (1 + i) ;
      nodes[i].size = - (size_t) i ;
      nodes[i].is_getbinarykey = 0 ;
   }

   // TEST typeadapt_getbinarykey_DECLARE
   static_assert(sizeof(testadapter_it) == sizeof(typeadapt_getbinarykey_it), "structure compatible") ;
   static_assert(offsetof(testadapter_it, getbinarykey) == offsetof(typeadapt_getbinarykey_it, getbinarykey), "structure compatible") ;

   // TEST asgeneric_typeadaptgetbinarykey
   TEST((typeadapt_getbinarykey_it*)&adpbinkey == asgeneric_typeadaptgetbinarykey(&adpbinkey, testadapter_t, testnode_t)) ;

   // TEST typeadapt_getbinarykey_INIT_FREEABLE
   TEST(0 == adpbinkey.getbinarykey) ;

   // TEST typeadapt_getbinarykey_INIT
   adpbinkey = (testadapter_it) typeadapt_getbinarykey_INIT(&impl_getbinarykey_testadapter) ;
   TEST(adpbinkey.getbinarykey == &impl_getbinarykey_testadapter) ;

   // TEST callgetbinarykey_typeadaptgetbinarykey
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      int callcount = testadp.callcount + 1 ;
      typeadapt_binarykey_t binkey = typeadapt_binarykey_INIT_FREEABLE ;
      callgetbinarykey_typeadaptgetbinarykey(&adpbinkey, &testadp, &nodes[i], &binkey) ;
      TEST(testadp.callcount == callcount) ;
      TEST(binkey.addr == nodes[i].addr) ;
      TEST(binkey.size == nodes[i].size) ;
      TEST(1 == nodes[i].is_getbinarykey) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt_getbinarykey()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_binarykey())      goto ONABORT ;
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
