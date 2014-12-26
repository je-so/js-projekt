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
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/errortimer.h"
#endif


// section: syncrunner_t

// group: constants

// TODO: test constants and static variables

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

// group: memory

/* define: MOVE
 * Verschiebe Speicher von src nach dest in Einheiten von long.
 *
 * Unchecked Precondition:
 * o (0 == structsize % sizeof(long)) */
#define MOVE(dest, src, structsize) \
         do { \
            unsigned _nr = (structsize) / sizeof(long); \
            unsigned _n  = 0; \
            long * _s = (long*) (src); \
            long * _d = (long*) (dest); \
            do { \
               _d[_n] = _s[_n]; \
            } while (++_n < _nr); \
         } while (0)

/* function: move_syncfunc
 * Verschiebt <syncfunc_t> der Größe size von src nach dest.
 * In src enthaltene Links werden dabei angepasst, so daß
 * Linkziele nach dem Verschieben korrekt zurückzeigen. */
static void move_syncfunc(syncfunc_t * dest, syncfunc_t * src, uint16_t structsize)
{
   if (src != dest) {
      // move in memory

      MOVE(dest, src, structsize);

      // adapt links to new location

      relink_syncfunc(dest);

      // TODO: replace with initmove_syncfunc !!
   }
}

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
      err = init_syncqueue(&srun->rwqueue[qidx], s_syncrunner_qsize[qidx], (uint8_t) qidx);
      if (err) goto ONERR;
   }

   initself_linkd(&srun->wakeup);
   srun->isrun = 0;

   return 0;
ONERR:
   while (qidx > 0) {
      (void) free_syncqueue(&srun->rwqueue[--qidx]);
   }
   return err;
}

int free_syncrunner(syncrunner_t * srun)
{
   int err = 0;
   int err2;

   for (unsigned i = 0; i < lengthof(srun->rwqueue); ++i) {
      err2 = free_syncqueue(&srun->rwqueue[i]);
      SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err2);
      if (err2) err = err2;
   }

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: queue-helper

/* function: remove_syncqueue
 * Lösche sfunc aus squeue.
 * Entweder wird das letzte Element in squeue nach sfunc kopiert
 * oder, wenn das letzte Element das preallokierte Element ist,
 * einfach sfunc als neues freies Element markiert.
 * Das letzte Element wird dann aus der Queue gelöscht.
 *
 * Unchecked Precondition:
 * o Links are invalid;
 *   (sfunc->waitlist.prev == 0 && sfunc->waitlist.next == 0).
 * o sfunc ∈ queue / (squeue == castPaddr_syncqueue(sfunc))
 * */
