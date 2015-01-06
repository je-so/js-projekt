/* title: SyncRunner impl

   Implements <SyncRunner>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncrunner.h
    Header file <SyncRunner>.

   file: C-kern/task/syncrunner.c
    Implementation file <SyncRunner impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncrunner.h"
#include "C-kern/api/task/synccond.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/inmem/queue.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif
#ifdef KONFIG_PERFTEST
#include "C-kern/api/test/perftest.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/memory/memblock.h"
#endif


// section: syncrunner_t

// group: constants

/* define: RUNQ_OPTFLAGS
 * The value of <syncfunc_t.optflags>. */
#define RUNQ_OPTFLAGS syncfunc_opt_NONE

/* define: RUNQ_ELEMSIZE
 * The size of elements stored in <syncrunner_t.rwqueue>[RUNQ_ID]. */
#define RUNQ_ELEMSIZE (getsize_syncfunc(RUNQ_OPTFLAGS))

/* define: RUNQ_ID
 * The index into <syncrunner_t.rwqueue> and <s_syncrunner_qsize> for the single run queue. */
#define RUNQ_ID 0

/* define: WAITQ_OPTFLAGS
 * The value of <syncfunc_t.optflags>. */
#define WAITQ_OPTFLAGS syncfunc_opt_WAITFIELDS

/* define: WAITQ_ELEMSIZE
 * The size of elements stored in <syncrunner_t.rwqueue>[WAITQ_ID]. */
#define WAITQ_ELEMSIZE (getsize_syncfunc(WAITQ_OPTFLAGS))

/* define: WAITQ_ID
 * The index into <syncrunner_t.rwqueue> and <s_syncrunner_qsize> for the single run queue. */
#define WAITQ_ID 1

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_syncrunner_errtimer
 * Simulate errors in <free_syncrunner>. */
static test_errortimer_t s_syncrunner_errtimer = test_errortimer_FREE;
#endif

/* variable: s_syncrunner_qsize
 * Die Größe der Elemente in der run und der wait queue. */
static uint8_t s_syncrunner_qsize[2] = {
   [RUNQ_ID] = RUNQ_ELEMSIZE,
   [WAITQ_ID] = WAITQ_ELEMSIZE
};

// group: lifetime

int init_syncrunner(/*out*/syncrunner_t * srun)
{
   int err;
   unsigned qidx;

   static_assert( lengthof(s_syncrunner_qsize) == lengthof(srun->rwqueue),
                  "every queue has its own size");

   static_assert( RUNQ_ID == 0 && WAITQ_ID == 1 && lengthof(srun->rwqueue),
                  "all queues have an assigned id");

   for (qidx = 0; qidx < lengthof(srun->rwqueue); ++qidx) {
      err = init_queue(&srun->rwqueue[qidx], defaultpagesize_queue());
      if (err) goto ONERR;
   }

   initself_linkd(&srun->wakeup);
   memset(srun->freecache, 0, sizeof(syncrunner_t) - offsetof(syncrunner_t, freecache));

   return 0;
ONERR:
   while (qidx > 0) {
      (void) free_queue(&srun->rwqueue[--qidx]);
   }
   return err;
}

int free_syncrunner(syncrunner_t* srun)
{
   int err = 0;
   int err2;

   for (unsigned i = 0; i < lengthof(srun->rwqueue); ++i) {
      err2 = free_queue(&srun->rwqueue[i]);
      SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err2);
      if (err2) err = err2;
      srun->rwqsize[i] = 0;
      srun->freecache[i] = 0;
   }

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: queue-helper

/* function: fillcache_syncrunner
 * Falls freecache leer ist, werden nodesize Bytes an Speicher für eine syncfunc_t in der Queue mit ID queueud allokiert.
 *
 * Postcondition:
 * o !undefined! == ((syncfunc_t*)srun->freecache[queueid])->mainfct
 *
 * Unchecked Precondition:
 * o queueid == RUNQ_ID || queueid == WAITQ_ID */
static inline int fillcache_syncrunner(syncrunner_t* srun, unsigned queueid)
{
   return srun->freecache[queueid]
         ? 0
         : insertlast_queue(&srun->rwqueue[queueid], queueid == RUNQ_ID ? RUNQ_ELEMSIZE : WAITQ_ELEMSIZE, &srun->freecache[queueid]);
}

/* function: alloccachedfunc_syncrunner
 * Falls freecache leer ist, werden nodesize Bytes an Speicher für eine syncfunc_t in der Queue mit ID queueud allokiert.
 * Unchecked Precondition:
 * o queueid == RUNQ_ID || queueid == WAITQ_ID
 * o 0 != srun->freecache[queueid] */
static inline void alloccachedfunc_syncrunner(syncrunner_t* srun, unsigned queueid, /*out*/syncfunc_t** sfunc)
{
   *sfunc = srun->freecache[queueid];
   srun->freecache[queueid] = 0;
   ++ srun->rwqsize[queueid];
}

/* function: allocfunc_syncrunner
 * Allokiert nodesize Bytes an Speicher für eine syncfunc_t in der Queue mit ID queueud.
 * Unchecked Precondition:
 * o queueid == RUNQ_ID || queueid == WAITQ_ID */
