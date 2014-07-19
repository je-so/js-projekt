/* title: SyncFunc

   Definiert einen Ausführungskontext (execution context)
   für Funktionen der Signatur <syncfunc_f>.

   Im einfachsten Fall ist <syncfunc_t> ein einziger
   Funktionszeiger <syncfunc_f>.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncfunc.h
    Header file <SyncFunc>.

   file: C-kern/task/syncfunc.c
    Implementation file <SyncFunc impl>.
*/
#ifndef CKERN_TASK_SYNCFUNC_HEADER
#define CKERN_TASK_SYNCFUNC_HEADER

// forward
struct syncrunner_t;
struct syncwait_condition_t;

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
 * sfcmd    - Ein das auszuführenede Kommando beschreibende Wert aus <syncfunc_cmd_e>.
 *
 * Return:
 * Der Rückgabewert ist das vom Aufrufer auszuführende Kommando – auch aus <syncfunc_cmd_e>. */
typedef int (* syncfunc_f) (syncfunc_param_t * sfparam, uint32_t sfcmd);

/* enums: syncfunc_cmd_e
 * Kommando Parameter. Wird verwendet für sfcmd Parameter in <syncfunc_f>
 * und als »return« Wert von <syncfunc_f>.
 *
 * syncfunc_cmd_RUN         - *Ein*: Das Kommando wird von <execmd_syncfunc> verarbeitet und
 *                            startet die Ausführung an einer zumeist mit »ONRUN:« gekennzeichneten Stelle.
 *                            *Aus*: Als Rückgabewert verlangt es, die nächste Ausführung mit dem Kommando <syncfunc_cmd_RUN>
 *                            zu starten – <syncfunc_param_t.contlabel> ist ungültig.
 * syncfunc_cmd_CONTINUE    - *Ein*: Das Kommando wird von <execmd_syncfunc> verarbeitet und
 *                            setzt die Ausführung an der Stelle, die in <syncfunc_param_t.contlabel> gespeichert wurde.
 *                            *Aus*: Als Rückgabewert verlangt es, die nächste Ausführung mit dem Kommando <syncfunc_cmd_CONTINUE>
 *                            zu starten, genau an der Stelle, die vor dem »return« in <syncfunc_param_t.contlabel> abgelegt wurde.
 * syncfunc_cmd_EXIT        - *Ein*: Das Kommando wird von <execmd_syncfunc> verarbeitet und
 *                            startet die Ausführung an einer zumeist mit »ONEXIT:« gekennzeichneten Stelle.
 *                            Die Umgebung hat zu wenig Ressourcen, um die Funktion weiter auszuführen. Deshalb sollte sie
 *                            alle belegten und durch <syncfunc_param_t.state> referenzierten Ressourcen freigeben.
 *                            Danach mit dem Rückgabewert <syncfunc_cmd_EXIT> zurückkehren – jeder andere »return« Wert wird aber
 *                            auch als <syncfunc_cmd_EXIT> interpretiert.
 *                            *Aus*: Als Rückgabewert verlangt es, die Funktion nicht mehr aufzurufen, da die Berechnung beendet wurde.
 *                            Der als Erfolgsmeldung interpretierte Rückgabewert muss vorher in <syncfunc_param_t.retcode> abgelegt werden.
 *                            Die Funktion <syncfunc_t.exit_syncfunc> übernimmt diese Aufgabe. Wert 0 wird als Erfolg und ein Wert > 0
 *                            als Fehlercode interpretiert.
 * syncfunc_cmd_WAIT       -  *Ein*: Wird für die Kommandoeingabe nicht verwendet.
 *                            *Aus*: Als Rückgabewert bedeutet es, daß vor der nächsten Ausführung die durch die Variable
 *                            <syncfunc_param_t.contlabel> referenzierte Bedingung erfüllt sein muss. Die Ausführung pausiert
 *                            solange und wird mit der Erfüllung der Bedingung durch das Kommando <syncfunc_cmd_CONTINUE>
 *                            wieder aufgenommen. Also genau an der Stelle wieder gestartet, die vor dem »return«
 *                            in <syncfunc_param_t.contlabel> abgelegt wurde. Falls die Warteoperation mangels Ressourcen
 *                            nicht durchgeführt werden kann, wird in <syncfunc_param_t.waiterr> ein Fehlercode abgelegt,
 *                            der nur bei der nächsten Ausführung mit Kommando <syncfunc_cmd_CONTINUE> gültig ist.
 *                            Der Wert 0 zeigt eine gültige Warteoperation an. Der spezielle Wert 0 in <syncfunc_param_t.condition>
 *                            bedeutet, daß auf das Ende der zuletzt erzeugten <syncfunc_t> gewartet werdfen soll – siehe <syncrunner_t>.
 *                            Die Funktionen <wait_syncfunc> und <waitexit_syncfunc> implementieren dieses Protokoll.
 * */
