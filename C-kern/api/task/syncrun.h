/* title: SyncRun

   This object helps to manage a set of <syncthread_t>
   in a set of different memory queues.

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
#ifndef CKERN_TASK_SYNCRUN_HEADER
#define CKERN_TASK_SYNCRUN_HEADER

#include "C-kern/api/task/syncqueue.h"
#include "C-kern/api/task/syncwlist.h"

// forward
struct dlist_node_t ;
struct syncevent_t ;
struct syncthread_t ;

/* typedef: struct syncrun_t
 * Export <syncrun_t> into global namespace. */
typedef struct syncrun_t                  syncrun_t ;

/* typedef: syncrun_f
 * Function pointer to syncthread implementation - same as <syncthread_f>.
 * Defined only to make <syncrun_t> independent of header of <syncthread_t>. */
typedef int /*err code (0 == OK)*/     (* syncrun_f) (struct syncthread_t * sthread, uint32_t signalstate) ;

/* enums: syncrun_returncode_e
 * Values returned from <retcode_syncrun> indicating an abnormal condition.
 *
 * syncrun_returncode_ABORT   - The process was aborted.
 * */
enum syncrun_returncode_e {
   syncrun_returncode_ABORT = -1,
} ;

typedef enum syncrun_returncode_e         syncrun_returncode_e ;

/* enums: syncrun_state_e
 * The state of the running thread.
 * This state is stored in <syncrun_t.runningstate> and
 * checked after the thread returns (yields the processor).
 *
 * syncrun_state_CONTINUE - The thread needs to run at least one mroe time.
 * syncrun_state_ABORT    - The thread has generated some error and should be aborted. This value is set internally
 *                          if something goes wrong.
 * syncrun_state_EXIT     - The thread has terminated, freed all resources, and wants to be removed from its run queue.
 * syncrun_state_WAIT     - The thread wants to wait for an event. Either for the exit of another thread or an external
 *                          event. It could wait also for an event stored in queue <syncrun_qid_WLIST>. */
enum syncrun_state_e {
   syncrun_state_CONTINUE,
   syncrun_state_ABORT,
   syncrun_state_EXIT,
   syncrun_state_WAIT
} ;

typedef enum syncrun_state_e              syncrun_state_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncrun
 * Test <syncrun_t> functionality. */
int unittest_task_syncrun(void) ;
#endif


/* struct: syncrun_t
 * Manages a set of <syncthread_t>.
 * They are in queues of type <syncqueue_t>. Each queue corresponds to a state of a <syncthread_t>.
 *
 * o All new thread are stored in the init queue (<syncrun_qid_INIT>) which calls <callinit_syncthread>
 *   for every stored thread.
 * o An already initialized thread is stored in the run queue (<syncrun_qid_RUN> or <syncrun_qid_RUN2>)
 *   which calls <callrun_syncthread> for every stored thread repeatedly.
 * o A thread which waits for an event is stored in the wait queue (<syncrun_qid_WAIT> or <syncrun_qid_WAIT2>)
 *   which is ignored until the event is signaled.
 * o There is also a waiting list queue (<syncrun_qid_WLIST>) which stores nodes of the list type <syncwlist_t>.
 *   This queue serves as storage for <syncwlist_t> and contains multiple lists of waiting threads. Every chain
 *   of waiting threads wait for the same condition. Any synchronization mechanism derived from <syncwlist_t> can handle
 *   therefore any number of waiting threads.
 * o A reference to a thread whose event is signaled is put in the wakeup queue (<syncrun_qid_WAKEUP>).
 * o A signaled entry of a <syncwlist_t> is transfered into wakeup_list. */
