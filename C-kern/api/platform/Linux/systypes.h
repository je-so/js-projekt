/* title: LinuxSystemTypes
   Defines system specific types which are used in public interfaces.
   System specific known only to the implementation do not need to be public.

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

   file: C-kern/api/platform/Linux/systypes.h
    Header file <LinuxSystemTypes>.

   file: C-kern/konfig.h
    Included from <Konfiguration>.
*/
#ifndef CKERN_PLATFORM_LINUX_SYSTYPES_HEADER
#define CKERN_PLATFORM_LINUX_SYSTYPES_HEADER


// section: sytem specific configurations

// group: System Specific Public Types

/* about: Public System Types
 *
 * sys_iochannel_t             - Type holding system specific description of a file descriptor.
 *                               Which is used for files and network connections (sockets).
 * sys_iochannel_INIT_FREEABLE - Static initializer for <sys_iochannel_t>. It marks the file descriptor as invalid.
 * sys_mutex_t                 - Type holding system specific description of a mutex (lock).
 * sys_mutex_INIT_DEFAULT      - Static initializer for a mutex useable by threads of the same process.
 * sys_process_t               - Type represents a system process.
 * sys_process_INIT_FREEABLE   - Static initializer for a process. It sets a process id or handle to an invalid value.
 * sys_semaphore_t             - Type holding system specific description of a semaphore.
 * sys_semaphore_INIT_FREEABLE - Static initializer for a semaphore. It sets a semaphore handle to an invalid value.
 * sys_socketaddr_t            - Type holding network addresses used from sockets.
 * sys_socketaddr_MAXSIZE      - Value which holds max size in bytes of all possible socket addresses.
 * sys_thread_t                - Type holding system specific description of a thread.
 * sys_thread_INIT_FREEABLE    - Static initializer for a thread. It sets a thread id or handle to an invalid value.
 * sys_userid_t                - Type holding system specific description of a system user id.
 * sys_userid_INIT_FREEABLE    - Static initializer for a system user id. It sets a user id to an invalid value.
 *
 * */


/* define: sys_iochannel_t
 * Choose Posix file descriptor type. */
#define sys_iochannel_t                int

/* define: sys_iochannel_STDIN
 * Choose Posix STDIN file descriptor number. */
#define sys_iochannel_STDIN            STDIN_FILENO

/* define: sys_iochannel_STDOUT
 * Choose Posix STDOUT file descriptor number. */
#define sys_iochannel_STDOUT           STDOUT_FILENO

/* define: sys_iochannel_STDERR
 * Choose Posix STDERR file descriptor number. */
#define sys_iochannel_STDERR           STDERR_FILENO

/* define: sys_iochannel_INIT_FREEABLE
 * Choose Posix file descriptor type. */
#define sys_iochannel_INIT_FREEABLE    (-1)

/* define: sys_mutex_t
 * Chooses Posix mutex type. Needs pthread support. */
#define sys_mutex_t                    pthread_mutex_t

/* define: sys_mutex_INIT_DEFAULT
 * Static initializer for <sys_mutex_t>. */
#define sys_mutex_INIT_DEFAULT         PTHREAD_MUTEX_INITIALIZER

/* define: sys_process_t
 * Chooses Posix process id. */
#define sys_process_t                  pid_t

/* define: sys_process_INIT_FREEABLE
 * Static initializer for <sys_process_t>. */
#define sys_process_INIT_FREEABLE      (0)

/* define: sys_semaphore_t
 * Chooses Posix semaphore handle. */
#define sys_semaphore_t                int

/* define: sys_semaphore_INIT_FREEABLE
 * Static initializer for <sys_semaphore_t>. */
#define sys_semaphore_INIT_FREEABLE    (-1)

/* define: sys_socketaddr_t
 * Chooses Posix socket address type. */
#define sys_socketaddr_t               struct sockaddr

/* define: sys_socketaddr_MAXSIZE
 * Defined as Posix specific size of IPv6 addresses. Change this if you want to support other type of addresses than IP. */
#define sys_socketaddr_MAXSIZE         sizeof(struct sockaddr_in6)

/* define: sys_thread_t
 * Chooses Posix thread type.  Needs pthread support. */
#define sys_thread_t                   pthread_t

/* define: sys_thread_INIT_FREEABLE
 * Static initializer for <sys_thread_t>. */
#define sys_thread_INIT_FREEABLE       (0)

/* define: sys_userid_t
 * Chooses Posix system user id. */
#define sys_userid_t                   uid_t

/* define: sys_userid_INIT_FREEABLE
 * Static initializer for <sys_userid_t>. */
#define sys_userid_INIT_FREEABLE       ((uid_t)(-1))

#endif
