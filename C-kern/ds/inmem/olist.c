/* title: DoubleLinkedListUsingOffsets impl

   Implements <DoubleLinkedListUsingOffsets>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2017 Jörg Seebohn

   file: C-kern/api/ds/inmem/olist.h
    Header file <DoubleLinkedListUsingOffsets>.

   file: C-kern/ds/inmem/olist.c
    Implementation file <DoubleLinkedListUsingOffsets impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/olist.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#endif


// section: olist_t

// group: lifetime


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

typedef struct test_t {
   olist_node_t node1;
   olist_node_t node2;
} test_t;

void set_offsets(test_t* node, unsigned next, unsigned prev)
{
   node->node1.next = (uint16_t) next;
   node->node2.next = (uint16_t) next;
   node->node1.prev = (uint16_t) prev;
   node->node2.prev = (uint16_t) prev;
}

static int test_initfree(void)
{
   olist_t list = olist_FREE;

   // TEST olist_FREE
   TEST( 0 == list.last);

   // TEST olist_INIT
   list = (olist_t) olist_INIT;
   TEST( 0 == list.last);

   // TEST init_olist
   list.last = 1;
   init_olist(&list);
   TEST( 0 == list.last);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   olist_t  list = olist_INIT;
   test_t   nodes[100];
   unsigned shift;

   // prepare
   static_assert( ispowerof2_int(sizeof(test_t)), "supports shift for offset caclulation");
   shift = (unsigned) log2_int(sizeof(test_t));

   // TEST isempty_olist
   for (unsigned i=1; i<100; ++i) {
      list.last = (uint16_t)i;
      TEST( 0 == isempty_olist(&list));
   }
   list.last = 0;
   TEST( 1 == isempty_olist(&list));

   // TEST first_olist: empty list ==> access with out of bounds index
   // TEST( 0 == first_olist(&list, &nodes[0].node1, shift));
   // TEST( 0 == first_olist(&list, &nodes[0].node2, shift));

   // TEST last_olist: empty list ==> index out of bounds
   TEST( (uint16_t)-1 == last_olist(&list, &nodes[0].node1, shift));
   TEST( (uint16_t)-1 == last_olist(&list, &nodes[0].node2, shift));

   // TEST first_olist: single element list
   for (unsigned i=0; i<lengthof(nodes); ++i) {
      list.last = (uint16_t) (i+1);
      set_offsets(&nodes[i], i, i);
      TEST( i == first_olist(&list, &nodes[0].node1, shift));
      TEST( i == first_olist(&list, &nodes[0].node2, shift));
   }

   // TEST last_olist: single element list
   for (unsigned i=0; i<lengthof(nodes); ++i) {
      list.last = (uint16_t) (i+1);
      set_offsets(&nodes[i], i, i);
      TEST( i == last_olist(&list, &nodes[0].node1, shift));
      TEST( i == last_olist(&list, &nodes[0].node2, shift));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   olist_t list1;
   olist_t list2;
   test_t  nodes[100];
   unsigned shift;

   // prepare
   static_assert( ispowerof2_int(sizeof(test_t)), "supports shift for offset caclulation");
   shift = (unsigned) log2_int(sizeof(test_t));

   // TEST insertfirst_olist
   list1 = (olist_t) olist_INIT;
   list2 = (olist_t) olist_INIT;
   for (unsigned i=0; i<lengthof(nodes); i+=2) {
      insertfirst_olist(&list1, (uint16_t) (i), &nodes[0].node1, shift);
      insertfirst_olist(&list2, (uint16_t) (i+1), &nodes[0].node2, shift);
      TEST( 1 == list1.last);
      TEST( 2 == list2.last);
   }
   for (unsigned i=0; i<lengthof(nodes); i+=2) {
      TEST( (i?i-2:lengthof(nodes)-2) == nodes[i].node1.next);
      TEST( (i+2)%lengthof(nodes) == nodes[i].node1.prev);
      TEST( (i?i-1:lengthof(nodes)-1) == nodes[i+1].node2.next);
      TEST( (i+3)%lengthof(nodes) == nodes[i+1].node2.prev);
   }

   // TEST insertlast_olist
   list1 = (olist_t) olist_INIT;
   list2 = (olist_t) olist_INIT;
   for (unsigned i=0; i<lengthof(nodes); i+=2) {
      insertlast_olist(&list1, (uint16_t) (i), &nodes[0].node1, shift);
      insertlast_olist(&list2, (uint16_t) (i+1), &nodes[0].node2, shift);
      TEST( i+1 == list1.last);
      TEST( i+2 == list2.last);
   }
   for (unsigned i=0; i<lengthof(nodes); i+=2) {
      TEST( (i+2)%lengthof(nodes) == nodes[i].node1.next);
      TEST( (i?i-2:lengthof(nodes)-2) == nodes[i].node1.prev);
      TEST( (i+3)%lengthof(nodes) == nodes[i+1].node2.next);
      TEST( (i?i-1:lengthof(nodes)-1) == nodes[i+1].node2.prev);
   }

   // TEST remove_olist: first element
   list1 = (olist_t) olist_INIT;
   list2 = (olist_t) olist_INIT;
   for (unsigned i=lengthof(nodes)-2; i<lengthof(nodes); i-=2) {
      insertfirst_olist(&list1, (uint16_t) (i), &nodes[0].node1, shift);
      insertfirst_olist(&list2, (uint16_t) (i+1), &nodes[0].node2, shift);
   }
   for (unsigned i=0; i<lengthof(nodes); i+=2) {
      TEST( lengthof(nodes)-1 == list1.last);
      TEST( lengthof(nodes)-0 == list2.last);
      remove_olist(&list1, (uint16_t) (i), &nodes[0].node1, shift);
      remove_olist(&list2, (uint16_t) (i+1), &nodes[0].node2, shift);
      if (i+2<lengthof(nodes)) {
         TEST( lengthof(nodes)-2 == nodes[i+2].node1.prev);
         TEST( lengthof(nodes)-1 == nodes[i+3].node2.prev);
         TEST( i+2 == nodes[lengthof(nodes)-2].node1.next);
         TEST( i+3 == nodes[lengthof(nodes)-1].node2.next);
      }
   }
   TEST( 0 == list1.last);
   TEST( 0 == list2.last);

   // TEST remove_olist: last element
   list1 = (olist_t) olist_INIT;
   list2 = (olist_t) olist_INIT;
   for (unsigned i=lengthof(nodes)-2; i<lengthof(nodes); i-=2) {
      insertfirst_olist(&list1, (uint16_t) (i), &nodes[0].node1, shift);
      insertfirst_olist(&list2, (uint16_t) (i+1), &nodes[0].node2, shift);
   }
   for (unsigned i=lengthof(nodes)-2; i<lengthof(nodes); i-=2) {
      TEST( i+1 == list1.last);
      TEST( i+2 == list2.last);
      remove_olist(&list1, (uint16_t) (i), &nodes[0].node1, shift);
      remove_olist(&list2, (uint16_t) (i+1), &nodes[0].node2, shift);
      if (i>=2) {
         TEST( i-2 == nodes[0].node1.prev);
         TEST( i-1 == nodes[1].node2.prev);
         TEST( 0   == nodes[i-2].node1.next);
         TEST( 1   == nodes[i-1].node2.next);
      }
   }
   TEST( 0 == list1.last);
   TEST( 0 == list2.last);

   return 0;
ONERR:
   return EINVAL;
}

static test_t* N(test_t* first, size_t i, size_t size)
{
   return (test_t*) ((uintptr_t)first + (i<<size));
}

int test_dynamicsize(void)
{
   memblock_t  mem = memblock_FREE;
   olist_t     list;

   // prepare
   TEST(0 == ALLOC_MM(16*(1<<16), &mem));

   for (unsigned size=8; size<=16; ++size) {
      test_t* first = (test_t*) mem.addr;

      for (unsigned i=0; i<16; ++i) {
         test_t* node = N(first, i, size);
         set_offsets(node, (i+1)%16, (i-1)%16);
      }

      // TEST first_olist
      for (unsigned i=1; i<=16; ++i) {
         list = (olist_t) { (uint16_t) i };
         TEST( (i%16) == first_olist(&list, &first->node1, size));
         TEST( (i%16) == first_olist(&list, &first->node2, size));
      }

      // TEST insertfirst_olist
      for (unsigned i=0; i<16; ++i) {
         test_t* node = N(first, i, size);
         set_offsets(node, 16, 16);
      }
      list = (olist_t) olist_INIT;
      for (unsigned i=0; i<16; ++i) {
         insertfirst_olist(&list, (uint16_t)i, &first->node1, size);
         TEST( 1 == list.last);
         TEST( i == first->node1.next);
      }
      list = (olist_t) olist_INIT;
      for (unsigned i=0; i<16; ++i) {
         insertfirst_olist(&list, (uint16_t)(15-i), &first->node2, size);
         TEST( 16 == list.last);
         TEST( (15-i) == N(first,15,size)->node2.next);
      }
      for (unsigned i=0; i<16; ++i) {
         TEST( (i-1)%16 == N(first,i,size)->node1.next);
         TEST( (i+1)%16 == N(first,i,size)->node1.prev);
         TEST( (i+1)%16 == N(first,i,size)->node2.next);
         TEST( (i-1)%16 == N(first,i,size)->node2.prev);
      }

      // TEST insertlast_olist
      for (unsigned i=0; i<16; ++i) {
         test_t* node = N(first, i, size);
         set_offsets(node, 16, 16);
      }
      list = (olist_t) olist_INIT;
      for (unsigned i=0; i<16; ++i) {
         insertlast_olist(&list, (uint16_t)i, &first->node1, size);
         TEST( i+1 == list.last);
         TEST( 0   == N(first,i,size)->node1.next);
      }
      list = (olist_t) olist_INIT;
      for (unsigned i=0; i<16; ++i) {
         insertlast_olist(&list, (uint16_t)(15-i), &first->node2, size);
         TEST( 16-i == list.last);
         TEST( 15   == N(first,15-i,size)->node2.next);
      }
      for (unsigned i=0; i<16; ++i) {
         TEST( (i+1)%16 == N(first,i,size)->node1.next);
         TEST( (i-1)%16 == N(first,i,size)->node1.prev);
         TEST( (i-1)%16 == N(first,i,size)->node2.next);
         TEST( (i+1)%16 == N(first,i,size)->node2.prev);
      }

      // TEST remove_olist
      for (unsigned l=1; l<=16; l+=8) {
         for (unsigned i=0; i<16; ++i) {
            test_t* node = N(first, i, size);
            set_offsets(node, (i+1)%16, (i-1)%16);
         }
         list = (olist_t) { (uint16_t) l };
         for (unsigned i=0; i<16; ++i) {
            remove_olist(&list, (uint16_t)i, &first->node1, size);
            if (i+1 < 16) {
               test_t* pnode = N(first, 15, size);
               TEST( i+1 == pnode->node1.next);
               test_t* nnode = N(first, i+1, size);
               TEST( 15  == nnode->node1.prev);
            }
            if (i+1==l) {
               TEST( 16 == list.last);
            }
         }
         TEST( 0 == list.last);
         list = (olist_t) { (uint16_t) l };
         for (unsigned i=0; i<16; ++i) {
            remove_olist(&list, (uint16_t)i, &first->node2, size);
            if (i+1 < 16) {
               test_t* pnode = N(first, 15, size);
               TEST( i+1 == pnode->node2.next);
               test_t* nnode = N(first, i+1, size);
               TEST( 15  == nnode->node2.prev);
            }
            if (i+1==l) {
               TEST( 16 == list.last);
            }
         }
         TEST( 0 == list.last);
      }
   }

   // reset
   TEST(0 == FREE_MM(&mem));

   return 0;
ONERR:
   FREE_MM(&mem);
   return EINVAL;
}

int unittest_ds_inmem_olist()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_update())         goto ONERR;
   if (test_dynamicsize())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