enum syncfunc_cmd_e {
   syncfunc_cmd_RUN,
   syncfunc_cmd_CONTINUE,
   syncfunc_cmd_EXIT,
   syncfunc_cmd_WAIT
} ;

typedef enum syncfunc_cmd_e syncfunc_cmd_e;

/* enums: syncfunc_opt_e
 * Bitfeld das die vorhandenen Felder in <syncfunc_t> kodiert.
 *
 * syncfunc_opt_NONE      - Weder <syncfunc_t.state> noch <syncfunc_t.contlabel> sind vorhanden.
 * syncfunc_opt_STATE     - Das Feld <syncfunc_t.state> ist vorhanden.
 * syncfunc_opt_CONTLABEL - Das Feld <syncfunc_t.contlabel> ist vorhanden.
 * syncfunc_opt_ALL       - Beide Felder <syncfunc_t.state> und <syncfunc_t.contlabel> sind vorhanden. */
enum syncfunc_opt_e {
   syncfunc_opt_NONE      = 0,
   syncfunc_opt_STATE     = 1,
   syncfunc_opt_CONTLABEL = 2,
   syncfunc_opt_ALL       = 3
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
   struct syncrunner_t * syncrun;
   /* variable: state
    * Ein- Ausgabe-Param: Der gespeicherte Funktionszustand.
    * Muss von der Funktion verwaltet werden. */
   void *      state;
   /* variable: contlabel
    * Ein- Ausgabe-Param: Die Stelle, and der mit der Ausführung weitergemacht werden soll.
    * Nur gültig, wenn Parameter sfcmd den Wert <syncfunc_cmd_CONTINUE> besitzt.
    * Der Wert wird nach »return« gespeichert, wenn die Funktion <syncfunc_cmd_CONTINUE>
    * oder <syncfunc_cmd_WAIT> zurückgibt. */
   void *      contlabel;
   /* variable: condition
    * Ausgabe-Param: Referenziert die Bedingung, auf die gewartet werden soll.
    * Der Wert wird nach »return« verwendet, wenn die Funktion den Wert <syncfunc_cmd_WAIT> zurückgibt. */
   struct syncwait_condition_t
             * condition;
   /* variable: waiterr
    * Eingabe-Param: Das Ergebnis der Warteoperation.
    * Eine 0 zeigt Erfolg an, != 0 ist ein Fehlercode – etwa ENOMEM wenn zu wenig Ressourcen
    * frei waren, um fie Funktion in die Warteliste einzutragen.
    * Nur gültig, wenn das Kommando <syncfunc_cmd_CONTINUE> ist und zuletzt die Funktion mit
    * »return <syncfunc_cmd_WAIT>« beendet wurde. */
    int        waiterr;
   /* variable: retcode
    * Ein- Ausgabe-Param: Der Returnwert der Funktion.
    * Beendet sich die Funktion mit »return <syncfunc_cmd_EXIT>«, dann steht in diesem Wert
    * der Erfolgswert drin (0 für erfolgreich, != 0 Fehlercode).
    * Der Eingabewert ist nur gültig, wenn das Kommando <syncfunc_cmd_CONTINUE> ist, zuletzt
    * die Funktion mit »return <syncfunc_cmd_WAIT>« beendet wurde, wobei <condition> 0 war.
    * Der Wert trägt dann den Erfolgswert der Funktion, auf deren Ende gewartet wurde. */
   int         retcode;
};

// group: lifetime

