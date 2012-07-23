/* title: DataStructure-TypeAdapter impl

   Implements <DataStructure-TypeAdapter>.

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

   file: C-kern/api/ds/typeadapter.h
    Header file <DataStructure-TypeAdapter>.

   file: C-kern/ds/typeadapter.c
    Implementation file <DataStructure-TypeAdapter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/typeadapter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: typeadapter_t

// group: helper

static int copyobj_typeadapter(typeadapter_t * typeimpl, /*out*/struct generic_object_t ** copiedobject, struct generic_object_t * object)
{
   int err ;
   memblock_t fromblock = memblock_INIT(typeimpl->objectsize, (uint8_t*)object) ;
   memblock_t toblock   = memblock_INIT_FREEABLE ;

   err = MM_RESIZE(typeimpl->objectsize, &toblock) ;
   if (err) goto ONABORT ;

   memcpy(toblock.addr, fromblock.addr, typeimpl->objectsize) ;

   *copiedobject = (struct generic_object_t*)toblock.addr ;

   return 0 ;
ONABORT:
   LOG_ABORT(err) ;
   return err ;
}

static int freeobj_typeadapter(typeadapter_t * typeimpl, struct generic_object_t * object)
{
   int err ;

   if (object)  {
      memblock_t mblock = memblock_INIT(typeimpl->objectsize, (uint8_t*)object) ;
      err = MM_FREE(&mblock) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: variables

const typeadapter_it    g_typeadapter_iimpl = typeadapter_it_INIT(
                            &copyobj_typeadapter
                           ,&freeobj_typeadapter
                        ) ;

// group: lifetime

int init_typeadapter(/*out*/typeadapter_t * tadapt, size_t objectsize)
{
   tadapt->objectsize = objectsize ;
   return 0 ;
}


int free_typeadapter(typeadapter_t * tadapt)
{
   tadapt->objectsize = 0 ;
   return 0 ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree_iot(void)
{
   typeadapter_it    typeadt_ft = typeadapter_it_INIT_FREEABLE ;
   typeadapter_iot   typeadt    = typeadapter_iot_INIT_FREEABLE ;

   // TEST static init INIT_FREEABLE
   TEST(0 == typeadt_ft.copyobj) ;
   TEST(0 == typeadt_ft.freeobj) ;
   TEST(0 == typeadt.object) ;
   TEST(0 == typeadt.iimpl) ;

   // TEST static init INIT
   typeadt_ft = (typeadapter_it) typeadapter_it_INIT((typeof(typeadt_ft.copyobj))1, (typeof(typeadt_ft.freeobj))2) ;
   TEST(typeadt_ft.copyobj == (typeof(typeadt_ft.copyobj))1) ;
   TEST(typeadt_ft.freeobj == (typeof(typeadt_ft.freeobj))2) ;
   typeadt = (typeadapter_iot) typeadapter_iot_INIT((typeadapter_t*)1, &typeadt_ft);
   TEST(typeadt.object == (void*)1) ;
   TEST(typeadt.iimpl  == &typeadt_ft) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

typedef struct typeimplementor_t       typeimplementor_t ;

typedef struct implobject_t            implobject_t ;

typedef struct typeimplementor_it      typeimplementor_it ;

typedef struct typeimplementor_iot     typeimplementor_iot ;

typeadapter_it_DECLARE(typeimplementor_it, typeimplementor_t, implobject_t)

typeadapter_iot_DECLARE(typeimplementor_iot, typeimplementor_t, typeimplementor_it)

struct typeimplementor_t  {
   implobject_t   ** copy ;
   implobject_t   * object ;
   implobject_t   * free ;
} ;

static int test_copyfct(typeimplementor_t * typeimpl,  implobject_t ** copiedobject, implobject_t * object)
{
   typeimpl->copy   = copiedobject ;
   typeimpl->object = object ;
   return 0 ;
}

static int test_freefct(typeimplementor_t * typeimpl, implobject_t * object)
{
   typeimpl->free = object ;
   return 0 ;
}


static int test_generic_iot(void)
{
   typeimplementor_t    typeimpl     = { 0, 0, 0 } ;
   typeimplementor_it   typeimpl_ft  = typeadapter_it_INIT(&test_copyfct, &test_freefct) ;
   typeimplementor_iot  typeimpl_iot = typeadapter_iot_INIT(&typeimpl, &typeimpl_ft) ;

   // TEST static init
   TEST(typeimpl_ft.copyobj == &test_copyfct) ;
   TEST(typeimpl_ft.freeobj == &test_freefct) ;
   TEST(typeimpl_iot.object == &typeimpl) ;
   TEST(typeimpl_iot.iimpl  == &typeimpl_ft) ;

   // TEST execcopy_typeadapteriot
   execcopy_typeadapteriot(&typeimpl_iot, (implobject_t**)12, (implobject_t*)13) ;
   TEST((void*)12 == typeimpl.copy) ;
   TEST((void*)13 == typeimpl.object) ;
   TEST((void*)0  == typeimpl.free) ;

   // TEST execfree_typeadapteriot
   execfree_typeadapteriot(&typeimpl_iot, (implobject_t*)14) ;
   TEST((void*)12 == typeimpl.copy) ;
   TEST((void*)13 == typeimpl.object) ;
   TEST((void*)14 == typeimpl.free) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

struct test_type_t {
   unsigned a ;
   unsigned b ;
   unsigned c ;
} ;

static int test_initfree_t(void)
{
   typeadapter_iot   tiot   = typeadapter_iot_INIT_FREEABLE ;
   typeadapter_t     tadapt = typeadapter_INIT_FREEABLE ;

   // TEST g_typeadapter_iimpl
   TEST(g_typeadapter_iimpl.copyobj == &copyobj_typeadapter) ;
   TEST(g_typeadapter_iimpl.freeobj == &freeobj_typeadapter) ;

   // TEST static init typeadapter_INIT_FREEABLE
   TEST(0 == tadapt.objectsize) ;

   // TEST init, double free
   TEST(0 == init_typeadapter(&tadapt, 8)) ;
   TEST(8 == tadapt.objectsize) ;
   TEST(0 == free_typeadapter(&tadapt)) ;
   TEST(0 == tadapt.objectsize) ;
   TEST(0 == free_typeadapter(&tadapt)) ;
   TEST(0 == tadapt.objectsize) ;

   // TEST iimpl_typeadapter
   TEST(&g_typeadapter_iimpl == iimpl_typeadapter()) ;

   // TEST asiot_typeadapter
   TEST(tiot.object == 0) ;
   TEST(tiot.iimpl  == 0) ;
   asiot_typeadapter(&tadapt, &tiot) ;
   TEST(tiot.object == &tadapt) ;
   TEST(tiot.iimpl  == &g_typeadapter_iimpl) ;
   tiot.iimpl = (void*)1 ;
   asiot_typeadapter((&tadapt)+1, &tiot) ;
   TEST(tiot.object == (&tadapt)+1) ;
   TEST(tiot.iimpl  == &g_typeadapter_iimpl) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_helper_t(void)
{
   typeadapter_iot      tiot   = typeadapter_iot_INIT_FREEABLE ;
   typeadapter_t        tadapt = typeadapter_INIT_FREEABLE ;
   struct test_type_t   value ;
   struct test_type_t   * valuecopy[100] ;

   // prepare
   TEST(0 == init_typeadapter(&tadapt, sizeof(struct test_type_t))) ;
   asiot_typeadapter(&tadapt, &tiot) ;

   // TEST copyobj_typeadapter
   for(unsigned i = 0; i < nrelementsof(valuecopy); ++i) {
      value        = (struct test_type_t) { .a = i+1, .b = i+2, .c = i+3 } ;
      valuecopy[i] = 0 ;
      TEST(0 == execcopy_typeadapteriot(&tiot, (struct generic_object_t**)&valuecopy[i], (struct generic_object_t*)&value)) ;
      TEST(0 != valuecopy[i]) ;
      TEST(i+1 == valuecopy[i]->a) ;
      TEST(i+2 == valuecopy[i]->b) ;
      TEST(i+3 == valuecopy[i]->c) ;
   }

   // TEST freeobj_typeadapter
   for(unsigned i = 0; i < nrelementsof(valuecopy); ++i) {
      TEST(i+1 == valuecopy[i]->a) ;
      TEST(i+2 == valuecopy[i]->b) ;
      TEST(i+3 == valuecopy[i]->c) ;
      TEST(0 == execfree_typeadapteriot(&tiot, (struct generic_object_t*)valuecopy[i])) ;
      valuecopy[i] = 0 ;
   }

   // unprepare
   TEST(0 == free_typeadapter(&tadapt)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapter()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree_iot())   goto ONABORT ;
   if (test_generic_iot())    goto ONABORT ;
   if (test_initfree_t())     goto ONABORT ;
   if (test_helper_t())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif