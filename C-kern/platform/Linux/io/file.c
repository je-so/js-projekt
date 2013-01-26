/* title: File Linux
   Implements <File> on Linux.

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

   file: C-kern/api/io/filesystem/file.h
    Header file <File>.

   file: C-kern/platform/Linux/io/file.c
    Linux specific implementation <File Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/thread.h"
#include "C-kern/api/string/cstring.h"
#include <sys/statvfs.h>
#endif


// section: file_t

// group: functions

/* function: nropen_file
 * Uses Linux specific "/proc/self/fd" interface. */
int nropen_file(/*out*/size_t * number_open_fd)
{
   int err ;
   int fd = -1 ;
   DIR * procself = 0 ;

   fd = open("/proc/self/fd", O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC) ;
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
   for (;;) {
      ++ open_fds ;
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
   (void) free_file(&fd) ;
   if (procself) {
      closedir(procself) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_file(const char * filepath, struct directory_t * relative_to)
{
   int err ;
   int unlinkatfd = AT_FDCWD ;

   if (relative_to) {
      unlinkatfd = fd_directory(relative_to) ;
   }

   err = unlinkat(unlinkatfd, filepath, 0) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("unlinkat(unlinkatfd, filepath)", err) ;
      PRINTINT_LOG(unlinkatfd) ;
      PRINTCSTR_LOG(filepath) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: lifetime

int init_file(/*out*/file_t * fileobj, const char* filepath, accessmode_e iomode, const struct directory_t * relative_to)
{
   int err ;
   int fd       = -1 ;
   int openatfd = AT_FDCWD ;

   VALIDATE_INPARAM_TEST(iomode, ONABORT, ) ;
   VALIDATE_INPARAM_TEST(0 == (iomode & ~((unsigned)accessmode_RDWR)), ONABORT, ) ;

   if (relative_to) {
      openatfd = fd_directory(relative_to) ;
   }

   static_assert( (O_RDONLY+1) == accessmode_READ, "simple conversion") ;
   static_assert( (O_WRONLY+1) == accessmode_WRITE, "simple conversion") ;
   static_assert( (O_RDWR+1)   == (accessmode_READ|accessmode_WRITE), "simple conversion") ;

   fd = openat(openatfd, filepath, ((int)iomode - 1)|O_CLOEXEC ) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("openat", err) ;
      PRINTCSTR_LOG(filepath) ;
      goto ONABORT ;
   }

   *fileobj = fd ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int initappend_file(/*out*/file_t * fileobj, const char* filepath, const struct directory_t * relative_to/*0 => current working dir*/)
{
   int err ;
   int fd       = -1 ;
   int openatfd = AT_FDCWD ;

   if (relative_to) {
      openatfd = fd_directory(relative_to) ;
   }

   fd = openat(openatfd, filepath, O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("openat", err) ;
      PRINTINT_LOG(openatfd) ;
      PRINTCSTR_LOG(filepath) ;
      goto ONABORT ;
   }

   *fileobj = fd ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int initcreate_file(/*out*/file_t * fileobj, const char* filepath, const struct directory_t * relative_to)
{
   int err ;
   int fd       = -1 ;
   int openatfd = AT_FDCWD ;

   if (relative_to) {
      openatfd = fd_directory(relative_to) ;
   }

   fd = openat(openatfd, filepath, O_RDWR|O_EXCL|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("openat", err) ;
      PRINTINT_LOG(openatfd) ;
      PRINTCSTR_LOG(filepath) ;
      goto ONABORT ;
   }

   *fileobj = fd ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_file(file_t * fileobj)
{
   int err ;
   int close_fd = *fileobj ;

   if (isinit_file(close_fd)) {
      *fileobj = file_INIT_FREEABLE ;

      err = close(close_fd) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("close", err) ;
         PRINTINT_LOG(close_fd) ;
      }

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

accessmode_e accessmode_file(const file_t fileobj)
{
   int err ;
   int flags ;

   flags = fcntl(fileobj, F_GETFL) ;
   if (-1 == flags) {
      err = errno ;
      TRACESYSERR_LOG("fcntl", err) ;
      PRINTINT_LOG(fileobj) ;
      goto ONABORT ;
   }

   static_assert((O_RDONLY+1) == accessmode_READ, "simple conversion") ;
   static_assert((O_WRONLY+1) == accessmode_WRITE, "simple conversion") ;
   static_assert((O_RDWR+1)   == (accessmode_READ|accessmode_WRITE), "simple conversion") ;
   static_assert(O_ACCMODE    == (O_RDWR|O_WRONLY|O_RDONLY), "simple conversion") ;

   return (accessmode_e) (1 + (flags & O_ACCMODE)) ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return accessmode_NONE ;
}

/* function: isopen_file
 * Uses fcntl to query file descriptor flags (FD_CLOEXEC). */
bool isopen_file(file_t fileobj)
{
   int err ;

   err = fcntl(fileobj, F_GETFD) ;

   return (-1 != err) ;
}

int size_file(const file_t fileobj, /*out*/off_t * file_size)
{
   int err ;
   struct stat stat_result ;

   err = fstat(fileobj, &stat_result) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("fstat", err) ;
      PRINTINT_LOG(fileobj) ;
      goto ONABORT ;
   }

   *file_size = stat_result.st_size ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: io

int read_file(file_t fileobj, size_t buffer_size, /*out*/void * buffer/*[buffer_size]*/, size_t * bytes_read)
{
   int err ;
   ssize_t bytes ;
   size_t  total_read = 0 ;

   do {
      do {
         bytes = read(fileobj, (uint8_t*)buffer + total_read, buffer_size - total_read) ;
      } while (-1 == bytes && EINTR == errno) ;

      if (-1 == bytes) {
         if (total_read) break ;
         err = errno ;
         // non blocking io ?
         if (EAGAIN == err || EWOULDBLOCK == err) return EAGAIN ;
         TRACESYSERR_LOG("read", err) ;
         PRINTINT_LOG(fileobj) ;
         PRINTSIZE_LOG(buffer_size) ;
         goto ONABORT ;
      }

      total_read += (size_t) bytes ;
      assert(total_read <= buffer_size) ;
   } while (/*not closed*/bytes && total_read < buffer_size) ;

   if (bytes_read) {
      *bytes_read = total_read ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int write_file(file_t fileobj, size_t buffer_size, const void * buffer/*[buffer_size]*/, size_t * bytes_written)
{
   int err ;
   ssize_t bytes ;
   size_t  total_written = 0 ;

   while (total_written < buffer_size) {
      /* buffer_size > 0 */
      do {
         bytes = write(fileobj, (const uint8_t*)buffer + total_written, buffer_size - total_written) ;
      } while (-1 == bytes && EINTR == errno) ;
      if (-1 == bytes) {
         if (total_written) break ;
         err = errno ;
         // non blocking io ?
         if (EAGAIN == err || EWOULDBLOCK == err) return EAGAIN ;
         TRACESYSERR_LOG("write", err) ;
         PRINTINT_LOG(fileobj) ;
         PRINTSIZE_LOG(buffer_size) ;
         goto ONABORT ;
      }
      total_written += (size_t) bytes ;
      assert(bytes && total_written <= buffer_size) ;
   }

   if (bytes_written) {
      *bytes_written = total_written ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int advisereadahead_file(file_t fileobj, off_t offset, off_t length)
{
   int err ;

   err = posix_fadvise(fileobj, offset, length, POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED) ;
   if (err) {
      TRACESYSERR_LOG("posix_fadvise", err) ;
      PRINTINT_LOG(fileobj) ;
      PRINTINT64_LOG(offset) ;
      PRINTINT64_LOG(length) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int advisedontneed_file(file_t fileobj, off_t offset, off_t length)
{
   int err ;

   err = posix_fadvise(fileobj, offset, length, POSIX_FADV_DONTNEED) ;
   if (err) {
      TRACESYSERR_LOG("posix_fadvise", err) ;
      PRINTINT_LOG(fileobj) ;
      PRINTINT64_LOG(offset) ;
      PRINTINT64_LOG(length) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: allocation

int truncate_file(file_t fileobj, off_t file_size)
{
   int err ;

   err = ftruncate(fileobj, file_size) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("ftruncate", err) ;
      PRINTINT_LOG(fileobj) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int allocate_file(file_t fileobj, off_t file_size)
{
   int err ;

   err = fallocate(fileobj, 0/*adapt file size*/, 0, file_size) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("fallocate", err) ;
      PRINTINT_LOG(fileobj) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_nropen(void)
{
   size_t  openfd ;
   size_t  openfd2 ;
   int     fds[128] ;

   // prepare
   for (unsigned i = 0; i < lengthof(fds); ++i) {
      fds[i] = file_INIT_FREEABLE ;
   }

   // TEST nropen_file: std file descriptors are open
   openfd = 0 ;
   TEST(0 == nropen_file(&openfd)) ;
   TEST(3 <= openfd) ;

   // TEST nropen_file: increment
   for (unsigned i = 0; i < lengthof(fds); ++i) {
      fds[i] = open("/dev/null", O_RDONLY|O_CLOEXEC) ;
      TEST(0 < fds[i]) ;
      openfd2 = 0 ;
      TEST(0 == nropen_file(&openfd2)) ;
      ++ openfd ;
      TEST(openfd == openfd2) ;
   }

   // TEST nropen_file: decrement
   for (unsigned i = 0; i < lengthof(fds); ++i) {
      TEST(0 == free_file(&fds[i])) ;
      TEST(fds[i] == file_INIT_FREEABLE) ;
      openfd2 = 0 ;
      TEST(0 == nropen_file(&openfd2)) ;
      -- openfd ;
      TEST(openfd == openfd2) ;
   }

   return 0 ;
ONABORT:
   for (unsigned i = 0; i < lengthof(fds); ++i) {
      free_file(&fds[i]) ;
   }
   return EINVAL ;
}

static int test_remove(directory_t * tempdir)
{
   file_t   file = file_INIT_FREEABLE ;
   off_t    filesize ;

   // TEST remove_file
   for (unsigned i = 0; i < 10; ++i) {
      // create
      const size_t datasize = i * 1000 ;
      TEST(0 == makefile_directory(tempdir, "remove", datasize)) ;
      TEST(0 == filesize_directory(tempdir, "remove", &filesize)) ;
      TEST(filesize == datasize) ;
      // now remove
      TEST(0 == checkpath_directory(tempdir, "remove")) ;
      TEST(0 == remove_file("remove", tempdir)) ;
      TEST(ENOENT == checkpath_directory(tempdir, "remove")) ;
   }

   return 0 ;
ONABORT:
   free_file(&file) ;
   return EINVAL ;
}

static int test_query(directory_t * tempdir)
{
   file_t fd  = file_INIT_FREEABLE ;
   file_t fd2 = file_INIT_FREEABLE ;
   int    pipefd[2] = { file_INIT_FREEABLE, file_INIT_FREEABLE } ;
   off_t  filesize ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "testfile", 1)) ;

   // TEST static init
   TEST(0 == file_STDIN) ;
   TEST(1 == file_STDOUT) ;
   TEST(2 == file_STDERR) ;

   // TEST isinit_file
   fd = file_INIT_FREEABLE ;
   TEST(0 == isinit_file(fd)) ;
   fd = file_STDIN ;
   TEST(1 == isinit_file(fd)) ;
   fd = file_STDOUT ;
   TEST(1 == isinit_file(fd)) ;
   fd = file_STDERR ;
   TEST(1 == isinit_file(fd)) ;

   // TEST isopen_file
   TEST(0 == isopen_file(file_INIT_FREEABLE)) ;
   TEST(0 == isopen_file(100)) ;
   TEST(1 == isopen_file(file_STDIN)) ;
   TEST(1 == isopen_file(file_STDOUT)) ;
   TEST(1 == isopen_file(file_STDERR)) ;

   // TEST accessmode_file: accessmode_READ
   fd = openat(fd_directory(tempdir), "testfile", O_RDONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   fd2 = dup(fd) ;
   TEST(fd2 > 0) ;
   TEST(accessmode_READ == accessmode_file(fd)) ;
   TEST(accessmode_READ == accessmode_file(fd2)) ;
   TEST(0 == free_file(&fd)) ;
   TEST(0 == free_file(&fd2)) ;
   TEST(fd == file_INIT_FREEABLE) ;
   TEST(fd2 == file_INIT_FREEABLE) ;

   // TEST accessmode_file: accessmode_WRITE
   fd = openat(fd_directory(tempdir), "testfile", O_WRONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   fd2 = dup(fd) ;
   TEST(fd2 > 0) ;
   TEST(accessmode_WRITE == accessmode_file(fd)) ;
   TEST(accessmode_WRITE == accessmode_file(fd2)) ;
   TEST(0 == free_file(&fd)) ;
   TEST(0 == free_file(&fd2)) ;
   TEST(fd == file_INIT_FREEABLE) ;
   TEST(fd2 == file_INIT_FREEABLE) ;

   // TEST accessmode_file: accessmode_RDWR
   fd = openat(fd_directory(tempdir), "testfile", O_RDWR|O_CLOEXEC) ;
   fd2 = fd ;
   TEST(fd > 0) ;
   TEST(accessmode_RDWR == accessmode_file(fd)) ;
   TEST(accessmode_RDWR == accessmode_file(fd2)) ;
   TEST(0 == free_file(&fd)) ;
   TEST(fd == file_INIT_FREEABLE) ;

   // TEST accessmode_file: accessmode_NONE
   TEST(accessmode_NONE == accessmode_file(fd2)) ;
   TEST(accessmode_NONE == accessmode_file(fd2)) ;
   TEST(accessmode_NONE == accessmode_file(fd)) ;
   TEST(accessmode_NONE == accessmode_file(fd)) ;
   fd2 = file_INIT_FREEABLE ;

   // TEST size_file: regular file
   TEST(0 == initappend_file(&fd, "testfilesize", tempdir)) ;
   filesize = 1 ;
   TEST(0 == size_file(fd, &filesize)) ;
   TEST(0 == filesize) ;
   for (unsigned i = 1; i <= 256; ++i) {
      uint8_t  buffer[257] ;
      memset(buffer, 3, sizeof(buffer)) ;
      TEST(0 == write_file(fd, sizeof(buffer), buffer, 0)) ;
      TEST(0 == size_file(fd, &filesize)) ;
      TEST(filesize == sizeof(buffer)*i) ;
      filesize = 0 ;
      TEST(0 == init_file(&fd2, "testfilesize", accessmode_READ, tempdir)) ;
      TEST(0 == size_file(fd2, &filesize)) ;
      TEST(0 == free_file(&fd2)) ;
      TEST(filesize == sizeof(buffer)*i) ;
      filesize = 0 ;
      TEST(0 == filesize_directory(tempdir, "testfilesize", &filesize)) ;
      TEST(filesize == sizeof(buffer)*i) ;
   }
   TEST(0 == free_file(&fd)) ;

   // TEST size_file: pipe
   TEST(0 == pipe2(pipefd, O_NONBLOCK|O_CLOEXEC)) ;
   TEST(0 == size_file(pipefd[0], &filesize)) ;
   TEST(0 == filesize) ;
   TEST(0 == size_file(pipefd[1], &filesize)) ;
   TEST(0 == filesize) ;
   TEST(0 == free_file(&pipefd[0])) ;
   TEST(0 == free_file(&pipefd[1])) ;

   // TEST EBADF
   TEST(EBADF == size_file(file_INIT_FREEABLE, &filesize)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testfile")) ;
   TEST(0 == removefile_directory(tempdir, "testfilesize")) ;

   return 0 ;
ONABORT:
   free_file(&fd) ;
   free_file(&fd2) ;
   free_file(&pipefd[0]) ;
   free_file(&pipefd[1]) ;
   removefile_directory(tempdir, "testfile") ;
   return EINVAL ;
}

static int test_initfree(directory_t * tempdir)
{
   file_t   file = file_INIT_FREEABLE ;
   size_t   nropenfd ;
   size_t   nropenfd2 ;

   // TEST static init
   TEST(-1 == file) ;
   TEST(!isinit_file(file)) ;

   // TEST init_file, free_file
   accessmode_e modes[3] = {  accessmode_READ,  accessmode_WRITE, accessmode_RDWR } ;
   TEST(0 == makefile_directory(tempdir, "init1", 1999)) ;
   TEST(0 == checkpath_directory(tempdir, "init1")) ;
   TEST(0 == nropen_file(&nropenfd)) ;
   for (unsigned i = 0; i < lengthof(modes); ++i) {
      TEST(0 == init_file(&file, "init1", modes[i], tempdir)) ;
      TEST(modes[i] == accessmode_file(file)) ;
      TEST(isinit_file(file)) ;
      TEST(0 == nropen_file(&nropenfd2)) ;
      TEST(nropenfd+1 == nropenfd2) ;
      TEST(0 == free_file(&file)) ;
      TEST(file == file_INIT_FREEABLE) ;
      TEST(!isinit_file(file)) ;
      TEST(0 == nropen_file(&nropenfd2)) ;
      TEST(nropenfd == nropenfd2) ;
      TEST(0 == free_file(&file)) ;
      TEST(file == file_INIT_FREEABLE) ;
      TEST(0 == nropen_file(&nropenfd2)) ;
      TEST(nropenfd == nropenfd2) ;
   }
   TEST(0 == removefile_directory(tempdir, "init1")) ;

   // TEST initcreate_file, free_file
   TEST(ENOENT == checkpath_directory(tempdir, "init2")) ;
   TEST(0 == initcreate_file(&file, "init2", tempdir)) ;
   TEST(accessmode_RDWR == accessmode_file(file)) ;
   TEST(isinit_file(file)) ;
   TEST(0 == nropen_file(&nropenfd2)) ;
   TEST(nropenfd+1 == nropenfd2) ;
   TEST(0 == checkpath_directory(tempdir, "init2")) ;
   TEST(0 == free_file(&file)) ;
   TEST(file == file_INIT_FREEABLE) ;
   TEST(0 == nropen_file(&nropenfd2)) ;
   TEST(nropenfd == nropenfd2) ;
   TEST(0 == free_file(&file)) ;
   TEST(file == file_INIT_FREEABLE) ;
   TEST(0 == nropen_file(&nropenfd2)) ;
   TEST(nropenfd == nropenfd2) ;
   TEST(0 == removefile_directory(tempdir, "init2")) ;

   // TEST initappend_file, free_file
   TEST(ENOENT == checkpath_directory(tempdir, "init3")) ;
   TEST(0 == initappend_file(&file, "init3", tempdir)) ;
   TEST(accessmode_WRITE == accessmode_file(file)) ;
   TEST(isinit_file(file)) ;
   TEST(0 == nropen_file(&nropenfd2)) ;
   TEST(nropenfd+1 == nropenfd2) ;
   TEST(0 == checkpath_directory(tempdir, "init3")) ;
   TEST(0 == free_file(&file)) ;
   TEST(file == file_INIT_FREEABLE) ;
   TEST(0 == nropen_file(&nropenfd2)) ;
   TEST(nropenfd == nropenfd2) ;
   TEST(0 == free_file(&file)) ;
   TEST(file == file_INIT_FREEABLE) ;
   TEST(0 == nropen_file(&nropenfd2)) ;
   TEST(nropenfd == nropenfd2) ;
   TEST(0 == removefile_directory(tempdir, "init3")) ;

   // TEST initmove_file
   for (int i = 0; i < 100; ++i) {
      file_t destfile   = file_INIT_FREEABLE ;
      file_t sourcefile = i ;
      TEST(isinit_file(sourcefile)) ;
      initmove_file(&destfile, &sourcefile) ;
      TEST(!isinit_file(sourcefile)) /*source is reset*/ ;
      TEST(destfile == i) /*dest contains former value of source*/ ;
   }

   // TEST initmove_file: move to itself does not work !
   file = 0 ;
   TEST(isinit_file(file)) ;
   initmove_file(&file, &file) ;
   TEST(!isinit_file(file)) ;

   // TEST EEXIST
   TEST(0 == makefile_directory(tempdir, "init1", 0)) ;
   TEST(EEXIST == initcreate_file(&file, "init1", tempdir)) ;
   TEST(file == file_INIT_FREEABLE) ;
   TEST(0 == removefile_directory(tempdir, "init1")) ;

   // TEST EINVAL
   TEST(0 == makefile_directory(tempdir, "init1", 0)) ;
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_EXEC, tempdir)) ;
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_PRIVATE, tempdir)) ;
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_SHARED, tempdir)) ;
   TEST(0 == removefile_directory(tempdir, "init1")) ;

   return 0 ;
ONABORT:
   free_file(&file) ;
   removefile_directory(tempdir, "init1") ;
   removefile_directory(tempdir, "init2") ;
   return EINVAL ;
}

static int compare_file_content(directory_t * tempdir, const char * filename, unsigned times)
{
   file_t file = file_INIT_FREEABLE ;

   TEST(0 == init_file(&file, filename, accessmode_READ, tempdir)) ;

   for (unsigned t = 0; t < times; ++t) {
      uint8_t buffer[256] = { 0 } ;
      size_t  bread = 0 ;
      TEST(0 == read_file(file, 256, buffer, &bread)) ;
      TEST(256 == bread) ;
      for (unsigned i = 0; i < 256; ++i) {
         TEST(i == buffer[i]) ;
      }
   }

   TEST(0 == free_file(&file)) ;

   return 0 ;
ONABORT:
   free_file(&file) ;
   return EINVAL ;
}

static int test_create(directory_t * tempdir)
{
   file_t   file = file_INIT_FREEABLE ;
   size_t   bwritten ;
   uint8_t  buffer[256] ;
   off_t    size ;

   // prepare
   for (unsigned i = 0; i < 256; ++i) {
      buffer[i] = (uint8_t) i ;
   }

   // TEST initcreate_file: file does not exist
   TEST(ENOENT == checkpath_directory(tempdir, "testcreate")) ;
   TEST(0 == initcreate_file(&file, "testcreate", tempdir)) ;
   TEST(0 == filesize_directory(tempdir, "testcreate", &size)) ;
   TEST(0 == size) ;
   TEST(0 == write_file(file, 256, buffer, &bwritten)) ;
   TEST(256 == bwritten) ;
   TEST(0 == filesize_directory(tempdir, "testcreate", &size)) ;
   TEST(256 == size) ;
   TEST(0 == free_file(&file)) ;
   TEST(file == file_INIT_FREEABLE) ;
   if (compare_file_content(tempdir, "testcreate", 1)) goto ONABORT ;

   // TEST initcreate_file: EEXIST
   TEST(0 == checkpath_directory(tempdir, "testcreate")) ;
   TEST(EEXIST == initcreate_file(&file, "testcreate", tempdir)) ;
   TEST(0 == filesize_directory(tempdir, "testcreate", &size)) ;
   TEST(256 == size) ;
   TEST(file == file_INIT_FREEABLE) ;
   if (compare_file_content(tempdir, "testcreate", 1)) goto ONABORT ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testcreate")) ;

   return 0 ;
ONABORT:
   free_file(&file) ;
   removefile_directory(tempdir, "testcreate") ;
   return EINVAL ;
}

static int test_append(directory_t * tempdir)
{
   file_t   file = file_INIT_FREEABLE ;
   size_t   bwritten ;
   uint8_t  buffer[256] ;
   off_t    size ;

   // prepare
   for (unsigned i = 0; i < 256; ++i) {
      buffer[i] = (uint8_t) i ;
   }

   // TEST initappend_file: file does not exist
   TEST(ENOENT == checkpath_directory(tempdir, "testappend")) ;
   TEST(0 == initappend_file(&file, "testappend", tempdir)) ;
   TEST(0 == filesize_directory(tempdir, "testappend", &size)) ;
   TEST(0 == size) ;
   TEST(0 == write_file(file, 256, buffer, &bwritten)) ;
   TEST(256 == bwritten) ;
   TEST(0 == filesize_directory(tempdir, "testappend", &size)) ;
   TEST(256 == size) ;
   TEST(0 == free_file(&file)) ;
   TEST(file == file_INIT_FREEABLE) ;
   if (compare_file_content(tempdir, "testappend", 1)) goto ONABORT ;

   // TEST initappend_file: file already exists
   TEST(0 == checkpath_directory(tempdir, "testappend")) ;
   TEST(0 == initappend_file(&file, "testappend", tempdir)) ;
   TEST(0 == filesize_directory(tempdir, "testappend", &size)) ;
   TEST(256 == size) ;
   TEST(0 == write_file(file, 256, buffer, &bwritten)) ;
   TEST(256 == bwritten) ;
   TEST(0 == filesize_directory(tempdir, "testappend", &size)) ;
   TEST(512 == size) ;
   TEST(0 == free_file(&file)) ;
   TEST(file == file_INIT_FREEABLE) ;
   if (compare_file_content(tempdir, "testappend", 2)) goto ONABORT ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testappend")) ;

   return 0 ;
ONABORT:
   free_file(&file) ;
   removefile_directory(tempdir, "testappend") ;
   return EINVAL ;
}

// test read / write with interrupts

typedef struct thread_arg_t   thread_arg_t ;

struct thread_arg_t {
   thread_t    * caller ;
   file_t      fd ;
} ;

static int thread_reader(thread_arg_t * startarg)
{
   int err ;
   size_t  bytes_read = 0 ;
   uint8_t byte = 0 ;
   resume_thread(startarg->caller) ;
   err = read_file(startarg->fd, 1, &byte, &bytes_read) ;
   return (err != 0) || (bytes_read != 1) || (byte != 200) ;
}

static int thread_writer(thread_arg_t * startarg)
{
   int err ;
   size_t  bytes_written = 0 ;
   uint8_t byte = 200 ;
   resume_thread(startarg->caller) ;
   err = write_file(startarg->fd, 1, &byte, &bytes_written) ;
   return (err != 0) || (bytes_written != 1)  ;
}

static int thread_writer2(thread_arg_t * startarg)
{
   int err ;
   uint8_t buffer[2] = { 1, 2 } ;
   resume_thread(startarg->caller) ;
   err = write_file(startarg->fd, sizeof(buffer), buffer, 0) ;
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
   file_t            fd        = file_INIT_FREEABLE ;
   file_t            pipefd[2] = { file_INIT_FREEABLE, file_INIT_FREEABLE } ;
   memblock_t        buffer    = memblock_INIT_FREEABLE ;
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

   // TEST write_file: blocking write
   TEST(0 < (fd = openat(fd_directory(tempdir), "readwrite1", O_WRONLY|O_CLOEXEC))) ;
   for (unsigned i = 0; i < 10000; ++i) {
      byte = (uint8_t) i ;
      bytes_written = 0 ;
      TEST(0 == write_file(fd, 1, &byte, &bytes_written)) ;
      TEST(1 == bytes_written) ;
   }
   TEST(0 == free_file(&fd)) ;

   // TEST read_file: blocking read
   TEST(0 < (fd = openat(fd_directory(tempdir), "readwrite1", O_RDONLY|O_CLOEXEC))) ;
   for (unsigned i = 0; i < 10000-1; ++i) {
      byte = (uint8_t) (1+i) ;
      bytes_read = 0 ;
      TEST(0 == read_file(fd, 1, &byte, &bytes_read)) ;
      TEST(1 == bytes_read) ;
      TEST((uint8_t)i == byte) ;
   }
   TEST(0 == read_file(fd, 2, &byte, &bytes_read)) ;
   TEST(1 == bytes_read) ;
   TEST(0 == read_file(fd, 1, &byte, &bytes_read)) ;
   TEST(0 == bytes_read /*end of file*/) ;
   TEST(0 == free_file(&fd)) ;

   // TEST write_file: write non blocking mode
   TEST(0 == pipe2(pipefd, O_NONBLOCK|O_CLOEXEC)) ;
   for (bytes_written = 0; ;++ bytes_written) {
      size_t bytes_written2 = 0 ;
      byte = (uint8_t)bytes_written ;
      int err = write_file(pipefd[1], 1, &byte, &bytes_written2) ;
      if (err) {
         TEST(EAGAIN == err) ;
         break ;
      }
      TEST(1 == bytes_written2) ;
   }
   TEST(-1 == write(pipefd[1], &byte, 1)) ;
   TEST(EAGAIN == errno) ;
   bytes_read = 2 ;
   TEST(EAGAIN == write_file(pipefd[1], 1, &byte, &bytes_read)) ;
   TEST(2 == bytes_read /*has not changed*/) ;
   pipe_buffersize = bytes_written ;

   // TEST read_file: read non blocking mode
   TEST(0 == RESIZE_MM(100 + bytes_written, &buffer)) ;
   TEST(0 == read_file(pipefd[0], 100 + bytes_written, buffer.addr, &bytes_read)) ;
   TEST(bytes_written == bytes_read) ;
   TEST(-1 == read(pipefd[0], &byte, 1)) ;
   TEST(EAGAIN == errno) ;
   TEST(EAGAIN == read_file(pipefd[0], 1, &byte, &bytes_read)) ;
   TEST(bytes_written == bytes_read /*has not changed*/) ;
   TEST(0 == FREE_MM(&buffer)) ;

   // TEST read_file: read with interrupts
   TEST(0 == free_file(&pipefd[0])) ;
   TEST(0 == free_file(&pipefd[1])) ;
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
   for (int i = 0; i < 50; ++i) {
      pthread_kill(thread->sys_thread, SIGUSR1) ;
      sleepms_thread(5) ;
   }
   byte = 200 ;
   TEST(0 == write_file(pipefd[1], 1, &byte, &bytes_written)) ;
   TEST(1 == bytes_written) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(50 == s_siguser_count) ;
   TEST(thread == s_siguser_thread) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST write with interrupts
   for (size_t i = 0; i < pipe_buffersize; ++i) {
      byte = 0 ;
      TEST(0 == write_file(pipefd[1], 1, &byte, 0)) ;
   }
   startarg.fd = pipefd[1] ;
   TEST(0 == new_thread(&thread, thread_writer, &startarg)) ;
   suspend_thread() ;
   sleepms_thread(100) ;
   s_siguser_count  = 0 ;
   s_siguser_thread = 0 ;
   for (int i = 0; i < 50; ++i) {
      pthread_kill(thread->sys_thread, SIGUSR1) ;
      sleepms_thread(5) ;
   }
   for (size_t i = 0; i < pipe_buffersize; ++i) {
      byte = 1 ;
      TEST(0 == read_file(pipefd[0], 1, &byte, &bytes_read)) ;
      TEST(1 == bytes_read) ;
      TEST(0 == byte) ;
   }
   TEST(0 == read_file(pipefd[0], 1, &byte, &bytes_read)) ;
   TEST(1 == bytes_read) ;
   TEST(200 == byte) ;
   TEST(0 == join_thread(thread)) ;
   TEST(0 == returncode_thread(thread)) ;
   TEST(50 == s_siguser_count) ;
   TEST(thread == s_siguser_thread) ;
   TEST(0 == delete_thread(&thread)) ;

   // TEST write_file: EPIPE, write with receiving end closed during write
   TEST(0 == free_file(&pipefd[0])) ;
   TEST(0 == free_file(&pipefd[1])) ;
   TEST(0 == pipe2(pipefd, O_CLOEXEC)) ;
   for (size_t i = 0; i < pipe_buffersize-1; ++i) {
      byte = 0 ;
      TEST(0 == write_file(pipefd[1], 1, &byte, 0)) ;
   }
   startarg = (thread_arg_t) { .caller = self_thread(), .fd = pipefd[1] } ;
   // thread_writer2 writes 2 bytes and therefore waits
   // (pipe buffer can only hold 1 more byte)
   TEST(0 == new_thread(&thread, thread_writer2, &startarg)) ;
   suspend_thread() ;
   sleepms_thread(100) ;
   TEST(0 == free_file(&pipefd[0])) ;
   TEST(0 == join_thread(thread)) ;
   // check that thread_writer2 encountered EPIPE
   TEST(0 == returncode_thread(thread)) ;
   TEST(0 == delete_thread(&thread)) ;
   TEST(EPIPE == write_file(pipefd[1], 1, &byte, &bytes_written)) ;
   TEST(EPIPE == write_file(pipefd[1], 1, &byte, &bytes_written)) ;
   TEST(0 == free_file(&pipefd[1])) ;

   // TEST read_file: returns bytes read == 0 if writer closed pipe
   TEST(0 == pipe2(pipefd, O_CLOEXEC)) ;
   TEST(0 == free_file(&pipefd[1])) ;
   bytes_read = 1 ;
   TEST(0 == read_file(pipefd[0], 1, &byte, &bytes_read)) ;
   TEST(0 == bytes_read) ;
   TEST(0 == free_file(&pipefd[0])) ;

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
   FREE_MM(&buffer) ;
   removefile_directory(tempdir, "readwrite1") ;
   free_file(&fd) ;
   free_file(&pipefd[0]) ;
   free_file(&pipefd[1]) ;
   return EINVAL ;
}

static int test_allocate(directory_t * tempdir)
{
   file_t   file  = file_INIT_FREEABLE ;
   file_t   file2 = file_INIT_FREEABLE ;
   int      pipefd[2] = { file_INIT_FREEABLE, file_INIT_FREEABLE } ;
   size_t   bwritten ;
   size_t   bread ;
   uint32_t buffer[1024] ;
   uint32_t buffer2[1024] ;
   off_t    size ;

   // prepare
   TEST(0 == pipe2(pipefd, O_CLOEXEC)) ;
   for (uint32_t i = 0; i < lengthof(buffer); ++i) {
      buffer[i] = i ;
   }

   // TEST truncate_file: shrink file
   TEST(ENOENT == checkpath_directory(tempdir, "testallocate")) ;
   TEST(0 == initcreate_file(&file, "testallocate", tempdir)) ;
   TEST(0 == filesize_directory(tempdir, "testallocate", &size)) ;
   TEST(0 == size) ;
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(0 == write_file(file, sizeof(buffer), buffer, &bwritten)) ;
      TEST(bwritten == sizeof(buffer)) ;
      TEST(0 == size_file(file, &size)) ;
      TEST(size == sizeof(buffer)*i) ;
   }
   for (unsigned i = 256; i >= 1; --i) {
      TEST(0 == truncate_file(file, sizeof(buffer)*(i-1))) ;
      TEST(0 == size_file(file, &size)) ;
      TEST(size == sizeof(buffer)*(i-1)) ;
      TEST(0 == init_file(&file2, "testallocate", accessmode_READ, tempdir)) ;
      for (unsigned i2 = 1; i2 < i; i2++) {
         memset(buffer2, 1, sizeof(buffer2)) ;
         TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread)) ;
         TEST(bread == sizeof(buffer2)) ;
         TEST(0 == memcmp(buffer, buffer2, sizeof(buffer))) ;
      }
      TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread)) ;
      TEST(0 == bread/*end of input*/) ;
      TEST(0 == free_file(&file2)) ;
   }

   // TEST truncate_file: grow file with 0 bytes
   memset(buffer, 0, sizeof(buffer)) ;
   TEST(0 == truncate_file(file, 11/*support sizes smaller than blocksize*/)) ;
   TEST(0 == size_file(file, &size)) ;
   TEST(size == 11) ;
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(0 == truncate_file(file, sizeof(buffer)*i)) ;
      TEST(0 == size_file(file, &size)) ;
      TEST(size == sizeof(buffer)*i) ;
      TEST(0 == init_file(&file2, "testallocate", accessmode_READ, tempdir)) ;
      for (unsigned i2 = 0; i2 < i; i2++) {
         memset(buffer2, 1, sizeof(buffer2)) ;
         TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread)) ;
         TEST(bread == sizeof(buffer2)) ;
         TEST(0 == memcmp(buffer, buffer2, sizeof(buffer))) ;
      }
      TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread)) ;
      TEST(0 == bread/*end of input*/) ;
      TEST(0 == free_file(&file2)) ;
   }
   TEST(0 == free_file(&file)) ;

   // TEST allocate_file: grow
   TEST(0 == removefile_directory(tempdir, "testallocate")) ;
   TEST(0 == initcreate_file(&file, "testallocate", tempdir)) ;
   TEST(0 == allocate_file(file, 12/*support sizes smaller than blocksize*/)) ;
   TEST(0 == size_file(file, &size)) ;
   TEST(size == 12) ;
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(0 == allocate_file(file, sizeof(buffer)*i)) ;
      TEST(0 == size_file(file, &size)) ;
      TEST(size == sizeof(buffer)*i) ;
      TEST(0 == init_file(&file2, "testallocate", accessmode_READ, tempdir)) ;
      for (unsigned i2 = 0; i2 < i; i2++) {
         memset(buffer2, 1, sizeof(buffer2)) ;
         TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread)) ;
         TEST(bread == sizeof(buffer2)) ;
         TEST(0 == memcmp(buffer, buffer2, sizeof(buffer))) ;
      }
      TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread)) ;
      TEST(0 == bread/*end of input*/) ;
      TEST(0 == free_file(&file2)) ;
   }

   // TEST allocate_file: no shrink possible
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(0 == allocate_file(file, sizeof(buffer)*i)) ;
      TEST(0 == size_file(file, &size)) ;
      TEST(size == sizeof(buffer)*256) ;
   }
   TEST(0 == free_file(&file)) ;

   // TEST allocate_file: free disk blocks really allocated on file system
   TEST(0 == removefile_directory(tempdir, "testallocate")) ;
   TEST(0 == initcreate_file(&file, "testallocate", tempdir)) ;
   struct statvfs statvfs_result1 ;
   struct statvfs statvfs_result2 ;
   TEST(0 == fstatvfs(file, &statvfs_result1)) ;
   TEST(0 == allocate_file(file, statvfs_result1.f_frsize * 10000)) ;
   TEST(0 == fstatvfs(file, &statvfs_result2)) ;
   TEST(statvfs_result2.f_bfree + 10000 <= statvfs_result1.f_bfree) ;
   TEST(0 == truncate_file(file, 0)) ;
   TEST(0 == fstatvfs(file, &statvfs_result2)) ;
   TEST(statvfs_result2.f_bfree + 100 >= statvfs_result1.f_bfree) ;
   TEST(0 == free_file(&file)) ;

   // TEST truncate_file: free disk blocks are not allocated on file system
   TEST(0 == removefile_directory(tempdir, "testallocate")) ;
   TEST(0 == initcreate_file(&file, "testallocate", tempdir)) ;
   TEST(0 == fstatvfs(file, &statvfs_result1)) ;
   TEST(0 == truncate_file(file, statvfs_result1.f_frsize * 10000)) ;
   TEST(0 == fstatvfs(file, &statvfs_result2)) ;
   TEST(statvfs_result2.f_bfree + 100 >= statvfs_result1.f_bfree) ;
   TEST(0 == free_file(&file)) ;

   // TEST EINVAL
   // resize pipe
   TEST(EINVAL == truncate_file(pipefd[1], 4096))
   // read only
   TEST(0 == init_file(&file, "testallocate", accessmode_READ, tempdir)) ;
   TEST(EINVAL == truncate_file(file, 4096))
   TEST(0 == free_file(&file)) ;
   // negative size
   TEST(0 == init_file(&file, "testallocate", accessmode_RDWR, tempdir)) ;
   TEST(EINVAL == truncate_file(file, -4096))
   TEST(EINVAL == allocate_file(file, -4096))
   TEST(0 == free_file(&file)) ;

   // TEST ESPIPE (illegal seek)
   // resize pipe
   TEST(ESPIPE == allocate_file(pipefd[1], 4096))

   // TEST EBADF
   // read only
   TEST(0 == init_file(&file, "testallocate", accessmode_READ, tempdir)) ;
   TEST(EBADF == allocate_file(file, 4096)) ;
   int oldfile = file ;
   TEST(0 == free_file(&file)) ;
   // closed file
   TEST(EBADF == truncate_file(oldfile, 4096))
   TEST(EBADF == allocate_file(oldfile, 4096))
   // invalid file descriptor
   TEST(EBADF == truncate_file(file_INIT_FREEABLE, 4096))
   TEST(EBADF == allocate_file(file_INIT_FREEABLE, 4096))

   // TEST ENOSPC
   TEST(0 == init_file(&file, "testallocate", accessmode_RDWR, tempdir)) ;
   TEST(0 == fstatvfs(file, &statvfs_result1)) ;
   TEST(ENOSPC == allocate_file(file, (off_t) (statvfs_result1.f_frsize * (1+statvfs_result1.f_bavail)))) ;
   TEST(0 == free_file(&file)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testallocate")) ;
   TEST(0 == free_file(&pipefd[0])) ;
   TEST(0 == free_file(&pipefd[1])) ;

   return 0 ;
ONABORT:
   free_file(&file) ;
   free_file(&file2) ;
   free_file(&pipefd[0]) ;
   free_file(&pipefd[1]) ;
   removefile_directory(tempdir, "testallocate") ;
   return EINVAL ;
}

