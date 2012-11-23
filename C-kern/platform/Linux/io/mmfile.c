/* title: Memory-Mapped-File Linux
   Implements <Memory-Mapped-File> on Linux.

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

   file: C-kern/api/io/filesystem/mmfile.h
    Header file of <Memory-Mapped-File>.

   file: C-kern/platform/Linux/io/mmfile.c
    Linux specific implementation <Memory-Mapped-File Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/platform/virtmemory.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/string/cstring.h"
#endif

// section: mmfile_t

// group: helper

static int init2_mmfile(/*out*/mmfile_t * mfile, int fd, off_t file_offset, size_t size, mmfile_openmode_e mode)
{
   int err ;
   const size_t    pagesize  = pagesize_vm() ;
   void          * mem_start = MAP_FAILED ;

   struct stat file_info ;
   err = fstat(fd, &file_info) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("fstat", err) ;
      goto ONABORT ;
   }

   if (file_info.st_size < file_offset) {
      err = ENODATA ;
      goto ONABORT ;
   }

   off_t filesize_remaining = file_info.st_size - file_offset ;

   if (!size) {
      if (filesize_remaining >= (size_t)-1) {
         err = ENOMEM ;
         goto ONABORT ;
      }
      size = (size_t) filesize_remaining ;
   } else {
      size_t aligned_size = ((size + (pagesize - 1)) & ~(pagesize - 1)) ;
      if (aligned_size > size) {
         size = aligned_size ;
      }
      if (size > filesize_remaining) {
         size = (size_t) filesize_remaining ;
      }
   }

   if (size) {
#define protection_flags   ((mode & accessmode_WRITE)   ? (PROT_READ|PROT_WRITE) : PROT_READ)
#define shared_flags       ((mode & accessmode_PRIVATE) ? MAP_PRIVATE : MAP_SHARED)
      mem_start = mmap(0, size, protection_flags, shared_flags, fd, file_offset) ;
#undef  shared_flags
#undef  protection_flags
      if (MAP_FAILED == mem_start) {
         err = errno ;
         TRACESYSERR_LOG("mmap", err) ;
         goto ONABORT ;
      }

      err = madvise(mem_start, size, MADV_SEQUENTIAL) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("madvise", err) ;
         goto ONABORT ;
      }
   } else {
      mem_start = 0 ;
   }

   mfile->addr        = mem_start ;
   mfile->size        = size ;

   return 0 ;
