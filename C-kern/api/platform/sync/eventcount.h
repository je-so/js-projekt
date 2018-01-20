/* title: EventCounter

   _SHARED_

   Reimplementation einer Semaphore zur einfachen Synchronisation von Threads.
   Producerthreads (Writer) erzeugen Events und Consumerthreads (Reader) warten
   auf eines. Der Evenrcounter zählt die Anzahl erzeugter und noch nicht
   konsumierter Events bis maximal INT32_MAX.

   Events können z.B. die Anzahl freier Plätze in einer Queue sein oder
   die Anzahl nicht verarbeiteter Nachrichten usw.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2017 Jörg Seebohn

   file: C-kern/api/platform/sync/eventcount.h
    Header file <EventCounter>.

   file: C-kern/platform/Linux/sync/eventcount.c
    Implementation file <EventCounter impl>.
*/
#ifndef CKERN_PLATFORM_SYNC_EVENTCOUNT_HEADER
#define CKERN_PLATFORM_SYNC_EVENTCOUNT_HEADER

// imported types
struct slist_node_t;

// exported types
struct eventcount_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_sync_eventcount
 * Test <eventcount_t> functionality. */
int unittest_platform_sync_eventcount(void);
#endif


/* struct: eventcount_t
 * Zählt aufgetretene Events eines unbestimmten aber nur genau eines Typs.
 * Lesende Threads rufen [try]wait_eventcount auf, um auf ein Event zu warten
 * (falls nicht try) und den Zähler um eins zu dekrementieren.
 * Schreibende Threads rufen count_eventcount auf, erhöhen so den Zähler um 1
 * und wecken damit möglicherweise einen wartenden Thread auf.
 *
 * Wenn beim Aufruf von wait_eventcount kein Event vorhanden ist, wird der Aufrufer schlafen
 * gelegt. Ist der Eventzähler aber größer 0, wird er bei Aufruf von wait_eventcount
 * nur um eins dekremetiert und der aufrufende Thread kehrt sofort zurück.
 *
 * 32 Bit Counter:
 * Es dürfen niemals mehr als INT32_MAX Events gleichzeitig erzeugt
 * werden. Ebenso dürfen niemals mehr als INT32_MAX(+1) Threads auf ein Event warten.
 * Durch Architekturmaßnahmen muss diese Annahme abgesichert werden, sonst
 * bricht das Programm ab (checked Precondition).
 *
 * _SHARED_(process, nR, nW):
 *  Shared between threads in the context of one process.
 *  Readers(R) call [try]wait_eventcount and writers(W) call count_eventcount.
 */
typedef struct eventcount_t {
   /* variable: nrevents
    * The number of counted events. There are no waiting threads if number >= 0.
    * If number < 0 the number of waiting threads is (-nrevents). */
   int32_t                 nrevents;
   /* variable: last
    * Points to the last thread of a list of waiting threads. */
   struct slist_node_t  *  last;
   /* variable: lockflag
    * Lock flag used to protect access to data members.
    * Set and cleared with atomic operations. */
   uint8_t                 lockflag;
} eventcount_t;

// group: lifetime

/* define: eventcount_FREE
 * Static initializer. */
#define eventcount_FREE \
         { 0, 0, 0 }

/* define: eventcount_INIT
 * Static initializer. */
#define eventcount_INIT \
         { 0, 0, 0 }

/* function: init_eventcount
 * Initialisert counter. */
void init_eventcount(/*out*/eventcount_t* counter);

/* function: free_eventcount
 * Gibt die belegte Ressourcen frei. Wartende Threads werden alle aufgeweckt. */
int free_eventcount(eventcount_t* counter);

// group: query

/* function: isfree_eventcount
 * Gibt true zurück, wenn *counter == eventcount_FREE. */
static inline int isfree_eventcount(eventcount_t* counter);

/* function: nrwaiting_eventcount
 * Gibt Anzahl wartender Thread zurück. Ist diese Wert > 0, dann liefert
 * ein Aufruf von <nrevents_eventcount> den Wert 0. */
uint32_t nrwaiting_eventcount(eventcount_t* counter);

/* function: nrevents_eventcount
 * Gibt Anzahl gezählter aber noch nicht konsumierter Events zurück.
 * Ist diese Wert > 0, dann liefert ein Aufruf von <nrwaiting_eventcount> den Wert 0. */
uint32_t nrevents_eventcount(eventcount_t* counter);

// group: update

/* function: count_eventcount
 * Erhöht die Anzahl gezählter Events um eins.
 * Falls es wartende Threads gibt, wird der erste davon aufgeweckt.
 *
 * Checked Precondition:
 * assert(nrevents_eventcount(counter) != INT32_MAX);
 * Program is aborted if otherwise */
void count_eventcount(eventcount_t* counter);

/* function: trywait_eventcount
 * Ist die gezählte Anzahl an Events > 0, wird diese um eins verringert und der OK-Wert (0) zurückgegeben.
 * Ist die gezählte Anzahl 0, wird nichts getan und der Fehlercode (EAGAIN) zurückgegeben. */
int trywait_eventcount(eventcount_t* counter);

/* function: wait_eventcount
 * Ist die gezählte Anzahl an Events > 0, wird diese um eins verringert und der OK-Wert (0) zurückgegeben.
 * Ist die gezählte Anzahl 0, wird counter kurz gelockt und der aufrufende Thread in die Warteliste eingetragen.
 * Der aufrufende Thread kehrt erst dann zurück, wenn er der erste in der Liste wartender Threads ist
 * und von einem anderen Thread aus <count_eventcount> aufgerufen wurde.
 *
 * Checked Precondition:
 * assert(nrwaiting_eventcount(counter) != -INT32_MIN);
 * Program is aborted if otherwise */
void wait_eventcount(eventcount_t* counter);


// section: inline implementation

/* define: init_eventcount
 * Implements <eventcount_t.init_eventcount>. */
#define init_eventcount(counter) \
         ((void)(*(counter)=(eventcount_t)eventcount_INIT))

static inline int isfree_eventcount(eventcount_t* counter)
{
   return 0 == counter->nrevents && 0 == counter->last && 0 == counter->lockflag;
}


#endif
