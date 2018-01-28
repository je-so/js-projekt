/* title: IOThread impl

   Implements <IOThread>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/io/iosys/iothread.h
    Header file <IOThread>.

   file: C-kern/io/iosys/iothread.c
    Implementation file <IOThread impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/iosys/iothread.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/platform/sync/eventcount.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/wbuffer.h"
#endif


// section: iothread_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_iothread_errtimer
 * Simuliert Fehler in <init_iothread> und <free_iothread>. */
static test_errortimer_t s_iothread_errtimer = test_errortimer_FREE;
static size_t            s_iothread_errtimer_count = 0;
#endif

// group: runtime-helper

static inline size_t SIZE(size_t size, size_t off) \
{
         size_t size2 = size - off;
         #ifdef KONFIG_UNITTEST
         int err;
         if (process_testerrortimer(&s_iothread_errtimer, &err)) {
            ++ s_iothread_errtimer_count;
            init_testerrortimer(&s_iothread_errtimer, 1, err);
            if (size2 > size/32) size2 = size/32;
         }
         #endif
         return size2;
}

static int ioop_worker_thread(iothread_t* iothr)
{
   int err;

   do {
      suspend_thread();
   } while (0 == read_atomicint(&iothr->thread));

   while (! read_atomicint(&iothr->request_stop)) {
      iostate_e state;
      iotask_t* iot;

      if (tryremovefirst_iolist(&iothr->iolist, &iot)) {
         suspend_thread(); /*err == ENODATA // (no other error possible)*/
         continue; // retry remove but check for request_stop
      }

      // process iot

      if (PROCESS_testerrortimer(&s_iothread_errtimer, &err)) {
         iothr->request_stop = 1; // test cancel of current iot
      }

      if (! isvalid_iotask(iot)) {
         iot->err = EINVAL;
         state = iostate_ERROR;

      } else if (iothr->request_stop/*no atomic op needed cause full mem barrier at end of loop*/) {
         iot->err = ECANCELED;
         state = iostate_CANCELED;

      } else {
         err = 0;
         size_t off = 0;

         switch ((ioop_e)iot->op) {
         case ioop_NOOP:
            break;

         case ioop_READ:
            for (;;) {
               ssize_t bytes;
               if (iot->offset < 0) {
                  bytes = read(iot->ioc, (uint8_t*)iot->bufaddr+off,
                           SIZE(iot->bufsize,off));
               } else {
                  bytes = pread(iot->ioc, (uint8_t*)iot->bufaddr+off,
                           SIZE(iot->bufsize,off), iot->offset+(off_t)off);
               }

               if (bytes < 0) {
                  err = errno;
                  if (EAGAIN != EWOULDBLOCK && EWOULDBLOCK == err) err = EAGAIN; // only EAGAIN

               } else {
                  off += (size_t) bytes;
                  if (off != iot->bufsize && bytes) continue;
               }

               break;
            }
            break;

         case ioop_WRITE:
            for (;;) {
               ssize_t bytes;
               if (iot->offset < 0) {
                  bytes = write(iot->ioc, (uint8_t*)iot->bufaddr+off,
                           SIZE(iot->bufsize,off));
               } else {
                  bytes = pwrite(iot->ioc, (uint8_t*)iot->bufaddr+off,
                           SIZE(iot->bufsize,off), iot->offset+(off_t)off);
               }

               if (bytes < 0) {
                  err = errno;
                  if (EAGAIN != EWOULDBLOCK && EWOULDBLOCK == err) err = EAGAIN; // only EAGAIN

               } else {
                  off += (size_t) bytes;
                  if (off != iot->bufsize && bytes) continue;
               }

               break;
            }
            break;
         }

         if (off || !err) {
            iot->bytesrw = off;
            state = iostate_OK;
         } else {
            iot->err = err;
            state = iostate_ERROR;
         }

      }

      write_atomicint(&iot->state, state); // assume release or write memory barrier
      if (iot->readycount) count_eventcount(iot->readycount);
   }

   return 0;
}

// group: lifetime

