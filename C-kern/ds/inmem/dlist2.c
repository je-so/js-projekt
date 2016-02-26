/* title: DoubleLinkedList2 impl

   Implements <DoubleLinkedList2>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/ds/inmem/dlist2.h
    Header file <DoubleLinkedList2>.

   file: C-kern/ds/inmem/dlist2.c
    Implementation file <DoubleLinkedList2 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/dlist2.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapt.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/errortimer.h"
#endif


// section: dlist2_t

// group: lifetime

int free_dlist2(dlist2_t* list, uint16_t nodeoffset, struct typeadapt_t* typeadp/*0=>no free called*/)
{
   int err;

   if (list->last) {

      dlist_node_t * next = list->last;

      const bool isDelete = (typeadp && iscalldelete_typeadapt(typeadp));

      err = 0;

      do {
         dlist_node_t * node = next;
         next = next->next;
         *node = (dlist_node_t) dlist_node_INIT;
         if (isDelete) {
            typeadapt_object_t * delobj = cast2object_typeadaptnodeoffset(nodeoffset, node);
            int err2 = calldelete_typeadapt(typeadp, &delobj);
            if (err2) err = err2;
         }
      } while (next != list->last);

      *list = (dlist2_t) dlist2_FREE;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: update

void insertfirst_dlist2(dlist2_t* list, dlist_node_t * new_node)
{
   if (list->first) {
      new_node->prev = list->last;
      new_node->next = list->first;
      list->last->next = new_node;
      list->first->prev = new_node;
   } else {
      new_node->prev = new_node;
      new_node->next = new_node;
      list->last     = new_node;
   }

   list->first = new_node;
}

void insertlast_dlist2(dlist2_t * list, dlist_node_t * new_node)
{
   if (list->last) {
      new_node->prev = list->last;
      new_node->next = list->first;
      list->last->next  = new_node;
      list->first->prev = new_node;
   } else {
      new_node->prev = new_node;
      new_node->next = new_node;
      list->first    = new_node;
   }

   list->last = new_node;
}

void insertafter_dlist2(dlist2_t * list, dlist_node_t * prev_node, dlist_node_t * new_node)
{
   new_node->prev  = prev_node;
   new_node->next  = prev_node->next;
   prev_node->next = new_node;
   new_node->next->prev = new_node;

   if (list->last == prev_node) {
      list->last = new_node;
   }
}

void insertbefore_dlist2(dlist2_t * list, dlist_node_t * next_node, dlist_node_t * new_node)
{
   new_node->prev  = next_node->prev;
   new_node->prev->next = new_node;
   new_node->next  = next_node;
   next_node->prev = new_node;

   if (list->first == next_node) {
      list->first = new_node;
   }
}

static inline void remove_helper(dlist_node_t * node)
{
   node->prev->next = node->next;
   node->next->prev = node->prev;
}

void removefirst_dlist2(dlist2_t * list, struct dlist_node_t ** removed_node)
{
   dlist_node_t * const first = list->first;

   if (list->last == first) {
      list->last = 0;
      list->first = 0;
   } else {
      list->first = first->next;
      remove_helper(first);
   }

   first->next = 0;
   first->prev = 0;

   *removed_node = first;
}

void removelast_dlist2(dlist2_t * list, struct dlist_node_t ** removed_node)
{
   dlist_node_t * const last = list->last;

   if (last == list->first) {
      list->last = 0;
      list->first = 0;
   } else {
      list->last = last->prev;
      remove_helper(last);
   }

   last->next = 0;
   last->prev = 0;

   *removed_node = last;
}

void remove_dlist2(dlist2_t * list, struct dlist_node_t * node)
{
   if (node == list->last) {
      if (node == list->first) {
         list->last = 0;
         list->first = 0;
      } else {
         list->last = node->prev;
      }
   } else if (node == list->first) {
      list->first = node->next;
   }

   remove_helper(node);

   node->next = 0;
   node->prev = 0;
}

void replacenode_dlist2(dlist2_t * list, dlist_node_t * oldnode, dlist_node_t * newnode)
{
   if (list->last == oldnode) {
      list->last = newnode;
      if (list->first == oldnode) {
         list->first = newnode;
         newnode->next = newnode;
         newnode->prev = newnode;
         oldnode->next = 0;
         oldnode->prev = 0;
         return;
      }
   } else if (list->first == oldnode) {
      list->first = newnode;
   }

   newnode->next = oldnode->next;
   oldnode->next->prev = newnode;
   newnode->prev = oldnode->prev;
   oldnode->prev->next = newnode;

   oldnode->next = 0;
   oldnode->prev = 0;
}

// group: set-update

void insertlastPlist_dlist2(dlist2_t * list, dlist2_t * nodes)
{

   if (isempty_dlist2(list)) {
      *list = *nodes;

   } else if (! isempty_dlist2(nodes)) {
      list->last->next = nodes->first;
      nodes->first->prev = list->last;

      list->first->prev = nodes->last;
      nodes->last->next = list->first;

      list->last = nodes->last;
   }

   *nodes = (dlist2_t) dlist2_INIT;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_dlistnode(void)
{
   dlist_node_t   node = dlist_node_INIT;
   struct {
      dlist_node_t * next;
      dlist_node_t * prev;
   }              node1;

   struct {
      size_t dummy;
      dlist_node_t * next;
      dlist_node_t * prev;
   }              node2;

   // TEST dlist_node_INIT
   TEST(0 == node.next);
   TEST(0 == node.prev);

   // TEST cast_dlistnode
   TEST(cast_dlistnode(&node)  == &node);
   TEST(cast_dlistnode(&node1) == (dlist_node_t*)&node1.next);
   TEST(cast_dlistnode(&node2) == (dlist_node_t*)&node2.next);

   // TEST next_dlistnode
   node.next = (void*)4;
   TEST(next_dlistnode(&node) == (void*)4);
   node.next = 0;
   TEST(next_dlistnode(&node) == 0);

   // TEST prev_dlistnode
   node.prev = (void*)4;
   TEST(prev_dlistnode(&node) == (void*)4);
   node.prev = 0;
   TEST(prev_dlistnode(&node) == 0);

   // TEST isinlist_dlistnode
   node.next = (void*)1;
   TEST(isinlist_dlistnode(&node));
   node.next = 0;
   TEST(! isinlist_dlistnode(&node));

   return 0;
ONERR:
   return EINVAL;
}

typedef struct testnode_t {
   dlist_node_t   node;
   int is_freed;
   dlist_node_t * next;
   dlist_node_t * prev;
   dlist_node_t   node2;
} testnode_t;

typedef struct testadapt_t {
   struct {
      typeadapt_EMBED(struct testadapt_t, testnode_t, void*);
   };
   test_errortimer_t errcounter;
   unsigned          freenode_count;
} testadapt_t;

static int freenode_testdapt(testadapt_t * typeadp, testnode_t ** node)
{
   int err;

   if (! process_testerrortimer(&typeadp->errcounter, &err)) {
      ++ typeadp->freenode_count;
      ++ (*node)->is_freed;
   }

   *node = 0;

   return err;
}

static int test_initfree(void)
{
   dlist2_t list = dlist2_FREE;
   testadapt_t   typeadapt  = { typeadapt_INIT_LIFETIME(0, &freenode_testdapt), test_errortimer_FREE, 0 };
   typeadapt_t * typeadp    = cast_typeadapt(&typeadapt, testadapt_t, testnode_t, void*);
   testadapt_t   typeadapt2 = { typeadapt_INIT_LIFETIME(0, 0/*==NULL*/), test_errortimer_FREE, 0 };
   typeadapt_t * typeadp2   = cast_typeadapt(&typeadapt2, testadapt_t, testnode_t, void*);
   testnode_t    nodes[1000];

   // prepare
   typeadapt2.lifetime.delete_object = 0;
   memset(nodes, 0, sizeof(nodes));

   // TEST dlist2_FREE
   TEST(0 == list.last);
   TEST(0 == list.first);

   // TEST dlist2_INIT_POINTER
   list = (dlist2_t) dlist2_INIT_POINTER((dlist_node_t*)3, (dlist_node_t*)4);
   TEST(list.last  == (void*)3);
   TEST(list.first == (void*)4);
   list = (dlist2_t) dlist2_INIT_POINTER(0, 0);
   TEST(list.last  == 0);
   TEST(list.first == 0);

   // TEST init_dlist2
   list.last = (void*)1;
   list.first = (void*)1;
   init_dlist2(&list);
   TEST(0 == list.last);
   TEST(0 == list.first);

   // TEST free_dlist2
   TEST(0 == free_dlist2(&list, 0, typeadp));
   TEST(0 == list.last);
   TEST(0 == list.first);
   TEST(0 == free_dlist2(&list, 0, typeadp));
   TEST(0 == list.last);
   TEST(0 == list.first);

   for (unsigned ta = 0; ta < 4; ++ta) {
      bool          isDelete   = ta < 2;
      uint16_t      nodeoffset = ta % 2 ? offsetof(testnode_t, node) : offsetof(testnode_t, node2);
      typeadapt_t * typeadp3   = typeadp;

      if (ta == 2) {
         // no delete called if (typeadp3 == 0)
         typeadp3 = 0;
      } else if (ta == 3) {
         // no delete called if (typeadapt_t.lifetime.delete_object == 0)
         typeadp3 = typeadp2;
      }

      // TEST free_dlist2: free inserted nodes
      // prepare
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         if (nodeoffset == offsetof(testnode_t, node))
            insertfirst_dlist2(&list, &nodes[i].node);
         else
            insertfirst_dlist2(&list, &nodes[i].node2);
      }
      // test
      typeadapt.freenode_count = 0;
      TEST(0 == free_dlist2(&list, nodeoffset, typeadp3));
      // check list
      TEST(0 == list.last);
      TEST(0 == list.first);
      // check nodes
      TEST(typeadapt.freenode_count == (isDelete ? lengthof(nodes) : 0));
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         TEST(0 == nodes[i].node.next);
         TEST(0 == nodes[i].node.prev);
         TEST(0 == nodes[i].node2.next);
         TEST(0 == nodes[i].node2.prev);
         TEST(isDelete == nodes[i].is_freed);
         nodes[i].is_freed = 0;
      }

      // TEST free_dlist2: simulated ERROR
      // prepare
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         if (nodeoffset == offsetof(testnode_t, node))
            insertlast_dlist2(&list, &nodes[i].node);
         else
            insertlast_dlist2(&list, &nodes[i].node2);
      }
      // test
      typeadapt.freenode_count = 0;
      init_testerrortimer(&typeadapt.errcounter, ta+1u, EFAULT);
      TEST(EFAULT == free_dlist2(&list, nodeoffset, typeadp/*always call free*/));
      // check list
      TEST(0 == list.last);
      TEST(0 == list.first);
      // check nodes
      TEST(lengthof(nodes)-1 == typeadapt.freenode_count);
      unsigned I = (lengthof(nodes)-1+ta) % lengthof(nodes);
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         TEST(0 == nodes[i].node.next);
         TEST(0 == nodes[i].node.prev);
         TEST(0 == nodes[i].node2.next);
         TEST(0 == nodes[i].node2.prev);
         TEST((i!=I) == nodes[i].is_freed);
         nodes[i].is_freed = 0;
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   dlist2_t list = dlist2_INIT;
   dlist_node_t node;

   // TEST isempty_dlist2
   list.last = (void*) 1;
   TEST(! isempty_dlist2(&list));
   list.last = 0;
   TEST(isempty_dlist2(&list));

   // TEST first_dlist2
   for (intptr_t i = 1; i; i <<= 1) {
      list.first = (void*)i;
      TEST(first_dlist2(&list) == (void*)i);
   }
   list.first = 0;
   TEST(0 == first_dlist2(&list));

   // TEST last_dlist2
   for (intptr_t i = 1; i; i <<= 1) {
      list.last = (void*)i;
      TEST(last_dlist2(&list) == (void*)i);
   }
   list.last = 0;
   TEST(0 == last_dlist2(&list));

   // TEST next_dlist2
   node.next = (void*)4;
   TEST(next_dlist2(&node) == (void*)4);
   node.next = 0;
   TEST(next_dlist2(&node) == 0);

   // TEST prev_dlist2
   node.prev = (void*)4;
   TEST(prev_dlist2(&node) == (void*)4);
   node.prev = 0;
   TEST(prev_dlist2(&node) == 0);

   // TEST isinlist_dlist2
   node.next = (void*)1;
   TEST(isinlist_dlist2(&node));
   node.next = 0;
   TEST(! isinlist_dlist2(&node));

   return 0;
ONERR:
   return EINVAL;
}

static int test_iterator(void)
{
   dlist2_t          list = dlist2_INIT;
   dlist2_iterator_t iter = dlist2_iterator_FREE;
   testnode_t        nodes[999];
   dlist_node_t *    next;
   dlist_node_t *    prev;

   // prepare
   memset(nodes, 0, sizeof(nodes));

   // TEST dlist2_iterator_FREE
   TEST(0 == iter.next);
   TEST(0 == iter.list);

   // TEST initfirst_dlist2iterator: empty list
   TEST(ENODATA == initfirst_dlist2iterator(&iter, &list));

   // TEST initlast_dlist2iterator: empty list
   TEST(ENODATA == initlast_dlist2iterator(&iter, &list));

   // TEST foreach, foreachReverse: empty list
   for (unsigned i = 0; 0 == i; i=1) {
      foreach (_dlist2, node, &list) {
         ++ i;
      }
      TEST(0 == i);

      foreachReverse (_dlist2, node, &list) {
         ++ i;
      }
      TEST(0 == i);
   }

   // TEST free_dlist2iterator
   iter.next = &nodes[0].node;
   iter.list = &list;
   TEST(0 == free_dlist2iterator(&iter));
   TEST(iter.next == 0);
   TEST(iter.list == &list);

   for (unsigned s = 1; s <= lengthof(nodes); ++s) {
      if (s == 3) s = lengthof(nodes);

      // prepare: fill list
      for (unsigned i = 0; i < s; ++i) {
         insertlast_dlist2(&list, &nodes[i].node);
      }

      // TEST initfirst_dlist2iterator
      iter = (dlist2_iterator_t) dlist2_iterator_FREE;
      TEST(0 == initfirst_dlist2iterator(&iter, &list));
      TEST(iter.next == first_dlist2(&list));
      TEST(iter.list == &list);

      // TEST initlast_dlist2iterator
      iter = (dlist2_iterator_t) dlist2_iterator_FREE;
      TEST(0 == initlast_dlist2iterator(&iter, &list));
      TEST(iter.next == last_dlist2(&list));
      TEST(iter.list == &list);

      // TEST next_dlist2iterator
      TEST(0 == initfirst_dlist2iterator(&iter, &list));
      for (unsigned i = 0; i < s; ++i) {
         TEST(next_dlist2iterator(&iter, &next));
         TEST(next == &nodes[i].node);
         TEST(iter.next == (i == s-1 ? 0 : &nodes[i+1].node));
         TEST(iter.list == &list);
      }
      TEST(0 == next_dlist2iterator(&iter, &next));
      TEST(iter.next == 0);
      TEST(iter.list == &list);

      // TEST prev_dlist2iterator
      TEST(0 == initlast_dlist2iterator(&iter, &list));
      for (unsigned i = 1; i <= s; ++i) {
         TEST(prev_dlist2iterator(&iter, &prev));
         TEST(prev == &nodes[s-i].node);
         TEST(iter.next == (i==s ? 0 : &nodes[s-i-1].node));
         TEST(iter.list == &list);
      }
      TEST(0 == prev_dlist2iterator(&iter, &prev))
      TEST(iter.next == 0);
      TEST(iter.list == &list);

      // TEST foreach
      for (unsigned i = 0; i == 0; ) {
         foreach (_dlist2, node, &list) {
            TEST(node == &nodes[i++].node);
         }
         TEST(i == s);
      }

      // TEST foreachReverse
      for (unsigned i = 0; i == 0; ) {
         foreachReverse (_dlist2, node, &list) {
            TEST(node == &nodes[s-1-i++].node);
         }
         TEST(i == s);
      }

      // reset
      TEST(0 == removeall_dlist2(&list, 0, 0));
   }

   for (unsigned s = 1; s <= lengthof(nodes); ++s) {
      if (s == 3) s = lengthof(nodes);

      // prepare: fill list
      for (unsigned i = 0; i < s; ++i) {
         insertlast_dlist2(&list, &nodes[i].node);
      }

      // TEST next_dlist2iterator: remove current node
      // prepare: fill list
      for (unsigned i = 0; i < s; ++i) {
         insertlast_dlist2(&list, &nodes[i].node);
      }
      TEST(0 == initfirst_dlist2iterator(&iter, &list));
      for (unsigned i = 0; i < s; ++i) {
         // test
         TEST(next_dlist2iterator(&iter, &next));
         TEST(next == &nodes[i].node);
         remove_dlist2(&list, next);
      }
      // check
      TEST(! next_dlist2iterator(&iter, &next));
      TEST(isempty_dlist2(&list));

      // TEST next_dlist2iterator: remove first node
      // prepare: fill list
      for (unsigned i = 0; i < s; ++i) {
         insertlast_dlist2(&list, &nodes[i].node);
      }
      TEST(0 == initfirst_dlist2iterator(&iter, &list));
      for (unsigned i = 0; i < s; ++i) {
         // test
         TEST(next_dlist2iterator(&iter, &next));
         TEST(next == &nodes[i].node);
         if (i) {
            removefirst_dlist2(&list, &next);
            TEST(next == &nodes[i-1].node);
         }
      }
      // check
      TEST(! next_dlist2iterator(&iter, &next));
      TEST(first_dlist2(&list) == &nodes[s-1].node);
      TEST(last_dlist2(&list) == &nodes[s-1].node);
      // reset
      TEST(0 == removeall_dlist2(&list, 0, 0));

      // TEST prev_dlist2iterator: remove current node
      // prepare: fill list
      for (unsigned i = 0; i < s; ++i) {
         insertlast_dlist2(&list, &nodes[i].node);
      }
      TEST(0 == initlast_dlist2iterator(&iter, &list));
      for (unsigned i = 0; i < s; ++i) {
         // test
         TESTP(prev_dlist2iterator(&iter, &prev), "s:%d i:%d", s, i);
         TEST(prev == &nodes[s-1-i].node);
         remove_dlist2(&list, prev);
      }
      // check
      TEST(! prev_dlist2iterator(&iter, &prev));
      TEST(isempty_dlist2(&list));

      // TEST prev_dlist2iterator: remove last node
      // prepare: fill list
      for (unsigned i = 0; i < s; ++i) {
         insertlast_dlist2(&list, &nodes[i].node);
      }
      TEST(0 == initlast_dlist2iterator(&iter, &list));
      for (unsigned i = 0; i < s; ++i) {
         // test
         TEST(prev_dlist2iterator(&iter, &prev));
         TEST(prev == &nodes[s-1-i].node);
         if (i) {
            removelast_dlist2(&list, &prev);
            TEST(prev == &nodes[s-i].node);
         }
      }
      // check
      TEST(! prev_dlist2iterator(&iter, &prev));
      TEST(first_dlist2(&list) == &nodes[0].node);
      TEST(last_dlist2(&list) == &nodes[0].node);
      // reset
      TEST(0 == removeall_dlist2(&list, 0, 0));

      // ! removing other nodes than the current or previously iterated is not supported !

   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_insert(void)
{
   dlist2_t   list = dlist2_INIT;
   testnode_t nodes[1000];

   // prepare
   memset(nodes, 0, sizeof(nodes));

   for (int tc = 0; tc <= 1; ++tc) {

      // TEST insertfirst_dlist2: single element (no error check)
      insertfirst_dlist2(&list, &nodes[0].node);
      TEST(nodes[0].node.next == &nodes[0].node);
      TEST(nodes[0].node.prev == &nodes[0].node);
      TEST(list.last  == &nodes[0].node);
      TEST(list.first == &nodes[0].node);
      // keep node to make sure no error check
      list = (dlist2_t) dlist2_INIT;

      // TEST insertlast_dlist2: single element (no error check)
      insertlast_dlist2(&list, &nodes[1].node);
      TEST(nodes[1].node.next == &nodes[1].node);
      TEST(nodes[1].node.prev == &nodes[1].node);
      TEST(list.last  == &nodes[1].node);
      TEST(list.first == &nodes[1].node);
      // reset list (keep node to make sure no error check)
      list = (dlist2_t) dlist2_INIT;

      // TEST insertafter_dlist2: single element (no error check)
      insertfirst_dlist2(&list, &nodes[0].node);
      insertafter_dlist2(&list, &nodes[0].node, &nodes[1].node);
      // check
      TEST(list.last  == &nodes[1].node);
      TEST(list.first == &nodes[0].node);
      TEST(nodes[0].node.next == &nodes[1].node);
      TEST(nodes[0].node.prev == &nodes[1].node);
      TEST(nodes[1].node.next == &nodes[0].node);
      TEST(nodes[1].node.prev == &nodes[0].node);
      // reset
      list = (dlist2_t) dlist2_INIT;

      // TEST insertbefore_dlist2: single element (no error check)
      insertfirst_dlist2(&list, &nodes[0].node);
      insertbefore_dlist2(&list, &nodes[0].node, &nodes[1].node);
      // check
      TEST(list.last  == &nodes[0].node);
      TEST(list.first == &nodes[1].node);
      TEST(nodes[0].node.next == &nodes[1].node);
      TEST(nodes[0].node.prev == &nodes[1].node);
      TEST(nodes[1].node.next == &nodes[0].node);
      TEST(nodes[1].node.prev == &nodes[0].node);
      // reset
      list = (dlist2_t) dlist2_INIT;
   }
   // reset
   memset(nodes, 0, 2*sizeof(nodes[0]));

   // TEST insertfirst_dlist2: many
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertfirst_dlist2(&list, &nodes[i].node);
      // check list
      TEST(list.last  == &nodes[0].node);
      TEST(list.first == &nodes[i].node);
      if (i < 4) {
         // check nodes
         for (unsigned i2 = 0; i2 <= i; ++i2) {
            TEST(nodes[i2].node.next == &nodes[(i2+i)%(i+1)].node);
            TEST(nodes[i2].node.prev == &nodes[(i2+1)%(i+1)].node);
         }
      }
   }
   // check nodes
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[i].node.next == &nodes[(i+lengthof(nodes)-1)%lengthof(nodes)].node);
      TEST(nodes[i].node.prev == &nodes[(i+1)%lengthof(nodes)].node);
   }
   // reset
   list = (dlist2_t) dlist2_INIT;
   memset(nodes, 0, sizeof(nodes));

   // TEST insertlast_dlist2: many
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_dlist2(&list, &nodes[i].node);
      // check list
      TEST(list.last  == &nodes[i].node);
      TEST(list.first == &nodes[0].node);
      if (i < 4) {
         // check nodes
         for (unsigned i2 = 0; i2 <= i; ++i2) {
            TEST(nodes[i2].node.next == &nodes[(i2+1)%(i+1)].node);
            TEST(nodes[i2].node.prev == &nodes[(i2+i)%(i+1)].node);
         }
      }
   }
   // check nodes
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[i].node.next == &nodes[(i+1)%lengthof(nodes)].node);
      TEST(nodes[i].node.prev == &nodes[(i+lengthof(nodes)-1)%lengthof(nodes)].node);
   }
   // reset
   list = (dlist2_t) dlist2_INIT;
   memset(nodes, 0, sizeof(nodes));

   // TEST insertafter_dlist2: many (after first)
   insertfirst_dlist2(&list, &nodes[0].node);
   for (unsigned i = 1; i < lengthof(nodes); ++i) {
      insertafter_dlist2(&list, &nodes[0].node, &nodes[i].node);
      TEST(list.last == &nodes[1].node);
      TEST(list.first == &nodes[0].node);
      if (i < 4) {
         // check nodes
         for (unsigned i2 = 0; i2 <= i; ++i2) {
            TEST(nodes[i2].node.next == &nodes[(i2+i)%(i+1)].node);
            TEST(nodes[i2].node.prev == &nodes[(i2+1)%(i+1)].node);
         }
      }
   }
   // check nodes
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[i].node.next == &nodes[(i+lengthof(nodes)-1)%lengthof(nodes)].node);
      TEST(nodes[i].node.prev == &nodes[(i+1)%lengthof(nodes)].node);
   }
   // reset
   list = (dlist2_t) dlist2_INIT;
   memset(nodes, 0, sizeof(nodes));

   // TEST insertafter_dlist2: many (after last)
   insertfirst_dlist2(&list, &nodes[0].node);
   for (unsigned i = 1; i < lengthof(nodes); ++i) {
      insertafter_dlist2(&list, &nodes[i-1].node, &nodes[i].node);
      TEST(list.last == &nodes[i].node);
      TEST(list.first == &nodes[0].node);
      if (i < 4) {
         // check nodes
         for (unsigned i2 = 0; i2 <= i; ++i2) {
            TEST(nodes[i2].node.next == &nodes[(i2+1)%(i+1)].node);
            TEST(nodes[i2].node.prev == &nodes[(i2+i)%(i+1)].node);
         }
      }
   }
   // check nodes
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[i].node.next == &nodes[(i+1)%lengthof(nodes)].node);
      TEST(nodes[i].node.prev == &nodes[(i+lengthof(nodes)-1)%lengthof(nodes)].node);
   }
   // reset
   list = (dlist2_t) dlist2_INIT;
   memset(nodes, 0, sizeof(nodes));

   // TEST insertbefore_dlist2: many (before first)
   insertfirst_dlist2(&list, &nodes[0].node);
   for (unsigned i = 1; i < lengthof(nodes); ++i) {
      insertbefore_dlist2(&list, &nodes[i-1].node, &nodes[i].node);
      TEST(list.last == &nodes[0].node);
      TEST(list.first == &nodes[i].node);
      if (i < 4) {
         // check nodes
         for (unsigned i2 = 0; i2 <= i; ++i2) {
            TEST(nodes[i2].node.next == &nodes[(i2+i)%(i+1)].node);
            TEST(nodes[i2].node.prev == &nodes[(i2+1)%(i+1)].node);
         }
      }
   }
   // check nodes
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[i].node.next == &nodes[(i+lengthof(nodes)-1)%lengthof(nodes)].node);
      TEST(nodes[i].node.prev == &nodes[(i+1)%lengthof(nodes)].node);
   }
   // reset
   list = (dlist2_t) dlist2_INIT;
   memset(nodes, 0, sizeof(nodes));

   // TEST insertbefore_dlist2: many (before last)
   insertfirst_dlist2(&list, &nodes[0].node);
   for (unsigned i = 1; i < lengthof(nodes); ++i) {
      insertbefore_dlist2(&list, &nodes[0].node, &nodes[i].node);
      TEST(list.last == &nodes[0].node);
      TEST(list.first == &nodes[1].node);
      if (i < 4) {
         // check nodes
         for (unsigned i2 = 0; i2 <= i; ++i2) {
            TEST(nodes[i2].node.next == &nodes[(i2+1)%(i+1)].node);
            TEST(nodes[i2].node.prev == &nodes[(i2+i)%(i+1)].node);
         }
      }
   }
   // check nodes
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(nodes[i].node.next == &nodes[(i+1)%lengthof(nodes)].node);
      TEST(nodes[i].node.prev == &nodes[(i+lengthof(nodes)-1)%lengthof(nodes)].node);
   }
   // reset
   list = (dlist2_t) dlist2_INIT;
   memset(nodes, 0, sizeof(nodes));

   return 0;
