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
struct sysuser_t ;

/* typedef: struct processcontext_t
 * Export <processcontext_t>. */
typedef struct processcontext_t           processcontext_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_processcontext
 * Test initialization context of whole process succeeds.
 * And global variables are set correctly. */
int unittest_context_processcontext(void) ;
#endif


/* struct: processcontext_t
 * Defines computing environment / context which is shared
 * between all threads of computation.
 * It contains e.g. services which offfer read only values or services which
 * can be implemented with use of global locks. */
struct processcontext_t {
   /* variable: valuecache
    * Points to global read only variables. */
   struct valuecache_t *      valuecache ;
   /* variable: sysuser
    * Context for <sysuser_t> module. */
   struct sysuser_t *         sysuser ;
   /* variable: error
    * Data for <errorcontext_t> module. */
   struct {
      uint16_t *  stroffset ;
      uint8_t  *  strdata ;
   }                          error ;
   /* variable: staticmem
    * Describes static memory block and how much is in use. */
   struct {
      uint8_t *   addr ;
      uint16_t    size ;
      uint16_t    used ;
   }                          staticmem ;
   /* variable: initcount
    * Counts the number of successfull initialized services/subsystems.
    * This number is can be higher than 1 cause there are subsystems which
    * do not have a reference stored in <processcontext_t>. */
   uint16_t                   initcount ;
} ;

// group: lifetime

/* define: processcontext_INIT_STATIC
 * Static initializer. */
#define processcontext_INIT_STATIC        { 0, 0, errorcontext_INIT_STATIC, { 0, 0, 0 }, 0 }

/* function: init_processcontext
 * Initializes the current process context. There is exactly one process context
 * for the whole process. It is shared by all threads.
 * Function is called from <init_maincontext>. */
int init_processcontext(/*out*/processcontext_t * pcontext) ;

/* function: free_processcontext
 * Frees resources associated with <processcontext_t>.
 * This function is called from <free_maincontext> and you should never need to call it. */
int free_processcontext(processcontext_t * pcontext) ;

// group: static-memory

/* function: allocstatic_processcontext
 * Allocates size bytes of memory and returns the start address.
 * Used by modules during execution of their initonce_ functions.
 * This memory lives as long <processcontext_t> lives.
 * Must be called in reverse order of calls to <allocstatic_processcontext>. */
void * allocstatic_processcontext(processcontext_t * pcontext, uint8_t size) ;

/* function: freestatic_processcontext
 * Frees size bytes of last allocated memory.
 * Must be called in reverse order of calls to <allocstatic_processcontext>.
 * It is possible to free x calls to <allocstatic_processcontext> with one
 * call to <freestatic_processcontext> where all sizes are summed up. */
int freestatic_processcontext(processcontext_t * pcontext, uint8_t size) ;

/* function: sizestatic_processcontext
 * Returns size in bytes of allocated static memory. */
uint16_t sizestatic_processcontext(const processcontext_t * pcontext) ;



// section: inline implementation

/* define: sizestatic_processcontext
 * Implements <processcontext_t.sizestatic_processcontext>. */
#define sizestatic_processcontext(pcontext)  \
         ((uint16_t)(pcontext)->staticmem.used)

#endif
