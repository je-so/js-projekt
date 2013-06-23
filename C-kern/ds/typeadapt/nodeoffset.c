/* title: Typeadapt-Nodeoffset impl

   Implements <Typeadapt-Nodeoffset>.

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

   file: C-kern/api/ds/typeadapt/nodeoffset.h
    Header file <Typeadapt-Nodeoffset>.

   file: C-kern/ds/typeadapt/nodeoffset.c
    Implementation file <Typeadapt-Nodeoffset impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/typeadapt/nodeoffset.h"
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
   typeadapt_nodeoffset_t nodeoff = typeadapt_nodeoffset_INIT(0) ;

   // TEST typeadapt_nodeoffset_INIT: 0
   TEST(0 == nodeoff) ;

   // TEST typeadapt_nodeoffset_INIT: node0, ..., node3
   nodeoff = (typeadapt_nodeoffset_t) typeadapt_nodeoffset_INIT(offsetof(testobject_t, node0)) ;
   TEST(offsetof(testobject_t, node0) == nodeoff) ;
   nodeoff = (typeadapt_nodeoffset_t) typeadapt_nodeoffset_INIT(offsetof(testobject_t, node1)) ;
   TEST(offsetof(testobject_t, node1) == nodeoff) ;
   nodeoff = (typeadapt_nodeoffset_t) typeadapt_nodeoffset_INIT(offsetof(testobject_t, node2)) ;
   TEST(offsetof(testobject_t, node2) == nodeoff) ;
   nodeoff = (typeadapt_nodeoffset_t) typeadapt_nodeoffset_INIT(offsetof(testobject_t, node3)) ;
   TEST(offsetof(testobject_t, node3) == nodeoff) ;

   // TEST init_typeadaptnodeoffset
   for (uint32_t i = 0; i <= UINT16_MAX; i += 1 + (UINT16_MAX-i)/16) {
      init_typeadaptnodeoffset(&nodeoff, (uint16_t)i) ;
      TEST(i == nodeoff) ;
   }

   // TEST isequal_typeadaptnodeoffset
   for (uint16_t i = 1; i <= 10; ++i) {
      typeadapt_nodeoffset_t nodeoff2 = typeadapt_nodeoffset_INIT(i) ;
      init_typeadaptnodeoffset(&nodeoff, i) ;
      TEST(isequal_typeadaptnodeoffset(nodeoff, nodeoff2)) ;
      TEST(isequal_typeadaptnodeoffset(nodeoff2, nodeoff)) ;
      init_typeadaptnodeoffset(&nodeoff, 0) ;
      TEST(! isequal_typeadaptnodeoffset(nodeoff, nodeoff2)) ;
      TEST(! isequal_typeadaptnodeoffset(nodeoff2, nodeoff)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_objectnodeconversion(void)
{
   typeadapt_nodeoffset_t  nodeoff[4] = {
                              typeadapt_nodeoffset_INIT(offsetof(testobject_t, node0)),
                              typeadapt_nodeoffset_INIT(offsetof(testobject_t, node1)),
                              typeadapt_nodeoffset_INIT(offsetof(testobject_t, node2)),
                              typeadapt_nodeoffset_INIT(offsetof(testobject_t, node3))
                           } ;
   testobject_t            objects[100] ;

   // TEST memberasobject_typeadaptnodeoffset
   for (unsigned i = 0; i < lengthof(objects); ++i) {
      TEST((struct typeadapt_object_t*)&objects[i] == memberasobject_typeadaptnodeoffset(nodeoff[0], &objects[i].node0)) ;
      TEST((struct typeadapt_object_t*)&objects[i] == memberasobject_typeadaptnodeoffset(nodeoff[1], &objects[i].node1)) ;
      TEST((struct typeadapt_object_t*)&objects[i] == memberasobject_typeadaptnodeoffset(nodeoff[2], &objects[i].node2)) ;
      TEST((struct typeadapt_object_t*)&objects[i] == memberasobject_typeadaptnodeoffset(nodeoff[3], &objects[i].node3)) ;
   }

   // TEST objectasmember_typeadaptnodeoffset
   for (unsigned i = 0; i < lengthof(objects); ++i) {
      TEST(&objects[i].node0 == objectasmember_typeadaptnodeoffset(nodeoff[0], (struct typeadapt_object_t*)&objects[i])) ;
      TEST(&objects[i].node1 == objectasmember_typeadaptnodeoffset(nodeoff[1], (struct typeadapt_object_t*)&objects[i])) ;
      TEST(&objects[i].node2 == objectasmember_typeadaptnodeoffset(nodeoff[2], (struct typeadapt_object_t*)&objects[i])) ;
      TEST(&objects[i].node3 == objectasmember_typeadaptnodeoffset(nodeoff[3], (struct typeadapt_object_t*)&objects[i])) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt_nodeoffset()
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