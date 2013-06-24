/* title: SyncRun impl

   Implements <SyncRun>.

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

   file: C-kern/api/task/syncrun.h
    Header file <SyncRun>.

   file: C-kern/task/syncrun.c
    Implementation file <SyncRun impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncrun.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/inmem/queue.h"
#include "C-kern/api/ds/inmem/dlist.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/task/syncthread.h"
#include "C-kern/api/task/syncwait.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* typedef: struct initqueue_entry_t
 * Exports <initqueue_entry_t> into global namespace. */
typedef struct initqueue_entry_t          initqueue_entry_t ;

/* typedef: runqueue_entry_t
 * Make <runqueue_entry_t> an alias for <syncthread_t>. */
typedef struct runqueue_entry_t           runqueue_entry_t ;

/* typedef: struct run2queue_entry_t
 * Exports <run2queue_entry_t> into global namespace. */
typedef struct run2queue_entry_t          run2queue_entry_t ;

/* typedef: struct waitqueue_entry_t
 * Exports <waitqueue_entry_t> into global namespace. */
typedef struct waitqueue_entry_t          waitqueue_entry_t ;

/* typedef: struct wait2queue_entry_t
 * Exports <wait2queue_entry_t> into global namespace. */
typedef struct wait2queue_entry_t         wait2queue_entry_t ;


/* enums: syncrun_qid_e
 * Index into <syncrun_t.queues>.
 * syncrun_qid_INIT   - Index of queue holding <initqueue_entry_t> which describe an unitialized thread with its init argument.
 * syncrun_qid_RUN    - Index of queue holding <runqueue_entry_t> which describe already initialized threads.
 * syncrun_qid_RUN2   - Index of queue holding <run2queue_entry_t> which describe already initialized threads.
 *                      These threads generate an exit event.
 * syncrun_qid_WAIT   - Index of queue holding <waitqueue_entry_t> which describes a thread waiting for an event/condition.
 * syncrun_qid_WAIT2  - Index of queue holding <wait2queue_entry_t> which describes a thread waiting for an event/condition.
 *                      These threads generate an exit event.
 * syncrun_qid_WLIST  - Index of queue holding entries of <syncwlist_t> (see <wlistentry_t>).
 *                      Every entry references a waiting thread in <syncrun_qid_WAIT> or <syncrun_qid_WAIT2>.
 * syncrun_qid_WAKEUP - Index of queue holding entries of <syncevent_t> which references a waiting thread in <syncrun_qid_WAIT>
 *                      or <syncrun_qid_WAIT2>.
 * */
enum syncrun_qid_e {
   syncrun_qid_INIT,
   syncrun_qid_RUN,
   syncrun_qid_RUN2,
   syncrun_qid_WAIT,
   syncrun_qid_WAIT2,
   syncrun_qid_WLIST,
   syncrun_qid_WAKEUP,
   syncrun_qid_NROFQUEUES,
} ;


/* struct: initqueue_entry_t
 * Helds all data needed to construct a new <syncthread_t>.
 * The structure contains a <syncthread_t> its possible unused exitevent (<syncevent_t>)
 * and the initargsize and initarg[] data of an possible allocated init argument. */
struct initqueue_entry_t {
   syncthread_t   thread ;
   syncevent_t    exitevent ;
   uint8_t        initargsize ;
   void *         initarg[] ;
} ;

// group: lifetime

/* function: init_initqueueentry
 * Initialize initentry with mainfct and initarg. The size initargsize of the allocated init argument is set to 0. */
static inline void init_initqueueentry(initqueue_entry_t * initentry, syncrun_f mainfct, void * initarg)
{
   initentry->thread    = (syncthread_t) syncthread_INIT(mainfct, initarg) ;
   initentry->exitevent = (syncevent_t) syncevent_INIT_FREEABLE ;
   initentry->initargsize = 0 ;
}

/* function: init2_initqueueentry
 * Initialize initentry with mainfct and initarg and initargsize.
 * The caller must have reserved <sizeentry_initqueueentry> bytes for initentry. */
static inline void init2_initqueueentry(initqueue_entry_t * initentry, syncrun_f mainfct, uint8_t initargsize)
{
   initentry->thread    = (syncthread_t) syncthread_INIT(mainfct, initentry->initarg) ;
   initentry->exitevent = (syncevent_t) syncevent_INIT_FREEABLE ;
   initentry->initargsize = initargsize ;
}

// group: query

/* function: sizeentry_initqueueentry
 * The size in bytes of <initqueue_entry_t> depending on initargsize.
 * If initargsize is set to 0 the returned value equals sizeof(initqueue_entry_t). */
static inline uint16_t sizeentry_initqueueentry(uint8_t initargsize)
{
   const unsigned alignsize = sizeof(initqueue_entry_t) ;
   const unsigned argsize   = (unsigned)initargsize + (alignsize-1u) ;
   return (uint16_t) (sizeof(initqueue_entry_t) + argsize - (argsize % alignsize)) ;
}


/* struct: runqueue_entry_t
 * Contains only a single <syncthread_t>.
 * Entry will be stored in queue with id <syncrun_qid_RUN>. */
struct runqueue_entry_t {
   syncthread_t   thread ;
} ;

// group: lifetime

/* function: init_runqueueentry
 * Initializes runentry with thread. Content of thread is copied as plain bytes. */
static inline void init_runqueueentry(/*out*/runqueue_entry_t * runentry, const syncthread_t * thread)
{
   runentry->thread = *thread ;
}

/* function: initmove_runqueueentry
 * Initializes dest with src. src is not changed but considered uninitialized after the operation. */
static inline void initmove_runqueueentry(/*out*/runqueue_entry_t * dest, runqueue_entry_t * src)
{
   *dest = *src ;
}


/* struct: run2queue_entry_t
 * Contains a single <syncthread_t> and its corresponding exitevent.
 * Entry will be stored in queue with id <syncrun_qid_RUN2>. */
struct run2queue_entry_t {
   syncthread_t   thread ;
   syncevent_t    exitevent ;
} ;

// group: lifetime

/* function: init_run2queueentry
 * Initializes runentry with thread and exitevent.
 * After the operation exitevent has been moved into runentry and is considered uninitialized. */
static inline void init_run2queueentry(/*out*/run2queue_entry_t * runentry, syncthread_t * thread, syncevent_t * exitevent)
{
   static_assert(offsetof(runqueue_entry_t, thread) == offsetof(run2queue_entry_t, thread), "run2queue_entry_t extends runqueue_entry_t") ;
   runentry->thread = *thread ;
   initmove_syncevent(&runentry->exitevent, exitevent) ;
}

/* function: initmove_run2queueentry
 * Initializes dest with src. After the operation src is considered uninitialized. */
static inline void initmove_run2queueentry(/*out*/run2queue_entry_t * dest, run2queue_entry_t * src)
{
   dest->thread = src->thread ;
   initmove_syncevent(&dest->exitevent, &src->exitevent) ;
}


/* struct: waitqueue_entry_t
 * Contains waiting thread (<syncwait_t>).
 * Entry will be stored in queue with id <syncrun_qid_WAIT>. */
struct waitqueue_entry_t {
   syncwait_t     syncwait ;
} ;

// group: lifetime

/* function: init_waitqueueentry
 * Initializes waitentry with thread and uses srun->waitinfo to read the event and the continuelabel.
 * The continuelabel stored in <syncrun_t.waitinfo> is the place, where execution will continue after wakeup. */
static inline void init_waitqueueentry(/*out*/waitqueue_entry_t * waitentry, syncrun_t * srun, syncthread_t * thread)
{
   init_syncwait(&waitentry->syncwait, thread, srun->waitinfo.event, srun->waitinfo.continuelabel) ;
}

/* function: initmove_waitqueueentry
 * Moves content of initialized src to dest.
 * After return dest contains previous content of dest (back pointer of referenced objects are adapted).
 * Src is left untouched but is considered in an invalid, uninitialized state.
 * You have to initialize it before src can be used again. */
static inline void initmove_waitqueueentry(/*out*/waitqueue_entry_t * dest, waitqueue_entry_t * src)
{
   initmove_syncwait(&dest->syncwait, &src->syncwait) ;
}

// group: query

/* function: cast_waitqueueentry
 * Casts pointer to <syncwait_t> into pointer to <waitqueue_entry_t>. */
static inline waitqueue_entry_t * cast_waitqueueentry(syncwait_t * waitentry)
{
   static_assert(offsetof(waitqueue_entry_t, syncwait) == 0, "no offset") ;
   return (waitqueue_entry_t*)waitentry ;
}


/* struct: wait2queue_entry_t
 * Contains waiting thread (<syncwait_t>) and its exitevent.
 * Entry will be stored in queue with id <syncrun_qid_WAIT2>. */
struct wait2queue_entry_t {
   syncwait_t     syncwait ;
   syncevent_t    exitevent ;
} ;

// group: lifetime

/* function: init_wait2queueentry
 * Initializes waitentry with thread and exitevent and uses srun->waitinfo to read the event and the continuelabel.
 * The continuelabel stored in <syncrun_t.waitinfo> is the place, where execution will continue after wakeup. */
static inline void init_wait2queueentry(/*out*/wait2queue_entry_t * waitentry, syncrun_t * srun, syncthread_t * thread, syncevent_t * exitevent)
{
   static_assert(offsetof(waitqueue_entry_t, syncwait) == offsetof(wait2queue_entry_t, syncwait), "wait2queue_entry_t extends waitqueue_entry_t") ;
   init_syncwait(&waitentry->syncwait, thread, srun->waitinfo.event, srun->waitinfo.continuelabel) ;
   initmove_syncevent(&waitentry->exitevent, exitevent) ;
}

/* function: initmove_wait2queueentry
 * Moves content of initialized src to dest.
 * After return dest contains previous content of dest (back pointer of referenced objects are adapted).
 * Src is left untouched but is considered in an invalid, uninitialized state.
 * You have to initialize it before src can be used again. */
static inline void initmove_wait2queueentry(/*out*/wait2queue_entry_t * dest, wait2queue_entry_t * src)
{
   initmove_syncwait(&dest->syncwait, &src->syncwait) ;
   initmove_syncevent(&dest->exitevent, &src->exitevent) ;
}

// group: query

/* function: cast_wait2queueentry
 * Casts pointer to <syncwait_t> into pointer to <wait2queue_entry_t>. */
static inline wait2queue_entry_t * cast_wait2queueentry(syncwait_t * waitentry)
{
   static_assert(offsetof(wait2queue_entry_t, syncwait) == 0, "no offset") ;
   return (wait2queue_entry_t*)waitentry ;
}



// section: syncrun_t

// forward
static int clearevents_syncrun(syncrun_t * srun) ;

// group: variable

/* variable: s_syncrun_errtimer
 * Defines local <test_errortimer_t> used in Unit Test.
 * Only defined if <KONFIG_UNITTEST> is defined. */
#ifdef KONFIG_UNITTEST
static test_errortimer_t      s_syncrun_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: lifetime

int init_syncrun(/*out*/syncrun_t * srun)
{
   static_assert(syncrun_qid_NROFQUEUES == lengthof(srun->queues), "enum values are valid array indices") ;

   static_assert(offsetof(syncrun_t, queues) == 0, "first member") ;
   static_assert(offsetof(syncrun_t, wakeup_list) == sizeof(srun->queues), "second member") ;
   static_assert(offsetof(syncrun_t, wakeup) == sizeof(srun->queues) + sizeof(srun->wakeup_list), "third member") ;
   for (unsigned i = 0; i < lengthof(srun->queues); ++i) {
      init_syncqueue(&srun->queues[i]) ;
   }
   init_syncwlist(&srun->wakeup_list) ;
   memset((uint8_t*)srun + offsetof(syncrun_t, wakeup), 0, sizeof(syncrun_t) - offsetof(syncrun_t, wakeup)) ;

   return 0 ;
}

