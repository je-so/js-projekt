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
struct syncwait_t;

// === exported types
struct syncfunc_t;
struct syncfunc_it;
struct syncfunc_param_t;

/* TODO: IDEA implement producer_queue for producer/generator function
         producer runs as many times as there are empty entries until queue is full
         consumer reads entries until queue is empty which signals the producer to resume running
         if consumer reads as many entries as the producer generates the producer never gives up
         running
 */

/* typedef: syncfunc_f
 * Definiert Signatur einer wiederholt auszuführenden Funktion.
 * Diese Funktionen werden müssen kooperativ ihre Rechenzeit abgeben bezüglich
 * einer Gruppe von <syncfunc_t> die von einem <syncrunner_t> veraltet werden.
 *
 * Parameter:
 * sfparam  - Eingabe- Parameter. Alle (in) Felder sind gültig.
 *            Siehe <syncfunc_param_t> für eine ausführliche Beschreibung.
 *
 */
typedef void (* syncfunc_f) (struct syncfunc_param_t *sfparam);


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncfunc
 * Teste <syncfunc_t>. */
int unittest_task_syncfunc(void);
#endif

/* struct: syncfunc_it
 * Definiert das Interface zum Syncfunc-Service-Provider (siehe <syncrunner_t>). */
typedef struct syncfunc_it {
   void  (*exitsf) (struct syncfunc_param_t *sfparam);
   void  (*waitsf) (struct syncfunc_param_t *sfparam, struct syncwait_t *waitlist);
} syncfunc_it;

// group: lifetime

/* define: syncfunc_it_INIT
 * Static initializer.
 * Parameter:
 * exisf  - Must be called before <syncfunc_t> returns to remove it from the run queue.
 * waitsf - Must be called before <syncfunc_t> returns to add it to a waitlist.
 *          Next time syncfunc is called after it is removed from waitlist,
 *          i.e. woken up by some met condition. */
#define syncfunc_it_INIT(exitsf, waitsf) \
         { exitsf, waitsf }

/* struct: syncfunc_param_t
 * Definiert Ein- Ausgabeparameter von <syncfunc_f> Funktionen. */
typedef struct syncfunc_param_t {
   /* variable: srun
    * In-Param: Der Verwalter-Kontext von <syncfunc_t>. */
   struct syncrunner_t* const srun;
   /* variable: sfunc
    * In-Param: Zeigt auf aktuell ausgeführte Funktion. */
   struct syncfunc_t*         sfunc;
   /* variable: iimpl;
    * In-Param: Die von <srun> exportierte Service-Schnittstelle für eine <syncfunc_t>. */
   struct syncfunc_it*  const iimpl;
} syncfunc_param_t;

// group: lifetime

/* define: syncfunc_param_FREE
 * Static initializer. */
#define syncfunc_param_FREE \
         { 0, 0, 0 }

/* define: syncfunc_param_INIT
 * Static initializer.
 *
 * Parameter:
 * syncrun - zeigt auf gültigen <syncrunner_t>.
 * iimpl   - zeigt auf ein von syncrun exportiertes Interface vom Typ <syncfunc_it>.
 */
#define syncfunc_param_INIT(syncrun, iimpl) \
         { syncrun, 0, iimpl }


