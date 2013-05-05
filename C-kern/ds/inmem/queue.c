/* title: Queue impl

   Implements <Queue>.

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

   file: C-kern/api/ds/inmem/queue.h
    Header file <Queue>.

   file: C-kern/ds/inmem/queue.c
    Implementation file <Queue impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/queue.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/dlist.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/ds/foreach.h"
#endif


// section: queue_page_t

// group: variable

#ifdef KONFIG_UNITTEST
static test_errortimer_t      s_queuepage_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: helper

static inline void compiletime_assert(void)
{
   static_assert(offsetof(queue_page_t, next) == 0, "next pointer could be cast into (queue_page_t*)") ;
}

// group: lifetime

/* function: new_queuepage
 * Allocates single memory page with <ALLOC_PAGECACHE> with size <pagesize_4096>. */
static int new_queuepage(/*out*/queue_page_t ** qpage, queue_t * queue)
{
   int err ;
   memblock_t page ;

   ONERROR_testerrortimer(&s_queuepage_errtimer, ONABORT) ;
   static_assert(pagesizeinbytes_queue() == 4096u, "pagesizeinbytes_queue() matches pagesize_4096") ;
   err = ALLOC_PAGECACHE(pagesize_4096, &page) ;
   if (err) goto ONABORT ;

   queue_page_t * new_qpage = (queue_page_t*) page.addr ;

   static_assert(0 == (sizeof(queue_page_t) % KONFIG_MEMALIGN), "first byte is correct aligned") ;
   // new_qpage->next      = ; // is set in addlastpage_queue, addfirstpage_queue
   // new_qpage->prev      = ; // is set in addlastpage_queue, addfirstpage_queue
   new_qpage->queue        = queue ;
   // new_qpage->end_offset   = ; // is set in addlastpage_queue, addfirstpage_queue
   // new_qpage->start_offset = ; // is set in addlastpage_queue, addfirstpage_queue

   *qpage = new_qpage ;

   return 0 ;
ONABORT:
   return err ;
}

/* function: delete_queuepage
 * Frees single memory page with <RELEASE_PAGECACHE>. */
static int delete_queuepage(queue_page_t ** qpage)
{
   int err ;
   queue_page_t * del_qpage = *qpage ;

   if (del_qpage) {
      *qpage = 0 ;

      memblock_t page = memblock_INIT(pagesizeinbytes_queue(), (uint8_t*)del_qpage) ;

      err = RELEASE_PAGECACHE(&page) ;

      if (err) goto ONABORT ;
      ONERROR_testerrortimer(&s_queuepage_errtimer, ONABORT) ;
   }

   return 0 ;
ONABORT:
   return err ;
}

// group: query

/* function: end_queuepage
 * Returns address after the last element on page. */
static void * end_queuepage(queue_page_t * qpage)
{
   return (uint8_t*)qpage + qpage->end_offset ;
}

/* function: end_queuepage
 * Returns address after the last element on page. */
static inline bool isempty_queuepage(const queue_page_t * qpage)
{
   return qpage->end_offset == qpage->start_offset ;
}


// group: update

static inline int pushfirst_queuepage(queue_page_t * qpage, /*out*/void ** nodeaddr, uint16_t nodesize)
{
   if ((qpage->start_offset-sizeof(queue_page_t)) < nodesize) return ENOMEM ;

   qpage->start_offset -= nodesize ;

   *nodeaddr = (uint8_t*)qpage + qpage->start_offset ;

   return 0 ;
}

static inline int pushlast_queuepage(queue_page_t * qpage, uint32_t pagesize, /*out*/void  ** nodeaddr, uint16_t nodesize)
{
   if ((qpage->end_offset + nodesize) > pagesize) return ENOMEM ;

   *nodeaddr = (uint8_t*)qpage + qpage->end_offset ;

   qpage->end_offset = (qpage->end_offset + nodesize) ;

   return 0 ;
}

static inline int removefirst_queuepage(queue_page_t * qpage, uint16_t nodesize)
{
   if ((qpage->start_offset + nodesize) > qpage->end_offset) return EOVERFLOW ;

   qpage->start_offset = (qpage->start_offset + nodesize) ;

   return 0 ;
}

static inline int removelast_queuepage(queue_page_t * qpage, uint16_t nodesize)
{
   if ((qpage->end_offset - qpage->start_offset) < nodesize) return EOVERFLOW ;

   qpage->end_offset = (qpage->end_offset - nodesize) ;

   return 0 ;
}



// section: queue_t

// group: helper

dlist_IMPLEMENT(_pagelist, queue_page_t, )

static inline int addlastpage_queue(queue_t * queue)
{
   int err ;
   queue_page_t * qpage ;

   err = new_queuepage(&qpage, queue) ;
   if (err) return err ;

   qpage->end_offset   = sizeof(queue_page_t) ;
   qpage->start_offset = sizeof(queue_page_t) ;
   insertlast_pagelist(genericcast_dlist(queue), qpage) ;

   return 0 ;
}

static inline int addfirstpage_queue(queue_t * queue)
{
   int err ;
   queue_page_t * qpage ;

   err = new_queuepage(&qpage, queue) ;
   if (err) return err ;

   qpage->end_offset   = pagesizeinbytes_queue() ;
   qpage->start_offset = pagesizeinbytes_queue() ;
   insertfirst_pagelist(genericcast_dlist(queue), qpage) ;

   return 0 ;
}

static inline int removefirstpage_queue(queue_t * queue)
{
   int err ;
   queue_page_t * first ;

   err = removefirst_pagelist(genericcast_dlist(queue), &first) ;
   if (err) return err ;

   err = delete_queuepage(&first) ;
   if (err) return err ;

   return 0 ;
}

static inline int removelastpage_queue(queue_t * queue)
{
   int err ;
   queue_page_t * last ;

   err = removelast_pagelist(genericcast_dlist(queue), &last) ;
   if (err) return err ;

   err = delete_queuepage(&last) ;
   if (err) return err ;

   return 0 ;
}

// group: lifetime

void initmove_queue(/*out*/queue_t * dest, queue_t * src)
{
   dest->last = src->last ;
   src->last = 0 ;

   queue_page_t * last = last_pagelist(genericcast_dlist(dest)) ;
   if (last) {
      queue_page_t * qpage = last ;
      do {
         qpage->queue = dest ;
         qpage = next_pagelist(qpage) ;
      } while (qpage != last) ;
   }
}