static int remove_syncqueue(syncqueue_t * squeue, syncfunc_t * sfunc)
{
   int err;
   queue_t * queue = cast_queue(squeue);
   void    * last  = last_queue(queue, elemsize_syncqueue(squeue));

   if (!last) {
      // should never happen cause at least sfunc is stored in squeue
      err = ENODATA;
      goto ONERR;
   }

   // TODO: replace with initmove_syncfunc
   // TODO: implement initmove_link / initmove_linkd which keeps self pointers
   move_syncfunc(sfunc, last, elemsize_syncqueue(squeue));

   err = removelast_syncqueue(squeue);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

/* function: wait_queue
 * Gib <syncqueue_t> zurück, in der sfunc abgelegt ist.
 * Der Wert 0 wird zurückgegeben, wenn sfunc nicht zu srun gehört. */
static inline syncqueue_t * wait_queue(syncrunner_t * srun, syncfunc_t * sfunc)
{
   syncqueue_t* squeue = castPaddr_syncqueue(sfunc);
   return squeue == &srun->rwqueue[WAITQ_ID] ? squeue : 0;
}

// group: query

bool iswakeup_syncrunner(const syncrunner_t* srun)
{
   return ! isself_linkd(&srun->wakeup);
}

size_t size_syncrunner(const syncrunner_t * srun)
{
   size_t size = size_syncqueue(&srun->rwqueue[0]);

   for (unsigned i = 1; i < lengthof(srun->rwqueue); ++i) {
      size += size_syncqueue(&srun->rwqueue[i]);
   }

   return size;
}

// group: update

int addfunc_syncrunner(syncrunner_t * srun, syncfunc_f mainfct, void* state)
{
   int err;

   void* node;
   const unsigned qidx = RUNQ_ID;

   ONERROR_testerrortimer(&s_syncrunner_errtimer, &err, ONERR);
   err = preallocate_syncqueue(&srun->rwqueue[qidx], &node);
   if (err) goto ONERR;

   syncfunc_t* sf = node;
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

   syncfunc_t  * wakeupfunc = waitfunc_synccond(scond);
   linkd_t     * waitlist = waitlist_syncfunc(wakeupfunc);
   syncqueue_t * squeue   = wait_queue(srun, wakeupfunc);

   if (!squeue) return EINVAL;

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

/* define: CALL_SYNCFUNC
 * Bereite srun und param vor und rufe sfunc->mainfct auf.
 * Parameter werden mehrfach ausgewertet!
 * Kopiert <syncfunc_t.contoffset> nach <syncfunc_param_t.contoffset>.
 * Kopiert <syncfunc_t.state> nach <syncfunc_param_t.state>, falls isstate != 0,
 * ansonsten wird <syncfunc_param_t.state> auf 0 gesetzt.
 * <syncfunc_t.mainfct> wird mit Kommando <synccmd_RUN> bzw. <synccmd_CONTINUE> aufgerufen,
 * je nachdem, ob <syncfunc_t.contoffset> 0 ist oder nicht.
 *
 * Parameter:
 * param - Name von lokalem <syncfunc_param_t>
 * sfunc - Pointer auf <syncfunc_t>.
 *
 * // TODO: remove size
 *
 * Unchecked Precondition:
 * o param is initialized with pointer to <syncrunner_t> */
#define CALL_SYNCFUNC(param, sfunc) \
         ( __extension__ ({                                                                     \
            param.contoffset = (sfunc)->contoffset;                                             \
            param.state      = (sfunc)->state;                                                  \
            (sfunc)->mainfct(&param, param.contoffset == 0 ? synccmd_RUN : synccmd_CONTINUE);   \
         }))

// TODO: add doc
#define TERMINATE_SYNCFUNC(param, sfunc, errcode) \
         ( __extension__ ({                            \
            (param)->contoffset = (sfunc)->contoffset; \
            (param)->state      = (sfunc)->state;      \
            (param)->err        = (errcode);           \
            (sfunc)->mainfct((param), synccmd_EXIT);   \
         }))

// TODO: replace cleanup_after_exit with handle_exit_returncode

/* function: cleanup_after_exit
 * TODO: description
 *
 * Unchecked Precondition:
 * o errcode == syncfunc_param_t.err
 * o sfunc returned synccmd_EXIT or was terminated !
 * */
static inline void cleanup_after_exit(syncrunner_t* srun, syncfunc_t* sfunc, int errcode)
{
   // move code remove from queue into this function !!

   (void)srun;
   (void)sfunc;
   (void)errcode;
}

// TODO: describe + test function
static inline void call_terminate(syncfunc_param_t* param, syncfunc_t* sfunc, int errcode)
{
   // ignore return value
   (void) TERMINATE_SYNCFUNC(param, sfunc, errcode);

   // !! TODO: implement other stuff !!
}

/* function: link_waitfields
 * Initialisiere <syncfunc_t.waitlist> und <syncfunc_t.waitresult>.
 *
 * Unchecked Precondition:
 * o sfunc has optional waitresult/waitlist fields
 */
static inline void link_waitfields(syncrunner_t* srun, syncfunc_t* sfunc, syncfunc_param_t* param)
{
   if (! param->condition) goto ONERR;

   if (  iswaiting_synccond(param->condition)
         && !wait_queue(srun, waitfunc_synccond(param->condition))/*other srun*/) {
         goto ONERR;
   }

   setwaitresult_syncfunc(sfunc, 0);
   link_synccond(param->condition, sfunc);

   return;
ONERR:
   // use syncfunc_param_t.err to transport error
   setwaitresult_syncfunc(sfunc, EINVAL);
   link_to_wakeup(srun, waitlist_syncfunc(sfunc));
}

/* function: process_wakeup_list
 * Starte alle in <syncrunner_t.wakeup> gelistete <syncfunc_t>.
 * Der Rückgabewert vom Typ <synccmd_e> entscheidet, ob die Funktion weiterhin
 * in der Wartequeue verbleibt oder in die Runqueue verschoben wird.
 *
 * <syncfunc_param_t.err> = <syncfunc_t.waitresult>.
 *
 * */
static inline int process_wakeup_list(syncrunner_t * srun)
{
   int err;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   syncqueue_t    * squeue;
   syncfunc_t     * sfunc;
   syncfunc_t     * sfunc2;
   int              cmd;
   linkd_t          wakeup;
   void*            freenode; // TODO: remove

   if (isself_linkd(&srun->wakeup)) return 0;

   // this ensures that newly woken up syncfunc_t are not executed this time

   wakeup = srun->wakeup;
   relink_linkd(&wakeup);
   initself_linkd(&srun->wakeup);

   squeue = &srun->rwqueue[WAITQ_ID];

   while (wakeup.next != &wakeup) {
      sfunc = castPwaitlist_syncfunc(wakeup.next);
      unlinkself_linkd(wakeup.next);

      param.err = waitresult_syncfunc(sfunc);
      cmd = CALL_SYNCFUNC(param, sfunc);

      switch ((synccmd_e)cmd) {
      default:
      case synccmd_RUN:
         param.contoffset = 0;
         // continue synccmd_CONTINUE;

      case synccmd_CONTINUE:
         // move from wait to run queue

         // TODO: add test that fails for preallocate
         err = preallocate_syncqueue(&srun->rwqueue[RUNQ_ID], &freenode);
         assert(!err); // TODO: remove
         sfunc2 = freenode;
         init2_syncfunc(sfunc2, RUNQ_ELEMSIZE, param.contoffset, RUNQ_OPTFLAGS, param.state, sfunc, WAITQ_ELEMSIZE);
         // TODO: add test that fails for remove
         goto REMOVE_FROM_OLD_QUEUE;

      case synccmd_EXIT:
         cleanup_after_exit(srun, sfunc, param.err);
         // TODO: add test that fails for remove
         goto REMOVE_FROM_OLD_QUEUE;

      case synccmd_WAIT:

         sfunc->contoffset = param.contoffset;
         sfunc->state = param.state;

         // syncfunc_t: waitresult, waitlist are invalid ==> set them !
         link_waitfields(srun, sfunc, &param);
         break;
      }

      continue ;

      REMOVE_FROM_OLD_QUEUE: ;

      // TODO: set mainfct == 0 in remove (in case of error) !! check mainfct == 0 in loop
      err = remove_syncqueue(squeue, sfunc);
      SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   if (!isself_linkd(&wakeup)) {
      splice_linkd(&srun->wakeup, &wakeup);
      unlinkself_linkd(&wakeup);
   }
   return err;
}

int run2_syncrunner(syncrunner_t * srun, bool runwakeup)
{
   int err;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   queue_iterator_t iter = queue_iterator_FREE;
   syncqueue_t    * squeue;
   queue_t        * queue;
   syncfunc_t     * sfunc;
   syncfunc_t     * sfunc2;
   int              cmd;
   unsigned         qidx2;
   uint16_t         size;
   uint16_t         size2;
   void*            freenode; // TODO: remove

   if (srun->isrun) return EINPROGRESS;

   // prepare
   srun->isrun = true;

   // run every entry in run queue once

   squeue = &srun->rwqueue[RUNQ_ID];
   queue  = cast_queue(squeue);
   size   = RUNQ_ELEMSIZE;

   // new entries are added to end of queue ==> do not run them during this invocation

   err = initlast_queueiterator(&iter, queue, size);
   if (err) {
      if (err != ENODATA) goto ONERR;
      err = 0;

   } else  {
      void* prev;
      bool  isPrev = prev_queueiterator(&iter, &prev);
      while (isPrev) {
         sfunc = prev;
         isPrev = prev_queueiterator(&iter, &prev); // makes calling remove_syncqueue(squeue, sfunc) safe

         cmd = CALL_SYNCFUNC(param, sfunc);

         switch ((synccmd_e)cmd) {
         default:
         case synccmd_RUN:
            param.contoffset = 0;
            // continue synccmd_CONTINUE;

         case synccmd_CONTINUE:
            sfunc->contoffset = param.contoffset;
            sfunc->state = param.state;

            break;

         case synccmd_EXIT:
            cleanup_after_exit(srun, sfunc, param.err);
            goto REMOVE_FROM_OLD_QUEUE;

         case synccmd_WAIT:
            // calculate correct wait queue id and move into new queue
            qidx2 = WAITQ_ID;
            // TODO: add test that fails for preallocate
            err = preallocate_syncqueue(&srun->rwqueue[qidx2], &freenode);
            assert(!err); // TODO: remove
            sfunc2 = freenode;
            size2  = elemsize_syncqueue(&srun->rwqueue[qidx2]);
            // TODO: remove size parameter // replace with init_syncfunc ??
            init2_syncfunc(sfunc2, size2, param.contoffset, WAITQ_OPTFLAGS, param.state, sfunc, size);
            // waitresult & waitlist are undefined ==> set it !
            link_waitfields(srun, sfunc2, &param);
            // TODO: add test that fails for remove
            goto REMOVE_FROM_OLD_QUEUE;
         }

         continue ;

         REMOVE_FROM_OLD_QUEUE: ;

         // TODO: set mainfct == 0 in remove (in case of error) !! check mainfct == 0 in loop
         err = remove_syncqueue(squeue, sfunc);
         SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
         if (err) goto ONERR;
      }

      err = free_queueiterator(&iter);
      if (err) goto ONERR;
   }

   if (runwakeup) {
      err = process_wakeup_list(srun);
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

int terminate_syncrunner(syncrunner_t* srun)
{
   int err;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   queue_iterator_t iter = queue_iterator_FREE;
   syncqueue_t* squeue;
   queue_t*     queue;
   syncfunc_t*  sfunc;
   uint16_t     size;

   if (srun->isrun) return EINPROGRESS;

   // prepare
   srun->isrun = true;

   for (int qidx = (int)lengthof(srun->rwqueue)-1; qidx >= 0; --qidx) {
      squeue = &srun->rwqueue[qidx];
      queue  = cast_queue(squeue);
      size   = elemsize_syncqueue(&srun->rwqueue[qidx]);

      void * prev;
      bool isPrev;
      err = initlast_queueiterator(&iter, queue, size);
      if (err) {
         if (err != ENODATA) goto ONERR;
         err = 0;
         continue;
      }
      isPrev = prev_queueiterator(&iter, &prev);
      while (isPrev) {
         sfunc = prev;
         isPrev = prev_queueiterator(&iter, &prev); // makes calling remove_syncqueue(squeue, sfunc) safe

         // TODO: implement inline function call_terminate which contains unlink
         //       and call it everywhere where an internal execution error occurs
         //       also set param.err to specific error code of internal erro
         // TODO: add test for this new functionality

         unlink_syncfunc(sfunc);

         // TODO: replace with macro TERMINATE_SYNCFUNC
         param.contoffset = sfunc->contoffset;
         param.state      = sfunc->state;
         param.err        = ECANCELED;
         (void) sfunc->mainfct(&param, synccmd_EXIT);

         // TODO: call cleanup_after_terminate (which does nothing!!)

         // TODO: move remove out of iteration ==> even in case of error no function is called twice
         err = remove_syncqueue(squeue, sfunc);
         SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
         if (err) goto ONERR;
      }

      err = free_queueiterator(&iter);
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

#ifdef KONFIG_UNITTEST

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
   unsigned long src[100];
   unsigned long dest[100];
   linkd_t waitlist  = linkd_FREE;

   // TEST MOVE: check precondition
   for (unsigned i = 0; i < lengthof(s_syncrunner_qsize); ++i) {
      TEST(0 == s_syncrunner_qsize[i] % sizeof(long));
   }

   // TEST MOVE: copy memory
   for (unsigned size = 1; size <= lengthof(src); ++size) {
      for (unsigned offset = 0; offset + size <= lengthof(src); ++offset) {
         memset(src, 0, sizeof(src));
         memset(dest, 255, sizeof(dest));
         MOVE(&dest[offset], src, size*sizeof(long));
         for (unsigned i = 0; i < lengthof(src); ++i) {
            TEST(0 == src[i]);
         }
         for (unsigned i = 0; i < offset; ++i) {
            TEST(ULONG_MAX == dest[i]);
         }
         for (unsigned i = offset; i < offset+size; ++i) {
            TEST(0 == dest[i]);
         }
         for (unsigned i = offset+size; i < lengthof(dest); ++i) {
            TEST(ULONG_MAX == dest[i]);
         }
      }
   }

   // TEST move_syncfunc: memory is copied and links are adapted to new location
   for (unsigned optflags = syncfunc_opt_NONE; optflags <= syncfunc_opt_ALL; ++optflags) {
      uint16_t size = getsize_syncfunc(optflags);
      for (unsigned soffset = 0; soffset <= 10; soffset += 10) {
         for (unsigned doffset = 0; doffset <= 20; doffset += 5) {
            syncfunc_t * src_func = (syncfunc_t*) &src[soffset];
            syncfunc_t * dst_func = (syncfunc_t*) &dest[doffset];
            void *       state    = (void*)(uintptr_t)(64 * soffset + doffset);

            init_syncfunc(src_func, &dummy_sf, state, optflags);
            src_func->waitresult = (int) soffset;
            init_linkd(waitlist_syncfunc(src_func), &waitlist);

            // test move_syncfunc
            memset(dst_func, 255, sizeof(syncfunc_t));
            move_syncfunc(dst_func, src_func, size);

            // check src_func/dst_func valid & links adapted
            for (int isdst = 0; isdst <= 1; ++isdst) {
               syncfunc_t* sfunc = isdst ? dst_func : src_func;
               TEST(sfunc->mainfct    == &dummy_sf);
               TEST(sfunc->state      == state);
               TEST(sfunc->contoffset == 0);
               TEST(sfunc->optflags   == optflags);
               if (optflags & syncfunc_opt_WAITFIELDS) {
                  TEST((int)soffset == sfunc->waitresult);
                  TEST(&waitlist == waitlist_syncfunc(sfunc)->prev);
                  TEST(&waitlist == waitlist_syncfunc(sfunc)->next);
               }
            }
            if (0 != (optflags & syncfunc_opt_WAITFIELDS)) {
               TEST(waitlist.prev == waitlist_syncfunc(dst_func));
               TEST(waitlist.next == waitlist_syncfunc(dst_func));
            }
         }
      }
   }

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
      TEST(isfree_syncqueue(&srun.rwqueue[i]));
   }
   TEST(0 == srun.isrun);

   // TEST init_syncrunner
   memset(&srun, 255, sizeof(srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(srun.wakeup.prev == &srun.wakeup);
   TEST(srun.wakeup.next == &srun.wakeup);
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST( isfree_syncqueue(&srun.rwqueue[i]));
      TEST(0 == size_syncqueue(&srun.rwqueue[i]));
      TEST(s_syncrunner_qsize[i] == elemsize_syncqueue(&srun.rwqueue[i]));
   }
   TEST(0 == srun.isrun);

   // TEST free_syncrunner: free queues
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      void* node;
      TEST(0 == preallocate_syncqueue(&srun.rwqueue[i], &node));
      TEST(1 == size_syncqueue(&srun.rwqueue[i]));
   }
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == isfree_syncqueue(&srun.rwqueue[i]));
   }
   TEST(0 == free_syncrunner(&srun));
   // only queues are freed
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(isfree_syncqueue(&srun.rwqueue[i]));
   }

   // TEST free_syncrunner: double free
   TEST(0 == free_syncrunner(&srun));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(isfree_syncqueue(&srun.rwqueue[i]));
   }

   // TEST free_syncrunner: ERROR
   for (unsigned tc = 1; tc <= lengthof(srun.rwqueue); ++tc) {
      TEST(0 == init_syncrunner(&srun));
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         void* node;
         TEST(0 == preallocate_syncqueue(&srun.rwqueue[i], &node));
         TEST(0 == isfree_syncqueue(&srun.rwqueue[i]));
      }
      init_testerrortimer(&s_syncrunner_errtimer, tc, EINVAL);
      TEST(EINVAL == free_syncrunner(&srun));
      // queues are freed
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         TEST(isfree_syncqueue(&srun.rwqueue[i]));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_queuehelper(void)
{
   syncrunner_t srun = syncrunner_FREE;

   // prepare
   TEST(0 == init_syncrunner(&srun));

   for (unsigned qidx = 0; qidx < lengthof(srun.rwqueue); ++qidx) {

      linkd_t        waitlist = linkd_FREE;
      syncqueue_t*   squeue   = &srun.rwqueue[qidx];
      uint16_t       size     = WAITQ_ID == qidx ? WAITQ_ELEMSIZE : RUNQ_ELEMSIZE;
      syncfunc_opt_e optflags = WAITQ_ID == qidx ? WAITQ_OPTFLAGS : RUNQ_OPTFLAGS;
      void*          node;
      TEST(0 == preallocate_syncqueue(squeue, &node));
      syncfunc_t*    sfunc    = node;
      init_syncfunc(sfunc, 0, 0, (syncfunc_opt_e) (optflags+1));
      if (WAITQ_ID == qidx) {
         sfunc->waitresult = 0;
         sfunc->waitlist   = (linkd_t) linkd_FREE;
      }

      // TEST remove_syncqueue: remove not last ==> last element copied to removed
      TEST(0 == preallocate_syncqueue(squeue, &node));
      syncfunc_t* last = node;
      init_syncfunc(last, &dummy_sf, (void*)1, optflags);
      last->contoffset = 2;
      if (WAITQ_ID == qidx) {
         last->waitresult = 0x1234;
         init_linkd(&last->waitlist, &waitlist);
      }
      // test
      TEST(2 == size_syncqueue(squeue));
      TEST(0 == remove_syncqueue(squeue, sfunc));
      // check result
      TEST(1 == size_syncqueue(squeue));
      TEST(sfunc == last_queue(cast_queue(squeue), size));
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

      // TEST remove_syncqueue: queuesize > 1 && remove last element
      TEST(0 == preallocate_syncqueue(squeue, &node));
      sfunc = node;
      TEST(2 == size_syncqueue(squeue));
      memset(sfunc, 255, size);
      TEST(0 == remove_syncqueue(squeue, sfunc));
      // check result
      TEST(1 == size_syncqueue(squeue));
      TEST(sfunc == (syncfunc_t*) (size + (uint8_t*)last_queue(cast_queue(squeue), size)));
      for (uint8_t * data = (uint8_t*) sfunc; data != size + (uint8_t*)sfunc; ++data) {
         TEST(*data == 255); // content not changed
      }

      // TEST remove_syncqueue: queuesize == 1 && last element
      node = last_queue(cast_queue(squeue), size);
      TEST(0 == remove_syncqueue(squeue, node));
      TEST(0 == size_syncqueue(squeue));

      // TEST remove_syncqueue: ENODATA
      TEST(ENODATA == remove_syncqueue(squeue, node));
   }

   for (unsigned i = 0; i < 100; ++i) {

      // TEST wait_queue: OK
      syncqueue_t* squeue = &srun.rwqueue[WAITQ_ID];
      syncfunc_t*  sfunc;
      void*        node;

      TEST(0 == preallocate_syncqueue(squeue, &node));
      sfunc = node;
      init_syncfunc(sfunc, 0, 0, WAITQ_OPTFLAGS);
      TEST(squeue == wait_queue(&srun, sfunc));

      // TEST wait_queue: ERROR
      squeue = &srun.rwqueue[RUNQ_ID];
      TEST(0 == preallocate_syncqueue(squeue, &node));
      sfunc = node;
      init_syncfunc(sfunc, 0, 0, RUNQ_OPTFLAGS);
      TEST(0 == wait_queue(&srun, sfunc));
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

   // prepare
   TEST(0 == init_syncrunner(&srun));

   // TEST iswakeup_syncrunner: after init
   TEST(0 == iswakeup_syncrunner(&srun));

   // TEST iswakeup_syncrunner: true ==> wakeup list not empty
   link_to_wakeup(&srun, &sfunc.waitlist);
   TEST(1 == iswakeup_syncrunner(&srun));

   // TEST iswakeup_syncrunner: false ==> wakeup list empty
   unlinkself_linkd(&sfunc.waitlist);
   TEST(0 == iswakeup_syncrunner(&srun));

   // TEST size_syncrunner: after init
   TEST(0 == size_syncrunner(&srun));

   // TEST size_syncrunner: size of single queue
   for (unsigned size = 1; size <= 4; ++size) {
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         for (unsigned s = 1; s <= size; ++s) {
            void* node;
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[i], &node));
            TEST(s == size_syncrunner(&srun));
         }
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST size_syncrunner: size of all queues
   for (unsigned size = 1; size <= 4; ++size) {
      unsigned S = 1;
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         for (unsigned s = 0; s < size+i; ++s, ++S) {
            void* node;
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[i], &node));
            TEST(S == size_syncrunner(&srun));
         }
      }
      TEST(0 == free_syncrunner(&srun));
      TEST(0 == init_syncrunner(&srun));
      TEST(0 == size_syncrunner(&srun));
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   return EINVAL;
}

