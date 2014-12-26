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

#include "C-kern/api/task/synccmd.h"
#include "C-kern/api/task/syncfunc.h"
#include "C-kern/api/task/syncqueue.h"

// forward
struct syncfunc_t;

/* typedef: struct syncrunner_t
 * Export <syncrunner_t> into global namespace. */
typedef struct syncrunner_t syncrunner_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncrunner
 * Test <syncrunner_t> functionality. */
int unittest_task_syncrunner(void);
#endif


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
 * */
struct syncrunner_t {
   /* variable: wakeup
    * Verlinkt Einträge in <waitqueue>. Die Felder <syncfunc.waitresult> und <syncfunc.waitlist> sind vorhanden. */
   linkd_t        wakeup;
   /* variable: rwqueue
    * (Run-Wait-Queues) Speichert ausführbare und wartende <syncfunc_t> verschiedener Bytegrößen.
    * Die Größe in Bytes einer syncfunc_t bestimmt sich aus dem Vorhandensein optionaler Felder. */
   syncqueue_t    rwqueue[1+1];
   /* variable: isrun
    * Falls true, wird <run_syncrunner> bzw. <teminate_syncrunner> ausgeführt. */
   bool           isrun;
};
// TODO: add iterator to syncqueue_t + ... !!
// TODO: remove every cast from syncqueue_t to queue_t !!!!!

// group: lifetime

/* define: syncrunner_FREE
 * Static initializer. */
#define syncrunner_FREE \
         {  linkd_FREE, { syncqueue_FREE, syncqueue_FREE }, false }

/* function: init_syncrunner
 * Initialisiere srun, insbesondere die Warte- und Run-Queues. */
int init_syncrunner(/*out*/syncrunner_t * srun);

/* function: free_syncrunner
 * Gib Speicher frei, insbesondere den SPeicher der Warte- und Run-Queues.
 * Die Ressourcen noch auszuführender <syncfunc_t> und wartender <syncfunc_t>
 * werden nicht freigegeben! Falls dies erforderlich ist, bitte vorher
 * <teminate_syncrunner> aufrufen. */
int free_syncrunner(syncrunner_t * srun);

// group: query

/* function: size_syncrunner
 * Liefere Anzahl wartender und auszuführender <syncfunc_t>. */
size_t size_syncrunner(const syncrunner_t * srun);

/* function: iswakeup_syncrunner
 * Liefert true, falls Funktionen aufgeweckt aber noch nicht ausgeführt wurden.
 * Funktionen werden mittels <wakeup_syncrunner> oder <wakeupall_syncrunner> aufgeweckt,
 * sie warten mit der Ausführung aber auf den nächsten Aufruf von <run_syncrunner>. */
bool iswakeup_syncrunner(const syncrunner_t* srun);

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
 */
int addfunc_syncrunner(syncrunner_t* srun, syncfunc_f mainfct, void* state);

/* function: wakeup_syncrunner
 * Wecke die erste wartende <syncfunc_t> auf.
 * Falls <iswaiting_synccond>(scond)==false, wird nichts getan.
 * Return EINVAL, falls scond zu einem anderen srun gehört.
 * Die aufgeweckte Funktion wird in <syncrunner_t.wakeup> eingefügt
 * und beim nächsten Aufruf von <run_syncrunner> wieder mit ausgeführt. */
int wakeup_syncrunner(syncrunner_t* srun, struct synccond_t* scond);

/* function: wakeupall_syncrunner
 * Wecke alle auf scond wartenden <syncfunc_t> auf.
 * Falls <iswaiting_synccond>(scond)==false, wird nichts getan.
 * Return EINVAL, falls scond zu einem anderen srun gehört.
 * Die aufgeweckten Funktionen werden in <syncrunner_t.wakeup> eingefügt
 * und beim nächsten Aufruf von <run_syncrunner> wieder mit ausgeführt. */
int wakeupall_syncrunner(syncrunner_t* srun, struct synccond_t* scond);

// group: execute

/* function: run_syncrunner
 * Führt alle gespeicherten <syncfunc_t> genau einmal aus.
 * <syncfunc_t>, die auf eine Bedingung warten (<wait_syncfunc>)
 * oder auf die Beendigung einer anderen Funktion (<waitexit_syncfunc>),
 * stehen in einer Warteschlange und werden nicht ausgeführt. Erst mit
 * der Erfüllung der Wartebedingung werden sie wieder ausgeführt.
 *
 * Ruft <run2_syncrunner>(srun,true) auf. */
int run_syncrunner(syncrunner_t* srun);

/* function: run2_syncrunner
 * Führt alle gespeicherten <syncfunc_t> genau einmal aus.
 * <syncfunc_t>, die auf eine Bedingung warten (<wait_syncfunc>)
 * oder auf die Beendigung einer anderen Funktion (<waitexit_syncfunc>),
 * stehen in einer Warteschlange und werden nicht ausgeführt. Erst mit
 * der Erfüllung der Wartebedingung werden sie wieder ausgeführt.
 *
 * Wenn Parameter runwakeup auf false gesetzt ist, werden gerade aufgeweckte
 * Funktionen (<wakeup_syncrunner>, <wakeupall_syncrunner>) nicht mit
 * ausgeführt, sondern verbleiben weiterhin in der Aufweckliste (<syncrunner_t.wakeup>). */
int run2_syncrunner(syncrunner_t * srun, bool runwakeup);

/* function: terminate_syncrunner
 * Führt alle Funktionen, auch die wartenden, genau einmal aus.
 * Als Kommando wird <synccmd_EXIT> übergeben, was so viel wie
 * alle Ressourcen freizugeben bedeutet. Danach werden alle
 * Funktionen gelöscht und aller Speicher freigegeben. */
int terminate_syncrunner(syncrunner_t * srun);



// section: inline implementation

// group: syncrunner_t

#if !defined(KONFIG_SUBSYS_SYNCRUNNER)

/* define: init_syncrunner
 * ! defined(KONFIG_SUBSYS_SYNCRUNNER) ==> Implementiert <syncrunner_t.init_syncrunner> als No-Op. */
#define init_syncrunner(srun) \
         (0)

/* define: free_syncrunner
 * ! defined(KONFIG_SUBSYS_SYNCRUNNER) ==> Implementiert <syncrunner_t.free_syncrunner> als No-Op. */
#define free_syncrunner(srun) \
         (0)

#endif

/* define: run_syncrunner
 * Implementiert <syncrunner_t.run_syncrunner>. */
#define run_syncrunner(srun) \
         (run2_syncrunner((srun), true))

#endif
