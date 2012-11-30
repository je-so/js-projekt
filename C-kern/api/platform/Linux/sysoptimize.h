/* title: LinuxSystemOptimizations

   Contains optimizations, for example fast redefinitions for functions.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/platform/Linux/sysoptimize.h
    Header file <LinuxSystemOptimizations>.

   file: C-kern/konfig.h
    Included from <Konfiguration>.
*/
#ifndef CKERN_PLATFORM_LINUX_SYSOPTIMIZE_HEADER
#define CKERN_PLATFORM_LINUX_SYSOPTIMIZE_HEADER


// section: system specific configurations

/* about: Supported Function Replacements
 *
 * sys_sqroot_int64 - Replaces <sqroot_int64>.
 *                    Set it either to sqroot_int64 or another system specific implementation.
 *
 * */

// group: GNU C-Compiler
#if defined(__GNUC__)

#if defined(__i386) || defined(__i686)
/* define: sys_sqroot_int64
 * Replaces <sqroot_int64> with faster sqrtl (long double version).
 * The x86 fpu is faster than the standard integer algorithm for computing the square root. */
#define sys_sqroot_int64               sqrtl
#endif

// group: Unknown Compiler
#else

#define de 1
#if (KONFIG_LANG==de)
#error "nicht unterstützter Compiler unter Linux"
#else
#error "unsupported Compiler on Linux"
#endif
#undef de

#endif   // end compiler selection


// group: Select Defaults

#ifndef sys_sqroot_int64
/* define: sys_sqroot_int64
 * Use default implementation <sqroot_int64>. */
#define sys_sqroot_int64               sqroot_int64
#endif


#endif