static int test_addfunc(void)
{
   syncrunner_t srun;
   uint16_t     size;
   syncfunc_t*  sfunc;

   // prepare
   TEST(0 == init_syncrunner(&srun));

   // TEST addfunc_syncrunner
   for (uintptr_t i = 1, s = 1; i; i <<= 1, ++s) {
      TEST(0 == addfunc_syncrunner(&srun, &dummy_sf, (void*)i));
      TEST(s == size_syncqueue(&srun.rwqueue[RUNQ_ID]));
      size  = elemsize_syncqueue(&srun.rwqueue[RUNQ_ID]);
      sfunc = last_queue(cast_queue(&srun.rwqueue[RUNQ_ID]), size);
      TEST(0 != sfunc)
      TEST(sfunc->mainfct    == &dummy_sf);
      TEST(sfunc->state      == (void*)i);
      TEST(sfunc->contoffset == 0);
      TEST(sfunc->optflags   == syncfunc_opt_NONE);
   }

   // TEST addfunc_syncrunner: ERROR
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   init_testerrortimer(&s_syncrunner_errtimer, 1, ENOMEM);
   TEST(ENOMEM == addfunc_syncrunner(&srun, &dummy_sf, 0));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == size_syncqueue(&srun.rwqueue[i]));
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
   void*        node;
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

   // TEST cleanup_after_exit
   for (int errcode = -4; errcode <= 4; ++errcode) {

      // does nothing
      cleanup_after_exit(0, 0, errcode);

      // TODO: reimplement test

   }

   // prepare
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   for (int i = 0; i < (int)lengthof(sfunc2); ++i) {
      TEST(0 == preallocate_syncqueue(&srun.rwqueue[WAITQ_ID], &node));
      sfunc2[i] = node;
      memset(sfunc2[i], 0, elemsize_syncqueue(&srun.rwqueue[WAITQ_ID]));
      sfunc2[i]->waitresult = i;
      sfunc2[i]->optflags = WAITQ_OPTFLAGS;
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

   // TEST wakeup_syncrunner: ERROR
   link_synccond(&cond, &sfunc[0]);
   TEST(EINVAL == wakeup_syncrunner(&srun, &cond));
   TEST(0 != isself_linkd(&srun.wakeup));
   TEST(&sfunc[0] == waitfunc_synccond(&cond));
   unlink_synccond(&cond);

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

   // TEST wakeupall_syncrunner: ERROR
   link_synccond(&cond, &sfunc[0]);
   TEST(EINVAL == wakeupall_syncrunner(&srun, &cond));
   TEST( isself_linkd(&srun.wakeup));
   TEST(&sfunc[0] == waitfunc_synccond(&cond));
   unlink_synccond(&cond);

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
   void*             node;

   // prepare
   s_test_srun = &srun;
   TEST(0 == init_syncrunner(&srun));
   init_synccond(&scond);

   // TEST CALL_SYNCFUNC
   for (int retcode = -2; retcode <= 2; ++retcode) {
      for (uint16_t contoffset = 0; contoffset <= 3; ++contoffset) {
         // prepare
         void* state = (void*)(uintptr_t)(contoffset*2);
         syncfunc_t sfunc;
         init_syncfunc(&sfunc, &test_call_sf, state, 0);
         sfunc.contoffset = contoffset;
         param.state = (void*)1;
         param.contoffset = (uint16_t)-1;
         s_test_cmd   = (uint32_t) -1;
         s_test_param = 0;
         s_test_set_cmd = retcode;
         // test
         TEST(retcode == CALL_SYNCFUNC(param, &sfunc));
         // check param
         TEST(param.state      == state);
         TEST(param.contoffset == contoffset);
         // check function parameter
         TEST(s_test_cmd   == (contoffset ? synccmd_CONTINUE : synccmd_RUN));
         TEST(s_test_param == &param);
      }
   }

   // TEST link_waitfields: wait for condition
   param.condition = &scond;
   {
      syncfunc_t*  sfunc[10];
      syncqueue_t* squeue = &srun.rwqueue[WAITQ_ID];
      for (int i = 0; i < (int)lengthof(sfunc); ++i) {
         TEST(0 == preallocate_syncqueue(squeue, &node));
         sfunc[i] = node;
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

   // TEST link_waitfields: ERROR (func not in queue)
   initself_linkd(&srun.wakeup);
   param.condition = &scond;
   {
      syncfunc_t sfunc = syncfunc_FREE;
      syncfunc_t from_other_srun = syncfunc_FREE;
      from_other_srun.optflags = syncfunc_opt_WAITFIELDS;
      link_synccond(&scond, &from_other_srun);
      sfunc.optflags = syncfunc_opt_WAITFIELDS;
      // test
      TEST(0 == waitresult_syncfunc(&sfunc));
      link_waitfields(&srun, &sfunc, &param);
      // check wait result EINVAL
      TEST(EINVAL == waitresult_syncfunc(&sfunc));
      // check added to wakeup list
      TEST(srun.wakeup.prev == waitlist_syncfunc(&sfunc));
      TEST(srun.wakeup.next == waitlist_syncfunc(&sfunc));
      TEST(&srun.wakeup == waitlist_syncfunc(&sfunc)->prev);
      TEST(&srun.wakeup == waitlist_syncfunc(&sfunc)->next);
      unlink_synccond(&scond);
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

static int test_wakeup_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   assert(s_test_srun == sfparam->srun);

   s_test_runcount += 1;
   s_test_errcount += (sfcmd != s_test_expect_cmd);
   s_test_errcount += (sfparam->state != s_test_expect_state);
   s_test_errcount += (sfparam->contoffset != s_test_expect_contoffset);
   s_test_errcount += (sfparam->err != s_test_expect_err);

   sfparam->contoffset = s_test_set_contoffset;
   sfparam->state = s_test_set_state;

   if (s_test_set_cmd == synccmd_WAIT) {
      sfparam->condition = s_test_set_condition;

   } else if (s_test_set_cmd == synccmd_EXIT) {
      sfparam->err = s_test_set_retcode;
   }

   return s_test_set_cmd;
}

static int test_exec_wakeup(void)
{
   syncrunner_t   srun;
   syncrunner_t   srun2;
   void*          node;
   syncfunc_t*    sfunc[10];
   synccond_t     scond;
   syncfunc_t*    dummy_sfunc;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun2));
   init_synccond(&scond);
   TEST(0 == preallocate_syncqueue(&srun2.rwqueue[WAITQ_ID], &node));
   dummy_sfunc = node;
   memset(dummy_sfunc, 0, WAITQ_ELEMSIZE);

   // TEST process_wakeup_list: empty list
   TEST(0 == process_wakeup_list(&srun));
   TEST(isself_linkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == size_syncqueue(&srun.rwqueue[i]));
   }

   // TEST process_wakeup_list: all possible wakeup parameter
   s_test_set_cmd = synccmd_EXIT;
   for (int isstate = 0; isstate <= 1; ++isstate) {
      s_test_expect_state = isstate ? (void*)(uintptr_t)0x123 : 0;
      for (int waitresult = 0; waitresult <= 256; waitresult += 64) {
         s_test_expect_err = waitresult;
         for (int contoffset = 0; contoffset <= 1; ++contoffset) {
            s_test_expect_contoffset = (uint16_t) contoffset;
            s_test_expect_cmd = contoffset ? synccmd_CONTINUE : synccmd_RUN;
            for (unsigned i = 0; i < lengthof(sfunc); ++i) {
               // TODO: move alloc+init function into helper function syncrunner_t
               // replace syncqueue with queue !!
               TEST(0 == preallocate_syncqueue(&srun.rwqueue[WAITQ_ID], &node));
               sfunc[i] = node;
               init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
               sfunc[i]->contoffset = (uint16_t) contoffset;
               setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
               initprev_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
            }

            // test process_wakeup_list
            s_test_runcount = 0;
            TEST(0 == process_wakeup_list(&srun));
            // check all sfunc woken up and exited
            TEST(0 == s_test_errcount);
            TEST(lengthof(sfunc) == s_test_runcount);
            TEST(0 == size_syncrunner(&srun));
            TEST(isself_linkd(&srun.wakeup));
         }
      }
   }

   // TEST process_wakeup_list: synccmd_RUN && synccmd_CONTINUE
   for (int retcmd = 0; retcmd <= 1; ++retcmd) {
      s_test_set_cmd = retcmd ? synccmd_CONTINUE : synccmd_RUN;
      s_test_expect_state = 0;
      s_test_expect_err = 0;
      s_test_expect_cmd = synccmd_RUN;
      s_test_expect_contoffset = 0;
      s_test_set_contoffset = 100;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? (void*)3 : (void*)0;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[WAITQ_ID], &node));
            sfunc[i] = node;
            init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
            setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
            initprev_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
         }

         // test process_wakeup_list
         s_test_runcount = 0;
         TEST(0 == process_wakeup_list(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc) == size_syncrunner(&srun));
         TEST(isself_linkd(&srun.wakeup));
         // check length of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST((i == RUNQ_ID ? lengthof(sfunc) : 0) == size_syncqueue(&srun.rwqueue[i]));
         }
         // check content of run queue
         unsigned i = 0;
         foreach (_queue, next, cast_queue(&srun.rwqueue[RUNQ_ID]), RUNQ_ELEMSIZE) {
            syncfunc_t* sf = next;
            TEST(sf->mainfct    == &test_wakeup_sf);
            TEST(sf->state      == s_test_set_state);
            TEST(sf->contoffset == (retcmd ? s_test_set_contoffset/*continue*/ : 0/*run*/));
            TEST(sf->optflags   == syncfunc_opt_NONE);
            ++ i;
         }
         TEST(i == lengthof(sfunc));
         // prepare for next run
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST process_wakeup_list: synccmd_WAIT
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
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[WAITQ_ID], &node));
            sfunc[i] = node;
            init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
            setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
            initprev_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
         }

         // test process_wakeup_list
         s_test_runcount = 0;
         TEST(0 == process_wakeup_list(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc) == size_syncrunner(&srun));
         TEST(isself_linkd(&srun.wakeup));
         TEST(iswaiting_synccond(&scond));
         TEST(sfunc[0] == waitfunc_synccond(&scond));
         // check length of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST((i == WAITQ_ID ? lengthof(sfunc) : 0) == size_syncqueue(&srun.rwqueue[i]));
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

   // TEST process_wakeup_list: wait error ==> add function to waitlist with wait result == EINVAL
   s_test_set_cmd = synccmd_WAIT;
   s_test_expect_state = 0;
   s_test_expect_err = 0;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   link_synccond(&scond, dummy_sfunc); // error 1
   for (int condition = 0; condition <= 1; ++condition) {
      s_test_set_condition = condition ? &scond/*error 1*/ : 0/*error 2*/;
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_set_contoffset = (uint16_t) contoffset;
         for (int setstate = 0; setstate <= 1; ++setstate) {
            s_test_set_state = setstate ? (void*)1 : (void*)0;
            for (unsigned i = 0; i < lengthof(sfunc); ++i) {
               TEST(0 == preallocate_syncqueue(&srun.rwqueue[WAITQ_ID], &node));
               sfunc[i] = node;
               init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
               setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
               initprev_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
            }

            // test process_wakeup_list
            s_test_runcount = 0;
            TEST(0 == process_wakeup_list(&srun));
            // check result
            TEST(0 == s_test_errcount);
            TEST(lengthof(sfunc) == s_test_runcount);
            TEST(lengthof(sfunc) == size_syncrunner(&srun));
            // check length of queues
            for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
               TEST((i == WAITQ_ID ? lengthof(sfunc) : 0) == size_syncqueue(&srun.rwqueue[i]));
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
   }
   unlink_synccond(&scond);

   // TEST process_wakeup_list: EINVAL (remove_queue)
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
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[WAITQ_ID], &node));
            sfunc[i] = node;
            init_syncfunc(sfunc[i], &test_wakeup_sf, s_test_expect_state, WAITQ_OPTFLAGS);
            setwaitresult_syncfunc(sfunc[i], s_test_expect_err);
            initnext_linkd(waitlist_syncfunc(sfunc[i]), &srun.wakeup);
         }
         // test process_wakeup_list
         s_test_runcount = 0;
         init_testerrortimer(&s_syncrunner_errtimer, errcount, EINVAL);
         TEST(EINVAL == process_wakeup_list(&srun));
         TEST(errcount < lengthof(sfunc) || isself_linkd(&srun.wakeup));
         // check result
         TEST(0 == s_test_errcount);
         TEST(errcount == s_test_runcount);
         // check size of queues
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST((i == WAITQ_ID ? lengthof(sfunc)-errcount : cmd ? errcount : 0) == size_syncqueue(&srun.rwqueue[i]));
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
   TEST(0 == free_syncrunner(&srun2));
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncrunner(&srun2);
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_run_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   assert(s_test_srun == sfparam->srun);

   s_test_runcount += 1;
   s_test_errcount += (sfparam->srun->isrun != 1);
   s_test_errcount += (sfcmd != s_test_expect_cmd);
   s_test_errcount += (sfparam->state != s_test_expect_state);
   s_test_errcount += (sfparam->contoffset != s_test_expect_contoffset);

   sfparam->contoffset = s_test_set_contoffset;
   sfparam->state = s_test_set_state;

   sfparam->condition = s_test_set_condition;
   sfparam->err       = s_test_set_retcode;

   return s_test_set_cmd;
}

