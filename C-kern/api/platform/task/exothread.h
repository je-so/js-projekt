/* title: Exothread
   See also <Taskmanagement>.

   *exo*       - A prefix meaning outside or external (Greek origin).
   *exothread* - A thread which stores its execution context and state
                 in an external object.


   Exothreads are grouped together and every such group is executed
   by one system thread. Synchronization between exothreads of one group
   is therefore not necessary.

   A system thread has a stack, a processor state and process context.
   Its state (local variables and call hierarchie) is implicitly stored
   on the stack and the sytem kernel manages the execution context in some
   internal structures.

   An exothread is implemented as a function with an object of type
   exothread_state_t as its parameter.
   Local variables can be used but are not preserved between different
   invocations. Other functions can also be called. After a short period
   of time the exothread should return to the exothread scheduler, so
   that more than one exothread can be run in a cooperative
   quasi parallel manner.

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

   file: C-kern/api/platform/task/exothread.h
    Header file <Exothread>.

   file: C-kern/platform/shared/task/exothread.c
    Implementation file <Exothread impl>.
*/
#ifndef CKERN_PLATFORM_TASK_EXOTHREAD_HEADER
#define CKERN_PLATFORM_TASK_EXOTHREAD_HEADER

// forward
struct slist_node_t ;

/* typedef: exothread_t typedef
 * Export <exothread_t>. */
typedef struct exothread_t             exothread_t ;

/* typedef: exothread_subtype_t typedef
 * Export subtype template <exothread_subtype_t>.
 * This type is only a place holder for a concrete subtype and it is never used. */
typedef struct exothread_subtype_t     exothread_subtype_t ;

/* typedef: exothread_main_f
 * Function pointer to exothread implementation. */
typedef int /*err code (0 == OK)*/  (* exothread_main_f) (exothread_t * xthread) ;

/* typedef: exothread_subtype_main_f
 * Function pointer to exothread subtype implementation.
 * This type is only a place holder for a concrete subtype implementation function and it is never used. */
typedef int /*err code (0 == OK)*/  (* exothread_subtype_main_f) (exothread_subtype_t * xthread) ;

/* enums: exothread_flag_e
 * Flags to describe internal state of <exothread_t>.
 *
 * exothread_flag_HOLDINGRESOURCE - Indicates that an <exothread_t> holds resources which must freed.
 *                                  If its main function returns an error the state is set to label *exothread_FREE:*
 *                                  before it is executed again.
 *                                  This flag is set with a call to <setholdingresource_exothread>.
 * exothread_flag_FINISH          - The xthread routine signaled that it has finished its computation.
 *                                  This flag is set automatically if xthread returns an error and flag
 *                                  <exothread_flag_HOLDINGRESOURCE> is not set.
 * exothread_flag_RUN             - Indicates that <exothread_t.main> has called at least once.
 * */
enum exothread_flag_e {
    exothread_flag_HOLDINGRESOURCE = 1
   ,exothread_flag_FINISH          = 2
   ,exothread_flag_RUN             = 4
} ;

typedef enum exothread_flag_e          exothread_flag_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_exothread
 * Tests exothread functionality. */
int unittest_platform_task_exothread(void) ;
#endif


/* struct: exothread_t
 * Holds state and execution context of an exothread.
 * To support additional state variables and in/out arguments
 * to have to subtype from this type. */
struct exothread_t {
   // group: private variables
   /* variable: next
    * Pointer to next exothread. Used by <exoscheduler_t>. */
   struct slist_node_t  * next ;
   /* variable: main
    * Pointer to implementation of exothread function. */
   exothread_main_f     main ;
   /* variable: instr_ptr
    * Instruction pointer holds position of instruction where execution should continue.
    * The value 0 is a special value. It indicates the execution of thread for the first time. */
   void                 * instr_ptr ;
   /* variable: returncode
    * Holds return code of exothread.
    * The first returned error code is stored.
    * <iserror_exothread> uses this field to determine if an error has occurred.
    *
    * Values:
    * 0     - Exothread returned success
    * else  - Error code */
   int                  returncode ;
   /* variable: flags
    * Holds status information encoded as <exothread_flag_e>. */
   uint16_t             flags ;
} ;

// group: lifetime

/* define: exothread_INIT_FREEABLE
 * Static initializer. */
#define exothread_INIT_FREEABLE        { .main = 0 }

/* function: init_exothread
 * Inits <exothread_t> objects.
 * The object is not registered with any type of scheduler. */
int init_exothread(/*out*/exothread_t * xthread, exothread_main_f main_fct) ;

