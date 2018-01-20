/* title: IOList

   _SHARED_

   Manage list or sequence of I/O operations.
   I/O operations are performed by special I/O threads (<iothread_t>).

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/io/iosys/iolist.h
    Header file <IOList>.

   file: C-kern/io/iosys/iolist.c
    Implementation file <IOList impl>.
*/
#ifndef CKERN_IO_IOSYS_IOLIST_HEADER
#define CKERN_IO_IOSYS_IOLIST_HEADER

// forward
struct thread_t;
struct iothread_t;
struct eventcount_t;

// === exported typees
struct iolist_t;
struct iotask_t;

/* enums: ioop_e
 * Bezeichnet die auszuführende Operation. Siehe <iotask_t.op>.
 *
 * ioop_NOOP  - Keine Operation, ignoriere Eintrag.
 * ioop_READ  - Initiiere lesende Operation.
 * ioop_WRITE - Initiiere schreibende Operation.
 */
typedef enum ioop_e {
   ioop_NOOP,
   ioop_READ,
   ioop_WRITE,
} ioop_e;

#define ioop__NROF (ioop_WRITE+1)

/* enums: iostate_e
 * Bezeichnet den Zustand einer <iotask_t>.
 *
 * iostate_NULL      - Ungesetzter Zustand.
 * iostate_QUEUED    - Gültiger Eintrag, der auf Bearbeitung wartet.
 * iostate_OK        - Zeigt eine ohne Fehler abgeschlossene Operation an.
 * iostate_ERROR     - Zeigt eine fehlerhaft abgeschlossene Operation an.
 * iostate_CANCELED  - Zeigt eine stornierte Operation an.
 *                     Die Operation kann nur storniert werden (siehe <iolist_t.cancelall_iolist>), wenn sie noch
 *                     nicht aus der <iolist_t> entfernt wurde, also noch nicht durch einen <iothread_t> bearbeitet wurde.
 *                     Der Wert von <iotask_t.state> wird auf <iostate_CANCELED>
 *                     und zusätzlich der Wert <iotask_t.err> auf ECANCELED gesetzt.
 * iostate_READY_MASK - (iostate & iostate_READY_MASK) == iostate_NULL
 *                      || (iostate & iostate_READY_MASK) == iostate_OK
 *                      || (iostate & iostate_READY_MASK) == iostate_ERROR
 *                      || (iostate & iostate_READY_MASK) == iostate_CANCEL
 */
typedef enum iostate_e {
   iostate_NULL      = 0,
   iostate_QUEUED    = 1,
   iostate_OK        = 2,
   iostate_ERROR     = 4,
   iostate_CANCELED  = 6
} iostate_e;

#define iostate_READY_MASK 6


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_iosys_iolist
 * Test <iolist_t> functionality. */
int unittest_io_iosys_iolist(void);
#endif


/* struct: iotask_t
 * Beschreibt den Zustand einer I/O Operation.
 *
 * _SHARED_(process, 1R, 1W):
 * Siehe <iolist_t>.
 *
 * Init-Operationen:
 * Das Nutzungsrecht an <eventcount_t> – übergeben durch Parameter readycount - geht temporär an iotask über.
 * Erst wenn dieser readycount nicht mehr benötigt, z.B. weil alle IOs vollständig abgearbeitet wurden, darf
 * readycount freigegeben werden. Ein Counter darf auch zwischen mehreren <iotask_t> geteilt werden,
 * da er das Schreiben durch mehr als einen <iothread_t> unterstützt. */