struct syncrun_t {
   /* variable: queues
    * Queues wich are used to store <syncthread_t>.
    * Every queue symbolizes a different running state. */
   syncqueue_t       queues[7] ;
   /* variable: wakeup_list
    * List of <syncevent_t>.
    * Every event references a waiting <syncthread_t> (see <syncwait_t>).
    * If the event of a waiting thread is signaled it is removed from its waiting list and stored into wakeup_list.
    * During the next call to <runall_syncrun> all referenced <syncthread_t> are run and removed from wakeup_list. */
   syncwlist_t       wakeup_list ;
   /* variable: wakeup
    * Stores information used during wakeup of a thread which waited for an event.
    * These values are only valid if the running thread receives a <syncthread_signal_WAKEUP>.
    *
    * continuelabel - The address of the label where execution should continue after wakeup.
    * retcode       - The return code of the exited thread the woken up thread waited for.
    *                 This value is only valid if a call to <waitforexit_syncrun> was made enter waiting state. */
   struct {
      void *            continuelabel ;
      int               retcode ;
   }                 wakeup ;
   /* variable: waitinfo
    * Stores waiting information of the running thread if its state is changed to <syncrun_state_WAIT>.
    *
    * wlist         - The <syncwlist_t> which contains the following event. If this value is 0
    *                 the event is not contained in a <syncwlist_t>.
    * event         - The event the running thread should wait for.
    *                 Set with internal function <setstatewait_syncrun>.
    * continuelabel - The address of a label where execution should continue.
    *                 Set with internal function <setstatewait_syncrun>. */
   struct {
      struct
      syncwlist_t *     wlist ;
      struct
      syncevent_t *     event ;
      void *            continuelabel ;
   }                 waitinfo ;
   /* variable: running
    * Stores information of the current thread and its running state.
    *
    * laststarted - Points to exitevent (<syncevent_t>) of <syncthread_t> which was last started
    *               by the running thread. Used in function call <waitforexit_syncrun>.
    * thread      - Points to the running thread.
    * state       - Value from <syncrun_state_e> which indicates the state of the running thread.
    *               Before a thread is run this value is set to <syncrun_state_CONTINUE>.
    *               If the thread calls functions like <abortthread_syncrun>, <exitthread_syncrun> or <waitforexit_syncrun>
    *               this value is changed to <syncrun_state_ABORT>, <syncrun_state_EXIT> resp. <syncrun_state_WAIT>.
    *               Also any protocol error, for example calling <setstateexit_syncrun> twice,
    *               changes this value to <syncrun_state_ABORT>. */
   struct {
      struct
      syncevent_t *     laststarted ;
      struct
      syncthread_t *    thread ;
      uint8_t           state ;
   }                 running ;
} ;

// group: lifetime

/* define: syncrun_FREE
 * Static initializer. */
