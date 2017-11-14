/* title: SyncRunner

   Verwaltet ein Menge von <syncfunc_t> in
   einer Reihe von Run- und Wait-Queues.

   Jeder Thread verwendet seinen eigenen <syncrunner_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncrunner.h
    Header file <SyncRunner>.

   file: C-kern/task/syncrunner.c
    Implementation file <SyncRunner impl>.
*/
#ifndef CKERN_TASK_SYNCRUNNER_HEADER
#define CKERN_TASK_SYNCRUNNER_HEADER

#include "C-kern/api/task/syncfunc.h"

// forward
struct perftest_info_t;

// === exported types
struct syncrunner_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncrunner
 * Test <syncrunner_t> functionality. */
int unittest_task_syncrunner(void);
#endif

#ifdef KONFIG_PERFTEST
/* function: perftest_task_syncrunner
 * Test <syncrunner_t> call performance. */
int perftest_task_syncrunner(/*out*/struct perftest_info_t* info);

/* function: perftest_task_syncrunner_raw
 * Test raw call performance without <syncrunner_t>. */
int perftest_task_syncrunner_raw(/*out*/struct perftest_info_t* info);
#endif

/* struct: syncrunner_page_t
 * Eine Speicherseite der Run- oder Waitqueue. Siehe auch <syncrunner_queue_t>. */
typedef struct syncrunner_page_t {
   /* variable: otherpages
    * Verweist auf sich selbst oder auf zusätzliche Seiten in der Queue.
    * Das Feld otherpages.next der ersten Speicherseite verweist auf die nächste,
    * also zweite Seite der Queue. */
   linkd_t     otherpages;
   /* variable: sfunc
    * Ein Array von <syncfunc_t>, das auf einer Speicherseite der Queue gespeichert
    * wird. Eine Speicherseite umfasst exakt 4096 Bytes. */
   syncfunc_t  sfunc[(4096 -sizeof(linkd_t)) / sizeof(syncfunc_t)];
} syncrunner_page_t;


/* struct: syncrunner_queue_t
 * Verwaltet eine Reihe von <syncfunc_t>.
 * Die Queue ist für die Speicherverwaltung zuständig.
 *
 * */
typedef struct syncrunner_queue_t {
   /* variable: first
    * Verwaltet Blockweise eine Liste von <syncfunc_t>. */
   syncrunner_page_t *first;
   /* variable: firstfree
    * Zeigt auf nächste freie Seite. */
   syncrunner_page_t *firstfree;
   /* variable: freelist
    * Kopf der Liste freier Einträge. Diese Liste verkettet Einträge innerhalb
    * der Seiten von der ersten bis zur Seite firstfree. */
   linkd_t  freelist;
   /* variable: freelist_size
    * Anzahl freier Einträge, die über freelist referenziert werden. */
   size_t   freelist_size;
   /* variable: size
    * Anzahl allokierter syncfunc_t aller allokierten Seiten. */
   size_t   size;
   /* variable: nextfree
    * Anzahl benutzter Einträge der Seite firstfree. */
   size_t   nextfree;
   /* variable: nrfree
    * Anzahl freier Einträge auf den Seiten firstfree bis zur letzten Seite. */
   size_t   nrfree;
} syncrunner_queue_t;

// group: lifetime

/* define: syncrunner_queue_FREE
 * Static initializer. */
#define syncrunner_queue_FREE { 0, 0, linkd_FREE, 0, 0, 0, 0 }


/* struct: syncrunner_t
 * Verwaltet ein Menge von <syncfunc_t> in
 * einer Reihe von Run- und Wait-Queues.
 *
 * Jeder Thread verwendet seinen eigenen <syncrunner_t>.
 *
 * Alle verwalteten <syncfunc_t> werden der Reihe nach ausgeführt,
 * aber niemals gleichzeitig. Wenn eine ausgeführte Funktion die Rechenzeit
 * nicht abgibt, werden alle anderen nicht oder verzögert ausgeführt.
 *
 * Synchronisation innerhalb eines Threads:
 * Locking zum Schutz vor gleichzeitigem Zugriff innerhalb der Menge der
 * Funktionen eines <syncrunner_t> ist nicht erforderlich.
 * Synchronisation zwischen einzelnen Funktionen kann mittels Nachrichtenqueues
 * gelöst werden.
 *
 * Synchronisation zwischen Threads:
 * Die Verwendung von blockierendem Locking zum Zugriff auf Ressourcen,
 * die von mehreren Threads verwendet werden, ist wegen der Unbestimmheit
 * der Wartezeit eine schlechte Idee. Besser ist, auch zwischen Threads
 * Nachrichtenqueues einzusetzen, die – etwa mittels Spinlocks – eine geringe
 * Wartezeit garantieren.
 *
 * ! Achtung !
 * Ein einmal allokierter <syncrunner_t> kann nicht per memcpy im Speicher verschoben werden,
 * da <queue_t> die Funktion <queue_t.initmove_queue> definiert.
 * */
typedef struct syncrunner_t {
   /* variable: wakeup
    * Verlinkt Einträge in <sq>[1]. */
   linkd_t              wakeup;
   /* variable: sq
    * Umfasst die RUN- und WAIT-Queue, in denen <syncfunc_t> gespeichert werden. */
   syncrunner_queue_t   sq[2];
   /* variable: isrun
    * Falls true, wird <run_syncrunner> bzw. <terminate_syncrunner> ausgeführt. */
   bool                 isrun;
   /* variable: isterminate
    * Falls true, wird <terminate_syncrunner> ausgeführt und das Hinzufügen
    * neuer Funktionen wird daher abgewiesen. */
   bool                 isterminate;
} syncrunner_t;

