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

// forward
struct logwriter_t;
struct processcontext_t;
struct syncrunner_t;

/* typedef: struct threadcontext_t
 * Export <threadcontext_t>. */
typedef struct threadcontext_t threadcontext_t;

/* typedef: threadcontext_mm_t
 * Definiert als <iobj_T>(mm). */
typedef iobj_T(mm) threadcontext_mm_t;

/* typedef: threadcontext_pagecache_t
 * Definiert als <iobj_T>(pagecache). */
typedef iobj_T(pagecache) threadcontext_pagecache_t;

/* typedef: threadcontext_objectcache_t
 * Definiert als <iobj_T>(objectcache). */
typedef iobj_T(objectcache) threadcontext_objectcache_t;

/* typedef: threadcontext_log_t
 * Definiert als <iobj_T>(log). */
typedef iobj_T(log) threadcontext_log_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_threadcontext
 * Test interface of <threadcontext_t>. */
int unittest_task_threadcontext(void);
#endif


/* struct: threadcontext_t
 * Stores services useable exclusively from one thread.
 * */
struct threadcontext_t {
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
   threadcontext_log_t        log;
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
   /* variable: staticmemblock
    * Start address of static memory block. */
   void*    staticmemblock;
};

// group: lifetime

/* define: threadcontext_FREE
 * Static initializer for <threadcontext_t>. */
#define threadcontext_FREE   \
         { 0, iobj_FREE, iobj_FREE, 0, iobj_FREE, iobj_FREE, 0, 0, 0 }

/* define: threadcontext_INIT_STATIC
 * Static initializer for <threadcontext_t>.
 * This pre-initializer ensures that during initialization the static log service is available
 * even before calling <init_threadcontext> or during call to <free_threadcontext>.
 *
 * Parameter:
 * tls - Pointer to thread_localstore_t the threadcontext is located. */
#define threadcontext_INIT_STATIC(tls) \
         { &g_maincontext, iobj_FREE, iobj_FREE, 0, iobj_FREE, { (struct log_t*)logwriter_threadlocalstore(tls), interface_logwriter() }, 0, 0, 0 }

/* function: init_threadcontext
 * Creates all top level services which are bound to a single thread.
 * Services do *not* need to be multi thread safe cause a new tcontext is created for every new thread.
 * If a service shares information between threads then it must be programmed in a thread safe manner.
 * This function is called from <maincontext_t.init_maincontext>. The parameter context_type is of type <maincontext_e>.
 *
 * Parameter tcontext must be initialized with a static context before this functino is called.
 * This allows to call the log service any time even if tcontext is the currently active context. */
int init_threadcontext(/*out*/threadcontext_t* tcontext, uint8_t context_type);

/* function: free_threadcontext
 * Frees all resources bound to this object.
 * This function is called from <maincontext_t.free_maincontext>. */
int free_threadcontext(threadcontext_t* tcontext);

// group: query

/* function: isstatic_threadcontext
 * Returns true if tcontext == <threadcontext_INIT_STATIC>. */
bool isstatic_threadcontext(const threadcontext_t* tcontext);

/* function: extsize_threadcontext
 * Gibt Speicher zurück, der zusätzlich von <init_threadcontext> benötigt wird. */
size_t extsize_threadcontext(void);

// group: change

/* function: resetthreadid_threadcontext
 * Resets the the thread id. The next created thread will be assigned the id 2.
 * Call this function only in test situations. */
void resetthreadid_threadcontext(void);

/* function: setmm_threadcontext
 * Overwrites old mm_t of threadcontext_t with new_mm. */
void setmm_threadcontext(threadcontext_t* tcontext, const threadcontext_mm_t* new_mm);

#endif
