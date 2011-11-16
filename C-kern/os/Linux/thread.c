/* title: Thread Linux
   Implements <Thread>.

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

   file: C-kern/api/os/thread.h
    Header file of <Thread>.

   file: C-kern/os/Linux/thread.c
    Linux specific implementation <Thread Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/os/sync/semaphore.h"
#include "C-kern/api/os/sync/signal.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/errortimer.h"
#endif

typedef struct osthread_startargument_t   osthread_startargument_t ;

struct osthread_startargument_t {
   /* variable: nr_threads
    * Contains the number of threads in this group.
    * All threads share the same thread_main function and the same argumen at the beginning. */
   uint32_t          nr_threads ;
   osthread_t      * osthread ;
   /* variable: isAbort
    * Indicates if not all threads could have been started successfully.
    * Ensures transactional behaviour. */
   volatile bool     isAbort ;
   /* variable: isFreeEvents
    * Indicates to a thread that it is responsible to free all event semaphores. */
   bool              isFreeEvents ;
   /* variable: isfreeable_semaphore
    * Threads signal on this event before entering the main function.
    * The first thread waits on this event to know for all other threads have entered
    * the main function. Therefore it is safe to free the two event semaphores which
    * are no longer of use. */
   semaphore_t       isfreeable_semaphore ;
   /* variable: isvalid_abortflag
    * Threads wait on startup for this event.
    * After this event has occurred variable <isAbort> contains the correct value. */
   semaphore_t       isvalid_abortflag ;
   umgebung_t        umg ;
   stack_t           signalstack ;
} ;

/* variable: gt_self_osthread
 * Refers for every thread to the osthread_t variable.
 * Is is located on the thread stack so no additional memory is needed. */
__thread  osthread_t       gt_self_osthread        = { sys_mutex_INIT_DEFAULT, 0, task_callback_INIT_FREEABLE, sys_thread_INIT_FREEABLE, 0, memoryblock_aspect_INIT_FREEABLE, 0, 0 } ;

/* variable: s_offset_osthread
 * Contains the calculated offset from start of stack thread to &<gt_self_osthread>. */
static size_t              s_offset_osthread       = 0 ;

#ifdef KONFIG_UNITTEST
/* variable: s_error_newgroup
 * Simulates an error in <newgroup_osthread>. */
static test_errortimer_t   s_error_newgroup = test_errortimer_INIT_FREEABLE ;
#endif



// section: osthread_stack_t

/* function: signalstacksize_osthreadstack
 * Returns the minimum size of the signal stack.
 * The signal stack is used in case of a signal or
 * exceptions which throw a signal. If for example the
 * the thread stack overflows SIGSEGV signal is thrown.
 * To handle this case the system must have an extra signal stack
 * cause signal handling needs stack space. */
static inline unsigned signalstacksize_osthreadstack(void)
{
   return MINSIGSTKSZ ;
}

/* function: threadstacksize_osthreadstack
 * Returns the minimum size of the thread stack.
 * The stack should be protected against overflow with help
 * of protected virtual memory pages. */
static inline unsigned threadstacksize_osthreadstack(void)
{
   return PTHREAD_STACK_MIN ;
}

/* function: framestacksize_osthreadstack
 * Returns the size of a stack frame for one thread.
 * The last tail protection page is not included. */
static inline size_t framestacksize_osthreadstack(void)
{
   const size_t page_size = pagesize_vm() ;
   const size_t nr_pages1 = (size_t) (signalstacksize_osthreadstack() + page_size - 1) / page_size ;
   const size_t nr_pages2 = (size_t) (threadstacksize_osthreadstack() + page_size - 1) / page_size ;

   return page_size * (2 + nr_pages1 + nr_pages2) ;
}

static osthread_stack_t getsignalstack_osthreadstack(const osthread_stack_t * stackframe)
{
   const size_t page_size = pagesize_vm() ;
   const size_t nr_pages1 = (size_t) (signalstacksize_osthreadstack() + page_size - 1) / page_size ;

   osthread_stack_t stack = {
         .addr = stackframe->addr + page_size,
         .size = page_size * nr_pages1
   } ;

   return stack ;
}

static osthread_stack_t getthreadstack_osthreadstack(const osthread_stack_t * stackframe)
{
   const size_t page_size = pagesize_vm() ;
   const size_t nr_pages1 = (size_t) (signalstacksize_osthreadstack() + page_size - 1) / page_size ;
   const size_t nr_pages2 = (size_t) (threadstacksize_osthreadstack() + page_size - 1) / page_size ;

   osthread_stack_t stack = {
         .addr = stackframe->addr + page_size * (2 + nr_pages1),
         .size = page_size * nr_pages2
   } ;

   return stack ;
}