/* struct: syncfunc_t
 * Der Ausführungskontext einer Funktion der Signatur <syncfunc_f>.
 * Dieser wird von einem <syncrunner_t> verwaltet, der alle
 * verwalteten Funktionen nacheinander aufruft. Durch die Ausführung
 * nacheinander und den Verzicht auf präemptive Zeiteinteilung,
 * ist die Ausführung synchron. Dieses kooperative Multitasking zwischen
 * allen Funktionen eines einzigen <syncrunner_t>s erlaubt und gebietet
 * den Verzicht Locks verzichtet werden (siehe <mutex_t>). Warteoperationen
 * müssen mittels einer <syncwait_t> durchgeführt werden.
 *
 * Optionale Datenfelder:
 * Die Variablen <state> und <contlabel> sind optionale Felder.
 *
 * In einfachsten Fall beschreibt <mainfct> alleine eine syncfunc_t.
 *
 * Auf 0 gesetzte Felder sind ungültig.
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
 * >    begin_syncfunc(sfparam);
 * >    // INIT
 * >    void* state = alloc_memory(...);
 * >    setstate_syncfunc(sfparam, state);
 * >    // COMPUTE
 * >    ...
 * >    yield_syncfunc(sfparam);
 * >    ...
 * >    syncwait_t* waitlist = ...;
 * >    err = wait_syncfunc(sfparam, waitlist);
 * >    if (err) exit_syncfunc(sfparam,err);
 * >    ...
 * >    end_syncfunc(sfparam, {
 * >       if (0 != state_syncfunc(sfparam)) {
 * >          free_memory( state_syncfunc(sfparam) );
 * >       }
 * >    })
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
    * Speichert Offset von syncfunc_START (siehe <begin_syncfunc>)
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
    * >       return 0;
    * >       // execution continues here
    * >       continue_after_wait: ;
    * >    }
    * >    ...
    * > } */
   int16_t     contoffset;
   /* variable: endoffset
    * Offset zu Label "syncfunc_END: ;" für Abbruch der Funktion ausgehend von <syncrunner_t>. */
   int16_t     endoffset;
   /* variable: err
    * in:  Der Fehlercode, der vom Warten zurückgegeben wird.
    * out: Beendet sich die Funktion mit <exit_syncfunc>, dann steht in diesem Wert
    * der Erfolgswert drin (0 für erfolgreich, != 0 Fehlercode). */
   int         err;
   /* variable: waitnode
    * Verbindet wartende <syncfunc_t> in einer Liste mit der Wartebdingung <syncwait_t>. */
   linkd_t     waitnode;
} syncfunc_t;

// group: lifetime

/* define: syncfunc_FREE
 * Static initializer. */
#define syncfunc_FREE \
         { 0, 0, 0, 0, 0, linkd_FREE }

/* function: init_syncfunc
 * Initialisiert alle, außer den optionalen Feldern. */
static inline void init_syncfunc(/*out*/syncfunc_t* sfunc, syncfunc_f mainfct, void* state);

/* function: initcopy_syncfunc
 * Kopiert alle nicht wartebzeogenen Felder src nach dest und überschreibt qid.
 * dest->qid wird auf qid gesetzt. Der Wartelink wird ungültig gesetzt.
 * */
static inline void initcopy_syncfunc(/*out*/syncfunc_t* __restrict__ dest, syncfunc_t* __restrict__ src);

/* function: initmove_syncfunc
 * Kopiert src nach dest und adaptiert mögliches Links.
 * Der Inhalt von src ist danach ungültig.
 *
 * Unchecked Precondition:
 * o ! isself_linkd(&src->waitnode)
 * o is_aligned_long(src) && is_aligned_long(dest)
 * */
static inline void initmove_syncfunc(/*out*/syncfunc_t* __restrict__ dest, const syncfunc_t* __restrict__ src);


// group: query

/* function: waitnode_syncfunc
 * Liefere die Adresse des Feldes <waitnode>.
 * */
linkd_t* waitnode_syncfunc(syncfunc_t* sfunc);

/* function: err_syncfunc
 * Gibt den mittels <seterr_syncfunc> gesetzten Fehlercode zurück.
 * So kann zwischen normalem Betreten von <end_syncfunc> und Betreten
 * mittels <exit_syncfunc>(sfparam,ERRCODE) unterschieden werden.
 * Der Wert ist auf ECANCELED gestzt, falls die Funktion
 * aussserordentlich terminiert wird.
 * <exit_syncfunc> setztz diesen Wert.
 * <end_syncfunc> setzt ihn beim Betreten auf 0.
 * Wird auch auf den Fehlercode einer abgebrochenen oder erfolgreichen
 * Warteoperation gesetzt. */
