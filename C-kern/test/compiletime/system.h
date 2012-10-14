/* title: CompiletimeTest System
   Test the system dependencies the C-kernel is relying on.

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

   file: C-kern/test/compiletime/system.h
    Header file of <CompiletimeTest System>.

   file: C-kern/api/test/compiletime.h
    Included from <Compiletime-Tests>.
*/
#ifndef CKERN_TEST_COMPILETIME_SYSTEM_HEADER
#define CKERN_TEST_COMPILETIME_SYSTEM_HEADER

#define  de    1
#define  Linux 1

/* about: Test architecture size
 * Only 32 Bits supported */

#if   (!defined(INT_MAX) || (INT_MAX != 2147483647)) && (KONFIG_LANG==de)
#  error  Momentan wird nur 32 Bit unterstützt
#  error  TODO: Baue und teste 64 Bit Architektur
#elif (!defined(INT_MAX) || (INT_MAX != 2147483647))
#  error  only 32 Bit architecture supported
#  error  TODO: Compile and test 64 bit architecture
#endif

/* about: Filesize-64bit Test
 * Test (in os specific manner) system supports 64 bit file size. */

static inline void compiletime_tests_system(void)
{
#if (KONFIG_OS == Linux)
   static_assert( sizeof(((struct stat*)0)->st_size) == sizeof(int64_t), "No 64bit file-system support" );
#elif (KONFIG_OS != Linux) && (KONFIG_LANG==de)
#  error Baue Test für 64 bit Files
#elif (KONFIG_OS != Linux)
#  error Implement test for 64 bit filesize support
#endif
}

/* about: Treadsafe Test
 * Test (in os specific manner) system library is thread safe. */

#if    (KONFIG_OS == Linux) && !defined(__USE_REENTRANT) && (KONFIG_LANG==de)
#  error Compiler unterstützt keine Threads
#elsif (KONFIG_OS == Linux) && !defined(__USE_REENTRANT)
#  error Compiler supports no threads
#elsif (KONFIG_OS != Linux) && (KONFIG_LANG==de)
#  error Baue Test für Threadsicherheit
#elsif (KONFIG_OS != Linux)
#  error Implement test for thread safety of libraries
#endif


#undef Linux
#undef de

#endif