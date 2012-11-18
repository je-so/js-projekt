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
 * 1. Generic defines: some useful preprocessor macros.
 * 2. Configuration switches: list of all configuration options to switch language and os.
 * 3. Declare system specific types.
 * 4. Declare system specific functions.
 * 5. Include os specific settings which redefine system specific types.
 * 6. Include standard runtime environment
 */

// section: Definitions

// group: 1. Generic defines
//
// Includes:
// * <C-Preprocessor-macros>
// * <C-Type-Extensions>

//{
#include "C-kern/api/C/ppmacros.h"
#include "C-kern/api/C/typeext.h"

/* define: MALLOC
 * Calls specific malloc function and converts the returned pointer to
 * the corresponding type.
 * The list of parameters must always end with a "," !
 * > usertype_t * ptr = MALLOC(usertype_t, malloc, ) ;
 *
 * Parameter:
 * type_t    - Name of the type memory should be allocated for.
 *             The parameter sizeof(type_t) is given as last parameter to malloc function.
 * malloc_f  - The name of the malloc function which should be called.
 * ...       - Additional parameters which comes before the sizeof() parameter. **/
#define MALLOC(type_t,malloc_f,...)    ((type_t*) ((malloc_f)( __VA_ARGS__ sizeof(type_t))))

/* define: MEMCOPY
 * Copies memory from source to destination.
 * The values of sizeof(*(source)) and sizeof(*(destination)) must compare equal.
 * This is checked by <static_assert>.
 *
 * Parameter:
 * destination - Pointer to destination address
 * source      - Pointer to source address
 */
#define MEMCOPY(destination, source)   do { static_assert(sizeof(*(destination)) == sizeof(*(source)),"same size") ; memcpy((destination), (source), sizeof(*(destination))) ; } while(0)

/* define: MEMSET0
 * Sets memory of variable to 0.
 *
 * Parameter:
 * pointer - Pointer to the variable which will be cleared.
 *
 * To clear a whole array use &array as parameter:
 * >  int array[100] ;
 * >  MEMSET0(&array) ; */
#define MEMSET0(pointer)               memset((pointer), 0, sizeof(*(pointer)))
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
 * THREAD -  for thread support
 * none   -  for a minimal system.
 */
#define KONFIG_SUBSYS                  (THREAD)
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

// group: 3 Declare system specific types
// Declares a bunch of system specific types which must be defined in some architecture dependent way.

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

/* define: sys_filedescr_t
 * Type holding system specific description of a file.
 * It is also used for network connections.
 * Overwritten in system specific include file. */
#define sys_filedescr_t                void
/* define: sys_filedescr_INIT_FREEABLE
 * Static initializer for a mutex useable by threads of the same process. */
#define sys_filedescr_INIT_FREEABLE    void
/* define: sys_mutex_t
 * Type holding system specific description of a mutex. */
#define sys_mutex_t                    void
/* define: sys_mutex_INIT_DEFAULT
 * Static initializer for a mutex useable by threads of the same process. */
#define sys_mutex_INIT_DEFAULT         void
/* define: sys_process_t
 * Static initializer for a mutex useable by threads of the same process. */
#define sys_process_t                  void
/* define: sys_process_INIT_FREEABLE
 * Static initializer for a process which id invalid. */
#define sys_process_INIT_FREEABLE      void
/* define: sys_semaphore_t
 * Type holding system specific description of a semaphore. Overwritten in system specific include file. */
#define sys_semaphore_t                void
/* define: sys_semaphore_INIT_FREEABLE
 * Init value to declare an invalid semaphore handle. Overwritten in system specific include file. */
#define sys_semaphore_INIT_FREEABLE    void
/* define: sys_socketaddr_t
 * Type which holds addresses received from sockets. Overwritten in system specific include file. */
#define sys_socketaddr_t               void
/* define: sys_socketaddr_MAXSIZE
 * Value which holds max size of all versions of socket addresses. Overwritten in system specific include file. */
#define sys_socketaddr_MAXSIZE         0
/* define: sys_thread_t
 * Type holding system specific description of a thread. Overwritten in system specific include file. */
#define sys_thread_t                   void
/* define: sys_thread_INIT_FREEABLE
 * Value of invalid thread ID. Overwritten in system specific include file. */
#define sys_thread_INIT_FREEABLE       void
//}

// group: 4 Declare system specific functions
// Declares a bunch of system specific function which can or must be defined in some architecture dependent way.

//{
/* define: sys_context_thread
 * System specific implementation to query <threadcontext_t>. */
#undef sys_context_thread
/* define: sys_sqroot_int64
 * Fast system specific implementation of <sqroot_int64>.
 * The default is to call <sqroot_int64> as the standard implementation. */
#define sys_sqroot_int64               sqroot_int64
//}

// group: 5. System Specific Redefinitions
// Include system specific settings which redefine system specific types.
// Includes an operating system dependent include file.
// It redefines all system specific settings and includes all system headers relevant for implementation.
// > #include STR(C-kern/api/platform/KONFIG_OS/syskonfig.h)
//
// Filename:
// The location of this system specific header is "C-kern/api/platform/KONFIG_OS/syskonfig.h".
// <KONFIG_OS> is replaced by the name of the configured operating system this project is compiled for.

//{
#include STR(C-kern/api/platform/KONFIG_OS/syskonfig.h)
//}

// group: 6. Standard environment
// Includes all C-kern(el) headers which define the standard runtime and compiletime environment.
//
// Includes:
// * <MainContext>.

//{
#include "C-kern/api/maincontext.h"
//}


#endif