static inline int err_syncfunc(const syncfunc_t *sfunc);

/* function: castPwaitnode_syncfunc
 * Caste waitnode nach <syncfunc_t>.
 *
 * Unchecked Precondition:
 * o waitnode != 0 && waitnode == waitnode_syncfunc(sfunc) */
static inline syncfunc_t* castPwaitnode_syncfunc(linkd_t* waitnode);

/* function: iswaiting_syncfunc
 * Returns true, wenn sfunc in einer Warteliste eingebunden ist. */
static inline int iswaiting_syncfunc(const syncfunc_t* sfunc);

/* function: contoffset_syncfunc
 * Gibt gespeicherten Offset von syncfunc_START (siehe <begin_syncfunc>)
 * zu einer Labeladresse. Dieser Offset erlaubt es,
 * die Ausführung an der Stelle des Labels fortzusetzen. */
static inline int16_t contoffset_syncfunc(const syncfunc_t *sfunc);

// group: update

/* function: linkwaitnode_syncfunc
 * Binde sfunc in Warteliste swait ein. Ändert nur <syncfunc_t.waitnode>. */
void linkwaitnode_syncfunc(syncfunc_t* sfunc, struct syncwait_t* swait);

/* function: seterr_syncfunc
 * Setzt <syncfunc_t.err> auf err. */
static inline void seterr_syncfunc(syncfunc_t *sfunc, int err);

/* function: unlink_syncfunc
 * Entfernt sfunc aus Warteliste.
 * Inhalt von <waitnode> ist danach unverändert, aber Nachbarknoten verweisen nicht mehr auf sfunc. */
void unlink_syncfunc(syncfunc_t* sfunc);

/* function: setcontoffset_syncfunc
 * Setzt gespeicherten Offset von syncfunc_START (siehe <begin_syncfunc>)
 * zu einer Labeladresse. Dieser Offset erlaubt es,
 * die Ausführung an der Stelle des Labels fortzusetzen. */
static inline void setcontoffset_syncfunc(syncfunc_t *sfunc, int16_t contoffset);

// group: helper-macros

/* function: getoffset_syncfunc
 * Berechnet Offset des Labels label relativ zu syncfunc_START. */
int16_t getoffset_syncfunc(LABEL label);

/* function: return_syncfunc
 * Kehrt zu aufrufendem <syncrunner_t> zurück.
 * Die nächste Ausführung beginnt direkt nach dem Makro return_syncfunc.
 * Dieses Makro funktioniert nur, wenn es zwischen <begin_syncfunc> und
 * <end_syncfunc> eingebettet wurde. */
void return_syncfunc(syncfunc_param_t *sfparam);

/* function: setrestart_syncfunc
 * Merkt sich Position für die nächste Ausführung direkt nach diesem Makro.
 * Die Ausführung wird nicht unterbrochen. Ein simples return beendet die Ausführung
 * und beim nächsten Start wird die Funktion an dem durch <setrestart_syncfunc> zuletzt gesetzten
 * Wiedereintrittspunkt fortgeführt.
 * Dieses Makro funktioniert nur, wenn es zwischen <begin_syncfunc> und
 * <end_syncfunc> eingebettet wurde. */
void setrestart_syncfunc(syncfunc_param_t *sfparam);

// group: implementation-macros
// Macros which makes implementing a <syncfunc_f> possible.
// They make use of helper-macros

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
static inline void setstate_syncfunc(syncfunc_param_t* sfparam, void* new_state);

/* function: begin_syncfunc
 * Springt zum letzten Punkt, wo die Ausführung beendet wurde.
 * Berechnet auch den Sprungoffset zum Label syncfunc_END, welches von <end_syncfunc>
 * definiert wird. Dieser Offset wird in <endoffset> gespeichert, um das vorzeitige
 * terminieren der Funktion zu ermöglichen.
 *
 * Als Makro implementiert definiert begin_syncfunc zusätzlich das Label syncfunc_START.
 * Es findet Verwendung bei der Berechnung von Sprungzielen.
 *
 * */
