/* title: MainContext
   Defines service context used by every software component in C-kern(el).

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/maincontext.h
    Header file of <MainContext>.

   file: C-kern/main/maincontext.c
    Implementation file <MainContext impl>.

   file: C-kern/api/platform/Linux/syscontext.h
    Linux specific configuration file <LinuxSystemContext>.

  file: C-kern/api/platform/task/thread.h
    Platform specific initialization used by this module.
*/
#ifndef CKERN_API_MAINCONTEXT_HEADER
#define CKERN_API_MAINCONTEXT_HEADER

// import
#include "C-kern/api/task/threadcontext.h"
struct logwriter_t;
struct syslogin_t;
struct signals_t;

// === exported types
struct maincontext_t;

/* typedef: mainthread_f
 * Signature of of new main function. It is stored in <maincontext_t>. */
typedef int (* mainthread_f) (struct maincontext_t * maincontext);


/* enums: maincontext_e
 * Used to switch between different implementations.
 * Services in <threadcontext_t> can not be shared between threads.
 * Services in <maincontext_t> are shared between threads.
 *
 * maincontext_STATIC -  An implementation which is configured by a static initializer.
 *                       This configuration is working after <syscontext_t.initrun_syscontext> has been called
 *                       and only the log service is supported.
 *                       As long as g_maincontext.type is set to this value do not try
 *                       to call the log service. If g_maincontext.type is != maincontext_STATIC
 *                       <syscontext_t.initrun_syscontext> has been run successfully and at least
 *                       the static log service is working.
 * maincontext_DEFAULT - Default single or multi threading implementation.
 *                       All content logged to channel <log_channel_USERERR> is ignored.
 * maincontext_CONSOLE - Default single or multi threading implementation for commandline tools.
 *                       All content logged to channel <log_channel_USERERR> is immediately written (<log_state_UNBUFFERED>).
 *                       All content logged to channel <log_channel_ERR> is ignored.
 *
 * maincontext__NROF   - Number of different context types.
 *
 * */
typedef enum maincontext_e {
   maincontext_STATIC  = 0,
   maincontext_DEFAULT = 1,
   maincontext_CONSOLE = 2,
} maincontext_e;

#define maincontext__NROF (maincontext_CONSOLE + 1)


/* variable: g_maincontext
 * Global variable which describes the main context for the current process.
 * The variable is located in process global storage.
 * So every thread references the same <maincontext_t>. */
extern struct maincontext_t   g_maincontext;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_main_maincontext
 * Test initialization process succeeds and global variables are set correctly. */
int unittest_main_maincontext(void);
#endif


/* struct: maincontext_t
 * Defines the main top level context of the whole process.
 * Allows access to process specific top level context.
 * Defines context which is shared between all threads of computation.
 * Query macros conveniently offer access to thread local objects also.
 * It contains e.g. services which offer read only values or services which
 * must be implemented with help of mutex locks.
 *
 * This process context is combined with <threadcontext_t> to get the full context
 * for every thread. Every thread has its own copy of <threadcontext_t> but shares
 * the same <maincontext_t>.
 *
 * TODO: implement SIGTERM support for shutting down the system in an ordered way
 *       (=> abort handler ? => abort handler calls free_maincontext ?)
 *
 * */
typedef struct maincontext_t {
   // group: private fields

   /* variable: type
    * The value of type <maincontext_e> given as first parameter to <initrun_maincontext>.
    * The initlog macros (see <LogMacros>) use this value to determine if standard error log is available ! */
   maincontext_e     type;

   /* variable: staticmemblock
    * Start address of static memory block. */
   void*             staticmemblock;

   /* variable: initcount
    * Counts the number of successfully initialized services/subsystems.
    * This number counts all subsystems even if they
    * do not have state stored in <maincontext_t>. */
   uint16_t          initcount;

   // group: public fields

   // --- services shared by all threads

   /* variable: sysinfo
    * Contains queried once information about the platform/OS. */
   // TODO: integrate it into initmain !!!
   syscontext_t         sysinfo;

   /* variable: syslogin
    * Context for <syslogin_t> module. */
   struct syslogin_t*   syslogin;
   /* variable: signals
    * Context for <signals_t> module. */
   struct signals_t*    signals;
   /* variable: error
    * Data for <errorcontext_t> module. */
   struct {
      uint16_t* stroffset;
      uint8_t*  strdata;
   }                    error;

   // --- program arguments

   /* variable: progname
    * The filename of the program without path - it is calculated from argv[0] (Unix convention). */
   const char *      progname;
   /* variable: argc
    * The number of process arguments. Same value as 1st arg received by
    * > int main(int argc, const char * argv[]). */
   int               argc;
   /* variable: argv
    * An array of program arguments. The last argument argv[ærgc]
    * should be set to 0. Same value as received as 2nd argument by
    * > int main(int argc, const char * argv[]). */
   const char **     argv;

} maincontext_t;

