/*
   C-System-Layer: C-kern/api/os/Linux/syskonfig.h
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/
#ifndef CKERN_LINUX_SYSKONFIG_HEADER
#define CKERN_LINUX_SYSKONFIG_HEADER

/* set compiler specific defines */

#if defined __GNUC__      // GNU C-Compiler //
#define _THREAD_SAFE
#define _FILE_OFFSET_BITS  64
#define _GNU_SOURCE
#else                // Unknown Compiler //
#define de 1
#if (KONFIG_LANG==de)
#error "nicht unterstützter Compiler unter Linux"
#else
#error "unsupported Compiler on Linux"
#endif
#undef de
#endif

#include <arpa/inet.h>
#include <assert.h>
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
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <wchar.h>

/* !OVERWRITE! system specific settings */
#undef  sys_directory_t
#define sys_directory_t          DIR*
#undef  sys_directory_entry_t
#define sys_directory_entry_t    struct dirent
#undef  sys_thread_t
#define sys_thread_t             pthread_t
#undef  sys_thread_mutex_t
#define sys_thread_mutex_t       pthread_mutex_t
#undef  sys_thread_mutex_INIT_DEFAULT
#define sys_thread_mutex_INIT_DEFAULT  PTHREAD_MUTEX_INITIALIZER
#undef  sys_timerid_t
#define sys_timerid_t                  int
#undef  sys_timerid_INIT_FREEABLE
#define sys_timerid_INIT_FREEABLE      -1

#endif
