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
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

// section: slist_t

// group: helper

#define OFFSET(object)      addoffset_helper(object, offset_next)

static inline slist_node_t* addoffset_helper(slist_node_t * object, uint32_t offset_next) __attribute__ ((always_inline)) ;

static inline slist_node_t* addoffset_helper(slist_node_t * object, uint32_t offset_next)
{
   return ((slist_node_t*)(offset_next + (uint8_t*)object)) ;
}

// group: lifetime

int free_slist(slist_t * list, slist_freecb_t * freehandler, uint32_t offset_next)
{
   int err ;
   slist_node_t * const last = list->last ;

   if (last) {
      slist_node_t * node ;
      slist_node_t * next = OFFSET(last)->next ;

      list->last = 0 ;

      if (freehandler) {

         err = 0 ;

         do {
            node = next ;
            next = OFFSET(next)->next ;
            OFFSET(node)->next = 0 ;
            int err2 = freehandler->fct(freehandler, node) ;
            if (err2) err = err2 ;
         } while( last != node ) ;

         if (err) goto ABBRUCH ;

      } else {

         do {
            node = next ;
            next = OFFSET(next)->next ;
            OFFSET(node)->next = 0 ;
         } while( last != node ) ;

      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int insertfirst_slist(slist_t * list, slist_node_t * new_node, uint32_t offset_next)
{
   int err ;

   if (OFFSET(new_node)->next) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (!list->last) {
      list->last = new_node ;
      OFFSET(new_node)->next = new_node ;
   } else {
      OFFSET(new_node)->next   = OFFSET(list->last)->next ;
      OFFSET(list->last)->next = new_node ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int insertlast_slist(slist_t * list, slist_node_t * new_node, uint32_t offset_next)
{
   int err ;

   if (OFFSET(new_node)->next) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (!list->last) {
      list->last = new_node ;
      OFFSET(new_node)->next = new_node ;
   } else {
      OFFSET(new_node)->next   = OFFSET(list->last)->next ;
      OFFSET(list->last)->next = new_node ;
      list->last               = new_node ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int insertafter_slist(slist_t * list, slist_node_t * prev_node, slist_node_t * new_node, uint32_t offset_next)
{
   int err ;

   if (OFFSET(new_node)->next) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (!list->last) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   OFFSET(new_node)->next  = OFFSET(prev_node)->next ;
   OFFSET(prev_node)->next = new_node ;
   if (list->last == prev_node) {
      list->last = new_node ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int removefirst_slist(slist_t * list, slist_node_t ** removed_node, uint32_t offset_next)
{
   int err ;

   slist_node_t * last = list->last ;

   if (!last) {
      err = ESRCH ;
      goto ABBRUCH ;
   }

   slist_node_t * const first = OFFSET(last)->next ;

   if (first == last) {
      list->last = 0 ;
   } else {
      OFFSET(last)->next = OFFSET(first)->next ;
   }

   OFFSET(first)->next = 0 ;
   *removed_node       = first ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int removeafter_slist(slist_t * list, slist_node_t * prev_node, slist_node_t ** removed_node, uint32_t offset_next)
{
   int err ;

   slist_node_t * const next = OFFSET(prev_node)->next ;

   if (!next) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (!list->last) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   OFFSET(prev_node)->next = OFFSET(next)->next ;
   OFFSET(next)->next      = 0 ;
   if (list->last == next) {
      if (list->last == prev_node) {
         list->last = 0 ;
      } else {
         list->last = prev_node ;
      }
   }

   *removed_node = next ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct test_node_t    test_node_t ;

struct test_node_t {
   test_node_t * next ;
   int           is_freed ;
} ;

static int test_freecallback(slist_freecb_t * freehandler, slist_node_t * node)
{
   (void) freehandler ;
   ++ ((test_node_t*)(node))->is_freed ;
   return 0 ;
}

static int test_initfree(void)
{
   slist_t        slist       = slist_INIT ;
   slist_freecb_t freehandler = { &test_freecallback } ;
   test_node_t    nodes[100]  = { { 0, 0 } } ;

   // TEST static initializer
   TEST(0 == slist.last) ;

   // TEST init, double free
   slist.last  = (void*) 1 ;
   TEST(0 == init_slist(&slist)) ;
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   TEST(0 == slist.last) ;

   // TEST insert, double free
   slist.last  = (void*) 1 ;
   TEST(0 == init_slist(&slist)) ;
   TEST(0 == slist.last) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_slist(&slist, (slist_node_t*)&nodes[i], offsetof(test_node_t, next))) ;
   }
   {
      unsigned i = 0 ;
      foreach (_slist, &slist, next) {
         TEST(next == (slist_node_t*)&nodes[i]) ;
         ++ i ;
      }
      TEST(nrelementsof(nodes) == i) ;
   }
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   TEST(0 == slist.last) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next ) ;
      TEST(1 == nodes[i].is_freed ) ;
      nodes[i].is_freed = 0 ;
   }
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   TEST(0 == slist.last) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].is_freed ) ;
   }

   // TEST insert, removeall
   TEST(0 == init_slist(&slist)) ;
   TEST(0 == slist.last) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_slist(&slist, (slist_node_t*)&nodes[i], 0)) ;
   }
   {
      unsigned i = nrelementsof(nodes) ;
      foreach (_slist, &slist, next) {
         -- i ;
         TEST(next == (slist_node_t*)&nodes[i]) ;
      }
      TEST(0 == i) ;
   }
   TEST(0 == removeall_slist(&slist, &freehandler, 0)) ;
   TEST(0 == slist.last) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next ) ;
      TEST(1 == nodes[i].is_freed ) ;
      nodes[i].is_freed = 0 ;
   }
   TEST(0 == removeall_slist(&slist, &freehandler, 0)) ;
   TEST(0 == slist.last) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].is_freed ) ;
   }

   return 0 ;