static inline int allocfunc_syncrunner(syncrunner_t* srun, unsigned queueid, /*out*/syncfunc_t** sfunc)
{
   int err;

   err = fillcache_syncrunner(srun, queueid);
   if (err) goto ONERR;

   alloccachedfunc_syncrunner(srun, queueid, sfunc);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: removefunc_syncrunner
 * Lösche sfunc aus squeue.
 * Entweder wird das letzte Element in squeue nach sfunc kopiert
 * oder, wenn das letzte Element das preallokierte Element ist,
 * einfach sfunc als neues freies Element markiert.
 * Das letzte Element wird dann aus der Queue gelöscht.
 *
 * Unchecked Precondition:
 * o sfunc != srun->freecache[queueid] // do not delete freecache entry !!
 * o Links are invalid;
 *   (sfunc->waitlist.prev == 0 && sfunc->waitlist.next == 0).
 * o sfunc ∈ queue / (squeue == castPaddr_syncqueue(sfunc))
 * */
static inline int removefunc_syncrunner(syncrunner_t* srun, unsigned queueid, syncfunc_t* sfunc)
{
   int err;
   uint16_t nodesize = queueid == RUNQ_ID ? RUNQ_ELEMSIZE : WAITQ_ELEMSIZE;
   queue_t*    queue = &srun->rwqueue[queueid];
   syncfunc_t* last  = last_queue(queue, nodesize);

   if (!last) {
      // should never happen cause at least sfunc is stored in squeue
      err = ENODATA;
      goto ONERR;
   }

   sfunc->mainfct = 0;

   if (! srun->freecache[queueid]) {
      srun->freecache[queueid] = sfunc;

   } else {
      if (sfunc != last) {
         if (last == srun->freecache[queueid]) {
            srun->freecache[queueid] = sfunc;
         } else {
            initmove_syncfunc(sfunc, last);
            last->mainfct = 0; // in case of remove error crash
         }
      }

      err = removelast_queue(queue, nodesize);
      if (err) goto ONERR;
   }

   -- srun->rwqsize[queueid];

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: clearqueue_syncrunner
 * Speicher der Queue wird freigegeben.
 *
 * Unchecked Precondition:
 * o All contained functions must be in a unlinked/free state. */
static inline int clearqueue_syncrunner(syncrunner_t* srun, unsigned queueid)
{
   srun->freecache[queueid] = 0;
   srun->rwqsize[queueid] = 0;

   int err = removeall_queue(&srun->rwqueue[queueid]);

   return err;
}

// group: query

bool iswakeup_syncrunner(const syncrunner_t* srun)
{
   return ! isself_linkd(&srun->wakeup);
}

size_t size_syncrunner(const syncrunner_t* srun)
{
   size_t size = srun->rwqsize[0];

   for (unsigned i = 1; i < lengthof(srun->rwqueue); ++i) {
      size += srun->rwqsize[i];
   }

   return size;
}

// group: update

int addfunc_syncrunner(syncrunner_t* srun, syncfunc_f mainfct, void* state)
{
   int err;
   syncfunc_t* sf;

   ONERROR_testerrortimer(&s_syncrunner_errtimer, &err, ONERR);

   err = allocfunc_syncrunner(srun, RUNQ_ID, &sf);
   if (err) goto ONERR;

   init_syncfunc(sf, mainfct, state, RUNQ_OPTFLAGS);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: link_to_wakeup
 * Füge einzelnes waitlist von <syncfunc_t> zu <syncrunner_t.wakeup> hinzu. */
static inline void link_to_wakeup(syncrunner_t * srun, linkd_t * waitlist)
{
   initprev_linkd(waitlist, &srun->wakeup);
}

/* function: linkall_to_wakeup
 * Füge waitlist von <syncfunc_t> und alle, auf die es zeigt, zu <syncrunner_t.wakeup> hinzu. */
static inline void linkall_to_wakeup(syncrunner_t * srun, linkd_t * waitlist)
{
   splice_linkd(waitlist, &srun->wakeup);
}

/* function: wakeup2_syncrunner
 * Implementiert <wakeup_syncrunner> und <wakeupall_syncrunner>.
 * Parameter isall == true bestimmt, daß alle wartenden Funktionen aufgeweckt werden sollen. */
static inline int wakeup2_syncrunner(syncrunner_t * srun, synccond_t * scond, const bool isall)
{
   if (! iswaiting_synccond(scond)) return 0;

   syncfunc_t*  wakeupfunc = waitfunc_synccond(scond);
   linkd_t*     waitlist = waitlist_syncfunc(wakeupfunc);

   if (isall) {
      unlinkall_synccond(scond);
      linkall_to_wakeup(srun, waitlist);

   } else {
      unlink_synccond(scond);
      link_to_wakeup(srun, waitlist);
   }

   return 0;
}

int wakeup_syncrunner(syncrunner_t * srun, struct synccond_t * scond)
{
   int err;

   err = wakeup2_syncrunner(srun, scond, false);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int wakeupall_syncrunner(syncrunner_t * srun, struct synccond_t * scond)
{
   int err;

   err = wakeup2_syncrunner(srun, scond, true);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: execute

/* define: RUN_SYNCFUNC
 * <syncfunc_t.mainfct> wird mit Kommando <synccmd_RUN> aufgerufen.
 *
 * Parameter:
 * param - Name von lokalem <syncfunc_param_t>. Wird mehrfach ausgewertet !!
 *
 * Unchecked Precondition:
 * o param.srun points to a valid <syncrunner_t>.
 * o param.sfunc points to a valid <syncfunc_t>. */
#define RUN_SYNCFUNC(param) \
         ( __extension__ ({                            \
            param.sfunc->mainfct(&param, synccmd_RUN); \
         }))

/* define: EXIT_SYNCFUNC
 * Ruft syncfunc_t* sf auf mit Kommando synccmd_EXIT.
 * param.err wird vor dem Aufruf auf ECANCELED gesetzt.
 *
 * Parameter:
 * param - Name von lokalem <syncfunc_param_t>. Wird mehrfach ausgewertet !!
 *
 * Unchecked Precondition:
 * o param.srun points to a valid <syncrunner_t>.
 * o param.sfunc points to a valid <syncfunc_t>. */
#define EXIT_SYNCFUNC(param) \
         ( __extension__ ({                             \
            param.err = ECANCELED;                      \
            param.sfunc->mainfct(&param, synccmd_EXIT); \
         }))

/* function: link_waitfields
 * Initialisiere <syncfunc_t.waitlist> und <syncfunc_t.waitresult>.
 *
 * Unchecked Precondition:
 * o sfunc has optional waitresult/waitlist fields
 */
static inline void link_waitfields(syncrunner_t* srun, syncfunc_t* sfunc, syncfunc_param_t* param)
{
   if (! param->condition) goto ONERR;

   setwaitresult_syncfunc(sfunc, 0);
   link_synccond(param->condition, sfunc);

   return;
ONERR:
   // use syncfunc_param_t.err to transport error
   setwaitresult_syncfunc(sfunc, EINVAL);
   link_to_wakeup(srun, waitlist_syncfunc(sfunc));
}

/* function: process_waitq_syncrunner
 * Starte alle in <syncrunner_t.wakeup> gelisteten <syncfunc_t>.
 * Der Rückgabewert vom Typ <synccmd_e> entscheidet, ob die Funktion weiterhin
 * in der Wartequeue verbleibt oder in die Runqueue verschoben wird.
 *
 * <syncfunc_param_t.err> = <syncfunc_t.waitresult>.
 *
 * */
static inline int process_waitq_syncrunner(syncrunner_t * srun)
{
   int err;
   int cmd;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   linkd_t     wakeup;

   if (srun->isrun) return EINPROGRESS;

   if (isself_linkd(&srun->wakeup)) return 0;

   // prepare
   srun->isrun = true;

   // this ensures that newly woken up syncfunc_t are not executed this time

   wakeup = srun->wakeup;
   relink_linkd(&wakeup);
   initself_linkd(&srun->wakeup);

   // preallocate enough resources

   err = fillcache_syncrunner(srun, RUNQ_ID);
   if (err) goto ONERR;

   // loop over all entries in moved wakeup list

   while (wakeup.next != &wakeup) {
      param.sfunc = castPwaitlist_syncfunc(wakeup.next);
      unlink_linkd(wakeup.next);

      param.err = waitresult_syncfunc(param.sfunc);
      cmd = RUN_SYNCFUNC(param);

      if (synccmd_WAIT == cmd) {
         link_waitfields(srun, param.sfunc, &param);
         continue;
      }

      if (synccmd_EXIT != cmd) {
         // synccmd_RUN or invalid value: move from wait to run queue

         syncfunc_t* sfunc2;
         alloccachedfunc_syncrunner(srun, RUNQ_ID, &sfunc2);
         initcopy_syncfunc(sfunc2, param.sfunc, RUNQ_OPTFLAGS);
         ONERROR_testerrortimer(&s_syncrunner_errtimer, &err, ONERR);
         err = fillcache_syncrunner(srun, RUNQ_ID);
         if (err) goto ONERR;
      }

      err = removefunc_syncrunner(srun, WAITQ_ID, param.sfunc);
      SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
      if (err) goto ONERR;
   }

ONUNPREPARE:
   srun->isrun = false;
   return err;
ONERR:
   if (!isself_linkd(&wakeup)) {
      splice_linkd(&srun->wakeup, &wakeup);
      unlink_linkd(&wakeup);
   }
   TRACEEXIT_ERRLOG(err);
   goto ONUNPREPARE;
}

int process_runq_syncrunner(syncrunner_t* srun)
{
   int err;
   int cmd;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   queue_iterator_t iter = queue_iterator_FREE;

   if (srun->isrun) return EINPROGRESS;

   // prepare
   srun->isrun = true;

   err = fillcache_syncrunner(srun, WAITQ_ID);
   if (err) goto ONERR;

   queue_t* rqueue = &srun->rwqueue[RUNQ_ID];

   // run every entry in run queue once
   // new entries are added to end of queue ==> do not run them during this invocation

   err = initlast_queueiterator(&iter, rqueue, RUNQ_ELEMSIZE);
   if (err) {
      if (err != ENODATA) goto ONERR;
      err = 0;

   } else  {
      // TODO: speed up process_runq_syncrunner loop
      // WHY? Even without RUN_SYNCFUNC only 40000ops/msec <==> raw 60000ops/msec

      void* prev;
      bool  isPrev = prev_queueiterator(&iter, &prev);
      while (isPrev) {
         param.sfunc = prev;
         isPrev = prev_queueiterator(&iter, &prev); // makes calling removefunc_syncrunner( rqueue ) safe

         if (param.sfunc == srun->freecache[RUNQ_ID]) continue;

         cmd = RUN_SYNCFUNC(param);

         if (synccmd_RUN == cmd) continue;

         if (synccmd_WAIT == cmd) {
            // move from run into wait queue

            syncfunc_t* sfunc2;
            alloccachedfunc_syncrunner(srun, WAITQ_ID, &sfunc2);
            initcopy_syncfunc(sfunc2, param.sfunc, WAITQ_OPTFLAGS);
            // waitresult & waitlist are undefined ==> set it !
            link_waitfields(srun, sfunc2, &param);
            ONERROR_testerrortimer(&s_syncrunner_errtimer, &err, ONERR);
            err = fillcache_syncrunner(srun, WAITQ_ID);
            if (err) goto ONERR;

         } else if (synccmd_EXIT != cmd) {
            // invalid value same as synccmd_RUN: do nothing
            continue;
         }

         err = removefunc_syncrunner(srun, RUNQ_ID, param.sfunc);
         SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
         if (err) goto ONERR;
      }

      err = free_queueiterator(&iter);
      if (err) goto ONERR;
   }

ONUNPREPARE:
   srun->isrun = false;
   return err;
ONERR:
   free_queueiterator(&iter);
   TRACEEXIT_ERRLOG(err);
   goto ONUNPREPARE;
}

int run_syncrunner(syncrunner_t* srun)
{
   int err;

   err = process_runq_syncrunner(srun);
   if (err) goto ONERR;

   err = process_waitq_syncrunner(srun);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int terminate_syncrunner(syncrunner_t* srun)
{
   int err;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   queue_iterator_t iter = queue_iterator_FREE;

   if (srun->isrun) return EINPROGRESS;

   // prepare
   srun->isrun = true;

   for (unsigned qidx = lengthof(srun->rwqueue); qidx > 0; ) {
      queue_t* queue = &srun->rwqueue[--qidx];
      uint16_t size  = s_syncrunner_qsize[qidx];

      err = initlast_queueiterator(&iter, queue, size);
      if (err) {
         if (err != ENODATA) goto ONERR;
         err = 0;
         continue;
      }

      void* prev;
      while (prev_queueiterator(&iter, &prev)) {
         if (prev == srun->freecache[qidx]) continue;

         param.sfunc = prev;

         unlink_syncfunc(param.sfunc);

         EXIT_SYNCFUNC(param);
      }

      err = free_queueiterator(&iter);

      int err2 = clearqueue_syncrunner(srun, qidx);
      if (err2) err = err2;

      SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
      if (err) goto ONERR;
   }

ONUNPREPARE:
   initself_linkd(&srun->wakeup);
   srun->isrun = false;
   return err;

ONERR:
   free_queueiterator(&iter);
   TRACEEXIT_ERRLOG(err);
   goto ONUNPREPARE;
}



// section: Functions

// group: test

#ifdef KONFIG_PERFTEST

typedef struct state_t {
   unsigned count;
   void*    msg;
} state_t;

static int syncfunc_client(syncfunc_param_t* param, uint32_t cmd) __attribute__((noinline));
static int syncfunc_client(syncfunc_param_t* param, uint32_t cmd)
{
   if (cmd == synccmd_RUN) {
      state_t* state = state_syncfunc(param);
      if (state->msg) {
         state->msg = 0; // processed !
      }
   }
   return 0;
}

static int syncfunc_server(syncfunc_param_t* param, uint32_t cmd) __attribute__((noinline));
static int syncfunc_server(syncfunc_param_t* param, uint32_t cmd)
{
   if (cmd == synccmd_RUN) {
      state_t* state = state_syncfunc(param);
      if (!state->msg) {
         state->msg = (void*) (++state->count); // generate new msg
      }
   }
   return 0;
}

static int pt_prepare(perftest_instance_t* tinst)
{
   int err;

   memblock_t mblock;

   err = ALLOC_MM(sizeof(state_t), &mblock);
   if (err) return err;

   memset(mblock.addr, 0, mblock.size);

   err = addfunc_syncrunner(syncrunner_maincontext(), &syncfunc_client, mblock.addr);
   if (err) return err;

   err = addfunc_syncrunner(syncrunner_maincontext(), &syncfunc_server, mblock.addr);
   if (err) return err;

   tinst->nrops = 1000000;
   tinst->addr  = mblock.addr;
   tinst->size  = mblock.size;

   err = fillcache_syncrunner(syncrunner_maincontext(), WAITQ_ID);
   if (err) return err;

   return 0;
}

static int pt_unprepare(perftest_instance_t* tinst)
{
   memblock_t mblock = memblock_INIT(tinst->size, tinst->addr);
   int err = FREE_MM(&mblock);
   int err2 = terminate_syncrunner(syncrunner_maincontext());
   if (err2) err = err2;
   return err;
}

static int pt_run(perftest_instance_t* tinst)
{
   state_t* state = tinst->addr;
   // int count = 0;
   while (state->count < tinst->nrops) {
      int err = run_syncrunner(syncrunner_maincontext());
      if (err) return err;
      // ++count;
      // if (count == tinst->nrops) break;
   }
   return 0;
}

static int pt_run_raw(perftest_instance_t* tinst)
{
   state_t* state = tinst->addr;
   syncfunc_t sfunc = syncfunc_FREE;
   sfunc.state = state;
   syncfunc_param_t param = syncfunc_param_FREE;
   param.sfunc = &sfunc;
   while (state->count < tinst->nrops) {
      syncfunc_server(&param, synccmd_RUN);
      syncfunc_client(&param, synccmd_RUN);
   }
   return 0;
}

int perftest_task_syncrunner(/*out*/perftest_info_t* info)
{
   *info = (perftest_info_t) perftest_info_INIT(
               perftest_INIT(&pt_prepare, &pt_run, &pt_unprepare),
               "Sending and receiving a message",
               0, 0, 0
            );

   return 0;
}

int perftest_task_syncrunner_raw(/*out*/perftest_info_t* info)
{
   *info = (perftest_info_t) perftest_info_INIT(
               perftest_INIT(&pt_prepare, &pt_run_raw, &pt_unprepare),
               "Sending and receiving a message",
               0, 0, 0
            );

   return 0;
}

#endif


#ifdef KONFIG_UNITTEST

// test helper

int check_queue_size(syncrunner_t* srun, unsigned size, unsigned qid)
{
   TEST(qid == RUNQ_ID || qid == WAITQ_ID);
   unsigned elemsize = qid == RUNQ_ID ? RUNQ_ELEMSIZE : WAITQ_ELEMSIZE;
   unsigned bytes = (size + (srun->freecache[qid] != 0)) * elemsize;
   TEST(size == srun->rwqsize[qid]);
   TEST(bytes == sizebytes_queue(&srun->rwqueue[qid]));

   return 0;
ONERR:
   return EINVAL;
}

static int dummy_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   (void) sfparam;
   (void) sfcmd;
   return synccmd_EXIT;
}

static int test_constants(void)
{
   // TEST RUNQ_ID, WAITQ_ID
   TEST(0 == RUNQ_ID);
   TEST(1 == WAITQ_ID);
   TEST(WAITQ_ID < lengthof(((syncrunner_t*)0)->rwqueue));

   // TEST RUNQ_ELEMSIZE, WAITQ_ELEMSIZE
   TEST(RUNQ_ELEMSIZE  <  WAITQ_ELEMSIZE);
   TEST(RUNQ_ELEMSIZE  == offsetof(syncfunc_t, waitresult));
   TEST(WAITQ_ELEMSIZE == sizeof(syncfunc_t));

   // TEST RUNQ_OPTFLAGS, WAITQ_OPTFLAGS
   TEST(RUNQ_OPTFLAGS  == syncfunc_opt_NONE);
   TEST(WAITQ_OPTFLAGS == syncfunc_opt_WAITFIELDS);

   return 0;
ONERR:
   return EINVAL;
}

static int test_staticvars(void)
{
   TEST(2 == lengthof(s_syncrunner_qsize));
   TEST(RUNQ_ELEMSIZE  != WAITQ_ELEMSIZE);
   TEST(RUNQ_ELEMSIZE  == s_syncrunner_qsize[0]);
   TEST(WAITQ_ELEMSIZE == s_syncrunner_qsize[1]);

   return 0;
ONERR:
   return EINVAL;
}

static int test_memory(void)
{
   unsigned long src[sizeof(syncfunc_t) / sizeof(long)];
   unsigned long dest[sizeof(syncfunc_t) / sizeof(long)];
   linkd_t waitlist  = linkd_FREE;

   static_assert(sizeof(s_syncrunner_qsize) == 2, "only two kind of optflags");

   static_assert( RUNQ_ELEMSIZE < sizeof(syncfunc_t)
                  && WAITQ_ELEMSIZE == sizeof(syncfunc_t), "src+dest has enough free space");

   static_assert( 0 == (RUNQ_ELEMSIZE % sizeof(long))
                  && 0 == (WAITQ_ELEMSIZE % sizeof(long)), "size multiple of sizeof(long)");

   // TEST initmove_syncfunc: RUNQ_OPTFLAGS
   memset(dest, 0, sizeof(dest));
   for (unsigned i = 0; i < RUNQ_ELEMSIZE; ++i) {
      ((uint8_t*)src)[i] = (uint8_t) (i+1);
   }
   ((syncfunc_t*)src)->optflags = RUNQ_OPTFLAGS;
   initmove_syncfunc((syncfunc_t*)dest, (const syncfunc_t*)src);
   // check content of dest
   TEST(0 == memcmp(dest, src, RUNQ_ELEMSIZE));
   for (unsigned i = RUNQ_ELEMSIZE; i < sizeof(dest); ++i) {
      TEST(((uint8_t*)dest)[i] == 0);
   }

   // TEST initmove_syncfunc: WAITQ_OPTFLAGS
   memset(dest, 0, sizeof(dest));
   for (unsigned i = 0; i < WAITQ_ELEMSIZE; ++i) {
      ((uint8_t*)src)[i] = (uint8_t) (i+1);
   }
   ((syncfunc_t*)src)->optflags = WAITQ_OPTFLAGS;
   init_linkd(&waitlist, &((syncfunc_t*)src)->waitlist);
   initmove_syncfunc((syncfunc_t*)dest, (const syncfunc_t*)src);
   // check link adapted
   TEST(waitlist.prev == &((syncfunc_t*)dest)->waitlist);
   TEST(waitlist.next == &((syncfunc_t*)dest)->waitlist);
   // check content equal
   TEST(0 == memcmp(dest, src, WAITQ_ELEMSIZE));

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   syncrunner_t srun = syncrunner_FREE;

   // TEST syncrunner_FREE
   TEST(0 == isvalid_linkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(isfree_queue(&srun.rwqueue[i]));
      TEST(0 == srun.rwqsize[i]);
   }
   TEST(0 == srun.isrun);

   // TEST init_syncrunner
   memset(&srun, 255, sizeof(srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(srun.wakeup.prev == &srun.wakeup);
   TEST(srun.wakeup.next == &srun.wakeup);
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == check_queue_size(&srun, 0, i));
      TEST(defaultpagesize_queue() == pagesize_queue(&srun.rwqueue[i]));
      TEST(0 == srun.freecache[i]);
   }
   TEST(0 == srun.isrun);

   // TEST free_syncrunner: free queues
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      syncfunc_t* sf;
      TEST(0 == allocfunc_syncrunner(&srun, i, &sf));
      TEST(0 == check_queue_size(&srun, 1, i));
      srun.freecache[i] = (void*)1;
   }
   TEST(0 == free_syncrunner(&srun));
   // check queues are freed (only)
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == check_queue_size(&srun, 0, i));
      TEST(0 == srun.freecache[i]);
   }

   // TEST free_syncrunner: double free
   TEST(0 == free_syncrunner(&srun));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == check_queue_size(&srun, 0, i));
   }

   // TEST free_syncrunner: EINVAL
   for (unsigned ec = 1; ec <= lengthof(srun.rwqueue); ++ec) {
      TEST(0 == init_syncrunner(&srun));
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         syncfunc_t* sf;
         TEST(0 == allocfunc_syncrunner(&srun, i, &sf));
         TEST(0 == check_queue_size(&srun, 1, i));
         srun.freecache[i] = (void*)1;
      }
      init_testerrortimer(&s_syncrunner_errtimer, ec, EINVAL);
      TEST(EINVAL == free_syncrunner(&srun));
      // queues are freed
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         TEST(0 == check_queue_size(&srun, 0, i));
         TEST(0 == srun.freecache[i]);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_queuehelper(void)
{
   syncrunner_t srun = syncrunner_FREE;
   syncfunc_t * sfunc;

   // prepare
   TEST(0 == init_syncrunner(&srun));

   for (unsigned qidx = 0; qidx < lengthof(srun.rwqueue); ++qidx) {

      for (unsigned i = 1; i <= 10000; ++i) {
         // TEST allocfunc_syncrunner: freecache == 0
         TEST(0 == allocfunc_syncrunner(&srun, qidx, &sfunc));
         TEST(0 == check_queue_size(&srun, i, qidx));
         TEST(0 == srun.freecache[qidx]);

         // TEST allocfunc_syncrunner: freecache != 0
         syncfunc_t* old = sfunc;
         // simulate remove func (store in cache)
         srun.freecache[qidx] = sfunc;
         -- srun.rwqsize[qidx];
         sfunc = 0;
         // test
         TEST(0 == allocfunc_syncrunner(&srun, qidx, &sfunc));
         // check removed from cache
         TEST(old == sfunc);
         TEST(0 == check_queue_size(&srun, i, qidx));
         TEST(0 == srun.freecache[qidx]);
      }

      // TEST clearqueue_syncrunner
      srun.freecache[qidx] = last_queue(&srun.rwqueue[qidx], s_syncrunner_qsize[qidx]);
      TEST(0 != srun.freecache[qidx]);
      TEST(0 == clearqueue_syncrunner(&srun, qidx));
      // check result
      TEST(0 == check_queue_size(&srun, 0, qidx));
      TEST(0 == srun.freecache[qidx]);

      // TEST fillcache_syncrunner: freecache == 0
      TEST(0 == srun.freecache[qidx]);
      TEST(0 == fillcache_syncrunner(&srun, qidx));
      TEST(0 != srun.freecache[qidx]);
      TEST(0 == check_queue_size(&srun, 0, qidx));

      // TEST fillcache_syncrunner: freecache != 0
      void* F = srun.freecache[qidx];
      TEST(0 == fillcache_syncrunner(&srun, qidx));
      TEST(F == srun.freecache[qidx]);
      TEST(0 == check_queue_size(&srun, 0, qidx));

      // TEST alloccachedfunc_syncrunner: freecache != 0
      sfunc = 0;
      F = srun.freecache[qidx];
      alloccachedfunc_syncrunner(&srun, qidx, &sfunc);
      TEST(0 == srun.freecache[qidx]);
      TEST(0 == check_queue_size(&srun, 1, qidx));
      TEST(F == sfunc);

      // TEST alloccachedfunc_syncrunner: freecache == 0
      TEST(0 != sfunc);
      alloccachedfunc_syncrunner(&srun, qidx, &sfunc);
      TEST(2 == srun.rwqsize[qidx]);
      TEST(0 == srun.freecache[qidx]);
      TEST(0 == sfunc);
      -- srun.rwqsize[qidx];
      TEST(0 == check_queue_size(&srun, 1, qidx));
   }

   // reset
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));

   for (unsigned qidx = 0; qidx < lengthof(srun.rwqueue); ++qidx) {

      static_assert(lengthof(srun.rwqueue) == 2, "assumes two queues with size + optflags");
      queue_t*       const queue    = &srun.rwqueue[qidx];
      uint16_t       const size     = WAITQ_ID == qidx ? WAITQ_ELEMSIZE : RUNQ_ELEMSIZE;
      syncfunc_opt_e const optflags = WAITQ_ID == qidx ? WAITQ_OPTFLAGS : RUNQ_OPTFLAGS;
      linkd_t        waitlist = linkd_FREE;
      syncfunc_t     save;
      syncfunc_t*    last;

      // prepare
      TEST(0 == allocfunc_syncrunner(&srun, qidx, &sfunc));
      init_syncfunc(sfunc, 0, 0, (syncfunc_opt_e) (optflags+1));
      if (WAITQ_ID == qidx) {
         sfunc->waitresult = 0;
         sfunc->waitlist   = (linkd_t) linkd_FREE;
      }
      TEST(0 == allocfunc_syncrunner(&srun, qidx, &last));
      init_syncfunc(last, &dummy_sf, (void*)1, optflags);
      last->contoffset = 2;
      if (WAITQ_ID == qidx) {
         last->waitresult = 0x1234;
         init_linkd(&last->waitlist, &waitlist);
      }

      // TEST removefunc_syncrunner: freecache == 0
      for (int ti = 0; ti <= 1; ++ti) {
         syncfunc_t* sf = ti ? last : sfunc;
         memcpy(&save, sf, size);
         TEST(0 == check_queue_size(&srun, 2, qidx));
         TEST(0 == removefunc_syncrunner(&srun, qidx, sf));
         // check stored in cache + mainfct changed
         TEST(sf == srun.freecache[qidx]);
         TEST(0 == sf->mainfct);
         TEST(0 == check_queue_size(&srun, 1, qidx));
         // restore
         memcpy(sf, &save, size);
         srun.freecache[qidx] = 0;
         ++ srun.rwqsize[qidx];
         TEST(0 == check_queue_size(&srun, 2, qidx));
      }

      // TEST removefunc_syncrunner: remove not last ==> last element copied to removed
      srun.freecache[qidx] = (void*) 1; // simulate freecache != 0
      TEST(0 == removefunc_syncrunner(&srun, qidx, sfunc));
      // check last mainfct changed
      TEST(0 == last->mainfct);
      // check cache
      TEST((void*)1 == srun.freecache[qidx]);
      srun.freecache[qidx] = 0;
      // check result
      TEST(0 == check_queue_size(&srun, 1, qidx));
      TEST(sfunc == last_queue(queue, size));
      TEST(sfunc->mainfct    == &dummy_sf);
      TEST(sfunc->state      == (void*)1);
      TEST(sfunc->contoffset == 2);
      TEST(sfunc->optflags   == optflags);
      if (WAITQ_ID == qidx) {
         TEST(sfunc->waitresult == 0x1234);
         TEST(sfunc->waitlist.prev == &waitlist);
         TEST(sfunc->waitlist.next == &waitlist);
         TEST(waitlist.prev        == &sfunc->waitlist);
         TEST(waitlist.next        == &sfunc->waitlist);
      }

      // TEST removefunc_syncrunner: remove not last / last == freecache ==> no copy
      memcpy(&save, sfunc, size);
      TEST(0 == allocfunc_syncrunner(&srun, qidx, &last));
      memset(last, 255, size);
      // simulate removefunc -> add to cache
      srun.freecache[qidx] = last;
      -- srun.rwqsize[qidx];
      // test
      TEST(0 == removefunc_syncrunner(&srun, qidx, sfunc));
      // check sfunc mainfct changed
      TEST(0 == sfunc->mainfct);
      // check cache
      TEST(sfunc == srun.freecache[qidx]);
      // check result
      TEST(0 == check_queue_size(&srun, 0, qidx));
      // check content not changed
      sfunc->mainfct = save.mainfct;
      TEST(0 == memcmp(&save, sfunc, size));
      for (uint8_t * data = (uint8_t*) last; data != size + (uint8_t*)last; ++data) {
         TEST(*data == 255);
      }
      // restore
      srun.freecache[qidx] = 0;
      ++ srun.rwqsize[qidx];

      // TEST removefunc_syncrunner: queuesize > 1 && remove last element
      memcpy(&save, sfunc, size);
      TEST(0 == allocfunc_syncrunner(&srun, qidx, &last));
      TEST(0 == check_queue_size(&srun, 2, qidx));
      memset(last, 255, size);
      // test
      srun.freecache[qidx] = (void*) 1; // simulate freecache != 0
      TEST(0 == removefunc_syncrunner(&srun, qidx, last));
      // check last mainfct changed
      TEST(0 == last->mainfct);
      // check cache
      TEST((void*)1 == srun.freecache[qidx]);
      srun.freecache[qidx] = 0;
      // check result
      TEST(0 == check_queue_size(&srun, 1, qidx));
      TEST(last == (syncfunc_t*) (size + (uint8_t*)last_queue(queue, size)));
      // check other content not changed
      memset(&last->mainfct, 255, sizeof(last->mainfct));
      TEST(0 == memcmp(&save, sfunc, size));
      for (uint8_t * data = (uint8_t*) last; data != size + (uint8_t*)last; ++data) {
         TEST(*data == 255); // content not changed
      }

      // TEST removefunc_syncrunner: queuesize == 1 && last element
      srun.freecache[qidx] = (void*)1; // simulate freecache != 0
      TEST(0 == removefunc_syncrunner(&srun, qidx, sfunc));
      // check cache
      TEST((void*)1 == srun.freecache[qidx]);
      srun.freecache[qidx] = 0;
      // check result
      TEST(0 == check_queue_size(&srun, 0, qidx));

      // TEST removefunc_syncrunner: ENODATA
      TEST(ENODATA == removefunc_syncrunner(&srun, qidx, sfunc));
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_query(void)
{
   syncrunner_t srun;
   syncfunc_t   sfunc;
   syncfunc_t*  sf;

   // prepare
   TEST(0 == init_syncrunner(&srun));

   // TEST iswakeup_syncrunner: after init
   TEST(0 == iswakeup_syncrunner(&srun));

   // TEST iswakeup_syncrunner: true ==> wakeup list not empty
   link_to_wakeup(&srun, &sfunc.waitlist);
   TEST(1 == iswakeup_syncrunner(&srun));

   // TEST iswakeup_syncrunner: false ==> wakeup list empty
   unlink_linkd(&sfunc.waitlist);
   TEST(0 == iswakeup_syncrunner(&srun));

   // TEST size_syncrunner: after init
   TEST(0 == size_syncrunner(&srun));

   // TEST size_syncrunner: size of single queue
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      for (unsigned s = 1; s <= 256; ++s) {
         TEST(0 == allocfunc_syncrunner(&srun, i, &sf));
         TEST(s == size_syncrunner(&srun));
      }
      for (unsigned s = 256; (s--) > 0; ) {
         sf = last_queue(cast_queue(&srun.rwqueue[i]), s_syncrunner_qsize[i]);
         TEST(0 == removefunc_syncrunner(&srun, i, sf));
         TEST(s == size_syncrunner(&srun));
      }
      TEST(0 == free_syncrunner(&srun));
      TEST(0 == init_syncrunner(&srun));
   }

   // TEST size_syncrunner: size of all queues
   for (unsigned i = 0, S = 1; i < lengthof(srun.rwqueue); ++i) {
      for (unsigned s = 1; s <= 256; ++s, ++S) {
         TEST(0 == allocfunc_syncrunner(&srun, i, &sf));
         TEST(S == size_syncrunner(&srun));
      }
   }
   for (unsigned i = 0, S = size_syncrunner(&srun)-1; i < lengthof(srun.rwqueue); ++i) {
      for (unsigned s = 1; s <= 256; ++s, --S) {
         sf = last_queue(cast_queue(&srun.rwqueue[i]), s_syncrunner_qsize[i]);
         TEST(0 == removefunc_syncrunner(&srun, i, sf));
         TEST(S == size_syncrunner(&srun));
      }
   }
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   return EINVAL;
}

