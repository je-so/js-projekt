/* title: LinuxSystemKonfig

   System specific configuration for Linux.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#define _FILE_OFFSET_BITS 64
/* define: _GNU_SOURCE
 * Makes file api support of files with size of 2GB * 4GB. */
#define _GNU_SOURCE

// == Unknown Compiler ==
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
 * This ensures that all files are compiled with the same system headers.
 * But system specific include files for X11, OpenGL and other graphic libraries
 * are included only in the implementation files.
 *
 * May be changed:
 * This rule may be changed in the future to speed up compilation.
 *
 * */

#include <arpa/inet.h>
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <malloc.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>    // -lpthread
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <time.h>       // -lrt (clock_getres,clock_gettime,clock_settime)
#include <ucontext.h>
#include <unistd.h>
#include <wchar.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>


#endif
