/* title: IOChannel impl

   Implements <IOChannel>.

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

   file: C-kern/api/io/iochannel.h
    Header file <IOChannel>.

   file: C-kern/platform/Linux/io/iochannel.c
    Implementation file <IOChannel impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/ip/ipaddr.h"
#include "C-kern/api/io/ip/ipsocket.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: iochannel_t

// group: lifetime

int initcopy_iochannel(/*out*/iochannel_t * ioc, iochannel_t from_ioc)
{
   int err ;
   int fd ;

   fd = fcntl(from_ioc, F_DUPFD_CLOEXEC, (long)0) ;

   if (fd < 0) {
      err = errno ;
      TRACESYSCALL_ERRLOG("fcntl", err) ;
      PRINTINT_ERRLOG(from_ioc) ;
      goto ONABORT ;
   }

   *ioc = fd ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_iochannel(iochannel_t * ioc)
{
   int err ;
   int close_ioc = *ioc ;

   if (!isfree_iochannel(close_ioc)) {
      *ioc = iochannel_INIT_FREEABLE ;

      err = close(close_ioc) ;
      if (err) {
         err = errno ;
         TRACESYSCALL_ERRLOG("close", err) ;
         PRINTINT_ERRLOG(close_ioc) ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

/* function: nropen_iochannel
 * Uses Linux specific "/proc/self/fd" interface. */
int nropen_iochannel(/*out*/size_t * number_open)
{
   int err ;
   int   fd       = iochannel_INIT_FREEABLE ;
   DIR * procself = 0 ;

   fd = open("/proc/self/fd", O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSCALL_ERRLOG("open(/proc/self/fd)", err) ;
      goto ONABORT ;
   }

   procself = fdopendir(fd) ;
   if (!procself) {
      err = errno ;
      TRACESYSCALL_ERRLOG("fdopendir", err) ;
      goto ONABORT ;
   }
   fd = iochannel_INIT_FREEABLE ;

   size_t            open_iocs = (size_t)0 ;
   struct dirent *   name ;
   for (;;) {
      ++ open_iocs ;
      errno = 0 ;
      name  = readdir(procself) ;
      if (!name) {
         if (errno) {
            err = errno ;
            goto ONABORT ;
         }
         break ;
      }
   }

   err = closedir(procself) ;
   procself = 0 ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("closedir", err) ;
      goto ONABORT ;
   }

   /* adapt open_iocs for
      1. counts one too high
      2. counts "."
      3. counts ".."
      4. counts fd
   */
   *number_open = open_iocs >= 4 ? open_iocs-4 : 0 ;

   return 0 ;
ONABORT:
   free_iochannel(&fd) ;
   if (procself) {
      closedir(procself) ;
   }
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

uint8_t accessmode_iochannel(const iochannel_t ioc)
{
   int err ;
   int flags ;

   flags = fcntl(ioc, F_GETFL) ;
   if (-1 == flags) {
      err = errno ;
      TRACESYSCALL_ERRLOG("fcntl", err) ;
      PRINTINT_ERRLOG(ioc) ;
      goto ONABORT ;
   }

   static_assert((O_RDONLY+1) == accessmode_READ, "simple conversion") ;
   static_assert((O_WRONLY+1) == accessmode_WRITE, "simple conversion") ;
   static_assert((O_RDWR+1)   == (accessmode_READ|accessmode_WRITE), "simple conversion") ;
   static_assert(O_ACCMODE    == (O_RDWR|O_WRONLY|O_RDONLY), "simple conversion") ;

   return (uint8_t) (1 + (flags & O_ACCMODE)) ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return accessmode_NONE ;
}

bool isvalid_iochannel(const iochannel_t ioc)
{
   int err ;
   err = fcntl(ioc, F_GETFD) ;
   return (-1 != err) ;
}

// group: I/O

int read_iochannel(iochannel_t ioc, size_t size, /*out*/void * buffer/*[size]*/, /*out*/size_t * bytes_read)
{
   int err ;
   ssize_t bytes ;

   for (;;) {
      bytes = read(ioc, buffer, size) ;

      if (-1 == bytes) {
         err = errno ;
         // non blocking io ?
         if (EAGAIN == err || EWOULDBLOCK == err) return EAGAIN ;
         // or interrupted ?
         if (EINTR == err) continue ;
         TRACESYSCALL_ERRLOG("read", err) ;
         PRINTINT_ERRLOG(ioc) ;
         PRINTSIZE_ERRLOG(size) ;
         goto ONABORT ;
      }

      break ;
   }

   if (bytes_read) {
      *bytes_read = (size_t) bytes ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int write_iochannel(iochannel_t ioc, size_t size, const void * buffer/*[size]*/, /*out*/size_t * bytes_written)
{
   int err ;
   ssize_t bytes ;

   VALIDATE_INPARAM_TEST(size <= SSIZE_MAX, ONABORT, ) ;

   for (;;) {
      bytes = write(ioc, buffer, size) ;

      if (-1 == bytes) {
         err = errno ;
         // non blocking io ?
         if (EAGAIN == err || EWOULDBLOCK == err) return EAGAIN ;
         if (EPIPE == err) return err ;
         // or interrupted ?
         if (EINTR == err) continue ;
         TRACESYSCALL_ERRLOG("write", err) ;
         PRINTINT_ERRLOG(ioc) ;
         PRINTSIZE_ERRLOG(size) ;
         goto ONABORT ;
      }

      break ;
   }

   if (bytes_written) {
      *bytes_written = (size_t) bytes ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_nropen(void)
{
   size_t  nropen ;
   size_t  nropen2 ;
   int     ioc[128] ;

   // prepare
   for (unsigned i = 0; i < lengthof(ioc); ++i) {
      ioc[i] = file_INIT_FREEABLE ;
   }

   // TEST nropen_iochannel: std file descriptors are open
   nropen = 0 ;
   TEST(0 == nropen_iochannel(&nropen)) ;
   TEST(3 <= nropen) ;

   // TEST nropen_iochannel: increment
   for (unsigned i = 0; i < lengthof(ioc); ++i) {
      ioc[i] = open("/dev/null", O_RDONLY|O_CLOEXEC) ;
      TEST(0 < ioc[i]) ;
      nropen2 = 0 ;
      TEST(0 == nropen_iochannel(&nropen2)) ;
      ++ nropen ;
      TEST(nropen == nropen2) ;
   }

   // TEST nropen_iochannel: decrement
   for (unsigned i = 0; i < lengthof(ioc); ++i) {
      TEST(0 == free_iochannel(&ioc[i])) ;
      nropen2 = 0 ;
      TEST(0 == nropen_iochannel(&nropen2)) ;
      -- nropen ;
      TEST(nropen == nropen2) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < lengthof(ioc); ++i) {
      free_iochannel(&ioc[i]) ;
   }
   return EINVAL ;
}

static int test_initfree(void)
{
   const int   N         = 20 ;  // next free file descriptor number
   iochannel_t ioc       = iochannel_INIT_FREEABLE ;
   iochannel_t pipeioc[] = { iochannel_INIT_FREEABLE, iochannel_INIT_FREEABLE } ;

   // TEST iochannel_INIT_FREEABLE
   TEST(-1 == sys_iochannel_INIT_FREEABLE) ;
   TEST(-1 == ioc) ;

   // TEST initcopy_iochannel
   static_assert(0 == iochannel_STDIN, "used in tests") ;
   TEST(0 == initcopy_iochannel(&ioc, iochannel_STDIN)) ;
   TEST(N == ioc) ;

   // TEST free_iochannel
   TEST(0 == free_iochannel(&ioc)) ;
   TEST(-1 == ioc) ;
   TEST(0 == free_iochannel(&ioc)) ;
   TEST(-1 == ioc) ;

   // TEST initcopy_iochannel: underlying data stream keeps open if another iochannel refs it
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   for (unsigned i = 0; i < 2; ++i) {
      uint8_t buffer[1] ;
      TEST(0 == initcopy_iochannel(&ioc, pipeioc[i])) ;
      TEST(N+2 == ioc) ;
      if (i) {
         TEST(1 == write(ioc, "1", 1)) ;
         TEST(1 == read(pipeioc[0], buffer, 1)) ;
      } else {
         TEST(1 == write(pipeioc[1], "1", 1)) ;
         TEST(1 == read(ioc, buffer, 1)) ;
      }
      TEST('1' == buffer[0]) ;
      TEST(0 == free_iochannel(&ioc)) ;
      TEST(-1 == ioc) ;
      TEST(1 == write(pipeioc[1], "2", 1)) ;
      TEST(1 == read(pipeioc[0], buffer, 1)) ;
      TEST('2' == buffer[0]) ;
   }
   TEST(0 == free_iochannel(&pipeioc[0])) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;

   return 0 ;
ONABORT:
   free_iochannel(&ioc) ;
   free_iochannel(&pipeioc[0]) ;
   free_iochannel(&pipeioc[1]) ;
   return EINVAL ;
}

static int test_query(directory_t * tempdir)
{
   iochannel_t ioc   = iochannel_INIT_FREEABLE ;
   iochannel_t pipeioc[] = { iochannel_INIT_FREEABLE, iochannel_INIT_FREEABLE } ;
   file_t      file  = file_INIT_FREEABLE ;
   ipsocket_t  ssock = ipsocket_INIT_FREEABLE ;
   ipsocket_t  csock = ipsocket_INIT_FREEABLE ;

   // TEST isfree_iochannel
   iochannel_t testioc[] =  { iochannel_STDIN, iochannel_STDOUT, iochannel_STDERR, 100, INT_MAX } ;
   for (unsigned i = 0; i < lengthof(testioc); ++i) {
      TEST(0 == isfree_iochannel(testioc[i])) ;
   }
   TEST(1 == isfree_iochannel(ioc)) ;

   // TEST isvalid_iochannel
   TEST(0 == isvalid_iochannel(file_INIT_FREEABLE)) ;
   TEST(0 == isvalid_iochannel(100)) ;
   TEST(0 == isvalid_iochannel(INT_MAX)) ;
   TEST(1 == isvalid_iochannel(file_STDIN)) ;
   TEST(1 == isvalid_iochannel(file_STDOUT)) ;
   TEST(1 == isvalid_iochannel(file_STDERR)) ;
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   for (unsigned i = 0; i < 2; ++i) {
      ioc = pipeioc[i] ;
      TEST(1 == isvalid_iochannel(pipeioc[i])) ;
      TEST(0 == free_iochannel(&pipeioc[i])) ;
      TEST(0 == isvalid_iochannel(ioc)) ;
   }

   // TEST accessmode_iochannel: predefined channels
   TEST(accessmode_NONE  == accessmode_iochannel(iochannel_INIT_FREEABLE)) ;
   TEST(accessmode_READ  == (accessmode_READ&accessmode_iochannel(iochannel_STDIN))) ;
   TEST(accessmode_WRITE == (accessmode_WRITE&accessmode_iochannel(iochannel_STDOUT))) ;
   TEST(accessmode_WRITE == (accessmode_WRITE&accessmode_iochannel(iochannel_STDERR))) ;

   // TEST accessmode_iochannel: pipe
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   for (unsigned i = 0; i < 2; ++i) {
      accessmode_e amode = i ? accessmode_WRITE : accessmode_READ ;
      TEST(amode == accessmode_iochannel(pipeioc[i])) ;
      ioc = pipeioc[i] ;
      TEST(0 == free_iochannel(&pipeioc[i])) ;
      TEST(accessmode_NONE == accessmode_iochannel(ioc)) ;
   }

   // TEST accessmode_iochannel: initcopy_iochannel
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   for (unsigned i = 0; i < 2; ++i) {
      accessmode_e amode = i ? accessmode_WRITE : accessmode_READ ;
      TEST(0 == initcopy_iochannel(&ioc, pipeioc[i]))
      TEST(amode == accessmode_iochannel(ioc)) ;
      TEST(0 == free_iochannel(&ioc)) ;
      TEST(amode == accessmode_iochannel(pipeioc[i])) ;
      TEST(0 == free_iochannel(&pipeioc[i])) ;
   }

   // TEST accessmode_iochannel: file_t
   TEST(0 == initcreate_file(&file, "accessmode", tempdir)) ;
   TEST(accessmode_RDWR == accessmode_iochannel(io_file(file))) ;
   TEST(0 == free_file(&file)) ;
   TEST(0 == init_file(&file, "accessmode", accessmode_READ, tempdir)) ;
   TEST(accessmode_READ == accessmode_iochannel(io_file(file))) ;
   TEST(0 == free_file(&file)) ;
   TEST(0 == init_file(&file, "accessmode", accessmode_WRITE, tempdir)) ;
   TEST(accessmode_WRITE == accessmode_iochannel(io_file(file))) ;
   TEST(0 == free_file(&file)) ;
   TEST(0 == init_file(&file, "accessmode", accessmode_RDWR, tempdir)) ;
   TEST(accessmode_RDWR == accessmode_iochannel(io_file(file))) ;
   TEST(0 == free_file(&file)) ;

   // TEST accessmode_iochannel: ipsocket_t
   ipaddr_storage_t ipaddr ;
   ipaddr_storage_t ipaddr2 ;
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_UDP, ipport_ANY, ipversion_4)) ;
   TEST(0 == init_ipsocket(&ssock, (ipaddr_t*)&ipaddr)) ;
   TEST(accessmode_RDWR == accessmode_iochannel(io_ipsocket(&ssock))) ;
   TEST(0 == free_ipsocket(&ssock)) ;
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_TCP, ipport_ANY, ipversion_4)) ;
   TEST(0 == initlisten_ipsocket(&ssock, (ipaddr_t*)&ipaddr, 1)) ;
   TEST(accessmode_RDWR == accessmode_iochannel(io_ipsocket(&ssock))) ;
   TEST(0 == localaddr_ipsocket(&ssock, (ipaddr_t*)&ipaddr)) ;
   TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_TCP, ipport_ANY, ipversion_4)) ;
   TEST(0 == initconnect_ipsocket(&csock, (const ipaddr_t*)&ipaddr, (const ipaddr_t*)&ipaddr2))
   TEST(accessmode_RDWR == accessmode_iochannel(io_ipsocket(&csock))) ;
   TEST(0 == free_ipsocket(&csock)) ;
   TEST(0 == free_ipsocket(&ssock)) ;

   return 0 ;
ONABORT:
   free_iochannel(&ioc) ;
   free_iochannel(&pipeioc[0]) ;
   free_iochannel(&pipeioc[1]) ;
   free_ipsocket(&ssock) ;
   free_ipsocket(&csock) ;
   free_file(&file) ;
   return EINVAL ;
}

static void sigusr1_handler(int signr)
{
   assert(signr == SIGUSR1) ;
   send_signalrt(0, 0) ;
}

static volatile size_t s_thread_count     = 0 ;
static volatile bool   s_thread_isrunning = 0 ;

typedef struct threadarg_t    threadarg_t ;

struct threadarg_t {
   iochannel_t ioc ;
   uint8_t *   buffer ;
   size_t      size ;
} ;

static int thread_reader(threadarg_t * arg)
{
   size_t   bytes_read ;
   size_t   value = 0 ;

   s_thread_isrunning = true ;

   while (s_thread_isrunning) {
      TEST(0 == read_iochannel(arg->ioc, arg->size, arg->buffer, &bytes_read)) ;
      TEST(bytes_read == arg->size) ;
      for (unsigned i = 0; i < arg->size; ++i, value += 61) {
         TEST(arg->buffer[i] == (uint8_t)value) ;
      }
      ++ s_thread_count ;
   }

   return 0 ;
ONABORT:
   s_thread_isrunning = false ;
   return EINVAL ;
}

static int thread_writer(threadarg_t * arg)
{
   size_t   bytes_written ;
   size_t   value = 0 ;

   s_thread_isrunning = true ;

   while (s_thread_isrunning) {
      for (unsigned i = 0; i < arg->size; ++i, value += 61) {
         arg->buffer[i] = (uint8_t)value ;
      }
      TEST(0 == write_iochannel(arg->ioc, arg->size, arg->buffer, &bytes_written)) ;
      TEST(bytes_written == arg->size) ;
      ++ s_thread_count ;
   }

   return 0 ;
ONABORT:
   s_thread_isrunning = false ;
   return EINVAL ;
}

static int thread_readerror(threadarg_t * arg)
{
   s_thread_isrunning = true ;

   int err = read_iochannel(arg->ioc, 1, arg->buffer, 0) ;
   ++ s_thread_count ;

   s_thread_isrunning = false ;
   return err ;
}

static int thread_writeerror(threadarg_t * arg)
{
   s_thread_isrunning = true ;

   int err = write_iochannel(arg->ioc, arg->size, arg->buffer, 0) ;
   ++ s_thread_count ;

   if (0 == err) {
      err = write_iochannel(arg->ioc, 1, arg->buffer, 0) ;
      ++ s_thread_count ;
   }

   s_thread_isrunning = false ;
   return err ;
}

static int test_readwrite(directory_t * tempdir)
{
   iochannel_t ioc       = iochannel_INIT_FREEABLE ;
   iochannel_t pipeioc[] = { iochannel_INIT_FREEABLE, iochannel_INIT_FREEABLE } ;
   thread_t *  thread    = 0 ;
   threadarg_t threadarg ;
   file_t      file      = file_INIT_FREEABLE ;
   ipsocket_t  ssock     = ipsocket_INIT_FREEABLE ;
   ipsocket_t  csock     = ipsocket_INIT_FREEABLE ;
   memblock_t  buffer[2] = { memblock_INIT_FREEABLE, memblock_INIT_FREEABLE } ;
   bool        isoldsignalmask = false ;
   sigset_t    oldsignalmask ;
   sigset_t    signalmask ;
   bool        isoldhandler = false ;
   struct
   sigaction   newact ;
   struct
   sigaction   oldact ;
   size_t      buffersize   = 0 ;
   size_t      bytes_read ;
   size_t      bytes_written ;

   // prepare
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &buffer[0])) ;
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &buffer[1])) ;
   TEST(0 == sigemptyset(&signalmask)) ;
   TEST(0 == sigaddset(&signalmask, SIGUSR1)) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK, &signalmask, &oldsignalmask)) ;
   isoldsignalmask = true ;
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags   = 0 ;
   newact.sa_handler = &sigusr1_handler ;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact)) ;
   isoldhandler = true ;
   // calculate size of pipe buffer
   {
      TEST(0 == pipe2(pipeioc, O_CLOEXEC|O_NONBLOCK)) ;
      ssize_t written ;
      while (0 < (written = write(pipeioc[1], buffer[0].addr, 1024*1024))) {
         buffersize += (size_t)written ;
      }
      TEST(-1 == write(pipeioc[1], buffer, 1)) ;
      TEST(EAGAIN == errno) ;
      TEST(0 == free_iochannel(&pipeioc[0])) ;
      TEST(0 == free_iochannel(&pipeioc[1])) ;
   }
   TEST(buffersize <= 1024*1204) ;

   // TEST read_iochannel: blocking I/O
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   s_thread_count = 0 ;
   threadarg = (threadarg_t) { pipeioc[0], buffer[1].addr, buffersize } ;
   TEST(0 == newgeneric_thread(&thread, &thread_reader, &threadarg)) ;
   while (!s_thread_isrunning) {
      yield_thread() ;
   }
   for (size_t value = 0, nrbuffer = 0; nrbuffer < 8; ++nrbuffer) {
      for (size_t i = 0; i < buffersize; ++i, value += 61) {
         buffer[0].addr[i] = (uint8_t)value ;
      }
      yield_thread() ;
      if (nrbuffer == 7) s_thread_isrunning = false ;
      TEST(s_thread_count == nrbuffer) ;
      TEST(buffersize == (size_t)write(pipeioc[1], buffer[0].addr, buffersize)) ;
      while (s_thread_count == nrbuffer) {
         yield_thread() ;
      }
      TEST(s_thread_count == (nrbuffer+1)) ;
   }
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == free_iochannel(&pipeioc[0])) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;

   // TEST write_iochannel: blocking I/O
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   s_thread_count = 0 ;
   threadarg = (threadarg_t) { pipeioc[1], buffer[1].addr, buffersize } ;
   TEST(0 == newgeneric_thread(&thread, &thread_writer, &threadarg)) ;
   while (!s_thread_isrunning) {
      yield_thread() ;
   }
   for (size_t value = 0, nrbuffer = 0; nrbuffer < 8; ++nrbuffer) {
      while (s_thread_count == nrbuffer) {
         yield_thread() ;
      }
      TEST(s_thread_count == (nrbuffer+1)) ;
      if (nrbuffer == 7) s_thread_isrunning = false ;
      bytes_read = 0 ;
      TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[0].addr, &bytes_read)) ;
      TEST(bytes_read == buffersize) ;
      for (unsigned i = 0; i < buffersize; ++i, value += 61) {
         TEST(buffer[0].addr[i] == (uint8_t)value) ;
      }
   }
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == free_iochannel(&pipeioc[0])) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;

   // TEST write_iochannel, read_iochannel: EBADF
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   // write on readonly and vice versa
   TEST(EBADF == read_iochannel(pipeioc[1], buffersize, buffer[0].addr, 0)) ;
   TEST(EBADF == write_iochannel(pipeioc[0], buffersize, buffer[0].addr, 0)) ;
   // wrong file descriptor value
   TEST(EBADF == read_iochannel(iochannel_INIT_FREEABLE, buffersize, buffer[0].addr, 0)) ;
   TEST(EBADF == write_iochannel(iochannel_INIT_FREEABLE, buffersize, buffer[0].addr, 0)) ;
   TEST(0 == free_iochannel(&pipeioc[0])) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;

   // TEST read_iochannel: end of input
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   TEST(0 == write_iochannel(pipeioc[1], buffersize, buffer[0].addr, &bytes_written)) ;
   TEST(bytes_written == buffersize) ;
   TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[0].addr, &bytes_read)) ;
   TEST(bytes_read == buffersize) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;
   for (unsigned i = 0; i < 100; ++i) {
      TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[0].addr, &bytes_read)) ;
      TEST(0 == bytes_read) ;
   }
   TEST(0 == free_iochannel(&pipeioc[0])) ;

   // TEST write_iochannel: EPIPE
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   TEST(0 == write_iochannel(pipeioc[1], buffersize, buffer[0].addr, &bytes_written)) ;
   TEST(bytes_written == buffersize) ;
   TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[0].addr, &bytes_read)) ;
   TEST(bytes_read == buffersize) ;
   TEST(0 == free_iochannel(&pipeioc[0])) ;
   TEST(EPIPE == write_iochannel(pipeioc[1], buffersize, buffer[0].addr, &bytes_written)) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;

   // TEST read_iochannel, write_iochannel: non blocking I/O (EAGAIN)
   TEST(0 == pipe2(pipeioc, O_CLOEXEC|O_NONBLOCK)) ;
   for (size_t nrbuffer = 0, wvalue = 0, rvalue = 0; nrbuffer < 8; ++nrbuffer) {
      for (size_t i = 0; i < buffersize; ++i, wvalue += 61) {
         buffer[0].addr[i] = (uint8_t)wvalue ;
      }
      TEST(0 == write_iochannel(pipeioc[1], buffersize, buffer[0].addr, &bytes_written)) ;
      TEST(bytes_written == buffersize) ;
      TEST(EAGAIN == write_iochannel(pipeioc[1], 1, buffer[0].addr, &bytes_written)) ;
      TEST(EAGAIN == write_iochannel(pipeioc[1], 1, buffer[0].addr, &bytes_written)) ;
      TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[1].addr, &bytes_read)) ;
      TEST(bytes_read == buffersize) ;
      for (size_t i = 0; i < buffersize; ++i, rvalue += 61) {
         TEST(buffer[1].addr[i] == (uint8_t)rvalue) ;
      }
      TEST(EAGAIN == read_iochannel(pipeioc[0], 1, buffer[1].addr, &bytes_read)) ;
      TEST(EAGAIN == read_iochannel(pipeioc[0], 1, buffer[1].addr, &bytes_read)) ;
   }
   TEST(0 == free_iochannel(&pipeioc[0])) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;

   // TEST read_iochannel: no EINTR !!
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   s_thread_count = 0 ;
   threadarg = (threadarg_t) { pipeioc[0], buffer[1].addr, buffersize } ;
   TEST(0 == newgeneric_thread(&thread, &thread_readerror, &threadarg)) ;
   while (!s_thread_isrunning) {
      yield_thread() ;
   }
   while (0 == trywait_signalrt(0, 0)) ;
   pthread_kill(thread->sys_thread, SIGUSR1) ;
   TEST(0 == wait_signalrt(0, 0)) ; // wait for SIGUSR1 processed
   TEST(0 == s_thread_count) ;
   TEST(1 == s_thread_isrunning) ;
   TEST(1 == write(pipeioc[1], buffer[0].addr, 1)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(1 == s_thread_count) ;
   TEST(0 == s_thread_isrunning) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == free_iochannel(&pipeioc[0])) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;

   // TEST write_iochannel: no EINTR !
   TEST(0 == pipe2(pipeioc, O_CLOEXEC)) ;
   s_thread_count = 0 ;
   threadarg = (threadarg_t) { pipeioc[1], buffer[1].addr, buffersize } ;
   TEST(0 == newgeneric_thread(&thread, &thread_writeerror, &threadarg)) ;
   while (!s_thread_isrunning || !s_thread_count) {
      yield_thread() ;
   }
   while (0 == trywait_signalrt(0, 0)) ;
   pthread_kill(thread->sys_thread, SIGUSR1) ;
   TEST(0 == wait_signalrt(0, 0)) ; // wait for SIGUSR1 processed
   TEST(1 == s_thread_count) ;
   TEST(1 == s_thread_isrunning) ;
   TEST(buffersize == (size_t)read(pipeioc[0], buffer[0].addr, buffersize)) ;
   TEST(0 == join_thread(thread)) ;
   TEST(2 == s_thread_count) ;
   TEST(0 == s_thread_isrunning) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(0 == free_iochannel(&pipeioc[0])) ;
   TEST(0 == free_iochannel(&pipeioc[1])) ;

   // TEST read_iochannel, write_iochannel: file_t
   TEST(0 == initcreate_file(&file, "readtest", tempdir)) ;
   TEST(0 == free_file(&file)) ;
   for (size_t nrbuffer = 0, wvalue = 0, rvalue = 0; nrbuffer < 8; ++nrbuffer) {
      for (size_t i = 0; i < buffersize; ++i, wvalue += 61) {
         buffer[0].addr[i] = (uint8_t)wvalue ;
      }
      TEST(0 == init_file(&file, "readtest", accessmode_WRITE, tempdir)) ;
      TEST(0 == write_iochannel(io_file(file), buffersize, buffer[0].addr, &bytes_written)) ;
      TEST(bytes_written == buffersize) ;
      TEST(0 == free_file(&file)) ;
      TEST(0 == init_file(&file, "readtest", accessmode_READ, tempdir)) ;
      TEST(0 == read_iochannel(io_file(file), buffersize, buffer[1].addr, &bytes_read)) ;
      TEST(bytes_read == buffersize) ;
      TEST(0 == free_file(&file)) ;
      for (size_t i = 0; i < buffersize; ++i, rvalue += 61) {
         buffer[1].addr[i] = (uint8_t)rvalue ;
      }
   }

   // TEST read_iochannel, write_iochannel: ipsocket_t
   ipaddr_storage_t ipaddr ;
   ipaddr_storage_t ipaddr2 ;
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_TCP, ipport_ANY, ipversion_4)) ;
   TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_TCP, ipport_ANY, ipversion_4)) ;
   TEST(0 == initlisten_ipsocket(&ssock, (ipaddr_t*)&ipaddr, 1)) ;
   TEST(0 == localaddr_ipsocket(&ssock, (ipaddr_t*)&ipaddr)) ;
   TEST(0 == initconnect_ipsocket(&csock, (const ipaddr_t*)&ipaddr, (const ipaddr_t*)&ipaddr2))
   ipsocket_t lsock = ssock ;
   TEST(0 == initwaitconnect_ipsocket(&ssock, &lsock, 0)) ;
   TEST(0 == free_ipsocket(&lsock)) ;
   for (size_t nrbuffer = 0, wvalue = 0, rvalue = 0; nrbuffer < 8; ++nrbuffer) {
      for (size_t i = 0; i < buffersize; ++i, wvalue += 61) {
         buffer[0].addr[i] = (uint8_t)wvalue ;
      }
      TEST(0 == write_iochannel(io_ipsocket(&ssock), buffersize, buffer[0].addr, &bytes_written)) ;
      TEST(bytes_written == buffersize) ;
      TEST(0 == read_iochannel(io_ipsocket(&csock), buffersize, buffer[1].addr, &bytes_read)) ;
      if (bytes_read < buffersize) {
         size_t extra ;
         TEST(0 == read_iochannel(io_ipsocket(&csock), buffersize - bytes_read, buffer[1].addr, &extra)) ;
         bytes_read += extra ;
      }
      TEST(bytes_read == buffersize) ;
      for (size_t i = 0; i < buffersize; ++i, rvalue += 61) {
         buffer[1].addr[i] = (uint8_t)rvalue ;
      }
   }
   TEST(0 == shutdown(io_ipsocket(&csock), SHUT_RD)) ;
   TEST(0 == shutdown(io_ipsocket(&ssock), SHUT_WR)) ;
   TEST(EPIPE == write_iochannel(io_ipsocket(&ssock), buffersize, buffer[0].addr, &bytes_written)) ;
   TEST(0 == read_iochannel(io_ipsocket(&csock), buffersize, buffer[1].addr, &bytes_read)) ;
   TEST(0 == bytes_read) ;
   TEST(0 == free_ipsocket(&csock)) ;
   TEST(0 == free_ipsocket(&ssock)) ;

   // unprepare
   TEST(0 == RELEASE_PAGECACHE(&buffer[0])) ;
   TEST(0 == RELEASE_PAGECACHE(&buffer[1])) ;
   isoldsignalmask = false ;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0)) ;
   isoldhandler = false ;
   TEST(0 == sigaction(SIGUSR1, &oldact, 0)) ;

   return 0 ;
