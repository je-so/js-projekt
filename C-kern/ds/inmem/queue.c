/* title: Queue impl

   Implements <Queue>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/ds/foreach.h"
#endif


// section: queue_page_t

// group: static variables

#ifdef KONFIG_UNITTEST
static test_errortimer_t      s_queuepage_errtimer = test_errortimer_FREE;
#endif

// group: helper

static inline void compiletime_assert(void)
{
   static_assert(offsetof(queue_page_t, next) == 0, "next pointer could be cast into (queue_page_t*)");
}

// group: lifetime

/* function: new_queuepage
 * Allocates single memory page with <ALLOC_PAGECACHE> with size <pagesize_4096>. */
static int new_queuepage(/*out*/queue_page_t** qpage, queue_t* queue)
{
   int err;
   memblock_t page;

   if (! PROCESS_testerrortimer(&s_queuepage_errtimer, &err)) {
      err = ALLOC_PAGECACHE(queue->pagesize, &page);
   }
   if (err) goto ONERR;

   queue_page_t* new_qpage = (queue_page_t*) page.addr;

   static_assert(0 == (sizeof(queue_page_t) % KONFIG_MEMALIGN), "first byte is correctly aligned");
   // new_qpage->next      =; // is set in addlastpage_queue, addfirstpage_queue
   // new_qpage->prev      =; // is set in addlastpage_queue, addfirstpage_queue
   new_qpage->queue        = queue;
   // new_qpage->end_offset   =; // is set in addlastpage_queue, addfirstpage_queue
   // new_qpage->start_offset =; // is set in addlastpage_queue, addfirstpage_queue

   *qpage = new_qpage;

   return 0;
ONERR:
   return err;
}

/* function: delete_queuepage
 * Frees single memory page with <RELEASE_PAGECACHE>. */
