/* title: SyncThread

   A simple execution context for execution of functions in a cooperative way.

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

   file: C-kern/api/task/syncthread.h
    Header file <SyncThread>.

   file: C-kern/task/syncthread.c
    Implementation file <SyncThread impl>.
*/
#ifndef CKERN_TASK_SYNCTHREAD_HEADER
#define CKERN_TASK_SYNCTHREAD_HEADER

/* typedef: struct syncthread_t
 * Export <syncthread_t> into global namespace. */
typedef struct syncthread_t            syncthread_t ;

/* typedef: syncthread_f
 * Function pointer to syncthread implementation.
 * Every return value != 0 is considered an error but is ignored
 * except in case of <syncthread_signal_ABORT> which will terminate the whole process.
 * To end the thread call <exit_syncrun>. */
typedef int /*err code (0 == OK)*/  (* syncthread_f) (syncthread_t * sthread, uint32_t signalstate) ;

/* enums: syncthread_signal_e
 * The signal flags delivered as second argument to function <syncthread_f>.
 *
 * syncthread_signal_NULL   - No signal indicates normal running mode.
 * syncthread_signal_WAKEUP - Signal received after <syncthread_t> waited for some event which has occurred.
 * syncthread_signal_INIT   - Initialization signal meaning <syncthread_t.state> contains a pointer to the function arguments.
 *                            A value of 0 means no arguments.
 * syncthread_signal_ABORT  - Abort signal. The syncthread should free all resources, clear variables and return.
 *                            After return its returned value should be zero else it is assumed abortion failed
 *                            and the whole process must therefore be aborted !
 * */
enum syncthread_signal_e {
   syncthread_signal_NULL   = 0,
   syncthread_signal_WAKEUP = 1,
   syncthread_signal_INIT   = 2,
   syncthread_signal_ABORT  = 3,
} ;

typedef enum syncthread_signal_e          syncthread_signal_e ;



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncthread
 * Test <syncthread_t> functionality. */
int unittest_task_syncthread(void) ;
#endif


/* struct: syncthread_t
 * A simple function context which is executed in a cooperative way.
 * Yielding the execution context to another <syncthread_t> is done in a synchronous way.
 * Therefore no locking is required to synchronize between syncthreads in the same thread
 * context. */
struct syncthread_t {
   /* variable: mainfct
    * Function where execution should continue. */
   syncthread_f   mainfct ;
   /* variable: state
    * Holds pointer to variables, or init argument or execution state internal to <mainfct>.
    * See also <state_syncthread>. */
   void *         state ;
} ;

// group: lifetime

/* define: syncthread_INIT_FREEABLE
 * Static initializer. */
#define syncthread_INIT_FREEABLE \
         { 0, 0 }

/* define: syncthread_INIT
 * Static initializer.
 *
 * Parameters:
 * mainfct - The function which implements the thread main loop.
 * state   - The function argument during initialization. */
#define syncthread_INIT(mainfct, state)   \
         { mainfct, state }

/* function: init_syncthread
 * Initializes sthread with { mainfct, state } . */
void init_syncthread(/*out*/syncthread_t * sthread, syncthread_f mainfct, void * state) ;

/* function: free_syncthread
 * Sets all members to 0. No resources are freed. */
void free_syncthread(syncthread_t * sthread) ;

// group: query

/* function: isfree_syncthread
 * Returns true if sthread is intialized with <syncthread_INIT_FREEABLE>. */
bool isfree_syncthread(const syncthread_t * sthread) ;

/* function: state_syncthread
 * Returns <syncthread_t.state> from sthread.
 * The state serves 3 purposes:
 * o  If argument signalstate of <syncthread_f> is set to syncthread_signal_INIT
 *    state contains the value initarg given as argument in <createthread_syncrun>.
 * o  State could point to a label address if previously used as argument to function call <setcontinuelabel_syncthread>.
 * o  State could point to a structure allocated on the heap which contains more complex state information if previously
 *    set with a call to function <setstate_syncthread>. */
void * state_syncthread(syncthread_t * sthread) ;

// group: execution state

/* function: continue_syncthread
 * Jumps to <state_syncthread> which was set with <setcontinuelabel_syncthread>.
 * The address of the label must have been stored into <syncthread_t.state>
 * previously with a call to <setcontinuelabel_syncthread>. */
void continue_syncthread(syncthread_t * sthread) ;

/* function: setcontinuelabel_syncthread
 * Calls <setstate_syncthread> with address of a label.
 * Use <continue_syncthread> at the start of the thread to continue
 * at the stored label address. */
void setcontinuelabel_syncthread(syncthread_t * sthread, IDNAME labelname) ;

/* function: setstate_syncthread
 * Sets the state of the thread.
 * The function <setcontinuelabel_syncthread> uses the same variable.
 * Therefore you store either a pointer to an allocated variable or an address of a label where execution should continue. */
void setstate_syncthread(syncthread_t * sthread, void * state) ;

// group: signal state

