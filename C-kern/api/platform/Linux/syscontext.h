/* title: LinuxSystemContext

   Defines <sys_context_threadtls> which calculates a pointer to <threadcontext_t>.
   The address of the pointer is determined relative to the address of the current stack address.

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

   file: C-kern/api/platform/Linux/syscontext.h
    Header file <LinuxSystemContext>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER
#define CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER

// forward
struct threadcontext_t ;


// struct: thread_tls_t
struct thread_tls_t ;

/* function: sys_context_threadtls
 * Returns <threadcontext_t> of the current thread.
 * This function is called from all other functions using <threadcontext_t>.
 * The returned pointer is caluclated relative to the address of the current stack. */
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

#endif