static int test_advise(directory_t * tempdir)
{
   file_t         fd     = file_INIT_FREEABLE ;
   uint8_t        buffer[256] ;
   size_t         bytes_read ;
   size_t         bytes_written ;
   const size_t   filesize = 1024*1024 ;

   // prepare
   TEST(0 == makefile_directory(tempdir, "advise1", filesize)) ;
   TEST(0 == init_file(&fd, "advise1", accessmode_WRITE, tempdir)) ;
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t)i ;
   }
   for (unsigned i = 0; i < filesize; i += sizeof(buffer)) {
      buffer[0] = (uint8_t) (i/sizeof(buffer)) ;
      TEST(0 == write_file(fd, sizeof(buffer), buffer, &bytes_written)) ;
      TEST(bytes_written == sizeof(buffer)) ;
   }
   TEST(0 == free_file(&fd)) ;

   // TEST advisereadahead_file
   TEST(0 == init_file(&fd, "advise1", accessmode_READ, tempdir)) ;
   TEST(0 == advisereadahead_file(fd, 0, 0/*whole file*/)) ;
   TEST(0 == advisereadahead_file(fd, 0, filesize)) ;
   for (unsigned i = 0; i < filesize; i += sizeof(buffer)) {
      TEST(0 == read_file(fd, sizeof(buffer), buffer, &bytes_read)) ;
      TEST(bytes_read == sizeof(buffer)) ;
      TEST(buffer[0] == (uint8_t)(i/sizeof(buffer))) ;
   }
   TEST(0 == free_file(&fd)) ;

   // TEST advisereadahead_file: EINVAL
   TEST(0 == init_file(&fd, "advise1", accessmode_READ, tempdir)) ;
   TEST(EINVAL == advisereadahead_file(fd, 0, -1)) ;
   file_t badfd = fd ;
   TEST(0 == free_file(&fd)) ;

   // TEST advisereadahead_file: EBADF
   TEST(EBADF == advisereadahead_file(badfd, 0, 0)) ;
   TEST(EBADF == advisereadahead_file(file_INIT_FREEABLE, 0, 0)) ;

   // TEST advisedontneed_file: EINVAL
   TEST(0 == init_file(&fd, "advise1", accessmode_READ, tempdir)) ;
   TEST(EINVAL == advisedontneed_file(fd, 0, -1)) ;
   badfd = fd ;
   TEST(0 == free_file(&fd)) ;

   // TEST advisedontneed_file
   TEST(0 == init_file(&fd, "advise1", accessmode_READ, tempdir)) ;
   TEST(0 == advisedontneed_file(fd, 0, 0/*whole file*/)) ;
   TEST(0 == advisedontneed_file(fd, 0, filesize)) ;
   for (unsigned i = 0; i < filesize; i += sizeof(buffer)) {
      TEST(0 == read_file(fd, sizeof(buffer), buffer, &bytes_read)) ;
      TEST(bytes_read == sizeof(buffer)) ;
      TEST(buffer[0] == (uint8_t)(i/sizeof(buffer))) ;
   }
   TEST(0 == free_file(&fd)) ;

   // TEST advisedontneed_file: EBADF
   TEST(EBADF == advisedontneed_file(badfd, 0, 0)) ;
   TEST(EBADF == advisedontneed_file(file_INIT_FREEABLE, 0, 0)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "advise1")) ;

   return 0 ;