int free_syncrun(syncrun_t * srun)
{
   int err ;

   err = clearevents_syncrun(srun) ;

   static_assert(offsetof(syncrun_t, queues) == 0, "first member")
   for (unsigned i = 0; i < lengthof(srun->queues); ++i) {
      int err2 = free_syncqueue(&srun->queues[i]) ;
      if (err2) err = err2 ;
      SETONERROR_testerrortimer(&s_syncrun_errtimer, &err) ;
   }
   memset((uint8_t*)srun + sizeof(srun->queues), 0, sizeof(syncrun_t) - sizeof(srun->queues)) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

bool isfree_syncrun(const syncrun_t * srun)
{
   for (unsigned i = 0; i < lengthof(srun->queues); ++i) {
      if (! isfree_syncqueue(&srun->queues[i])) return false ;
   }

   return   0 == srun->wakeup.continuelabel     && 0 == srun->wakeup.retcode
            && 0 == srun->waitinfo.event        && 0 == srun->waitinfo.continuelabel
            && 0 == srun->running.laststarted   && 0 == srun->running.thread  && 0 == srun->running.state ;
}

size_t leninitqueue_syncrun(const syncrun_t * srun)
{
   return len_syncqueue(&srun->queues[syncrun_qid_INIT]) ;
}

size_t lenrunqueue_syncrun(const syncrun_t * srun)
{
   return   len_syncqueue(&srun->queues[syncrun_qid_RUN])
            + len_syncqueue(&srun->queues[syncrun_qid_RUN2]) ;
}

size_t lenwaitqueue_syncrun(const syncrun_t * srun)
{
   return   len_syncqueue(&srun->queues[syncrun_qid_WAIT])
            + len_syncqueue(&srun->queues[syncrun_qid_WAIT2]) ;
}

// group: internal

void setstateabort_syncrun(syncrun_t * srun)
{
   srun->running.state = syncrun_state_ABORT ;
}

void setstateexit_syncrun(syncrun_t * srun)
{
   int err ;

   VALIDATE_STATE_TEST(srun->running.state == syncrun_state_CONTINUE, ONABORT, ) ;

   srun->running.state = syncrun_state_EXIT ;

   return ;
ONABORT:
   srun->running.state = syncrun_state_ABORT ;
   TRACEABORT_ERRLOG(err) ;
}

void setstatewait_syncrun(syncrun_t * srun, struct syncevent_t * event, void * continuelabel)
{
   int err ;

   VALIDATE_INPARAM_TEST(event && false == iswaiting_syncevent(event), ONABORT, ) ;
   VALIDATE_STATE_TEST(srun->running.state == syncrun_state_CONTINUE, ONABORT, ) ;

   srun->waitinfo.wlist         = 0 ;
   srun->waitinfo.event         = event ;
   srun->waitinfo.continuelabel = continuelabel ;
   // (state==syncrun_state_WAIT) ==> waitinfo is valid
   srun->running.state          = syncrun_state_WAIT ;

   return ;
ONABORT:
   srun->running.state = syncrun_state_ABORT ;
   TRACEABORT_ERRLOG(err) ;
}

void setstatewaitlist_syncrun(syncrun_t * srun, struct syncwlist_t * wlist, void * continuelabel)
{
   int err ;
   syncqueue_t * queue = queue_syncwlist(wlist) ;

   VALIDATE_INPARAM_TEST(queue == 0 || queue == &srun->queues[syncrun_qid_WLIST], ONABORT, ) ;
   VALIDATE_STATE_TEST(srun->running.state == syncrun_state_CONTINUE, ONABORT, ) ;

   syncevent_t * event ;
   ONERROR_testerrortimer(&s_syncrun_errtimer, ONABORT) ;
   err = insert_syncwlist(wlist, &srun->queues[syncrun_qid_WLIST], &event) ;
   if (err) goto ONABORT ;

   srun->waitinfo.wlist         = wlist ;
   srun->waitinfo.event         = event ;
   srun->waitinfo.continuelabel = continuelabel ;
   // (state==syncrun_state_WAIT) ==> waitinfo is valid
   srun->running.state          = syncrun_state_WAIT ;

   return ;
ONABORT:
   srun->running.state = syncrun_state_ABORT ;
   TRACEABORT_ERRLOG(err) ;
}

// group: thread-lifetime

int startthread_syncrun(syncrun_t * srun, syncrun_f mainfct, void * initarg)
{
   int err ;
   initqueue_entry_t * initentry ;

   ONERROR_testerrortimer(&s_syncrun_errtimer, ONABORT) ;
   err = insert_syncqueue(&srun->queues[syncrun_qid_INIT], &initentry) ;
   if (err) goto ONABORT ;

   init_initqueueentry(initentry, mainfct, initarg) ;

   srun->running.laststarted = &initentry->exitevent ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int startthread2_syncrun(syncrun_t * srun, syncrun_f mainfct, uint8_t initargsize, /*out*/void ** initarg)
{
   int err ;
   initqueue_entry_t *  initentry ;
   uint16_t             entrysize = sizeentry_initqueueentry(initargsize) ;

   ONERROR_testerrortimer(&s_syncrun_errtimer, ONABORT) ;
   err = insert2_syncqueue(&srun->queues[syncrun_qid_INIT], entrysize, &initentry) ;
   if (err) goto ONABORT ;

   init2_initqueueentry(initentry, mainfct, initargsize) ;

   srun->running.laststarted = &initentry->exitevent ;

   *initarg = initentry->initarg ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: synchronization

int signalevent_syncrun(syncrun_t * srun, struct syncevent_t * syncevent)
{
   int err ;

   if (! iswaiting_syncevent(syncevent)) return 0 ;

   syncwait_t *   syncwait = waiting_syncevent(syncevent) ;
   syncqueue_t *  queue    = queuefromaddr_syncqueue(syncwait) ;

   // check that waiting_syncevent(syncevent) points to waitqueue or wait2queue
   VALIDATE_INPARAM_TEST(  queue == &srun->queues[syncrun_qid_WAIT]
                           || queue == &srun->queues[syncrun_qid_WAIT2], ONABORT, ) ;
   // check that waiting_syncevent(syncevent) and syncevent match
   VALIDATE_INPARAM_TEST(syncevent == event_syncwait(syncwait), ONABORT, ) ;

   syncevent_t * wakeupentry ;
   ONERROR_testerrortimer(&s_syncrun_errtimer, ONABORT) ;
   err = insert_syncqueue(&srun->queues[syncrun_qid_WAKEUP], &wakeupentry) ;
   if (err) goto ONABORT ;

   initmove_syncevent(wakeupentry, syncevent) ;
   // remove waiting from syncevent; signal it only once
   *syncevent = (syncevent_t) syncevent_INIT_FREEABLE ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int signalfirst_syncrun(syncrun_t * srun, struct syncwlist_t * syncwlist)
{
   int err ;

   if (!len_syncwlist(syncwlist)) return 0 ;

   syncqueue_t * queue = queue_syncwlist(syncwlist) ;

   VALIDATE_INPARAM_TEST(queue == &srun->queues[syncrun_qid_WLIST], ONABORT, ) ;

   err = transferfirst_syncwlist(&srun->wakeup_list, syncwlist) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int signalall_syncrun(syncrun_t * srun, struct syncwlist_t * syncwlist)
{
   int err ;

   if (!len_syncwlist(syncwlist)) return 0 ;

   syncqueue_t * queue = queue_syncwlist(syncwlist) ;

   VALIDATE_INPARAM_TEST(queue == &srun->queues[syncrun_qid_WLIST], ONABORT, ) ;

   err = transferall_syncwlist(&(srun)->wakeup_list, syncwlist) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: run

/* function: preparewakeup_syncrun
 * Sets all values in srun->wakeup (<syncrun_t.wakeup>).
 * Parameter continuelabel and retcode are copied into the corresponding data members of wakeup.
 * This function is called before a waiting threads is woken up. */
static inline void preparewakeup_syncrun(syncrun_t * srun, void * continuelabel, int retcode)
{
   srun->wakeup.continuelabel = continuelabel ;
   srun->wakeup.retcode       = retcode ;
}

/* function: preparerun_syncrun
 * Sets all values in srun->running (<syncrun_t.running>).
 * The exitevent of the last started thread is set to 0 meaning that there is no previously started thread.
 * The running thread thread is set to value of parameter running_thread. The state of the thread is initialized
 * to <syncrun_state_CONTINUE>. Therefore threads run until they exit or abort themselves. */
static inline int preparerun_syncrun(syncrun_t * srun, syncthread_t * running_thread)
{
   int err ;

   if (srun->waitinfo.wlist) {
      // remove empty entry if setstatewaitlist_syncrun called and an abort happened
      err = removeempty_syncwlist(srun->waitinfo.wlist, &srun->queues[syncrun_qid_WLIST]) ;
      srun->waitinfo.wlist = 0 ;
      if (err) goto ONABORT ;
      ONERROR_testerrortimer(&s_syncrun_errtimer, ONABORT) ;
   }

   srun->running.laststarted = 0 ;
   srun->running.thread = running_thread ;
   srun->running.state  = syncrun_state_CONTINUE ;
   return 0 ;
ONABORT:
   return err ;
}

/* function: execwaiting_syncrun
 * Calls <callwakeup_syncthread> for a waiting thread given in waiting.
 * The argument retcode is set into srun->wakeup and is the return code of the exited <syncthread_t>.
 * Function <waitforexit_syncrun> returns this retcode as its own return value.
 * For other types of events retcode is ignored (see <waitforevent_syncrun> and <waitforlist_syncrun>).
 * If the called syncthread exits or aborts and another thread waits for its exit the function calls
 * itself. If the waiting thread waits for another event after return it will be kept in the wait queue.
 * If its state is <syncrun_state_CONTINUE> after return it will be moved into queue <syncrun_qid_RUN>
 * or queue <syncrun_qid_RUN2>. */
static int execwaiting_syncrun(syncrun_t * srun, syncwait_t * waiting, int retcode)
{
   int err ;
   syncevent_t exitevent ;

   for (bool iswaitqueue;;) {  // handle chain of waiting syncthreads (optimized recursive call)
      {
         syncqueue_t * queue = queuefromaddr_syncqueue(waiting) ;

         if (queue == &srun->queues[syncrun_qid_WAIT]) {
            iswaitqueue = true ;
         } else if (queue == &srun->queues[syncrun_qid_WAIT2]) {
            iswaitqueue = false ;
         } else {
            err = EINVAL ;
            goto ONABORT ;
         }
      }

      preparewakeup_syncrun(srun, waiting->continuelabel, retcode) ;
      err = preparerun_syncrun(srun, &waiting->thread) ;
      if (err) goto ONABORT ;
      retcode = callwakeup_syncthread(&waiting->thread) ;

      switch ((enum syncrun_state_e)srun->running.state) {
      case syncrun_state_CONTINUE:
         if (iswaitqueue) {
            runqueue_entry_t  * runentry = 0 ;
            ONERROR_testerrortimer(&s_syncrun_errtimer, CASE_syncrun_state_ABORT) ;
            err = insert_syncqueue(&srun->queues[syncrun_qid_RUN], &runentry) ;
            if (err) goto CASE_syncrun_state_ABORT ;
            init_runqueueentry(runentry, &waiting->thread) ;
            err = remove_syncqueue(&srun->queues[syncrun_qid_WAIT], cast_waitqueueentry(waiting), &initmove_waitqueueentry) ;
            if (err) goto ONABORT ;
         } else {
            run2queue_entry_t  * run2entry = 0 ;
            ONERROR_testerrortimer(&s_syncrun_errtimer, CASE_syncrun_state_ABORT) ;
            err = insert_syncqueue(&srun->queues[syncrun_qid_RUN2], &run2entry) ;
            if (err) goto CASE_syncrun_state_ABORT ;
            init_run2queueentry(run2entry, &waiting->thread, &cast_wait2queueentry(waiting)->exitevent) ;
            err = remove_syncqueue(&srun->queues[syncrun_qid_WAIT2], cast_wait2queueentry(waiting), &initmove_wait2queueentry) ;
            if (err) goto ONABORT ;
         }
         break ;
      case syncrun_state_ABORT:
      CASE_syncrun_state_ABORT:
         (void) callabort_syncthread(&waiting->thread) ;
         retcode = syncrun_returncode_ABORT ;
         // fall through
      case syncrun_state_EXIT:
         if (iswaitqueue) {
            err = remove_syncqueue(&srun->queues[syncrun_qid_WAIT], cast_waitqueueentry(waiting), &initmove_waitqueueentry) ;
            if (err) goto ONABORT ;
         } else {
            initmove_syncevent(&exitevent, &cast_wait2queueentry(waiting)->exitevent) ;
            err = remove_syncqueue(&srun->queues[syncrun_qid_WAIT2], cast_wait2queueentry(waiting), &initmove_wait2queueentry) ;
            if (err) goto ONABORT ;

            waiting = waiting_syncevent(&exitevent) ;
            continue ;  // wake up next waiting thread in chain !
         }
         break ;
      case syncrun_state_WAIT:
         update_syncwait(waiting, srun->waitinfo.event, srun->waitinfo.continuelabel) ;
         break ;
      }

      break ;  // end of chain
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int execinitqueue_syncrun(syncrun_t * srun)
{
   int err ;

   while (len_syncqueue(&srun->queues[syncrun_qid_INIT])) {
      initqueue_entry_t * initentry = first_queue(genericcast_queue(&srun->queues[syncrun_qid_INIT]), sizeof(initqueue_entry_t)) ;
      uint16_t            entrysize = sizeentry_initqueueentry(initentry->initargsize) ;

      err = preparerun_syncrun(srun, &initentry->thread) ;
      if (err) goto ONABORT ;
      int retcode = callinit_syncthread(&initentry->thread) ;

      switch ((enum syncrun_state_e)srun->running.state) {
      case syncrun_state_CONTINUE:
         if (isfree_syncevent(&initentry->exitevent)) {
            runqueue_entry_t * runentry = 0 ;
            ONERROR_testerrortimer(&s_syncrun_errtimer, CASE_syncrun_state_ABORT) ;
            err = insert_syncqueue(&srun->queues[syncrun_qid_RUN], &runentry) ;
            if (err) goto CASE_syncrun_state_ABORT ;
            init_runqueueentry(runentry, &initentry->thread) ;
         } else {
            run2queue_entry_t * run2entry = 0 ;
            ONERROR_testerrortimer(&s_syncrun_errtimer, CASE_syncrun_state_ABORT) ;
            err = insert_syncqueue(&srun->queues[syncrun_qid_RUN2], &run2entry) ;
            if (err) goto CASE_syncrun_state_ABORT ;
            init_run2queueentry(run2entry, &initentry->thread, &initentry->exitevent) ;
         }
         break ;
      case syncrun_state_ABORT:
      CASE_syncrun_state_ABORT:
         (void) callabort_syncthread(&initentry->thread) ;
         retcode = syncrun_returncode_ABORT ;
         // fall through
      case syncrun_state_EXIT:
         if (!isfree_syncevent(&initentry->exitevent)) {
            err = execwaiting_syncrun(srun, waiting_syncevent(&initentry->exitevent), retcode) ;
            if (err) goto ONABORT ;
         }
         break ;
      case syncrun_state_WAIT:
         if (isfree_syncevent(&initentry->exitevent)) {
            waitqueue_entry_t * waitentry ;
            ONERROR_testerrortimer(&s_syncrun_errtimer, CASE_syncrun_state_ABORT) ;
            err = insert_syncqueue(&srun->queues[syncrun_qid_WAIT], &waitentry) ;
            if (err) goto CASE_syncrun_state_ABORT ;
            init_waitqueueentry(waitentry, srun, &initentry->thread) ;
         } else {
            wait2queue_entry_t * wait2entry ;
            ONERROR_testerrortimer(&s_syncrun_errtimer, CASE_syncrun_state_ABORT) ;
            err = insert_syncqueue(&srun->queues[syncrun_qid_WAIT2], &wait2entry) ;
            if (err) goto CASE_syncrun_state_ABORT ;
            init_wait2queueentry(wait2entry, srun, &initentry->thread, &initentry->exitevent) ;
         }
         break ;
      }

      err = removefirst_syncqueue(&srun->queues[syncrun_qid_INIT], entrysize) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int execrunqueue_syncrun(syncrun_t * srun)
{
   int err ;
   dlist_t              freelist  = dlist_INIT ;
   waitqueue_entry_t *  waitentry ;

   static_assert(sizeof(dlist_node_t) <= sizeof(runqueue_entry_t), "free list entries fit in queue entry") ;

   foreach (_queue, entry, genericcast_queue(&srun->queues[syncrun_qid_RUN]), sizeof(runqueue_entry_t)) {
      runqueue_entry_t * runentry = entry ;

      err = preparerun_syncrun(srun, &runentry->thread) ;
      if (err) goto ONABORT ;
      (void) callrun_syncthread(&runentry->thread) ;

      switch ((enum syncrun_state_e)srun->running.state) {
      case syncrun_state_CONTINUE:
         break ;
      case syncrun_state_ABORT:
      CASE_syncrun_state_ABORT:
         (void) callabort_syncthread(&runentry->thread) ;
         // fall through
      case syncrun_state_EXIT:
         addtofreelist_syncqueue(&srun->queues[syncrun_qid_RUN], &freelist, runentry) ;
         break ;
      case syncrun_state_WAIT:
         ONERROR_testerrortimer(&s_syncrun_errtimer, CASE_syncrun_state_ABORT) ;
         err = insert_syncqueue(&srun->queues[syncrun_qid_WAIT], &waitentry) ;
         if (err) goto CASE_syncrun_state_ABORT ;
         init_waitqueueentry(waitentry, srun, &runentry->thread) ;
         addtofreelist_syncqueue(&srun->queues[syncrun_qid_RUN], &freelist, runentry) ;
         break ;
      }
   }

   err = compact_syncqueue(&srun->queues[syncrun_qid_RUN], runqueue_entry_t, &freelist, &initmove_runqueueentry) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   (void) compact_syncqueue(&srun->queues[syncrun_qid_RUN], runqueue_entry_t, &freelist, &initmove_runqueueentry) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int execrun2queue_syncrun(syncrun_t * srun)
{
   int err ;
   dlist_t              freelist = dlist_INIT ;
   wait2queue_entry_t * wait2entry ;

   static_assert(sizeof(dlist_node_t) <= sizeof(run2queue_entry_t), "free list entries fit in queue entry") ;

   foreach (_queue, entry, genericcast_queue(&srun->queues[syncrun_qid_RUN2]), sizeof(run2queue_entry_t)) {
      run2queue_entry_t * run2entry = entry ;

      err = preparerun_syncrun(srun, &run2entry->thread) ;
      if (err) goto ONABORT ;
      int retcode = callrun_syncthread(&run2entry->thread) ;

      switch ((enum syncrun_state_e)srun->running.state) {
      case syncrun_state_CONTINUE:
         break ;
      case syncrun_state_ABORT:
      CASE_syncrun_state_ABORT:
         (void) callabort_syncthread(&run2entry->thread) ;
         retcode = syncrun_returncode_ABORT ;
         // fall through
      case syncrun_state_EXIT:
         err = execwaiting_syncrun(srun, waiting_syncevent(&run2entry->exitevent), retcode) ;
         if (err) goto ONABORT ;
         addtofreelist_syncqueue(&srun->queues[syncrun_qid_RUN2], &freelist, run2entry) ;
         break ;
      case syncrun_state_WAIT:
         ONERROR_testerrortimer(&s_syncrun_errtimer, CASE_syncrun_state_ABORT) ;
         err = insert_syncqueue(&srun->queues[syncrun_qid_WAIT2], &wait2entry) ;
         if (err) goto CASE_syncrun_state_ABORT ;
         init_wait2queueentry(wait2entry, srun, &run2entry->thread, &run2entry->exitevent) ;
         addtofreelist_syncqueue(&srun->queues[syncrun_qid_RUN2], &freelist, run2entry) ;
         break ;
      }
   }

   err = compact_syncqueue(&srun->queues[syncrun_qid_RUN2], run2queue_entry_t, &freelist, &initmove_run2queueentry) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   (void) compact_syncqueue(&srun->queues[syncrun_qid_RUN2], run2queue_entry_t, &freelist, &initmove_run2queueentry) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

/* function: execabort_syncrun
 * Calls <callabort_syncthread> for every thread stored in queue qith id qid.
 * The parameter entrysize gives the size of an entry in the queue.
 * The initqueue is therefore not supported.
 * All queue entries are removed.
 * This function assumes that <clearevents_syncrun> has been called before.
 * If you abort threads stored in queue with qid <syncrun_qid_RUN2> or <syncrun_qid_WAIT2> other threads
 * waiting for them to exit are not woken up. Therefore <execabort_syncrun> is always called in the
 * context of <abortall_syncrun>. */
static int execabort_syncrun(syncrun_t * srun, enum syncrun_qid_e qid, uint16_t entrysize)
{
   int err ;
   int err2 ;

   static_assert( 0 == offsetof(runqueue_entry_t, thread)
                  && 0 == offsetof(run2queue_entry_t, thread)
                  && 0 == offsetof(waitqueue_entry_t, syncwait)
                  && 0 == offsetof(wait2queue_entry_t, syncwait)
                  && 0 == offsetof(typeof(((waitqueue_entry_t*)0)->syncwait), thread)
                  && 0 == offsetof(typeof(((wait2queue_entry_t*)0)->syncwait), thread)
                  , "sycnthread is at offset 0") ;

   err = 0 ;

   foreach (_queue, entry, genericcast_queue(&srun->queues[qid]), entrysize) {
      syncthread_t * syncthread = entry ;
      err2 = preparerun_syncrun(srun, syncthread) ;
      if (err2) err = err2 ;
      (void) callabort_syncthread(syncthread) ;
   }

   err2 = free_syncqueue(&srun->queues[qid]) ;
   if (err2) err = err2 ;
   init_syncqueue(&srun->queues[qid]) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

/* function: clearevents_syncrun
 * Set all <syncevent_t.waiting> pointer to 0 which reference threads in the wait queues.
 * This ensures that events do not referencewaiting threads after the waiting queues are
 * freed. */
static int clearevents_syncrun(syncrun_t * srun)
{
   static_assert(syncrun_qid_WAIT + 1 == syncrun_qid_WAIT2, "for loop assumes that") ;
   for (unsigned qid = syncrun_qid_WAIT; qid <= syncrun_qid_WAIT2; ++qid) {

      if (! len_syncqueue(&srun->queues[qid])) continue ;

      uint16_t entrysize = (qid == syncrun_qid_WAIT ? sizeof(waitqueue_entry_t) : sizeof(wait2queue_entry_t)) ;

      foreach (_queue, entry, genericcast_queue(&srun->queues[qid]), entrysize) {
         waitqueue_entry_t * waitentry = entry ;

         *waitentry->syncwait.event = (syncevent_t) syncevent_INIT_FREEABLE ;
      }
   }

   return 0 ;
}

int runall_syncrun(syncrun_t * srun)
{
   int err ;
   syncqueue_t    copyqueue = syncqueue_INIT_FREEABLE ;
   syncwlist_t    copylist  = syncwlist_INIT_FREEABLE ;

   if (srun->running.thread) {
      err = EINPROGRESS ;
      goto ONABORT ;
   }

   // - prepare -
   // (running.thread!=0) ==> indicates runall_syncrun "is in progress"
   err = preparerun_syncrun(srun, (void*)1/*dummy*/) ;
   if (err) goto ONABORT ;

   // - run queues -
   err = execrunqueue_syncrun(srun) ;
   if (err) goto ONABORT ;
   err = execrun2queue_syncrun(srun) ;
   if (err) goto ONABORT ;

   // - init queue -
   err = execinitqueue_syncrun(srun) ;
   if (err) goto ONABORT ;

   // - wait queues -

   // - copy and clear -
   // initmove_queue not called => queuefromaddr_queue will return wrong pointer (but is not used)
   copyqueue = srun->queues[syncrun_qid_WAKEUP] ;
   srun->queues[syncrun_qid_WAKEUP] = (syncqueue_t) syncqueue_INIT ;
   initmove_syncwlist(&copylist, &srun->wakeup_list) ;

   // - wakeup list -
   while (!isempty_syncwlist(&copylist)) {
      syncevent_t event ;
      err = remove_syncwlist(&copylist, &srun->queues[syncrun_qid_WLIST], &event) ;
      if (err) goto ONABORT ;
      if (iswaiting_syncevent(&event)) {
         err = execwaiting_syncrun(srun, waiting_syncevent(&event), 0/*dummy*/) ;
         if (err) goto ONABORT ;
      }
   }

   // - wakeup queue -
   while (len_syncqueue(&copyqueue)) {
      syncevent_t * event = first_queue(genericcast_queue(&copyqueue), sizeof(syncevent_t)) ;
      if (!event) {
         err = EINVAL ;
         goto ONABORT ;
      }
      // queue contains only initialized events
      err = execwaiting_syncrun(srun, waiting_syncevent(event), 0/*dummy*/) ;
      if (err) goto ONABORT ;
      err = removefirst_syncqueue(&copyqueue, sizeof(syncevent_t)) ;
      if (err) goto ONABORT ;
   }

   // - free copy -
   err = free_syncwlist(&copylist, &srun->queues[syncrun_qid_WLIST]) ;
   if (err) goto ONABORT ;
   err = free_syncqueue(&copyqueue) ;
   if (err) goto ONABORT ;

   // - unprepare -
   err = preparerun_syncrun(srun, 0) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   free_syncqueue(&copyqueue) ;
   free_syncwlist(&copylist, &srun->queues[syncrun_qid_WLIST]) ;
   preparerun_syncrun(srun, 0) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int abortall_syncrun(syncrun_t * srun)
{
   int err ;
   int err2 ;

   // ensures that all <syncevent_t.waiting> pointer are set to 0
   // after freeing the wait queue they would become invalid
   err = clearevents_syncrun(srun) ;

   err2 = free_syncwlist(&srun->wakeup_list, &srun->queues[syncrun_qid_WLIST]) ;
   if (err2) err = err2 ;
   err2 = free_syncqueue(&srun->queues[syncrun_qid_WAKEUP]) ;
   if (err2) err = err2 ;

   struct {
      enum syncrun_qid_e   qid ;
      uint16_t             entrysize ;
   } queues[4] = {

      { syncrun_qid_RUN,   sizeof(runqueue_entry_t)   },
      { syncrun_qid_RUN2,  sizeof(run2queue_entry_t)  },
      { syncrun_qid_WAIT,  sizeof(waitqueue_entry_t)  },
      { syncrun_qid_WAIT2, sizeof(wait2queue_entry_t) }

   } ;

   // abort all threads stored in queues and free content of queue
   for (unsigned i = 0; i < lengthof(queues); ++i) {
      err2 = execabort_syncrun(srun, queues[i].qid, queues[i].entrysize) ;
      if (err2) err = err2 ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initqueueentry(void)
{
   // TEST sizeentry_initqueueentry
   TEST(sizeentry_initqueueentry(0) == sizeof(initqueue_entry_t)) ;
   for (unsigned i = 1; i <= 255; ++i) {
      uint16_t entrysize = sizeentry_initqueueentry((uint8_t)i) ;
      TEST(entrysize >= sizeof(initqueue_entry_t)+i) ;
      TEST(entrysize <  2*sizeof(initqueue_entry_t)+i) ;
      TEST(0 == entrysize % sizeof(initqueue_entry_t)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   syncrun_t  srun = syncrun_INIT_FREEABLE ;

   // TEST syncrun_INIT_FREEABLE
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST init_syncrun, free_syncrun
   memset(&srun, 255, sizeof(srun)) ;
   TEST(0 == init_syncrun(&srun)) ;
   TEST(1 == isfree_syncrun(&srun)) ;
   static_assert(offsetof(syncrun_t, queues) == 0, "ensures following memset is correct") ;
   memset(sizeof(srun.queues) + (uint8_t*)srun.queues, 255, sizeof(srun) - sizeof(srun.queues)) ;
   for (unsigned i = 0; i < lengthof(srun.queues); ++i) {
      void * dummy ;
      TEST(0 == insertlast_queue(genericcast_queue(&srun.queues[i]), &dummy, 256)) ;
      TEST(0 != srun.queues[i].last) ;
   }
   TEST(0 == isfree_syncrun(&srun)) ;
   TEST(0 == free_syncrun(&srun)) ;
   TEST(1 == isfree_syncrun(&srun)) ;
   TEST(0 == free_syncrun(&srun)) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   syncrun_t   srun = syncrun_INIT_FREEABLE ;
   syncevent_t event ;

   // prepare
   TEST(0 == init_syncrun(&srun)) ;

   // TEST isfree_syncrun
   struct {
      size_t offset ;
      size_t size ;
   } syncrun_members[] = {
      { offsetof(syncrun_t, queues), sizeof(srun.queues) },
      { offsetof(syncrun_t, wakeup) + offsetof(typeof(srun.wakeup), continuelabel), sizeof(srun.wakeup.continuelabel) },
      { offsetof(syncrun_t, wakeup) + offsetof(typeof(srun.wakeup), retcode), sizeof(srun.wakeup.retcode) },
      { offsetof(syncrun_t, waitinfo) + offsetof(typeof(srun.waitinfo), event), sizeof(srun.waitinfo.event) },
      { offsetof(syncrun_t, waitinfo) + offsetof(typeof(srun.waitinfo), continuelabel), sizeof(srun.waitinfo.continuelabel) },
      { offsetof(syncrun_t, running) + offsetof(typeof(srun.running), laststarted), sizeof(srun.running.laststarted) },
      { offsetof(syncrun_t, running) + offsetof(typeof(srun.running), thread), sizeof(srun.running.thread) },
      { offsetof(syncrun_t, running) + offsetof(typeof(srun.running), state), sizeof(srun.running.state) }
   } ;
   TEST(1 == isfree_syncrun(&srun)) ;
   for (unsigned i = 0; i < lengthof(syncrun_members); ++i) {
      for (size_t offset = 0; offset < syncrun_members[i].size; ++offset) {
         memset(syncrun_members[i].offset + offset + (uint8_t*)&srun, 1, 1) ;
         TEST(0 == isfree_syncrun(&srun)) ;
         memset(syncrun_members[i].offset + offset + (uint8_t*)&srun, 0, 1) ;
         TEST(1 == isfree_syncrun(&srun)) ;
      }
   }

   // TEST continuelabel_syncrun
   srun.wakeup.continuelabel = 0 ;
   TEST(0 == continuelabel_syncrun(&srun)) ;
   for (uintptr_t i = 1; i; i <<= 1) {
      srun.wakeup.continuelabel = (void*)i ;
      TEST((void*)i == continuelabel_syncrun(&srun)) ;
   }

   // TEST retcode_syncrun
   srun.wakeup.retcode = 0 ;
   TEST(0 == retcode_syncrun(&srun)) ;
   for (int i = 1; i; i <<= 1) {
      srun.wakeup.retcode = i ;
      TEST(i == retcode_syncrun(&srun)) ;
   }

   // TEST leninitqueue_syncrun
   for (unsigned i = 1; i <= 1000; ++i) {
      initqueue_entry_t * entry ;
      TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_INIT], &entry))
      TEST(i == leninitqueue_syncrun(&srun)) ;
   }

   // TEST lenrunqueue_syncrun
   for (unsigned i = 1; i <= 1000; i += 1) {
      runqueue_entry_t * entry ;
      run2queue_entry_t * entry2 ;
      TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_RUN], &entry))
      TEST(2*i-1 == lenrunqueue_syncrun(&srun)) ;
      TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_RUN2], &entry2))
      TEST(2*i   == lenrunqueue_syncrun(&srun)) ;
   }

   // TEST lenwaitqueue_syncrun
   srun.waitinfo.event         = &event ;
   srun.waitinfo.continuelabel = 0 ;
   for (unsigned i = 1; i <= 1000; i += 1) {
      waitqueue_entry_t * entry ;
      wait2queue_entry_t * entry2 ;
      TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAIT], &entry))
      init_waitqueueentry(entry, &srun, &(syncthread_t)syncthread_INIT_FREEABLE) ;
      TEST(2*i-1 == lenwaitqueue_syncrun(&srun)) ;
      TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAIT2], &entry2))
      init_wait2queueentry(entry2, &srun, &(syncthread_t)syncthread_INIT_FREEABLE, &(syncevent_t)syncevent_INIT(&entry->syncwait)) ;
      TEST(2*i   == lenwaitqueue_syncrun(&srun)) ;
   }

   // TEST leninitqueue_syncrun, lenrunqueue_syncrun, lenwaitqueue_syncrun: other queues are not considered
   int * dummy ;
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAKEUP], &dummy))
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WLIST], &dummy))
   TEST(1000 == leninitqueue_syncrun(&srun)) ;
   TEST(2000 == lenrunqueue_syncrun(&srun)) ;
   TEST(2000 == lenwaitqueue_syncrun(&srun)) ;

   // unprepare
   TEST(0 == free_syncrun(&srun)) ;

   return 0 ;