int init_iothread(/*out*/iothread_t* iothr)
{
   int err;
   thread_t* old_thread = iothr->thread;
   thread_t* thread;

   // DESIGN-TASK:::
   // TODO: implement thread pool / one thread per io device !
   // - move thread into thread pool
   // - remove old_thread and iothr->thread = 0 assignment ! after thread pool creation
   // - use iothread pool
   iothr->thread = 0;
   if (! PROCESS_testerrortimer(&s_iothread_errtimer, &err)) {
      err = newgeneric_thread(&thread, &ioop_worker_thread, iothr);
   }
   if (err) goto ONERR;

   // set out

   iothr->request_stop = 0;
   init_iolist(&iothr->iolist);
   write_atomicint(&iothr->thread, thread);

   // start worker
   resume_thread(thread);

   return 0;
ONERR:
   iothr->thread = old_thread;
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_iothread(iothread_t* iothr)
{
   int err;

   if (iothr->thread) {
      requeststop_iothread(iothr);
      (void) join_thread(iothr->thread);

      cancelall_iolist(&iothr->iolist);

      err = delete_thread(&iothr->thread);
      (void) PROCESS_testerrortimer(&s_iothread_errtimer, &err);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: update

void requeststop_iothread(iothread_t* iothr)
{
   write_atomicint(&iothr->request_stop, 1);
   resume_thread(iothr->thread);
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int compare_iotask( volatile iotask_t* iotask,
   int err, size_t bytesrw, iostate_e state,
   ioop_e op, iochannel_t ioc, off_t off,
   void* bufaddr, size_t bufsize, eventcount_t* counter)
{
   TEST(iotask->iolist_next == 0);
   if (err) {
   TEST(iotask->err        == err);
   } else {
   TESTP(iotask->bytesrw   == bytesrw, "expect:%zu read:%zu", bytesrw, iotask->bytesrw);
   }
   TEST(iotask->state      == state);
   TEST(iotask->op         == op);
   TEST(iotask->ioc        == ioc);
   TEST(iotask->offset     == off);
   TEST(iotask->bufaddr    == bufaddr);
   TEST(iotask->bufsize    == bufsize);
   TEST(iotask->readycount == counter);

   return 0;
ONERR:
   return EINVAL;
}

static int test_helper(void)
{
   // TEST SIZE: calc
   for (unsigned size = 0; size < 100*1024*1024; ++size, size *= 4) {
      for (unsigned off = 0; off <= size; ++off) {
         if (off > 10) off += (size - off) / 32;
         TEST(size-off == SIZE(size, off));
      }
   }

   // TEST SIZE: testcase ==> returns size/32
   init_testerrortimer(&s_iothread_errtimer, 1, 2);
   for (unsigned size = 0; size < 100*1024*1024; ++size, size *= 4) {
      for (unsigned off = 0; off <= size; ++off) {
         if (off > 10) off += (size - off) / 32;
         unsigned expect = size/32;
         if (size-off < expect)  {
            expect = size-off;
         }
         TEST(expect == SIZE(size, off));
         /*reinit*/
         TEST(2 == s_iothread_errtimer.errcode);
         TEST(1 == s_iothread_errtimer.timercount);
      }
   }
   free_testerrortimer(&s_iothread_errtimer);

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   iothread_t iothr  = iothread_FREE;
   iothread_t iothr2;
   iotask_t   iot_buffer[5];
   iotask_t*  iot[lengthof(iot_buffer)];

   // prepare
   memset(iot_buffer, 0, sizeof(iot_buffer));
   for (unsigned i = 0; i < lengthof(iot); ++i) {
      iot[i] = &iot_buffer[i];
   }

   // TEST iothread_FREE
   TEST(0 == iothr.thread);
   TEST(0 == iothr.request_stop);
   TEST(0 == iothr.iolist.lock);
   TEST(0 == iothr.iolist.size);
   TEST(0 == iothr.iolist.last);

   // TEST init_iothread
   memset(&iothr, 255, sizeof(iothr));
   iothr.thread = 0;
   TEST(0 == init_iothread(&iothr));
   // check iothr
   TEST(0 != iothr.thread);
   TEST(0 == iothr.request_stop);
   TEST(0 == iothr.iolist.lock);
   TEST(0 == iothr.iolist.size);
   TEST(0 == iothr.iolist.last);

   // TEST free_iothread: empty iolist
   TEST(0 == free_iothread(&iothr));
   TEST(0 == iothr.thread);
   TEST(1 == iothr.request_stop);
   TEST(0 == iothr.iolist.lock);
   TEST(0 == iothr.iolist.size);
   TEST(0 == iothr.iolist.last);

   // TEST free_iothread: cancel iolist
   TEST(0 == init_iothread(&iothr));
   insertlast_iolist(&iothr.iolist, lengthof(iot), iot, 0);
   // test
   TEST(0 == free_iothread(&iothr));
   // check iothr
   TEST(0 == iothr.thread);
   TEST(1 == iothr.request_stop);
   TEST(0 == iothr.iolist.lock);
   TEST(0 == iothr.iolist.size);
   TEST(0 == iothr.iolist.last);
   // check iot canceled
   for (unsigned i = 0; i < lengthof(iot); ++i) {
      TEST(iot[i]->err   == ECANCELED);
      TEST(iot[i]->state == iostate_CANCELED);
      // reset
      iot[i]->err   = 0;
      iot[i]->state = iostate_NULL;
   }

   // TEST init_iothread: simulated ERROR
   memset(&iothr, 255, sizeof(iothr));
   memset(&iothr2, 255, sizeof(iothr2));
   for (int t = 1; t <= 1; ++t) {
      init_testerrortimer(&s_iothread_errtimer, (unsigned)t, t);
      TEST(t == init_iothread(&iothr));
      // check iothr not changed
      TEST(0 == memcmp(&iothr, &iothr2, sizeof(iothr)));
   }

   // TEST free_iothread: simulated ERROR
   for (int t = 1; t <= 1; ++t) {
      // prepare
      TEST(0 == init_iothread(&iothr));
      insertlast_iolist(&iothr.iolist, lengthof(iot), iot, 0);
      // test
      init_testerrortimer(&s_iothread_errtimer, (unsigned)t, t);
      TEST(t == free_iothread(&iothr));
      // check iothr
      TEST(0 == iothr.thread);
      TEST(1 == iothr.request_stop);
      TEST(0 == iothr.iolist.lock);
      TEST(0 == iothr.iolist.size);
      TEST(0 == iothr.iolist.last);
      // check iot canceled
      for (unsigned i = 0; i < lengthof(iot); ++i) {
         TEST(iot[i]->err   == ECANCELED);
         TEST(iot[i]->state == iostate_CANCELED);
         // reset
         iot[i]->err   = 0;
         iot[i]->state = iostate_NULL;
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_noop(void)
{
   iothread_t  iothr = iothread_FREE;
   iotask_t    iotask_buffer[255];
   iotask_t*   iotask[lengthof(iotask_buffer)];
   eventcount_t counter = eventcount_FREE;
   uint8_t     buffer[1];

   // prepare0
   TEST(0 == init_iothread(&iothr));
   init_eventcount(&counter);
   memset(iotask_buffer, 0, sizeof(iotask_buffer));
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      iotask[i] = &iotask_buffer[i];
   }

   // TEST insertiotask_iothread: noop
   for (unsigned nrio = 1; nrio <= lengthof(iotask); ++nrio) {
      // prepare
      for (unsigned i = 0; i < nrio; ++i) {
         initwrite_iotask(iotask[i], file_STDERR, 1, buffer, &counter);
         iotask[i]->op = ioop_NOOP;
         iotask[i]->bytesrw = 1; /*!= 0*/
      }
      // test
      insertiotask_iothread(&iothr, (uint8_t) nrio, &iotask[0]);
      // check
      for (size_t i = 0; i < nrio; ++i) {
         // wait for ready counter
         wait_eventcount(&counter,0);
         // check iotask
         TEST(0 == compare_iotask(iotask[i], 0, 0, iostate_OK, ioop_NOOP, file_STDERR, -1, buffer, 1, &counter));
      }
   }

   // reset0
   TEST(0 == free_iothread(&iothr));
   TEST(0 == free_eventcount(&counter));

   return 0;
ONERR:
   free_iothread(&iothr);
   free_eventcount(&counter);
   return EINVAL;
}

static int test_read(directory_t* tmpdir)
{
   iothread_t  iothr = iothread_FREE;
   file_t      file = file_FREE;
   memblock_t  mblock[10] = { memblock_FREE };
   iotask_t    iotask_buffer[lengthof(mblock)];
   iotask_t*   iotask[lengthof(mblock)];
   eventcount_t counter = eventcount_INIT;

   // prepare0
   TEST(0 == init_iothread(&iothr));
   memset(iotask_buffer, 0, sizeof(iotask_buffer));
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      iotask[i] = &iotask_buffer[i];
   }
   for (unsigned i = 0; i < lengthof(mblock); ++i) {
      TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &mblock[i]));
      memset(mblock[i].addr, 0, mblock[i].size);
   }
   // create test file
   TEST(0 == initcreate_file(&file, "testread", tmpdir));
   for (size_t i = 0, val = 0; i < lengthof(mblock); ++i) {
      for (size_t off = 0; off < mblock[i].size/sizeof(uint32_t); ++off, ++val) {
         ((uint32_t*)mblock[0].addr)[off] = (uint32_t) val;
      }
      TEST((int)mblock[0].size == write(io_file(file), mblock[0].addr, mblock[0].size));
   }
   memset(mblock[0].addr, 0, mblock[0].size);
   TEST(0 == free_iochannel(&file));

   // TEST insertiotask_iothread: readp (forward)
   static_assert((lengthof(iotask)-1) % 3 == 0, "full range 1..lengthof(iotask)");
   for (unsigned nrio = 1; nrio <= lengthof(iotask); nrio += 3) {
      // prepare
      TEST(0 == init_file(&file, "testread", accessmode_READ, tmpdir));
      for (unsigned i = 0; i < nrio; ++i) {
         initreadp_iotask(iotask[i], io_file(file), mblock[i].size, mblock[i].addr, (off_t) (i*mblock[0].size), &counter);
      }
      // test
      insertiotask_iothread(&iothr, (uint8_t) nrio, &iotask[0]);
      // check
      for (size_t i = 0; i < nrio; ++i) {
         // wait for ready counter
         while (iostate_QUEUED == read_atomicint(&iotask[i]->state)) {
            wait_eventcount(&counter,0);
         }
         // check iotask
         TEST(0 == compare_iotask(iotask[i], 0, mblock[i].size, iostate_OK, ioop_READ, io_file(file),
                     (off_t) (i*mblock[0].size), mblock[i].addr, mblock[i].size, &counter));
         // check read content of mblock
         for (size_t vi = 0, val = (size_t)iotask[i]->offset/sizeof(uint32_t); vi < mblock[i].size/sizeof(uint32_t); ++vi, ++val) {
            TEST(val == ((uint32_t*)mblock[i].addr)[vi]);
         }
         // reset
         iotask[i]->bytesrw = 0;
         memset(mblock[i].addr, 0, mblock[i].size);
      }
      // check file position (offset) not changed
      TEST(0 == lseek(io_file(file), 0, SEEK_CUR));
      // reset
      TEST(0 == free_iochannel(&file));
      counter.nrevents = 0;
   }

   // TEST insertiotask_iothread: readp (backward)
   static_assert((lengthof(iotask)-1) % 3 == 0, "full range 1..lengthof(iotask)");
   for (unsigned nrio = 1; nrio <= lengthof(iotask); nrio += 3) {
      // prepare
      TEST(0 == init_file(&file, "testread", accessmode_READ, tmpdir));
      for (unsigned i = 0; i < nrio; ++i) {
         initreadp_iotask(iotask[i], io_file(file), mblock[i].size, mblock[i].addr, (off_t)((nrio-1-i)*mblock[0].size), &counter);
      }
      // test
      insertiotask_iothread(&iothr, (uint8_t) nrio, &iotask[0]);
      // check
      for (unsigned i = 0; i < nrio; ++i) {
         // wait for ready counter
         while (iostate_QUEUED == read_atomicint(&iotask[i]->state)) {
            wait_eventcount(&counter,0);
         }
         // check iotask
         TEST(0 == compare_iotask(iotask[i], 0, mblock[i].size, iostate_OK, ioop_READ, io_file(file),
                     (off_t) ((nrio-1-i)*mblock[0].size), mblock[i].addr, mblock[i].size, &counter));
         // check read content of mblock
         for (size_t vi = 0, val = (size_t)iotask[i]->offset/sizeof(uint32_t); vi < mblock[i].size/sizeof(uint32_t); ++vi, ++val) {
            TEST(val == ((uint32_t*)mblock[i].addr)[vi]);
         }
         // reset
         iotask[i]->bytesrw = 0;
         memset(mblock[i].addr, 0, mblock[i].size);
      }
      // check file position (offset) not changed
      TEST(0 == lseek(io_file(file), 0, SEEK_CUR));
      // reset
      TEST(0 == free_iochannel(&file));
      counter.nrevents = 0;
   }

   // TEST insertiotask_iothread: read
   // prepare
   TEST(0 == init_file(&file, "testread", accessmode_READ, tmpdir));
   for (size_t i = 0; i < lengthof(iotask); ++i) {
      initread_iotask(iotask[i], io_file(file), mblock[i].size, mblock[i].addr, 0);
   }
   // test
   insertiotask_iothread(&iothr, lengthof(iotask), &iotask[0]);
   // check
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      // wait for ready counter
      for (int w = 0; w < 500000; ++w) {
         if (iostate_QUEUED != read_atomicint(&iotask[i]->state)) break;
         yield_thread();
      }
      // check iotask
      TEST(0 == compare_iotask(iotask[i], 0, mblock[i].size, iostate_OK, ioop_READ, io_file(file),
                  -1, mblock[i].addr, mblock[i].size, 0));
      // check read content of mblock
      for (size_t vi = 0, val = (i*mblock[0].size)/sizeof(uint32_t); vi < mblock[i].size/sizeof(uint32_t); ++vi, ++val) {
         TEST(val == ((uint32_t*)mblock[i].addr)[vi]);
      }
      // reset
      iotask[i]->bytesrw = 0;
      memset(mblock[i].addr, 0, mblock[i].size);
   }
   // check file position (offset) changed
   TEST((off_t) (lengthof(mblock)*mblock[0].size) == lseek(io_file(file), 0, SEEK_CUR));
   // reset
   TEST(0 == free_iochannel(&file));

   // reset0
   TEST(0 == free_iothread(&iothr));
   TEST(0 == free_eventcount(&counter));
   TEST(0 == free_iochannel(&file));
   TEST(0 == removefile_directory(tmpdir, "testread"));
   for (unsigned i = 0; i < lengthof(mblock); ++i) {
      TEST(0 == RELEASE_PAGECACHE(&mblock[i]));
   }

   return 0;
ONERR:
   free_iothread(&iothr);
   free_eventcount(&counter);
   free_iochannel(&file);
   for (unsigned i = 0; i < lengthof(mblock); ++i) {
      RELEASE_PAGECACHE(&mblock[i]);
   }
   removefile_directory(tmpdir, "testread");
   return EINVAL;
}

static int test_write(directory_t* tmpdir)
{
   iothread_t  iothr = iothread_FREE;
   file_t      file = file_FREE;
   memblock_t  mblock[10] = { memblock_FREE };
   memblock_t  readbuf = memblock_FREE;
   iotask_t    iotask_buffer[lengthof(mblock)];
   iotask_t*   iotask[lengthof(mblock)];
   eventcount_t counter = eventcount_INIT;

   // prepare0
   TEST(0 == init_iothread(&iothr));
   memset(iotask_buffer, 0, sizeof(iotask_buffer));
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      iotask[i] = &iotask_buffer[i];
   }
   // fill write buffer
   for (size_t i = 0, val = 0; i < lengthof(mblock); ++i) {
      TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &mblock[i]));
      for (size_t off = 0; off < mblock[i].size/sizeof(uint32_t); ++off, ++val) {
         ((uint32_t*)mblock[i].addr)[off] = (uint32_t) val;
      }
   }
   // alloc read buffer
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &readbuf));
   memset(readbuf.addr, 0, readbuf.size);

   // TEST insertiotask_iothread: writep (forward)
   static_assert((lengthof(iotask)-1) % 3 == 0, "full range 1..lengthof(iotask)");
   for (unsigned nrio = 1; nrio <= lengthof(iotask); nrio += 3) {
      // prepare
      TEST(0 == initcreate_file(&file, "testwrite", tmpdir));
      for (size_t i = 0; i < nrio; ++i) {
         initwritep_iotask(iotask[i], io_file(file), mblock[i].size, mblock[i].addr, (off_t) (i*mblock[0].size), &counter);
      }
      // test
      insertiotask_iothread(&iothr, (uint8_t) nrio, &iotask[0]);
      // check
      for (unsigned i = 0; i < nrio; ++i) {
         // wait for ready counter
         while (iostate_QUEUED == read_atomicint(&iotask[i]->state)) {
            wait_eventcount(&counter,0);
         }
         // check iotask
         TEST(0 == compare_iotask(iotask[i], 0, mblock[i].size, iostate_OK, ioop_WRITE, io_file(file),
                     (off_t) (i*mblock[0].size), mblock[i].addr, mblock[i].size, &counter));
         // check written bytes
         TEST((int)readbuf.size == pread(io_file(file), readbuf.addr, readbuf.size, iotask[i]->offset));
         for (size_t vi = 0, val = (size_t)iotask[i]->offset/sizeof(uint32_t); vi < readbuf.size/sizeof(uint32_t); ++vi, ++val) {
            TEST(val == ((uint32_t*)readbuf.addr)[vi]);
         }
         // reset
         iotask[i]->bytesrw = 0;
      }
      // check file position (offset) not changed
      TEST(0 == lseek(io_file(file), 0, SEEK_CUR));
      // reset
      TEST(0 == free_iochannel(&file));
      TEST(0 == removefile_directory(tmpdir, "testwrite"));
      counter.nrevents = 0;
   }

   // TEST insertiotask_iothread: writep (backward)
   static_assert((lengthof(iotask)-1) % 3 == 0, "full range 1..lengthof(iotask)");
   for (unsigned nrio = 1; nrio <= lengthof(iotask); nrio += 3) {
      // prepare
      TEST(0 == initcreate_file(&file, "testwrite", tmpdir));
      for (unsigned i = 0; i < nrio; ++i) {
         initwritep_iotask(iotask[i], io_file(file), mblock[i].size, mblock[i].addr, (off_t)((nrio-1-i)*mblock[0].size), &counter);
      }
      // test
      insertiotask_iothread(&iothr, (uint8_t) nrio, &iotask[0]);
      // check
      for (unsigned i = 0; i < nrio; ++i) {
         // wait for ready counter
         while (iostate_QUEUED == read_atomicint(&iotask[i]->state)) {
            wait_eventcount(&counter,0);
         }
         // check iotask
         TEST(0 == compare_iotask(iotask[i], 0, mblock[i].size, iostate_OK, ioop_WRITE, io_file(file),
                     (off_t) ((nrio-1-i)*mblock[0].size), mblock[i].addr, mblock[i].size, &counter));
         // check written bytes
         TEST((int)readbuf.size == pread(io_file(file), readbuf.addr, readbuf.size, iotask[i]->offset));
         for (size_t vi = 0, val = i*mblock[0].size/sizeof(uint32_t); vi < readbuf.size/sizeof(uint32_t); ++vi, ++val) {
            TEST(val == ((uint32_t*)readbuf.addr)[vi]);
         }
         // reset
         iotask[i]->bytesrw = 0;
      }
      // check file position (offset) not changed
      TEST(0 == lseek(io_file(file), 0, SEEK_CUR));
      // reset
      TEST(0 == free_iochannel(&file));
      TEST(0 == removefile_directory(tmpdir, "testwrite"));
      counter.nrevents = 0;
   }

   // TEST insertiotask_iothread: write
   // prepare
   TEST(0 == initcreate_file(&file, "testwrite", tmpdir));
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      initwrite_iotask(iotask[i], io_file(file), mblock[i].size, mblock[i].addr, 0);
   }
   // test
   insertiotask_iothread(&iothr, lengthof(iotask), &iotask[0]);
   // check
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      // wait for ready counter
      for (int w = 0; w < 500000; ++w) {
         if (iostate_QUEUED != read_atomicint(&iotask[i]->state)) break;
         yield_thread();
      }
      // check iotask
      TEST(0 == compare_iotask(iotask[i], 0, mblock[i].size, iostate_OK, ioop_WRITE, io_file(file),
                  -1, mblock[i].addr, mblock[i].size, 0));
      // check written bytes
      TEST((int)readbuf.size == pread(io_file(file), readbuf.addr, readbuf.size, (off_t)(i*mblock[0].size)));
      for (size_t vi = 0, val = (i*mblock[0].size)/sizeof(uint32_t); vi < readbuf.size/sizeof(uint32_t); ++vi, ++val) {
         TEST(val == ((uint32_t*)readbuf.addr)[vi]);
      }
      // reset
      iotask[i]->bytesrw = 0;
   }
   // check file position (offset) changed
   TEST((off_t)(lengthof(mblock)*mblock[0].size) == lseek(io_file(file), 0, SEEK_CUR));

   // reset
   TEST(0 == free_iochannel(&file));
   TEST(0 == removefile_directory(tmpdir, "testwrite"));

   // reset0
   TEST(0 == free_iothread(&iothr));
   TEST(0 == free_eventcount(&counter));
   for (unsigned i = 0; i < lengthof(mblock); ++i) {
      TEST(0 == RELEASE_PAGECACHE(&mblock[i]));
   }
   TEST(0 == RELEASE_PAGECACHE(&readbuf));

   return 0;
