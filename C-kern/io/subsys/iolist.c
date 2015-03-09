/* title: IOList impl

   Implements <IOList>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/io/subsys/iolist.h
    Header file <IOList>.

   file: C-kern/io/subsys/iolist.c
    Implementation file <IOList impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/subsys/iolist.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/io/subsys/iothread.h"
#include "C-kern/api/platform/task/thread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/task/itc/itccounter.h"
#endif


// section: iolist_t

// group: lifetime

// group: update

void insertlast_iolist(iolist_t* iolist, uint8_t nrtask, /*own*/iotask_t* iot[nrtask], struct thread_t* thread/*0 => ignored*/)
{
   if (nrtask == 0) return;

   // TODO: extract lock into task/sync/spinlock.h
   while (0 != set_atomicflag(&iolist->lock)) {
      yield_thread();
   }

   int i = nrtask-1;
   iotask_t* last = iolist->last;
   iolist->last = iot[i];

   iot[i]->state = iostate_QUEUED;

   if (last) {
      iot[i]->iolist_next = last->iolist_next;
      last->iolist_next  = iot[0];
   } else {
      iot[i]->iolist_next = iot[0];
   }

   for (; i > 0; --i) {
      iot[i-1]->iolist_next = iot[i];
      iot[i-1]->state = iostate_QUEUED;
   }

   iolist->size += nrtask;

   clear_atomicflag(&iolist->lock);

   if (!last && thread) {
      resume_thread(thread);
   }
}

int tryremovefirst_iolist(iolist_t* iolist, /*out*/iotask_t** iot)
{
   while (0 != set_atomicflag(&iolist->lock)) {
      yield_thread();
   }

   iotask_t* last = iolist->last;
   if (! last) {
      clear_atomicflag(&iolist->lock);
      return ENODATA;
   }

   iotask_t* first = last->iolist_next;

   last->iolist_next = first->iolist_next;
   first->iolist_next = 0;

   if (first == last) {
      iolist->last = 0;
   }

   -- iolist->size;

   clear_atomicflag(&iolist->lock);

   // set out param

   *iot = first;
   return 0;
}