static int test_addfunc(void)
{
   syncrunner_t srun;
   syncfunc_t*  sfunc;
   syncfunc_t*  prev;

   // prepare
   TEST(0 == init_syncrunner(&srun));

   // TEST addfunc_syncrunner
   prev = 0;
   for (uintptr_t i = 1, s = 1; i; i <<= 1, ++s, prev = sfunc) {
      TEST(0 == addfunc_syncrunner(&srun, &dummy_sf, (void*)i));
      // check result
      TEST(0 == check_queue_size(&srun, s, RUNQ_ID));
      // check content of stored function
      sfunc = last_queue(cast_queue(&srun.rwqueue[RUNQ_ID]), RUNQ_ELEMSIZE);
      TEST(0 != sfunc)
      TEST(sfunc->mainfct    == &dummy_sf);
      TEST(sfunc->state      == (void*)i);
      TEST(sfunc->contoffset == 0);
      TEST(sfunc->optflags   == syncfunc_opt_NONE);
      TEST(!prev || (uint8_t*)sfunc == (uint8_t*)prev + RUNQ_ELEMSIZE);
   }

   // TEST addfunc_syncrunner: ERROR
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   init_testerrortimer(&s_syncrunner_errtimer, 1, ENOMEM);
   TEST(ENOMEM == addfunc_syncrunner(&srun, &dummy_sf, 0));
   // check result
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == check_queue_size(&srun, 0, i));
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   return EINVAL;
}