ABBRUCH:
   free_slist(&slist, 0, 0) ;
   return EINVAL ;
}

static int test_query(void)
{
   slist_t        slist = slist_INIT ;

   // TEST isempty
   TEST(1 == isempty_slist(&slist)) ;
   slist.last = (void*) 1 ;
   TEST(0 == isempty_slist(&slist)) ;
   slist.last = 0 ;
   TEST(1 == isempty_slist(&slist)) ;

   // TEST getfirst
   TEST((void*)0 == first_slist(&slist, offsetof(test_node_t, next))) ;
   {
      slist_node_t lastnode = { .next = (void*) 3 } ;
      slist.last = &lastnode ;
      TEST((void*)3 == first_slist(&slist, offsetof(test_node_t, next))) ;
      slist.last = 0 ;
   }
   TEST((void*)0 == first_slist(&slist, offsetof(test_node_t, next))) ;

   // TEST getlast
   TEST((void*)0 == last_slist(&slist)) ;
   slist.last = (void*) 4 ;
   TEST((void*)4 == last_slist(&slist)) ;
   slist.last = 0 ;
   TEST((void*)0 == last_slist(&slist)) ;

   return 0 ;
ABBRUCH:
   free_slist(&slist, 0, 0) ;
   return EINVAL ;
}