ONABORT:
   free_syncrun(&srun) ;
   return EINVAL ;
}

static int test_internal(void)
{
   syncrun_t      srun         = syncrun_INIT_FREEABLE ;
   syncevent_t    events[100]  = { syncevent_INIT_FREEABLE } ;
   syncevent_t *  events2[100] = { 0 } ;
   syncwlist_t    wlist        = syncwlist_INIT_FREEABLE ;

   // TEST setstateabort_syncrun
   memset(&srun, 0, sizeof(srun)) ;
   setstateabort_syncrun(&srun) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   setstateabort_syncrun(&srun) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   // check that only srun.running.state is changed
   TEST(0 == isfree_syncrun(&srun)) ;
   srun.running.state = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST setstateexit_syncrun
   memset(&srun, 0, sizeof(srun)) ;
   for (unsigned i = 0, isvalid = 1; isvalid; ++i) {
      srun.running.state = (enum syncrun_state_e)i ;
      setstateexit_syncrun(&srun) ;
      uint8_t teststate = srun.running.state ;
      srun.running.state = 0 ;
      // check that only state is changed
      TEST(1 == isfree_syncrun(&srun)) ;
      switch ((enum syncrun_state_e)i) {
      case syncrun_state_CONTINUE:  // OK
         TEST(teststate == syncrun_state_EXIT) ;
         continue ;
      case syncrun_state_ABORT:     // error
      case syncrun_state_EXIT:
      case syncrun_state_WAIT:
         TEST(teststate == syncrun_state_ABORT) ;
         continue ;
      }
      // error
      TEST(teststate == syncrun_state_ABORT) ;
      isvalid = 0 ;
   }

   // TEST setstatewait_syncrun: different arguments
   for (size_t i = 0; i < lengthof(events); ++ i) {
      events[i] = (syncevent_t) syncevent_INIT_FREEABLE ;
      srun.running.state = syncrun_state_CONTINUE ;
      srun.waitinfo.wlist = (void*)1 ;
      setstatewait_syncrun(&srun, &events[i], (void*)i) ; // OK
      TEST(srun.waitinfo.wlist         == 0) ;
      TEST(srun.waitinfo.event         == &events[i]) ;
      TEST(srun.waitinfo.continuelabel == (void*)i) ;
      TEST(srun.running.state          == syncrun_state_WAIT) ;
      // check that only waitinfo and state is changed
      srun.waitinfo.event         = 0 ;
      srun.waitinfo.continuelabel = 0 ;
      srun.running.state          = 0 ;
      TEST(1 == isfree_syncrun(&srun)) ;
   }

   // TEST setstatewait_syncrun: different states
   for (unsigned i = 0, isvalid = 1; isvalid; ++i) {
      srun.running.state  = (uint8_t) i ;
      srun.waitinfo.wlist = (void*) 1 ;
      setstatewait_syncrun(&srun, &events[0], (void*)1) ;
      isvalid = 0 ;
      switch ((enum syncrun_state_e)i) {
      case syncrun_state_CONTINUE:  // OK
         isvalid = 1 ;
         TEST(srun.waitinfo.wlist         == 0) ;
         TEST(srun.waitinfo.event         == &events[0]) ;
         TEST(srun.waitinfo.continuelabel == (void*)1) ;
         TEST(srun.running.state          == syncrun_state_WAIT) ;
         srun.waitinfo.event         = 0 ;
         srun.waitinfo.continuelabel = 0 ;
         break ;
      case syncrun_state_ABORT:     // error
      case syncrun_state_EXIT:
      case syncrun_state_WAIT:
         isvalid = 1 ;
         // only state is set to abort (waitinfo keeps 0 values)
         TEST(srun.waitinfo.wlist == (void*)1) ;
         TEST(srun.running.state  == syncrun_state_ABORT) ;
         break ;
      }
      // check that only state is changed
      srun.waitinfo.wlist = 0 ;
      srun.running.state  = 0 ;
      TEST(1 == isfree_syncrun(&srun)) ;
   }

   // TEST setstatewait_syncrun: EINVAL
   srun.waitinfo.wlist = (void*)1 ;
   setstatewait_syncrun(&srun, 0/*no last started thread*/, (void*)3) ;
   TEST(srun.waitinfo.wlist == (void*)1) ;
   TEST(srun.running.state  == syncrun_state_ABORT) ;
   srun.waitinfo.wlist = 0 ;
   srun.running.state  = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;
   srun.waitinfo.wlist = (void*)1 ;
   srun.running.state  = 0 ;
   events[0].waiting   = (void*)1 ;  // someone waiting on this event already
   setstatewait_syncrun(&srun, &events[0], (void*)3) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   TEST(srun.waitinfo.wlist == (void*)1) ;
   srun.waitinfo.wlist = 0 ;
   srun.running.state  = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST setstatewaitlist_syncrun: multiple entries
   init_syncwlist(&wlist) ;
   for (size_t i = 0; i < lengthof(events2); ++ i) {
      srun.running.state = syncrun_state_CONTINUE ;
      setstatewaitlist_syncrun(&srun, &wlist, (void*)i) ;
      events2[i] = srun.waitinfo.event ;
      TEST(len_syncwlist(&wlist)       == 1+i) ;
      TEST(srun.waitinfo.wlist         == &wlist) ;
      TEST(srun.waitinfo.event         != 0) ;
      TEST(srun.waitinfo.event         == last_syncwlist(&wlist)) ;
      TEST(srun.waitinfo.continuelabel == (void*)i) ;
      TEST(srun.running.state          == syncrun_state_WAIT) ;
      TEST(isfree_syncevent(events2[i])== 1) ;
      syncqueue_t * queue = queuefromaddr_syncqueue(events2[i]) ;
      TEST(queue                       == &srun.queues[syncrun_qid_WLIST]) ;
      TEST(i == 0 || events2[i-1]      <  events2[i]) ;
      srun.running.state               = 0 ;
   }
   // check that only waitinfo and state is changed
   srun.waitinfo.wlist         = 0 ;
   srun.waitinfo.event         = 0 ;
   srun.waitinfo.continuelabel = 0 ;
   srun.running.state          = 0 ;
   TEST(0 == free_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST])) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST setstatewait_syncrun: different states
   init_syncwlist(&wlist) ;
   for (unsigned i = 0, isvalid = 1; isvalid; ++i) {
      srun.running.state = (uint8_t) i ;
      setstatewaitlist_syncrun(&srun, &wlist, (void*)1) ;
      isvalid = 0 ;
      switch ((enum syncrun_state_e)i) {
      case syncrun_state_CONTINUE:  // OK
         isvalid = 1 ;
         TEST(srun.waitinfo.wlist         == &wlist) ;
         TEST(srun.waitinfo.event         == last_syncwlist(&wlist)) ;
         TEST(srun.waitinfo.continuelabel == (void*)1) ;
         TEST(srun.running.state          == syncrun_state_WAIT) ;
         srun.waitinfo.wlist         = 0 ;
         srun.waitinfo.event         = 0 ;
         srun.waitinfo.continuelabel = 0 ;
         TEST(1 == len_syncwlist(&wlist)) ;
         TEST(0 == free_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST])) ;
         init_syncwlist(&wlist) ;
         break ;
      case syncrun_state_ABORT:     // error
      case syncrun_state_EXIT:
      case syncrun_state_WAIT:
         isvalid = 1 ;
         // only state is set to abort (waitinfo keeps 0 values)
         TEST(srun.running.state == syncrun_state_ABORT) ;
         break ;
      }
      // check that only state is changed
      srun.running.state = 0 ;
      TEST(0 == len_syncwlist(&wlist)) ;
      TEST(1 == isfree_syncrun(&srun)) ;
   }

   // TEST setstatewaitlist_syncrun: EINVAL/ENOMEM
   init_syncwlist(&wlist) ;
   TEST(0 == insert_syncwlist(&wlist, &srun.queues[syncrun_qid_RUN], &events2[0])) ;
   setstatewaitlist_syncrun(&srun, &wlist, (void*)3) ;   // wrong queue
   TEST(srun.running.state == syncrun_state_ABORT) ;
   TEST(0 == free_syncwlist(&wlist, &srun.queues[syncrun_qid_RUN])) ;
   srun.running.state = 0;
   TEST(1 == isfree_syncrun(&srun)) ;
   init_testerrortimer(&s_syncrun_errtimer, 1, ENOMEM) ;
   init_syncwlist(&wlist) ;
   setstatewaitlist_syncrun(&srun, &wlist, (void*)3) ;   // out of memory
   TEST(srun.running.state == syncrun_state_ABORT) ;
   srun.running.state = 0;
   TEST(0 == len_syncwlist(&wlist)) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   return 0 ;
