/* title: Filedescriptor Linux
   Implements <Filedescriptor> on Linux.

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

   file: C-kern/api/io/filedescr.h
    Header file <Filedescriptor>.

   file: C-kern/platform/Linux/io/filedescr.c
    Linux specific implementation <Filedescriptor Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/thread.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: filedescr_t

// group: lifetime

int free_filedescr(filedescr_t * fd)
{
   int err ;
   int del_fd = *fd ;

   if (isinit_filedescr(del_fd)) {
      *fd = sys_filedescr_INIT_FREEABLE ;

      err = close(del_fd) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("close", err) ;
         PRINTINT_LOG(del_fd) ;
      }

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

accessmode_e accessmode_filedescr(filedescr_t fd)
{
   int err ;
   int flags ;

   flags = fcntl(fd, F_GETFL) ;
   if (-1 == flags) {
      err = errno ;
      TRACESYSERR_LOG("fcntl", err) ;
      PRINTINT_LOG(fd) ;
      goto ONABORT ;
   }

   static_assert( (O_RDONLY+1) == accessmode_READ, "simple conversion") ;
   static_assert( (O_WRONLY+1) == accessmode_WRITE, "simple conversion") ;
   static_assert( (O_RDWR+1)   == (accessmode_READ|accessmode_WRITE), "simple conversion") ;
   static_assert( O_ACCMODE    == (O_RDWR|O_WRONLY|O_RDONLY), "simple conversion") ;

   return (accessmode_e) (1 + (flags & O_ACCMODE)) ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return accessmode_NONE ;
}

/* function: isopen_filedescr
 * Uses Linux specific "/proc/self/fd" interface. */
bool isopen_filedescr(filedescr_t fd)
{
   int err ;

   err = fcntl(fd, F_GETFD) ;
   if (-1 == err) {
      return false ;
   }

   return true ;
}

/* function: nropen_filedescr
 * Uses Linux specific "/proc/self/fd" interface. */
