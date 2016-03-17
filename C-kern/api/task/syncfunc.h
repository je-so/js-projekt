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

// === exported types
struct syncfunc_t;
struct syncfunc_param_t;

/* typedef: syncfunc_f
 * Definiert Signatur einer wiederholt auszuführenden Funktion.
 * Diese Funktionen werden *nicht* präemptiv unterbrochen und basieren
 * auf Kooperation mit anderen Funktionen.
 *
 * Parameter:
 * sfparam  - Ein- Ausgabe Parameter.
 *            Einige Eingabe (in) Felder sind nur bei einem bestimmten Wert in sfcmd gültig.
 *            Siehe <syncfunc_param_t> für eine ausführliche Beschreibung.
 *
 * Return:
 * Der Rückgabewert ist das vom Aufrufer auszuführende Kommando – auch aus <synccmd_e>. */
typedef int (* syncfunc_f) (struct syncfunc_param_t * sfparam);

/* enums: syncfunc_opt_e
 * Bitfeld das die vorhandenen Felder in <syncfunc_t> kodiert.
 *
 * syncfunc_opt_NONE     - Weder <syncfunc_t.state> noch <syncfunc_t.contlabel> sind vorhanden.
 * syncfunc_opt_WAITLIST - Die optionalen Felder <syncfunc_t.waitresult> und <syncfunc_t.waitlist> sind vorhanden.
 * */
typedef enum syncfunc_opt_e {
   syncfunc_opt_NONE     = 0,
   syncfunc_opt_WAITLIST = 1
} syncfunc_opt_e;

#define syncfunc_opt__NROF 2



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncfunc
 * Teste <syncfunc_t>. */
int unittest_task_syncfunc(void);
#endif


/* struct: syncfunc_param_t
 * Definiert Ein- Ausgabeparameter von <syncfunc_f> Funktionen. */
typedef struct syncfunc_param_t {
   /* variable: srun
    * In-Param: Der Verwalter-Kontext von <syncfunc_t>. */
   struct syncrunner_t* const srun;
   /* variable: isterminate
    * In-Param: Falls true, wird die Funktion abnormal beendet.
    * Dieser Wert wird von <syncrunner_t.terminate_syncrunner> auf true (!= 0) gesetzt,
    * ansonsten ist er 0. */
   uint8_t              const isterminate;
   /* variable: srun
    * In-Param: Zeigt auf aktuell ausgeführte Funktion. */
   struct syncfunc_t*         sfunc;
   /* variable: condition
    * Out-Param: Referenziert die Bedingung, auf die gewartet werden soll.
    * Der Wert wird genau dann verwendet, wenn die Funktion den Wert <synccmd_WAIT> zurückgibt. */
   struct synccond_t*         condition;
   /* variable: err
    * In-Param: Das Ergebnis der Warteoperation.
    * Eine 0 zeigt Erfolg an, != 0 ist ein Fehlercode – etwa ENOMEM wenn zu wenig Ressourcen
    * frei waren, um die Funktion in die Warteliste einzutragen.
    *
    * Out-Param: Der Returnwert der Funktion.
    * Beendet sich die Funktion mit »return <synccmd_EXIT>«, dann steht in diesem Wert
    * der Erfolgswert drin (0 für erfolgreich, != 0 Fehlercode). */
   int                        err;
} syncfunc_param_t;

// group: lifetime

/* define: syncfunc_param_FREE
 * Static initializer. */
#define syncfunc_param_FREE \
         { 0, 0, 0, 0, 0 }

/* define: syncfunc_param_INIT
 * Static initializer.
 *
 * Parameter:
 * syncrun - zeigt auf gültigen <syncrunner_t>.
 * isterminate - true: Funktion wird aus dem Kontext von <syncrunner_t.terminate_syncrunner> ausgeführt. */