int free_queue(queue_t * queue)
{
   int err ;

   if (queue->last) {
      queue_page_t * qpage = last_pagelist(genericcast_dlist(queue)) ;
      qpage = next_pagelist(qpage) ;
      queue->last->next = 0 ;
      queue->last = 0 ;
      err = 0 ;
      do {
         queue_page_t * delpage = qpage ;
         qpage = next_pagelist(qpage) ;
         int err2 = delete_queuepage(&delpage) ;
         if (err2) err = err2 ;
      } while (qpage) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: update

int insertfirst_queue(queue_t * queue, /*out*/void ** nodeaddr, uint16_t nodesize)
{
   int err ;

   VALIDATE_INPARAM_TEST(nodesize <= 512, ONABORT, ) ;

   queue_page_t * first = first_pagelist(genericcast_dlist(queue)) ;

   if (!first || 0 != pushfirst_queuepage(first, nodeaddr, nodesize)) {
      err = addfirstpage_queue(queue) ;
      if (err) goto ONABORT ;

      first = first_pagelist(genericcast_dlist(queue)) ;

      err = pushfirst_queuepage(first, nodeaddr, nodesize) ;
      assert(!err) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int insertlast_queue(queue_t * queue, /*out*/void ** nodeaddr, uint16_t nodesize)
{
   int err ;

   VALIDATE_INPARAM_TEST(nodesize <= 512, ONABORT, ) ;

   queue_page_t * last = last_pagelist(genericcast_dlist(queue)) ;

   if (!last || 0 != pushlast_queuepage(last, pagesizeinbytes_queue(), nodeaddr, nodesize)) {
      err = addlastpage_queue(queue) ;
      if (err) goto ONABORT ;

      last = last_pagelist(genericcast_dlist(queue)) ;

      err = pushlast_queuepage(last, pagesizeinbytes_queue(), nodeaddr, nodesize) ;
      assert(!err) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removefirst_queue(queue_t * queue, uint16_t nodesize)
{
   int err ;
   queue_page_t * first = first_pagelist(genericcast_dlist(queue)) ;

   if (!first) {
      err = ENODATA ;
      goto ONABORT ;
   }

   err = removefirst_queuepage(first, nodesize) ;
   if (err) goto ONABORT ;

   if (isempty_queuepage(first)) {
      err = removefirstpage_queue(queue) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removelast_queue(queue_t * queue, uint16_t nodesize)
{
   int err ;
   queue_page_t * last = last_pagelist(genericcast_dlist(queue)) ;

   if (!last) {
      err = ENODATA ;
      goto ONABORT ;
   }

   err = removelast_queuepage(last, nodesize) ;
   if (err) goto ONABORT ;

   if (isempty_queuepage(last)) {
      err = removelastpage_queue(queue) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int resizelast_queue(queue_t * queue, /*out*/void ** nodeaddr, uint16_t oldsize, uint16_t newsize)
{
   int err ;
   queue_page_t * last = last_pagelist(genericcast_dlist(queue)) ;

   VALIDATE_INPARAM_TEST(newsize <= 512, ONABORT, ) ;

   if (!last) {
      err = ENODATA ;
      goto ONABORT ;
   }

   err = removelast_queuepage(last, oldsize) ;
   if (err) goto ONABORT ;

   if (0 != pushlast_queuepage(last, pagesizeinbytes_queue(), nodeaddr, newsize)) {
      void * oldcontent = end_queuepage(last) ;
      if (isempty_queuepage(last)) {
         // shifts content to start of page
         last->end_offset   = sizeof(queue_page_t) ;
         last->start_offset = sizeof(queue_page_t) ;
      } else {
         err = addlastpage_queue(queue) ;
         if (err) {
            void * dummy ;
            (void) pushlast_queuepage(last, pagesizeinbytes_queue(), &dummy, oldsize) ;
            goto ONABORT ;
         }
      }
      last = last_pagelist(genericcast_dlist(queue)) ;
      err = pushlast_queuepage(last, pagesizeinbytes_queue(), nodeaddr, newsize) ;
      assert(!err) ;
      memmove(*nodeaddr, oldcontent, oldsize/* oldsize < newsize !*/) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_queuepage(void)
{
   queue_t        queue  = queue_INIT ;
   queue_page_t * qpage  = 0 ;

   // TEST new_queuepage
   size_t oldsize = SIZEALLOCATED_PAGECACHE() ;
   TEST(0 == new_queuepage(&qpage, &queue)) ;
   TEST(0 != qpage) ;
   TEST(qpage->queue == &queue) ;
   TEST(SIZEALLOCATED_PAGECACHE() == oldsize + pagesizeinbytes_queue()) ;

   // TEST delete_queuepage
   TEST(0 == delete_queuepage(&qpage)) ;
   TEST(0 == qpage) ;
   TEST(SIZEALLOCATED_PAGECACHE() == oldsize) ;
   TEST(0 == delete_queuepage(&qpage)) ;
   TEST(0 == qpage) ;

   // TEST new_queuepage: ENOMEM
   TEST(0 == new_queuepage(&qpage, &queue)) ;
   queue_page_t * const old = qpage ;
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == new_queuepage(&qpage, &queue)) ;
   TEST(old == qpage) ;

   // TEST delete_queuepage: ENOMEM
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM) ;
   TEST(qpage  != 0) ;
   TEST(ENOMEM == delete_queuepage(&qpage)) ;
   TEST(qpage  == 0) ;

   // TEST end_queuepage
   TEST(0 == new_queuepage(&qpage, &queue)) ;
   qpage->end_offset = 0 ;
   TEST(qpage == end_queuepage(qpage)) ;
   for (uint32_t i = 1; i; i <<= 1) {
      qpage->end_offset = i ;
      TEST(i + (uint8_t*)qpage == end_queuepage(qpage)) ;
   }
   TEST(0 == delete_queuepage(&qpage)) ;

   // TEST isempty_queuepage
   TEST(0 == new_queuepage(&qpage, &queue)) ;
   for (uint32_t i = 0; i < 100; ++i) {
      qpage->end_offset = i ;
      qpage->start_offset = i ;
      TEST(1 == isempty_queuepage(qpage)) ;
      qpage->end_offset = i + 1 + i ;
      qpage->start_offset = i ;
      TEST(0 == isempty_queuepage(qpage)) ;
      qpage->end_offset = i ;
      qpage->start_offset = i + 1 + i ;
      TEST(0 == isempty_queuepage(qpage)) ;
   }
   TEST(0 == delete_queuepage(&qpage)) ;

   return 0 ;
ONABORT:
   delete_queuepage(&qpage) ;
   return EINVAL ;
}

static int test_initfree(void)
{
   queue_t queue = queue_INIT_FREEABLE ;

   // TEST queue_INIT_FREEABLE
   TEST(0 == queue.last) ;

   // TEST queue_INIT
   queue = (queue_t) queue_INIT ;
   TEST(0 == queue.last) ;

   // TEST init_queue
   memset(&queue, 255, sizeof(queue)) ;
   TEST(0 == init_queue(&queue)) ;
   TEST(0 == queue.last) ;

   // TEST addlastpage_queue
   TEST(0 == queue.last) ;
   TEST(0 == addlastpage_queue(&queue)) ;
   TEST(0 != queue.last) ;
   queue_page_t * first = (queue_page_t*)queue.last ;
   TEST(queue.last == first->next) ;
   for (int i = 0; i < 5; ++i) {
      queue_page_t * last1 = (queue_page_t*)queue.last ;
      TEST(0 == addlastpage_queue(&queue)) ;
      queue_page_t * last2 = (queue_page_t*)queue.last ;
      TEST(last2 == (void*)last1->next) ;
      TEST(last2->next == (void*)first) ;
      TEST(last2->prev == (void*)last1) ;
      TEST(last2->end_offset   == sizeof(queue_page_t)) ;
      TEST(last2->start_offset == sizeof(queue_page_t)) ;
   }

   // TEST addfirstpage_queue
   queue_page_t * last = (queue_page_t*)queue.last ;
   for (int i = 0; i < 5; ++i) {
      queue_page_t * first1 = next_pagelist(last) ;
      TEST(0 == addfirstpage_queue(&queue)) ;
      TEST(last == (void*)queue.last) ;
      queue_page_t * first2 = next_pagelist(last) ;
      TEST(first2 != first1) ;
      TEST(first2->next == (void*)first1) ;
      TEST(first2->prev == (void*)last) ;
      TEST(first2->end_offset   == pagesizeinbytes_queue()) ;
      TEST(first2->start_offset == pagesizeinbytes_queue()) ;
   }

   // TEST free_queue
   TEST(0 != queue.last) ;
   TEST(0 == free_queue(&queue)) ;
   TEST(0 == queue.last) ;
   TEST(0 == free_queue(&queue)) ;
   TEST(0 == queue.last) ;

   // TEST initmove_queue
   TEST(0 == init_queue(&queue)) ;
   for (int i = 0; i < 5; ++i) {
      TEST(0 == addlastpage_queue(&queue)) ;
   }
   queue_t queue2 = queue_INIT_FREEABLE ;
   last = (queue_page_t*)queue.last ;
   TEST(0 != queue.last) ;
   TEST(0 == queue2.last) ;
   initmove_queue(&queue2, &queue) ;
   TEST(0 == queue.last) ;
   TEST(0 != queue2.last) ;
   TEST(last == (void*)queue2.last)
   TEST(0 == free_queue(&queue)) ;
   TEST(0 == free_queue(&queue2)) ;

   // TEST free_queue: works after direct copy without initmove_queue
   TEST(0 == init_queue(&queue)) ;
   for (int i = 0; i < 5; ++i) {
      TEST(0 == addlastpage_queue(&queue)) ;
   }
   queue2 = queue ;
   TEST(0 == free_queue(&queue2)) ;

   return 0 ;
ONABORT:
   free_queue(&queue) ;
   return EINVAL ;
}

static int test_query(void)
{
   queue_t queue = queue_INIT ;

   // TEST isempty_queue
   queue.last = (dlist_node_t*)1 ;
   TEST(0 == isempty_queue(&queue)) ;
   queue.last = 0 ;
   TEST(1 == isempty_queue(&queue)) ;

   // prepare
   queue = (queue_t) queue_INIT ;
   TEST(0 == addlastpage_queue(&queue)) ;
   queue_page_t * fpage = (queue_page_t*)queue.last ;
   TEST(0 == addlastpage_queue(&queue)) ;
   queue_page_t * lpage = (queue_page_t*)queue.last ;
   TEST(lpage != fpage) ;

   // TEST pagesizeinbytes_queue()
   TEST(4096u == pagesizeinbytes_queue()) ; // current implementation supports only fixed sizes

   // TEST queuefromaddr_queue
   for (unsigned i = 0; i < pagesizeinbytes_queue(); ++i) {
      void * nodeaddr ;
      nodeaddr = (uint8_t*)fpage + i ;
      TEST(&queue == queuefromaddr_queue(nodeaddr)) ;
      fpage->queue = (void*)1 ;
      TEST((void*)1 == queuefromaddr_queue(nodeaddr)) ;
      fpage->queue = &queue ;
      nodeaddr = (uint8_t*)lpage + i ;
      TEST(&queue == queuefromaddr_queue(nodeaddr)) ;
      lpage->queue = (void*)2 ;
      TEST((void*)2 == queuefromaddr_queue(nodeaddr)) ;
      lpage->queue = &queue ;
   }

   // TEST first_queue
   for (uint32_t off = 0; off < 65536; off += 233) {
      uint8_t * first = off + (uint8_t*)fpage ;
      fpage->start_offset = off ;
      fpage->end_offset   = off + 1 + (off&0x1ff) ;
      TEST(0 == first_queue(&queue, (uint16_t)(2+(off&0x1ff)))) ;
      TEST(first == first_queue(&queue, (uint16_t)(1+(off&0x1ff)))) ;
      TEST(first == first_queue(&queue, (uint16_t)(off&0x1ff))) ;
   }

   // TEST last_queue
   for (uint32_t off = 0; off < 65536; off += 233) {
      uint8_t * last = off + (uint8_t*)lpage ;
      lpage->start_offset = off ;
      lpage->end_offset   = off + 1 + (off & 0x1ff) ;
      TEST(0 == last_queue(&queue, (uint16_t)(2+(off & 0x1ff)))) ;
      TEST(last+0 == last_queue(&queue, (uint16_t)(1+(off & 0x1ff)))) ;
      TEST(last+1 == last_queue(&queue, (uint16_t)(off & 0x1ff))) ;
   }

   // TEST sizefirst_queue
   for (uint32_t off = 0; off < 65536; off += 233) {
      fpage->start_offset = off ;
      fpage->end_offset   = off + (off & 0x1ff) ;
      TEST((off & 0x1ff) == sizefirst_queue(&queue)) ;
   }

   // TEST sizelast_queue
   for (uint32_t off = 0; off < 65536; off += 233) {
      lpage->start_offset = off ;
      lpage->end_offset   = off + (off & 0x1ff) ;
      TEST((off & 0x1ff) == sizelast_queue(&queue)) ;
   }

   // unprepare
   TEST(0 == free_queue(&queue)) ;

   return 0 ;
ONABORT:
   free_queue(&queue) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   queue_t           queue     = queue_INIT ;
   queue_page_t *    qpages[5] = { 0 } ;
   queue_iterator_t  iter      = queue_iterator_INIT_FREEABLE ;
   void *            node      = 0 ;

   // TEST queue_iterator_INIT_FREEABLE
   TEST(0 == iter.lastpage) ;
   TEST(0 == iter.nextpage) ;
   TEST(0 == iter.next_offset) ;
   TEST(0 == iter.end_offset) ;
   TEST(0 == iter.nodesize) ;

   // TEST initfirst_queueiterator: empty queue
   TEST(0 == initfirst_queueiterator(&iter, &queue, 3)) ;
   TEST(0 == iter.lastpage) ;
   TEST(0 == iter.nextpage) ;
   TEST(0 == iter.next_offset) ;
   TEST(0 == iter.end_offset) ;
   TEST(1 == iter.nodesize) ;

   // TEST next_queueiterator: empty queue
   TEST(0 == next_queueiterator(&iter, &node)) ;
   TEST(0 == node) ;

   // TEST nextskip_queueiterator: empty queue
   TEST(1 == nextskip_queueiterator(&iter, 0)) ;
   TEST(0 == nextskip_queueiterator(&iter, 1)) ;
   TEST(0 == nextskip_queueiterator(&iter, 65535)) ;
   TEST(0 == iter.lastpage) ;
   TEST(0 == iter.nextpage) ;
   TEST(0 == iter.next_offset) ;
   TEST(0 == iter.end_offset) ;
   TEST(1 == iter.nodesize) ;

   // TEST free_queueiterator: empty queue
   TEST(0 == free_queueiterator(&iter)) ;
   TEST(0 == iter.lastpage) ;
   TEST(0 == iter.nextpage) ;
   TEST(0 == iter.next_offset) ;
   TEST(0 == iter.end_offset) ;
   TEST(0 == iter.nodesize) ;

   // prepare: add list of qpages but no content
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      TEST(0 == addlastpage_queue(&queue)) ;
      qpages[i] = (queue_page_t*)queue.last ;
   }

   // TEST initfirst_queueiterator: list of qpages but no content
   TEST(0 == initfirst_queueiterator(&iter, &queue, 3)) ;
   TEST(iter.lastpage   == qpages[lengthof(qpages)-1]) ;
   TEST(iter.nextpage   == qpages[0]) ;
   TEST(iter.next_offset== sizeof(queue_page_t)) ;
   TEST(iter.end_offset == sizeof(queue_page_t)) ;
   TEST(iter.nodesize   == 3) ;

   // TEST next_queueiterator: list of qpages but no content
   TEST(0 == next_queueiterator(&iter, &node)) ;
   TEST(0 == node) ;
   TEST(iter.lastpage   == qpages[lengthof(qpages)-1]) ;
   TEST(iter.nextpage   == qpages[lengthof(qpages)-1]) ;
   TEST(iter.next_offset== sizeof(queue_page_t)) ;
   TEST(iter.end_offset == sizeof(queue_page_t)) ;
   TEST(iter.nodesize   == 3) ;

   // TEST nextskip_queueiterator: list of qpages but no content
   TEST(0 == free_queueiterator(&iter)) ;
   TEST(0 == initfirst_queueiterator(&iter, &queue, 3)) ;
   TEST(1 == nextskip_queueiterator(&iter, 0)) ;
   TEST(0 == nextskip_queueiterator(&iter, 1)) ;
   TEST(0 == nextskip_queueiterator(&iter, 65535)) ;
   TEST(iter.lastpage   == qpages[lengthof(qpages)-1]) ;
   TEST(iter.nextpage   == qpages[0]) ;
   TEST(iter.next_offset== sizeof(queue_page_t)) ;
   TEST(iter.end_offset == sizeof(queue_page_t)) ;
   TEST(iter.nodesize   == 3) ;
   TEST(0 == next_queueiterator(&iter, &node)) ;
   TEST(1 == nextskip_queueiterator(&iter, 0)) ;
   TEST(0 == nextskip_queueiterator(&iter, 1)) ;
   TEST(0 == nextskip_queueiterator(&iter, 65535)) ;
   TEST(iter.lastpage   == qpages[lengthof(qpages)-1]) ;
   TEST(iter.nextpage   == qpages[lengthof(qpages)-1]) ;
   TEST(iter.next_offset== sizeof(queue_page_t)) ;
   TEST(iter.end_offset == sizeof(queue_page_t)) ;
   TEST(iter.nodesize   == 3) ;

   // TEST free_queueiterator: list of qpages but no content
   TEST(0 == free_queueiterator(&iter)) ;
   TEST(0 == iter.lastpage) ;
   TEST(0 == iter.nextpage) ;
   TEST(0 == iter.next_offset) ;
   TEST(0 == iter.end_offset) ;
   TEST(0 == iter.nodesize) ;

   // prepare: fill qpages
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      qpages[i]->end_offset = pagesizeinbytes_queue() ;
   }

   for (unsigned step = 1; step <= 1024; step *= 2, step += 1) {
      // TEST initfirst_queueiterator: list of qpages with content
      TEST(0 == initfirst_queueiterator(&iter, &queue, (uint16_t)step)) ;
      TEST(iter.lastpage   == qpages[lengthof(qpages)-1]) ;
      TEST(iter.nextpage   == qpages[0]) ;
      TEST(iter.next_offset== sizeof(queue_page_t)) ;
      TEST(iter.end_offset == pagesizeinbytes_queue()) ;
      TEST(iter.nodesize   == step) ;

      // TEST next_queueiterator: list of qpages with content
      for (unsigned i = 0; i < lengthof(qpages); ++i) {
         for (uint32_t offset = sizeof(queue_page_t); (offset + step) <= qpages[i]->end_offset; offset += step) {
            TEST(1 == next_queueiterator(&iter, &node)) ;
            TEST(node == (uint8_t*)qpages[i] + offset) ;
            TEST(iter.lastpage   == qpages[lengthof(qpages)-1]) ;
            TEST(iter.nextpage   == qpages[i]) ;
            TEST(iter.next_offset== (offset + step)) ;
            TEST(iter.end_offset == pagesizeinbytes_queue()) ;
            TEST(iter.nodesize   == step) ;
         }
      }

      // TEST nextskip_queueiterator
      TEST(0 == free_queueiterator(&iter)) ;
      TEST(0 == initfirst_queueiterator(&iter, &queue, 1)) ;
      for (unsigned i = 0; i < lengthof(qpages); ++i) {
         TEST(1 == next_queueiterator(&iter, &node)) ;
         iter.next_offset = sizeof(queue_page_t) ; // reset next step
         for (uint32_t offset = sizeof(queue_page_t), step2 = step; offset < qpages[i]->end_offset; offset += step2) {
            if ((offset + step2) > qpages[i]->end_offset) {
               step2 = qpages[i]->end_offset - offset ;
            }
            TEST(1 == nextskip_queueiterator(&iter, (uint16_t)step2)) ;
            TEST(iter.lastpage   == qpages[lengthof(qpages)-1]) ;
            TEST(iter.nextpage   == qpages[i]) ;
            TEST(iter.next_offset== (offset + step2)) ;
            TEST(iter.end_offset == pagesizeinbytes_queue()) ;
            TEST(iter.nodesize   == 1) ;
         }
      }

      // TEST free_queueiterator: list of qpages with content
      TEST(0 == free_queueiterator(&iter)) ;
      TEST(0 == iter.lastpage) ;
      TEST(0 == iter.nextpage) ;
      TEST(0 == iter.next_offset) ;
      TEST(0 == iter.end_offset) ;
      TEST(0 == iter.nodesize) ;

      // TEST foreach
      unsigned i = 0;
      uint32_t offset = sizeof(queue_page_t);
      foreach(_queue, node2, &queue, (uint16_t)step) {
         if ((offset + step) > qpages[i]->end_offset) {
            offset = sizeof(queue_page_t);
            ++ i ;
         }
         TEST(node2 == (uint8_t*)qpages[i] + offset) ;
         offset += step ;
      }
      TEST(i == lengthof(qpages)-1) ;
      TEST(offset <= qpages[i]->end_offset) ;
      TEST(offset + step > qpages[i]->end_offset) ;
   }

   // unprepare
   TEST(0 == free_queue(&queue)) ;

   return 0 ;
ONABORT:
   free_queue(&queue) ;
   return EINVAL ;
}

static int test_update(void)
{
   queue_t        queue     = queue_INIT ;
   void *         node      = 0 ;
   queue_page_t * qpages[5] = { 0 } ;

   // TEST insertfirst_queue: empty queue
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      TEST(0 == queue.last) ;
      TEST(0 == insertfirst_queue(&queue, &node, nodesize)) ;
      TEST(0 != queue.last) ;
      queue_page_t * last = (void*)queue.last ;
      TEST(last->next  == (void*)last) ;
      TEST(last->prev  == (void*)last) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == pagesizeinbytes_queue()) ;
      TEST(last->start_offset == pagesizeinbytes_queue() - nodesize) ;
      TEST(node == (uint8_t*)last + last->start_offset) ;
      TEST(0 == free_queue(&queue)) ;
   }

   // TEST insertlast_queue: empty queue
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      TEST(0 == queue.last) ;
      TEST(0 == insertlast_queue(&queue, &node, nodesize)) ;
      TEST(0 != queue.last) ;
      queue_page_t * last = (void*)queue.last ;
      TEST(last->next  == (void*)last) ;
      TEST(last->prev  == (void*)last) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == sizeof(queue_page_t) + nodesize) ;
      TEST(last->start_offset == sizeof(queue_page_t)) ;
      TEST(node == (uint8_t*)last + last->start_offset) ;
      TEST(0 == free_queue(&queue)) ;
   }

   // TEST insertfirst_queue: add pages
   TEST(0 == queue.last) ;
   TEST(0 == insertfirst_queue(&queue, &node, 0)) ;
   TEST(0 != queue.last) ;
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t * last  = last_pagelist(genericcast_dlist(&queue)) ;
      queue_page_t * first = first_pagelist(genericcast_dlist(&queue)) ;
      queue_page_t * old   = (void*)first->next ;
      bool           isNew = (first->start_offset - sizeof(queue_page_t)) < nodesize ;
      uint32_t       off   = isNew ? pagesizeinbytes_queue() : first->start_offset ;
      TEST(0 == insertfirst_queue(&queue, &node, nodesize)) ;
      if (isNew) {
         TEST(first == next_pagelist(first_pagelist(genericcast_dlist(&queue)))) ;
         old   = first ;
         first = first_pagelist(genericcast_dlist(&queue)) ;
      }
      TEST(first == first_pagelist(genericcast_dlist(&queue))) ;
      TEST(first->next  == (void*)old) ;
      TEST(first->prev  == (void*)last) ;
      TEST(first->queue == &queue) ;
      TEST(first->end_offset   == pagesizeinbytes_queue()) ;
      TEST(first->start_offset == off - nodesize) ;
   }
   TEST(0 == free_queue(&queue)) ;

   // TEST insertlast_queue: add pages
   TEST(0 == queue.last) ;
   TEST(0 == insertlast_queue(&queue, &node, 0)) ;
   TEST(0 != queue.last) ;
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t * last  = last_pagelist(genericcast_dlist(&queue)) ;
      queue_page_t * first = first_pagelist(genericcast_dlist(&queue)) ;
      queue_page_t * old   = (void*)last->prev ;
      bool           isNew = (pagesizeinbytes_queue() - last->end_offset) < nodesize ;
      uint32_t       off   = isNew ? sizeof(queue_page_t) : last->end_offset ;
      TEST(0 == insertlast_queue(&queue, &node, nodesize)) ;
      if (isNew) {
         TEST(queue.last == last->next) ;
         old  = last ;
         last = last_pagelist(genericcast_dlist(&queue)) ;
      }
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->next  == (void*)first) ;
      TEST(last->prev  == (void*)old) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == off + nodesize) ;
      TEST(last->start_offset == sizeof(queue_page_t)) ;
   }
   TEST(0 == free_queue(&queue)) ;

   // TEST insertfirst_queue: ENOMEM
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == insertfirst_queue(&queue, &node, 1)) ;
   TEST(0 == queue.last) ; // nothing added

   // TEST insertlast_queue: ENOMEM
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == insertlast_queue(&queue, &node, 1)) ;
   TEST(0 == queue.last) ; // nothing added

   // TEST insertfirst_queue: EINVAL
   TEST(EINVAL == insertfirst_queue(&queue, &node, 513)) ;
   TEST(EINVAL == insertfirst_queue(&queue, &node, 65535)) ;

   // TEST insertlast_queue: EINVAL
   TEST(EINVAL == insertlast_queue(&queue, &node, 513)) ;
   TEST(EINVAL == insertlast_queue(&queue, &node, 65535)) ;

   // TEST removefirst_queue: empty queue
   TEST(0 == queue.last) ;
   TEST(ENODATA == removefirst_queue(&queue, 0)) ;

   // TEST removelast_queue: empty queue
   TEST(0 == queue.last) ;
   TEST(ENODATA == removelast_queue(&queue, 0)) ;

   // TEST resizelast_queue: empty queue
   TEST(0 == queue.last) ;
   TEST(ENODATA == resizelast_queue(&queue, &node, 0, 1)) ;

   // TEST removefirst_queue: single page queue
   TEST(0 == queue.last) ;
   TEST(0 == addlastpage_queue(&queue)) ;
   TEST(0 != queue.last) ;
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      last->start_offset  = sizeof(queue_page_t) ;
      last->end_offset    = pagesizeinbytes_queue() ;
      TEST(0 == removefirst_queue(&queue, nodesize)) ;
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == pagesizeinbytes_queue()) ;
      TEST(last->start_offset == sizeof(queue_page_t) + nodesize) ;
   }

   // TEST removelast_queue: single page queue
   TEST(0 != queue.last) ;
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      last->start_offset  = sizeof(queue_page_t) ;
      last->end_offset    = pagesizeinbytes_queue() ;
      TEST(0 == removelast_queue(&queue, nodesize)) ;
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == pagesizeinbytes_queue() - nodesize) ;
      TEST(last->start_offset == sizeof(queue_page_t)) ;
   }

   // TEST resizelast_queue: single page queue
   TEST(0 != queue.last) ;
   for (uint32_t i = sizeof(queue_page_t); i < pagesizeinbytes_queue(); ++i) {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      *(i + (uint8_t*)last) = (uint8_t)i ;
   }
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      last->start_offset  = sizeof(queue_page_t) ;
      last->end_offset    = pagesizeinbytes_queue() ;
      TEST(0 == resizelast_queue(&queue, &node, 512, nodesize)) ;
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == pagesizeinbytes_queue() - 512 + nodesize) ;
      TEST(last->start_offset == sizeof(queue_page_t)) ;
      TEST(node               == (uint8_t*)last + last->end_offset - nodesize) ;
      TEST(0 == resizelast_queue(&queue, &node, nodesize, 512)) ;
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == pagesizeinbytes_queue()) ;
      TEST(last->start_offset == sizeof(queue_page_t)) ;
      TEST(node               == (uint8_t*)last + last->end_offset - 512) ;
   }
   // content has not changed !
   for (uint32_t i = sizeof(queue_page_t); i < pagesizeinbytes_queue(); ++i) {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      TEST((uint8_t)i == *(i + (uint8_t*)last)) ;
   }

   // TEST resizelast_queue: single page queue (shift from end to start)
   TEST(0 != queue.last) ;
   for (uint16_t nodesize = 128; nodesize <= 512; ++nodesize) {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      last->start_offset  = pagesizeinbytes_queue() - 127 ;
      last->end_offset    = pagesizeinbytes_queue() ;
      memset((uint8_t*)last + last->start_offset, (uint8_t)nodesize, 127) ;
      TEST(0 == resizelast_queue(&queue, &node, 127, nodesize)) ;
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->queue        == &queue) ;
      TEST(last->end_offset   == sizeof(queue_page_t) + nodesize) ;
      TEST(last->start_offset == sizeof(queue_page_t)) ;
      TEST(node               == (uint8_t*)last + last->start_offset) ;
      // content preserved
      for (unsigned i = 0; i < 127; ++i) {
         TEST(((uint8_t*)node)[i] == (uint8_t)nodesize) ;
         unsigned e = pagesizeinbytes_queue() -1 -i ;
         TEST(((uint8_t*)last)[e] == (uint8_t)nodesize) ;
      }
   }

   // TEST removefirst_queue: EOVERFLOW
   TEST(0 != queue.last) ;
   {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      last->start_offset  = sizeof(queue_page_t) + 1 ;
      last->end_offset    = sizeof(queue_page_t) + 2 ;
      TEST(EOVERFLOW == removefirst_queue(&queue, 2)) ;
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == sizeof(queue_page_t) + 2) ;
      TEST(last->start_offset == sizeof(queue_page_t) + 1) ;
   }

   // TEST removelast_queue: EOVERFLOW
   TEST(0 != queue.last) ;
   {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      last->start_offset  = sizeof(queue_page_t) + 1 ;
      last->end_offset    = sizeof(queue_page_t) + 3 ;
      TEST(EOVERFLOW == removelast_queue(&queue, 3)) ;
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == sizeof(queue_page_t) + 3) ;
      TEST(last->start_offset == sizeof(queue_page_t) + 1) ;
   }

   // TEST resizelast_queue: EOVERFLOW
   TEST(0 != queue.last) ;
   {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      last->start_offset  = sizeof(queue_page_t) + 1 ;
      last->end_offset    = sizeof(queue_page_t) + 4 ;
      TEST(EOVERFLOW == resizelast_queue(&queue, &node, 4, 3)) ;
      TEST(last == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == sizeof(queue_page_t) + 4) ;
      TEST(last->start_offset == sizeof(queue_page_t) + 1) ;
   }

   // TEST resizelast_queue: EINVAL
   TEST(EINVAL== resizelast_queue(&queue, &node, 1, 513)) ;
   TEST(0 == free_queue(&queue)) ;

   // TEST removefirst_queue: ENOMEM
   TEST(0 == queue.last) ;
   TEST(0 == addlastpage_queue(&queue)) ;
   TEST(0 != queue.last) ;
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == removefirst_queue(&queue, 0)) ;
   TEST(0 == queue.last) ; // nevertheless freed

   // TEST removelast_queue: ENOMEM
   TEST(0 == queue.last) ;
   TEST(0 == addlastpage_queue(&queue)) ;
   TEST(0 != queue.last) ;
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == removelast_queue(&queue, 0)) ;
   TEST(0 == queue.last) ; // nevertheless freed

   // TEST removefirst_queue: multiple pages
   TEST(0 == queue.last) ;
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      TEST(0 == addlastpage_queue(&queue)) ;
      qpages[i] = (queue_page_t*)queue.last ;
      qpages[i]->end_offset = pagesizeinbytes_queue() ;
   }
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      uint32_t pagesize = pagesizeinbytes_queue() ;
      for (uint32_t off = sizeof(queue_page_t); off < pagesize; ++off) {
         TEST(qpages[i] == first_pagelist(genericcast_dlist(&queue))) ;
         TEST(qpages[i]->queue        == &queue) ;
         TEST(qpages[i]->end_offset   == pagesizeinbytes_queue()) ;
         TEST(qpages[i]->start_offset == off) ;
         TEST(0 == removefirst_queue(&queue, 1)) ;
      }
   }
   TEST(0 == queue.last) ;

   // TEST removelast_queue: multiple pages
   TEST(0 == queue.last) ;
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      TEST(0 == addlastpage_queue(&queue)) ;
      qpages[i] = (queue_page_t*)queue.last ;
      qpages[i]->end_offset = pagesizeinbytes_queue() ;
   }
   for (int i = lengthof(qpages)-1; i >= 0; --i) {
      uint32_t pagesize = pagesizeinbytes_queue() ;
      for (uint32_t off = pagesize; off > sizeof(queue_page_t); --off) {
         TEST(qpages[i] == last_pagelist(genericcast_dlist(&queue))) ;
         TEST(qpages[i]->queue        == &queue) ;
         TEST(qpages[i]->end_offset   == off) ;
         TEST(qpages[i]->start_offset == sizeof(queue_page_t)) ;
         TEST(0 == removelast_queue(&queue, 1)) ;
      }
   }
   TEST(0 == queue.last) ;

   // TEST resizelast_queue: add new page
   TEST(0 == queue.last) ;
   TEST(0 == addlastpage_queue(&queue)) ;
   // fill page
   for (uint32_t i = sizeof(queue_page_t); i < pagesizeinbytes_queue(); ++i) {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      *(i + (uint8_t*)last) = (uint8_t)i ;
   }
   for (uint16_t nodesize = 0; nodesize <= 256; ++nodesize) {
      queue_page_t * first = first_pagelist(genericcast_dlist(&queue)) ;
      TEST(first == last_pagelist(genericcast_dlist(&queue))) ;
      first->start_offset  = sizeof(queue_page_t) ;
      first->end_offset    = pagesizeinbytes_queue() ;
      memset((uint8_t*)end_queuepage(first) - nodesize, (uint8_t)nodesize, nodesize) ;
      TEST(0 == resizelast_queue(&queue, &node, nodesize, (uint16_t)(nodesize+256))) ;
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      TEST(last  != first) ;
      TEST(first == first_pagelist(genericcast_dlist(&queue))) ;
      TEST(first->queue == &queue) ;
      TEST(first->end_offset   == pagesizeinbytes_queue()-nodesize) ;
      TEST(first->start_offset == sizeof(queue_page_t)) ;
      TEST(last->queue == &queue) ;
      TEST(last->end_offset   == sizeof(queue_page_t)+nodesize+256) ;
      TEST(last->start_offset == sizeof(queue_page_t)) ;
      TEST(node               == (uint8_t*)last + last->start_offset) ;
      // content is copied to new page
      for (uint32_t i = sizeof(queue_page_t); i < sizeof(queue_page_t) + nodesize; ++i) {
         TEST(*(i + (uint8_t*)last) == (uint8_t)(nodesize)) ;
      }
      TEST(0 == removelastpage_queue(&queue)) ;
   }
   // content of old page has not changed
   for (uint32_t i = sizeof(queue_page_t); i < pagesizeinbytes_queue(); ++i) {
      queue_page_t * last = last_pagelist(genericcast_dlist(&queue)) ;
      if (i >= pagesizeinbytes_queue()-256) {
         TEST((uint8_t)0 == *(i + (uint8_t*)last)) ;
      } else {
         TEST((uint8_t)i == *(i + (uint8_t*)last)) ;
      }
   }

   // TEST resizelast_queue: ENOMEM
   {
      queue_page_t * first = first_pagelist(genericcast_dlist(&queue)) ;
      queue_page_t * last  = last_pagelist(genericcast_dlist(&queue)) ;
      TEST(0 != last) ;
      TEST(first == last) ;
      first->start_offset  = sizeof(queue_page_t) ;
      first->end_offset    = pagesizeinbytes_queue() ;
      init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM) ;
      TEST(ENOMEM == resizelast_queue(&queue, &node, 1, 127)) ;
         // nothing done
      TEST(first == first_pagelist(genericcast_dlist(&queue))) ;
      TEST(last  == last_pagelist(genericcast_dlist(&queue))) ;
      TEST(first->start_offset  == sizeof(queue_page_t))
      TEST(first->end_offset    == pagesizeinbytes_queue()) ;
   }

   // unprepare
   TEST(0 == free_queue(&queue)) ;

   return 0 ;
