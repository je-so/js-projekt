/* title: TestPerformance

   Erlaubt das Ausführen von Performanztests.
   Mit Hilfe von n erzeugten Prozessen mit jeweils m Threads
   werden m*n Testinstanzen aufgerufen und die Zeit der Ausführung
   auf die Mikrosekunde genau gemessen, sofern das OS diese
   Genauigkeit unterstützt.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/test/perftest.h
    Header file <TestPerformance>.

   file: C-kern/test/perftest.c
    Implementation file <TestPerformance impl>.
*/
#ifndef CKERN_TEST_PERFTEST_HEADER
#define CKERN_TEST_PERFTEST_HEADER

// forward;
struct thread_t;

/* typedef: struct perftest_t
 * Export <perftest_t> into global namespace. */
typedef struct perftest_t perftest_t;

/* typedef: struct perftest_it
 * Export <perftest_it> into global namespace. */
typedef struct perftest_it perftest_it;

/* typedef: struct perftest_process_t
 * Export <perftest_instance_t> into global namespace. */
typedef struct perftest_process_t perftest_process_t;

/* typedef: struct perftest_instance_t
 * Export <perftest_instance_t> into global namespace. */
typedef struct perftest_instance_t perftest_instance_t;

/* typedef: struct perftest_info_t
 * Export <perftest_info_t> into global namespace. */
typedef struct perftest_info_t perftest_info_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_perftest
 * Test <perftest_t> functionality. */
int unittest_test_perftest(void);
#endif


/* struct: perftest_it
 * Interface zum Messen einer Testfunktion bzw. Testobjektes.
 *
 * Beispiel:
 * perftest_task_syncrun */
struct perftest_it {
   // group: publi fields
   /* variable: prepare
    * Interfacefunktion wird aus erzeugtem Thread aufgerufen, um Initialisierungen vorzunehmen.
    * Der Wert tinst->nrops (<perftest_instance_t.nrops>) sollte mit der Anzahl an ausgeführten
    * Operationen (z.B. transferierte Messages) der Testfunktion überschrieben werden.
    * In den Feldern <perftest_instance_t.addr> und <perftest_instance_t.size> kann ein allokiertes
    * Objekt beschrieben werden, das nur für diesen Thread bestand hat. */
   int (* prepare)   (perftest_instance_t* tinst);
   /* variable: run
    * Führt die Testfunktion aus, wobei tinst->nrops Wiederholungen derselben Operationen ausgeführt werden.
    * Threadspezifische Daten können aus tinst->addr und tinst->size entnommen werden.
    * Auf Daten, die für alle Testinstanzen gültig sind und in Shared Memory gehalten werden, kann
    * mittels <perftest_t.sharedaddr_perftest>(tinst->ptest) und <perftest_t.sharedsize_perftest>(tinst->ptest)
    * zugegriffen werden. */
   int (* run)       (perftest_instance_t* tinst);
   /* variable: unprepare
    * Gibt die in <prepare> allokierten Ressourcen wieder frei. */
   int (* unprepare) (perftest_instance_t* tinst);
};

/* define: perftest_INIT
 * Statischer Initialisierer. */
#define perftest_INIT(prepare, run, unprepare) \
         { prepare, run, unprepare }


/* struct: perftest_info_t
 * Wird bei der Ausführung von PErformanztests benötigt. Siehe auch <perftest_t.run_perftest>. */
struct perftest_info_t {
   /* variable: iimpl
    * Interface, das Implementierung des Tests beschreibt. Siehe auch <perftest_it>. */
   perftest_it iimpl;
   /* variable: ops_description
    * Beschreibung in Englisch, was eine Operation bedeutet. */
   const char* ops_description;
   /* variable: shared_addr
    * Zeigt auf Shared Memory. */
   void*       shared_addr;
   /* variable: shared_size
    * Größe in Bytes des Shared Memory. */
   size_t      shared_size;
   /* variable: free_shared
    * Gib Shared Memory, der mittelts <shared_addr> und <shared_size> beschrieben wird, frei. */
   int      (* free_shared) (perftest_info_t* info);
};

/* define: perftest_info_INIT
 * Statischer Initialisierer.
 *
 * Parameter:
 * iimpl - Interface <perftest_it>, das auf die Implementierung des Tests zeigt.
 * ops_description - Beschreibt in Englisch, was eine Operation bedeutet. Z.B. "Sending and receiving a message.".
 * shared_addr - Zeigt auf Shared Memory, der für alle Testinsstanzen sichtbar ist bzw. 0.
 * shared_size - Die Größe in Bytes des allokierten Shared Memory oder 0.
 * free_shared_f - Funktionszeiger zur Freigabe des gemeinsamen Speichers oder 0.
 * */
