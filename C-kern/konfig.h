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
 * KONFIG_de - German
 * KONFIG_en - English */
#define KONFIG_LANG                    KONFIG_en
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
 * KONFIG_linux - The only supported operating system during design stage. */
#define KONFIG_OS                      KONFIG_linux
#if !defined(KONFIG_SUBSYS)
/* define: KONFIG_SUBSYS
 * Defines which subsystems should be included.
 * You can choose more than one subsystem, seperate them by operator '|'.
 *
 * Supported values are:
 * KONFIG_thread  -  for thread support
 * KONFIG_sysuser -  for supporting system user and authentication
 * KONFIG_none    -  for a minimal system.
 */
#define KONFIG_SUBSYS                  (KONFIG_thread|KONFIG_sysuser)
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
 * KONFIG_none  - no graphics support. This is the default value if you do not provide a value.
 * KONFIG_html5 - Supports http connection to a browser and HTML views which communicate with help
 *                of Javascript and XMLHttpRequest(.upload).
 * KONFIG_x11   - X11 window system + OpenGL graphics (currently no development, not supported yet). */
#define KONFIG_USERINTERFACE           KONFIG_none
#endif
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
// Path:
// The location of these system specific headers is "C-kern/api/platform/KONFIG_OS/".
// <KONFIG_OS> is replaced by the name of the configured operating system this project is compiled for.

//{
#define KONFIG_linux Linux
#include STR(C-kern/api/platform/KONFIG_OS/syskonfig.h)
#include STR(C-kern/api/platform/KONFIG_OS/systypes.h)
#include STR(C-kern/api/platform/KONFIG_OS/sysoptimize.h)
#include STR(C-kern/api/platform/KONFIG_OS/syscontext.h)
#undef KONFIG_linux
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
