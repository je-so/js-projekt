/* title: SyncCondition

   Beschreibt eine Wartebedingung.
   Arbeitet eng mit <syncrunner_t> zusammen.

   Includes:
   Dateien "C-kern/api/task/syncfunc.h" und "C-kern/api/task/syncrunner.h"
   sollten vorher includiert werden.

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
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/synccond.h
    Header file <SyncCondition>.

   file: C-kern/task/synccond.c
    Implementation file <SyncCondition impl>.
*/
#ifndef CKERN_TASK_SYNCCOND_HEADER
#define CKERN_TASK_SYNCCOND_HEADER

#include "C-kern/api/task/synclink.h"

// forward
struct syncfunc_t;
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
   synclink_t  waitfunc;
};

// group: lifetime

/* define: synccond_FREE
 * Static initializer. */
#define synccond_FREE \
         { synclink_FREE }

/* function: init_synccond
 * Setze scond auf Initialwert - eine leere Warteliste. */
int init_synccond(/*out*/synccond_t * scond);

/* function: free_synccond
 * Setze scond auf <synccond_FREE>.
 * Falls noch eine wartende <syncfunc_t> verlinkt ist,
 * sollte vor dieser Funktion <wakeupall_synccond> aufgerufen werden.
 * Ansonsten gibt es einen verwaisten link. */
int free_synccond(synccond_t * scond);

// group: query

/* function: iswaiting_synccond
 * Gib true zurück, falls mindestens eine Funktion wartet, sonst false. */
int iswaiting_synccond(const synccond_t * scond);

/* function: iswaiting_synccond
 * Gib die wartende <syncfunc_t> zurück.
 *
 * Unchecked Precondition:
 * o iswaiting_synccond(scond) */
struct syncfunc_t * waitfunc_synccond(const synccond_t * scond);

// group: update

/* function: link_synccond
 * Verlinkt eine wartende <syncfunc_t>.
 * Es kann nur eine einzige verlinkt werden, weitere
 * <syncfunc_t> müssen mittels über das optionale Felde
 * <syncfunc_t.waitlist> verwaltet werden.
 * Siehe <syncrunner_t> für die Implementierung.
 *
 * Unchecked Precondition:
 * o ! iswaiting_synccond(scond)
 * o ! isvalid_synclink(addrwaitfor_syncfunc(sfunc)) */
void link_synccond(synccond_t * scond, struct syncfunc_t * sfunc);

/* function: unlink_synccond
 * Löscht die Verlinkung zur wartenden Funktion <syncfunc_t>. */
void unlink_synccond(synccond_t * scond);

/* function: wakeup_synccond
 * Wecke die erste wartende Funktion auf. NO-OP, wenn <iswaiting_synccond>(srun)==false.
 * Leitet Aufruf weiter an <wakeup_syncrunner>. */
int wakeup_synccond(struct synccond_t * scond, struct syncrunner_t * srun);

/* function: wakeupall_synccond
 * Wecke alle wartende Funktionen auf. NO-OP, wenn <iswaiting_synccond>(srun)==false.
 * Leitet Aufruf weiter an <wakeupall_syncrunner>. */
int wakeupall_synccond(struct synccond_t * scond, struct syncrunner_t * srun);


// section: inline implementation

/* define: free_synccond
 * Implements <synccond_t.free_synccond>. */
#define free_synccond(scond) \
         (*(scond) = (synccond_t) synccond_FREE, 0)

/* define: init_synccond
 * Implements <synccond_t.init_synccond>. */
#define init_synccond(scond) \
         (*(scond) = (synccond_t) synccond_FREE, 0)

/* define: iswaiting_synccond
 * Implements <synccond_t.iswaiting_synccond>. */
#define iswaiting_synccond(scond) \
         (isvalid_synclink(&(scond)->waitfunc))

/* define: link_synccond
 * Implements <synccond_t.link_synccond>. */
#define link_synccond(scond, sfunc) \
         init_synclink(&(scond)->waitfunc, addrwaitfor_syncfunc(sfunc))

/* define: unlink_synccond
 * Implements <synccond_t.unlink_synccond>. */
#define unlink_synccond(scond) \
         free_synclink(&(scond)->waitfunc)

/* define: waitfunc_synccond
 * Implements <synccond_t.waitfunc_synccond>. */
#define waitfunc_synccond(scond) \
         (waitforcast_syncfunc((scond)->waitfunc.link))

/* define: wakeup_synccond
 * Implements <synccond_t.wakeup_synccond>. */
#define wakeup_synccond(scond, srun) \
         wakeup_syncrunner(srun, scond)

/* define: wakeupall_synccond
 * Implements <synccond_t.wakeupall_synccond>. */
#define wakeupall_synccond(scond, srun) \
         wakeupall_syncrunner(srun, scond)

#endif