// group: lifetime

/* define: maincontext_INIT_STATIC
 * Static initializer for <maincontext_t>. */
#define maincontext_INIT_STATIC \
         {  maincontext_STATIC, (void*)0, (uint16_t)0, syscontext_FREE, (struct syslogin_t*)0, (struct signals_t*)0, errorcontext_INIT_STATIC, (const char*)0, (int)0, (const char**)0 }

/* function: init_maincontext
 * Initializes <maincontext_t> and all its contained shared services (shared between threads).
 * This function is called from <thread_t.runmain_thread>.
 * The parameter context_type is of type <maincontext_e>.
 *
 * Architecture:
 * Function init_maincontext calls every initonce_NAME function and others in the same order
 * as defined in "C-kern/resource/config/initmain".
 * This init database file is checked against the whole project with
 * "C-kern/test/static/check_textdb.sh". */
int init_maincontext(const maincontext_e context_type, int argc, const char* argv[]);

/* function: initrun_maincontext
 * Initializes global program context. Must be called as first function from the main thread.
 *
 * EALREADY is returned if it is called more than once.
 *
 * Architecture:
 * This function calls the platform specific function <thread_t.runmain_thread>
 * to set up the thread environment and thread local storage.
 *
 * Module thread_t calls <maincontext_t.init_maincontext> and <threadcontext_t.init_threadcontext>.
 * These functions call every initonce_NAME and initthread_NAME in the same order as defined in
 * "C-kern/resource/config/initmain" and "C-kern/resource/config/initthread".
 * This init database files are checked against the whole project with "C-kern/test/static/check_textdb.sh".
 *
 * After initialization main_thread is called.
 *
 * Before initrun_maincontext returns the initialized main/thread context is freed.
 *
 * Returns:
 * If initialization or freeing of main context produces an error this error is returned (> 0)
 * else the return code of main_thread.
 * */
int initrun_maincontext(maincontext_e type, mainthread_f main_thread, int argc, const char** argv);

/* function: free_maincontext
 * Frees <g_maincontext> of type <maincontext_t>.
 * All initialized shared services are freed.
 * After return <isstatic_maincontext> returns true. */
int free_maincontext(void);

/* function: abort_maincontext
 * Exits the whole process in a controlled manner.
 * Tries to free as many external resources as possible and
 * aborts all transactions. i
 * Before exit TRACE_NOARG_ERRLOG(PROGRAM_ABORT, err) is called. */
void abort_maincontext(int err) __attribute__((noreturn));

/* function: assertfail_maincontext
 * Exits the whole process in a controlled manner.
 * writes »Assertion failed« to log and calls <abort_maincontext>.
 *
 * Do not call <assertfail_maincontext> directly use the <assert> macro instead. */
void assertfail_maincontext(const char * condition, const char * file, int line, const char * funcname);

// group: query

/* function: isstatic_maincontext
 * Returns (maincontext == maincontext_INIT_STATIC). */
bool isstatic_maincontext(const maincontext_t* maincontext);

/* function: extsize_maincontext
 * Gibt Speichergröße zurück, der während Ausführung von <init_maincontext> statisch allokiert wird. */
size_t extsize_maincontext(void);

/* function: self_maincontext
 * Returns <maincontext_t> of the current process. */
maincontext_t *            self_maincontext(void);

/* function: tcontext_maincontext
 * Returns <threadcontext_t> of the current thread. */
threadcontext_t *          tcontext_maincontext(void);

/* function: type_maincontext
 * Returns type <context_e> of current <maincontext_t>. */
maincontext_e              type_maincontext(void);

/* function: progname_maincontext
 * Returns the program name of the running process.
 * The returned value is the argv[0] delivered as parameter in <initmain_maincontext>
 * without any leading path. */