ONABORT:
   if (MAP_FAILED != mem_start) {
      (void) munmap(mem_start, size) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: implementation

size_t pagesize_mmfile()
{
   return pagesize_vm() ;
}

int init_mmfile(/*out*/mmfile_t * mfile, const char * file_path, off_t file_offset, size_t size, mmfile_openmode_e mode, const struct directory_t * relative_to)
{
   int err ;
   int             fd        = -1 ;
   const size_t    pagesize  = pagesize_vm() ;
   int             openatfd  = AT_FDCWD ;

   VALIDATE_INPARAM_TEST(0 <= file_offset && 0 == (file_offset % pagesize), ONABORT, PRINTUINT64_LOG(file_offset)) ;

   VALIDATE_INPARAM_TEST(
            (mode & accessmode_READ)
         && !(mode & (typeof(mode))(~(accessmode_READ|accessmode_WRITE|accessmode_SHARED|accessmode_PRIVATE)))
         &&  (mode & (accessmode_SHARED|accessmode_PRIVATE)) != (accessmode_SHARED|accessmode_PRIVATE)
         &&  (    !(mode & accessmode_WRITE)
               || (mode & (accessmode_SHARED|accessmode_PRIVATE)) ),
         ONABORT, PRINTINT_LOG(mode)) ;

   if (relative_to) {
      openatfd = fd_directory(relative_to) ;
   }

   fd = openat(openatfd, file_path, ((mode&accessmode_WRITE) ? O_RDWR : O_RDONLY) | O_CLOEXEC ) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("openat", err) ;
      PRINTCSTR_LOG(file_path) ;
      goto ONABORT ;
   }

   err = init2_mmfile(mfile, fd, file_offset, size, mode) ;
   if (err) goto ONABORT ;

   (void) free_filedescr(&fd) ;

   return 0 ;
ONABORT:
   free_filedescr(&fd) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int initcreate_mmfile(/*out*/mmfile_t * mfile, const char * file_path, size_t size, const struct directory_t * relative_to)
{
   int err ;
   int  fd       = -1 ;
   int  openatfd = AT_FDCWD ;

   VALIDATE_INPARAM_TEST(0 != size, ONABORT, ) ;

   if (relative_to) {
      openatfd = fd_directory(relative_to) ;
   }

   fd = openat(openatfd, file_path, O_RDWR|O_EXCL|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("openat", err) ;
      PRINTCSTR_LOG(file_path) ;
      goto ONABORT ;
   }

   err = ftruncate(fd, size) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("ftruncate", err) ;
      PRINTSIZE_LOG(size) ;
      goto ONABORT ;
   }

   err = init2_mmfile(mfile, fd, 0, size, mmfile_openmode_RDWR_SHARED) ;
   if (err) goto ONABORT ;

   (void) free_filedescr(&fd) ;

   return 0 ;
ONABORT:
   if (-1 != fd) {
      (void) unlinkat(openatfd, file_path, 0) ;
      free_filedescr(&fd) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_mmfile(mmfile_t * mfile)
{
   int err ;

   if (0 != mfile->size) {

      err = munmap( mfile->addr, mfile->size ) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("munmap", err) ;
         PRINTPTR_LOG(mfile->addr) ;
         PRINTSIZE_LOG(mfile->size) ;
      }
      mfile->addr = 0 ;
      mfile->size = 0 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

static int test_alignedsize(void)
{
   size_t   pagesize = pagesize_vm() ;
   mmfile_t    mfile = mmfile_INIT_FREEABLE ;

   for (size_t i = 0, aligned2 = 0; i < 5*pagesize; ++i) {
      mfile.size = i ;
      if (1 == (i % pagesize)) {
         aligned2 += pagesize ;
      }
      size_t aligned  = alignedsize_mmfile(&mfile) ;
      TEST(aligned2 == aligned) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_creat_mmfile(directory_t * tempdir, const char* tmppath)
{
   size_t   pagesize = pagesize_vm() ;
   mmfile_t    mfile = mmfile_INIT_FREEABLE ;
   int            fd = -1 ;

   // TEST init, double free
   TEST(0 == initcreate_mmfile(&mfile, "mmfile", 1, tempdir)) ;
   TEST(mfile.addr             != 0) ;
   TEST(mfile.size             == 1) ;
   TEST(alignedsize_mmfile(&mfile) == pagesize) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(mfile.addr             ==  0) ;
   TEST(mfile.size             ==  0) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == removefile_directory(tempdir, "mmfile")) ;

   // TEST write
   TEST(0 == initcreate_mmfile(&mfile, "mmfile", 111, tempdir)) ;
   TEST(mfile.addr             != 0) ;
   TEST(mfile.addr             == addr_mmfile(&mfile)) ;
   TEST(mfile.size             == 111) ;
   TEST(mfile.size             == size_mmfile(&mfile)) ;
   TEST(pagesize               == alignedsize_mmfile(&mfile)) ;
   uint8_t * memory = addr_mmfile(&mfile) ;
   for (uint8_t i = 0; i < 111; ++i) {
      memory[i] = i ;
   }
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(mfile.addr             ==  0) ;
   TEST(mfile.size             ==  0) ;
   TEST(alignedsize_mmfile(&mfile) ==  0) ;
   // read written content
   {
      fd = openat(fd_directory(tempdir), "mmfile", O_RDONLY|O_CLOEXEC) ;
      TEST(fd > 0) ;
      uint8_t buffer[111] = {0} ;
      TEST(111 == read(fd, buffer, 111)) ;
      for (uint8_t i = 0; i < 111; ++i) {
         TEST(i == buffer[i]) ;
      }
      TEST(0 == free_filedescr(&fd)) ;
   }
   TEST(0 == removefile_directory(tempdir, "mmfile")) ;

   // TEST mapping is shared
   {
      char pathname[ strlen("/mmfile") + strlen(tmppath) + 1 ] ;
      strcpy( pathname, tmppath ) ;
      strcat( pathname, "/mmfile" ) ;
      TEST(0 == initcreate_mmfile(&mfile, pathname, 111, 0)) ;
   }
   TEST(mfile.addr             != 0) ;
   TEST(mfile.addr             == addr_mmfile(&mfile)) ;
   TEST(mfile.size             == 111) ;
   TEST(mfile.size             == size_mmfile(&mfile)) ;
   TEST(pagesize               == alignedsize_mmfile(&mfile)) ;
   memory = addr_mmfile(&mfile) ;
   for (uint8_t i = 0; i < 111; ++i) {
      memory[i] = '2' ;
   }
   for (uint8_t i = 0; i < 111; ++i) {
      TEST('2' == memory[i]) ;
   }
   // write different content
   {
      fd = openat(fd_directory(tempdir), "mmfile", O_WRONLY|O_CLOEXEC) ;
      TEST(fd > 0) ;
      uint8_t buffer[111] ;
      memset( buffer, '3', 111 ) ;
      TEST(111 == write(fd, buffer, 111)) ;
      TEST(0 == free_filedescr(&fd)) ;
   }
   for (uint8_t i = 0; i < 111; ++i) {
      TEST('3' == memory[i]) ;
   }
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == removefile_directory(tempdir, "mmfile")) ;

   // TEST EINVAL
   {
      // size == 0
      TEST(EINVAL == initcreate_mmfile(&mfile, "mmfile", 0, tempdir)) ;
      // offset < 0
      TEST(EINVAL == init_mmfile(&mfile, "mmfile", -1, 1, mmfile_openmode_RDWR_SHARED, tempdir)) ;
      // offset != n*pagesize_vm()
      TEST(EINVAL == init_mmfile(&mfile, "mmfile", pagesize_vm()+1, 1, mmfile_openmode_RDWR_SHARED, tempdir)) ;
   }

   // TEST EEXIST
   TEST(0 == initcreate_mmfile(&mfile, "mmfile", 1, tempdir)) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(EEXIST == initcreate_mmfile(&mfile, "mmfile", 1, tempdir)) ;
   TEST(0 == removefile_directory(tempdir, "mmfile")) ;

   return 0 ;
ONABORT:
   free_filedescr(&fd) ;
   (void) free_mmfile(&mfile) ;
   (void) removefile_directory(tempdir, "mmfile") ;
   return 1 ;
}

static ucontext_t s_usercontext ;

static void sigsegfault(int _signr)
{
   (void) _signr ;
   setcontext(&s_usercontext) ;
}

static int test_initfree(directory_t * tempdir, const char * tmppath)
{
   size_t            pagesize = pagesize_vm() ;
   mmfile_t          mfile    = mmfile_INIT_FREEABLE ;
   uint8_t           buffer[256] = { 0 } ;
   int               fd       = -1 ;
   bool              isOldact = false ;
   struct sigaction  newact ;
   struct sigaction  oldact ;

   // prepare
   TEST(0 == initcreate_mmfile(&mfile, "mmfile", 256, tempdir)) ;
   for (unsigned i = 0; i < 256; ++i) {
      addr_mmfile(&mfile)[i] = (uint8_t)i ;
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST mmfile_INIT_FREEABLE
   TEST(0 == mfile.addr) ;
   TEST(0 == mfile.size) ;

   // TEST init, double free
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 0, mmfile_openmode_RDONLY, tempdir)) ;
   TEST(mfile.addr             != 0) ;
   TEST(mfile.size             == 256) ;
   TEST(pagesize               == alignedsize_mmfile(&mfile)) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(mfile.addr             ==  0) ;
   TEST(alignedsize_mmfile(&mfile) == 0) ;
   TEST(mfile.size             ==  0) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(mfile.addr             ==  0) ;
   TEST(mfile.size             ==  0) ;
   TEST(alignedsize_mmfile(&mfile) == 0) ;

   // TEST init_mmfile: offset same size as file
   TEST(0 == makefile_directory(tempdir, "mmpage", pagesize)) ;
   TEST(0 == init_mmfile(&mfile, "mmpage", pagesize, 10, mmfile_openmode_RDONLY, tempdir)) ;
   TEST(0 == mfile.addr) ;
   TEST(0 == mfile.size) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == removefile_directory(tempdir, "mmpage")) ;

   // TEST init_mmfile: empty file
   TEST(0 == makefile_directory(tempdir, "empty", 0)) ;
   TEST(0 == init_mmfile(&mfile, "empty", 0, 0, mmfile_openmode_RDWR_SHARED, tempdir)) ;
   TEST(0 == mfile.addr) ;
   TEST(0 == mfile.size) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == removefile_directory(tempdir, "empty")) ;

   // TEST absolute pathname with no relative tempdir path
   {
      char pathname[ strlen("/mmfile") + strlen(tmppath) + 1 ] ;
      strcpy( pathname, tmppath ) ;
      strcat( pathname, "/mmfile" ) ;
      TEST(0 == init_mmfile(&mfile, pathname, 0, 0, mmfile_openmode_RDONLY, 0)) ;
      TEST(size_mmfile(&mfile)       == 256 ) ;
      TEST(alignedsize_mmfile(&mfile) == pagesize) ;
      for (unsigned i = 0; i < 256; ++i) {
         TEST(addr_mmfile(&mfile)[i] == (uint8_t)i) ;
      }
      TEST(0 == free_mmfile(&mfile)) ;
   }

   // TEST read only / shared
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 0, mmfile_openmode_RDONLY, tempdir)) ;
   TEST(size_mmfile(&mfile)       == 256 ) ;
   TEST(alignedsize_mmfile(&mfile) == pagesize) ;
   for (unsigned i = 0; i < 256; ++i) {
      TEST(addr_mmfile(&mfile)[i] == (uint8_t)i) ;
   }
   // writing generates exception
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags   = 0 ;
   newact.sa_handler = &sigsegfault ;
   TEST(0 == sigaction(SIGSEGV, &newact, &oldact)) ;
   isOldact = true ;
   volatile int is_exception = 0 ;
   TEST(0 == getcontext(&s_usercontext)) ;
   if (!is_exception) {
      is_exception = 1 ;
      addr_mmfile(&mfile)[0] = 1 ;
      ++ is_exception ;
   }
   TEST(1 == is_exception) ;
   // write different content
   fd = openat(fd_directory(tempdir), "mmfile", O_WRONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   memset(buffer, '3', 256 ) ;
   TEST(256 == write(fd, buffer, 256)) ;
   TEST(0   == free_filedescr(&fd)) ;
   // test change is shared
   for (unsigned i = 0; i < 256; ++i) {
      TEST(addr_mmfile(&mfile)[i] == '3') ;
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST RDWR / shared
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 512, mmfile_openmode_RDWR_SHARED, tempdir)) ;
   TEST(size_mmfile(&mfile)       == 256 ) ;
   TEST(alignedsize_mmfile(&mfile) == pagesize) ;
   for (unsigned i = 0; i < 256; ++i) {
      TEST(addr_mmfile(&mfile)[i] == '3') ;
   }
   // write different content
   for (unsigned i = 0; i < 256; ++i) {
      addr_mmfile(&mfile)[i] = (uint8_t)i ;
   }
   // test change is shared
   fd = openat(fd_directory(tempdir), "mmfile", O_RDONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   TEST(256 == read(fd, buffer, 256)) ;
   TEST(0   == free_filedescr(&fd)) ;
   for (unsigned i = 0; i < 256; ++i) {
      TEST(buffer[i] == (uint8_t)i) ;
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST RDWR / private
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 1024, mmfile_openmode_RDWR_PRIVATE, tempdir)) ;
   TEST(size_mmfile(&mfile)       == 256 ) ;
   TEST(alignedsize_mmfile(&mfile) == pagesize) ;
   for (unsigned i = 0; i < 256; ++i) {
      TEST(buffer[i] == (uint8_t)i) ;
   }
   // write different content
   for (unsigned i = 0; i < 256; ++i) {
      addr_mmfile(&mfile)[i] = '1' ;
   }
   for (unsigned i = 0; i < 256; ++i) {
      TEST(addr_mmfile(&mfile)[i] == '1') ;
   }
   // test change is *not* shared
   fd = openat(fd_directory(tempdir), "mmfile", O_RDONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   TEST(256 == read(fd, buffer, 256)) ;
   TEST(0   == free_filedescr(&fd)) ;
   for (unsigned i = 0; i < 256; ++i) {
      TEST(buffer[i] == (uint8_t)i) ; // old content
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST EINVAL
   // mmfile_openmode_e
   TEST(EINVAL == init_mmfile(&mfile, "mmfile", 0, 0, mmfile_openmode_RDWR_PRIVATE+1, tempdir)) ;
   // offset not page aligned
   TEST(EINVAL == init_mmfile(&mfile, "mmfile", pagesize-1, 0, mmfile_openmode_RDWR_PRIVATE, tempdir)) ;

   // TEST ENODATA
   // offset > filesize
   TEST(ENODATA == init_mmfile(&mfile, "mmfile", pagesize, 0, mmfile_openmode_RDWR_PRIVATE, tempdir)) ;

   // TEST ENOMEM
   if (sizeof(size_t) == sizeof(uint32_t)) {
      fd = openat(fd_directory(tempdir), "mmfile", O_WRONLY|O_CLOEXEC) ;
      TEST(fd > 0) ;
      TEST(0 == ftruncate(fd, (size_t)-1)) ; // 4 GB
      TEST(0 == free_filedescr(&fd)) ;
      // mmap is never called
      TEST(ENOMEM == init_mmfile(&mfile, "mmfile", 0, 0, mmfile_openmode_RDWR_PRIVATE, tempdir)) ;
      // mmap returns ENOMEM (size == 0)
      TEST(ENOMEM == init_mmfile(&mfile, "mmfile", pagesize, 0, mmfile_openmode_RDWR_PRIVATE, tempdir)) ;
      // mmap returns ENOMEM (size != 0)
      TEST(ENOMEM == init_mmfile(&mfile, "mmfile", 0, (size_t)0xffffffff - pagesize +2, mmfile_openmode_RDWR_PRIVATE, tempdir)) ;
   }
   TEST(0 == removefile_directory(tempdir, "mmfile")) ;

   isOldact = 0 ;
   TEST(0 == sigaction(SIGSEGV, &oldact, 0)) ;

   return 0 ;
ONABORT:
   if (isOldact) sigaction(SIGSEGV, &oldact, 0) ;
   free_filedescr(&fd) ;
   (void) free_mmfile(&mfile) ;
   (void) removefile_directory(tempdir, "mmfile") ;
   return EINVAL ;
}

static int test_writeoffset(directory_t * tempdir)
{
   size_t   pagesize = pagesize_vm() ;
   mmfile_t mfile    = mmfile_INIT_FREEABLE ;

   // create content
   TEST(0 == initcreate_mmfile(&mfile, "mmfile", 10*pagesize, tempdir)) ;
   memset( addr_mmfile(&mfile), 0, 10*pagesize) ;
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST offset is correctly calculated
   for (int i = 0; i < 10; ++i) {
      off_t offset = (size_t)i * pagesize ;
      TEST(0 == init_mmfile(&mfile, "mmfile", offset, pagesize, mmfile_openmode_RDWR_SHARED, tempdir)) ;
      memset( addr_mmfile(&mfile), 1+i, pagesize) ;
      TEST(0 == free_mmfile(&mfile)) ;

      TEST(0 == init_mmfile(&mfile, "mmfile", 0, 10*pagesize, mmfile_openmode_RDWR_SHARED, tempdir)) ;
      for (int j = 0; j <= i; ++j) {
         for (size_t x = (size_t)j*pagesize; x < (size_t)(j+1)*pagesize; ++x) {
            TEST((j+1) == addr_mmfile(&mfile)[x]) ;
         }
      }
      for (size_t x = (size_t)offset + pagesize; x < 10*pagesize; ++x) {
         TEST(0 == addr_mmfile(&mfile)[x]) ;
      }
      TEST(0 == free_mmfile(&mfile)) ;
   }

   TEST(0 == removefile_directory(tempdir, "mmfile")) ;

   return 0 ;
ONABORT:
   (void) free_mmfile(&mfile) ;
   (void) removefile_directory(tempdir, "mmfile") ;
   return EINVAL ;
}

int unittest_io_mmfile()
{
   directory_t     * tempdir = 0 ;
   const char      * tmpstr  = 0 ;
   cstring_t         tmppath = cstring_INIT ;
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;
   TEST(0 == newtemp_directory(&tempdir, "mmfile", &tmppath )) ;

   tmpstr = str_cstring(&tmppath) ;

   if (test_alignedsize())                   goto ONABORT ;
   if (test_creat_mmfile(tempdir, tmpstr))   goto ONABORT ;
   if (test_initfree(tempdir, tmpstr))       goto ONABORT ;
   if (test_writeoffset(tempdir))            goto ONABORT ;

   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_cstring(&tmppath) ;
   (void) delete_directory(&tempdir) ;
   (void) free_resourceusage(&usage) ;
   return 1 ;
}

#endif