/* define: syncfunc_param_FREE
 * Static initializer. */
#define syncfunc_param_FREE \
         { 0, 0, 0, 0, 0, 0 }


/* struct: syncfunc_t
 * Der Ausführungskontext einer Funktion der Signatur <syncfunc_f>.
 * Dieser wird von einem <syncrunner_t> verwaltet, der alle
 * verwalteten Funktionen nacheinander aufruft. Durch die Ausführung
 * nacheinander und den Verzicht auf präemptive Zeiteinteilung,
 * ist die Ausführung synchron. Dieses kooperative Multitasking zwischen
 * allen Funktionen eines einzigen <syncrunner_t>s erlaubt und gebietet
 * den Verzicht Locks verzichtet werden (siehe <mutex_t>). Warteoperationen
 * müssen mittels einer <syncwait_condition_t> durchgeführt werden.
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
 * >                     // syncfunc_cmd_EXIT empfangen wird
 * >    execmd_syncfunc(sfparam, sfcmd, ONRUN, ONERR);
 * >    goto ONERR;
 * > ONRUN:
 * >    void * state = malloc(...);
 * >    setstate_syncfunc(sfparam, state);
 * >    ...
 * >    yield_syncfunc(sfparam);
 * >    ...
 * >    syncwait_condition_t * condition = ...;
 * >    err = wait_syncfunc(sfparam, condition);
 * >    if (err) goto ONERR;
 * >    ...
 * >    exit_syncfunc(sfparam, 0);
 * > ONERR:
 * >    free( getstate_syncfunc(sfparam) );
 * >    exit_syncfunc(sfparam, err);
 * > }
 * */
struct syncfunc_t {
   // group: private fields
   /* variable: mainfct
    * Zeiger auf immer wieder auszuführende Funktion. */
   syncfunc_f  mainfct;
   /* variable: state
    * *Optional*: Zeigt auf gesicherten Zustand der Funktion.
    * Anfangs kann dieser Wert 0 sein. Der Speicher wird von <mainfct> verwaltet. */
   void *      state;
   /* variable: contlabel
    * *Optional*: Speichert Labeladresse und erlaubt die Ausführung an dieser Stelle fortzusetzen.
    * Fungiert als Wert des »Instruction Pointer«.
    * Diese GCC Erweiterung wird folgendermaßen genutzt,
    * Erläuterung auf https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html.
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
    * >       sfparam->contlabel = &&continue_after_wait;
    * >       // yield processor to other functions
    * >       return syncfunc_cmd_CONTINUE;
    * >       // execution continues here
    * >       continue_after_wait: ;
    * >    }
    * >    ...
    * > } */
   void *      contlabel;
};

// group: lifetime

/* define: syncfunc_FREE
 * Static initializer. */
#define syncfunc_FREE \
         { 0, 0, 0 }

// group: query

/* function: getsize_syncfunc
 * Returns the size in bytes of a <syncfunc_t> with optional fields encoded in optfields.
 * See also <syncfunc_opt_e>. */
size_t getsize_syncfunc(const syncfunc_opt_e optfields);

/* function: optstate_syncfunc
 * Gebe den Wert des optionalen Feldes <state> oder 0 zurück.
 * Dabei bestimmt Parameter optfields, welche optionalen Felder verfügbar sind. */
void * optstate_syncfunc(const syncfunc_t * sfunc, const syncfunc_opt_e optfields);

/* function: optcontlabel_syncfunc
 * Gebe den Wert des optionalen Feldes <contlabel> oder 0 zurück.
 * Dabei bestimmt Parameter optfields, welche optionalen Felder verfügbar sind. */
void * optcontlabel_syncfunc(const syncfunc_t * sfunc, const syncfunc_opt_e optfields);

// group: update

/* function: setall_syncfunc
 * Setzt alle Felder in <syncfunc_t>.
 * Der Parameter optfields zeigt an, welche optionalen Felder
 * vorhanden sind. Nicht vorhandene Felder (Bits gelöscht)
 * werden ignoriert. */
void setall_syncfunc(syncfunc_t * sfunc, const syncfunc_opt_e optfields, syncfunc_f mainfct, void * state, void * contlabel);