void cancelall_iolist(iolist_t* iolist)
{
   while (0 != set_atomicflag(&iolist->lock)) {
      yield_thread();
   }

   iolist->size = 0;

   iotask_t* last = iolist->last;

   if (last) {
      iolist->last = 0;

      iotask_t* node = last;
      do {
         iotask_t* next = node->iolist_next;
         node->iolist_next = 0;
         node->err = ECANCELED;
         write_atomicint(&node->state, iostate_CANCELED);
         if (node->readycount) {
            add_itccounter(node->readycount, 1);
         }
         node = next;
      } while (last != node);
   }

   clear_atomicflag(&iolist->lock);
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_enums(void)
{
   // iostate_e

   static_assert(0 == iostate_NULL, "clearing with 0 does work");
   static_assert(0 == (iostate_QUEUED & iostate_READY_MASK), "not ready");
   static_assert(iostate_OK       == (iostate_OK       & iostate_READY_MASK), "ready");
   static_assert(iostate_ERROR    == (iostate_ERROR    & iostate_READY_MASK), "ready");
   static_assert(iostate_CANCELED == (iostate_CANCELED & iostate_READY_MASK), "ready");
   static_assert(0 == (iostate_OK       & iostate_QUEUED), "no overlap with queued");
   static_assert(0 == (iostate_ERROR    & iostate_QUEUED), "no overlap with queued");
   static_assert(0 == (iostate_CANCELED & iostate_QUEUED), "no overlap with queued");

   // ioop_e

   static_assert( ioop_NOOP == 0 && ioop_READ == 1
                  && ioop_WRITE == 2 && ioop__NROF == 3,
                  "correct value of ioop__NROF");

   return 0;
}

static int test_iotask(void)
{
   iotask_t     iotask = iotask_FREE;
   itccounter_t counter;
   iotask_t     iotask0;
   iotask_t     iotask255;

   // prepare
   memset(&iotask0, 0, sizeof(iotask0));
   memset(&iotask255, 255, sizeof(iotask255));

   // group lifetime

   // TEST iotask_FREE
   TEST(0 == iotask.iolist_next);
   TEST(0 == iotask.err);
   TEST(0 == iotask.bytesrw);
   TEST(0 == iotask.state);
   TEST(0 == iotask.op);
   TEST(! isvalid_iochannel(iotask.ioc));
   TEST(0 == iotask.offset);
   TEST(0 == iotask.bufaddr);
   TEST(0 == iotask.bufsize);
   TEST(0 == iotask.readycount);

   for (size_t size = 1; size; size <<= 1) {
      for (void* addr = (void*)(uintptr_t)1; addr; addr = (void*)((uintptr_t)addr << 1)) {
         for (uintptr_t off = 1; off; off <<= 1) {
            for (int ioc = 1; ioc <= 256; ioc <<= 1) {
               for (int iscounter = 0; iscounter <= 1; ++iscounter) {
                  itccounter_t* c = iscounter ? &counter : 0;

                  // TEST initreadp_iotask
                  memset(&iotask, 255, sizeof(iotask));
                  initreadp_iotask(&iotask, ioc, size, addr, off, c);
                  TEST(iotask.iolist_next == 0);
                  TEST(iotask.bytesrw == (size_t)-1); // not changed
                  TEST(iotask.state == iostate_NULL);
                  TEST(iotask.op == ioop_READ);
                  TEST(iotask.ioc == ioc);
                  TEST(iotask.offset == off);
                  TEST(iotask.bufaddr == addr);
                  TEST(iotask.bufsize == size);
                  TEST(iotask.readycount == c);

                  // TEST initread_iotask
                  memset(&iotask, 255, sizeof(iotask));
                  initread_iotask(&iotask, ioc, size, addr, c);
                  TEST(iotask.iolist_next == 0);
                  TEST(iotask.bytesrw == (size_t)-1); // not changed
                  TEST(iotask.state == iostate_NULL);
                  TEST(iotask.op == ioop_READ);
                  TEST(iotask.ioc == ioc);
                  TEST(iotask.offset == -1);
                  TEST(iotask.bufaddr == addr);
                  TEST(iotask.bufsize == size);
                  TEST(iotask.readycount == c);

                  // TEST initwritep_iotask
                  memset(&iotask, 255, sizeof(iotask));
                  initwritep_iotask(&iotask, ioc, size, (const void*)addr, off, c);
                  TEST(iotask.iolist_next == 0);
                  TEST(iotask.bytesrw == (size_t)-1); // not changed
                  TEST(iotask.state == iostate_NULL);
                  TEST(iotask.op == ioop_WRITE);
                  TEST(iotask.ioc == ioc);
                  TEST(iotask.offset == off);
                  TEST(iotask.bufaddr == addr);
                  TEST(iotask.bufsize == size);
                  TEST(iotask.readycount == c);

                  // TEST initwrite_iotask
                  memset(&iotask, 255, sizeof(iotask));
                  initwrite_iotask(&iotask, ioc, size, (const void*)addr, c);
                  TEST(iotask.iolist_next == 0);
                  TEST(iotask.bytesrw == (size_t)-1); // not changed
                  TEST(iotask.state == iostate_NULL);
                  TEST(iotask.op == ioop_WRITE);
                  TEST(iotask.ioc == ioc);
                  TEST(iotask.offset == -1);
                  TEST(iotask.bufaddr == addr);
                  TEST(iotask.bufsize == size);
                  TEST(iotask.readycount == c);
               }
            }
         }
      }
   }

   // group query

   // TEST isvalid_iotask: uses only op/bufaddr/bufsize
   iotask = (iotask_t) iotask_FREE;
   for (int ioff = 0; ioff < 3; ++ioff) {
      for (int iba = 0; iba < 2; ++iba) {
         for (int ibs = 0; ibs < 3; ++ibs) {
            for (int iop = 0; iop <= ioop__NROF; ++iop) {
               iotask.op = (uint8_t) iop;
               iotask.offset  = ioff == 0 ? -1 : ioff == 1  ? 0 : OFF_MAX; // is not checked
               iotask.bufaddr = iba == 0 ? 0  : &iotask;
               iotask.bufsize = ibs == 0 ? 0  : ibs == 1 ? sizeof(iotask) : SIZE_MAX;
               TEST((iba&&ibs&&iop!=ioop__NROF) == isvalid_iotask(&iotask));
            }
         }
      }
   }

   // group: update

   // TEST setoffset_iotask
   for (off_t off = OFF_MAX; off >= 0; off = (off == 0 ? -1 : off >> 1)) {
      // test >= 0
      memset(&iotask, 0, sizeof(iotask));
      setoffset_iotask(&iotask, off);
      // check only offset changed
      iotask0.offset = off;
      TEST(0 == memcmp(&iotask, &iotask0, sizeof(iotask)));

      // test <= 0
      memset(&iotask, 255, sizeof(iotask));
      setoffset_iotask(&iotask, -off);
      // check only offset changed
      iotask255.offset = -off;
      TEST(0 == memcmp(&iotask, &iotask255, sizeof(iotask)));
   }
   // reset
   memset(&iotask0, 0, sizeof(iotask0));
   memset(&iotask255, 255, sizeof(iotask255));

   // TEST setsize_iotask
   for (size_t size = (size_t)-2; size != (size_t)-1; size = (size == 0 ? (size_t)-1 : size >> 1)) {
      // test size
      memset(&iotask, 0, sizeof(iotask));
      setsize_iotask(&iotask, size);
      // check only offset changed
      iotask0.bufsize = size;
      TEST(0 == memcmp(&iotask, &iotask0, sizeof(iotask)));

      // test -size
      memset(&iotask, 255, sizeof(iotask));
      setsize_iotask(&iotask, -size);
      // check only size changed
      iotask255.bufsize = -size;
      TEST(0 == memcmp(&iotask, &iotask255, sizeof(iotask)));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   iolist_t  iolist = iolist_INIT;
   iotask_t  iotask_buffer[4];
   iotask_t* iotask[lengthof(iotask_buffer)];
   itccounter_t counter = itccounter_FREE;

   // prepare0
   TEST(0 == init_itccounter(&counter));
   memset(iotask_buffer, 0, sizeof(iotask_buffer));
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      iotask[i] = &iotask_buffer[i];
      iotask[i]->iolist_next = &iotask_buffer[(i+1) % lengthof(iotask)];
      iotask[i]->readycount = (i&1) ? 0 : &counter;
   }

   // TEST iolist_INIT
   TEST(0 == iolist.lock);
   TEST(0 == iolist.size);
   TEST(0 == iolist.last);

   // TEST init_iolist
   memset(&iolist, 255, sizeof(iolist));
   init_iolist(&iolist);
   TEST(0 == iolist.lock);
   TEST(0 == iolist.size);
   TEST(0 == iolist.last);

   // TEST free_iolist
   iolist.last = iotask[0];
   iolist.size = lengthof(iotask);
   free_iolist(&iolist);
   // check iolist
   TEST(0 == iolist.lock);
   TEST(0 == iolist.size);
   TEST(0 == iolist.last);
   // check counter
   TEST(lengthof(iotask)/2 == reset_itccounter(&counter));
   // check iotask are canceled
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      TEST(iotask[i]->iolist_next == 0);
      TEST(iotask[i]->err == ECANCELED);
      TEST(iotask[i]->state == iostate_CANCELED);
      TEST(iotask[i]->readycount  == ((i&1) ? 0 : &counter));
   }

   // reset0
   TEST(0 == free_itccounter(&counter));

   return 0;
ONERR:
   free_itccounter(&counter);
   return EINVAL;
}