ONABORT:
   if (isoldsignalmask) {
      sigprocmask(SIG_SETMASK, &oldsignalmask, 0) ;
   }
   if (isoldhandler) {
      sigaction(SIGUSR1, &oldact, 0) ;
   }
   free_iochannel(&ioc) ;
   free_iochannel(&pipeioc[0]) ;
   free_iochannel(&pipeioc[1]) ;
   free_ipsocket(&ssock) ;
   free_ipsocket(&csock) ;
   free_file(&file) ;
   delete_thread(&thread) ;
   RELEASE_PAGECACHE(&buffer[0]) ;
   RELEASE_PAGECACHE(&buffer[1]) ;
   return EINVAL ;
}

int unittest_io_iochannel()
{
   resourceusage_t   usage      = resourceusage_INIT_FREEABLE ;
   unsigned          open_count = 0 ;
   directory_t *     tempdir    = 0 ;
   iochannel_t       ioc[20] ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == newtemp_directory(&tempdir, "iochanneltest")) ;

   {
      size_t nrfdopen ;
      TEST(0 == nropen_iochannel(&nrfdopen)) ;
      while (nrfdopen < lengthof(ioc)) {
         TEST(0 == initcopy_iochannel(&ioc[open_count], iochannel_STDOUT)) ;
         ++ open_count ;
         ++ nrfdopen ;
      }
   }

   if (test_nropen())            goto ONABORT ;
   if (test_initfree())          goto ONABORT ;
   if (test_query(tempdir))      goto ONABORT ;
   if (test_readwrite(tempdir))  goto ONABORT ;

   while (open_count) {
      TEST(0 == free_iochannel(&ioc[--open_count])) ;
   }

   TEST(0 == delete_directory(&tempdir)) ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   while (open_count) {
      (void) free_iochannel(&ioc[--open_count]) ;
   }
   delete_directory(&tempdir) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
