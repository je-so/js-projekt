/* title: LinuxSystemOptimizations

   Contains optimizations, for example fast redefinitions for functions.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
 * The x86 fpu is faster than the standard integer algorithm for computing the square root.
 * If no compiler specific version is defined <sqroot_int64> is used as default. */
#define sys_sqroot_int64 sqrtl
#endif

// == Unknown Compiler ==
#else

#define de 1
#if (KONFIG_LANG==de)
#error "nicht unterstützter Compiler unter Linux"
#else
#error "unsupported Compiler on Linux"
#endif
#undef de

#endif   // end compiler selection


// group: Default Definitions

#ifndef sys_sqroot_int64
/* Use <sqroot_int64> as default. */
#define sys_sqroot_int64 sqroot_int64
#endif


#endif
