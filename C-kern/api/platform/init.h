/* title: PlatformInit

   Offers platform specific initialization function <syscontext_t.initrun_syscontext>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/init.h
    Header file <PlatformInit>.

   file: C-kern/platform/Linux/init.c
    Implementation file <PlatformInit Linux>.
*/
#ifndef CKERN_PLATFORM_INIT_HEADER
#define CKERN_PLATFORM_INIT_HEADER

// === exported types

/* typedef: thread_f
 * Signature of main thread. */
typedef int (* thread_f) (void * thread_arg);


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_init
 * Test <syscontext_t.initrun_syscontext> functionality. */
int unittest_platform_init(void);
#endif

// TODO: remove init.h/init.c, reimplement initrun_syscontext as initrun_thread in module thread !!

/* function: initrun_syscontext
 * Initialize platform and os specific parts of <maincontext_t> and then calls main_thread.
 * If scontext (see <syscontext_t>) could be initialized then main_thread is called
 * and its return value is returned as return value. If an error occurs during
 * initialization only an error code (> 0) is returned, main_thread is not called.
 * If an error occurs during freeing of resources after main_thread has been run, an error
 * code (> 0) is returned.
 *
 * The local store of the main thread (<thread_localstore_t>) is initialized and the
 * contained thread context (see <threadcontext_t>) is initialized to <threadcontext_t.threadcontext_INIT_STATIC>.
 *
 * This function is called during execution of <maincontext_t.initrun_maincontext>
 * to set up the main thread environment which supports the default logging functions.
 *
 * Before calling this function scontext should be set to <syscontext_FREE> else EALREADY is returned.
 * Before this function returns scontext is reset to <syscontext_FREE> to indicate the end of an
 * initialized system context. During execuion of main_thread scontext is initialized to a valid value.
 *
 * */
int initrun_syscontext(thread_f main_thread, void * main_arg);


#endif
