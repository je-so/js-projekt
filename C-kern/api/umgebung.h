/* title: Umgebung
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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/umgebung.h
    Header file of <Umgebung>.

   file: C-kern/umgebung/umgebung.c
    Implementation file <Umgebung impl>.
*/
#ifndef CKERN_API_UMGEBUNG_HEADER
#define CKERN_API_UMGEBUNG_HEADER

#include "C-kern/api/aspect/interface/log_oit.h"
#include "C-kern/api/aspect/interface/objectcache_oit.h"
#include "C-kern/api/umg/umgebung_shared.h"

/* typedef: struct umgebung_t
 * Export <umgebung_t>. */
typedef struct umgebung_t              umgebung_t ;

/* typedef: struct umgebung_services_t
 * Export <umgebung_services_t>. */
typedef struct umgebung_services_t     umgebung_services_t ;

/* enums: umgebung_type_e
 * Used to switch between different implementations.
 *
 * umgebung_type_STATIC  - An implementation which is configured by a static initializer.
 *                         Only the log service is supported.
 *                         This configuration is default at program startup and can not be
 *                         set with a call to <initmain_umgebung>.
 * umgebung_type_SINGLETHREAD - Default single threading implementation.
 *                              Services in <umgebung_t> can not be shared between threads.
 * umgebung_type_MULTITHREAD  - Default multi threading safe implementation.
 *                              Services in <umgebung_t> can be shared between threads.
 * */
enum umgebung_type_e {
    umgebung_type_STATIC       = 0
   ,umgebung_type_SINGLETHREAD = 1
   ,umgebung_type_MULTITHREAD  = 2
} ;

typedef enum umgebung_type_e        umgebung_type_e ;


/* variable: gt_umgebung extern
 * Global variable which describes the context for the current thread.
 * The variables is located in thread local storage.
 * So every thread has its own copy the content. */
extern __thread struct umgebung_t   gt_umgebung ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung
 * Test initialization process succeeds and global variables are set correctly. */
extern int unittest_umgebung(void) ;
#endif


/* struct: umgebung_services_t
 * Defines top level services for all software modules.
 *
 * */
struct umgebung_services_t {
   size_t                  resource_count ;
   log_oit                 ilog ;
   objectcache_oit         objectcache ;
} ;

/* define: umgebung_INIT_MAINSERVICES
 * Static initializer for <umgebung_services_t>.
 * These initializer ensures that in function main the global log service is available
 * even without calling <initmain_umgebung> first.
 */
#define umgebung_services_INIT_MAINSERVICES  { 0, { (struct callback_param_t*)&g_main_logwriter, (log_it*)&g_main_logwriter_interface }, objectcache_oit_INIT_FREEABLE }

/* define: umgebung_services_INIT_FREEABLE
 * Static initializer for <umgebung_services_t>.
 * This ensures that you can call <free_umgebung> without harm. */
#define umgebung_services_INIT_FREEABLE      { 0, log_oit_INIT_FREEABLE, objectcache_oit_INIT_FREEABLE }


/* struct: umgebung_t
 * Defines thread specific top level context for all software modules.
 *
 * TODO: implement SIGTERM support for shutting down the system in an ordered way
 *       (=> abort handler ? => abort handler calls freemain_umgebung ?)
 *
 * */
struct umgebung_t {
   // group: private
   umgebung_type_e         type ;
   size_t                * copy_count ;

   // group: public services

   /* variable: shared
    * Points to shared services. */
   umgebung_shared_t     * shared ;

   /* variable: svc
    * Points to services for a single thread.
    * They can also be shared (multithread) or exclusiv per thread (singlethread). */
   umgebung_services_t     svc ;

} ;

// group: lifetime

/* define: umgebung_INIT_MAINSERVICES
 * Static initializer for <umgebung_t>.
 * These initializer ensures that in function main the global log service is available
 * even without calling <initmain_umgebung> first.
 *
 * This initializer is used internally. It is reserved to be used
 * only as initializer for the main thread.
 * The reason is that services in <umgebung_t> are not thread safe
 * so every thread keeps its own initialized <umgebung_t>. */
#define umgebung_INIT_MAINSERVICES  { umgebung_type_STATIC, 0, 0, umgebung_services_INIT_MAINSERVICES }

/* define: umgebung_INIT_FREEABLE
 * Static initializer for <umgebung_t>.
 * This ensures that you can call <free_umgebung> without harm. */
#define umgebung_INIT_FREEABLE      { umgebung_type_STATIC, 0, 0, umgebung_services_INIT_FREEABLE }

/* function: initmain_umgebung
 * Initializes (global) process context. Must be called as first function from the main thread.
 * EALREADY is returned if it is called more than once.
 * The only service which works without calling this function is logging.
 *
 * Background:
 * This function calls every initonce_NAME functions
 * in the same order as defined in "C-kern/resource/text.db/initonce".
 * This init database is checked against the whole project with "C-kern/test/static/check_textdb.sh".
 * So that no entry is forgotten. */
extern int initmain_umgebung(umgebung_type_e implementation_type) ;

/* function: init_umgebung
 * Initializes (global) thread context.
 * Must be called before a new thread is started.
 *
 * Background:
 * This function calls call initumgebung_NAME functions in the same order
 * as defined in "C-kern/resource/text.db/initumgebung".
 * This init database is checked against the whole project with "C-kern/test/static/check_textdb.sh".
 * So that no entry is forgotten. */