const char *               progname_maincontext(void);

/* function: threadid_maincontext
 * Returns the thread id of the calling thread. */
size_t threadid_maincontext(void);

// group: query-service

/* function: error_maincontext
 * Returns error string table (see <errorcontext_t>). */
/*ref*/typeof(((maincontext_t*)0)->error) error_maincontext(void);

/* function: log_maincontext
 * Returns log service <log_t> (see <logwriter_t>).
 * This function can only be implemented as a macro. C99 does not support
 * references. */
/*ref*/struct iobj_log_t log_maincontext(void);

/* function: mm_maincontext
 * Returns interfaceable object <mm_t> for access of memory manager. */
/*ref*/struct iobj_mm_t mm_maincontext(void);

/* function: objectcache_maincontext
 * Returns interfaceable object <objectcache_t> for access of cached singleton objects. */
/*ref*/struct iobj_objectcache_t objectcache_maincontext(void);

/* function: pagecache_maincontext
 * Returns object interface <pagecache_t> to access functionality of <pagecache_impl_t>. */
/*ref*/struct iobj_pagecache_t pagecache_maincontext(void);

/* function: syncrunner_maincontext
 * Returns <syncrunner_t> of current <maincontext_t>. It is used to store and run
 * all <syncfunc_t> of the current thread. */
struct syncrunner_t* syncrunner_maincontext(void);

/* function: sysinfo_maincontext
 * Returns reference to <syscontext_t> holding precomputed values.
 * Every value is cached as a single copy for the whole process. */
/*ref*/syscontext_t sysinfo_maincontext(void);

/* function: syslogin_maincontext
 * Returns <syslogin_t> of current <maincontext_t>. It is used in module <SystemLogin>. */
/*ref*/struct syslogin_t* syslogin_maincontext(void);



// section: inline implementation

/* define: error_maincontext
 * Implementation of <maincontext_t.error_maincontext>. */
#define error_maincontext()               (self_maincontext()->error)

/* define: log_maincontext
 * Inline implementation of <maincontext_t.log_maincontext>.
 * Uses a global thread-local storage variable to implement the functionality. */
#define log_maincontext()                 (tcontext_maincontext()->log)

/* define: mm_maincontext
 * Inline implementation of <maincontext_t.mm_maincontext>.
 * Uses a global thread-local storage variable to implement the functionality. */
#define mm_maincontext()                  (tcontext_maincontext()->mm)

/* define: objectcache_maincontext
 * Inline implementation of <maincontext_t.objectcache_maincontext>.
 * Uses a global thread-local storage variable to implement the functionality. */
#define objectcache_maincontext()         (tcontext_maincontext()->objectcache)

/* define: pagecache_maincontext
 * Implements <maincontext_t.pagecache_maincontext>. */
#define pagecache_maincontext()           (tcontext_maincontext()->pagecache)

/* define: progname_maincontext
 * Inline implementation of <maincontext_t.progname_maincontext>. */
#define progname_maincontext()            (self_maincontext()->progname)

/* define: self_maincontext
 * Inline implementation of <maincontext_t.self_maincontext>. */
#define self_maincontext()                (tcontext_maincontext()->maincontext)

/* define: syncrunner_maincontext
 * Inline implementation of <maincontext_t.syncrunner_maincontext>. */
#define syncrunner_maincontext()          (tcontext_maincontext()->syncrunner)

/* define: syslogin_maincontext
 * Inline implementation of <maincontext_t.syslogin_maincontext>. */
#define syslogin_maincontext() \
         (self_maincontext()->syslogin)

/* define: sysinfo_maincontext
 * Inline implementation of <maincontext_t.sysinfo_maincontext>. */
#define sysinfo_maincontext() \
         (self_maincontext()->sysinfo)

/* define: tcontext_maincontext
 * Inline implementation of <maincontext_t.tcontext_maincontext>. */
#define tcontext_maincontext()            (context_syscontext())

/* define: threadid_maincontext
 * Inline implementation of <maincontext_t.threadid_maincontext>. */
#define threadid_maincontext()            (tcontext_maincontext()->thread_id)

/* define: type_maincontext
 * Inline implementation of <maincontext_t.type_maincontext>. */
#define type_maincontext()                (self_maincontext()->type)

#endif