ONABORT:
   free_syncrun(&srun) ;
   return EINVAL ;
}

static int call_abortthread(syncrun_t * srun)
{
   abortthread_syncrun(srun) ;
}

static int call_exitthread(syncrun_t * srun, int err)
{
   exitthread_syncrun(srun, err) ;
}

static int test_threadlifetime(void)
{
   syncrun_t   srun      = syncrun_INIT_FREEABLE ;
   queue_t *   initqueue = genericcast_queue(&srun.queues[syncrun_qid_INIT]) ;

   // TEST startthread_syncrun
   TEST(0 == init_syncrun(&srun)) ;
   for (uintptr_t i = 1, size = 0; i <= 10000; ++i) {
      srun.running.laststarted = 0 ;
      TEST(0 == startthread_syncrun(&srun, (syncrun_f)i, (void*)(i+1))) ;
      TEST(i == leninitqueue_syncrun(&srun)) ;
      size += sizeof(initqueue_entry_t) ;
      if (size != sizelast_queue(initqueue)) {
         size = sizeof(initqueue_entry_t) ;
      }
      TEST(size == sizelast_queue(initqueue)) ;
      initqueue_entry_t * initentry = last_queue(initqueue, sizeof(initqueue_entry_t)) ;
      TEST(&initentry->exitevent == srun.running.laststarted) ;
   }
   for (uintptr_t i = 0; i == 0; ) {
      foreach (_queue, entry, initqueue, sizeof(initqueue_entry_t)) {
         ++ i ;
         initqueue_entry_t * initentry = entry ;
         TEST(initentry->thread.mainfct == (syncrun_f)i) ;
         TEST(initentry->thread.state   == (void*)(i+1)) ;
         TEST(isfree_syncevent(&initentry->exitevent)) ;
         TEST(initentry->initargsize    == 0) ;
      }
      TEST(i == 10000) ;
   }
   TEST(0 == free_syncrun(&srun)) ;

   // TEST startthread2_syncrun
   TEST(0 == init_syncrun(&srun)) ;
   for (uintptr_t i = 1, size = 0; i <= 10000; ++i) {
      void * initarg ;
      srun.running.laststarted = 0 ;
      TEST(0 == startthread2_syncrun(&srun, (syncrun_f)i, (uint8_t)i, &initarg)) ;
      TEST(i == leninitqueue_syncrun(&srun)) ;
      size += sizeentry_initqueueentry((uint8_t)i) ;
      if (size != sizelast_queue(initqueue)) {
         size = sizeentry_initqueueentry((uint8_t)i) ;
      }
      TEST(size == sizelast_queue(initqueue)) ;
      initqueue_entry_t * initentry = last_queue(initqueue, sizeentry_initqueueentry((uint8_t)i)) ;
      TEST(initentry->initarg    == initarg) ;
      TEST(&initentry->exitevent == srun.running.laststarted) ;
      memset(initarg, (uint8_t)i, (uint8_t)i) ;
   }
   for (uintptr_t i = 0; i == 0; ) {
      foreach (_queue, entry, initqueue, sizeof(initqueue_entry_t)) {
         ++ i ;
         initqueue_entry_t * initentry = entry ;
         TEST(initentry->thread.mainfct == (syncrun_f)i) ;
         TEST(initentry->thread.state   == initentry->initarg) ;
         TEST(isfree_syncevent(&initentry->exitevent)) ;
         TEST(initentry->initargsize    == (uint8_t)i) ;
         uint16_t argsize = (uint16_t) (sizeentry_initqueueentry((uint8_t)i) - sizeof(initqueue_entry_t)) ;
         TEST(nextskip_queueiterator(&iter_entry, argsize)) ;
         // check arg content
         for (uint8_t i2 = 0; i2 < (uint8_t)i; ++i2) {
            TEST(((uint8_t*)initentry->initarg)[i2] == (uint8_t)i) ;
         }
      }
      TEST(i == 10000) ;
   }
   TEST(0 == free_syncrun(&srun)) ;

   // TEST abortthread_syncrun
   TEST(0 == init_syncrun(&srun)) ;
   // abortthread_syncrun returns 0
   TEST(0 == call_abortthread(&srun)) ;
   // abortthread_syncrun changes state only
   TEST(srun.running.state == syncrun_state_ABORT) ;
   srun.running.state = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;
   TEST(0 == free_syncrun(&srun)) ;

   // TEST exitthread_syncrun
   for (int i = -10; i <= 10; i += 5) {
      TEST(0 == init_syncrun(&srun)) ;
      // exitthread_syncrun returns err
      TEST(i == call_exitthread(&srun, i)) ;
      // exitthread_syncrun changes state only
      TEST(srun.running.state == syncrun_state_EXIT) ;
      srun.running.state = 0 ;
      TEST(1 == isfree_syncrun(&srun)) ;
      TEST(0 == free_syncrun(&srun)) ;
   }

   // TEST exitthread_syncrun: EINVAL
   for (uint8_t state = 0, isvalid = 1; isvalid; ++state) {
      isvalid = false ;
      switch ((syncrun_state_e)state) {
      case syncrun_state_CONTINUE:
         isvalid = true ;
         continue ;
      case syncrun_state_ABORT:
      case syncrun_state_EXIT:
      case syncrun_state_WAIT:
         isvalid = true ;
         break ;
      }
      TEST(0 == init_syncrun(&srun)) ;
      srun.running.state = state ;
      TEST(0 == call_exitthread(&srun, 0)) ;
      // exitthread_syncrun changes state to syncrun_state_ABORT
      TEST(srun.running.state == syncrun_state_ABORT) ;
      srun.running.state = 0 ;
      TEST(1 == isfree_syncrun(&srun)) ;
      TEST(0 == free_syncrun(&srun)) ;
   }

   return 0 ;
ONABORT:
   free_syncrun(&srun) ;
   return EINVAL ;
}