static int test_wakeup(void)
{
   syncrunner_t srun;
   syncfunc_t   sfunc[10];
   syncfunc_t*  sfunc2[10];
   synccond_t   cond = synccond_FREE;

   // prepare
   init_synccond(&cond);
   TEST(0 == init_syncrunner(&srun));
   TEST(isself_linkd(&srun.wakeup));

   // TEST link_to_wakeup
   for (unsigned i = 0; i < lengthof(sfunc); ++i) {
      link_to_wakeup(&srun, &sfunc[i].waitlist);
      TEST(sfunc[i].waitlist.prev == (i ? &sfunc[i-1].waitlist : &srun.wakeup))
      TEST(sfunc[i].waitlist.next == &srun.wakeup);
   }

   // TEST linkall_to_wakeup
   initself_linkd(&srun.wakeup);
   init_linkd(&sfunc[0].waitlist, &sfunc[1].waitlist);
   for (unsigned i = 2; i < lengthof(sfunc); ++i) {
      initnext_linkd(&sfunc[i].waitlist, &sfunc[i-1].waitlist);
   }
   linkall_to_wakeup(&srun, &sfunc[0].waitlist);
   for (unsigned i = 0; i < lengthof(sfunc); ++i) {
      TEST(sfunc[i].waitlist.prev == (i ? &sfunc[i-1].waitlist : &srun.wakeup))
      TEST(sfunc[i].waitlist.next == (i < lengthof(sfunc)-1? &sfunc[i+1].waitlist : &srun.wakeup))
   }

   // prepare
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   for (int i = 0; i < (int)lengthof(sfunc2); ++i) {
      TEST(0 == allocfunc_syncrunner(&srun, WAITQ_ID, &sfunc2[i]));
      init_syncfunc(sfunc2[i], 0, 0, WAITQ_OPTFLAGS);
      sfunc2[i]->waitresult = i;
   }

   // TEST wakeup_syncrunner: waitlist not empty / waitlist empty
   // build waiting list on cond
   for (unsigned i = 0; i < lengthof(sfunc2); ++i) {
      link_synccond(&cond, sfunc2[i]);
   }
   for (int i = 0; i < (int)lengthof(sfunc2); ++i) {
      // test
      TEST(cond.waitfunc.next == waitlist_syncfunc(sfunc2[i]));
      TEST(0 == wakeup_syncrunner(&srun, &cond));
      // check added to wakeup
      linkd_t * const prev = i > 0 ? waitlist_syncfunc(sfunc2[i-1]) : &srun.wakeup;
      linkd_t * const next = &srun.wakeup;
      TEST(prev == waitlist_syncfunc(sfunc2[i])->prev);
      TEST(next == waitlist_syncfunc(sfunc2[i])->next);
      // check waitresult not changed
      TEST(i == waitresult_syncfunc(sfunc2[i]));
      TEST(syncfunc_opt_WAITFIELDS == sfunc2[i]->optflags);
   }
   TEST(! iswaiting_synccond(&cond));

   // TEST wakeup_syncrunner: empty condition ==> call ignored
   initself_linkd(&srun.wakeup);
   TEST(0 == iswaiting_synccond(&cond));
   TEST(0 == wakeup_syncrunner(&srun, &cond));
   TEST(0 != isself_linkd(&srun.wakeup));
   TEST(0 == iswaiting_synccond(&cond));

   // TEST wakeupall_syncrunner
   // prepare (build waiting list on cond)
   for (unsigned i = 0; i < lengthof(sfunc2); ++i) {
      link_synccond(&cond, sfunc2[i]);
   }
   // test
   TEST(0 == wakeupall_syncrunner(&srun, &cond));
   // check cond clear
   TEST(0 == iswaiting_synccond(&cond));
   // check added to wakeup
   for (int i = 0; i < (int)lengthof(sfunc2); ++i) {
      linkd_t * const prev = i > 0 ? waitlist_syncfunc(sfunc2[i-1]) : &srun.wakeup;
      linkd_t * const next = i < (int)lengthof(sfunc2)-1 ? waitlist_syncfunc(sfunc2[i+1]) : &srun.wakeup;
      TEST(prev == waitlist_syncfunc(sfunc2[i])->prev);
      TEST(next == waitlist_syncfunc(sfunc2[i])->next);
      // check waitresult not changed
      TEST(i == waitresult_syncfunc(sfunc2[i]));
      TEST(syncfunc_opt_WAITFIELDS == sfunc2[i]->optflags);
   }

   // TEST wakeupall_syncrunner: empty condition ==> call ignored
   initself_linkd(&srun.wakeup);
   TEST(0 == iswaiting_synccond(&cond));
   TEST(0 == wakeupall_syncrunner(&srun, &cond));
   TEST(isself_linkd(&srun.wakeup));
   TEST(0 == iswaiting_synccond(&cond));

   // unprepare
   free_synccond(&cond);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_synccond(&cond);
   free_syncrunner(&srun);
   return EINVAL;
}

