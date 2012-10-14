/* title: Typeadapt-Typeinfo impl

   Implements <Typeadapt-Typeinfo>.

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

   file: C-kern/api/ds/typeadapt/typeinfo.h
    Header file <Typeadapt-Typeinfo>.

   file: C-kern/ds/typeadapt/typeinfo.c
    Implementation file <Typeadapt-Typeinfo impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/typeadapt/typeinfo.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

typedef struct testobject_t   testobject_t ;

struct testobject_t {
   uint32_t node0 ;
   uint32_t node1 ;
   uint64_t node2 ;
   uint64_t node3 ;
} ;

static int test_initfree(void)
{
   typeadapt_typeinfo_t tinfo = typeadapt_typeinfo_INIT(0) ;

   // TEST typeadapt_typeinfo_INIT: 0
   TEST(0 == tinfo.memberoffset) ;

   // TEST typeadapt_typeinfo_INIT: node0, ..., node3
   tinfo = (typeadapt_typeinfo_t) typeadapt_typeinfo_INIT(offsetof(testobject_t, node0)) ;
   TEST(offsetof(testobject_t, node0) == tinfo.memberoffset) ;
   tinfo = (typeadapt_typeinfo_t) typeadapt_typeinfo_INIT(offsetof(testobject_t, node1)) ;
   TEST(offsetof(testobject_t, node1) == tinfo.memberoffset) ;
   tinfo = (typeadapt_typeinfo_t) typeadapt_typeinfo_INIT(offsetof(testobject_t, node2)) ;
   TEST(offsetof(testobject_t, node2) == tinfo.memberoffset) ;
   tinfo = (typeadapt_typeinfo_t) typeadapt_typeinfo_INIT(offsetof(testobject_t, node3)) ;
   TEST(offsetof(testobject_t, node3) == tinfo.memberoffset) ;

   // TEST totypeinfo_typeadapttypeinfo
   for (uint32_t i = 0; i < UINT32_MAX; i += 1 + (UINT32_MAX-i)/16) {
      init_typeadapttypeinfo(&tinfo, i) ;
      TEST(i == tinfo.memberoffset) ;
   }

   // TEST isequal_typeadapttypeinfo
   for (uint32_t i = 1; i <= 10; ++i) {
      typeadapt_typeinfo_t tinfo2 = typeadapt_typeinfo_INIT(i) ;
      init_typeadapttypeinfo(&tinfo, i) ;
      TEST(isequal_typeadapttypeinfo(&tinfo, &tinfo2)) ;
      TEST(isequal_typeadapttypeinfo(&tinfo2, &tinfo)) ;
      init_typeadapttypeinfo(&tinfo, 0) ;
      TEST(! isequal_typeadapttypeinfo(&tinfo, &tinfo2)) ;
      TEST(! isequal_typeadapttypeinfo(&tinfo2, &tinfo)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_objectnodeconversion(void)
{
   typeadapt_typeinfo_t tinfo[4] = {
                           typeadapt_typeinfo_INIT(offsetof(testobject_t, node0)),
                           typeadapt_typeinfo_INIT(offsetof(testobject_t, node1)),
                           typeadapt_typeinfo_INIT(offsetof(testobject_t, node2)),
                           typeadapt_typeinfo_INIT(offsetof(testobject_t, node3))
                        } ;
   testobject_t         objects[100] ;

   // TEST memberasobject_typeadapttypeinfo
   for (unsigned i = 0; i < nrelementsof(objects); ++i) {
      TEST((struct typeadapt_object_t*)&objects[i] == memberasobject_typeadapttypeinfo(tinfo[0], &objects[i].node0)) ;
      TEST((struct typeadapt_object_t*)&objects[i] == memberasobject_typeadapttypeinfo(tinfo[1], &objects[i].node1)) ;
      TEST((struct typeadapt_object_t*)&objects[i] == memberasobject_typeadapttypeinfo(tinfo[2], &objects[i].node2)) ;
      TEST((struct typeadapt_object_t*)&objects[i] == memberasobject_typeadapttypeinfo(tinfo[3], &objects[i].node3)) ;
   }

   // TEST objectasmember_typeadapttypeinfo
   for (unsigned i = 0; i < nrelementsof(objects); ++i) {
      TEST(&objects[i].node0 == objectasmember_typeadapttypeinfo(tinfo[0], (struct typeadapt_object_t*)&objects[i])) ;
      TEST(&objects[i].node1 == objectasmember_typeadapttypeinfo(tinfo[1], (struct typeadapt_object_t*)&objects[i])) ;
      TEST(&objects[i].node2 == objectasmember_typeadapttypeinfo(tinfo[2], (struct typeadapt_object_t*)&objects[i])) ;
      TEST(&objects[i].node3 == objectasmember_typeadapttypeinfo(tinfo[3], (struct typeadapt_object_t*)&objects[i])) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt_typeinfo()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())             goto ONABORT ;
   if (test_objectnodeconversion()) goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