static int call_waitforexit_syncrun(syncrun_t * srun)
{
   int err ;

   if (continuelabel_syncrun(srun)) {
      err = EINVAL ;
      handlesignal_syncthread(syncthread_signal_WAKEUP, continuelabel_syncrun(srun), ONABORT, ONABORT, ONABORT) ;
      goto ONABORT ;
   }

   err = waitforexit_syncrun(srun) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

static int call_waitforevent_syncrun(syncrun_t * srun, syncevent_t * syncevent)
{
   int err ;

   if (continuelabel_syncrun(srun)) {
      err = EINVAL ;
      handlesignal_syncthread(syncthread_signal_WAKEUP, continuelabel_syncrun(srun), ONABORT, ONABORT, ONABORT) ;
      goto ONABORT ;
   }

   waitforevent_syncrun(srun, syncevent) ;

   return 0 ;
ONABORT:
   return err ;
}

static int call_waitforlist_syncrun(syncrun_t * srun, syncwlist_t * syncwlist)
{
   int err ;

   if (continuelabel_syncrun(srun)) {
      err = EINVAL ;
      handlesignal_syncthread(syncthread_signal_WAKEUP, continuelabel_syncrun(srun), ONABORT, ONABORT, ONABORT) ;
      goto ONABORT ;
   }

   waitforlist_syncrun(srun, syncwlist) ;

   return 0 ;
ONABORT:
   return err ;
}

static int test_synchronize(void)
{
   syncrun_t      srun  = syncrun_INIT_FREEABLE ;
   syncevent_t    event = syncevent_INIT_FREEABLE ;
   syncwlist_t    wlist ;
   void *         continuelabel ;

   // prepare
   TEST(0 == init_syncrun(&srun)) ;
   // replace isfree with isinitstate is needed (some value != 0)
   // initstate is the same as freestate
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST waitforexit_syncrun
   srun.running.laststarted = &event ;
   TEST(0 == call_waitforexit_syncrun(&srun)) ;
   TEST(srun.waitinfo.event          == &event) ;
   TEST(srun.waitinfo.continuelabel != 0);
   TEST(srun.running.state          == syncrun_state_WAIT) ;
   continuelabel = srun.waitinfo.continuelabel ;
   srun.running.laststarted    = 0 ;
   srun.waitinfo.event         = 0 ;
   srun.waitinfo.continuelabel = 0 ;
   srun.running.state          = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;
   for (int rc = -2; rc <= 0; ++rc) {
      srun.wakeup.continuelabel = continuelabel ;
      srun.wakeup.retcode       = rc ;
      TEST(rc == call_waitforexit_syncrun(&srun)) ;
   }
   srun.wakeup.continuelabel = 0 ;
   srun.wakeup.retcode       = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST waitforexit_syncrun: EINVAL
   TEST(0 == call_waitforexit_syncrun(&srun)) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   srun.running.state = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;
   srun.running.laststarted = &event ;
   srun.running.state       = syncrun_state_WAIT ;
   TEST(0 == call_waitforexit_syncrun(&srun)) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   srun.running.laststarted = 0 ;
   srun.running.state       = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST waitforevent_syncrun
   TEST(0 == call_waitforevent_syncrun(&srun, &event)) ;
   TEST(srun.waitinfo.event         == &event) ;
   TEST(srun.waitinfo.continuelabel != 0);
   TEST(srun.running.state          == syncrun_state_WAIT) ;
   continuelabel = srun.waitinfo.continuelabel ;
   srun.waitinfo.event         = 0 ;
   srun.waitinfo.continuelabel = 0 ;
   srun.running.state          = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;
   for (int rc = -1; rc <= 1; ++rc) {
      srun.wakeup.continuelabel = continuelabel ;
      TEST(0 == call_waitforevent_syncrun(&srun, 0)) ;
   }
   srun.wakeup.continuelabel = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST waitforevent_syncrun: EINVAL
   event.waiting = (void*) 1 ;
   TEST(0 == call_waitforevent_syncrun(&srun, &event)) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   srun.running.state = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;
   srun.running.laststarted = &event ;
   srun.running.state       = syncrun_state_WAIT ;
   TEST(0 == call_waitforevent_syncrun(&srun, &event)) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   srun.running.laststarted = 0 ;
   srun.running.state       = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST waitforlist_syncrun
   init_syncwlist(&wlist) ;
   for (unsigned i = 1, nodesize = 0; i <= 5; ++i) {
      queue_t * queue = genericcast_queue(&srun.queues[syncrun_qid_WLIST]) ;
      call_waitforlist_syncrun(&srun, &wlist) ;
      TEST(srun.waitinfo.event         != 0) ;
      TEST(srun.waitinfo.continuelabel != 0) ;
      TEST(srun.running.state          == syncrun_state_WAIT) ;
      TEST(srun.waitinfo.event         == last_syncwlist(&wlist)) ;
      TEST(len_syncwlist(&wlist)       == i) ;
      if (!nodesize) {
         nodesize = sizefirst_queue(queue) ;
         TEST(nodesize > sizeof(syncevent_t)) ;
      }
      TEST(sizefirst_queue(queue)      == nodesize * i) ;
      continuelabel = srun.waitinfo.continuelabel ;
      for (int rc = -1; rc <= 1; ++rc) {
         srun.wakeup.continuelabel = continuelabel ;
         TEST(0 == call_waitforlist_syncrun(&srun, &wlist)) ;
      }
      srun.wakeup.continuelabel = 0 ;
      srun.running.state        = 0 ;
   }
   srun.waitinfo.event         = 0 ;
   srun.waitinfo.continuelabel = 0 ;
   TEST(0 == free_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST])) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST waitforlist_syncrun: EINVAL
   syncevent_t * event2 ;
   init_syncwlist(&wlist) ;
   TEST(0 == insert_syncwlist(&wlist, &srun.queues[syncrun_qid_RUN], &event2)) ;  // wrong queue !
   TEST(0 == call_waitforlist_syncrun(&srun, &wlist)) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   TEST(0 == free_syncwlist(&wlist, &srun.queues[syncrun_qid_RUN])) ;
   init_syncwlist(&wlist) ;
   srun.running.state      = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;
   srun.running.state      = syncrun_state_WAIT ;
   TEST(0 == call_waitforlist_syncrun(&srun, &wlist)) ;
   TEST(srun.running.state == syncrun_state_ABORT) ;
   srun.running.state      = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST signalevent_syncrun
   syncqueue_t * wakeup = &srun.queues[syncrun_qid_WAKEUP] ;
   event = (syncevent_t) syncevent_INIT_FREEABLE ;
   TEST(0 == signalevent_syncrun(&srun, &event)) ; // will be ignored
   TEST(1 == isfree_syncrun(&srun)) ;              // has been ignored
   waitqueue_entry_t * waitentry ;
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAIT], &waitentry)) ;
   init_syncwait(&waitentry->syncwait, &(syncthread_t)syncthread_INIT_FREEABLE, &event, 0) ;
   TEST(0 == signalevent_syncrun(&srun, &event)) ; // passes
   TEST(0 == iswaiting_syncevent(&event)) ;        // cleared
   TEST(1 == len_syncqueue(wakeup)) ;
   TEST(0 != waitentry->syncwait.event) ;
   TEST(&event != waitentry->syncwait.event) ;
   TEST(wakeup == queuefromaddr_syncqueue(waitentry->syncwait.event)) ;
   wait2queue_entry_t * wait2entry ;
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAIT2], &wait2entry)) ;
   init_syncwait(&wait2entry->syncwait, &(syncthread_t)syncthread_INIT_FREEABLE, &event, 0) ;
   TEST(0 == signalevent_syncrun(&srun, &event)) ; // passes
   TEST(0 == iswaiting_syncevent(&event)) ;        // cleared
   TEST(2 == len_syncqueue(wakeup)) ;
   TEST(waitentry->syncwait.event + 1 == wait2entry->syncwait.event) ;   // next in queue
   // nothing else changed
   TEST(0 == free_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
   TEST(0 == free_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
   TEST(0 == free_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
   init_syncqueue(&srun.queues[syncrun_qid_WAKEUP]) ;
   init_syncqueue(&srun.queues[syncrun_qid_WAIT]) ;
   init_syncqueue(&srun.queues[syncrun_qid_WAIT2]) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST signalevent_syncrun: EINVAL/ENOMEM
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_RUN], &waitentry)) ;
   init_syncwait(&waitentry->syncwait, &(syncthread_t)syncthread_INIT_FREEABLE, &event, 0) ;
   TEST(EINVAL == signalevent_syncrun(&srun, &event)) ;   // wrong queue
   TEST(1 == iswaiting_syncevent(&event))
   TEST(0 == free_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
   init_syncqueue(&srun.queues[syncrun_qid_RUN]) ;
   TEST(1 == isfree_syncrun(&srun)) ;
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAIT], &waitentry)) ;
   init_syncwait(&waitentry->syncwait, &(syncthread_t)syncthread_INIT_FREEABLE, &event, 0) ;
   waitentry->syncwait.event = 0 ;                       // clear backpointer
   TEST(EINVAL == signalevent_syncrun(&srun, &event)) ;  // wrong backpointer
   TEST(1 == iswaiting_syncevent(&event))
   TEST(0 == free_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
   init_syncqueue(&srun.queues[syncrun_qid_WAIT]) ;
   TEST(1 == isfree_syncrun(&srun)) ;
   init_testerrortimer(&s_syncrun_errtimer, 1, ENOMEM) ;
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAIT], &waitentry)) ;
   init_syncwait(&waitentry->syncwait, &(syncthread_t)syncthread_INIT_FREEABLE, &event, 0) ;
   TEST(ENOMEM == signalevent_syncrun(&srun, &event)) ;  // errtimer !
   TEST(1 == iswaiting_syncevent(&event))
   TEST(0 == free_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
   init_syncqueue(&srun.queues[syncrun_qid_WAIT]) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST signalfirst_syncrun
   init_syncwlist(&wlist) ;
   // empty list will be ignored
   TEST(0 == signalfirst_syncrun(&srun, &wlist)) ;
   TEST(1 == isfree_syncrun(&srun)) ;              // has been ignored
   // single entry will be transfered
   TEST(0 == insert_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST], &event2)) ;
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAIT], &waitentry)) ;
   init_syncwait(&waitentry->syncwait, &(syncthread_t)syncthread_INIT_FREEABLE, event2, 0) ;
   TEST(0 == signalfirst_syncrun(&srun, &wlist)) ;
   TEST(event2 == event_syncwait(&waitentry->syncwait)) ;
   TEST(1 == len_syncwlist(&srun.wakeup_list)) ;
   TEST(0 == len_syncwlist(&wlist)) ;
   // empty entry will be accepted also
   TEST(0 == insert_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST], &event2)) ;
   TEST(0 == signalfirst_syncrun(&srun, &wlist)) ;
   TEST(2 == len_syncwlist(&srun.wakeup_list)) ;
   TEST(0 == len_syncwlist(&wlist)) ;
   TEST(0 == free_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
   init_syncqueue(&srun.queues[syncrun_qid_WAIT]) ;
   TEST(0 == remove_syncwlist(&srun.wakeup_list, &srun.queues[syncrun_qid_WLIST], &event)) ;
   TEST(0 == remove_syncwlist(&srun.wakeup_list, &srun.queues[syncrun_qid_WLIST], &event)) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST signalfirst_syncrun: EINVAL
   TEST(0 == insert_syncwlist(&wlist, &srun.queues[syncrun_qid_INIT], &event2)) ;
   TEST(EINVAL == signalfirst_syncrun(&srun, &wlist)) ;  // wrong queue
   TEST(0 == remove_syncwlist(&wlist, &srun.queues[syncrun_qid_INIT], &event)) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST signalall_syncrun
   init_syncwlist(&wlist) ;
   // empty list will be ignored
   TEST(0 == signalall_syncrun(&srun, &wlist)) ;
   TEST(1 == isfree_syncrun(&srun)) ;              // has been ignored
   // single entry will be transfered
   TEST(0 == insert_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST], &event2)) ;
   TEST(0 == insert_syncqueue(&srun.queues[syncrun_qid_WAIT], &waitentry)) ;
   init_syncwait(&waitentry->syncwait, &(syncthread_t)syncthread_INIT_FREEABLE, event2, 0) ;
   TEST(0 == signalall_syncrun(&srun, &wlist)) ;
   TEST(event2 == event_syncwait(&waitentry->syncwait)) ;
   TEST(1 == len_syncwlist(&srun.wakeup_list)) ;
   TEST(0 == len_syncwlist(&wlist)) ;
   // empty entries will be accepted also
   for (unsigned i = 0; i < 100; ++i) {
      TEST(0 == insert_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST], &event2)) ;
   }
   TEST(0 == signalall_syncrun(&srun, &wlist)) ;
   TEST(101 == len_syncwlist(&srun.wakeup_list)) ;
   TEST(0 == len_syncwlist(&wlist)) ;
   TEST(0 == free_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
   init_syncqueue(&srun.queues[syncrun_qid_WAIT]) ;
   for (unsigned i = 0; i < 101; ++i) {
      TEST(0 == remove_syncwlist(&srun.wakeup_list, &srun.queues[syncrun_qid_WLIST], &event)) ;
   }
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST signalall_syncrun: EINVAL
   TEST(0 == insert_syncwlist(&wlist, &srun.queues[syncrun_qid_INIT], &event2)) ;
   TEST(EINVAL == signalall_syncrun(&srun, &wlist)) ;  // wrong queue
   TEST(0 == remove_syncwlist(&wlist, &srun.queues[syncrun_qid_INIT], &event)) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // unprepare
   TEST(0 == free_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST])) ;
   TEST(0 == free_syncrun(&srun)) ;

   return 0 ;
ONABORT:
   free_syncwlist(&wlist, &srun.queues[syncrun_qid_WLIST]) ;
   free_syncrun(&srun) ;
   return EINVAL ;
}


// == test_run ==

static syncrun_t *   s_test_srun = 0 ;

typedef struct teststateparam_t  teststateparam_t ;

struct teststateparam_t {
   syncevent_t    event ;
   syncwlist_t *  wlist ;
   uint8_t        initcount;
   uint8_t        runcount ;
   uint8_t        abortcount ;
   uint8_t        wakeupcount ;
   uint8_t        doaction ;
   uint8_t        doaction2 ;
   uint8_t        wakeupaction ;
} ;

enum teststate_e {
   teststate_DORETURN,
   teststate_DOEXIT,
   teststate_DOABORT,
   teststate_DOWEVENT,
   teststate_DOWLIST,
   teststate_DOWEXIT1,
   teststate_DOWEXIT2,
   teststate_NROFACTION
} ;

typedef enum teststate_e   teststate_e ;

static int mainteststate_syncthread(syncthread_t * thread, uint32_t signalstate)
{
   assert(s_test_srun) ;
   assert(s_test_srun->running.thread == thread) ;
   teststateparam_t * param = state_syncthread(thread) ;
   param->initcount   = (uint8_t) (param->initcount + (signalstate == syncthread_signal_INIT)) ;
   param->runcount    = (uint8_t) (param->runcount + (signalstate == 0)) ;
   param->wakeupcount = (uint8_t) (param->wakeupcount + (signalstate == syncthread_signal_WAKEUP)) ;
   param->abortcount  = (uint8_t) (param->abortcount + (signalstate == syncthread_signal_ABORT)) ;
   handlesignal_syncthread(signalstate, continuelabel_syncrun(s_test_srun), DOACTION, DOACTION, DOACTION) ;
   assert(0) ;
DOACTION:
   switch ((teststate_e)param->doaction) {
   case teststate_DORETURN:
      break ;
   case teststate_DOEXIT:
      exitthread_syncrun(s_test_srun, 0) ;
      break ;
   case teststate_DOABORT:
      abortthread_syncrun(s_test_srun) ;
      break ;
   case teststate_DOWEVENT:
      waitforevent_syncrun(s_test_srun, &param->event) ;
      param->doaction = param->wakeupaction ;
      goto DOACTION ;
      break ;
   case teststate_DOWLIST:
      waitforlist_syncrun(s_test_srun, param->wlist) ;
      param->doaction = param->wakeupaction ;
      goto DOACTION ;
      break ;
   case teststate_DOWEXIT1:
      param->doaction  = param->doaction2 ;
      param->doaction2 = teststate_DORETURN ;
      if (0 == startthread_syncrun(s_test_srun, &mainteststate_syncthread, param)) {
         waitforexit_syncrun(s_test_srun) ;
         param->doaction = param->wakeupaction ;
         goto DOACTION ;
      }
      break ;
   case teststate_DOWEXIT2:
      param->doaction = teststate_DOWEXIT1 ;
      if (0 == startthread_syncrun(s_test_srun, &mainteststate_syncthread, param)) {
         waitforexit_syncrun(s_test_srun) ;
         exitthread_syncrun(s_test_srun, 0) ;
      }
      break ;
   case teststate_NROFACTION:
      break ;
   }
   return 0 ;
}

/* function: test_run
 * Test runall_syncrun with all possible state transitions.
 * All state transitions of a thread in every queue are tested. */
