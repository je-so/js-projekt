/* title: Typeadapt-GetKey impl

   Implements <Typeadapt-GetKey>.

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

   file: C-kern/api/ds/typeadapt/getkey.h
    Header file <Typeadapt-GetKey>.

   file: C-kern/ds/typeadapt/getkey.c
    Implementation file <Typeadapt-GetKey impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/typeadapt/getkey.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/string/string.h"
#endif


// section: typeadapt_getkey_it

// group: query

bool isequal_typeadaptgetkey(const typeadapt_getkey_it * ladpgetkey, const typeadapt_getkey_it * radpgetkey)
{
   return ladpgetkey->getbinarykey == radpgetkey->getbinarykey ;
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
   string_t                str1   = string_INIT_CSTR("12345") ;
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

   // TEST genericcast_typeadaptbinarykey: with type string_t
   ptrkey = genericcast_typeadaptbinarykey(&str1) ;
   TEST(ptrkey == (void*)&str1) ;

   // TEST genericcast_typeadaptbinarykey: with anonymous type
   ptrkey = genericcast_typeadaptbinarykey(&anonym) ;
   TEST(ptrkey == (void*)&anonym) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   typeadapt_getkey_it adpgetkey  = typeadapt_getkey_INIT_FREEABLE ;
   typeadapt_getkey_it adpgetkey2 ;

   // TEST typeadapt_getkey_INIT_FREEABLE
   TEST(0 == adpgetkey.getbinarykey) ;

   // TEST typeadapt_getkey_INIT
   adpgetkey = (typeadapt_getkey_it) typeadapt_getkey_INIT((typeof(&impl_getbinarykey_typeadapt))1) ;
   TEST((typeof(&impl_getbinarykey_typeadapt))1 == adpgetkey.getbinarykey) ;
   adpgetkey = (typeadapt_getkey_it) typeadapt_getkey_INIT(&impl_getbinarykey_typeadapt) ;
   TEST(&impl_getbinarykey_typeadapt == adpgetkey.getbinarykey) ;

   // TEST isequal_typeadaptgetkey
   adpgetkey2 = (typeadapt_getkey_it) typeadapt_getkey_INIT(&impl_getbinarykey_typeadapt) ;
   TEST(1 == isequal_typeadaptgetkey(&adpgetkey, &adpgetkey2)) ;
   TEST(1 == isequal_typeadaptgetkey(&adpgetkey2, &adpgetkey)) ;
   adpgetkey  = (typeadapt_getkey_it) typeadapt_getkey_INIT_FREEABLE ;
   TEST(0 == isequal_typeadaptgetkey(&adpgetkey, &adpgetkey2)) ;
   TEST(0 == isequal_typeadaptgetkey(&adpgetkey2, &adpgetkey)) ;
   adpgetkey2 = (typeadapt_getkey_it) typeadapt_getkey_INIT_FREEABLE ;
   TEST(1 == isequal_typeadaptgetkey(&adpgetkey, &adpgetkey2)) ;
   TEST(1 == isequal_typeadaptgetkey(&adpgetkey2, &adpgetkey)) ;
   for (unsigned i = 0; i < sizeof(typeadapt_getkey_it)/sizeof(void*); ++i) {
      ((void**)&adpgetkey)[i] = (void*)1 ;
      TEST(0 == isequal_typeadaptgetkey(&adpgetkey, &adpgetkey2)) ;
      TEST(0 == isequal_typeadaptgetkey(&adpgetkey2, &adpgetkey)) ;
      ((void**)&adpgetkey)[i] = (void*)0 ;
      TEST(1 == isequal_typeadaptgetkey(&adpgetkey, &adpgetkey2)) ;
      TEST(1 == isequal_typeadaptgetkey(&adpgetkey2, &adpgetkey)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_callfunctions(void)
{
   typeadapt_getkey_it  adpgetkey = typeadapt_getkey_INIT(&impl_getbinarykey_typeadapt) ;
   testadapter_t        testadp   = { .callcount = 0 } ;
   testnode_t           nodes[100] ;

   // prepare
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      nodes[i].addr = (const void*) (1 + i) ;
      nodes[i].size = - (size_t) i ;
      nodes[i].is_getbinarykey = 0 ;
   }

   // TEST callgetbinarykey_typeadaptgetkey
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      int callcount = testadp.callcount + 1 ;
      typeadapt_binarykey_t binkey = typeadapt_binarykey_INIT_FREEABLE ;
      callgetbinarykey_typeadaptgetkey(&adpgetkey, (struct typeadapt_t*)&testadp, (struct typeadapt_object_t*)&nodes[i], &binkey) ;
      TEST(testadp.callcount == callcount) ;
      TEST(binkey.addr == nodes[i].addr) ;
      TEST(binkey.size == nodes[i].size) ;
      TEST(1 == nodes[i].is_getbinarykey) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

typeadapt_getkey_DECLARE(testadapter_it, testadapter_t, testnode_t) ;

static int test_generic(void)
{
   testadapter_t  testadp   = { .callcount = 0 } ;
   testadapter_it adpgetkey = typeadapt_getkey_INIT_FREEABLE ;
   testnode_t     nodes[100] ;

   // prepare
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      nodes[i].addr = (const void*) (1 + i) ;
      nodes[i].size = - (size_t) i ;
      nodes[i].is_getbinarykey = 0 ;
   }

   // TEST typeadapt_getkey_DECLARE
   static_assert(sizeof(testadapter_it) == sizeof(typeadapt_getkey_it), "structure compatible") ;
   static_assert(offsetof(testadapter_it, getbinarykey) == offsetof(typeadapt_getkey_it, getbinarykey), "structure compatible") ;

   // TEST genericcast_typeadaptgetkey
   TEST((typeadapt_getkey_it*)&adpgetkey == genericcast_typeadaptgetkey(&adpgetkey, testadapter_t, testnode_t)) ;

   // TEST typeadapt_getkey_INIT_FREEABLE
   TEST(0 == adpgetkey.getbinarykey) ;

   // TEST typeadapt_getkey_INIT
   adpgetkey = (testadapter_it) typeadapt_getkey_INIT(&impl_getbinarykey_testadapter) ;
   TEST(adpgetkey.getbinarykey == &impl_getbinarykey_testadapter) ;

   // TEST callgetbinarykey_typeadaptgetkey
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      int callcount = testadp.callcount + 1 ;
      typeadapt_binarykey_t binkey = typeadapt_binarykey_INIT_FREEABLE ;
      callgetbinarykey_typeadaptgetkey(&adpgetkey, &testadp, &nodes[i], &binkey) ;
      TEST(testadp.callcount == callcount) ;
      TEST(binkey.addr == nodes[i].addr) ;
      TEST(binkey.size == nodes[i].size) ;
      TEST(1 == nodes[i].is_getbinarykey) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt_getkey()
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