static int test_exec_run(void)
{
   syncrunner_t   srun;
   syncrunner_t   srun2;
   syncfunc_t*    sfunc[10];
   void*          node;
   syncfunc_t*    dummy_sfunc;
   synccond_t     scond;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun2));
   init_synccond(&scond);
   TEST(0 == preallocate_syncqueue(&srun2.rwqueue[WAITQ_ID], &node));
   dummy_sfunc = node;
   memset(dummy_sfunc, 0, WAITQ_ELEMSIZE);

   // TEST run_syncrunner: EINPROGRESS
   srun.isrun = 1;
   TEST(EINPROGRESS == run_syncrunner(&srun));
   TEST(0 == size_syncrunner(&srun));
   TEST(1 == srun.isrun);
   TEST(isself_linkd(&srun.wakeup));
   srun.isrun = 0;

   // TEST run_syncrunner: empty queues
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
   TEST(0 == preallocate_syncqueue(&srun.rwqueue[RUNQ_ID], &node));
   init_syncfunc((syncfunc_t*)node, &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
   // test
   s_test_runcount = 0;
   TEST(0 == run_syncrunner(&srun));
   // check result
   TEST(0 == s_test_errcount);
   TEST(1 == s_test_runcount);
   TEST(0 == size_syncrunner(&srun));
   TEST(0 == srun.isrun);
   TEST(isself_linkd(&srun.wakeup));

   // TEST run_syncrunner: all possible run parameter
   s_test_set_cmd = synccmd_EXIT;
   s_test_set_condition = 0;
   s_test_set_contoffset = 0;
   s_test_set_retcode = 0;
   s_test_set_state = 0;
   for (int isstate = 0; isstate <= 1; ++isstate) {
      s_test_expect_state = isstate ? (void*)(uintptr_t)0x234 : 0;
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_expect_contoffset = (uint16_t) contoffset;
         s_test_expect_cmd = contoffset ? synccmd_CONTINUE : synccmd_RUN;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[RUNQ_ID], &node));
            sfunc[i] = node;
            // TODO: add contoffset to init_syncfunc !!
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

   // TEST run_syncrunner: synccmd_RUN && synccmd_CONTINUE
   for (int retcmd = 0; retcmd <= 1; ++retcmd) {
      s_test_set_cmd = retcmd ? synccmd_CONTINUE : synccmd_RUN;
      s_test_expect_cmd = synccmd_RUN;
      s_test_expect_contoffset = 0;
      s_test_expect_state = 0;
      s_test_set_contoffset = 100;
      s_test_set_state = 0;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? (void*)9 : (void*)0;
         for (int isstate = 0; isstate <= 1; ++isstate) {
            // generate running functions
            for (unsigned i = 0; i < lengthof(sfunc); ++i) {
               TEST(0 == preallocate_syncqueue(&srun.rwqueue[RUNQ_ID], &node));
               sfunc[i] = node;
               init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
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
            // check size of single queues
            for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
               TEST((i == RUNQ_ID ? lengthof(sfunc) : 0) == size_syncqueue(&srun.rwqueue[i]));
            }
            // check content of run queue
            unsigned i = 0;
            foreach (_queue, next, cast_queue(&srun.rwqueue[RUNQ_ID]), RUNQ_ELEMSIZE) {
               TEST(sfunc[i] == next);
               TEST(sfunc[i]->mainfct    == &test_run_sf);
               TEST(sfunc[i]->state      == s_test_set_state);
               TEST(sfunc[i]->contoffset == (retcmd ? s_test_set_contoffset : 0));
               TEST(sfunc[i]->optflags   == syncfunc_opt_NONE);
               ++i;
            }
            TEST(lengthof(sfunc) == i);
            // prepare for next run
            TEST(0 == free_syncrunner(&srun));
            TEST(0 == init_syncrunner(&srun));
         }
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
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[RUNQ_ID], &node));
            sfunc[i] = node;
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
            TEST((i == WAITQ_ID ? lengthof(sfunc) : 0) == size_syncqueue(&srun.rwqueue[i]));
         }
         // check scond
         TEST(iswaiting_synccond(&scond));
         TEST(&srun.rwqueue[WAITQ_ID] == castPaddr_syncqueue(waitfunc_synccond(&scond)));
         // check content of wait queue
         unsigned i = 0;
         linkd_t* wlprev = &scond.waitfunc;
         foreach (_queue, next, cast_queue(&srun.rwqueue[WAITQ_ID]), WAITQ_ELEMSIZE) {
            syncfunc_t* sf = next;
            TEST(sf->mainfct    == &test_run_sf);
            TEST(sf->state      == s_test_set_state);
            TEST(sf->contoffset == s_test_set_contoffset);
            TEST(sf->optflags   == WAITQ_OPTFLAGS);
            TEST(sf->waitresult == 0);
            TEST(sf->waitlist.prev == wlprev);
            TEST(&sf->waitlist     == wlprev->next);
            wlprev = &sf->waitlist;
            ++i;
         }
         TEST(i == lengthof(sfunc));
         // prepare for next run
         unlinkall_synccond(&scond);
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST run_syncrunner: wait error ==> add function to waitlist with wait result == EINVAL
   s_test_set_cmd = synccmd_WAIT;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   link_synccond(&scond, dummy_sfunc); // error 1
   for (int condition = 0; condition <= 1; ++condition) {
      s_test_set_condition = condition ? &scond/*error 1*/ : 0/*error 2*/;
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_set_contoffset = (uint16_t) contoffset;
         for (int setstate = 0; setstate <= 1; ++setstate) {
            s_test_set_state = setstate ? (void*)2 : (void*)0;
            for (unsigned i = 0; i < lengthof(sfunc); ++i) {
               TEST(0 == preallocate_syncqueue( &srun.rwqueue[RUNQ_ID], &node));
               sfunc[i] = node;
               init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
            }
            // test run_syncrunner
            s_test_runcount = 0;
            TEST(0 == run2_syncrunner(&srun, false));
            // check result
            TEST(0 == s_test_errcount);
            TEST(lengthof(sfunc) == s_test_runcount);
            TEST(lengthof(sfunc) == size_syncrunner(&srun));
            TEST(0 == srun.isrun);
            TEST(isvalid_linkd(&srun.wakeup) && ! isself_linkd(&srun.wakeup));
            // check length of queues
            for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
               TEST((i == WAITQ_ID ? lengthof(sfunc) : 0) == size_syncqueue(&srun.rwqueue[i]));
            }
            // check content of wait queue
            unsigned i = 0;
            linkd_t* wlprev = &srun.wakeup;
            foreach (_queue, next, cast_queue(&srun.rwqueue[WAITQ_ID]), WAITQ_ELEMSIZE) {
               syncfunc_t* sf = next;
               TEST(sf->mainfct    == &test_run_sf);
               TEST(sf->state      == s_test_set_state);
               TEST(sf->contoffset == s_test_set_contoffset);
               TEST(sf->optflags   == WAITQ_OPTFLAGS);
               TEST(sf->waitresult == EINVAL);
               TEST(sf->waitlist.prev == wlprev);
               TEST(&sf->waitlist     == wlprev->next);
               wlprev = &sf->waitlist;
               ++i;
            }
            TEST(i == lengthof(sfunc));
            // prepare for next run
            TEST(0 == free_syncrunner(&srun));
            TEST(0 == init_syncrunner(&srun));
         }
      }
   }
   unlink_synccond(&scond);

   // TEST run_syncrunner: EINVAL (remove_queue)
   s_test_set_cmd = synccmd_EXIT;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   for (unsigned errcount = 1; errcount <= lengthof(sfunc); ++errcount) {
      for (unsigned i = 0; i < lengthof(sfunc); ++i) {
         TEST(0 == preallocate_syncqueue(&srun.rwqueue[RUNQ_ID], &node));
         sfunc[i] = node;
         init_syncfunc(sfunc[i], &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
      }
      // test run_syncrunner
      s_test_runcount = 0;
      init_testerrortimer(&s_syncrunner_errtimer, errcount, EINVAL);
      TEST(EINVAL == run_syncrunner(&srun));
      // check result
      TEST(0 == s_test_errcount);
      TEST(errcount == s_test_runcount);
      TEST(lengthof(sfunc)-errcount == size_syncrunner(&srun));
      TEST(0 == srun.isrun);
      TEST(isself_linkd(&srun.wakeup));
      // check length of every single queue
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         TEST((i == RUNQ_ID ? lengthof(sfunc)-errcount : 0) == size_syncqueue(&srun.rwqueue[i]));
      }
      // check content of run queue
      unsigned i = 0;
      foreach (_queue, next, cast_queue(&srun.rwqueue[RUNQ_ID]), RUNQ_ELEMSIZE) {
         TEST(sfunc[i] == next);
         TEST(sfunc[i]->mainfct    == &test_run_sf);
         TEST(sfunc[i]->state      == s_test_expect_state);
         TEST(sfunc[i]->contoffset == 0);
         TEST(sfunc[i]->optflags   == syncfunc_opt_NONE);
         ++i;
      }
      TEST(i == lengthof(sfunc)-errcount);
      // prepare for next run
      TEST(0 == free_syncrunner(&srun));
      TEST(0 == init_syncrunner(&srun));
   }

   // unprepare
   free_synccond(&scond);
   TEST(0 == free_syncrunner(&srun2));
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_synccond(&scond);
   free_syncrunner(&srun2);
   free_syncrunner(&srun);
   return EINVAL;
}