static int test_run(void)
{
   syncrun_t         srun = syncrun_INIT_FREEABLE ;
   teststateparam_t  params[128] ;
   syncwlist_t       wlist ;

   // prepare
   s_test_srun = &srun ;

   // TEST preparerun_syncrun: srun.waitinfo.wlist == 0
   TEST(0 == init_syncrun(&srun)) ;
   for (unsigned i = 0; i < 10; ++i) {
      srun.running.laststarted = (void*)1 ;
      srun.running.thread = (void*)11 ;
      srun.running.state  = syncrun_state_ABORT ;
      TEST(0 == preparerun_syncrun(&srun, (void*)i)) ;
      TEST(0 == srun.running.laststarted) ;
      TEST(i == (uintptr_t)srun.running.thread) ;
      TEST(syncrun_state_CONTINUE == srun.running.state) ;
      srun.running.thread = 0 ;
      TEST(isfree_syncrun(&srun)) ;
   }

   // TEST preparerun_syncrun: srun.waitinfo.wlist != 0
   for (unsigned i = 0; i < 10; ++i) {
      init_syncwlist(&wlist) ;
      setstatewaitlist_syncrun(&srun, &wlist, 0) ;
      TEST(1 == len_syncwlist(&wlist)) ;
      TEST(syncrun_state_WAIT == srun.running.state) ;
      TEST(&wlist == srun.waitinfo.wlist) ;
      TEST(0 != srun.waitinfo.event) ;
      srun.waitinfo.event      = 0 ;
      srun.running.laststarted = (void*)1 ;
      srun.running.thread      = (void*)11 ;
      TEST(0 == preparerun_syncrun(&srun, (void*)i)) ;
      TEST(0 == len_syncwlist(&wlist)) ;
      TEST(0 == srun.waitinfo.wlist) ;
      TEST(0 == srun.running.laststarted) ;
      TEST(i == (uintptr_t)srun.running.thread) ;
      TEST(syncrun_state_CONTINUE == srun.running.state) ;
      srun.running.thread = 0 ;
      TEST(isfree_syncrun(&srun)) ;
   }

   // TEST preparerun_syncrun: ETIME
   init_syncwlist(&wlist) ;
   setstatewaitlist_syncrun(&srun, &wlist, 0) ;
   init_testerrortimer(&s_syncrun_errtimer, 1, ETIME) ;
   TEST(0 != srun.waitinfo.wlist) ;
   TEST(ETIME == preparerun_syncrun(&srun, (void*)1)) ;
   // srun.waitinfo.wlist is cleared even in if error
   TEST(0 == srun.waitinfo.wlist) ;
   TEST(syncrun_state_WAIT == srun.running.state) ;
   srun.waitinfo.event = 0 ;
   srun.running.state  = 0 ;
   TEST(isfree_syncrun(&srun)) ;

   // TEST preparewakeup_syncrun
   for (intptr_t i = 0; i < 10; ++i) {
      preparewakeup_syncrun(&srun, (void*)i, (int)(2*i)) ;
      TEST(i   == (intptr_t)srun.wakeup.continuelabel) ;
      TEST(2*i == srun.wakeup.retcode) ;
      srun.wakeup.continuelabel = 0 ;
      srun.wakeup.retcode       = 0 ;
      TEST(isfree_syncrun(&srun)) ;
   }

   // TEST runall_syncrun: empty queue
   TEST(0 == runall_syncrun(&srun)) ;
   TEST(1 == isfree_syncrun(&srun)) ;

   // TEST runall_syncrun: EINPROGRESS
   srun.running.thread = (void*)1 ;
   TEST(EINPROGRESS == runall_syncrun(&srun)) ;
   srun.running.thread = 0 ;
   TEST(1 == isfree_syncrun(&srun)) ;
   TEST(0 == free_syncrun(&srun)) ;

   // TEST runall_syncrun: execinitqueue_syncrun
   for (uint8_t act = 0; act < teststate_NROFACTION; act++) {
      TEST(0 == init_syncrun(&srun)) ;
      init_syncwlist(&wlist) ;
      memset(&params, 0, sizeof(params)) ;
      for (unsigned i = 0; i < lengthof(params); ++i) {
         params[i].doaction  = act ;
         params[i].wlist     = &wlist ;
         TEST(0 == startthread_syncrun(&srun, &mainteststate_syncthread, &params[i])) ;
      }
      TEST(0 == runall_syncrun(&srun)) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_INIT])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
      if (act == teststate_DOWLIST) {
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
      } else {
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
      }
      switch ((enum teststate_e)act) {
      case teststate_DORETURN:
      case teststate_DOEXIT:
      case teststate_DOABORT:
         if (act == teststate_DORETURN) {
            TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         } else {
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         }
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_DOWEVENT:
      case teststate_DOWLIST:
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_DOWEXIT1:
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_DOWEXIT2:
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_NROFACTION:
         break ;
      }
      for (unsigned i = 0; i < lengthof(params); ++i) {
         TEST(params[i].initcount   == 1u + (act == teststate_DOWEXIT1) + 2u*(act == teststate_DOWEXIT2)) ;
         TEST(params[i].runcount    == 0) ;
         TEST(params[i].abortcount  == (act == teststate_DOABORT)) ;
         TEST(params[i].wakeupcount == 0) ;
      }
      TEST(0 == free_syncrun(&srun)) ;
   }

   // TEST runall_syncrun: execrunqueue_syncrun
   for (uint8_t act = 0; act < teststate_NROFACTION; act++) {
      if (act == teststate_DOWEXIT2) continue ;
      TEST(0 == init_syncrun(&srun)) ;
      init_syncwlist(&wlist) ;
      // exec initqueue
      memset(&params, 0, sizeof(params)) ;
      for (unsigned i = 0; i < lengthof(params); ++i) {
         params[i].doaction   = teststate_DORETURN ;
         params[i].wlist      = &wlist ;
         TEST(0 == startthread_syncrun(&srun, &mainteststate_syncthread, &params[i])) ;
      }
      TEST(0 == runall_syncrun(&srun)) ;
      for (unsigned i = 0; i < syncrun_qid_NROFQUEUES; ++i) {
         unsigned l = 0 ;
         if (i == syncrun_qid_RUN) {
            l = lengthof(params) ;
         }
         TEST(l == len_syncqueue(&srun.queues[i])) ;
      }
      // exec runqueue
      for (unsigned i = 0; i < lengthof(params); ++i) {
         params[i].doaction = act ;
      }
      TEST(0 == runall_syncrun(&srun)) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_INIT])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
      if (act == teststate_DOWLIST) {
         TEST(len_syncqueue(&srun.queues[syncrun_qid_WLIST]) == lengthof(params)) ;
      } else {
         TEST(len_syncqueue(&srun.queues[syncrun_qid_WLIST]) == 0) ;
      }
      switch ((enum teststate_e)act) {
      case teststate_DORETURN:
      case teststate_DOEXIT:
      case teststate_DOABORT:
         if (act == teststate_DORETURN) {
            TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         } else {
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         }
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_DOWEVENT:
      case teststate_DOWLIST:
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_DOWEXIT1:
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
         break ;
      case teststate_DOWEXIT2:
      case teststate_NROFACTION:
         break ;
      }
      for (unsigned i = 0; i < lengthof(params); ++i) {
         TEST(params[i].initcount   == 1u + (act == teststate_DOWEXIT1)) ;
         TEST(params[i].runcount    == 1) ;
         TEST(params[i].abortcount  == (act == teststate_DOABORT)) ;
         TEST(params[i].wakeupcount == 0) ;
      }
      TEST(0 == free_syncrun(&srun)) ;
   }

   // TEST runall_syncrun: execrun2queue_syncrun
   for (uint8_t act = 0; act < teststate_NROFACTION; act++) {
      if (act == teststate_DOWEXIT2) continue ;
      TEST(0 == init_syncrun(&srun)) ;
      init_syncwlist(&wlist) ;
      // exec initqueue
      memset(&params, 0, sizeof(params)) ;
      for (unsigned i = 0; i < lengthof(params); ++i) {
         params[i].doaction     = teststate_DOWEXIT1 ;
         params[i].wakeupaction = teststate_DOEXIT ;
         params[i].wlist        = &wlist ;
         TEST(0 == startthread_syncrun(&srun, &mainteststate_syncthread, &params[i])) ;
      }
      TEST(0 == runall_syncrun(&srun)) ;
      for (unsigned i = 0; i < syncrun_qid_NROFQUEUES; ++i) {
         unsigned l = 0 ;
         if (  i == syncrun_qid_RUN2
               || i == syncrun_qid_WAIT) {
            l = lengthof(params) ;
         }
         TEST(l == len_syncqueue(&srun.queues[i])) ;
      }
      // exec runqueue
      for (unsigned i = 0; i < lengthof(params); ++i) {
         params[i].doaction = act ;
      }
      TEST(0 == runall_syncrun(&srun)) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_INIT])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
      if (act == teststate_DOWLIST) {
         TEST(len_syncqueue(&srun.queues[syncrun_qid_WLIST]) == lengthof(params)) ;
      } else {
         TEST(len_syncqueue(&srun.queues[syncrun_qid_WLIST]) == 0) ;
      }
      switch ((enum teststate_e)act) {
      case teststate_DORETURN:
      case teststate_DOEXIT:
      case teststate_DOABORT:
         if (act == teststate_DORETURN) {
            TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         } else {
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         }
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_DOWEVENT:
      case teststate_DOWLIST:
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_DOWEXIT1:
         TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
         TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
         break ;
      case teststate_DOWEXIT2:
      case teststate_NROFACTION:
         break ;
      }
      for (unsigned i = 0; i < lengthof(params); ++i) {
         TEST(params[i].initcount   == 2u + (act == teststate_DOWEXIT1)) ;
         TEST(params[i].runcount    == 1) ;
         TEST(params[i].abortcount  == (act == teststate_DOABORT)) ;
         TEST(params[i].wakeupcount == (act == teststate_DOEXIT || act == teststate_DOABORT)) ;
      }
      TEST(0 == free_syncrun(&srun)) ;
   }

   // TEST runall_syncrun: waitqueue
   static_assert(teststate_DOWEVENT + 1 == teststate_DOWLIST && teststate_DOWLIST + 1 == teststate_DOWEXIT1, "for loop iterates over 3 actions") ;
   for (uint8_t act = teststate_DOWEVENT; act <= teststate_DOWEXIT1; ++act) {
      for (uint8_t wakeupact = 0; wakeupact < teststate_NROFACTION; ++wakeupact) {
         if (wakeupact == teststate_DOWEXIT1 || wakeupact == teststate_DOWEXIT2) continue ;
         for (unsigned qi = 0; qi < 2; ++qi) {
            TEST(0 == init_syncrun(&srun)) ;
            init_syncwlist(&wlist) ;
            memset(&params, 0, sizeof(params)) ;

            switch (qi) {
            case 0:
               // initqueue -> waitqueue
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  params[i].doaction     = act ;
                  params[i].wakeupaction = wakeupact ;
                  params[i].wlist        = &wlist ;
                  TEST(0 == startthread_syncrun(&srun, &mainteststate_syncthread, &params[i])) ;
               }
               TEST(0 == runall_syncrun(&srun)) ;
               break ;
            case 1:
               // runqueue -> waitqueue
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  params[i].doaction     = teststate_DORETURN ;
                  params[i].wakeupaction = wakeupact ;
                  params[i].wlist        = &wlist ;
                  TEST(0 == startthread_syncrun(&srun, &mainteststate_syncthread, &params[i])) ;
               }
               TEST(0 == runall_syncrun(&srun)) ;
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  params[i].doaction = act ;
               }
               TEST(0 == runall_syncrun(&srun)) ;
               break ;
            }
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_INIT])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
            if (act == teststate_DOWEXIT1) {
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            } else {
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            }
            TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
            switch ((enum teststate_e)act) {
            case teststate_DOWEVENT:
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncwlist(&wlist)) ;
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  TEST(0   == signalevent_syncrun(&srun, &params[i].event)) ;
                  TEST(i+1 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
                  TEST(0   == lenrunqueue_syncrun(&srun)) ;
               }
               break ;
            case teststate_DOWLIST:
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(lengthof(params) == len_syncwlist(&wlist)) ;
               TEST(0 == signalall_syncrun(&srun, &wlist)) ;
               TEST(0 == lenrunqueue_syncrun(&srun)) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               break ;
            case teststate_DOWEXIT1:
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncwlist(&wlist)) ;
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  params[i].doaction = teststate_DOEXIT ;
               }
               break ;
            default:
               assert(0) ;
            }
            // exec wakeupact after wakeup
            TEST(0 == runall_syncrun(&srun)) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
            switch ((enum teststate_e)wakeupact) {
            case teststate_DORETURN:
               TEST(0 == len_syncwlist(&wlist)) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
               break ;
            case teststate_DOEXIT:
            case teststate_DOABORT:
               TEST(0 == len_syncwlist(&wlist)) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
               break ;
            case teststate_DOWEVENT:
               TEST(0 == len_syncwlist(&wlist)) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
               break ;
            case teststate_DOWLIST:
               TEST(lengthof(params) == len_syncwlist(&wlist)) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
               break ;
            case teststate_DOWEXIT1:
            case teststate_DOWEXIT2:
            case teststate_NROFACTION:
               break ;
            }
            for (unsigned i = 0; i < lengthof(params); ++i) {
               TEST(params[i].initcount   == 1u + (act == teststate_DOWEXIT1))
               TEST(params[i].runcount    == qi + (act == teststate_DOWEXIT1)) ;
               TEST(params[i].abortcount  == (wakeupact == teststate_DOABORT)) ;
               TEST(params[i].wakeupcount == 1) ;
            }
            TEST(0 == free_syncrun(&srun)) ;
         }
      }
   }

   // TEST runall_syncrun: wait2queue
   static_assert(teststate_DOWEVENT + 1 == teststate_DOWLIST && teststate_DOWLIST + 1 == teststate_DOWEXIT1, "for loop iterates over 3 actions") ;
   for (uint8_t act = teststate_DOWEVENT; act <= teststate_DOWEXIT1; ++act) {
      for (uint8_t wakeupact = 0; wakeupact < teststate_NROFACTION; ++wakeupact) {
         if (wakeupact == teststate_DOWEXIT1 || wakeupact == teststate_DOWEXIT2) continue ;
         for (unsigned qi = 0; qi < 2; ++qi) {
            TEST(0 == init_syncrun(&srun)) ;
            init_syncwlist(&wlist) ;
            memset(&params, 0, sizeof(params)) ;

            switch (qi) {
            case 0:
               // initqueue -> wait2queue
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  params[i].doaction     = teststate_DOWEXIT1 ;
                  params[i].doaction2    = act ;
                  params[i].wakeupaction = wakeupact ;
                  params[i].wlist        = &wlist ;
                  TEST(0 == startthread_syncrun(&srun, &mainteststate_syncthread, &params[i])) ;
               }
               TEST(0 == runall_syncrun(&srun)) ;
               break ;
            case 1:
               // runqueue -> wait2queue
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  params[i].doaction     = teststate_DOWEXIT1 ;
                  params[i].wakeupaction = wakeupact ;
                  params[i].wlist        = &wlist ;
                  TEST(0 == startthread_syncrun(&srun, &mainteststate_syncthread, &params[i])) ;
               }
               TEST(0 == runall_syncrun(&srun)) ;
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  params[i].doaction = act ;
               }
               TEST(0 == runall_syncrun(&srun)) ;
               break ;
            }
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
            if (act == teststate_DOWEXIT1) {
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            } else {
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            }
            TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
            TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
            switch ((enum teststate_e)act) {
            case teststate_DOWEVENT:
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncwlist(&wlist)) ;
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  TEST(0   == signalevent_syncrun(&srun, &params[i].event)) ;
                  TEST(i+1 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
                  TEST(0   == lenrunqueue_syncrun(&srun)) ;
               }
               break ;
            case teststate_DOWLIST:
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(lengthof(params) == len_syncwlist(&wlist)) ;
               TEST(0 == signalall_syncrun(&srun, &wlist)) ;
               TEST(0 == lenrunqueue_syncrun(&srun)) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               break ;
            case teststate_DOWEXIT1:
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncwlist(&wlist)) ;
               for (unsigned i = 0; i < lengthof(params); ++i) {
                  params[i].doaction = teststate_DOEXIT ;
               }
               TEST(lengthof(params) == lenrunqueue_syncrun(&srun)) ;
               break ;
            default:
               assert(0) ;
            }
            // exec wakeupact after wakeup
            TEST(0 == runall_syncrun(&srun)) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
            switch ((enum teststate_e)wakeupact) {
            case teststate_DORETURN:
               TEST(0 == len_syncwlist(&wlist)) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
               break ;
            case teststate_DOEXIT:
            case teststate_DOABORT:
               TEST(0 == len_syncwlist(&wlist)) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
               break ;
            case teststate_DOWEVENT:
               TEST(0 == len_syncwlist(&wlist)) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
               break ;
            case teststate_DOWLIST:
               TEST(lengthof(params) == len_syncwlist(&wlist)) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
               TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
               TEST(lengthof(params) == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
               break ;
            case teststate_DOWEXIT1:
            case teststate_DOWEXIT2:
            case teststate_NROFACTION:
               break ;
            }
            for (unsigned i = 0; i < lengthof(params); ++i) {
               TEST(params[i].initcount   == 2u + (act == teststate_DOWEXIT1))
               TEST(params[i].runcount    == qi + (act == teststate_DOWEXIT1)) ;
               TEST(params[i].abortcount  == 2u*(wakeupact == teststate_DOABORT)) ;
               TEST(params[i].wakeupcount == 1u+(wakeupact == teststate_DOABORT || wakeupact == teststate_DOEXIT)) ;
            }
            TEST(0 == free_syncrun(&srun)) ;
         }
      }
   }

   // TEST runall_syncrun: initqueue wakeup from exit
   static_assert(teststate_DOEXIT + 1 == teststate_DOABORT, "for loop iterates over 2 actions") ;
   for (uint8_t act = teststate_DOEXIT; act <= teststate_DOABORT; ++act) {
      TEST(0 == init_syncrun(&srun)) ;
      memset(&params, 0, sizeof(params)) ;

      // initqueue -> wait2queue -> initaction exit/abort
      for (unsigned i = 0; i < lengthof(params); ++i) {
         params[i].doaction     = teststate_DOWEXIT2 ;
         params[i].doaction2    = act ;
         params[i].wakeupaction = teststate_DOEXIT ;
         TEST(0 == startthread_syncrun(&srun, &mainteststate_syncthread, &params[i])) ;
      }
      TEST(0 == runall_syncrun(&srun)) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_INIT])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
      TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
      for (unsigned i = 0; i < lengthof(params); ++i) {
         TEST(params[i].initcount   == 3)
         TEST(params[i].runcount    == 0) ;
         TEST(params[i].abortcount  == (act == teststate_DOABORT)) ;
         TEST(params[i].wakeupcount == 2) ;
      }
      TEST(0 == free_syncrun(&srun)) ;
   }

   // unprepare
   s_test_srun = 0 ;
   TEST(0 == free_syncrun(&srun)) ;

   return 0 ;
ONABORT:
   s_test_srun = 0 ;
   free_syncrun(&srun) ;
   return EINVAL ;
}

// == test_run2 ==

static unsigned   s_test_nrthreads = 0 ;
static unsigned   s_test_initcount = 0 ;
static unsigned   s_test_execcount = 0 ;

typedef struct startparam_t      startparam_t ;

typedef struct testrun2state_t   testrun2state_t ;

struct startparam_t {
   // check that data is correct transfered
   uint8_t           testdata[128] ;
   bool              isstart2nd ;
   testrun2state_t * state2nd ;
   testrun2state_t * state ;
} ;