static int free_osthreadstack(osthread_stack_t * stackframe)
{
   int err ;

   void *  addr = stackframe->addr ;
   size_t  size = stackframe->size ;

   if (size) {
      *stackframe = (memoryblock_aspect_t) memoryblock_aspect_INIT_FREEABLE ;
      if (munmap(addr, size)) {
         err = errno ;
         LOG_SYSERR("munmap", err) ;
         LOG_PTR(addr) ;
         LOG_SIZE(size) ;
         goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int init_osthreadstack(osthread_stack_t * stackframe, uint32_t nr_threads)
{
   int err ;
   const size_t page_size = pagesize_vm() ;
   const size_t framesize = framestacksize_osthreadstack() ;
   osthread_stack_t stack = {
         .addr = MAP_FAILED,
         .size = page_size/*last tail protection page*/ + nr_threads * framesize
   } ;

   PRECONDITION_INPUT(nr_threads != 0, ABBRUCH, ) ;

   if (     stack.size  < (size_t)  (stack.size-page_size)
         || framesize  != (size_t) ((stack.size-page_size) / nr_threads) ) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(0) ;
      goto ABBRUCH ;
   }

   stack.addr = (uint8_t*) mmap( 0, stack.size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 ) ;
   if (MAP_FAILED == stack.addr) {
      err = errno ;
      LOG_SYSERR("mmap", err) ;
      LOG_SIZE(stack.size) ;
      goto ABBRUCH ;
   }

   /* Stack layout:
    *      size        : protection
    * ----------------------------------
    * | [page_size]    : NONE       |
    * | [signalstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * | [threadstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * ...
    * | [signalstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * | [threadstack]  : READ,WRITE |
    * | [page_size]    : NONE       |
    * */

   osthread_stack_t signalstack = getsignalstack_osthreadstack(&stack) ;
   osthread_stack_t threadstack = getthreadstack_osthreadstack(&stack) ;
   for(uint32_t i = 0; i < nr_threads; ++i) {
      if (mprotect(signalstack.addr, signalstack.size, PROT_READ|PROT_WRITE)) {
         err = errno ;
         LOG_SYSERR("mprotect", err) ;
         goto ABBRUCH ;
      }
      signalstack.addr += framesize ;

      if (mprotect(threadstack.addr, threadstack.size, PROT_READ|PROT_WRITE)) {
         err = errno ;
         LOG_SYSERR("mprotect", err) ;
         goto ABBRUCH ;
      }
      threadstack.addr += framesize ;
   }

   *stackframe = stack ;

   return 0 ;
ABBRUCH:
   if (MAP_FAILED != stack.addr) {
      if (munmap(stack.addr, stack.size)) {
         LOG_SYSERR("munmap", errno) ;
         LOG_PTR(stack.addr) ;
         LOG_SIZE(stack.size) ;
      }
   }
   LOG_ABORT(err) ;
   return err ;
}


// section: osthread_t

// group: helper

/* function: startpoint_osthread
 * The start function of the thread.
 * This is the same for all threads.
 * It initializes signalstack the thread global umgebung variable
 * and calls the user supplied main function. */
static void * startpoint_osthread(osthread_startargument_t * startarg)
{
   int            err ;
   osthread_t   * osthread = startarg->osthread ;

   assert(osthread         == &gt_self_osthread) ;

   err = initmove_umgebung(&gt_umgebung, &startarg->umg) ;
   if (err) {
      LOG_CALLERR("initmove_umgebung",err) ;
      goto ABBRUCH ;
   }

   if (sys_thread_INIT_FREEABLE == pthread_self()) {
      err = EINVAL ;
      LOG_ERRTEXT(FUNCTION_WRONG_RETURNVALUE("pthread_self", STR(sys_thread_INIT_FREEABLE))) ;
      goto ABBRUCH ;
   }

   err = wait_semaphore(&startarg->isvalid_abortflag) ;
   if (err) {
      LOG_CALLERR("wait_semaphore",err) ;
      goto ABBRUCH ;
   }

   if (startarg->isAbort) {
      // silently ignore abort, exit created thread
   } else {

      err = signal_semaphore(&startarg->isfreeable_semaphore, 1) ;
      if (err) {
         LOG_CALLERR("signal_semaphore",err) ;
         goto ABBRUCH ;
      }

      if (startarg->isFreeEvents) {
         for(uint32_t i = startarg->nr_threads; i ; --i) {
            err = wait_semaphore(&startarg->isfreeable_semaphore) ;
            if (err) {
               LOG_CALLERR("wait_semaphore",err) ;
               goto ABBRUCH ;
            }
         }
         err = free_semaphore(&startarg->isfreeable_semaphore) ;
         if (!err) err = free_semaphore(&startarg->isvalid_abortflag) ;
         if (err) {
            LOG_CALLERR("free_semaphore",err) ;
            goto ABBRUCH ;
         }
      }

      // do not access startarg after sigaltstack (startarg is stored on this stack)
      err = sigaltstack( &startarg->signalstack, (stack_t*)0) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("sigaltstack", err) ;
         goto ABBRUCH ;
      }

      osthread->returncode = osthread->task.fct(osthread->task.arg) ;
   }

   err = free_umgebung(&gt_umgebung) ;
   if (err) {
      LOG_CALLERR("free_umgebung",err) ;
      goto ABBRUCH ;
   }

   return (void*)0 ;
ABBRUCH:
   abort_umgebung(err) ;
   return (void*)err ;
}

void * calculateoffset_thread(osthread_stack_t * start_arg)
{
   uint8_t          * osthread    = (uint8_t*) &gt_self_osthread ;

   s_offset_osthread = (size_t) (osthread - start_arg->addr) ;
   assert(s_offset_osthread < start_arg->size) ;

   return 0 ;
}

// group: implementation

int initonce_osthread(umgebung_t * umg)
{
   /* calculate position of &gt_self_osthread
    * relative to start threadstack. */
   int err ;
   pthread_attr_t    thread_attr ;
   sys_thread_t      sys_thread        = sys_thread_INIT_FREEABLE ;
   osthread_stack_t  stackframe        = memoryblock_aspect_INIT_FREEABLE ;
   bool              isThreadAttrValid = false ;

   (void) umg ;

   // init main osthread_t
   if (!gt_self_osthread.groupnext) {
      err = init_mutex(&gt_self_osthread.lock) ;
      if (err) goto ABBRUCH ;
      gt_self_osthread.sys_thread = pthread_self() ;
      gt_self_osthread.nr_threads = 1 ;
      gt_self_osthread.groupnext  = &gt_self_osthread ;
   }

   err = init_osthreadstack(&stackframe, 1) ;
   if (err) goto ABBRUCH ;

   osthread_stack_t threadstack = getthreadstack_osthreadstack(&stackframe) ;

   err = pthread_attr_init(&thread_attr) ;
   if (err) {
      LOG_SYSERR("pthread_attr_init",err) ;
      goto ABBRUCH ;
   }
   isThreadAttrValid = true ;

   err = pthread_attr_setstack(&thread_attr, threadstack.addr, threadstack.size) ;
   if (err) {
      LOG_SYSERR("pthread_attr_setstack", err) ;
      goto ABBRUCH ;
   }

   memset( threadstack.addr, 0, threadstack.size) ;
   static_assert( (void* (*) (typeof(&threadstack)))0 == (typeof(&calculateoffset_thread))0, "calculateoffset_osthread expects &threadstack as argument" ) ;
   err = pthread_create( &sys_thread, &thread_attr, (void*(*)(void*))&calculateoffset_thread, &threadstack) ;
   if (err) {
      sys_thread = sys_thread_INIT_FREEABLE ;
      LOG_SYSERR("pthread_create", err) ;
      goto ABBRUCH ;
   }

   err = pthread_join(sys_thread, 0) ;
   if (err) {
      LOG_SYSERR("pthread_join", err) ;
      goto ABBRUCH ;
   }

   isThreadAttrValid = false ;
   err = pthread_attr_destroy(&thread_attr) ;
   if (err) {
      LOG_SYSERR("pthread_attr_destroy", err) ;
      goto ABBRUCH ;
   }

   err = free_osthreadstack(&stackframe) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   if (sys_thread_INIT_FREEABLE != sys_thread) {
      (void) pthread_join(sys_thread, 0) ;
   }
   if (isThreadAttrValid) {
      (void) pthread_attr_destroy(&thread_attr) ;
   }
   (void) free_osthreadstack(&stackframe) ;
   LOG_ABORT(err) ;
   return err ;
}

int freeonce_osthread(umgebung_t * umg)
{
   (void) umg ;
   return 0 ;
}

int delete_osthread(osthread_t ** threadobj)
{
   int err = 0 ;
   int err2 ;

   if (!threadobj || !*threadobj) {
      return 0 ;
   }

   osthread_t      * osthread   = *threadobj ;
   osthread_stack_t  stackframe = osthread->stackframe ;

   *threadobj = 0 ;

   err2 = join_osthread(osthread) ;
   if (err2) err = err2 ;

   osthread_t * nextthread = osthread ;
   do {
      err2 = free_mutex(&nextthread->lock) ;
      if (err2) err = err2 ;
      nextthread = nextthread->groupnext ;
   } while( osthread != nextthread ) ;

   err2 = free_osthreadstack(&stackframe) ;
   if (err2) err = err2 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

#undef newgroup_osthread
int newgroup_osthread(/*out*/osthread_t ** threadobj, task_callback_f thread_main, struct callback_param_t * start_arg, uint32_t nr_of_threads)
{
   int err ;
   int err2 = 0 ;
   pthread_attr_t    thread_attr ;
   umgebung_t        shared_umgebung   = umgebung_INIT_FREEABLE ;
   osthread_t      * prev_osthread     = 0 ;
   osthread_t      * next_osthread     = 0 ;
   osthread_t      * osthread          = 0 ;
   osthread_stack_t  stackframe        = memoryblock_aspect_INIT_FREEABLE ;
   semaphore_t       isfreeable_semaphore = semaphore_INIT_FREEABLE ;
   semaphore_t       isvalid_abortflag = semaphore_INIT_FREEABLE ;
   bool              isThreadAttrValid = false ;
   const size_t      framesize         = framestacksize_osthreadstack() ;

   PRECONDITION_INPUT(nr_of_threads != 0, ABBRUCH,)
   PRECONDITION_INPUT(nr_of_threads < 256, ABBRUCH,)

   err = init_osthreadstack(&stackframe, nr_of_threads) ;
   if (err) goto ABBRUCH ;

   err = init_semaphore(&isfreeable_semaphore, 0) ;
   if (err) goto ABBRUCH ;

   err = init_semaphore(&isvalid_abortflag, 0) ;
   if (err) goto ABBRUCH ;

   osthread_stack_t signalstack = getsignalstack_osthreadstack(&stackframe) ;
   osthread_stack_t threadstack = getthreadstack_osthreadstack(&stackframe) ;
   prev_osthread = (osthread_t*) (threadstack.addr + s_offset_osthread) ;
   next_osthread = prev_osthread  ;
   osthread      = prev_osthread  ;

   if (nr_of_threads > 1) {
      err = init_umgebung(&shared_umgebung, umgebung_type_MULTITHREAD) ;
   } else {
      err = init_umgebung(&shared_umgebung, umgebung_type_SINGLETHREAD) ;
   }
   if (err) goto ABBRUCH ;

   for(uint32_t i = 0; i < nr_of_threads; ++i) {

      sys_thread_t               sys_thread = sys_thread_INIT_FREEABLE ;
      osthread_startargument_t * startarg   = (osthread_startargument_t*) signalstack.addr ;

      *startarg = (osthread_startargument_t) {
         .nr_threads   = nr_of_threads,
         .osthread     = next_osthread,
         .isAbort      = false,
         .isFreeEvents = (0 == i),
         .isfreeable_semaphore = isfreeable_semaphore,
         .isvalid_abortflag    = isvalid_abortflag,
         .umg          = umgebung_INIT_FREEABLE,
         .signalstack  = (stack_t) { .ss_sp = signalstack.addr, .ss_flags = 0, .ss_size = signalstack.size }
      } ;

      ONERROR_testerrortimer(&s_error_newgroup, UNDO_LOOP) ;
      err = initcopy_umgebung(&startarg->umg, &shared_umgebung) ;
      if (err) {
         LOG_CALLERR("initcopy_umgebung",err) ;
         goto UNDO_LOOP ;
      }

      ONERROR_testerrortimer(&s_error_newgroup, UNDO_LOOP) ;
      err = pthread_attr_init(&thread_attr) ;
      if (err) {
         LOG_SYSERR("pthread_attr_init",err) ;
         goto UNDO_LOOP ;
      }
      isThreadAttrValid = true ;

      ONERROR_testerrortimer(&s_error_newgroup, UNDO_LOOP) ;
      err = pthread_attr_setstack(&thread_attr, threadstack.addr, threadstack.size) ;
      if (err) {
         LOG_SYSERR("pthread_attr_setstack",err) ;
         LOG_PTR(threadstack.addr) ;
         LOG_SIZE(threadstack.size) ;
         goto UNDO_LOOP ;
      }

      ONERROR_testerrortimer(&s_error_newgroup, UNDO_LOOP) ;
      static_assert( (void* (*) (typeof(startarg)))0 == (typeof(&startpoint_osthread))0, "startpoint_osthread has argument of type startarg") ;
      err = pthread_create( &sys_thread, &thread_attr, (void*(*)(void*))&startpoint_osthread, startarg) ;
      if (err) {
         sys_thread = sys_thread_INIT_FREEABLE ;
         LOG_SYSERR("pthread_create",err) ;
         goto UNDO_LOOP ;
      }

      ONERROR_testerrortimer(&s_error_newgroup, UNDO_LOOP) ;
      err = pthread_attr_destroy(&thread_attr) ;
      isThreadAttrValid = false ;
      if (err) {
         LOG_SYSERR("pthread_attr_destroy",err) ;
         goto UNDO_LOOP ;
      }

      // init osthread_t fields
      err = init_mutex(&next_osthread->lock) ;
      if (err) {
         LOG_CALLERR("init_mutex",err) ;
         goto UNDO_LOOP ;
      }
      next_osthread->wlistnext   = 0 ;
      next_osthread->task.fct    = thread_main ;
      next_osthread->task.arg    = start_arg ;
      next_osthread->sys_thread  = sys_thread ;
      next_osthread->returncode  = 0 ;
      next_osthread->stackframe  = stackframe ;
      next_osthread->nr_threads  = nr_of_threads ;
      next_osthread->groupnext   = osthread ;
      prev_osthread->groupnext   = next_osthread ;

      signalstack.addr += framesize ;
      threadstack.addr += framesize ;
      prev_osthread     = next_osthread ;
      next_osthread     = (osthread_t*) (((uint8_t*)next_osthread) + framesize) ;
      continue ;
   UNDO_LOOP:
      if (sys_thread_INIT_FREEABLE == sys_thread) {
         assert(0 == free_umgebung(&startarg->umg)) ;
      }
      next_osthread->lock        = (mutex_t) mutex_INIT_DEFAULT ;
      next_osthread->wlistnext   = 0 ;
      next_osthread->sys_thread  = sys_thread ;
      next_osthread->stackframe  = stackframe ;
      next_osthread->groupnext   = osthread ;
      prev_osthread->groupnext   = next_osthread ;
      for(; i < nr_of_threads; --i) {
         startarg = (osthread_startargument_t*) signalstack.addr ;
         startarg->isAbort = true ;
         signalstack.addr -= framesize ;
         threadstack.addr -= framesize ;
      }
      break ;
   }

   err2 = signal_semaphore(&isvalid_abortflag, nr_of_threads) ;
   if (err2) {
      LOG_CALLERR("signal_semaphore", err2) ;
      goto ABBRUCH ;
   }

   if (err) {
      err2 = join_osthread(osthread) ;
      if (err2) {
         LOG_CALLERR("join_osthread", err2) ;
      }
      goto ABBRUCH ;
   }

   // semaphore are freed in first created thread !
   assert(0 == free_umgebung(&shared_umgebung)) ;

   *threadobj = osthread ;
   return 0 ;
ABBRUCH:
   if (err2) {
      abort_umgebung(err2) ;
   }
   if (isThreadAttrValid) {
      (void) pthread_attr_destroy(&thread_attr) ;
   }
   (void) free_semaphore(&isvalid_abortflag) ;
   (void) free_semaphore(&isfreeable_semaphore) ;
   (void) delete_osthread(&osthread) ;
   (void) free_umgebung(&shared_umgebung) ;

   LOG_ABORT(err) ;
   return err ;
}

static int joinsingle_osthread(osthread_t * threadobj)
{
   int err ;

   if (sys_thread_INIT_FREEABLE != threadobj->sys_thread) {

      err = pthread_join(threadobj->sys_thread, 0) ;
      threadobj->sys_thread = sys_thread_INIT_FREEABLE ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int join_osthread(osthread_t * threadobj)
{
   int err = 0 ;
   int err2 ;
   osthread_t * osthread = threadobj ;

   do {
      err2 = joinsingle_osthread(osthread) ;
      if (err2) err = err2 ;
      osthread = osthread->groupnext ;
   } while( osthread != threadobj ) ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

void suspend_osthread()
{
   int err ;
   sigset_t signalmask ;

   err = sigemptyset(&signalmask) ;
   assert(!err) ;

   err = sigaddset(&signalmask, SIGINT) ;
   assert(!err) ;

   do {
      err = sigwaitinfo(&signalmask, 0) ;
   } while(-1 == err && EINTR == errno) ;

   if (-1 == err) {
      err = errno ;
      LOG_SYSERR("sigwaitinfo", err) ;
      abort_umgebung(err) ;
   }
}

void resume_osthread(osthread_t * threadobj)
{
   int err ;

   err = pthread_kill(threadobj->sys_thread, SIGINT) ;
   if (err) {
      LOG_SYSERR("pthread_kill", err) ;
      abort_umgebung(err) ;
   }
}

void sleepms_osthread(uint32_t msec)
{
   struct timespec reqtime = { .tv_sec = (int32_t) (msec / 1000), .tv_nsec = (int32_t) ((msec%1000) * 1000000) } ;

   int err ;
   err = nanosleep(&reqtime, 0) ;

   if (-1 == err) {
      err = errno ;
      if (err != EINTR) {
         LOG_SYSERR("nanosleep", err) ;
         goto ABBRUCH ;
      }
   }

   return ;
ABBRUCH:
   LOG_ABORT(err) ;
   return ;
}


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_thread,ABBRUCH)

#define newgroup_osthread(threadobj, thread_main, start_arg, nr_of_threads) \
   /*do not forget to adapt definition in thead.c test section*/                    \
   ( __extension__ ({ int _err ;                                                    \
      int (*_thread_main) (typeof(start_arg)) = (thread_main) ;                     \
      static_assert(sizeof(start_arg) <= sizeof(void*), "cast 2 void*") ;           \
      _err = newgroup_osthread(threadobj, (task_callback_f) _thread_main,           \
                              (struct callback_param_t*) start_arg, nr_of_threads) ;\
      _err ; }))

static uint8_t  * s_sigaddr ;
static pthread_t  s_threadid ;

static void sigusr1handler(int sig)
{
   int errno_backup = errno ;
   assert(sig == SIGUSR1) ;
   s_threadid = pthread_self() ;
   s_sigaddr  = (uint8_t*) &sig ;
   errno = errno_backup ;
}

static int thread_sigaltstack(int dummy)
{
   (void) dummy ;
   memset(&s_threadid, 0, sizeof(s_threadid)) ;
   s_sigaddr = 0 ;
   osthread_stack_t signalstack = getsignalstack_osthreadstack(&self_osthread()->stackframe) ;
   TEST( ! pthread_equal(s_threadid, pthread_self()) ) ;
   TEST( ! (signalstack.addr < s_sigaddr && s_sigaddr < signalstack.addr+signalstack.size) ) ;
   TEST(0 == pthread_kill(pthread_self(), SIGUSR1)) ;
   TEST( pthread_equal(s_threadid, pthread_self()) ) ;
   TEST( (signalstack.addr < s_sigaddr && s_sigaddr < signalstack.addr+signalstack.size) ) ;
   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_thread_sigaltstack(void)
{
   int err = 1 ;
   osthread_t     * osthread = 0 ;
   uint8_t    * s_alt_stack1 = malloc(SIGSTKSZ) ;
   stack_t             newst = {
                        .ss_sp = s_alt_stack1,
                        .ss_size = SIGSTKSZ,
                        .ss_flags = 0
         } ;
   stack_t            oldst ;
   sigset_t           oldprocmask ;
   struct sigaction newact, oldact ;
   bool    isStack = false ;
   bool isProcmask = false ;
   bool   isAction = false ;

   if (!s_alt_stack1) {
      LOG_OUTOFMEMORY((2*SIGSTKSZ)) ;
      goto ABBRUCH ;
   }

   // test that thread 'thread_sigaltstack' runs under its own sigaltstack
   {
      sigemptyset(&newact.sa_mask) ;
      sigaddset(&newact.sa_mask,SIGUSR1) ;
      TEST(0==sigprocmask(SIG_UNBLOCK,&newact.sa_mask,&oldprocmask)) ;  isProcmask = true ;
      sigemptyset(&newact.sa_mask) ;
      newact.sa_flags = SA_ONSTACK ;
      newact.sa_handler = &sigusr1handler ;
      TEST(0 == sigaction(SIGUSR1, &newact, &oldact)) ; isAction = true ;
      TEST(0 == sigaltstack(&newst, &oldst)) ; isStack = true ;
      TEST(0 == new_osthread(&osthread, thread_sigaltstack, 0)) ;
      TEST(osthread) ;
      TEST((task_callback_f)&thread_sigaltstack == osthread->task.fct) ;
      TEST(0 == osthread->task.arg) ;
      TEST(sys_thread_INIT_FREEABLE != osthread->sys_thread) ;
      TEST(0 == join_osthread(osthread)) ;
      TEST(sys_thread_INIT_FREEABLE == osthread->sys_thread) ;
      TEST(0 == osthread->returncode) ;
      // signal own thread
      memset(&s_threadid, 0, sizeof(s_threadid)) ;
      s_sigaddr = 0 ;
      TEST( ! pthread_equal(s_threadid, pthread_self())) ;
      TEST( ! (s_alt_stack1 < s_sigaddr && s_sigaddr < s_alt_stack1+SIGSTKSZ) ) ;
      TEST(0 == pthread_kill(pthread_self(), SIGUSR1)) ;
      TEST( pthread_equal(s_threadid, pthread_self())) ;
      TEST( (s_alt_stack1 < s_sigaddr && s_sigaddr < s_alt_stack1+SIGSTKSZ) ) ;
   }

   err = 0 ;
ABBRUCH:
   delete_osthread(&osthread) ;
   if (isStack)      sigaltstack(&oldst, 0) ;
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isAction)     sigaction(SIGUSR1, &oldact, 0) ;
   free(s_alt_stack1) ;
   return err ;
}

static int        s_isStackoverflow = 0 ;
static ucontext_t s_thread_usercontext ;

static void sigstackoverflow(int sig)
{
   int errno_backup = errno ;
   assert(sig == SIGSEGV) ;
   s_isStackoverflow = 1 ;
   setcontext(&s_thread_usercontext) ;
   errno = errno_backup ;
}

static int thread_stackoverflow(void * argument)
{
   static int count = 0 ;
   if (argument) {
      assert(count == 0) ;
      s_isStackoverflow  = 0 ;
      TEST(0 == getcontext(&s_thread_usercontext)) ;
   } else {
      assert(count > 0) ;
   }
   ++ count ;
   if (!s_isStackoverflow) (void) thread_stackoverflow(0) ;
   count = 0 ;
   return 0 ; // OK
ABBRUCH:
   return EINVAL ;
}

static int test_thread_stackoverflow(void)
{
   sigset_t oldprocmask ;
   struct sigaction newact, oldact ;
   osthread_t * osthread = 0 ;
   bool       isProcmask = false ;
   bool         isAction = false ;

   // test that thread 'thread_stackoverflow' recovers from stack overflow
   // and that a stack overflow is detected with signal SIGSEGV
   sigemptyset(&newact.sa_mask) ;
   sigaddset(&newact.sa_mask,SIGSEGV) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK,&newact.sa_mask,&oldprocmask)) ;
   isProcmask = true ;
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags = SA_ONSTACK ;
   newact.sa_handler = &sigstackoverflow ;
   TEST(0 == sigaction(SIGSEGV, &newact, &oldact)) ;
   isAction = true ;
   s_isStackoverflow = 0 ;
   TEST(0 == new_osthread(&osthread, &thread_stackoverflow, (void*)1)) ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(1 == s_isStackoverflow) ;
   TEST(osthread->task.arg   == (void*)1) ;
   TEST(osthread->task.fct   == (task_callback_f)&thread_stackoverflow) ;
   TEST(osthread->returncode == 0) ;
   TEST(osthread->sys_thread == sys_thread_INIT_FREEABLE) ;
   TEST(0 == delete_osthread(&osthread)) ;

   // signal own thread
   s_isStackoverflow = 0 ;
   TEST(0 == getcontext(&s_thread_usercontext)) ;
   if (!s_isStackoverflow) {
      TEST(0 == pthread_kill(pthread_self(), SIGSEGV)) ;
   }
   TEST(s_isStackoverflow) ;

   TEST(0 == sigprocmask(SIG_SETMASK, &oldprocmask, 0)) ;
   TEST(0 == sigaction(SIGSEGV, &oldact, 0)) ;

   return 0 ;
ABBRUCH:
   delete_osthread(&osthread) ;
   if (isProcmask)   sigprocmask(SIG_SETMASK, &oldprocmask, 0) ;
   if (isAction)     sigaction(SIGSEGV, &oldact, 0) ;
   return EINVAL ;
}

static volatile int s_returncode_signal  = 0 ;
static volatile int s_returncode_running = 0 ;

static int thread_returncode(int retcode)
{
   s_returncode_running = 1 ;
   while( !s_returncode_signal ) {
      pthread_yield() ;
   }
   return (int) retcode ;
}

static int test_thread_init(void)
{
   osthread_t  * osthread = 0 ;

   // TEST initonce => self_osthread()
   TEST(&gt_self_osthread == self_osthread()) ;
   TEST(self_osthread()->wlistnext  == 0) ;
   TEST(self_osthread()->task.fct   == 0) ;
   TEST(self_osthread()->task.arg   == 0) ;
   TEST(self_osthread()->sys_thread == pthread_self()) ;
   TEST(self_osthread()->returncode == 0) ;
   TEST(self_osthread()->stackframe.addr == 0) ;
   TEST(self_osthread()->stackframe.size == 0) ;
   TEST(self_osthread()->nr_threads == 1) ;
   TEST(self_osthread()->groupnext  == self_osthread()) ;

   // TEST init, double free
   s_returncode_signal = 0 ;
   TEST(0 == new_osthread(&osthread, thread_returncode, 0)) ;
   TEST(osthread) ;
   TEST(osthread->wlistnext  == 0) ;
   TEST(osthread->task.fct   == (task_callback_f)&thread_returncode) ;
   TEST(osthread->task.arg   == 0) ;
   TEST(osthread->sys_thread != sys_thread_INIT_FREEABLE) ;
   TEST(osthread->returncode == 0) ;
   TEST(osthread->stackframe.addr != 0) ;
   TEST(osthread->stackframe.size == pagesize_vm() + framestacksize_osthreadstack()) ;
   TEST(osthread->nr_threads == 1) ;
   TEST(osthread->groupnext  == osthread) ;
   s_returncode_signal = 1 ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->wlistnext  == 0) ;
   TEST(osthread->task.fct   == (task_callback_f)&thread_returncode) ;
   TEST(osthread->task.arg   == 0) ;
   TEST(osthread->sys_thread == sys_thread_INIT_FREEABLE) ;
   TEST(osthread->returncode == 0) ;
   TEST(osthread->stackframe.addr != 0) ;
   TEST(osthread->stackframe.size == pagesize_vm() + framestacksize_osthreadstack()) ;
   TEST(osthread->nr_threads == 1) ;
   TEST(osthread->groupnext  == osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(!osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(!osthread) ;

   // TEST double join
   s_returncode_signal = 0 ;
   TEST(0 == new_osthread(&osthread, thread_returncode, 11)) ;
   TEST(osthread) ;
   TEST(osthread->wlistnext  == 0) ;
   TEST(osthread->task.fct   == (task_callback_f)&thread_returncode) ;
   TEST(osthread->task.arg   == (void*)11) ;
   TEST(osthread->sys_thread != sys_thread_INIT_FREEABLE) ;
   TEST(osthread->returncode == 0) ;
   TEST(osthread->stackframe.addr != 0) ;
   TEST(osthread->stackframe.size == pagesize_vm() + framestacksize_osthreadstack()) ;
   TEST(osthread->nr_threads == 1) ;
   TEST(osthread->groupnext  == osthread) ;
   s_returncode_signal = 1 ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->wlistnext  == 0) ;
   TEST(osthread->task.fct   == (task_callback_f)&thread_returncode) ;
   TEST(osthread->task.arg   == (void*)11) ;
   TEST(osthread->sys_thread == sys_thread_INIT_FREEABLE) ;
   TEST(returncode_osthread(osthread) == 11) ;
   TEST(osthread->stackframe.addr != 0) ;
   TEST(osthread->stackframe.size == pagesize_vm() + framestacksize_osthreadstack()) ;
   TEST(osthread->nr_threads == 1) ;
   TEST(osthread->groupnext  == osthread) ;
   TEST(0 == join_osthread(osthread)) ;
   TEST(osthread->wlistnext  == 0) ;
   TEST(osthread->task.fct   == (task_callback_f)&thread_returncode) ;
   TEST(osthread->task.arg   == (void*)11) ;
   TEST(osthread->sys_thread == sys_thread_INIT_FREEABLE) ;
   TEST(returncode_osthread(osthread) == 11) ;
   TEST(osthread->stackframe.addr != 0) ;
   TEST(osthread->stackframe.size == pagesize_vm() + framestacksize_osthreadstack()) ;
   TEST(osthread->nr_threads == 1) ;
   TEST(osthread->groupnext  == osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(!osthread) ;

   // TEST free does also join
   s_returncode_signal  = 1 ;
   s_returncode_running = 0 ;
   TEST(0 == new_osthread(&osthread, thread_returncode, 0)) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(1 == s_returncode_running) ; // => delete has waited until thread has run

   // TEST returncode in case of unhandled signal
   // could not be tested cause unhandled signals terminate whole process

   // TEST returncode
   for(int i = -5; i < 5; ++i) {
      const int arg = 1111 * i ;
      s_returncode_signal  = 0 ;
      s_returncode_running = 0 ;
      TEST(0 == new_osthread(&osthread, thread_returncode, arg)) ;
      TEST(osthread) ;
      TEST(osthread->task.arg   == (void*)arg) ;
      TEST(osthread->task.fct   == (task_callback_f)&thread_returncode ) ;
      TEST(osthread->sys_thread != sys_thread_INIT_FREEABLE) ;
      for(int yi = 0; yi < 100000 && !s_returncode_running; ++yi) {
         pthread_yield() ;
      }
      TEST(s_returncode_running) ;
      TEST(osthread->sys_thread != sys_thread_INIT_FREEABLE) ;
      TEST(!osthread->returncode) ;
      s_returncode_signal = 1 ;
      TEST(0 == join_osthread(osthread)) ;
      TEST(osthread->sys_thread          == sys_thread_INIT_FREEABLE) ;
      TEST(returncode_osthread(osthread) == arg) ;
      TEST(0 == delete_osthread(&osthread)) ;
      TEST(!osthread) ;
   }

   // TEST EDEADLK
   {
      osthread_t self_thread = { .groupnext = &self_thread, .nr_threads = 1, .sys_thread = pthread_self() } ;
      TEST(sys_thread_INIT_FREEABLE != self_thread.sys_thread) ;
      TEST(EDEADLK == join_osthread(&self_thread)) ;
   }


   // TEST ESRCH
   {
      osthread_t copied_thread ;
      s_returncode_signal = 0 ;
      TEST(0 == new_osthread(&osthread, thread_returncode, 0)) ;
      TEST(osthread) ;
      copied_thread           = *osthread ;
      copied_thread.groupnext = &copied_thread ;
      TEST(sys_thread_INIT_FREEABLE != osthread->sys_thread) ;
      s_returncode_signal = 1 ;
      TEST(0 == join_osthread(osthread)) ;
      TEST(sys_thread_INIT_FREEABLE == osthread->sys_thread) ;
      TEST(ESRCH == join_osthread(&copied_thread)) ;
      TEST(0 == delete_osthread(&osthread)) ;
   }

   return 0 ;
ABBRUCH:
   delete_osthread(&osthread) ;
   return EINVAL ;
}


static __thread int st_int = 123 ;
static __thread int (*st_func)(void) = &test_thread_init ;
static __thread struct test_123 {
   int i ;
   double d ;
} st_struct = { 1, 2 } ;

static int thread_returnvar1(int start_arg)
{
   assert(0 == start_arg) ;
   int err = (st_int != 123) ;
   st_int = 0 ;
   return err ;
}

static int thread_returnvar2(int start_arg)
{
   assert(0 == start_arg) ;
   int err = (st_func != &test_thread_init) ;
   st_func = 0 ;
   return err ;
}

static int thread_returnvar3(int start_arg)
{
   assert(0 == start_arg) ;
   int err = (st_struct.i != 1) || (st_struct.d != 2) ;
   st_struct.i = 0 ;
   st_struct.d = 0 ;
   return err ;
}

static int test_thread_localstorage(void)
{
   osthread_t       * thread1 = 0 ;
   osthread_t       * thread2 = 0 ;
   osthread_t       * thread3 = 0 ;

   // TEST TLS variables are correct initialized before thread is created
   TEST(st_int  == 123) ;
   TEST(st_func == &test_thread_init) ;
   TEST(st_struct.i == 1 && st_struct.d == 2) ;
   TEST(0 == new_osthread(&thread1, thread_returnvar1, 0)) ;
   TEST(0 == new_osthread(&thread2, thread_returnvar2, 0)) ;
   TEST(0 == new_osthread(&thread3, thread_returnvar3, 0)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == join_osthread(thread3)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread3)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(0 == delete_osthread(&thread3)) ;
   TEST(st_int  == 123) ;  // TEST TLS variables in main thread have not changed
   TEST(st_func == &test_thread_init) ;
   TEST(st_struct.i == 1 && st_struct.d == 2) ;

   // TEST TLS variables are always initialized with static initializers !
   st_int  = 124 ;
   st_func = &test_thread_sigaltstack ;
   st_struct.i = 2 ;
   st_struct.d = 4 ;
   TEST(0 == new_osthread(&thread1, thread_returnvar1, 0)) ;
   TEST(0 == new_osthread(&thread2, thread_returnvar2, 0)) ;
   TEST(0 == new_osthread(&thread3, thread_returnvar3, 0)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == join_osthread(thread3)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread3)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;
   TEST(0 == delete_osthread(&thread3)) ;
   TEST(st_int  == 124) ;  // TEST TLS variables in main thread have not changed
   TEST(st_func == &test_thread_sigaltstack) ;
   TEST(st_struct.i == 2 && st_struct.d == 4) ;
   st_int  = 123 ;
   st_func = &test_thread_init ;
   st_struct.i = 1 ;
   st_struct.d = 2 ;

   return 0 ;
ABBRUCH:
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   delete_osthread(&thread3) ;
   return EINVAL ;
}

static int test_thread_stack(void)
{
   osthread_stack_t  stack = memoryblock_aspect_INIT_FREEABLE ;

   // TEST query signalstacksize
   TEST(MINSIGSTKSZ == signalstacksize_osthreadstack()) ;

   // TEST query stacksize
   TEST(PTHREAD_STACK_MIN == threadstacksize_osthreadstack()) ;

   // TEST query nextstackoffset_osthreadstack
   {
      size_t nr_pages1 = (signalstacksize_osthreadstack() + pagesize_vm() -1) / pagesize_vm() ;
      size_t nr_pages2 = (threadstacksize_osthreadstack() + pagesize_vm() -1) / pagesize_vm() ;
      TEST(framestacksize_osthreadstack() == pagesize_vm() * (2 + nr_pages1 + nr_pages2)) ;
   }

   // TEST init, double free
   TEST(0 == stack.addr) ;
   TEST(0 == stack.size) ;
   TEST(0 == init_osthreadstack(&stack, 1)) ;
   TEST(0 != stack.addr) ;
   TEST(0 != stack.size) ;
   TEST(0 == free_osthreadstack(&stack)) ;
   TEST(0 == stack.addr) ;
   TEST(0 == stack.size) ;
   TEST(0 == free_osthreadstack(&stack)) ;
   TEST(0 == stack.addr) ;
   TEST(0 == stack.size) ;

   for(uint32_t i = 1; i < 64; ++i) {
      size_t nr_pages1 = (signalstacksize_osthreadstack() + pagesize_vm() -1) / pagesize_vm() ;
      size_t nr_pages2 = (threadstacksize_osthreadstack() + pagesize_vm() -1) / pagesize_vm() ;
      // TEST init array
      TEST(0 == init_osthreadstack(&stack, i)) ;
      TEST(stack.addr != 0) ;
      TEST(stack.size == pagesize_vm() + i * framestacksize_osthreadstack()) ;
      // TEST getsignalstack
      TEST(getsignalstack_osthreadstack(&stack).addr == &stack.addr[pagesize_vm()]) ;
      TEST(getsignalstack_osthreadstack(&stack).size == pagesize_vm() * nr_pages1) ;
      // TEST getthreadstack
      TEST(getthreadstack_osthreadstack(&stack).addr == &stack.addr[pagesize_vm() * (2+nr_pages1)]) ;
      TEST(getthreadstack_osthreadstack(&stack).size == pagesize_vm() * nr_pages2) ;
      // Test stack protection
      for(uint32_t o = 0; o < i; ++o) {
         size_t  offset = o * framestacksize_osthreadstack() ;
         uint8_t * addr = &stack.addr[offset] ;
         addr[pagesize_vm()] = 0 ;
         addr[pagesize_vm() * (1+nr_pages1)-1] = 0 ;
         addr[pagesize_vm() * (2+nr_pages1)] = 0 ;
         addr[pagesize_vm() * (2+nr_pages1+nr_pages2)-1] = 0 ;
      }
      TEST(0 == free_osthreadstack(&stack)) ;
      TEST(0 == stack.addr) ;
      TEST(0 == stack.size) ;
   }

   // EINVAL
   TEST(EINVAL == init_osthreadstack(&stack, 0)) ;

   // ENOMEM (variable overflow)
   if (sizeof(size_t) <= sizeof(uint32_t)) {
      TEST(ENOMEM == init_osthreadstack(&stack, 0x0FFFFFFF)) ;
   }

   return 0 ;
ABBRUCH:
   free_osthreadstack(&stack) ;
   return EINVAL ;
}

typedef struct thread_isvalidstack_t   thread_isvalidstack_t ;
struct thread_isvalidstack_t
{
   bool  isSelfValid[30] ;
   bool  isSignalstackValid[30] ;
   bool  isThreadstackValid[30] ;
   osthread_t      * osthread[30] ;
   osthread_stack_t  signalstack[30] ;
   osthread_stack_t  threadstack[30] ;
   mutex_t           lock ;
} ;

static int thread_isvalidstack(thread_isvalidstack_t * startarg)
{
   int err ;
   stack_t  current_sigaltstack ;

   if (  0 != sigaltstack(0, &current_sigaltstack)
      || 0 != current_sigaltstack.ss_flags) {
      goto ABBRUCH ;
   }

   err = lock_mutex(&startarg->lock) ;
   if (err) goto ABBRUCH ;
   err = unlock_mutex(&startarg->lock) ;
   if (err) goto ABBRUCH ;

   for(unsigned i = 0; i < nrelementsof(startarg->isSelfValid); ++i) {
      if (  (void*)startarg->osthread[i] == self_osthread()) {
         startarg->isSelfValid[i] = true ;
         break ;
      }
   }

   for(unsigned i = 0; i < nrelementsof(startarg->isSignalstackValid); ++i) {
      if (  (void*)startarg->signalstack[i].addr == current_sigaltstack.ss_sp
         &&        startarg->signalstack[i].size == current_sigaltstack.ss_size ) {
         startarg->isSignalstackValid[i] = true ;
         break ;
      }
   }

   for(unsigned i = 0; i < nrelementsof(startarg->isSignalstackValid); ++i) {
      if (  startarg->threadstack[i].addr < (uint8_t*)&startarg
         && (uint8_t*)&startarg < startarg->threadstack[i].addr + startarg->threadstack[i].size ) {
         startarg->isThreadstackValid[i] = true ;
         break ;
      }
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_thread_array(void)
{
   osthread_t            * osthread = 0 ;
   osthread_t            * prev ;
   osthread_t            * next ;
   thread_isvalidstack_t   startarg = { .lock = mutex_INIT_DEFAULT } ;

   // TEST init, double free
   s_returncode_signal = 0 ;
   TEST(0 == newgroup_osthread(&osthread, thread_returncode, 0, 23)) ;
   TEST(osthread) ;
   prev = 0 ;
   next = osthread ;
   for(unsigned i = 0; i < osthread->nr_threads; ++i) {
      TEST(prev < next) ;
      TEST(next->wlistnext  == 0) ;
      TEST(next->task.arg   == 0) ;
      TEST(next->task.fct   == (task_callback_f)&thread_returncode) ;
      TEST(next->returncode == 0) ;
      TEST(next->nr_threads == 23) ;
      TEST(next->sys_thread != sys_thread_INIT_FREEABLE) ;
      prev = next ;
      next = next->groupnext ;
   }
   TEST(next == osthread) ;
   s_returncode_signal = 1 ;
   TEST(0 == join_osthread(osthread)) ;
   prev = 0 ;
   next = osthread ;
   for(unsigned i = 0; i < osthread->nr_threads; ++i) {
      TEST(prev < next) ;
      TEST(next->wlistnext  == 0) ;
      TEST(next->task.arg   == 0) ;
      TEST(next->task.fct   == (task_callback_f)&thread_returncode) ;
      TEST(next->returncode == 0) ;
      TEST(next->nr_threads == 23) ;
      TEST(next->sys_thread == sys_thread_INIT_FREEABLE) ;
      prev = next ;
      next = next->groupnext ;
   }
   TEST(next == osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(0 == osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;
   TEST(0 == osthread) ;

   // Test return values (== 0)
   s_returncode_signal = 1 ;
   TEST(0 == newgroup_osthread(&osthread, thread_returncode, 0, 53)) ;
   TEST(0 == join_osthread(osthread)) ;
   prev = 0 ;
   next = osthread ;
   for(unsigned i = 0; i < osthread->nr_threads; ++i) {
      TEST(prev < next) ;
      TEST(next->returncode == 0) ;
      TEST(next->nr_threads == 53) ;
      TEST(next->sys_thread == sys_thread_INIT_FREEABLE) ;
      prev = next ;
      next = next->groupnext ;
   }
   TEST(next == osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;

   // Test return values (!= 0)
   s_returncode_signal = 1 ;
   TEST(0 == newgroup_osthread(&osthread, thread_returncode, 0x0FABC, 87)) ;
   TEST(0 == join_osthread(osthread)) ;
   prev = 0 ;
   next = osthread ;
   for(unsigned i = 0; i < osthread->nr_threads; ++i) {
      TEST(prev < next) ;
      TEST(next->returncode == 0x0FABC) ;
      TEST(next->nr_threads == 87) ;
      TEST(next->sys_thread == sys_thread_INIT_FREEABLE) ;
      prev = next ;
      next = next->groupnext ;
   }
   TEST(next == osthread) ;
   TEST(0 == delete_osthread(&osthread)) ;

   // Test every thread has its own stackframe + self_osthread
   TEST(0 == lock_mutex(&startarg.lock)) ;
   TEST(0 == newgroup_osthread(&osthread, thread_isvalidstack, &startarg, nrelementsof(startarg.isSignalstackValid))) ;

   osthread_stack_t signalstack = getsignalstack_osthreadstack(&osthread->stackframe) ;
   osthread_stack_t threadstack = getthreadstack_osthreadstack(&osthread->stackframe) ;
   size_t           framesize   = framestacksize_osthreadstack() ;
   prev = 0 ;
   next = osthread ;
   for(unsigned i = 0; i < nrelementsof(startarg.isSignalstackValid); ++i) {
      TEST(prev < next) ;
      startarg.isSelfValid[i]        = false ;
      startarg.isSignalstackValid[i] = false ;
      startarg.isThreadstackValid[i] = false ;
      startarg.osthread[i]    = next ;
      startarg.signalstack[i] = signalstack ;
      startarg.threadstack[i] = threadstack ;
      signalstack.addr += framesize ;
      threadstack.addr += framesize ;
      prev = next ;
      next = next->groupnext ;
   }
   TEST(next == osthread) ;

   TEST(0 == unlock_mutex(&startarg.lock)) ;
   TEST(0 == delete_osthread(&osthread)) ;

   for(unsigned i = 0; i < nrelementsof(startarg.isSignalstackValid); ++i) {
      TEST(startarg.isSelfValid[i]) ;
      TEST(startarg.isSignalstackValid[i]) ;
      TEST(startarg.isThreadstackValid[i]) ;
   }

   // Test error in newmany => executing UNDO_LOOP
   for(int i = 1; i < 27; i += 1) {
      TEST(0 == init_testerrortimer(&s_error_newgroup, (unsigned)i, 99 + i)) ;
      s_returncode_signal = 1 ;
      TEST((99 + i) == newgroup_osthread(&osthread, thread_returncode, 0, 33)) ;
   }

   TEST(0 == free_mutex(&startarg.lock)) ;

   return 0 ;
ABBRUCH:
   (void) unlock_mutex(&startarg.lock) ;
   (void) free_mutex(&startarg.lock) ;
   delete_osthread(&osthread) ;
   return EINVAL ;
}

static int wait_for_signal(int signr)
{
   int err ;
   sigset_t signalmask ;

   err = sigemptyset(&signalmask) ;
   if (err) return EINVAL ;

   err = sigaddset(&signalmask, signr) ;
   if (err) return EINVAL ;

   do {
      err = sigwaitinfo(&signalmask, 0) ;
   } while(-1 == err && EINTR == errno) ;

   return err == signr ? 0 : EINVAL ;
}

static int thread_sendsignal1(osthread_t * receiver)
{
   int err ;
   err = pthread_kill(receiver->sys_thread, SIGUSR1) ;
   assert(0 == err) ;
   return err ;
}

static int thread_sendsignal2(int dummy)
{
   int err ;
   (void) dummy ;
   err = kill(getpid(), SIGUSR1) ;
   assert(0 == err) ;
   return err ;
}

static int thread_receivesignal(int dummy)
{
   int err ;
   (void) dummy ;
   err = wait_for_signal(SIGUSR1) ;
   return err ;
}

static int thread_receivesignal2(int dummy)
{
   int err ;
   (void) dummy ;
   err = wait_for_signal(SIGUSR2) ;
   return err ;
}


static int thread_sendreceivesignal2(int dummy)
{
   int err ;
   (void) dummy ;
   err = kill(getpid(), SIGUSR1) ;
   assert(0 == err) ;
   err = wait_for_signal(SIGRTMIN) ;
   return err ;
}

static int test_thread_signal(void)
{
   struct timespec   ts              = { 0, 0 } ;
   bool              isoldsignalmask = false ;
   sigset_t          oldsignalmask ;
   sigset_t          signalmask ;
   osthread_t      * thread1         = 0 ;
   osthread_t      * thread2         = 0 ;

   TEST(0 == sigemptyset(&signalmask)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR2)) ;
   TEST(0 == sigaddset(&signalmask, SIGRTMIN)) ;
   TEST(0 == sigprocmask(SIG_BLOCK, &signalmask, &oldsignalmask)) ;
   isoldsignalmask = true ;

   // TEST: main thread receives from 1st thread
   TEST(0 == new_osthread(&thread1, thread_sendsignal1, self_osthread())) ;
   TEST(0 == wait_for_signal(SIGUSR1)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == delete_osthread(&thread1)) ;

   // TEST: 2nd thread receives from 1st thread
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   TEST(0 == new_osthread(&thread2, thread_receivesignal, 0)) ;
   TEST(0 == new_osthread(&thread1, thread_sendsignal1, thread2)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;

   // TEST: main thread can not receive from 1st thread if it sends to 2nd thread
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   TEST(0 == new_osthread(&thread2, thread_receivesignal2, 0)) ;
   TEST(0 == new_osthread(&thread1, thread_sendsignal1, thread2)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(-1 == sigtimedwait(&signalmask, 0, &ts)) ;
   TEST(EAGAIN == errno) ;
   TEST(0 == pthread_kill(thread2->sys_thread, SIGUSR2)) ; // wake up thread2
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;

   // TEST: kill() can be received by main thread !
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   TEST(0 == new_osthread(&thread1, thread_sendsignal2, 0)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == wait_for_signal(SIGUSR1)) ;
   TEST(0 == delete_osthread(&thread1)) ;

   // TEST: kill() can be received by second thread !
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   TEST(0 == new_osthread(&thread1, thread_sendsignal2, 0)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == new_osthread(&thread2, thread_receivesignal, 0)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;

   // ONLY realtime signals do add a new entry into an internal system queue

   // TEST: SIGRTMIN does queue up (threads receive) !
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   TEST(0 == new_osthread(&thread1, thread_sendreceivesignal2, 0)) ;
   TEST(0 == wait_for_signal(SIGUSR1)) ;
   TEST(0 == new_osthread(&thread2, thread_sendreceivesignal2, 0)) ;
   TEST(0 == wait_for_signal(SIGUSR1)) ;
   TEST(0 == kill(getpid(), SIGRTMIN)) ;
   TEST(0 == kill(getpid(), SIGRTMIN)) ;
   TEST(0 == kill(getpid(), SIGRTMIN)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(SIGRTMIN == sigtimedwait(&signalmask, 0, &ts)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;

   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   isoldsignalmask = false ;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0)) ;

   return 0 ;
ABBRUCH:
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   while( 0 < sigtimedwait(&signalmask, 0, &ts) ) ;
   if (isoldsignalmask) sigprocmask(SIG_SETMASK, &oldsignalmask, 0) ;
   return EINVAL ;
}

static int thread_suspend(int signr)
{
   int err ;
   err = send_rtsignal((rtsignal_t)signr) ;
   assert(!err) ;
   suspend_osthread() ;
   err = send_rtsignal((rtsignal_t)(signr+1)) ;
   assert(!err) ;
   return 0 ;
}

static int thread_resume(osthread_t * receiver)
{
   resume_osthread(receiver) ;
   return 0 ;
}

static int thread_suspend2(int signr)
{
   int err ;
   err = wait_rtsignal((rtsignal_t)signr, 1) ;
   assert(!err) ;
   suspend_osthread() ;
   return 0 ;
}

static int test_thread_suspendresume(void)
{
   osthread_t * thread1 = 0 ;
   osthread_t * thread2 = 0 ;

   // TEST: main thread resumes thread_suspend
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == new_osthread(&thread1, thread_suspend, 0)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   resume_osthread(thread1) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == trywait_rtsignal(1)) ;
   TEST(0 == delete_osthread(&thread1)) ;

   // TEST: thread_suspend is resumed by thread_resume
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == new_osthread(&thread1, &thread_suspend, 0)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == new_osthread(&thread2, &thread_resume, thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == trywait_rtsignal(1)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;

   // TEST: main thread resumes thread1, thread2 before they are started
   //       test that resume is preserved !
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(0 == new_osthread(&thread1, thread_suspend2, 0)) ;
   TEST(0 == new_osthread(&thread2, thread_suspend2, 0)) ;
   resume_osthread(thread1) ;
   resume_osthread(thread2) ;
   TEST(0 == send_rtsignal(0)) ; // start threads
   TEST(0 == send_rtsignal(0)) ;
   TEST(0 == join_osthread(thread1)) ;
   TEST(0 == returncode_osthread(thread1)) ;
   TEST(0 == join_osthread(thread2)) ;
   TEST(0 == returncode_osthread(thread2)) ;
   TEST(0 == delete_osthread(&thread1)) ;
   TEST(0 == delete_osthread(&thread2)) ;

   // TEST: main resumes itself
   //       test that resume is preserved even for myself !
   resume_osthread(self_osthread()) ;
   suspend_osthread() ;
   resume_osthread(self_osthread()) ;
   suspend_osthread() ;

   return 0 ;
ABBRUCH:
   delete_osthread(&thread1) ;
   delete_osthread(&thread2) ;
   return EINVAL ;
}

static int thread_lockunlock(osthread_t * mainthread)
{
   int err ;
   err = send_rtsignal(0) ;
   assert(!err) ;
   lock_osthread(mainthread) ;
   mainthread->task.arg = (void*) (1 + (int) mainthread->task.arg) ;
   err = send_rtsignal(1) ;
   assert(!err) ;
   err = wait_rtsignal(2, 1) ;
   assert(!err) ;
   unlock_osthread(mainthread) ;
   err = send_rtsignal(3) ;
   assert(!err) ;
   return 0 ;
}

static int thread_doublelock(int err)
{
   lock_osthread(self_osthread()) ;
   err = lock_mutex(&self_osthread()->lock) ;
   unlock_osthread(self_osthread()) ;
   LOG_CLEARBUFFER() ;
   return err ;
}

static int thread_doubleunlock(int err)
{
   lock_osthread(self_osthread()) ;
   unlock_osthread(self_osthread()) ;
   err = unlock_mutex(&self_osthread()->lock) ;
   LOG_CLEARBUFFER() ;
   return err ;
}

static int test_thread_lockunlock(void)
{
   osthread_t * thread = 0 ;

   // TEST: lock on main thread protects access
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(EAGAIN == trywait_rtsignal(2)) ;
   lock_osthread(self_osthread()) ;
   self_osthread()->task.arg = 0 ;
   TEST(0 == newgroup_osthread(&thread, thread_lockunlock, self_osthread(), 99)) ;
   TEST(0 == wait_rtsignal(0, 99)) ;
   TEST(EAGAIN == trywait_rtsignal(1)) ;
   TEST(EAGAIN == trywait_rtsignal(3)) ;
   unlock_osthread(self_osthread()) ;
   for(int i = 0; i < 99; ++i) {
      TEST(0 == wait_rtsignal(1, 1)) ;
      void * volatile * cmdaddr = (void**) & (self_osthread()->task.arg) ;
      TEST(1+i == (int)(*cmdaddr)) ;
      TEST(EAGAIN == trywait_rtsignal(1)) ;
      TEST(EAGAIN == trywait_rtsignal(3)) ;
      TEST(0 == send_rtsignal(2)) ;
      TEST(0 == wait_rtsignal(3, 1)) ;
      TEST(EAGAIN == trywait_rtsignal(3)) ;
   }
   TEST(0 == join_osthread(thread)) ;
   TEST(0 == delete_osthread(&thread)) ;
   self_osthread()->task.arg = 0 ;

   // TEST EDEADLK: calling lock twice is prevented
   lock_osthread(self_osthread()) ;
   TEST(EDEADLK == lock_mutex(&self_osthread()->lock)) ;
   unlock_osthread(self_osthread()) ;
   TEST(0 == new_osthread(&thread, thread_doublelock, 0)) ;
   TEST(0 == join_osthread(thread)) ;
   TEST(EDEADLK == returncode_osthread(thread)) ;
   TEST(0 == delete_osthread(&thread)) ;

   // TEST EPERM: calling unlock twice is prevented
   lock_osthread(self_osthread()) ;
   unlock_osthread(self_osthread()) ;
   TEST(EPERM == unlock_mutex(&self_osthread()->lock)) ;
   TEST(0 == new_osthread(&thread, thread_doubleunlock, 0)) ;
   TEST(0 == join_osthread(thread)) ;
   TEST(EPERM == returncode_osthread(thread)) ;
   TEST(0 == delete_osthread(&thread)) ;

   return 0 ;
ABBRUCH:
   for(int i = 0; i < 99; ++i) {
      send_rtsignal(2) ;
   }
   delete_osthread(&thread) ;
   while( 0 == trywait_rtsignal(0) ) ;
   while( 0 == trywait_rtsignal(1) ) ;
   while( 0 == trywait_rtsignal(2) ) ;
   while( 0 == trywait_rtsignal(3) ) ;
   self_osthread()->task.arg = 0 ;
   return EINVAL ;
}

int test_thread_sleep(void)
{
   struct timeval tv ;
   struct timeval tv2 ;
   int32_t msec ;

   // TEST 250 msec
   TEST(0 == gettimeofday(&tv, 0)) ;
   sleepms_osthread(250) ;
   TEST(0 == gettimeofday(&tv2, 0)) ;

   msec = 1000 * (tv2.tv_sec - tv.tv_sec) + (tv2.tv_usec - tv.tv_usec) / 1000 ;
   TEST(msec > 200) ;
   TEST(msec < 300) ;

   // TEST 100 msec
   TEST(0 == gettimeofday(&tv, 0)) ;
   sleepms_osthread(100) ;
   TEST(0 == gettimeofday(&tv2, 0)) ;

   msec = 1000 * (tv2.tv_sec - tv.tv_sec) + (tv2.tv_usec - tv.tv_usec) / 1000 ;
   TEST(msec > 80) ;
   TEST(msec < 120) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_os_thread()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_thread_array())            goto ABBRUCH ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_thread_stack())            goto ABBRUCH ;
   if (test_thread_init())             goto ABBRUCH ;
   if (test_thread_sigaltstack())      goto ABBRUCH ;
   if (test_thread_stackoverflow())    goto ABBRUCH ;
   if (test_thread_localstorage())     goto ABBRUCH ;
   if (test_thread_array())            goto ABBRUCH ;
   if (test_thread_signal())           goto ABBRUCH ;
   if (test_thread_suspendresume())    goto ABBRUCH ;
   if (test_thread_lockunlock())       goto ABBRUCH ;
   if (test_thread_sleep())            goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