static int delete_queuepage(queue_page_t** qpage, uint32_t pagesize)
{
   int err;
   queue_page_t* del_qpage = *qpage;

   if (del_qpage) {
      *qpage = 0;

      memblock_t page = memblock_INIT(pagesize, (uint8_t*)del_qpage);

      err = RELEASE_PAGECACHE(&page);
      (void) PROCESS_testerrortimer(&s_queuepage_errtimer, &err);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

// group: query

/* function: end_queuepage
 * Returns address after the last element on page. */
static void* end_queuepage(queue_page_t* qpage)
{
   return (uint8_t*)qpage + qpage->end_offset;
}

/* function: end_queuepage
 * Returns address after the last element on page. */
static inline bool isempty_queuepage(const queue_page_t* qpage)
{
   return qpage->end_offset == qpage->start_offset;
}

// group: update

static inline int pushfirst_queuepage(queue_page_t* qpage, /*out*/void** nodeaddr, uint16_t nodesize)
{
   const int32_t new_start = qpage->start_offset - (int32_t)nodesize;

   if ((int32_t)sizeof(queue_page_t) > new_start) return ENOMEM;

   qpage->start_offset = (uint16_t) new_start;

   *nodeaddr = (uint8_t*)qpage + qpage->start_offset;

   return 0;
}

static inline int pushlast_queuepage(queue_page_t* qpage, uint32_t pagesize, /*out*/void  ** nodeaddr, uint16_t nodesize)
{
   const uint32_t new_end = qpage->end_offset + (uint32_t)nodesize;

   if (pagesize < new_end) return ENOMEM;

   *nodeaddr = (uint8_t*)qpage + qpage->end_offset;

   qpage->end_offset = (uint16_t) new_end;

   return 0;
}

static inline int removefirst_queuepage(queue_page_t* qpage, uint16_t nodesize)
{
   const uint32_t new_start = qpage->start_offset + (uint32_t)nodesize;

   if (qpage->end_offset < new_start) return EOVERFLOW;

   qpage->start_offset = (uint16_t) new_start;

   return 0;
}

static inline int removelast_queuepage(queue_page_t* qpage, uint16_t nodesize)
{
   const int32_t new_end = qpage->end_offset - (int32_t)nodesize;

   if (qpage->start_offset > new_end) return EOVERFLOW;

   qpage->end_offset = (uint16_t) new_end;

   return 0;
}



// section: queue_t

// group: helper

dlist_IMPLEMENT(_pagelist, queue_page_t, )

static inline int addlastpage_queue(queue_t* queue)
{
   int err;
   queue_page_t* qpage;

   err = new_queuepage(&qpage, queue);
   if (err) return err;

   qpage->end_offset   = sizeof(queue_page_t);
   qpage->start_offset = sizeof(queue_page_t);
   insertlast_pagelist(cast_dlist(queue), qpage);

   return 0;
}

static inline int addfirstpage_queue(queue_t* queue)
{
   int err;
   queue_page_t* qpage;

   err = new_queuepage(&qpage, queue);
   if (err) return err;

   qpage->end_offset   = pagesize_queue(queue);
   qpage->start_offset = pagesize_queue(queue);
   insertfirst_pagelist(cast_dlist(queue), qpage);

   return 0;
}

static inline int removefirstpage_queue(queue_t* queue)
{
   int err;
   queue_page_t* first = removefirst_pagelist(cast_dlist(queue));

   err = delete_queuepage(&first, pagesize_queue(queue));
   if (err) return err;

   return 0;
}

static inline int removelastpage_queue(queue_t* queue)
{
   int err;
   queue_page_t* last;

   removelast_pagelist(cast_dlist(queue), &last);

   err = delete_queuepage(&last, pagesize_queue(queue));
   if (err) return err;

   return 0;
}

static inline bool issupported(size_t pagesize_in_bytes)
{
   return   256 <= pagesize_in_bytes && pagesize_in_bytes <= 16384
            && ispowerof2_int(pagesize_in_bytes);
}

// group: lifetime

int init_queue(/*out*/queue_t* queue, size_t pagesize)
{
   if (!issupported(pagesize)) goto ONERR;

   queue->last = 0;
   queue->pagesize = pagesizefrombytes_pagecache(pagesize);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(EINVAL);
   return EINVAL;
}

void initmove_queue(/*out*/queue_t* dest, queue_t* src)
{
   dest->last = src->last;
   dest->pagesize = src->pagesize;
   src->last = 0;

   queue_page_t* last = last_pagelist(cast_dlist(dest));
   if (last) {
      queue_page_t* qpage = last;
      do {
         qpage->queue = dest;
         qpage = next_pagelist(qpage);
      } while (qpage != last);
   }
}

int free_queue(queue_t* queue)
{
   int err;

   err = removeall_queue(queue);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

size_t sizebytes_queue(const queue_t * queue)
{
   size_t size = 0;

   if (queue->last) {
      queue_page_t* last = last_pagelist(castconst_dlist(queue));
      queue_page_t* next = last;
      do {
         size += next->end_offset;
         size -= next->start_offset;
         next = next_pagelist(next);
      } while (next != last);
   }

   return size;
}

// group: update

int insertfirst_queue(queue_t * queue, uint16_t nodesize, /*out*/void ** nodeaddr)
{
   int err;

   queue_page_t* first = first_pagelist(cast_dlist(queue));

   if (!first || 0 != pushfirst_queuepage(first, nodeaddr, nodesize)) {
      err = addfirstpage_queue(queue);
      if (err) goto ONERR;

      first = first_pagelist(cast_dlist(queue));

      err = pushfirst_queuepage(first, nodeaddr, nodesize);
      assert(!err);
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int insertlast_queue(queue_t * queue, uint16_t nodesize, /*out*/void ** nodeaddr)
{
   int err;

   queue_page_t* last = last_pagelist(cast_dlist(queue));
   uint16_t  pagesize = pagesize_queue(queue);

   if (!last || 0 != pushlast_queuepage(last, pagesize, nodeaddr, nodesize)) {
      err = addlastpage_queue(queue);
      if (err) goto ONERR;

      last = last_pagelist(cast_dlist(queue));

      err = pushlast_queuepage(last, pagesize, nodeaddr, nodesize);
      assert(!err);
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int removefirst_queue(queue_t* queue, uint16_t nodesize)
{
   int err;
   queue_page_t* first = first_pagelist(cast_dlist(queue));

   if (!first) {
      err = ENODATA;
      goto ONERR;
   }

   err = removefirst_queuepage(first, nodesize);
   if (err) goto ONERR;

   if (isempty_queuepage(first)) {
      err = removefirstpage_queue(queue);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int removelast_queue(queue_t* queue, uint16_t nodesize)
{
   int err;
   queue_page_t* last = last_pagelist(cast_dlist(queue));

   if (!last) {
      err = ENODATA;
      goto ONERR;
   }

   err = removelast_queuepage(last, nodesize);
   if (err) goto ONERR;

   if (isempty_queuepage(last)) {
      err = removelastpage_queue(queue);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int removeall_queue(queue_t* queue)
{
   int err;

   if (queue->last) {
      uint16_t const pagesize = pagesize_queue(queue);
      queue_page_t* qpage = last_pagelist(cast_dlist(queue));
      qpage = next_pagelist(qpage);
      queue->last->next = 0;
      queue->last = 0;
      err = 0;
      do {
         queue_page_t* delpage = qpage;
         qpage = next_pagelist(qpage);
         int err2 = delete_queuepage(&delpage, pagesize);
         if (err2) err = err2;
      } while (qpage);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int resizelast_queue(queue_t* queue, /*out*/void** nodeaddr, uint16_t oldsize, uint16_t newsize)
{
   int err;
   queue_page_t* last = last_pagelist(cast_dlist(queue));
   uint16_t pagesize = pagesize_queue(queue);

   VALIDATE_INPARAM_TEST(newsize <= 512, ONERR, );

   if (!last) {
      err = ENODATA;
      goto ONERR;
   }

   err = removelast_queuepage(last, oldsize);
   if (err) goto ONERR;

   if (0 != pushlast_queuepage(last, pagesize, nodeaddr, newsize)) {
      void* oldcontent = end_queuepage(last);
      if (isempty_queuepage(last)) {
         // shifts content to start of page
         last->end_offset   = sizeof(queue_page_t);
         last->start_offset = sizeof(queue_page_t);
      } else {
         err = addlastpage_queue(queue);
         if (err) {
            void* dummy;
            (void) pushlast_queuepage(last, pagesize, &dummy, oldsize);
            goto ONERR;
         }
      }
      last = last_pagelist(cast_dlist(queue));
      err = pushlast_queuepage(last, pagesize, nodeaddr, newsize);
      assert(!err);
      memmove(*nodeaddr, oldcontent, oldsize/* oldsize < newsize !*/);
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_queuepage(void)
{
   queue_t       queue = queue_INIT;
   queue_page_t* qpage = 0;
   void*         nodeaddr;

   TEST(! issupported(128));
   TEST(! issupported(32768));
   for (size_t ps = 256; ps <= 16384; ps *= 2) {

      // TEST pagesize_queue
      TEST(issupported(ps));
      queue.pagesize = pagesizefrombytes_pagecache(ps);
      TEST(ps == pagesize_queue(&queue));

      // TEST maxelemsize_queue
      TEST(ps == maxelemsize_queue((uint16_t)ps) + sizeof(queue_page_t));

      // TEST new_queuepage
      size_t oldsize = SIZEALLOCATED_PAGECACHE();
      TEST(0 == new_queuepage(&qpage, &queue));
      TEST(0 != qpage);
      TEST(qpage->queue == &queue);
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + (unsigned) pagesize_queue(&queue));

      // TEST delete_queuepage
      TEST(0 == delete_queuepage(&qpage, pagesize_queue(&queue)));
      TEST(0 == qpage);
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize);
      TEST(0 == delete_queuepage(&qpage, pagesize_queue(&queue)));
      TEST(0 == qpage);
   }

   // prepare
   queue = (queue_t) queue_INIT;

   // TEST new_queuepage: ENOMEM
   TEST(0 == new_queuepage(&qpage, &queue));
   queue_page_t* const old = qpage;
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM);
   TEST(ENOMEM == new_queuepage(&qpage, &queue));
   TEST(old == qpage);

   // TEST delete_queuepage: ENOMEM
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM);
   TEST(qpage  != 0);
   TEST(ENOMEM == delete_queuepage(&qpage, pagesize_queue(&queue)));
   TEST(qpage  == 0);

   // TEST end_queuepage
   TEST(0 == new_queuepage(&qpage, &queue));
   qpage->end_offset = 0;
   TEST(qpage == end_queuepage(qpage));
   for (uint16_t i = 1; i; i = (uint16_t) (i << 1)) {
      qpage->end_offset = i;
      TEST(i + (uint8_t*)qpage == end_queuepage(qpage));
   }
   TEST(0 == delete_queuepage(&qpage, pagesize_queue(&queue)));

   // prepare
   TEST(0 == new_queuepage(&qpage, &queue));

   // TEST isempty_queuepage
   for (uint16_t i = 0; i < 100; ++i) {
      qpage->end_offset = i;
      qpage->start_offset = i;
      TEST(1 == isempty_queuepage(qpage));
      qpage->end_offset = (uint16_t) (i + 1 + i);
      qpage->start_offset = i;
      TEST(0 == isempty_queuepage(qpage));
      qpage->end_offset = i;
      qpage->start_offset = (uint16_t) (i + 1 + i);
      TEST(0 == isempty_queuepage(qpage));
   }

   // TEST pushfirst_queuepage
   qpage->end_offset = defaultpagesize_queue();
   for (unsigned size = 0; size < defaultpagesize_queue(); ++size) {
      for (unsigned i = 0; i < defaultpagesize_queue(); ++i) {
         qpage->start_offset = (uint16_t) (defaultpagesize_queue() - size);
         nodeaddr = 0;
         if (qpage->start_offset < sizeof(queue_page_t) + i) {
            TEST(ENOMEM == pushfirst_queuepage(qpage, &nodeaddr, (uint16_t) i));
            if (  qpage->start_offset + 10u < sizeof(queue_page_t) + i
                  && i < defaultpagesize_queue()-10)
               i = defaultpagesize_queue()-10;
            TEST((uint16_t) (defaultpagesize_queue() - size) == qpage->start_offset);
            TEST(0 == nodeaddr);
         } else {
            TEST(0 == pushfirst_queuepage(qpage, &nodeaddr, (uint16_t) i));
            TEST((uint16_t) (defaultpagesize_queue() - size -i) == qpage->start_offset);
            TEST((uint8_t*)qpage + qpage->start_offset == (uint8_t*)nodeaddr);
         }
         TEST(defaultpagesize_queue() == qpage->end_offset);
      }
   }

   // TEST pushfirst_queuepage: maxelemsize_queue
   qpage->end_offset = defaultpagesize_queue();
   qpage->start_offset = defaultpagesize_queue();
   TEST(0 == pushfirst_queuepage(qpage, &nodeaddr, maxelemsize_queue(defaultpagesize_queue())));
   TEST(defaultpagesize_queue() == qpage->end_offset);
   TEST(sizeof(queue_page_t) == qpage->start_offset);
   TEST((uint8_t*)qpage + qpage->start_offset == (uint8_t*)nodeaddr);
   TEST(ENOMEM == pushfirst_queuepage(qpage, &nodeaddr, 1));

   // TEST pushlast_queuepage
   qpage->start_offset = 0;
   for (unsigned size = 0; size < defaultpagesize_queue(); ++size) {
      for (unsigned i = 0; i < defaultpagesize_queue(); ++i) {
         qpage->end_offset = (uint16_t) (defaultpagesize_queue() - size);
         nodeaddr = 0;
         if (qpage->end_offset + i > defaultpagesize_queue()) {
            TEST(ENOMEM == pushlast_queuepage(qpage, defaultpagesize_queue(), &nodeaddr, (uint16_t) i));
            if (  qpage->end_offset + i > defaultpagesize_queue() + 10
                  && i < defaultpagesize_queue()-10)
               i = defaultpagesize_queue()-10;
            TEST((uint16_t) (defaultpagesize_queue() - size) == qpage->end_offset);
            TEST(0 == nodeaddr);
         } else {
            TEST(0 == pushlast_queuepage(qpage, defaultpagesize_queue(), &nodeaddr, (uint16_t) i));
            TEST((uint16_t) (defaultpagesize_queue() - size +i) == qpage->end_offset);
            TEST((uint8_t*)qpage -i + qpage->end_offset == (uint8_t*)nodeaddr);
         }
         TEST(0 == qpage->start_offset);
      }
   }

   // TEST pushlast_queuepage: maxelemsize_queue
   qpage->end_offset = sizeof(queue_page_t);
   qpage->start_offset = sizeof(queue_page_t);
   TEST(0 == pushlast_queuepage(qpage, defaultpagesize_queue(), &nodeaddr, maxelemsize_queue(defaultpagesize_queue())));
   TEST(sizeof(queue_page_t) == qpage->start_offset);
   TEST(defaultpagesize_queue() == qpage->end_offset);
   TEST((uint8_t*)qpage + qpage->start_offset == (uint8_t*)nodeaddr);
   TEST(ENOMEM == pushlast_queuepage(qpage, defaultpagesize_queue(), &nodeaddr, 1));

   // TEST removefirst_queuepage
   qpage->end_offset = defaultpagesize_queue();
   for (unsigned size = 0; size < defaultpagesize_queue(); ++size) {
      for (unsigned i = 0; i < defaultpagesize_queue(); ++i) {
         qpage->start_offset = (uint16_t) (defaultpagesize_queue() - size);
         nodeaddr = 0;
         if (qpage->start_offset + i > qpage->end_offset) {
            TEST(EOVERFLOW == removefirst_queuepage(qpage, (uint16_t) i));
            if (  qpage->start_offset + i < qpage->end_offset + 10u
                  && i < defaultpagesize_queue()-10)
               i = defaultpagesize_queue()-10;
            TEST((uint16_t) (defaultpagesize_queue() - size) == qpage->start_offset);
         } else {
            TEST(0 == removefirst_queuepage(qpage, (uint16_t) i));
            TEST((uint16_t) (defaultpagesize_queue() - size +i) == qpage->start_offset);
         }
         TEST(defaultpagesize_queue() == qpage->end_offset);
      }
   }

   // TEST removelast_queuepage
   qpage->start_offset = 0;
   for (unsigned size = 0; size < defaultpagesize_queue(); ++size) {
      for (unsigned i = 0; i < defaultpagesize_queue(); ++i) {
         qpage->end_offset = (uint16_t) (defaultpagesize_queue() - size);
         nodeaddr = 0;
         if (qpage->end_offset < i) {
            TEST(EOVERFLOW == removelast_queuepage(qpage, (uint16_t) i));
            if (  qpage->end_offset + 10u < i
                  && i < defaultpagesize_queue()-10)
               i = defaultpagesize_queue()-10;
            TEST((uint16_t) (defaultpagesize_queue() - size) == qpage->end_offset);
         } else {
            TEST(0 == removelast_queuepage(qpage, (uint16_t) i));
            TEST((uint16_t) (defaultpagesize_queue() - size -i) == qpage->end_offset);
         }
         TEST(0 == qpage->start_offset);
      }
   }

   // unprepare
   TEST(0 == delete_queuepage(&qpage, pagesize_queue(&queue)));

   return 0;
ONERR:
   delete_queuepage(&qpage, pagesize_queue(&queue));
   return EINVAL;
}

static int test_initfree(void)
{
   queue_t       queue = queue_FREE;
   queue_page_t* last;

   // TEST queue_FREE
   TEST(0 == queue.last);
   TEST(0 == queue.pagesize);

   // TEST queue_INIT
   queue = (queue_t) queue_INIT;
   TEST(0 == queue.last);
   TEST(pagesize_4096 == queue.pagesize);
   TEST(defaultpagesize_queue() == pagesize_queue(&queue));

   // TEST issupported
   for (uint32_t pagesize = 0; pagesize <= 65536; ++pagesize) {
      bool expect =  256 == pagesize || 512 == pagesize || 1024 == pagesize  || 2048 == pagesize
                     || 4096 == pagesize || 8192 == pagesize || 16384 == pagesize;
      TEST(expect == issupported(pagesize));
   }

   for (uint32_t pagesize = 256, p = 0; pagesize <= 16384; pagesize *= 2, ++p) {

      // TEST init_queue
      memset(&queue, 255, sizeof(queue));
      TEST(0 == init_queue(&queue, pagesize));
      TEST(0 == queue.last);
      TEST(p == queue.pagesize);

      // TEST addlastpage_queue
      TEST(0 == queue.last);
      TEST(0 == addlastpage_queue(&queue));
      TEST(0 != queue.last);
      queue_page_t* first = (queue_page_t*) queue.last;
      TEST(queue.last == first->next);
      for (int i = 0; i < 5; ++i) {
         queue_page_t* last1 = (queue_page_t*) queue.last;
         TEST(0 == addlastpage_queue(&queue));
         queue_page_t* last2 = (queue_page_t*) queue.last;
         TEST(last2 == (void*)last1->next);
         TEST(last2->next == (void*)first);
         TEST(last2->prev == (void*)last1);
         TEST(last2->end_offset   == sizeof(queue_page_t));
         TEST(last2->start_offset == sizeof(queue_page_t));
      }

      // TEST addfirstpage_queue
      last = (queue_page_t*)queue.last;
      for (int i = 0; i < 5; ++i) {
         queue_page_t* first1 = next_pagelist(last);
         TEST(0 == addfirstpage_queue(&queue));
         TEST(last == (void*)queue.last);
         queue_page_t* first2 = next_pagelist(last);
         TEST(first2 != first1);
         TEST(first2->next == (void*)first1);
         TEST(first2->prev == (void*)last);
         TEST(first2->end_offset   == pagesize);
         TEST(first2->start_offset == pagesize);
      }

      // TEST free_queue
      TEST(0 != queue.last);
      TEST(0 == free_queue(&queue));
      TEST(0 == queue.last);
      TEST(0 == free_queue(&queue));
      TEST(0 == queue.last);
   }

   // TEST initmove_queue
   queue = (queue_t) queue_INIT;
   for (int i = 0; i < 5; ++i) {
      TEST(0 == addlastpage_queue(&queue));
   }
   queue_t queue2 = queue_FREE;
   last = (queue_page_t*)queue.last;
   TEST(0 != queue.last);
   TEST(0 == queue2.last);
   initmove_queue(&queue2, &queue);
   TEST(0 == queue.last);
   TEST(0 != queue2.last);
   TEST(last == (void*)queue2.last)
   TEST(0 == free_queue(&queue));
   TEST(0 == free_queue(&queue2));

   // TEST free_queue: works after direct copy without initmove_queue
   TEST(0 == init_queue(&queue, defaultpagesize_queue()));
   for (int i = 0; i < 5; ++i) {
      TEST(0 == addlastpage_queue(&queue));
   }
   queue2 = queue;
   TEST(0 == free_queue(&queue2));

   return 0;
ONERR:
   free_queue(&queue);
   return EINVAL;
}

static int test_query(void)
{
   queue_t queue = queue_INIT;

   // TEST defaultpagesize_queue()
   TEST(4096u == defaultpagesize_queue());

   // TEST isempty_queue
   queue.last = (dlist_node_t*)1;
   TEST(0 == isempty_queue(&queue));
   queue.last = 0;
   TEST(1 == isempty_queue(&queue));

   // TEST isfree_queue
   queue.last = (dlist_node_t*)1;
   TEST(0 == isfree_queue(&queue));
   queue.last = 0;
   TEST(1 == isfree_queue(&queue));

   for (uint32_t pagesize = 256, p = 0; pagesize <= 16384; pagesize *= 4, ++p) {

      // prepare
      TEST(0 == init_queue(&queue, pagesize));
      TEST(pagesize == pagesize_queue(&queue));
      TEST(0 == addlastpage_queue(&queue));
      queue_page_t* fpage = (queue_page_t*) queue.last;
      TEST(0 == addlastpage_queue(&queue));
      queue_page_t* lpage = (queue_page_t*) queue.last;
      TEST(lpage != fpage);

      // TEST castPaddr_queue
      for (unsigned i = 0; i < pagesize_queue(&queue); ++i) {
         void* nodeaddr;
         nodeaddr = (uint8_t*)fpage + i;
         TEST(&queue == castPaddr_queue(nodeaddr, pagesize_queue(&queue)));
         fpage->queue = (void*)1;
         TEST((void*)1 == castPaddr_queue(nodeaddr, pagesize_queue(&queue)));
         fpage->queue = &queue;
         nodeaddr = (uint8_t*)lpage + i;
         TEST(&queue == castPaddr_queue(nodeaddr, pagesize_queue(&queue)));
         lpage->queue = (void*)2;
         TEST((void*)2 == castPaddr_queue(nodeaddr, pagesize_queue(&queue)));
         lpage->queue = &queue;
      }

      // TEST first_queue
      for (uint32_t off = 0; off < 65536; off += 233) {
         uint8_t* first = off + (uint8_t*)fpage;
         fpage->start_offset = (uint16_t) off;
         fpage->end_offset   = (uint16_t) (off + 1 + (off&0x1ff));
         if (fpage->end_offset < fpage->start_offset) break;
         TEST(0 == first_queue(&queue, (uint16_t)(2+(off&0x1ff))));
         TEST(first == first_queue(&queue, (uint16_t)(1+(off&0x1ff))));
         TEST(first == first_queue(&queue, (uint16_t)(off&0x1ff)));
         TEST(first == first_queue(&queue, (uint16_t)(1)));
      }

      // TEST last_queue
      for (uint32_t off = 0; off < 65536; off += 233) {
         uint8_t* last = off + (uint8_t*)lpage;
         lpage->start_offset = (uint16_t) off;
         lpage->end_offset   = (uint16_t) (off + 1 + (off & 0x1ff));
         if (lpage->end_offset < lpage->start_offset) break;
         TEST(0 == last_queue(&queue, (uint16_t)(2+(off & 0x1ff))));
         TEST(last+0 == last_queue(&queue, (uint16_t)(1+(off & 0x1ff))));
         TEST(last+1 == last_queue(&queue, (uint16_t)(off & 0x1ff)));
         TEST(last+(off & 0x1ff) == last_queue(&queue, (uint16_t)(1)));
      }

      // TEST sizefirst_queue
      for (uint32_t off = 0; off < 65536; off += 233) {
         fpage->start_offset = (uint16_t) off;
         fpage->end_offset   = (uint16_t) (off + (off & 0x1ff));
         if (fpage->end_offset < fpage->start_offset) break;
         TEST((off & 0x1ff) == sizefirst_queue(&queue));
      }

      // TEST sizelast_queue
      for (uint32_t off = 0; off < 65536; off += 233) {
         lpage->start_offset = (uint16_t) off;
         lpage->end_offset   = (uint16_t) (off + (off & 0x1ff));
         if (lpage->end_offset < lpage->start_offset) break;
         TEST((off & 0x1ff) == sizelast_queue(&queue));
      }

      // TEST sizebytes_queue
      for (unsigned size1 = 0; size1 < 65536; size1 += 233) {
         fpage->start_offset = (uint16_t) (size1&0xf);
         fpage->end_offset   = (uint16_t) size1;
         for (unsigned size2 = 0; size2 < 65536; size2 += 513) {
            lpage->start_offset = (uint16_t) (size2&0xf);
            lpage->end_offset   = (uint16_t) size2;
            unsigned size = size1 + size2 - (size1&0xf) - (size2&0xf);
            TEST(size == sizebytes_queue(&queue));
         }
      }

      // unprepare
      TEST(0 == free_queue(&queue));
   }

   return 0;
ONERR:
   free_queue(&queue);
   return EINVAL;
}

typedef struct opaque_t opaque_t;

static int test_iterator(void)
{
   queue_t          queue     = queue_INIT;
   queue_page_t*    qpages[5] = { 0 };
   queue_iterator_t iter      = queue_iterator_FREE;
   void*            node      = 0;
   void*            N         = 0;
   opaque_t*        obj       = 0;

   // TEST queue_iterator_FREE
   TEST(0 == iter.first);
   TEST(0 == iter.last);
   TEST(0 == iter.fpage);
   TEST(0 == iter.endpage);
   TEST(0 == iter.nodesize);

   for (unsigned t = 0; t <= 1; ++t) {

      if (t == 1) {
         // prepare: add single page
         TEST(0 == addlastpage_queue(&queue));
         qpages[0] = (queue_page_t*)queue.last;
      }

      // TEST initfirst_queueiterator: empty queue or page
      TEST(ENODATA == initfirst_queueiterator(&iter, &queue, 1));

      // TEST initlast_queueiterator: empty queue or page
      TEST(ENODATA == initlast_queueiterator(&iter, &queue, 1));

      // TEST foreach, foreachReverse: empty queue or page
      for (uint16_t step = 0; step <= 10; ++step) {
         unsigned count = 0;
         foreach(_queue, node2, &queue, step) {
            ++ count;
         }
         TEST(0 == count);
         foreachReverse(_queue, node2, &queue, step) {
            ++ count;
         }
         TEST(0 == count);
      }
   }

   // TEST initfirst_queueiterator: single page
   qpages[0]->end_offset = (uint16_t) (qpages[0]->start_offset + 10);
   TEST(0 == initfirst_queueiterator(&iter, &queue, 5));
   TEST(iter.first    == (uint8_t*)qpages[0] + sizeof(queue_page_t));
   TEST(iter.last     == (uint8_t*)qpages[0] + sizeof(queue_page_t) + 10 - 5);
   TEST(iter.fpage    == 0);
   TEST(iter.endpage  == qpages[0]);
   TEST(iter.nodesize == 5);

   // TEST initlast_queueiterator: single page
   TEST(0 == initlast_queueiterator(&iter, &queue, 3));
   TEST(iter.first    == (uint8_t*)qpages[0] + sizeof(queue_page_t) + 3);
   TEST(iter.last     == (uint8_t*)qpages[0] + sizeof(queue_page_t) + 10);
   TEST(iter.fpage    == 0);
   TEST(iter.endpage  == qpages[0]);
   TEST(iter.nodesize == 3);

   // TEST free_queueiterator: does nothing !
   TEST(0 == free_queueiterator(&iter));
   TEST(iter.first    == (uint8_t*)qpages[0] + sizeof(queue_page_t) + 3);
   TEST(iter.last     == (uint8_t*)qpages[0] + sizeof(queue_page_t) + 10);
   TEST(iter.fpage    == 0);
   TEST(iter.endpage  == qpages[0]);
   TEST(iter.nodesize == 3);

   // prepare: add list of qpages but no content
   TEST(0 == removeall_queue(&queue));
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      TEST(0 == addlastpage_queue(&queue));
      qpages[i] = (queue_page_t*)queue.last;
   }

   // TEST initfirst_queueiterator: first page size is compared with nodesize
   for (uint16_t s = 1; s <= 256; ++s) {
      qpages[0]->end_offset = (uint16_t) (qpages[0]->start_offset + s);

      // test nodesize > pagesize
      TEST(ENODATA == initfirst_queueiterator(&iter, &queue, (uint16_t)(s+1)));
      TEST(ENODATA == initfirst_queueiterator(&iter, &queue, 65535));

      // test nodesize == pagesize
      iter = (queue_iterator_t) queue_iterator_FREE;
      TEST(0 == initfirst_queueiterator(&iter, &queue, s));
      TEST(iter.first    == (uint8_t*)qpages[0] + sizeof(queue_page_t));
      TEST(iter.last     == (uint8_t*)qpages[0] + sizeof(queue_page_t));
      TEST(iter.fpage    == qpages[1]);
      TEST(iter.endpage  == qpages[lengthof(qpages)-1]);
      TEST(iter.nodesize == s);

      TEST(0 == free_queueiterator(&iter));
   }

   // TEST initfirst_queueiterator: nodesize == 0
   TEST(qpages[0]->end_offset > qpages[0]->start_offset);
   TEST(ENODATA == initfirst_queueiterator(&iter, &queue, 0));
   qpages[0]->end_offset = qpages[0]->start_offset;

   // TEST initlast_queueiterator: last page size is compared with nodesize
   for (uint16_t s = 1; s <= 256; ++s) {
      qpages[lengthof(qpages)-1]->end_offset = (uint16_t) (qpages[lengthof(qpages)-1]->start_offset + s);

      // test nodesize > pagesize
      TEST(ENODATA == initlast_queueiterator(&iter, &queue, (uint16_t)(s+1)));
      TEST(ENODATA == initlast_queueiterator(&iter, &queue, 65535));

      // test nodesize == pagesize
      iter = (queue_iterator_t) queue_iterator_FREE;
      TEST(0 == initlast_queueiterator(&iter, &queue, s));
      TEST(iter.first    == (uint8_t*)qpages[lengthof(qpages)-1] + sizeof(queue_page_t) + s);
      TEST(iter.last     == (uint8_t*)qpages[lengthof(qpages)-1] + sizeof(queue_page_t) + s);
      TEST(iter.fpage    == qpages[lengthof(qpages)-2]);
      TEST(iter.endpage  == qpages[0]);
      TEST(iter.nodesize == s);

      iter = (queue_iterator_t) queue_iterator_FREE;
   }

   // TEST initlast_queueiterator: nodesize == 0
   TEST(qpages[lengthof(qpages)-1]->end_offset > qpages[lengthof(qpages)-1]->start_offset);
   TEST(ENODATA == initlast_queueiterator(&iter, &queue, 0));
   qpages[lengthof(qpages)-1]->end_offset = qpages[lengthof(qpages)-1]->start_offset;

   // TEST next_queueiterator: stops at first empty or with pagesize < nodesize
   for (uint16_t s = 1; s <= 7; ++s) {
      for (uint16_t s2 = 0; s2 < s; ++s2) {
         for (uint16_t np = 1; np <= 3; ++np) {
            qpages[np]->end_offset = (uint16_t) (qpages[np]->start_offset + s2);
            for (unsigned i = 0; i < np; ++i) {
               qpages[i]->end_offset = (uint16_t) (qpages[i]->start_offset + s);
            }
            TEST(0 == initfirst_queueiterator(&iter, &queue, s));
            for (unsigned i = 0; i < np; ++i) {
               TEST(1 == next_queueiterator(&iter, &node));
               TEST(node == (uint8_t*)qpages[i] + qpages[i]->start_offset);
               // check iter
               TEST(iter.first    == (uint8_t*)qpages[i] + sizeof(queue_page_t) + s);
               TEST(iter.last     == (uint8_t*)qpages[i] + sizeof(queue_page_t));
               TEST(iter.fpage    == qpages[i+1]);
               TEST(iter.endpage  == qpages[lengthof(qpages)-1]);
               TEST(iter.nodesize == s);
            }
            /*obj == other type than void* supported*/
            TEST(0 == next_queueiterator(&iter, &obj));
            TEST(0 == free_queueiterator(&iter));
         }
      }
   }
   for (unsigned i = 0; i <= 3; ++i) {
      qpages[i]->end_offset = qpages[i]->start_offset;
   }

   // TEST prev_queueiterator: stops at first empty or with pagesize < nodesize
   for (uint16_t s = 1; s <= 7; ++s) {
      for (uint16_t s2 = 0; s2 < s; ++s2) {
         for (uint16_t np = lengthof(qpages)-2; np >= lengthof(qpages)-4; --np) {
            qpages[np]->end_offset = (uint16_t) (qpages[np]->start_offset + s2);
            for (unsigned i = lengthof(qpages)-1; i > np; --i) {
               qpages[i]->end_offset = (uint16_t) (qpages[i]->start_offset + s);
            }
            TEST(0 == initlast_queueiterator(&iter, &queue, s));
            for (unsigned i = lengthof(qpages)-1; i > np; --i) {
               TEST(1 == prev_queueiterator(&iter, &node));
               TEST(node == (uint8_t*)qpages[i] + sizeof(queue_page_t));
               // check iter
               TEST(iter.first    == (uint8_t*)qpages[i] + sizeof(queue_page_t) + s);
               TEST(iter.last     == (uint8_t*)qpages[i] + sizeof(queue_page_t));
               TEST(iter.fpage    == qpages[i-1]);
               TEST(iter.endpage  == qpages[0]);
               TEST(iter.nodesize == s);
            }
            /*obj == other type than void* supported*/
            TEST(0 == prev_queueiterator(&iter, &obj));
            TEST(0 == free_queueiterator(&iter));
         }
      }
   }
   for (unsigned i = lengthof(qpages)-1; i >= lengthof(qpages)-4; --i) {
      qpages[i]->end_offset = qpages[i]->start_offset;
   }

   // TEST nextskip_queueiterator: returns 0
   for (uint16_t s = 2; s <= 7; ++s) {
      for (uint16_t s2 = 1; s2 < s; ++s2) {
         qpages[0]->end_offset = (uint16_t) (qpages[0]->start_offset + s2);
         TEST(0 == initfirst_queueiterator(&iter, &queue, s2));
         TEST(0 == nextskip_queueiterator(&iter, s));
         TEST(0 == nextskip_queueiterator(&iter, 65535));
         // check nothing changed
         TEST(iter.first    == (uint8_t*)qpages[0] + sizeof(queue_page_t));
         TEST(iter.last     == (uint8_t*)qpages[0] + sizeof(queue_page_t));
         TEST(iter.fpage    == qpages[1]);
         TEST(iter.endpage  == qpages[lengthof(qpages)-1]);
         TEST(iter.nodesize == s2);
         TEST(0 == free_queueiterator(&iter));
      }
   }

   // prepare: fill qpages
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      qpages[i]->end_offset = defaultpagesize_queue();
   }

   for (unsigned step = 1; step <= 1024; step *= 2, step += 1) {
      // TEST initfirst_queueiterator: list of qpages with content
      TEST(0 == initfirst_queueiterator(&iter, &queue, (uint16_t)step));
      TEST(iter.first    == (uint8_t*)qpages[0] + sizeof(queue_page_t));
      TEST(iter.last     == (uint8_t*)qpages[0] + qpages[0]->end_offset - step);
      TEST(iter.fpage    == qpages[1]);
      TEST(iter.endpage  == qpages[lengthof(qpages)-1]);
      TEST(iter.nodesize == step);

      // TEST next_queueiterator: list of qpages with content
      for (unsigned i = 0; i < lengthof(qpages); ++i) {
         for (size_t offset = sizeof(queue_page_t); (offset + step) <= qpages[i]->end_offset; offset += step) {
            TEST(1 == next_queueiterator(&iter, &node));
            TEST(node == (uint8_t*)qpages[i] + offset);
            TEST(iter.first    == (uint8_t*)qpages[i] + (offset + step));
            TEST(iter.last     == (uint8_t*)qpages[i] + qpages[i]->end_offset - step);
            TEST(iter.fpage    == (i+1==lengthof(qpages) ? 0 : qpages[i+1]));
            TEST(iter.endpage  == qpages[lengthof(qpages)-1]);
            TEST(iter.nodesize == step);
         }
      }

      // TEST initlast_queueiterator: list of qpages with content
      TEST(0 == initlast_queueiterator(&iter, &queue, (uint16_t)step));
      TEST(iter.first    == (uint8_t*)qpages[lengthof(qpages)-1] + sizeof(queue_page_t) + step);
      TEST(iter.last     == (uint8_t*)qpages[lengthof(qpages)-1] + defaultpagesize_queue());
      TEST(iter.fpage    == qpages[lengthof(qpages)-2]);
      TEST(iter.endpage  == qpages[0]);
      TEST(iter.nodesize == step);

      // TEST prev_queueiterator: list of qpages with content
      for (unsigned i = lengthof(qpages); i--; ) {
         for (uint32_t offset = defaultpagesize_queue(); (sizeof(queue_page_t) + step) >= offset; ) {
            offset -= step;
            TEST(1 == prev_queueiterator(&iter, &node));
            TEST(node == (uint8_t*)qpages[i] + offset);
            TEST(iter.first    == (uint8_t*)qpages[i] + sizeof(queue_page_t) + step);
            TEST(iter.last     == (uint8_t*)qpages[i] + offset);
            TEST(iter.fpage    == (i ? qpages[i-1] : 0));
            TEST(iter.endpage  == qpages[0]);
            TEST(iter.nodesize == step);
         }
      }

      // TEST nextskip_queueiterator
      TEST(0 == free_queueiterator(&iter));
      TEST(0 == initfirst_queueiterator(&iter, &queue, 100));
      for (unsigned i = 0; i < lengthof(qpages); ++i) {
         TEST(1 == next_queueiterator(&iter, &node));
         TEST(iter.first == (uint8_t*)qpages[i] + sizeof(queue_page_t) + 100);
         iter.first = (uint8_t*)qpages[i] + sizeof(queue_page_t); // reset
         for (size_t offset = sizeof(queue_page_t), step2 = step; offset < qpages[i]->end_offset; offset += step2) {
            if ((offset + step2) > qpages[i]->end_offset) {
               step2 = qpages[i]->end_offset - offset;
            }
            TEST(1 == nextskip_queueiterator(&iter, (uint16_t)step2));
            TEST(iter.first    == (uint8_t*)qpages[i] + offset + step2);
            TEST(iter.last     == (uint8_t*)qpages[i] + defaultpagesize_queue() - 100);
            TEST(iter.fpage    == (i+1==lengthof(qpages) ? 0 : qpages[i+1]));
            TEST(iter.endpage  == qpages[lengthof(qpages)-1]);
            TEST(iter.nodesize == 100);
         }
      }
      TEST(0 == free_queueiterator(&iter));

      // TEST foreach: full queue
      unsigned i = 0;
      size_t offset = sizeof(queue_page_t);
      foreach(_queue, node2, &queue, (uint16_t)step) {
         if ((offset + step) > qpages[i]->end_offset) {
            offset = sizeof(queue_page_t);
            ++ i;
         }
         TEST(node2 == (uint8_t*)qpages[i] + offset);
         offset += step;
      }
      TEST(i == lengthof(qpages)-1);
      TEST(offset <= qpages[i]->end_offset);
      TEST(offset + step > qpages[i]->end_offset);

      // TEST foreachReverse: full queue
      i = lengthof(qpages)-1;
      offset = defaultpagesize_queue();
      foreachReverse(_queue, node2, &queue, (uint16_t)step) {
         if ((sizeof(queue_page_t) + step) > offset) {
            offset = defaultpagesize_queue();
            -- i;
         }
         offset -= step;
         TEST(node2 == (uint8_t*)qpages[i] + offset);
      }
      TEST(i == 0);
      TEST(offset >= qpages[i]->start_offset);
      TEST(offset <  qpages[i]->start_offset + step);
   }

   // TEST next_queueiterator: insertlast_queue not considered on same page or any created page
   N = 0;
   TEST(0 == removeall_queue(&queue));
   TEST(0 == insertlast_queue(&queue, 1, &node));
   TEST(0 == initfirst_queueiterator(&iter, &queue, 1));
   TEST(1 == next_queueiterator(&iter, &N));
   TEST(N == node)
   TEST(0 == insertlast_queue(&queue, 1, &node));
   TEST(0 == next_queueiterator(&iter, &N));
   TEST(0 == free_queueiterator(&iter));
   TEST(0 == removeall_queue(&queue));
   TEST(0 == insertfirst_queue(&queue, 1, &node));
   TEST(0 == initfirst_queueiterator(&iter, &queue, 1));
   TEST(1 == next_queueiterator(&iter, &N));
   TEST(N == node)
   TEST(0 == insertlast_queue(&queue, 1, &node));
   TEST(0 == next_queueiterator(&iter, &N));
   TEST(0 == free_queueiterator(&iter));

   // TEST next_queueiterator: remove and insert elements
   for (unsigned size = 16384; size <= 65536; size *= 2) {
      for (uint16_t step = 2; step <= 256; step = (uint16_t) (step * 2)) {
         TEST(0 == removeall_queue(&queue));
         // insert elements
         unsigned nrelem;
         for (nrelem = 0; (nrelem < size / step); ++nrelem) {
            TEST(0 == insertlast_queue(&queue, step, &node));
            *(uint16_t*)node = (uint16_t) nrelem;
         }

         // test remove current element
         nrelem = 0;
         foreach(_queue, node2, &queue, (uint16_t)step) {
            TEST(*(uint16_t*)node2 == (uint16_t) nrelem);
            ++ nrelem;
            TEST(0 == removefirst_queue(&queue, step));
         }
         TEST(nrelem == size / step);
         // only 1 element left
         TEST(last_queue(&queue, step) == first_queue(&queue, step));

         // test insert first elements
         TEST(0 == insertfirst_queue(&queue, step, &node));
         *(uint16_t*)node = 5;
         nrelem = 0;
         foreach(_queue, node2, &queue, (uint16_t)step) {
            ++ nrelem;
            TEST(*(uint16_t*)node2 == 5);
            TEST(0 == insertfirst_queue(&queue, step, &node));
         }
         TEST(1 == nrelem);
      }
   }

   // TEST prev_queueiterator: insertfirst_queue not considered on same page or any created page
   N = 0;
   TEST(0 == removeall_queue(&queue));
   TEST(0 == insertfirst_queue(&queue, 1, &node));
   TEST(0 == initlast_queueiterator(&iter, &queue, 1));
   TEST(1 == prev_queueiterator(&iter, &N));
   TEST(N == node)
   TEST(0 == insertfirst_queue(&queue, 1, &node));
   TEST(0 == prev_queueiterator(&iter, &N));
   TEST(0 == free_queueiterator(&iter));
   TEST(0 == removeall_queue(&queue));
   TEST(0 == insertlast_queue(&queue, 1, &node));
   TEST(0 == initlast_queueiterator(&iter, &queue, 1));
   TEST(1 == prev_queueiterator(&iter, &N));
   TEST(N == node)
   TEST(0 == insertfirst_queue(&queue, 1, &node));
   TEST(0 == prev_queueiterator(&iter, &N));
   TEST(0 == free_queueiterator(&iter));

   // TEST prev_queueiterator: remove and insert elements
   for (unsigned size = 16384; size <= 65536; size *= 2) {
      for (uint16_t step = 2; step <= 256; step = (uint16_t) (step * 2)) {
         TEST(0 == removeall_queue(&queue));
         // insert elements
         unsigned nrelem;
         for (nrelem = 0; (nrelem < size / step); ++nrelem) {
            TEST(0 == insertfirst_queue(&queue, step, &node));
            *(uint16_t*)node = (uint16_t) nrelem;
         }

         // test remove current element
         nrelem = 0;
         foreachReverse(_queue, node2, &queue, (uint16_t)step) {
            TEST(*(uint16_t*)node2 == (uint16_t) nrelem);
            ++ nrelem;
            TEST(0 == removelast_queue(&queue, step));
         }
         TEST(nrelem == size / step);
         // only 1 element left
         TEST(last_queue(&queue, step) == first_queue(&queue, step));

         // test insert last elements
         TEST(0 == insertlast_queue(&queue, step, &node));
         *(uint16_t*)node = 6;
         nrelem = 0;
         foreachReverse(_queue, node2, &queue, (uint16_t)step) {
            ++ nrelem;
            TEST(*(uint16_t*)node2 == 6);
            TEST(0 == insertlast_queue(&queue, step, &node));
         }
         TEST(1 == nrelem);
      }
   }

   // unprepare
   TEST(0 == free_queue(&queue));

   return 0;
ONERR:
   free_queue(&queue);
   return EINVAL;
}

static int test_update(void)
{
   queue_t       queue     = queue_INIT;
   void*         node      = 0;
   queue_page_t* qpages[5] = { 0 };

   // TEST insertfirst_queue: empty queue
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      TEST(0 == queue.last);
      TEST(0 == insertfirst_queue(&queue, nodesize, &node));
      TEST(0 != queue.last);
      queue_page_t* last = (void*) queue.last;
      TEST(last->next  == (void*) last);
      TEST(last->prev  == (void*) last);
      TEST(last->queue == &queue);
      TEST(last->end_offset   == defaultpagesize_queue());
      TEST(last->start_offset == defaultpagesize_queue() - nodesize);
      TEST(node == (uint8_t*)last + last->start_offset);
      TEST(0 == free_queue(&queue));
   }

   // TEST insertlast_queue: empty queue
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      TEST(0 == queue.last);
      TEST(0 == insertlast_queue(&queue, nodesize, &node));
      TEST(0 != queue.last);
      queue_page_t* last = (void*) queue.last;
      TEST(last->next  == (void*) last);
      TEST(last->prev  == (void*) last);
      TEST(last->queue == &queue);
      TEST(last->end_offset   == sizeof(queue_page_t) + nodesize);
      TEST(last->start_offset == sizeof(queue_page_t));
      TEST(node == (uint8_t*)last + last->start_offset);
      TEST(0 == free_queue(&queue));
   }

   // TEST insertfirst_queue: add pages
   TEST(0 == queue.last);
   TEST(0 == insertfirst_queue(&queue, 0, &node));
   TEST(0 != queue.last);
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t* last  = last_pagelist(cast_dlist(&queue));
      queue_page_t* first = first_pagelist(cast_dlist(&queue));
      queue_page_t* old   = (void*)first->next;
      bool           isNew = (first->start_offset - sizeof(queue_page_t)) < nodesize;
      uint32_t       off   = isNew ? defaultpagesize_queue() : first->start_offset;
      TEST(0 == insertfirst_queue(&queue, nodesize, &node));
      if (isNew) {
         TEST(first == next_pagelist(first_pagelist(cast_dlist(&queue))));
         old   = first;
         first = first_pagelist(cast_dlist(&queue));
      }
      TEST(first == first_pagelist(cast_dlist(&queue)));
      TEST(first->next  == (void*)old);
      TEST(first->prev  == (void*)last);
      TEST(first->queue == &queue);
      TEST(first->end_offset   == defaultpagesize_queue());
      TEST(first->start_offset == off - nodesize);
   }
   TEST(0 == free_queue(&queue));

   // TEST insertlast_queue: add pages
   TEST(0 == queue.last);
   TEST(0 == insertlast_queue(&queue, 0, &node));
   TEST(0 != queue.last);
   for (unsigned nodesize = 0; nodesize <= maxelemsize_queue(defaultpagesize_queue()); nodesize = nodesize < 512 ? nodesize+1 : 2*nodesize) {
      queue_page_t* last  = last_pagelist(cast_dlist(&queue));
      queue_page_t* first = first_pagelist(cast_dlist(&queue));
      queue_page_t* old   = (void*)last->prev;
      bool           isNew = ((size_t)defaultpagesize_queue() - last->end_offset) < nodesize;
      uint32_t       off   = isNew ? sizeof(queue_page_t) : last->end_offset;
      TEST(0 == insertlast_queue(&queue, (uint16_t)nodesize, &node));
      if (isNew) {
         TEST(queue.last == last->next);
         old  = last;
         last = last_pagelist(cast_dlist(&queue));
      }
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->next  == (void*)first);
      TEST(last->prev  == (void*)old);
      TEST(last->queue == &queue);
      TEST(last->end_offset   == off + nodesize);
      TEST(last->start_offset == sizeof(queue_page_t));
   }
   TEST(0 == free_queue(&queue));

   // TEST insertfirst_queue: ENOMEM
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM);
   TEST(ENOMEM == insertfirst_queue(&queue, 1, &node));
   TEST(0 == queue.last); // nothing added

   // TEST insertlast_queue: ENOMEM
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM);
   TEST(ENOMEM == insertlast_queue(&queue, 1, &node));
   TEST(0 == queue.last); // nothing added

   // TEST removefirst_queue: empty queue
   TEST(0 == queue.last);
   TEST(ENODATA == removefirst_queue(&queue, 0));

   // TEST removelast_queue: empty queue
   TEST(0 == queue.last);
   TEST(ENODATA == removelast_queue(&queue, 0));

   // TEST resizelast_queue: empty queue
   TEST(0 == queue.last);
   TEST(ENODATA == resizelast_queue(&queue, &node, 0, 1));

   // TEST removefirst_queue: single page queue
   TEST(0 == queue.last);
   TEST(0 == addlastpage_queue(&queue));
   TEST(0 != queue.last);
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      last->start_offset = sizeof(queue_page_t);
      last->end_offset   = defaultpagesize_queue();
      TEST(0 == removefirst_queue(&queue, nodesize));
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->queue == &queue);
      TEST(last->end_offset   == defaultpagesize_queue());
      TEST(last->start_offset == sizeof(queue_page_t) + nodesize);
   }

   // TEST removelast_queue: single page queue
   TEST(0 != queue.last);
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      last->start_offset = sizeof(queue_page_t);
      last->end_offset   = defaultpagesize_queue();
      TEST(0 == removelast_queue(&queue, nodesize));
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->queue == &queue);
      TEST(last->end_offset   == defaultpagesize_queue() - nodesize);
      TEST(last->start_offset == sizeof(queue_page_t));
   }

   // TEST resizelast_queue: single page queue
   TEST(0 != queue.last);
   for (uint32_t i = sizeof(queue_page_t); i < defaultpagesize_queue(); ++i) {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      *(i + (uint8_t*)last) = (uint8_t)i;
   }
   for (uint16_t nodesize = 0; nodesize <= 512; ++nodesize) {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      last->start_offset = sizeof(queue_page_t);
      last->end_offset   = defaultpagesize_queue();
      TEST(0 == resizelast_queue(&queue, &node, 512, nodesize));
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->queue == &queue);
      TEST(last->end_offset   == defaultpagesize_queue() - 512 + nodesize);
      TEST(last->start_offset == sizeof(queue_page_t));
      TEST(node               == (uint8_t*)last + last->end_offset - nodesize);
      TEST(0 == resizelast_queue(&queue, &node, nodesize, 512));
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->queue == &queue);
      TEST(last->end_offset   == defaultpagesize_queue());
      TEST(last->start_offset == sizeof(queue_page_t));
      TEST(node               == (uint8_t*)last + last->end_offset - 512);
   }
   // content has not changed !
   for (uint32_t i = sizeof(queue_page_t); i < defaultpagesize_queue(); ++i) {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      TEST((uint8_t)i == *(i + (uint8_t*)last));
   }

   // TEST resizelast_queue: single page queue (shift from end to start)
   TEST(0 != queue.last);
   for (uint16_t nodesize = 128; nodesize <= 512; ++nodesize) {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      last->start_offset = defaultpagesize_queue() - 127;
      last->end_offset   = defaultpagesize_queue();
      memset((uint8_t*)last + last->start_offset, (uint8_t)nodesize, 127);
      TEST(0 == resizelast_queue(&queue, &node, 127, nodesize));
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->queue        == &queue);
      TEST(last->end_offset   == sizeof(queue_page_t) + nodesize);
      TEST(last->start_offset == sizeof(queue_page_t));
      TEST(node               == (uint8_t*)last + last->start_offset);
      // content preserved
      for (unsigned i = 0; i < 127; ++i) {
         TEST(((uint8_t*)node)[i] == (uint8_t)nodesize);
         unsigned e = defaultpagesize_queue() -1 -i;
         TEST(((uint8_t*)last)[e] == (uint8_t)nodesize);
      }
   }

   // TEST removefirst_queue: EOVERFLOW
   TEST(0 != queue.last);
   {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      last->start_offset = sizeof(queue_page_t) + 1;
      last->end_offset   = sizeof(queue_page_t) + 2;
      TEST(EOVERFLOW == removefirst_queue(&queue, 2));
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->queue == &queue);
      TEST(last->end_offset   == sizeof(queue_page_t) + 2);
      TEST(last->start_offset == sizeof(queue_page_t) + 1);
   }

   // TEST removelast_queue: EOVERFLOW
   TEST(0 != queue.last);
   {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      last->start_offset = sizeof(queue_page_t) + 1;
      last->end_offset   = sizeof(queue_page_t) + 3;
      TEST(EOVERFLOW == removelast_queue(&queue, 3));
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->queue == &queue);
      TEST(last->end_offset   == sizeof(queue_page_t) + 3);
      TEST(last->start_offset == sizeof(queue_page_t) + 1);
   }

   // TEST resizelast_queue: EOVERFLOW
   TEST(0 != queue.last);
   {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      last->start_offset = sizeof(queue_page_t) + 1;
      last->end_offset   = sizeof(queue_page_t) + 4;
      TEST(EOVERFLOW == resizelast_queue(&queue, &node, 4, 3));
      TEST(last == last_pagelist(cast_dlist(&queue)));
      TEST(last->queue == &queue);
      TEST(last->end_offset   == sizeof(queue_page_t) + 4);
      TEST(last->start_offset == sizeof(queue_page_t) + 1);
   }

   // TEST resizelast_queue: EINVAL
   TEST(EINVAL== resizelast_queue(&queue, &node, 1, 513));
   TEST(0 == free_queue(&queue));

   // TEST removefirst_queue: ENOMEM
   TEST(0 == queue.last);
   TEST(0 == addlastpage_queue(&queue));
   TEST(0 != queue.last);
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM);
   TEST(ENOMEM == removefirst_queue(&queue, 0));
   TEST(0 == queue.last); // nevertheless freed

   // TEST removelast_queue: ENOMEM
   TEST(0 == queue.last);
   TEST(0 == addlastpage_queue(&queue));
   TEST(0 != queue.last);
   init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM);
   TEST(ENOMEM == removelast_queue(&queue, 0));
   TEST(0 == queue.last); // nevertheless freed

   // TEST removefirst_queue: multiple pages
   TEST(0 == queue.last);
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      TEST(0 == addlastpage_queue(&queue));
      qpages[i] = (queue_page_t*)queue.last;
      qpages[i]->end_offset = defaultpagesize_queue();
   }
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      uint32_t pagesize = defaultpagesize_queue();
      for (uint32_t off = sizeof(queue_page_t); off < pagesize; ++off) {
         TEST(qpages[i] == first_pagelist(cast_dlist(&queue)));
         TEST(qpages[i]->queue        == &queue);
         TEST(qpages[i]->end_offset   == defaultpagesize_queue());
         TEST(qpages[i]->start_offset == off);
         TEST(0 == removefirst_queue(&queue, 1));
      }
   }
   TEST(0 == queue.last);

   // TEST removelast_queue: multiple pages
   TEST(0 == queue.last);
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      TEST(0 == addlastpage_queue(&queue));
      qpages[i] = (queue_page_t*)queue.last;
      qpages[i]->end_offset = defaultpagesize_queue();
   }
   for (int i = lengthof(qpages)-1; i >= 0; --i) {
      uint32_t pagesize = defaultpagesize_queue();
      for (uint32_t off = pagesize; off > sizeof(queue_page_t); --off) {
         TEST(qpages[i] == last_pagelist(cast_dlist(&queue)));
         TEST(qpages[i]->queue        == &queue);
         TEST(qpages[i]->end_offset   == off);
         TEST(qpages[i]->start_offset == sizeof(queue_page_t));
         TEST(0 == removelast_queue(&queue, 1));
      }
   }
   TEST(0 == queue.last);

   // TEST removeall_queue: multiple pages
   TEST(0 == queue.last);
   for (unsigned i = 0; i < lengthof(qpages); ++i) {
      TEST(0 == addlastpage_queue(&queue));
      qpages[i] = (queue_page_t*)queue.last;
      qpages[i]->end_offset = defaultpagesize_queue();
   }
   TEST(0 == removeall_queue(&queue));
   TEST(0 == queue.last);

   // TEST removeall_queue: EINVAL
   for (unsigned errcount = 1; errcount <= lengthof(qpages); errcount += (unsigned)lengthof(qpages)-1) {
      TEST(0 == queue.last);
      for (unsigned i = 0; i < lengthof(qpages); ++i) {
         TEST(0 == addlastpage_queue(&queue));
         qpages[i] = (queue_page_t*)queue.last;
         qpages[i]->end_offset = defaultpagesize_queue();
      }
      init_testerrortimer(&s_queuepage_errtimer, errcount, EINVAL);
      TEST(EINVAL == removeall_queue(&queue));
      TEST(0 == queue.last);
   }

   // TEST resizelast_queue: add new page
   TEST(0 == queue.last);
   TEST(0 == addlastpage_queue(&queue));
   // fill page
   for (uint32_t i = sizeof(queue_page_t); i < defaultpagesize_queue(); ++i) {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      *(i + (uint8_t*)last) = (uint8_t)i;
   }
   for (uint16_t nodesize = 0; nodesize <= 256; ++nodesize) {
      queue_page_t* first = first_pagelist(cast_dlist(&queue));
      TEST(first == last_pagelist(cast_dlist(&queue)));
      first->start_offset  = sizeof(queue_page_t);
      first->end_offset    = defaultpagesize_queue();
      memset((uint8_t*)end_queuepage(first) - nodesize, (uint8_t)nodesize, nodesize);
      TEST(0 == resizelast_queue(&queue, &node, nodesize, (uint16_t)(nodesize+256)));
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      TEST(last  != first);
      TEST(first == first_pagelist(cast_dlist(&queue)));
      TEST(first->queue == &queue);
      TEST(first->end_offset   == defaultpagesize_queue()-nodesize);
      TEST(first->start_offset == sizeof(queue_page_t));
      TEST(last->queue == &queue);
      TEST(last->end_offset   == sizeof(queue_page_t)+nodesize+256);
      TEST(last->start_offset == sizeof(queue_page_t));
      TEST(node               == (uint8_t*)last + last->start_offset);
      // content is copied to new page
      for (uint32_t i = sizeof(queue_page_t); i < sizeof(queue_page_t) + nodesize; ++i) {
         TEST(*(i + (uint8_t*)last) == (uint8_t)(nodesize));
      }
      TEST(0 == removelastpage_queue(&queue));
   }
   // content of old page has not changed
   for (uint32_t i = sizeof(queue_page_t); i < defaultpagesize_queue(); ++i) {
      queue_page_t* last = last_pagelist(cast_dlist(&queue));
      if (i >= defaultpagesize_queue()-256) {
         TEST((uint8_t)0 == *(i + (uint8_t*)last));
      } else {
         TEST((uint8_t)i == *(i + (uint8_t*)last));
      }
   }

   // TEST resizelast_queue: ENOMEM
   {
      queue_page_t* first = first_pagelist(cast_dlist(&queue));
      queue_page_t* last  = last_pagelist(cast_dlist(&queue));
      TEST(0 != last);
      TEST(first == last);
      first->start_offset  = sizeof(queue_page_t);
      first->end_offset    = defaultpagesize_queue();
      init_testerrortimer(&s_queuepage_errtimer, 1, ENOMEM);
      TEST(ENOMEM == resizelast_queue(&queue, &node, 1, 127));
         // nothing done
      TEST(first == first_pagelist(cast_dlist(&queue)));
      TEST(last  == last_pagelist(cast_dlist(&queue)));
      TEST(first->start_offset  == sizeof(queue_page_t))
      TEST(first->end_offset    == defaultpagesize_queue());
   }

   // unprepare
   TEST(0 == free_queue(&queue));

   return 0;