static int test_iterate(void)
{
   slist_t        slist       = slist_INIT ;
   slist_freecb_t freehandler = { &test_freecallback } ;
   test_node_t    nodes[100]  = { { 0, 0 } } ;

   // prepare
   for(unsigned i = 0, idx = 3; i < nrelementsof(nodes); ++i, idx = (idx + 3) % nrelementsof(nodes)) {
      nodes[idx].next = &nodes[(idx + 3) % nrelementsof(nodes)] ;
   }

   // TEST iterate every third element
   {
      unsigned idx   = 3 ;
      unsigned count = 0 ;
      slist.last     = (slist_node_t*) &nodes[0] ;
      foreach (_slist, &slist, node) {
         TEST(node == (slist_node_t*)&nodes[idx]) ;
         idx = (idx + 3) % nrelementsof(nodes) ;
         ++ count ;
      }
      TEST(idx   == 3) ;
      TEST(count == nrelementsof(nodes)) ;
   }

   // unprepare
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_insertremove(void)
{
   slist_t        slist       = slist_INIT ;
   slist_freecb_t freehandler = { &test_freecallback } ;
   test_node_t    nodes[100]  = { { 0, 0 } } ;
   test_node_t    * node      = 0 ;

   // prepare
   TEST(0 == init_slist(&slist)) ;

   // TEST insertfirst, removefirst single element
   TEST(0 == insertfirst_slist(&slist, (slist_node_t*)&nodes[0], 0)) ;
   TEST(&nodes[0] == nodes[0].next) ;
   TEST(slist.last  == (slist_node_t*)&nodes[0]) ;
   TEST(0 == removefirst_slist(&slist, (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST(0 == slist.last) ;
   TEST(node == &nodes[0]) ;
   TEST(0 == nodes[0].is_freed) ;

   // TEST insertlast, removefirst single element
   TEST(0 == insertlast_slist(&slist, (slist_node_t*)&nodes[0], offsetof(test_node_t, next))) ;
   TEST(&nodes[0] == nodes[0].next) ;
   TEST(slist.last  == (slist_node_t*)&nodes[0]) ;
   TEST(0 == removefirst_slist(&slist, (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST(0 == slist.last) ;
   TEST(node == &nodes[0]) ;
   TEST(0 == nodes[0].is_freed) ;

   // TEST insertafter, removeafter three elements
   TEST(0 == insertlast_slist(&slist, (slist_node_t*)&nodes[0], offsetof(test_node_t, next))) ;
   TEST(slist.last  == (slist_node_t*)&nodes[0]) ;
   TEST(0 == insertafter_slist(&slist, (slist_node_t*)&nodes[0], (slist_node_t*)&nodes[1], offsetof(test_node_t, next))) ;
   TEST(first_slist(&slist, offsetof(test_node_t, next)) == (slist_node_t*)&nodes[0]) ;
   TEST(last_slist(&slist)  == (slist_node_t*)&nodes[1]) ;
   TEST(0 == insertafter_slist(&slist, (slist_node_t*)&nodes[0], (slist_node_t*)&nodes[2], offsetof(test_node_t, next))) ;
   TEST((slist_node_t*)&nodes[0] == first_slist(&slist, offsetof(test_node_t, next))) ;
   TEST((slist_node_t*)&nodes[1] == last_slist(&slist)) ;
   TEST(&nodes[2] == next_slist(&nodes[0], 0)) ;
   TEST(&nodes[1] == next_slist(&nodes[2], offsetof(test_node_t, next))) ;
   TEST(0 == removeafter_slist(&slist, (slist_node_t*)&nodes[1], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST((slist_node_t*)&nodes[2] == first_slist(&slist, offsetof(test_node_t, next))) ;
   TEST((slist_node_t*)&nodes[1] == last_slist(&slist)) ;
   TEST(node == &nodes[0]) ;
   TEST(0 == removeafter_slist(&slist, (slist_node_t*)&nodes[2], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST((slist_node_t*)&nodes[2] == first_slist(&slist, offsetof(test_node_t, next))) ;
   TEST((slist_node_t*)&nodes[2] == last_slist(&slist)) ;
   TEST(node == &nodes[1]) ;
   TEST(0 == removeafter_slist(&slist, (slist_node_t*)&nodes[2], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST(last_slist(&slist)  == 0) ;
   TEST(node == &nodes[2]) ;
   for(int i = 0; i < 3; ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST insertfirst
   TEST(0 == init_slist(&slist)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_slist(&slist, (slist_node_t*)&nodes[i], 0)) ;
      TEST((slist_node_t*)&nodes[i] == first_slist(&slist, offsetof(test_node_t, next))) ;
      TEST((slist_node_t*)&nodes[0] == last_slist(&slist)) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[(i+1)%nrelementsof(nodes)].next == &nodes[i]) ;
   }
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST insertlast
   TEST(0 == init_slist(&slist)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_slist(&slist, (slist_node_t*)&nodes[i], offsetof(test_node_t, next))) ;
      TEST((slist_node_t*)&nodes[0] == first_slist(&slist, offsetof(test_node_t, next))) ;
      TEST((slist_node_t*)&nodes[i] == last_slist(&slist)) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[i].next == &nodes[(i+1)%nrelementsof(nodes)]) ;
   }
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST insertafter
   TEST(0 == init_slist(&slist)) ;
   TEST(0 == insertfirst_slist(&slist, (slist_node_t*)&nodes[0], offsetof(test_node_t, next))) ;
   for(unsigned i = 2; i < nrelementsof(nodes); i += 2) {
      TEST(0 == insertafter_slist(&slist, (slist_node_t*)&nodes[i-2], (slist_node_t*)&nodes[i], offsetof(test_node_t, next))) ;
      TEST((slist_node_t*)&nodes[0] == first_slist(&slist, offsetof(test_node_t, next))) ;
      TEST((slist_node_t*)&nodes[i] == last_slist(&slist)) ;
   }
   for(unsigned i = 1; i < nrelementsof(nodes); i += 2) {
      TEST(0 == insertafter_slist(&slist, (slist_node_t*)&nodes[i-1], (slist_node_t*)&nodes[i], offsetof(test_node_t, next))) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[i].next == &nodes[(i+1)%nrelementsof(nodes)]) ;
   }
   TEST((slist_node_t*)&nodes[0] == first_slist(&slist, offsetof(test_node_t, next))) ;
   TEST((slist_node_t*)&nodes[nrelementsof(nodes)-1] == last_slist(&slist)) ;
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST removefirst
   TEST(0 == init_slist(&slist)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_slist(&slist, (slist_node_t*)&nodes[i], offsetof(test_node_t, next))) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST((slist_node_t*)&nodes[i] == first_slist(&slist, offsetof(test_node_t, next))) ;
      TEST((slist_node_t*)&nodes[nrelementsof(nodes)-1] == last_slist(&slist)) ;
      TEST(0 == removefirst_slist(&slist, (slist_node_t**)&node, offsetof(test_node_t, next))) ;
      TEST(node                == &nodes[i])
   }
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST removeafter
   TEST(0 == init_slist(&slist)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_slist(&slist, (slist_node_t*)&nodes[i], offsetof(test_node_t, next))) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes)-1; i += 2) {
      TEST((slist_node_t*)&nodes[0] == first_slist(&slist, offsetof(test_node_t, next))) ;
      TEST((slist_node_t*)&nodes[nrelementsof(nodes)-1] == last_slist(&slist)) ;
      TEST(0 == removeafter_slist(&slist, (slist_node_t*)&nodes[i], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
      TEST(node                == &nodes[i+1])
   }
   for(unsigned i = nrelementsof(nodes)-2; i > 1; i -= 2) {
      TEST((slist_node_t*)&nodes[0] == first_slist(&slist, offsetof(test_node_t, next))) ;
      TEST((slist_node_t*)&nodes[i] == last_slist(&slist)) ;
      TEST(0 == removeafter_slist(&slist, (slist_node_t*)&nodes[i-2], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
      TEST(node                == &nodes[i])
   }
   TEST(0 == removeafter_slist(&slist, (slist_node_t*)&nodes[0], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST(node == &nodes[0])
   TEST(0 == slist.last) ;
   TEST(0 == free_slist(&slist, &freehandler, 0)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST removeall
   TEST(0 == init_slist(&slist)) ;
   for(unsigned i = 0; i < nrelementsof(nodes)/2; ++i) {
      TEST(0 == insertlast_slist(&slist, (slist_node_t*)&nodes[i], offsetof(test_node_t, next))) ;
   }
   for(unsigned i = nrelementsof(nodes)/2; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_slist(&slist, (slist_node_t*)&nodes[i], offsetof(test_node_t, next))) ;
   }
   TEST((slist_node_t*)&nodes[nrelementsof(nodes)-1] == first_slist(&slist, offsetof(test_node_t, next))) ;
   TEST((slist_node_t*)&nodes[nrelementsof(nodes)/2-1] == last_slist(&slist)) ;
   TEST(0 == removeall_slist(&slist, &freehandler, 0)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST EINVAL
   TEST(0 == init_slist(&slist)) ;
   nodes[0].next = (void*) 1 ;
   TEST(EINVAL == removeafter_slist(&slist, (slist_node_t*)&nodes[0], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST(EINVAL == insertfirst_slist(&slist, (slist_node_t*)&nodes[0], offsetof(test_node_t, next))) ;
   TEST(EINVAL == insertlast_slist(&slist, (slist_node_t*)&nodes[0], offsetof(test_node_t, next))) ;
   TEST(0 == insertfirst_slist(&slist, (slist_node_t*)&nodes[1], offsetof(test_node_t, next))) ;
   TEST(EINVAL == insertafter_slist(&slist, (slist_node_t*)&nodes[1], (slist_node_t*)&nodes[0], offsetof(test_node_t, next))) ;
   nodes[0].next = (void*) 0 ;
   TEST(EINVAL == removeafter_slist(&slist, (slist_node_t*)&nodes[0], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST(0 == removefirst_slist(&slist, (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST(EINVAL == insertafter_slist(&slist, (slist_node_t*)&nodes[1], (slist_node_t*)&nodes[0], offsetof(test_node_t, next))) ;
   TEST(EINVAL == removeafter_slist(&slist, (slist_node_t*)&nodes[1], (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST ESRCH
   TEST(0 == init_slist(&slist)) ;
   TEST(ESRCH == removefirst_slist(&slist, (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   TEST(0 == insertfirst_slist(&slist, (slist_node_t*)&nodes[0], offsetof(test_node_t, next))) ;
   TEST(0 == removefirst_slist(&slist, (slist_node_t**)&node, offsetof(test_node_t, next))) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].next) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   return 0 ;
ABBRUCH:
   free_slist(&slist, 0, offsetof(test_node_t, next)) ;
   return EINVAL ;
}

typedef struct gnode_t    gnode_t ;

struct gnode_t {
   int       marker1 ;
   gnode_t * next ;
   int       marker2 ;
   gnode_t * list2_next ;
   int       marker3 ;
   int       is_freed ;
} ;

typedef struct gnode_freecb_t          gnode_freecb_t ;

struct gnode_freecb_t {
   int (*fct) (gnode_freecb_t * freehandler, gnode_t * node) ;
} ;

slist_DECLARE(slist1_t, gnode_t)
slist_DECLARE(slist2_t, gnode_t)
slist_IMPLEMENT(slist1_t, _slist1, next, gnode_freecb_t)
slist_IMPLEMENT(slist2_t, _slist2, list2_next, gnode_freecb_t)

static int gnode_freecb(gnode_freecb_t * freehandler, gnode_t * node)
{
   (void) freehandler ;
   ++ node->is_freed ;
   return 0 ;
}

static int check_gnodes(unsigned size, gnode_t nodes[size], int is_freed)
{
   for(unsigned i = 0; i < size; ++i) {
      if (nodes[i].marker1) goto ABBRUCH ;
      if (nodes[i].marker2) goto ABBRUCH ;
      if (nodes[i].marker3) goto ABBRUCH ;
      if (nodes[i].next)                  goto ABBRUCH ;
      if (nodes[i].list2_next)            goto ABBRUCH ;
      if (is_freed != nodes[i].is_freed)  goto ABBRUCH ;
      nodes[i].is_freed = 0 ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_generic(void)
{
   slist1_t       slist1      = slist_INIT ;
   slist2_t       slist2      = slist_INIT ;
   gnode_freecb_t freehandler = { &gnode_freecb } ;
   gnode_t        nodes[100]  = { { 0, 0, 0, 0, 0, 0 } } ;
   gnode_t        * node      = 0 ;

   // TEST query
   init_slist1(&slist1) ;
   init_slist2(&slist2) ;
   TEST(1 == isempty_slist1(&slist1)) ;
   TEST(1 == isempty_slist2(&slist2)) ;
   TEST(0 == first_slist1(&slist1)) ;
   TEST(0 == first_slist2(&slist2)) ;
   TEST(0 == last_slist1(&slist1)) ;
   TEST(0 == last_slist2(&slist2)) ;
   TEST(0 == insertfirst_slist1(&slist1, &nodes[0])) ;
   TEST(0 == insertfirst_slist2(&slist2, &nodes[0])) ;
   TEST(0 == insertlast_slist1(&slist1, &nodes[1])) ;
   TEST(0 == insertlast_slist2(&slist2, &nodes[1])) ;
   TEST(0 == isempty_slist1(&slist1)) ;
   TEST(0 == isempty_slist2(&slist2)) ;
   TEST(&nodes[0] == first_slist1(&slist1)) ;
   TEST(&nodes[0] == first_slist2(&slist2)) ;
   TEST(&nodes[1] == last_slist1(&slist1)) ;
   TEST(&nodes[1] == last_slist2(&slist2)) ;
   TEST(0 == removefirst_slist1(&slist1, &node)) ;
   TEST(node == &nodes[0]) ;
   TEST(0 == removefirst_slist2(&slist2, &node)) ;
   TEST(node == &nodes[0]) ;
   TEST(0 == removefirst_slist1(&slist1, &node)) ;
   TEST(node == &nodes[1]) ;
   TEST(0 == removefirst_slist2(&slist2, &node)) ;
   TEST(node == &nodes[1]) ;
   TEST(0 == check_gnodes(nrelementsof(nodes), nodes, 0)) ;

   // TEST iterate
   TEST(0 == init_slist(&slist1)) ;
   TEST(0 == init_slist(&slist2)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertlast_slist1(&slist1,  &nodes[i])) ;
      TEST(0 == insertfirst_slist2(&slist2, &nodes[i])) ;
   }
   {
      unsigned i = 0 ;
      gnode_t * nodex = first_slist1(&slist1) ;
      while( nodex ) {
         TEST(nodex == &nodes[i]) ;
         ++ i ;
         nodex = (nodex == last_slist1(&slist1)) ? 0 : next_slist1(nodex) ;
      }
      TEST(nrelementsof(nodes) == i) ;
   }
   {
      unsigned i = 0 ;
      foreach (_slist1, &slist1, nodex) {
         TEST(nodex == &nodes[i]) ;
         ++ i ;
      }
      TEST(nrelementsof(nodes) == i) ;
   }
   {
      unsigned i = nrelementsof(nodes) ;
      gnode_t * nodex = first_slist2(&slist2) ;
      while( nodex ) {
         -- i ;
         TEST(nodex == &nodes[i]) ;
         nodex = (nodex == last_slist2(&slist2)) ? 0 : next_slist2(nodex) ;
      }
      TEST(0 == i) ;
   }
   {
      unsigned i = nrelementsof(nodes) ;
      foreach (_slist2, &slist2, nodex) {
         -- i;
         TEST(nodex == &nodes[i]) ;
      }
      TEST(0 == i) ;
   }
   TEST(0 == free_slist1(&slist1, &freehandler)) ;
   TEST(0 == free_slist2(&slist2, &freehandler)) ;
   TEST(0 == check_gnodes(nrelementsof(nodes), nodes, 2)) ;

   // TEST insertXXX, free
   TEST(0 == init_slist(&slist1)) ;
   TEST(0 == init_slist(&slist2)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); i += 2) {
      TEST(0 == insertlast_slist1(&slist1, &nodes[i])) ;
      TEST(0 == insertfirst_slist2(&slist2, &nodes[i])) ;
   }
   for(unsigned i = 1; i < nrelementsof(nodes); i += 2) {
      TEST(0 == insertafter_slist1(&slist1, &nodes[i-1], &nodes[i])) ;
      if (i < nrelementsof(nodes)-1) {
         TEST(0 == insertafter_slist2(&slist2, &nodes[i+1], &nodes[i])) ;
      } else {
         TEST(0 == insertfirst_slist2(&slist2, &nodes[i])) ;
      }
   }
   {
      unsigned i = 0 ;
      foreach(_slist1, &slist1, node2) {
         TEST(node2 == &nodes[i]) ;
         ++ i ;
      }
      TEST(nrelementsof(nodes) == i) ;
   }
   {
      unsigned i = nrelementsof(nodes) ;
      foreach(_slist2, &slist2, node2) {
         -- i;
         TEST(node2 == &nodes[i]) ;
      }
      TEST(0 == i) ;
   }
   TEST(0 == free_slist1(&slist1, 0)) ;
   TEST(0 == free_slist2(&slist2, 0)) ;
   TEST(0 == check_gnodes(nrelementsof(nodes), nodes, 0)) ;

   // TEST removefirst
   TEST(0 == init_slist(&slist1)) ;
   TEST(0 == init_slist(&slist2)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_slist1(&slist1, &nodes[i])) ;
      TEST(0 == insertlast_slist2(&slist2, &nodes[i])) ;
   }
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == removefirst_slist1(&slist1, &node)) ;
      TEST(node == &nodes[nrelementsof(nodes)-1-i]) ;
      TEST(0 == removefirst_slist2(&slist2, &node)) ;
      TEST(node == &nodes[i]) ;
   }
   TEST(0 == check_gnodes(nrelementsof(nodes), nodes, 0)) ;

   // TEST removeafter
   TEST(0 == init_slist(&slist1)) ;
   TEST(0 == init_slist(&slist2)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_slist1(&slist1, &nodes[i])) ;
      TEST(0 == insertlast_slist2(&slist2, &nodes[i])) ;
   }
   for(unsigned i = 1; i < nrelementsof(nodes); ++i) {
      TEST(0 == removeafter_slist1(&slist1, &nodes[i], &node)) ;
      TEST(node == &nodes[i-1]) ;
      TEST(0 == removeafter_slist2(&slist2, &nodes[nrelementsof(nodes)-1-i], &node)) ;
      TEST(node == &nodes[nrelementsof(nodes)-i]) ;
   }
   TEST(0 == removefirst_slist1(&slist1, &node)) ;
   TEST(node == &nodes[nrelementsof(nodes)-1]) ;
   TEST(0 == removefirst_slist2(&slist2, &node)) ;
   TEST(node == &nodes[0]) ;
   TEST(0 == check_gnodes(nrelementsof(nodes), nodes, 0)) ;
   TEST(1 == isempty_slist(&slist1)) ;
   TEST(1 == isempty_slist(&slist2)) ;

   // TEST removeall
   TEST(0 == init_slist(&slist1)) ;
   TEST(0 == init_slist(&slist2)) ;
   for(unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insertfirst_slist1(&slist1, &nodes[i])) ;
      TEST(0 == insertlast_slist2(&slist2, &nodes[i])) ;
   }
   TEST(0 == removeall_slist1(&slist1, &freehandler)) ;
   TEST(0 == removeall_slist2(&slist2, &freehandler)) ;
   TEST(0 == check_gnodes(nrelementsof(nodes), nodes, 2)) ;

   return 0 ;
ABBRUCH:
   free_slist1(&slist1, 0) ;
   free_slist2(&slist2, 0) ;
   return EINVAL ;
}

int unittest_ds_inmem_slist()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_query())          goto ABBRUCH ;
   if (test_iterate())        goto ABBRUCH ;
   if (test_insertremove())   goto ABBRUCH ;
   if (test_generic())        goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
