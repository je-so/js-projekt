/* title: SyncWaitlist

   Beschreibt eine Warteliste.
   Arbeitet eng mit <syncrunner_t> zusammen.

   Includes:
   Dateien "C-kern/api/task/syncfunc.h" und "C-kern/api/task/syncrunner.h"
   sollten vorher includiert werden.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncwait.h
    Header file <SyncWaitlist>.

   file: C-kern/task/syncwait.c
    Implementation file <SyncWaitlist impl>.
*/
#ifndef CKERN_TASK_SYNCWAIT_HEADER
#define CKERN_TASK_SYNCWAIT_HEADER

#include "C-kern/api/ds/link.h"

// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncwait
 * Test <syncwait_t> functionality. */
int unittest_task_syncwait(void);
#endif


/* struct: syncwait_t
 * Implementiert eine Wartebedingung.
 * Das Objekt zeigt auf den Wartelisteknoten der ersten wartenden <syncfunc_t> und
 * überlässt die Implementierung der Speicherverwaltung und
 * einer ganzen Liste von wartenden Funktionen dem <syncrunner_t>.
 * Nur ein einziger <syncrunner_t> (ein Thread) wird unterstützt. Sollte ein <syncwait_t>
 * von mehreren <syncrunner_t> gleichzeitig benutzt werden,
 * ist das Verhalten undefiniert/fehlerhaft. */
typedef struct syncwait_t {
   linkd_t funclist;
} syncwait_t;

// group: lifetime

/* define: syncwait_FREE
 * Static initializer. */
#define syncwait_FREE \
         { linkd_FREE }

/* function: init_syncwait
 * Setze swait auf Initialwert - eine leere Warteliste. */
void init_syncwait(/*out*/syncwait_t* swait);

/* function: free_syncwait
 * Setze swait auf <syncwait_FREE>.
 * Falls noch eine wartende <syncfunc_t> verlinkt ist,
 * sollte vor dieser Funktion <removelist_syncwait> aufgerufen werden.
 * Ansonsten gibt es einen verwaisten link. */
void free_syncwait(syncwait_t* swait);

// group: query

/* function: iswaiting_syncwait
 * Gib true zurück, falls mindestens eine Funktion wartet, sonst false. */
static inline int iswaiting_syncwait(const syncwait_t* swait);

/* function: getfirst_syncwait
 * Gib die erste wartende <syncfunc_t> zurück.
 *
 * Unchecked Precondition:
 * o iswaiting_syncwait(swait) */
linkd_t* getfirst_syncwait(const syncwait_t* swait);

// group: update

/* function: addnode_syncwait
 * Fügt einen neuen Knoten sfunc ans Ende der Warteliste.
 * Siehe <syncrunner_t> für die Verwendung dieser Funktion.
 * */
static inline void addnode_syncwait(syncwait_t* swait, linkd_t* sfunc);

/* function: removenode_syncwait
 * Entfernt die erste wartende Funktion <getfirst_syncwait> aus der Warteliste.
 *
 * Unchecked Precondition:
 * o iswaiting_syncwait(swait) */
static inline linkd_t* removenode_syncwait(syncwait_t* swait);

/* function: removelist_syncwait
 * Entfernt alle wartenden Funktionen und gibt diese als verkette Liste zurück.
 *
 * Unchecked Precondition:
 * o iswaiting_syncwait(swait) */
static inline linkd_t* removelist_syncwait(syncwait_t* swait);


// section: inline implementation

/* define: free_syncwait
 * Implements <syncwait_t.free_syncwait>. */
#define free_syncwait(swait) \
         ((void)(*(swait) = (syncwait_t) syncwait_FREE))

/* define: init_syncwait
 * Implements <syncwait_t.init_syncwait>. */
#define init_syncwait(swait) \
         initself_linkd(&(swait)->funclist)

/* define: iswaiting_syncwait
 * Implements <syncwait_t.iswaiting_syncwait>. */
static inline int iswaiting_syncwait(const syncwait_t* swait)
{
         return ! isself_linkd(&swait->funclist) && isvalid_linkd(&swait->funclist);
}

/* define: addnode_syncwait
 * Implements <syncwait_t.addnode_syncwait>. */
static inline void addnode_syncwait(syncwait_t* swait, linkd_t* sfunc)
{
         initprev_linkd(sfunc, &swait->funclist);
}

/* define: getfirst_syncwait
 * Implements <syncwait_t.getfirst_syncwait>. */
#define getfirst_syncwait(swait) \
         ((swait)->funclist.next)

/* define: removenode_syncwait
 * Implements <syncwait_t.removenode_syncwait>. */
static inline linkd_t* removenode_syncwait(syncwait_t* swait)
{
         linkd_t * waitnode = getfirst_syncwait(swait);
         unlink_linkd(waitnode);
         return waitnode;
}

/* define: removelist_syncwait
 * Implements <syncwait_t.removelist_syncwait>. */
static inline linkd_t* removelist_syncwait(syncwait_t* swait)
{
            linkd_t *waitlist = getfirst_syncwait(swait);
            unlink_linkd(&swait->funclist);
            init_syncwait(swait);
            return waitlist;
}

#endif
