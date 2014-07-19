/* title: SyncWait

   Exports the type <syncwait_t> which extends <syncthread_t>
   with additional information to be able to wait for a <syncevent_t>.

   >  ╭────────────╮      ╭─────────────╮
   >  | syncwait_t ├──────┤ syncevent_t |
   >  ╰────────────╯1    1╰─────────────╯

   Includes:
   You need to include type <syncthread_t> before including this file.

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

   file: C-kern/api/task/syncwait.h
    Header file <SyncWait>.

   file: C-kern/task/syncwait.c
    Implementation file <SyncWait impl>.
*/
#ifndef CKERN_TASK_SYNCWAIT_HEADER
#define CKERN_TASK_SYNCWAIT_HEADER

/* typedef: struct syncwait_t
 * Export <syncwait_t> into global namespace. */
typedef struct syncwait_t                 syncwait_t ;

/* typedef: struct syncevent_t
 * Export <syncevent_t> into global namespace. */
typedef struct syncevent_t                syncevent_t ;

// TODO: implement syncwait_condition_t
// TODO: remove all other

/* typedef: struct syncwait_condition_t
 * Export <syncwait_condition_t> into global namespace. */
typedef struct syncwait_condition_t syncwait_condition_t;

// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncwait
 * Teste <syncwait_condition_t>. */
int unittest_task_syncwait(void) ;
#endif


/* struct: syncevent_t
 * Event type stores a pointer to a single waiting <syncwait_t>.
 * If the event occurs you can get the waiting thread with <waiting_syncevent>
 * and call it or move into another queue (see <syncrun_t>). */
struct syncevent_t {
   syncwait_t *   waiting ;
} ;

// group: lifetime

/* define: syncevent_FREE
 * Static initializer. */
#define syncevent_FREE { 0 }

/* define: syncevent_INIT
 * Static initializer. The event supports a single waiting <syncthread_t>
 * and is initialized with a pointer to <syncwait_t>. */
#define syncevent_INIT(waiting)           { waiting }

/* function: initmove_syncevent
 * Moves *initialized* srcsyncevent to destsyncevent. srcsyncevent is not changed.
 * Before calling this function make sure <iswaiting_syncevent>(srcsyncevent) returns true.
 * Do not access srcsyncevent after this operation - it is considered in an invalid, uninitialized state. */
void initmove_syncevent(/*out*/syncevent_t * destsyncevent, const syncevent_t * srcsyncevent) ;

/* function: initmovesafe_syncevent
 * Moves initialized or free srcsyncevent to destsyncevent. srcsyncevent is not changed.
 * Do not access srcsyncevent after this operation - it is considered in an invalid, uninitialized state. */
void initmovesafe_syncevent(/*out*/syncevent_t * destsyncevent, const syncevent_t * srcsyncevent) ;

// group: query

/* function: isfree_syncevent
 * Returns true if syncevent equals to <syncevent_FREE>. */
bool isfree_syncevent(const syncevent_t * syncevent) ;

/* function: iswaiting_syncevent
 * Returns true if there is a waiting thread. */
bool iswaiting_syncevent(const syncevent_t * syncevent) ;

/* function: waiting_syncevent
 * Returns the single thread which waits for this event. */
syncwait_t * waiting_syncevent(const syncevent_t * syncevent) ;



/* struct: syncwait_t
 * Describes a <syncthread_t> waiting for a <syncevent_t>.
 *
 * Bidirectional Link And Memory Compaction:
 * The <syncwait_t> points to <syncevent_t> and vice versa.
 * The bidirectional link helps to move both types of objects in memory
 * without breaking the connection. The movement is done to compact memory.
 *
 * Use <initmove_syncevent> and <initmove_syncwait> to move
 * initialized objects of both types in memory.
 *
 * Use <update_syncwait> if you want to assign a new <syncevent_t>
 * to an already initialized <syncwait_t>.
 *
 * Freeing of objects is not implemented so it is possible to remove either <syncevent_t>
 * from the queue and then <syncwait_t> or vice versa without any access to the other object.
 * Both objects do no allocate any additional resources.
 * */
struct syncwait_t {
   /* variable: thread
    * Stores the <syncthread_t> which is waiting. */
   syncthread_t      thread ;
   /* variable: event
    * A pointer to type <syncevent_t> the syncthread is waiting for. */
   syncevent_t *     event ;
   /* variable: continuelabel
    * The address where execution should continue after the thread is woken up. */
   void *            continuelabel ;
} ;

// group: lifetime

/* define: syncwait_FREE
 * Static initializer. */
#define syncwait_FREE { syncthread_FREE, 0, 0 }

/* function: init_syncwait
 * Initializes syncwait and registers itself at event.
 * The content of event is overwritten - so make sure it is either empty or undefined.
 *
 * Precondition:
 * o This function assumes that the pointer argument event is not NULL. */
void init_syncwait(/*out*/syncwait_t * syncwait, const syncthread_t * thread, syncevent_t * event, void * continuelabel) ;

