/* title: ThreadContext

   TODO: move thrreadcontext into directory task/

   Defines the global (thread local) context of a running system thread.
   If more than one thread is running in a process each thread
   has its own context. It contains references to services which
   belong only to one thread and therefore are not thread safe.
   They can not be shared between threads.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/context/threadcontext.h
    Header file <ThreadContext>.

   file: C-kern/context/threadcontext.c
    Implementation file <ThreadContext impl>.
*/
#ifndef CKERN_CONTEXT_THREADCONTEXT_HEADER
#define CKERN_CONTEXT_THREADCONTEXT_HEADER

// forward
struct processcontext_t;
struct syncrunner_t;

/* typedef: struct threadcontext_t
 * Export <threadcontext_t>. */
typedef struct threadcontext_t threadcontext_t;

/* typedef: iobj_mm_t
 * Defines iobj_mm_t as <iobj_T>(mm). */
typedef iobj_T(mm) iobj_mm_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_threadcontext
 * Test interface of <threadcontext_t>. */
int unittest_context_threadcontext(void);
#endif


/* struct: threadcontext_t
 * Stores services useable exclusively from one thread.
 * */
struct threadcontext_t {
   /* variable: pcontext
    * Points to shared <processcontext_t>. */
   struct processcontext_t*   pcontext;
   /* variable: pagecache
    * Thread local virtual memory page manager. */
   iobj_T(pagecache)    pagecache;
   /* variable: mm
    * Thread local memory manager. */
   iobj_T(mm)           mm;
   /* variable: syncrunner
    * Synchronous task support. */
   struct syncrunner_t* syncrunner;
   /* variable: objectcache
    * Thread local erorr object cache. */
   iobj_T(objectcache)  objectcache;
   /* variable: log
    * Thread local erorr log. */
   iobj_T(log)          log;
   /* variable: thread_id
    * Identification number which is incremented every time a thread is created.
    * The main thread has id 1. If SIZE_MAX is reached the value is wrapped around
    * to number 2 which may be no more unique.
    * TODO: implement reuse of id
    *       refactor id management into own thread-manager which checks also that threads are alive
    *       make thread-manager check that every thread is alive with tryjoin_thread
    *       + send_interrupt which sets flag !! (-> make all syscalls restartable) .. */
   size_t               thread_id;
   /* variable: initcount
    * Number of correct initialized objects. */
   size_t               initcount;
};

// group: lifetime

/* define: threadcontext_FREE
 * Static initializer for <threadcontext_t>. */
#define threadcontext_FREE   \
         { 0, iobj_FREE, iobj_FREE, 0, iobj_FREE, iobj_FREE, 0, 0 }

/* define: threadcontext_INIT_STATIC
 * Static initializer for <threadcontext_t>.
 * These initializer ensures that in function main the global log service is available
 * even without calling <maincontext_t.init_maincontext> first.
 */
#define threadcontext_INIT_STATIC   \
         { &g_maincontext.pcontext, iobj_FREE, iobj_FREE, 0, iobj_FREE, { 0, &g_logmain_interface }, 0, 0 }

/* function: init_threadcontext
 * Creates all top level services which are bound to a single thread.
 * Services do *not* need to be multi thread safe cause a new one is created for every new thread.
 * If a service shares information between threads then it must be programmed in a thread safe manner.
 * This function is called from <maincontext_t.init_maincontext>. The parameter context_type is of type <maincontext_e>. */
int init_threadcontext(/*out*/threadcontext_t * tcontext, struct processcontext_t * pcontext, uint8_t context_type) ;

/* function: free_threadcontext
 * Frees all resources bound to this object.
 * This function is called from <maincontext_t.free_maincontext>. */
int free_threadcontext(threadcontext_t * tcontext) ;

// group: query

/* function: isstatic_threadcontext
 * Returns true if tcontext == <threadcontext_INIT_STATIC>. */
bool isstatic_threadcontext(const threadcontext_t * tcontext) ;

// group: change

/* function: resetthreadid_threadcontext
 * Resets the the thread id. The next created thread will be assigned the id 2.
 * Call this function only in test situations. */
void resetthreadid_threadcontext(void) ;

/* function: setmm_threadcontext
 * Overwrites old mm_t of threadcontext_t with new_mm. */
void setmm_threadcontext(threadcontext_t* tcontext, const iobj_mm_t* new_mm);

#endif