ONERR:
   free_iothread(&iothr);
   free_eventcount(&counter);
   free_iochannel(&file);
   for (unsigned i = 0; i < lengthof(mblock); ++i) {
      RELEASE_PAGECACHE(&mblock[i]);
   }
   RELEASE_PAGECACHE(&readbuf);
   removefile_directory(tmpdir, "testwrite");
   return EINVAL;
}

static int test_rwerror(void)
{
   iothread_t  iothr = iothread_FREE;
   uint8_t     buffer[10];
   iotask_t    iotask_buffer[18];
   iotask_t*   iotask[lengthof(iotask_buffer)];
   eventcount_t counter = eventcount_INIT;

   // prepare
   TEST(0 == init_iothread(&iothr));
   memset(iotask_buffer, 0, sizeof(iotask_buffer));
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      iotask[i] = &iotask_buffer[i];
   }

   // TEST insertiotask_iothread: EBADF
   for (unsigned nrio = 1; nrio <= lengthof(iotask); ++nrio) {
      // prepare
      for (unsigned  i = 0; i < nrio; ++i) {
         switch (i%2) {
         case 0: initwrite_iotask(iotask[i], -1, sizeof(buffer), buffer, &counter); break;
         case 1: initread_iotask(iotask[i], -1, sizeof(buffer), buffer, &counter); break;
         }
      }
      // test
      insertiotask_iothread(&iothr, (uint8_t) nrio, &iotask[0]);
      // check
      for (size_t i = 0; i < nrio; ++i) {
         // wait for ready counter
         wait_eventcount(&counter,0);
         // check iotask
         TEST(0 == compare_iotask(iotask[i], EBADF, 0, iostate_ERROR, (i%2) ? ioop_READ : ioop_WRITE, -1,
                     -1, buffer, sizeof(buffer), &counter));
         // reset
         iotask[i]->bytesrw = 0;
      }
   }

   // TEST insertiotask_iothread: ! isvalid_iotask
   for (unsigned nrio = 1; nrio <= lengthof(iotask); ++nrio) {
      // prepare
      for (unsigned i = 0; i < nrio; ++i) {
         switch (i%3) {
         case 0: initwrite_iotask(iotask[i], file_STDOUT, sizeof(buffer), 0, &counter);
                 break;
         case 1: initread_iotask(iotask[i], file_STDOUT, 0, buffer, &counter);
                 break;
         case 2: initread_iotask(iotask[i], file_STDOUT, sizeof(buffer), buffer, &counter);
                 iotask[i]->op = ioop__NROF;
                 break;
         }
      }
      // test
      insertiotask_iothread(&iothr, (uint8_t) nrio, &iotask[0]);
      // check
      for (size_t i = 0; i < nrio; ++i) {
         // wait for ready counter
         wait_eventcount(&counter,0);
         // check iotask
         TEST(0 == compare_iotask(iotask[i], EINVAL, 0, iostate_ERROR, (i%3) == 2 ? ioop__NROF : (i%3) == 1 ? ioop_READ : ioop_WRITE, file_STDOUT,
                     -1, (i%3) == 0 ? 0 : buffer, (i%3) == 1 ? 0 : sizeof(buffer), &counter));
         // reset
         iotask[i]->bytesrw = 0;
      }
   }

   // TEST insertiotask_iothread: request_stop = 1
   // prepare
   for (unsigned i = 0; i < lengthof(iotask); ++i) {
      initwrite_iotask(iotask[i], file_STDOUT, 1, buffer, &counter);
   }
   // simulates request_stop=1
   init_testerrortimer(&s_iothread_errtimer, 1, 1);
   // test
   insertiotask_iothread(&iothr, lengthof(iotask), &iotask[0]);
   wait_eventcount(&counter,0);
   // check iolist
   TEST(iothr.iolist.last != 0);
   TEST(iothr.iolist.size == lengthof(iotask)-1);
   // check iotask
   TEST(0 == compare_iotask(iotask[0], ECANCELED, 0, iostate_CANCELED, ioop_WRITE, file_STDOUT,
               -1, buffer, 1, &counter));
   for (unsigned i = 1; i < lengthof(iotask); ++i) {
      TEST(iotask[i]->iolist_next != 0);
      iotask[i]->iolist_next = 0;
      TEST(0 == compare_iotask(iotask[i], 0, 0, iostate_QUEUED, ioop_WRITE, file_STDOUT,
               -1, buffer, 1, &counter));
      // reset
      iotask[i]->bytesrw = 0;
   }
   // check iothread
   TEST(1 == iothr.request_stop);
   // reset
   iothr.iolist.last = 0;
   iothr.iolist.size = 0;

   // reset
   TEST(0 == free_iothread(&iothr));
   TEST(0 == free_eventcount(&counter));

   return 0;
