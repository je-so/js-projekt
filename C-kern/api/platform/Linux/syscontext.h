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
struct thread_t;
struct threadcontext_t;


// struct: thread_tls_t
struct thread_tls_t;

/* function: sys_context_threadtls
 * Returns <threadcontext_t> of the current thread.
 * This function is called from all other functions using <threadcontext_t>.
 * The returned pointer is calculated relative to the address of the current stack. */
struct threadcontext_t* sys_context_threadtls(void);

/* function: sys_thread_threadtls
 * Returns current <thread_t> object. */
struct thread_t* sys_thread_threadtls(void);

/* function: sys_size_threadtls
 * Returns the size in bytes of the thread local storage.
 * This size is reserved for every created thread and the main thread.
 * The size includes the stack and the signal stack size. */
size_t sys_size_threadtls(void);

/* function: sys_self_threadtls
 * Returns <thread_tls_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack.
 * This function is called from <sys_context_threadtls>. */
struct thread_tls_t* sys_self_threadtls(void);

/* function: sys_self2_threadtls
 * Returns <thread_tls_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack.
 * This function is called from <sys_self_threadtls>. */
struct thread_tls_t* sys_self2_threadtls(void * local_var);


// section: inline implementation

/* define: sys_context_threadtls
 * Implements <thread_tls_t.sys_context_threadtls>. */
#define sys_context_threadtls() \
         ((struct threadcontext_t*) sys_self_threadtls())

/* define: sys_size_threadtls
 * Implements <thread_tls_t.sys_size_threadtls>. */
#define sys_size_threadtls() \
         (512*1024)

/* define: sys_self_threadtls
 * Implements <thread_tls_t.sys_self_threadtls>. */
#define sys_self_threadtls() \
         ( __extension__ ({               \
            int _addr;                    \
            sys_self2_threadtls(&_addr);  \
         }))

/* define: sys_self2_threadtls
 * Implements <thread_tls_t.sys_self2_threadtls>. */
#define sys_self2_threadtls(local_var) \
         ((struct thread_tls_t*) ( (uintptr_t)(local_var) & ~(uintptr_t) (sys_size_threadtls()-1)))

/* define: sys_thread_threadtls
 * Implements <thread_tls_t.sys_thread_threadtls>. */
#define sys_thread_threadtls() \
         ((struct thread_t*) ( ((uint8_t*)sys_self_threadtls()) + sizeof(threadcontext_t)))

#endif
