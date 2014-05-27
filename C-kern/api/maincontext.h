/* title: MainContext
   Defines service context used by every software component in C-kern(el).

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/maincontext.h
    Header file of <MainContext>.

   file: C-kern/context/maincontext.c
    Implementation file <MainContext impl>.

   file: C-kern/api/platform/Linux/syscontext.h
    Linux specific configuration file <LinuxSystemContext>.
*/
#ifndef CKERN_API_MAINCONTEXT_HEADER
#define CKERN_API_MAINCONTEXT_HEADER

#include "C-kern/api/context/threadcontext.h"
#include "C-kern/api/context/processcontext.h"

/* typedef: struct maincontext_t
 * Export <maincontext_t>. */
typedef struct maincontext_t  maincontext_t;

/* typedef: struct maincontext_startparam_t
 * Export <maincontext_startparam_t>. */
typedef struct maincontext_startparam_t  maincontext_startparam_t;

/* typedef: maincontext_thread_f
 * Define <maincontext_thread_f> as signature of start function for main thread. */
typedef int (* maincontext_thread_f) (maincontext_t * maincontext);


/* enums: maincontext_e
 * Used to switch between different implementations.
 * Services in <threadcontext_t> can not be shared between threads.
 * Services in <processcontext_t> are shared between threads.
 *
 * maincontext_STATIC -  An implementation which is configured by a static initializer.
 *                       Only the log service is supported.
 *                       This configuration is default after <platform_t.init_platform> has been called and can not be
 *                       set with a call to <init_maincontext>.
 * maincontext_DEFAULT - Default single or multi threading implementation.
 *                       All content logged to channel <log_channel_USERERR> is ignored.
 * maincontext_CONSOLE - Default single or multi threading implementation for commandline tools.
 *                       All content logged to channel <log_channel_USERERR> is immediately written (<log_state_UNBUFFERED>).
 *                       All content logged to channel <log_channel_ERR> is ignored.
 *
 * */
enum maincontext_e {
   maincontext_STATIC  = 0,
   maincontext_DEFAULT = 1,
   maincontext_CONSOLE = 2,
};

typedef enum maincontext_e    maincontext_e;

/* variable: g_maincontext
 * Global variable which describes the main context for the current process.
 * The variable is located in process global storage.
 * So every thread references the same <maincontext_t>. */
extern struct maincontext_t   g_maincontext;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_maincontext
 * Test initialization process succeeds and global variables are set correctly. */
int unittest_context_maincontext(void);
#endif


/* struct: maincontext_startparam_t
 * Start parameters used in <initstart_maincontext>. */
struct maincontext_startparam_t {
   maincontext_e  context_type;
   int            argc;
   const char **  argv;
   maincontext_thread_f main_thread;
};

/* define: maincontext_startparam_INIT
 * Static initializer. */
#define maincontext_startparam_INIT(context_type, argc, argv, main_thread) \
         { context_type, argc, argv, main_thread }


/* struct: maincontext_t
 * Defines the main top level context of the whole process.
 *
 * It extends the functionality of <processcontext_t> and combines it with <threadcontext_t>.
 *
 * It contains a single copy of <processcontext_t> and initializes
 * <threadcontext_t> of the caller (main thread of the process).
 *
 * Allows access to process & thread specific top level context.
 *
 * TODO: implement SIGTERM support for shutting down the system in an ordered way
 *       (=> abort handler ? => abort handler calls free_maincontext ?)
 *
 * */
struct maincontext_t {
   processcontext_t  pcontext;
   maincontext_e     type;
   const char *      progname;
   int               argc;
   const char **     argv;
   /* variable: size_staticmem
    * Size in bytes of how much of the static memory is allocated. */
   uint16_t          size_staticmem;
};

// group: lifetime

/* function: initstart_maincontext
 * Calls <platform_t.init_platform>, <init_maincontext> and runs main_thread.
 * This is a convenience function so you do not have to remember the
 * process initialization sequence pattern. */
int initstart_maincontext(const maincontext_startparam_t * startparam);

/* function: init_maincontext
 * Initializes global program context. Must be called as first function from the main thread.
 * The main thread must be initialized with <platform_t.init_platform> before calling this function.
 * EALREADY is returned if it is called more than once.
 * The only service which works without calling this function is logging.
 *
 * Background:
 * This function calls init_processcontext and init_threadcontext which call every
 * initonce_NAME and initthread_NAME functions in the same order as defined in
 * "C-kern/resource/config/initprocess" and "C-kern/resource/config/initthread".
 * This init database files are checked against the whole project with "C-kern/test/static/check_textdb.sh".
 * So that no entry is forgotten. */
int init_maincontext(const maincontext_e context_type, int argc, const char ** argv);

/* function: free_maincontext
 * Frees global context. Must be called as last function from the main
 * thread of the whole system.
 *
 * Ensures that after return the basic logging functionality is working.
 *
 * Background:
 * This function calls free_threadcontext and then free_processcontext which call every
 * freethread_NAME and freeonce_NAME functions in the reverse order as defined in
 * "C-kern/resource/config/initthread" and "C-kern/resource/config/initprocess".
 * This init database files are checked against the whole project with "C-kern/test/static/check_textdb.sh".
 * So that no entry is forgotten. */
int free_maincontext(void);

/* function: abort_maincontext
 * Exits the whole process in a controlled manner.
 * Tries to free as many external resources as possible and
 * aborts all transactions. i
 * Before exit TRACE_NOARG_ERRLOG(PROGRAM_ABORT, err) is called. */