static int wait_sf(syncfunc_param_t* param, uint32_t cmd)
{
   syncfunc_START: ;

   s_test_runcount += 1;
   s_test_errcount += (param->srun->isrun != 1);
   s_test_errcount += (param->state != s_test_expect_state);

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
   syncrunner_t   srun  = syncrunner_FREE;
   synccond_t     scond = synccond_FREE;
   unsigned       qidx;
   uint16_t       size;
   syncqueue_t *  squeue;
   void*          node;
   syncfunc_t*    sf;
   unsigned       MAX_PER_QUEUE = 5000;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));
   init_synccond(&scond);

   // TEST terminate_syncrunner: different parameter values
   s_test_set_cmd = synccmd_EXIT;
   s_test_expect_cmd = synccmd_EXIT;
   s_test_expect_state = 0;
   s_test_set_condition = 0;
   s_test_set_contoffset = 1;
   s_test_set_retcode = 100;
   s_test_set_state = 0;
   for (int isstate = 0; isstate <= 1; ++isstate) {
      s_test_expect_state = isstate ? (void*)(uintptr_t)0x234 : 0;
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_expect_contoffset = (uint16_t) contoffset;
         for (qidx = 0; qidx < lengthof(srun.rwqueue); ++qidx) {
            squeue = &srun.rwqueue[qidx];
            size   = elemsize_syncqueue(squeue);
            for (unsigned i = 0; i < MAX_PER_QUEUE; ++i) {
               TEST(0 == preallocate_syncqueue(squeue, &node));
               sf = node;
               memset(sf, 0, size);
               init_syncfunc(sf, &test_run_sf, s_test_expect_state, syncfunc_opt_NONE);
               sf->contoffset = s_test_expect_contoffset;
               if (qidx == WAITQ_ID) {
                  initprev_linkd(waitlist_syncfunc(sf), &srun.wakeup);
               }
            }
         }
         // test terminate_syncrunner
         s_test_runcount = 0;
         TEST(0 == terminate_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(srun.rwqueue)*MAX_PER_QUEUE == s_test_runcount);
         TEST(isself_linkd(&srun.wakeup));
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST(0 == size_syncqueue(&srun.rwqueue[i]));
         }
      }
   }

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
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == size_syncqueue(&srun.rwqueue[i]));
   }

   // TEST terminate_syncrunner: EINVAL (remove_queue)
   s_test_set_cmd = synccmd_EXIT;
   s_test_expect_cmd = synccmd_EXIT;
   s_test_expect_contoffset = 0;
   for (unsigned iswait = 0; iswait <= 1; ++iswait) {
      for (unsigned errcount = 1; errcount <= 3; ++errcount) {
         for (int isstate = 0; isstate <= 1; ++isstate) {
            s_test_expect_state = isstate ? (void*)5 : (void*)0;
            qidx   = iswait ? WAITQ_ID : RUNQ_ID;
            squeue = &srun.rwqueue[qidx];
            size   = elemsize_syncqueue(squeue);
            for (unsigned i = 0; i < 3; ++i) {
               TEST(0 == preallocate_syncqueue(squeue, &node));
               sf = node;
               memset(sf, 0, size);
               init_syncfunc(sf, &test_run_sf, s_test_expect_state, iswait ? WAITQ_OPTFLAGS : RUNQ_OPTFLAGS);
               if (iswait) initprev_linkd(waitlist_syncfunc(sf), &srun.wakeup);
            }
            // test terminate_syncrunner
            init_testerrortimer(&s_syncrunner_errtimer, errcount, EINVAL);
            s_test_runcount = 0;
            TEST(EINVAL == terminate_syncrunner(&srun));
            // check result
            TEST(0 == s_test_errcount);
            TEST(errcount == s_test_runcount);
            TEST(isself_linkd(&srun.wakeup));
            for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
               TEST((i == qidx ? 3-errcount : 0) == size_syncqueue(&srun.rwqueue[i]));
            }
            // prepare for next run
            TEST(0 == free_syncrunner(&srun));
            TEST(0 == init_syncrunner(&srun));
         }
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
   start_syncfunc(param, cmd, ONRUN, ONEXIT);

ONRUN:
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
   start_syncfunc(param, cmd, ONRUN, ONEXIT);

ONRUN:
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

// TODO: test example3_sf
static int example3_sf(syncfunc_param_t * param, uint32_t cmd)
{
   unsigned id = (unsigned) state_syncfunc(param);
   example_state_t * state = &s_example_state[id];
   ++ state->runcount;
   start_syncfunc(param, cmd, ONRUN, ONEXIT);

ONRUN:
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
   TEST(iswaiting_synccond(&cond));
   TEST(lengthof(s_example_state)/2 == size_syncqueue(&srun.rwqueue[RUNQ_ID]));
   TEST(lengthof(s_example_state)/2 == size_syncqueue(&srun.rwqueue[WAITQ_ID]));
   TEST(isself_linkd(&srun.wakeup));
   // 50% call wakeup and exit / 50% other woken up and exit
   TEST(0 == run_syncrunner(&srun));
   TEST(0 == size_syncrunner(&srun));
   // check no error
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