typedef struct iotask_t {
   // group: set by iothread; read by owner

   /* variable: iolist_next
    * Erlaubt die Verlinkung aller in die <iolist_t> eingefügten <iotask_t>. */
   struct iotask_t* iolist_next;

   union {
      /* variable: err
       * Der Fehlercode einer fehlerhaft ausgeführten Operation. Nur gültig, falls <state> einen Fehler anzeigt. */
      int      err;
      /* variable: bytesrw
       * Die Anzahl fehlerfrei übertragener Daten. Nur gültig, falls <state> keinen Fehler anzeigt. */
      size_t   bytesrw;
   };
   /* variable: state
    * Der Zustand der Operation. Die möglichen Wert werden durch <iostate_e> beschrieben. */
   uint8_t  state; // <iostate_e>

   // group: set by owner; read by <iothread_t>
   // See <initreadp_iotask>, <initread_iotask>, <initwritep_iotask>, and <initwrite_iotask>

   /* variable: op
    * Gibt die auszuführende Operation an: Lesen oder Schreiben. Wird mit Wert aus <ioop_e> belegt. */
   uint8_t  op;
   /* variable: ioc
    * I/O Kanal von dem gelesen oder auf den geschrieben werden soll. */
   sys_iochannel_t ioc;
   /* variable: offset
    * Byteoffset, ab dem gelesen oder geschrieben werden soll.
    * Sollte im Falle von O_DIRECT ein vielfaches der Systempagesize sein. */
   off_t    offset;
   /* variable: bufaddr
    * Die Startadresse des zu transferierenden Speichers.
    * Beim Lesen werden die Daten nach [bufaddr..buffaddr+bufsize-1] geschrieben,
    * beim Schreiben von [bufaddr..buffaddr+bufsize-1] in den I/O Kanal transferiert. */
   void*    bufaddr;
   /* variable: bufsize
    * Die Anzahl zu transferierender Datenbytes.
    * Sollte im Falle von O_DIRECT ein vielfaches
    * beim Schreiben von [bufaddr..buffaddr+bufsize-1] in den I/O Kanal transferriert.
    * Sollte im Falle von O_DIRECT ein vielfaches der Systempagesize sein. */
   size_t   bufsize;
   /* variable: readycount
    * Pro fertig bearbeitetem <iotask_t> wird der counter um eins inkrementiert.
    * Der Wert kann 0 sein. */
   struct eventcount_t* readycount;
} iotask_t;

// group: lifetime

/* define: iotask_FREE
 * Statischer Initialisierer. */
#define iotask_FREE \
         { 0, { 0 }, iostate_NULL, ioop_NOOP, sys_iochannel_FREE, 0, 0, 0, 0 }

/* function: initreadp_iotask
 * Initialisiert ioop zum positionierten Lesen. Die aktuelle Fileposition wird dabei nicht verändert.
 * Das Nutzungsrecht an readycount geht temporär an iotask über.
 * */
static inline void initreadp_iotask(/*out*/iotask_t* iotask, sys_iochannel_t ioc, size_t size, void* buffer/*[size]*/, off_t off/*>= 0*/, /*own*/struct eventcount_t* readycount/*could be 0*/);

/* function: initread_iotask
 * Initialisiert ioop zum Lesen ab der aktuellen Fileposition. Lese und Schreiboperationen
 * teilen sich dieselbe Fileposition.
 * Das Nutzungsrecht an readycount geht temporär an iotask über. */
static inline void initread_iotask(/*out*/iotask_t* iotask, sys_iochannel_t ioc, size_t size, void* buffer/*[size]*/, /*own*/struct eventcount_t* readycount/*could be 0*/);

/* function: initwritep_iotask
 * Initialisiert ioop zum positionierten Schreiben. Die aktuelle Fileposition wird dabei nicht verändert.
 * Das Nutzungsrecht an readycount geht temporär an iotask über. */
static inline void initwritep_iotask(/*out*/iotask_t* iotask, sys_iochannel_t ioc, size_t size, const void* buffer/*[size]*/, off_t offset/*>= 0*/, /*own*/struct eventcount_t* readycount/*could be 0*/);

/* function: initwrite_iotask
 * Initialisiert ioop zum Schreiben ab der aktuellen Fileposition. Lese und Schreiboperationen
 * teilen sich dieselbe Fileposition.
 * Das Nutzungsrecht an readycount geht temporär an iotask über. */
static inline void initwrite_iotask(/*out*/iotask_t* iotask, sys_iochannel_t ioc, size_t size, const void* buffer/*[size]*/, /*own*/struct eventcount_t* readycount/*could be 0*/);

// group: query

/* function: isvalid_iotask
 * Gibt true zurück, wenn ioop gültige Werte (außer ioc) enthält. */
static inline int isvalid_iotask(const volatile iotask_t* iotask);

// group: update

/* function: setoffset_iotask
 * Setzt den offset der positionierten Lese- oder Schreiboperation neu. */
static inline void setoffset_iotask(iotask_t* iotask, off_t offset);

/* function: setsize_iotask
 * Setzt die Buffergröße der (positionierten) Lese- oder Schreiboperation neu. */
static inline void setsize_iotask(iotask_t* iotask, size_t size);


