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

#include "C-kern/api/cache/objectcache_iot.h"
#include "C-kern/api/memory/mm/mm_iot.h"
#include "C-kern/api/io/writer/log/log_iot.h"

/* typedef: struct threadcontext_t
 * Export <threadcontext_t>. */
typedef struct threadcontext_t         threadcontext_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_threadcontext
 * Test interface of <threadcontext_t>. */
int unittest_context_threadcontext(void) ;
#endif


/* struct: threadcontext_t
 * Offers path to services useable exclusively from one thread. */
struct threadcontext_t {
   log_iot                 ilog ;
   mm_iot                  mm_transient ;
   objectcache_iot         objectcache ;
   uint16_t                initcount ;
} ;

// group: lifetime

/* define: threadcontext_INIT_STATIC
 * Static initializer for <threadcontext_t>.
 * These initializer ensures that in function main the global log service is available
 * even without calling <init_context> first.
 */
#define threadcontext_INIT_STATIC      { { &g_logmain, &g_logmain_interface }, mm_iot_INIT_FREEABLE, objectcache_iot_INIT_FREEABLE, 0 }

/* function: init_threadcontext
 * Creates all top level services which are bound to a single thread.
 * Services do *not* need to be multi thread safe.
 * Is called from <init_context>. */
int init_threadcontext(/*out*/threadcontext_t * tcontext) ;

/* function: free_threadcontext
 * Frees all resources bound to this object.
 * Is called from <free_context>. */
int free_threadcontext(threadcontext_t * tcontext) ;


#endif