/* function: initmove_syncwait
 * Moves initialized (!) srcsyncwait to destsyncwait and srcsyncwait is not changed.
 * Do not access srcsyncwait after this operation - it is considered in an invalid, uninitialized state.
 *
 * Precondition:
 * o This function assumes that srcsyncwait is initialized. */
void initmove_syncwait(/*out*/syncwait_t * destsyncwait, const syncwait_t * srcsyncwait) ;

// group: query

/* function: thread_syncwait
 * Returns pointer to syncwait->thread (see <syncwait_t.thread>). */
syncthread_t * thread_syncwait(syncwait_t * syncwait) ;

/* function: event_syncwait
 * Returns value syncwait->event (see <syncwait_t.event>). */
syncevent_t * event_syncwait(const syncwait_t * syncwait) ;

/* function: continuelabel_syncwait
 * Returns pointer to syncwait->continuelabel (see <syncwait_t.continuelabel>). */
void * continuelabel_syncwait(const syncwait_t * syncwait) ;

// group: update

/* function: update_syncwait
 * Changes event and continuelabel in <syncwait_t>.
 * The old reference to <syncevent_t> os overwritten but the
 * referenced old <syncevent_t> is not cleared. This function assumes
 * that the old <syncevent_t> no longer exists.
 * The event is set as new reference and the referenced <syncevent_t>
 * is updated to point to syncwait.
 *
 * Precondition:
 * o This function assumes that <iswaiting_syncevent>(event) returns true. */
void update_syncwait(syncwait_t * syncwait, syncevent_t * event, void * continuelabel) ;



// section: inline implementation

// group: syncevent_t

/* define: initmove_syncevent
 * Implements <syncevent_t.initmove_syncevent>. */
#define initmove_syncevent(destsyncevent, srcsyncevent)     \
         do {                                               \
            typeof(destsyncevent) _dest = (destsyncevent) ; \
            *_dest = *(srcsyncevent) ;                      \
            _dest->waiting->event = _dest ;                 \
         } while(0)

/* define: initmovesafe_syncevent
 * Implements <syncwait_t.initmovesafe_syncevent>. */
#define initmovesafe_syncevent(destsyncevent, srcsyncevent)    \
         do {                                                  \
            typeof(destsyncevent) _dest = (destsyncevent) ;    \
            *_dest = *(srcsyncevent) ;                         \
            if (_dest->waiting) {                              \
               _dest->waiting->event = _dest ;                 \
            }                                                  \
         } while(0)

/* define: isfree_syncevent
 * Implements <syncevent_t.isfree_syncevent>. */
#define isfree_syncevent(syncevent)    \
         (0 == (syncevent)->waiting)

/* define: iswaiting_syncevent
 * Implements <syncevent_t.iswaiting_syncevent>. */
#define iswaiting_syncevent(syncevent) \
         (0 != (syncevent)->waiting)

/* define: waiting_syncevent
 * Implements <syncevent_t.waiting_syncevent>. */
#define waiting_syncevent(syncevent)  \
         ((syncevent)->waiting)

// group: syncwait_t

/* define: init_syncwait
 * Implements <syncwait_t.init_syncwait>. */
#define init_syncwait(syncwait, thread, _event, continuelabel) \
         do {                                                  \
            typeof(syncwait) _sw = (syncwait) ;                \
            *_sw = (syncwait_t) {                              \
                        *(thread),                             \
                        (_event),                              \
                        (continuelabel)                        \
                  } ;                                          \
            _sw->event->waiting = _sw ;                        \
         } while(0)

/* define: continuelabel_syncwait
 * Implements <syncwait_t.continuelabel_syncwait>. */
#define continuelabel_syncwait(syncwait)   \
         ((syncwait)->continuelabel)

/* define: initmove_syncwait
 * Implements <syncwait_t.initmove_syncwait>. */
#define initmove_syncwait(destsyncwait, srcsyncwait)           \
         do {                                                  \
            typeof(destsyncwait) _dest = (destsyncwait) ;      \
            *_dest = *(srcsyncwait) ;                          \
            _dest->event->waiting = _dest ;                    \
         } while(0)

/* define: thread_syncwait
 * Implements <syncwait_t.thread_syncwait>. */
#define thread_syncwait(syncwait)   \
         (&(syncwait)->thread)

/* define: update_syncwait
 * Implements <syncwait_t.update_syncwait>. */
#define update_syncwait(syncwait, _event, _continuelabel)      \
         do {                                                  \
            typeof(syncwait) _sw = (syncwait) ;                \
            _sw->event         = (_event) ;                    \
            _sw->continuelabel = (_continuelabel) ;            \
            _sw->event->waiting = _sw ;                        \
         } while(0)

/* define: event_syncwait
 * Implements <syncwait_t.event_syncwait>. */
#define event_syncwait(syncwait)   \
         ((syncwait)->event)


#endif
