/* title: SingleLinkedList impl
   Implements a linear linked list data structure.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/ds/inmem/slist.h
    Header file of <SingleLinkedList>.

   file: C-kern/ds/inmem/slist.c
    Implementation file of <SingleLinkedList impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/slist.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/errortimer.h"
#endif

// section: slist_t

// group: lifetime

int free_slist(slist_t * list, uint16_t nodeoffset, struct typeadapt_t * typeadp)
{
   int err ;
   slist_node_t * const last = list->last ;

   if (last) {
      slist_node_t * node ;
      slist_node_t * next = last->next ;

      list->last = 0 ;

      const bool isDelete = (typeadp && iscalldelete_typeadapt(typeadp)) ;

      err = 0 ;

      do {
         node = next ;
         next = node->next ;
         node->next = 0 ;
         if (isDelete) {
            typeadapt_object_t * delobj = memberasobject_typeadaptnodeoffset(nodeoffset, node) ;
            int err2 = calldelete_typeadapt(typeadp, &delobj) ;
            if (err2) err = err2 ;
         }
      } while (last != node) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

void insertfirst_slist(slist_t * list, struct slist_node_t * new_node)
{
   if (!list->last) {
      list->last = new_node ;
      new_node->next = new_node ;
   } else {
      new_node->next   = list->last->next ;
      list->last->next = new_node ;
   }
}

void insertlast_slist(slist_t * list, struct slist_node_t * new_node)
{
   if (!list->last) {
      list->last = new_node ;
      new_node->next = new_node ;
   } else {
      new_node->next   = list->last->next ;
      list->last->next = new_node ;
      list->last       = new_node ;
   }
}

void insertafter_slist(slist_t * list, struct slist_node_t * prev_node, struct slist_node_t * new_node)
{
   new_node->next  = prev_node->next ;
   prev_node->next = new_node ;
   if (list->last == prev_node) {
      list->last = new_node ;
   }
}

int removefirst_slist(slist_t * list, struct slist_node_t ** removed_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(! isempty_slist(list), ONABORT, ) ;

   struct slist_node_t * const last  = list->last ;
   struct slist_node_t * const first = last->next ;

   if (first == last) {
      list->last = 0 ;
   } else {
      last->next = first->next ;
   }

   first->next    = 0 ;
   *removed_node  = first ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removeafter_slist(slist_t * list, struct slist_node_t * prev_node, struct slist_node_t ** removed_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 != prev_node->next && ! isempty_slist(list), ONABORT, ) ;

   struct slist_node_t * const next = prev_node->next ;

   prev_node->next = next->next ;
   next->next      = 0 ;
   if (list->last == next) {
      if (list->last == prev_node) {
         list->last = 0 ;
      } else {
         list->last = prev_node ;
      }
   }

   *removed_node = next ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testnode_t        testnode_t ;
typedef struct testnode_adapt_t  testnode_adapt_t ;

struct testnode_t {
   int            dummy1 ;
   slist_node_t   * next ;
   int            is_freed ;
} ;

struct testnode_adapt_t {
   struct {
       typeadapt_EMBED(testnode_adapt_t, testnode_t, void*) ;
   } ;
   test_errortimer_t errcounter ;
} ;

static int impl_delete_testnodeadapt(testnode_adapt_t * typeadp, testnode_t ** node)
{
   int err = process_testerrortimer(&typeadp->errcounter) ;

   if (!err && *node) {
      ++ (*node)->is_freed ;
   }

   *node = 0 ;

   return err ;
}

static int test_initfree(void)
{
   slist_t            slist     = slist_INIT ;
   slist_node_t       node      = slist_node_INIT ;
   testnode_adapt_t   typeadapt = { typeadapt_INIT_LIFETIME(0, &impl_delete_testnodeadapt), test_errortimer_INIT_FREEABLE } ;
   typeadapt_t *      typeadp   = genericcast_typeadapt(&typeadapt, testnode_adapt_t, testnode_t, void*) ;
   testnode_t         nodes[100] = { { 0, 0, 0 } } ;

   // TEST slist_node_INIT
   TEST(0 == node.next) ;

   // TEST slist_INIT
   TEST(0 == slist.last) ;

   // TEST init_slist, double free_slist
   slist.last  = (void*) 1 ;
   init_slist(&slist) ;
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   TEST(0 == slist.last) ;

   // TEST initsingle_slist
   initsingle_slist(&slist, (struct slist_node_t*)&nodes[0].next) ;
   TEST(nodes[0].next == (struct slist_node_t*)&nodes[0].next) ;
   TEST(first_slist(&slist) == (struct slist_node_t*)&nodes[0].next) ;
   TEST(last_slist(&slist)  == (struct slist_node_t*)&nodes[0].next) ;

   // TEST free_slist: call free callback (single element)
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   TEST(0 == slist.last) ;
   TEST(0 == nodes[0].next) ;
   TEST(1 == nodes[0].is_freed) ;
   nodes[0].is_freed = 0 ;

   // TEST free_slist: call free callback
   init_slist(&slist) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 != nodes[i].next) ;
   }
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   TEST(0 == slist.last) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST free_slist: no free callback
   init_slist(&slist) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertfirst_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 != nodes[i].next) ;
   }
   TEST(0 == free_slist(&slist, 0, 0)) ;
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, 0, 0)) ;
   TEST(0 == slist.last) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST free_slist: ENOMEM
   init_slist(&slist) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertfirst_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
   }
   init_testerrortimer(&typeadapt.errcounter, 3, ENOMEM) ;
   TEST(ENOMEM == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST((lengthof(nodes)-3!=i) == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   return 0 ;
ONABORT:
   free_slist(&slist, offsetof(testnode_t, next), typeadp) ;
   return EINVAL ;
}

static int test_query(void)
{
   slist_t      slist = slist_INIT ;

   // TEST isempty_slist
   slist.last = (void*) 1 ;
   TEST(0 == isempty_slist(&slist)) ;
   slist.last = 0 ;
   TEST(1 == isempty_slist(&slist)) ;

   // TEST first_slist
   slist_node_t lastnode = slist_node_INIT ;
   slist.last = &lastnode ;
   TEST(0 == first_slist(&slist)) ;
   lastnode = (slist_node_t) { .next = (void*) 3 } ;
   TEST((void*)3  == first_slist(&slist)) ;
   lastnode = (slist_node_t) { .next = &lastnode } ;
   TEST(&lastnode == first_slist(&slist)) ;
   slist.last = 0 ;
   TEST(0 == first_slist(&slist)) ;

   // TEST last_slist
   lastnode = (slist_node_t) slist_node_INIT ;
   slist.last = &lastnode ;
   TEST(&lastnode == last_slist(&slist)) ;
   slist.last = (void*) 4 ;
   TEST((void*)4 == last_slist(&slist)) ;
   slist.last = 0 ;
   TEST(0 == last_slist(&slist)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_iterate(void)
{
   slist_t            slist = slist_INIT ;
   slist_iterator_t   iter  = slist_iterator_INIT_FREEABLE ;
   testnode_t         nodes[100] = { { 0, 0, 0 } } ;

   // prepare
   slist.last = (slist_node_t*) &nodes[0].next ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      nodes[(3*i)%lengthof(nodes)].next = (slist_node_t*)&nodes[(3*i+3)%lengthof(nodes)].next ;
   }

   // TEST slist_iterator_INIT_FREEABLE
   TEST(0 == iter.next) ;
   TEST(0 == iter.list) ;

   // TEST initfirst_slistiterator
   TEST(0 == initfirst_slistiterator(&iter, &slist)) ;
   TEST(iter.next == (slist_node_t*)&nodes[3].next) ;
   TEST(iter.list == &slist) ;

   // TEST free_slistiterator
   TEST(0 == free_slistiterator(&iter)) ;
   TEST(iter.next == 0) ;
   TEST(iter.list != 0/*not changed*/) ;

   // TEST foreach
   for (unsigned count = 0; 0 == count; count = 1) {
      foreach (_slist, node, &slist) {
         ++ count ;
         TEST(node == (slist_node_t*)&nodes[(3*count) % lengthof(nodes)].next) ;
      }
      TEST(count == lengthof(nodes)) ;
   }

   // TEST foreach: remove current element
   for (unsigned count = 0; 0 == count; count = 1) {
      slist_node_t * prev = last_slist(&slist) ;
      foreach (_slist, node, &slist) {
         ++ count ;
         TEST(node == (struct slist_node_t*)&nodes[(3*count) % lengthof(nodes)].next) ;
         slist_node_t * removed_node ;
         TEST(0 == removeafter_slist(&slist, prev, &removed_node)) ;
         TEST(node == removed_node) ;
      }
      TEST(count == lengthof(nodes)) ;
      TEST(isempty_slist(&slist)) ;
   }

   // unprepare
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_insertremove(void)
{
   slist_t            slist      = slist_INIT ;
   testnode_adapt_t   typeadapt  = { typeadapt_INIT_LIFETIME(0, &impl_delete_testnodeadapt), test_errortimer_INIT_FREEABLE } ;
   typeadapt_t *      typeadp    = genericcast_typeadapt(&typeadapt, testnode_adapt_t, testnode_t, void*) ;
   testnode_t         nodes[100] = { { 0, 0, 0 } } ;
   slist_node_t       * node     = 0 ;

   // prepare
   init_slist(&slist) ;

   // TEST insertfirst, removefirst single element
   insertfirst_slist(&slist, (struct slist_node_t*)&nodes[0].next) ;
   TEST(&nodes[0].next  == (void*)nodes[0].next) ;
   TEST(slist.last == (void*)&nodes[0].next) ;
   TEST(0 == removefirst_slist(&slist, &node)) ;
   TEST(0 == slist.last) ;
   TEST(node == (struct slist_node_t*)&nodes[0].next) ;
   TEST(0 == nodes[0].is_freed) ;

   // TEST insertlast, removefirst single element
   insertlast_slist(&slist, (struct slist_node_t*)&nodes[0].next) ;
   TEST(&nodes[0].next == (slist_node_t**)nodes[0].next) ;
   TEST(slist.last     == (slist_node_t*)&nodes[0].next) ;
   TEST(0 == removefirst_slist(&slist, &node)) ;
   TEST(0 == slist.last) ;
   TEST(node == (slist_node_t*)&nodes[0].next) ;
   TEST(0 == nodes[0].is_freed) ;

   // TEST insertafter, removeafter three elements
   insertlast_slist(&slist, (struct slist_node_t*)&nodes[0].next) ;
   TEST(slist.last  == (void*)&nodes[0].next) ;
   insertafter_slist(&slist, (struct slist_node_t*)&nodes[0].next, (struct slist_node_t*)&nodes[1].next) ;
   TEST(first_slist(&slist) == (void*)&nodes[0].next) ;
   TEST(last_slist(&slist)  == (void*)&nodes[1].next) ;
   insertafter_slist(&slist, (struct slist_node_t*)&nodes[0].next, (struct slist_node_t*)&nodes[2].next) ;
   TEST(&nodes[0].next == (void*)first_slist(&slist)) ;
   TEST(&nodes[1].next == (void*)last_slist(&slist)) ;
   TEST(&nodes[2].next == (void*)next_slist((slist_node_t*)&nodes[0].next)) ;
   TEST(&nodes[1].next == (void*)next_slist((slist_node_t*)&nodes[2].next)) ;
   TEST(0 == removeafter_slist(&slist, (struct slist_node_t*)&nodes[1].next, &node)) ;
   TEST(&nodes[2].next == (void*)first_slist(&slist)) ;
   TEST(&nodes[1].next == (void*)last_slist(&slist)) ;
   TEST(node == (slist_node_t*)&nodes[0].next) ;
   TEST(0 == removeafter_slist(&slist, (struct slist_node_t*)&nodes[2].next, &node)) ;
   TEST(&nodes[2].next == (void*)first_slist(&slist)) ;
   TEST(&nodes[2].next == (void*)last_slist(&slist)) ;
   TEST(node == (slist_node_t*)&nodes[1].next) ;
   TEST(0 == removeafter_slist(&slist, (struct slist_node_t*)&nodes[2].next, &node)) ;
   TEST(0 == last_slist(&slist)) ;
   TEST(node == (slist_node_t*)&nodes[2].next) ;
   for (int i = 0; i < 3; ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST insertfirst
   init_slist(&slist) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertfirst_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
      TEST(&nodes[i].next == (void*)first_slist(&slist)) ;
      TEST(&nodes[0].next == (void*)last_slist(&slist)) ;
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[(i+1)%lengthof(nodes)].next == (slist_node_t*)&nodes[i].next) ;
   }
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST insertlast
   init_slist(&slist) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
      TEST(&nodes[0].next == (void*)first_slist(&slist)) ;
      TEST(&nodes[i].next == (void*)last_slist(&slist)) ;
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[i].next == (slist_node_t*)&nodes[(i+1)%lengthof(nodes)].next) ;
   }
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST insertafter
   init_slist(&slist) ;
   insertfirst_slist(&slist, (struct slist_node_t*)&nodes[0].next) ;
   for (unsigned i = 2; i < lengthof(nodes); i += 2) {
      insertafter_slist(&slist, (struct slist_node_t*)&nodes[i-2].next, (struct slist_node_t*)&nodes[i].next) ;
      TEST(&nodes[0].next == (void*)first_slist(&slist)) ;
      TEST(&nodes[i].next == (void*)last_slist(&slist)) ;
   }
   for (unsigned i = 1; i < lengthof(nodes); i += 2) {
      insertafter_slist(&slist, (struct slist_node_t*)&nodes[i-1].next, (struct slist_node_t*)&nodes[i].next) ;
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[i].next == (slist_node_t*)&nodes[(i+1)%lengthof(nodes)].next) ;
   }
   TEST(&nodes[0].next                     == (void*)first_slist(&slist)) ;
   TEST(&nodes[lengthof(nodes)-1].next == (void*)last_slist(&slist)) ;
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST removefirst
   init_slist(&slist) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(&nodes[i].next                     == (void*)first_slist(&slist)) ;
      TEST(&nodes[lengthof(nodes)-1].next == (void*)last_slist(&slist)) ;
      TEST(0 == removefirst_slist(&slist, &node)) ;
      TEST(node == (slist_node_t*)&nodes[i].next)
   }
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST removeafter
   init_slist(&slist) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
   }
   for (unsigned i = 0; i < lengthof(nodes)-1; i += 2) {
      TEST(&nodes[0].next                     == (void*)first_slist(&slist)) ;
      TEST(&nodes[lengthof(nodes)-1].next == (void*)last_slist(&slist)) ;
      TEST(0    == removeafter_slist(&slist, (struct slist_node_t*)&nodes[i].next, &node)) ;
      TEST(node == (slist_node_t*)&nodes[i+1].next)
   }
   for (unsigned i = lengthof(nodes)-2; i > 1; i -= 2) {
      TEST(&nodes[0].next == (void*)first_slist(&slist)) ;
      TEST(&nodes[i].next == (void*)last_slist(&slist)) ;
      TEST(0         == removeafter_slist(&slist, (struct slist_node_t*)&nodes[i-2].next, &node)) ;
      TEST(node      == (slist_node_t*)&nodes[i].next)
   }
   TEST(0 == removeafter_slist(&slist, (struct slist_node_t*)&nodes[0].next, &node)) ;
   TEST(node == (slist_node_t*)&nodes[0].next)
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST removeall
   init_slist(&slist) ;
   for (unsigned i = 0; i < lengthof(nodes)/2; ++i) {
      insertlast_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
   }
   for (unsigned i = lengthof(nodes)/2; i < lengthof(nodes); ++i) {
      insertfirst_slist(&slist, (struct slist_node_t*)&nodes[i].next) ;
   }
   TEST(&nodes[lengthof(nodes)-1].next   == (void*)first_slist(&slist)) ;
   TEST(&nodes[lengthof(nodes)/2-1].next == (void*)last_slist(&slist)) ;
   TEST(0 == removeall_slist(&slist, offsetof(testnode_t, next), typeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST EINVAL
   init_slist(&slist) ;
   insertfirst_slist(&slist, (slist_node_t*)&nodes[1].next) ;
   TEST(EINVAL == removeafter_slist(&slist, (slist_node_t*)&nodes[0].next, &node)) ;
   slist.last = 0 ;
   TEST(EINVAL == removefirst_slist(&slist, &node)) ;
   TEST(EINVAL == removeafter_slist(&slist, (slist_node_t*)&nodes[1].next, &node)) ;

   return 0 ;
ONABORT:
   free_slist(&slist, 0, 0) ;
   return EINVAL ;
}

typedef struct gnode_t    gnode_t ;

struct gnode_t {
   long      marker1 ;
   slist_node_EMBED(next) ;
   long      marker2 ;
   slist_node_t next2 ;
   long      marker3 ;
   int       is_freed ;
} ;

slist_IMPLEMENT(_slist1, gnode_t, next)
slist_IMPLEMENT(_slist2, gnode_t, next2.next)

typedef struct gnodeadapter_t    gnodeadapter_t ;

struct gnodeadapter_t {
   struct {
      typeadapt_EMBED(gnodeadapter_t, gnode_t, void*) ;
   } ;
   test_errortimer_t errcounter ;
   unsigned          freenode_count ;
} ;

static int impl_deleteobject_gnodeadapter(gnodeadapter_t * typeadp, gnode_t ** node)
{
   int err = process_testerrortimer(&typeadp->errcounter) ;

   if (!err && *node) {
      ++ typeadp->freenode_count ;
      ++ (*node)->is_freed ;
   }

   *node = 0 ;

   return err ;
}

static int test_generic(void)
{
   slist_t              slist1     = slist_INIT ;
   slist_t              slist2     = slist_INIT ;
   gnodeadapter_t       typeadapt  = { typeadapt_INIT_LIFETIME(0, &impl_deleteobject_gnodeadapter), test_errortimer_INIT_FREEABLE, 0 } ;
   typeadapt_t *        typeadp    = genericcast_typeadapt(&typeadapt, gnodeadapter_t, gnode_t, void*) ;
   gnode_t              nodes[100] = { { 0, 0, 0, {0}, 0, 0 } } ;
   gnode_t *            removed_node ;

   // TEST slist_node_EMBED
   static_assert(offsetof(gnode_t, next) == sizeof(((gnode_t*)0)->marker1), "next after marker1 defined") ;
   static_assert(offsetof(gnode_t, next)+sizeof(((gnode_t*)0)->next) == offsetof(gnode_t, marker2), "next before marker2 defined") ;
   static_assert(sizeof(((gnode_t*)0)->next) == sizeof(void*), "next is pointer") ;

   // TEST genericcast_slist
   struct {
      slist_node_t * last ;
   }  xlist ;
   TEST((slist_t*)&xlist == genericcast_slist(&xlist)) ;

   // TEST empty list
   TEST(0 == first_slist1(&slist1)) ;
   TEST(0 == last_slist1(&slist1)) ;
   TEST(0 == first_slist2(&slist1)) ;
   TEST(0 == last_slist2(&slist1)) ;
   TEST(1 == isempty_slist2(&slist1)) ;
   TEST(1 == isempty_slist2(&slist1)) ;
   for (unsigned i = 0; !i; i =1) {
      foreach (_slist1, node, &slist1) {
         ++ i ;
      }
      TEST(i == 0) ;
      foreach (_slist2, node, &slist2) {
         ++ i ;
      }
      TEST(i == 0) ;
   }
   TEST(0 == free_slist1(&slist1, typeadp)) ;
   TEST(0 == free_slist2(&slist2, typeadp)) ;

   // TEST init_slist
   slist1.last = (void*) 1 ;
   slist2.last = (void*) 1 ;
   init_slist1(&slist1) ;
   init_slist2(&slist2) ;
   TEST(0 == slist1.last) ;
   TEST(0 == slist2.last) ;

   // TEST initsingle_slist
   initsingle_slist1(&slist1, &nodes[1]) ;
   initsingle_slist2(&slist2, &nodes[1]) ;
   TEST(nodes[1].next       == (slist_node_t*)&nodes[1].next) ;
   TEST(nodes[1].next2.next == (slist_node_t*)&nodes[1].next2.next) ;
   TEST(&nodes[1] == first_slist1(&slist1)) ;
   TEST(&nodes[1] == last_slist1(&slist1)) ;
   TEST(&nodes[1] == first_slist2(&slist2)) ;
   TEST(&nodes[1] == last_slist2(&slist2)) ;

   // TEST initfirst_slistiterator
   slist_iterator_t it1 = slist_iterator_INIT_FREEABLE ;
   slist_iterator_t it2 = slist_iterator_INIT_FREEABLE ;
   TEST(0 == initfirst_slist1iterator(&it1, &slist1)) ;
   TEST(it1.next == (slist_node_t*)&first_slist1(&slist1)->next) ;
   TEST(it1.list == &slist1) ;
   TEST(0 == initfirst_slist2iterator(&it2, &slist2)) ;
   TEST(it2.next == (slist_node_t*)&first_slist2(&slist2)->next2.next) ;
   TEST(it2.list == &slist2) ;

   // TEST free_slistiterator
   TEST(0 == free_slist1iterator(&it1)) ;
   TEST(it1.next == 0) ;
   TEST(it1.list != 0) ;
   TEST(0 == free_slist2iterator(&it2)) ;
   TEST(it2.next == 0) ;
   TEST(it2.list != 0) ;

   // TEST single element
   TEST(0 == free_slist1(&slist1, 0)) ;
   TEST(0 == free_slist2(&slist2, 0)) ;
   init_slist1(&slist1) ;
   init_slist2(&slist2) ;
   insertfirst_slist1(&slist1, &nodes[0]) ;
   TEST(nodes[0].next == (slist_node_t*)&nodes[0].next) ;
   TEST(0 == nodes[0].next2.next) ;
   insertfirst_slist2(&slist2, &nodes[0]) ;
   TEST(nodes[0].next2.next == (slist_node_t*)&nodes[0].next2.next) ;
   TEST(&nodes[0] == first_slist1(&slist1)) ;
   TEST(&nodes[0] == last_slist1(&slist1)) ;
   TEST(&nodes[0] == first_slist2(&slist2)) ;
   TEST(&nodes[0] == last_slist2(&slist2)) ;
   TEST(0 == isempty_slist1(&slist1)) ;
   TEST(0 == isempty_slist2(&slist2)) ;
   TEST(0 == free_slist1(&slist1, typeadp)) ;
   TEST(1 == nodes[0].is_freed)
   TEST(1 == typeadapt.freenode_count) ;
   TEST(0 == free_slist2(&slist2, typeadp)) ;
   TEST(2 == nodes[0].is_freed)
   TEST(2 == typeadapt.freenode_count) ;
   nodes[0].is_freed = 0 ;
   typeadapt.freenode_count = 0 ;
   TEST(0 == nodes[0].next) ;
   TEST(0 == nodes[0].next2.next) ;
   TEST(1 == isempty_slist1(&slist1)) ;
   TEST(1 == isempty_slist2(&slist2)) ;

   // TEST insertfirst_slist
   insertfirst_slist1(&slist1, &nodes[1]) ;
   insertfirst_slist2(&slist2, &nodes[1]) ;
   insertfirst_slist1(&slist1, &nodes[0]) ;
   insertfirst_slist2(&slist2, &nodes[0]) ;
   TEST(&nodes[0] == first_slist1(&slist1)) ;
   TEST(&nodes[1] == last_slist1(&slist1)) ;
   TEST(&nodes[0] == first_slist2(&slist2)) ;
   TEST(&nodes[1] == last_slist2(&slist2)) ;

   // TEST insertlast_slist
   insertlast_slist1(&slist1, &nodes[2]) ;
   insertlast_slist2(&slist2, &nodes[2]) ;
   TEST(&nodes[0] == first_slist1(&slist1)) ;
   TEST(&nodes[2] == last_slist1(&slist1)) ;
   TEST(&nodes[0] == first_slist2(&slist2)) ;
   TEST(&nodes[2] == last_slist2(&slist2)) ;

   // TEST insertafter_slist
   insertafter_slist1(&slist1, &nodes[2], &nodes[3]) ;
   insertafter_slist2(&slist2, &nodes[2], &nodes[3]) ;
   TEST(&nodes[0] == first_slist1(&slist1)) ;
   TEST(&nodes[3] == last_slist1(&slist1)) ;
   TEST(&nodes[0] == first_slist2(&slist2)) ;
   TEST(&nodes[3] == last_slist2(&slist2)) ;

   // TEST removefirst_slist
   removed_node = 0 ;
   TEST(0 == removefirst_slist1(&slist1, &removed_node)) ;
   TEST(&nodes[0] == removed_node) ;
   removed_node = 0 ;
   TEST(0 == removefirst_slist2(&slist2, &removed_node)) ;
   TEST(&nodes[0] == removed_node) ;

   // TEST removeafter_slist
   removed_node = 0 ;
   TEST(0 == removeafter_slist1(&slist1, &nodes[2], &removed_node)) ;
   TEST(&nodes[3] == removed_node) ;
   removed_node = 0 ;
   TEST(0 == removeafter_slist2(&slist2, &nodes[2], &removed_node)) ;
   TEST(&nodes[3] == removed_node) ;

   // TEST free_slist: no error
   typeadapt.freenode_count = 0 ;
   TEST(0 == free_slist1(&slist1, typeadp)) ;
   TEST(2 == typeadapt.freenode_count) ;
   TEST(0 == free_slist2(&slist2, typeadp)) ;
   TEST(4 == typeadapt.freenode_count) ;
   for (unsigned i = 1; i <= 2; ++i) {
      TEST(2 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].next2.next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST free_slist: error
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_slist1(&slist1, &nodes[i]) ;
      insertlast_slist2(&slist2, &nodes[i]) ;
   }
   typeadapt.freenode_count = 0 ;
   init_testerrortimer(&typeadapt.errcounter, 5, ENOSYS) ;
   TEST(ENOSYS == free_slist1(&slist1, typeadp)) ;
   TEST(1 == isempty_slist1(&slist1)) ;
   TEST(lengthof(nodes)-1 == typeadapt.freenode_count) ;
   typeadapt.freenode_count = 0 ;
   init_testerrortimer(&typeadapt.errcounter, 5, EINVAL) ;
   TEST(EINVAL == free_slist2(&slist2, typeadp)) ;
   TEST(1 == isempty_slist2(&slist2)) ;
   TEST(lengthof(nodes)-1 == typeadapt.freenode_count) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].next2.next) ;
      TEST(2*(i!=4) == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST iterator, next_dlist, prev_dlist
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_slist1(&slist1, &nodes[i]) ;
      insertfirst_slist2(&slist2, &nodes[i]) ;
   }
   for (unsigned i = 0; !i; i=1) {
      foreach (_slist1, node, &slist1) {
         TEST(&nodes[(i+1)%lengthof(nodes)] == next_slist1(node)) ;
         TEST(node == &nodes[i++]) ;
      }
      TEST(i == lengthof(nodes)) ;
      foreach (_slist2, node, &slist2) {
         -- i ;
         TEST(&nodes[i?i-1:lengthof(nodes)-1] == next_slist2(node)) ;
         TEST(node == &nodes[i]) ;
      }
      TEST(i == 0) ;
   }
   TEST(0 == free_slist1(&slist1, 0)) ;
   TEST(0 == free_slist2(&slist2, 0)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].next2.next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   return 0 ;
ONABORT:
   free_slist1(&slist1, 0) ;
   free_slist2(&slist2, 0) ;
   return EINVAL ;
}

int unittest_ds_inmem_slist()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_iterate())        goto ONABORT ;
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
