/* title: ProcessContext
   Defines the global context of the running system process.
   The context contains references to global services shared
   between system threads.

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

   file: C-kern/api/context/processcontext.h
    Header file <ProcessContext>.

   file: C-kern/context/processcontext.c
    Implementation file <ProcessContext impl>.
*/
#ifndef CKERN_CONTEXT_PROCESSCONTEXT_HEADER
#define CKERN_CONTEXT_PROCESSCONTEXT_HEADER

// forward
struct valuecache_t ;

/* typedef: struct processcontext_t
 * Export <processcontext_t>. */
typedef struct processcontext_t        processcontext_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_processcontext
 * Test initialization context of whole process succeeds.
 * And global variables are set correctly. */
extern int unittest_context_processcontext(void) ;
#endif


/* struct: processcontext_t
 * Defines computing environment / context which is shared
 * between all threads of computation.
 * It contains e.g. services which offfer read only values or services which
 * can be implemented with use of global locks. */
struct processcontext_t {
   struct valuecache_t        * valuecache ;
   /* variable: initcount
    * Counts the number of successfull initialized services/subsystems.
    * This number is can be higher than 1 cause there are subsystems which
    * do not have a reference stored in <processcontext_t>. */
   uint16_t                   initcount ;
} ;

// group: lifetime

/* define: processcontext_INIT_FREEABLE
 * Static initializer. */
#define processcontext_INIT_FREEABLE   { 0, 0 }

/* function: init_processcontext
 * Is called from <init_context> to init to current process context. */
extern int init_processcontext(processcontext_t * pcontext) ;

/* function: free_processcontext
 * Frees resources associated with <processcontext_t>.
 * This function is called from <free_context> and you should never need to call it. */
extern int free_processcontext(processcontext_t * pcontext) ;


#endif
