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
#include "C-kern/api/task/syncwait.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/process.h"
#endif
#ifdef KONFIG_PERFTEST
#include "C-kern/api/test/perftest.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/memory/memblock.h"
#endif


// section: syncrunner_queue_t

// === Invariants ==
// 1. syncfunc_t->mainfct != 0 ==> not stored in free list
// 2. syncfunc_t->mainfct == 0 ==> syncfunc_t->waitnode linked into syncrunner_queue_t->freelist

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_sq_errtimer
 * Simulate errors in <free_syncrunner>. */
static test_errortimer_t s_sq_errtimer = test_errortimer_FREE;
#endif

// group: constants

/* define: ELEMSIZE
 * The size of elements stored in <syncrunner_queue_t>. */
#define ELEMSIZE (sizeof(syncfunc_t))

/* define: NRELEMPERPAGE
 * The maximum nr of elements which could be stored on a single queue page. */
#define NRELEMPERPAGE (lengthof(((syncrunner_page_t*)0)->sfunc))

// group: lifetime

static int init_sq(/*out*/syncrunner_queue_t* sq)
{
   sq->first = 0;
   sq->firstfree = 0;
   initself_linkd(&sq->freelist);
   sq->freelist_size = 0;
   sq->size = 0;
   sq->nextfree = 0;
   sq->nrfree = 0;

   return 0;
}

static int shrink_sq(syncrunner_queue_t* sq); // forward