// group: implementation-support
// Macros which makes implementing a <syncfunc_f> possible.

/* function: getstate_syncfunc
 * Liest Zustand der aktuell ausgeführten Funktion.
 * Der Wert 0 ist ein ungültiger Wert und zeigt an, daß
 * der Zustand noch nicht gesetzt wurde.
 * Der Wert wird aber nicht aus <syncfunc_t> sondern
 * aus dem Parameter <syncfunc_param_t> herausgelesen.
 * Er trägt eine Kopie des Zustandes und
 * andere nützliche, über <syncfunc_t> hinausgehende Informationen. */
void * getstate_syncfunc(const syncfunc_param_t * sfparam);

/* function: setstate_syncfunc
 * Setzt den Zustand der gerade ausgeführten Funktion.
 * Wurde der Zustand gelöscht, ist der Wert 0 zu setzen.
 * Der Wert wird aber nicht nach <syncfunc_t> sondern
 * in den Parameter <syncfunc_param_t> geschrieben.
 * Er trägt eine Kopie des Zustandes und
 * andere nützliche, über <syncfunc_t> hinausgehende Informationen. */
void setstate_syncfunc(syncfunc_param_t * sfparam, void * new_state);

/* function: execmd_syncfunc
 * Springt zu onrun, onexit oder zu einer vorher gespeicherten Adresse.
 * Die ersten beiden Parameter müssen die Namen der Parameter von der aktuell
 * ausgeführten <syncfunc_f> sein. Das Verhalten des Makros hängt vom zweiten
 * Parameter sfcmd ab. Der erste ist nur von Interesse, falls sfcmd == <syncfunc_cmd_CONTINUE>.
 *
 * Verarbeitete sfcmd Werte:
 * syncfunc_cmd_RUN         - Springe zu Label onrun. Dies ist der normale Ausführungszweig.
 * syncfunc_cmd_CONTINUE    - Springt zu der Adresse, die bei der vorherigen Ausführung in
 *                            <syncfunc_param_t.contlabel> gespeichert wurde.
 * syncfunc_cmd_EXIT        - Springe zu Label onexit. Dieser Zweig sollte alle Ressourcen freigeben
 *                            (free(<getstate_syncfunc>(sfparam)) und <exit_syncfunc>(sfparam,EINTR) aufrufen.
 * Alle anderen Werte       - Das Makro tut nichts und kehrt zum Aufrufer zurück.
 * */
void execmd_syncfunc(const syncfunc_param_t * sfparam, uint32_t sfcmd, LABEL onrun, IDNAME onexit);

/* function: yield_syncfunc
 * Tritt Prozessor an andere <syncfunc_t> ab.
 * Die nächste Ausführung beginnt nach yield_syncfunc, sofern
 * <execmd_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
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
 * <execmd_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
int wait_syncfunc(const syncfunc_param_t * sfparam, struct syncwait_condition_t * condition);

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
 * <execmd_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
int waitexit_syncfunc(const syncfunc_param_t * sfparam, /*out;err*/int * retcode);



// section: inline implementation

// group: syncfunc_t

/* define: execmd_syncfunc
 * Implementiert <syncfunc_t.execmd_syncfunc>. */
#define execmd_syncfunc(sfparam, sfcmd, onrun, onexit) \
         ( __extension__ ({               \
         switch ( (syncfunc_cmd_e)        \
                  (sfcmd)) {              \
         case syncfunc_cmd_RUN:           \
            (void) (sfparam);             \
            goto onrun;                   \
         case syncfunc_cmd_CONTINUE:      \
            goto * (sfparam)->contlabel;  \
         case syncfunc_cmd_EXIT:          \
            (void) (sfparam);             \
            goto onexit;                  \
         default: /*ignoring all other*/  \
            break;                        \
         }}))

/* define: exit_syncfunc
 * Implementiert <syncfunc_t.exit_syncfunc>. */
#define exit_syncfunc(sfparam, _rc) \
         {                                \
            (sfparam)->retcode = (_rc);   \
            return syncfunc_cmd_EXIT;     \
         }

/* define: getsize_syncfunc
 * Implementiert <syncfunc_t.getsize_syncfunc>. */
