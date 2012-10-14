/* title: DoubleLinkedList impl

   Implements <DoubleLinkedList>.

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

   file: C-kern/api/ds/inmem/dlist.h
    Header file <DoubleLinkedList>.

   file: C-kern/ds/inmem/dlist.c
    Implementation file <DoubleLinkedList impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/dlist.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/errortimer.h"
#endif


// section: dlist_t

// group: lifetime

int free_dlist(dlist_t *list, struct typeadapt_member_t * nodeadp)
{
   int err ;

   if (list->last) {

      dlist_node_t * node = list->last->next ;
      list->last->next = 0 ;

      list->last = 0 ;

      const bool isDelete = (nodeadp && islifetimedelete_typeadapt(nodeadp->typeadp)) ;

      err = 0 ;

      do {
         dlist_node_t * next = node->next ;
         node->prev = 0 ;
         node->next = 0 ;
         if (isDelete) {
            typeadapt_object_t * delobj = memberasobject_typeadaptmember(nodeadp, node) ;
            int err2 = calldelete_typeadaptmember(nodeadp, &delobj) ;
            if (err2) err = err2 ;
         }
         node = next ;
      } while (node) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int insertfirst_dlist(dlist_t * list, dlist_node_t * new_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 == new_node->next, ONABORT, ) ;

   if (list->last) {
      new_node->prev = list->last ;
      new_node->next = list->last->next ; /*old head*/
      list->last->next     = new_node ; /*new head*/
      new_node->next->prev = new_node ;
   } else {
      new_node->prev = new_node ;
      new_node->next = new_node ;
      list->last     = new_node ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int insertlast_dlist(dlist_t * list, dlist_node_t * new_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 == new_node->next, ONABORT, ) ;

   if (list->last) {
      new_node->prev = list->last ;
      new_node->next = list->last->next ; /*head*/
      list->last->next     = new_node ; /*new tail*/
      new_node->next->prev = new_node ;
   } else {
      new_node->prev = new_node ;
      new_node->next = new_node ;
   }

   list->last = new_node ; /*new tail*/

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int insertafter_dlist(dlist_t * list, dlist_node_t * prev_node, dlist_node_t * new_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 == new_node->next && 0 != prev_node->next && !isempty_dlist(list), ONABORT, ) ;

   new_node->prev  = prev_node ;
   new_node->next  = prev_node->next ;
   prev_node->next = new_node ;
   new_node->next->prev = new_node ;

   if (list->last == prev_node) {
      list->last = new_node ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int insertbefore_dlist(dlist_t * list, dlist_node_t * next_node, dlist_node_t * new_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 == new_node->next && 0 != next_node->next && !isempty_dlist(list), ONABORT, ) ;

   new_node->prev  = next_node->prev ;
   new_node->prev->next = new_node ;
   new_node->next  = next_node ;
   next_node->prev = new_node ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static inline void removehelper_dlist(dlist_t * list, dlist_node_t * node)
{
   if (node == list->last) {
      list->last = 0 ; /*removed last element*/
   } else {
      node->prev->next = node->next ;
      node->next->prev = node->prev ;
   }

   node->prev = 0 ;
   node->next = 0 ;
}

int removefirst_dlist(dlist_t * list, dlist_node_t ** removed_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(!isempty_dlist(list), ONABORT, ) ;

   dlist_node_t * const first = list->last->next ;

   removehelper_dlist(list, first) ;

   *removed_node = first ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removelast_dlist(dlist_t * list, dlist_node_t ** removed_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(!isempty_dlist(list), ONABORT, ) ;

   dlist_node_t * const last = list->last ;
   list->last = last->prev ;

   removehelper_dlist(list, last) ;

   *removed_node = last ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_dlist(dlist_t * list, dlist_node_t * node)
{
   int err ;

   VALIDATE_INPARAM_TEST(!isempty_dlist(list) && node->next, ONABORT, ) ;

   if (node == list->last) list->last = node->prev ;

   removehelper_dlist(list, node) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct genericadapt_t       genericadapt_t ;
typedef struct testadapt_t          testadapt_t ;
typedef struct testnode_t           testnode_t ;
typedef struct genericnode_t        genericnode_t ;

struct testnode_t {
   dlist_node_t  node ;
   int is_freed ;
   int is_inserted ;
} ;

struct genericnode_t {
   int is_freed ;
   // both dlist_node_t with offset !
   dlist_node_t   node1 ;
   dlist_node_t   node2 ;
} ;

struct testadapt_t {
   struct {
      typeadapt_EMBED(testadapt_t, testnode_t, void) ;
   } ;
   test_errortimer_t errcounter ;
   unsigned          freenode_count ;
} ;

struct genericadapt_t {
   struct {
      typeadapt_EMBED(genericadapt_t, genericnode_t, void) ;
   } ;
   test_errortimer_t errcounter ;
   unsigned          freenode_count ;
} ;

static int freenode_testdapt(testadapt_t * testadp, testnode_t ** node)
{
   int err = process_testerrortimer(&testadp->errcounter) ;

   if (!err) {
      ++ testadp->freenode_count ;
      ++ (*node)->is_freed ;
   }

   *node = 0 ;

   return err ;
}

static int freenode_genericadapt(genericadapt_t * typeadp, genericnode_t ** node)
{
   int err = process_testerrortimer(&typeadp->errcounter) ;

   if (!err) {
      ++ typeadp->freenode_count ;
      ++ (*node)->is_freed ;
   }

   *node = 0 ;

   return err ;
}

static int test_initfree(void)
{
   testadapt_t          typeadapt = { typeadapt_INIT_LIFETIME(0, &freenode_testdapt), test_errortimer_INIT_FREEABLE, 0 } ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, void), 0) ;
   dlist_t              list = dlist_INIT ;
   dlist_node_t         node = dlist_node_INIT ;
   testnode_t           nodes[1000] ;

   memset(nodes, 0, sizeof(nodes)) ;

   // TEST dlist_node_INIT
   TEST(0 == node.next) ;
   TEST(0 == node.prev) ;

   // TEST dlist_INIT
   TEST(0 == list.last) ;

   // TEST init_dlist, double free_dlist
   list.last = (void*)1 ;
   init_dlist(&list) ;
   TEST(0 == list.last) ;
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   TEST(0 == free_dlist(&list, &nodeadapt)) ;
   TEST(0 == list.last) ;
   TEST(1 == nodes[0].is_freed) ;
   TEST(0 == free_dlist(&list, &nodeadapt)) ;
   TEST(0 == list.last) ;
   TEST(1 == nodes[0].is_freed) ;
   nodes[0].is_freed = 0 ;

   // TEST free_dlist: no delete called with 0 parameter
   init_dlist(&list) ;
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   TEST(0 == free_dlist(&list, 0)) ;
   TEST(0 == nodes[0].is_freed) ;

   // TEST free_dlist: no delete called if typeadapt_t.lifetime.delete_object set to 0
   testadapt_t  old_typeadapt = typeadapt ;
   typeadapt.lifetime.delete_object = 0 ;
   init_dlist(&list) ;
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   TEST(0 == free_dlist(&list, &nodeadapt)) ;
   TEST(0 == nodes[0].is_freed) ;
   typeadapt = old_typeadapt ;

   // TEST free_dlist: all inserted nodes
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_dlist(&list, &nodes[i].node)) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 != nodes[i].node.next) ;
      TEST(0 != nodes[i].node.prev) ;
      TEST(0 == nodes[i].is_freed) ;
   }
   typeadapt.freenode_count = 0 ;
   TEST(0 == free_dlist(&list, &nodeadapt)) ;
   TEST(0 == list.last) ;
   TEST(nrelementsof(nodes) == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST free_dlist: error in second node
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_dlist(&list, &nodes[i].node)) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 != nodes[i].node.next) ;
      TEST(0 != nodes[i].node.prev) ;
      TEST(0 == nodes[i].is_freed) ;
   }
   typeadapt.freenode_count = 0 ;
   init_testerrortimer(&typeadapt.errcounter, 2, ENOMEM) ;
   TEST(ENOMEM == free_dlist(&list, &nodeadapt)) ;
   TEST(0 == list.last) ;
   TEST(nrelementsof(nodes)-1 == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST((i!=1) == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   dlist_t  list = dlist_INIT ;

   // TEST isempty_dlist
   TEST(1 == isempty_dlist(&list)) ;
   list.last = (void*) 1 ;
   TEST(0 == isempty_dlist(&list)) ;
   list.last = 0 ;
   TEST(1 == isempty_dlist(&list)) ;

   // TEST first_dlist
   TEST(0 == first_dlist(&list)) ;
   dlist_node_t lastnode = { .next = (void*) 3 } ;
   list.last = &lastnode ;
   TEST(3 == (uintptr_t)first_dlist(&list)) ;
   list.last = 0 ;
   TEST(0 == first_dlist(&list)) ;

   // TEST last_slist
   TEST(0 == last_dlist(&list)) ;
   list.last = (void*) 4 ;
   TEST(4 == (uintptr_t)last_dlist(&list)) ;
   list.last = 0 ;
   TEST(0 == last_dlist(&list)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_dlistiterator(void)
{
   dlist_t     list = dlist_INIT ;
   testnode_t  nodes[999] ;

   memset(nodes, 0, sizeof(nodes)) ;

   // TEST foreach, foreachReverse: empty iteration
   for (unsigned i = 0; 0 == i; i=1) {
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i].node) ;
         ++ i ;
      }
      TEST(0 == i) ;

      foreachReverse (_dlist, &list, node) {
         TEST(node == &nodes[i].node) ;
         ++ i ;
      }
      TEST(0 == i) ;
   }

   // TEST foreach, foreachReverse: single element iteration
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   for (unsigned i = 0; 0 == i; i=1) {
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[0].node) ;
         ++ i ;
      }
      TEST(i == 1) ;

      foreachReverse (_dlist, &list, node) {
         TEST(node == &nodes[0].node) ;
         -- i ;
      }
      TEST(i == 0) ;
   }
   TEST(0 == remove_dlist(&list, &nodes[0].node)) ;

   // TEST foreach, foreachReverse: loop over all elements
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_dlist(&list, &nodes[i].node)) ;
   }
   TEST(first_dlist(&list) == &nodes[0].node) ;
   TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-1].node) ;
   for (unsigned i = 0; 0 == i; i=1) {
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i++].node) ;
      }
      TEST(i == nrelementsof(nodes)) ;

      foreachReverse (_dlist, &list, node) {
         TEST(node == &nodes[--i].node) ;
      }
      TEST(i == 0) ;
   }
   TEST(0 == removeall_dlist(&list, 0)) ;

   // TEST foreach, foreachReverse: loop over all elements (reverse)
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_dlist(&list, &nodes[i].node)) ;
   }
   TEST(first_dlist(&list) == &nodes[nrelementsof(nodes)-1].node) ;
   TEST(last_dlist(&list)  == &nodes[0].node) ;
   for (unsigned i = 0; 0 == i; i=1) {
      foreachReverse (_dlist, &list, node) {
         TEST(node == &nodes[i++].node) ;
      }
      TEST(i == nrelementsof(nodes)) ;

      foreach (_dlist, &list, node) {
         TEST(node == &nodes[--i].node) ;
      }
      TEST(i == 0) ;
   }
   TEST(0 == removeall_dlist(&list, 0)) ;

   // TEST foreach, foreachReverse: remove current node; single element
   for (unsigned i = 0; 0 == i; i=1) {
      TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[0].node) ;
         TEST(0 == remove_dlist(&list, node)) ;
         TEST(isempty_dlist(&list)) ;
         ++ i ;
      }
      TEST(i == 1) ;

      TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
      foreachReverse (_dlist, &list, node) {
         TEST(node == &nodes[0].node) ;
         TEST(0 == remove_dlist(&list, node)) ;
         TEST(isempty_dlist(&list)) ;
         -- i ;
      }
      TEST(i == 0) ;
   }

   // TEST foreach, foreachReverse: removing all current nodes
   for (unsigned i = 0; 0 == i; i=1) {
      for(unsigned ni = 0; ni < nrelementsof(nodes); ++ni) {
         TEST(0 == insertlast_dlist(&list, &nodes[ni].node)) ;
      }
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i++].node) ;
         TEST(0 == remove_dlist(&list, node)) ;
      }
      TEST(isempty_dlist(&list)) ;
      TEST(i == nrelementsof(nodes)) ;

      for(unsigned ni = 0; ni < nrelementsof(nodes); ++ni) {
         TEST(0 == insertlast_dlist(&list, &nodes[ni].node)) ;
      }
      foreachReverse (_dlist, &list, node) {
         TEST(node == &nodes[--i].node) ;
         TEST(0 == remove_dlist(&list, node)) ;
      }
      TEST(isempty_dlist(&list)) ;
      TEST(i == 0) ;
   }

   // TEST foreach, foreachReverse: removing every 2nd node
   for (unsigned i = 0; 0 == i; i=1) {
      for(unsigned ni = 0; ni < nrelementsof(nodes); ++ni) {
         TEST(0 == insertlast_dlist(&list, &nodes[ni].node)) ;
      }
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i++].node) ;
         if ((i&0x01)) {
            TEST(0 == remove_dlist(&list, node)) ;
         }
      }
      TEST(i == nrelementsof(nodes)) ;
      i = 0 ;
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i+1].node) ;
         i += 2 ;
      }
      TEST(i == nrelementsof(nodes) - (0x01 & nrelementsof(nodes))) ;

      removeall_dlist(&list, 0) ;
      for(unsigned ni = 0; ni < nrelementsof(nodes); ++ni) {
         TEST(0 == insertlast_dlist(&list, &nodes[ni].node)) ;
      }
      i = nrelementsof(nodes) ;
      foreachReverse (_dlist, &list, node) {
         TEST(node == &nodes[--i].node) ;
         if ((i&0x01)) {
            TEST(0 == remove_dlist(&list, node)) ;
         }
      }
      TEST(i == 0) ;
      i = nrelementsof(nodes) ;
      foreachReverse (_dlist, &list, node) {
         TEST(node == &nodes[i-1-(0x01 & (nrelementsof(nodes)-1))].node) ;
         i -= 2 ;
      }
      TEST(i == -(unsigned)(0x01 & nrelementsof(nodes))) ;
   }

   // ! removing of other nodes than the current is not supported !

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_insertremove(void)
{
   testadapt_t          typeadapt = { typeadapt_INIT_LIFETIME(0, &freenode_testdapt), test_errortimer_INIT_FREEABLE, 0 } ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, void), 0) ;
   dlist_t              list = dlist_INIT ;
   testnode_t           nodes[1000] ;
   dlist_node_t       * removed_node ;

   memset(nodes, 0, sizeof(nodes)) ;

   // TEST insertfirst_dlist: single element
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   TEST(nodes[0].node.next == &nodes[0].node) ;
   TEST(nodes[0].node.prev == &nodes[0].node) ;
   TEST(last_dlist(&list)  == &nodes[0].node) ;
   TEST(first_dlist(&list) == &nodes[0].node) ;
   TEST(0 == removefirst_dlist(&list, &removed_node)) ;
   TEST(0 == nodes[0].is_freed) ;
   TEST(removed_node == &nodes[0].node) ;
   TEST(1 == isempty_dlist(&list)) ;
   TEST(0 == last_dlist(&list)) ;
   TEST(0 == first_dlist(&list)) ;

   // TEST insertfirst_dlist: whole array
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_dlist(&list, &nodes[i].node)) ;
      TEST(nodes[i].node.next != 0) ;
      TEST(nodes[i].node.prev != 0) ;
      TEST(last_dlist(&list)  == &nodes[0].node) ;
      TEST(first_dlist(&list) == &nodes[i].node) ;
   }
   for(unsigned i = nrelementsof(nodes); (i--) > 0; ) {
      TEST(nodes[i].node.next == &nodes[(i?i:nrelementsof(nodes))-1].node) ;
      TEST(nodes[i].node.prev == &nodes[(i+1)%nrelementsof(nodes)].node) ;
   }

   // TEST removeall_dlist: free all objects
   typeadapt.freenode_count = 0 ;
   TEST(0 == removeall_dlist(&list, &nodeadapt)) ;
   TEST(0 == list.last) ;
   TEST(nrelementsof(nodes) == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST removeall_dlist: no free called if parameter 0
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   TEST(0 == insertfirst_dlist(&list, &nodes[1].node)) ;
   typeadapt.freenode_count = 0 ;
   TEST(0 == removeall_dlist(&list, 0)) ;
   TEST(0 == list.last) ;
   TEST(0 == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < 1; ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST removeall_dlist: no free called if typeadapt_t.lifetime.delete_object set to 0
   testadapt_t  old_typeadapt = typeadapt ;
   typeadapt.lifetime.delete_object = 0 ;
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   TEST(0 == insertfirst_dlist(&list, &nodes[1].node)) ;
   typeadapt.freenode_count = 0 ;
   TEST(0 == removeall_dlist(&list, &nodeadapt)) ;
   typeadapt = old_typeadapt ;
   TEST(0 == list.last) ;
   TEST(0 == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < 1; ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST insertlast_dlist: single element
   TEST(0 == insertlast_dlist(&list, &nodes[0].node)) ;
   TEST(nodes[0].node.next == &nodes[0].node) ;
   TEST(nodes[0].node.prev == &nodes[0].node) ;
   TEST(last_dlist(&list)  == &nodes[0].node) ;
   TEST(first_dlist(&list) == &nodes[0].node) ;
   TEST(0 == removelast_dlist(&list, &removed_node)) ;
   TEST(0 == nodes[0].is_freed) ;
   TEST(removed_node == &nodes[0].node) ;
   TEST(1 == isempty_dlist(&list)) ;
   TEST(0 == last_dlist(&list)) ;
   TEST(0 == first_dlist(&list)) ;

   // TEST insertlast_dlist: whole array
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_dlist(&list, &nodes[i].node)) ;
      TEST(nodes[i].node.next != 0) ;
      TEST(nodes[i].node.prev != 0) ;
      TEST(first_dlist(&list) == &nodes[0].node) ;
      TEST(last_dlist(&list)  == &nodes[i].node) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[i].node.prev == &nodes[(i?i:nrelementsof(nodes))-1].node) ;
      TEST(nodes[i].node.next == &nodes[(i+1)%nrelementsof(nodes)].node) ;
   }
   typeadapt.freenode_count = 0 ;
   TEST(0 == free_dlist(&list, &nodeadapt)) ;
   TEST(nrelementsof(nodes) == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST insertafter_dlist
   init_dlist(&list) ;
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   for(unsigned i = 2; i < nrelementsof(nodes); i+=2) {
      TEST(last_dlist(&list) == &nodes[i-2].node) ;
      TEST(0 == insertafter_dlist(&list, &nodes[i-2].node, &nodes[i].node)) ;
      TEST(last_dlist(&list)  == &nodes[i].node) ;
      TEST(first_dlist(&list) == &nodes[0].node) ;
   }
   for(unsigned i = 1; i < nrelementsof(nodes); i+=2) {
      TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-2].node) ;
      TEST(0 == insertafter_dlist(&list, &nodes[i-1].node, &nodes[i].node)) ;
      TEST(first_dlist(&list) == &nodes[0].node) ;
   }
   TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-1].node) ;
   for (unsigned i = 0; (i == 0); i = 1) {
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i].node) ;
         TEST(node->prev == &nodes[(i?i:nrelementsof(nodes))-1].node) ;
         TEST(node->next == &nodes[(i+1)%nrelementsof(nodes)].node) ;
         ++ i ;
      }
      TEST(i == nrelementsof(nodes)) ;
   }
   typeadapt.freenode_count = 0 ;
   TEST(0 == removeall_dlist(&list, &nodeadapt)) ;
   TEST(nrelementsof(nodes) == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST insertbefore_dlist
   TEST(0 == insertfirst_dlist(&list, &nodes[nrelementsof(nodes)-2].node)) ;
   for(unsigned i = nrelementsof(nodes)-2; i >= 2; i-=2) {
      TEST(first_dlist(&list) == &nodes[i].node) ;
      TEST(0 == insertbefore_dlist(&list, &nodes[i].node, &nodes[i-2].node)) ;
      TEST(first_dlist(&list) == &nodes[i-2].node) ;
      TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-2].node) ;
   }
   for(unsigned i = 1; i < nrelementsof(nodes); i+=2) {
      if (i+1 == nrelementsof(nodes)) {
         TEST(0 == insertafter_dlist(&list, &nodes[i-1].node, &nodes[i].node)) ;
         TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-1].node) ;
      } else {
         TEST(0 == insertbefore_dlist(&list, &nodes[i+1].node, &nodes[i].node)) ;
         TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-2].node) ;
      }
      TEST(first_dlist(&list) == &nodes[0].node) ;
   }
   for (unsigned i = 0; (i == 0); i = 1) {
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i].node) ;
         TEST(node->prev == &nodes[(i?i:nrelementsof(nodes))-1].node) ;
         TEST(node->next == &nodes[(i+1)%nrelementsof(nodes)].node) ;
         ++ i ;
      }
      TEST(i == nrelementsof(nodes)) ;
   }
   typeadapt.freenode_count = 0 ;
   TEST(0 == free_dlist(&list, &nodeadapt)) ;
   TEST(nrelementsof(nodes) == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST removefirst_dlist
   init_dlist(&list) ;
   typeadapt.freenode_count = 0 ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_dlist(&list, &nodes[i].node)) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(first_dlist(&list) == &nodes[i].node) ;
      TEST(last_dlist(&list)  == nodes[i].node.prev) ;
      TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-1].node) ;
      TEST(first_dlist(&list) == nodes[nrelementsof(nodes)-1].node.next) ;
      TEST(0 == removefirst_dlist(&list, &removed_node)) ;
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(removed_node == &nodes[i].node) ;
   }
   TEST(0 == first_dlist(&list)) ;
   TEST(0 == last_dlist(&list)) ;
   TEST(0 == typeadapt.freenode_count) ;

   // TEST removelast_dlist
   typeadapt.freenode_count = 0 ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_dlist(&list, &nodes[i].node)) ;
   }
   for(unsigned i = nrelementsof(nodes); (i--) > 0;) {
      TEST(first_dlist(&list) == &nodes[0].node) ;
      TEST(last_dlist(&list)  == nodes[0].node.prev) ;
      TEST(last_dlist(&list)  == &nodes[i].node) ;
      TEST(first_dlist(&list) == nodes[i].node.next) ;
      TEST(0 == removelast_dlist(&list, &removed_node)) ;
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(removed_node == &nodes[i].node) ;
   }
   TEST(0 == first_dlist(&list)) ;
   TEST(0 == last_dlist(&list)) ;
   TEST(0 == typeadapt.freenode_count) ;

   // TEST remove_dlist: node after (next node)
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_dlist(&list, &nodes[i].node)) ;
   }
   TEST(&nodes[1].node == next_dlist(&nodes[0].node)) ;
   TEST(0 == remove_dlist(&list, next_dlist(&nodes[0].node))) ;
   TEST(0 == nodes[1].node.next) ;
   TEST(0 == nodes[1].node.prev) ;
   TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-1].node) ;
   TEST(first_dlist(&list) == &nodes[0].node) ;
   TEST(first_dlist(&list) == next_dlist(last_dlist(&list))) ;
   TEST(0 == remove_dlist(&list, next_dlist(last_dlist(&list)))) ;
   TEST(0 == nodes[0].node.next) ;
   TEST(0 == nodes[0].node.prev) ;
   TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-1].node) ;
   TEST(first_dlist(&list) == &nodes[2].node) ;
   TEST(last_dlist(&list)  == next_dlist(&nodes[nrelementsof(nodes)-2].node)) ;
   TEST(0 == remove_dlist(&list, next_dlist(&nodes[nrelementsof(nodes)-2].node))) ;
   TEST(0 == nodes[nrelementsof(nodes)-1].node.next) ;
   TEST(0 == nodes[nrelementsof(nodes)-1].node.prev) ;
   TEST(first_dlist(&list) == &nodes[2].node) ;
   TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-2].node) ;
   for(unsigned i = 2; i < nrelementsof(nodes)-2; i+=2) {
      TEST(0 == remove_dlist(&list, next_dlist(&nodes[i].node))) ;
      TEST(0 == nodes[i+1].node.next) ;
      TEST(0 == nodes[i+1].node.prev) ;
      TEST(first_dlist(&list) == &nodes[2].node) ;
      TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-2].node) ;
   }
   for (unsigned i = 2; i == 2; ) {
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i].node) ;
         TEST(node->prev == &nodes[(i>2)?i-2:nrelementsof(nodes)-2].node) ;
         TEST(node->next == &nodes[(i<nrelementsof(nodes)-2)?i+2:2].node) ;
         i += 2 ;
      }
      TEST(i == nrelementsof(nodes)) ;
   }
   for(unsigned i = nrelementsof(nodes)-4; i >= 2; i-=2) {
      TEST(next_dlist(&nodes[i].node) == &nodes[i+2].node) ;
      TEST(0 == remove_dlist(&list, next_dlist(&nodes[i].node))) ;
      TEST(0 == nodes[i+2].node.next) ;
      TEST(0 == nodes[i+2].node.prev) ;
   }
   // if there is only one node remove removes last
   TEST(first_dlist(&list) == &nodes[2].node) ;
   TEST(last_dlist(&list) == &nodes[2].node) ;
   TEST(0 == remove_dlist( &list, next_dlist(&nodes[2].node))) ;
   TEST(0 == first_dlist(&list)) ;
   TEST(0 == last_dlist(&list)) ;
   TEST(1 == isempty_dlist(&list)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(0 == nodes[i].is_freed) ;
   }
   TEST(0 == typeadapt.freenode_count) ;

   // TEST remove_dlist: node before (prev node)
   TEST(0 == insertlast_dlist(&list, &nodes[nrelementsof(nodes)-1].node)) ;
   for(unsigned i = nrelementsof(nodes)-1; (i--) > 0;) {
      TEST(0 == insertbefore_dlist(&list, &nodes[i+1].node, &nodes[i].node)) ;
   }
   TEST(prev_dlist(last_dlist(&list)) == &nodes[nrelementsof(nodes)-2].node) ;
   TEST(0 == remove_dlist(&list, prev_dlist(last_dlist(&list)))) ;
   TEST(0 == nodes[nrelementsof(nodes)-2].node.next) ;
   TEST(0 == nodes[nrelementsof(nodes)-2].node.prev) ;
   TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-1].node) ;
   TEST(first_dlist(&list) == &nodes[0].node) ;
   TEST(0 == remove_dlist(&list, prev_dlist(first_dlist(&list)))) ;
   TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-3].node) ;
   for(unsigned i = 0; i < nrelementsof(nodes)-2; i+=2) {
      TEST(prev_dlist(&nodes[i].node) = &nodes[i?i-1:nrelementsof(nodes)-3].node) ;
      TEST(0 == remove_dlist(&list, prev_dlist(&nodes[i].node))) ;
      TEST(0 == nodes[i?i-1:nrelementsof(nodes)-3].node.next) ;
      TEST(0 == nodes[i?i-1:nrelementsof(nodes)-3].node.prev) ;
   }
   TEST(last_dlist(&list) == &nodes[nrelementsof(nodes)-4].node) ;
   for (unsigned i = 0; 0 == i; ) {
      foreach (_dlist, &list, node) {
         TEST(node == &nodes[i].node) ;
         TEST(node->prev == &nodes[i?i-2:nrelementsof(nodes)-4].node) ;
         TEST(node->next == &nodes[(i<nrelementsof(nodes)-4)?i+2:0].node) ;
         i += 2 ;
      }
      TEST(i == nrelementsof(nodes)-2) ;
   }
   for(unsigned i = 2; i <= nrelementsof(nodes)-4; i+=2) {
      TEST(0 == remove_dlist(&list, prev_dlist(&nodes[i].node))) ;
      TEST(0 == nodes[i-2].node.next) ;
      TEST(0 == nodes[i-2].node.prev) ;
   }
   // if there is only one node remove removes last
   TEST(first_dlist(&list) == &nodes[nrelementsof(nodes)-4].node) ;
   TEST(last_dlist(&list)  == &nodes[nrelementsof(nodes)-4].node) ;
   TEST(0 == remove_dlist( &list, prev_dlist(&nodes[nrelementsof(nodes)-4].node))) ;
   TEST(0 == first_dlist(&list)) ;
   TEST(0 == last_dlist(&list)) ;
   TEST(1 == isempty_dlist(&list)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(0 == nodes[i].is_freed) ;
   }
   TEST(0 == typeadapt.freenode_count) ;

   // TEST insertfirst_dlist, insertlast_dlist, removelast_dlist, removefirst_dlist, remove_dlist: random order
   srand(100) ;
   for (unsigned i = 0; i < 10000; ++i) {
      static_assert(100 <= nrelementsof(nodes), "no array overflow") ;
      unsigned id = (unsigned)rand() % 100u ;
      if (nodes[id].is_inserted) {
         nodes[id].is_inserted = 0 ;
         if (list.last == &nodes[id].node) {
            TEST(0 == removelast_dlist(&list, &removed_node)) ;
         } else if (list.last->next == &nodes[id].node) {
            TEST(0 == removefirst_dlist(&list, &removed_node)) ;
         } else {
            removed_node = &nodes[id].node ;
            remove_dlist(&list, removed_node) ;
         }
         TEST(removed_node == &nodes[id].node) ;
      } else {
         nodes[id].is_inserted = 1 ;
         if ((i & 0x01)) {
            TEST(0 == insertfirst_dlist(&list, &nodes[id].node)) ;
         } else {
            TEST(0 == insertlast_dlist(&list, &nodes[id].node)) ;
         }
      }
   }
   foreach (_dlist, &list, node) {
      TEST(1 == ((testnode_t*)(node->prev))->is_inserted) ;
      TEST(1 == ((testnode_t*)(node->next))->is_inserted) ;
   }
   while (!isempty_dlist(&list)) {
      dlist_node_t * const first = first_dlist(&list) ;
      TEST(0 == remove_dlist(&list, first)) ;
      ((testnode_t*)first)->is_inserted = 0 ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node.next) ;
      TEST(0 == nodes[i].node.prev) ;
      TEST(0 == nodes[i].is_freed) ;
      TEST(0 == nodes[i].is_inserted) ;
   }

   // TEST EINVAL
   dlist_t emptylist = dlist_INIT ;
   TEST(0 == insertfirst_dlist(&list, &nodes[0].node)) ;
   TEST(EINVAL == insertfirst_dlist(&list, &nodes[0].node)) ;
   TEST(EINVAL == insertlast_dlist(&list, &nodes[0].node)) ;
   TEST(EINVAL == insertafter_dlist(&list, &nodes[0].node, &nodes[0].node)) ;
   TEST(EINVAL == insertbefore_dlist(&list, &nodes[0].node, &nodes[0].node)) ;
   TEST(EINVAL == insertafter_dlist(&list, &nodes[1].node, &nodes[2].node)) ;
   TEST(EINVAL == insertbefore_dlist(&list, &nodes[1].node, &nodes[2].node)) ;
   TEST(EINVAL == insertafter_dlist(&emptylist, &nodes[0].node, &nodes[1].node)) ;
   TEST(EINVAL == insertbefore_dlist(&emptylist, &nodes[0].node, &nodes[1].node)) ;
   TEST(EINVAL == remove_dlist(&list, &nodes[1].node)) ;
   TEST(EINVAL == remove_dlist(&emptylist, &nodes[0].node)) ;
   TEST(EINVAL == removefirst_dlist(&emptylist, &removed_node)) ;
   TEST(EINVAL == removelast_dlist(&emptylist, &removed_node)) ;
   TEST(0 == remove_dlist(&list, &nodes[0].node)) ;

   return 0 ;