#define syncfunc_param_INIT(syncrun, isterminate) \
         { syncrun, isterminate, 0, 0, 0 }


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
 * > int test_sf(syncfunc_param_t * sfparam)
 * > {
 * >    int err = EINTR; // inidicates sfparam->isterminate is true
 * >    start_syncfunc(sfparam, ONERR);
 * > // init
 * >    void* state = alloc_memory(...);
 * >    setstate_syncfunc(sfparam, state);
 * >    ...
 * >    yield_syncfunc(sfparam);
 * >    ...
 * >    synccond_t* condition = ...;
 * >    err = wait_syncfunc(sfparam, condition);
 * >    if (err) goto ONERR;
 * >    ...
 * >    // err == 0
 * > ONERR:
 * >    if (0 != state_syncfunc(sfparam)) {
 * >       free_memory( state_syncfunc(sfparam) );
 * >    }
 * >    exit_syncfunc(sfparam, err);
 * > }
 * */
typedef struct syncfunc_t {
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
    * > int test_syncfunc(syncfunc_param_t * sfparam)
    * > {
    * >    ...
    * >    {  // yield MACRO
    * >       __label__ continue_after_wait;
    * >       sfparam->contoffset = &&continue_after_wait - &&syncfunc_START;
    * >       // yield processor to other functions
    * >       return synccmd_RUN;
    * >       // execution continues here
    * >       continue_after_wait: ;
    * >    }
    * >    ...
    * > } */
   uint16_t    contoffset;
   /* variable: optflags
    * Kodiert, welche optionalen Felder vorhanden sind – siehe <syncfunc_opt_e>. */
   uint8_t     optflags;
   /* variable: waitresult
    * Das Ergebnis der Warteoperation. */
   uint8_t     waitresult;

   // == optional fields for use in wait operations ==

   /* variable: waitlist
    * *Optional*: Verbindet wartende <syncfunc_t> in einer Liste mit der Wartebdingung <synccond_t>. */
   linkd_t     waitlist;
} syncfunc_t;

// group: lifetime

/* define: syncfunc_FREE
 * Static initializer. */
#define syncfunc_FREE \
         { 0, 0, 0, 0, 0, linkd_FREE }

/* function: init_syncfunc
 * Initialisiert alle, außer den optionalen Feldern. */
static inline void init_syncfunc(/*out*/syncfunc_t* sfunc, syncfunc_f mainfct, void* state, syncfunc_opt_e optflags);

/* function: initcopy_syncfunc
 * Kopiert all nicht-optionalen Felder außer optflags von src nach dest.
 * dest->optflags wird auf optflags gesetzt.
 *
 * Optionale Felder bleiben unitialisiert !
 * */
static inline void initcopy_syncfunc(/*out*/syncfunc_t* __restrict__ dest, syncfunc_t* __restrict__ src, syncfunc_opt_e optflags);

/* function: initmove_syncfunc
 * Kopiert getsize_syncfunc(src->optflags) Bytes von src nach dest und adaptiert mögliches Links.
 * Der Inhalt von src ist danach ungültig.
 *
 * Supported Condition:
 * o isself_linkd(src->waitlist)
 *
 * Unchecked Precondition:
 * o is_aligned_long(src) && is_aligned_long(dest)
 * o size_allocated_memory(dest) == getsize_syncfunc(src->optflags)
 * o 0 == (src->optflags & syncfunc_opt_WAITLIST)
 *   || ( (src->optflags & syncfunc_opt_WAITLIST)
 *        && isvalid_linkd(src->waitlist))
 * */
static inline void initmove_syncfunc(/*out*/syncfunc_t* __restrict__ dest, const syncfunc_t* __restrict__ src);


// group: query

/* function: waitresult_syncfunc
 * Liefert Wert, den <setwaitresult_syncfunc> gesetzt hat.
 * */
uint8_t waitresult_syncfunc(const syncfunc_t* sfunc);

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
 * Unchecked Precondition:
 * o waitlist != 0 && waitlist == waitlist_syncfunc(sfunc) */
static inline syncfunc_t* castPwaitlist_syncfunc(linkd_t* waitlist);

/* function: waitlist_syncfunc
 * Liefere die Adresse des optionalen Feldes <waitlist>.
 *
 * Unchecked Precondition:
 * o (<syncfunc_t.optflags> & syncfunc_opt_WAITLIST) != 0
 * o Space is reserved for optional <waitlist> and <waitresult>. */
linkd_t* waitlist_syncfunc(syncfunc_t* sfunc);

// group: update

