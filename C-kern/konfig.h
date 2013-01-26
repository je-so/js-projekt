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
 * 1. Include standard preprocessor macros.
 * 2. Configuration switches: list of all configuration options to switch language and os.
 * 3. Declare format/type specifiers.
 * 4. Include os specific settings and definitions of system specific types.
 * 5. Include standard runtime environment
 */

// section: Definitions

// group: 1. Standard Macros
//
// Includes:
// * <Standard-Macros>

//{
#include "C-kern/api/context/stdmacros.h"
//}

// group: 2. Configuration
// List of all configuration options.

//{
/* define: KONFIG_LANG
 * Choose default language for compiletime/runtime text output.
 *
 * Supported language codes:
 * 'de' - German
 * 'en' - English */
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
 * You can choose more than one subsystem, seperate them by operator '|'.
 *
 * Supported values are:
 * THREAD  -  for thread support
 * SYSUSER -  for supporting system user and authentication
 * none    -  for a minimal system.
 */
#define KONFIG_SUBSYS                  (THREAD|SYSUSER)
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
 * none  - no graphics support. This is the default value if you do not provide a value.
 * HTML5 - Supports http connection to a browser and HTML views which communicate with help
 *         of Javascript and XMLHttpRequest(.upload).
 * X11   - X11 window system + OpenGL graphics (currently no development, not supported yet). */
#define KONFIG_USERINTERFACE           none
#endif


//}

// group: 3 Declare some missing standard specifiers
// Declares some printf/scanf format specifiers and type descriptions.

//{
/* about: integer format specifiers
 * Adapts printf/scanf format specifiers to 32/64 bit architectures.
 * These specificiers are taken from the C99 std header file "inttypes.h".
 *
 * Rationale:
 * > int64_t i ; printf("i = %lld\n", i) ;
 * This code does only work on 32 bit architectures where *long long int* is of type 64 bit.
 * On 64 bit architectures using the LP64 data model int64_t is defined as *long int*.
 * Therefore the following macros exist to adapt integer type to different architecture data models.
 *
 * Usage:
 * Instead of non portable code:
 * > int64_t i ; printf("i = %lld\n", i) ; scanf( "%lld", &i) ;
 * Use portable code:
 * > int64_t i ; printf("i = %" PRId64 "\n", i) ; scanf( "%" SCNd64, &i) ;
 *
 * printf specifiers:
 * They are prefixed with the correct length modifier ( 'l', 'll' )
 *
 * PRId8   - "d" for int8_t integer types
 * PRId16  - "d" for int16_t integer types
 * PRId32  - "d" for int32_t integer types
 * PRId64  - "d" for int64_t integer types
 * PRIu8   - "u" for uint8_t unsigned integer types
 * PRIu16  - "u" for uint16_t unsigned integer types
 * PRIu32  - "u" for uint32_t unsigned integer types
 * PRIu64  - "u" for uint64_t unsigned integer types
 * PRIuPTR - "u" for uintptr_t unsigned integer types
 * PRIuSIZE - "u" for size_t unsigned integer types
 *
 * scanf specifiers:
 * They are prefixed with the correct length modifier ( 'hh', 'h', 'l', 'll' )
 *
 * SCNd8   - "d" for int8_t integer types
 * SCNd16  - "d" for int16_t integer types
 * SCNd32  - "d" for int32_t integer types
 * SCNd64  - "d" for int64_t integer types
 * SCNu8   - "u" for uint8_t unsigned integer types
 * SCNu16  - "u" for uint16_t unsigned integer types
 * SCNu32  - "u" for uint32_t unsigned integer types
 * SCNu64  - "u" for uint64_t unsigned integer types
 * SCNuPTR - "u" for uintptr_t unsigned integer types
 * */

/* define: PRIuSIZE
 * printf unsigned int format specifier 'zu' for *size_t*. */
#define PRIuSIZE                       "zu"

/* define: SCNuSIZE
 * scanf unsigned int format specifier 'zu' for *size_t*. */
#define SCNuSIZE                       "zu"

/* define: OFF_MAX
 * Declares the maximum value of type off_t.
 * The size of off_t is checked in file "C-kern/test/compiletime/stdtypes.h"*/
#define OFF_MAX                        INT64_MAX
//}

// group: 4. System Specific Definitions
// Include system settings:
// - Include operating system headers relevant for implementation.
// - Include system specific types.
// - Include system specific optimizations.
// > #include STR(C-kern/api/platform/KONFIG_OS/syskonfig.h)
// > #include STR(C-kern/api/platform/KONFIG_OS/systypes.h)
// > #include STR(C-kern/api/platform/KONFIG_OS/sysoptimize.h)
//
// Path:
// The location of these system specific headers is "C-kern/api/platform/KONFIG_OS/".
// <KONFIG_OS> is replaced by the name of the configured operating system this project is compiled for.

//{
#include STR(C-kern/api/platform/KONFIG_OS/syskonfig.h)
#include STR(C-kern/api/platform/KONFIG_OS/systypes.h)
#include STR(C-kern/api/platform/KONFIG_OS/sysoptimize.h)
//}

// group: 5. Standard environment
// Includes all C-kern(el) headers which define the standard runtime and compiletime environment.
//
// Includes:
// * <MainContext>.
// * <Unicode> (Will be removed if upcoming C11 is supported)

//{
#include "C-kern/api/context/unicode.h"
#include "C-kern/api/maincontext.h"
//}


#endif
