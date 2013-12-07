/* title: SyncQueue impl

   Implements <SyncQueue>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/task/syncqueue.h
    Header file <SyncQueue>.

   file: C-kern/task/syncqueue.c
    Implementation file <SyncQueue impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncqueue.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/dlist.h"
#include "C-kern/api/ds/inmem/queue.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/ds/foreach.h"
#endif


// section: syncqueue_t

// group: lifetime

int free_syncqueue(syncqueue_t * syncqueue)
{
   int err ;

   err = free_queue(genericcast_queue(syncqueue)) ;

   syncqueue->nrelements = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

bool isfree_syncqueue(const syncqueue_t * syncqueue)
{
   return 0 == syncqueue->last && 0 == syncqueue->nrelements ;
}

// group: update

int compact2_syncqueue(syncqueue_t * syncqueue, uint16_t elemsize, struct dlist_t * freelist, void (*initmove_elem)(void * dest, void * src))
{
   int err = 0 ;
   dlist_node_t * firstfree = first_dlist(freelist) ;

   /* last node in freelist points to first free entry in queue */
   /* firstnode in freelist points to last free entry in queue */

   while (!isempty_dlist(freelist)) {
      void * lastentry = last_queue(genericcast_queue(syncqueue), elemsize) ;

      if (lastentry != firstfree) {
         dlist_node_t * lastfree ;
         err = removelast_dlist(freelist, &lastfree) ;
         if (err) goto ONABORT ;
         initmove_elem(lastfree, lastentry) ;
      } else {
         err = removefirst_dlist(freelist, &firstfree) ;
         if (err) goto ONABORT ;
         firstfree = first_dlist(freelist) ;
      }

      err = removelast_queue(genericcast_queue(syncqueue), elemsize) ;
      if (err) goto ONABORT ;

      -- syncqueue->nrelements ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   syncqueue_t    syncqueue = syncqueue_INIT_FREEABLE ;
   queue_t        queue ;

   // TEST syncqueue_t compatible with queue_t
   queue = (queue_t) queue_INIT_FREEABLE ;
   TEST(0 == queue.last) ;
   queue = (queue_t) queue_INIT ;
   TEST(0 == queue.last) ;
   TEST((queue_t*)&syncqueue == genericcast_queue(&syncqueue)) ;

   // TEST syncqueue_INIT_FREEABLE
   TEST(0 == syncqueue.last) ;
   TEST(0 == syncqueue.nrelements) ;

   // TEST syncqueue_INIT
   memset(&syncqueue, 255, sizeof(syncqueue)) ;
   syncqueue = (syncqueue_t) syncqueue_INIT ;
   TEST(1 == isfree_syncqueue(&syncqueue)) ;

   // TEST init_syncqueue
   memset(&syncqueue, 255, sizeof(syncqueue)) ;
   init_syncqueue(&syncqueue) ;
   TEST(1 == isfree_syncqueue(&syncqueue)) ;

   // TEST free_syncqueue
   void * dummy ;
   TEST(0 == syncqueue.last) ;
   TEST(0 == insertfirst_queue(genericcast_queue(&syncqueue), &dummy, 24))
   TEST(0 != syncqueue.last) ;
   syncqueue.nrelements = 1 ;
   TEST(0 == free_syncqueue(&syncqueue)) ;
   TEST(1 == isfree_syncqueue(&syncqueue)) ;
   TEST(0 == free_syncqueue(&syncqueue)) ;
   TEST(1 == isfree_syncqueue(&syncqueue)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   syncqueue_t    syncqueue ;
   syncqueue_t    syncqueue2 = syncqueue_INIT_FREEABLE ;

   // TEST isfree_syncqueue
   memset(&syncqueue, 255 ,sizeof(syncqueue)) ;
   TEST(0 == isfree_syncqueue(&syncqueue)) ;
   syncqueue.last = 0 ;
   syncqueue.nrelements = 0 ;
   TEST(1 == isfree_syncqueue(&syncqueue)) ;
   syncqueue.last = (void*)1 ;
   TEST(0 == isfree_syncqueue(&syncqueue)) ;
   syncqueue.last = 0 ;
   syncqueue.nrelements = 1 ;
   TEST(0 == isfree_syncqueue(&syncqueue)) ;
   syncqueue.nrelements = 0 ;
   TEST(1 == isfree_syncqueue(&syncqueue)) ;

   // TEST len_syncqueue
   syncqueue.nrelements = 0 ;
   TEST(0 == len_syncqueue(&syncqueue)) ;
   for (size_t i = 1; i; i <<= 1 ) {
      syncqueue.nrelements = i ;
      TEST(i == len_syncqueue(&syncqueue)) ;
   }

   // TEST queuefromaddr_syncqueue
   init_syncqueue(&syncqueue2) ;
   void * nodeaddr[128] = { 0 } ;
   for (unsigned i = 0; i < lengthof(nodeaddr); ++i) {
      TEST(0 == insertlast_queue(genericcast_queue(&syncqueue2), &nodeaddr[i], 128)) ;
   }
   for (unsigned i = 0; i < lengthof(nodeaddr); ++i) {
      for (unsigned offset = 0; offset < 128; ++offset) {
         TEST(&syncqueue2 == queuefromaddr_syncqueue(offset+(uint8_t*)nodeaddr[i])) ;
      }
   }
   TEST(0 == free_syncqueue(&syncqueue2)) ;

   return 0 ;
ONABORT:
   free_syncqueue(&syncqueue2) ;
   return EINVAL ;
}

typedef struct testelem_t     testelem_t ;

/* struct: testelem_t
 * Element which is interconnected to a buddy with <buddy>. */
struct testelem_t {
   /* variable: buddy
    * Points to buddy node.
    * The buddy points back to this node. */
   testelem_t *   buddy ;
   size_t         id ;
   size_t         buddy_id ;
} ;

static size_t s_testelem_initcount = 0 ;
static size_t s_testelem_initmovecount = 0 ;

static void init_testelem(testelem_t * testelem, size_t id)
{
   ++ s_testelem_initcount ;
   testelem->buddy = 0 ;
   testelem->id    = id ;
   testelem->buddy_id = 0 ;
}

static void initmove_testelem(testelem_t * dest, testelem_t * src)
{
   ++ s_testelem_initmovecount ;
   // check src is connected
   assert(src->buddy != 0) ;
   assert(src->buddy->buddy == src) ;
   src->buddy->buddy = dest ;
   *dest = *src ;
}

static void connect_testelem(testelem_t * testelem, testelem_t * buddy)
{
   assert(testelem->buddy == 0) ;
   assert(buddy->buddy    == 0) ;
   testelem->buddy = buddy ;
   buddy->buddy    = testelem ;
   testelem->buddy_id = buddy->id ;
   buddy->buddy_id    = testelem->id ;
}

static int test_update(void)
{
   syncqueue_t    syncqueue ;
   queue_t        testelemlist ;

   // prepare
   init_syncqueue(&syncqueue) ;
   init_queue(&testelemlist) ;

   // TEST insert2_syncqueue
   for (uint16_t elemsize = sizeof(testelem_t); elemsize <= 512; elemsize = (uint16_t)(elemsize + 16u)) {
      s_testelem_initcount = 0 ;
      testelem_t * testelem = 0 ;
      TEST(0 == insert2_syncqueue(&syncqueue, elemsize, &testelem)) ;
      TEST(0 != testelem) ;
      init_testelem(testelem, elemsize) ;
      TEST(1 == syncqueue.nrelements) ;
      TEST(1 == s_testelem_initcount) ;
      TEST(elemsize == sizelast_queue(genericcast_queue(&syncqueue))) ;
      testelem_t * testelem1 = last_queue(genericcast_queue(&syncqueue), elemsize) ;
      TEST(testelem == testelem1) ;
      TEST(0 == insert2_syncqueue(&syncqueue, elemsize, &testelem)) ;
      TEST(0 != testelem) ;
      init_testelem(testelem, 2u*elemsize) ;
      TEST(2 == syncqueue.nrelements) ;
      TEST(2 == s_testelem_initcount) ;
      testelem_t * testelem2 = last_queue(genericcast_queue(&syncqueue), elemsize) ;
      TEST(testelem == testelem2) ;
      TEST(testelem1->id == elemsize) ;
      TEST(testelem2->id == 2u*elemsize) ;
      TEST(0 == free_syncqueue(&syncqueue)) ;
      init_syncqueue(&syncqueue) ;
   }

   // TEST insert2_syncqueue: EINVAL
   {
      s_testelem_initcount = 0 ;
      testelem_t * dummy ;
      TEST(EINVAL == insert2_syncqueue(&syncqueue, 65535, &dummy)) ;
      TEST(0 == syncqueue.nrelements) ;
      TEST(0 == s_testelem_initcount) ;
   }

   // TEST insert_syncqueue
   s_testelem_initcount = 0 ;
   for (unsigned i = 0; i < 5000; ++i) {
      size_t oldsize = syncqueue.nrelements ;
      testelem_t * testelem = 0 ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem)) ;
      TEST(0 != testelem) ;
      init_testelem(testelem, 2u*i) ;
      TEST(oldsize+1 == syncqueue.nrelements) ;
      TEST(oldsize+1 == s_testelem_initcount) ;
      testelem_t * testelem1 = last_queue(genericcast_queue(&syncqueue), sizeof(testelem_t)) ;
      TEST(testelem == testelem1) ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem)) ;
      TEST(0 != testelem) ;
      init_testelem(testelem, 2*i+1) ;
      TEST(oldsize+2 == syncqueue.nrelements) ;
      TEST(oldsize+2 == s_testelem_initcount) ;
      testelem_t * testelem2 = last_queue(genericcast_queue(&syncqueue), sizeof(testelem_t)) ;
      TEST(testelem == testelem2) ;
      connect_testelem(testelem1, testelem2) ;
   }
   // check for correct content
   for (unsigned i = 0; i == 0; ) {
      testelem_t * buddy = 0 ;
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         testelem_t * testelem = elem ;
         TEST(testelem->id == i) ;
         ++ i ;
         if (!buddy) {
            buddy = testelem ;
         } else {
            TEST(buddy        == testelem->buddy) ;
            TEST(buddy->id    == testelem->buddy_id) ;
            TEST(testelem     == buddy->buddy) ;
            TEST(testelem->id == buddy->buddy_id) ;
            buddy = 0 ;
         }
      }
      TEST(i == 10000) ;
   }

   // TEST insert_syncqueue: EINVAL
   struct LARGE {
      uint8_t large[1024] ;
   } * large ;
   TEST(10000 == s_testelem_initcount) ;
   TEST(10000 == syncqueue.nrelements) ;
   TEST(EINVAL == insert_syncqueue(&syncqueue, &large)) ;
   TEST(10000 == syncqueue.nrelements) ;
   TEST(10000 == s_testelem_initcount) ;

   // TEST remove_syncqueue: remove elements beginning from first
   for (unsigned i = 0; i == 0; ) {
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         void * ptr ;
         TEST(0 == insertlast_queue(&testelemlist, &ptr, sizeof(void*))) ;
         *(void**)ptr = elem ;
         ++ i ;
         if (i == 5000) break ;
      }
      TEST(i == 5000) ;
   }
   s_testelem_initmovecount = 0 ;
   for (unsigned i = 0; i == 0; ) {
      foreach (_queue, elem, &testelemlist, sizeof(void*)) {
         ++ i ;
         TEST(0 == remove_syncqueue(&syncqueue, *(testelem_t**)elem, &initmove_testelem))
         TEST(i == s_testelem_initmovecount) ;
         TEST(10000-i == syncqueue.nrelements) ;
      }
      TEST(i == 5000) ;
   }
   // check for correct content
   for (unsigned i = 0; i == 0; ) {
      testelem_t * buddy = 0 ;
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         testelem_t * testelem = elem ;
         TEST(testelem->id == 10000-1-i) ;
         ++ i ;
         if (!buddy) {
            buddy = testelem ;
         } else {
            TEST(buddy        == testelem->buddy) ;
            TEST(buddy->id    == testelem->buddy_id) ;
            TEST(testelem     == buddy->buddy) ;
            TEST(testelem->id == buddy->buddy_id) ;
            buddy = 0 ;
         }
      }
      TEST(i == 5000) ;
   }

   // TEST remove_syncqueue: remove elements beginning from last
   s_testelem_initmovecount = 0 ;
   for (unsigned i = 0; i < 2500; ++i) {
      testelem_t * last1 = last_queue(genericcast_queue(&syncqueue), sizeof(testelem_t)) ;
      testelem_t * last1_buddy = last1->buddy ;
      TEST(last1->id == 5000+2*i) ;
      TEST(0 == remove_syncqueue(&syncqueue, last1, &initmove_testelem)) ;
      TEST(0 == s_testelem_initmovecount) ;
      TEST(5000-1-2*i == syncqueue.nrelements) ;
      testelem_t * last2 = last_queue(genericcast_queue(&syncqueue), sizeof(testelem_t)) ;
      TEST(last2->id == 5000+1+2*i) ;
      TEST(last2->buddy == last1) ;
      TEST(last1_buddy  == last2) ;
      TEST(0 == remove_syncqueue(&syncqueue, last2, &initmove_testelem)) ;
      TEST(0 == s_testelem_initmovecount) ;
      TEST(5000-2-2*i == syncqueue.nrelements) ;
   }

   // TEST remove_syncqueue: ENODATA
   TEST(0 == syncqueue.nrelements) ;
   testelem_t dummy ;
   TEST(ENODATA == remove_syncqueue(&syncqueue, &dummy, &initmove_testelem)) ;
   TEST(0 == s_testelem_initmovecount) ;
   TEST(0 == syncqueue.nrelements) ;

   // TEST removefirst_syncqueue
   s_testelem_initcount = 0 ;
   for (unsigned i = 0; i < 5000; ++i) {
      testelem_t * testelem1 = 0 ;
      testelem_t * testelem2 = 0 ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem1)) ;
      init_testelem(testelem1, 2*i) ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem2)) ;
      init_testelem(testelem2, 2*i+1) ;
      connect_testelem(testelem1, testelem2) ;
   }
   TEST(10000 == syncqueue.nrelements) ;
   TEST(10000 == s_testelem_initcount) ;
   for (unsigned i = 0; i < 5000; ++i) {
      TEST(0 == removefirst_syncqueue(&syncqueue, sizeof(testelem_t))) ;
      TEST(10000-1-i == syncqueue.nrelements) ;
   }
   // check content
   for (unsigned i = 5000; i == 5000; ) {
      testelem_t * buddy = 0 ;
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         testelem_t * testelem = elem ;
         TEST(testelem->id == i) ;
         if (buddy) {
            TEST(buddy        == testelem->buddy) ;
            TEST(buddy->id    == testelem->buddy_id) ;
            TEST(testelem     == buddy->buddy) ;
            TEST(testelem->id == buddy->buddy_id) ;
         }
         buddy = buddy ? 0 : testelem ;
         ++ i ;
      }
      TEST(i == 10000) ;
   }
   for (unsigned i = 0; i < 5000; ++i) {
      TEST(0 == removefirst_syncqueue(&syncqueue, sizeof(testelem_t))) ;
      TEST(5000-1-i == syncqueue.nrelements) ;
   }
   TEST(1 == isfree_syncqueue(&syncqueue)) ;

   // TEST removelast_syncqueue
   s_testelem_initcount = 0 ;
   for (unsigned i = 0; i < 5000; ++i) {
      testelem_t * testelem1 = 0 ;
      testelem_t * testelem2 = 0 ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem1)) ;
      init_testelem(testelem1, 2*i) ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem2)) ;
      init_testelem(testelem2, 2*i+1) ;
      connect_testelem(testelem1, testelem2) ;
   }
   TEST(10000 == syncqueue.nrelements) ;
   TEST(10000 == s_testelem_initcount) ;
   for (unsigned i = 0; i < 5000; ++i) {
      TEST(0 == removelast_syncqueue(&syncqueue, sizeof(testelem_t))) ;
      TEST(10000-1-i == syncqueue.nrelements) ;
   }
   // check content
   for (unsigned i = 0; i == 0; ) {
      testelem_t * buddy = 0 ;
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         testelem_t * testelem = elem ;
         TEST(testelem->id == i) ;
         if (buddy) {
            TEST(buddy        == testelem->buddy) ;
            TEST(buddy->id    == testelem->buddy_id) ;
            TEST(testelem     == buddy->buddy) ;
            TEST(testelem->id == buddy->buddy_id) ;
         }
         buddy = buddy ? 0 : testelem ;
         ++ i ;
      }
      TEST(i == 5000) ;
   }
   for (unsigned i = 0; i < 5000; ++i) {
      TEST(0 == removelast_syncqueue(&syncqueue, sizeof(testelem_t))) ;
      TEST(5000-1-i == syncqueue.nrelements) ;
   }
   TEST(1 == isfree_syncqueue(&syncqueue)) ;

   // TEST removefirst_syncqueue: ENODATA
   TEST(0 == syncqueue.nrelements) ;
   TEST(ENODATA == removefirst_syncqueue(&syncqueue, sizeof(testelem_t))) ;
   TEST(0 == syncqueue.nrelements) ;

   // TEST addtofreelist_syncqueue, compact_syncqueue: free last half
   dlist_t freelist = dlist_INIT ;
   s_testelem_initcount = 0 ;
   for (unsigned i = 0; i < 5000; ++i) {
      testelem_t * testelem1 = 0 ;
      testelem_t * testelem2 = 0 ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem1)) ;
      init_testelem(testelem1, 2*i) ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem2)) ;
      init_testelem(testelem2, 2*i+1) ;
      connect_testelem(testelem1, testelem2) ;
   }
   TEST(10000 == syncqueue.nrelements) ;
   TEST(10000 == s_testelem_initcount) ;
   for (unsigned i = 0; i == 0; ) {
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         ++ i ;
         if (5000 < i) {
            addtofreelist_syncqueue(&syncqueue, &freelist, (testelem_t*)elem) ;
         }
      }
      TEST(10000 == i) ;
      TEST(10000 == syncqueue.nrelements) ;
   }
   s_testelem_initmovecount = 0 ;
   TEST(0 != freelist.last) ;
   TEST(0 == compact_syncqueue(&syncqueue, testelem_t, &freelist, &initmove_testelem)) ;
   TEST(0 == freelist.last) ;
   TEST(0 == s_testelem_initmovecount) ;
   // check content
   for (unsigned i = 0; i == 0; ) {
      testelem_t * buddy = 0 ;
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         testelem_t * testelem = elem ;
         TEST(testelem->id == i) ;
         if (buddy) {
            TEST(buddy        == testelem->buddy) ;
            TEST(buddy->id    == testelem->buddy_id) ;
            TEST(testelem     == buddy->buddy) ;
            TEST(testelem->id == buddy->buddy_id) ;
         }
         buddy = buddy ? 0 : testelem ;
         ++ i ;
      }
      TEST(i == 5000) ;
   }
   TEST(0 == free_syncqueue(&syncqueue)) ;

   // TEST addtofreelist_syncqueue, compact_syncqueue: free first half
   s_testelem_initcount = 0 ;
   for (unsigned i = 0; i < 5000; ++i) {
      testelem_t * testelem1 = 0 ;
      testelem_t * testelem2 = 0 ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem1)) ;
      init_testelem(testelem1, 2*i) ;
      TEST(0 == insert_syncqueue(&syncqueue, &testelem2)) ;
      init_testelem(testelem2, 2*i+1) ;
      connect_testelem(testelem1, testelem2) ;
   }
   TEST(10000 == syncqueue.nrelements) ;
   TEST(10000 == s_testelem_initcount) ;
   for (unsigned i = 0; i == 0; ) {
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         ++ i ;
         if (i <= 5000) {
            addtofreelist_syncqueue(&syncqueue, &freelist, (testelem_t*)elem) ;
         }
      }
      TEST(10000 == i) ;
      TEST(10000 == syncqueue.nrelements) ;
   }
   s_testelem_initmovecount = 0 ;
   TEST(0 == compact_syncqueue(&syncqueue, testelem_t, &freelist, &initmove_testelem)) ;
   TEST(5000 == s_testelem_initmovecount) ;
   // check content
   for (unsigned i = 0; i == 0; ) {
      testelem_t * buddy = 0 ;
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         testelem_t * testelem = elem ;
         TEST(testelem->id == 10000-1-i) ;
         if (buddy) {
            TEST(buddy        == testelem->buddy) ;
            TEST(buddy->id    == testelem->buddy_id) ;
            TEST(testelem     == buddy->buddy) ;
            TEST(testelem->id == buddy->buddy_id) ;
         }
         buddy = buddy ? 0 : testelem ;
         ++ i ;
      }
      TEST(i == 5000) ;
   }

   // unprepare
   TEST(0 == free_syncqueue(&syncqueue)) ;
   TEST(0 == free_queue(&testelemlist)) ;

   return 0 ;
ONABORT:
   free_syncqueue(&syncqueue) ;
   free_queue(&testelemlist) ;
   return EINVAL ;
}

int unittest_task_syncqueue()
{
   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_update())         goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