/* function: setwaitresult_syncfunc
 * Setzt <waitresult> auf result. */
static inline void setwaitresult_syncfunc(syncfunc_t* sfunc, uint8_t result);

/* function: unlink_syncfunc
 * Entfernt sfunc aus allen Listen.
 * Entfernt sfunc aus <waitlist>, falls 0!=(src->optflags & syncfunc_opt_WAITLIST).
 * Inhalt von <waitlist> ist danach unverändert, aber Nachbarknoten verweisen nicht mehr auf sfunc. */
void unlink_syncfunc(syncfunc_t* sfunc);

// group: implementation-support
// Macros which makes implementing a <syncfunc_f> possible.

/* function: contoffset_syncfunc
 * Gibt gespeicherten Offset von syncfunc_START (siehe <start_syncfunc>)
 * zu einer Labeladresse. Dieser Offset erlaubt es,
 * die Ausführung an der Stelle des Labels fortzusetzen. */
uint16_t contoffset_syncfunc(const syncfunc_param_t* sfparam);

/* function: setcontoffset_syncfunc
 * Setzt gespeicherten Offset von syncfunc_START (siehe <start_syncfunc>)
 * zu einer Labeladresse. Dieser Offset erlaubt es,
 * die Ausführung an der Stelle des Labels fortzusetzen. */
