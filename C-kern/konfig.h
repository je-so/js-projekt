/* title: Konfiguration
   Global Configurations.

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
   (C) 2010 Jörg Seebohn

   file: C-kern/konfig.h
    Header file of <Konfiguration>.
*/
#ifndef CKERN_KONFIG_HEADER
#define CKERN_KONFIG_HEADER

/** Global visible (k)configuration
 * 1. Generic defines
 * 2. Configuration switches
 * 3. System and compiler specific settings
 * 4. Include os specific settings
 * 5. Standard environment
 * */

// section: Definitions

// group: 1. Generic defines
// Useful preprocessor macros.

//{
/* define: CONCAT_
 * Used by <CONCAT>. Combines two language tokens into one. */
#define CONCAT_(S1,S2)  S1 ## S2
/* define: CONCAT
 * Combines two language tokens into one. Calls <CONCAT_> to ensure expansion of arguments.
 *
 * Example:
 * > CONCAT(var,__LINE__)  // generates token var10 if current line number is 10
 * > CONCAT_(var,__LINE__) // generates token var__LINE__ */
#define CONCAT(S1,S2)   CONCAT_(S1,S2)
#define MEMSET0(ptr)    memset(ptr, 0, sizeof(*(ptr)))
/* define: MALLOC
 * Ruft spezifisches malloc auf, wobei der Rückgabewert in den entsprechenden Typ konvertiert wird.
 * Die Paramterliste muss immer mit einem Komma enden !
 * > usertype_t * ptr = MALLOC(usertype_t, malloc, ) ;
 *
 * Parameter:
 * type_t    - Der Name des Typs, für den Speicher reserviert werden soll.
 *             Der Parameter sizeof(type_t) wird der malloc Funktion als letzter übergeben.
 * malloc_f  - Der Name der aufzurufenden malloc Funktion.
 * ...       - Weitere, malloc spezifische Parameter, welche an erster Stelle noch vor der Grössenangabe stehen. */
#define MALLOC(type_t,malloc_f,...)    ((type_t*) ((malloc_f)( __VA_ARGS__ sizeof(type_t))))
#define MEMCOPY(destination, source)   do { static_assert_void(sizeof(*(destination)) == sizeof(*(source))) ; memcpy((destination), (source), sizeof(*(destination))) ; } while(0)
/* define: STR_
 * Used by <STR>. Makes string token out of argument. */
#define STR_(S1)        # S1
/* define: STR
 * Makes string token out of argument. Calls <STR_> to ensure expansion of argument.
 *
 * Example:
 * > STR(__LINE__)  // generates token "10" if current line number is 10
 * > STR_(__LINE__) // generates token "__LINE__" */
#define STR(S1)         STR_(S1)
/* define: static_assert
 * Checks condition to be true during compilation. No runtime code is generated. Use it in global context.
 *
 * Paramters:
 *  C - Condition which must hold true
 *  S - human readable explanation (ignored) */
#define static_assert(C,S)     extern int CONCAT(_extern_static_assert,__LINE__) [ (C) ? 1 : -1]
/* define: static_assert_void
 * Same as <static_assert> but this time the implementaion suppresses an unused variable warning.
 * Can only be used in function context. It expects only the condition to test for as paramter. */
#define static_assert_void(C)  extern int CONCAT(_extern_static_assert,__LINE__) [ (C) ? 1 : -1] ; (void)CONCAT(_extern_static_assert,__LINE__)
//}

// group: 2. Configuration
// List of all configuration options.

//{
/* about: Options
 * List of all options.
 *
 * KONFIG_LANG       - Language code: values 'de' or 'en' are supported to select static language of text strings for compiled code.
 *                     See <KONFIG_LANG>.
 * KONFIG_MEMALIGN   - Every allocated memory address must be aligned with this value.
 *                     See <KONFIG_MEMALIGN>.
 * KONFIG_OS         - Name of operating system (used as include path for system specific settings).
 *                     See <KONFIG_OS>.
 * KONFIG_UNITTEST   - Define this in your Makefile to include additional code for testing single components. */

/* define: KONFIG_LANG
 * Choose default language for compiletime/runtime text output.
 * > #define KONFIG_LANG   en */
#define KONFIG_LANG        en
/*define: KONFIG_MEMALIGN
 * Alignment of allocated memory. The value of 4 is suitable for architectures with a i386 processor.
 * > #define KONFIG_MEMALIGN    4 */
#define KONFIG_MEMALIGN    4
/* define: KONFIG_OS
 * Choose name of operating system this project is compiled for.
 * > #define KONFIG_OS     Linux */
#define KONFIG_OS          Linux

//}

// group: 3 System specific settings
// List of system specific settings which must be defined in some architecture dependent way.

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
 * */

/* define: PRIuSIZE
 * printf unsigned int format specifier ( 'u' or 'lu') for *size_t*.
 * This macro uses __PRIPTR_PREFIX from C99 std header 'inttypes.h'.
 * If it does not exist you must define it in the makefile. */
#define PRIuSIZE             __PRIPTR_PREFIX "u"

/** define: sys_processid_t
 * integer type holding system specific id of a running process. Overwritten in system specific include file. */
#define sys_processid_t      void
/** define: sys_thread_t
 * structure holding system specific description of a thread. Overwritten in system specific include file. */
#define sys_thread_t         void
/** define: sys_timerid_t
 * Handle for system specific timer. */
#define sys_timerid_t        void
/** define: sys_timerid_INIT_FREEABLE
 * Init value to declare an invalid timer handle. */
#define sys_timerid_INIT_FREEABLE   void
/** define: sys_directory_t
 * (pointer) type holding system specific description of an opened directory stream. Overwritten in system specific include file. */
#define sys_directory_t      void
/** define: sys_directory_entry_t
 * structure holding system specific description of read directory entry. Overwritten in system specific include file. */
#define sys_directory_entry_t      void
//}

// group: 4. Include
// Include os specific settings.

//{
/* about: Include
 * Includes an operating system dependent include file.
 * It redefines all system specific settings and includes all system headers relevant for implementation.
 * > #include STR(C-kern/api/os/KONFIG_OS/syskonfig.h)
 *
 * Filename:
 * The location of this system specific header is "C-kern/api/os/KONFIG_OS/syskonfig.h".
 * <KONFIG_OS> is replaced by the name of the configured operating system this project is compiled for. */
#include STR(C-kern/api/os/KONFIG_OS/syskonfig.h)
//}

// group: 5. Standard environment
// Includes all C-kern(el) headers which define the standard runtime environment.

//{
// TODO
//}


#endif