// == in params ==
static syncrunner_t     * s_test_srun   = 0;
static int                s_test_set_cmd = synccmd_RUN;
static uint16_t           s_test_set_contoffset = 0;
static int                s_test_set_retcode = 0;
static void *             s_test_set_state   = 0;
static synccond_t *       s_test_set_condition = 0;
static uint16_t           s_test_expect_contoffset = 0;
static void *             s_test_expect_state = 0;
static uint32_t           s_test_expect_cmd = 0;
static int                s_test_expect_err = 0;
static syncfunc_t**       s_test_expect_sfunc; // accessed with [s_test_runcount]
// == out params ==
static size_t             s_test_runcount = 0;
static size_t             s_test_errcount = 0;
static syncfunc_param_t * s_test_param  = 0;
static uint32_t           s_test_cmd    = 0;

static int test_call_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   assert(s_test_srun == sfparam->srun);
   s_test_cmd   = sfcmd;
   s_test_param = sfparam;
   return s_test_set_cmd;
}

static int test_exec_helper(void)
{
   syncrunner_t      srun;
   syncfunc_param_t  param = syncfunc_param_INIT(&srun);
   synccond_t        scond = synccond_FREE;

   // prepare
   s_test_srun = &srun;
   TEST(0 == init_syncrunner(&srun));
   init_synccond(&scond);

   // TEST RUN_SYNCFUNC
   for (int retcode = -2; retcode <= 2; ++retcode) {
      syncfunc_t sfunc;
      param.sfunc = &sfunc;
      param.err   = 12345;
      for (uint16_t contoffset = 0; contoffset <= 3; ++contoffset) {
         // prepare
         void* state = (void*)(uintptr_t)(10 + contoffset);
         init_syncfunc(&sfunc, &test_call_sf, state, 0);
         sfunc.contoffset = contoffset;
         s_test_cmd   = (uint32_t) -1;
         s_test_param = 0;
         s_test_set_cmd = retcode;
         // test
         TEST(retcode == RUN_SYNCFUNC(param));
         // check content not changed
         TEST(param.srun       == &srun);
         TEST(param.sfunc      == &sfunc);
         TEST(param.err        == 12345);
         TEST(sfunc.state      == state);
         TEST(sfunc.contoffset == contoffset);
         // check function parameter
         TEST(s_test_cmd   == synccmd_RUN);
         TEST(s_test_param == &param);
      }
   }

   // TEST EXIT_SYNCFUNC
   for (int retcode = -2; retcode <= 2; ++retcode) {
      syncfunc_t sfunc;
      param.sfunc = &sfunc;
      for (uint16_t contoffset = 0; contoffset <= 3; ++contoffset) {
         // prepare
         void* state = (void*)(uintptr_t)(10 + contoffset);
         init_syncfunc(&sfunc, &test_call_sf, state, 0);
         sfunc.contoffset = contoffset;
         param.err    = 0;
         s_test_cmd   = (uint32_t) -1;
         s_test_param = 0;
         s_test_set_cmd = retcode;
         // test
         TEST(retcode == EXIT_SYNCFUNC(param));
         // check content not changed exccept err == ECANCELED
         TEST(param.srun       == &srun);
         TEST(param.sfunc      == &sfunc);
         TEST(param.err        == ECANCELED);
         TEST(sfunc.state      == state);
         TEST(sfunc.contoffset == contoffset);
         // check function parameter
         TEST(s_test_cmd   == synccmd_EXIT);
         TEST(s_test_param == &param);
      }
   }

   // TEST link_waitfields: wait for condition
   param.condition = &scond;
   {
      syncfunc_t* sfunc[10];
      for (int i = 0; i < (int)lengthof(sfunc); ++i) {
         TEST(0 == allocfunc_syncrunner(&srun, WAITQ_ID, &sfunc[i]));
         init_syncfunc(sfunc[i], &test_call_sf, &param, WAITQ_OPTFLAGS);
         setwaitresult_syncfunc(sfunc[i], i+1);
         *waitlist_syncfunc(sfunc[i]) = (linkd_t) linkd_FREE;
         // test link_waitfields
         link_waitfields(&srun, sfunc[i], &param);
      }
      // check result
      TEST(isself_linkd(&srun.wakeup));
      TEST(sfunc[0] == waitfunc_synccond(&scond));
      // check content of wait list
      for (unsigned i = 0; i < lengthof(sfunc); ++i) {
         TEST(sfunc[i]->mainfct    == &test_call_sf);
         TEST(sfunc[i]->state      == &param);
         TEST(sfunc[i]->contoffset == 0);
         TEST(sfunc[i]->optflags   == WAITQ_OPTFLAGS);
         TEST(sfunc[i]->waitresult == 0/*cleared*/);
         linkd_t* prev = i == 0 ? &scond.waitfunc : waitlist_syncfunc(sfunc[i-1]);
         linkd_t* next = i == (int)lengthof(sfunc)-1 ? &scond.waitfunc : waitlist_syncfunc(sfunc[i+1]);
         TEST(prev == waitlist_syncfunc(sfunc[i])->prev);
         TEST(next == waitlist_syncfunc(sfunc[i])->next);
      }
      unlinkall_synccond(&scond);
   }

   // TEST link_waitfields: ERROR (condition == 0)
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   param.condition = 0;
   {
      syncfunc_t sfunc = syncfunc_FREE;
      sfunc.optflags = syncfunc_opt_WAITFIELDS;
      TEST(0 == waitresult_syncfunc(&sfunc));
      link_waitfields(&srun, &sfunc, &param);
      // check wait result EINVAL
      TEST(EINVAL == waitresult_syncfunc(&sfunc));
      // check added to wakeup list
      TEST(srun.wakeup.prev == waitlist_syncfunc(&sfunc));
      TEST(srun.wakeup.next == waitlist_syncfunc(&sfunc));
      TEST(&srun.wakeup == waitlist_syncfunc(&sfunc)->prev);
      TEST(&srun.wakeup == waitlist_syncfunc(&sfunc)->next);
   }

   // unprepare
   free_synccond(&scond);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_synccond(&scond);
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_wakeup_sf(syncfunc_param_t* sfparam, uint32_t sfcmd)
{
   assert(s_test_srun == sfparam->srun);

   s_test_runcount += 1;
   s_test_errcount += (sfcmd != s_test_expect_cmd);
   s_test_errcount += (state_syncfunc(sfparam) != s_test_expect_state);
   s_test_errcount += (contoffset_syncfunc(sfparam) != s_test_expect_contoffset);
   s_test_errcount += (sfparam->err != s_test_expect_err);
   if (s_test_expect_sfunc) {
      s_test_errcount += (sfparam->sfunc != s_test_expect_sfunc[s_test_runcount-1]);
   }

   setcontoffset_syncfunc(sfparam, s_test_set_contoffset);
   setstate_syncfunc(sfparam, s_test_set_state);

   if (s_test_set_cmd == synccmd_WAIT) {
      sfparam->condition = s_test_set_condition;

   } else if (s_test_set_cmd == synccmd_EXIT) {
      sfparam->err = s_test_set_retcode;
   }

   return s_test_set_cmd;
}

static int test_exec_wakeup(void)
{
   syncrunner_t srun;
   syncfunc_t*  sfunc[10];
   synccond_t   scond;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));
   init_synccond(&scond);

   // TEST process_waitq_syncrunner: EINPROGRESS
   srun.isrun = 1;
   TEST(EINPROGRESS == process_waitq_syncrunner(&srun));
   TEST(1 == srun.isrun);
   srun.isrun = 0;

   // TEST process_waitq_syncrunner: empty list
   TEST(isself_linkd(&srun.wakeup));
   TEST(0 == process_waitq_syncrunner(&srun));
   TEST(isself_linkd(&srun.wakeup));
   TEST(0 == size_syncrunner(&srun));

   // TEST process_waitq_syncrunner: synccmd_EXIT + all possible wakeup parameter
   s_test_set_cmd = synccmd_EXIT;
   s_test_expect_sfunc = sfunc;
   for (int isstate = 0; isstate <= 1; ++isstate) {
      s_test_expect_state = isstate ? (void*)(uintptr_t)0x123 : 0;
      for (int waitresult = 0; waitresult <= 256; waitresult += 64) {
         s_test_expect_err = waitresult;
         for (int contoffset = 0; contoffset <= 1; ++contoffset) {
            s_test_expect_contoffset = (uint16_t) contoffset;
            s_test_expect_cmd = synccmd_RUN;
            for (unsigned i = lengthof(sfunc)-1; i < lengthof(sfunc); --i) {
               TEST(0 == allocfunc_syncrunner(&srun, WAITQ_ID, &sfunc[i]));
               init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
               sfunc[i]->contoffset = (uint16_t) contoffset;
               setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
               initnext_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
            }

            // test process_waitq_syncrunner
            s_test_runcount = 0;
            TEST(0 == process_waitq_syncrunner(&srun));
            // check all sfunc woken up and exited
            TEST(0 == s_test_errcount);
            TEST(lengthof(sfunc) == s_test_runcount);
            TEST(0 == srun.isrun);
            TEST(0 == size_syncrunner(&srun));
            TEST(isself_linkd(&srun.wakeup));
         }
      }
   }
   s_test_expect_sfunc = 0;

   // TEST process_waitq_syncrunner: synccmd_RUN
   s_test_set_cmd = synccmd_RUN;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_state = 0;
   s_test_expect_err = 0;
   s_test_expect_contoffset = 0;
   for (int contoffset = 0; contoffset <= 32; contoffset += 32) {
      s_test_set_contoffset = (uint16_t) contoffset;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? (void*)3 : (void*)0;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == allocfunc_syncrunner(&srun, WAITQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
            setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
            initprev_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
         }

         // test
         s_test_runcount = 0;
         TEST(0 == process_waitq_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc) == size_syncrunner(&srun));
         TEST(0 == srun.isrun);
         TEST(isself_linkd(&srun.wakeup));
         // check length of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i == RUNQ_ID ? lengthof(sfunc) : 0, i));
            TEST(0 != srun.freecache[i]);
         }
         // check content of run queue
         unsigned i = 0;
         foreach (_queue, next, cast_queue(&srun.rwqueue[RUNQ_ID]), RUNQ_ELEMSIZE) {
            if (next != srun.freecache[RUNQ_ID]) {
               syncfunc_t* sf = next;
               TEST(sf->mainfct    == &test_wakeup_sf);
               TEST(sf->state      == s_test_set_state);
               TEST(sf->contoffset == s_test_set_contoffset);
               TEST(sf->optflags   == syncfunc_opt_NONE);
               TEST(i < lengthof(sfunc));
            }
            ++ i;
         }
         TEST(i == lengthof(sfunc)+1);
         // prepare for next run
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST process_waitq_syncrunner: synccmd_WAIT
   s_test_set_cmd = synccmd_WAIT;
   s_test_expect_state = 0;
   s_test_expect_err = 123;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_set_condition = &scond;
   for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
      s_test_set_contoffset = (uint16_t) contoffset;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? (void*)1 : (void*)0;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == allocfunc_syncrunner(&srun, WAITQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
            setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
            initprev_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
         }

         // test process_waitq_syncrunner
         s_test_runcount = 0;
         TEST(0 == process_waitq_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc) == size_syncrunner(&srun));
         TEST(0 == srun.isrun);
         TEST(isself_linkd(&srun.wakeup));
         TEST(iswaiting_synccond(&scond));
         TEST(sfunc[0] == waitfunc_synccond(&scond));
         // check length of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i == WAITQ_ID ? lengthof(sfunc) : 0, i));
            TEST(0 == srun.freecache[i] || i == RUNQ_ID);
         }
         // check content of wait queue
         unsigned i = 0;
         foreach (_queue, next, cast_queue(&srun.rwqueue[WAITQ_ID]), WAITQ_ELEMSIZE) {
            TEST(sfunc[i] == next);
            TEST(sfunc[i]->mainfct    == &test_wakeup_sf);
            TEST(sfunc[i]->state      == s_test_set_state);
            TEST(sfunc[i]->contoffset == s_test_set_contoffset);
            TEST(sfunc[i]->optflags   == syncfunc_opt_WAITFIELDS);
            TEST(sfunc[i]->waitresult == 0/*cleared*/);
            TEST(sfunc[i]->waitlist.prev == (i == 0 ? &scond.waitfunc : waitlist_syncfunc(sfunc[i-1])));
            TEST(sfunc[i]->waitlist.next == (i+1 == lengthof(sfunc) ? &scond.waitfunc : waitlist_syncfunc(sfunc[i+1])));
            ++i;
         }
         TEST(i == lengthof(sfunc));
         // prepare for next run
         unlinkall_synccond(&scond);
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST process_waitq_syncrunner: wait error ==> add function to waitlist with wait result == EINVAL
   s_test_set_cmd = synccmd_WAIT;
   s_test_expect_state = 0;
   s_test_expect_err = 0;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_set_condition = 0; // error
   for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
      s_test_set_contoffset = (uint16_t) contoffset;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? (void*)1 : (void*)0;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == allocfunc_syncrunner(&srun, WAITQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
            setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
            initprev_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
         }

         // test process_waitq_syncrunner
         s_test_runcount = 0;
         TEST(0 == process_waitq_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc) == size_syncrunner(&srun));
         TEST(0 == srun.isrun);
         // check length of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i == WAITQ_ID ? lengthof(sfunc) : 0, i));
            TEST(0 == srun.freecache[i] || i == RUNQ_ID);
         }
         // check wakeup list
         unsigned i = 0;
         for (linkd_t* next = srun.wakeup.next; next != &srun.wakeup; next = next->next) {
            TEST(sfunc[i] == castPwaitlist_syncfunc(next));
            TEST(sfunc[i]->mainfct    == &test_wakeup_sf);
            TEST(sfunc[i]->state      == s_test_set_state);
            TEST(sfunc[i]->contoffset == s_test_set_contoffset);
            TEST(sfunc[i]->optflags   == syncfunc_opt_WAITFIELDS);
            TEST(sfunc[i]->waitresult == EINVAL);
            TEST(sfunc[i]->waitlist.prev == (i == 0 ? &srun.wakeup : waitlist_syncfunc(sfunc[i-1])));
            TEST(sfunc[i]->waitlist.next == (i+1 == lengthof(sfunc) ? &srun.wakeup : waitlist_syncfunc(sfunc[i+1])));
            ++i;
         }
         TEST(i == lengthof(sfunc));
         // prepare for next run
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST process_waitq_syncrunner: EINVAL (removefunc_syncrunner + allocfunc_syncrunner)
   s_test_set_contoffset = 0;
   s_test_set_state = 0;
   s_test_expect_state = 0;
   s_test_expect_err = 0;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   for (unsigned cmd = 0; cmd <= 1; ++cmd) {
      s_test_set_cmd = cmd ? synccmd_RUN : synccmd_EXIT;
      for (unsigned errcount = 1; errcount <= lengthof(sfunc); ++errcount) {
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == allocfunc_syncrunner(&srun, WAITQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
            setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
            initnext_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
         }
         // test process_waitq_syncrunner
         s_test_runcount = 0;
         init_testerrortimer(&s_syncrunner_errtimer, cmd ? 2*errcount-1 : errcount, EINVAL);
         TEST(EINVAL == process_waitq_syncrunner(&srun));
         TEST(errcount < lengthof(sfunc) || isself_linkd(&srun.wakeup));
         // check result
         TEST(0 == s_test_errcount);
         TEST(errcount == s_test_runcount);
         TEST(0 == srun.isrun);
         // check size of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i == WAITQ_ID ? lengthof(sfunc)-errcount+cmd : cmd ? errcount : 0, i));
            if (0 == cmd || (errcount > 1 && i == WAITQ_ID)) {
               TEST(0 != srun.freecache[i]);
            } else {
               TEST(0 == srun.freecache[i]);
            }
         }
         // check content of wakeup list
         if (errcount < lengthof(sfunc)) {
            TEST(srun.wakeup.next == waitlist_syncfunc(sfunc[lengthof(sfunc)-1-errcount]));
            for (unsigned i = lengthof(sfunc)-1-errcount; i < lengthof(sfunc); --i) {
               TEST(sfunc[i]->mainfct    == &test_wakeup_sf);
               TEST(sfunc[i]->contoffset == 0);
               TEST(sfunc[i]->state      == s_test_expect_state);
               TEST(sfunc[i]->optflags   == syncfunc_opt_WAITFIELDS);
               TEST(sfunc[i]->waitresult == s_test_expect_err);
               TEST(sfunc[i]->waitlist.prev == (i+errcount+1 == lengthof(sfunc) ? &srun.wakeup : waitlist_syncfunc(sfunc[i+1])));
               TEST(sfunc[i]->waitlist.next == (i ? waitlist_syncfunc(sfunc[i-1]) : &srun.wakeup));
            }
         }
         // prepare for next run
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // unprepare
   free_synccond(&scond);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_run_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   assert(s_test_srun == sfparam->srun);

   s_test_runcount += 1;
   s_test_errcount += (sfparam->srun->isrun != 1);
   s_test_errcount += (sfcmd != s_test_expect_cmd);
   s_test_errcount += (state_syncfunc(sfparam) != s_test_expect_state);
   s_test_errcount += (contoffset_syncfunc(sfparam) != s_test_expect_contoffset);
   if (s_test_expect_sfunc) {
      s_test_errcount += (sfparam->sfunc != s_test_expect_sfunc[s_test_runcount-1]);
   }

   setcontoffset_syncfunc(sfparam, s_test_set_contoffset);
   setstate_syncfunc(sfparam, s_test_set_state);

   sfparam->condition = s_test_set_condition;
   sfparam->err       = s_test_set_retcode;

   return s_test_set_cmd;
}

