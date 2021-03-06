/* title: ThreadContext

   Defines the global (thread local) context of a running system thread.
   If more than one thread is running in a process each thread
   has its own context. It contains references to services which
   belong only to one thread and therefore are not thread safe.
   They can not be shared between threads.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/task/threadcontext.h
    Header file <ThreadContext>.

   file: C-kern/task/threadcontext.c
    Implementation file <ThreadContext impl>.
*/
#ifndef CKERN_TASK_THREADCONTEXT_HEADER
#define CKERN_TASK_THREADCONTEXT_HEADER

// import
struct maincontext_t;
struct syncrunner_t;

// === exported types
struct threadcontext_t;

/* typedef: threadcontext_mm_t
 * Definiert als <iobj_T>(mm). */
typedef iobj_T(mm) threadcontext_mm_t;

/* typedef: threadcontext_pagecache_t
 * Definiert als <iobj_T>(pagecache). */
typedef iobj_T(pagecache) threadcontext_pagecache_t;

/* typedef: threadcontext_objectcache_t
 * Definiert als <iobj_T>(objectcache). */
typedef iobj_T(objectcache) threadcontext_objectcache_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_threadcontext
 * Test interface of <threadcontext_t>. */
int unittest_task_threadcontext(void);
#endif


/* struct: threadcontext_t
 * Stores services useable exclusively from one thread.
 *
 * Architecture:
 * Function init_threadcontext calls every initthread_ and other functions in the same order as defined in
 * "C-kern/resource/config/initthread". This database file is checked against the whole project with
 * "C-kern/test/static/check_textdb.sh".
 * */
typedef struct threadcontext_t {
   /* variable: maincontext
    * Points to shared <maincontext_t>. */
   struct maincontext_t *     maincontext;
   /* variable: pagecache
    * Thread local virtual memory page manager. */
   threadcontext_pagecache_t  pagecache;
   /* variable: mm
    * Thread local memory manager. */
   threadcontext_mm_t         mm;
   /* variable: syncrunner
    * Synchronous task support. */
   struct syncrunner_t*       syncrunner;
   /* variable: objectcache
    * Thread local erorr object cache. */
   threadcontext_objectcache_t objectcache;
   /* variable: log
    * Thread local erorr log. */
   ilog_t                     log;
   /* variable: thread_id
    * Identification number which is incremented every time a thread is created.
    * The main thread has id 1. If SIZE_MAX is reached the value is wrapped around
    * to number 2 which may be no more unique.
    * TODO: implement reuse of id
    *       refactor id management into own thread-manager which checks also that threads are alive
    *       make thread-manager check that every thread is alive with tryjoin_thread
    *       + send_interrupt which sets flag !! (-> make all syscalls restartable) .. */
   size_t   thread_id;
   /* variable: initcount
    * Number of correct initialized objects. */
   size_t   initcount;
   /* variable: staticdata
    * Start address of memory block allocated with <thread_stack_t.allocstatic_threadstack>. */
   uint8_t *staticdata;
} threadcontext_t;

// group: lifetime

/* define: threadcontext_FREE
 * Static initializer for <threadcontext_t>. */
#define threadcontext_FREE   \
         { 0, iobj_FREE, iobj_FREE, 0, iobj_FREE, iobj_FREE, 0, 0, 0 }

/* function: init_threadcontext
 * Creates all top level services which are bound to a single thread.
 * Services do *not* need to be multi thread safe cause a new tcontext is created for every new thread.
 * If a service shares information between threads then it must be programmed in a thread safe manner.
 * This function is called from both <thread_t.new_thread> and <thread_t.runmain_thread>.
 * The parameter context_type is of type <maincontext_e>.
 *
 * Parameter tcontext must be initialized with a static context before this functino is called.
 * This allows to call the log service any time even if tcontext is the currently active context. */
int init_threadcontext(/*out*/threadcontext_t* tcontext, uint8_t context_type);

/* function: free_threadcontext
 * Frees resources allocated by <init_threadcontext>.
 * This function is called from <maincontext_t.free_maincontext>. */
int free_threadcontext(threadcontext_t* tcontext);

/* function: initstatic_threadcontext
 * Initialisiert tcontext, so daß das grundlegendes Logging funktioniert.
 * Siehe auch <logwriter_t.initstatic_logwriter>.
 * Diese Funktion wird von <thread_t.new_thread> aufgerufen,
 * so dass bei Ausführung von <init_threadcontext> innerhalb der neuen Threadumgebung
 * schon ein statisch initialisierter Kontext vorliegt.
 *
 * Unchecked Precondition:
 * *tcontext == threadcontext_FREE && _is_initialized_(castPcontext_threadstack(tcontext)) */
int initstatic_threadcontext(threadcontext_t* tcontext, struct maincontext_t* maincontext, ilog_t* initlog);

/* function: freestatic_threadcontext
 * Gibt Ressourcen von tcontext frei, die während Ausführung von <initstatic_threadcontext> belegt wurden.
 * Diese Funktion wird von <thread_t.delete_thread> aufgerufen.
 * Aber erst nachdem <free_threadcontext> am Ende der Ausführung des zu löschenden Threads aufgerufen wurde.
 * */
int freestatic_threadcontext(threadcontext_t* tcontext, ilog_t* initlog);

// group: query

/* function: isstatic_threadcontext
 * Returns true if tcontext == <threadcontext_INIT_STATIC>. */
bool isstatic_threadcontext(const threadcontext_t* tcontext);

/* function: extsize_threadcontext
 * Gibt Speicher zurück, der zusätzlich von <init_threadcontext> benötigt wird. */
size_t extsize_threadcontext(void);

/* function: maincontext_threadcontext
 * Returns pointer to <maincontext_t>. */
struct maincontext_t* maincontext_threadcontext(const threadcontext_t* tcontext);

// group: change

/* function: resetthreadid_threadcontext
 * Resets the the thread id. The next created thread will be assigned the id 2.
 * Call this function only in test situations. */
void resetthreadid_threadcontext(void);


// section: inline implementation

// group: threadcontext_t

#define maincontext_threadcontext(tcontext) \
         ((tcontext)->maincontext)

#endif