static int test_query(void)
{
   iolist_t iolist = iolist_INIT;

   // TEST size_iolist
   for (size_t size = 1;; size <<= 1) {
      // test
      iolist.size = size;
      TEST(size == size_iolist(&iolist));
      // check iolist not changed
      TEST(0 == iolist.lock);
      TEST(size == iolist.size);
      TEST(0 == iolist.last);

      if (!size) break;
   }

   return 0;
ONERR:
   return EINVAL;
}

typedef struct {
   iolist_t* iolist;
   iotask_t* iot;
   thread_t* thread;
   int       state; // out param
} thread_param_t;

static int thread_callinsert(thread_param_t* param)
{
   write_atomicint(&param->state, 1);
   resume_thread(param->thread);
   insertlast_iolist(param->iolist, 1, &param->iot, param->thread);
   write_atomicint(&param->state, 2);
   return 0;
}

static int thread_callremove(thread_param_t* param)
{
   write_atomicint(&param->state, 1);
   resume_thread(param->thread);
   int err = tryremovefirst_iolist(param->iolist, &param->iot);
   if (!err) write_atomicint(&param->state, 2);
   return 0;
}

static int thread_callcancel(thread_param_t* param)
{
   resume_thread(param->thread);
   write_atomicint(&param->state, 1);
   cancelall_iolist(param->iolist);
   write_atomicint(&param->state, 2);
   return 0;
}

