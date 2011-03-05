/* title: Umgebung Interface
   Defines runtime context used by every software component in C-kern(el).

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
    Header file of <Umgebung Interface>.

   file: C-kern/umgebung/umgebung_init.c
    Implementation file of <Umgebung Interface>.
*/
#ifndef CKERN_API_UMGEBUNG_HEADER
#define CKERN_API_UMGEBUNG_HEADER


/* typedef: typedef umgebung_tempmman_t
 * Shortcut for <umgebung_tempmman_t >. */
typedef struct umgebung_tempmman_t umgebung_tempmman_t ;

/* typedef: typedef umgebung_t
 * Shortcut for <umgebung_t>. */
typedef struct umgebung_t          umgebung_t ;

/* enums: umgebung_type_e
 * Used to switch between different implementations.
 *
 * umgebung_STATIC_IMPL  - An implementation which is configured by a static initializer.
 *                         Currently only the log service is supported by this type of implementation.
 * umgebung_DEFAULT_IMPL - Default production ready implementation.
 * umgebung_TEST_IMPL    - Implements functionality without use of internal components but only with help of
 *                         C library calls. This ensures that software components which depends on <umgebung_t>
 *                         can be tested without mutual dependencies.
 * */
enum umgebung_type_e {
    umgebung_type_STATIC  = 0
   ,umgebung_type_DEFAULT = 1
   ,umgebung_type_TEST    = 2
} ;
typedef enum umgebung_type_e umgebung_type_e ;

/* forward reference to all offered services */
struct log_config_t ;

extern __thread struct umgebung_t gt_umgebung ;


// section: Functions

// group: query

/* function: umgebung
 * Returns the <umgebung_t> for the current thread. */
extern umgebung_t * umgebung(void) ;

/* function: log_umgebung
 * Returns log configuration object <log_config_t> for the current thread. */
extern struct log_config_t * log_umgebung(void) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung
 * Test initialization process succeeds and global variables are set correctly. */
extern int unittest_umgebung(void) ;
/* function: unittest_umgebung_default
 * Test initialization thread with default impl. succeeds. */
extern int unittest_umgebung_default(void) ;
/* function: unittest_umgebung_testproxy
 * Test initialization thread with test impl. succeeds. */
extern int unittest_umgebung_testproxy(void) ;
#endif


/* struct: umgebung_tempmman_t
 * Defines interface for managing temporary memory. */
struct umgebung_tempmman_t {
   void * (*alloc)  (umgebung_tempmman_t * tempmman, size_t size_in_bytes ) ;
   int    (*resize) (umgebung_tempmman_t * tempmman, size_t size_in_bytes, void * memory_block ) ;
   int    (*free)   (umgebung_tempmman_t * tempmman, void ** memory_block ) ;
   //{ memory context
   int (*new_tempmem)    (umgebung_t * umg, /*out*/umgebung_tempmman_t ** tempmman ) ;
   int (*delete_tempmem) (umgebung_t * umg, umgebung_tempmman_t ** tempmman ) ;
   //}
} ;

/* struct: umgebung_t
 * Defines top level context for all software modules. */
struct umgebung_t {
   //{ object management
   umgebung_type_e         type ;
   uint16_t                resource_thread_count ;
   /* Virtual destructor: Allows different implementations to store a different desctructor. */
   int                  (* free_umgebung)  (umgebung_t * umg) ;
   //}

   struct log_config_t   * log ;
} ;

// group: lifetime

/* define: umgebung_INIT_MAINSERVICES
 * Static initializer for <umgebung_t>.
 * This ensures that in the main even without calling <init_process_umgebung> first
 * the global log service is available. */
#define umgebung_INIT_MAINSERVICES { umgebung_type_STATIC, 0, 0, &g_main_logservice }

/* function: init_process_umgebung
 * Initializes global context. Must be called as first function from the main thread.
 * The caller must ensure that it is called only once ! */
extern int init_process_umgebung(umgebung_type_e implementation_type) ;

/* function: init_thread_umgebung
 * */
extern int init_thread_umgebung(/*out*/umgebung_t * umg, umgebung_type_e implementation_type) ;

/* function: free_process_umgebung
 * Initializes global context. Must be called as first function in the whole system.
 * The caller must ensure that it is called only once ! */
extern int free_process_umgebung(void) ;

/* function: free_thread_umgebung
 * */
extern int free_thread_umgebung(umgebung_t * umg) ;

// group: internal

/* function: init_default_umgebung
 * */
extern int init_default_umgebung(/*out*/umgebung_t * umg) ;

/* function: init_testproxy_umgebung
 * */
extern int init_testproxy_umgebung(/*out*/umgebung_t * umg) ;


// section: inline implementations

/* define: inline umgebung
 * Inline implementation of <umgebung>.
 * Uses a global thread-local storage variable to implement the functionality.
 * > #define umgebung() (&gt_umgebung) */
#define umgebung() \
   ((const umgebung_t*)(&gt_umgebung))

#define log_umgebung() \
   (gt_umgebung.log)

#endif