void abort_maincontext(int err);

/* function: assertfail_maincontext
 * Exits the whole process in a controlled manner.
 * writes »Assertion failed« to log and calls <abort_maincontext>.
 *
 * Do not call <assertfail_maincontext> directly use the <assert> macro instead. */
void assertfail_maincontext(const char * condition, const char * file, int line, const char * funcname);

// group: query

/* function: self_maincontext
 * Returns <maincontext_t> of the current process. */
maincontext_t *            self_maincontext(void);

/* function: pcontext_maincontext
 * Returns <processcontext_t> of the current process. */
processcontext_t *         pcontext_maincontext(void);

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

/* function: blockmap_maincontext
 * Returns shared blockmap used by <pagecache_impl_t> (see <pagecache_blockmap_t >). */
struct pagecache_blockmap_t * blockmap_maincontext(void);

/* function: error_maincontext
 * Returns error string table (see <errorcontext_t>). */
/*ref*/typeof(((processcontext_t*)0)->error) error_maincontext(void);

/* function: log_maincontext
 * Returns log service <log_t> (see <logwriter_t>).
 * This function can only be implemented as a macro. C99 does not support
 * references. */
/*ref*/iobj_DECLARE(,log)  log_maincontext(void);

/* function: mm_maincontext
 * Returns interfaceable object <mm_t> for access of memory manager. */
/*ref*/iobj_DECLARE(,mm)   mm_maincontext(void);

/* function: objectcache_maincontext
 * Returns interfaceable object <objectcache_t> for access of cached singleton objects. */
/*ref*/iobj_DECLARE(,objectcache) objectcache_maincontext(void);

/* function: pagecache_maincontext
 * Returns object interface <pagecache_t> to access functionality of <pagecache_impl_t>. */
/*ref*/iobj_DECLARE(,pagecache) pagecache_maincontext(void);

/* function: syncrun_maincontext
 * Returns <syncrun_t> of current <maincontext_t>. It is used to store and run
 * all <syncthread_t> of the current thread. */
struct syncrun_t *         syncrun_maincontext(void);

/* function: sysuser_maincontext
 * Returns <sysusercontext_t> of current <maincontext_t>. It is used in implementation of module <SystemUser>. */
/*ref*/struct sysuser_t *  sysuser_maincontext(void);

/* function: valuecache_maincontext
 * Returns <valuecache_t> holding precomputed values.
 * Every value is cached as a single copy for the whole process. */
struct valuecache_t *      valuecache_maincontext(void);

// group: static-memory

/* function: allocstatic_maincontext
 * Allocates size bytes of memory and returns the start address.
 * Used by modules during execution of their initonce_ functions.
 * This memory lives as long <maincontext_t> lives.
 * Must be called in reverse order of calls to <allocstatic_maincontext>. */
void * allocstatic_maincontext(uint8_t size);

/* function: freestatic_maincontext
 * Frees size bytes of last allocated memory.
 * Must be called in reverse order of calls to <allocstatic_maincontext>.
 * It is possible to free x calls to <allocstatic_maincontext> with one
 * call to <freestatic_maincontext> where all sizes are summed up. */
int freestatic_maincontext(uint8_t size);

/* function: sizestatic_maincontext
 * Returns size in bytes of allocated static memory. */
uint16_t sizestatic_maincontext(void);



// section: inline implementation

/* define: blockmap_maincontext
 * Implementation of <maincontext_t.blockmap_maincontext>. */
#define blockmap_maincontext()            (pcontext_maincontext()->blockmap)

/* define: error_maincontext
 * Implementation of <maincontext_t.error_maincontext>. */
#define error_maincontext()               (pcontext_maincontext()->error)

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

/* define: pcontext_maincontext
 * Inline implementation of <maincontext_t.pcontext_maincontext>. */
#define pcontext_maincontext()            (tcontext_maincontext()->pcontext)

/* define: progname_maincontext
 * Inline implementation of <maincontext_t.progname_maincontext>. */
#define progname_maincontext()            (self_maincontext()->progname)

/* define: self_maincontext
 * Inline implementation of <maincontext_t.self_maincontext>. */
#define self_maincontext()                ((maincontext_t*)pcontext_maincontext())

/* define: sizestatic_maincontext
 * Inline implementation of <maincontext_t.sizestatic_maincontext>. */
#define sizestatic_maincontext()          (self_maincontext()->size_staticmem)

/* define: syncrun_maincontext
 * Inline implementation of <maincontext_t.syncrun_maincontext>. */
#define syncrun_maincontext()             (tcontext_maincontext()->syncrun)

/* define: sysuser_maincontext
 * Inline implementation of <maincontext_t.sysuser_maincontext>. */
#define sysuser_maincontext()             (pcontext_maincontext()->sysuser)

/* define: tcontext_maincontext
 * Inline implementation of <maincontext_t.tcontext_maincontext>. */
#define tcontext_maincontext()            (sys_context_threadtls())

/* define: threadid_maincontext
 * Inline implementation of <maincontext_t.threadid_maincontext>. */
#define threadid_maincontext()            (tcontext_maincontext()->thread_id)

/* define: type_maincontext
 * Inline implementation of <maincontext_t.type_maincontext>. */
#define type_maincontext()                (self_maincontext()->type)

/* define: valuecache_maincontext
 * Inline implementation of <maincontext_t.valuecache_maincontext>.
 * Uses a global thread-local storage variable to implement the functionality. */
#define valuecache_maincontext()          (pcontext_maincontext()->valuecache)

#endif