ONABORT:
   free_queue(&queue) ;
   return EINVAL ;
}

struct test_node_t {
   int x ;
   int y ;
   char str[16] ;
} ;

// TEST queue_IMPLEMENT
queue_IMPLEMENT(_testqueue, struct test_node_t)

typedef struct test_node_t    test_node_t ;

static int test_generic(void)
{
   queue_t        queue = queue_INIT ;
   struct {
      dlist_node_t * last ;
   }              queue2 ;
   test_node_t *  node = 0 ;

   // TEST genericcast_queue
   TEST(genericcast_queue(&queue)  == &queue) ;
   TEST(genericcast_queue(&queue2) == (queue_t*)&queue2) ;

   // TEST init_queue
   memset(&queue, 255, sizeof(queue)) ;
   TEST(0 == init_testqueue(&queue)) ;
   TEST(0 == queue.last) ;

   // TEST initmove_queue
   for (int i = 0; i < 5; ++i) {
      TEST(0 == addlastpage_queue(&queue)) ;
   }
   TEST(0 != queue.last) ;
   queue_page_t * last = (queue_page_t*) queue.last ;
   queue2.last = 0 ;
   initmove_testqueue(genericcast_queue(&queue2), &queue) ;
   TEST(queue2.last == (void*)last) ;
   TEST(queue.last  == 0) ;
   initmove_testqueue(&queue, genericcast_queue(&queue2)) ;
   TEST(queue2.last == 0) ;
   TEST(queue.last  == (void*)last) ;

   // TEST free_queue
   TEST(queue.last != 0) ;
   TEST(0 == free_testqueue(&queue)) ;
   TEST(queue.last == 0) ;

   // TEST isempty_queue
   queue.last = (void*)1 ;
   TEST(0 == isempty_testqueue(&queue)) ;
   queue.last = 0 ;
   TEST(1 == isempty_testqueue(&queue)) ;

   // TEST first_queue, last_queue, sizefirst_queue, sizelast_queue
   TEST(queue.last == 0) ;
   TEST(0 == addlastpage_queue(&queue)) ;
   queue_page_t * first = (queue_page_t *)queue.last ;
   first->end_offset += sizeof(test_node_t) ;
   TEST(0 == addlastpage_queue(&queue)) ;
   last  = (queue_page_t *)queue.last ;
   last->end_offset += 2*sizeof(test_node_t) ;
   TEST(first_testqueue(&queue)     == (test_node_t*)((uint8_t*)first + first->start_offset)) ;
   TEST(last_testqueue(&queue)      == (test_node_t*)((uint8_t*)last  + last->end_offset-sizeof(test_node_t))) ;
   TEST(sizefirst_testqueue(&queue) == sizeof(test_node_t)) ;
   TEST(sizelast_testqueue(&queue)  == 2*sizeof(test_node_t)) ;

   // TEST insertfirst_queue
   TEST(0 == free_testqueue(&queue)) ;
   TEST(queue.last == 0) ;
   TEST(0 == insertfirst_testqueue(&queue, &node)) ;
   TEST(queue.last != 0) ;
   last = (queue_page_t *)queue.last ;
   TEST(last->end_offset   == pagesizeinbytes_queue()) ;
   TEST(last->start_offset == pagesizeinbytes_queue() - sizeof(test_node_t)) ;
   TEST(node               == (test_node_t*)((uint8_t*)last + last->start_offset)) ;

   // TEST insertlast_queue
   TEST(0 == free_testqueue(&queue)) ;
   TEST(queue.last == 0) ;
   TEST(0 == insertlast_testqueue(&queue, &node)) ;
   TEST(queue.last != 0) ;
   last = (queue_page_t *)queue.last ;
   TEST(last->end_offset   == sizeof(queue_page_t) + sizeof(test_node_t)) ;
   TEST(last->start_offset == sizeof(queue_page_t)) ;
   TEST(node               == (test_node_t*)((uint8_t*)last + last->start_offset)) ;

   // TEST removefirst_queue
   TEST(0 == free_testqueue(&queue)) ;
   TEST(queue.last == 0) ;
   TEST(0 == insertfirst_testqueue(&queue, &node)) ;
   TEST(0 == insertfirst_testqueue(&queue, &node)) ;
   TEST(queue.last != 0) ;
   last = (queue_page_t *)queue.last ;
   TEST(last->end_offset   == pagesizeinbytes_queue()) ;
   TEST(last->start_offset == pagesizeinbytes_queue() - 2*sizeof(test_node_t)) ;
   TEST(node               == (test_node_t*)((uint8_t*)last + last->start_offset)) ;
   TEST(0 == removefirst_testqueue(&queue)) ;
   TEST(last->end_offset   == pagesizeinbytes_queue()) ;
   TEST(last->start_offset == pagesizeinbytes_queue() - 1*sizeof(test_node_t)) ;

   // TEST removelast_queue
   TEST(0 == free_testqueue(&queue)) ;
   TEST(queue.last == 0) ;
   TEST(0 == insertfirst_testqueue(&queue, &node)) ;
   TEST(0 == insertfirst_testqueue(&queue, &node)) ;
   TEST(queue.last != 0) ;
   last = (queue_page_t *)queue.last ;
   TEST(last->end_offset   == pagesizeinbytes_queue()) ;
   TEST(last->start_offset == pagesizeinbytes_queue() - 2*sizeof(test_node_t)) ;
   TEST(node               == (test_node_t*)((uint8_t*)last + last->start_offset)) ;
   TEST(0 == removelast_testqueue(&queue)) ;
   TEST(last->end_offset   == pagesizeinbytes_queue() - 1*sizeof(test_node_t)) ;
   TEST(last->start_offset == pagesizeinbytes_queue() - 2*sizeof(test_node_t)) ;

   // foreach
   TEST(0 == free_testqueue(&queue)) ;
   for (int x = 1; x <= 5; ++x) {
      TEST(0 == insertlast_testqueue(&queue, &node)) ;
      node->x = x ;
   }
   for (int x = 1; x <= 5; ++x) {
      TEST(0 == insertfirst_testqueue(&queue, &node)) ;
      node->x = -x ;
   }
   for (int x = -5; x == -5; ) {
      node = 0 ;
      foreach (_testqueue, node2, &queue) {
         TEST(node2->x == x) ;
         TEST(!node || node2 == node+1) ;
         node = node2 ;
         ++ x ;
         if (0 == x) {
            node = 0 ;
            x = 1 ;
         }
      }
      TEST(x == 6) ;
   }

   // unprepare
   TEST(0 == free_queue(&queue)) ;

   return 0 ;
ONABORT:
   free_queue(&queue) ;
   return EINVAL ;
}

int unittest_ds_inmem_queue()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_queuepage())      goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_iterator())       goto ONABORT ;
   if (test_update())         goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