#define perftest_info_INIT(iimpl, ops_description , shared_addr, shared_size, free_shared_f) \
         { iimpl, ops_description, shared_addr, shared_size, free_shared_f }


/* struct: perftest_process_t
 * Von <perftest_t> gestarteter Testprozess, der wiederum zugehörige Testinstanz-Threads startet.
 * Siehe <perftest_instance_t> für die Beschreibung einer Testinstanz. */
struct perftest_process_t  {
   // group: read-only fields
   sys_process_t process;
   /* variable: pid
    * ID des zugeörigen Prozesses (0,1,2,...,<perftest_t.nrprocess>-1). */
   uint16_t      pid;
   /* variable: nrthreads
    * Anzahl Threads in diesem Prozess. */
   uint16_t      nrthread;
   /* variable: tinst
    * Zeigt auf ersten zugeörige Threadinstanz <perftest_instance_t>.
    * Alle zugehörigen Instanzen sind tinst[0..nrthread-1]. */
   perftest_instance_t* tinst/*[nrthread]*/;
};


/* struct: perftest_instance_t
 * Eine von <perftest_process_t> verwalteter Testinstanz-Thread. */
struct perftest_instance_t {
   // group: read-only fields

   /* variable: thread
    * Zeigt auf Thread-Instanz. */
   struct thread_t*    thread;
   /* variable: proc
    * Zeigt zum zugehörigen Prozess dieser Instanz. */
   perftest_process_t* proc;
   /* variable: ptest
    * Zu welchem ptest diese Instanz gehört. */
   perftest_t*         ptest;
   /* variable: tid
    * ID der Testinstanz (0,1,2,...,<perftest_t.nrinstance_perftest>-1).
    *
    * tid == proc->pid * <perftest_t.nrthreads_per_process> + [0..<perftest_t.nrthreads_per_process>-1] */
   uint32_t tid;
   /* variable: usec
    * Benötigte Zeit in Mikrosekunden. */
   int64_t  usec;

   // group: public fields

   /* variable: nrops
    * Defaultwert ist 1. Sollte von <perftest_it.prepare> überschrieben werden. */
   uint64_t nrops;
   /* variable: addr
    * ungültig: ==0, gültig: !=0. Wird von <perftest_it.prepare> gesetzt
    * und mit <perftest_it.unprepare> freigegeben. */
   void*    addr;
   /* variable: size
    * ungültig: ==0, gültig: !=0. Wird von <perftest_it.prepare> gesetzt
    * und mit <perftest_it.unprepare> freigegeben. */
   size_t   size;
};


/* struct: perftest_t
 * Typ, der die Ausführung von Performanztests verwaltet.
 * Erlaubt das Erzeugen von n Prozessen mit jeweils m Threads.
 * Die so erzeugten m*n Testinstanzen rufen eine Testfunktion
 * auf, wobei die Zeit auf die Mikrosekunde genau gemessen wird,
 * die bis zur Beendigung aller Instanzen vergeht.
 * Die exakte Genauigkeit hängt von der Untertützung des OS und
 * der Hardware ab. */
struct perftest_t {
   // group: private fields
   /* variable: pagesize
    * Größe der allokierten <vmpage_t>. */
   size_t         pagesize;
   /* variable: iimpl
    * Interface des implementierenden Objektes. */
   const
   perftest_it*   iimpl;
   sys_iochannel_t pipe[6];
   /* variable: nrinstance
    * Anzahl Testinstanzen, korrespondiert mit Anzahl an gestarteten Threads über alle gestarteten Prozesse. */
   uint32_t       nrinstance;
   /* variable: nrprocess
    * Anzahl Prozesse mit jeweils nrthreads_per_process Testinstanzen. */
   uint16_t       nrprocess;
   /* variable: nrthread_per_process
    * Anzahl Threads per <proc>. */
   uint16_t       nrthread_per_process;
   /* variable: start_time
    * Startzeitpunkt der Messung. */
   struct { int64_t seconds; int32_t nanosec; }
                  start_time;
   /* variable: sshared_addr
    * Siehe <sharedaddr_perftest>. */
   void*          shared_addr;
   /* variable: shared_size
    * Siehe <sharedsize_perftest>. */
   size_t         shared_size;
   perftest_process_t* proc/*[nrprocess]*/;
   perftest_instance_t tinst[/*nrinstance*/];
};

// group: lifetime