void setcontoffset_syncfunc(const syncfunc_param_t* sfparam, uint16_t contoffset);

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
 * Springt zu onexit oder zu einer vorher gespeicherten Adresse.
 * Die ersten beiden Parameter müssen die Namen der Parameter von der aktuell
 * ausgeführten <syncfunc_f> sein. Das Verhalten des Makros hängt vom zweiten
 * Parameter sfcmd ab.
 *
 * Als Makro implementiert definiert start_syncfunc zusätzlich das Label syncfunc_START.
 * Es findet Verwendung bei der Berechnung von Sprungzielen.
 *
 * Verarbeitete sfcmd Werte:
 * synccmd_EXIT        - Springe zu Label onexit. Dieser Zweig sollte alle Ressourcen freigeben
 *                       (free_memory(<state_syncfunc>(sfparam)) und <exit_syncfunc>(sfparam,EINTR) aufrufen.
 * Alle anderen Werte  - Falls <contoffset_syncfunc> != 0: Springt zu der Adresse, die bei der vorherigen Ausführung
 *                       mit <setcontoffset_syncfunc> gespeichert wurde.
 *                       Falls <contoffset_syncfunc> == 0: Das Makro tut nichts und kehrt zum Aufrufer zurück.
 *                       Dies dient der Initialisierung der Funktion.
 * */
void start_syncfunc(const syncfunc_param_t* sfparam, uint32_t sfcmd, IDNAME onexit);

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
 * <start_syncfunc> am Anfang dieser Funktion aufgerufen wird.
 *
 * Unchecked Precondition:
 * o sfparam will be evaluated more than once (implemented as macro)
 *   ==> Use the name of the first parameter of <syncfunc_t> as only
 *   argument; never use an expression with side effects. */
int wait_syncfunc(const syncfunc_param_t * sfparam, struct synccond_t * condition);



// section: inline implementation

// group: syncfunc_t

/* define: castPwaitlist_syncfunc
 * Implementiert <syncfunc_t.castPwaitlist_syncfunc>. */
static inline syncfunc_t* castPwaitlist_syncfunc(linkd_t* waitlist)
{
         return (syncfunc_t*) ((uint8_t*) (waitlist) - offsetof(syncfunc_t, waitlist));
}

/* define: contoffset_syncfunc
 * Implementiert <syncfunc_t.contoffset_syncfunc>. */
#define contoffset_syncfunc(sfparam) \
         ((sfparam)->sfunc->contoffset)

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
         ( (uint16_t) (                               \
              (((optflags) & syncfunc_opt_WAITLIST)   \
              ? sizeof(syncfunc_t)                    \
              : offsetof(syncfunc_t, waitlist)))      \
         )

/* define: init_syncfunc
 * Implementiert <syncfunc_t.init_syncfunc>. */
static inline void init_syncfunc(/*out*/syncfunc_t* sfunc, syncfunc_f mainfct, void* state, syncfunc_opt_e optflags)
{
         sfunc->mainfct = mainfct;
         sfunc->state = state;
         sfunc->contoffset = 0;
         sfunc->optflags = (uint8_t) optflags;
}

/* define: initcopy_syncfunc
 * Implementiert <syncfunc_t.initcopy_syncfunc>. */
static inline void initcopy_syncfunc(/*out*/syncfunc_t* __restrict__ dest, syncfunc_t* __restrict__ src, syncfunc_opt_e optflags)
{
         dest->mainfct    = src->mainfct;
         dest->state      = src->state;
         dest->contoffset = src->contoffset;
         dest->optflags   = optflags;
}

/* define: initmove_syncfunc
 * Implementiert <syncfunc_t.initmove_syncfunc>. */
static inline void initmove_syncfunc(/*out*/syncfunc_t* __restrict__ dest, const syncfunc_t* __restrict__ src)
{
         int iswaitlist = (src->optflags & syncfunc_opt_WAITLIST);
         static_assert(3*sizeof(long) == getsize_syncfunc(syncfunc_opt_NONE), "supports long copy");
         ((long*)dest)[0] = ((const long*)src)[0];
         ((long*)dest)[1] = ((const long*)src)[1];
         ((long*)dest)[2] = ((const long*)src)[2];
         if (iswaitlist) {
            if (isself_linkd(&src->waitlist)) {
               initself_linkd(&dest->waitlist);
            } else {
               dest->waitlist = src->waitlist;
               relink_linkd(&dest->waitlist);
            }
         }
}

/* define: setcontoffset_syncfunc
 * Implementiert <syncfunc_t.setcontoffset_syncfunc>. */
#define setcontoffset_syncfunc(sfparam, _contoffset) \
         do { (sfparam)->sfunc->contoffset = (_contoffset) ; } while(0)

/* define: setwaitresult_syncfunc
 * Implementiert <syncfunc_t.setwaitresult_syncfunc>. */
static inline void setwaitresult_syncfunc(syncfunc_t * sfunc, uint8_t result)
{
         sfunc->waitresult = result;
}

/* define: setstate_syncfunc
 * Implementiert <syncfunc_t.setstate_syncfunc>. */
#define setstate_syncfunc(sfparam, new_state) \
         do { (sfparam)->sfunc->state = (new_state) ; } while(0)

/* define: start_syncfunc
 * Implementiert <syncfunc_t.start_syncfunc>. */
#define start_syncfunc(sfparam, onexit) \
         syncfunc_START:                     \
         ( __extension__ ({                  \
            const syncfunc_param_t * _p;     \
            _p = (sfparam);                  \
            if (_p->isterminate) {           \
               goto onexit;                  \
            }                                \
            if (_p->sfunc->contoffset) {     \
               goto * (void*) ( (uintptr_t)  \
                  &&syncfunc_START           \
                  + _p->sfunc->contoffset);  \
            }                                \
         }))

/* define: state_syncfunc
 * Implementiert <syncfunc_t.state_syncfunc>. */
#define state_syncfunc(sfparam) \
         ((sfparam)->sfunc->state)

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
            (sfparam)->sfunc->contoffset = (uint16_t) (     \
                           (uintptr_t)&&continue_after_wait \
                           - (uintptr_t)&&syncfunc_START);  \
            return synccmd_WAIT;                            \
            continue_after_wait: ;                          \
            (sfparam)->err;                                 \
         }))

/* define: waitlist_syncfunc
 * Implementiert <syncfunc_t.waitlist_syncfunc>. */
#define waitlist_syncfunc(sfunc) \
         (&(sfunc)->waitlist)

/* define: waitresult_syncfunc
 * Implementiert <syncfunc_t.waitresult_syncfunc>. */
#define waitresult_syncfunc(sfunc) \
         ((sfunc)->waitresult)

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
            (sfparam)->sfunc->contoffset = (uint16_t) (     \
                        (uintptr_t)&&continue_after_yield   \
                        - (uintptr_t)&&syncfunc_START);     \
            return synccmd_RUN;                             \
            continue_after_yield: ;                         \
         }))

#endif
