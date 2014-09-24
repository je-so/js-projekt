/* title: LinuxSystemContext

   Defines <sys_context_threadtls> and other thread context specific functions.
   The defined functions are implemented inline to speed up performance.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/platform/Linux/syscontext.h
    Header file <LinuxSystemContext>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER
#define CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER

// forward
struct thread_t ;
struct threadcontext_t ;


// struct: thread_tls_t
struct thread_tls_t ;

/* function: sys_context_threadtls
 * Returns <threadcontext_t> of the current thread.
 * This function is called from all other functions using <threadcontext_t>.
 * The returned pointer is calculated relative to the address of the current stack. */
struct threadcontext_t * sys_context_threadtls(void) ;

/* function: sys_context2_threadtls
 * Returns <threadcontext_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack.
 * This function is called from <sys_context_threadtls>. */
struct threadcontext_t * sys_context2_threadtls(void * local_var) ;

/* function: sys_size_threadtls
 * Returns the size in bytes of the thread local storage.
 * This size is reserved for every created thread and the main thread.
 * The size includes the stack and the signal stack size. */
size_t sys_size_threadtls(void) ;

/* function: sys_thread_threadtls
 * Returns current <thread_t> object.
 * Calls <sys_context_threadtls> and adds a constant offset. */
struct thread_t * sys_thread_threadtls(void) ;


// section: inline implementation

/* define: sys_context_threadtls
 * Implements <thread_tls_t.sys_context_threadtls>. */
#define sys_context_threadtls()           \
         ( __extension__ ({               \
            int _addr ;                   \
            sys_context2_threadtls(       \
               &_addr) ;                  \
         }))

/* define: sys_context2_threadtls
 * Implements <thread_tls_t.sys_context2_threadtls>. */
#define sys_context2_threadtls(local_var) \
         (threadcontext_t*) (             \
            (uintptr_t)(local_var)        \
            & ~(uintptr_t)                \
               (sys_size_threadtls()-1)   \
         )

/* define: sys_size_threadtls
 * Implements <thread_tls_t.sys_size_threadtls>. */
#define sys_size_threadtls()              (512*1024)

/* define: sys_thread_threadtls
 * Implements <thread_tls_t.sys_thread_threadtls>. */
#define sys_thread_threadtls()            ((thread_t*)(&sys_context_threadtls()[1]))

#endif