void begin_syncfunc(const syncfunc_param_t *sfparam);

/* function: end_syncfunc
 * Kehrt zum aufrufenden <syncrunner_t> mittels return zurück, nachdem sfparam->iiimpl->exitsf
 * aufgerufen wurde.
 * Setzt zuvor den Fehlercode (siehe <seterr_syncfunc>) auf 0 und führt dann
 * { free_resources } aus.
 * Es wird auch das Label synfunc_END definiert, das direkt am Anfang von { free_resources }
 * steht, so daß ein gesetzter Fehlercode nicht mehr mit 0 überschrieben wird,
 * falls dieses Label mittels <exit_syncfunc> angesprungen wird. */
void end_syncfunc(syncfunc_param_t* sfparam, CODEBLOCK free_resources);

/* function: yield_syncfunc
 * Tritt Prozessor an andere <syncfunc_t> ab.
 * Die nächste Ausführung beginnt nach yield_syncfunc, sofern
 * <begin_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
void yield_syncfunc(const syncfunc_param_t* sfparam);

/* function: exit_syncfunc
 * Setzt err als Fehlercode und beendet diese Funktion.
 * Konvention: err == 0 meint OK, err > 0 sind System & App. Fehlercodes.
 * Intern wird das Label syncfunc_END, das mittels <end_syncfunc> definiert wird,
 * angesprungen, so dass auch alle belegten Ressourcen freigegeben werden, bevor
 * diese Funktion beendet wird. */
void exit_syncfunc(const syncfunc_param_t * sfparam, int err);

/* function: wait_syncfunc
 * Fügt sfparam->sfunc in waitlist zum Warten ein.
 * Gibt 0 zurück, wenn das Warten erfolgreich war.
 * Der Wert ENOMEM zeigt an, daß nicht genügend Ressourcen bereitstanden
 * und die Warteoperation abgebrochen oder gar nicht erst gestartet wurde.
 * Andere Fehlercodes sind auch möglich.
 * Die nächste Ausführung beginnt nach wait_syncfunc, sofern
 * <begin_syncfunc> am Anfang dieser Funktion aufgerufen wird.
 *
 * Unchecked Precondition:
 * o sfparam will be evaluated more than once (implemented as macro)
 *   ==> Only use a name as argument for sfparam; never use an expression with side effects. */
int wait_syncfunc(const syncfunc_param_t *sfparam, struct syncwait_t * waitlist);

/* function: spinwait_syncfunc
 * Testet mit jeder Ausführung die Bedingung condition.
 * Ist diese wahr, wird die Schleife beendet und mit dem darauffolgenden
 * Befehl fortgefahren. Ist condition falsch, wird an den Aufrufer zurückgegeben
 * mittels return. Die darauffolgende nächste Ausführen testet
 * dann wieder die Bedingung condition. */
void spinwait_syncfunc(const syncfunc_param_t *sfparam, CODEBLOCK condition);


// section: inline implementation

// group: syncfunc_t

/* define: begin_syncfunc
 * Implementiert <syncfunc_t.begin_syncfunc>. */
#define begin_syncfunc(sfparam) \
         ( (void) __extension__ ({                    \
            goto * (void*) ( (intptr_t)               \
                  &&syncfunc_START                    \
                  + (sfparam)->sfunc->contoffset);    \
         }));                                         \
         syncfunc_START: /* start init */             \
         (sfparam)->sfunc->endoffset =                \
               getoffset_syncfunc(syncfunc_END)       \
         /* ... user init ... */                      \

/* define: castPwaitnode_syncfunc
 * Implementiert <syncfunc_t.castPwaitnode_syncfunc>. */