// group: lifetime

/* define: syncrunner_FREE
 * Static initializer. */
#define syncrunner_FREE \
         {  linkd_FREE, { syncrunner_queue_FREE, syncrunner_queue_FREE }, false, false }

/* function: init_syncrunner
 * Initialisiere srun, insbesondere die Warte- und Run-Queues. */
int init_syncrunner(/*out*/syncrunner_t *srun);

/* function: free_syncrunner
 * Gib Speicher frei, insbesondere den Speicher der Warte- und Run-Queues.
 * Die Ressourcen noch auszuführender <syncfunc_t> und wartender <syncfunc_t>
 * werden nicht freigegeben! Falls dies erforderlich ist, bitte vorher
 * <terminate_syncrunner> aufrufen. */
int free_syncrunner(syncrunner_t *srun);

// group: query

/* function: size_syncrunner
 * Liefere Anzahl wartender und auszuführender <syncfunc_t>. */
size_t size_syncrunner(const syncrunner_t *srun);

/* function: iswakeup_syncrunner
 * Liefert true, falls Funktionen aufgeweckt aber noch nicht ausgeführt wurden.
 * Funktionen werden mittels <wakeup_syncrunner> oder <wakeupall_syncrunner> aufgeweckt,
 * sie warten mit der Ausführung aber auf den nächsten Aufruf von <run_syncrunner>. */
bool iswakeup_syncrunner(const syncrunner_t *srun);

// group: update

/* function: addfunc_syncrunner
 * Erzeugt neue <syncfunc_t> und fügt diese in die Runqueue ein.
 * Auf die Beendigung einer solchen Funktion kann nicht gewartet werden.
 *
 * Parameter:
 * mainfct - Auszuführende Funktion. Wird mit jedem Aurfuf von <run_syncrunner>
 *           genau einmal ausgeführt.
 * state   - Zustand, welcher mainfct mit Hilfe des ersten Parameter
 *           (siehe <syncfunc_param_t.state> übergeben wird.
 *
 * Returns:
 * 0      - OK
 * EINVAL - Der Parameter mainfct hat den Wert 0.
 * EAGAIN - Die Funktion <terminate_syncrunner> wird gerade ausgeführt und neue Funktionen
 *          dürfen daher nicht mehr erzeugt werden.
 */
int addfunc_syncrunner(syncrunner_t *srun, syncfunc_f mainfct, void* state);

/* function: wakeup_syncrunner
 * Wecke die erste wartende <syncfunc_t> auf.
 * Falls <iswaiting_syncwait>(scond)==false, wird nichts getan.
 * Return EINVAL, falls scond zu einem anderen srun gehört.
 * Die aufgeweckte Funktion wird in <syncrunner_t.wakeup> eingefügt
 * und beim nächsten Aufruf von <run_syncrunner> wieder mit ausgeführt. */
int wakeup_syncrunner(syncrunner_t *srun, struct syncwait_t* scond);

/* function: wakeupall_syncrunner
 * Wecke alle auf scond wartenden <syncfunc_t> auf.
 * Falls <iswaiting_syncwait>(scond)==false, wird nichts getan.
 * Return EINVAL, falls scond zu einem anderen srun gehört.
 * Die aufgeweckten Funktionen werden in <syncrunner_t.wakeup> eingefügt
 * und beim nächsten Aufruf von <run_syncrunner> wieder mit ausgeführt. */
int wakeupall_syncrunner(syncrunner_t *srun, struct syncwait_t* scond);

// group: execute

/* function: run_syncrunner
 * Führt alle gespeicherten <syncfunc_t> genau einmal aus.
 * <syncfunc_t>, die auf eine Bedingung warten (<wait_syncfunc>)
 * oder auf die Beendigung einer anderen Funktion (<waitexit_syncfunc>),
 * stehen in einer Warteschlange und werden nicht ausgeführt. Erst mit
 * der Erfüllung der Wartebedingung werden sie wieder ausgeführt.
 *
 * Am Ende werden alle während der Ausführung aufgeweckten Funktionen
 * (siehe <wakeup_syncrunner>, <wakeupall_syncrunner>) einmal ausgeführt. */
int run_syncrunner(syncrunner_t *srun);

/* function: terminate_syncrunner
 * Führt alle Funktionen, auch die wartenden, genau einmal aus.
 * Als Kommando wird <synccmd_EXIT> übergeben, was so viel wie
 * alle Ressourcen freizugeben bedeutet. Danach werden alle
 * Funktionen gelöscht und aller Speicher freigegeben. */
int terminate_syncrunner(syncrunner_t *srun);



// section: inline implementation

// group: syncrunner_t

#if !defined(KONFIG_SUBSYS_SYNCRUNNER)

/* define: init_syncrunner
 * ! defined(KONFIG_SUBSYS_SYNCRUNNER) ==> Implementiert <syncrunner_t.init_syncrunner> als No-Op. */
#define init_syncrunner(srun) \
         ((void) srun, 0)

/* define: free_syncrunner
 * ! defined(KONFIG_SUBSYS_SYNCRUNNER) ==> Implementiert <syncrunner_t.free_syncrunner> als No-Op. */
#define free_syncrunner(srun) \
         ((void) srun, 0)

#endif

#endif
