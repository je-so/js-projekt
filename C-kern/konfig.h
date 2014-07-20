/* title: Konfiguration
   Global generic configurations.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/konfig.h
    Header file of <Konfiguration>.

   file: C-kern/api/platform/Linux/syskonfig.h
    Linux specific configuration file <LinuxSystemKonfig>.
*/
#ifndef CKERN_KONFIG_HEADER
#define CKERN_KONFIG_HEADER

/*
 * List of sections:
 *
 * 1. Configuration switches: list of all configuration options to switch language and os.
 * 2. Include standard preprocessor macros and additional format/type specifiers.
 * 3. Include os specific settings and definitions of system specific types.
 * 4. Include standard runtime environment
 */

// section: Definitions

// group: 1. Configuration
// List of all configuration options.

//{
/* define: KONFIG_LANG
 * Choose default language for compiletime/runtime text output.
 *
 * Supported language codes:
 * de - German
 * en - English */
#define KONFIG_LANG                    en
/*define: KONFIG_MEMALIGN
 * Alignment of allocated memory.
 * Every allocated memory address must be aligned with this value.
 * The value of 4 is suitable for architectures with a i386 processor. */
#define KONFIG_MEMALIGN                4
/* define: KONFIG_OS
 * Choose name of operating system this project is compiled for.
 * The value is used as include path for system specific settings.
 *
 * Supported values:
 * Linux - The only supported operating system during design stage. */
#define KONFIG_OS                      Linux
#if !defined(KONFIG_SUBSYS)
/* define: KONFIG_SUBSYS
 * Defines which subsystems should be included.
 * You can choose more than one subsystem, separate them by operator '|'.
 *
 * Supported values are:
 * NONE       -  Support a minimal system.
 * THREAD     -  Support also threads.
 * SYSUSER    -  Support system users and authentication.
 * SYNCRUNNER -  Support thread local <syncrunner_t> which manages <syncfunc_t>.
 */
#define KONFIG_SUBSYS                  (THREAD|SYSUSER|SYNCRUNNER)
#endif
#if 0
/* define: KONFIG_UNITTEST
 * Define this in your Makefile to include additional code for testing single components. */
#define KONFIG_UNITTEST
#endif
#if !defined(KONFIG_USERINTERFACE)
/* define: KONFIG_USERINTERFACE
 * Sets the graphic subsystem.
 * Defines which user interface subsystem should be included if any at all.
 * You can choose more than one user interface subsystem, seperate them by operator '|'.
 *
 * Supported values are:
 * NONE  - No graphics support. This is the default value if you do not provide a value.
 * EGL   - Supports OpenGL binding to native (X11 and other) windowing system.
 * X11   - X11 window system. */
#define KONFIG_USERINTERFACE NONE
#endif

#define EGL 1
#define X11 4
#define THREAD   8
#define SYSUSER 16
#define SYNCRUNNER 32

#if ((KONFIG_SUBSYS)&THREAD)
/* define: KONFIG_SUBSYS_THREAD
 * Will be automatically defined if <KONFIG_SUBSYS> contains THREAD. */
#define KONFIG_SUBSYS_THREAD
#endif
#if ((KONFIG_SUBSYS)&SYSUSER)
/* define: KONFIG_SUBSYS_SYSUSER
 * Will be automatically defined if <KONFIG_SUBSYS> contains SYSUSER. */
#define KONFIG_SUBSYS_SYSUSER
#endif
#if ((KONFIG_SUBSYS)&SYNCRUNNER)
/* define: KONFIG_SUBSYS_SYNCRUNNER
 * Will be automatically defined if <KONFIG_SUBSYS> contains SYNCRUNNER. */
#define KONFIG_SUBSYS_SYNCRUNNER
#endif
#if ((KONFIG_SUBSYS)&(THREAD|SYSUSER|SYNCRUNNER))
/* define: KONFIG_SUBSYS_NONE
 * Will be automatically defined if <KONFIG_SUBSYS> contains no other valid option. */
#define KONFIG_SUBSYS_NONE
#endif
#if ((KONFIG_USERINTERFACE)&EGL)
/* define: KONFIG_USERINTERFACE_EGL
 * Will be automatically defined if <KONFIG_USERINTERFACE> contains EGL. */
#define KONFIG_USERINTERFACE_EGL
#endif
#if ((KONFIG_USERINTERFACE)&X11)
/* define: KONFIG_USERINTERFACE_X11
 * Will be automatically defined if <KONFIG_USERINTERFACE> contains X11. */
#define KONFIG_USERINTERFACE_X11
#endif
#if !((KONFIG_USERINTERFACE)&(X11|EGL))
/* define: KONFIG_USERINTERFACE_NONE
 * Will be automatically defined if <KONFIG_USERINTERFACE> contains no other valid option. */
#define KONFIG_USERINTERFACE_NONE
#endif

#undef EGL
#undef X11
#undef THREAD
#undef SYSUSER
#undef SYNCRUNNER
//}

// group: 2. Standard Macros
//
// Includes:
// * <Standard-Macros>

//{
#include "C-kern/api/context/stdmacros.h"
//}

// group: 3. System Specific Definitions
// Include system settings:
// - Include <LinuxSystemKonfig>
// - Include <LinuxSystemTypes>
// - Include <LinuxSystemOptimizations>
// - Include <LinuxSystemContext>
//
// Path:
// The location of these system specific headers is "C-kern/api/platform/KONFIG_OS/".
// <KONFIG_OS> is replaced by the name of the configured operating system this project is compiled for.

//{
#include STR(C-kern/api/platform/KONFIG_OS/syskonfig.h)
#include STR(C-kern/api/platform/KONFIG_OS/systypes.h)
#include STR(C-kern/api/platform/KONFIG_OS/sysoptimize.h)
#include STR(C-kern/api/platform/KONFIG_OS/syscontext.h)
//}

// group: 4. Standard environment
// Includes all C-kern(el) headers which define the standard runtime and compiletime environment.
//
// Includes:
// * <Standard-Types>
// * <InterfaceableObject> Standard type used to access objects independently from their implementation
// * <MainContext>.

//{
#include "C-kern/api/context/stdtypes.h"
#include "C-kern/api/context/iobj.h"
#include "C-kern/api/maincontext.h"
//}


#endif
