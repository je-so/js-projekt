/* title: LinuxSystemContext
   Defines system specific accessor function for context of the current thread.

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

   file: C-kern/api/context.h
    Included from header <MainContext>.
*/
#ifndef CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER
#define CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER

// forward
struct threadcontext_t ;

/* variable: gt_thread_self
 * Refers for every thread to corresponding <thread_t> object.
 * Is is located on the thread stack so no heap memory is allocated.
 * This variable is defined in <Thread Linux>. */
extern __thread struct threadcontext_t gt_thread_context ;


// section: context_t

/* function: sys_thread_context
 * Returns the <threadcontext_t> of the current thread. */
extern /*ref*/ struct threadcontext_t  sys_thread_context(void) ;


// section: inline implementation

/* define: sys_thread_context
 * Implements <context_t.sys_thread_context>. */
#define sys_thread_context()           (gt_thread_context)


#endif