/* struct: iolist_t
 * Eine Liste von auszuführenden I/O Operationen.
 * Der Zugriff ist durch ein Spinlock geschützt.
 *
 * _SHARED_(process, 1R, nW):
 * Der <iothread_t> liest Einträge aus der Liste, entfernt diese und bearbeitet sie.
 * Andere Threads, die I/O Operationen ausführen wollen, erzeugen eine oder mehrere
 * Einträge in einer <iolist_t>. Für jedes I/O-Device gibt es in der Regel einen
 * zuständigen <iothread_t> und eine zugehörige <iolist_t>.
 *
 * Writer:
 * Darf neue Elemente vom Typ <iotask_t> einfügen.
 * Aufrufbare Funktionen sind <insertlast_iolist> und <cancelall_iolist>.
 *
 * Reader:
 * Entfernt Elemente aus der Liste mit <tryremovefirst_iolist> und bearbeitet diese dann.
 *
 * */
typedef struct iolist_t {
   // group: private fields
   /* variable: lock
    * Schützt Zugriff auf <size> und <last>. */
   uint8_t  lock;
   /* variable: lock
    * Speichert die Anzahl der über <last> verlinkten <iotask_t>. */
   size_t   size;
   /* variable: last
    * Single linked Liste von <iotask_t>. Verlinkt wird über <iotask_t.iolist_next>.
    * last->next zeigt auf first. last wird nur von den schreibenden Threads
    * verwendet, so daß auf einen Lock verzichtet werden kann. last wird nur dann vom lesenden
    * <iothread_t> auf 0 gesetzt, dann nämlich, wenn <first> == 0. */
   iotask_t* last;
} iolist_t;

// group: lifetime

/* define: iolist_INIT
 * Static initializer. */
#define iolist_INIT \
         { 0, 0, 0 }

/* function: init_iolist
 * Initialisiert iolist als leere Liste. */
void init_iolist(/*out*/iolist_t* iolist);

/* function: free_iolist
 * Setzt alle Felder von iolist auf 0.
 * Ressourcen werden keine freigegeben.
 * Noch verlinkte <iotask_t> werden entfernt und deren state auf <iostate_CANCELED> gesetzt.
 * Diese Funktion darf nur dann aufgerufen werden, wenn garantiert ist, daß kein
 * anderer Thread mehr Zugriff auf iolist hat. */
void free_iolist(iolist_t* iolist);

// group: query

/* function: size_iolist
 * Gibt Anzahl in iolist verlinkter <iotask_t> zurück. */
size_t size_iolist(const iolist_t* iolist);

// group: update

/* function: insertlast_iolist
 * Fügt iot am Ende von iolist ein. Der Inhalt von iot
 * bis auf <iotask_t.iolist_next> wird nicht geändert.
 * Der Miteigentumsrecht an iot wechselt temporär zu iolist, nur solange,
 * bis iot bearbeitet wurde. Dann liegt es automatisch wieder beim Aufrufer.
 *
 * Solange iot noch nicht bearbeitet wurde, darf das Objekt nicht gelöscht werden.
 * Nachdem alle Einträge bearbeitet wurden (<iotask_t.state>), wird der Besitz implizit
 * wieder an den Aufrufer transferiert, wobei der Eintrag mittels <iotask_t.owner_next>
 * immer in der Eigentumsliste des Owners verbleibt, auch während der Bearbeitung
 * durch iolist.
 *
 * Der Parameter thread dient dazu, <thread_t.resume_thread> aufzufrufen, falls
 * die Liste vor dem Einfügen leer war.
 *
 * Unchecked Precondition:
 * - forall (int t = 0; t < nrtask; ++t)
 *      iot[t]->iolist_next == 0
 * */
void insertlast_iolist(iolist_t* iolist, uint8_t nrtask, /*own*/iotask_t* iot[nrtask], struct thread_t* thread/*0 => ignored*/);

/* function: tryremovefirst_iolist
 * Entfernt das erste Elemente aus der Liste und gibt es in iot zurück.
 * iot->iolist_next wird auf 0 gesetzt, alle anderen Felder bleiben unverändert.
 * Ist die Liste leer, wird der Fehler ENODATA zurückgegeben.
 * Nachdem der Aufrufer (<iothread_t>) das Element bearbeitet hat,
 * geht es automatisch an den Eigentümer zurück.
 * Das Feld <iotask_t.state> dokumentiert, wann *iot komplett bearbeitet ist.
 *
 * Returncode:
 * 0       - *iot zeigt auf ehemals erstes Element.
 * ENODATA - Die Liste war leer.
 *
 * Ensures:
 * iot->iolist_next == 0 */
