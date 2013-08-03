/* title: Standard-Types

   Declares some printf/scanf format specifiers and type descriptions.
   And adds standard definition for supporting unicode encoded wide characters.

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

   file: C-kern/api/context/stdtypes.h
    Header file <Standard-Types>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_CONTEXT_STDTYPES_HEADER
#define CKERN_CONTEXT_STDTYPES_HEADER


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
#define PRIuSIZE                          "zu"

/* define: SCNuSIZE
 * scanf unsigned int format specifier 'zu' for *size_t*. */
#define SCNuSIZE                          "zu"

// group: integer

/* typedef: ramsize_t
 * Ramsize could be bigger than size_t to match 32 bit machines with more than 4GTB of ram. */
typedef uint64_t                          ramsize_t ;

// group: limits

/* define: OFF_MAX
 * Declares the maximum value of type off_t.
 * The size of off_t is checked in file "C-kern/test/compiletime/stdtypes.h"*/
#define OFF_MAX                           INT64_MAX

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
typedef uint32_t                          char32_t ;

#endif
