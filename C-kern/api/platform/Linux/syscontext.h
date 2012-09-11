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

   file: C-kern/api/maincontext.h
    Included from header <MainContext>.
*/
#ifndef CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER
#define CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER

// forward
struct threadcontext_t ;

/* variable: gt_threadcontext
 * Refers for every thread to corresponding <threadcontext_t> object.
 * Is is located on the thread stack so no heap memory is allocated.
 * This variable is defined in <Thread Linux>. */
extern __thread struct threadcontext_t gt_threadcontext ;


// section: thread_t

/* function: sys_context_thread
 * Returns the <threadcontext_t> of the current thread.
 * This function is called from all other <threadcontext_t> returning functions. */
/*ref*/ struct threadcontext_t  sys_context_thread(void) ;


// section: inline implementation

/* define: sys_context_thread
 * Implements <thread_t.sys_context_thread>. */
#define sys_context_thread()           (gt_threadcontext)


#endif
