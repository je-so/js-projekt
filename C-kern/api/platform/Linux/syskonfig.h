/* title: LinuxSystemKonfig
   System specific configuration for Linux.

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

   file: C-kern/api/platform/Linux/syskonfig.h
    Header file of <LinuxSystemKonfig>.

   file: C-kern/konfig.h
    Generic header <Konfiguration>.
*/
#ifndef CKERN_PLATFORM_LINUX_SYSKONFIG_HEADER
#define CKERN_PLATFORM_LINUX_SYSKONFIG_HEADER


// section: system specific configurations

// group: GNU C-Compiler
#if defined(__GNUC__)
/* define: _THREAD_SAFE
 * Makes calls to library functions thread safe. */
#define _THREAD_SAFE
/* define: _FILE_OFFSET_BITS
 * Makes file api support of files with size of 2GB * 4GB. */
#define _FILE_OFFSET_BITS  64
/* define: _GNU_SOURCE
 * Makes file api support of files with size of 2GB * 4GB. */
#define _GNU_SOURCE

// group: Unknown Compiler
#else
#define de 1
#if (KONFIG_LANG==de)
#error "nicht unterstützter Compiler unter Linux"
#else
#error "unsupported Compiler on Linux"
#endif
#undef de
#endif

// group: Include Files

/* about: Linux Specific Includes
 * Include all C99, Posix and Linux specific header files.
 * This ensures that all files are compiled in the same way.
 * No system specific includes are used in any implmentation file.
 *
 * May be changed:
 * This rule may be changed in the future to make up for faster compilation.
 *
 * libpam:
 * If you call functions of PAM (Pluggable Authentication Modules) included from
 * > #include <security/pam_appl.h>
 * you need to link the binary with libpam (-lpam).
 * */

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>   // -lpthread
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>   // -lrt (clock_getres,clock_gettime,clock_settime)
#include <ucontext.h>
#include <unistd.h>
#include <wchar.h>
#define SYSUSER 1
#if ((KONFIG_SUBSYS)&SYSUSER)
#include <security/pam_appl.h>   // -lpam
#endif
#undef SYSUSER
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>


#endif
