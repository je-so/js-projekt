/* title: Standard-Types

   Declares some printf/scanf format specifiers and type descriptions.
   And adds standard definition for supporting unicode encoded wide characters.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/stdtypes/stdtypes.h
    Header file <Standard-Types>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_STDTYPES_STDTYPES_HEADER
#define CKERN_STDTYPES_STDTYPES_HEADER


// section: Macros

// group: format

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
 * PRId8   - used to print type int8_t
 * PRId16  - used to print type int16_t
 * PRId32  - used to print type int32_t
 * PRId64  - used to print type int64_t
 * PRIu8   - used to print type uint8_t
 * PRIu16  - used to print type uint16_t
 * PRIu32  - used to print type uint32_t
 * PRIu64  - used to print type uint64_t
 * PRIuPTR - used to print type uintptr_t
 * PRIxFCT - used to print type uintptrf_t
 * PRIuSIZE - used to print type size_t
 *
 * scanf specifiers:
 * They are prefixed with the correct length modifier ( 'hh', 'h', 'l', 'll' )
 *
 * SCNd8   - used to scan type int8_t
 * SCNd16  - used to scan type int16_t
 * SCNd32  - used to scan type int32_t
 * SCNd64  - used to scan type int64_t
 * SCNu8   - used to scan type uint8_t
 * SCNu16  - used to scan type uint16_t
 * SCNu32  - used to scan type uint32_t
 * SCNu64  - used to scan type uint64_t
 * SCNuPTR - used to scan type uintptr_t
 * */

/* define: PRIuSIZE
 * printf unsigned int format specifier 'zu' for type *size_t*. */
#define PRIuSIZE     "zu"

/* define: PRIxFCT
 * printf hexadecimal format specifier for type *uintptrf_t*. */
#define PRIxFCT      PRIxPTR

/* define: SCNuSIZE
 * scanf unsigned int format specifier 'zu' for type *size_t*. */
#define SCNuSIZE     "zu"

// group: integer

/* typedef: ramsize_t
 * Ramsize could be bigger than size_t to match 32 bit machines with more than 4GTB of ram. */
typedef uint64_t     ramsize_t;

/* typedef: uintptrf_t
 * An integer type big enough to hold a pointer to a function.
 * Type uintptr_t and uintptrf_t could differ in size.
 * But in POSIX pointer to symbols (man dlsym) are considered to fit in type (void*)
 * therefore in most environments sizeof(uintptrf_t) == sizeof(uintptr_t).*/
typedef uintptr_t    uintptrf_t;

// group: limits

/* define: OFF_MAX
 * Declares the maximum value of type off_t.
 * The size of off_t is checked in file "C-kern/test/compiletime/stdtypes.h"*/
#define OFF_MAX      INT64_MAX

/* function: castPoff_size
 * Casts Parameter off (type off_t) to type size_t.
 *
 * Unchecked Precondition:
 * - off >= 0 */
#if (SIZE_MAX/2 == OFF_MAX)
static inline size_t castPoff_size(off_t off)
{
         static_assert(sizeof(size_t) == sizeof(off_t), "same byte size; but unsigned vs. signed");
         return (size_t) off;
}
#else
static inline off_t castPoff_size(off_t off)
{
         static_assert(sizeof(size_t) != sizeof(off_t), "different byte size ==> smaller is propagated to larger type (no casting necessary)");
         return off;
}
#endif

// group: unicode

/* typedef: char32_t
 * Defines the unicode *code point* as »32 bit unsigned integer«.
 * The unicode character set assigns to every contained abstract character
 * an integer number - also named its *code point*. The range for all
 * available code points ("code space") is [0 ... 10FFFF].
 *
 * UTF-32:
 * <char32_t> represents a single unicode character as a single integer value
 * - same as UTF-32.
 *
 * wchar_t:
 * The wide character type wchar_t is either of type signed 32 bit or unsigned 16 bit.
 * Depending on the chosen platform. So it is not portable.
 *
 * (Will be removed if upcoming C11 is supported by compiler)
 *
 * */
typedef uint32_t     char32_t;

#endif