int tryremovefirst_iolist(iolist_t* iolist, /*out*/iotask_t** iot);

/* function: cancelall_iolist
 * Entferne alle noch nicht bearbeiteten <iotask_t> und setze deren state auf <iostate_CANCELED>.
 * Der Fehlercode einer <iotask_t> wird auf ECANCELED gesetzt. */
void cancelall_iolist(iolist_t* iolist);



// section: inline implementation

// group: iotask_t

/* define: initreadp_iotask
 * Implements <iotask_t.initreadp_iotask>. */
static inline void initreadp_iotask(/*out*/iotask_t* iotask, sys_iochannel_t ioc, size_t size, void* buffer/*[size]*/, off_t offset/*>= 0*/, /*own*/struct eventcount_t* readycount/*could be 0*/)
{
         iotask->iolist_next = 0;
         iotask->state = iostate_NULL;
         iotask->op = ioop_READ;
         iotask->ioc = ioc;
         iotask->offset = offset;
         iotask->bufaddr = buffer;
         iotask->bufsize = size;
         iotask->readycount = readycount;
}

/* define: initread_iotask
 * Implements <iotask_t.initread_iotask>. */
static inline void initread_iotask(/*out*/iotask_t* iotask, sys_iochannel_t ioc, size_t size, void* buffer/*[size]*/, /*own*/struct eventcount_t* readycount/*could be 0*/)
{
         iotask->iolist_next = 0;
         iotask->state = iostate_NULL;
         iotask->op = ioop_READ;
         iotask->ioc = ioc;
         iotask->offset = -1;
         iotask->bufaddr = buffer;
         iotask->bufsize = size;
         iotask->readycount = readycount;
}
/* define: initwritep_iotask
 * Implements <iotask_t.initwritep_iotask>. */
static inline void initwritep_iotask(/*out*/iotask_t* iotask, sys_iochannel_t ioc, size_t size, const void* buffer/*[size]*/, off_t offset/*>= 0*/, /*own*/struct eventcount_t* readycount/*could be 0*/)
{
         iotask->iolist_next = 0;
         iotask->state = iostate_NULL;
         iotask->op = ioop_WRITE;
         iotask->ioc = ioc;
         iotask->offset = offset;
         iotask->bufaddr = (void*)(uintptr_t)buffer;
         iotask->bufsize = size;
         iotask->readycount = readycount;
}

/* define: initwrite_iotask
 * Implements <iotask_t.initwrite_iotask>. */
static inline void initwrite_iotask(/*out*/iotask_t* iotask, sys_iochannel_t ioc, size_t size, const void* buffer/*[size]*/, /*own*/struct eventcount_t* readycount/*could be 0*/)
{
         iotask->iolist_next = 0;
         iotask->state = iostate_NULL;
         iotask->op = ioop_WRITE;
         iotask->ioc = ioc;
         iotask->offset = -1;
         iotask->bufaddr = (void*)(uintptr_t)buffer;
         iotask->bufsize = size;
         iotask->readycount = readycount;
}

/* define: isvalid_iotask
 * Implements <iotask_t.isvalid_iotask>. */
static inline int isvalid_iotask(const volatile iotask_t* iotask)
{
         return iotask->bufaddr != 0 && iotask->bufsize != 0 && iotask->op < ioop__NROF;
}

/* define: setoffset_iotask
 * Implements <iotask_t.setoffset_iotask>. */
static inline void setoffset_iotask(iotask_t* iotask, off_t offset)
{
         iotask->offset = offset;
}

/* define: setsize_iotask
 * Implements <iotask_t.setsize_iotask>. */
static inline void setsize_iotask(iotask_t* iotask, size_t size)
{
         iotask->bufsize = size;
}

// group: iolist_t

/* define: free_iolist
 * Implementiert <iolist_t.free_iolist>. */
#define free_iolist(iolist) \
         (cancelall_iolist(iolist))

/* define: init_iolist
 * Implementiert <iolist_t.init_iolist>. */
#define init_iolist(iolist) \
         ((void)(*(iolist) = (iolist_t) iolist_INIT))

/* define: size_iolist
 * Implementiert <iolist_t.size_iolist>. */
#define size_iolist(iolist) \
         ((iolist)->size)

#endif