static inline syncfunc_t* castPwaitnode_syncfunc(linkd_t* waitnode)
{
         return (syncfunc_t*) ((uint8_t*) (waitnode) - offsetof(syncfunc_t, waitnode));
}

/* define: contoffset_syncfunc
 * Implementiert <syncfunc_t.contoffset_syncfunc>. */
static inline int16_t contoffset_syncfunc(const syncfunc_t *sfunc)
{
         return sfunc->contoffset;
}

/* define: end_syncfunc
 * Implementiert <syncfunc_t.end_syncfunc>. */
#define end_syncfunc(sfparam, free_resources) \
         seterr_syncfunc((sfparam)->sfunc, 0);  \
         syncfunc_END:                          \
         { free_resources }                     \
         (sfparam)->iimpl->exitsf((sfparam));   \
         return


/* define: err_syncfunc
 * Implementiert <syncfunc_t.err_syncfunc>. */
static inline int err_syncfunc(const syncfunc_t *sfunc)
{
         return sfunc->err;
}

/* define: exit_syncfunc
 * Implementiert <syncfunc_t.exit_syncfunc>. */
#define exit_syncfunc(sfparam, err) \
         do {                                     \
            seterr_syncfunc(sfparam->sfunc, err); \
            goto syncfunc_END;                    \
         } while(0)

/* define: getoffset_syncfunc
 * Implementiert <syncfunc_t.getoffset_syncfunc>. */
#define getoffset_syncfunc(label) \
         ( __extension__ ({                                                            \
            static_assert( ((intptr_t)&&label - (intptr_t)&&syncfunc_START) <= 32767   \
                        && ((intptr_t)&&label - (intptr_t)&&syncfunc_START) >= -32768, \
                           "cast to int16_t possible?");                               \
            (int16_t) ((intptr_t)&&label - (intptr_t)&&syncfunc_START);                \
         }))

/* define: init_syncfunc
 * Implementiert <syncfunc_t.init_syncfunc>. */
static inline void init_syncfunc(/*out*/syncfunc_t* sfunc, syncfunc_f mainfct, void* state)
{
         sfunc->mainfct = mainfct;
         sfunc->state = state;
         sfunc->contoffset = 0;
         sfunc->endoffset = 0;
         sfunc->err = 0;
         initinvalid_linkd(&sfunc->waitnode);
}

/* define: initcopy_syncfunc
 * Implementiert <syncfunc_t.initcopy_syncfunc>. */
static inline void initcopy_syncfunc(/*out*/syncfunc_t* __restrict__ dest, syncfunc_t* __restrict__ src)
{
         static_assert( 4*sizeof(long) >= offsetof(syncfunc_t,waitnode)
                        && 3*sizeof(long) <= offsetof(syncfunc_t,waitnode), "supports long copy");
         ((long*)dest)[0] = ((const long*)src)[0];
         ((long*)dest)[1] = ((const long*)src)[1];
         ((long*)dest)[2] = ((const long*)src)[2];
         if (4*sizeof(long) <= offsetof(syncfunc_t,waitnode)) {
         ((long*)dest)[3] = ((const long*)src)[3];
         }
         initinvalid_linkd(&dest->waitnode);
}

/* define: initmove_syncfunc
 * Implementiert <syncfunc_t.initmove_syncfunc>. */
static inline void initmove_syncfunc(/*out*/syncfunc_t* __restrict__ dest, const syncfunc_t* __restrict__ src)
{
         static_assert( 5*sizeof(long) <= sizeof(syncfunc_t)
                        && 6*sizeof(long) >= sizeof(syncfunc_t), "supports long copy");
         ((long*)dest)[0] = ((const long*)src)[0];
         ((long*)dest)[1] = ((const long*)src)[1];
         ((long*)dest)[2] = ((const long*)src)[2];
         ((long*)dest)[3] = ((const long*)src)[3];
         ((long*)dest)[4] = ((const long*)src)[4];
         if (6*sizeof(long) <= sizeof(syncfunc_t)) {
         ((long*)dest)[5] = ((const long*)src)[5];
         }
         if (iswaiting_syncfunc(src)) {
            relink_linkd(&dest->waitnode);
         }
}

