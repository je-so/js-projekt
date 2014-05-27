/* title: PlatformInit

   Offers platform specific initialization function <platform_t.init_platform>.

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

   file: C-kern/api/platform/init.h
    Header file <PlatformInit>.

   file: C-kern/platform/Linux/init.c
    Implementation file <PlatformInit Linux>.
*/
#ifndef CKERN_PLATFORM_STARTUP_HEADER
#define CKERN_PLATFORM_STARTUP_HEADER

/* typedef: mainthread_f
 * Signature of main thread is the same as function main. */
typedef int (* mainthread_f) (void * user);


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_init
 * Test <init_platform> functionality. */
int unittest_platform_init(void);
#endif


/* struct: platform_t
 * Dummy type which represents the operating system platform. */
struct platform_t;

// group: lifetime

/* function: init_platform
 * Initialize system context and calls main_thread.
 * If the system context could be initialized and the main_thread was called
 * the return value is the value returned from main_thread. If an error occurs during
 * initialization only an error code (value > 0) is returned, main_thread is not called.
 *
 * You have to call this function before calling <init_maincontext>.
 *
 * This function is implemented in a system specific way.
 * */
int init_platform(mainthread_f main_thread, void * user);


#endif