extern int init_umgebung(/*out*/umgebung_t * umg, umgebung_type_e implementation_type) ;

/* function: initcopy_umgebung
 * Allows another thread to share all services.
 * In case shared_with is MULTITHREAD safe umg is a copy of copy_from after return.
 * The same is true if copy_from is *not* MULTITHREAD safe except copy_from is
 * freed before return. */
extern int initcopy_umgebung(/*out*/umgebung_t * umg, umgebung_t * copy_from) ;

/* function: initmove_umgebung
 * Allows you to move <umgebung_t> to another address.
 * After successful return dest is a copy of source and source is cleared. */
extern int initmove_umgebung(/*out*/umgebung_t * dest, umgebung_t * source) ;

/* function: freemain_umgebung
 * Frees global context. Must be called as last function from the main
 * thread of the whole system.
 *
 * Uses <free_umgebung> internally but ensures that after return
 * the global <umgebung_t> is set to umgebung_INIT_MAINSERVICES.
 * This ensures that basic services like logging always work
 * even in case of an unitialized system. */
extern int freemain_umgebung(void) ;

/* function: free_umgebung
 * Frees all resources bound to *umg*.
 * After return not all services of <umgebung_t> atre unoperational.
 * At least the log service is set to a static allocated service
 * which allows to write into the log if an error occurs afterwards.
 *
 * This function should be called as last before a thread exits. */
extern int free_umgebung(umgebung_t * umg) ;

/* function: abort_umgebung
 * Exits the whole process in a controlled manner.
 * Tries to free as many external resources as possible and
 * aborts all transactions. Before exit a LOG_ERRTEXT(ABORT_FATAL(err)) is done. */
extern void abort_umgebung(int err) ;

/* function: assertfail_umgebung
 * Exits the whole process in a controlled manner.
 * writes »Assertion failed« to log and calls <abort_umgebung>.
 *
 * Do not call <assertfail_umgebung> directly use the <assert> macro instead. */
extern void assertfail_umgebung(const char * condition, const char * file, unsigned line, const char * funcname) ;

// group: query

/* function: umgebung
 * Returns a reference to <umgebung_t> for the current thread.
 * This function can only be implemented as a macro. C99 does not support
 * references. */
extern /*ref*/ umgebung_t     umgebung(void) ;

/* function: log_umgebung
 * Returns log service <log_oit> (see <logwritermt_t>).
 * This function can only be implemented as a macro. C99 does not support
 * references. */
extern /*ref*/ log_oit        log_umgebung(void) ;

/* function: objectcache_umgebung
 * Returns object interface <objectcache_oit> for access of cached singelton objects. */
extern /*ref*/objectcache_oit objectcache_umgebung(void) ;

/* function: valuecache_umgebung
 * Returns cache for precomputed values of type <valuecache_t> for the current thread. */
extern struct valuecache_t *  valuecache_umgebung(void) ;

/* function: type_umgebung
 * Returns type <umgebung_type_e> of <umgebung_t>. */
extern umgebung_type_e        type_umgebung(void) ;


// section: inline implementations

/* define: initmove_umgebung
 * Inline implementation of <umgebung_t.initmove_umgebung>.
 * > #define initmove_umgebung(dest, source)  memcpy( dest, source, sizeof(umgebung_t)) */
#define initmove_umgebung(dest, source) \
   ( __extension__ ({                                          \
      umgebung_t * _temp_dest = (dest) ;                       \
      umgebung_t * _temp_src  = (source) ;                     \
      memcpy(_temp_dest, _temp_src, sizeof(umgebung_t)) ;      \
      *_temp_src = (umgebung_t) umgebung_INIT_FREEABLE ;       \
      0 /*always success*/ ;                                   \
   }))

/* define: log_umgebung
 * Inline implementation of <umgebung_t.log_umgebung>.
 * Uses a global thread-local storage variable to implement the functionality.
 * > #define log_umgebung() (gt_umgebung.ilog) */
#define log_umgebung() \
   (gt_umgebung.svc.ilog)

/* define: objectcache_umgebung
 * Inline implementation of <umgebung_t.objectcache_umgebung>.
 * Uses a global thread-local storage variable to implement the functionality.
 * > #define objectcache_umgebung() (gt_umgebung.objectcache) */
#define objectcache_umgebung() \
   (gt_umgebung.svc.objectcache)

/* define: umgebung
 * Inline implementation of <umgebung_t.umgebung>.
 * Uses a global thread-local storage variable to implement the functionality.
 * > #define umgebung() (gt_umgebung) */
#define umgebung() \
   (gt_umgebung)

/* define: type_umgebung
 * Inline implementation of <umgebung_t.type_umgebung>.
 * Uses a global thread-local storage variable to implement the functionality.
 * > #define type_umgebung() (gt_umgebung.type) */
#define type_umgebung() \
   (gt_umgebung.type)

/* define: valuecache_umgebung
 * Inline implementation of <umgebung_t.valuecache_umgebung>.
 * Uses a global thread-local storage variable to implement the functionality.
 * > #define valuecache_umgebung() (gt_umgebung.valuecache) */
#define valuecache_umgebung() \
   (gt_umgebung.shared->valuecache)

#endif