static int test_update(void)
{
   iolist_t  iolist = iolist_INIT;
   iotask_t  iotask_buffer[255];
   iotask_t* iotask[lengthof(iotask_buffer)];
   iotask_t* iot = 0;
   iotask_t  zero;
   thread_t* thread = 0;
   thread_param_t param = { .iolist = &iolist, .iot = 0, .thread = self_thread(), .state = 0 };
   itccounter_t counter = itccounter_FREE;

   // prepare0
   TEST(0 == init_itccounter(&counter));
   memset(iotask_buffer, 0, sizeof(iotask_buffer));
   memset(&zero, 0, sizeof(zero));
   zero.state = iostate_QUEUED;
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      iotask[i] = &iotask_buffer[i];
      iotask[i]->readycount = (i&1) ? 0 : &counter;
   }

   for (unsigned nrtask = 1; nrtask <= lengthof(iotask); nrtask <<= 1, ++nrtask) {
      // TEST insertlast_iolist: iolist.last == 0
      trysuspend_thread();
      TEST(EAGAIN == trysuspend_thread());
      TEST(0 == iolist.last);
      insertlast_iolist(&iolist, (uint8_t)nrtask, iotask, self_thread());
      // check iolist
      TEST(iolist.lock == 0);
      TEST(iolist.size == nrtask);
      TEST(iolist.last == iotask[nrtask-1]);
      // check iotask[..]
      for (unsigned i = 0; i < nrtask; ++i) {
         TEST(iotask[i]->iolist_next == iotask[(i+1)%nrtask]);
         TEST(iotask[i]->state == iostate_QUEUED);
         zero.iolist_next = iotask[(i+1)%nrtask];
         zero.readycount = (i&1) ? 0 : &counter;
         TEST(0 == memcmp(iotask[i], &zero, sizeof(zero)));
      }
      // check resume called
      TEST(0 == trysuspend_thread());

      // TEST tryremovefirst_iolist: all nrtask
      for (unsigned i = 0; i < nrtask; ++i) {
         TEST(0 == tryremovefirst_iolist(&iolist, &iot));
         // check iot
         TEST(iot == iotask[i]);
         // check iolist
         TEST(iolist.lock == 0);
         TEST(iolist.size == nrtask-1-i);
         TEST(iolist.last == (i+1==nrtask?0:iotask[nrtask-1]));
         // check iotask[i]
         TEST(iotask[i]->iolist_next == 0);
         TEST(iotask[i]->state == iostate_QUEUED);
         zero.iolist_next = 0;
         zero.readycount = (i&1) ? 0 : &counter;
         TEST(0 == memcmp(iotask[i], &zero, sizeof(zero)));
         // check no resume
         TEST(EAGAIN == trysuspend_thread());
         // reset
         iotask[i]->state = iostate_NULL;
      }
   }

   for (unsigned nrtask = 1; nrtask <= lengthof(iotask); nrtask <<= 1, ++nrtask) {
      // TEST insertlast_iolist: parameter thread == 0
      insertlast_iolist(&iolist, (uint8_t)nrtask, iotask, 0/*thread == 0*/);
      // check iolist
      TEST(iolist.lock == 0);
      TEST(iolist.size == nrtask);
      TEST(iolist.last == iotask[nrtask-1]);
      // check iotask[..]
      for (unsigned i = 0; i < nrtask; ++i) {
         TEST(iotask[i]->iolist_next == iotask[(i+1)%nrtask]);
         TEST(iotask[i]->state == iostate_QUEUED);
         zero.iolist_next = iotask[(i+1)%nrtask];
         zero.readycount = (i&1) ? 0 : &counter;
         TEST(0 == memcmp(iotask[i], &zero, sizeof(zero)));
      }
      // check no resume called
      TEST(EAGAIN == trysuspend_thread());
      // reset
      iolist.size = 0;
      iolist.last = 0;
      for (unsigned i = 0; i < nrtask; ++i) {
         iotask[i]->iolist_next = 0;
         iotask[i]->state = iostate_NULL;
      }
   }

   // TEST insertlast_iolist: iolist.size != 0
   for (unsigned nrtask = 1; nrtask <= lengthof(iotask); nrtask <<= 1, ++nrtask) {
      // prepare
      insertlast_iolist(&iolist, (uint8_t)nrtask, iotask, self_thread());
      TEST(0 == trysuspend_thread());
      // test
      insertlast_iolist(&iolist, (uint8_t)(lengthof(iotask)-nrtask), &iotask[nrtask], self_thread());
      // check iolist
      TEST(iolist.lock == 0);
      TEST(iolist.size == lengthof(iotask));
      TEST(iolist.last == iotask[lengthof(iotask)-1]);
      // check iotask[..]
      for (unsigned i = 0; i < lengthof(iotask); ++i) {
         TEST(iotask[i]->iolist_next == iotask[(i+1)%lengthof(iotask)]);
         TEST(iotask[i]->state == iostate_QUEUED);
         zero.iolist_next = iotask[(i+1)%lengthof(iotask)];
         zero.readycount = (i&1) ? 0 : &counter;
         TEST(0 == memcmp(iotask[i], &zero, sizeof(zero)));
      }
      // check no resume
      TEST(EAGAIN == trysuspend_thread());
      // reset
      iolist.size = 0;
      iolist.last = 0;
      for (unsigned i = 0; i < lengthof(iotask); ++i) {
         iotask[i]->iolist_next = 0;
         iotask[i]->state = iostate_NULL;
      }
   }

   // TEST insertlast_iolist: lock is acquired
   // prepare
   TEST(0 == set_atomicflag(&iolist.lock)); // acquire lock
   param.iot = iotask[0];
   // test
   TEST(0 == newgeneric_thread(&thread, &thread_callinsert, &param));
   suspend_thread();
   // check state / thread is waiting in lock
   sleepms_thread(1);
   TEST(1 == read_atomicint(&param.state));
   TEST(0 == iolist.size);
   // check thread continues if lock is cleared
   clear_atomicflag(&iolist.lock);
   suspend_thread();
   TEST(0 == join_thread(thread));
   TEST(2 == read_atomicint(&param.state));
   // check iolist
   TEST(0 == iolist.lock);
   TEST(1 == iolist.size);
   TEST(iotask[0] == iolist.last);
   // reset
   param.state = 0;
   TEST(0 == delete_thread(&thread));

   // TEST tryremovefirst_iolist: lock is acquired
   // prepare
   TEST(0 == set_atomicflag(&iolist.lock)); // acquire lock
   param.iot = 0;
   // test
   TEST(0 == newgeneric_thread(&thread, &thread_callremove, &param));
   suspend_thread();
   // check state / thread is waiting in lock
   sleepms_thread(1);
   TEST(1 == read_atomicint(&param.state));
   TEST(1 == iolist.size);
   // check thread continues if lock is cleared
   clear_atomicflag(&iolist.lock);
   TEST(0 == join_thread(thread));
   TEST(2 == read_atomicint(&param.state));
   TEST(iotask[0] == param.iot);
   // check iolist
   TEST(0 == iolist.lock);
   TEST(0 == iolist.size);
   TEST(0 == iolist.last);
   // reset
   param.state = 0;
   TEST(0 == delete_thread(&thread));

   // TEST tryremovefirst_iolist: ENODATA
   iot = (void*)1;
   TEST(ENODATA == tryremovefirst_iolist(&iolist, &iot));
   TEST(iot == (void*)1);
   TEST(iolist.lock == 0);
   TEST(iolist.size == 0);
   TEST(iolist.last == 0);

   // TEST cancelall_iolist: empty list
   cancelall_iolist(&iolist);
   TEST(iolist.lock == 0);
   TEST(iolist.size == 0);
   TEST(iolist.last == 0);

   // TEST cancelall_iolist: full list
   // prepare
   insertlast_iolist(&iolist, lengthof(iotask), iotask, 0);
   cancelall_iolist(&iolist);
   // check iolist
   TEST(0 == iolist.lock);
   TEST(0 == iolist.size);
   TEST(0 == iolist.last);
   // check counter (only half of iotask have counter != 0)
   TEST((lengthof(iotask)+1)/2 == reset_itccounter(&counter));
   // check iotask / state and err changed
   zero.iolist_next = 0;
   zero.state = 0;
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      TEST(iotask[i]->iolist_next == 0);
      TEST(iotask[i]->err   == ECANCELED);
      TEST(iotask[i]->state == iostate_CANCELED);
      iotask[i]->state = 0; // reset
      iotask[i]->err   = 0; // reset
      // other fields unchanged
      zero.readycount = (i&1) ? 0 : &counter;
      TEST(0 == memcmp(iotask[i], &zero, sizeof(zero)));
   }

   // TEST cancelall_iolist: lock is acquired
   // prepare
   insertlast_iolist(&iolist, lengthof(iotask), iotask, 0);
   TEST(0 == set_atomicflag(&iolist.lock)); // acquire lock
   // test
   TEST(0 == newgeneric_thread(&thread, &thread_callcancel, &param));
   suspend_thread();
   // check state / thread is waiting in lock
   sleepms_thread(1);
   TEST(1 == read_atomicint(&param.state));
   TEST(lengthof(iotask) == iolist.size);
   // check thread continues if lock is cleared
   clear_atomicflag(&iolist.lock);
   TEST(0 == join_thread(thread));
   TEST(2 == read_atomicint(&param.state));
   // check iolist
   TEST(0 == iolist.lock);
   TEST(0 == iolist.size);
   TEST(0 == iolist.last);
   // check counter
   TEST((lengthof(iotask)+1)/2 == reset_itccounter(&counter));
   // check iotask / state and err changed
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      TEST(iotask[i]->iolist_next == 0);
      TEST(iotask[i]->err   == ECANCELED);
      TEST(iotask[i]->state == iostate_CANCELED);
      iotask[i]->state = 0; // reset
      iotask[i]->err   = 0; // reset
      // other fields unchanged
      zero.readycount = (i&1) ? 0 : &counter;
      TEST(0 == memcmp(iotask[i], &zero, sizeof(zero)));
   }
   // reset
   param.state = 0;
   TEST(0 == delete_thread(&thread));

   // reset0
   TEST(0 == free_itccounter(&counter));

   return 0;
ONERR:
   free_itccounter(&counter);
   clear_atomicflag(&iolist.lock);
   delete_thread(&thread);
   return EINVAL;
}

int unittest_io_subsys_iolist()
{
   if (test_enums())    goto ONERR; // iostate_e + ioop_e
   if (test_iotask())   goto ONERR; // iotask_t
   if (test_initfree()) goto ONERR; // iolist_t
   if (test_query())    goto ONERR; // iolist_t
   if (test_update())   goto ONERR; // iolist_t

   return 0;
ONERR:
   return EINVAL;
}

#endif