static int test_exec_run(void)
{
   syncrunner_t   srun;
   syncfunc_t*    sfunc[10];
   synccond_t     scond;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));
   init_synccond(&scond);

   // TEST run_syncrunner: EINPROGRESS
   srun.isrun = 1;
   TEST(EINPROGRESS == run_syncrunner(&srun));
   TEST(0 == size_syncrunner(&srun));
   TEST(1 == srun.isrun);
   TEST(isself_linkd(&srun.wakeup));
   srun.isrun = 0;

   // TEST run_syncrunner: empty queues
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == fillcache_syncrunner(&srun, i));
      memset(srun.freecache[i], 0, s_syncrunner_qsize[i]);
   }
   TEST(0 == run_syncrunner(&srun));
   TEST(0 == s_test_errcount);
   TEST(0 == size_syncrunner(&srun));
   TEST(0 == srun.isrun);
   TEST(isself_linkd(&srun.wakeup));

   // TEST run_syncrunner: delete last entry in queue
   s_test_set_cmd = synccmd_EXIT;
   s_test_set_state = 0;
   s_test_expect_state = 0;
   s_test_expect_contoffset = 0;
   s_test_expect_cmd = synccmd_RUN;
   TEST(0 == addfunc_syncrunner(&srun, &test_run_sf, s_test_expect_state));
   // test
   s_test_runcount = 0;
   TEST(0 == run_syncrunner(&srun));
   // check result
   TEST(0 == s_test_errcount);
   TEST(1 == s_test_runcount);
   TEST(0 == size_syncrunner(&srun));
   TEST(0 == srun.isrun);
   TEST(isself_linkd(&srun.wakeup));

   // TEST run_syncrunner: synccmd_EXIT + all possible run parameter
   s_test_set_cmd = synccmd_EXIT;
   s_test_set_condition = 0;
   s_test_set_contoffset = 0;
   s_test_set_retcode = 0;
   s_test_set_state = 0;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_sfunc = sfunc;
   for (int isstate = 0; isstate <= 1; ++isstate) {
      s_test_expect_state = isstate ? (void*)(uintptr_t)0x234 : 0;
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_expect_contoffset = (uint16_t) contoffset;
         for (unsigned i = lengthof(sfunc)-1; i < lengthof(sfunc); --i) {
            TEST(0 == allocfunc_syncrunner(&srun, RUNQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
            sfunc[i]->contoffset = (uint16_t) contoffset;
         }
         // test run_syncrunner
         s_test_runcount = 0;
         TEST(0 == run_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(0 == size_syncrunner(&srun));
         TEST(0 == srun.isrun);
         TEST(isself_linkd(&srun.wakeup));
      }
   }
   s_test_expect_sfunc = 0;

   // TEST run_syncrunner: synccmd_RUN
   s_test_set_cmd = synccmd_RUN;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   for (int contoffset = 0; contoffset <= 256; contoffset += 256) {
      s_test_set_contoffset = (uint16_t) contoffset;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? (void*)9 : (void*)0;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == allocfunc_syncrunner(&srun, RUNQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
         }
         // test
         s_test_runcount = 0;
         TEST(0 == run_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc) == size_syncrunner(&srun));
         TEST(0 == srun.isrun);
         TEST(isself_linkd(&srun.wakeup));
         // check size of single queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i == RUNQ_ID ? lengthof(sfunc) : 0, i));
            if (i == RUNQ_ID) {
               TEST(0 == srun.freecache[i]);
            } else {
               TEST(0 != srun.freecache[i]);
            }
         }
         // check content of run queue
         unsigned i = 0;
         foreach (_queue, next, cast_queue(&srun.rwqueue[RUNQ_ID]), RUNQ_ELEMSIZE) {
            TEST(sfunc[i] == next);
            TEST(sfunc[i]->mainfct    == &test_run_sf);
            TEST(sfunc[i]->state      == s_test_set_state);
            TEST(sfunc[i]->contoffset == s_test_set_contoffset);
            TEST(sfunc[i]->optflags   == syncfunc_opt_NONE);
            ++i;
         }
         TEST(lengthof(sfunc) == i);
         // prepare for next run
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST run_syncrunner: synccmd_WAIT
   s_test_set_cmd = synccmd_WAIT;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   s_test_set_condition = &scond;
   for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
      s_test_set_contoffset = (uint16_t) contoffset;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? (void*)4 : 0;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == allocfunc_syncrunner(&srun, RUNQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, RUNQ_OPTFLAGS);
         }
         // test run_syncrunner
         s_test_runcount = 0;
         TEST(0 == run_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc) == size_syncrunner(&srun));
         TEST(0 == srun.isrun);
         TEST(isself_linkd(&srun.wakeup));
         // check length of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i == WAITQ_ID ? lengthof(sfunc) : 0, i));
            TEST(0 != srun.freecache[i]);
         }
         // check scond
         TEST(iswaiting_synccond(&scond));
         // check content of wait queue
         unsigned i = 0;
         linkd_t* wlprev = &scond.waitfunc;
         foreach (_queue, next, cast_queue(&srun.rwqueue[WAITQ_ID]), WAITQ_ELEMSIZE) {
            if (next != srun.freecache[WAITQ_ID]) {
               syncfunc_t* sf = next;
               TEST(sf->mainfct    == &test_run_sf);
               TEST(sf->state      == s_test_set_state);
               TEST(sf->contoffset == s_test_set_contoffset);
               TEST(sf->optflags   == WAITQ_OPTFLAGS);
               TEST(sf->waitresult == 0);
               TEST(sf->waitlist.prev == wlprev);
               TEST(&sf->waitlist     == wlprev->next);
               wlprev = &sf->waitlist;
               TEST(i < lengthof(sfunc));
            }
            ++i;
         }
         TEST(i == lengthof(sfunc)+1);
         // prepare for next run
         unlinkall_synccond(&scond);
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST run_syncrunner: run woken up functions
   s_test_set_cmd = synccmd_WAIT;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   s_test_set_condition = &scond;
   s_test_set_state = 0;
   s_test_set_contoffset = 0;
   for (unsigned i = 0; i < lengthof(sfunc); ++i) {
      TEST(0 == addfunc_syncrunner(&srun, &test_run_sf, s_test_expect_state));
   }
   // functions wait on scond
   TEST(0 == run_syncrunner(&srun));
   TEST(0 == s_test_errcount);
   TEST(0 == wakeupall_syncrunner(&srun, &scond));
   // test
   s_test_set_cmd = synccmd_EXIT;
   s_test_runcount = 0;
   TEST(0 == run_syncrunner(&srun));
   // check result
   TEST(0 == s_test_errcount);
   TEST(lengthof(sfunc) == s_test_runcount);
   TEST(0 == size_syncrunner(&srun));

   // TEST process_runq_syncrunner: wait error ==> add function to waitlist with wait result == EINVAL
   s_test_set_cmd = synccmd_WAIT;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   s_test_set_condition = 0;
   for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
      s_test_set_contoffset = (uint16_t) contoffset;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? (void*)2 : (void*)0;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == allocfunc_syncrunner(&srun, RUNQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
         }
         // test
         s_test_runcount = 0;
         TEST(0 == process_runq_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc) == size_syncrunner(&srun));
         TEST(0 == srun.isrun);
         TEST(isvalid_linkd(&srun.wakeup) && ! isself_linkd(&srun.wakeup));
         // check length of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i == WAITQ_ID ? lengthof(sfunc) : 0, i));
            TEST(0 != srun.freecache[i]);
         }
         // check content of wait queue
         unsigned i = 0;
         linkd_t* wlprev = &srun.wakeup;
         foreach (_queue, next, cast_queue(&srun.rwqueue[WAITQ_ID]), WAITQ_ELEMSIZE) {
            if (srun.freecache[WAITQ_ID] != next) {
               syncfunc_t* sf = next;
               TEST(sf->mainfct    == &test_run_sf);
               TEST(sf->state      == s_test_set_state);
               TEST(sf->contoffset == s_test_set_contoffset);
               TEST(sf->optflags   == WAITQ_OPTFLAGS);
               TEST(sf->waitresult == EINVAL);
               TEST(sf->waitlist.prev == wlprev);
               TEST(&sf->waitlist     == wlprev->next);
               wlprev = &sf->waitlist;
               TEST(i < lengthof(sfunc));
            }
            ++i;
         }
         TEST(i == lengthof(sfunc)+1);
         // prepare for next run
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST run_syncrunner: EINVAL (removefunc_syncrunner + allocfunc_syncrunner)
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   s_test_set_condition = &scond;
   s_test_set_contoffset = 0;
   s_test_set_state = 0;
   for (unsigned cmd = 0; cmd <= 1; ++cmd) {
      s_test_set_cmd = cmd ? synccmd_WAIT : synccmd_EXIT;
      for (unsigned errcount = 1; errcount <= lengthof(sfunc); ++errcount) {
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == allocfunc_syncrunner(&srun, RUNQ_ID, &sfunc[i]));
            init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
         }
         // test run_syncrunner
         s_test_runcount = 0;
         init_testerrortimer(&s_syncrunner_errtimer, cmd ? 2*errcount-1 : errcount, EINVAL);
         TEST(EINVAL == run_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(errcount == s_test_runcount);
         TEST(0 == srun.isrun);
         TEST(isself_linkd(&srun.wakeup));
         unlinkall_synccond(&scond); // in case of synccmd_WAIT
         // check length of every single queue
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i == RUNQ_ID ? lengthof(sfunc)-errcount+cmd : cmd ? errcount : 0, i));
            if (cmd == 0 || (errcount > 1 && i == RUNQ_ID)) {
               TEST(0 != srun.freecache[i]);
            } else {
               TEST(0 == srun.freecache[i]);
            }
         }
         // check content of run queue
         unsigned i = 0;
         foreach (_queue, next, cast_queue(&srun.rwqueue[RUNQ_ID]), RUNQ_ELEMSIZE) {
            if (next != srun.freecache[RUNQ_ID]) {
               TEST(sfunc[i] == next);
               TEST(sfunc[i]->mainfct    == &test_run_sf);
               TEST(sfunc[i]->state      == s_test_expect_state);
               TEST(sfunc[i]->contoffset == 0);
               TEST(sfunc[i]->optflags   == syncfunc_opt_NONE);
               TEST(i < lengthof(sfunc)-errcount+cmd);
            }
            ++i;
         }
         TEST(i == (srun.freecache[RUNQ_ID]!=0)+lengthof(sfunc)-errcount+cmd/*alloc error no remove called*/);
         // prepare for next run
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST run_syncrunner: EINVAL ==> no woken up functions run
   s_test_set_cmd = synccmd_EXIT;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   TEST(0 == addfunc_syncrunner(&srun, &test_run_sf, s_test_expect_state));
   TEST(0 == allocfunc_syncrunner(&srun, WAITQ_ID, &sfunc[0]));
   init_syncfunc(sfunc[0], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
   setwaitresult_syncfunc(sfunc[0], s_test_expect_err);
   initprev_linkd(waitlist_syncfunc(sfunc[0]), &srun.wakeup);
   // test run_syncrunner
   s_test_runcount = 0;
   init_testerrortimer(&s_syncrunner_errtimer, 1, EINVAL);
   TEST(EINVAL == run_syncrunner(&srun));
   // check result
   TEST(! isself_linkd(&srun.wakeup)); // not run
   TEST(0 == s_test_errcount);
   TEST(1 == s_test_runcount);
   TEST(1 == size_syncrunner(&srun));
   TEST(0 == srun.isrun);
   // check length of every single queue
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == check_queue_size(&srun, (i == WAITQ_ID), i));
   }

   // TEST run_syncrunner: ERROR from process_waitq_syncrunner returned
   s_test_runcount = 0;
   init_testerrortimer(&s_syncrunner_errtimer, 1, EINVAL);
   TEST(EINVAL == run_syncrunner(&srun));
   // check result
   TEST(isself_linkd(&srun.wakeup)); // woken up
   TEST(0 == s_test_errcount);
   TEST(1 == s_test_runcount);
   TEST(0 == size_syncrunner(&srun));
   TEST(0 == srun.isrun);

   // unprepare
   free_synccond(&scond);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_synccond(&scond);
   free_syncrunner(&srun);
   return EINVAL;
}

