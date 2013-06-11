/* title: ThreadLocalStorage

   TODO: describe module interface

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/task/thread_tls.h
    Header file <ThreadLocalStorage>.

   file: C-kern/platform/Linux/task/thread_tls.c
    Implementation file <ThreadLocalStorage impl>.
*/
#ifndef CKERN_PLATFORM_TASK_THREAD_TLS_HEADER
#define CKERN_PLATFORM_TASK_THREAD_TLS_HEADER

// forward
struct memblock_t ;
struct threadcontext_t ;

/* typedef: struct thread_tls_t
 * Export <thread_tls_t> into global namespace. */
typedef struct thread_tls_t               thread_tls_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_thread_tls
 * Test <thread_tls_t> functionality. */
int unittest_platform_task_thread_tls(void) ;
#endif


/* struct: thread_tls_t
 * Holds thread local memory.
 * The memory comprises the variables <thread_t> and <threadcontext_t>,
 * the signal stack and thread stack and 3 protection pages in between. */
struct thread_tls_t {
   uint8_t *   addr ;
} ;

// group: lifetime

/* define: thread_tls_INIT_FREEABLE
 * Static initializer. */
#define thread_tls_INIT_FREEABLE          { 0 }

/* function: init_threadtls
 * Allocates a memory block big enoug to hold all thread local storage data.
 * The access rights of parts of the memory block is changed to protect
 * the stack from overflowing.
 * The allocated memory block is aligned to its own size. */
int init_threadtls(/*out*/thread_tls_t * tls) ;

/* function: free_threadtls
 * Changes protection of memory to normal and frees it. */
int free_threadtls(thread_tls_t * tls) ;

// group: query

/* function: context_threadtls
 * Returns <threadcontext_t> of the current thread.
 * This function is identical with <sys_context_thread>.
 * If you change this function change also sys_context_thread. */
struct threadcontext_t * context_threadtls(void) ;

/* function: context2_threadtls
 * Returns <threadcontext_t> from any address on the stack.
 * This function is called from <context_threadtls>.
 * The returned <threadcontext_t> corresponds to the thread on which
 * stack stackaddr is located. */
struct threadcontext_t * context2_threadtls(uintptr_t stackaddr) ;

/* function: size_threadtls
 * Returns the size of the allocated memory block. */
size_t size_threadtls(void) ;

/* function: signalstack_threadtls
 * Returns in stackmem the signalstack from tls.
 * If tls is in a freed state stackmem is set to <memblock_INIT_FREEABLE>.
 * The signal stack is used in case of a signal (exceptions).
 * For example if the thread stack overflows SIGSEGV signal is thrown.
 * To handle this case the system must have an extra signal stack
 * cause signal handling needs stack space. */
void signalstack_threadtls(const thread_tls_t * tls,/*out*/struct memblock_t * stackmem) ;

/* function: threadstack_threadtls
 * Returns in stackmem the thread stack from tls.
 * If tls is in a freed state stackmem is set to <memblock_INIT_FREEABLE>. */
void threadstack_threadtls(const thread_tls_t * tls,/*out*/struct memblock_t * stackmem) ;

// group: generic

/* function: genericcast_threadtls
 * Casts pointer to an object into pointer to <thread_tls_t>.
 * The object must have a single member with name nameprefix##addr
 * of the same type as <thread_tls_t>. */
thread_tls_t * genericcast_threadtls(void * obj, IDNAME nameprefix) ;


// section: inline implementation

/* define: size_threadtls
 * Implements <thread_tls_t.size_threadtls>. */
#define size_threadtls()                  (65536)

/* define: context_threadtls
 * Implements <thread_tls_t.context_threadtls>. */
#define context_threadtls()                  \
         ( __extension__ ({                  \
            int _addr_of_stack ;             \
            context2_threadtls((uintptr_t)   \
                        &_addr_of_stack) ;   \
         }))

/* define: context_threadtls
 * Implements <thread_tls_t.context_threadtls>. */
#define context2_threadtls(stackaddr)        \
         ( __extension__ ({                  \
            (threadcontext_t*) (             \
               (stackaddr)                   \
               & ~(uintptr_t)                \
                  (size_threadtls()-1)) ;    \
         }))

/* define: genericcast_threadtls
 * Implements <thread_tls_t.genericcast_threadtls>. */
#define genericcast_threadtls(obj, nameprefix)           \
         ( __extension__ ({                              \
            typeof(obj) _obj = (obj) ;                   \
            static_assert(                               \
               sizeof(_obj->nameprefix##addr)            \
               == sizeof(((thread_tls_t*)0)->addr)       \
               && (typeof(_obj->nameprefix##addr))10     \
                  == (uint8_t*)10                        \
               && 0 == offsetof(thread_tls_t, addr),     \
               "obj is compatible") ;                    \
            (thread_tls_t *)(&_obj->nameprefix##addr) ;  \
         }))

#endif