struct testrun2state_t {
   syncevent_t    event ;
   syncwlist_t *  wlist ;
   uint8_t        action ;
   uint8_t        errtimercount ;
   uint8_t        initcount ;
   uint8_t        runcount ;
   uint8_t        abortcount ;
   uint8_t        wakeupcount ;
   uint8_t        afterwaitforexitcount ;
   uint8_t        afterwaitforeventcount ;
   uint8_t        afterwaitforlistcount ;
} ;

enum testrun2action_e {
   testrun2action_CONTINUE,
   testrun2action_WAITEVENT,
   testrun2action_WAITLIST,
   testrun2action_EXIT,
   testrun2action_ABORT
} ;

static int maintestrun2_syncthread(syncthread_t * thread, uint32_t signalstate)
{
   assert(s_test_srun) ;
   assert(s_test_srun->running.thread == thread) ;

   ++ s_test_execcount ;

   testrun2state_t * state      = state_syncthread(thread) ;
   startparam_t *    startparam = state_syncthread(thread) ;
   if (signalstate == syncthread_signal_WAKEUP) {
      ++ state->wakeupcount ;
   }
   handlesignal_syncthread(signalstate, continuelabel_syncrun(s_test_srun), ONINIT, ONRUN, ONABORT) ;

ONINIT:
   ++ s_test_initcount ;
   for (unsigned i = 0; i < lengthof(startparam->testdata); ++i) {
      if (startparam->testdata[i] != (uint8_t)(i + s_test_initcount -1)) {
         exitthread_syncrun(s_test_srun, 1) ;
      }
   }
   setstate_syncthread(thread, startparam->state) ;
   state = state_syncthread(thread) ;
   ++ state->initcount ;
   if (startparam->isstart2nd) {
      startparam_t * startparam2 ;
      if (0 != startthread2_syncrun(s_test_srun, &maintestrun2_syncthread, sizeof(startparam_t), (void**)&startparam2)) {
         exitthread_syncrun(s_test_srun, 1) ;
      }
      for (unsigned i = 0; i < lengthof(startparam2->testdata); ++i) {
         startparam2->testdata[i] = (uint8_t)(i + s_test_initcount -1 + 2*s_test_nrthreads) ;
      }
      startparam2->isstart2nd = false ;
      startparam2->state2nd   = 0 ;
      startparam2->state      = startparam->state2nd ;
      waitforexit_syncrun(s_test_srun) ;
      ++ state->afterwaitforexitcount ;
      if (state->errtimercount) {
         abortthread_syncrun(s_test_srun) ;
      }
   }
   goto DOACTION ;

ONRUN:
   ++ state->runcount ;
   goto DOACTION ;

ONABORT:
   ++ state->abortcount ;
   return 0 ;

DOACTION:
   if (state->errtimercount) {
      init_testerrortimer(&s_syncrun_errtimer, state->errtimercount, ENOMEM) ;
   }
   switch ((enum testrun2action_e)state->action) {
   case testrun2action_CONTINUE:
      // ... continue is default ...
      break ;
   case testrun2action_WAITEVENT:
      waitforevent_syncrun(s_test_srun, &state->event) ;
      ++ state->afterwaitforeventcount ;
      goto DOACTION ;
   case testrun2action_WAITLIST:
      waitforlist_syncrun(s_test_srun, state->wlist) ;
      ++ state->afterwaitforlistcount ;
      goto DOACTION ;
   case testrun2action_EXIT:
      exitthread_syncrun(s_test_srun, 0) ;
      break ;
   case testrun2action_ABORT:
      abortthread_syncrun(s_test_srun) ;
      break ;
   }
   return 0 ;
}