/* function: initsubtype_exothread
 * Inits the <exothread_t> part of a subtype.
 * The exothread_t part (basetype) is accessed with
 * > &xthread->xthread
 * All other fields of the subtype are left untouched.
 * The functionality is equal to <init_exothread>.
 * The name <initsubtype_exothread> is derived from the type of the first parameter
 * which must be a subtype of <exothread_t>. */
int initsubtype_exothread(/*out*/exothread_subtype_t * xthread, exothread_subtype_main_f main_fct) ;

/* function: free_exothread
 *
 * Returns:
 * 0     - All resource freed.
 * EBUSY - Xthread has not run to end. Allocated resources may be lost. */
int free_exothread(exothread_t * xthread) ;

// group: execution

/* function: abort_exothread
 * Marks xthread eiter as *FINISH* or state set to *exothread_FREE:*.
 * If the xthread holds no resources then it is marked a as finished.
 * If it holds resources the state is changed to *exothread_FREE:*
 * and returncode is set to ECANCELED. */
void abort_exothread(exothread_t * xthread) ;

/* function: run_exothread
 * Calls <exothread_t.main> function a single time.
 * The flags are changed if necessary and the state
 * could be set to *exothread_FREE:*. */
int run_exothread(exothread_t * xthread) ;

/* function: yield_exothread
 * Gives up processing time to other exothreads.
 * It returns from current exothread and sets the state
 * to a generated anonymous label after the return statememt.
 * Next time the xthread is executed it starts after the yield. */
void yield_exothread(void) ;

// group: change

/* function: clearholdingresource_exothread
 * Marks current xthread that it holds no more resources.
 * This flag is cleared as default value.
 * See <setholdingresource_exothread> for a description. */
void clearholdingresource_exothread(void) ;

/* function: finish_exothread
 * Marks current xthread that it has finished its computation.
 * After having set this flag the xthread function is never called back again.
 * Even in case of returning an error and <isholdingresource_exothread>
 * returning true.
 *
 * Automatism:
 * This flag is set for you if <isholdingresource_exothread> returns false
 * and the xthread function returns an error. */
void finish_exothread(void) ;

/* function: rememberstate_exothread
 * Sets the state label of the current exothread to current position.
 * The next time the xthread is run it continues execution after
 * this function call.
 *
 * Attention:
 * Do not forget to set the state after *exothread_INIT:* initialized
 * the current thread successfully and before you give up the processor
 * (i.e. return 0). */
void rememberstate_exothread(void) ;

/* function: setholdingresource_exothread
 * Sets flag of current xthread indicating that it holds resources.
 * You should set this flag after initializing the resources
 * as _INIT_FREEABLE. If something goes wrong during set up of multiple resources
 * and an error is returned the function is called another time
 * with the state set to *exothread_FREE:*.
 * After having freed all resource it is safe to clear this flag.
 *
 * Automatism:
 * This flag is cleared for you if xthread function returns an error
 * and before itis called with the state set to *exothread_FREE:*. */
void setholdingresource_exothread(void) ;

/* function: setstate_exothread
 * Sets the state label of the current exothread.
 * The next time the xthread is run it continues execution at this label.
 *
 * Attention:
 * Do not forget to set the state after *exothread_INIT:* initialized
 * the current thread successfully and before you give up the processor
 * (i.e. return 0). */
void setstate_exothread(void * instr_ptr) ;

// group: query

/* function: iserror_exothread
 * Returns true if xthread once returned an error.
 * Only the first returned error is reported in returncode (0 => OK). */
bool iserror_exothread(const exothread_t * xthread) ;

/* function: isfinish_exothread
 * Returns true if xthread has finished its computation. */
bool isfinish_exothread(const exothread_t * xthread) ;

/* function: isholdingresource_exothread
 * Returns true if xthread holds resources which must be freed. */
bool isholdingresource_exothread(const exothread_t * xthread) ;

/* function: inarg_exothread
 * Returns pointer to in arguments of exothread.
 * You should call this function only from within an exothread.
 * You need to subtype <exothread_t> and make parameter xthread
 * a pointer to the subtype before this works. */
const void * inarg_exothread(void) ;

/* function: outarg_exothread
 * Returns pointer to out arguments of exothread.
 * You should call this function only from within an exothread.
 * You need to subtype <exothread_t> and make parameter xthread
 * a pointer to the subtype before this works. */
void * outarg_exothread(void) ;

/* function: returncode_exothread
 * This function returns the returned value of a finished exothread.
 * This value is only if <isfinish_exothread> returns true.
 * A value != 0 indicates an error. */
int returncode_exothread(const exothread_t * xthread) ;

// group: implementation macros

