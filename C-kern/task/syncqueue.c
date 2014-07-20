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

/* define: init_syncqueue
 * Implements <syncqueue_t.init_syncqueue>. */
int init_syncqueue(/*out*/syncqueue_t * syncqueue, uint16_t elemsize, uint8_t qidx)
{
   int err;

   err = init_queue(genericcast_queue(syncqueue), syncqueue_PAGESIZE);
   if (err) goto ONERR;

   syncqueue->elemsize = elemsize;
   syncqueue->qidx     = qidx;
   syncqueue->size     = 0;
   syncqueue->nextfree = 0;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_syncqueue(syncqueue_t * syncqueue)
{
   int err;

   err = free_queue(genericcast_queue(syncqueue));

   syncqueue->size = 0;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

bool isfree_syncqueue(const syncqueue_t * syncqueue)
{
   return 0 == syncqueue->last && 0 == syncqueue->size;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   syncqueue_t    syncqueue = syncqueue_FREE ;
   queue_t        queue ;

   // TEST syncqueue_t compatible with queue_t
   queue = (queue_t) queue_FREE ;
   TEST(0 == queue.last) ;
   queue = (queue_t) queue_INIT ;
   TEST(0 == queue.last) ;
   TEST((queue_t*)&syncqueue == genericcast_queue(&syncqueue)) ;

   // TEST syncqueue_FREE
   TEST(0 == syncqueue.last);
   TEST(0 == syncqueue.pagesize);
   TEST(0 == syncqueue.qidx);
   TEST(0 == syncqueue.elemsize);
   TEST(0 == syncqueue.size) ;
   TEST(0 == syncqueue.nextfree);
   TEST(1 == isfree_syncqueue(&syncqueue)) ;

   // TEST init_syncqueue
   for (int i = 1; i <= 128; ++i) {
      memset(&syncqueue, 255, sizeof(syncqueue)) ;
      init_syncqueue(&syncqueue, (uint16_t)i, (uint8_t)(i+1));
      TEST(0 == syncqueue.last);
      TEST(1 == syncqueue.pagesize);
      TEST(syncqueue_PAGESIZE == pagesize_queue(genericcast_queue(&syncqueue)));
      TEST(i == syncqueue.qidx-1);
      TEST(i == syncqueue.elemsize);
      TEST(0 == syncqueue.size) ;
      TEST(0 == syncqueue.nextfree);

      // preapre free: allocate some memory
      TEST(0 == preallocate_syncqueue(&syncqueue));
      TEST(0 != syncqueue.last);
      TEST(0 != syncqueue.size);
      TEST(0 != syncqueue.nextfree);
      TEST(&syncqueue == queuefromaddr_syncqueue(syncqueue.nextfree));

      // TEST free_syncqueue
      TEST(0 == free_syncqueue(&syncqueue)) ;
      TEST(0 == syncqueue.last);
      TEST(0 == syncqueue.size) ;
      TEST(0 == free_syncqueue(&syncqueue)) ;
      TEST(0 == syncqueue.last);
      TEST(0 == syncqueue.size) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_query(void)
{
   syncqueue_t syncqueue;
   bool        isinit = false;

   // TEST syncqueue_PAGESIZE
   static_assert(syncqueue_PAGESIZE == 1024, "supported by queue_t");

   // TEST isfree_syncqueue
   memset(&syncqueue, 255 ,sizeof(syncqueue)) ;
   TEST(0 == isfree_syncqueue(&syncqueue)) ;
   syncqueue.last = 0 ;
   syncqueue.size = 0 ;
   TEST(1 == isfree_syncqueue(&syncqueue)) ;
   syncqueue.last = (void*)1 ;
   TEST(0 == isfree_syncqueue(&syncqueue)) ;
   syncqueue.last = 0 ;
   syncqueue.size = 1 ;
   TEST(0 == isfree_syncqueue(&syncqueue)) ;
   syncqueue.size = 0 ;
   TEST(1 == isfree_syncqueue(&syncqueue)) ;

   // TEST elemsize_syncqueue
   for (uint16_t i = 1; i; i = (uint16_t) (i << 1)) {
      syncqueue.elemsize = i;
      TEST(i == elemsize_syncqueue(&syncqueue));
   }

   // TEST idx_syncqueue
   for (uint8_t i = 1; i; i = (uint8_t) (i << 1)) {
      syncqueue.qidx = i;
      TEST(i == idx_syncqueue(&syncqueue));
   }

   // TEST size_syncqueue
   for (size_t i = 1; i; i <<= 1 ) {
      syncqueue.size = i ;
      TEST(i == size_syncqueue(&syncqueue)) ;
   }
   syncqueue.size = 0;
   TEST(0 == size_syncqueue(&syncqueue));

   // TEST queuefromaddr_syncqueue
   init_syncqueue(&syncqueue, 512, 10);
   isinit = true;
   void * nodeaddr[128] = { 0 };
   for (unsigned i = 0; i < lengthof(nodeaddr); ++i) {
      TEST(0 == preallocate_syncqueue(&syncqueue));
      nodeaddr[i] = nextfree_syncqueue(&syncqueue);
   }
   for (unsigned i = 0; i < lengthof(nodeaddr); ++i) {
      for (unsigned offset = 0; offset < 512; ++offset) {
         TEST(&syncqueue == queuefromaddr_syncqueue(offset + (uint8_t*)nodeaddr[i]));
      }
   }
   isinit = false;
   TEST(0 == free_syncqueue(&syncqueue));

   // TEST nextfree_syncqueue
   for (uintptr_t i = 1; i; i <<= 1 ) {
      syncqueue.nextfree = (void*)i;
      TEST((void*)i == nextfree_syncqueue(&syncqueue));
   }
   syncqueue.nextfree = 0;
   TEST(0 == nextfree_syncqueue(&syncqueue));

   return 0;
ONERR:
   if (isinit) {
      free_syncqueue(&syncqueue);
   }
   return EINVAL;
}

typedef struct testelem_t     testelem_t;

/* struct: testelem_t
 * Stored in queue for testing purpose. */
struct testelem_t {
   size_t id;
};

static int test_update(void)
{
   syncqueue_t    syncqueue;
   queue_t        testelemlist;

   // prepare
   init_syncqueue(&syncqueue, sizeof(testelem_t), 1);
   init_queue(&testelemlist, defaultpagesize_queue());

   // TEST setnextfree_syncqueue
   for (uintptr_t i = 1; i; i <<= 1) {
      void * old = nextfree_syncqueue(&syncqueue);
      setnextfree_syncqueue(&syncqueue, (void*)i);
      TEST(syncqueue.nextfree == (void*)i);
      TEST(syncqueue.size     == 0);
      setnextfree_syncqueue(&syncqueue, old);
      TEST(syncqueue.nextfree == old);
   }

   // TEST preallocate_syncqueue
   TEST(0 == nextfree_syncqueue(&syncqueue));
   for (unsigned i = 1; i <= 5000; ++i) {
      TEST(0 == preallocate_syncqueue(&syncqueue));
      testelem_t * testelem = nextfree_syncqueue(&syncqueue);
      TEST(0 != testelem);
      testelem->id = i;
      TEST(i == syncqueue.size);
      TEST(testelem == last_queue(genericcast_queue(&syncqueue), sizeof(testelem_t)));
   }
   // check content
   for (unsigned i = 0; i == 0; ) {
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         testelem_t * testelem = elem;
         TEST(testelem->id == ++ i);
      }
      TEST(i == 5000);
   }

   // TEST removefirst_syncqueue
   for (unsigned i = 0; i == 0; ) {
      foreach (_queue, elem, genericcast_queue(&syncqueue), sizeof(testelem_t)) {
         void * ptr;
         TEST(0 == insertlast_queue(&testelemlist, &ptr, sizeof(void*)));
         *(void**)ptr = elem;
         ++ i;
      }
      TEST(i == 5000);
   }
   for (unsigned i = 0; i == 0; ) {
      foreach (_queue, elem, &testelemlist, sizeof(void*)) {
         testelem_t * testelem = *(void**)elem;
         TEST(testelem->id == ++ i);
         TEST(testelem == first_queue(genericcast_queue(&syncqueue), sizeof(testelem_t)));
         TEST(0 == removefirst_syncqueue(&syncqueue));
         TEST(5000-i == syncqueue.size);
         if (i == 2500) break;
      }
      TEST(i == 2500);
   }

   // TEST removelast_syncqueue
   for (unsigned i = 5000; i == 5000; ) {
      foreachReverse(_queue, elem, &testelemlist, sizeof(void*)) {
         testelem_t * testelem = *(void**)elem;
         TEST(testelem->id == i);
         --i;
         TEST(testelem == last_queue(genericcast_queue(&syncqueue), sizeof(testelem_t)));
         TEST(0 == removelast_syncqueue(&syncqueue));
         TEST(i-2500 == syncqueue.size);
         if (i == 2500) break;
      }
      TEST(i == 2500);
   }

   // TEST removefirst_syncqueue: ENODATA
   TEST(0 == syncqueue.size);
   TEST(ENODATA == removefirst_syncqueue(&syncqueue));
   TEST(0 == syncqueue.size);

   // TEST removelast_syncqueue: ENODATA
   TEST(0 == syncqueue.size);
   TEST(ENODATA == removelast_syncqueue(&syncqueue));
   TEST(0 == syncqueue.size);

   // unprepare
   TEST(0 == free_syncqueue(&syncqueue));
   TEST(0 == free_queue(&testelemlist));

   return 0;
ONERR:
   free_syncqueue(&syncqueue);
   free_queue(&testelemlist);
   return EINVAL;
}

int unittest_task_syncqueue()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_update())         goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
