/* title: PlatformStartup

   Offers platform specific initialization function <startup_platform>.

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

   file: C-kern/api/platform/startup.h
    Header file <PlatformStartup>.

   file: C-kern/platform/Linux/startup.c
    Implementation file <PlatformStartup Linuximpl>.
*/
#ifndef CKERN_PLATFORM_STARTUP_HEADER
#define CKERN_PLATFORM_STARTUP_HEADER

/* typedef: mainthread_f
 * Signature of main thread is the same as function main. */
typedef int                            (* mainthread_f) (int argc, const char ** argv) ;


// section: Functions

// group: startup

/* function: startup_platform
 * Initialize system context and calls main_thread.
 * If the system context could be initialized and the main_thread was called
 * the return value is the value returned by main_thread. If an error occurs during
 * initialization only an error code (value > 0) is returned, main_thread is not called.
 *
 * You have to call this function before calling <init_maincontext>.
 *
 * This function is implemented in a system specific way.
 * */
int startup_platform(int argc, const char ** argv, mainthread_f main_thread) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_startup
 * Test <startup_system> functionality. */
int unittest_platform_startup(void) ;
#endif


#endif
