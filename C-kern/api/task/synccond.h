/* title: SyncCondition

   Beschreibt eine Wartebedingung.
   Arbeitet eng mit <syncrunner_t> zusammen.

   Includes:
   Dateien "C-kern/api/task/syncfunc.h" und "C-kern/api/task/syncrunner.h"
   sollten vorher includiert werden.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/synccond.h
    Header file <SyncCondition>.

   file: C-kern/task/synccond.c
    Implementation file <SyncCondition impl>.
*/
#ifndef CKERN_TASK_SYNCCOND_HEADER
#define CKERN_TASK_SYNCCOND_HEADER

#include "C-kern/api/ds/link.h"

// forward
struct syncfunc_t;
struct syncfunc_param_t;
struct syncrunner_t;

/* typedef: struct synccond_t
 * Export <synccond_t> into global namespace. */
typedef struct synccond_t synccond_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_synccond
 * Test <synccond_t> functionality. */
int unittest_task_synccond(void);
#endif


/* struct: synccond_t
 * Implementiert eine Wartebedingung.
 * Das Objekt zeigt auf die erste wartende <syncfunc_t> und
 * überlässt die Implementierung der Speicherverwaltung und
 * einer ganzen Liste von wartenden Funktionen dem Runmanager
 * Objekt <syncrunner_t>. Nur ein einziger <syncrunner_t>
 * (ein Thread) wird unterstützt. Sollte ein <synccond_t>
 * von mehreren <syncrunner_t> gleichzeitig benutzt werden,
 * ist das Verhalten undefiniert/fehlerhaft. */
struct synccond_t {
   linkd_t waitfunc;
};

// group: lifetime

/* define: synccond_FREE
 * Static initializer. */
#define synccond_FREE \
         { linkd_FREE }

/* function: init_synccond
 * Setze scond auf Initialwert - eine leere Warteliste. */
void init_synccond(/*out*/synccond_t* scond);

/* function: free_synccond
 * Setze scond auf <synccond_FREE>.
 * Falls noch eine wartende <syncfunc_t> verlinkt ist,
 * sollte vor dieser Funktion <wakeupall_synccond> aufgerufen werden.
 * Ansonsten gibt es einen verwaisten link. */
void free_synccond(synccond_t* scond);

// group: query

/* function: iswaiting_synccond
 * Gib true zurück, falls mindestens eine Funktion wartet, sonst false. */
static inline int iswaiting_synccond(const synccond_t* scond);

/* function: waitfunc_synccond
 * Gib die erste wartende <syncfunc_t> zurück.
 *
 * Unchecked Precondition:
 * o iswaiting_synccond(scond) */
struct syncfunc_t* waitfunc_synccond(const synccond_t* scond);

// group: update

/* function: link_synccond
 * Verlinkt eine wartende <syncfunc_t> am Ende der Warteliste.
 * Siehe <syncrunner_t> für die Verwendung dieser Funktion.
 *
 * Unchecked Precondition:
 * o ! isvalid_link(waitlist_syncfunc(sfunc)) */
void link_synccond(synccond_t* scond, struct syncfunc_t* sfunc);

/* function: unlink_synccond
 * Löscht die Verlinkung zur ersten wartenden Funktion <waitfunc_synccond>.
 *
 * Unchecked Precondition:
 * o iswaiting_synccond(scond) */
void unlink_synccond(synccond_t* scond);

/* function: unlinkall_synccond
 * Löscht die Verlinkung zu allen wartenden Funktionen.
 * Mittels Aufruf von <waitfunc_synccond> vor Aufruf dieser Funktion
 * kann der Kopf der Warteliste ermittelt werden.
 *
 * Unchecked Precondition:
 * o iswaiting_synccond(scond) */
void unlinkall_synccond(synccond_t* scond);

/* function: wakeup_synccond
 * Wecke die erste wartende Funktion auf. NO-OP, wenn <iswaiting_synccond>(srun)==false.
 * Leitet Aufruf weiter an <wakeup_syncrunner>. */
int wakeup_synccond(struct synccond_t* scond, const struct syncfunc_param_t* sfparam);

/* function: wakeupall_synccond
 * Wecke alle wartende Funktionen auf. NO-OP, wenn <iswaiting_synccond>(srun)==false.
 * Leitet Aufruf weiter an <wakeupall_syncrunner>. */
int wakeupall_synccond(struct synccond_t* scond, const struct syncfunc_param_t* sfparam);


// section: inline implementation

/* define: free_synccond
 * Implements <synccond_t.free_synccond>. */
#define free_synccond(scond) \
         ((void)(*(scond) = (synccond_t) synccond_FREE))

/* define: init_synccond
 * Implements <synccond_t.init_synccond>. */
#define init_synccond(scond) \
         initself_linkd(&(scond)->waitfunc)

/* define: iswaiting_synccond
 * Implements <synccond_t.iswaiting_synccond>. */
static inline int iswaiting_synccond(const synccond_t* scond)
{
         return ! isself_linkd(&scond->waitfunc) && isvalid_linkd(&scond->waitfunc);
}

/* define: link_synccond
 * Implements <synccond_t.link_synccond>. */
#define link_synccond(scond, sfunc) \
         initprev_linkd(waitlist_syncfunc(sfunc), &(scond)->waitfunc)

/* define: unlink_synccond
 * Implements <synccond_t.unlink_synccond>. */
#define unlink_synccond(scond) \
         unlinkself_linkd((scond)->waitfunc.next)

/* define: unlinkall_synccond
 * Implements <synccond_t.unlinkall_synccond>. */
#define unlinkall_synccond(scond) \
         do {                                 \
            synccond_t* _sc=(scond);          \
            unlinkself_linkd(&_sc->waitfunc); \
            init_synccond(_sc);               \
         } while(0)

/* define: waitfunc_synccond
 * Implements <synccond_t.waitfunc_synccond>. */
#define waitfunc_synccond(scond) \
         (castPwaitlist_syncfunc((scond)->waitfunc.next))

/* define: wakeup_synccond
 * Implements <synccond_t.wakeup_synccond>. */
#define wakeup_synccond(scond, sfparam) \
         wakeup_syncrunner((sfparam)->srun, scond)

/* define: wakeupall_synccond
 * Implements <synccond_t.wakeupall_synccond>. */
#define wakeupall_synccond(scond, sfparam) \
         wakeupall_syncrunner((sfparam)->srun, scond)

#endif