/* define: declare_inparam_exothread
 * Declares parameter *_param_name* which points to input arguments.
 * > typeof(inarg_exothread())  _param_name = inarg_exothread() */
#define declare_inparam_exothread(_param_name) \
   typeof(inarg_exothread())  _param_name = inarg_exothread()

/* define: declare_outparam_exothread
 * Declares parameter *_param_name* which points to output arguments.
 * > typeof(outarg_exothread()) _param_name = outarg_exothread() */
#define declare_outparam_exothread(_param_name) \
   typeof(outarg_exothread()) _param_name = outarg_exothread()

/* define: for_exothread
 * Same as *C99 for()* except that after every cycle exothread is yielded.
 *
 * Parameter:
 * _init        - Initialize xthread variable.
 * _bCondition  - Condition which must be true to execute body of for loop.
 * _next        - Computation which changes the loop condition and which is executed after the body has executed
 *                and before the next check of the condition.
 *
 * Implemented as:
 * >    _init ;
 * >    rememberstate_exothread() ;
 * >    for(; _bCondition ; ( __extension__ ({ _next ; return 0 ; 0 ; })))
 *
 * Example:
 * > for_exothread( xthread->loopcount = 0, xthread->loopcount < 10, ++ xthread->loopcount ) {
 * >    do_some_stuff(xthread) ;
 * > } */
#define for_exothread( _init , _bCondition , _next ) \
   _init ;                                                              \
   rememberstate_exothread() ;                                          \
   for(; _bCondition ; ( __extension__ ({ _next ; return 0 ; 0 ; })))   \

/* define: jumpstate_exothread
 * Jumps to state of current thread.
 * Can only be used within a running exothread.
 * This function is implemented as a macro
 * cause it uses the *not portable* gcc extension
 * (Labels as Values + Computed Goto Statement).
 *
 * Portable solution:
 * It is possible to use a switch() statement
 * instead of a goto but it is not implemented.
 *
 * >  do {
 * >     exothread_t * self_ = (exothread_t*) xthread ;
 * >     if (self_->instr_ptr) {
 * >        goto *self_->instr_ptr ;
 * >     }
 * >     if (isholdingresource_exothread(self_)) {
 * >        clearholdingresource_exothread() ;
 * >        goto exothread_FREE ;
 * >     }
 * >     goto exothread_INIT ;
 * >  } while(0) */
#define jumpstate_exothread()                            \
   do {                                                  \
      exothread_t * self_ = (exothread_t*)xthread;       \
      if (self_->instr_ptr) {                            \
         __extension__ ({ goto *self_->instr_ptr; 0; }); \
      }                                                  \
      if (isholdingresource_exothread(self_)) {          \
         clearholdingresource_exothread() ;              \
         goto exothread_FREE ;                           \
      }                                                  \
      goto exothread_INIT ;                              \
   } while(0)

/* define: while_exothread( _bCondition )
 * Same as *C99 while()* except that after every cycle exothread is yielded.
 *
 * Parameter:
 * _bCondition  - Condition which must be true to execute body of while loop.
 *
 * Implemented as:
 * >    rememberstate_exothread() ;
 * >    for(; _bCondition ; ( __extension__ ({ return 0 ; 0 ; })))
 *
 * Example:
 * > while_exothread( xthread->command_queue_length > 0 ) {
 * >    process_command_from_queue(xthread) ;
 * > } */
#define while_exothread( _bCondition ) \
   rememberstate_exothread() ;         \
   for(; _bCondition ; ( __extension__ ({ return 0 ; 0 ; })))


/* struct: exothread_subtype_t
 * Defines an abstract template of <exothread_t> subtypes.
 * The template must contain at least the xthread field as first one.
 * All other fields are optional. */
struct exothread_subtype_t {
   /* variable: xthread
    * Base type <exothread_t>.
    * Must be the first in the subtype structure. */
   exothread_t  xthread ;
   /* variable: inarg
    * Anonymous structures which contains the input arguments.
    * Can be at any position (except first) in the subtype structure.
    * You can access this field with <inarg_exothread> or <declare_inparam_exothread>.
    * Accessing it directly should be avoided. */
   struct {
      int x, y, z;
   } inarg ;
   /* variable: outarg
    * Anonymous structures which contains the output arguments.
    * Can be at any position (except first) in the subtype structure.
    * You can access this field with <outarg_exothread> or <declare_outparam_exothread>.
    * Accessing it directly should be avoided. */
   struct {
      int sum ;
   } outarg ;
   /* variable: internal
    * One or variables which describes the internal state of the <exothread_t> subtype.
    * Can be at any position (except first) in the subtype structure. */
   int internal ;
} ;


