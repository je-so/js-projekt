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

   file: C-kern/api/context.h
    Header file of <MainContext>.

   file: C-kern/context/context.c
    Implementation file <MainContext impl>.

   file: C-kern/api/platform/Linux/syscontext.h
    Linux specific configuration file <LinuxSystemContext>.
*/
#ifndef CKERN_API_CONTEXT_HEADER
#define CKERN_API_CONTEXT_HEADER

#include STR(C-kern/api/platform/KONFIG_OS/syscontext.h)
#include "C-kern/api/context/threadcontext.h"
#include "C-kern/api/context/processcontext.h"

/* typedef: struct context_t
 * Export <context_t>. */
typedef struct context_t               context_t ;

/* enums: contextconfig_e
 * Used to switch between different implementations.
 *
 * context_STATIC  - An implementation which is configured by a static initializer.
 *                   Only the log service is supported.
 *                   This configuration is default at program startup and can not be
 *                   set with a call to <initmain_context>.
 * context_DEFAULT - Default single threading implementation.
 *                   Services in <threadcontext_t> can not be shared between threads.
 *
 * */
enum context_e {
    context_STATIC  = 0
   ,context_DEFAULT = 1
} ;

typedef enum context_e                 context_e ;

/* variable: g_context
 * Global variable which describes the main context for the current process.
 * The variable is located in process global storage.
 * So every thread references the same <context_t>. */
extern struct context_t                g_context ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context
 * Test initialization process succeeds and global variables are set correctly. */
extern int unittest_context(void) ;
#endif


/* struct: context_t
 * Defines the main top level context of the whole process.
 * It contains a single copy of <processcontext_t> and in case
 * the thread subsystem is not needed also a copy of <threadcontext_t>.
 * Allows access to process & thread specific top level context.
 *
 * TODO: implement SIGTERM support for shutting down the system in an ordered way
 *       (=> abort handler ? => abort handler calls freemain_context ?)
 *
 * */
struct context_t {
   processcontext_t  pcontext ;
#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
   threadcontext_t   tcontext ;
#endif
#undef THREAD
   context_e         type ;
} ;

// group: lifetime

/* function: initmain_context
 * Initializes global program context. Must be called as first function from the main thread.
 * EALREADY is returned if it is called more than once.
 * The only service which works without calling this function is logging.
 *
 * Background:
 * This function calls init_processcontext and init_threadcontext which call every
 * initonce_NAME and initthread_NAME functions in the same order as defined in
 * "C-kern/resource/text.db/initprocess" and "C-kern/resource/text.db/initthread".
 * This init database files are checked against the whole project with "C-kern/test/static/check_textdb.sh".
 * So that no entry is forgotten. */
extern int initmain_context(context_e context_type) ;

// extern int initmain2_context(context_e context_type, int argc, const char ** argv) ;

/* function: freemain_context
 * Frees global context. Must be called as last function from the main
 * thread of the whole system.
 *
 * Ensures that after return the basic logging functionality is working.
 *
 * Background:
 * This function calls free_threadcontext and then free_processcontext which call every
 * freethread_NAME and freeonce_NAME functions in the reverse order as defined in
 * "C-kern/resource/text.db/initthread" and "C-kern/resource/text.db/initprocess".
 * This init database files are checked against the whole project with "C-kern/test/static/check_textdb.sh".
 * So that no entry is forgotten. */
extern int freemain_context(void) ;

/* function: abort_context
 * Exits the whole process in a controlled manner.
 * Tries to free as many external resources as possible and
 * aborts all transactions. Before exit a LOG_ERRTEXT(ABORT_FATAL(err)) is done. */
extern void abort_context(int err) ;

/* function: assertfail_context
 * Exits the whole process in a controlled manner.
 * writes »Assertion failed« to log and calls <abort_context>.
 *
 * Do not call <assertfail_context> directly use the <assert> macro instead. */
extern void assertfail_context(const char * condition, const char * file, unsigned line, const char * funcname) ;

// group: query

/* function: log_context
 * Returns log service <log_oit> (see <logwritermt_t>).
 * This function can only be implemented as a macro. C99 does not support
 * references. */
extern /*ref*/log_oit            log_context(void) ;

/* function: objectcache_context
 * Returns object interface <objectcache_oit> for access of cached singelton objects. */
extern /*ref*/objectcache_oit    objectcache_context(void) ;

/* function: process_context
 * Returns <processcontext_t> of the current process. */
extern /*ref*/processcontext_t   process_context(void) ;

/* function: thread_context
 * Returns <threadcontext_t> of the current thread. */
extern /*ref*/threadcontext_t    thread_context(void) ;

/* function: valuecache_context
 * Returns cache for precomputed values of type <valuecache_t> for the current thread. */
extern struct valuecache_t *     valuecache_context(void) ;

/* function: type_context
 * Returns type <context_e> of current <context_t>. */
extern context_e                 type_context(void) ;


// section: inline implementations

/* define: log_context
 * Inline implementation of <context_t.log_context>.
 * Uses a global thread-local storage variable to implement the functionality. */
#define log_context()                  (thread_context().ilog)

/* define: objectcache_context
 * Inline implementation of <context_t.objectcache_context>.
 * Uses a global thread-local storage variable to implement the functionality. */
#define objectcache_context()          (thread_context().objectcache)

/* define: process_context
 * Inline implementation of <context_t.process_context>. */
#define process_context()              (g_context.pcontext)

#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
#define thread_context()               (g_context.tcontext)
#else
#define thread_context()               sys_thread_context()
#endif
#undef THREAD

/* define: type_context
 * Inline implementation of <context_t.type_context>. */
#define type_context()                 (g_context.type)

/* define: valuecache_context
 * Inline implementation of <context_t.valuecache_context>.
 * Uses a global thread-local storage variable to implement the functionality. */
#define valuecache_context()           (g_context.pcontext.valuecache)

#endif
