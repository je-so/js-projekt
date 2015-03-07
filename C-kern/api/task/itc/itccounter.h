/* title: ITCCounter

   _SHARED_

   Einfacher Ereigniszähler für Thread-zu-Thread-Synchronisation
   (*I*nter-*T*hread-*C*ommunication).

   TODO: replace sys_iochannel_t used for waiting in poll
         with thread - event mechanism - in itccounter_t
         The event mechanism should use one I/O event thread
         which waits for events on sys_iochannel_t
         It should also support software queues, itccounter_t
         and other event generating sources.
         So that a thread could wait for any event with a single call.
         task/syncfunc.h should also be supported so that many state
         machines (implemented as syncfunc_t) could be processes by
         a single thread.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/task/itc/itccounter.h
    Header file <ITCCounter>.

   file: C-kern/platform/Linux/sync/itccounter.c
    Implementation file <ITCCounter impl>.
*/
#ifndef CKERN_TASK_ITC_ITCCOUNTER_HEADER
#define CKERN_TASK_ITC_ITCCOUNTER_HEADER

/* typedef: struct itccounter_t
 * Export <itccounter_t> into global namespace. */
typedef struct itccounter_t itccounter_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_itc_itccounter
 * Test <itccounter_t> functionality. */
int unittest_task_itc_itccounter(void);
#endif


/* struct: itccounter_t
 * Ereigniszähler zum Benachrichtigen eines wartenden Threads.
 * Es können maximal UINT32_MAX Ereignisse gezählt werden.
 * Das Lesen des Counters setzt diesen zurück auf 0.
 * Bei Erreichen des Maximalwertes bleiben alle weiterhin
 * gemeldeten Ereignisse ungezählt, der Maximalwert bleibt
 * erhalten, ein Überlauf wird damit verhindert.
 *
 * _SHARED_(process, 1R, nW):
*/
struct itccounter_t {
   // group: private fields
   /* variable: sysio
    * Der vom System bereitgestellte I/O Mechanismus, der zum Warten dient. */
   sys_iochannel_t sysio;
   /* variable: count
    * Die Anzahl gezählter Ereignisse, die seit dem letzten Lesen gezählt wurden. */
   uint32_t        count;
};

// group: lifetime

/* define: itccounter_FREE
 * Static initializer. */
#define itccounter_FREE \
         { sys_iochannel_FREE, 0 }

/* function: init_itccounter
 * Initialisiert counter auf 0 und allokiert I/O Kanal der zjum Warten benutzt werden kann. */
int init_itccounter(/*out*/itccounter_t* counter);

/* function: free_itccounter
 * Schließt den dahinterliegenden I/O Kanal.
 *
 * Unchecked Precondition:
 * - No waiting reader, <io_itccounter> is not used anymore.
 * - No writer thread could access this object. */
int free_itccounter(itccounter_t* counter);

// group: query

/* function: isfree_itccounter
 * Gibt true zurück, wenn counter nicht initialisiert ist.
 * Er also entweder auf <itccounter_FREE> gesetzt oder zuletzt mit
 * <free_itccounter> freigegeben wurde. */
int isfree_itccounter(const itccounter_t* counter);

// group: writer

/* function: increment_itccounter
 * Inkrementiert den Counter um 1 und gibt den vorherigen Wert zurück.
 * Wird UINT32_MAX zurückgegeben, dann wäre der Zähler übergelaufen
 * und deshalb wurde der Inkrement ignoriert. */
uint32_t increment_itccounter(itccounter_t* counter);

/* function: add_itccounter
 * Inkrementiert den Counter um incr und gibt den vorherigen Wert zurück.
 * Wird ein Wert r größer UINT32_MAX-incr zurückgegeben, dann ist counter
 * nur bis UINT32_MAX erhöht worden und ein Teil von incr wurde ignoriert,
 * damit der Counter nicht überläuft. */
uint32_t add_itccounter(itccounter_t* counter, uint16_t incr);

// group: reader

/* function: io_itccounter
 * Gibt I/O Kanal zurück, mit dem auf Events gewartet werden kann.
 * Ist der I/O Kanal lesbar markiert (selct/poll), dann gibt es mindestens ein gezähltes Ereignis. */
sys_iochannel_t/*readable=>event(s) available*/ io_itccounter(const itccounter_t* counter);

/* function: wait_itccounter
 * Warte maximal msec_timeout Millisekunden, bis ein Ereignis eingetreten ist.
 * Ein Timeoutwert von -1 wartet unendlich lange (msec_timeout==-1).
 *
 * Return:
 * 0     - Ereignis ist eingetreten.
 * ETIME - Timeout von msec_timeout Millisekunden ist abgelaufen, ohne daß ein Ereignis eingetreten ist.
 *
 * Unchecked Precondition:
 * - Called from single reader. */
int wait_itccounter(itccounter_t* counter, const int32_t msec_timeout/*<0: infinite timeout*/);

/* function: reset_itccounter
 * Gibt den aktuellen Zählerstand zurück und setzt ihn zurück auf 0.
 *
 * Falls 0 zurückgegeben wird, ist kein Ereignis seit dem letzten
 * Aufruf von <read_itccounter> aufgetreten.
 *
 * Falls UINT32_MAX zurückgegeben wird, ist ein Überlauf passiert
 * und der Aufrufer muss anderweitig die Anzahl der Ereignisse ermitteln.
 *
 * Unchecked Precondition:
 * - Called from single reader. */
uint32_t/*value*/ reset_itccounter(itccounter_t* counter/*is reset to 0 before return*/);


// section: inline implementation

/* define: io_itccounter
 * Implements <itccounter_t.io_itccounter>. */
#define io_itccounter(counter) \
         ((counter)->sysio)

/* define: isfree_itccounter
 * Implements <itccounter_t.isfree_itccounter>. */
#define isfree_itccounter(counter) \
         (sys_iochannel_FREE == io_itccounter(counter))


#endif
