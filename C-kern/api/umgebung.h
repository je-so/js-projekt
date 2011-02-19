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
 * umgebung_DEFAULT_IMPL - Default production ready implementation.
 * umgebung_TEST_IMPL    - Implements functionality without use of internal components but only with help of
 *                         C library calls. This ensures that software components which depends on <umgebung_t>
 *                         can be tested without mutual dependencies.
 * */
enum umgebung_type_e {
    umgebung_DEFAULT_IMPL = 1
   ,umgebung_TEST_IMPL
} ;
typedef enum umgebung_type_e umgebung_type_e ;

extern __thread umgebung_t * gt_current_umgebung ;

// section: Functions

// group: lifetime

/* function: init_process_umgebung
 * Initializes global context. Must be called as first function in the whole system.
 * The caller must ensure that it is called only once ! */
extern int init_process_umgebung(umgebung_type_e implementation_type) ;

/* function: init_thread_umgebung
 * Not implemented. */
extern int init_thread_umgebung(void) ;

/* function: free_process_umgebung
 * Initializes global context. Must be called as first function in the whole system.
 * The caller must ensure that it is called only once ! */
extern int free_process_umgebung(void) ;

/* function: free_thread_umgebung
 * Not implemented. */
extern int free_thread_umgebung(void) ;

// group: query

/* function: umgebung
 * Returns the <umgebung_t> for the current thread. */
extern umgebung_t * umgebung(void) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_umgebung_initprocess
 * Test initialization process succeeds and global variables are set correctly. */
extern int unittest_umgebung_initprocess(void) ;
/* function: unittest_umgebung_initthread
 * Not implemented. */
extern int unittest_umgebung_initthread(void) ;
/* function: unittest_umgebung_default
 * Not implemented.
 * Tests that default-umgebung implements <umgebung_t> correct. */
extern int unittest_umgebung_default(void) ;
/* function: unittest_umgebung_testproxy
 * Tests that testproxy implements <umgebung_t> correct. */
extern int unittest_umgebung_testproxy(void) ;
#endif


/* struct: umgebung_tempmman_t
 * Defines interface for managing temporary memory. */
struct umgebung_tempmman_t {
   void * (*alloc)  (umgebung_tempmman_t * tempmman, size_t size_in_bytes ) ;
   int    (*resize) (umgebung_tempmman_t * tempmman, size_t size_in_bytes, void * memory_block ) ;
   int    (*free)   (umgebung_tempmman_t * tempmman, void ** memory_block ) ;
} ;

/* struct: umgebung_t
 * Defines top level context for all software modules. */
struct umgebung_t {
   //{ memory context
   int (*new_tempmem)    (umgebung_t * umg, /*out*/umgebung_tempmman_t ** tempmman ) ;
   int (*delete_tempmem) (umgebung_t * umg, umgebung_tempmman_t ** tempmman ) ;
   //}

   //{ umgebung_t lifetime + type management
   umgebung_type_e type ;
   /* Virtual destructor. Allows different implementations to store a different desctructor. */
   int (*free_umgebung)  (umgebung_t * umg) ;
   //}
} ;

// group: lifetime

#define umgebung_INIT_FREEABLE   { 0, 0, 0, 0 }

/* function: init_umgebung
 * Initializes umgebung's interface. All other used components must be initialized before this call.
 * Do not call this constructor directly instead use <init_process_umgebung> or <init_thread_umgebung>.
 *
 * Not implemented. */
extern int init_umgebung(umgebung_t * umg) ;

/* function: init_umgebung_testproxy
 * Not implemented. */
extern int init_umgebung_testproxy(umgebung_t * umg) ;

/* function: free_umgebung
 * Not implemented. */
extern int free_umgebung(umgebung_t * umg) ;

/* function: free_umgebung_testproxy
 * Not implemented. */
extern int free_umgebung_testproxy(umgebung_t * umg) ;

// section: inline implementations

/* define: inline umgebung
 * Inline implementation of <umgebung>.
 * Uses a global thread-local storage variable ti implement the functionality.
 * > #define umgebung() (gt_current_umgebung) */
#define umgebung() \
   (gt_current_umgebung)


#endif
