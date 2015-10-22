/* title: ProcessContext

   Defines the global context of the running system process.
   The context contains references to global services shared
   between system threads.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/task/processcontext.h
    Header file <ProcessContext>.

   file: C-kern/task/processcontext.c
    Implementation file <ProcessContext impl>.
*/
#ifndef CKERN_TASK_PROCESSCONTEXT_HEADER
#define CKERN_TASK_PROCESSCONTEXT_HEADER

// forward
struct pagecache_blockmap_t;
struct syslogin_t;
struct valuecache_t;

/* typedef: struct processcontext_t
 * Export <processcontext_t>. */
typedef struct processcontext_t processcontext_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_processcontext
 * Test initialization context of whole process succeeds.
 * And global variables are set correctly. */
int unittest_task_processcontext(void);
#endif


/* struct: processcontext_t
 * Defines computing environment / context which is shared
 * between all threads of computation.
 * It contains e.g. services which offfer read only values or services which
 * can be implemented with use of global locks. */
struct processcontext_t {
   /* variable: syslogin
    * Context for <syslogin_t> module. */
   struct syslogin_t*        syslogin;
   /* variable: error
    * Data for <errorcontext_t> module. */
   struct {
      uint16_t* stroffset;
      uint8_t*  strdata;
   }                         error;
   /* variable: error
    * Shared <pagecache_blockmap_t> used in <pagecache_impl_t>. */
   struct
   pagecache_blockmap_t*     blockmap;
   /* variable: staticmemblock
    * Start address of static memory block. */
   void*                     staticmemblock;
   /* variable: initcount
    * Counts the number of successfully initialized services/subsystems.
    * This number counts all subsystems even if they
    * do not have state stored in <processcontext_t>. */
   uint16_t                  initcount;
} ;

// group: lifetime

/* define: processcontext_INIT_STATIC
 * Static initializer. */
#define processcontext_INIT_STATIC { 0, errorcontext_INIT_STATIC, 0, 0, 0 }

/* function: init_processcontext
 * Initializes the current process context. There is exactly one process context
 * for the whole process. It is shared by all threads.
 * Function is called from <maincontext_t.init_maincontext>. */
int init_processcontext(/*out*/processcontext_t * pcontext) ;

/* function: free_processcontext
 * Frees resources associated with <processcontext_t>.
 * This function is called from <maincontext_t.free_maincontext> and you should never need to call it. */
int free_processcontext(processcontext_t * pcontext) ;

// group: query

/* function: isstatic_processcontext
 * Returns true if pcontext equals <processcontext_INIT_STATIC>. */
bool isstatic_processcontext(const processcontext_t * pcontext) ;

/* function: extsize_processcontext
 * Gibt Speicher zurück, der zusätzlich von <init_processcontext> benötigt wird. */
size_t extsize_processcontext(void);

#endif