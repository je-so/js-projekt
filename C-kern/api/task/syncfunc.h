/* title: SyncFunc

   Definiert einen Ausführungskontext (execution context)
   für Funktionen der Signatur <syncfunc_f>.

   Im einfachsten Fall ist <syncfunc_t> ein einziger
   Funktionszeiger <syncfunc_f>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncfunc.h
    Header file <SyncFunc>.

   file: C-kern/task/syncfunc.c
    Implementation file <SyncFunc impl>.
*/
#ifndef CKERN_TASK_SYNCFUNC_HEADER
#define CKERN_TASK_SYNCFUNC_HEADER

#include "C-kern/api/ds/link.h"

// forward
struct syncrunner_t;
struct synccond_t;

/* typedef: struct syncfunc_t
 * Export <syncfunc_t> into global namespace. */
typedef struct syncfunc_t syncfunc_t;

/* typedef: struct syncfunc_param_t
 * Export <syncfunc_param_t> into global namespace. */
typedef struct syncfunc_param_t syncfunc_param_t;

/* typedef: syncfunc_f
 * Definiert Signatur einer wiederholt auszuführenden Funktion.
 * Diese Funktionen werden *nicht* präemptiv unterbrochen und basieren
 * auf Kooperation mit anderen Funktionen.
 *
 * Parameter:
 * sfparam  - Ein- Ausgabe Parameter.
 *            Einige Eingabe (in) Felder sind nur bei einem bestimmten Wert in sfcmd gültig.
 *            Siehe <syncfunc_param_t> für eine ausführliche Beschreibung.
 * sfcmd    - Ein das auszuführenede Kommando beschreibende Wert aus <synccmd_e>.
 *
 * Return:
 * Der Rückgabewert ist das vom Aufrufer auszuführende Kommando – auch aus <synccmd_e>. */
typedef int (* syncfunc_f) (syncfunc_param_t * sfparam, uint32_t sfcmd);

/* enums: syncfunc_opt_e
 * Bitfeld das die vorhandenen Felder in <syncfunc_t> kodiert.
 *
 * syncfunc_opt_NONE      - Weder <syncfunc_t.state> noch <syncfunc_t.contlabel> sind vorhanden.
 * syncfunc_opt_WAITFOR_CALLED    - Das Feld <syncfunc_t.waitfor> ist vorhanden und zeigt auf einen <syncfunc_t.caller>.
 * syncfunc_opt_WAITFOR_CONDITION - Das Feld <syncfunc_t.waitfor> ist vorhanden und zeigt auf einen <synccond_t.waitfunc>.
 * syncfunc_opt_WAITFOR_MASK - Maskiert die Bits für <syncfunc_opt_WAITFOR_CALLED> und <syncfunc_opt_WAITFOR_CONDITION>.
 *                             Entweder sind alle Bits in <syncfunc_opt_WAITFOR_MASK> 0 oder gleich
 *                             <syncfunc_opt_WAITFOR_CALLED> bzw. gleich <syncfunc_opt_WAITFOR_CONDITION>.
 * syncfunc_opt_WAITRESULT - Ist diese Bit gesetzt, dann ist nicht <syncfunc_t.waitfor>, sondern <syncfunc_t.waitresult> gültig.
 *                           Ist nur gültig, wenn entweder <syncfunc_opt_WAITFOR_CALLED> oder <syncfunc_opt_WAITFOR_CONDITION>
 *                           gesetzt ist.
 * syncfunc_opt_WAITLIST  - Das Feld <syncfunc_t.waitlist> ist vorhanden.
 * syncfunc_opt_CALLER    - Das Feld <syncfunc_t.caller> ist vorhanden.
 * syncfunc_opt_STATE     - Das Feld <syncfunc_t.state> ist vorhanden.
 * syncfunc_opt_ALL       - Alle Felder sind vorhanden, wobei <syncfunc_opt_WAITFOR_CONDITION> gültig ist und
 *                          nicht <syncfunc_opt_WAITFOR_CALLER>. */
enum syncfunc_opt_e {
   syncfunc_opt_NONE              = 0,
   syncfunc_opt_WAITFOR_CALLED    = 1,
   syncfunc_opt_WAITFOR_CONDITION = 2,
   syncfunc_opt_WAITFOR           = syncfunc_opt_WAITFOR_CALLED + syncfunc_opt_WAITFOR_CONDITION,
   syncfunc_opt_WAITRESULT        = 4,
   syncfunc_opt_WAITLIST          = 8,
   syncfunc_opt_CALLER            = 16,
   syncfunc_opt_STATE             = 32,
   syncfunc_opt_ALL               = 63
};

typedef enum syncfunc_opt_e syncfunc_opt_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncfunc
 * Teste <syncfunc_t>. */
int unittest_task_syncfunc(void);
#endif


/* struct: syncfunc_param_t
 * Definiert Ein- Ausgabeparameter von <syncfunc_f> Funktionen. */
struct syncfunc_param_t {
   /* variable: syncrun
    * Eingabe-Param: Der Verwalter-Kontext von <syncfunc_t>. */
   struct syncrunner_t *
         const syncrun;
   /* variable: contoffset
    * Ein- Ausgabe-Param: Die Stelle, and der mit der Ausführung weitergemacht werden soll.
    * Nur gültig, wenn Parameter sfcmd den Wert <synccmd_CONTINUE> besitzt.
    * Der Wert wird nach »return« gespeichert, wenn die Funktion <synccmd_CONTINUE>
    * oder <synccmd_WAIT> zurückgibt. */
   uint16_t    contoffset;
   /* variable: state
    * Ein- Ausgabe-Param: Der gespeicherte Funktionszustand.
    * Muss von der Funktion verwaltet werden. */
   void *      state;
   /* variable: condition
    * Ausgabe-Param: Referenziert die Bedingung, auf die gewartet werden soll.
    * Der Wert wird genau dann verwendet, wenn die Funktion den Wert <synccmd_WAIT> zurückgibt. */
   struct synccond_t
             * condition;
   /* variable: waiterr
    * Eingabe-Param: Das Ergebnis der Warteoperation.
    * Eine 0 zeigt Erfolg an, != 0 ist ein Fehlercode – etwa ENOMEM wenn zu wenig Ressourcen
    * frei waren, um die Funktion in die Warteliste einzutragen.
    * Nur gültig, wenn das Kommando <synccmd_CONTINUE> ist und zuletzt die Funktion mit
    * »return <synccmd_WAIT>« beendet wurde. */
   int         waiterr;
   /* variable: retcode
    * Ein- Ausgabe-Param: Der Returnwert der Funktion.
    * Beendet sich die Funktion mit »return <synccmd_EXIT>«, dann steht in diesem Wert
    * der Erfolgswert drin (0 für erfolgreich, != 0 Fehlercode).
    * Der Eingabewert ist nur gültig, wenn das Kommando <synccmd_CONTINUE> ist, zuletzt
    * die Funktion mit »return <synccmd_WAIT>« beendet wurde, wobei <condition> 0 war.
    * Der Wert trägt dann den Erfolgswert der Funktion, auf deren Ende gewartet wurde. */
   int         retcode;
};

// group: lifetime

/* define: syncfunc_param_FREE
 * Static initializer. */
#define syncfunc_param_FREE \
         { 0, 0, 0, 0, 0, 0 }

#define syncfunc_param_INIT(syncrun) \
         { syncrun, 0, 0, 0, 0, 0 }

/* struct: syncfunc_t
 * Der Ausführungskontext einer Funktion der Signatur <syncfunc_f>.
 * Dieser wird von einem <syncrunner_t> verwaltet, der alle
 * verwalteten Funktionen nacheinander aufruft. Durch die Ausführung
 * nacheinander und den Verzicht auf präemptive Zeiteinteilung,
 * ist die Ausführung synchron. Dieses kooperative Multitasking zwischen
 * allen Funktionen eines einzigen <syncrunner_t>s erlaubt und gebietet
 * den Verzicht Locks verzichtet werden (siehe <mutex_t>). Warteoperationen
 * müssen mittels einer <synccond_t> durchgeführt werden.
 *
 * Optionale Datenfelder:
 * Die Variablen <state> und <contlabel> sind optionale Felder.
 *
 * In einfachsten Fall beschreibt <mainfct> alleine eine syncfunc_t.
 *
 * Auf 0 gesetzte Felder sind ungültig. Das Bitfeld <syncfunc_opt_e>
 * wird verwendet, um optionale Felder als vorhanden zu markieren.
 *
 * Die Grundstruktur einer Implementierung:
 * Als ersten Parameter bekommt eine <syncfunc_f> nicht <syncfunc_t>
 * sondern den Parametertyp <syncfunc_param_t>. Das erlaubt ein
 * komplexeres Protokoll mit verschiedenen Ein- und Ausgabeparametern,
 * ohne <syncfunc_t> damit zu belasten.
 *
 * > int test_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
 * > {
 * >    int err = EINTR; // Melde Unterbrochen-Fehler, falls
 * >                     // synccmd_EXIT empfangen wird
 * >    start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);
 * >    goto ONERR;
 * > ONRUN:
 * >    void * state = alloc_memory(...);
 * >    setstate_syncfunc(sfparam, state);
 * >    ...
 * >    yield_syncfunc(sfparam);
 * >    ...
 * >    synccond_t * condition = ...;
 * >    err = wait_syncfunc(sfparam, condition);
 * >    if (err) goto ONERR;
 * >    ...
 * >    exit_syncfunc(sfparam, 0);
 * > ONERR:
 * >    free_memory( state_syncfunc(sfparam) );
 * >    exit_syncfunc(sfparam, err);
 * > }
 * */
struct syncfunc_t {
   // group: private fields

   /* variable: mainfct
    * Zeiger auf immer wieder auszuführende Funktion. */
   syncfunc_f  mainfct;
   /* variable: contoffset
    * Speichert Offset von syncfunc_START (siehe <start_syncfunc>)
    * zu einer Labeladresse und erlaubt es so, die Ausführung an dieser Stelle fortzusetzen.
    * Fungiert als Register für »Instruction Pointer« bzw. »Programm Zähler«.
    * Diese GCC Erweiterung – Erläuterung auf https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html –
    * wird folgendermaßen genutzt;
    *
    * > contlabel = && LABEL;
    * > goto * contlabel;
    *
    * Mit einer weiteren GCC Erweiterung "Locally Declared Labels"
    * wird das Schreiben eines Makros zur Abgabe des Prozessors an andere Funktionen
    * zum Kinderspiel.
    *
    * > int test_syncfunc(syncfunc_param_t * sfparam, uint32_t sfcmd)
    * > {
    * >    ...
    * >    {  // yield MACRO
    * >       __label__ continue_after_wait;
    * >       sfparam->contoffset = &&continue_after_wait - &&syncfunc_START;
    * >       // yield processor to other functions
    * >       return synccmd_CONTINUE;
    * >       // execution continues here
    * >       continue_after_wait: ;
    * >    }
    * >    ...
    * > } */
   uint16_t    contoffset;
   /* variable: optfields
    * Kodiert, welche optionalen Felder vorhanden sind – siehe <syncfunc_opt_e>. */
   uint8_t     optfields;

   // == optional fields for use in wait operations ==

   union {
      /* variable: waitresult
       * *Optional*: Das Ergebnis der Warteoperation. */
      int      waitresult;
      /* variable: waitfor
       * *Optional*: Entweder ist der Link verbunden mit <syncfunc_t.caller> oder mit <synccond_t.waitfunc>. */
      link_t   waitfor;
   };
   /* variable: waitlist
    * *Optional*: Verbindet weitere wartende <syncfunc_t> in einer Liste.
    * Nur der Kopf der Liste besitzt das Feld <waitfor> alle anderen Wartenden sind mit dem Kopf
    * über waitlist verbunden. */
   linkd_t     waitlist;
   // == optional fields describing run state ==

   /* variable: caller
    * *Optional*: Zeigt auf aufrufende syncfunc_t und ist verbunden mit <syncfunc_t.waitfor>. */
   link_t      caller;
   /* variable: state
    * *Optional*: Zeigt auf gesicherten Zustand der Funktion.
    * Der Wert ist anfangs 0 (ungültig). Der Speicher wird von <mainfct> verwaltet. */
   void *      state;
};

// group: lifetime

/* define: syncfunc_FREE
 * Static initializer. */
#define syncfunc_FREE \
         { 0, 0, 0, link_FREE, linkd_FREE, link_FREE, 0 }

/* function: init_syncfunc
 * Initialisiert alle, außer den optionalen Feldern. */
static inline void init_syncfunc(/*out*/syncfunc_t * sfunc, syncfunc_f mainfct, syncfunc_opt_e optfields);

/* function: initmove_syncfunc
 * Kopiert <mainfct>, setzt <optfields>, <contoffset> und optionale Felder <state> und <caller>.
 * <mainfct> und <caller> werden von src kopiert.
 * Die Felder <contoffset>, <optfields> und <state> werden mit destcontoffset, destoptfields bzw. deststate initialisiert.
 * <state> wird nur dann gesetzt, wenn deststate != 0.
 *
 * Nach Return sind die Felder in src ungültig.
 *
 * Unitialisiert:
 * Alle anderen optionalen Felder bleiben unitialisiert.
 *
 * Unchecked Preconditon:
 * o srcsize  == getsize_syncfunc(src->optfields)
 * o destsize == getsize_syncfunc(destoptfields)
 * o isstate  == (0 != (src->optfields & syncfunc_opt_STATE))
 * o (destisstate != 0) == (0 != (destoptfields & syncfunc_opt_STATE))
 * o (destoptfields & syncfunc_opt_CALLER)
 *   == (src->optfields & syncfunc_opt_CALLER)
 * */
static inline void initmove_syncfunc(/*out*/syncfunc_t * dest, uint16_t destsize, uint16_t destcontoffset, syncfunc_opt_e destoptfields, void * deststate, syncfunc_t * src, uint16_t srcsize, int isstate);

// group: query

/* function: getsize_syncfunc
 * Liefere die aus den optionalen Feldern optfields berechnte Größe in Byte einer <syncfunc_t>.
 * Siehe auch <syncfunc_opt_e>.
 *
 * Precondition:
 * Das Argument optfields wird mehrfach ausgewertet, es muss daher ein konstanter Ausdruck
 * ohne Nebeneffekte sein. */
uint16_t getsize_syncfunc(const syncfunc_opt_e optfields);

/* function: waitforcast_syncfunc
 * Caste waitfor nach <syncfunc_t>.
 *
 * Precondition:
 * o waitfor != 0
 * o waitfor == addrwaitfor_syncfunc(...) || waitfor == addrcaller_syncfunc(...)->link */
syncfunc_t * waitforcast_syncfunc(link_t * waitfor);

/* function: waitlistcast_syncfunc
 * Caste waitlist nach <syncfunc_t>.
 *
 * Precondition:
 * o waitlist != 0
 * o waitlist == addrwaitlist_syncfunc(...)
 *   || waitfor == addrwaitlist_syncfunc(...)->next
 *   || waitfor == addrwaitlist_syncfunc(...)->prev */
syncfunc_t * waitlistcast_syncfunc(link_t * waitlist, const bool iswaitfor);

/* function: offwaitfor_syncfunc
 * Liefere den Byteoffset zum Feld <waitfor>. */
size_t offwaitfor_syncfunc(void);

/* function: offwaitlist_syncfunc
 * Liefere den Byteoffset zum Feld <waitlist>.
 * Parameter iswaitfor gibt an, ob Feld <waitfor> vorhanden ist. */
size_t offwaitlist_syncfunc(const bool iswaitfor);

/* function: offcaller_syncfunc
 * Liefere den BYteoffset zum Feld <caller>.
 * Die Parameter isstate und iscaller zeigen an, ob Felder <state> und/oder Feld <caller> vorhanden sind. */
size_t offcaller_syncfunc(size_t structsize, const bool isstate, const bool iscaller);

/* function: offstate_syncfunc
 * Liefere den Byteoffset zum Feld <state>.
 * Parameter structsize gibt die mit <getsize_syncfunc> ermittelte Größe der Struktur an
 * und isstate, ob das Feld state vorhanden ist. */
size_t offstate_syncfunc(size_t structsize, const bool isstate);

/* function: addrwaitresult_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <waitresult>.
 *
 * Precondition:
 * Feld <waitresult> ist vorhanden. */
int * addrwaitresult_syncfunc(syncfunc_t * sfunc);

/* function: addrwaitfor_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <waitfor>.
 *
 * Precondition:
 * Feld <waitfor> ist vorhanden. */
link_t * addrwaitfor_syncfunc(syncfunc_t * sfunc);

/* function: addrwaitlist_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <waitlist>.
 * Parameter iswaitfor gibt an, ob Feld <waitfor> vorhanden ist.
 *
 * Precondition:
 * Feld <waitlist> ist vorhanden. */
linkd_t * addrwaitlist_syncfunc(syncfunc_t * sfunc, const bool iswaitfor);

/* function: addrcaller_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <state>.
 * Parameter structsize gibt die mit <getsize_syncfunc> ermittelte Größe der Struktur an
 * und isstate, ob Feld <state> vorhanden ist.
 *
 * Precondition:
 * Feld <caller> ist vorhanden. */
link_t * addrcaller_syncfunc(syncfunc_t * sfunc, const size_t structsize, const bool isstate);

/* function: addrstate_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <state>.
 * Parameter structsize gibt die mit <getsize_syncfunc> ermittelte Größe der Struktur an.
 *
 * Precondition:
 * Feld <state> ist vorhanden. */
void ** addrstate_syncfunc(syncfunc_t * sfunc, const size_t structsize);

// group: update

/* function: clearopt_syncfunc
 * Löscht alle Bits in sfunc->optfields, die in optfield gesetzt sind. */
void clearopt_syncfunc(syncfunc_t * sfunc, syncfunc_opt_e optfield);

/* function: setopt_syncfunc
 * Setzt alle Bits in sfunc->optfields, die in optfield gesetzt sind. */
void setopt_syncfunc(syncfunc_t * sfunc, syncfunc_opt_e optfield);

/* function: relink_syncfunc
 * Korrigiert die Ziellinks von <waitfor>, <waitlist> und <caller>, nachdem sfunc im Speicher verschoben wurde. */
void relink_syncfunc(syncfunc_t * sfunc, const size_t structsize);

/* function: setresult_syncfunc
 * Setzt <waitresult> auf Wert result.
 *
 * Precondition:
 * Feld <waitfor> bzw. <waitresult> ist vorhanden. */
static inline void setresult_syncfunc(syncfunc_t * sfunc, int result);

/* function: unlink_syncfunc
 * Invalidiert die Ziellinks von <waitfor>, <waitlist> und <caller>.
 * Inhalte von <waitfor>, <waitlist> und <caller> sind danach ungültig, aber unverändert. */
void unlink_syncfunc(syncfunc_t * sfunc, const size_t structsize);

// group: implementation-support
// Macros which makes implementing a <syncfunc_f> possible.

/* function: state_syncfunc
 * Liest Zustand der aktuell ausgeführten Funktion.
 * Der Wert 0 ist ein ungültiger Wert und zeigt an, daß
 * der Zustand noch nicht gesetzt wurde.
 * Der Wert wird aber nicht aus <syncfunc_t> sondern
 * aus dem Parameter <syncfunc_param_t> herausgelesen.
 * Er trägt eine Kopie des Zustandes und
 * andere nützliche, über <syncfunc_t> hinausgehende Informationen. */
void * state_syncfunc(const syncfunc_param_t * sfparam);

/* function: setstate_syncfunc
 * Setzt den Zustand der gerade ausgeführten Funktion.
 * Wurde der Zustand gelöscht, ist der Wert 0 zu setzen.
 * Der Wert wird aber nicht nach <syncfunc_t> sondern
 * in den Parameter <syncfunc_param_t> geschrieben.
 * Er trägt eine Kopie des Zustandes und
 * andere nützliche, über <syncfunc_t> hinausgehende Informationen. */
void setstate_syncfunc(syncfunc_param_t * sfparam, void * new_state);

/* function: start_syncfunc
 * Springt zu onrun, onexit oder zu einer vorher gespeicherten Adresse.
 * Die ersten beiden Parameter müssen die Namen der Parameter von der aktuell
 * ausgeführten <syncfunc_f> sein. Das Verhalten des Makros hängt vom zweiten
 * Parameter sfcmd ab. Der erste ist nur von Interesse, falls sfcmd == <synccmd_CONTINUE>.
 *
 * Implementiert als Makro definiert start_syncfunc zusätzlich das Label syncfunc_START.
 * Es findet Verwendung bei der Berechnung von Sprungzielen.
 *
 * Verarbeitete sfcmd Werte:
 * synccmd_RUN         - Springe zu Label onrun. Dies ist der normale Ausführungszweig.
 * synccmd_CONTINUE    - Springt zu der Adresse, die bei der vorherigen Ausführung in
 *                       <syncfunc_param_t.contlabel> gespeichert wurde.
 * synccmd_EXIT        - Springe zu Label onexit. Dieser Zweig sollte alle Ressourcen freigeben
 *                       (free_memory(<state_syncfunc>(sfparam)) und <exit_syncfunc>(sfparam,EINTR) aufrufen.
 * Alle anderen Werte  - Das Makro tut nichts und kehrt zum Aufrufer zurück.
 * */
void start_syncfunc(const syncfunc_param_t * sfparam, uint32_t sfcmd, LABEL onrun, IDNAME onexit);

/* function: yield_syncfunc
 * Tritt Prozessor an andere <syncfunc_t> ab.
 * Die nächste Ausführung beginnt nach yield_syncfunc, sofern
 * <start_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
void yield_syncfunc(const syncfunc_param_t * sfparam);

/* function: exit_syncfunc
 * Beendet diese Funktion und setzt retcode als Ergebnis.
 * Konvention: Ein retcode von 0 meint OK, Werte > 0 sind System & App. Fehlercodes. */
void exit_syncfunc(const syncfunc_param_t * sfparam, int retcode);

/* function: wait_syncfunc
 * Wartet bis condition wahr ist.
 * Gibt 0 zurück, wenn das Warten erfolgreich war.
 * Der Wert ENOMEM zeigt an, daß nicht genügend Ressourcen bereitstanden
 * und die Warteoperation abgebrochen oder gar nicht erst gestartet wurde.
 * Andere Fehlercodes sind auch möglich.
 * Die nächste Ausführung beginnt nach wait_syncfunc, sofern
 * <start_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
int wait_syncfunc(const syncfunc_param_t * sfparam, struct synccond_t * condition);

/* function: waitexit_syncfunc
 * Wartet auf Exit der zuletzt gestarteten Funktionen.
 * In retcode wird der Rückgabewert der beendeten Funktion abgelegt (siehe <exit_syncfunc>),
 * auch im Fehlerfall, dann ist der Wert allerdings ungültig.
 *
 * Gibt 0 im Erfolgsfall zurück. Der Fehler EINVAL zeigt an,
 * daß keine Funktion innerhalb der aktuellen Ausführung dieser Funktion gestartet wurde
 * und somit kein Warten mölich ist. Im Falle zu weniger Ressourcen wird ENOMEM zurückgegeben.
 *
 * Die nächste Ausführung beginnt nach waitexit_syncfunc, sofern
 * <start_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
int waitexit_syncfunc(const syncfunc_param_t * sfparam, /*out;err*/int * retcode);



// section: inline implementation

// group: syncfunc_t

/* define: addrwaitresult_syncfunc
 * Implementiert <syncfunc_t.addrwaitresult_syncfunc>. */
#define addrwaitresult_syncfunc(sfunc) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            int        * _ptr;            \
            _sf = (sfunc);                \
            _ptr = (int*) (               \
                   (uint8_t*) _sf         \
                  + offwaitfor_syncfunc() \
                   );                     \
            _ptr;                         \
         }))


/* define: addrwaitfor_syncfunc
 * Implementiert <syncfunc_t.addrwaitfor_syncfunc>. */
#define addrwaitfor_syncfunc(sfunc) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            link_t * _ptr;                \
            _sf = (sfunc);                \
            _ptr = (link_t*) (            \
                   (uint8_t*) _sf         \
                  + offwaitfor_syncfunc() \
                   );                     \
            _ptr;                         \
         }))

/* define: addrwaitlist_syncfunc
 * Implementiert <syncfunc_t.addrwaitlist_syncfunc>. */
#define addrwaitlist_syncfunc(sfunc, iswaitfor) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            linkd_t * _ptr;               \
            _sf = (sfunc);                \
            _ptr = (linkd_t*) (           \
                   (uint8_t*) _sf         \
                  + offwaitlist_syncfunc( \
                     (iswaitfor)));       \
            _ptr;                         \
         }))

/* define: addrcaller_syncfunc
 * Implementiert <syncfunc_t.addrcaller_syncfunc>. */
#define addrcaller_syncfunc(sfunc, structsize, isstate) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            link_t * _ptr;                \
            _sf = (sfunc);                \
            _ptr = (link_t*) (            \
                   (uint8_t*) _sf         \
                  + offcaller_syncfunc(   \
                    (structsize),         \
                    (isstate), true));    \
            _ptr;                         \
         }))

/* define: addrstate_syncfunc
 * Implementiert <syncfunc_t.addrstate_syncfunc>. */
#define addrstate_syncfunc(sfunc, structsize) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            void ** _ptr;                 \
            _sf = (sfunc);                \
            _ptr = (void**) (             \
                   (uint8_t*) _sf         \
                  + offstate_syncfunc(    \
                    (structsize), true)); \
            _ptr;                         \
         }))

/* define: clearopt_syncfunc
 * Implementiert <syncfunc_t.clearopt_syncfunc>. */
#define clearopt_syncfunc(sfunc, optfield) \
         ( __extension__ ({               \
            syncfunc_t * _sf = (sfunc);   \
            _sf->optfields = (uint8_t) (  \
                     _sf->optfields       \
                     & ~(optfield));      \
         }))

/* define: exit_syncfunc
 * Implementiert <syncfunc_t.exit_syncfunc>. */
#define exit_syncfunc(sfparam, _rc) \
         {                                \
            (sfparam)->retcode = (_rc);   \
            return synccmd_EXIT;          \
         }

/* define: getsize_syncfunc
 * Implementiert <syncfunc_t.getsize_syncfunc>. */
#define getsize_syncfunc(optfields) \
         ( (uint16_t) (  offwaitfor_syncfunc()        \
            + (((optfields) & syncfunc_opt_WAITFOR)   \
              ? sizeof(link_t) : 0)                   \
            + (((optfields) & syncfunc_opt_WAITLIST)  \
              ? sizeof(linkd_t) : 0)                  \
            + (((optfields) & syncfunc_opt_CALLER)    \
              ? sizeof(link_t) : 0)                   \
            + (((optfields) & syncfunc_opt_STATE)     \
              ? sizeof(void*) : 0)))

/* define: state_syncfunc
 * Implementiert <syncfunc_t.state_syncfunc>. */
#define state_syncfunc(sfparam) \
         ((sfparam)->state)

/* define: init_syncfunc
 * Implementiert <syncfunc_t.init_syncfunc>. */
static inline void init_syncfunc(/*out*/syncfunc_t * sfunc, syncfunc_f mainfct, syncfunc_opt_e optfields)
         {
            sfunc->mainfct = mainfct;
            sfunc->contoffset = 0;
            sfunc->optfields = (uint8_t) optfields;
         }

/* define: offwaitfor_syncfunc
 * Implementiert <syncfunc_t.offwaitfor_syncfunc>. */
#define offwaitfor_syncfunc() \
         (offsetof(syncfunc_t, waitfor))

/* define: offwaitlist_syncfunc
 * Implementiert <syncfunc_t.offwaitlist_syncfunc>. */
#define offwaitlist_syncfunc(iswaitfor) \
         (offwaitfor_syncfunc() + ((iswaitfor) ? sizeof(link_t) : 0))

/* define: offcaller_syncfunc
 * Implementiert <syncfunc_t.offcaller_syncfunc>. */
#define offcaller_syncfunc(structsize, isstate, iscaller) \
         (offstate_syncfunc(structsize, isstate) - ((iscaller) ? sizeof(link_t) : 0))

/* define: offstate_syncfunc
 * Implementiert <syncfunc_t.offstate_syncfunc>. */
#define offstate_syncfunc(structsize, isstate) \
         ((structsize) - ((isstate) ? sizeof(void*) : 0))

/* define: setopt_syncfunc
 * Implementiert <syncfunc_t.setopt_syncfunc>. */
#define setopt_syncfunc(sfunc, optfield) \
         ( __extension__ ({               \
            syncfunc_t * _sf = (sfunc);   \
            _sf->optfields = (uint8_t) (  \
                     _sf->optfields       \
                     | (optfield));       \
         }))

/* define: setresult_syncfunc
 * Implementiert <syncfunc_t.setresult_syncfunc>. */
static inline void setresult_syncfunc(syncfunc_t * sfunc, int result)
         {
            sfunc->optfields = (uint8_t) (sfunc->optfields | syncfunc_opt_WAITRESULT);
            *addrwaitresult_syncfunc(sfunc) = result;
         }

/* define: setstate_syncfunc
 * Implementiert <syncfunc_t.setstate_syncfunc>. */
#define setstate_syncfunc(sfparam, new_state) \
         do { (sfparam)->state = (new_state) ; } while(0)

/* define: start_syncfunc
 * Implementiert <syncfunc_t.start_syncfunc>. */
#define start_syncfunc(sfparam, sfcmd, onrun, onexit) \
         syncfunc_START:                     \
         ( __extension__ ({                  \
            const syncfunc_param_t * _sf;    \
            _sf = (sfparam);                 \
            switch ( (synccmd_e) (sfcmd)) {  \
            case synccmd_RUN:                \
               goto onrun;                   \
            case synccmd_CONTINUE:           \
               goto * (void*) ( (uintptr_t)  \
                  &&syncfunc_START           \
                  + _sf->contoffset);        \
            case synccmd_EXIT:               \
               goto onexit;                  \
            default: /*ignoring all other*/  \
               break;                        \
         }}))