#define getsize_syncfunc(optfields) \
         ( __extension__ ({                     \
            syncfunc_opt_e _opt = (optfields);  \
            sizeof(syncfunc_f)                  \
            + ((_opt & syncfunc_opt_STATE)      \
              ? sizeof(void*) : 0)              \
            + ((_opt & syncfunc_opt_CONTLABEL)  \
              ? sizeof(void*) : 0);             \
         }))

/* define: getstate_syncfunc
 * Implementiert <syncfunc_t.getstate_syncfunc>. */
#define getstate_syncfunc(sfparam) \
         ((sfparam)->state)

/* define: optcontlabel_syncfunc
 * Implementiert <syncfunc_t.optcontlabel_syncfunc>. */
#define optcontlabel_syncfunc(sfunc, optfields) \
         ( __extension__ ({                     \
            syncfunc_t * _sf0 = (sfunc);        \
            void ** _sf = &_sf0->state;         \
            syncfunc_opt_e _opt = (optfields);  \
            if (_opt & syncfunc_opt_STATE) {    \
               ++ _sf;                          \
            }                                   \
            (_opt & syncfunc_opt_CONTLABEL)     \
            ? *_sf : (void*)0;                  \
         }))

/* define: optstate_syncfunc
 * Implementiert <syncfunc_t.optstate_syncfunc>. */
#define optstate_syncfunc(sfunc, optfields) \
         ( __extension__ ({                     \
            syncfunc_t * _sf0 = (sfunc);        \
            void ** _sf = &_sf0->state;         \
            syncfunc_opt_e _opt = (optfields);  \
            (_opt & syncfunc_opt_STATE)         \
            ? *_sf : (void*)0;                  \
         }))

/* define: setall_syncfunc
 * Implementiert <syncfunc_t.setall_syncfunc>. */
#define setall_syncfunc(sfunc, optfields, _mainfct, _state, _contlabel) \
         do {                                   \
            syncfunc_t * _sf0 = (sfunc);        \
            syncfunc_opt_e _opt = (optfields);  \
            void ** _sf;                        \
            _sf0->mainfct = _mainfct;           \
            _sf = &_sf0->state;                 \
            if (_opt & syncfunc_opt_STATE) {    \
               *_sf = _state;                   \
               ++_sf;                           \
            }                                   \
            if (_opt & syncfunc_opt_CONTLABEL){ \
               *_sf = _contlabel;               \
            }                                   \
         } while(0)

/* define: setstate_syncfunc
 * Implementiert <syncfunc_t.setstate_syncfunc>. */
#define setstate_syncfunc(sfparam, new_state) \
         do { (sfparam)->state = (new_state) ; } while(0)

/* define: wait_syncfunc
 * Implementiert <syncfunc_t.wait_syncfunc>. */
#define wait_syncfunc(sfparam, _condition) \
         ( __extension__ ({                                 \
            __label__ continue_after_wait;                  \
            (sfparam)->condition = _condition;              \
            (sfparam)->contlabel = &&continue_after_wait;   \
            return syncfunc_cmd_WAIT;                       \
            continue_after_wait: ;                          \
            (sfparam)->waiterr;                             \
         }))

/* define: waitexit_syncfunc
 * Implementiert <syncfunc_t.waitexit_syncfunc>. */
#define waitexit_syncfunc(sfparam, _retcode) \
         ( __extension__ ({                                 \
            __label__ continue_after_wait;                  \
            (sfparam)->condition = 0;                       \
            (sfparam)->contlabel = &&continue_after_wait;   \
            return syncfunc_cmd_WAIT;                       \
            continue_after_wait: ;                          \
            *(_retcode) = (sfparam)->retcode;               \
            (sfparam)->waiterr;                             \
         }))

/* define: yield_syncfunc
 * Implementiert <syncfunc_t.yield_syncfunc>. */
#define yield_syncfunc(sfparam) \
         ( __extension__ ({                                 \
            __label__ continue_after_yield;                 \
            (sfparam)->contlabel = &&continue_after_yield;  \
            return syncfunc_cmd_CONTINUE;                   \
            continue_after_yield: ;                         \
         }))

#endif