static int wait_sf(syncfunc_param_t* param, uint32_t cmd)
{
   syncfunc_START: ;

   s_test_runcount += 1;
   s_test_errcount += (param->srun->isrun != 1);
   s_test_errcount += (state_syncfunc(param) != s_test_expect_state);

   if (cmd == synccmd_RUN) {
      wait_syncfunc(param, state_syncfunc(param));
      s_test_errcount += 1; // never reached
   }

   s_test_errcount += (param->err != ECANCELED);
   s_test_errcount += (cmd != synccmd_EXIT);
   exit_syncfunc(param, 0);
}

static int test_exec_terminate(void)
{
   syncrunner_t srun  = syncrunner_FREE;
   synccond_t   scond = synccond_FREE;
   syncfunc_t*  sfunc[10*lengthof(srun.rwqueue)];
   syncfunc_t*  sf;
   unsigned     MAX_PER_QUEUE = 5000;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));
   init_synccond(&scond);

   // TEST terminate_syncrunner: EINPROGRESS
   srun.isrun = 1;
   TEST(EINPROGRESS == terminate_syncrunner(&srun));
   TEST(0 == size_syncrunner(&srun));
   TEST(1 == srun.isrun);
   TEST(isself_linkd(&srun.wakeup));
   srun.isrun = 0;

   // TEST terminate_syncrunner: empty queues
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == fillcache_syncrunner(&srun, i));
      memset(srun.freecache[i], 0, s_syncrunner_qsize[i]);
   }
   TEST(0 == terminate_syncrunner(&srun));
   TEST(0 == size_syncrunner(&srun));
   TEST(0 == srun.isrun);
   TEST(isself_linkd(&srun.wakeup));

   // TEST terminate_syncrunner: different parameter values
   s_test_set_cmd = synccmd_EXIT;
   s_test_set_condition = 0;
   s_test_set_contoffset = 1;
   s_test_set_retcode = 100;
   s_test_set_state = 0;
   s_test_expect_cmd = synccmd_EXIT;
   s_test_expect_state = 0;
   s_test_expect_sfunc = sfunc;
   for (int isstate = 0; isstate <= 1; ++isstate) {
      s_test_expect_state = isstate ? (void*)(uintptr_t)0x234 : 0;
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_expect_contoffset = (uint16_t) contoffset;
         for (unsigned qidx = 0, i = lengthof(sfunc)-1; qidx < lengthof(srun.rwqueue); ++qidx) {
            for (unsigned si = 0; si < lengthof(sfunc)/lengthof(srun.rwqueue); ++si,--i) {
               TEST(0 == allocfunc_syncrunner(&srun, qidx, &sfunc[i]));
               init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
               sfunc[i]->contoffset = s_test_expect_contoffset;
               if (qidx == WAITQ_ID) {
                  initprev_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
               }
            }
         }
         // test
         s_test_runcount = 0;
         TEST(0 == terminate_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(isself_linkd(&srun.wakeup));
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, 0, i));
            TEST(0 == srun.freecache[i]);
         }
      }
   }
   s_test_expect_sfunc = 0;

   // TEST terminate_syncrunner: unlink from condition
   s_test_expect_state = &scond;
   for (unsigned i = 0; i < MAX_PER_QUEUE; ++i) {
      TEST(0 == addfunc_syncrunner(&srun, &wait_sf, s_test_expect_state));
   }
   TEST(MAX_PER_QUEUE == size_syncrunner(&srun));
   TEST(! iswaiting_synccond(&scond));
   s_test_runcount = 0;
   TEST(0 == run_syncrunner(&srun));
   TEST(0 == s_test_errcount);
   TEST(MAX_PER_QUEUE == s_test_runcount);
   TEST(MAX_PER_QUEUE == size_syncrunner(&srun));
   TEST(  iswaiting_synccond(&scond));
   // test
   s_test_runcount = 0;
   TEST(0 == terminate_syncrunner(&srun));
   // check result
   TEST(0 == s_test_errcount);
   TEST(MAX_PER_QUEUE == s_test_runcount);
   TEST(! iswaiting_synccond(&scond));
   TEST(isself_linkd(&srun.wakeup));
   TEST(0 == srun.isrun);
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == check_queue_size(&srun, 0, i));
      TEST(0 == srun.freecache[i]);
   }

   // TEST terminate_syncrunner: EINVAL (clearqueue_syncrunner)
   s_test_set_cmd = synccmd_EXIT;
   s_test_expect_cmd = synccmd_EXIT;
   s_test_expect_contoffset = 0;
   for (unsigned errcount = 1; errcount <= lengthof(srun.rwqueue); ++errcount) {
      for (int isstate = 0; isstate <= 1; ++isstate) {
         for (unsigned qidx = 0; qidx < lengthof(srun.rwqueue); ++qidx) {
            s_test_expect_state = isstate ? (void*)5 : (void*)0;
            for (unsigned i = 0; i < MAX_PER_QUEUE; ++i) {
               TEST(0 == allocfunc_syncrunner(&srun, qidx, &sf));
               init_syncfunc(sf, &test_run_sf, s_test_expect_state, qidx==WAITQ_ID ? WAITQ_OPTFLAGS : RUNQ_OPTFLAGS);
               if (qidx==WAITQ_ID) initprev_linkd(waitlist_syncfunc(sf), &srun.wakeup);
            }
            // fill cache
            TEST(0 == allocfunc_syncrunner(&srun, qidx, &sf));
            TEST(0 == removefunc_syncrunner(&srun, qidx, sf));
         }
         // test
         init_testerrortimer(&s_syncrunner_errtimer, errcount, EINVAL);
         s_test_runcount = 0;
         TEST(EINVAL == terminate_syncrunner(&srun));
         // check result
         TEST(s_test_errcount == 0);
         TEST(s_test_runcount == errcount*MAX_PER_QUEUE);
         TEST(isself_linkd(&srun.wakeup));
         TEST(0 == srun.isrun);
         // check queue length of single queues
         for (unsigned i = 1; i <= lengthof(srun.rwqueue); ++i) {
            TEST(0 == check_queue_size(&srun, i <= errcount ? 0 : MAX_PER_QUEUE, lengthof(srun.rwqueue)-i));
            if (i <= errcount) {
               TEST(0 == srun.freecache[lengthof(srun.rwqueue)-i]);
            } else {
               TEST(0 != srun.freecache[lengthof(srun.rwqueue)-i]);
            }
         }
         // prepare for next run
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // unprepare
   free_synccond(&scond);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_synccond(&scond);
   free_syncrunner(&srun);
   return EINVAL;
}

typedef struct example_state_t {
   int inuse;
   int error;
   unsigned runcount;
   unsigned expect;
   synccond_t * cond;
} example_state_t;

static example_state_t s_example_state[100] = { {0,0,0,0,0} };

static int example1_sf(syncfunc_param_t* param, uint32_t cmd)
{
   unsigned id = (unsigned) state_syncfunc(param);
   example_state_t* state = &s_example_state[id];
   ++ state->runcount;
   start_syncfunc(param, cmd, ONEXIT);

// RUN
   // allocate resource
   if (state->inuse || state->runcount != 1) goto ONEXIT;
   state->inuse = 1;

   do {
      state->expect = state->runcount + 1;
      yield_syncfunc(param);
      if (state->expect != state->runcount) goto ONEXIT;
   } while (state->expect < 100);

   state->inuse = 0;
   exit_syncfunc(param, 0);

ONEXIT:
   // mark as free
   state->inuse = 0;
   state->error = 1;
   exit_syncfunc(param, ECANCELED);
}

static int example2_sf(syncfunc_param_t * param, uint32_t cmd)
{
   unsigned id = (unsigned) state_syncfunc(param);
   example_state_t * state = &s_example_state[id];
   ++ state->runcount;
   start_syncfunc(param, cmd, ONEXIT);

// RUN
   if (state->inuse || state->runcount != 1) goto ONEXIT;
   state->inuse = 1;

   if (wait_syncfunc(param, state->cond)) goto ONEXIT;

   if (2 != state->runcount) goto ONEXIT;

   state->inuse = 0;
   exit_syncfunc(param, 0);

ONEXIT:
   state->inuse = 0;
   state->error = 1;
   exit_syncfunc(param, EINTR);

}

static int example3_sf(syncfunc_param_t * param, uint32_t cmd)
{
   unsigned id = (unsigned) state_syncfunc(param);
   example_state_t * state = &s_example_state[id];
   ++ state->runcount;
   start_syncfunc(param, cmd, ONEXIT);

// RUN
   if (state->inuse || state->runcount != 1) goto ONEXIT;
   state->inuse = 1;

   if (id < lengthof(s_example_state)/2) {
      yield_syncfunc(param);
      if (0 != wakeup_synccond(state->cond, param)) goto ONEXIT;
   } else {
      if (wait_syncfunc(param, state->cond)) goto ONEXIT;
   }

   if (2 != state->runcount) goto ONEXIT;

   state->inuse = 0;
   exit_syncfunc(param, 0);

ONEXIT:
   state->inuse = 0;
   state->error = 1;
   exit_syncfunc(param, EINTR);
}

static int test_examples(void)
{
   syncrunner_t srun = syncrunner_FREE;
   synccond_t   cond = synccond_FREE;

   // prepare
   TEST(0 == init_syncrunner(&srun));
   init_synccond(&cond);

   // TEST run_syncrunner: functions yield
   memset(s_example_state, 0, sizeof(s_example_state));
   for (uintptr_t i = 0; i < lengthof(s_example_state); ++i) {
      TEST(0 == addfunc_syncrunner(&srun, example1_sf, (void*)i));
   }
   // run 100 times
   for (unsigned r = 1; r <= 100; ++r) {
      const bool islast = (r == 100);
      TEST(lengthof(s_example_state) == size_syncrunner(&srun));
      TEST(0 == run_syncrunner(&srun));
      if (islast) {  // exit called
         TEST(0 == size_syncrunner(&srun));
      }
      for (unsigned i = 0; i < lengthof(s_example_state); ++i) {
         TEST(!islast == s_example_state[i].inuse)
         TEST(0 == s_example_state[i].error);
         TEST(r == s_example_state[i].runcount);
      }
   }

   // TEST run_syncrunner: functions waiting on condition
   memset(s_example_state, 0, sizeof(s_example_state));
   for (uintptr_t i = 0; i < lengthof(s_example_state); ++i) {
      s_example_state[i].cond = &cond;
      TEST(0 == addfunc_syncrunner(&srun, example2_sf, (void*)i));
   }
   for (unsigned r = 1; r <= 3; ++r) {
      // after first run all are waiting for condition
      TEST(0 == run_syncrunner(&srun));
      TEST(iswaiting_synccond(&cond));
      TEST(lengthof(s_example_state) == size_syncrunner(&srun));
      for (unsigned i = 1; i < lengthof(s_example_state); ++i) {
         TEST(1 == s_example_state[i].inuse);
         TEST(1 == s_example_state[i].runcount);
         TEST(0 == s_example_state[i].error);
      }
   }
   TEST(0 == wakeupall_syncrunner(&srun, &cond));
   TEST(0 == run_syncrunner(&srun));
   for (unsigned i = 1; i < lengthof(s_example_state); ++i) {
      TEST(0 == s_example_state[i].inuse);
      TEST(2 == s_example_state[i].runcount);
      TEST(0 == s_example_state[i].error);
   }

   // TEST run_syncrunner: functions starting others
   memset(s_example_state, 0, sizeof(s_example_state));
   for (uintptr_t i = 0; i < lengthof(s_example_state); ++i) {
      s_example_state[i].cond = &cond;
      TEST(0 == addfunc_syncrunner(&srun, example3_sf, (void*)i));
   }
   // 50% yielding / 50% waiting
   TEST(0 == run_syncrunner(&srun));
   // check result
   TEST(iswaiting_synccond(&cond));
   TEST(isself_linkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == check_queue_size(&srun, lengthof(s_example_state)/2, i));
   }
   // 50% call wakeup and exit / 50% other woken up and exit
   TEST(0 == run_syncrunner(&srun));
   // check result
   TEST(0 == size_syncrunner(&srun));
   for (unsigned i = 1; i < lengthof(s_example_state); ++i) {
      TEST(0 == s_example_state[i].inuse);
      TEST(2 == s_example_state[i].runcount);
      TEST(0 == s_example_state[i].error);
   }

   // TEST terminate_syncrunner
   memset(s_example_state, 0, sizeof(s_example_state));
   for (uintptr_t i = 0; i < lengthof(s_example_state); ++i) {
      TEST(0 == addfunc_syncrunner(&srun, example2_sf, (void*)i));
      s_example_state[i].cond = &cond;
   }
   // after first run all are waiting for condition
   TEST(0 == run_syncrunner(&srun));
   TEST(  iswaiting_synccond(&cond));
   TEST(lengthof(s_example_state) == size_syncrunner(&srun));
   for (unsigned e = 1; e < lengthof(s_example_state); ++e) {
      TEST(1 == s_example_state[e].inuse);
      TEST(1 == s_example_state[e].runcount);
      TEST(0 == s_example_state[e].error);
   }
   TEST(0 == terminate_syncrunner(&srun));
   TEST(! iswaiting_synccond(&cond));
   TEST(0 == size_syncrunner(&srun));
   for (unsigned e = 1; e < lengthof(s_example_state); ++e) {
      TEST(0 == s_example_state[e].inuse);
      TEST(2 == s_example_state[e].runcount);
      TEST(1 == s_example_state[e].error);
   }

   // unprepare
   free_synccond(&cond);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_synccond(&cond);
   free_syncrunner(&srun);
   return EINVAL;
}

int unittest_task_syncrunner()
{
   if (test_constants())      goto ONERR;
   if (test_staticvars())     goto ONERR;
   if (test_memory())         goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_queuehelper())    goto ONERR;
   if (test_query())          goto ONERR;
   if (test_addfunc())        goto ONERR;
   if (test_wakeup())         goto ONERR;
   if (test_exec_helper())    goto ONERR;
   if (test_exec_wakeup())    goto ONERR;
   if (test_exec_run())       goto ONERR;
   if (test_exec_terminate()) goto ONERR;
   if (test_examples())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif