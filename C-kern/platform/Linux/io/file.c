/* title: File Linux
   Implements <File> on Linux.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/filesystem/file.h
    Header file <File>.

   file: C-kern/platform/Linux/io/file.c
    Linux specific implementation <File Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/filepath.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/string/cstring.h"
#include <sys/statvfs.h>
#endif


// section: file_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_file_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t s_file_errtimer = test_errortimer_FREE;
#endif

// group: constants

/* variable: TEMPFILENAME
 * Namenstemplate einer temporären Datei. */
#define TEMPFILENAME    P_tmpdir "/temp.XXXXXX"

// group: functions

int remove_file(const char * filepath, struct directory_t* relative_to)
{
   int err;
   int unlinkatfd = AT_FDCWD;

   if (relative_to) {
      unlinkatfd = io_directory(relative_to);
   }

   err = unlinkat(unlinkatfd, filepath, 0);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("unlinkat(unlinkatfd, filepath)", err);
      filepath_static_t fpath;
      init_filepathstatic(&fpath, relative_to, filepath);
      PRINTTEXT_ERRLOG(FILE_NAME, STRPARAM_filepathstatic(&fpath));
      PRINTTEXT_USER_ERRLOG(FILE_REMOVE, err, STRPARAM_filepathstatic(&fpath));
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: lifetime

int init_file(/*out*/file_t* fileobj, const char* filepath, accessmode_e iomode, const struct directory_t* relative_to)
{
   int err;
   int fd       = -1;
   int openatfd = AT_FDCWD;

   VALIDATE_INPARAM_TEST(iomode, ONERR, );
   VALIDATE_INPARAM_TEST(0 == (iomode & ~((unsigned)accessmode_RDWR)), ONERR, );

   if (relative_to) {
      openatfd = io_directory(relative_to);
   }

   static_assert( (O_RDONLY+1) == accessmode_READ, "simple conversion");
   static_assert( (O_WRONLY+1) == accessmode_WRITE, "simple conversion");
   static_assert( (O_RDWR+1)   == (accessmode_READ|accessmode_WRITE), "simple conversion");

   fd = openat(openatfd, filepath, ((int)iomode - 1)|O_CLOEXEC );
   if (-1 == fd) {
      err = errno;
      TRACESYSCALL_ERRLOG("openat", err);
      filepath_static_t fpath;
      init_filepathstatic(&fpath, relative_to, filepath);
      PRINTTEXT_ERRLOG(FILE_NAME, STRPARAM_filepathstatic(&fpath));
      PRINTTEXT_USER_ERRLOG(FILE_OPEN, err, STRPARAM_filepathstatic(&fpath));
      goto ONERR;
   }

   *fileobj = fd;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initappend_file(/*out*/file_t* fileobj, const char* filepath, const struct directory_t* relative_to/*0 => current working dir*/)
{
   int err;
   int fd;
   int openatfd = AT_FDCWD;

   if (relative_to) {
      openatfd = io_directory(relative_to);
   }

   fd = openat(openatfd, filepath, O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR );
   if (-1 == fd) {
      err = errno;
      TRACESYSCALL_ERRLOG("openat", err);
      filepath_static_t fpath;
      init_filepathstatic(&fpath, relative_to, filepath);
      PRINTTEXT_ERRLOG(FILE_NAME, STRPARAM_filepathstatic(&fpath));
      PRINTTEXT_USER_ERRLOG(FILE_OPEN, err, STRPARAM_filepathstatic(&fpath));
      goto ONERR;
   }

   *fileobj = fd;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initcreate_file(/*out*/file_t* fileobj, const char* filepath, const struct directory_t* relative_to)
{
   int err;
   int fd;
   int openatfd = AT_FDCWD;

   if (relative_to) {
      openatfd = io_directory(relative_to);
   }

   fd = openat(openatfd, filepath, O_RDWR|O_EXCL|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR );
   if (-1 == fd) {
      err = errno;
      TRACESYSCALL_ERRLOG("openat", err);
      filepath_static_t fpath;
      init_filepathstatic(&fpath, relative_to, filepath);
      PRINTTEXT_ERRLOG(FILE_NAME, STRPARAM_filepathstatic(&fpath));
      PRINTTEXT_USER_ERRLOG(FILE_CREATE, err, STRPARAM_filepathstatic(&fpath));
      goto ONERR;
   }

   *fileobj = fd;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int inittemp_file(/*out*/file_t* file)
{
   int err;
   int fd;
   char path[] = { TEMPFILENAME };

   fd = mkostemp(path, O_CLOEXEC);
   if (  -1 == fd
         || 0 != unlink(path)) {
      err = errno;
      TRACESYSCALL_ERRLOG(-1 == fd ? "mkostemp" : "unlink", err);
      filepath_static_t fpath;
      init_filepathstatic(&fpath, 0, path);
      PRINTTEXT_ERRLOG(FILE_NAME, STRPARAM_filepathstatic(&fpath));
      PRINTTEXT_USER_ERRLOG(FILE_CREATE, err, STRPARAM_filepathstatic(&fpath));
      goto ONERR;
   }

   *file = fd;

   return 0;
ONERR:
   if (-1 != fd) close(fd);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initcreatetemp_file(/*out*/file_t* file, /*ret*/struct wbuffer_t* path)
{
   int err;
   int fd;
   size_t   pathsize = sizeof(TEMPFILENAME);
   size_t   oldsize  = size_wbuffer(path);
   uint8_t* pathbuffer;

   err = appendbytes_wbuffer(path, pathsize, &pathbuffer);
   if (err) goto ONERR;
   memcpy(pathbuffer, TEMPFILENAME, pathsize);

   ONERROR_testerrortimer(&s_file_errtimer, &err, ONERR);
   fd = mkostemp((char*)pathbuffer, O_CLOEXEC);
   if (-1 == fd) {
      err = errno;
      TRACESYSCALL_ERRLOG("mkostemp", err);
      filepath_static_t fpath;
      init_filepathstatic(&fpath, 0, (const char*)pathbuffer);
      PRINTTEXT_ERRLOG(FILE_NAME, STRPARAM_filepathstatic(&fpath));
      PRINTTEXT_USER_ERRLOG(FILE_CREATE, err, STRPARAM_filepathstatic(&fpath));
      goto ONERR;
   }

   *file = fd;

   return 0;
ONERR:
   shrink_wbuffer(path, oldsize);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_file(file_t* fileobj)
{
   int err;
   int close_fd = *fileobj;

   if (!isfree_file(close_fd)) {
      *fileobj = file_FREE;

      err = close(close_fd);
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("close", err);
         PRINTINT_ERRLOG(close_fd);
      }

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

accessmode_e accessmode_file(const file_t fileobj)
{
   int err;
   int flags;

   flags = fcntl(fileobj, F_GETFL);
   if (-1 == flags) {
      err = errno;
      TRACESYSCALL_ERRLOG("fcntl", err);
      PRINTINT_ERRLOG(fileobj);
      goto ONERR;
   }

   static_assert((O_RDONLY+1) == accessmode_READ, "simple conversion");
   static_assert((O_WRONLY+1) == accessmode_WRITE, "simple conversion");
   static_assert((O_RDWR+1)   == (accessmode_READ|accessmode_WRITE), "simple conversion");
   static_assert(O_ACCMODE    == (O_RDWR|O_WRONLY|O_RDONLY), "simple conversion");

   return (accessmode_e) (1 + (flags & O_ACCMODE));
ONERR:
   TRACEEXIT_ERRLOG(err);
   return accessmode_NONE;
}

/* function: isvalid_file
 * Uses fcntl to query file descriptor flags (FD_CLOEXEC). */
bool isvalid_file(file_t fileobj)
{
   int err;

   err = fcntl(fileobj, F_GETFD);

   return (-1 != err);
}

int size_file(const file_t fileobj, /*out*/off_t * file_size)
{
   int err;
   struct stat stat_result;

   err = fstat(fileobj, &stat_result);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("fstat", err);
      PRINTINT_ERRLOG(fileobj);
      goto ONERR;
   }

   *file_size = stat_result.st_size;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: I/O

int advisereadahead_file(file_t fileobj, off_t offset, off_t length)
{
   int err;

   err = posix_fadvise(fileobj, offset, length, POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED);
   if (err) {
      TRACESYSCALL_ERRLOG("posix_fadvise", err);
      PRINTINT_ERRLOG(fileobj);
      PRINTINT64_ERRLOG(offset);
      PRINTINT64_ERRLOG(length);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int advisedontneed_file(file_t fileobj, off_t offset, off_t length)
{
   int err;

   err = posix_fadvise(fileobj, offset, length, POSIX_FADV_DONTNEED);
   if (err) {
      TRACESYSCALL_ERRLOG("posix_fadvise", err);
      PRINTINT_ERRLOG(fileobj);
      PRINTINT64_ERRLOG(offset);
      PRINTINT64_ERRLOG(length);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// group: allocation

int truncate_file(file_t fileobj, off_t file_size)
{
   int err;

   err = ftruncate(fileobj, file_size);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("ftruncate", err);
      PRINTINT_ERRLOG(fileobj);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int allocate_file(file_t fileobj, off_t file_size)
{
   int err;

   err = fallocate(fileobj, 0/*adapt file size*/, 0, file_size);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("fallocate", err);
      PRINTINT_ERRLOG(fileobj);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_remove(directory_t* tempdir)
{
   file_t   file = file_FREE;
   off_t    filesize;

   // TEST remove_file
   for (unsigned i = 0; i < 10; ++i) {
      // create
      const size_t datasize = i * 1000;
      TEST(0 == makefile_directory(tempdir, "remove", datasize));
      TEST(0 == filesize_directory(tempdir, "remove", &filesize));
      TEST(filesize == datasize);
      // now remove
      TEST(0 == trypath_directory(tempdir, "remove"));
      TEST(0 == remove_file("remove", tempdir));
      TEST(ENOENT == trypath_directory(tempdir, "remove"));
   }

   return 0;
ONERR:
   free_file(&file);
   return EINVAL;
}

static int test_query(directory_t* tempdir)
{
   file_t fd  = file_FREE;
   file_t fd2 = file_FREE;
   int    pipefd[2] = { file_FREE, file_FREE };
   off_t  filesize;

   // prepare
   TEST(0 == makefile_directory(tempdir, "testfile", 1));

   // TEST static init
   TEST(0 == file_STDIN);
   TEST(1 == file_STDOUT);
   TEST(2 == file_STDERR);

   // TEST isfree_file
   fd = file_FREE;
   TEST(1 == isfree_file(fd));
   fd = file_STDIN;
   TEST(0 == isfree_file(fd));
   fd = file_STDOUT;
   TEST(0 == isfree_file(fd));
   fd = file_STDERR;
   TEST(0 == isfree_file(fd));

   // TEST isvalid_file
   TEST(0 == isvalid_file(file_FREE));
   TEST(0 == isvalid_file(100));
   TEST(1 == isvalid_file(file_STDIN));
   TEST(1 == isvalid_file(file_STDOUT));
   TEST(1 == isvalid_file(file_STDERR));

   // TEST accessmode_file: accessmode_READ
   fd = openat(io_directory(tempdir), "testfile", O_RDONLY|O_CLOEXEC);
   TEST(fd > 0);
   fd2 = dup(fd);
   TEST(fd2 > 0);
   TEST(accessmode_READ == accessmode_file(fd));
   TEST(accessmode_READ == accessmode_file(fd2));
   TEST(0 == free_file(&fd));
   TEST(0 == free_file(&fd2));
   TEST(fd == file_FREE);
   TEST(fd2 == file_FREE);

   // TEST accessmode_file: accessmode_WRITE
   fd = openat(io_directory(tempdir), "testfile", O_WRONLY|O_CLOEXEC);
   TEST(fd > 0);
   fd2 = dup(fd);
   TEST(fd2 > 0);
   TEST(accessmode_WRITE == accessmode_file(fd));
   TEST(accessmode_WRITE == accessmode_file(fd2));
   TEST(0 == free_file(&fd));
   TEST(0 == free_file(&fd2));
   TEST(fd == file_FREE);
   TEST(fd2 == file_FREE);

   // TEST accessmode_file: accessmode_RDWR
   fd = openat(io_directory(tempdir), "testfile", O_RDWR|O_CLOEXEC);
   fd2 = fd;
   TEST(fd > 0);
   TEST(accessmode_RDWR == accessmode_file(fd));
   TEST(accessmode_RDWR == accessmode_file(fd2));
   TEST(0 == free_file(&fd));
   TEST(fd == file_FREE);

   // TEST accessmode_file: accessmode_NONE
   TEST(accessmode_NONE == accessmode_file(fd2));
   TEST(accessmode_NONE == accessmode_file(fd2));
   TEST(accessmode_NONE == accessmode_file(fd));
   TEST(accessmode_NONE == accessmode_file(fd));
   fd2 = file_FREE;

   // TEST size_file: regular file
   TEST(0 == initappend_file(&fd, "testfilesize", tempdir));
   filesize = 1;
   TEST(0 == size_file(fd, &filesize));
   TEST(0 == filesize);
   for (unsigned i = 1; i <= 256; ++i) {
      uint8_t  buffer[257];
      memset(buffer, 3, sizeof(buffer));
      TEST(0 == write_file(fd, sizeof(buffer), buffer, 0));
      TEST(0 == size_file(fd, &filesize));
      TEST(filesize == sizeof(buffer)*i);
      filesize = 0;
      TEST(0 == init_file(&fd2, "testfilesize", accessmode_READ, tempdir));
      TEST(0 == size_file(fd2, &filesize));
      TEST(0 == free_file(&fd2));
      TEST(filesize == sizeof(buffer)*i);
      filesize = 0;
      TEST(0 == filesize_directory(tempdir, "testfilesize", &filesize));
      TEST(filesize == sizeof(buffer)*i);
   }
   TEST(0 == free_file(&fd));

   // TEST size_file: pipe
   TEST(0 == pipe2(pipefd, O_NONBLOCK|O_CLOEXEC));
   TEST(0 == size_file(pipefd[0], &filesize));
   TEST(0 == filesize);
   TEST(0 == size_file(pipefd[1], &filesize));
   TEST(0 == filesize);
   TEST(0 == free_file(&pipefd[0]));
   TEST(0 == free_file(&pipefd[1]));

   // TEST EBADF
   TEST(EBADF == size_file(file_FREE, &filesize));

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testfile"));
   TEST(0 == removefile_directory(tempdir, "testfilesize"));

   return 0;
ONERR:
   free_file(&fd);
   free_file(&fd2);
   free_file(&pipefd[0]);
   free_file(&pipefd[1]);
   removefile_directory(tempdir, "testfile");
   return EINVAL;
}

static int test_initfree(directory_t* tempdir)
{
   file_t   file = file_FREE;
   size_t   nropenfd;
   size_t   nropenfd2;

   // TEST static init
   TEST(-1 == file);
   TEST(isfree_file(file));

   // TEST init_file, free_file
   accessmode_e modes[3] = {  accessmode_READ,  accessmode_WRITE, accessmode_RDWR };
   TEST(0 == makefile_directory(tempdir, "init1", 1999));
   TEST(0 == trypath_directory(tempdir, "init1"));
   TEST(0 == nropen_iochannel(&nropenfd));
   for (unsigned i = 0; i < lengthof(modes); ++i) {
      TEST(0 == init_file(&file, "init1", modes[i], tempdir));
      TEST(modes[i] == accessmode_file(file));
      TEST(!isfree_file(file));
      TEST(0 == nropen_iochannel(&nropenfd2));
      TEST(nropenfd+1 == nropenfd2);
      TEST(0 == free_file(&file));
      TEST(file == file_FREE);
      TEST(isfree_file(file));
      TEST(0 == nropen_iochannel(&nropenfd2));
      TEST(nropenfd == nropenfd2);
      TEST(0 == free_file(&file));
      TEST(file == file_FREE);
      TEST(0 == nropen_iochannel(&nropenfd2));
      TEST(nropenfd == nropenfd2);
   }
   TEST(0 == removefile_directory(tempdir, "init1"));

   // TEST initcreate_file, free_file
   TEST(ENOENT == trypath_directory(tempdir, "init2"));
   TEST(0 == initcreate_file(&file, "init2", tempdir));
   TEST(accessmode_RDWR == accessmode_file(file));
   TEST(!isfree_file(file));
   TEST(0 == nropen_iochannel(&nropenfd2));
   TEST(nropenfd+1 == nropenfd2);
   TEST(0 == trypath_directory(tempdir, "init2"));
   TEST(0 == free_file(&file));
   TEST(file == file_FREE);
   TEST(0 == nropen_iochannel(&nropenfd2));
   TEST(nropenfd == nropenfd2);
   TEST(0 == free_file(&file));
   TEST(file == file_FREE);
   TEST(0 == nropen_iochannel(&nropenfd2));
   TEST(nropenfd == nropenfd2);
   TEST(0 == removefile_directory(tempdir, "init2"));

   // TEST initappend_file, free_file
   TEST(ENOENT == trypath_directory(tempdir, "init3"));
   TEST(0 == initappend_file(&file, "init3", tempdir));
   TEST(accessmode_WRITE == accessmode_file(file));
   TEST(!isfree_file(file));
   TEST(0 == nropen_iochannel(&nropenfd2));
   TEST(nropenfd+1 == nropenfd2);
   TEST(0 == trypath_directory(tempdir, "init3"));
   TEST(0 == free_file(&file));
   TEST(file == file_FREE);
   TEST(0 == nropen_iochannel(&nropenfd2));
   TEST(nropenfd == nropenfd2);
   TEST(0 == free_file(&file));
   TEST(file == file_FREE);
   TEST(0 == nropen_iochannel(&nropenfd2));
   TEST(nropenfd == nropenfd2);
   TEST(0 == removefile_directory(tempdir, "init3"));

   // TEST initmove_file
   for (int i = 0; i < 100; ++i) {
      file_t destfile   = file_FREE;
      file_t sourcefile = i;
      TEST(!isfree_file(sourcefile));
      initmove_file(&destfile, &sourcefile);
      TEST(isfree_file(sourcefile)) /*source is reset*/;
      TEST(destfile == i) /*dest contains former value of source*/;
   }

   // TEST initmove_file: move to itself does not work !
   file = 0;
   TEST(!isfree_file(file));
   initmove_file(&file, &file);
   TEST(isfree_file(file));

   // TEST EEXIST
   TEST(0 == makefile_directory(tempdir, "init1", 0));
   TEST(EEXIST == initcreate_file(&file, "init1", tempdir));
   TEST(file == file_FREE);
   TEST(0 == removefile_directory(tempdir, "init1"));

   // TEST EINVAL
   TEST(0 == makefile_directory(tempdir, "init1", 0));
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_EXEC, tempdir));
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_PRIVATE, tempdir));
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_SHARED, tempdir));
   TEST(0 == removefile_directory(tempdir, "init1"));

   return 0;
ONERR:
   free_file(&file);
   removefile_directory(tempdir, "init1");
   removefile_directory(tempdir, "init2");
   return EINVAL;
}

static int compare_file_content(directory_t* tempdir, const char * filename, unsigned times)
{
   file_t file = file_FREE;

   TEST(0 == init_file(&file, filename, accessmode_READ, tempdir));

   for (unsigned t = 0; t < times; ++t) {
      uint8_t buffer[256] = { 0 };
      size_t  bread = 0;
      TEST(0 == read_file(file, 256, buffer, &bread));
      TEST(256 == bread);
      for (unsigned i = 0; i < 256; ++i) {
         TEST(i == buffer[i]);
      }
   }

   TEST(0 == free_file(&file));

   return 0;
ONERR:
   free_file(&file);
   return EINVAL;
}

static int test_create(directory_t* tempdir)
{
   file_t  file = file_FREE;
   size_t  bwritten;
   size_t  bread;
   uint8_t buffer[256];
   uint8_t buffer2[256];
   uint8_t filename[256];
   off_t   size;

   // prepare
   for (unsigned i = 0; i < 256; ++i) {
      buffer[i] = (uint8_t) i;
   }

   // TEST initcreate_file: file does not exist
   TEST(ENOENT == trypath_directory(tempdir, "testcreate"));
   TEST(0 == initcreate_file(&file, "testcreate", tempdir));
   TEST(0 == filesize_directory(tempdir, "testcreate", &size));
   TEST(0 == size);
   TEST(0 == write_file(file, 256, buffer, &bwritten));
   TEST(256 == bwritten);
   TEST(0 == filesize_directory(tempdir, "testcreate", &size));
   TEST(256 == size);
   TEST(0 == free_file(&file));
   TEST(file == file_FREE);
   if (compare_file_content(tempdir, "testcreate", 1)) goto ONERR;

   // TEST initcreate_file: EEXIST
   TEST(0 == trypath_directory(tempdir, "testcreate"));
   TEST(EEXIST == initcreate_file(&file, "testcreate", tempdir));
   TEST(0 == filesize_directory(tempdir, "testcreate", &size));
   TEST(256 == size);
   TEST(file == file_FREE);
   if (compare_file_content(tempdir, "testcreate", 1)) goto ONERR;

   // TEST inittemp_file
   TEST(0 == inittemp_file(&file));
   // check O_CLOEXEC
   TEST(FD_CLOEXEC == fcntl(file, F_GETFD));
   // check read / write
   TEST(0 == write_file(file, 256, buffer, &bwritten));
   TEST(256 == bwritten);
   TEST(0 == lseek(file, 0, SEEK_SET));
   TEST(0 == read_file(file, 256, buffer2, &bread));
   TEST(256 == bread);
   TEST(0 == memcmp(buffer, buffer2, 256));
   TEST(0 == free_file(&file));

   // TEST initcreatetemp_file
   memset(filename, 255, sizeof(filename));
   wbuffer_t wbuf = wbuffer_INIT_STATIC(sizeof(filename)-1, filename);
   TEST(0 == initcreatetemp_file(&file, &wbuf));
   TEST(sizeof(TEMPFILENAME) == size_wbuffer(&wbuf));
   TEST(0 == memcmp(TEMPFILENAME, filename, sizeof(TEMPFILENAME)-7));
   TEST(0   == filename[sizeof(TEMPFILENAME)-1]);
   TEST(255 == filename[sizeof(TEMPFILENAME)]);
   // check O_CLOEXEC
   TEST(FD_CLOEXEC == fcntl(file, F_GETFD));
   // check read / write
   TEST(0 == write_file(file, 256, buffer, &bwritten));
   TEST(256 == bwritten);
   TEST(0 == compare_file_content(0, (char*)filename, 1));
   TEST(0 == free_file(&file));
   TEST(0 == removefile_directory(tempdir, (char*)filename));

   // TEST initcreatetemp_file: ENOMEM (wbuffer too small)
   memset(filename, 1, sizeof(filename));
   wbuf = (wbuffer_t) wbuffer_INIT_STATIC(3, filename);
   TEST(ENOMEM == initcreatetemp_file(&file, &wbuf));
   TEST(0 == size_wbuffer(&wbuf));
   TEST(1 == filename[0]);
   TEST( isfree_file(file));

   // TEST initcreatetemp_file: simulated EMFILE (too many open files)
   uint8_t* dummy;
   wbuf = (wbuffer_t) wbuffer_INIT_STATIC(sizeof(filename)-1, filename);
   TEST(0 == appendbytes_wbuffer(&wbuf, 10, &dummy));
   init_testerrortimer(&s_file_errtimer, 1, EMFILE);
   TEST(EMFILE == initcreatetemp_file(&file, &wbuf));
   TEST(10 == size_wbuffer(&wbuf)); // length not changed
   TEST( isfree_file(file));

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testcreate"));

   return 0;
ONERR:
   free_file(&file);
   removefile_directory(tempdir, "testcreate");
   return EINVAL;
}

static int test_append(directory_t* tempdir)
{
   file_t   file = file_FREE;
   size_t   bwritten;
   uint8_t  buffer[256];
   off_t    size;

   // prepare
   for (unsigned i = 0; i < 256; ++i) {
      buffer[i] = (uint8_t) i;
   }

   // TEST initappend_file: file does not exist
   TEST(ENOENT == trypath_directory(tempdir, "testappend"));
   TEST(0 == initappend_file(&file, "testappend", tempdir));
   TEST(0 == filesize_directory(tempdir, "testappend", &size));
   TEST(0 == size);
   TEST(0 == write_file(file, 256, buffer, &bwritten));
   TEST(256 == bwritten);
   TEST(0 == filesize_directory(tempdir, "testappend", &size));
   TEST(256 == size);
   TEST(0 == free_file(&file));
   TEST(file == file_FREE);
   if (compare_file_content(tempdir, "testappend", 1)) goto ONERR;

   // TEST initappend_file: file already exists
   TEST(0 == trypath_directory(tempdir, "testappend"));
   TEST(0 == initappend_file(&file, "testappend", tempdir));
   TEST(0 == filesize_directory(tempdir, "testappend", &size));
   TEST(256 == size);
   TEST(0 == write_file(file, 256, buffer, &bwritten));
   TEST(256 == bwritten);
   TEST(0 == filesize_directory(tempdir, "testappend", &size));
   TEST(512 == size);
   TEST(0 == free_file(&file));
   TEST(file == file_FREE);
   if (compare_file_content(tempdir, "testappend", 2)) goto ONERR;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testappend"));

   return 0;
ONERR:
   free_file(&file);
   removefile_directory(tempdir, "testappend");
   return EINVAL;
}

// test read / write with interrupts

typedef struct thread_arg_t   thread_arg_t;

struct thread_arg_t {
   thread_t    * caller;
   file_t      fd;
};

static int thread_reader(thread_arg_t * startarg)
{
   int err;
   size_t  bytes_read = 0;
   uint8_t byte = 0;
   resume_thread(startarg->caller);
   err = read_file(startarg->fd, 1, &byte, &bytes_read);
   return err ? err : (bytes_read != 1) || (byte != 200);
}

static int thread_writer(thread_arg_t * startarg)
{
   int err;
   size_t  bytes_written = 0;
   uint8_t byte = 200;
   resume_thread(startarg->caller);
   err = write_file(startarg->fd, 1, &byte, &bytes_written);
   return err ? err : (bytes_written != 1);
}

static int thread_writer2(thread_arg_t * startarg)
{
   int err;
   uint8_t buffer[2] = { 1, 2 };
   resume_thread(startarg->caller);
   err = write_file(startarg->fd, sizeof(buffer), buffer, 0);
   CLEARBUFFER_ERRLOG();
   return (err != EPIPE);
}

static int        s_siguser_count  = 0;
static thread_t * s_siguser_thread = 0;

static void siguser(int signr)
{
   assert(SIGUSR1 == signr);
   if (s_siguser_count) {
      assert(s_siguser_thread == self_thread());
   } else {
      s_siguser_thread = self_thread();
   }
   ++ s_siguser_count;
}

static int test_readwrite(directory_t* tempdir)
{
   file_t            fd        = file_FREE;
   file_t            pipefd[2] = { file_FREE, file_FREE };
   memblock_t        buffer    = memblock_FREE;
   thread_t          * thread  = 0;
   uint8_t           byte;
   size_t            bytes_read;
   size_t            bytes_written;
   size_t            pipe_buffersize;
   sigset_t          oldset;
   bool              isOldact  = false;
   struct sigaction  newact;
   struct sigaction  oldact;

   // prepare
   TEST(0 == makefile_directory(tempdir, "readwrite1", sizeof(buffer)));

   // TEST write_file: blocking write
   fd = openat(io_directory(tempdir), "readwrite1", O_WRONLY|O_CLOEXEC);
   TEST(0 < fd);
   for (unsigned i = 0; i < 10000; ++i) {
      byte = (uint8_t) i;
      bytes_written = 0;
      TEST(0 == write_file(fd, 1, &byte, &bytes_written));
      TEST(1 == bytes_written);
   }
   TEST(0 == free_file(&fd));

   // TEST read_file: blocking read
   fd = openat(io_directory(tempdir), "readwrite1", O_RDONLY|O_CLOEXEC);
   TEST(0 < fd);
   for (unsigned i = 0; i < 10000-1; ++i) {
      byte = (uint8_t) (1+i);
      bytes_read = 0;
      TEST(0 == read_file(fd, 1, &byte, &bytes_read));
      TEST(1 == bytes_read);
      TEST((uint8_t)i == byte);
   }
   TEST(0 == read_file(fd, 2, &byte, &bytes_read));
   TEST(1 == bytes_read);
   TEST(0 == read_file(fd, 1, &byte, &bytes_read));
   TEST(0 == bytes_read /*end of file*/);
   TEST(0 == free_file(&fd));

   // TEST write_file: write non blocking mode
   TEST(0 == pipe2(pipefd, O_NONBLOCK|O_CLOEXEC));
   for (bytes_written = 0;;++ bytes_written) {
      size_t bytes_written2 = 0;
      byte = (uint8_t)bytes_written;
      int err = write_file(pipefd[1], 1, &byte, &bytes_written2);
      if (err) {
         TEST(EAGAIN == err);
         break;
      }
      TEST(1 == bytes_written2);
   }
   TEST(-1 == write(pipefd[1], &byte, 1));
   TEST(EAGAIN == errno);
   bytes_read = 2;
   TEST(EAGAIN == write_file(pipefd[1], 1, &byte, &bytes_read));
   TEST(2 == bytes_read /*has not changed*/);
   pipe_buffersize = bytes_written;

   // TEST read_file: read non blocking mode
   TEST(0 == RESIZE_MM(100 + bytes_written, &buffer));
   TEST(0 == read_file(pipefd[0], 100 + bytes_written, buffer.addr, &bytes_read));
   TEST(bytes_written == bytes_read);
   TEST(-1 == read(pipefd[0], &byte, 1));
   TEST(EAGAIN == errno);
   TEST(EAGAIN == read_file(pipefd[0], 1, &byte, &bytes_read));
   TEST(bytes_written == bytes_read /*has not changed*/);
   TEST(0 == FREE_MM(&buffer));

   // TEST read_file: interrupt ignored
   TEST(0 == free_file(&pipefd[0]));
   TEST(0 == free_file(&pipefd[1]));
   TEST(0 == pipe2(pipefd, O_CLOEXEC));
   TEST(0 == sigemptyset(&newact.sa_mask));
   TEST(0 == sigaddset(&newact.sa_mask, SIGUSR1));
   TEST(0 == sigprocmask(SIG_UNBLOCK, &newact.sa_mask, &oldset));
   sigemptyset(&newact.sa_mask);
   newact.sa_flags   = 0;
   newact.sa_handler = &siguser;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact));
   isOldact = true;
   thread_arg_t startarg = { .caller = self_thread(), .fd = pipefd[0] };
   TEST(0 == newgeneric_thread(&thread, thread_reader, &startarg));
   suspend_thread();
   sleepms_thread(100);
   s_siguser_count  = 0;
   s_siguser_thread = 0;
   pthread_kill(thread->sys_thread, SIGUSR1);
   byte = 200;
   TEST(1 == write(pipefd[1], &byte, 1));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(s_siguser_count  == 1);
   TEST(s_siguser_thread == thread);
   TEST(0 == delete_thread(&thread));

   // TEST write_file: interrupt ignored
   for (size_t i = 0; i < pipe_buffersize; ++i) {
      byte = 0;
      TEST(0 == write_file(pipefd[1], 1, &byte, 0));
   }
   startarg.fd = pipefd[1];
   TEST(0 == newgeneric_thread(&thread, thread_writer, &startarg));
   suspend_thread();
   sleepms_thread(50);
   s_siguser_count  = 0;
   s_siguser_thread = 0;
   pthread_kill(thread->sys_thread, SIGUSR1);
   for (size_t i = 0; i < pipe_buffersize; ++i) {
      TEST(0 == read_file(pipefd[0], 1, &byte, 0));
   }
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(s_siguser_count  == 1);
   TEST(s_siguser_thread == thread);
   TEST(0 == delete_thread(&thread));

   // TEST write_file: EPIPE, write with receiving end closed during write
   TEST(0 == free_file(&pipefd[0]));
   TEST(0 == free_file(&pipefd[1]));
   TEST(0 == pipe2(pipefd, O_CLOEXEC));
   for (size_t i = 0; i < pipe_buffersize-1; ++i) {
      byte = 0;
      TEST(0 == write_file(pipefd[1], 1, &byte, 0));
   }
   startarg = (thread_arg_t) { .caller = self_thread(), .fd = pipefd[1] };
   // thread_writer2 writes 2 bytes and therefore waits
   // (pipe buffer can only hold 1 more byte)
   TEST(0 == newgeneric_thread(&thread, thread_writer2, &startarg));
   suspend_thread();
   sleepms_thread(100);
   TEST(0 == free_file(&pipefd[0]));
   TEST(0 == join_thread(thread));
   // check that thread_writer2 encountered EPIPE
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(EPIPE == write_file(pipefd[1], 1, &byte, &bytes_written));
   TEST(EPIPE == write_file(pipefd[1], 1, &byte, &bytes_written));
   TEST(0 == free_file(&pipefd[1]));

   // TEST read_file: returns bytes read == 0 if writer closed pipe
   TEST(0 == pipe2(pipefd, O_CLOEXEC));
   TEST(0 == free_file(&pipefd[1]));
   bytes_read = 1;
   TEST(0 == read_file(pipefd[0], 1, &byte, &bytes_read));
   TEST(0 == bytes_read);
   TEST(0 == free_file(&pipefd[0]));

   // unprepare
   TEST(0 == removefile_directory(tempdir, "readwrite1"));
   TEST(0 == sigprocmask(SIG_SETMASK, &oldset, 0));
   isOldact = 0;
   TEST(0 == sigaction(SIGUSR1, &oldact, 0));

   return 0;
ONERR:
   if (isOldact) {
      sigprocmask(SIG_SETMASK, &oldset, 0);
      sigaction(SIGUSR1, &oldact, 0);
   }
   delete_thread(&thread);
   FREE_MM(&buffer);
   removefile_directory(tempdir, "readwrite1");
   free_file(&fd);
   free_file(&pipefd[0]);
   free_file(&pipefd[1]);
   return EINVAL;
}

static int test_allocate(directory_t* tempdir)
{
   file_t   file  = file_FREE;
   file_t   file2 = file_FREE;
   int      pipefd[2] = { file_FREE, file_FREE };
   size_t   bwritten;
   size_t   bread;
   uint32_t buffer[1024];
   uint32_t buffer2[1024];
   off_t    size;

   // prepare
   TEST(0 == pipe2(pipefd, O_CLOEXEC));
   for (uint32_t i = 0; i < lengthof(buffer); ++i) {
      buffer[i] = i;
   }

   // TEST truncate_file: shrink file
   TEST(ENOENT == trypath_directory(tempdir, "testallocate"));
   TEST(0 == initcreate_file(&file, "testallocate", tempdir));
   TEST(0 == filesize_directory(tempdir, "testallocate", &size));
   TEST(0 == size);
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(0 == write_file(file, sizeof(buffer), buffer, &bwritten));
      TEST(bwritten == sizeof(buffer));
      TEST(0 == size_file(file, &size));
      TEST(size == sizeof(buffer)*i);
   }
   for (unsigned i = 256; i >= 1; --i) {
      TEST(0 == truncate_file(file, sizeof(buffer)*(i-1)));
      TEST(0 == size_file(file, &size));
      TEST(size == sizeof(buffer)*(i-1));
      TEST(0 == init_file(&file2, "testallocate", accessmode_READ, tempdir));
      for (unsigned i2 = 1; i2 < i; i2++) {
         memset(buffer2, 1, sizeof(buffer2));
         TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread));
         TEST(bread == sizeof(buffer2));
         TEST(0 == memcmp(buffer, buffer2, sizeof(buffer)));
      }
      TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread));
      TEST(0 == bread/*end of input*/);
      TEST(0 == free_file(&file2));
   }

   // TEST truncate_file: grow file with 0 bytes
   memset(buffer, 0, sizeof(buffer));
   TEST(0 == truncate_file(file, 11/*support sizes smaller than blocksize*/));
   TEST(0 == size_file(file, &size));
   TEST(size == 11);
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(0 == truncate_file(file, sizeof(buffer)*i));
      TEST(0 == size_file(file, &size));
      TEST(size == sizeof(buffer)*i);
      TEST(0 == init_file(&file2, "testallocate", accessmode_READ, tempdir));
      for (unsigned i2 = 0; i2 < i; i2++) {
         memset(buffer2, 1, sizeof(buffer2));
         TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread));
         TEST(bread == sizeof(buffer2));
         TEST(0 == memcmp(buffer, buffer2, sizeof(buffer)));
      }
      TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread));
      TEST(0 == bread/*end of input*/);
      TEST(0 == free_file(&file2));
   }
   TEST(0 == free_file(&file));

   // TEST allocate_file: grow
   TEST(0 == removefile_directory(tempdir, "testallocate"));
   TEST(0 == initcreate_file(&file, "testallocate", tempdir));
   TEST(0 == allocate_file(file, 12/*support sizes smaller than blocksize*/));
   TEST(0 == size_file(file, &size));
   TEST(size == 12);
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(0 == allocate_file(file, sizeof(buffer)*i));
      TEST(0 == size_file(file, &size));
      TEST(size == sizeof(buffer)*i);
      TEST(0 == init_file(&file2, "testallocate", accessmode_READ, tempdir));
      for (unsigned i2 = 0; i2 < i; i2++) {
         memset(buffer2, 1, sizeof(buffer2));
         TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread));
         TEST(bread == sizeof(buffer2));
         TEST(0 == memcmp(buffer, buffer2, sizeof(buffer)));
      }
      TEST(0 == read_file(file2, sizeof(buffer2), (uint8_t*)buffer2, &bread));
      TEST(0 == bread/*end of input*/);
      TEST(0 == free_file(&file2));
   }

   // TEST allocate_file: no shrink possible
   for (unsigned i = 1; i <= 256; ++i) {
      TEST(0 == allocate_file(file, sizeof(buffer)*i));
      TEST(0 == size_file(file, &size));
      TEST(size == sizeof(buffer)*256);
   }
   TEST(0 == free_file(&file));

   // TEST allocate_file: free disk blocks really allocated on file system
   TEST(0 == removefile_directory(tempdir, "testallocate"));
   TEST(0 == initcreate_file(&file, "testallocate", tempdir));
   struct statvfs statvfs_result1;
   struct statvfs statvfs_result2;
   TEST(0 == fstatvfs(file, &statvfs_result1));
   TEST(0 == allocate_file(file, statvfs_result1.f_frsize * 10000));
   TEST(0 == fstatvfs(file, &statvfs_result2));
   TEST(statvfs_result2.f_bfree + 10000 <= statvfs_result1.f_bfree);
   TEST(0 == truncate_file(file, 0));
   TEST(0 == fstatvfs(file, &statvfs_result2));
   TEST(statvfs_result2.f_bfree + 100 >= statvfs_result1.f_bfree);
   TEST(0 == free_file(&file));

   // TEST truncate_file: free disk blocks are not allocated on file system
   TEST(0 == removefile_directory(tempdir, "testallocate"));
   TEST(0 == initcreate_file(&file, "testallocate", tempdir));
   TEST(0 == fstatvfs(file, &statvfs_result1));
   TEST(0 == truncate_file(file, statvfs_result1.f_frsize * 10000));
   TEST(0 == fstatvfs(file, &statvfs_result2));
   TEST(statvfs_result2.f_bfree + 100 >= statvfs_result1.f_bfree);
   TEST(0 == free_file(&file));

   // TEST EINVAL
   // resize pipe
   TEST(EINVAL == truncate_file(pipefd[1], 4096))
   // read only
   TEST(0 == init_file(&file, "testallocate", accessmode_READ, tempdir));
   TEST(EINVAL == truncate_file(file, 4096))
   TEST(0 == free_file(&file));
   // negative size
   TEST(0 == init_file(&file, "testallocate", accessmode_RDWR, tempdir));
   TEST(EINVAL == truncate_file(file, -4096))
   TEST(EINVAL == allocate_file(file, -4096))
   TEST(0 == free_file(&file));

   // TEST ESPIPE (illegal seek)
   // resize pipe
   TEST(ESPIPE == allocate_file(pipefd[1], 4096))

   // TEST EBADF
   // read only
   TEST(0 == init_file(&file, "testallocate", accessmode_READ, tempdir));
   TEST(EBADF == allocate_file(file, 4096));
   int oldfile = file;
   TEST(0 == free_file(&file));
   // closed file
   TEST(EBADF == truncate_file(oldfile, 4096))
   TEST(EBADF == allocate_file(oldfile, 4096))
   // invalid file descriptor
   TEST(EBADF == truncate_file(file_FREE, 4096))
   TEST(EBADF == allocate_file(file_FREE, 4096))

   // TEST ENOSPC
   TEST(0 == init_file(&file, "testallocate", accessmode_RDWR, tempdir));
   TEST(0 == fstatvfs(file, &statvfs_result1));
   TEST(ENOSPC == allocate_file(file, (off_t) (statvfs_result1.f_frsize * (1+statvfs_result1.f_bavail))));
   TEST(0 == free_file(&file));

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testallocate"));
   TEST(0 == free_file(&pipefd[0]));
   TEST(0 == free_file(&pipefd[1]));

   return 0;
ONERR:
   free_file(&file);
   free_file(&file2);
   free_file(&pipefd[0]);
   free_file(&pipefd[1]);
   removefile_directory(tempdir, "testallocate");
   return EINVAL;
}

static int test_advise(directory_t* tempdir)
{
   file_t         fd     = file_FREE;
   uint8_t        buffer[256];
   size_t         bytes_read;
   size_t         bytes_written;
   const size_t   filesize = 1024*1024;

   // prepare
   TEST(0 == makefile_directory(tempdir, "advise1", filesize));
   TEST(0 == init_file(&fd, "advise1", accessmode_WRITE, tempdir));
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t)i;
   }
   for (unsigned i = 0; i < filesize; i += sizeof(buffer)) {
      buffer[0] = (uint8_t) (i/sizeof(buffer));
      TEST(0 == write_file(fd, sizeof(buffer), buffer, &bytes_written));
      TEST(bytes_written == sizeof(buffer));
   }
   TEST(0 == free_file(&fd));

   // TEST advisereadahead_file
   TEST(0 == init_file(&fd, "advise1", accessmode_READ, tempdir));
   TEST(0 == advisereadahead_file(fd, 0, 0/*whole file*/));
   TEST(0 == advisereadahead_file(fd, 0, filesize));
   for (unsigned i = 0; i < filesize; i += sizeof(buffer)) {
      TEST(0 == read_file(fd, sizeof(buffer), buffer, &bytes_read));
      TEST(bytes_read == sizeof(buffer));
      TEST(buffer[0] == (uint8_t)(i/sizeof(buffer)));
   }
   TEST(0 == free_file(&fd));

   // TEST advisereadahead_file: EINVAL
   TEST(0 == init_file(&fd, "advise1", accessmode_READ, tempdir));
   TEST(EINVAL == advisereadahead_file(fd, 0, -1));
   file_t badfd = fd;
   TEST(0 == free_file(&fd));

   // TEST advisereadahead_file: EBADF
   TEST(EBADF == advisereadahead_file(badfd, 0, 0));
   TEST(EBADF == advisereadahead_file(file_FREE, 0, 0));

   // TEST advisedontneed_file: EINVAL
   TEST(0 == init_file(&fd, "advise1", accessmode_READ, tempdir));
   TEST(EINVAL == advisedontneed_file(fd, 0, -1));
   badfd = fd;
   TEST(0 == free_file(&fd));

   // TEST advisedontneed_file
   TEST(0 == init_file(&fd, "advise1", accessmode_READ, tempdir));
   TEST(0 == advisedontneed_file(fd, 0, 0/*whole file*/));
   TEST(0 == advisedontneed_file(fd, 0, filesize));
   for (unsigned i = 0; i < filesize; i += sizeof(buffer)) {
      TEST(0 == read_file(fd, sizeof(buffer), buffer, &bytes_read));
      TEST(bytes_read == sizeof(buffer));
      TEST(buffer[0] == (uint8_t)(i/sizeof(buffer)));
   }
   TEST(0 == free_file(&fd));

   // TEST advisedontneed_file: EBADF
   TEST(EBADF == advisedontneed_file(badfd, 0, 0));
   TEST(EBADF == advisedontneed_file(file_FREE, 0, 0));

   // unprepare
   TEST(0 == removefile_directory(tempdir, "advise1"));

   return 0;
ONERR:
   removefile_directory(tempdir, "advise1");
   free_file(&fd);
   return EINVAL;
}


int unittest_io_file()
{
   cstring_t    tmppath = cstring_INIT;
   directory_t* tempdir = 0;

   TEST(0 == newtemp_directory(&tempdir, "iofiletest"));
   TEST(0 == path_directory(tempdir, &(wbuffer_t)wbuffer_INIT_CSTRING(&tmppath)));

   if (test_remove(tempdir))     goto ONERR;
   if (test_query(tempdir))      goto ONERR;
   if (test_initfree(tempdir))   goto ONERR;
   if (test_create(tempdir))     goto ONERR;
   if (test_append(tempdir))     goto ONERR;
   if (test_readwrite(tempdir))  goto ONERR;
   if (test_allocate(tempdir))   goto ONERR;
   if (test_advise(tempdir))     goto ONERR;

   // adapt LOG
   uint8_t *logbuffer;
   size_t   logsize;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   if (logsize) {
      char * found = (char*)logbuffer;
      while ( (found = strstr(found, str_cstring(&tmppath))) ) {
         if (!strchr(found, '.')) break;
         memcpy( strchr(found, '.')+1, "123456", 6 );
         found += size_cstring(&tmppath);
      }
   }

   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath)));
   TEST(0 == free_cstring(&tmppath));
   TEST(0 == delete_directory(&tempdir));

   return 0;
ONERR:
   (void) free_cstring(&tmppath);
   (void) delete_directory(&tempdir);
   return EINVAL;
}

#endif