/* define: iswaiting_syncfunc
 * Implementiert <syncfunc_t.iswaiting_syncfunc>. */
static inline int iswaiting_syncfunc(const syncfunc_t *sfunc)
{
         return isvalid_linkd(&sfunc->waitnode);
}

/* define: linkwaitnode_syncfunc
 * Implementiert <syncfunc_t.linkwaitnode_syncfunc>. */
#define linkwaitnode_syncfunc(sfunc, swait) \
         do {                                                  \
            syncfunc_t * _sf = (sfunc);                        \
            addnode_syncwait((swait), waitnode_syncfunc(_sf)); \
         } while(0)

/* define: return_syncfunc
 * Implementiert <syncfunc_t.return_syncfunc>. */
#define return_syncfunc(sfparam) \
         ( (void) __extension__ ({                    \
            __label__ syncfunc_CONTINUE;              \
            (sfparam)->sfunc->contoffset =            \
               getoffset_syncfunc(syncfunc_CONTINUE); \
            return;                                   \
            syncfunc_CONTINUE: ;                      \
         }))

/* define: setrestart_syncfunc
 * Implementiert <syncfunc_t.setrestart_syncfunc>. */
#define setrestart_syncfunc(sfparam) \
         ( (void) __extension__ ({                    \
            __label__ syncfunc_CONTINUE;              \
            (sfparam)->sfunc->contoffset =            \
               getoffset_syncfunc(syncfunc_CONTINUE); \
            syncfunc_CONTINUE: ;                      \
         }))

/* define: setcontoffset_syncfunc
 * Implementiert <syncfunc_t.setcontoffset_syncfunc>. */
static inline void setcontoffset_syncfunc(syncfunc_t *sfunc, int16_t contoffset)
{
         sfunc->contoffset = contoffset;
}

/* define: seterr_syncfunc
 * Implementiert <syncfunc_t.seterr_syncfunc>. */
static inline void seterr_syncfunc(syncfunc_t *sfunc, int err)
{
         sfunc->err = err;
}

/* define: setstate_syncfunc
 * Implementiert <syncfunc_t.setstate_syncfunc>. */
static inline void setstate_syncfunc(syncfunc_param_t *sfparam, void *new_state)
{
         sfparam->sfunc->state = new_state;
}

/* define: state_syncfunc
 * Implementiert <syncfunc_t.state_syncfunc>. */
#define state_syncfunc(sfparam) \
         ((sfparam)->sfunc->state)

/* define: spinwait_syncfunc
 * Implementiert <syncfunc_t.spinwait_syncfunc>. */
#define spinwait_syncfunc(sfparam, condition) \
         do {                                   \
            setrestart_syncfunc((sfparam));     \
            if (!(__extension__(condition))) {  \
               return;                          \
            }                                   \
         } while(0)

/* define: wait_syncfunc
 * Implementiert <syncfunc_t.wait_syncfunc>. */
#define wait_syncfunc(sfparam, waitlist) \
         ( __extension__ ({                                    \
            (sfparam)->iimpl->waitsf((sfparam), (waitlist));   \
            return_syncfunc(sfparam);                          \
            err_syncfunc((sfparam)->sfunc);                    \
         }))

/* define: waitnode_syncfunc
 * Implementiert <syncfunc_t.waitnode_syncfunc>. */
#define waitnode_syncfunc(sfunc) \
         (&(sfunc)->waitnode)

/* define: yield_syncfunc
 * Implementiert <syncfunc_t.yield_syncfunc>. */
#define yield_syncfunc(sfparam) \
         return_syncfunc(sfparam)

#endif