/* function: handlesignal_syncthread
 * Jumps to oninitlabel, to onrunlabel, to onabortlabel or to address of continuelabel.
 * The behaviour depends on signalstate which must be set to a value from <syncthread_signal_e>.
 *
 * Value of signalstate:
 * syncthread_signal_NULL   - The function jumps to onrunlabel.
 * syncthread_signal_WAKEUP - The function jumps to dynamic address of continuelabel.
 *                            Use <continuelabel_syncrun> to set the value of continuelabel.
 * syncthread_signal_INIT   - The function jumps to oninitlabel.
 * syncthread_signal_ABORT  - The function jumps to onabortlabel.
 * undefined value          - The function jumps to onabortlabel.
 * */
void handlesignal_syncthread(const uint32_t signalstate, void * continuelabel, IDNAME oninitlabel, IDNAME onrunlabel, IDNAME onabortlabel) ;

// group: call convention

/* function: callrun_syncthread
 * Calls <syncthread_t.mainfct> with signalstate set to <syncthread_signal_NULL>.
 * The return value is the value returned from this call. See also <syncthread_f>. */
int callrun_syncthread(syncthread_t * sthread) ;

/* function: callwakeup_syncthread
 * Calls <syncthread_t.mainfct> with signalstate set to <syncthread_signal_WAKEUP>.
 * The return value is the value returned from this call. See also <syncthread_f>. */
int callwakeup_syncthread(syncthread_t * sthread) ;

/* function: callinit_syncthread
 * Calls <syncthread_t.mainfct> with signalstate set to <syncthread_signal_INIT>.
 * The return value is the value returned from this call. See also <syncthread_f>. */
int callinit_syncthread(syncthread_t * sthread) ;

/* function: callabort_syncthread
 * Calls <syncthread_t.mainfct> with signalstate set to <syncthread_signal_ABORT>.
 * The return value is the value returned from this call. See also <syncthread_f>. */
int callabort_syncthread(syncthread_t * sthread) ;



// section: inline implementation

/* define: callabort_syncthread
 * Implements <syncthread_t.callabort_syncthread>. */
#define callabort_syncthread(sthread)                                   \
         ( __extension__ ({                                             \
               typeof(sthread) _sthread = (sthread) ;                   \
            _sthread->mainfct(_sthread, syncthread_signal_ABORT) ;      \
         }))

/* define: callinit_syncthread
 * Implements <syncthread_t.callinit_syncthread>. */
#define callinit_syncthread(sthread)                                    \
         ( __extension__ ({                                             \
               typeof(sthread) _sthread = (sthread) ;                   \
            _sthread->mainfct(_sthread, syncthread_signal_INIT) ;       \
         }))

/* define: callrun_syncthread
 * Implements <syncthread_t.callrun_syncthread>. */
#define callrun_syncthread(sthread)                                     \
         ( __extension__ ({                                             \
               typeof(sthread) _sthread = (sthread) ;                   \
            _sthread->mainfct(_sthread, syncthread_signal_NULL) ;       \
         }))

/* define: callwakeup_syncthread
 * Implements <syncthread_t.callwakeup_syncthread>. */
#define callwakeup_syncthread(sthread)                                  \
         ( __extension__ ({                                             \
               typeof(sthread) _sthread = (sthread) ;                   \
            _sthread->mainfct(_sthread, syncthread_signal_WAKEUP) ;     \
         }))

/* define: free_syncthread
 * Implements <syncthread_t.free_syncthread>. */
#define free_syncthread(sthread) \
         ((void)(*(sthread) = (syncthread_t) syncthread_INIT_FREEABLE))

/* define: handlesignal_syncthread
 * Implements <syncthread_t.handlesignal_syncthread>. */
#define handlesignal_syncthread(signalstate, continuelabel, oninitlabel, onrunlabel, onabortlabel) \
         ( __extension__ ({                                             \
            const syncthread_signal_e _sign = (signalstate) ;           \
            switch (_sign) {                                            \
            case syncthread_signal_NULL:                                \
               goto onrunlabel ;                                        \
            case syncthread_signal_WAKEUP:                              \
               goto * (continuelabel) ;                                 \
            case syncthread_signal_INIT:                                \
               goto oninitlabel ;                                       \
            case syncthread_signal_ABORT:                               \
               goto onabortlabel ;                                      \
            }                                                           \
            goto onabortlabel ;                                         \
         }))

/* define: init_syncthread
 * Implements <syncthread_t.init_syncthread>. */
#define init_syncthread(sthread, mainfct, state) \
         ((void)(*(sthread) = (syncthread_t) syncthread_INIT(mainfct, state)))

/* define: continue_syncthread
 * Implements <syncthread_t.continue_syncthread>. */
#define continue_syncthread(sthread) \
         ((void) ( __extension__ ({ goto *state_syncthread(sthread); 0; })))

/* define: setstate_syncthread
 * Implements <syncthread_t.setstate_syncthread>. */
#define setstate_syncthread(sthread, state_) \
         ((void)((sthread)->state = state_))

/* define: state_syncthread
 * Implements <syncthread_t.state_syncthread>. */
#define state_syncthread(sthread)   \
         ((void*)((sthread)->state))

/* define: setcontinuelabel_syncthread
 * Implements <syncthread_t.setcontinuelabel_syncthread>. */
#define setcontinuelabel_syncthread(sthread, labelname)   \
         ((void)((sthread)->state = __extension__ && labelname))

#endif