#define syncrun_FREE    \
         { { syncqueue_FREE }, syncwlist_FREE, { 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }

/* function: init_syncrun
 * Initializes a set of queues. The queues are used to store <syncthread_t>.
 * If the state of a <syncthread_t> changes it is moved to another queue or is removed
 * from its queue in case of <syncrun_state_EXIT> or <syncrun_state_ABORT>. */
int init_syncrun(/*out*/syncrun_t * srun) ;

/* function: free_syncrun
 * Frees all queues and removed any stored <syncthread_t>.
 * Resources held by any thread are not freed.
 * Call <abortall_syncrun> before if you want to let know
 * all threads that they should free their resources. */
int free_syncrun(syncrun_t * srun) ;

// group: query

/* function: isfree_syncrun
 * Returns true if *srun equals to <syncrun_FREE>. */
bool isfree_syncrun(const syncrun_t * srun) ;

/* function: leninitqueue_syncrun
 * Returns the number of syncthreads which are started but not run at least once. */
size_t leninitqueue_syncrun(const syncrun_t * srun) ;

/* function: lenrunqueue_syncrun
 * Returns the number of syncthreads which are run at least once. */
size_t lenrunqueue_syncrun(const syncrun_t * srun) ;

/* function: lenwaitqueue_syncrun
 * Returns the number of syncthreads which are waiting for an event. */
size_t lenwaitqueue_syncrun(const syncrun_t * srun) ;

/* function: continuelabel_syncrun
 * Returns address where execution should continue after wakeup.
 * Use returned value for macro <handlesignal_syncthread>.
 * The returned value is only valid if the running thread received a <syncthread_signal_WAKEUP>. */
void * continuelabel_syncrun(const syncrun_t * srun) ;

/* function: retcode_syncrun
 * Returns return code of an exited <syncthread_t>.
 * A value of 0 means no error. A value > 0 means an error code.
 * A value < 0 is reserved for special values.
 * See <syncrun_returncode_e> for a list of special values.
 * The returned value is only valid if the running thread received a <syncthread_signal_WAKEUP>
 * and called <waitforexit_syncrun> to wait. <waitforexit_syncrun> uses this value as its return value. */
int retcode_syncrun(const syncrun_t * srun) ;

// group: thread-lifetime

/* function: startthread_syncrun
 * Starts a new <syncthread_t> and stores it in srun's initqueue.
 * The function mainfct is called the next time <runall_syncrun> is called.
 * The initialization argument initarg is passed as state value to the new <syncthread_t> the first time it is called. */
int startthread_syncrun(syncrun_t * srun, syncrun_f mainfct, void * initarg) ;

/* function: startthread2_syncrun
 * Starts a new <syncthread_t> and stores it in srun's initqueue.
 * The function mainfct is called the next time <runall_syncrun> is called.
 * The initialization argument initarg with size initargsize is allocated as part of srun's initqueue and is passed as
 * state value to the new <syncthread_t> the first time it is called.
 * The initialization argument is freed after it has processed <syncthread_signal_INIT> - always.
 * A pointer to the memory of the initialization argument is returned in initarg. */
int startthread2_syncrun(syncrun_t * srun, syncrun_f mainfct, uint8_t initargsize, /*out*/void ** initarg) ;

/* function: abortthread_syncrun
 * Sets the state of the running thread to <syncrun_state_ABORT> and returns 0.
 * The function is implemented as macro so the return statement let the caller
 * return to srun which called it. */
void abortthread_syncrun(syncrun_t * srun) ;

/* function: exitthread_syncrun
 * Sets the state of the running thread to <syncrun_state_EXIT> and returns err.
 * The function is implemented as macro so the return statement let the caller
 * return to srun which called it. */
void exitthread_syncrun(syncrun_t * srun, int err) ;

// group: synchronization

/* function: waitforexit_syncrun
 * Waits for the last started thread to exit.
 * If you start more than one thread only the last one can be waited for.
 * If no call to <startthread_syncrun> or <startthread2_syncrun> was made before the caller is aborted.
 * Yielding the processor (return from function) resets the internal state of the yielding thread to
 * "no thread started".
 * The function is implemented as macro and uses <setstatewait_syncrun> to change
 * the state of the calling thread and then returns to srun. After the last started thread has exited
 * the caller is woken up and continues execution after this function call. The return code of this
 * function is the return code of the exited thread. See <syncrun_returncode_e> for a list of special
 * return values. The convention is that a return value != 0 always indicates an error. */
int waitforexit_syncrun(syncrun_t * srun) ;

/* function: waitforevent_syncrun
 * Waits for syncevent to be signaled. Another syncthread_t must call
 * <signalevent_syncrun> on syncevent to wakeup the caller of this function.
 * The function is implemented as macro and uses <setstatewait_syncrun> to change
 * the state of the calling thread and then returns to srun.
 * After wakup the caller continues execution after this function. */
void waitforevent_syncrun(syncrun_t * srun, struct syncevent_t * syncevent) ;

/* function: waitforlist_syncrun
 * Waits for syncwlist to be signaled. The calling thread inserts a new
 * event node in syncwlist and waits for it. Another thread must call <signalall_syncrun>
 * on syncwlist or <signalfirst_syncrun> for one or more times to wakeup the caller
 * of this function.
 * The function is implemented as macro and uses <setstatewait_syncrun> to change
 * the state of the calling thread and then returns to srun.
 * After wakup the caller continues execution after this function call. */
void waitforlist_syncrun(syncrun_t * srun, struct syncwlist_t * syncwlist) ;

/* function: signalevent_syncrun
 * Stores the referenced waiting <syncthread_t> in a wakeup queue.
 * If there is no one waiting for this event this function does nothing.
 * If the event does not point to a waiting thread in the a wait queue
 * EINVAL is returned. If there is no more memory ENOMEM is returned. */
int signalevent_syncrun(syncrun_t * srun, struct syncevent_t * syncevent) ;

/* function: signalfirst_syncrun
 * Transfers the first entry of syncwlist to an internal wakup_list.
 * After successfull return syncwlist contains one entry less.
 * If syncwlist is empty this function does nothing.
 * If nodes of syncwlist are not stored in the internal queue
 * <syncrun_qid_WLIST> (allocated with call to <waitforlist_syncrun>) EINVAL
 * is returned. */
int signalfirst_syncrun(syncrun_t * srun, struct syncwlist_t * syncwlist) ;

/* function: signalall_syncrun
 * Transfers all nodes of syncwlist to an internal wakup_list.
 * After successfull return syncwlist is empty.
 * If syncwlist is empty this function does nothing.
 * If nodes of syncwlist are not stored in the internal queue
 * <syncrun_qid_WLIST> (allocated with call to <waitforlist_syncrun>) EINVAL
 * is returned. */
int signalall_syncrun(syncrun_t * srun, struct syncwlist_t * syncwlist) ;

// group: run

/* function: runall_syncrun
 * Runs all <syncthread_t> stored in internal queues.
 *
 * o 1. All syncthread stored in run queue are run.
 * o 2. All syncthread stored in initqueue are run and placed in normal run queue.
 * o 3. Preparation step:
 *        - The wakeup queue is copied and then cleared.
 *        - The wakeup_list is copied and then cleared.
 *      All syncthread stored in copy of wakeup queue are run and placed in correct queue. The copy is cleared.
 *      All syncthread stored in copy of wakeup_list are run and placed in correct queue. The copy is cleared.
 * o 4. This step is always considered during execution of the other steps if its condition is met;
 *      If a thread exits and if there is a waiting thread the waiting thread is run directly after.
 *      See <signalevent_syncrun>.
 * o 5. This step is always considered during execution of the other steps if its condition is met;
 *      If a thread signals an event another thread is waiting for a reference to the waiting thread
 *      is placed in the wakeup queue.
 *      If a thread signals a <syncwlist_t> the list entry is transfered into the wakeup_list.
 *      See <signalfirst_syncrun> and <signalall_syncrun>.
 * */
int runall_syncrun(syncrun_t * srun) ;

/* function: abortall_syncrun
 * Aborts all running and waiting threads.
 * The run, wait and wakeup queues and the wakeup_list are freed.
 * Threads in the initqueue are not executed. They are left untouched. */
int abortall_syncrun(syncrun_t * srun) ;

// group: internal

/* function: setstateabort_syncrun
 * Changes state of running thread to <syncrun_state_ABORT>.
 * If the running thread returns it will called again with its signal set to <syncthread_signal_ABORT>.
 * See also <callabort_syncthread>.
 * Therefore call this function if resources have to be freed after the thread returns.
 * The return code of the running thread will be set to <syncrun_returncode_ABORT> no matter what
 * it returns. This special value indicates to another thread that it has been aborted.
 * No error log is written. The calling thread has to protocol any error condition itself. */
void setstateabort_syncrun(syncrun_t * srun) ;

/* function: setstateexit_syncrun
 * Changes state of running thread to <syncrun_state_EXIT>.
 * If the running thread returns it will be removed from the running queue.
 * Therefore call this function only after all resources have been freed.
 * The return code of the running thread will be used as value
 * returned in <retcode_syncrun> if another threads waits for its exit.
 *
 * In case the running thread's state is not <syncrun_state_CONTINUE>
 * the state will be changed to <syncrun_state_ABORT>.
 * See <setstateabort_syncrun> what will happen then. */
void setstateexit_syncrun(syncrun_t * srun) ;

/* function: setstatewait_syncrun
 * Changes state of running thread to <syncrun_state_WAIT>.
 * Parameter event points to the event the thread should wait for and
 * parameter continuelabel determines the address of the label where execution
 * should continue after wakeup. Both parameters are stored in <syncrun_t.waitinfo>.
 * If the running thread returns it will be removed from the running queue
 * and inserted into the wait queue. Only after the event has been signaled
 * it will be removed from the wait queue and run again.
 *
 * Preconditions:
 * o The state of the running thread has to be <syncrun_state_CONTINUE>
 * o <iswaiting_syncevent>(event) has to return false.
 * If the preconditions are not met the state of the running thread will be changed to <syncrun_state_ABORT>.
 * See <setstateabort_syncrun> what will happen then. */
void setstatewait_syncrun(syncrun_t * srun, struct syncevent_t * event, void * continuelabel) ;

/* function: setstatewaitlist_syncrun
 * Same as <setstatewait_syncrun> except supports multiple waiting threads.
 * If an out of memory error occurs the state of the running thread will be changed to <syncrun_state_ABORT>. */
void setstatewaitlist_syncrun(syncrun_t * srun, struct syncwlist_t * wlist, void * continuelabel) ;



// section: inline implementation

/* define: abortthread_syncrun
 * Implements <syncrun_t.abortthread_syncrun>. */
#define abortthread_syncrun(srun)                        \
         do {                                            \
            setstateabort_syncrun(srun) ;                \
            return 0 ;                                   \
         } while (0)

/* define: exitthread_syncrun
 * Implements <syncrun_t.exitthread_syncrun>. */
#define exitthread_syncrun(srun, err)                    \
         do {                                            \
            setstateexit_syncrun(srun) ;                 \
            return err ;                                 \
         } while (0)

/* define: waitforexit_syncrun
 * Implements <syncrun_t.waitforexit_syncrun>. */
#define waitforexit_syncrun(srun)                        \
         ( __extension__ ({                              \
            __label__ waitlabel ;                        \
            setstatewait_syncrun((srun),                 \
                  srun->running.laststarted,             \
                  __extension__ && waitlabel) ;          \
            return 0 ;                                   \
            waitlabel:                                   \
            retcode_syncrun(srun) ;                      \
         }))

/* define: continuelabel_syncrun
 * Implements <syncrun_t.continuelabel_syncrun>. */
#define continuelabel_syncrun(srun) \
         ((srun)->wakeup.continuelabel)

/* define: retcode_syncrun
 * Implements <syncrun_t.retcode_syncrun>. */
#define retcode_syncrun(srun)       \
         ((srun)->wakeup.retcode)

/* define: waitforevent_syncrun
 * Implements <syncrun_t.waitforevent_syncrun>. */
#define waitforevent_syncrun(srun, syncevent)            \
         ( __extension__ ({                              \
            __label__ waitlabel ;                        \
            setstatewait_syncrun((srun),                 \
                  (syncevent),                           \
                  __extension__ && waitlabel) ;          \
            return 0 ;                                   \
            waitlabel:                                   \
            (void)0 ;                                    \
         }))

/* define: waitforlist_syncrun
 * Implements <syncrun_t.waitforlist_syncrun>. */
#define waitforlist_syncrun(srun, syncwlist)             \
         ( __extension__ ({                              \
            __label__ waitlabel ;                        \
            setstatewaitlist_syncrun((srun),             \
                  (syncwlist),                           \
                  __extension__ && waitlabel) ;          \
            return 0 ;                                   \
            waitlabel:                                   \
            (void)0 ;                                    \
         }))

#endif
