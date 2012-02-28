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
   if (err) goto ABBRUCH ;

   memcpy(toblock.addr, fromblock.addr, typeimpl->objectsize) ;

   *copiedobject = (struct generic_object_t*)toblock.addr ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int freeobj_typeadapter(typeadapter_t * typeimpl, struct generic_object_t * object)
{
   int err ;

   if (object)  {
      memblock_t mblock = memblock_INIT(typeimpl->objectsize, (uint8_t*)object) ;
      err = MM_FREE(&mblock) ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: variables

const typeadapter_it    g_typeadapter_functable = typeadapter_it_INIT(
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

static int test_initfree_oit(void)
{
   typeadapter_it    typeadt_ft = typeadapter_it_INIT_FREEABLE ;
   typeadapter_oit   typeadt    = typeadapter_oit_INIT_FREEABLE ;

   // TEST static init INIT_FREEABLE
   TEST(0 == typeadt_ft.copyobj) ;
   TEST(0 == typeadt_ft.freeobj) ;
   TEST(0 == typeadt.object) ;
   TEST(0 == typeadt.functable) ;

   // TEST static init INIT
   typeadt_ft = (typeadapter_it) typeadapter_it_INIT((typeof(typeadt_ft.copyobj))1, (typeof(typeadt_ft.freeobj))2) ;
   TEST(typeadt_ft.copyobj == (typeof(typeadt_ft.copyobj))1) ;
   TEST(typeadt_ft.freeobj == (typeof(typeadt_ft.freeobj))2) ;
   typeadt = (typeadapter_oit) typeadapter_oit_INIT((typeadapter_t*)1, &typeadt_ft);
   TEST(typeadt.object     == (void*)1) ;
   TEST(typeadt.functable  == &typeadt_ft) ;

   // TEST setcopy_typeadapterit
   typeadt_ft.freeobj = 0 ;
   for(uint32_t i = 0; i < 0xf0000000; i += 0x10000000) {
      setcopy_typeadapterit(&typeadt_ft, (typeof(typeadt_ft.copyobj))(i+2), typeadapter_t, struct generic_object_t) ;
      TEST(typeadt_ft.copyobj == (typeof(typeadt_ft.copyobj))(i+2)) ;
      TEST(typeadt_ft.freeobj == 0) ;
   }

   // TEST setfree_typeadapterit
   typeadt_ft.copyobj = 0 ;
   for(uint32_t i = 0; i < 0xf0000000; i += 0x10000000) {
    setfree_typeadapterit(&typeadt_ft, (typeof(typeadt_ft.freeobj))(i+2), typeadapter_t, struct generic_object_t) ;
      TEST(typeadt_ft.freeobj == (typeof(typeadt_ft.freeobj))(i+2)) ;
      TEST(typeadt_ft.copyobj == 0) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

typedef struct typeimplementor_t       typeimplementor_t ;

typedef struct implobject_t            implobject_t ;

typedef struct typeimplementor_it      typeimplementor_it ;

typedef struct typeimplementor_oit     typeimplementor_oit ;

typeadapter_it_DECLARE(typeimplementor_it, typeimplementor_t, implobject_t)

typeadapter_oit_DECLARE(typeimplementor_oit, typeimplementor_t, typeimplementor_it)

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


static int test_generic_oit(void)
{
   typeimplementor_t    typeimpl     = { 0, 0, 0 } ;
   typeimplementor_it   typeimpl_ft  = typeadapter_it_INIT(&test_copyfct, &test_freefct) ;
   typeimplementor_oit  typeimpl_oit = typeadapter_oit_INIT(&typeimpl, &typeimpl_ft) ;

   // TEST static init
   TEST(typeimpl_ft.copyobj == &test_copyfct) ;
   TEST(typeimpl_ft.freeobj == &test_freefct) ;
   TEST(typeimpl_oit.object     == &typeimpl) ;
   TEST(typeimpl_oit.functable  == &typeimpl_ft) ;

   // TEST set functions
   setcopy_typeadapterit(&typeimpl_ft, 0, typeimplementor_t, implobject_t) ;
   setfree_typeadapterit(&typeimpl_ft, 0, typeimplementor_t, implobject_t) ;
   TEST(typeimpl_ft.copyobj == 0) ;
   TEST(typeimpl_ft.freeobj == 0) ;
   setcopy_typeadapterit(&typeimpl_ft, &test_copyfct, typeimplementor_t, implobject_t) ;
   setfree_typeadapterit(&typeimpl_ft, &test_freefct, typeimplementor_t, implobject_t) ;
   TEST(typeimpl_ft.copyobj == &test_copyfct) ;
   TEST(typeimpl_ft.freeobj == &test_freefct) ;

   // TEST execcopy_typeadapteroit
   execcopy_typeadapteroit(&typeimpl_oit, (implobject_t**)12, (implobject_t*)13) ;
   TEST((void*)12 == typeimpl.copy) ;
   TEST((void*)13 == typeimpl.object) ;
   TEST((void*)0  == typeimpl.free) ;

   // TEST execfree_typeadapteroit
   execfree_typeadapteroit(&typeimpl_oit, (implobject_t*)14) ;
   TEST((void*)12 == typeimpl.copy) ;
   TEST((void*)13 == typeimpl.object) ;
   TEST((void*)14 == typeimpl.free) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

struct test_type_t {
   unsigned a ;
   unsigned b ;
   unsigned c ;
} ;

static int test_initfree_t(void)
{
   typeadapter_oit   toit   = typeadapter_oit_INIT_FREEABLE ;
   typeadapter_t     tadapt = typeadapter_INIT_FREEABLE ;

   // TEST g_typeadapter_functable
   TEST(g_typeadapter_functable.copyobj == &copyobj_typeadapter) ;
   TEST(g_typeadapter_functable.freeobj == &freeobj_typeadapter) ;

   // TEST static init typeadapter_INIT_FREEABLE
   TEST(0 == tadapt.objectsize) ;

   // TEST init, double free
   TEST(0 == init_typeadapter(&tadapt, 8)) ;
   TEST(8 == tadapt.objectsize) ;
   TEST(0 == free_typeadapter(&tadapt)) ;
   TEST(0 == tadapt.objectsize) ;
   TEST(0 == free_typeadapter(&tadapt)) ;
   TEST(0 == tadapt.objectsize) ;

   // TEST functable_typeadapter
   TEST(&g_typeadapter_functable == functable_typeadapter()) ;

   // TEST asoit_typeadapter
   TEST(toit.object     == 0) ;
   TEST(toit.functable  == 0) ;
   asoit_typeadapter(&tadapt, &toit) ;
   TEST(toit.object     == &tadapt) ;
   TEST(toit.functable  == &g_typeadapter_functable) ;
   toit.functable = (void*)1 ;
   asoit_typeadapter((&tadapt)+1, &toit) ;
   TEST(toit.object     == (&tadapt)+1) ;
   TEST(toit.functable  == &g_typeadapter_functable) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_helper_t(void)
{
   typeadapter_oit      toit   = typeadapter_oit_INIT_FREEABLE ;
   typeadapter_t        tadapt = typeadapter_INIT_FREEABLE ;
   struct test_type_t   value ;
   struct test_type_t   * valuecopy[100] ;

   // prepare
   TEST(0 == init_typeadapter(&tadapt, sizeof(struct test_type_t))) ;
   asoit_typeadapter(&tadapt, &toit) ;

   // TEST copyobj_typeadapter
   for(unsigned i = 0; i < nrelementsof(valuecopy); ++i) {
      value        = (struct test_type_t) { .a = i+1, .b = i+2, .c = i+3 } ;
      valuecopy[i] = 0 ;
      TEST(0 == execcopy_typeadapteroit(&toit, (struct generic_object_t**)&valuecopy[i], (struct generic_object_t*)&value)) ;
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
      TEST(0 == execfree_typeadapteroit(&toit, (struct generic_object_t*)valuecopy[i])) ;
      valuecopy[i] = 0 ;
   }

   // unprepare
   TEST(0 == free_typeadapter(&tadapt)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_ds_typeadapter()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree_oit())   goto ABBRUCH ;
   if (test_generic_oit())    goto ABBRUCH ;
   if (test_initfree_t())     goto ABBRUCH ;
   if (test_helper_t())       goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif