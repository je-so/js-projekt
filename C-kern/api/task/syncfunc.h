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
 * syncfunc_opt_NONE       - Weder <syncfunc_t.state> noch <syncfunc_t.contlabel> sind vorhanden.
 * syncfunc_opt_WAITFIELDS - Die optionalen Felder <syncfunc_t.waitresult> und <syncfunc_t.waitresult> sind vorhanden.
 * syncfunc_opt_ALL        - Alle Felder sind vorhanden, dasselbe wie <syncfunc_opt_WAITFIELDS>, da noch keine weiteren Bits
 *                           vorhanden sind.
 *                          */
typedef enum syncfunc_opt_e {
   syncfunc_opt_NONE            = 0,
   syncfunc_opt_WAITFIELDS      = 1,
   syncfunc_opt_ALL             = 1
} syncfunc_opt_e;



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
   /* variable: srun
    * Eingabe-Param: Der Verwalter-Kontext von <syncfunc_t>. */
   struct
   syncrunner_t* const  srun;
   /* variable: srun
    * Eingabe-Param: Zeigt auf aktuell ausgeführte Funktion. */
   syncfunc_t*          sfunc;
   /* variable: contoffset
    * Ein- Ausgabe-Param: Die Stelle, and der mit der Ausführung weitergemacht werden soll.
    * Nur gültig, wenn Parameter sfcmd den Wert <synccmd_CONTINUE> besitzt.
    * Der Wert wird nach »return« gespeichert, wenn die Funktion <synccmd_CONTINUE>
    * oder <synccmd_WAIT> zurückgibt. */
   uint16_t             contoffset;
   /* variable: state
    * Ein- Ausgabe-Param: Der gespeicherte Funktionszustand.
    * Muss von der Funktion verwaltet werden. */
   void*                state;
   /* variable: condition
    * Ausgabe-Param: Referenziert die Bedingung, auf die gewartet werden soll.
    * Der Wert wird genau dann verwendet, wenn die Funktion den Wert <synccmd_WAIT> zurückgibt. */
   struct synccond_t*   condition;
   /* variable: err
    * Eingabe-Param: Das Ergebnis der Warteoperation.
    * Eine 0 zeigt Erfolg an, != 0 ist ein Fehlercode – etwa ENOMEM wenn zu wenig Ressourcen
    * frei waren, um die Funktion in die Warteliste einzutragen.
    * Nur gültig, wenn das Kommando <synccmd_CONTINUE> ist und zuletzt die Funktion mit
    * »return <synccmd_WAIT>« beendet wurde.
    *
    * Ausgabe-Param: Der Returnwert der Funktion.
    * Beendet sich die Funktion mit »return <synccmd_EXIT>«, dann steht in diesem Wert
    * der Erfolgswert drin (0 für erfolgreich, != 0 Fehlercode). */
   int                  err;
};

// group: lifetime

/* define: syncfunc_param_FREE
 * Static initializer. */
#define syncfunc_param_FREE \
         { 0, 0, 0, 0, 0, 0 }

/* define: syncfunc_param_INIT
 * Static initializer. Parameter syncrun zeigt auf gültigen <syncrunner_t>. */
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
 * >    // err == 0
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
   /* variable: state
    * Zeigt auf gesicherten Zustand der Funktion.
    * Der Wert ist anfangs 0 (ungültig). Wird von <mainfct> verwaltet. */
   void*       state;
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
   /* variable: optflags
    * Kodiert, welche optionalen Felder vorhanden sind – siehe <syncfunc_opt_e>. */
   uint8_t     optflags;

   // == optional fields for use in wait operations ==

   /* variable: waitresult
    * *Optional*: Das Ergebnis der Warteoperation. */
   int         waitresult;
   /* variable: waitlist
    * *Optional*: Verbindet wartende <syncfunc_t> in einer Liste mit der Wartebdingung <synccond_t>. */
   linkd_t     waitlist;
};

// group: lifetime

/* define: syncfunc_FREE
 * Static initializer. */
#define syncfunc_FREE \
         { 0, 0, 0, 0, 0, linkd_FREE }

/* function: init_syncfunc
 * Initialisiert alle, außer den optionalen Feldern. */
static inline void init_syncfunc(/*out*/syncfunc_t * sfunc, syncfunc_f mainfct, void* state, syncfunc_opt_e optflags);

/* function: init2_syncfunc
 * Kopiert <mainfct>.
 * <mainfct> wird von src kopiert.
 * Die Felder <contoffset>, <optflags> und <state> werden mit destcontoffset, destoptflags bzw. deststate initialisiert.
 *
 * Nach Return sind die Felder in src ungültig.
 *
 * Unitialisiert:
 * Alle anderen optionalen Felder bleiben unitialisiert.
 *
 * Unchecked Preconditon:
 * o srcsize  == getsize_syncfunc(src->optflags)
 * o destsize == getsize_syncfunc(destoptflags)
 * o (destoptflags & syncfunc_opt_CALLER)
 *   == (src->optflags & syncfunc_opt_CALLER)
 * */
static inline void init2_syncfunc(/*out*/syncfunc_t * dest, uint16_t destsize, uint16_t destcontoffset, syncfunc_opt_e destoptflags, void * deststate, syncfunc_t * src, uint16_t srcsize);

// TODO: remove init2_syncfunc !! put parameter contoffset in init_syncfunc !!

// TODO: implement + test initmove_syncfunc(/*out*/syncfunc_t* dest, syncfunc_t* src);

// group: query

/* function: waitresult_syncfunc
 * Liefert Wert, den <setwaitresult_syncfunc> gesetzt hat.
 * Falls <setwaitresult_syncfunc> nicht aufgerufen wurde,
 * wird automatisch der Wert 0 für OK zurückgeliefert. */
static inline int waitresult_syncfunc(const syncfunc_t* sfunc);

/* function: getsize_syncfunc
 * Liefere die aus den optionalen Feldern optflags berechnte Größe in Byte einer <syncfunc_t>.
 * Siehe auch <syncfunc_opt_e>.
 *
 * Precondition:
 * Das Argument optflags wird mehrfach ausgewertet, es muss daher ein konstanter Ausdruck
 * ohne Nebeneffekte sein. */
uint16_t getsize_syncfunc(const syncfunc_opt_e optflags);

/* function: castPwaitlist_syncfunc
 * Caste waitlist nach <syncfunc_t>.
 *
 * Precondition:
 * o waitlist != 0
 * o waitlist == waitlist_syncfunc(...) */
static inline syncfunc_t* castPwaitlist_syncfunc(linkd_t* waitlist);

/* function: waitlist_syncfunc
 * Liefere die Adresse des optionalen Feldes <waitlist>.
 *
 * Precondition:
 * o (<syncfunc_t.optflags> & syncfunc_opt_WAITFIELDS) != 0
 * o Space is reserved for optional <waitlist> and <waitresult>. */
linkd_t* waitlist_syncfunc(syncfunc_t* sfunc);

// group: update

/* function: relink_syncfunc
 * Korrigiert die Ziellinks <waitlist>, nachdem sfunc im Speicher verschoben wurde. */
void relink_syncfunc(syncfunc_t* sfunc);

/* function: setwaitresult_syncfunc
 * Setzt <waitresult> auf result.
 *
 * Precondition:
 * o (<syncfunc_t.optflags> & syncfunc_opt_WAITFIELDS) != 0
 * o Space is reserved for optional <waitlist> and <waitresult>. */
static inline void setwaitresult_syncfunc(syncfunc_t* sfunc, int result);

/* function: unlink_syncfunc
 * Invalidiert die Ziellinks <waitlist>.
 * Inhalt von <waitlist> ist danach ungültig, aber unverändert. */
void unlink_syncfunc(syncfunc_t* sfunc);

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
void* state_syncfunc(const syncfunc_param_t* sfparam);

/* function: setstate_syncfunc
 * Setzt den Zustand der gerade ausgeführten Funktion.
 * Wurde der Zustand gelöscht, ist der Wert 0 zu setzen.
 * Der Wert wird aber nicht nach <syncfunc_t> sondern
 * in den Parameter <syncfunc_param_t> geschrieben.
 * Er trägt eine Kopie des Zustandes und
 * andere nützliche, über <syncfunc_t> hinausgehende Informationen. */
void setstate_syncfunc(syncfunc_param_t* sfparam, void* new_state);

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
// TODO: rename into continue_syncfunc // remove onrun (fall through) // wrong value ==> onexit //
void start_syncfunc(const syncfunc_param_t* sfparam, uint32_t sfcmd, LABEL onrun, IDNAME onexit);

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



// section: inline implementation

// group: syncfunc_t

/* define: waitlist_syncfunc
 * Implementiert <syncfunc_t.waitlist_syncfunc>. */
#define waitlist_syncfunc(sfunc) \
         (&(sfunc)->waitlist)

/* define: exit_syncfunc
 * Implementiert <syncfunc_t.exit_syncfunc>. */
#define exit_syncfunc(sfparam, _rc) \
         {                          \
            (sfparam)->err = (_rc); \
            return synccmd_EXIT;    \
         }

/* define: getsize_syncfunc
 * Implementiert <syncfunc_t.getsize_syncfunc>. */
#define getsize_syncfunc(optflags) \
         ( (uint16_t) (             \
            + (((optflags) & syncfunc_opt_WAITFIELDS) \
              ? sizeof(syncfunc_t)                    \
              : offsetof(syncfunc_t, waitresult))))

/* define: init_syncfunc
 * Implementiert <syncfunc_t.init_syncfunc>. */
static inline void init_syncfunc(/*out*/syncfunc_t* sfunc, syncfunc_f mainfct, void* state, syncfunc_opt_e optflags)
{
         sfunc->mainfct = mainfct;
         sfunc->state = state;
         sfunc->contoffset = 0;
         sfunc->optflags = (uint8_t) optflags;
}

// TODO: remove function !!
/* define: init2_syncfunc
 * Implementiert <syncfunc_t.init2_syncfunc>. */
static inline void init2_syncfunc(/*out*/syncfunc_t * dest, uint16_t destsize, uint16_t destcontoffset, syncfunc_opt_e destoptflags, void * deststate, syncfunc_t * src, uint16_t srcsize)
{
   (void) destsize; (void) srcsize;
         dest->mainfct    = src->mainfct;
         dest->state      = deststate;
         dest->contoffset = destcontoffset;
         dest->optflags  = destoptflags;
}

/* define: waitresult_syncfunc
 * Implementiert <syncfunc_t.waitresult_syncfunc>. */
static inline int waitresult_syncfunc(const syncfunc_t* sfunc)
{
         return (sfunc->optflags & syncfunc_opt_WAITFIELDS)
                ? sfunc->waitresult : 0;
}

/* define: setwaitresult_syncfunc
 * Implementiert <syncfunc_t.setwaitresult_syncfunc>. */
static inline void setwaitresult_syncfunc(syncfunc_t * sfunc, int result)
{
         sfunc->waitresult = result;
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

/* define: state_syncfunc
 * Implementiert <syncfunc_t.state_syncfunc>. */
#define state_syncfunc(sfparam) \
         ((sfparam)->state)

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
            (sfparam)->err;                                 \
         }))

/* define: castPwaitlist_syncfunc
 * Implementiert <syncfunc_t.castPwaitlist_syncfunc>. */
static inline syncfunc_t * castPwaitlist_syncfunc(linkd_t * waitlist)
{
         return (syncfunc_t*) ((uint8_t*) (waitlist) - offsetof(syncfunc_t, waitlist));
}

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

#endif
