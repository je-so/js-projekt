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

   file: C-kern/api/os/Linux/syskonfig.h
    Header file of <LinuxSystemKonfig>.

   file: C-kern/konfig.h
    Generic header <Konfiguration>.
*/
#ifndef CKERN_LINUX_SYSKONFIG_HEADER
#define CKERN_LINUX_SYSKONFIG_HEADER

// section: sytem specific configurations

// group: GNU C-Compiler
#if defined __GNUC__          //  //
/* define: _THREAD_SAFE
 * Makes calls to library functions thread safe. */
#define _THREAD_SAFE
/* define: _FILE_OFFSET_BITS
 * Makes file api support of files with size of 2GB * 4GB. */
#define _FILE_OFFSET_BITS  64
/* define: _GNU_SOURCE
 * Makes file api support of files with size of 2GB * 4GB. */
#define _GNU_SOURCE

#else                         //  //
// group: Unknown Compiler
#define de 1
#if (KONFIG_LANG==de)
#error "nicht unterstützter Compiler unter Linux"
#else
#error "unsupported Compiler on Linux"
#endif
#undef de
#endif

// group: Include files

/* about: Linux specific includes
 * Include all C99, Posix and Linux specific header files.
 * This ensures that all files are compiled in the same way.
 * No system specific includes are used in any implmentation file.
 *
 * May be changed:
 * This rule may be changed in the future to make up for faster compilation. */
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <wchar.h>

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


/* group: !OVERWRITE! system specific types */

/* define: sys_directory_t
 * Type represents an opened file system directory.
 * This type is Posix specific with pthread support. */
#undef  sys_directory_t
#define sys_directory_t                DIR*
/* define: sys_directory_entry_t
 * Type represents one file name entry in a directory.
 * This type is Posix specific with pthread support. */
#undef  sys_directory_entry_t
#define sys_directory_entry_t          struct dirent
/* define: sys_file_t
 * Type represents a system file descriptor.
 * This type is Posix specific. */
#undef  sys_file_t
#define sys_file_t                     int
/* define: sys_file_INIT_FREEABLE
 * Static initializer for a file which is in a closed state. */
#undef  sys_file_INIT_FREEABLE
#define sys_file_INIT_FREEABLE         (-1)
/* define: sys_file_STDIN
 * Static initializer for the standard input file of a process. */
#define sys_file_STDIN                 (STDIN_FILENO)
/* define: sys_file_STDOUT
 * Static initializer for the standard ouptut file of a process. */
#define sys_file_STDOUT                (STDOUT_FILENO)
/* define: sys_file_STDERR
 * Static initializer for the standard error output file of a process. */
#define sys_file_STDERR                (STDERR_FILENO)
/* define: sys_mutex_t
 * Type represents a mutual exclusion lock.
 * This type is Posix specific with pthread support. */
#undef  sys_mutex_t
#define sys_mutex_t                    pthread_mutex_t
/* define: sys_mutex_INIT_DEFAULT
 * Static initializer for <sys_mutex_t>. */
#undef  sys_mutex_INIT_DEFAULT
#define sys_mutex_INIT_DEFAULT         PTHREAD_MUTEX_INITIALIZER
/* define: sys_process_t
 * Type represents a system process.
 * This type is Posix specific. */
#undef  sys_process_t
#define sys_process_t                  pid_t
/* define: sys_process_INIT_FREEABLE
 * Static initializer for <sys_process_t>. */
#undef  sys_process_INIT_FREEABLE
#define sys_process_INIT_FREEABLE      (0)
/* define: sys_semaphore_t
 * Types represents a system semaphore.
 * This type is Posix specific. */
#undef  sys_semaphore_t
#define sys_semaphore_t                int
/* define: sys_semaphore_INIT_FREEABLE
 * Static initializer for <sys_semaphore_t>. */
#undef  sys_semaphore_INIT_FREEABLE
#define sys_semaphore_INIT_FREEABLE    (-1)
/* define: sys_thread_t
 * Types represents a system thread.
 * This type is Posix specific with pthread support. */
#undef  sys_thread_t
#define sys_thread_t                   pthread_t
/* define: sys_thread_INIT_FREEABLE
 * Static initializer for <sys_thread_t>. */
#undef  sys_thread_INIT_FREEABLE
#define sys_thread_INIT_FREEABLE       (0)
/* define: sys_timerid_t
 * Types represents a system timer.
 * This type is Linux specific. */
#undef  sys_timerid_t
#define sys_timerid_t                  int
/* define: sys_timerid_INIT_FREEABLE
 * Static initializer for <sys_timerid_t>. */
#undef  sys_timerid_INIT_FREEABLE
#define sys_timerid_INIT_FREEABLE      (-1)

#endif