static int startthreads(syncrun_t * srun, testrun2state_t (*state) [3][100], uint8_t nrofthreads)
{
   s_test_nrthreads = nrofthreads ;
   s_test_initcount = 0 ;

   TEST(nrofthreads <= lengthof((*state)[0])) ;

   for (unsigned i = 0; i < nrofthreads; ++i) {
      startparam_t * startparam ;
      TEST(0 == startthread2_syncrun(srun, &maintestrun2_syncthread, sizeof(startparam_t), (void**)&startparam)) ;
      for (unsigned i2 = 0; i2 < lengthof(startparam->testdata); ++i2) {
         startparam->testdata[i2] = (uint8_t)(i + i2) ;
      }
      startparam->isstart2nd = true ;
      startparam->state2nd   = &(*state)[2][i] ;
      startparam->state      = &(*state)[0][i] ;
   }

   for (unsigned i = 0; i < nrofthreads; ++i) {
      startparam_t * startparam ;
      TEST(0 == startthread2_syncrun(srun, &maintestrun2_syncthread, sizeof(startparam_t), (void**)&startparam)) ;
      for (unsigned i2 = 0; i2 < lengthof(startparam->testdata); ++i2) {
         startparam->testdata[i2] = (uint8_t)(i + i2 + nrofthreads) ;
      }
      startparam->isstart2nd = false ;
      startparam->state2nd   = 0 ;
      startparam->state      = &(*state)[1][i] ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

void setstateaction(testrun2state_t (*state) [3][100], syncwlist_t * wlist, enum testrun2action_e action, uint8_t errtimercount)
{
   for (unsigned y = 0; y < lengthof(*state); ++y) {
      for (unsigned i = 0; i < lengthof((*state)[0]); ++i) {
         (*state)[y][i].wlist      = wlist ;
         (*state)[y][i].action     = action ;
         (*state)[y][i].errtimercount = errtimercount ;
         (*state)[y][i].initcount  = 0 ;
         (*state)[y][i].runcount   = 0 ;
         (*state)[y][i].abortcount = 0 ;
         (*state)[y][i].wakeupcount= 0 ;
         (*state)[y][i].afterwaitforexitcount  = 0 ;
         (*state)[y][i].afterwaitforeventcount = 0 ;
         (*state)[y][i].afterwaitforlistcount  = 0;
      }
   }
}

/* function: test_run2
 * Test correct variable values of a single thread in every posible state. */
static int test_run2(void)
{
   syncrun_t         srun = syncrun_INIT_FREEABLE ;
   syncwlist_t       wlist ;
   testrun2state_t   state[3][100] ;
   // state[0][...] are waiters which create a second thread and wait for their exit
   // state[1][...] are threads to test runqueue and waitqueue
   // state[2][...] are started threads from state[0] to test run2queue and wait2queue

   // prepare
   s_test_srun = &srun ;

   // TEST startthreads: works as expected
   for (unsigned ti = 0; ti < 4; ++ti) {
      init_syncwlist(&wlist) ;
      TEST(0 == init_syncrun(&srun)) ;
      s_test_execcount = 0 ;
      memset(state, 0, sizeof(state)) ;
      TEST(0 == startthreads(&srun, &state, lengthof(state[0]))) ;
      TEST(0 == runall_syncrun(&srun)) ;
      TEST(3*lengthof(state[0]) == s_test_execcount) ;
      TEST(2*lengthof(state[0]) == lenrunqueue_syncrun(&srun)) ;
      TEST(1*lengthof(state[0]) == lenwaitqueue_syncrun(&srun)) ;
      for (unsigned i = 0; i < lengthof(state[0]); ++i) {
         for (unsigned y = 0; y < lengthof(state); ++y) {
            TEST(1 == state[y][i].initcount) ;
            TEST(0 == state[y][i].runcount) ;
            TEST(0 == state[y][i].abortcount) ;
            TEST(0 == state[y][i].wakeupcount) ;
            TEST(0 == state[y][i].afterwaitforexitcount) ;
            TEST(0 == state[y][i].afterwaitforeventcount) ;
            TEST(0 == state[y][i].afterwaitforlistcount) ;
         }
      }
      TEST(0 == free_syncrun(&srun)) ;
   }

   // TEST runall_syncrun: all possible state transitions
   uint8_t  actionpath[5] ;
   unsigned len_actionpath = lengthof(actionpath) ;
   for (unsigned i = 0; i < len_actionpath; ++i) {
      actionpath[i] = testrun2action_CONTINUE ;
   }
   while (len_actionpath) {
      // init
      init_syncwlist(&wlist) ;
      memset(state, 0, sizeof(state)) ;
      TEST(0 == init_syncrun(&srun)) ;
      // startthreads -> 100 waitqueue (wait for exit), 100 init2queue , 100initqueue
      TEST(0 == startthreads(&srun, &state, lengthof(state[0]))) ;
      bool isbeforerun   = false ;
      bool isbeforeevent = false ;
      bool isbeforelist  = false ;

      // execute complete action path
      // init -> run,wait,exit,abort , run -> run,wait,exit,abort , wait -> run,wait,exit,abort
      for (unsigned pi = 0; pi < len_actionpath; ++pi) {
         s_test_execcount = 0 ;
         setstateaction(&state, &wlist, actionpath[pi], 0) ;
         // wakeup if necessary
         if (isbeforeevent) {
            for (unsigned y = 1; y < lengthof(state); ++y) {
               for (unsigned i = 0; i < lengthof(state[0]); ++i) {
                  TEST(1 == iswaiting_syncevent(&state[y][i].event)) ;
                  TEST(0 == signalevent_syncrun(&srun, &state[y][i].event)) ;
                  TEST(0 == iswaiting_syncevent(&state[y][i].event)) ;
               }
            }
            TEST(2*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
         } else if (isbeforelist) {
            TEST(0 == signalall_syncrun(&srun, &wlist)) ;
            TEST(2*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
         }
         TEST(0 == runall_syncrun(&srun)) ;
         if (pi == 0) {
            // isbeforeinit
            s_test_execcount -= lengthof(state[0]) ;
            for (unsigned y = 0; y < lengthof(state); ++y) {
               for (unsigned i = 0; i < lengthof(state[0]); ++i) {
                  TEST(1 == state[y][i].initcount) ;
                  state[y][i].initcount = 0 ;
               }
            }
         }
         unsigned a = (actionpath[pi] == testrun2action_ABORT) ;
         unsigned e = a + (actionpath[pi] == testrun2action_EXIT) ;
         unsigned w = (isbeforeevent || isbeforelist) ;
         for (unsigned i = 0; i < lengthof(state[0]); ++i) {
            TEST(0 == state[0][i].initcount) ;
            TEST(0 == state[0][i].runcount) ;
            TEST(a == state[0][i].abortcount) ;
            TEST(e == state[0][i].wakeupcount) ;
            TEST(e == state[0][i].afterwaitforexitcount) ;
            TEST(0 == state[0][i].afterwaitforeventcount) ;
            TEST(0 == state[0][i].afterwaitforlistcount) ;
         }
         for (unsigned y = 1; y < lengthof(state); ++y) {
            for (unsigned i = 0; i < lengthof(state[0]); ++i) {
               TEST(0 == state[y][i].initcount) ;
               TEST(isbeforerun == state[y][i].runcount) ;
               TEST(a == state[y][i].abortcount) ;
               TEST(w == state[y][i].wakeupcount) ;
               TEST(0 == state[y][i].afterwaitforexitcount) ;
               TEST(isbeforeevent == state[y][i].afterwaitforeventcount) ;
               TEST(isbeforelist  == state[y][i].afterwaitforlistcount) ;
            }
         }

         isbeforerun   = false ;
         isbeforeevent = false ;
         isbeforelist  = false ;
         switch ((enum testrun2action_e)actionpath[pi]) {
         case testrun2action_CONTINUE:
            isbeforerun = true ;
            TEST(2*lengthof(state[0]) == s_test_execcount) ;
            TEST(1*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
            TEST(1*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            TEST(1*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
            TEST(0 == len_syncwlist(&wlist)) ;
            break ;
         case testrun2action_WAITEVENT:
            isbeforeevent = true ;
            TEST(2*lengthof(state[0]) == s_test_execcount) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            TEST(2*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
            TEST(1*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
            TEST(0 == len_syncwlist(&wlist)) ;
            break ;
         case testrun2action_WAITLIST:
            isbeforelist = true ;
            TEST(2*lengthof(state[0]) == s_test_execcount) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            TEST(2*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
            TEST(1*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(2*lengthof(state[0]) == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
            TEST(2*lengthof(state[0]) == len_syncwlist(&wlist)) ;
            break ;
         case testrun2action_EXIT:
            TEST(3*lengthof(state[0]) == s_test_execcount) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
            TEST(0 == len_syncwlist(&wlist)) ;
            break ;
         case testrun2action_ABORT:
            TEST(6*lengthof(state[0]) == s_test_execcount) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
            TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
            TEST(0 == len_syncwlist(&wlist)) ;
            break ;
         }
      }

      // next path
      do {
         ++ actionpath[len_actionpath-1] ;
         if (actionpath[len_actionpath-1] <= testrun2action_ABORT) break ;
         -- len_actionpath ;
      } while (len_actionpath) ;
      if (  len_actionpath
            && actionpath[len_actionpath-1] != testrun2action_EXIT
            && actionpath[len_actionpath-1] != testrun2action_ABORT) {
         while (len_actionpath < lengthof(actionpath)) {
            actionpath[len_actionpath] = 0 ;
            ++ len_actionpath ;
         }
      }

      // free
      TEST(0 == free_syncrun(&srun)) ;
   }

   // TEST runall_syncrun: ENOMEM aborts syncthread_t
   uint8_t testpath[][5] = {
      { 2, testrun2action_WAITEVENT, testrun2action_CONTINUE },    // waitqueue -> runqueue (ENOMEM)
      { 2, testrun2action_WAITLIST, testrun2action_WAITLIST },    // waitueue -> wlistqueue (ENOMEM)
      { 2, testrun2action_CONTINUE, testrun2action_WAITEVENT },   // runqueue -> waitqueue (ENOMEM)
      { 2, testrun2action_CONTINUE, testrun2action_WAITLIST },    // runqueue -> wlistqueue (ENOMEM)
      { 1, testrun2action_CONTINUE },  // initqueue -> runqueue (ENOMEM)
      { 1, testrun2action_WAITEVENT }, // initqueue -> waitqueue (ENOMEM)
      { 1, testrun2action_WAITLIST },  // initqueue -> wlistqueue (ENOMEM)
   } ;
   for (unsigned ti = 0; ti < lengthof(testpath); ++ti) {
      len_actionpath    = testpath[ti][0] ;
      for (unsigned i = 0; i < len_actionpath; ++i) {
         actionpath[i] = testpath[ti][i+1] ;
      }
      // init
      init_syncwlist(&wlist) ;
      memset(state, 0, sizeof(state)) ;
      TEST(0 == init_syncrun(&srun)) ;
      TEST(0 == startthreads(&srun, &state, 4)) ;
      bool isbeforerun   = false ;
      bool isbeforeevent = false ;
      bool isbeforelist  = false ;

      // execute complete action path
      // init -> run,wait,exit,abort , run -> run,wait,exit,abort , wait -> run,wait,exit,abort
      for (unsigned pi = 0; pi < len_actionpath; ++pi) {
         s_test_execcount = 0 ;
         setstateaction(&state, &wlist, actionpath[pi], (pi + 1 == len_actionpath)) ;

         // wakeup if necessary
         if (isbeforeevent) {
            for (unsigned y = 1; y < lengthof(state); ++y) {
               for (unsigned i = 0; i < 4; ++i) {
                  TEST(0 == signalevent_syncrun(&srun, &state[y][i].event)) ;
               }
            }
            TEST(2*4 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
         } else if (isbeforelist) {
            TEST(0 == signalall_syncrun(&srun, &wlist)) ;
            TEST(2*4 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
         }
         TEST(0 == runall_syncrun(&srun)) ;
         if (pi + 1 == len_actionpath) {
            TEST(((pi == 0)+6)*4 == s_test_execcount) ;
            unsigned w = (isbeforeevent || isbeforelist) ;
            for (unsigned i = 0; i < 4; ++i) {
               TEST((pi == 0) == state[0][i].initcount) ;
               TEST(0 == state[0][i].runcount) ;
               TEST(1 == state[0][i].abortcount) ;
               TEST(1 == state[0][i].wakeupcount) ;
               TEST(1 == state[0][i].afterwaitforexitcount) ;
               TEST(0 == state[0][i].afterwaitforeventcount) ;
               TEST(0 == state[0][i].afterwaitforlistcount) ;
            }
            for (unsigned y = 1; y < lengthof(state); ++y) {
               for (unsigned i = 0; i < 4; ++i) {
                  TEST((pi == 0) == state[y][i].initcount) ;
                  TEST(isbeforerun == state[y][i].runcount) ;
                  TEST(1 == state[y][i].abortcount) ;
                  TEST(w == state[y][i].wakeupcount) ;
                  TEST(0 == state[y][i].afterwaitforexitcount) ;
                  TEST(isbeforeevent == state[y][i].afterwaitforeventcount) ;
                  TEST(isbeforelist  == state[y][i].afterwaitforlistcount) ;
               }
            }
            TEST(0 == lenrunqueue_syncrun(&srun)) ;
            TEST(0 == lenwaitqueue_syncrun(&srun)) ;
            memset(&srun.waitinfo, 0, sizeof(srun.waitinfo)) ;
            memset(&srun.wakeup, 0, sizeof(srun.wakeup)) ;
            TEST(1 == isfree_syncrun(&srun)) ;
         } else {
            isbeforerun   = false ;
            isbeforeevent = false ;
            isbeforelist  = false ;
            switch ((enum testrun2action_e)actionpath[pi]) {
            case testrun2action_CONTINUE:
               isbeforerun = true ;
               break ;
            case testrun2action_WAITEVENT:
               isbeforeevent = true ;
               break ;
            case testrun2action_WAITLIST:
               isbeforelist = true ;
               break ;
            case testrun2action_EXIT:
            case testrun2action_ABORT:
               TEST(false) ;
            }
         }
      }

      // free
      TEST(0 == free_syncrun(&srun)) ;
   }

   // unprepare
   s_test_srun   = 0 ;
   s_test_initcount = 0 ;
   s_test_execcount = 0 ;
   TEST(0 == free_syncrun(&srun)) ;

   return 0 ;
ONABORT:
   s_test_srun   = 0 ;
   s_test_initcount = 0 ;
   s_test_execcount = 0 ;
   free_syncrun(&srun) ;
   return EINVAL ;
}

static int mainwaitchain_syncrun(syncthread_t * thread, uint32_t signalstate)
{
   assert(s_test_srun) ;
   assert(s_test_srun->running.thread == thread) ;
   uintptr_t counter = (uintptr_t) state_syncthread(thread) ;
   handlesignal_syncthread(signalstate, continuelabel_syncrun(s_test_srun), ONINIT, ONRUN, ONABORT) ;
ONINIT:
   if (counter) {
      if (0 == startthread_syncrun(s_test_srun, &mainwaitchain_syncrun, (void*)(counter-1))) {
         int err = waitforexit_syncrun(s_test_srun) ;
         s_test_execcount += (err == (int)counter-1) ;
      }
      exitthread_syncrun(s_test_srun, (int)counter) ;
   }
   return 0 ;
ONRUN:
   exitthread_syncrun(s_test_srun, (int)counter) ;
   return 0 ;
ONABORT:
   return 0 ;
}

static int mainwaitchain2_syncrun(syncthread_t * thread, uint32_t signalstate)
{
   assert(s_test_srun) ;
   assert(s_test_srun->running.thread == thread) ;
   uintptr_t counter = (uintptr_t) state_syncthread(thread) ;
   handlesignal_syncthread(signalstate, continuelabel_syncrun(s_test_srun), ONINIT, ONRUN, ONABORT) ;
ONINIT:
   if (counter) {
      if (0 == startthread_syncrun(s_test_srun, &mainwaitchain2_syncrun, (void*)(counter-1))) {
         int err = waitforexit_syncrun(s_test_srun) ;
         s_test_execcount += (err == syncrun_returncode_ABORT) ;
      }
      abortthread_syncrun(s_test_srun) ;
   }
   return 0 ;
ONRUN:
   abortthread_syncrun(s_test_srun) ;
   return 0 ;
ONABORT:
   return 0 ;
}

static int test_runwaitchain(void)
{
   syncrun_t   srun = syncrun_INIT_FREEABLE ;

   // prepare
   s_test_srun = &srun ;
   TEST(0 == init_syncrun(&srun)) ;

   // TEST runall_syncrun: wait for exit and test return value
   TEST(0 == startthread_syncrun(&srun, &mainwaitchain_syncrun, (void*)(10000))) ;
   TEST(0 == runall_syncrun(&srun)) ;
   TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
   TEST(1 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
   TEST(1 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
   TEST(10000-1 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
   s_test_execcount = 0 ;
   // trigger exit chain
   TEST(0 == runall_syncrun(&srun)) ;
   TEST(10000 == s_test_execcount) ;
   TEST(0 == lenrunqueue_syncrun(&srun)) ;
   TEST(0 == lenwaitqueue_syncrun(&srun)) ;

   // TEST runall_syncrun: wait for abort / syncrun_returncode_ABORT
   TEST(0 == startthread_syncrun(&srun, &mainwaitchain2_syncrun, (void*)(10000))) ;
   TEST(0 == runall_syncrun(&srun)) ;
   TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_RUN])) ;
   TEST(1 == len_syncqueue(&srun.queues[syncrun_qid_RUN2])) ;
   TEST(1 == len_syncqueue(&srun.queues[syncrun_qid_WAIT])) ;
   TEST(10000-1 == len_syncqueue(&srun.queues[syncrun_qid_WAIT2])) ;
   s_test_execcount = 0 ;
   // trigger exit chain
   TEST(0 == runall_syncrun(&srun)) ;
   TEST(10000 == s_test_execcount) ;
   TEST(0 == lenrunqueue_syncrun(&srun)) ;
   TEST(0 == lenwaitqueue_syncrun(&srun)) ;

   // unprepare
   s_test_srun = 0 ;
   s_test_execcount = 0 ;
   TEST(0 == free_syncrun(&srun)) ;

   return 0 ;
ONABORT:
   s_test_srun = 0 ;
   s_test_execcount = 0 ;
   free_syncrun(&srun) ;
   return EINVAL ;
}

// == test_abort ==

typedef struct testabortparam_t   testabortparam_t ;

struct testabortparam_t {
   syncwlist_t *  wlist ;
   syncevent_t    event ;
   bool           isrunner ;
   uint8_t        waitexit ;
   unsigned       abortcount ;
} ;

static int maintestabort_syncthread(syncthread_t * thread, uint32_t signalstate)
{
   assert(s_test_srun) ;
   assert(s_test_srun->running.thread == thread) ;
   testabortparam_t * param = state_syncthread(thread) ;
   handlesignal_syncthread(signalstate, continuelabel_syncrun(s_test_srun), ONINIT, ONRUN, ONABORT) ;
ONINIT:
   if (param->isrunner) {
      // no wait
   } else if (param->waitexit) {
      if (  param->waitexit > 1
            && 0 == startthread_syncrun(s_test_srun, &maintestabort_syncthread, param)) {
         -- param->waitexit ;
         waitforexit_syncrun(s_test_srun) ;
      }
   } else if (param->wlist) {
      waitforlist_syncrun(s_test_srun, param->wlist) ;
   } else {
      param->event = (syncevent_t) syncevent_INIT_FREEABLE ;
      waitforevent_syncrun(s_test_srun, &param->event) ;
   }
   return 0 ;
ONRUN:
   return 0 ;
ONABORT:
   ++ param->abortcount ;
   return 0 ;
}

static int test_abort(void)
{
   syncrun_t         srun = syncrun_INIT_FREEABLE ;
   syncqueue_t       wlistqueue = syncqueue_INIT_FREEABLE ;
   syncwlist_t       wlist ;
   testabortparam_t  eparams[128] ;
   testabortparam_t  xparams[128] ;
   testabortparam_t  lparam ;
   testabortparam_t  rparam ;

   // prepare
   s_test_srun = &srun ;

   // TEST clearevents_syncrun: normal / also called from free_syncrun and abortall_syncrun
   for (unsigned ti = 0; ti <= 2; ++ti) {
      TEST(0 == init_syncrun(&srun)) ;
      init_syncwlist(&wlist) ;
      memset(&eparams, 0, sizeof(eparams)) ;
      memset(&lparam, 0, sizeof(lparam)) ;
      lparam.wlist = &wlist ;
      for (unsigned i = 0; i < lengthof(eparams); ++i) {
         TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &eparams[i])) ;
         TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &lparam)) ;
      }
      TEST(0 == runall_syncrun(&srun)) ;
      TEST(0 == leninitqueue_syncrun(&srun)) ;
      TEST(lengthof(eparams)*2 == lenwaitqueue_syncrun(&srun)) ;
      TEST(lengthof(eparams)   == len_syncwlist(&wlist)) ;
      TEST(lengthof(eparams)   == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
      for (unsigned i = 0; i < lengthof(eparams); ++i) {
         TEST(1 == iswaiting_syncevent(&eparams[i].event)) ;
      }
      for (unsigned i = 0; i == 0; ) {
         foreach (_syncwlist, nextevent, &wlist) {
            TEST(1 == iswaiting_syncevent(nextevent)) ;
            ++ i;
         }
         TEST(i == lengthof(eparams)) ;
      }
      if (ti == 2) {
         // do not free memory for syncrun_qid_WLIST
         wlistqueue = srun.queues[syncrun_qid_WLIST] ;
         init_syncqueue(&srun.queues[syncrun_qid_WLIST]) ;
         TEST(0 == free_syncrun(&srun)) ;
         TEST(0 == init_syncrun(&srun)) ;
      } else if (ti == 1) {
         TEST(0 == abortall_syncrun(&srun)) ;
      } else {
         TEST(0 == clearevents_syncrun(&srun)) ;
      }
      // all events and wlist events are cleared !
      for (unsigned i = 0; i < lengthof(eparams); ++i) {
         TEST(0 == iswaiting_syncevent(&eparams[i].event)) ;
      }
      for (unsigned i = 0; i == 0; ) {
         foreach (_syncwlist, nextevent, &wlist) {
            TEST(0 == iswaiting_syncevent(nextevent)) ;
            ++ i;
         }
         TEST(i == lengthof(eparams)) ;
      }
      TEST(0 == free_syncqueue(&wlistqueue)) ;
      TEST(0 == free_syncrun(&srun)) ;
   }

   // TEST execabort_syncrun
   TEST(0 == init_syncrun(&srun)) ;
   init_syncwlist(&wlist) ;
   memset(&eparams, 0, sizeof(eparams)) ;
   memset(&xparams, 0 , sizeof(xparams)) ;
   memset(&lparam, 0, sizeof(lparam)) ;
   memset(&rparam, 0, sizeof(rparam)) ;
   lparam.wlist    = &wlist ;
   rparam.isrunner = true ;
   for (unsigned i = 0; i < lengthof(eparams); ++i) {
      TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &eparams[i])) ;
      xparams[i].waitexit = 3 ;
      TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &xparams[i])) ;
      TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &lparam)) ;
      TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &rparam)) ;
   }
   TEST(0 == runall_syncrun(&srun)) ;
   TEST(lengthof(eparams)*4 == lenwaitqueue_syncrun(&srun)) ;
   TEST(lengthof(eparams)*2 == lenrunqueue_syncrun(&srun)) ;
   TEST(0 == execabort_syncrun(&srun, syncrun_qid_RUN, sizeof(runqueue_entry_t))) ;
   TEST(lengthof(eparams)*4 == lenwaitqueue_syncrun(&srun)) ;
   TEST(lengthof(eparams)   == lenrunqueue_syncrun(&srun)) ;
   TEST(0 == execabort_syncrun(&srun, syncrun_qid_RUN2, sizeof(run2queue_entry_t))) ;
   TEST(lengthof(eparams)*4 == lenwaitqueue_syncrun(&srun)) ;
   TEST(0 == lenrunqueue_syncrun(&srun)) ;
   TEST(0 == execabort_syncrun(&srun, syncrun_qid_WAIT, sizeof(waitqueue_entry_t))) ;
   TEST(lengthof(eparams)   == lenwaitqueue_syncrun(&srun)) ;
   TEST(0 == lenrunqueue_syncrun(&srun)) ;
   TEST(0 == execabort_syncrun(&srun, syncrun_qid_WAIT2, sizeof(wait2queue_entry_t))) ;
   TEST(0 == lenwaitqueue_syncrun(&srun)) ;
   TEST(0 == lenrunqueue_syncrun(&srun)) ;
   // wlist is not changed
   TEST(lengthof(eparams)   == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
   TEST(lengthof(eparams)   == len_syncwlist(&wlist)) ;
   for (unsigned i = 0; i < lengthof(eparams); ++i) {
      TEST(1 == eparams[i].abortcount) ;
      TEST(3 == xparams[i].abortcount) ;
   }
   TEST(lengthof(eparams) == lparam.abortcount) ;
   TEST(lengthof(eparams) == rparam.abortcount) ;
   TEST(0 == free_syncrun(&srun)) ;

   // TEST abortall_syncrun
   TEST(0 == init_syncrun(&srun)) ;
   init_syncwlist(&wlist) ;
   memset(&eparams, 0, sizeof(eparams)) ;
   memset(&xparams, 0 , sizeof(xparams)) ;
   memset(&lparam, 0, sizeof(lparam)) ;
   memset(&rparam, 0, sizeof(rparam)) ;
   lparam.wlist    = &wlist ;
   rparam.isrunner = true ;
   for (unsigned i = 0; i < lengthof(eparams); ++i) {
      TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &eparams[i])) ;
      xparams[i].waitexit = 3 ;
      TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &xparams[i])) ;
      TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &lparam)) ;
      TEST(0 == startthread_syncrun(&srun, &maintestabort_syncthread, &rparam)) ;
   }
   TEST(0 == runall_syncrun(&srun)) ;
   for (unsigned i = 0; i < lengthof(eparams)/2; ++i) {
      TEST(0 == signalevent_syncrun(&srun, &eparams[i].event)) ;
      TEST(0 == signalfirst_syncrun(&srun, &wlist)) ;
   }
   TEST(lengthof(eparams)*4 == lenwaitqueue_syncrun(&srun)) ;
   TEST(lengthof(eparams)*2 == lenrunqueue_syncrun(&srun)) ;
   TEST(lengthof(eparams)   == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
   TEST(lengthof(eparams)/2 == len_syncwlist(&srun.wakeup_list)) ;
   TEST(lengthof(eparams)/2 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
   TEST(lengthof(eparams)/2 == len_syncwlist(&wlist)) ;
   TEST(0 == abortall_syncrun(&srun)) ;
   TEST(0 == lenwaitqueue_syncrun(&srun)) ;
   TEST(0 == lenrunqueue_syncrun(&srun)) ;
   // wlist will not be changed
   TEST(lengthof(eparams)/2 == len_syncwlist(&wlist)) ;
   TEST(lengthof(eparams)/2 == len_syncqueue(&srun.queues[syncrun_qid_WLIST])) ;
   // wakeup_list and wakeup queue are cleared
   TEST(0 == len_syncwlist(&srun.wakeup_list)) ;
   TEST(0 == len_syncqueue(&srun.queues[syncrun_qid_WAKEUP])) ;
   for (unsigned i = 0; i < lengthof(eparams); ++i) {
      TEST(1 == eparams[i].abortcount) ;
      TEST(3 == xparams[i].abortcount) ;
   }
   TEST(lengthof(eparams) == lparam.abortcount) ;
   TEST(lengthof(eparams) == rparam.abortcount) ;
   TEST(0 == free_syncrun(&srun)) ;

   // unprepare
   s_test_srun = 0 ;
   TEST(0 == free_syncrun(&srun)) ;

   return 0 ;
ONABORT:
   s_test_srun = 0 ;
   free_syncqueue(&wlistqueue) ;
   free_syncrun(&srun) ;
   return EINVAL ;
}

int unittest_task_syncrun()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initqueueentry()) goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_internal())       goto ONABORT ;
   if (test_threadlifetime()) goto ONABORT ;
   if (test_synchronize())    goto ONABORT ;
   if (test_run())            goto ONABORT ;
   if (test_run2())           goto ONABORT ;
   if (test_runwaitchain())   goto ONABORT ;
   if (test_abort())          goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
