/* title: IOThread

   _SHARED_

   Beschreibt Thread, der im Hintergrund eine <iolist_t> –
   also eine Liste von I/O Operationen ~ abarbeitet.
   Siehe dazu auch <iotask_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/io/subsys/iothread.h
    Header file <IOThread>.

   file: C-kern/io/subsys/iothread.c
    Implementation file <IOThread impl>.
*/
#ifndef CKERN_IO_SUBSYS_IOTHREAD_HEADER
#define CKERN_IO_SUBSYS_IOTHREAD_HEADER

#include "C-kern/api/io/subsys/iolist.h"

// forward
struct thread_t;

/* typedef: struct iothread_t
 * Export <iothread_t> into global namespace. */
typedef struct iothread_t iothread_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_subsys_iothread
 * Test <iothread_t> functionality. */
int unittest_io_subsys_iothread(void);
#endif


/* struct: iothread_t
 * Thread, der im Hintergrund I/O Operationen ausführt.
 * Dazu bearbeitet er alle I/O Aufträge vom Typ <iotask_t>,
 * die in seiner <iolist_t> gespeichert sind.
 * Ist die Liste vollständig abgearbeitet, legt sich der I/O Thread schlafen,
 * bis neue Aufträge eintreffen. Neue Aufträge werden von anderen Threads
 * an die Liste angefügt mittels <insertiotask_iothread>.
 *
 * _SHARED_(process, 1R, nW):
 * Siehe auch <iolist_t>.
 * Der <iothread_t> entfernt Einträge aus einer internen <iolist_t> und bearbeitet diese.
 * Threads, die I/O Operationen ausführen wollen, verwenden das I/O
 * Subsystem und erzeugen eine oder mehrere Einträge in einer <iolist_t>.
 * Für jedes I/O-Device gibt es in der Regel einen zuständigen <iothread_t>, der
 * diese Liste abarbeitet.
 *
 * Writer:
 * Darf neue Elemente vom Typ <iotask_t> einfügen mit <insertiotask_iothread>
 * oder den Thread stoppen mit <requeststop_iothread>.
 *
 * Reader:
 * Der im Hintergrund laufende <iothread_t> liest Elemente aus der Liste
 * und bearbeitet diese dann.
 *
 * */
struct iothread_t {
   struct thread_t* thread;
   uint8_t  request_stop;
   iolist_t iolist;
};

// group: lifetime

/* define: iothread_FREE
 * Statischer Initialisierer. */
#define iothread_FREE \
         { 0, 0, iolist_INIT }

/* function: init_iothread
 * Initialisiert iothr. Dazu wird ein <thread_t>
 * gestartet, der die in iothr verwaltete I/O Liste
 * vom Typ <iolist_t> abarbeitet. */
int init_iothread(/*out*/iothread_t* iothr);

/* function: free_iothread
 * Stoppt den laufenden Thread und gibt alle Ressourcen frei.
 *
 * Alle noch nicht bearbeiteten <iotask_t> werden storniert - siehe <iolist_t.cancelall_iolist>.
 *
 * Diese Funktion darf nur dann aufgerufen werden, wenn garantiert ist, daß kein
 * anderer Thread mehr Zugriff auf iothr und dessen zugehörige iolist hat. */
int free_iothread(iothread_t* iothr);

// group: update

/* function: insertiotask_iothread
 * Fügt iot am Ende einer internen Liste von iothr ein.
 * Der Miteigentumsrecht an iot wechselt temporär zu iothr, nur solange,
 * bis iot bearbeitet wurde. Dann liegt es automatisch wieder beim Aufrufer.
 *
 * Solange iot noch nicht bearbeitet wurde, darf das Objekt nicht gelöscht werden.
 * Nachdem alle Einträge bearbeitet wurden (<iotask_t.state>), wird der Besitz implizit
 * wieder an den Aufrufer transferiert, wobei der Eintrag mittels <iotask_t.owner_next>
 * immer in der Eigentumsliste des Owners verbleibt, auch während der Bearbeitung
 * durch iolist.
 *
 * Unchecked Precondition:
 * - forall (int t = 0; t < nrtask; ++t)
 *      iot[t]->iolist_next == 0
 * */
void insertiotask_iothread(iothread_t* iothr, uint8_t nrtask, /*own*/iotask_t* iot[nrtask]);

/* function: requeststop_iothread
 * Stoppt den laufenden Thread. Der aktuell in Bearbeitung befindliche
 * <iotask_t> wird beendet und alle weiteren noch nicht ausgeführten
 * werden storniert (<iostate_CANCELED>).
 *
 * Der Thread kann nicht wieder gestart werden. Diese Funktion wird
 * von <free_iothread> automatisch aufgerufen. */
void requeststop_iothread(iothread_t* iothr);



// section: inline implementation

// group: iothread_t

/* define: insertiotask_iothread
 * Implementiert <iothread_t.insertiotask_iothread>. */
#define insertiotask_iothread(iothr, nrtask, iot) \
         do {                                         \
            iothread_t* _t = iothr;                   \
            insertlast_iolist(                        \
               &_t->iolist, nrtask, iot, _t->thread); \
         } while(0)

#endif