ONERR:
   free_iothread(&iothr);
   free_eventcount(&counter);
   return EINVAL;
}

static int test_rwpartial(directory_t* tmpdir)
{
   iothread_t  iothr = iothread_FREE;
   file_t      file = file_FREE;
   memblock_t  writebuf = memblock_FREE;
   memblock_t  readbuf  = memblock_FREE;
   iotask_t    iotask_buffer;
   iotask_t*   iotask = &iotask_buffer;
   eventcount_t counter = eventcount_INIT;

   // prepare0
   TEST(0 == init_iothread(&iothr));
   // alloc buffer
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &readbuf));
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &writebuf));
   for (size_t val = 0; val < writebuf.size/sizeof(uint32_t); ++val) {
      ((uint32_t*)writebuf.addr)[val] = (uint32_t) val;
   }
   memset(readbuf.addr, 0, readbuf.size);

   for (int ispos = 0; ispos <= 1; ++ispos) {

      // TEST insertiotask_iothread: (writep,write) && syscall writes less than writebuf.size
      TEST(0 == initcreate_file(&file, "testpartial", tmpdir))
      if (ispos) {
         initwritep_iotask(iotask, io_file(file), writebuf.size, writebuf.addr, 0, &counter);
      } else {
         initwrite_iotask(iotask, io_file(file), writebuf.size, writebuf.addr, &counter);
      }
      // test
      s_iothread_errtimer_count = 0;
      init_testerrortimer(&s_iothread_errtimer, 2/*trigger partial io*/, 1);
      insertiotask_iothread(&iothr, 1, &iotask);
      // wait for writer ready
      wait_eventcount(&counter,0);
      // check iotask
      TEST(iotask->iolist_next == 0);
      TEST(iotask->bytesrw    == writebuf.size);
      TEST(iotask->state      == iostate_OK);
      TEST(iotask->op         == ioop_WRITE);
      TEST(iotask->ioc        == io_file(file));
      TEST(iotask->offset     == (ispos ? 0 : -1));
      TEST(iotask->bufaddr    == writebuf.addr);
      TEST(iotask->bufsize    == writebuf.size);
      TEST(iotask->readycount == &counter);
      // check 32 partial writes (1+32)
      TEST(32 == s_iothread_errtimer_count);
      // reset
      TEST(0 == free_file(&file));
      free_testerrortimer(&s_iothread_errtimer);

      // TEST insertiotask_iothread: (readp, read) && syscall reads less than writebuf.size
      TEST(0 == init_file(&file, "testpartial", accessmode_READ, tmpdir));
      if (ispos) {
         initreadp_iotask(iotask, io_file(file), readbuf.size, readbuf.addr, 0, &counter);
      } else {
         initread_iotask(iotask, io_file(file), readbuf.size, readbuf.addr, &counter);
      }
      // test
      s_iothread_errtimer_count = 0;
      init_testerrortimer(&s_iothread_errtimer, 2/*trigger partial io*/, 1);
      insertiotask_iothread(&iothr, 1, &iotask);
      // wait for reader ready
      wait_eventcount(&counter,0);
      // check iotask.ioop[0]
      TEST(iotask->iolist_next == 0);
      TEST(iotask->bytesrw    == writebuf.size);
      TEST(iotask->state      == iostate_OK);
      TEST(iotask->op         == ioop_READ);
      TEST(iotask->ioc        == io_file(file));
      TEST(iotask->offset     == (ispos ? 0 : -1));
      TEST(iotask->bufaddr    == readbuf.addr);
      TEST(iotask->bufsize    == readbuf.size);
      TEST(iotask->readycount == &counter);
      // check content of readbuf
      for (size_t val = 0; val < readbuf.size/sizeof(uint32_t); ++val) {
         TEST(val == ((uint32_t*)readbuf.addr)[val]);
      }
      // check 32 partial writes (1+32)
      TEST(32 == s_iothread_errtimer_count);
      // reset
      memset(readbuf.addr, 0, readbuf.size);
      TEST(0 == free_file(&file));
      TEST(0 == removefile_directory(tmpdir, "testpartial"));
      free_testerrortimer(&s_iothread_errtimer);
   }

   // reset
   TEST(0 == free_iothread(&iothr));
   TEST(0 == free_eventcount(&counter));
   TEST(0 == free_iochannel(&file));
   TEST(0 == RELEASE_PAGECACHE(&writebuf));
   TEST(0 == RELEASE_PAGECACHE(&readbuf));

   return 0;
ONERR:
   free_iothread(&iothr);
   free_eventcount(&counter);
   free_iochannel(&file);
   RELEASE_PAGECACHE(&writebuf);
   RELEASE_PAGECACHE(&readbuf);
   removefile_directory(tmpdir, "testpartial");
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_FREE;
   directory_t* dir = 0;
   uint8_t      dirpath[256];

   if (test_initfree())    goto ONERR;

   TEST(0 == init_resourceusage(&usage));

   // prepare
   TEST(0 == newtemp_directory(&dir, "iothread", &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(dirpath), dirpath)));

   if (test_helper())      goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_noop())        goto ONERR;
   if (test_read(dir))     goto ONERR;
   if (test_write(dir))    goto ONERR;
   if (test_rwerror())     goto ONERR;
   if (test_rwpartial(dir)) goto ONERR;

   // reset
   TEST(0 == delete_directory(&dir));
   TEST(0 == removedirectory_directory(0, (const char*)dirpath));

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   if (dir) {
      delete_directory(&dir);
      removedirectory_directory(0, (const char*)dirpath);
   }
   free_resourceusage(&usage);
   return EINVAL;
}

int unittest_io_iosys_iothread()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