// section: inline implementation

/* define: clearholdingresource_exothread
 * Implements <exothread_t.clearholdingresource_exothread>.
 * > xthread->flags &= ~exothread_flag_HOLDINGRESOURCE */
#define clearholdingresource_exothread() \
   ((exothread_t*)xthread)->flags = (typeof(((exothread_t*)0)->flags)) (((exothread_t*)xthread)->flags & ~exothread_flag_HOLDINGRESOURCE)

/* define: finish_exothread
 * Implements <exothread_t.finish_exothread>.
 * > xthread->flags |= xthread_flag_FINISH */
#define finish_exothread() \
   ((exothread_t*)xthread)->flags = (typeof(((exothread_t*)0)->flags)) (((exothread_t*)xthread)->flags | exothread_flag_FINISH)

/* define: inarg_exothread
 * Implements <exothread_t.inarg_exothread>.
 * > ((const typeof(xthread->inarg) *) & xthread->inarg) */
#define inarg_exothread() \
   ((const typeof(xthread->inarg) *) & xthread->inarg)

/* define: initsubtype_exothread
 * Implements <exothread_t.initsubtype_exothread>.
 * Checks with <static_assert> that main_fct expects xthread as single argument.
 * > init_exothread(&xthread->xthread, (exothread_main_f) main_fct) */
#define initsubtype_exothread(_xthread, _main_fct) \
   ( __extension__ ({                                                                   \
         static_assert( ((int(*)(typeof(_xthread)))0) == (typeof(_main_fct)) 0, "" ) ;  \
         static_assert( ((exothread_t*)0) == &((typeof(_xthread))0)->xthread, "" ) ;    \
         init_exothread(&(_xthread)->xthread, (exothread_main_f) (_main_fct)) ;         \
   }))

/* define: iserror_exothread
 * Implements <exothread_t.iserror_exothread>.
 * > (0 != xthread->returncode) */
#define iserror_exothread(_xthread) \
   (0 != (_xthread)->returncode)

/* define: isfinish_exothread
 * Implements <exothread_t.isfinish_exothread>.
 * > (0 != (xthread->flags & exothread_flag_FINISH)) */
#define isfinish_exothread(_xthread) \
   (0 != ((_xthread)->flags & exothread_flag_FINISH))

/* define: isholdingresource_exothread
 * Implements <exothread_t.isholdingresource_exothread>.
 * > (0 != (xthread->flags & exothread_flag_HOLDINGRESOURCE)) */
#define isholdingresource_exothread(_xthread) \
   (0 != ((_xthread)->flags & exothread_flag_HOLDINGRESOURCE))

/* define: outarg_exothread
 * Implements <exothread_t.outarg_exothread>.
 * > (& xthread->outarg) */
#define outarg_exothread() \
   (& xthread->outarg)

/* define: rememberstate_exothread
 * Implements <exothread_t.rememberstate_exothread>.
 * > setstate_exothread(  && STATE_REMEMBER_??? ) ; STATE_REMEMBER_???: */
#define rememberstate_exothread() \
   setstate_exothread( __extension__ && CONCAT( exothread_STATE_REMEMBER_, __LINE__ ) ) ; \
   CONCAT( exothread_STATE_REMEMBER_, __LINE__ ) :                                        \

/* define: returncode_exothread
 * Implements <exothread_t.returncode_exothread>.
 * > (xthread->returncode) */
#define returncode_exothread(_xthread) \
   ((_xthread)->returncode)

/* define: setholdingresource_exothread
 * Implements <exothread_t.setholdingresource_exothread>.
 * > xthread->flags |= exothread_flag_HOLDINGRESOURCE */
#define setholdingresource_exothread() \
   ((exothread_t*)xthread)->flags = (typeof(((exothread_t*)0)->flags)) (((exothread_t*)xthread)->flags | exothread_flag_HOLDINGRESOURCE)

/* define: setstate_exothread
 * Implements <exothread_t.setstate_exothread>.
 * > xthread->instr_ptr = instr_ptr */
#define setstate_exothread(_instr_ptr) \
   ((exothread_t*)xthread)->instr_ptr = _instr_ptr

/* define: yield_exothread
 * Implements <exothread_t.yield_exothread>.
 * > setstate_exothread(  && STATE_YIELD_??? ) ; return 0 ; STATE_YIELD_???: */
#define yield_exothread() \
   setstate_exothread( __extension__ && CONCAT( exothread_STATE_YIELD_, __LINE__ ) ) ; \
   return 0 ;                                                                          \
   CONCAT( exothread_STATE_YIELD_, __LINE__ ) :                                        \

#endif