ONABORT:
   free_dlist(&list, &nodeadapt) ;
   return EINVAL ;
}

dlist_IMPLEMENT(_glist1, genericnode_t, node1)
dlist_IMPLEMENT(_glist2, genericnode_t, node2)

static int test_generic(void)
{
   genericadapt_t       typeadapt  = { typeadapt_INIT_LIFETIME(0, &freenode_genericadapt), test_errortimer_INIT_FREEABLE, 0 } ;
   typeadapt_member_t   nodeadapt1 = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, genericadapt_t, genericnode_t, void), offsetof(genericnode_t, node1)) ;
   typeadapt_member_t   nodeadapt2 = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, genericadapt_t, genericnode_t, void), offsetof(genericnode_t, node2)) ;
   dlist_t              list1 = dlist_INIT ;
   dlist_t              list2 = dlist_INIT ;
   genericnode_t        nodes[1000] ;
   genericnode_t        * removed_node ;

   memset(nodes, 0, sizeof(nodes)) ;

   // TEST asgeneric_dlist
   struct {
      dlist_node_t * last ;
   }  xlist ;
   TEST((dlist_t*)&xlist == asgeneric_dlist(&xlist)) ;

   // TEST empty list
   TEST(0 == first_glist1(&list1)) ;
   TEST(0 == last_glist1(&list1)) ;
   TEST(0 == first_glist2(&list1)) ;
   TEST(0 == last_glist2(&list1)) ;
   TEST(1 == isempty_glist2(&list1)) ;
   TEST(1 == isempty_glist2(&list1)) ;
   for (unsigned i = 0; !i; i =1) {
      foreach (_glist1, &list1, node) {
         ++ i ;
      }
      TEST(i == 0) ;
      foreach (_glist2, &list2, node) {
         ++ i ;
      }
      TEST(i == 0) ;
   }
   TEST(0 == free_glist1(&list1, 0)) ;
   TEST(0 == free_glist2(&list2, 0)) ;

   // TEST single element
   list1.last = (void*) 1 ;
   list2.last = (void*) 1 ;
   init_glist1(&list1) ;
   init_glist2(&list2) ;
   TEST(0 == list1.last) ;
   TEST(0 == list2.last) ;
   TEST(0 == memcmp(&nodes[0], &nodes[1], sizeof(nodes[0]))) ;
   TEST(0 == insertfirst_glist1(&list1, &nodes[0])) ;
   TEST(nodes[0].node1.next == &nodes[0].node1) ;
   TEST(nodes[0].node1.prev == &nodes[0].node1) ;
   TEST(0 == memcmp(&nodes[0].node2, &nodes[1].node2, sizeof(nodes[0].node2))) ;
   TEST(0 == insertfirst_glist2(&list2, &nodes[0])) ;
   TEST(nodes[0].node2.next == &nodes[0].node2) ;
   TEST(nodes[0].node2.prev == &nodes[0].node2) ;
   TEST(&nodes[0] == first_glist1(&list1)) ;
   TEST(&nodes[0] == last_glist1(&list1)) ;
   TEST(&nodes[0] == first_glist2(&list2)) ;
   TEST(&nodes[0] == last_glist2(&list2)) ;
   TEST(0 == isempty_glist1(&list1)) ;
   TEST(0 == isempty_glist2(&list2)) ;
   TEST(0 == free_glist1(&list1, &nodeadapt1)) ;
   TEST(1 == nodes[0].is_freed)
   TEST(1 == typeadapt.freenode_count) ;
   TEST(0 == free_glist2(&list2, &nodeadapt2)) ;
   TEST(2 == nodes[0].is_freed)
   TEST(2 == typeadapt.freenode_count) ;
   nodes[0].is_freed = 0 ;
   typeadapt.freenode_count = 0 ;
   TEST(0 == memcmp(&nodes[0], &nodes[1], sizeof(nodes[0]))) ;
   TEST(1 == isempty_glist1(&list1)) ;
   TEST(1 == isempty_glist2(&list2)) ;

   // TEST insertfirst_dlist
   TEST(0 == insertfirst_glist1(&list1, &nodes[1])) ;
   TEST(0 == insertfirst_glist2(&list2, &nodes[1])) ;
   TEST(0 == insertfirst_glist1(&list1, &nodes[0])) ;
   TEST(0 == insertfirst_glist2(&list2, &nodes[0])) ;
   TEST(&nodes[0] == first_glist1(&list1)) ;
   TEST(&nodes[1] == last_glist1(&list1)) ;
   TEST(&nodes[0] == first_glist2(&list2)) ;
   TEST(&nodes[1] == last_glist2(&list2)) ;

   // TEST insertlast_dlist
   TEST(0 == insertlast_glist1(&list1, &nodes[3])) ;
   TEST(0 == insertlast_glist2(&list2, &nodes[3])) ;
   TEST(&nodes[0] == first_glist1(&list1)) ;
   TEST(&nodes[3] == last_glist1(&list1)) ;
   TEST(&nodes[0] == first_glist2(&list2)) ;
   TEST(&nodes[3] == last_glist2(&list2)) ;

   // TEST insertbefore_dlist
   TEST(0 == insertbefore_glist1(&list1, &nodes[3], &nodes[2])) ;
   TEST(0 == insertbefore_glist2(&list2, &nodes[3], &nodes[2])) ;
   TEST(&nodes[2] == prev_glist1(&nodes[3])) ;
   TEST(&nodes[3] == last_glist1(&list1)) ;
   TEST(&nodes[2] == prev_glist2(&nodes[3])) ;
   TEST(&nodes[3] == last_glist2(&list2)) ;

   // TEST insertafter_dlist
   TEST(0 == insertafter_glist1(&list1, &nodes[3], &nodes[4])) ;
   TEST(0 == insertafter_glist2(&list2, &nodes[3], &nodes[4])) ;
   TEST(&nodes[0] == first_glist1(&list1)) ;
   TEST(&nodes[4] == last_glist1(&list1)) ;
   TEST(&nodes[0] == first_glist2(&list2)) ;
   TEST(&nodes[4] == last_glist2(&list2)) ;

   // TEST removefirst_dlist
   removed_node = 0 ;
   TEST(0 == removefirst_glist1(&list1, &removed_node)) ;
   TEST(&nodes[0] == removed_node) ;
   removed_node = 0 ;
   TEST(0 == removefirst_glist2(&list2, &removed_node)) ;
   TEST(&nodes[0] == removed_node) ;

   // TEST removelast_dlist
   removed_node = 0 ;
   TEST(0 == removelast_glist1(&list1, &removed_node)) ;
   TEST(&nodes[4] == removed_node) ;
   removed_node = 0 ;
   TEST(0 == removelast_glist2(&list2, &removed_node)) ;
   TEST(&nodes[4] == removed_node) ;

   // TEST remove_dlist
   TEST(&nodes[1] == first_glist1(&list1)) ;
   TEST(0 == remove_glist1(&list1, &nodes[1])) ;
   TEST(&nodes[2] == first_glist1(&list1)) ;
   TEST(&nodes[1] == first_glist2(&list2)) ;
   TEST(0 == remove_glist2(&list2, &nodes[1])) ;
   TEST(&nodes[2] == first_glist2(&list2)) ;

   // TEST free_dlist: no error
   typeadapt.freenode_count = 0 ;
   TEST(0 == free_glist1(&list1, &nodeadapt1)) ;
   TEST(2 == typeadapt.freenode_count) ;
   TEST(0 == free_glist2(&list2, &nodeadapt2)) ;
   TEST(4 == typeadapt.freenode_count) ;
   TEST(2 == nodes[2].is_freed) ;
   TEST(2 == nodes[3].is_freed) ;
   nodes[2].is_freed = 0 ;
   nodes[3].is_freed = 0 ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node1.next) ;
      TEST(0 == nodes[i].node1.prev) ;
      TEST(0 == nodes[i].node2.next) ;
      TEST(0 == nodes[i].node2.prev) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST free_dlist: error
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_glist1(&list1, &nodes[i])) ;
      TEST(0 == insertlast_glist2(&list2, &nodes[i])) ;
   }
   typeadapt.freenode_count = 0 ;
   init_testerrortimer(&typeadapt.errcounter, 5, ENOSYS) ;
   TEST(ENOSYS == free_glist1(&list1, &nodeadapt1)) ;
   TEST(1 == isempty_glist1(&list1)) ;
   TEST(nrelementsof(nodes)-1 == typeadapt.freenode_count) ;
   typeadapt.freenode_count = 0 ;
   init_testerrortimer(&typeadapt.errcounter, 5, EINVAL) ;
   TEST(EINVAL == free_glist2(&list2, &nodeadapt2)) ;
   TEST(1 == isempty_glist2(&list2)) ;
   TEST(nrelementsof(nodes)-1 == typeadapt.freenode_count) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node1.next) ;
      TEST(0 == nodes[i].node1.prev) ;
      TEST(0 == nodes[i].node2.next) ;
      TEST(0 == nodes[i].node2.prev) ;
      TEST(2*(i!=4) == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST iterator, next_dlist, prev_dlist
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_glist1(&list1, &nodes[i])) ;
      TEST(0 == insertfirst_glist2(&list2, &nodes[i])) ;
   }
   for(unsigned i = 0; !i; i=1) {
      foreachReverse (_glist1, &list1, node) {
         TEST(&nodes[i?i-1:nrelementsof(nodes)-1] == next_glist1(node)) ;
         TEST(node == &nodes[i++]) ;
      }
      TEST(i == nrelementsof(nodes)) ;
      foreach (_glist1, &list1, node) {
         TEST(&nodes[i%nrelementsof(nodes)] == prev_glist1(node)) ;
         TEST(node == &nodes[--i]) ;
      }
      TEST(i == 0) ;
      foreachReverse (_glist2, &list2, node) {
         TEST(&nodes[i?i-1:nrelementsof(nodes)-1] == next_glist2(node)) ;
         TEST(node == &nodes[i++]) ;
      }
      TEST(i == nrelementsof(nodes)) ;
      foreach (_glist2, &list2, node) {
         TEST(&nodes[i%nrelementsof(nodes)] == prev_glist2(node)) ;
         TEST(node == &nodes[--i]) ;
      }
      TEST(i == 0) ;
   }
   TEST(0 == free_glist1(&list1, 0)) ;
   TEST(0 == free_glist2(&list2, 0)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].node1.next) ;
      TEST(0 == nodes[i].node1.prev) ;
      TEST(0 == nodes[i].node2.next) ;
      TEST(0 == nodes[i].node2.prev) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_inmem_dlist()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_dlistiterator())  goto ONABORT ;
   if (test_insertremove())   goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