ONERR:
   return EINVAL;
}

static int test_remove(void)
{
   dlist2_t list = dlist2_INIT;
   dlist_node_t* node;
   testnode_t    nodes[1000];

   // prepare
   memset(nodes, 0, sizeof(nodes));

   // TEST removefirst_dlist2
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_dlist2(&list, &nodes[i].node);
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      // test
      removefirst_dlist2(&list, &node);
      // check
      TEST(node == &nodes[i].node)
      TEST(0 == node->next);
      TEST(0 == node->prev);
      if (i < lengthof(nodes)-1) {
         TEST(list.last == &nodes[lengthof(nodes)-1].node);
         TEST(list.first == &nodes[i+1].node);
         // check nodes
         TEST(nodes[i+1].node.next == &nodes[i+2<lengthof(nodes) ? i+2 : i+1].node);
         TEST(nodes[i+1].node.prev == &nodes[lengthof(nodes)-1].node);
         TEST(&nodes[i+1].node == nodes[i+2<lengthof(nodes) ? i+2 : i+1].node.prev);
         TEST(&nodes[i+1].node == nodes[lengthof(nodes)-1].node.next);
      }
   }
   // check all
   TEST(list.last  == 0);
   TEST(list.first == 0);
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].node.next);
      TEST(0 == nodes[i].node.prev);
   }

   // TEST removefirst_dlist2: no error check
   // removefirst_dlist2(&list, &node); // dereference 0-pointer ==> segmentation fault

   // TEST removelast_dlist2
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_dlist2(&list, &nodes[i].node);
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      // test
      removelast_dlist2(&list, &node);
      // check
      TEST(node == &nodes[lengthof(nodes)-1-i].node)
      TEST(0 == node->next);
      TEST(0 == node->prev);
      if (i < lengthof(nodes)-1) {
         TEST(list.last == &nodes[lengthof(nodes)-2-i].node);
         TEST(list.first == &nodes[0].node);
         // check nodes
         TEST(nodes[lengthof(nodes)-2-i].node.next == &nodes[0].node);
         TEST(nodes[lengthof(nodes)-2-i].node.prev == &nodes[i+2==lengthof(nodes) ? 0 : lengthof(nodes)-3-i].node);
         TEST(&nodes[lengthof(nodes)-2-i].node == nodes[0].node.prev);
         TEST(&nodes[lengthof(nodes)-2-i].node == nodes[i+2==lengthof(nodes) ? 0 : lengthof(nodes)-3-i].node.next);
      }
   }
   // check all
   TEST(list.last  == 0);
   TEST(list.first == 0);
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].node.next);
      TEST(0 == nodes[i].node.prev);
   }

   // TEST removelast_dlist2: no error check
   // removelast_dlist2(&list, &node); // dereference 0-pointer ==> segmentation fault

   // TEST remove_dlist2: not first && not last
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_dlist2(&list, &nodes[i].node);
   }
   for (unsigned i = 1; i < lengthof(nodes)-1; ++i) {
      // test
      remove_dlist2(&list, &nodes[i].node);
      // check param
      TEST(nodes[i].node.next == 0);
      TEST(nodes[i].node.prev == 0);
      // check list
      TEST(list.last == &nodes[lengthof(nodes)-1].node);
      TEST(list.first == &nodes[0].node);
      // check surrounding nodes
      TEST(nodes[0].node.next == &nodes[i+1].node);
      TEST(nodes[0].node.prev == &nodes[lengthof(nodes)-1].node);
      TEST(nodes[i+1].node.next == &nodes[(i+2)%lengthof(nodes)].node);
      TEST(nodes[i+1].node.prev == &nodes[0].node);
   }
   // reset
   list = (dlist2_t) dlist2_INIT;
   memset(nodes, 0, sizeof(nodes));

   // TEST remove_dlist2: first node
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_dlist2(&list, &nodes[i].node);
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      // test
      remove_dlist2(&list, &nodes[i].node);
      // check param
      TEST(nodes[i].node.next == 0);
      TEST(nodes[i].node.prev == 0);
      if (i < lengthof(nodes)-1) {
         // check list
         TEST(list.last == &nodes[lengthof(nodes)-1].node);
         TEST(list.first == &nodes[i+1].node);
         // check surrounding nodes
         TEST(nodes[i+1].node.next == &nodes[i+2==lengthof(nodes) ? lengthof(nodes)-1 : i+2].node);
         TEST(nodes[i+1].node.prev == &nodes[lengthof(nodes)-1].node);
         TEST(nodes[lengthof(nodes)-1].node.next == &nodes[i+1].node);
         TEST(nodes[lengthof(nodes)-1].node.prev == &nodes[i+2==lengthof(nodes) ? lengthof(nodes)-1 : lengthof(nodes)-2].node);
      } else {
         // check list
         TEST(list.last  == 0);
         TEST(list.first == 0);
      }
   }

   // TEST remove_dlist2: last node
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      insertlast_dlist2(&list, &nodes[i].node);
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      // test
      remove_dlist2(&list, &nodes[lengthof(nodes)-1-i].node);
      // check param
      TEST(nodes[lengthof(nodes)-1-i].node.next == 0);
      TEST(nodes[lengthof(nodes)-1-i].node.prev == 0);
      if (i < lengthof(nodes)-1) {
         // check list
         TEST(list.last  == &nodes[lengthof(nodes)-2-i].node);
         TEST(list.first == &nodes[0].node);
         // check surrounding nodes
         TEST(nodes[lengthof(nodes)-2-i].node.next == &nodes[0].node);
         TEST(nodes[lengthof(nodes)-2-i].node.prev == &nodes[i+2==lengthof(nodes)?0:lengthof(nodes)-3-i].node);
         TEST(nodes[0].node.next == &nodes[(i+2!=lengthof(nodes))].node);
         TEST(nodes[0].node.prev == &nodes[lengthof(nodes)-2-i].node);
      } else {
         // check list
         TEST(list.last  == 0);
         TEST(list.first == 0);
      }
   }

   // TEST replacenode_dlist2: list length: 1..5 nodes
   for (int l = 1; l <= 5; ++l) {
      static_assert(lengthof(nodes) >= 10, "Verhindere Overflow bei Zugriff auf nodes");

      for (int i = 0; i < l; ++i) {
         insertlast_dlist2(&list, &nodes[i].node);
      }

      for (int i = 0; i < l; ++i) {
         replacenode_dlist2(&list, &nodes[i].node, &nodes[5+i].node);
         // check parameter
         TEST(0 == nodes[i].node.next);
         TEST(0 == nodes[i].node.prev);
         // check list
         TEST(list.first == &nodes[5].node);
         TEST(list.last  == &nodes[l-1+(i+1==l?5:0)].node);
         // check nodes
         for (int i2 = 0, p = l-1, n = l>1?1:0; i2 < l; ++i2, p = (p+1)%l, n = (n+1)%l) {
            TEST(nodes[i2+(i2<=i?5:0)].node.next = &nodes[n+(n<=i?5:0)].node);
            TEST(nodes[i2+(i2<=i?5:0)].node.prev = &nodes[p+(p<=i?5:0)].node);
         }
      }

      // reset
      list = (dlist2_t) dlist2_INIT;
      memset(nodes, 0, sizeof(nodes));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_setops(void)
{
   dlist2_t list = dlist2_INIT;
   dlist2_t list2 = dlist2_INIT;
   testadapt_t   typeadapt  = { typeadapt_INIT_LIFETIME(0, &freenode_testdapt), test_errortimer_FREE, 0 };
   typeadapt_t * typeadp    = cast_typeadapt(&typeadapt, testadapt_t, testnode_t, void*);
   testadapt_t   typeadapt2 = { typeadapt_INIT_LIFETIME(0, 0/*==NULL*/), test_errortimer_FREE, 0 };
   typeadapt_t * typeadp2   = cast_typeadapt(&typeadapt2, testadapt_t, testnode_t, void*);
   testnode_t    nodes[1000];

   // prepare
   memset(nodes, 0, sizeof(nodes));

   for (int no = 0; no <= 2; ++no) {
      uint16_t nodeoffset = no == 0
                            ? offsetof(testnode_t, node)
                            : no == 1
                            ? offsetof(testnode_t, next)
                            : offsetof(testnode_t, node2);
      for (int isNoDelete = 0; isNoDelete <= 2; ++isNoDelete) {
         typeadapt_t * TA = isNoDelete == 0
                            ? typeadp
                            : isNoDelete == 1
                            ? typeadp2 /*delete_object == 0*/
                            : 0; /*pointer to typeadapt_t == 0*/

         // TEST removeall_dlist2: remove all nodes (free called / or no free called)
         for (unsigned i = 0; i < lengthof(nodes); ++i) {
            insertlast_dlist2(&list, no == 0 ? &nodes[i].node : no == 1 ? cast_dlistnode(&nodes[i]) : &nodes[i].node2);
         }
         // test
         typeadapt.freenode_count = 0;
         TEST(0 == removeall_dlist2(&list, nodeoffset, TA));
         // check list
         TEST(0 == list.last);
         TEST(0 == list.first);
         // check nodes
         TEST(typeadapt.freenode_count == (isNoDelete ? 0 : lengthof(nodes)));
         for (unsigned i = 0; i < lengthof(nodes); ++i) {
            TEST(0 == nodes[i].node.next  && 0 == nodes[i].node.prev);
            TEST(0 == nodes[i].next       && 0 == nodes[i].prev);
            TEST(0 == nodes[i].node2.next && 0 == nodes[i].node2.prev);
            TEST((isNoDelete == 0) == nodes[i].is_freed);
            nodes[i].is_freed = 0;
         }

         // TEST removeall_dlist2: simulated ERROR
         // prepare
         for (unsigned i = 0; i < lengthof(nodes); ++i) {
            insertlast_dlist2(&list, no == 0 ? &nodes[i].node : no == 1 ? cast_dlistnode(&nodes[i]) : &nodes[i].node2);
         }
         // test
         typeadapt.freenode_count = 0;
         init_testerrortimer(&typeadapt.errcounter, (unsigned)(3*no+isNoDelete+1), EFAULT);
         TEST(EFAULT == free_dlist2(&list, nodeoffset, typeadp/*always call free*/));
         // check list
         TEST(0 == list.last);
         TEST(0 == list.first);
         // check nodes
         TEST(lengthof(nodes)-1 == typeadapt.freenode_count);
         unsigned I = (lengthof(nodes)-1+(unsigned)(3*no+isNoDelete)) % lengthof(nodes);
         for (unsigned i = 0; i < lengthof(nodes); ++i) {
            TEST(0 == nodes[i].node.next  && 0 == nodes[i].node.prev);
            TEST(0 == nodes[i].next       && 0 == nodes[i].prev);
            TEST(0 == nodes[i].node2.next && 0 == nodes[i].node2.prev);
            TEST((i!=I) == nodes[i].is_freed);
            nodes[i].is_freed = 0;
         }
      }
   }

   // TEST insertlastPlist_dlist2: list length 0..5 nodes
   for (unsigned l1 = 0; l1 <= 5; ++l1) {
      for (unsigned l2 = 0, L = l1; l2 <= 5; ++l2, L = l1+l2) {
         for (unsigned i = 0; i < l1; ++i) {
            insertlast_dlist2(&list, &nodes[i].node);
         }
         for (unsigned i = l1; i < l1+l2; ++i) {
            insertlast_dlist2(&list2, &nodes[i].node);
         }
         insertlastPlist_dlist2(&list, &list2);
         // check parameter
         TEST(0 == list2.last)
         TEST(0 == list2.first)
         if (!L) {
            TEST(list.last  == 0)
            TEST(list.first == 0)
         } else {
            TEST(list.last  == &nodes[L-1].node)
            TEST(list.first == &nodes[0].node)
            // check nodes
            for (unsigned i = 0, p=L-1, n=L>1?1:0; i < L; p=i, ++i, n = (n+1)%L) {
               TEST(nodes[i].node.next == &nodes[n].node);
               TEST(nodes[i].node.prev == &nodes[p].node);
            }
         }
         // reset
         init_dlist2(&list);
         memset(nodes, 0, (unsigned) L * sizeof(nodes[0]));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

dlist2_IMPLEMENT(_glist1, testnode_t, node.)
dlist2_IMPLEMENT(_glist2, testnode_t, )
dlist2_IMPLEMENT(_glist3, testnode_t, node2.)

static int test_generic(void)
{
   dlist2_t list = dlist2_INIT;
   dlist2_t list2 = dlist2_INIT;
   testadapt_t   typeadapt  = { typeadapt_INIT_LIFETIME(0, &freenode_testdapt), test_errortimer_FREE, 0 };
   typeadapt_t * typeadp    = cast_typeadapt(&typeadapt, testadapt_t, testnode_t, void*);
   testnode_t *  node;
   testnode_t    nodes[1000];
   dlist2_iterator_t iter;

   // prepare
   memset(&nodes, 0, sizeof(nodes));

   // TEST cast_dlist2
   struct {
      dlist_node_t * last;
      dlist_node_t * first;
   }  xlist;
   TEST((dlist2_t*)&xlist == cast_dlist2(&xlist));

   // TEST castconst_dlist2
   const struct {
      dlist_node_t * last;
      dlist_node_t * first;
   } xlist2;
   TEST((const dlist2_t*)&xlist2 == castconst_dlist2(&xlist2));

   // == initfree ==

   // TEST init_dlist2
   memset(&list, 255, sizeof(list));
   init_glist1(&list);
   TEST(0 == list.last);
   TEST(0 == list.first);

   // TEST free_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         switch (tc) {
         case 1: insertlast_glist1(&list, &nodes[i]); break;
         case 2: insertlast_glist2(&list, &nodes[i]); break;
         case 3: insertlast_glist3(&list, &nodes[i]); break;
         }
      }
      // test
      typeadapt.freenode_count = 0;
      switch (tc) {
      case 1: TEST(0 == free_glist1(&list, typeadp)); break;
      case 2: TEST(0 == free_glist2(&list, typeadp)); break;
      case 3: TEST(0 == free_glist3(&list, typeadp)); break;
      }
      // check list
      TEST(0 == list.last);
      TEST(0 == list.first);
      // check nodes
      TEST(lengthof(nodes) == typeadapt.freenode_count);
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         TEST(0 == nodes[i].node.next  && 0 == nodes[i].node.prev);
         TEST(0 == nodes[i].next       && 0 == nodes[i].prev);
         TEST(0 == nodes[i].node2.next && 0 == nodes[i].node2.prev);
         nodes[i].is_freed = 0;
      }
   }

   // == query ==

   // TEST isempty_dlist2
   list.last = (void*) 1;
   TEST(0 == isempty_glist1(&list));
   list.last = (void*) 0;
   TEST(1 == isempty_glist1(&list));

   // TEST first_dlist2
   list.first = (void*) &nodes[0].next;
   TEST(&nodes[0] == first_glist2(&list));
   list.first = (void*) 0;
   TEST(0 == first_glist2(&list));

   // TEST last_dlist2
   list.last = &nodes[4].node2;
   TEST(&nodes[4] == last_glist3(&list));
   list.last = (void*) 0;
   TEST(0 == last_glist3(&list));

   // TEST next_dlist2
   nodes[0].node.next = (void*) &nodes[4].node;
   TEST(&nodes[4] == next_glist1(&nodes[0]));
   nodes[0].node.next = 0;

   // TEST prev_dlist2
   nodes[0].prev = (void*) &nodes[3].next;
   TEST(&nodes[3] == prev_glist2(&nodes[0]));
   nodes[0].prev = 0;

   // TEST isinlist_dlist2
   nodes[0].node2.next = (void*) 1;
   TEST(1 == isinlist_glist3(&nodes[0]));
   nodes[0].node2.next = (void*) 0;
   TEST(0 == isinlist_glist3(&nodes[0]));

   // == insert ==

   // TEST insertfirst_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned i = 0; i < 3; ++i) {
         switch (tc) {
         case 1: insertfirst_glist1(&list, &nodes[i]); break;
         case 2: insertfirst_glist2(&list, &nodes[i]); break;
         case 3: insertfirst_glist3(&list, &nodes[i]); break;
         }
         switch (tc) {
         case 1: TEST(last_glist1(&list) == &nodes[0]); TEST(first_glist1(&list) == &nodes[i]); break;
         case 2: TEST(last_glist2(&list) == &nodes[0]); TEST(first_glist2(&list) == &nodes[i]); break;
         case 3: TEST(last_glist3(&list) == &nodes[0]); TEST(first_glist3(&list) == &nodes[i]); break;
         }
      }
      // reset
      init_glist1(&list);
      memset(&nodes, 0, 3*sizeof(nodes[0]));
   }

   // TEST insertlast_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned i = 0; i < 3; ++i) {
         switch (tc) {
         case 1: insertlast_glist1(&list, &nodes[i]); break;
         case 2: insertlast_glist2(&list, &nodes[i]); break;
         case 3: insertlast_glist3(&list, &nodes[i]); break;
         }
         switch (tc) {
         case 1: TEST(last_glist1(&list) == &nodes[i]); TEST(first_glist1(&list) == &nodes[0]); break;
         case 2: TEST(last_glist2(&list) == &nodes[i]); TEST(first_glist2(&list) == &nodes[0]); break;
         case 3: TEST(last_glist3(&list) == &nodes[i]); TEST(first_glist3(&list) == &nodes[0]); break;
         }
      }
      // reset
      init_glist1(&list);
      memset(&nodes, 0, 3*sizeof(nodes[0]));
   }

   // TEST insertafter_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      switch (tc) {
      case 1: insertlast_glist1(&list, &nodes[0]); break;
      case 2: insertlast_glist2(&list, &nodes[0]); break;
      case 3: insertlast_glist3(&list, &nodes[0]); break;
      }
      for (unsigned i = 1; i < 3; ++i) {
         switch (tc) {
         case 1: insertafter_glist1(&list, &nodes[0], &nodes[i]); break;
         case 2: insertafter_glist2(&list, &nodes[0], &nodes[i]); break;
         case 3: insertafter_glist3(&list, &nodes[0], &nodes[i]); break;
         }
         switch (tc) {
         case 1: TEST(last_glist1(&list) == &nodes[1]); TEST(first_glist1(&list) == &nodes[0]); break;
         case 2: TEST(last_glist2(&list) == &nodes[1]); TEST(first_glist2(&list) == &nodes[0]); break;
         case 3: TEST(last_glist3(&list) == &nodes[1]); TEST(first_glist3(&list) == &nodes[0]); break;
         }
         switch (tc) {
         case 1: TEST(next_glist1(&nodes[0]) == &nodes[i]); TEST(prev_glist1(&nodes[i]) == &nodes[0]); break;
         case 2: TEST(next_glist2(&nodes[0]) == &nodes[i]); TEST(prev_glist2(&nodes[i]) == &nodes[0]); break;
         case 3: TEST(next_glist3(&nodes[0]) == &nodes[i]); TEST(prev_glist3(&nodes[i]) == &nodes[0]); break;
         }
      }
      // reset
      init_glist1(&list);
      memset(&nodes, 0, 3*sizeof(nodes[0]));
   }

   // TEST insertbefore_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      switch (tc) {
      case 1: insertlast_glist1(&list, &nodes[0]); break;
      case 2: insertlast_glist2(&list, &nodes[0]); break;
      case 3: insertlast_glist3(&list, &nodes[0]); break;
      }
      for (unsigned i = 1; i < 3; ++i) {
         switch (tc) {
         case 1: insertbefore_glist1(&list, &nodes[0], &nodes[i]); break;
         case 2: insertbefore_glist2(&list, &nodes[0], &nodes[i]); break;
         case 3: insertbefore_glist3(&list, &nodes[0], &nodes[i]); break;
         }
         switch (tc) {
         case 1: TEST(last_glist1(&list) == &nodes[0]); TEST(first_glist1(&list) == &nodes[1]); break;
         case 2: TEST(last_glist2(&list) == &nodes[0]); TEST(first_glist2(&list) == &nodes[1]); break;
         case 3: TEST(last_glist3(&list) == &nodes[0]); TEST(first_glist3(&list) == &nodes[1]); break;
         }
         switch (tc) {
         case 1: TEST(next_glist1(&nodes[i]) == &nodes[0]); TEST(prev_glist1(&nodes[0]) == &nodes[i]); break;
         case 2: TEST(next_glist2(&nodes[i]) == &nodes[0]); TEST(prev_glist2(&nodes[0]) == &nodes[i]); break;
         case 3: TEST(next_glist3(&nodes[i]) == &nodes[0]); TEST(prev_glist3(&nodes[0]) == &nodes[i]); break;
         }
      }
      // reset
      init_glist1(&list);
      memset(&nodes, 0, 3*sizeof(nodes[0]));
   }

   // == remove ==

   // TEST removefirst_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned i = 0; i < 3; ++i) {
         switch (tc) {
         case 1: insertlast_glist1(&list, &nodes[i]); break;
         case 2: insertlast_glist2(&list, &nodes[i]); break;
         case 3: insertlast_glist3(&list, &nodes[i]); break;
         }
      }
      for (unsigned i = 0; i < 3; ++i) {
         // check list
         switch (tc) {
         case 1: TEST(last_glist1(&list) == &nodes[2]); TEST(first_glist1(&list) == &nodes[i]); break;
         case 2: TEST(last_glist2(&list) == &nodes[2]); TEST(first_glist2(&list) == &nodes[i]); break;
         case 3: TEST(last_glist3(&list) == &nodes[2]); TEST(first_glist3(&list) == &nodes[i]); break;
         }
         // check nodes
         for (unsigned i2 = i, p = 2, n = i==2?2:i+1; i2 < 3; p = i2, ++i2, n = n==2?i:n+1) {
            switch (tc) {
            case 1: TEST(next_glist1(&nodes[i2]) == &nodes[n]); TEST(prev_glist1(&nodes[i2]) == &nodes[p]); break;
            case 2: TEST(next_glist2(&nodes[i2]) == &nodes[n]); TEST(prev_glist2(&nodes[i2]) == &nodes[p]); break;
            case 3: TEST(next_glist3(&nodes[i2]) == &nodes[n]); TEST(prev_glist3(&nodes[i2]) == &nodes[p]); break;
            }
         }
         // test
         switch (tc) {
         case 1: removefirst_glist1(&list, &node); break;
         case 2: removefirst_glist2(&list, &node); break;
         case 3: removefirst_glist3(&list, &node); break;
         }
         // check node
         TEST(&nodes[i] == node);
         TEST(0 == node->node.next  && 0 == node->node.prev);
         TEST(0 == node->next       && 0 == node->prev);
         TEST(0 == node->node2.next && 0 == node->node2.prev);
      }
      // check list
      TEST(0 == list.last);
      TEST(0 == list.first);
   }

   // TEST removelast_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned i = 0; i < 3; ++i) {
         switch (tc) {
         case 1: insertfirst_glist1(&list, &nodes[i]); break;
         case 2: insertfirst_glist2(&list, &nodes[i]); break;
         case 3: insertfirst_glist3(&list, &nodes[i]); break;
         }
      }
      for (unsigned i = 0; i < 3; ++i) {
         // check list
         switch (tc) {
         case 1: TEST(last_glist1(&list) == &nodes[i]); TEST(first_glist1(&list) == &nodes[2]); break;
         case 2: TEST(last_glist2(&list) == &nodes[i]); TEST(first_glist2(&list) == &nodes[2]); break;
         case 3: TEST(last_glist3(&list) == &nodes[i]); TEST(first_glist3(&list) == &nodes[2]); break;
         }
         // check nodes
         for (unsigned i2 = 3, p = i, n = i==2?2:1; (i2--) > i; p = i2, n = n==i?2:n-1) {
            switch (tc) {
            case 1: TEST(next_glist1(&nodes[i2]) == &nodes[n]); TEST(prev_glist1(&nodes[i2]) == &nodes[p]); break;
            case 2: TEST(next_glist2(&nodes[i2]) == &nodes[n]); TEST(prev_glist2(&nodes[i2]) == &nodes[p]); break;
            case 3: TEST(next_glist3(&nodes[i2]) == &nodes[n]); TEST(prev_glist3(&nodes[i2]) == &nodes[p]); break;
            }
         }
         // test
         switch (tc) {
         case 1: removelast_glist1(&list, &node); break;
         case 2: removelast_glist2(&list, &node); break;
         case 3: removelast_glist3(&list, &node); break;
         }
         // check node
         TEST(&nodes[i] == node);
         TEST(0 == node->node.next  && 0 == node->node.prev);
         TEST(0 == node->next       && 0 == node->prev);
         TEST(0 == node->node2.next && 0 == node->node2.prev);
      }
      // check list
      TEST(0 == list.last);
      TEST(0 == list.first);
   }

   // TEST remove_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned r = 0; r < 3; ++r) {
         for (unsigned i = 0; i < 3; ++i) {
            switch (tc) {
            case 1: insertlast_glist1(&list, &nodes[i]); break;
            case 2: insertlast_glist2(&list, &nodes[i]); break;
            case 3: insertlast_glist3(&list, &nodes[i]); break;
            }
         }
         // test
         switch (tc) {
         case 1: remove_glist1(&list, &nodes[r]); break;
         case 2: remove_glist2(&list, &nodes[r]); break;
         case 3: remove_glist3(&list, &nodes[r]); break;
         }
         // check list
         switch (tc) {
         case 1: TEST(last_glist1(&list) == &nodes[2-(r==2)]); TEST(first_glist1(&list) == &nodes[r==0]); break;
         case 2: TEST(last_glist2(&list) == &nodes[2-(r==2)]); TEST(first_glist2(&list) == &nodes[r==0]); break;
         case 3: TEST(last_glist3(&list) == &nodes[2-(r==2)]); TEST(first_glist3(&list) == &nodes[r==0]); break;
         }
         // check nodes
         for (unsigned i = 0, p = (r==0), n = 2-(r==2); i < 2; ++i, n ^= p, p ^= n, n ^= p) {
            switch (tc) {
            case 1: TEST(next_glist1(&nodes[p]) == &nodes[n]); TEST(prev_glist1(&nodes[p]) == &nodes[n]); break;
            case 2: TEST(next_glist2(&nodes[p]) == &nodes[n]); TEST(prev_glist2(&nodes[p]) == &nodes[n]); break;
            case 3: TEST(next_glist3(&nodes[p]) == &nodes[n]); TEST(prev_glist3(&nodes[p]) == &nodes[n]); break;
            }
         }
         // check node
         TEST(0 == nodes[r].node.next  && 0 == nodes[r].node.prev);
         TEST(0 == nodes[r].next       && 0 == nodes[r].prev);
         TEST(0 == nodes[r].node2.next && 0 == nodes[r].node2.prev);
         // reset
         init_glist1(&list);
         memset(&nodes, 0, 3*sizeof(nodes[0]));
      }
   }

   // TEST replacenode_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned r = 0; r < 3; ++r) {
         for (unsigned i = 0; i < 3; ++i) {
            switch (tc) {
            case 1: insertlast_glist1(&list, &nodes[i]); break;
            case 2: insertlast_glist2(&list, &nodes[i]); break;
            case 3: insertlast_glist3(&list, &nodes[i]); break;
            }
         }
         // test
         switch (tc) {
         case 1: replacenode_glist1(&list, &nodes[r], &nodes[3]); break;
         case 2: replacenode_glist2(&list, &nodes[r], &nodes[3]); break;
         case 3: replacenode_glist3(&list, &nodes[r], &nodes[3]); break;
         }
         // check list
         switch (tc) {
         case 1: TEST(last_glist1(&list) == &nodes[3-(r!=2)]); TEST(first_glist1(&list) == &nodes[r==0?3:0]); break;
         case 2: TEST(last_glist2(&list) == &nodes[3-(r!=2)]); TEST(first_glist2(&list) == &nodes[r==0?3:0]); break;
         case 3: TEST(last_glist3(&list) == &nodes[3-(r!=2)]); TEST(first_glist3(&list) == &nodes[r==0?3:0]); break;
         }
         // check nodes
         for (unsigned i = 0, p = 2, n = 1; i < 3; p = i, ++i, n = (n+1) % 3) {
            switch (tc) {
            case 1: TEST(next_glist1(&nodes[i==r?3:i]) == &nodes[n==r?3:n]); TEST(prev_glist1(&nodes[i==r?3:i]) == &nodes[p==r?3:p]); break;
            case 2: TEST(next_glist2(&nodes[i==r?3:i]) == &nodes[n==r?3:n]); TEST(prev_glist2(&nodes[i==r?3:i]) == &nodes[p==r?3:p]); break;
            case 3: TEST(next_glist3(&nodes[i==r?3:i]) == &nodes[n==r?3:n]); TEST(prev_glist3(&nodes[i==r?3:i]) == &nodes[p==r?3:p]); break;
            }
         }
         // check node
         TEST(0 == nodes[r].node.next  && 0 == nodes[r].node.prev);
         TEST(0 == nodes[r].next       && 0 == nodes[r].prev);
         TEST(0 == nodes[r].node2.next && 0 == nodes[r].node2.prev);
         // reset
         init_glist1(&list);
         memset(&nodes, 0, 3*sizeof(nodes[0]));
      }
   }

   // == iterator ==

   // TEST initfirst_dlist2iterator: empty list
   TEST(ENODATA == initfirst_glist1iterator(&iter, &list));

   // TEST initlast_dlist2iterator: empty list
   TEST(ENODATA == initlast_glist2iterator(&iter, &list));

   // TEST free_dlist2iterator
   iter.next = (void*) 1;
   TEST(0 == free_glist3iterator(&iter));
   TEST(0 == iter.next);

   // test filled list
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         switch (tc) {
         case 1: insertlast_glist1(&list, &nodes[i]); break;
         case 2: insertlast_glist2(&list, &nodes[i]); break;
         case 3: insertlast_glist3(&list, &nodes[i]); break;
         }
      }

      // TEST initfirst_dlist2iterator: filled list
      switch (tc) {
      case 1: TEST(0 == initfirst_glist1iterator(&iter, &list)); break;
      case 2: TEST(0 == initfirst_glist2iterator(&iter, &list)); break;
      case 3: TEST(0 == initfirst_glist3iterator(&iter, &list)); break;
      }
      // check
      switch (tc) {
      case 1: TEST(iter.next == &nodes[0].node); break;
      case 2: TEST(iter.next == (void*) &nodes[0].next); break;
      case 3: TEST(iter.next == &nodes[0].node2); break;
      }

      // TEST initlast_dlist2iterator: filled list
      switch (tc) {
      case 1: TEST(0 == initlast_glist1iterator(&iter, &list)); break;
      case 2: TEST(0 == initlast_glist2iterator(&iter, &list)); break;
      case 3: TEST(0 == initlast_glist3iterator(&iter, &list)); break;
      }
      // check
      switch (tc) {
      case 1: TEST(iter.next == &nodes[lengthof(nodes)-1].node); break;
      case 2: TEST(iter.next == (void*) &nodes[lengthof(nodes)-1].next); break;
      case 3: TEST(iter.next == &nodes[lengthof(nodes)-1].node2); break;
      }

      // TEST prev_dlist2iterator: foreach
      unsigned i = 0;
      if (tc == 1) {
         foreach (_glist1, n, &list) {
            TEST(n == &nodes[i++]);
         }
      } else if (tc == 2) {
         foreach (_glist2, n, &list) {
            TEST(n == &nodes[i++]);
         }
      } else {
         foreach (_glist3, n, &list) {
            TEST(n == &nodes[i++]);
         }
      }
      TEST(i == lengthof(nodes));

      // TEST next_dlist2iterator: foreachReverse
      i = lengthof(nodes);
      if (tc == 1) {
         foreachReverse (_glist1, n, &list) {
            TEST(n == &nodes[--i]);
         }
      } else if (tc == 2) {
         foreachReverse (_glist2, n, &list) {
            TEST(n == &nodes[--i]);
         }
      } else {
         foreachReverse (_glist3, n, &list) {
            TEST(n == &nodes[--i]);
         }
      }
      TEST(i == 0);

      // reset
      init_glist2(&list);
      memset(&nodes, 0, sizeof(nodes));
   }

   // == setops ==

   // TEST removeall_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         switch (tc) {
         case 1: insertlast_glist1(&list, &nodes[i]); break;
         case 2: insertlast_glist2(&list, &nodes[i]); break;
         case 3: insertlast_glist3(&list, &nodes[i]); break;
         }
      }
      // test
      typeadapt.freenode_count = 0;
      switch (tc) {
      case 1: TEST(0 == removeall_glist1(&list, typeadp)); break;
      case 2: TEST(0 == removeall_glist2(&list, typeadp)); break;
      case 3: TEST(0 == removeall_glist3(&list, typeadp)); break;
      }
      // check list
      TEST(0 == list.last);
      TEST(0 == list.first);
      // check nodes
      TEST(lengthof(nodes) == typeadapt.freenode_count);
      for (unsigned i = 0; i < lengthof(nodes); ++i) {
         TEST(0 == nodes[i].node.next);
         TEST(0 == nodes[i].node.prev);
         TEST(0 == nodes[i].next);
         TEST(0 == nodes[i].prev);
         TEST(0 == nodes[i].node2.next);
         TEST(0 == nodes[i].node2.prev);
         TEST(1 == nodes[i].is_freed);
         nodes[i].is_freed = 0;
      }
   }

   // TEST insertlastPlist_dlist2
   for (int tc = 1; tc <= 3; ++tc) {
      for (unsigned i = 0; i < lengthof(nodes)/2; ++i) {
         switch (tc) {
         case 1: insertlast_glist1(&list, &nodes[i]); break;
         case 2: insertlast_glist2(&list, &nodes[i]); break;
         case 3: insertlast_glist3(&list, &nodes[i]); break;
         }
      }
      for (unsigned i = lengthof(nodes)/2; i < lengthof(nodes); ++i) {
         switch (tc) {
         case 1: insertlast_glist1(&list2, &nodes[i]); break;
         case 2: insertlast_glist2(&list2, &nodes[i]); break;
         case 3: insertlast_glist3(&list2, &nodes[i]); break;
         }
      }
      // test
      switch (tc) {
      case 1: insertlastPlist_glist1(&list, &list2); break;
      case 2: insertlastPlist_glist2(&list, &list2); break;
      case 3: insertlastPlist_glist3(&list, &list2); break;
      }
      // check list, list2
      TEST(0 == list2.last);
      TEST(0 == list2.first);
      switch (tc) {
      case 1: TEST(first_glist1(&list) == &nodes[0]); TEST(last_glist1(&list) == &nodes[lengthof(nodes)-1]); break;
      case 2: TEST(first_glist2(&list) == &nodes[0]); TEST(last_glist2(&list) == &nodes[lengthof(nodes)-1]); break;
      case 3: TEST(first_glist3(&list) == &nodes[0]); TEST(last_glist3(&list) == &nodes[lengthof(nodes)-1]); break;
      }
      // reset
      init_glist1(&list);
   }
   // check nodes
   for (unsigned i = 0, p = lengthof(nodes)-1, n = 1; i < lengthof(nodes); p = i, ++i, n = (n+1) % lengthof(nodes)) {
      TEST(next_glist1(&nodes[i]) == &nodes[n]);
      TEST(prev_glist1(&nodes[i]) == &nodes[p]);
      TEST(next_glist2(&nodes[i]) == &nodes[n]);
      TEST(prev_glist2(&nodes[i]) == &nodes[p]);
      TEST(next_glist3(&nodes[i]) == &nodes[n]);
      TEST(prev_glist3(&nodes[i]) == &nodes[p]);
   }
   // reset
   memset(&nodes, 0, sizeof(nodes));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_ds_inmem_dlist2()
{
   if (test_dlistnode())      goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_iterator())       goto ONERR;
   if (test_insert())         goto ONERR;
   if (test_remove())         goto ONERR;
   if (test_setops())         goto ONERR;
   if (test_generic())        goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