int nropen_filedescr(/*out*/size_t * number_open_fd)
{
   int err ;
   int fd = -1 ;
   DIR * procself = 0 ;

   fd = open( "/proc/self/fd", O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("open(/proc/self/fd)", err) ;
      goto ONABORT ;
   }

   procself = fdopendir(fd) ;
   if (!procself) {
      err = errno ;
      TRACESYSERR_LOG("fdopendir", err) ;
      goto ONABORT ;
   }
   fd = -1 ;

   size_t         open_fds = (size_t)0 ;
   struct dirent  * name ;
   for(;;) {
      ++ open_fds ;
      errno = 0 ;
      name  = readdir( procself ) ;
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
      TRACESYSERR_LOG("closedir", err) ;
      goto ONABORT ;
   }

   /* adapt open_fds for
      1. counts one too high
      2. counts "."
      3. counts ".."
      4. counts fd opened in init_directorystream
   */
   *number_open_fd = open_fds >= 4 ? open_fds-4 : 0 ;

   return 0 ;
ONABORT:
   (void) free_filedescr(&fd) ;
   if (procself) {
      closedir(procself) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: io

int read_filedescr(filedescr_t fd, size_t buffer_size, /*out*/uint8_t buffer[buffer_size], size_t * bytes_read)
{
   int err ;
   ssize_t bytes ;
   size_t  total_read = 0 ;

   do {
      do {
         bytes = read( fd, buffer + total_read, buffer_size - total_read) ;
      } while( -1 == bytes && EINTR == errno ) ;
      if (-1 == bytes) {
         if (total_read) break ;
         err = errno ;
         // non blocking io ?
         if (EAGAIN == err || EWOULDBLOCK == err) return EAGAIN ;
         TRACESYSERR_LOG("read", err) ;
         PRINTINT_LOG(fd) ;
         PRINTSIZE_LOG(buffer_size) ;
         goto ONABORT ;
      }
      total_read += (size_t) bytes ;
      assert(total_read <= buffer_size) ;
   } while( bytes && total_read < buffer_size) ;

   if (bytes_read) {
      *bytes_read = total_read ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int write_filedescr(filedescr_t fd, size_t buffer_size, const void * buffer, size_t * bytes_written)
{
   int err ;
   ssize_t bytes ;
   size_t  total_written = 0 ;

   do {
      do {
         bytes = write( fd, (const uint8_t*)buffer + total_written, buffer_size - total_written) ;
      } while( -1 == bytes && EINTR == errno ) ;
      if (-1 == bytes) {
         if (total_written) break ;
         err = errno ;
         // non blocking io ?
         if (EAGAIN == err || EWOULDBLOCK == err) return EAGAIN ;
         TRACESYSERR_LOG("write", err) ;
         PRINTINT_LOG(fd) ;
         PRINTSIZE_LOG(buffer_size) ;
         goto ONABORT ;
      }
      total_written += (size_t) bytes ;
      assert(total_written <= buffer_size) ;
   } while( bytes && total_written < buffer_size) ;

   if (bytes_written) {
      *bytes_written = total_written ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

static int test_query(void)
{
   size_t  openfd ;
   int     fds[128] = { -1 } ;

   // TEST std file descriptors are open
   openfd = 0 ;
   TEST(0 == nropen_filedescr(&openfd)) ;
   TEST(3 <= openfd) ;

   // TEST increment
   for(unsigned i = 0; i < nrelementsof(fds); ++i) {
      fds[i] = open("/dev/null", O_RDONLY|O_CLOEXEC) ;
      TEST(fds[i] > 0) ;
      size_t openfd2 = 0 ;
      TEST(0 == nropen_filedescr(&openfd2)) ;
      ++ openfd ;
      TEST(openfd == openfd2) ;
   }

   // TEST decrement
   for(unsigned i = 0; i < nrelementsof(fds); ++i) {
      TEST(0 == free_filedescr(&fds[i])) ;
      TEST(-1 == fds[i]) ;
      size_t openfd2 = 0 ;
      TEST(0 == nropen_filedescr(&openfd2)) ;
      -- openfd ;
      TEST(openfd == openfd2) ;
   }

   return 0 ;
ONABORT:
   for(unsigned i = 0; i < nrelementsof(fds); ++i) {
      free_filedescr(&fds[i]) ;
   }
   return EINVAL ;
}

static int test_initfree(directory_t * tempdir)
{
   filedescr_t fd = filedescr_INIT_FREEABLE ;
   size_t      openfd ;
   size_t      openfd2 ;

   TEST(0 == makefile_directory(tempdir, "testfile", 1)) ;

   // TEST static init
   TEST(-1 == fd) ;
   TEST(0  == filedescr_STDIN) ;
   TEST(1  == filedescr_STDOUT) ;
   TEST(2  == filedescr_STDERR) ;

   // TEST double free
   TEST(0 == nropen_filedescr(&openfd)) ;
   fd = openat(fd_directory(tempdir), "testfile", O_WRONLY|O_CLOEXEC) ;
   TEST(0 < fd) ;
   TEST(0 == nropen_filedescr(&openfd2)) ;
   TEST(openfd2 == openfd + 1) ;
   TEST(0 == free_filedescr(&fd)) ;
   TEST(-1 == fd) ;
   TEST(0 == nropen_filedescr(&openfd2)) ;
   TEST(openfd2 == openfd) ;
   TEST(0 == free_filedescr(&fd)) ;
   TEST(-1 == fd) ;
   TEST(0 == nropen_filedescr(&openfd2)) ;
   TEST(openfd2 == openfd) ;

   // TEST isinit
   fd = filedescr_INIT_FREEABLE ;
   TEST(0 == isinit_filedescr(fd)) ;
   fd = filedescr_STDIN ;
   TEST(1 == isinit_filedescr(fd)) ;
   fd = filedescr_STDOUT ;
   TEST(1 == isinit_filedescr(fd)) ;
   fd = filedescr_STDERR ;
   TEST(1 == isinit_filedescr(fd)) ;

   // TEST isopen
   TEST(0 == isopen_filedescr(filedescr_INIT_FREEABLE)) ;
   TEST(0 == isopen_filedescr(100)) ;
   TEST(1 == isopen_filedescr(filedescr_STDIN)) ;
   TEST(1 == isopen_filedescr(filedescr_STDOUT)) ;
   TEST(1 == isopen_filedescr(filedescr_STDERR)) ;

   // TEST accessmode accessmode_READ
   fd = openat(fd_directory(tempdir), "testfile", O_RDONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   TEST(accessmode_READ == accessmode_filedescr(fd)) ;
   TEST(accessmode_WRITE != accessmode_filedescr(fd)) ;
   TEST((accessmode_WRITE|accessmode_READ) != accessmode_filedescr(fd)) ;
   TEST(0 == free_filedescr(&fd)) ;
   TEST(-1 == fd) ;

   // TEST accessmode accessmode_WRITE
   fd = openat(fd_directory(tempdir), "testfile", O_WRONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   TEST(accessmode_WRITE == accessmode_filedescr(fd)) ;
   TEST(accessmode_READ != accessmode_filedescr(fd)) ;
   TEST((accessmode_WRITE|accessmode_READ) != accessmode_filedescr(fd)) ;
   TEST(0 == free_filedescr(&fd)) ;
   TEST(-1 == fd) ;

   // TEST accessmode accessmode_READ+accessmode_WRITE
   fd = openat(fd_directory(tempdir), "testfile", O_RDWR|O_CLOEXEC) ;
   int fd2 = fd ;
   TEST(fd > 0) ;
   TEST((accessmode_WRITE|accessmode_READ) == accessmode_filedescr(fd)) ;
   TEST(accessmode_WRITE != accessmode_filedescr(fd)) ;
   TEST(accessmode_READ != accessmode_filedescr(fd)) ;
   TEST(0 == free_filedescr(&fd)) ;
   TEST(-1 == fd) ;

   // TEST accessmode_NONE
   TEST(accessmode_NONE == accessmode_filedescr(fd2)) ;
   TEST(accessmode_NONE == accessmode_filedescr(fd)) ;


   TEST(0 == removefile_directory(tempdir, "testfile")) ;
   return 0 ;
ONABORT:
   (void) free_filedescr(&fd) ;
   (void) removefile_directory(tempdir, "testfile") ;
   return EINVAL ;
}

typedef struct thread_arg_t   thread_arg_t ;

struct thread_arg_t {
   thread_t    * caller ;
   int         fd ;
} ;

static int thread_reader(thread_arg_t * startarg)
{
   int err ;
   size_t  bytes_read = 0 ;
   uint8_t byte = 0 ;
   resume_thread(startarg->caller) ;
   err = read_filedescr(startarg->fd, 1, &byte, &bytes_read) ;
   return (err != 0) || (bytes_read != 1) || (byte != 200) ;
}

static int thread_writer(thread_arg_t * startarg)
{
   int err ;
   size_t  bytes_written = 0 ;
   uint8_t byte = 200 ;
   resume_thread(startarg->caller) ;
   err = write_filedescr(startarg->fd, 1, &byte, &bytes_written) ;
   return (err != 0) || (bytes_written != 1)  ;
}

static int thread_writer2(thread_arg_t * startarg)
{
   int err ;
   uint8_t buffer[2] = { 1, 2 } ;
   resume_thread(startarg->caller) ;
   err = write_filedescr(startarg->fd, sizeof(buffer), buffer, 0) ;
   CLEARBUFFER_LOG() ;
   return (err != EPIPE) ;
}

static int        s_siguser_count  = 0 ;
static thread_t * s_siguser_thread = 0 ;

static void siguser(int signr)
{
   assert(SIGUSR1 == signr) ;
   if (s_siguser_count) {
      assert(s_siguser_thread == self_thread()) ;
   } else {
      s_siguser_thread = self_thread() ;
   }
   ++ s_siguser_count ;
}

static int test_readwrite(directory_t * tempdir)
{
   int               fd        = -1 ;
   int               pipefd[2] = { -1 } ;
   uint8_t           * buffer  = 0 ;
   thread_t          * thread  = 0 ;
   uint8_t           byte ;
   size_t            bytes_read ;
   size_t            bytes_written ;
   size_t            pipe_buffersize ;
   sigset_t          oldset ;
   bool              isOldact  = false ;
   struct sigaction  newact ;
   struct sigaction  oldact ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "readwrite1", sizeof(buffer))) ;

   // TEST blocking write
   TEST(0 < (fd = openat(fd_directory(tempdir), "readwrite1", O_WRONLY|O_CLOEXEC))) ;
   for(unsigned i = 0; i < 10000; ++i) {
      byte = (uint8_t) i ;
      bytes_written = 0 ;
      TEST(0 == write_filedescr(fd, 1, &byte, &bytes_written)) ;
      TEST(1 == bytes_written) ;
   }
   TEST(0 == free_filedescr(&fd)) ;

   // TEST blocking read
   TEST(0 < (fd = openat(fd_directory(tempdir), "readwrite1", O_RDONLY|O_CLOEXEC))) ;
   for(unsigned i = 0; i < 10000-1; ++i) {
      byte = (uint8_t) (1+i) ;
      bytes_read = 0 ;
      TEST(0 == read_filedescr(fd, 1, &byte, &bytes_read)) ;
      TEST(1 == bytes_read) ;
      TEST((uint8_t)i == byte) ;
   }
   TEST(0 == read_filedescr(fd, 2, &byte, &bytes_read)) ;
   TEST(1 == bytes_read) ;
   TEST(0 == read_filedescr(fd, 1, &byte, &bytes_read)) ;
   TEST(0 == bytes_read /*end of file*/) ;
   TEST(0 == free_filedescr(&fd)) ;

   // TEST write non blocking mode
   TEST(0 == pipe2(pipefd, O_NONBLOCK|O_CLOEXEC)) ;
   for(bytes_written = 0; ;++ bytes_written) {
      size_t bytes_written2 = 0 ;
      byte = (uint8_t)bytes_written ;
      int err = write_filedescr(pipefd[1], 1, &byte, &bytes_written2) ;
      if (err) {
         TEST(EAGAIN == err) ;
         break ;
      }
      TEST(1 == bytes_written2) ;
   }
   TEST(-1 == write(pipefd[1], &byte, 1)) ;
   TEST(EAGAIN == errno) ;
   bytes_read = 2 ;
   TEST(EAGAIN == write_filedescr(pipefd[1], 1, &byte, &bytes_read)) ;
   TEST(2 == bytes_read /*has not changed*/) ;
   pipe_buffersize = bytes_written ;

   // TEST read non blocking mode
   TEST(0 != (buffer = (uint8_t*) malloc(100 + bytes_written))) ;
   TEST(0 == read_filedescr(pipefd[0], 100 + bytes_written, buffer, &bytes_read)) ;
   TEST(bytes_written == bytes_read) ;
   TEST(-1 == read(pipefd[0], &byte, 1)) ;
   TEST(EAGAIN == errno) ;
   TEST(EAGAIN == read_filedescr(pipefd[0], 1, &byte, &bytes_read)) ;
   TEST(bytes_written == bytes_read /*has not changed*/) ;
   free(buffer) ;
   buffer = 0 ;

   // TEST read with interrupts
   TEST(0 == free_filedescr(&pipefd[0])) ;
   TEST(0 == free_filedescr(&pipefd[1])) ;
   TEST(0 == pipe2(pipefd, O_CLOEXEC)) ;
   TEST(0 == sigemptyset(&newact.sa_mask)) ;
   TEST(0 == sigaddset(&newact.sa_mask, SIGUSR1)) ;
   TEST(0 == sigprocmask(SIG_UNBLOCK, &newact.sa_mask, &oldset)) ;
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags   = 0 ;
   newact.sa_handler = &siguser ;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact)) ;
   isOldact = true ;
   thread_arg_t startarg = { .caller = self_thread(), .fd = pipefd[0] } ;
   TEST(0 == new_thread(&thread, thread_reader, &startarg)) ;
   suspend_thread() ;
   sleepms_thread(100) ;
   s_siguser_count  = 0 ;
   s_siguser_thread = 0 ;
   for(int i = 0; i < 50; ++i) {
      pthread_kill(thread->sys_thread, SIGUSR1) ;
      sleepms_thread(5) ;
   }
   byte = 200 ;
   TEST(0 == write_filedescr(pipefd[1], 1, &byte, &bytes_written)) ;
   TEST(1 == bytes_written) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(50 == s_siguser_count) ;
   TEST(thread == s_siguser_thread) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST write with interrupts
   for(size_t i = 0; i < pipe_buffersize; ++i) {
      byte = 0 ;
      TEST(0 == write_filedescr(pipefd[1], 1, &byte, 0)) ;
   }
   startarg.fd = pipefd[1] ;
   TEST(0 == new_thread(&thread, thread_writer, &startarg)) ;
   suspend_thread() ;
   sleepms_thread(100) ;
   s_siguser_count  = 0 ;
   s_siguser_thread = 0 ;
   for(int i = 0; i < 50; ++i) {
      pthread_kill(thread->sys_thread, SIGUSR1) ;
      sleepms_thread(5) ;
   }
   for(size_t i = 0; i < pipe_buffersize; ++i) {
      byte = 1 ;
      TEST(0 == read_filedescr(pipefd[0], 1, &byte, &bytes_read)) ;
      TEST(1 == bytes_read) ;
      TEST(0 == byte) ;
   }
   TEST(0 == read_filedescr(pipefd[0], 1, &byte, &bytes_read)) ;
   TEST(1 == bytes_read) ;
   TEST(200 == byte) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(50 == s_siguser_count) ;
   TEST(thread == s_siguser_thread) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST EPIPE, write with receiving end closed during write
   TEST(0 == free_filedescr(&pipefd[0])) ;
   TEST(0 == free_filedescr(&pipefd[1])) ;
   TEST(0 == pipe2(pipefd, O_CLOEXEC)) ;
   for(size_t i = 0; i < pipe_buffersize-1; ++i) {
      byte = 0 ;
      TEST(0 == write_filedescr(pipefd[1], 1, &byte, 0)) ;
   }
   startarg = (thread_arg_t) { .caller = self_thread(), .fd = pipefd[1] } ;
   // thread_writer2 writes 2 bytes and therefore waits
   // (pipe buffer can only hold 1 more byte)
   TEST(0 == new_thread(&thread, thread_writer2, &startarg)) ;
   suspend_thread() ;
   sleepms_thread(100) ;
   TEST(0 == free_filedescr(&pipefd[0])) ;
   TEST(0 == join_thread(thread)) ;
   // check that thread_writer2 encountered EPIPE
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(EPIPE == write_filedescr(pipefd[1], 1, &byte, &bytes_written)) ;
   TEST(EPIPE == write_filedescr(pipefd[1], 1, &byte, &bytes_written)) ;
   TEST(0 == free_filedescr(&pipefd[1])) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "readwrite1")) ;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldset, 0)) ;
   isOldact = 0 ;
   TEST(0 == sigaction(SIGUSR1, &oldact, 0)) ;

   return 0 ;
ONABORT:
   if (isOldact) {
      sigprocmask(SIG_SETMASK, &oldset, 0) ;
      sigaction(SIGUSR1, &oldact, 0) ;
   }
   delete_thread(&thread) ;
   free(buffer) ;
   removefile_directory(tempdir, "readwrite1") ;
   free_filedescr(&fd) ;
   free_filedescr(&pipefd[0]) ;
   free_filedescr(&pipefd[1]) ;
   return EINVAL ;
}

int unittest_io_filedescr()
{
   resourceusage_t   usage     = resourceusage_INIT_FREEABLE ;
   cstring_t         tmppath   = cstring_INIT ;
   directory_t       * tempdir = 0 ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == newtemp_directory(&tempdir, "iofdtest", &tmppath)) ;

   if (test_query())             goto ONABORT ;
   if (test_initfree(tempdir))   goto ONABORT ;
   if (test_readwrite(tempdir))  goto ONABORT ;

   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_cstring(&tmppath) ;
   (void) delete_directory(&tempdir) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