ONERR:
   free_queue(&queue);
   return EINVAL;
}

typedef struct test_node_t {
   int x;
   int y;
   char str[16];
} test_node_t;

// TEST queue_IMPLEMENT
queue_IMPLEMENT(_testqueue, test_node_t)

static int test_generic(void)
{
   queue_t      queue = queue_INIT;
   struct {
      dlist_node_t* last;
      uint8_t       pagesize;
   }            queue2;
   test_node_t* node = 0;

   // TEST cast_queue
   TEST(cast_queue(&queue)  == &queue);
   TEST(cast_queue(&queue2) == (queue_t*)&queue2);

   // TEST init_queue
   memset(&queue, 255, sizeof(queue));
   TEST(0 == init_testqueue(&queue, defaultpagesize_queue()));
   TEST(0 == queue.last);

   // TEST initmove_queue
   for (int i = 0; i < 5; ++i) {
      TEST(0 == addlastpage_queue(&queue));
   }
   TEST(0 != queue.last);
   queue_page_t* last = (queue_page_t*) queue.last;
   queue2.last = 0;
   initmove_testqueue(cast_queue(&queue2), &queue);
   TEST(queue2.last == (void*)last);
   TEST(queue.last  == 0);
   initmove_testqueue(&queue, cast_queue(&queue2));
   TEST(queue2.last == 0);
   TEST(queue.last  == (void*)last);

   // TEST free_queue
   TEST(queue.last != 0);
   TEST(0 == free_testqueue(&queue));
   TEST(queue.last == 0);

   // TEST pagesize_queue
   TEST(defaultpagesize_queue() == pagesize_testqueue(&queue));

   // TEST isempty_queue
   queue.last = (void*)1;
   TEST(0 == isempty_testqueue(&queue));
   queue.last = 0;
   TEST(1 == isempty_testqueue(&queue));

   // TEST isfree_queue
   queue.last = (void*)1;
   TEST(0 == isfree_queue(&queue));
   queue.last = 0;
   TEST(1 == isfree_queue(&queue));

   // TEST first_queue, last_queue, sizefirst_queue, sizelast_queue, sizebytes_queue
   TEST(queue.last == 0);
   TEST(0 == addlastpage_queue(&queue));
   queue_page_t* first = (queue_page_t*) queue.last;
   first->end_offset = (uint16_t) (first->end_offset + sizeof(test_node_t));
   TEST(0 == addlastpage_queue(&queue));
   last = (queue_page_t*) queue.last;
   last->end_offset  = (uint16_t) (last->end_offset + 2*sizeof(test_node_t));
   TEST(first_testqueue(&queue)     == (test_node_t*)((uint8_t*)first + first->start_offset));
   TEST(last_testqueue(&queue)      == (test_node_t*)((uint8_t*)last  + last->end_offset-sizeof(test_node_t)));
   TEST(sizefirst_testqueue(&queue) == sizeof(test_node_t));
   TEST(sizelast_testqueue(&queue)  == 2*sizeof(test_node_t));
   TEST(sizebytes_testqueue(&queue) == 3*sizeof(test_node_t));

   // TEST insertfirst_queue
   TEST(0 == free_testqueue(&queue));
   TEST(queue.last == 0);
   TEST(0 == insertfirst_testqueue(&queue, &node));
   TEST(queue.last != 0);
   last = (queue_page_t*) queue.last;
   TEST(last->end_offset   == defaultpagesize_queue());
   TEST(last->start_offset == defaultpagesize_queue() - sizeof(test_node_t));
   TEST(node               == (test_node_t*)((uint8_t*)last + last->start_offset));

   // TEST insertlast_queue
   TEST(0 == free_testqueue(&queue));
   TEST(queue.last == 0);
   TEST(0 == insertlast_testqueue(&queue, &node));
   TEST(queue.last != 0);
   last = (queue_page_t*) queue.last;
   TEST(last->end_offset   == sizeof(queue_page_t) + sizeof(test_node_t));
   TEST(last->start_offset == sizeof(queue_page_t));
   TEST(node               == (test_node_t*)((uint8_t*)last + last->start_offset));

   // TEST removefirst_queue
   TEST(0 == free_testqueue(&queue));
   TEST(queue.last == 0);
   TEST(0 == insertfirst_testqueue(&queue, &node));
   TEST(0 == insertfirst_testqueue(&queue, &node));
   TEST(queue.last != 0);
   last = (queue_page_t*) queue.last;
   TEST(last->end_offset   == defaultpagesize_queue());
   TEST(last->start_offset == defaultpagesize_queue() - 2*sizeof(test_node_t));
   TEST(node               == (test_node_t*)((uint8_t*)last + last->start_offset));
   TEST(0 == removefirst_testqueue(&queue));
   TEST(last->end_offset   == defaultpagesize_queue());
   TEST(last->start_offset == defaultpagesize_queue() - 1*sizeof(test_node_t));

   // TEST removelast_queue
   TEST(0 == free_testqueue(&queue));
   TEST(queue.last == 0);
   TEST(0 == insertfirst_testqueue(&queue, &node));
   TEST(0 == insertfirst_testqueue(&queue, &node));
   TEST(queue.last != 0);
   last = (queue_page_t*) queue.last;
   TEST(last->end_offset   == defaultpagesize_queue());
   TEST(last->start_offset == defaultpagesize_queue() - 2*sizeof(test_node_t));
   TEST(node               == (test_node_t*)((uint8_t*)last + last->start_offset));
   TEST(0 == removelast_testqueue(&queue));
   TEST(last->end_offset   == defaultpagesize_queue() - 1*sizeof(test_node_t));
   TEST(last->start_offset == defaultpagesize_queue() - 2*sizeof(test_node_t));

   // TEST removeall_queue
   TEST(0 == addlastpage_queue(&queue));
   last = (queue_page_t*) queue.last;
   TEST(0 == addlastpage_queue(&queue));
   TEST(last != (queue_page_t*) queue.last);
   // test
   TEST(0 == removeall_testqueue(&queue));
   // check result
   TEST(0 == queue.last);

   // TEST foreach
   TEST(0 == free_testqueue(&queue));
   for (int x = 1; x <= 5; ++x) {
      TEST(0 == insertlast_testqueue(&queue, &node));
      node->x = x;
   }
   for (int x = 1; x <= 5; ++x) {
      TEST(0 == insertfirst_testqueue(&queue, &node));
      node->x = -x;
   }
   for (int x = -5; x == -5; ) {
      node = 0;
      foreach (_testqueue, node2, &queue) {
         TEST(node2->x == x);
         TEST(!node || node2 == node+1);
         node = node2;
         ++ x;
         if (0 == x) {
            node = 0;
            x = 1;
         }
      }
      TEST(x == 6);
   }

   // TEST foreachReverse
   for (int x = 5; x == 5; ) {
      foreachReverse (_testqueue, node2, &queue) {
         TEST(node2->x == x);
         -- x;
         if (0 == x) {
            x = -1;
         }
      }
      TEST(x == -6);
   }

   // unprepare
   TEST(0 == free_queue(&queue));

   return 0;
ONERR:
   free_queue(&queue);
   return EINVAL;
}

int unittest_ds_inmem_queue()
{
   if (test_queuepage())      goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_iterator())       goto ONERR;
   if (test_update())         goto ONERR;
   if (test_generic())        goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