ONABORT:
   removefile_directory(tempdir, "advise1") ;
   free_file(&fd) ;
   return EINVAL ;
}


int unittest_io_file()
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;
   cstring_t         tmppath = cstring_INIT ;
   directory_t     * tempdir = 0 ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == newtemp_directory(&tempdir, "iofiletest", &tmppath)) ;

   if (test_nropen())            goto ONABORT ;
   if (test_remove(tempdir))     goto ONABORT ;
   if (test_query(tempdir))      goto ONABORT ;
   if (test_initfree(tempdir))   goto ONABORT ;
   if (test_create(tempdir))     goto ONABORT ;
   if (test_append(tempdir))     goto ONABORT ;
   if (test_readwrite(tempdir))  goto ONABORT ;
   if (test_allocate(tempdir))   goto ONABORT ;
   if (test_advise(tempdir))     goto ONABORT ;

   // adapt LOG
   char * logbuffer ;
   size_t logbuffer_size ;
   GETBUFFER_LOG( &logbuffer, &logbuffer_size ) ;
   if (logbuffer_size) {
      char * found = logbuffer ;
      while ( (found = strstr( found, str_cstring(&tmppath))) ) {
         if (!strchr(found, '.')) break ;
         memcpy( strchr(found, '.')+1, "123456", 6 ) ;
         found += size_cstring(&tmppath) ;
      }
   }

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