/* define: wait_syncfunc
 * Implementiert <syncfunc_t.wait_syncfunc>. */
#define wait_syncfunc(sfparam, _condition) \
         ( __extension__ ({                                 \
            __label__ continue_after_wait;                  \
            (sfparam)->condition = _condition;              \
            static_assert(                                  \
               (uintptr_t)&&continue_after_wait >           \
               (uintptr_t)&&syncfunc_START                  \
               && (uintptr_t)&&continue_after_wait -        \
                  (uintptr_t)&&syncfunc_START < 65536,      \
                  "conversion to uint16_t works");          \
            (sfparam)->contoffset = (uint16_t) (            \
                           (uintptr_t)&&continue_after_wait \
                           - (uintptr_t)&&syncfunc_START);  \
            return synccmd_WAIT;                            \
            continue_after_wait: ;                          \
            (sfparam)->waiterr;                             \
         }))

/* define: waitexit_syncfunc
 * Implementiert <syncfunc_t.waitexit_syncfunc>. */
#define waitexit_syncfunc(sfparam, _retcode) \
         ( __extension__ ({                                 \
            __label__ continue_after_wait;                  \
            (sfparam)->condition = 0;                       \
            static_assert(                                  \
               (uintptr_t)&&continue_after_wait >           \
               (uintptr_t)&&syncfunc_START                  \
               && (uintptr_t)&&continue_after_wait -        \
                  (uintptr_t)&&syncfunc_START < 65536,      \
                  "conversion to uint16_t works");          \
            (sfparam)->contoffset = (uint16_t) (            \
                           (uintptr_t)&&continue_after_wait \
                           - (uintptr_t)&&syncfunc_START);  \
            return synccmd_WAIT;                            \
            continue_after_wait: ;                          \
            *(_retcode) = (sfparam)->retcode;               \
            (sfparam)->waiterr;                             \
         }))

/* define: waitforcast_syncfunc
 * Implementiert <syncfunc_t.waitforcast_syncfunc>. */
#define waitforcast_syncfunc(waitfor) \
            ((syncfunc_t*) ((uint8_t*) (waitfor) - offwaitfor_syncfunc()))

/* define: waitlistcast_syncfunc
 * Implementiert <syncfunc_t.waitlistcast_syncfunc>. */
#define waitlistcast_syncfunc(waitlist, iswaitfor) \
            ((syncfunc_t*) ((uint8_t*) (waitlist) - offwaitlist_syncfunc(iswaitfor)))

/* define: yield_syncfunc
 * Implementiert <syncfunc_t.yield_syncfunc>. */
#define yield_syncfunc(sfparam) \
         ( __extension__ ({                                 \
            __label__ continue_after_yield;                 \
            static_assert(                                  \
               (uintptr_t)&&continue_after_yield >          \
               (uintptr_t)&&syncfunc_START                  \
               && (uintptr_t)&&continue_after_yield -       \
                  (uintptr_t)&&syncfunc_START < 65536,      \
                  "conversion to uint16_t works");          \
            (sfparam)->contoffset = (uint16_t) (            \
                        (uintptr_t)&&continue_after_yield   \
                        - (uintptr_t)&&syncfunc_START);     \
            return synccmd_CONTINUE;                        \
            continue_after_yield: ;                         \
         }))

/* define: initmove_syncfunc
 * Implementiert <syncfunc_t.initmove_syncfunc>. */
static inline void initmove_syncfunc(/*out*/syncfunc_t * dest, uint16_t destsize, uint16_t destcontoffset, syncfunc_opt_e destoptfields, void * deststate, syncfunc_t * src, uint16_t srcsize, int isstate)
         {
            dest->mainfct    = src->mainfct;
            dest->contoffset = destcontoffset;
            dest->optfields  = destoptfields;

            if (deststate) {
               *addrstate_syncfunc(dest, destsize) = deststate;
            }

            if (0 != (src->optfields & syncfunc_opt_CALLER)) {
               link_t * caller = addrcaller_syncfunc(dest, destsize, deststate);
               *caller = *addrcaller_syncfunc(src, srcsize, isstate);
               if (isvalid_link(caller)) {
                  relink_link(caller);
               }
            }
         }

#endif
