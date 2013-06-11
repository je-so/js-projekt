/* title: ThreadContext
   Defines the global (thread local) context of a running system thread.
   If more than one thread is running in a process each thread
   has its own context. It contains references to services which
   belong only to one thread and therefore are not thread safe.
   They can not be shared between threads.

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

   file: C-kern/api/context/threadcontext.h
    Header file <ThreadContext>.

   file: C-kern/context/threadcontext.c
    Implementation file <ThreadContext impl>.
*/
#ifndef CKERN_CONTEXT_THREADCONTEXT_HEADER
#define CKERN_CONTEXT_THREADCONTEXT_HEADER

/* typedef: struct threadcontext_t
 * Export <threadcontext_t>. */
typedef struct threadcontext_t            threadcontext_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_threadcontext
 * Test interface of <threadcontext_t>. */
int unittest_context_threadcontext(void) ;
#endif


/* struct: threadcontext_t
 * Stores services useable exclusively from one thread.
 * */
struct threadcontext_t {
   iobj_DECLARE(,pagecache)   pagecache ;
   iobj_DECLARE(,mm)          mm ;
   struct syncrun_t *         syncrun ;
   iobj_DECLARE(,objectcache) objectcache ;
   iobj_DECLARE(,log)         log ;
   size_t                     initcount ;
} ;

// group: lifetime

/* define: threadcontext_INIT_STATIC
 * Static initializer for <threadcontext_t>.
 * These initializer ensures that in function main the global log service is available
 * even without calling <init_maincontext> first.
 */
#define threadcontext_INIT_STATIC   \
         { iobj_INIT_FREEABLE, iobj_INIT_FREEABLE, 0, iobj_INIT_FREEABLE, { (struct log_t*)&g_logmain, &g_logmain_interface }, 0 }

/* function: init_threadcontext
 * Creates all top level services which are bound to a single thread.
 * Services do *not* need to be multi thread safe cause a new one is created for every new thread.
 * If a service shares information between threads then it must be programmed in a thread safe manner.
 * This function is called from <init_maincontext>. */
int init_threadcontext(/*out*/threadcontext_t * tcontext) ;

/* function: free_threadcontext
 * Frees all resources bound to this object.
 * This function is called from <free_maincontext>. */
int free_threadcontext(threadcontext_t * tcontext) ;


#endif
