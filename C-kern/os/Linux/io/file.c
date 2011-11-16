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

   file: C-kern/os/Linux/io/file.c
    Linux specific implementation <File Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

int init_file(/*out*/file_t * fileobj, const char* filepath, accessmode_e iomode, const struct directory_t * relative_to)
{
   int err ;
   int fd       = -1 ;
   int openatfd = AT_FDCWD ;

   PRECONDITION_INPUT(!relative_to || relative_to->sys_dir, ABBRUCH, ) ;
   PRECONDITION_INPUT(iomode, ABBRUCH, ) ;
   PRECONDITION_INPUT(0 == (iomode & ~((unsigned)accessmode_RDWR)), ABBRUCH, ) ;

   if (relative_to) {
      openatfd = dirfd(relative_to->sys_dir) ;
   }

   static_assert( (O_RDONLY+1) == accessmode_READ, "simple conversion") ;
   static_assert( (O_WRONLY+1) == accessmode_WRITE, "simple conversion") ;
   static_assert( (O_RDWR+1)   == (accessmode_READ|accessmode_WRITE), "simple conversion") ;

   fd = openat(openatfd, filepath, ((int)iomode - 1)|O_CLOEXEC ) ;
   if (-1 == fd) {
      err = errno ;
      LOG_SYSERR("openat", err) ;
      LOG_STRING(filepath) ;
      goto ABBRUCH ;
   }

   *fileobj = fd ;

   return 0 ;
ABBRUCH:
   if (-1 != fd) {
      close(fd) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int initcreat_file(/*out*/file_t * fileobj, const char* filepath, const struct directory_t * relative_to)
{
   int err ;
   int fd       = -1 ;
   int openatfd = AT_FDCWD ;

   PRECONDITION_INPUT(!relative_to || relative_to->sys_dir, ABBRUCH, ) ;

   if (relative_to) {
      openatfd = dirfd(relative_to->sys_dir) ;
   }

   fd = openat(openatfd, filepath, O_RDWR|O_EXCL|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   if (-1 == fd) {
      err = errno ;
      LOG_SYSERR("openat", err) ;
      if (relative_to) {
         LOG_STRING(relative_to->path) ;
      }
      LOG_STRING(filepath) ;
      goto ABBRUCH ;
   }

   *fileobj = fd ;

   return 0 ;
ABBRUCH:
   if (-1 != fd) {
      (void) unlinkat(openatfd, filepath, 0) ;
      close(fd) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int free_file(file_t * fileobj)
{
   int err ;
   int fd = *fileobj ;

   if (isinit_filedescr(fd)) {
      *fileobj = file_INIT_FREEABLE ;

      err = close(fd) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("close", err) ;
         LOG_INT(fd) ;
         goto ABBRUCH ;
      }

   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_io_file,ABBRUCH)

static int test_initfree(directory_t * tempdir)
{
   file_t   file     = file_INIT_FREEABLE ;
   size_t   nropenfd ;
   size_t   nropenfd2 ;

   // TEST static init
   TEST(-1 == file) ;
   TEST(!isinit_filedescr(file)) ;
   TEST(!isinit_file(&file)) ;

   // TEST init, double free
   TEST(0 == makefile_directory(tempdir, "init1", 1999)) ;
   TEST(0 == checkpath_directory(tempdir, "init1")) ;
   TEST(0 == nropen_filedescr(&nropenfd)) ;
   TEST(0 == init_file(&file, "init1", accessmode_READ, tempdir)) ;
   TEST(isinit_filedescr(file)) ;
   TEST(isinit_file(&file)) ;
   TEST(0 == nropen_filedescr(&nropenfd2)) ;
   TEST(nropenfd+1 == nropenfd2) ;
   TEST(0 == free_file(&file)) ;
   TEST(-1 == file) ;
   TEST(!isinit_filedescr(file)) ;
   TEST(!isinit_file(&file)) ;
   TEST(0 == nropen_filedescr(&nropenfd2)) ;
   TEST(nropenfd == nropenfd2) ;
   TEST(0 == free_file(&file)) ;
   TEST(-1 == file) ;
   TEST(0 == nropen_filedescr(&nropenfd2)) ;
   TEST(nropenfd == nropenfd2) ;
   TEST(0 == removefile_directory(tempdir, "init1")) ;

   // TEST initcreat, double free
   TEST(ENOENT == checkpath_directory(tempdir, "init2")) ;
   TEST(0 == initcreat_file(&file, "init2", tempdir)) ;
   TEST(isinit_filedescr(file)) ;
   TEST(isinit_file(&file)) ;
   TEST(0 == nropen_filedescr(&nropenfd2)) ;
   TEST(nropenfd+1 == nropenfd2) ;
   TEST(0 == checkpath_directory(tempdir, "init2")) ;
   TEST(0 == free_file(&file)) ;
   TEST(-1 == file) ;
   TEST(0 == nropen_filedescr(&nropenfd2)) ;
   TEST(nropenfd == nropenfd2) ;
   TEST(0 == free_file(&file)) ;
   TEST(-1 == file) ;
   TEST(0 == nropen_filedescr(&nropenfd2)) ;
   TEST(nropenfd == nropenfd2) ;
   TEST(0 == removefile_directory(tempdir, "init2")) ;

   // TEST accessmode
   TEST(0 == initcreat_file(&file, "init1", tempdir)) ;
   TEST(accessmode_RDWR == accessmode_filedescr(fd_file(&file))) ;
   TEST(0 == free_file(&file)) ;
   TEST(0 == init_file(&file, "init1", accessmode_READ, tempdir)) ;
   TEST(accessmode_READ == accessmode_filedescr(fd_file(&file))) ;
   TEST(0 == free_file(&file)) ;
   TEST(0 == init_file(&file, "init1", accessmode_WRITE, tempdir)) ;
   TEST(accessmode_WRITE == accessmode_filedescr(fd_file(&file))) ;
   TEST(0 == free_file(&file)) ;
   TEST(0 == init_file(&file, "init1", accessmode_RDWR, tempdir)) ;
   TEST(accessmode_RDWR == accessmode_filedescr(fd_file(&file))) ;
   TEST(0 == free_file(&file)) ;
   TEST(0 == removefile_directory(tempdir, "init1")) ;

   // TEST EEXIST
   TEST(0 == makefile_directory(tempdir, "init1", 0)) ;
   TEST(EEXIST == initcreat_file(&file, "init1", tempdir)) ;
   TEST(-1 == file) ;
   TEST(0 == removefile_directory(tempdir, "init1")) ;

   // TEST EINVAL
   TEST(0 == makefile_directory(tempdir, "init1", 0)) ;
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_EXEC, tempdir)) ;
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_PRIVATE, tempdir)) ;
   TEST(EINVAL == init_file(&file, "init1", accessmode_READ|accessmode_SHARED, tempdir)) ;
   TEST(0 == removefile_directory(tempdir, "init1")) ;

   return 0 ;
ABBRUCH:
   free_file(&file) ;
   removefile_directory(tempdir, "init1") ;
   removefile_directory(tempdir, "init2") ;
   return EINVAL ;
}

static int test_readwrite(directory_t * tempdir)
{
   file_t   file = file_INIT_FREEABLE ;
   off_t    size ;

   // TEST create / write
   TEST(ENOENT == checkpath_directory(tempdir, "testwrite")) ;
   TEST(0 == initcreat_file(&file, "testwrite", tempdir)) ;
   TEST(0 == filesize_directory(tempdir, "testwrite", &size)) ;
   TEST(0 == size) ;
   for(int i = 0; i < 10; ++i) {
      uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ;
      size_t  written    = 0 ;
      TEST(0 == write_file(&file, 1+(size_t)i, buffer, &written)) ;
      TEST(1+i == (int)written) ;
      TEST(0 == filesize_directory(tempdir, "testwrite", &size)) ;
      TEST((1+i)*(2+i)/2 == size) ;
   }
   TEST(0 == filesize_directory(tempdir, "testwrite", &size)) ;
   TEST(55 == size) ;
   TEST(0 == free_file(&file)) ;

   // TEST write2
   TEST(0 == init_file(&file, "testwrite", accessmode_WRITE, tempdir)) ;
   TEST(55 == lseek(fd_file(&file), 55, SEEK_SET)) ;
   TEST(0 == write_file(&file, 2, (const uint8_t*)"xy", 0)) ;
   TEST(0 == free_file(&file)) ;
   TEST(0 == filesize_directory(tempdir, "testwrite", &size)) ;
   TEST(57 == size) ;

   // TEST read
   TEST(0 == init_file(&file, "testwrite", accessmode_READ, tempdir)) ;
   for(int i = 0; i < 10; ++i) {
      uint8_t buffer[10]  = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ;
      uint8_t buffer2[10] = { 0 } ;
      size_t  readbytes   = 0 ;
      TEST(0 == read_file(&file, 1+(size_t)i, buffer2, &readbytes)) ;
      TEST(1+i == (int)readbytes) ;
      TEST(0 == memcmp(buffer, buffer2, 1+(size_t)i)) ;
   }

   // TEST read2 (end of file)
   {
      uint8_t buffer[4] = { 0, 0 } ;
      size_t  readbytes = 0 ;
      // returns only last 2 bytes
      TEST(0 == read_file(&file, 4, buffer, &readbytes)) ;
      TEST(2 == readbytes) ;
      TEST(0 == memcmp(buffer, "xy", 2)) ;
      // end of file => 0
      TEST(0 == read_file(&file, 4, buffer, &readbytes)) ;
      TEST(0 == readbytes) ;
   }
   TEST(0 == free_file(&file)) ;

   // unprepare
   TEST(0 == removefile_directory(tempdir, "testwrite")) ;

   return 0 ;
ABBRUCH:
   free_file(&file) ;
   removefile_directory(tempdir, "testwrite") ;
   return EINVAL ;
}

int unittest_io_file()
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;
   directory_t     * tempdir = 0 ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == newtemp_directory(&tempdir, "iofiletest")) ;

   if (test_initfree(tempdir))   goto ABBRUCH ;
   if (test_readwrite(tempdir))  goto ABBRUCH ;

   // adapt LOG
   char * logbuffer ;
   size_t logbuffer_size ;
   LOG_GETBUFFER( &logbuffer, &logbuffer_size ) ;
   if (logbuffer_size) {
      char * found = logbuffer ;
      while ( (found=strstr( found, tempdir->path )) ) {
         if (!strchr(found, '.')) break ;
         memcpy( strchr(found, '.')+1, "123456", 6 ) ;
         found += strlen(tempdir->path) ;
      }
   }

   TEST(0 == remove_directory(tempdir)) ;
   TEST(0 == delete_directory(&tempdir)) ;
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) delete_directory(&tempdir) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