static int free_sq(syncrunner_queue_t* sq)
{
   int err = 0;

   while (sq->first) {

      sq->nrfree = NRELEMPERPAGE;
      int err2 = shrink_sq(sq);
      if (err2) err = err2;
   }

   sq->first   = 0;
   sq->firstfree = 0;
   initinvalid_linkd(&sq->freelist);
   sq->freelist_size = 0;
   sq->size   = 0;
   sq->nextfree = 0;
   sq->nrfree = 0;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: memory-management

static int grow_sq(syncrunner_queue_t* sq)
{
   int err;
   memblock_t mblock;
   syncrunner_page_t *page;

   // allocate pagewise !!

   static_assert(sizeof(syncrunner_page_t) <= 4096, "pagesize_4096 is sufficient");
   static_assert(sizeof(syncrunner_page_t) >  4096-sizeof(syncfunc_t), "pagesize_4096 is not too large");

   if (! PROCESS_testerrortimer(&s_sq_errtimer, &err)) {
      err = ALLOC_PAGECACHE(pagesize_4096, &mblock);
   }
   if (err) goto ONERR;

   page = (syncrunner_page_t*) mblock.addr;
   if (0 == sq->first) {
      initself_linkd(&page->otherpages);
      sq->first = page;
      sq->firstfree = page;
   } else {
      initprev_linkd(&page->otherpages, &sq->first->otherpages);
   }

   sq->size   += NRELEMPERPAGE;
   sq->nrfree += NRELEMPERPAGE;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

static int shrink_sq(syncrunner_queue_t* sq)
{
   int err;

   if (sq->nrfree < NRELEMPERPAGE) return 0;

   sq->size   -= NRELEMPERPAGE;
   sq->nrfree -= NRELEMPERPAGE;

   syncrunner_page_t *lastpage = (syncrunner_page_t*)sq->first->otherpages.prev;
   memblock_t mblock = memblock_INIT(4096, (uint8_t*)lastpage);

   if (lastpage == sq->first) {
      sq->first = 0;
      sq->firstfree = 0;
   } else {
      unlink_linkd(&lastpage->otherpages);
   }

   err = RELEASE_PAGECACHE(&mblock);
   (void) PROCESS_testerrortimer(&s_sq_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

static inline void sfalloc_sq(syncrunner_queue_t* sq, /*out*/syncfunc_t **sf)
{
   if (sq->freelist_size) {
      linkd_t* freenode = sq->freelist.next;
      -- sq->freelist_size;
      unlink_linkd(freenode);
      *sf = castPwaitnode_syncfunc(freenode);
   } else {
      assert(sq->nrfree);
      -- sq->nrfree;
      if (sq->nextfree == NRELEMPERPAGE) {
         sq->nextfree = 0;
         sq->firstfree = (syncrunner_page_t*) sq->firstfree->otherpages.next;
      }
      *sf = &sq->firstfree->sfunc[sq->nextfree++];
   }
}

/* Unchecked Precondition:
 * o sfunc is unlinked from any list. */
static inline void sffree_sq(syncrunner_queue_t* sq, syncfunc_t* sfunc)
{
   sfunc->mainfct = 0;
   initprev_linkd(waitnode_syncfunc(sfunc), &sq->freelist);
   ++ sq->freelist_size;
}

/* Die Funktionen auf der letzten verwendeten Seite der Queue werden
 * auf die freien Einträge, referenziert durch sq->freelist, verschoben.
 * Damit werden "Löcher" in den Seiten gefüllt und Funktionen am Ende
 * der Queue entfernt. Wird die letzte Seite komplett unbelegt, ist
 * sie ein Kandidat, später freigegeben zu werden mittels <syncrunner_queue_t.shrink_sq>. */
static inline void compact_sq(syncrunner_queue_t* sq)
{
   if (! sq->freelist_size) return; // no holes (==free entries), no compaction needed

   syncrunner_page_t *page = sq->firstfree;
   size_t   const nrshrink = sq->freelist_size < sq->nextfree ? sq->freelist_size : sq->nextfree;

   // free entries are removed from freelist so that function are never moved twice
   for (size_t i = 1; i <= nrshrink; ++i) {
      syncfunc_t *sf = &page->sfunc[sq->nextfree-i];
      if (0 == sf->mainfct) {  // in freelist
         unlink_linkd(waitnode_syncfunc(sf));
      }
   }

   // move allocated functions to free entries which are not on the next nrshrink entries
   for (size_t i = 1; i <= nrshrink; ++i) {
      syncfunc_t *sf = &page->sfunc[sq->nextfree-i];
      if (0 != sf->mainfct) {  // in use
         linkd_t *freenode = sq->freelist.next;
         syncfunc_t *dest = castPwaitnode_syncfunc(freenode);
         unlink_linkd(freenode);
         initmove_syncfunc(dest, sf);
      }
   }

   sq->freelist_size -= nrshrink;
   sq->nrfree += nrshrink;
   sq->nextfree -= nrshrink;
   if (0 == sq->nextfree && sq->firstfree != sq->first/*previous page available?*/) {
      sq->nextfree  = NRELEMPERPAGE;
      sq->firstfree = (syncrunner_page_t*) sq->firstfree->otherpages.prev;
   }
}

static inline int clear_sq(syncrunner_queue_t* sq)
{
   int err = free_sq(sq);
   initself_linkd(&sq->freelist);
   return err;
}

// section: syncrunner_t

// group: constants

/* define: RUN_QID
 * The index into <syncrunner_t.sq> for the single run queue. */
#define RUN_QID 0

/* define: WAIT_QID
 * The index into <syncrunner_t.sq> for the single run queue. */
#define WAIT_QID 1

// group: lifetime

int init_syncrunner(/*out*/syncrunner_t *srun)
{
   int err;
   unsigned qidx;

   static_assert( RUN_QID == 0 && WAIT_QID == 1 && lengthof(srun->sq) == 2,
                  "all queues have an assigned id");

   for (qidx = 0; qidx < lengthof(srun->sq); ++qidx) {
      err = init_sq(&srun->sq[qidx]);
      if (err) goto ONERR;
   }

   initself_linkd(&srun->wakeup);
   srun->isrun = false;
   srun->isterminate = false;

   return 0;
ONERR:
   while (qidx > 0) {
      (void) free_sq(&srun->sq[--qidx]);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_syncrunner(syncrunner_t *srun)
{
   int err = 0;
   int err2;

   for (unsigned i = 0; i < lengthof(srun->sq); ++i) {
      err2 = free_sq(&srun->sq[i]);
      if (err2) err = err2;
   }

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: queue-helper

/* function: allocfunc_syncrunner
 * Allokiert nodesize Bytes an Speicher für eine syncfunc_t in der Queue mit ID queueud.
 * Unchecked Precondition:
 * o queueid == RUN_QID || queueid == WAIT_QID */
static inline void allocfunc_syncrunner(syncrunner_t *srun, unsigned queueid, /*out*/syncfunc_t** sfunc)
{
   syncrunner_queue_t* sq = &srun->sq[queueid];
   sfalloc_sq(sq, sfunc);
}

/* function: removefunc_syncrunner
 * Lösche sfunc aus squeue.
 * Entweder wird das letzte Element in squeue nach sfunc kopiert
 * oder, wenn das letzte Element das preallokierte Element ist,
 * einfach sfunc als neues freies Element markiert.
 * Das letzte Element wird dann aus der Queue gelöscht.
 *
 * Unchecked Precondition:
 * o sfunc not linked in freelist or wakeup list
 * o Links of sfunc are invalid
 * o sfunc ∈ sq[queueid].queue / (&sq[queueid].queue == castPaddr_queue(sfunc, defaultpagesize_queue())
 * */
static inline void removefunc_syncrunner(syncrunner_t *srun, unsigned qid, syncfunc_t* sfunc)
{
   syncrunner_queue_t* sq = &srun->sq[qid];
   sffree_sq(sq, sfunc);
}

/* Kompaktiert Queues und löscht die letzte leere Seite, falls
 * noch mindestens NRELEMPERPAGE weitere Elemente nicht belegt sind.
 * */
static inline int shrinkqueues_syncrunner(syncrunner_t *srun)
{
   int err;

   // remove unnecessary pages
   // but ensure that moving between queues is always possible

   size_t run_nrfree = srun->sq[RUN_QID].freelist_size +srun->sq[RUN_QID].nrfree;
   size_t wait_nrfree = srun->sq[WAIT_QID].freelist_size +srun->sq[WAIT_QID].nrfree;
   size_t run_inuse = srun->sq[RUN_QID].size - run_nrfree;
   size_t wait_inuse = srun->sq[WAIT_QID].size - wait_nrfree;
   if (NRELEMPERPAGE < run_nrfree && wait_inuse < run_nrfree - NRELEMPERPAGE) {
      compact_sq(&srun->sq[RUN_QID]);
      err = shrink_sq(&srun->sq[RUN_QID]);
      if (err) goto ONERR;
   }
   if (NRELEMPERPAGE < wait_nrfree && run_inuse < wait_nrfree - NRELEMPERPAGE) {
      compact_sq(&srun->sq[WAIT_QID]);
      err = shrink_sq(&srun->sq[WAIT_QID]);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* Erweitert jede Queue um eine weitere Seite, falls nötig.
 * Immer wenn die Anzahl allokierter Plätze einer Queue größer
 * oder gleich der Anzahl freier Plätze der anderen Queue ist,
 * werden die Anzahl freier Plätze erhöht.
 *
 * Nach Return ist gesichert, dass eine weitere Funktion allokiert
 * werden kann. Und diese beliebig von einer Queue in die andere
 * verschoben werden kann.
 *
 * Pro allokierter Funktion werden also in beiden Queues ein freier
 * Platz erzeugt.
 * */
static inline int growqueues_syncrunner(syncrunner_t *srun)
{
   int err;

   // ensure that moving between queues is always possible
   size_t run_nrfree = srun->sq[RUN_QID].freelist_size +srun->sq[RUN_QID].nrfree;
   size_t wait_nrfree = srun->sq[WAIT_QID].freelist_size +srun->sq[WAIT_QID].nrfree;
   size_t run_inuse = srun->sq[RUN_QID].size - run_nrfree;
   size_t wait_inuse = srun->sq[WAIT_QID].size - wait_nrfree;
   if (wait_inuse >= run_nrfree) {
      err = grow_sq(&srun->sq[RUN_QID]);
      if (err) goto ONERR;
   }
   if (run_inuse >= wait_nrfree) {
      err = grow_sq(&srun->sq[WAIT_QID]);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: clearqueue_syncrunner
 * Speicher der Queue wird freigegeben.
 *
 * Unchecked Precondition:
 * o queueid == RUN_QID || queueid == WAIT_QID
 * o if (queueid == WAIT_QID) ==> isself_linkd(srun->wakup) == true
 * o All contained functions must be in a unlinked/free state. */
static inline int clearqueue_syncrunner(syncrunner_t *srun, unsigned queueid)
{
   int err = clear_sq(&srun->sq[queueid]);
   return err;
}

// group: query

bool iswakeup_syncrunner(const syncrunner_t *srun)
{
   return ! isself_linkd(&srun->wakeup);
}

size_t size_syncrunner(const syncrunner_t *srun)
{
   size_t size = 0;

   for (unsigned i = 0; i < lengthof(srun->sq); ++i) {
      size += srun->sq[i].size-srun->sq[i].nrfree-srun->sq[i].freelist_size;
   }

   return size;
}

// group: update

int addfunc_syncrunner(syncrunner_t *srun, syncfunc_f mainfct, void* state)
{
   int err;
   syncfunc_t *sf;

   VALIDATE_INPARAM_TEST(mainfct != 0, ONERR, );

   if (srun->isterminate) return EAGAIN;

   // ensure that moving between queues is always possible
   err = growqueues_syncrunner(srun);
   if (err) goto ONERR;

   allocfunc_syncrunner(srun, RUN_QID, &sf);
   init_syncfunc(sf, mainfct, state);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* Füge einzelnes waitlist von <syncfunc_t> zu <syncrunner_t.wakeup> hinzu. */
int wakeup_syncrunner(syncrunner_t *srun, struct syncwait_t *swait)
{
   if (! iswaiting_syncwait(swait)) return EAGAIN;

   linkd_t    *node = removenode_syncwait(swait);
   syncfunc_t *sf = castPwaitnode_syncfunc(node);
   seterr_syncfunc(sf, 0);
   initprev_linkd(node, &srun->wakeup); // after(srun->wakeup->prev == node)

   return 0;
}

/* Füge waitlist von swait zu <syncrunner_t.wakeup> hinzu. */
int wakeupall_syncrunner(syncrunner_t *srun, struct syncwait_t *swait)
{
   if (! iswaiting_syncwait(swait)) return EAGAIN;

   linkd_t *first = removelist_syncwait(swait);

   linkd_t *next = first;
   do {
      syncfunc_t *sf = castPwaitnode_syncfunc(next);
      seterr_syncfunc(sf, 0);
      next = next->next;
   } while (next != first);

   splice_linkd(first, &srun->wakeup); // add to wakeup list

   return 0;
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
            (param.sfunc->mainfct(&param))

/* define: END_SYNCFUNC
 * <syncfunc_t.mainfct> wird mit Kommando <synccmd_RUN> aufgerufen,
 * vorher wird aber <syncfunc_t.contoffset> auf Ende-Marke gesetzt,
 * so daß die Funktion korrekt beendet wird.
 *
 * Nach Ausführen wird die Funktion aus allen verlinkten Listen entfernt.
 *
 * Sollte die Funktion noch niemals aktiviert worden sein, wird
 * sie einmalig aktiviert, damit <syncfunc_t.endoffset> korrekt
 * initialisiert wurde.
 *
 * Der Fehlercode wird vor Aufruf auf ECANCELED gesetzt (siehe <syncfunc_t.seterr_syncfunc>).
 *
 * Parameter:
 * param - Name von lokalem <syncfunc_param_t>. Wird mehrfach ausgewertet !!
 *
 * Unchecked Precondition:
 * o param.srun points to a valid <syncrunner_t>.
 * o param.sfunc points to a valid <syncfunc_t>. */
#define END_SYNCFUNC(param) \
         do {                                               \
            if (0 != (param).sfunc->endoffset               \
               /* run at least once ! */                    \
               /* => (param).sfunc->endoffset != 0 */       \
                || synccmd_EXIT != RUN_SYNCFUNC(param)) {   \
                  (param).sfunc->contoffset =               \
                     (param).sfunc->endoffset;              \
                     (param).sfunc->err = ECANCELED;        \
                     RUN_SYNCFUNC(param);                   \
            }                                               \
            unlink_syncfunc(param.sfunc);                   \
         } while(0)

static inline void process_cmd(syncfunc_param_t *param, int cmd, const unsigned qid)
{
   syncfunc_t *sf = param->sfunc;

   if (synccmd_RUN == cmd) {
      if (WAIT_QID == qid) { // move from wait to run queue ?
         allocfunc_syncrunner(param->srun, RUN_QID, &param->sfunc);
         initcopy_syncfunc(param->sfunc, sf);
         removefunc_syncrunner(param->srun, WAIT_QID, sf);
      }

   } else if (synccmd_WAIT == cmd) {
      if (RUN_QID == qid) { // move from run to wait queue ?
         allocfunc_syncrunner(param->srun, WAIT_QID, &param->sfunc);
         initcopy_syncfunc(param->sfunc, sf);
         removefunc_syncrunner(param->srun, RUN_QID, sf);
      }
      linkwaitnode_syncfunc(param->sfunc, param->waitlist);

   } else { // synccmd_EXIT or "invalid value"
      removefunc_syncrunner(param->srun, qid, sf);
   }
}

/* function: process_wakeuplist
 * Starte alle in <syncrunner_t.wakeup> gelisteten <syncfunc_t>.
 * Der Rückgabewert vom Typ <synccmd_e> entscheidet, ob die Funktion weiterhin
 * in der Wartequeue verbleibt oder in die Runqueue verschoben wird.
 *
 * <syncfunc_param_t.err> = <syncfunc_t.waitresult>.
 *
 * Unchecked Precondition:
 * !isself_linkd(&srun->wakeup)
 * */
static inline void process_wakeuplist(syncrunner_t *srun)
{
   int cmd;

   syncfunc_param_t param = syncfunc_param_INIT(srun);

   // build shadow wakeup list
   // ensure that newly woken up syncfunc_t are not executed this time

   linkd_t wakeup = srun->wakeup;
   relink_linkd(&wakeup);
   initself_linkd(&srun->wakeup);

   // loop over all entries in shadow wakeup list

   while (wakeup.next != &wakeup) {
      param.sfunc = castPwaitnode_syncfunc(wakeup.next);
      unlink_linkd(wakeup.next);

      cmd = RUN_SYNCFUNC(param);

      process_cmd(&param, cmd, WAIT_QID);
   }
}

static inline int exec_syncrunner(syncrunner_t *srun)
{
   int err;
   int cmd;
   syncfunc_param_t param = syncfunc_param_INIT(srun);

   // run every entry in run queue once

   syncrunner_page_t *page = srun->sq[RUN_QID].first;
   syncrunner_page_t *lastpage = srun->sq[RUN_QID].firstfree;
   size_t             lastsize = srun->sq[RUN_QID].nextfree;

   for (; page != lastpage; page = (syncrunner_page_t*)page->otherpages.next) {
      for (size_t i=0; i<NRELEMPERPAGE; ++i) {
         param.sfunc = &page->sfunc[i];
         cmd = RUN_SYNCFUNC(param);
         process_cmd(&param, cmd, RUN_QID);
      }
   }
   for (size_t i=0; i<lastsize; ++i) {
      param.sfunc = &page->sfunc[i];
      cmd = RUN_SYNCFUNC(param);
      process_cmd(&param, cmd, RUN_QID);
   }

   // removes every hole in array
   if (srun->sq[RUN_QID].freelist_size) {
      while (srun->sq[RUN_QID].freelist_size) {
         compact_sq(&srun->sq[RUN_QID]);
      }
      if (srun->sq[RUN_QID].nrfree > NRELEMPERPAGE) {
         err = shrinkqueues_syncrunner(srun);
         if (err) goto ONERR;
      }
   }
   if (! isself_linkd(&srun->wakeup)) {
      process_wakeuplist(srun);
      if (srun->sq[WAIT_QID].freelist_size > NRELEMPERPAGE) {
         err = shrinkqueues_syncrunner(srun);
         if (err) goto ONERR;
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int run_syncrunner(syncrunner_t *srun)
{
   int err;

   if (srun->isrun) return EINPROGRESS;

   // prepare
   srun->isrun = true;

   err = exec_syncrunner(srun);

   // unprepare
   srun->isrun = false;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int terminate_syncrunner(syncrunner_t *srun)
{
   int err = 0;
   syncfunc_param_t param = syncfunc_param_INIT(srun);

   if (srun->isrun) return EINPROGRESS;

   // prepare
   srun->isrun = true;
   srun->isterminate = true;

   for (unsigned qid = 0; qid < lengthof(srun->sq); ++qid) {
      syncrunner_page_t *page = srun->sq[qid].first;
      syncrunner_page_t *lastpage = srun->sq[qid].firstfree;
      size_t             lastsize = srun->sq[qid].nextfree;

      for (;;) {
         size_t size = (lastpage != page ? NRELEMPERPAGE : lastsize);
         for (size_t i=0; i<size; ++i) {
            param.sfunc = &page->sfunc[i];
            if (param.sfunc->mainfct != 0) {
               END_SYNCFUNC(param);
            }
         }
         if (page == lastpage) break;
         page = (syncrunner_page_t*)page->otherpages.next;
      }
   }

   for (unsigned qid = 0; qid < lengthof(srun->sq); ++qid) {
      int err2 = clearqueue_syncrunner(srun, qid);
      if (err2) err = err2;
   }

   // unprepare
   initself_linkd(&srun->wakeup);
   srun->isterminate = false;
   srun->isrun = false;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_PERFTEST

static int syncfunc_client(syncfunc_param_t* param) __attribute__((noinline));
static int syncfunc_server(syncfunc_param_t* param) __attribute__((noinline));

typedef struct state_t {
   uintptr_t count;
   void*     msg;
} state_t;

static int syncfunc_client(syncfunc_param_t* param)
{
   state_t* state = state_syncfunc(param);
   if (state->msg) {
      // printf("client: %d\n",(int)state->msg);
      state->msg = 0; // processed !
   }
   return synccmd_RUN;
}

static int syncfunc_server(syncfunc_param_t* param)
{
   state_t* state = state_syncfunc(param);
   if (!state->msg) {
      state->msg = (void*) (++state->count); // generate new msg
      // printf("server: %d\n",state->count);
   }
   return synccmd_RUN;
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

   return 0;
}

static int pt_unprepare(perftest_instance_t* tinst)
{
   memblock_t mblock = memblock_INIT(tinst->size, tinst->addr);
   int err = terminate_syncrunner(syncrunner_maincontext());
   int err2 = FREE_MM(&mblock);
   if (err2) err = err2;
   return err;
}

static int pt_run(perftest_instance_t* tinst)
{
   state_t* state = tinst->addr;
   syncrunner_t *srun = syncrunner_maincontext();
   while (state->count < tinst->nrops) {
      exec_syncrunner(srun);
      //int err = run_syncrunner(srun);
      //if (err) return err;
   }
   return 0;
}

static int pt_run_raw(perftest_instance_t* tinst)
{
   syncfunc_param_t param = syncfunc_param_FREE;
   syncfunc_t sfunc = syncfunc_FREE;
   state_t *state = tinst->addr;

   sfunc.state = state;
   param.sfunc = &sfunc;
   assert(state->count == 0);
   assert(state->msg   == 0);
   while (state->count < tinst->nrops) {
      syncfunc_server(&param);
      syncfunc_client(&param);
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

// Ruft sfalloc auf geschützt durch ein eigens dafür gestarteten Childprozess.
// Damit kann auf Abbruch durch assert geprüft werden.
static int childprocess_sfalloc(void *_sq)
{
   syncrunner_queue_t *sq = _sq;
   syncfunc_t *sf;
   sfalloc_sq(sq, &sf);
   return 0;
}

// Ruft allocfunc auf geschützt durch ein eigens dafür gestarteten Childprozess.
// Damit kann auf Abbruch durch assert geprüft werden.
static int childprocess_allocfunc(void *_srun)
{
   syncrunner_t *srun = _srun;
   syncfunc_t *sf;
   allocfunc_syncrunner(srun, RUN_QID, &sf);
   return 0;
}

static int dummy_sf(syncfunc_param_t* sfparam)
{
   (void) sfparam;
   return synccmd_EXIT;
}

/* Allokierte x Seiten, wobei x == (allocated+nrfree+freelist_size)/NRELEMPERPAGE.
 * Danach werden allocated+freelist_size syncfunc_t allokiert.
 * Davon werden dann wieder freelist_size freigegeben.
 * Precondition:
 * 0 == (allocated+nrfree+freelist_size) % NRELEMPERPAGE
 * */
static int testalloc_sq(syncrunner_queue_t *sq, size_t size, size_t nrfree, size_t freelist_size)
{
   linkd_t sflist;
   syncfunc_t *sf;
   initself_linkd(&sflist);
   TEST(0 == (size % NRELEMPERPAGE));
   TEST(size >= nrfree);
   TEST(size >= freelist_size);
   TEST(nrfree <= nrfree+freelist_size);
   TEST(0 == clear_sq(sq));
   for (size_t i=size/NRELEMPERPAGE; i>0; --i) {
      TEST(0 == grow_sq(sq));
   }
   for (size_t i=size-nrfree; i>0; --i) {
      sfalloc_sq(sq, &sf);
      init_syncfunc(sf, &dummy_sf, 0);
      initprev_linkd(&sf->waitnode,&sflist);
   }
   for (size_t i=freelist_size; i>0; --i) {
      sf = castPwaitnode_syncfunc(sflist.next);
      unlink_linkd(sflist.next);
      sffree_sq(sq, sf);
   }
   unlink_linkd(&sflist);
   TEST(freelist_size == sq->freelist_size);
   TEST(nrfree == sq->nrfree);
   TEST(size == sq->size);

   return 0;
ONERR:
   return EINVAL;
}

static int test_sq_initfree(void)
{
   syncrunner_queue_t sq = syncrunner_queue_FREE;

   // TEST syncrunner_queue_FREE
   TEST( 0 == sq.first);
   TEST( 0 == sq.firstfree);
   TEST( ! isvalid_linkd(&sq.freelist));
   TEST( 0 == sq.freelist_size);
   TEST( 0 == sq.size);
   TEST( 0 == sq.nextfree);
   TEST( 0 == sq.nrfree);

   // TEST init_sq: always works, no error possible
   memset(&sq, 255, sizeof(sq));
   TEST( 0 == init_sq(&sq));
   TEST( 0 == sq.first);
   TEST( 0 == sq.firstfree);
   TEST( isself_linkd(&sq.freelist));
   TEST( 0 == sq.freelist_size);
   TEST( 0 == sq.size);
   TEST( 0 == sq.nextfree);
   TEST( 0 == sq.nrfree);

   // TEST free_sq: empty queue
   TEST( 0 == free_sq(&sq));
   TEST( 0 == sq.first);
   TEST( 0 == sq.firstfree);
   TEST( 0 == sq.freelist_size);
   TEST( 0 == sq.size);
   TEST( 0 == sq.nextfree);
   TEST( 0 == sq.nrfree);

   // TEST free_sq: queue not empty
   TEST(0 == init_sq(&sq));
   grow_sq(&sq);
   sq.freelist_size = 1;
   sq.nextfree = 1;
   TEST( 0 != sq.first);
   TEST( NRELEMPERPAGE == sq.nrfree);
   TEST( 0 == free_sq(&sq));
   TEST( 0 == sq.first);
   TEST( 0 == sq.firstfree);
   TEST( ! isvalid_linkd(&sq.freelist));
   TEST( 0 == sq.freelist_size);
   TEST( 0 == sq.size);
   TEST( 0 == sq.nextfree);
   TEST( 0 == sq.nrfree);

   // TEST free_sq: EINVAL
   for (unsigned i = 1; i<10; ++i) {
      TEST(0 == init_sq(&sq));
      for (unsigned i2 = 0; i2<i; ++i2) {
         TEST(0 == grow_sq(&sq));
      }
      sq.freelist_size = 1;
      sq.nextfree = 1;
      init_testerrortimer(&s_sq_errtimer, i, EINVAL);
      TEST( EINVAL == free_sq(&sq));
      TEST( 0 == sq.first);
      TEST( 0 == sq.firstfree);
      TEST( 0 == sq.freelist_size);
      TEST( 0 == sq.size);
      TEST( 0 == sq.nextfree);
      TEST( 0 == sq.nrfree);
   }

   return 0;
ONERR:
   free_sq(&sq);
   return EINVAL;
}

static int test_sq_update(void)
{
   syncrunner_queue_t sq = syncrunner_queue_FREE;
   syncrunner_page_t *page;
   syncrunner_page_t *first;
   syncrunner_page_t *last;
   syncfunc_t        *sf = 0;
   process_t         process = process_FREE;
   process_result_t  process_result;
   linkd_t           usedlist;
   unsigned          nrused;

   // TEST grow_sq
   TEST(0 == init_sq(&sq));
   last = 0;
   for (unsigned i = 1; i <= 3; ++i) {
      TEST(0 == grow_sq(&sq));
      TEST( isself_linkd(&sq.freelist));
      TEST( 0               != sq.first);
      TEST( sq.first        == sq.firstfree);
      TEST( 0               == sq.freelist_size);
      TEST( i*NRELEMPERPAGE == sq.size);
      TEST( 0               == sq.nextfree);
      TEST( i*NRELEMPERPAGE == sq.nrfree);
      // check pages are linked in list
      last = last ? (syncrunner_page_t*)last->otherpages.next : sq.first;
      TEST( last == (syncrunner_page_t*)sq.first->otherpages.prev);
      page = sq.first;
      for (unsigned i2 = 1; i2 <= i; ++i2) {
         TEST(page == (syncrunner_page_t*)page->otherpages.next->prev);
         TEST(page == (syncrunner_page_t*)page->otherpages.prev->next);
         page = (syncrunner_page_t*)page->otherpages.next;
      }
      TEST( page == sq.first);
   }

   // TEST grow_sq: ENOMEM
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   // test
   init_testerrortimer(&s_sq_errtimer, 1, ENOMEM);
   TEST( ENOMEM == grow_sq(&sq));
   // check sq
   TEST( 0 == sq.first);
   TEST( 0 == sq.firstfree);
   TEST( 0 == sq.freelist_size);
   TEST( 0 == sq.size);
   TEST( 0 == sq.nextfree);
   TEST( 0 == sq.nrfree);

   // TEST shrink_sq: empty queue / single empty page
   for (unsigned i = 0; i <= 1; ++i) {
      TEST(0 == free_sq(&sq));
      TEST(0 == init_sq(&sq));
      if (i) TEST(0 == grow_sq(&sq));
      TEST(0 == shrink_sq(&sq));
      // check
      TEST( 0 == sq.first);
      TEST( 0 == sq.firstfree);
      TEST( isself_linkd(&sq.freelist));
      TEST( 0 == sq.freelist_size);
      TEST( 0 == sq.size);
      TEST( 0 == sq.nextfree);
      TEST( 0 == sq.nrfree);
   }

   // TEST shrink_sq: nrfree < NRELEMPERPAGE
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   TEST(0 == grow_sq(&sq));
   -- sq.nrfree;
   TEST(0 == shrink_sq(&sq));
   // check
   TEST( 0        != sq.first);
   TEST( sq.first == sq.firstfree);
   TEST( NRELEMPERPAGE == sq.size);
   TEST( NRELEMPERPAGE-1 == sq.nrfree);

   // TEST shrink_sq: multiple empty pages
   for (unsigned i = 1; i <= 29; ++i) {
      // build queue with i empty pages (free syncfunc_t)
      TEST(0 == free_sq(&sq));
      TEST(0 == init_sq(&sq));
      for (unsigned i2 = 0; i2 <= i; ++i2) {
         TEST(0 == grow_sq(&sq));
      }
      TEST( 0 != sq.first);
      first = (syncrunner_page_t*)sq.first;
      last  = (syncrunner_page_t*)sq.first->otherpages.prev;
      for (unsigned i2 = 0; i2 < i; ++i2) {
         // prepare: shrink removes last page
         last = (syncrunner_page_t*)last->otherpages.prev;
         // test
         TEST(0 == shrink_sq(&sq));
         // check
         TEST( first == sq.first);
         TEST( first == sq.firstfree);
         TEST( isself_linkd(&sq.freelist));
         TEST( 0 == sq.freelist_size);
         TEST( 0 == sq.nextfree);
         TEST( (i-i2)*NRELEMPERPAGE == sq.size);
         TEST( (i-i2)*NRELEMPERPAGE == sq.nrfree);
         // check pages are linked in list
         TEST( last == (syncrunner_page_t*)sq.first->otherpages.prev);
         page = sq.first;
         for (unsigned i3 = i2; i3 < i; ++i3) {
            TEST(page == (syncrunner_page_t*)page->otherpages.next->prev);
            TEST(page == (syncrunner_page_t*)page->otherpages.prev->next);
            page = (syncrunner_page_t*)page->otherpages.next;
         }
         TESTP( page == sq.first, "i:%d i2:%d", i, i2);
      }
   }

   // TEST shrink_sq: EINVAL
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   for (unsigned i = 0; i <= 5; ++i) {
      TEST(0 == grow_sq(&sq));
   }
   first = (syncrunner_page_t*)sq.first;
   last  = (syncrunner_page_t*)sq.first->otherpages.prev;
   for (unsigned i = 0; i < 5; ++i) {
      // prepare: shrink removes last page
      last = (syncrunner_page_t*)last->otherpages.prev;
      // test
      init_testerrortimer(&s_sq_errtimer, 1, EINVAL);
      TEST( EINVAL == shrink_sq(&sq));
      // check
      TEST( first == sq.first);
      TEST( first == sq.firstfree);
      TEST( isself_linkd(&sq.freelist));
      TEST( 0 == sq.freelist_size);
      TEST( 0 == sq.nextfree);
      TEST( (5-i)*NRELEMPERPAGE == sq.size);
      TEST( (5-i)*NRELEMPERPAGE == sq.nrfree);
      // check pages are linked in list
      TEST( last == (syncrunner_page_t*)sq.first->otherpages.prev);
      page = sq.first;
      for (unsigned i2 = i; i2 < i; ++i2) {
         TEST(page == (syncrunner_page_t*)page->otherpages.next->prev);
         TEST(page == (syncrunner_page_t*)page->otherpages.prev->next);
         page = (syncrunner_page_t*)page->otherpages.next;
      }
      TESTP( page == sq.first, "i:%d", i);
   }

   // TEST sfalloc_sq: nrfree > 0
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   TEST(0 == grow_sq(&sq));
   first = sq.first;
   for (unsigned i = 1; i <= NRELEMPERPAGE; ++i) {
      sfalloc_sq(&sq, &sf);
      // check sf
      TEST( sf            == sq.first->sfunc + i-1)
      // check sq
      TEST( first         == sq.first);
      TEST( first         == sq.firstfree);
      TEST( isself_linkd(&sq.freelist));
      TEST( 0             == sq.freelist_size);
      TEST( NRELEMPERPAGE == sq.size);
      TEST( i             == sq.nextfree);
      TEST( NRELEMPERPAGE == sq.nrfree + i);
   }

   // TEST sfalloc_sq: nrfree == 0 (assert(nrfree) fails)
   TEST(0 == init_process(&process, &childprocess_sfalloc, &sq, 0));
   TEST(0 == wait_process(&process, &process_result));
   // check
   TEST(process_state_ABORTED == process_result.state);
   TEST(SIGABRT == process_result.returncode);
   TEST(0 == free_process(&process));

   // TEST sfalloc_sq: freelist prefered
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   TEST(0 == grow_sq(&sq));
   TEST(0 == grow_sq(&sq));
   first = sq.first;
   for (unsigned i = 1; i <= NRELEMPERPAGE; ++i) {
      sfalloc_sq(&sq, &sf); // nrfree = 2*NRELEMPERPAGE-i
      TEST(sf == sq.first->sfunc + i-1)
   }
   for (unsigned i = 1; i <= NRELEMPERPAGE; ++i) {
      sffree_sq(&sq, sq.first->sfunc + i-1);
      TEST(i == sq.freelist_size);
   }
   for (unsigned i = 1; i <= NRELEMPERPAGE; ++i) {
      // test
      sfalloc_sq(&sq, &sf);
      // check sf
      TEST( sf            == sq.first->sfunc + i-1)
      // check sq
      TEST( first         == sq.first);
      TEST( first         == sq.firstfree);
      TEST( i < NRELEMPERPAGE ? ! isself_linkd(&sq.freelist) : isself_linkd(&sq.freelist));
      TEST( NRELEMPERPAGE == sq.freelist_size + i);
      TEST( NRELEMPERPAGE == sq.size - NRELEMPERPAGE);
      TEST( NRELEMPERPAGE == sq.nextfree);
      TEST( NRELEMPERPAGE == sq.nrfree);
   }

   // TEST sffree_sq: adds synfunc_t to freelist
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   TEST(0 == grow_sq(&sq));
   first = sq.first;
   for (unsigned i = 1; i <= NRELEMPERPAGE; ++i) {
      sfalloc_sq(&sq, &sf);
      TEST(sf == sq.first->sfunc + i-1)
      sf->mainfct = (syncfunc_f)(uintptrf_t)256;
   }
   for (unsigned i = 1; i <= NRELEMPERPAGE; ++i) {
      // test
      sf = &first->sfunc[i-1];
      TEST( 0 != sf->mainfct);
      sffree_sq(&sq, sf);
      // check sf: mainfct cleared ==> marked as free
      TEST( 0 == sf->mainfct);
      // check sq
      TEST( first         == sq.first);
      TEST( first         == sq.firstfree);
      TEST( &first->sfunc[i-1].waitnode == sq.freelist.prev);
      TEST( &first->sfunc[0].waitnode == sq.freelist.next);
      TEST( i             == sq.freelist_size);
      TEST( NRELEMPERPAGE == sq.size);
      TEST( NRELEMPERPAGE == sq.nextfree);
      TEST( 0             == sq.nrfree);
      // check freelist
      for (unsigned i2 = 0; i2 < i; ++i2) {
         TEST( first->sfunc[i2].waitnode.next == (i2+1 < i ? &first->sfunc[i2+1].waitnode:&sq.freelist));
         TEST( first->sfunc[i2].waitnode.prev == (i2 ? &first->sfunc[i2-1].waitnode:&sq.freelist));
      }
   }

   // TEST clear_sq
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   for (unsigned i = 0; i < 100; ++i) {
      TEST(0 == grow_sq(&sq));
   }
   sfalloc_sq(&sq, &sf);
   sffree_sq(&sq, sf);
   // test
   TEST(0 == clear_sq(&sq));
   // check
   TEST( 0 == sq.first);
   TEST( 0 == sq.firstfree);
   TEST( isself_linkd(&sq.freelist));
   TEST( 0 == sq.freelist_size);
   TEST( 0 == sq.size);
   TEST( 0 == sq.nextfree);
   TEST( 0 == sq.nrfree);

   // TEST compact_sq: empty queue
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   // test
   compact_sq(&sq);
   // check
   TEST( 0 == sq.first);
   TEST( 0 == sq.firstfree);
   TEST( isself_linkd(&sq.freelist));
   TEST( 0 == sq.freelist_size);
   TEST( 0 == sq.size);
   TEST( 0 == sq.nextfree);
   TEST( 0 == sq.nrfree);

   // TEST compact_sq: freelist_size == 0 ==> does nothing
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   for (unsigned i = 1; i <= NRELEMPERPAGE; ++i) {
      TEST(0 == grow_sq(&sq));
      sfalloc_sq(&sq, &sf);
      // test
      TEST(0 != (first = sq.first));
      compact_sq(&sq);
      // check
      TEST( first == sq.first);
      TEST( first == sq.firstfree);
      TEST( isself_linkd(&sq.freelist));
      TEST( 0 == sq.freelist_size);
      TEST( NRELEMPERPAGE*i == sq.size);
      TEST( i               == sq.nextfree);
      TEST( (NRELEMPERPAGE-1)*i == sq.nrfree);
   }

   // TEST compact_sq:  first == firstfree, freelist_size > 0
   for (unsigned sz=1; sz<=5; ++sz) {
      TEST(0 == free_sq(&sq));
      TEST(0 == init_sq(&sq));
      for (unsigned i = 0; i<sz; ++i) {
         TEST(0 == grow_sq(&sq));
      }
      first = sq.first;
      for (unsigned i = 0; i < NRELEMPERPAGE; ++i) {
         sfalloc_sq(&sq, &sf);
      }
      for (unsigned i = 0; i < NRELEMPERPAGE; ++i) {
         sffree_sq(&sq, &sq.first->sfunc[i]);
      }
      // test
      compact_sq(&sq);
      // check
      TEST( first == sq.first);
      TEST( first == sq.firstfree);
      TEST( isself_linkd(&sq.freelist));
      TEST( 0                == sq.freelist_size);
      TEST( sz*NRELEMPERPAGE == sq.size);
      TEST( 0                == sq.nextfree);
      TEST( sz*NRELEMPERPAGE == sq.nrfree);
   }

   // TEST compact_sq: compact last page (freelist_size < nextfree)
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   nrused = 0;
   initself_linkd(&usedlist);
   for (unsigned i = 1; i <= 10; ++i) {
      TEST(0 == grow_sq(&sq));
      for (unsigned i3 = 0; i3 < NRELEMPERPAGE; ++i3) {
         sfalloc_sq(&sq, &sf);
         sf->mainfct = (syncfunc_f)(uintptrf_t)(++nrused);
         initnext_linkd(&sf->waitnode, &usedlist);
      }
   }
   first = sq.first;
   last  = (syncrunner_page_t*)sq.first->otherpages.prev;
   for (unsigned i = 1, nrfreed = 0; i <= 10; ++i) {
      for (unsigned i3 = nrfreed; i3 < nrfreed+i; ++i3) {
         unlink_syncfunc(&first->sfunc[i3]);
         sffree_sq(&sq, &first->sfunc[i3]);
      }
      // test
      TEST(i == sq.freelist_size);
      TEST(NRELEMPERPAGE-nrfreed == sq.nextfree);
      compact_sq(&sq);
      nrfreed += i;
      // check sq
      TEST( first                 == sq.first);
      TEST( last                  == sq.firstfree);
      TEST( isself_linkd(&sq.freelist));
      TEST( 0                     == sq.freelist_size);
      TEST( NRELEMPERPAGE-nrfreed == sq.nextfree);
      TEST( 10*NRELEMPERPAGE      == sq.size);
      // check moved sfunc
      linkd_t *next = usedlist.next;
      for (unsigned i3 = 0; i3 < nrfreed; ++i3, next=next->next) {
         sf = castPwaitnode_syncfunc(next);
         TEST( sf == &first->sfunc[i3]);
         TEST( nrused-i3 == (uintptrf_t)sf->mainfct);
      }
   }

   // TEST compact_sq: compact last page (freelist_size > nextfree)
   TEST(0 == free_sq(&sq));
   TEST(0 == init_sq(&sq));
   nrused = 0;
   initself_linkd(&usedlist);
   for (unsigned i = 1; i <= 10; ++i) {
      TEST(0 == grow_sq(&sq));
      for (unsigned i3 = 0; i3 < NRELEMPERPAGE; ++i3) {
         sfalloc_sq(&sq, &sf);
         sf->mainfct = (syncfunc_f)(uintptrf_t)(++nrused);
         initnext_linkd(&sf->waitnode, &usedlist);
      }
   }
   // free first 5 pages
   page = sq.first;
   for (unsigned i = 1; i <= 5; ++i, page=(syncrunner_page_t*)page->otherpages.next) {
      for (unsigned i3 = 0; i3 < NRELEMPERPAGE; ++i3) {
         unlink_syncfunc(&page->sfunc[i3]);
         sffree_sq(&sq, &page->sfunc[i3]);
      }
   }
   TEST(5*NRELEMPERPAGE == sq.freelist_size);
   first = sq.first;
   last  = (syncrunner_page_t*)sq.first->otherpages.prev;
   page  = last;
   for (unsigned i = 1; i <= 5; ++i) {
      // test
      compact_sq(&sq);
      // check sq
      page = (syncrunner_page_t*)page->otherpages.prev;
      TEST( first                 == sq.first);
      TEST( last                  == (syncrunner_page_t*)first->otherpages.prev);
      TEST( page                  == sq.firstfree);
      TEST( i < 5 ? ! isself_linkd(&sq.freelist) : isself_linkd(&sq.freelist));
      TEST( (5-i)*NRELEMPERPAGE   == sq.freelist_size);
      TEST( NRELEMPERPAGE         == sq.nextfree);
      TEST( 10*NRELEMPERPAGE      == sq.size); // size keeps same, shrink_sq removes empty pages
      // check moved sfunc
      syncrunner_page_t *next = first;
      linkd_t *prev = &usedlist;
      for (unsigned i2 = 0; i2 < i; ++i2, next=(syncrunner_page_t*)next->otherpages.next) {
         for (unsigned i3 = 0; i3 < NRELEMPERPAGE; ++i3, prev=prev->next) {
            TEST( nrused-(i2*NRELEMPERPAGE)-i3 == (uintptrf_t)next->sfunc[i3].mainfct);
            TEST( prev       == next->sfunc[i3].waitnode.prev);
            TEST( prev->next == &next->sfunc[i3].waitnode);
         }
      }
   }

   // TEST compact_sq: pattern of free/used syncfunc_t
   static_assert(NRELEMPERPAGE > 19 * (3), "size of pattern does not exceed one queue page");
   for (unsigned i = 2; i <= 19; ++i) {
      nrused = 0;
      initself_linkd(&usedlist);
      TEST(0 == free_sq(&sq));
      TEST(0 == init_sq(&sq));
      // allocate all blocks
      for (unsigned i2 = 0; i2 < i; ++i2) {
         TEST(0 == grow_sq(&sq));
         for (unsigned i3 = 0; i3 < NRELEMPERPAGE; ++i3) {
            sfalloc_sq(&sq, &sf);
         }
      }
      // free first block
      page = sq.first;
      for (unsigned i3 = 0; i3 < NRELEMPERPAGE; ++i3) {
         sffree_sq(&sq, &page->sfunc[i3]);
      }
      // allocate pattern in other blocks
      for (unsigned i2 = 1; i2 < i; ++i2) {
         page = (syncrunner_page_t*)page->otherpages.next;
         for (unsigned i3 = 0; i3 < NRELEMPERPAGE; ++i3) {
            sf = &page->sfunc[i3];
            if ((i2-1) <= i3 && i3 <= i2+1) {
               sf->mainfct = (syncfunc_f)(uintptrf_t)(++nrused);
               initnext_linkd(&sf->waitnode, &usedlist);
            } else {
               sffree_sq(&sq, sf);
            }
         }
      }
      first = sq.first;
      last  = (syncrunner_page_t*)sq.first->otherpages.prev;
      page  = last;
      TEST( last            == sq.firstfree);
      TEST( i*NRELEMPERPAGE == sq.freelist_size + nrused);
      TEST( i*NRELEMPERPAGE == sq.size);
      TEST( NRELEMPERPAGE   == sq.nextfree);
      TEST( 0               == sq.nrfree);
      for (unsigned i2 = 1; i2 < i; ++i2) {
         // test
         compact_sq(&sq);
         // check sq
         page = (syncrunner_page_t*)page->otherpages.prev;
         TEST( first            == sq.first);
         TEST( last             == (syncrunner_page_t*)sq.first->otherpages.prev);
         TEST( page             == sq.firstfree);
         TEST( i*NRELEMPERPAGE  == sq.freelist_size + nrused + i2*NRELEMPERPAGE);
         TEST( i*NRELEMPERPAGE  == sq.size);
         TEST( NRELEMPERPAGE    == sq.nextfree);
         TEST( i2*NRELEMPERPAGE == sq.nrfree);
         // check moved sfunc
         linkd_t *prev = &usedlist;
         for (unsigned i3 = 0; i3 < 3*i2; ++i3, prev=prev->next) {
            TEST( nrused-i3  == (uintptrf_t)first->sfunc[i3].mainfct);
            TEST( prev       == first->sfunc[i3].waitnode.prev);
            TEST( prev->next == &first->sfunc[i3].waitnode);
         }
         // check freelist
         unsigned i3 = 3*i2;
         TEST( 0 == first->sfunc[i3].mainfct);
         syncrunner_page_t* next = first;
         size_t nrfree = sq.freelist_size;
         prev = &sq.freelist;
         for (linkd_t *node = sq.freelist.next; node != &sq.freelist; prev = node, node = node->next, --nrfree, ++i3) {
            if (i3 == NRELEMPERPAGE) {
               i3 = 0;
               next = (syncrunner_page_t*)next->otherpages.next;
            }
            while (next->sfunc[i3].mainfct != 0) { ++i3; }
            TEST( 0 == next->sfunc[i3].mainfct);
            TEST( &next->sfunc[i3] == castPwaitnode_syncfunc(node));
            TEST( prev == node->prev);
            TEST( node == prev->next);
         }
         TEST( 0 == nrfree);
      }
   }

   // reset
   TEST(0 == free_process(&process));
   TEST(0 == free_sq(&sq));

   return 0;
ONERR:
   (void) free_process(&process);
   (void) free_sq(&sq);
   return EINVAL;
}

// test helper

static int test_constants(void)
{
   // TEST RUN_QID, WAIT_QID
   TEST(0 == RUN_QID);
   TEST(1 == WAIT_QID);
   TEST(WAIT_QID + 1 == lengthof(((syncrunner_t*)0)->sq));

   // TEST ELEMSIZE
   TEST(ELEMSIZE == sizeof(syncfunc_t));

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   syncrunner_t srun = syncrunner_FREE;

   // TEST syncrunner_FREE
   TEST( ! isvalid_linkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.sq); ++i) {
      TEST( 0 == srun.sq[i].first);
      TEST( 0 == srun.sq[i].size);
   }
   TEST( 0 == srun.isrun);
   TEST( 0 == srun.isterminate);

   // TEST init_syncrunner
   memset(&srun, 255, sizeof(srun));
   TEST( 0 == init_syncrunner(&srun));
   TEST( 1 == isself_linkd(&srun.wakeup));
   TEST( 0 == srun.isrun);
   TEST( 0 == srun.isterminate);
   for (unsigned i = 0; i < lengthof(srun.sq); ++i) {
      TEST( 0 == srun.sq[i].first);
      TEST( 0 == srun.sq[i].firstfree);
      TEST( 1 == isself_linkd(&srun.sq[i].freelist));
   }

   // TEST free_syncrunner: free queues
   TEST( 0 == growqueues_syncrunner(&srun));
   for (unsigned i = 0; i < lengthof(srun.sq); ++i) {
      TEST( 0 != srun.sq[i].first);
      TEST( NRELEMPERPAGE == srun.sq[i].size);
      TEST( NRELEMPERPAGE == srun.sq[i].nrfree);
   }
   TEST(0 == free_syncrunner(&srun));
   // check queues are freed (only)
   for (unsigned i = 0; i < lengthof(srun.sq); ++i) {
      TEST( 0 == srun.sq[i].first);
      TEST( 0 == srun.sq[i].firstfree);
      TEST( 0 == srun.sq[i].size);
      TEST( 0 == srun.sq[i].nrfree);
   }

   // TEST free_syncrunner: double free
   TEST(0 == free_syncrunner(&srun));
   for (unsigned i = 0; i < lengthof(srun.sq); ++i) {
      TEST( 0 == srun.sq[i].first);
      TEST( 0 == srun.sq[i].firstfree);
      TEST( 0 == srun.sq[i].size);
      TEST( 0 == srun.sq[i].nrfree);
   }

   // TEST free_syncrunner: EINVAL
   for (unsigned ec = 1; ec <= lengthof(srun.sq); ++ec) {
      TEST(0 == init_syncrunner(&srun));
      TEST(0 == growqueues_syncrunner(&srun));
      init_testerrortimer(&s_sq_errtimer, ec, EINVAL);
      TEST(EINVAL == free_syncrunner(&srun));
      // queues are freed
      for (unsigned i = 0; i < lengthof(srun.sq); ++i) {
         TEST( 0 == srun.sq[i].first);
         TEST( 0 == srun.sq[i].size);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_queuehelper(void)
{
   syncrunner_t srun    = syncrunner_FREE;
   process_t    process = process_FREE;
   syncfunc_t  *sf;

   // prepare
   static_assert(lengthof(srun.sq) == 2, "assumes 2 queues");
   TEST(0 == init_syncrunner(&srun));

   // TEST growqueues_syncrunner: empty queues, all values zero ==> (sq1.totalfree == sq2.nrallocated)
   TEST(0 == srun.sq[RUN_QID].size);
   TEST(0 == srun.sq[WAIT_QID].size);
   // test
   TEST( 0 == growqueues_syncrunner(&srun));
   // check sq
   TEST( NRELEMPERPAGE == srun.sq[RUN_QID].size);  // allocated additional page
   TEST( NRELEMPERPAGE == srun.sq[WAIT_QID].size); // allocated additional page

   // TEST growqueues_syncrunner: sq1.nrallocated < sq2.total_free ==> no growth of sq2
   for (uint8_t qid=0; qid < lengthof(srun.sq); ++qid) {
      for (size_t size=NRELEMPERPAGE; size <= 3*NRELEMPERPAGE; size+=NRELEMPERPAGE) {
         for (size_t nrfree=0; nrfree <= size; nrfree=2*nrfree+1) {
            for (size_t freelist_size=0; freelist_size <= size-nrfree; freelist_size=2*freelist_size+1) {
               size_t nrfree1=nrfree+freelist_size;
               for (size_t nrallocated=0; nrallocated < nrfree1; nrallocated+=(nrfree1>1 ? nrfree1-1 : 1)) {
                  for (unsigned isList=0; isList<=1; ++isList) {
                     size_t size2=6*NRELEMPERPAGE;
                     size_t nrfree2=size2-nrallocated;
                     TEST(0 == testalloc_sq(&srun.sq[qid], size, nrfree, freelist_size));
                     TEST(0 == testalloc_sq(&srun.sq[!qid], size2, isList?0:nrfree2, isList?nrfree2:0));
                     // test
                     TEST( 0 == growqueues_syncrunner(&srun));
                     // check sq
                     TEST( size == srun.sq[qid].size);   // does not grow
                     TEST( size2 == srun.sq[!qid].size); // always big enough
                  }
               }
            }
         }
      }
   }

   // TEST growqueues_syncrunner: sq1.nrallocated >= sq2.total_free ==> sq2 grows exactly 1 page
   for (uint8_t qid=0; qid < lengthof(srun.sq); ++qid) {
      for (size_t size=NRELEMPERPAGE; size <= 3*NRELEMPERPAGE; size+=NRELEMPERPAGE) {
         for (size_t nrfree=0; nrfree <= size; nrfree=2*nrfree+1) {
            for (size_t freelist_size=0; freelist_size <= size-nrfree; freelist_size=2*freelist_size+1) {
               for (size_t nradd=0; nradd <= 2; ++nradd) {
                  for (unsigned isList=0; isList<=1; ++isList) {
                     size_t nrallocated=(nrfree+freelist_size+nradd+(nradd==2?NRELEMPERPAGE:0));
                     size_t size2=6*NRELEMPERPAGE;
                     size_t nrfree2=size2-nrallocated;
                     TEST(0 == testalloc_sq(&srun.sq[qid], size, nrfree, freelist_size));
                     TEST(0 == testalloc_sq(&srun.sq[!qid], size2, isList?0:nrfree2, isList?nrfree2:0));
                     // test
                     TEST( 0 == growqueues_syncrunner(&srun));
                     // check sq
                     TEST( NRELEMPERPAGE+size == srun.sq[qid].size);  // does grow only one page
                     TEST( size2              == srun.sq[!qid].size); // always big enough
                  }
               }
            }
         }
      }
   }

   // TEST growqueues_syncrunner: both queues grow or keep size
   for (size_t size=NRELEMPERPAGE; size <= 2*NRELEMPERPAGE; size+=NRELEMPERPAGE) {
      for (size_t nrfree=0; nrfree <= size; ++nrfree) {
         for (size_t freelist_size=0; freelist_size <= size-nrfree; ++freelist_size) {
            size_t totalfree=nrfree+freelist_size;
            size_t nrallocated=size-totalfree;
            TEST(0 == testalloc_sq(&srun.sq[RUN_QID], size, nrfree, freelist_size));
            TEST(0 == testalloc_sq(&srun.sq[WAIT_QID], size, nrfree, freelist_size));
            // test
            TEST( 0 == growqueues_syncrunner(&srun));
            // check sq
            if (totalfree<=nrallocated) {
               TEST( NRELEMPERPAGE+size == srun.sq[RUN_QID].size);  // has been grown
               TEST( NRELEMPERPAGE+size == srun.sq[WAIT_QID].size); // has been grown
            } else {
               TEST( size == srun.sq[RUN_QID].size);  // no growth
               TEST( size == srun.sq[WAIT_QID].size); // no growth
            }
         }
      }
   }

   // TEST shrinkqueues_syncrunner: empty queue, single allocated page
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   for (unsigned i=0; i<2; ++i) {
      if (i) { TEST( 0 == growqueues_syncrunner(&srun)); }
      // test
      TEST(0 == shrinkqueues_syncrunner(&srun));
      // check sq (nothing changed)
      TEST( i*NRELEMPERPAGE == srun.sq[RUN_QID].size);
      TEST( i*NRELEMPERPAGE == srun.sq[WAIT_QID].size);
   }

   // TEST shrinkqueues_syncrunner: free pages are removed except last (one by one)
   for (unsigned i=2; i<16; ++i) {
      TEST(0 == free_syncrunner(&srun));
      TEST(0 == init_syncrunner(&srun));
      for (unsigned i2=0; i2<i; ++i2) {
         TEST( 0 == grow_sq(&srun.sq[RUN_QID]));
         TEST( 0 == grow_sq(&srun.sq[WAIT_QID]));
      }
      TEST( i*NRELEMPERPAGE == srun.sq[RUN_QID].size);
      TEST( i*NRELEMPERPAGE == srun.sq[WAIT_QID].size);
      for (unsigned i2=1; i2<i; ++i2) {
         // test
         TEST(0 == shrinkqueues_syncrunner(&srun));
         // check sq (nothing changed)
         TEST( (i-i2)*NRELEMPERPAGE == srun.sq[RUN_QID].size);  // one page removed
         TEST( (i-i2)*NRELEMPERPAGE == srun.sq[WAIT_QID].size); // one page removed
      }
   }

   // TEST shrinkqueues_syncrunner: sq1.nrallocated >= sq2.total_free-NRELEMPERPAGE ==> sq2 does not shrink
   for (uint8_t qid=0; qid < lengthof(srun.sq); ++qid) {
      for (size_t size=NRELEMPERPAGE; size <= 3*NRELEMPERPAGE; size+=NRELEMPERPAGE) {
         for (size_t nrfree=0; nrfree <= size; nrfree=2*nrfree+1) {
            for (size_t freelist_size=0; freelist_size <= size-nrfree; freelist_size=2*freelist_size+1) {
               size_t nrfree1=nrfree+freelist_size;
               if (nrfree1>=NRELEMPERPAGE) { nrfree1-=NRELEMPERPAGE; }
               for (size_t nrallocated=nrfree1; nrallocated <= nrfree1+2; ++nrallocated) {
                  for (unsigned isList=0; isList<=1; ++isList) {
                     size_t size2=6*NRELEMPERPAGE;
                     size_t nrfree2=size2-nrallocated;
                     TEST(0 == testalloc_sq(&srun.sq[qid], size, nrfree, freelist_size));
                     TEST(0 == testalloc_sq(&srun.sq[!qid], size2, isList?0:nrfree2, isList?nrfree2:0));
                     // test
                     TEST( 0 == shrinkqueues_syncrunner(&srun));
                     // check sq
                     TEST( size                == srun.sq[qid].size);  // does not shrink
                     TEST( size2-NRELEMPERPAGE == srun.sq[!qid].size); // always shrink
                  }
               }
            }
         }
      }
   }

   // TEST shrinkqueues_syncrunner: sq1.nrallocated < sq2.total_free-NRELEMPERPAGE ==> sq2 does shrink 1 page
   for (uint8_t qid=0; qid < lengthof(srun.sq); ++qid) {
      for (size_t size=2*NRELEMPERPAGE; size <= 3*NRELEMPERPAGE; size+=NRELEMPERPAGE) {
         for (size_t nrfree=0; nrfree <= size; nrfree=2*nrfree+1) {
            for (size_t freelist_size=0; freelist_size <= size-nrfree; freelist_size=2*freelist_size+1) {
               size_t nrfree1=nrfree+freelist_size;
               if (nrfree1<=NRELEMPERPAGE) continue;
               nrfree1-=NRELEMPERPAGE;
               for (size_t nrdiff=1; nrdiff <= nrfree1; ++nrdiff) {
                  for (unsigned isList=0; isList<=1; ++isList) {
                     size_t nrallocated=nrfree1-nrdiff;
                     size_t size2=6*NRELEMPERPAGE;
                     size_t nrfree2=size2-nrallocated;
                     TEST(0 == testalloc_sq(&srun.sq[qid], size, nrfree, freelist_size));
                     TEST(0 == testalloc_sq(&srun.sq[!qid], size2, isList?0:nrfree2, isList?nrfree2:0));
                     // test
                     TEST( 0 == shrinkqueues_syncrunner(&srun));
                     // check sq
                     TEST( size-NRELEMPERPAGE  == srun.sq[qid].size);  // does shrink one page
                     TEST( size2-NRELEMPERPAGE == srun.sq[!qid].size); // always shrink
                  }
               }
            }
         }
      }
   }

   // TEST allocfunc_syncrunner: ABORT if no free nodes
   process_result_t process_result;
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == init_process(&process, &childprocess_allocfunc, &srun, 0));
   TEST(0 == wait_process(&process, &process_result));
   // check
   TEST(process_state_ABORTED == process_result.state);
   TEST(SIGABRT == process_result.returncode);
   TEST(0 == free_process(&process));

   // TEST allocfunc_syncrunner: allocate from free page
   TEST(0 == growqueues_syncrunner(&srun));
   for (uint8_t qid=0; qid < lengthof(srun.sq); ++qid) {
      for (unsigned i=0; i<NRELEMPERPAGE; ++i) {
         allocfunc_syncrunner(&srun, qid, &sf);
         TEST(sf == &srun.sq[qid].first->sfunc[i]);
         TEST(NRELEMPERPAGE-1-i == srun.sq[qid].nrfree);
      }
   }

   // TEST clearqueue_syncrunner
   for (uint8_t qid=0; qid < lengthof(srun.sq); ++qid) {
      TEST(0 == clearqueue_syncrunner(&srun, qid));
      TEST(0 == srun.sq[qid].size);
   }

   // TEST removefunc_syncrunner: freed function added to freelist, mainfct set to null
   TEST(0 == growqueues_syncrunner(&srun));
   for (uint8_t qid=0; qid < lengthof(srun.sq); ++qid) {
      for (unsigned i=0; i<NRELEMPERPAGE; ++i) {
         allocfunc_syncrunner(&srun, qid, &sf);
         init_syncfunc(sf, &dummy_sf, 0);
      }
      for (unsigned i=0; i<NRELEMPERPAGE; ++i) {
         sf = &srun.sq[qid].first->sfunc[i];
         // test
         removefunc_syncrunner(&srun, qid, sf);
         // check
         TEST(&sf->waitnode == srun.sq[qid].freelist.prev); // added to freelist
         TEST(0 == sf->mainfct);                            // mainfct cleared
      }
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_process(&process);
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
   initnext_linkd(&sfunc.waitnode, &srun.wakeup);
   TEST(1 == iswakeup_syncrunner(&srun));

   // TEST iswakeup_syncrunner: false ==> wakeup list empty
   unlink_linkd(&sfunc.waitnode);
   TEST(0 == iswakeup_syncrunner(&srun));

   // TEST size_syncrunner: after init
   TEST(0 == size_syncrunner(&srun));

   // TEST size_syncrunner: size of single queue
   for (uint8_t qid=0; qid<lengthof(srun.sq); ++qid) {
      for (size_t size=NRELEMPERPAGE; size<=3*NRELEMPERPAGE; size+=NRELEMPERPAGE) {
         for (size_t nrfree=0; nrfree<=size; nrfree+=((size-nrfree)<10)+(size-nrfree)/10) {
            for (size_t freelist_size=0; freelist_size<=size-nrfree; freelist_size+=((size-nrfree-freelist_size)<10)+(size-nrfree-freelist_size)/10) {
               TEST(0 == testalloc_sq(&srun.sq[qid], size, nrfree, freelist_size));
               TEST(size-nrfree-freelist_size == size_syncrunner(&srun));
            }
         }
      }
   }

   // TEST size_syncrunner: size of all queues
   for (size_t size=NRELEMPERPAGE; size<=3*NRELEMPERPAGE; size+=NRELEMPERPAGE) {
      for (size_t nrfree=0; nrfree<=size; nrfree+=((size-nrfree)<10)+(size-nrfree)/10) {
         for (size_t freelist_size=0; freelist_size<=size-nrfree; freelist_size+=((size-nrfree-freelist_size)<10)+(size-nrfree-freelist_size)/10) {
            TEST(0 == testalloc_sq(&srun.sq[RUN_QID], size, nrfree, freelist_size));
            TEST(0 == testalloc_sq(&srun.sq[WAIT_QID], size, nrfree, freelist_size));
            TEST(2*(size-nrfree-freelist_size) == size_syncrunner(&srun));
         }
      }
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   (void) free_syncrunner(&srun);
   return EINVAL;
}

static int test_addfunc(void)
{
   syncrunner_t srun = syncrunner_FREE;
   syncfunc_t*  sf;

   // TEST addfunc_syncrunner: empty queue
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   for (uintptr_t i = 1, s = 1; i; i <<= 1, ++s) {
      TEST( 0 == addfunc_syncrunner(&srun, &dummy_sf, (void*)i));
      // check sq
      TEST( NRELEMPERPAGE == srun.sq[RUN_QID].size);
      TEST( NRELEMPERPAGE == srun.sq[WAIT_QID].size);
      TEST( NRELEMPERPAGE-s == srun.sq[RUN_QID].nrfree);
      TEST( NRELEMPERPAGE == srun.sq[WAIT_QID].nrfree);
      // check sfunc
      sf = &srun.sq[RUN_QID].first->sfunc[s-1];
      TEST( &dummy_sf == sf->mainfct);
      TEST( (void*)i  == sf->state);
      TEST( 0         == sf->contoffset);
      TEST( ! isvalid_linkd(&sf->waitnode));
   }

   // TEST addfunc_syncrunner: reuse freed slots
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   for (uintptr_t i = 0; i<NRELEMPERPAGE; ++i) {
      TEST( 0 == addfunc_syncrunner(&srun, &dummy_sf, (void*)i));
      removefunc_syncrunner(&srun, RUN_QID, &srun.sq[RUN_QID].first->sfunc[i]);
      // test
      TEST( 1 == srun.sq[RUN_QID].freelist_size);
      TEST( 0 == addfunc_syncrunner(&srun, &dummy_sf, (void*)(i+12)));
      // check sq
      TEST( NRELEMPERPAGE == srun.sq[RUN_QID].size);
      TEST( NRELEMPERPAGE == srun.sq[WAIT_QID].size);
      TEST( 0             == srun.sq[RUN_QID].freelist_size);
      TEST( i+1           == srun.sq[RUN_QID].nextfree);
      TEST( NRELEMPERPAGE == srun.sq[WAIT_QID].nrfree);
      // check sfunc
      sf = &srun.sq[RUN_QID].first->sfunc[i];
      TEST( (void*)(i+12) == sf->state);
   }

   // TEST addfunc_syncrunner: EINVAL
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(EINVAL == addfunc_syncrunner(&srun, 0/*null not allowed*/, (void*)1));
   // check result
   TEST( 0 == srun.sq[RUN_QID].size);
   TEST( 0 == srun.sq[WAIT_QID].size);

   // TEST addfunc_syncrunner: EAGAIN
   srun.isterminate = true;
   TEST(EAGAIN == addfunc_syncrunner(&srun, &dummy_sf, 0));
   srun.isterminate = false;
   // check result
   TEST( 0 == srun.sq[RUN_QID].size);
   TEST( 0 == srun.sq[WAIT_QID].size);

   // TEST addfunc_syncrunner: ENOMEM
   for (unsigned i=1; i<=2; ++i) {
      init_testerrortimer(&s_sq_errtimer, i, ENOMEM);
      TEST(ENOMEM == addfunc_syncrunner(&srun, &dummy_sf, 0));
      // check result
      TEST( (i==2?NRELEMPERPAGE:0) == srun.sq[RUN_QID].size);
      TEST( (i==2?NRELEMPERPAGE:0) == srun.sq[RUN_QID].nrfree);
      TEST( 0                      == srun.sq[WAIT_QID].size);
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   (void) free_syncrunner(&srun);
   return EINVAL;
}

static int test_wakeup(void)
{
   syncrunner_t srun = syncrunner_FREE;
   syncfunc_t  *sfunc2[NRELEMPERPAGE];
   syncwait_t   swait = syncwait_FREE;
   linkd_t     *prev;
   linkd_t     *next;

   // prepare
   init_syncwait(&swait);
   TEST(0 == init_syncrunner(&srun));

   // TEST wakeup_syncrunner: EAGAIN
   TEST( EAGAIN == wakeup_syncrunner(&srun, &swait));
   TEST( isself_linkd(&srun.wakeup));
   TEST( !iswaiting_syncwait(&swait));

   // TEST wakeupall_syncrunner: EAGAIN
   TEST( EAGAIN == wakeupall_syncrunner(&srun, &swait));
   TEST( isself_linkd(&srun.wakeup));
   TEST( !iswaiting_syncwait(&swait));

   // TEST wakeup_syncrunner: waitlist not empty
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == growqueues_syncrunner(&srun));
   for (unsigned i = 0; i < lengthof(sfunc2); ++i) {
      allocfunc_syncrunner(&srun, WAIT_QID, &sfunc2[i]);
      init_syncfunc(sfunc2[i], &dummy_sf, 0);
      addnode_syncwait(&swait, waitnode_syncfunc(sfunc2[i]));
      sfunc2[i]->err = -1;
   }
   prev = &srun.wakeup;
   next = &srun.wakeup;
   for (unsigned i = 0; i < lengthof(sfunc2); ++i) {
      // check swait
      TEST( getfirst_syncwait(&swait) == waitnode_syncfunc(sfunc2[i]));
      // test
      TEST( 0 == wakeup_syncrunner(&srun, &swait));
      // check added to wakeup
      TEST( prev == waitnode_syncfunc(sfunc2[i])->prev);
      TEST( next == waitnode_syncfunc(sfunc2[i])->next);
      // check err set to 0
      TEST( 0 == err_syncfunc(sfunc2[i]));
      // prepare next
      prev = waitnode_syncfunc(sfunc2[i]);
   }
   TEST( !iswaiting_syncwait(&swait));

   // TEST wakeupall_syncrunner: waitlist not empty
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == growqueues_syncrunner(&srun));
   for (unsigned i = 0; i < lengthof(sfunc2); ++i) {
      allocfunc_syncrunner(&srun, WAIT_QID, &sfunc2[i]);
      init_syncfunc(sfunc2[i], &dummy_sf, 0);
      addnode_syncwait(&swait, waitnode_syncfunc(sfunc2[i]));
      sfunc2[i]->err = -1;
   }
   // test
   TEST( 0 == wakeupall_syncrunner(&srun, &swait));
   // check swait
   TEST( !iswaiting_syncwait(&swait));
   // check err set to 0
   for (unsigned i = 0; i < lengthof(sfunc2); ++i) {
      TEST( 0 == err_syncfunc(sfunc2[i]));
   }
   // check wakeup
   prev = &srun.wakeup;
   next = waitnode_syncfunc(sfunc2[1]);
   for (unsigned i = 0; i < lengthof(sfunc2); ++i) {
      TEST( prev == waitnode_syncfunc(sfunc2[i])->prev);
      TEST( next == waitnode_syncfunc(sfunc2[i])->next);
      // prepare next
      prev = waitnode_syncfunc(sfunc2[i]);
      next = i < lengthof(sfunc2)-2 ? waitnode_syncfunc(sfunc2[i+2]) : &srun.wakeup;
   }

   // unprepare
   free_syncwait(&swait);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncwait(&swait);
   free_syncrunner(&srun);
   return EINVAL;
}

// == in params ==

static struct {
   // == sfparam
   syncfunc_param_t *sfparam_ptr;
   syncfunc_param_t  sfparam;
   // == sfunc
   int16_t           contoffset;
   int               err;
   void             *state;
   void             *sfunc_range_start;
   void             *sfunc_range_end;
} s_in;

static void init_s_in(syncrunner_t *srun)
{
   memset(&s_in, 0, sizeof(s_in));
   s_in.sfparam.srun = srun;
}

// == out params ==

static struct {
   syncfunc_f        sfunc;
   size_t            runcount;
   size_t            errcount;
} s_out;

static void clear_s_out(void)
{
   s_out.sfunc    = 0;
   s_out.runcount = 0;
   s_out.errcount = 0;
}

static void touch_s_out(syncfunc_f sfunc)
{
   assert(!s_out.sfunc || s_out.sfunc == sfunc);
   s_out.sfunc = sfunc;
   ++ s_out.runcount;
}

static void testok_s_out(int ok_condition)
{
   s_out.errcount += (ok_condition == 0);
}


static int check_s_out(syncfunc_f sfunc, size_t runcount)
{
   TEST( s_out.sfunc == sfunc);
   TEST( s_out.runcount == runcount);
   TEST( s_out.errcount == 0);
   return 0;
ONERR:
   return EINVAL;
}

static struct {
   int               retval;
   int               exiterr;
   int16_t           contoffset;
   syncwait_t       *waitlist;
   void             *state;
} s_set;

static int test_in_sf(syncfunc_param_t *sfparam)
{
   assert(s_in.sfparam_ptr   == sfparam);
   assert(s_in.sfparam.srun  == sfparam->srun);
   assert(s_in.sfparam.sfunc == sfparam->sfunc);
   assert(sfparam->sfunc->contoffset == s_in.contoffset);
   assert(sfparam->sfunc->err        == s_in.err);
   touch_s_out(&test_in_sf);
   return s_set.retval;
}

static int test_cancel_sf(syncfunc_param_t *sfparam)
{
   assert(sfparam          == s_in.sfparam_ptr);
   assert(sfparam->srun    == s_in.sfparam.srun);
   assert(sfparam->sfunc   == s_in.sfparam.sfunc);
   assert(sfparam->sfunc->contoffset == (s_out.runcount == 0 ? s_in.contoffset : 15));
   assert(sfparam->sfunc->endoffset  == (s_out.runcount == 0 ? 0 : 15));
   assert(sfparam->sfunc->err        == (s_out.runcount == 0 ? 0 : ECANCELED));
   if (!s_out.runcount) { sfparam->sfunc->endoffset = 15; }
   touch_s_out(&test_cancel_sf);
   return synccmd_RUN;
}

static int test_exec_helper(void)
{
   syncrunner_t      srun  = syncrunner_FREE;
   syncfunc_param_t  param = syncfunc_param_INIT(&srun);
   syncfunc_t        sfunc = syncfunc_FREE;

   // prepare
   init_s_in(&srun);
   s_in.sfparam_ptr   = &param;
   s_in.sfparam.sfunc = &sfunc;
   param.sfunc = &sfunc;

   // TEST RUN_SYNCFUNC
   for (unsigned retcode = 0; retcode <= 4; ++retcode) {
      const int R = (int)retcode -2;
      s_set.retval = R;
      s_in.err = 12345 + (int)retcode;
      for (unsigned contoffset = 0; contoffset <= 3; ++contoffset) {
         void* state = (void*)((uintptr_t)10 + contoffset);
         init_syncfunc(&sfunc, &test_in_sf, state);
         sfunc.contoffset = (int16_t) contoffset;
         s_in.contoffset  = (int16_t) contoffset;
         sfunc.err = s_in.err;
         // test
         clear_s_out();
         TEST( R == RUN_SYNCFUNC(param));
         // check sfunc
         TEST( sfunc.mainfct    == &test_in_sf);
         TEST( sfunc.state      == state);
         TEST( sfunc.contoffset == (int16_t) contoffset);
         TEST( sfunc.endoffset  == 0);
         TEST( sfunc.err        == s_in.err);
         // check function called
         TEST( 0 == check_s_out(&test_in_sf, 1));
      }
   }

   // TEST END_SYNCFUNC: endoffset!=0 ==> sets err=ECANCELED && sets contoffset=endoffset
   s_in.err = ECANCELED;
   for (unsigned endoffset = 1; endoffset <= 3; ++endoffset) {
      // prepare
      linkd_t waitlist;
      void *state = (void*)((uintptr_t)10 + endoffset);
      init_syncfunc(&sfunc, &test_in_sf, state);
      sfunc.endoffset = (int16_t) endoffset;
      s_in.contoffset = (int16_t) endoffset;
      init_linkd(&sfunc.waitnode, &waitlist);
      clear_s_out();
      // test
      END_SYNCFUNC(param);
      // check function called
      TEST( 0 == check_s_out(&test_in_sf, 1));
      // check sfunc
      TEST(sfunc.state      == state);
      TEST(sfunc.contoffset == (int16_t) endoffset);
      TEST(sfunc.endoffset  == (int16_t) endoffset);
      TEST(sfunc.err        == ECANCELED);
      TEST( !isvalid_linkd(&sfunc.waitnode));   // cleared
      TEST( isself_linkd(&waitlist));           // removed from waitlist
   }

   // TEST END_SYNCFUNC: endoffset == 0 && fct. returns synccmd_EXIT
   s_set.retval = synccmd_EXIT;
   s_in.err = 0;
   for (unsigned contoffset = 1; contoffset <= 3; ++contoffset) {
      // prepare
      linkd_t waitlist;
      void *state = (void*)((uintptr_t)10 + contoffset);
      init_syncfunc(&sfunc, &test_in_sf, state);
      sfunc.contoffset = (int16_t) contoffset;
      s_in.contoffset  = (int16_t) contoffset;
      init_linkd(&sfunc.waitnode, &waitlist);
      clear_s_out();
      // test
      END_SYNCFUNC(param);
      // check function called
      TEST( 0 == check_s_out(&test_in_sf, 1));
      // check sfunc
      TEST( sfunc.state      == state);
      TEST( sfunc.contoffset == (int16_t) contoffset);
      TEST( sfunc.endoffset  == 0);
      TEST( sfunc.err        == 0);
      TEST( !isvalid_linkd(&sfunc.waitnode));   // cleared
      TEST( isself_linkd(&waitlist));           // removed from waitlist
   }

   // TEST END_SYNCFUNC: endoffset == 0 && fct. returns synccmd_RUN ==> called 2nd time with ECANCELED
   for (unsigned contoffset = 1; contoffset <= 3; ++contoffset) {
      // prepare
      linkd_t waitlist;
      void *state = (void*)((uintptr_t)10 + contoffset);
      init_syncfunc(&sfunc, &test_cancel_sf, state);
      sfunc.contoffset = (int16_t) contoffset;
      s_in.contoffset  = (int16_t) contoffset;
      init_linkd(&sfunc.waitnode, &waitlist);
      clear_s_out();
      // test
      END_SYNCFUNC(param);
      // check function called
      TEST( 0 == check_s_out(&test_cancel_sf, 2/*called two times!*/));
      // check sfunc
      TEST( sfunc.state      == state);
      TEST( sfunc.contoffset == 15);
      TEST( sfunc.endoffset  == 15);
      TEST( sfunc.err        == ECANCELED);
      TEST( !isvalid_linkd(&sfunc.waitnode));   // cleared
      TEST( isself_linkd(&waitlist));           // removed from waitlist
   }

   // unprepare

   return 0;
ONERR:
   return EINVAL;
}

static int test_wakeup_sf(syncfunc_param_t* sfparam)
{
   assert(sfparam->srun    == s_in.sfparam.srun);
   testok_s_out(sfparam->srun->isrun == 0);
   testok_s_out(sfparam->srun->isterminate == 0);
   testok_s_out(state_syncfunc(sfparam) == s_in.state);
   testok_s_out(contoffset_syncfunc(sfparam->sfunc) == s_in.contoffset);
   if (s_in.sfunc_range_start) {
      assert(s_in.sfunc_range_start <= (void*)sfparam->sfunc);
      assert(s_in.sfunc_range_end   >  (void*)sfparam->sfunc);
   }
   assert(sfparam->sfunc->err == s_in.err);

   setcontoffset_syncfunc(sfparam->sfunc, s_set.contoffset);
   setstate_syncfunc(sfparam, s_set.state);

   if (s_set.retval == synccmd_WAIT) {
      sfparam->waitlist = s_set.waitlist;

   } else if (s_set.retval == synccmd_EXIT) {
      sfparam->sfunc->err = s_set.exiterr;
   }

   touch_s_out(&test_wakeup_sf);
   return s_set.retval;
}

static int child_proces_wakeuplist(void *_srun)
{
   syncrunner_t *srun = _srun;
   TEST( isself_linkd(&srun->wakeup));
   process_wakeuplist(srun);
   return 0;
ONERR:
   return EINVAL;
}

static int test_exec_wakeup(void)
{
   syncrunner_t srun;
   syncfunc_t*  sfunc[10];
   syncwait_t   swait;
   process_t    process = process_FREE;
   process_result_t process_result;

   // prepare
   init_s_in(&srun);
   init_syncwait(&swait);
   TEST(0 == init_syncrunner(&srun));

   // TEST process_wakeuplist: violated precondition
   TEST(0 == init_process(&process, &child_proces_wakeuplist, 0, 0));
   TEST(0 == wait_process(&process, &process_result));
   TEST(0 == free_process(&process));
   // check
   TEST( process_result.state      == process_state_ABORTED);
   TEST( process_result.returncode == SIGSEGV);

   // TEST process_wakeuplist: synccmd_EXIT
   TEST(0 == free_process(&process));
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == growqueues_syncrunner(&srun));
   s_set.retval = synccmd_EXIT;
   s_in.sfunc_range_start = srun.sq[WAIT_QID].first->sfunc;
   s_in.sfunc_range_end   = srun.sq[WAIT_QID].first->sfunc+lengthof(sfunc);
   for (uintptr_t state = 0; state <= 0x123; state+=0x123) {
      s_in.state = (void*)state;
      for (unsigned waitresult = 0; waitresult <= 250; waitresult += 50) {
         s_in.err = (int) waitresult;
         for (unsigned contoffset = 0; contoffset <= 1; ++contoffset) {
            s_in.contoffset = (int16_t) contoffset;
            for (unsigned i = 0; i < lengthof(sfunc); ++i) {
               allocfunc_syncrunner(&srun, WAIT_QID, &sfunc[i]);
               init_syncfunc(sfunc[i], &test_wakeup_sf, (void*)state);
               sfunc[i]->contoffset = (int16_t) contoffset;
               seterr_syncfunc(sfunc[i], (int)waitresult);
               initprev_linkd(waitnode_syncfunc(sfunc[i]), &srun.wakeup);
            }

            // test
            clear_s_out();
            process_wakeuplist(&srun);
            // check all sfunc woken up and exited
            TEST( 0 == check_s_out(&test_wakeup_sf, lengthof(sfunc)));
            TEST( 0 == size_syncrunner(&srun));
            TEST( isself_linkd(&srun.wakeup));
            TEST( lengthof(sfunc) == srun.sq[WAIT_QID].freelist_size);
         }
      }
   }
   s_in.sfunc_range_start = 0;

   // TEST process_wakeuplist: synccmd_RUN
   s_set.retval = synccmd_RUN;
   init_s_in(&srun);
   for (unsigned contoffset = 0; contoffset <= 32; contoffset+=32) {
      s_set.contoffset = (int16_t) contoffset;
      for (uintptr_t state = 0; state <= 3; state+=3) {
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
         TEST(0 == growqueues_syncrunner(&srun));
         s_in.sfunc_range_start = srun.sq[WAIT_QID].first->sfunc;
         s_in.sfunc_range_end   = srun.sq[WAIT_QID].first->sfunc+lengthof(sfunc);
         s_set.state = (void*) state;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            allocfunc_syncrunner(&srun, WAIT_QID, &sfunc[i]);
            init_syncfunc(sfunc[i], &test_wakeup_sf, 0);
            initprev_linkd(waitnode_syncfunc(sfunc[i]), &srun.wakeup);
         }

         // test
         clear_s_out();
         process_wakeuplist(&srun);
         // check result
         TEST( 0 == check_s_out(&test_wakeup_sf, lengthof(sfunc)));
         TEST( lengthof(sfunc) == size_syncrunner(&srun));
         TEST( isself_linkd(&srun.wakeup));
         TEST( lengthof(sfunc) == srun.sq[WAIT_QID].freelist_size);
         TEST( lengthof(sfunc) == srun.sq[RUN_QID].nextfree);
         TEST( 0 == srun.sq[RUN_QID].freelist_size);
         // check content of run queue
         syncfunc_t *sf = srun.sq[RUN_QID].first->sfunc;
         for (unsigned i=0; i<lengthof(sfunc); ++i) {
            TEST(sf[i].mainfct    == &test_wakeup_sf);
            TEST(sf[i].state      == s_set.state);
            TEST(sf[i].contoffset == s_set.contoffset);
         }
      }
   }
   s_in.sfunc_range_start = 0;

   // TEST process_wakeuplist: synccmd_WAIT
   s_set.retval = synccmd_WAIT;
   s_set.waitlist = &swait;
   s_in.err = 123;
   s_in.state = 0;
   s_in.contoffset = 0;
   for (unsigned contoffset = 0; contoffset <= 256; contoffset+=128) {
      s_set.contoffset = (int16_t) contoffset;
      for (uintptr_t state = 0; state <= 1; ++state) {
         s_set.state = (void*) state;
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
         TEST(0 == growqueues_syncrunner(&srun));
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            allocfunc_syncrunner(&srun, WAIT_QID, &sfunc[i]);
            init_syncfunc(sfunc[i], &test_wakeup_sf, 0);
            seterr_syncfunc(sfunc[i], 123);
            initprev_linkd(waitnode_syncfunc(sfunc[i]), &srun.wakeup);
         }

         // test
         clear_s_out();
         process_wakeuplist(&srun);
         // check result
         TEST( 0 == check_s_out(&test_wakeup_sf, lengthof(sfunc)));
         TEST( lengthof(sfunc) == size_syncrunner(&srun));
         TEST( isself_linkd(&srun.wakeup));
         TEST( iswaiting_syncwait(&swait));
         TEST( &sfunc[0]->waitnode == getfirst_syncwait(&swait));
         TEST( 0 == srun.sq[WAIT_QID].freelist_size);
         TEST( 0 == srun.sq[RUN_QID].nextfree);
         // check content of wait queue
         syncfunc_t *sf = srun.sq[WAIT_QID].first->sfunc;
         for (unsigned i=0; i<lengthof(sfunc); ++i) {
            TEST(sf[i].mainfct    == &test_wakeup_sf);
            TEST(sf[i].state      == (void*) state);
            TEST(sf[i].contoffset == (int16_t) contoffset);
            TEST(sf[i].err        == 123); // waitresult err not cleared
            TEST(sf[i].waitnode.prev == (i == 0 ? &swait.funclist : waitnode_syncfunc(sfunc[i-1])));
            TEST(sf[i].waitnode.next == (i+1 == lengthof(sfunc) ? &swait.funclist : waitnode_syncfunc(sfunc[i+1])));
         }
         // prepare next
         removelist_syncwait(&swait);
      }
   }

   // unprepare
   free_syncwait(&swait);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_process(&process);
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_run_sf(syncfunc_param_t * sfparam)
{
   assert(sfparam->srun == s_in.sfparam.srun);
   testok_s_out(sfparam->srun->isrun == 1);
   testok_s_out(sfparam->srun->isterminate == 0);
   testok_s_out(state_syncfunc(sfparam) == s_in.state);
   testok_s_out(contoffset_syncfunc(sfparam->sfunc) == s_in.contoffset);
   testok_s_out(sfparam->sfunc->err == s_in.err);

   if (s_in.sfunc_range_start) {
      assert(s_in.sfunc_range_start <= (void*)sfparam->sfunc);
      assert(s_in.sfunc_range_end   >  (void*)sfparam->sfunc);
   }

   setcontoffset_syncfunc(sfparam->sfunc, s_set.contoffset);
   setstate_syncfunc(sfparam, s_set.state);
   sfparam->waitlist   = s_set.waitlist;
   sfparam->sfunc->err = s_set.exiterr;

   touch_s_out(&test_run_sf);
   return s_set.retval;
}

static int test_exec_run(void)
{
   syncrunner_t   srun;
   syncfunc_t    *sf[NRELEMPERPAGE];
   syncwait_t     swait;

   // prepare
   init_s_in(&srun);
   init_syncwait(&swait);
   TEST(0 == init_syncrunner(&srun));

   // TEST run_syncrunner: EINPROGRESS
   srun.isrun = 1;
   TEST( EINPROGRESS == run_syncrunner(&srun));
   TEST( 0 == size_syncrunner(&srun));
   TEST( 1 == srun.isrun);
   TEST( isself_linkd(&srun.wakeup));
   srun.isrun = 0;

   // TEST run_syncrunner: empty queues
   TEST( 0 == run_syncrunner(&srun));
   TEST( 0 == size_syncrunner(&srun));
   TEST( 0 == srun.isrun);
   TEST( isself_linkd(&srun.wakeup));

   // TEST run_syncrunner: synccmd_EXIT
   TEST(0 == growqueues_syncrunner(&srun));
   s_set.retval = synccmd_EXIT;
   s_set.waitlist = 0;
   s_set.contoffset = 0;
   s_set.exiterr = 0;
   s_set.state = 0;
   s_in.sfunc_range_start = srun.sq[RUN_QID].first->sfunc;
   s_in.sfunc_range_end   = srun.sq[RUN_QID].first->sfunc+lengthof(sf);
   for (uintptr_t state = 0; state <= 0x234; state+=0x234) {
      s_in.state = (void*) state;
      for (unsigned contoffset = 0; contoffset <= 256; contoffset+=128) {
         s_in.contoffset = (int16_t) contoffset;
         for (unsigned sferr = 0; sferr <= 28; sferr += 14) {
            s_in.err = (int)sferr;
            for (unsigned i=0; i<lengthof(sf); ++i) {
               allocfunc_syncrunner(&srun, RUN_QID, &sf[i]);
               init_syncfunc(sf[i], &test_run_sf, (void*) state);
               sf[i]->contoffset = (int16_t) contoffset;
               sf[i]->err = (int)sferr;
            }
            // test run_syncrunner
            clear_s_out();
            TEST( 0 == run_syncrunner(&srun));
            // check
            TEST( 0 == check_s_out(&test_run_sf, lengthof(sf)));
            TEST( 0 == size_syncrunner(&srun));
            TEST( 0 == srun.sq[RUN_QID].freelist_size); // run queue is compact
            TEST( 0 == srun.isrun);
            TEST( isself_linkd(&srun.wakeup));
            // check run queue
            for (unsigned i=0; i<lengthof(sf); ++i) {
               TEST(sf[i]->mainfct    == 0);
               TEST(sf[i]->state      == 0);
               TEST(sf[i]->contoffset == 0);
               TEST(sf[i]->err        == 0);
            }
         }
      }
   }
   s_in.sfunc_range_start = 0;

   // TEST run_syncrunner: synccmd_RUN
   s_set.retval = synccmd_RUN;
   init_s_in(&srun);
   for (unsigned contoffset = 0; contoffset <= 256; contoffset+=256) {
      s_set.contoffset = (int16_t) contoffset;
      for (uintptr_t state = 0; state <= 0x9; state+=0x9) {
         s_set.state = (void*) state;
         for (unsigned sferr = 0; sferr <= 3; sferr += 3) {
            s_set.exiterr = (int)sferr;
            TEST(0 == free_syncrunner(&srun));
            TEST(0 == init_syncrunner(&srun));
            TEST(0 == growqueues_syncrunner(&srun));
            s_in.sfunc_range_start = srun.sq[RUN_QID].first->sfunc;
            s_in.sfunc_range_end   = srun.sq[RUN_QID].first->sfunc+lengthof(sf);
            for (unsigned i=0; i<lengthof(sf); ++i) {
               allocfunc_syncrunner(&srun, RUN_QID, &sf[i]);
               init_syncfunc(sf[i], &test_run_sf, 0);
            }
            // test
            clear_s_out();
            TEST( 0 == run_syncrunner(&srun));
            // check
            TEST( 0 == check_s_out(&test_run_sf, lengthof(sf)));
            TEST( 0 == srun.sq[RUN_QID].freelist_size);
            TEST( 0 == srun.isrun);
            TEST( lengthof(sf) == size_syncrunner(&srun));
            TEST( lengthof(sf) == srun.sq[RUN_QID].nextfree);
            TEST( isself_linkd(&srun.wakeup));
            // check content of run queue
            for (unsigned i=0; i<lengthof(sf); ++i) {
               TEST( sf[i]->mainfct    == &test_run_sf);
               TEST( sf[i]->state      == s_set.state);
               TEST( sf[i]->contoffset == s_set.contoffset);
               TEST( sf[i]->err        == s_set.exiterr);
            }
         }
      }
   }
   s_in.sfunc_range_start = 0;

   // TEST run_syncrunner: synccmd_WAIT
   s_set.retval = synccmd_WAIT;
   init_s_in(&srun);
   s_set.waitlist = &swait;
   for (unsigned contoffset = 0; contoffset <= 256; contoffset+=128) {
      s_set.contoffset = (int16_t) contoffset;
      for (uintptr_t state = 0; state <= 0x4; state+=0x4) {
         s_set.state = (void*) state;
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
         TEST(0 == growqueues_syncrunner(&srun));
         s_in.sfunc_range_start = srun.sq[RUN_QID].first->sfunc;
         s_in.sfunc_range_end   = srun.sq[RUN_QID].first->sfunc+lengthof(sf);
         for (unsigned i=0; i<lengthof(sf); ++i) {
            allocfunc_syncrunner(&srun, RUN_QID, &sf[i]);
            init_syncfunc(sf[i], &test_run_sf, 0);
         }
         // test
         clear_s_out();
         TEST( 0 == run_syncrunner(&srun));
         // check
         TEST( 0 == check_s_out(&test_run_sf, lengthof(sf)));
         TEST( 0 == srun.sq[RUN_QID].freelist_size);
         TEST( 0 == srun.isrun);
         TEST( lengthof(sf) == size_syncrunner(&srun));
         TEST( 0 == srun.sq[RUN_QID].freelist_size);
         TEST( 0 == srun.sq[RUN_QID].nextfree);
         TEST( 0 == srun.sq[WAIT_QID].freelist_size);
         TEST( lengthof(sf) == srun.sq[WAIT_QID].nextfree);
         TEST( isself_linkd(&srun.wakeup));
         TEST( iswaiting_syncwait(&swait));
         // check content of wait queue
         linkd_t *waitprev = &swait.funclist;
         syncfunc_t *sf2 = srun.sq[WAIT_QID].first->sfunc;
         static_assert(lengthof(sf) <= NRELEMPERPAGE, "nr of functions fts on a single page");
         for (unsigned i=0; i<lengthof(sf); ++i) {
            TEST( sf2[i].mainfct    == &test_run_sf);
            TEST( sf2[i].state      == s_set.state);
            TEST( sf2[i].contoffset == s_set.contoffset);
            TEST( sf2[i].err        == s_set.exiterr);
            TEST( sf2[i].waitnode.prev == waitprev);
            TEST( &sf2[i].waitnode     == waitprev->next);
            waitprev = &sf2[i].waitnode;
         }
         TEST( swait.funclist.prev == waitprev);
         TEST( &swait.funclist     == waitprev->next);
         // prepare next
         removelist_syncwait(&swait);
      }
   }
   s_in.sfunc_range_start = 0;

   // TEST run_syncrunner: run woken up functions
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   removelist_syncwait(&swait);
   s_set.retval = synccmd_WAIT;
   init_s_in(&srun);
   s_set.waitlist = &swait;
   s_set.state = 0;
   s_set.contoffset = 0;
   for (unsigned i=0; i<lengthof(sf); ++i) {
      TEST(0 == addfunc_syncrunner(&srun, &test_run_sf, 0));
   }
   // functions wait on swait
   clear_s_out();
   TEST(0 == run_syncrunner(&srun));
   TEST(0 == check_s_out(&test_run_sf, lengthof(sf)));
   TEST(lengthof(sf) == size_syncrunner(&srun));
   TEST(iswaiting_syncwait(&swait));
   // test do nothing (nothing woken up)
   s_set.retval = synccmd_EXIT;
   TEST( 0 == run_syncrunner(&srun));
   // check
   TEST( 0 == check_s_out(&test_run_sf, lengthof(sf)));
   TEST( lengthof(sf) == size_syncrunner(&srun));
   TEST( iswaiting_syncwait(&swait));
   // test run woken up functions
   TEST( 0 == wakeupall_syncrunner(&srun, &swait));
   TEST( 0 == run_syncrunner(&srun));
   // check
   TEST( 0 == check_s_out(&test_run_sf, 2*lengthof(sf)));
   TEST( 0 == size_syncrunner(&srun));
   TEST( ! iswaiting_syncwait(&swait));

   // TEST run_syncrunner: EINVAL in shrink run queue ==> no woken up functions run
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == growqueues_syncrunner(&srun));
   TEST(0 == grow_sq(&srun.sq[RUN_QID]));
   TEST(2*NRELEMPERPAGE == srun.sq[RUN_QID].size);
   s_set.retval = synccmd_EXIT;
   init_s_in(&srun);
   TEST(0 == addfunc_syncrunner(&srun, &test_run_sf, 0));
   allocfunc_syncrunner(&srun, WAIT_QID, &sf[0]);
   init_syncfunc(sf[0], &test_wakeup_sf, 0);
   initprev_linkd(waitnode_syncfunc(sf[0]), &srun.wakeup);
   // test
   clear_s_out();
   init_testerrortimer(&s_sq_errtimer, 1, EINVAL);
   TEST( EINVAL == run_syncrunner(&srun));
   // check result
   TEST( 0 == check_s_out(&test_run_sf, 1));
   TEST( ! isself_linkd(&srun.wakeup)); // not run
   TEST( 1 == size_syncrunner(&srun));
   TEST( NRELEMPERPAGE == srun.sq[RUN_QID].size);  // shrink called
   TEST( NRELEMPERPAGE == srun.sq[WAIT_QID].size);
   TEST( 0 == srun.sq[RUN_QID].nextfree);
   TEST( 1 == srun.sq[WAIT_QID].nextfree);
   TEST( 0 == srun.isrun);

   // TEST run_syncrunner: EINVAL in shrink wait queue
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == growqueues_syncrunner(&srun));
   TEST(0 == grow_sq(&srun.sq[WAIT_QID]));
   TEST(2*NRELEMPERPAGE == srun.sq[WAIT_QID].size);
   s_set.retval = synccmd_EXIT;
   init_s_in(&srun);
   TEST(0 == addfunc_syncrunner(&srun, &test_run_sf, 0));
   for (unsigned i=0; i<1+NRELEMPERPAGE; ++i) {
      allocfunc_syncrunner(&srun, WAIT_QID, &sf[0]);
      init_syncfunc(sf[0], &test_run_sf, 0);
      initprev_linkd(waitnode_syncfunc(sf[0]), &srun.wakeup);
   }
   // test
   clear_s_out();
   init_testerrortimer(&s_sq_errtimer, 1, EINVAL);
   TEST( EINVAL == run_syncrunner(&srun));
   // check result
   TEST( 0 == check_s_out(&test_run_sf, 2+NRELEMPERPAGE));
   TEST( isself_linkd(&srun.wakeup)); // wakeup run
   TEST( 0 == size_syncrunner(&srun));
   TEST( NRELEMPERPAGE == srun.sq[RUN_QID].size);
   TEST( NRELEMPERPAGE == srun.sq[WAIT_QID].size); // shrink called
   TEST( 0 == srun.isrun);

   // unprepare
   free_syncwait(&swait);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncwait(&swait);
   (void) free_syncrunner(&srun);
   return EINVAL;
}

static int test_terminate_sf(syncfunc_param_t * sfparam)
{
   assert(sfparam->srun == s_in.sfparam.srun);
   testok_s_out(sfparam->srun->isrun == 1);
   testok_s_out(sfparam->srun->isterminate == 1);
   testok_s_out(contoffset_syncfunc(sfparam->sfunc) == (int16_t)(intptr_t)sfparam->sfunc);
   if (sfparam->sfunc->endoffset == 0) {
      testok_s_out(state_syncfunc(sfparam) == sfparam->sfunc);
      testok_s_out(sfparam->sfunc->err == s_in.err);
      sfparam->sfunc->endoffset  = contoffset_syncfunc(sfparam->sfunc);
      sfparam->sfunc->contoffset = 0;
      setstate_syncfunc(sfparam, (void*)((uintptr_t)sfparam->sfunc+1));
   } else {
      testok_s_out(state_syncfunc(sfparam) == (void*)((uintptr_t)sfparam->sfunc+1));
      testok_s_out(sfparam->sfunc->err == ECANCELED);
      testok_s_out(sfparam->sfunc->endoffset == (int16_t)(intptr_t)sfparam->sfunc);
      setstate_syncfunc(sfparam, 0); // mark as run
   }

   sfparam->waitlist = 0; /* segmentation fault if linked to waitlist during terminate */
   touch_s_out(&test_terminate_sf);
   return s_set.retval;
}

static int test_exec_terminate(void)
{
   syncrunner_t srun  = syncrunner_FREE;
   syncwait_t   swait = syncwait_FREE;
   syncfunc_t*  sfunc[lengthof(srun.sq)][NRELEMPERPAGE];

   // prepare
   init_s_in(&srun);
   init_syncwait(&swait);
   TEST(0 == init_syncrunner(&srun));

   // TEST terminate_syncrunner: EINPROGRESS
   srun.isrun = 1;
   TEST( EINPROGRESS == terminate_syncrunner(&srun));
   TEST( 0 == size_syncrunner(&srun));
   TEST( 1 == srun.isrun);
   TEST( isself_linkd(&srun.wakeup));
   srun.isrun = 0;

   // TEST terminate_syncrunner: empty queues
   TEST( 0 == terminate_syncrunner(&srun));
   TEST( 0 == size_syncrunner(&srun));
   TEST( 0 == srun.isrun);
   TEST( 0 == srun.isterminate);
   TEST( isself_linkd(&srun.wakeup));

   // TEST terminate_syncrunner: synccmd_RUN  / synccmd_WAIT / synccmd_EXIT
   for (unsigned isEndoffset = 0; isEndoffset <= 1; ++isEndoffset) {
      static_assert(synccmd__NROF==3 && synccmd_RUN==0 && synccmd_EXIT==1 && synccmd_WAIT==2,"simple loop is enough");
      for (unsigned retval=0; retval<=2; ++retval) {
         s_set.retval = (synccmd_e) retval;
         // synccmd_EXIT ==> (run once despite of endoffset == 0)
         unsigned const isRunTwice = (retval != synccmd_EXIT) && (isEndoffset == 0);
         // error is only checked if isEndoffset == 0 else overwritten to ECANCELED
         for (unsigned sferr = 0; sferr <= 33; sferr+=33+isEndoffset) {
            s_in.err = (int)sferr;
            TEST(0 == free_syncrunner(&srun));
            TEST(0 == init_syncrunner(&srun));
            TEST(0 == growqueues_syncrunner(&srun));
            for (unsigned q = 0; q < lengthof(srun.sq); ++q) {
               for (unsigned i = 0; i < lengthof(sfunc[0]); ++i) {
                  allocfunc_syncrunner(&srun, q, &sfunc[q][i]);
                  init_syncfunc(sfunc[q][i], &test_terminate_sf, /*state*/(void*)((uintptr_t)sfunc[q][i]+isEndoffset));
                  if (isEndoffset) {
                     sfunc[q][i]->endoffset  = (int16_t)(intptr_t)sfunc[q][i]; // else == 0
                  } else {
                     sfunc[q][i]->contoffset = (int16_t)(intptr_t)sfunc[q][i]; // else == 0
                  }
                  sfunc[q][i]->err = s_in.err;
                  if (q == WAIT_QID) {
                     if (i&1) {
                        initprev_linkd(waitnode_syncfunc(sfunc[q][i]), &srun.wakeup);
                     } else {
                        addnode_syncwait(&swait, waitnode_syncfunc(sfunc[q][i]));
                     }
                  }
               }
            }
            // test
            clear_s_out();
            TEST( 0 == terminate_syncrunner(&srun));
            // check
            TEST( 0 == check_s_out(&test_terminate_sf, (1+isRunTwice)*lengthof(sfunc)*lengthof(sfunc[0])));
            TEST( 0 == srun.isrun);
            TEST( 0 == srun.isterminate);
            TEST( isself_linkd(&srun.wakeup));     // unlinked from wakeup list
            TEST( ! iswaiting_syncwait(&swait));   // unlinked from waitlist
            TEST( 0 == srun.sq[RUN_QID].size);
            TEST( 0 == srun.sq[WAIT_QID].size);
            // check sfunc
            for (unsigned q = 0; q < lengthof(srun.sq); ++q) {
               for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                  TEST(sfunc[q][i]->state     == (void*)(isRunTwice||isEndoffset ? 0 :(uintptr_t)sfunc[q][i]+1));
                  TEST(sfunc[q][i]->endoffset == (int16_t)(intptr_t)sfunc[q][i]);
               }
            }
         }
      }
   }

   // TEST terminate_syncrunner: EINVAL (clearqueue_syncrunner)
   s_set.retval = (synccmd_e) synccmd_WAIT;
   s_in.err = -1;
   for (unsigned qid = 0; qid < lengthof(srun.sq); ++qid) {
      TEST(0 == free_syncrunner(&srun));
      TEST(0 == init_syncrunner(&srun));
      TEST(0 == growqueues_syncrunner(&srun));
      for (unsigned q = 0; q < lengthof(srun.sq); ++q) {
         for (unsigned i = 0; i < lengthof(sfunc[0]); ++i) {
            allocfunc_syncrunner(&srun, q, &sfunc[q][i]);
            init_syncfunc(sfunc[q][i], &test_terminate_sf, /*state*/(void*)(uintptr_t)sfunc[q][i]);
            sfunc[q][i]->contoffset = (int16_t)(intptr_t)sfunc[q][i];
            sfunc[q][i]->err = s_in.err;
            if (q == WAIT_QID) {
               if (i&1) {
                  initprev_linkd(waitnode_syncfunc(sfunc[q][i]), &srun.wakeup);
               } else {
                  addnode_syncwait(&swait, waitnode_syncfunc(sfunc[q][i]));
               }
            }
         }
      }
      // test
      clear_s_out();
      init_testerrortimer(&s_sq_errtimer, 1+qid, EINVAL);
      TEST( EINVAL == terminate_syncrunner(&srun));
      // check
      TEST( 0 == check_s_out(&test_terminate_sf, 2*lengthof(sfunc)*lengthof(sfunc[0])));
      TEST( 0 == srun.isrun);
      TEST( 0 == srun.isterminate);
      TEST( isself_linkd(&srun.wakeup));     // unlinked from wakeup list
      TEST( ! iswaiting_syncwait(&swait));   // unlinked from waitlist
      TEST( 0 == srun.sq[RUN_QID].size);
      TEST( 0 == srun.sq[WAIT_QID].size);
      // check sfunc
      for (unsigned q = 0; q < lengthof(srun.sq); ++q) {
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(sfunc[q][i]->state     == 0);
            TEST(sfunc[q][i]->endoffset == (int16_t)(intptr_t)sfunc[q][i]);
         }
      }
   }

   // unprepare
   free_syncwait(&swait);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncwait(&swait);
   free_syncrunner(&srun);
   return EINVAL;
}

typedef struct example_state_t {
   int inuse;
   int error;
   unsigned runcount;
   unsigned expect;
   syncwait_t * cond;
} example_state_t;

static example_state_t s_example_state[NRELEMPERPAGE&~(size_t)1] = { {0,0,0,0,0} };

static int yield_example_sf(syncfunc_param_t *sfparam)
{
   uintptr_t id = (uintptr_t) state_syncfunc(sfparam);
   assert(id < lengthof(s_example_state));
   example_state_t* state = &s_example_state[id];
   ++ state->runcount;
   begin_syncfunc(sfparam);

   // allocate resource
   if (state->inuse || state->runcount != 1) exit_syncfunc(sfparam, EINVAL);
   state->inuse = 1;

   do {
      state->expect = state->runcount + 1;
      yield_syncfunc(sfparam);
      if (state->expect != state->runcount) exit_syncfunc(sfparam, EINVAL);
   } while (state->expect < 100);

   state->inuse = 0;

   end_syncfunc(sfparam, {
      state->error = err_syncfunc(sfparam->sfunc);
   });
}

static int wait_example_sf(syncfunc_param_t *sfparam)
{
   uintptr_t id = (uintptr_t) state_syncfunc(sfparam);
   assert(id < lengthof(s_example_state));
   example_state_t * state = &s_example_state[id];
   ++ state->runcount;
   begin_syncfunc(sfparam);

   if (state->inuse || state->runcount != 1) exit_syncfunc(sfparam, EINVAL);
   state->inuse = 1;

   if (wait_syncfunc(sfparam, state->cond)) exit_syncfunc(sfparam, EINVAL);

   if (2 != state->runcount) exit_syncfunc(sfparam, EINVAL);

   state->inuse = 0;

   end_syncfunc(sfparam, {
      state->error = err_syncfunc(sfparam->sfunc);
   });
}

static int sync_example_sf(syncfunc_param_t *sfparam)
{
   uintptr_t id = (uintptr_t) state_syncfunc(sfparam);
   assert(id < lengthof(s_example_state));
   example_state_t * state = &s_example_state[id];
   ++ state->runcount;
   begin_syncfunc(sfparam);

   if (state->inuse || state->runcount != 1) exit_syncfunc(sfparam, EINVAL);
   state->inuse = 1;

   if (id < lengthof(s_example_state)/2) {
      yield_syncfunc(sfparam);
      if (wakeup_syncrunner(sfparam->srun, state->cond)) exit_syncfunc(sfparam, EINVAL);
   } else {
      if (wait_syncfunc(sfparam, state->cond)) exit_syncfunc(sfparam, EINVAL);
   }

   if (2 != state->runcount) exit_syncfunc(sfparam, EINVAL);

   state->inuse = 0;

   end_syncfunc(sfparam, {
      state->error = err_syncfunc(sfparam->sfunc);
   });
}

static int test_examples(void)
{
   syncrunner_t srun = syncrunner_FREE;
   syncwait_t   cond = syncwait_FREE;

   // prepare
   init_syncwait(&cond);
   TEST(0 == init_syncrunner(&srun));

   // TEST run_syncrunner: 100 * yield
   memset(s_example_state, 0, sizeof(s_example_state));
   for (uintptr_t i=0; i < lengthof(s_example_state); ++i) {
      TEST(0 == addfunc_syncrunner(&srun, &yield_example_sf, (void*)i));
   }
   // test
   for (unsigned r=1; r < 100; ++r) {
      TEST( 0 == run_syncrunner(&srun));
      TEST( lengthof(s_example_state) == size_syncrunner(&srun));
   }
   TEST( 0 == run_syncrunner(&srun));
   // check
   TEST( 0 == size_syncrunner(&srun));
   for (unsigned i=0; i < lengthof(s_example_state); ++i) {
      TEST( 0 == s_example_state[i].inuse);
      TEST( 0 == s_example_state[i].error);
      TEST( 100 == s_example_state[i].runcount);
   }

   // TEST run_syncrunner: wait
   memset(s_example_state, 0, sizeof(s_example_state));
   for (uintptr_t i = 0; i < lengthof(s_example_state); ++i) {
      s_example_state[i].cond = &cond;
      TEST(0 == addfunc_syncrunner(&srun, wait_example_sf, (void*)i));
   }
   for (unsigned r = 1; r <= 3; ++r) {
      // test
      TEST( 0 == run_syncrunner(&srun));
      // check all are waiting on waitlist
      TEST( iswaiting_syncwait(&cond));
      TEST( lengthof(s_example_state) == size_syncrunner(&srun));
      for (unsigned i = 1; i < lengthof(s_example_state); ++i) {
         TEST( 1 == s_example_state[i].inuse);
         TEST( 1 == s_example_state[i].runcount);
         TEST( 0 == s_example_state[i].error);
      }
   }

   // TEST run_syncrunner: wakeup
   TEST(0 == wakeupall_syncrunner(&srun, &cond));
   TEST(0 == run_syncrunner(&srun));
   // check
   for (unsigned i = 1; i < lengthof(s_example_state); ++i) {
      TEST( 0 == s_example_state[i].inuse);
      TEST( 2 == s_example_state[i].runcount);
      TEST( 0 == s_example_state[i].error);
   }

   // TEST run_syncrunner: functions waiting on others (50% yielding / 50% waiting)
   memset(s_example_state, 0, sizeof(s_example_state));
   for (uintptr_t i = 0; i < lengthof(s_example_state); ++i) {
      s_example_state[i].cond = &cond;
      TEST(0 == addfunc_syncrunner(&srun, sync_example_sf, (void*)i));
   }
   // test
   TEST( 0 == run_syncrunner(&srun));
   // check result
   TEST( iswaiting_syncwait(&cond));
   TEST( isself_linkd(&srun.wakeup));
   TEST( lengthof(s_example_state)   == size_syncrunner(&srun));
   TEST( lengthof(s_example_state)/2 == srun.sq[RUN_QID].nextfree);
   TEST( lengthof(s_example_state)/2 == srun.sq[WAIT_QID].freelist_size);
   // test
   TEST( 0 == run_syncrunner(&srun));
   // check result
   TEST( 0 == size_syncrunner(&srun));
   for (unsigned i = 1; i < lengthof(s_example_state); ++i) {
      TEST( 0 == s_example_state[i].inuse);
      TEST( 2 == s_example_state[i].runcount);
      TEST( 0 == s_example_state[i].error);
   }

   // TEST terminate_syncrunner
   memset(s_example_state, 0, sizeof(s_example_state));
   for (uintptr_t i = 0; i < lengthof(s_example_state); ++i) {
      TEST(0 == addfunc_syncrunner(&srun, wait_example_sf, (void*)i));
      s_example_state[i].cond = &cond;
   }
   // after first run all are waiting for waitlist
   TEST(0 == run_syncrunner(&srun));
   TEST(iswaiting_syncwait(&cond));
   TEST(lengthof(s_example_state) == size_syncrunner(&srun));
   for (unsigned e = 1; e < lengthof(s_example_state); ++e) {
      TEST(1 == s_example_state[e].inuse);
      TEST(1 == s_example_state[e].runcount);
      TEST(0 == s_example_state[e].error);
   }
   // test
   TEST( 0 == terminate_syncrunner(&srun));
   // check
   TEST( ! iswaiting_syncwait(&cond));
   TEST( 0 == size_syncrunner(&srun));
   for (unsigned e = 1; e < lengthof(s_example_state); ++e) {
      TEST( 1 == s_example_state[e].inuse);
      TEST( 2 == s_example_state[e].runcount);
      TEST( ECANCELED == s_example_state[e].error);
   }

   // unprepare
   free_syncwait(&cond);
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncwait(&cond);
   free_syncrunner(&srun);
   return EINVAL;
}

int unittest_task_syncrunner()
{
   if (test_sq_initfree())    goto ONERR;
   if (test_sq_update())      goto ONERR;
   if (test_constants())      goto ONERR;
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