/* function: new_perftest
 * Erzeugt eine Perfomanz-Testumgebung von nrprocess Prozessen mit jeweils nrthread_per_process Threads.
 * Diese dienen der Ausführung eines Testobjektes/Testfunktion beschrieben durch das Interface iimpl.
 *
 * Die Ausführung selbst wird mittels <measure_perftest> gestartet.
 *
 * Geborgte Werte:
 * Das durch iimpl referenzierte Interface muss gleich lang wie ptest gültig bleiben.
 */
int new_perftest(/*out*/perftest_t** ptest, const perftest_it* iimpl/*only ref stored*/, uint16_t nrprocess/*>0*/, uint16_t nrthread_per_process/*>0*/);

/* function: delete_perftest
 * Gibt alle durch die Prozesse belegten Ressourcen frei. */
int delete_perftest(perftest_t** ptest);

// group: query

/* function: sharedaddr_perftest
 * Startadresse des gemeinsam von allen Instanzen geteilten Datenbereichs. */
static inline void* sharedaddr_perftest(const perftest_t* ptest);

/* function: sharedsize_perftest
 * Größe in Bytes des gemeinsam von allen Instanzen geteilten Datenbereichs. */
static inline size_t sharedsize_perftest(const perftest_t* ptest);

/* function: nrinstance_perftest
 * Liefert die Anzahl an erzeugten Test-Instanzen (Threads). */
static inline uint32_t nrinstance_perftest(const perftest_t* ptest);

// group: update

/* function: setshared_perftest
 * Setze gemeinsame Daten, die alle Instanzen per <sharedaddr_perftest> und <sharedsize_perftest> abfragen können.
 * Der Speicherbereich, auf den addr zeigt, muß mit <accessmode_SHARED> allokiert worden sein. */
static inline void setshared_perftest(perftest_t* ptest, void* addr, size_t size);

/* function: sharedsize_perftest
 * Liefert die Größe in Bytes des gemeinsam von allen Instanzen geteilten Datenbereichs. */
static inline size_t sharedsize_perftest(const perftest_t* ptest);

// group: measure

/* function: measure_perftest
 * Führt ein Testobjekt/Testfunktion genau einmal aus. Wird die Funktion zweimal aufgerufen,
 * wird EALREADY zurückgegeben. Um mehrmals dieselbe Testfunktion auszuführen, muss ptest
 * vorher freigegeben und wieder intialisiert werden. */
int measure_perftest(perftest_t* ptest, /*out*/uint64_t* nrops, /*out*/uint64_t* usec);

/* function: exec_perftest
 * Führt ein Testobjekt/Testfunktion genau einmal aus.
 * Parameter iimpl beschreibt das Testobjekt (siehe <perftest_it>). Parameter shared_addr und shared_size
 * beschreiben Teestdaten, die für alle Testinstanzen gültig sind. Diese Testdaten müssen in Shared Memory
 * abgelegt sein (siehe auch <accessmode_SHARED>). Es werden nrprocess Prozesse mit jeweils nrthread_per_process
 * Threads pro Prozess angelegt. Insgesamt wreden also nrprocess*nrthread_per_process Testinstanzen ausgeführt.
 * Die Anzahl der ausgeführten Operationen, die sich aus der Summe aller Anzahlen an durch Testinstanzen ausgeführten
 * Operationen zusammensetzt, wird in nrops zurückgegeben. Die Zeit in Mikrosekunden, die zur Ausführung aller
 * Operationen durch alle Testinstanzen benötigt wurde, wird in usec zurückgegeben. */
int exec_perftest(perftest_it* iimpl, void* shared_addr, size_t shared_size, uint16_t nrprocess/*>0*/, uint16_t nrthread_per_process/*>0*/, /*out*/uint64_t* nrops, /*out*/uint64_t* usec);



// section: inline implementation

/* define: nrinstance_perftest
 * Implements <perftest_t.nrinstance_perftest>. */
static inline uint32_t nrinstance_perftest(const perftest_t* ptest)
{
         return ptest->nrinstance;
}

/* define: sharedaddr_perftest
 * Implements <perftest_t.sharedaddr_perftest>. */
static inline void* sharedaddr_perftest(const perftest_t* ptest)
{
         return ptest->shared_addr;
}

/* define: sharedsize_perftest
 * Implements <perftest_t.sharedsize_perftest>. */
static inline size_t sharedsize_perftest(const perftest_t* ptest)
{
         return ptest->shared_size;
}

/* define: setshared_perftest
 * Implements <perftest_t.setshared_perftest>. */
static inline void setshared_perftest(perftest_t* ptest, void* addr, size_t size)
{
         ptest->shared_addr = addr;
         ptest->shared_size = size;
}

#endif
