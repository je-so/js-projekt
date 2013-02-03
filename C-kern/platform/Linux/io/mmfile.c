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
#include "C-kern/api/err.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: mmfile_t

// group: lifetime

static int init2_mmfile(/*out*/mmfile_t * mfile, void * addr, sys_file_t fd, off_t file_offset, size_t size, accessmode_e mode)
{
   int err ;
   const size_t    pagesize  = pagesize_vm() ;
   void          * mem_start = 0 ;

   VALIDATE_INPARAM_TEST(0 <= file_offset && 0 == (file_offset % pagesize), ONABORT, PRINTUINT64_LOG(file_offset)) ;

   VALIDATE_INPARAM_TEST(  mode == accessmode_READ
                           || mode == accessmode_RDWR_PRIVATE
                           || mode == accessmode_RDWR_SHARED, ONABORT, PRINTINT_LOG(mode)) ;

   accessmode_e fdmode = accessmode_file(fd) ;

   VALIDATE_INPARAM_TEST(  0 != (fdmode & accessmode_READ)
                           && (mode == accessmode_READ
                              || 0 != (fdmode & accessmode_WRITE)), ONABORT, PRINTINT_LOG(fdmode)) ;

   if (size) {
      const int protection = (mode & accessmode_WRITE)   ? (PROT_READ|PROT_WRITE) : PROT_READ ;
      const int flags      = (addr ? MAP_FIXED : 0) | ((mode & accessmode_PRIVATE) ? MAP_PRIVATE : MAP_SHARED) ;
      mem_start = mmap(addr, size, protection, flags, fd, file_offset) ;
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
   }

   mfile->addr = mem_start ;
   mfile->size = size ;

   return 0 ;
ONABORT:
   if (mem_start && MAP_FAILED != mem_start) {
      (void) munmap(mem_start, size) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int initfd_mmfile(/*out*/mmfile_t * mfile, sys_file_t fd, off_t file_offset, size_t size, accessmode_e mode)
{
   int err ;

   err = init2_mmfile(mfile, 0, fd, file_offset, size, mode) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int init_mmfile(/*out*/mmfile_t * mfile, const char * file_path, off_t file_offset, size_t size, accessmode_e mode, const struct directory_t * relative_to)
{
   int err ;
   int             fd        = -1 ;
   const size_t    pagesize  = pagesize_vm() ;
   int             openatfd  = AT_FDCWD ;

   if (relative_to) {
      openatfd = fd_directory(relative_to) ;
   }

   fd = openat(openatfd, file_path, ((mode&accessmode_WRITE) ? O_RDWR : O_RDONLY) | O_CLOEXEC) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("openat", err) ;
      PRINTCSTR_LOG(file_path) ;
      goto ONABORT ;
   }

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

   err = init2_mmfile(mfile, 0, fd, file_offset, size, mode) ;
   if (err) goto ONABORT ;

   (void) free_file(&fd) ;

   return 0 ;
ONABORT:
   free_file(&fd) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int initsplit_mmfile(/*out*/mmfile_t * destheadmfile, /*out*/mmfile_t * desttailmfile, size_t headsize, mmfile_t * sourcemfile)
{
   int err ;
   const size_t pagesize = pagesize_vm() ;

   VALIDATE_INPARAM_TEST(0 < headsize && headsize < size_mmfile(sourcemfile) && 0 == (headsize % pagesize), ONABORT, PRINTSIZE_LOG(headsize); PRINTSIZE_LOG(size_mmfile(sourcemfile))) ;

   destheadmfile->addr = sourcemfile->addr ;
   desttailmfile->addr = sourcemfile->addr + headsize ;
   desttailmfile->size = sourcemfile->size - headsize ;
   destheadmfile->size = headsize ;

   if (sourcemfile != destheadmfile && sourcemfile != desttailmfile) {
      *sourcemfile = (mmfile_t) mmfile_INIT_FREEABLE ;
   }

   return 0 ;
ONABORT:
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

// group: change

int seek_mmfile(mmfile_t * mfile, sys_file_t fd, off_t file_offset, accessmode_e mode)
{
   int err ;

   err = init2_mmfile(mfile, mfile->addr, fd, file_offset, mfile->size, mode) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_query(void)
{
   size_t   pagesize = pagesize_vm() ;
   mmfile_t    mfile = mmfile_INIT_FREEABLE ;

   // TEST addr_mmfile
   mfile.addr = (void*) 1234 ;
   TEST(1234 == (uintptr_t) addr_mmfile(&mfile)) ;
   mfile.addr = (void*) 0 ;
   TEST(0 == addr_mmfile(&mfile)) ;

   // TEST alignedsize_mmfile
   for (size_t i = 0, aligned2 = 0; i < 5*pagesize; ++i) {
      mfile.size = i ;
      if (1 == (i % pagesize)) {
         aligned2 += pagesize ;
      }
      size_t aligned  = alignedsize_mmfile(&mfile) ;
      TEST(aligned2 == aligned) ;
   }

   // TEST isinit_mmfile
   mfile = (mmfile_t) mmfile_INIT_FREEABLE ;
   TEST(false == isinit_mmfile(&mfile)) ;
   mfile.addr = (void*) 1 ;
   TEST(true == isinit_mmfile(&mfile)) ;
   mfile.addr = (void*) 0 ;
   TEST(false == isinit_mmfile(&mfile)) ;
   mfile.size = 1u ;
   TEST(true == isinit_mmfile(&mfile)) ;
   mfile.size = 0 ;
   TEST(false == isinit_mmfile(&mfile)) ;

   // TEST size_mmfile
   mfile.size = 4567 ;
   TEST(4567 == size_mmfile(&mfile)) ;
   mfile.size = 0 ;
   TEST(0 == size_mmfile(&mfile)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
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
   mmfile_t          split[2] = { mmfile_INIT_FREEABLE, mmfile_INIT_FREEABLE } ;
   uint8_t           buffer[256] = { 0 } ;
   int               fd       = -1 ;
   bool              isOldact = false ;
   struct sigaction  newact ;
   struct sigaction  oldact ;

   // prepare
   fd = openat(fd_directory(tempdir), "mmfile", O_RDWR|O_EXCL|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   TEST(0 <= fd) ;
   TEST(0 == ftruncate(fd, 256)) ;
   for (unsigned i = 0; i < 256; ++i) {
      buffer[i] = (uint8_t)i ;
   }
   TEST(256 == write(fd, buffer, 256)) ;
   TEST(0 == free_file(&fd)) ;
   fd = openat(fd_directory(tempdir), "mmsplit", O_RDWR|O_EXCL|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   TEST(0 <= fd) ;
   TEST(0 == ftruncate(fd, 2*pagesize+sizeof(uint32_t))) ;
   TEST(0 == (pagesize % sizeof(uint32_t))) ;
   for (uint32_t i = 0; i < 2*pagesize+sizeof(uint32_t); i += sizeof(uint32_t)) {
      TEST(sizeof(uint32_t) == write(fd, &i, sizeof(uint32_t))) ;
   }
   TEST(0 == free_file(&fd)) ;

   // TEST mmfile_INIT_FREEABLE
   TEST(0 == mfile.addr) ;
   TEST(0 == mfile.size) ;

   // TEST init, double free
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 0, accessmode_READ, tempdir)) ;
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
   TEST(0 == init_mmfile(&mfile, "mmpage", pagesize, 10, accessmode_READ, tempdir)) ;
   TEST(0 == mfile.addr) ;
   TEST(0 == mfile.size) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == removefile_directory(tempdir, "mmpage")) ;

   // TEST init_mmfile: empty file
   TEST(0 == makefile_directory(tempdir, "empty", 0)) ;
   TEST(0 == init_mmfile(&mfile, "empty", 0, 0, accessmode_RDWR_SHARED, tempdir)) ;
   TEST(0 == mfile.addr) ;
   TEST(0 == mfile.size) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == removefile_directory(tempdir, "empty")) ;

   // TEST init_mmfile: absolute pathname with no relative tempdir path
   {
      char pathname[strlen("/mmfile") + strlen(tmppath) + 1] ;
      strcpy(pathname, tmppath) ;
      strcat(pathname, "/mmfile") ;
      TEST(0 == init_mmfile(&mfile, pathname, 0, 0, accessmode_READ, 0)) ;
      TEST(size_mmfile(&mfile)        == 256) ;
      TEST(alignedsize_mmfile(&mfile) == pagesize) ;
      for (unsigned i = 0; i < 256; ++i) {
         TEST(addr_mmfile(&mfile)[i] == (uint8_t)i) ;
      }
      TEST(0 == free_mmfile(&mfile)) ;
   }

   // TEST init_mmfile: read only / shared
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 0, accessmode_READ, tempdir)) ;
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
   TEST(0   == free_file(&fd)) ;
   // test change is shared
   for (unsigned i = 0; i < 256; ++i) {
      TEST(addr_mmfile(&mfile)[i] == '3') ;
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST init_mmfile: RDWR / shared
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 512, accessmode_RDWR_SHARED, tempdir)) ;
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
   TEST(0   == free_file(&fd)) ;
   for (unsigned i = 0; i < 256; ++i) {
      TEST(buffer[i] == (uint8_t)i) ;
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST init_mmfile: RDWR / private
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 1024, accessmode_RDWR_PRIVATE, tempdir)) ;
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
   TEST(0   == free_file(&fd)) ;
   for (unsigned i = 0; i < 256; ++i) {
      TEST(buffer[i] == (uint8_t)i) ; // old content
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST initmove_mmfile
   for (unsigned i = 0; i < 256; ++i) {
      mmfile_t destmfile = mmfile_INIT_FREEABLE ;
      mmfile_t sourcemfile = { .addr = (uint8_t*)(100+i), .size = i } ;
      TEST(isinit_mmfile(&sourcemfile)) ;
      initmove_mmfile(&destmfile, &sourcemfile) ;
      TEST(!isinit_mmfile(&sourcemfile)) /*source is reset*/ ;
      TEST(addr_mmfile(&destmfile) == (uint8_t*)(100+i)) /*dest contains former addr of source*/ ;
      TEST(size_mmfile(&destmfile) == i) /*dest contains former size of source*/ ;
   }

   // TEST initmove_mmfile: the same address twice does not work
   mfile.addr = (uint8_t*)1 ;
   mfile.size = 1u ;
   initmove_mmfile(&mfile, &mfile) ;
   TEST(!isinit_mmfile(&mfile)) /*resets mfile instead of doing nothing*/ ;

   // TEST initsplit_mmfile
   for (size_t splitoffset = pagesize; splitoffset <= 2*pagesize; splitoffset += pagesize) {
      TEST(0 == init_mmfile(&mfile, "mmsplit", 0, 0, accessmode_READ, tempdir)) ;
      uint8_t  * addr = addr_mmfile(&mfile) ;
      size_t   size   = size_mmfile(&mfile) ;
      TEST(size == 2*pagesize+sizeof(uint32_t)) ;
      TEST(0 == initsplit_mmfile(&split[0], &split[1], splitoffset, &mfile)) ;
      TEST(!isinit_mmfile(&mfile)) /*source is reset*/ ;
      // head
      TEST(split[0].addr == addr) ;
      TEST(split[0].size == splitoffset) ;
      for (uint32_t i = 0; i < splitoffset; i += sizeof(uint32_t)) {
         TEST(*(uint32_t*)(&addr_mmfile(&split[0])[i]) == i) ;
      }
      // tail
      TEST(split[1].addr == addr + splitoffset) ;
      TEST(split[1].size == size - splitoffset) ;
      for (uint32_t i = 0; i < size-splitoffset; i += sizeof(uint32_t)) {
         TEST(*(uint32_t*)(&addr_mmfile(&split[1])[i]) == splitoffset+i) ;
      }
      TEST(0 == free_mmfile(&split[0])) ;
      TEST(0 == free_mmfile(&split[1])) ;
   }

   // TEST initsplit_mmfile: source equals head or tail
   for (size_t splitoffset = pagesize; splitoffset <= 2*pagesize; splitoffset += pagesize) {
      for (unsigned si = 0; si < 2; ++si) {
         TEST(0 == init_mmfile(&split[si], "mmsplit", 0, 0, accessmode_READ, tempdir)) ;
         uint8_t  * addr = addr_mmfile(&split[si]) ;
         size_t   size   = size_mmfile(&split[si]) ;
         TEST(0 == initsplit_mmfile(&split[0], &split[1], splitoffset, &split[si])) ;
         TEST(split[0].addr == addr) ;
         TEST(split[0].size == splitoffset) ;
         for (uint32_t i = 0; i < splitoffset; i += sizeof(uint32_t)) {
            TEST(*(uint32_t*)(&addr_mmfile(&split[0])[i]) == i) ;
         }
         TEST(split[1].addr == addr + splitoffset) ;
         TEST(split[1].size == size - splitoffset) ;
         for (uint32_t i = 0; i < size-splitoffset; i += sizeof(uint32_t)) {
            TEST(*(uint32_t*)(&addr_mmfile(&split[1])[i]) == splitoffset+i) ;
         }
         TEST(0 == free_mmfile(&split[0])) ;
         TEST(0 == free_mmfile(&split[1])) ;
      }
   }

   // TEST initsplit_mmfile: dest(tail + head) equals source does not work
   TEST(0 == init_mmfile(&mfile, "mmsplit", 0, 0, accessmode_READ, tempdir)) ;
   split[0] = mfile ;
   TEST(0 == initsplit_mmfile(&split[0], &split[0], pagesize, &split[0])) ;
   TEST(addr_mmfile(&split[0]) == addr_mmfile(&mfile)+pagesize) /*value from tail*/ ;
   TEST(size_mmfile(&split[0]) == pagesize) /*value from head*/ ;
   split[0] = (mmfile_t) mmfile_INIT_FREEABLE ;
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST init_mmfile, initfd_mmfile: EINVAL
   fd = openat(fd_directory(tempdir), "mmfile", O_RDONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   // accessmode_e
   TEST(EINVAL == init_mmfile(&mfile, "mmfile", 0, 0, accessmode_RDWR_PRIVATE+accessmode_EXEC, tempdir)) ;
   TEST(EINVAL == initfd_mmfile(&mfile, fd, 0, 0, accessmode_RDWR_PRIVATE+accessmode_SHARED)) ;
   // offset not page aligned
   TEST(EINVAL == init_mmfile(&mfile, "mmfile", 1, 0, accessmode_RDWR_PRIVATE, tempdir)) ;
   TEST(EINVAL == initfd_mmfile(&mfile, fd, 1, 0, accessmode_RDWR_PRIVATE)) ;
   // file must always be opened with write access
   TEST(EINVAL == initfd_mmfile(&mfile, fd, 0, 1, accessmode_RDWR_PRIVATE)) ;
   TEST(EINVAL == initfd_mmfile(&mfile, fd, 0, 1, accessmode_RDWR_SHARED)) ;
   // file must always be opened with read access
   TEST(0 == free_file(&fd)) ;
   fd = openat(fd_directory(tempdir), "mmfile", O_WRONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   TEST(EINVAL == initfd_mmfile(&mfile, fd, 0, 1, accessmode_READ)) ;
   TEST(0 == free_file(&fd)) ;

   // initsplit_mmfile: EINVAL
   TEST(0 == init_mmfile(&mfile, "mmsplit", 0, 0, accessmode_READ, tempdir)) ;
   // offset 0 not supported
   TEST(EINVAL == initsplit_mmfile(&split[0], &split[1], 0, &mfile)) ;
   // offset not page aligned
   TEST(EINVAL == initsplit_mmfile(&split[0], &split[1], pagesize-1, &mfile)) ;
   // offset larger than filesize
   TEST(EINVAL == initsplit_mmfile(&split[0], &split[1], 3*pagesize, &mfile)) ;
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST init_mmfile: ENODATA
   // offset > filesize
   TEST(ENODATA == init_mmfile(&mfile, "mmfile", pagesize, 0, accessmode_RDWR_PRIVATE, tempdir)) ;

   // TEST init_mmfile: ENOMEM
   if (sizeof(size_t) == sizeof(uint32_t)) {
      fd = openat(fd_directory(tempdir), "mmfile", O_WRONLY|O_CLOEXEC) ;
      TEST(fd > 0) ;
      TEST(0 == ftruncate(fd, (size_t)-1)) ; // 4 GB
      TEST(0 == free_file(&fd)) ;
      // mmap is never called
      TEST(ENOMEM == init_mmfile(&mfile, "mmfile", 0, 0, accessmode_RDWR_PRIVATE, tempdir)) ;
      // mmap returns ENOMEM (size == 0)
      TEST(ENOMEM == init_mmfile(&mfile, "mmfile", pagesize, 0, accessmode_RDWR_PRIVATE, tempdir)) ;
      // mmap returns ENOMEM (size != 0)
      TEST(ENOMEM == init_mmfile(&mfile, "mmfile", 0, (size_t)0xffffffff - pagesize +2, accessmode_RDWR_PRIVATE, tempdir)) ;
   }

   // unprepare
   TEST(0 == removefile_directory(tempdir, "mmfile")) ;
   TEST(0 == removefile_directory(tempdir, "mmsplit")) ;
   isOldact = 0 ;
   TEST(0 == sigaction(SIGSEGV, &oldact, 0)) ;

   return 0 ;
ONABORT:
   if (isOldact) sigaction(SIGSEGV, &oldact, 0) ;
   free_file(&fd) ;
   (void) free_mmfile(&mfile) ;
   (void) free_mmfile(&split[0]) ;
   (void) free_mmfile(&split[1]) ;
   (void) removefile_directory(tempdir, "mmfile") ;
   return EINVAL ;
}

static int test_fileoffset(directory_t * tempdir)
{
   size_t   pagesize = pagesize_vm() ;
   mmfile_t mfile    = mmfile_INIT_FREEABLE ;
   int      fd       = -1 ;

   // create content
   fd = openat(fd_directory(tempdir), "mmfile", O_RDWR|O_EXCL|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   TEST(0 <= fd) ;
   TEST(0 == fallocate(fd, 0, 0, 10*pagesize)) ;
   TEST(0 == free_file(&fd)) ;

   // TEST init_mmfile: offset is correctly calculated
   for (int i = 0; i < 10; ++i) {
      // update content
      off_t offset = (size_t)i * pagesize ;
      TEST(0 == init_mmfile(&mfile, "mmfile", offset, pagesize, accessmode_RDWR_SHARED, tempdir)) ;
      memset( addr_mmfile(&mfile), 1+i, pagesize) ;
      TEST(0 == free_mmfile(&mfile)) ;

      // read content
      TEST(0 == init_mmfile(&mfile, "mmfile", 0, 10*pagesize, accessmode_RDWR_SHARED, tempdir)) ;
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

   // unprepare
   TEST(0 == removefile_directory(tempdir, "mmfile")) ;

   return 0 ;
ONABORT:
   (void) free_file(&fd) ;
   (void) free_mmfile(&mfile) ;
   (void) removefile_directory(tempdir, "mmfile") ;
   return EINVAL ;
}

static int test_seek(directory_t * tempdir)
{
   size_t            pagesize = pagesize_vm() ;
   mmfile_t          mfile    = mmfile_INIT_FREEABLE ;
   mmfile_t          split[2] = { mmfile_INIT_FREEABLE, mmfile_INIT_FREEABLE } ;
   file_t            fd       = file_INIT_FREEABLE ;
   const uint16_t    nrpages  = 10 ;
   bool              isoldact = false ;
   struct sigaction  newact ;
   struct sigaction  oldact ;

   // create content
   TEST(0 == initcreate_file(&fd, "mmfile", tempdir)) ;
   TEST(0 == allocate_file(fd, nrpages*pagesize + 1/*test access beyond filelength*/)) ;
   TEST(0 == initfd_mmfile(&mfile, fd, 0, nrpages*pagesize + 1, accessmode_RDWR_SHARED)) ;
   TEST(0 == free_file(&fd)) ;
   for (unsigned i = 0; i < nrpages; ++i) {
      memset(addr_mmfile(&mfile)+i*pagesize, (int)i, pagesize) ;
   }
   addr_mmfile(&mfile)[nrpages*pagesize] = 99 ;
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST seek_mmfile: read file page by page
   TEST(0 == init_file(&fd, "mmfile", accessmode_READ, tempdir)) ;
   TEST(0 == init_mmfile(&mfile, "mmfile", pagesize, pagesize, accessmode_READ, tempdir)) ;
   TEST(1 == addr_mmfile(&mfile)[0]) ;
   TEST(1 == addr_mmfile(&mfile)[pagesize-1]) ;
   for (unsigned i = 0; i < nrpages; ++i) {
      TEST(0 == seek_mmfile(&mfile, fd, i*pagesize, accessmode_READ)) ;
      for (size_t x = 0; x < pagesize; ++x) {
         TEST(i == addr_mmfile(&mfile)[x]) ;
      }
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST seek_mmfile: splitted mfile
   TEST(0 == init_mmfile(&mfile, "mmfile", pagesize, 2*pagesize, accessmode_READ, tempdir)) ;
   TEST(0 == initsplit_mmfile(&split[0], &split[1], pagesize, &mfile)) ;
   for (unsigned si = 0; si < 2; ++si) {
      initmove_mmfile(&mfile, &split[si]) ;
      for (unsigned i = 0; i < nrpages; ++i) {
         TEST(0 == seek_mmfile(&mfile, fd, i*pagesize, accessmode_READ)) ;
         for (size_t x = 0; x < pagesize; ++x) {
            TEST(i == addr_mmfile(&mfile)[x]) ;
         }
      }
      TEST(0 == free_mmfile(&mfile)) ;
   }

   // TEST seek_mmfile: access out of bounds (1 byte of page in use)
   TEST(0 == initfd_mmfile(&mfile, fd, 0, pagesize, accessmode_READ)) ;
   TEST(0 == seek_mmfile(&mfile, fd, nrpages*pagesize, accessmode_READ)) ;
   TEST(99 == addr_mmfile(&mfile)[0]) ;
   TEST(0  == addr_mmfile(&mfile)[1]) ;
   for (size_t x = 1; x < pagesize; ++x) {
      /*out of bound compare 0*/
      TEST(0 == addr_mmfile(&mfile)[x]) ;
   }

   // TEST seek_mmfile: access out of bounds (no byte of page in use)
   TEST(0 == seek_mmfile(&mfile, fd, (nrpages+1u)*pagesize, accessmode_READ)) ;
   sigemptyset(&newact.sa_mask) ;
   newact.sa_flags   = 0 ;
   newact.sa_handler = &sigsegfault ;
   TEST(0 == sigaction(SIGBUS, &newact, &oldact)) ;
   isoldact = true ;
   volatile int is_exception = 0 ;
   TEST(0 == getcontext(&s_usercontext)) ;
   if (!is_exception) {
      is_exception = 1 ;
      TEST(0 == addr_mmfile(&mfile)[0]) ;
      ++ is_exception ;
   }
   TEST(1 == is_exception) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == free_file(&fd)) ;

   // TEST seek_mmfile: EINVAL
   TEST(0 == init_file(&fd, "mmfile", accessmode_READ, tempdir)) ;
   TEST(0 == initfd_mmfile(&mfile, fd, 0, 0, accessmode_READ)) ;
   // write on read only file
   TEST(EINVAL == seek_mmfile(&mfile, fd, 0, accessmode_RDWR_SHARED)) ;
   TEST(EINVAL == seek_mmfile(&mfile, fd, 0, accessmode_RDWR_PRIVATE)) ;
   // accessmode_e
   TEST(EINVAL == seek_mmfile(&mfile, fd, 0, accessmode_RDWR_PRIVATE+accessmode_EXEC)) ;
   TEST(EINVAL == seek_mmfile(&mfile, fd, 0, accessmode_RDWR_PRIVATE+accessmode_SHARED)) ;
   // offset not page aligned
   TEST(EINVAL == seek_mmfile(&mfile, fd, pagesize-1, accessmode_READ)) ;
   // bad file descriptor
   TEST(EINVAL == seek_mmfile(&mfile, file_INIT_FREEABLE, 0, accessmode_READ)) ;
   // file has no read access
   TEST(0 == free_file(&fd)) ;
   TEST(0 == initappend_file(&fd, "mmfile", tempdir)) ;
   TEST(EINVAL == seek_mmfile(&mfile, fd, 0, accessmode_READ)) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == free_file(&fd)) ;

   // unprepare
   isoldact = 0 ;
   TEST(0 == sigaction(SIGBUS, &oldact, 0)) ;
   TEST(0 == removefile_directory(tempdir, "mmfile")) ;

   return 0 ;
ONABORT:
   if (isoldact) sigaction(SIGBUS, &oldact, 0) ;
   (void) free_file(&fd) ;
   (void) free_mmfile(&mfile) ;
   (void) free_mmfile(&split[0]) ;
   (void) free_mmfile(&split[1]) ;
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
   TEST(0 == newtemp_directory(&tempdir, "mmfile", &tmppath)) ;
   tmpstr = str_cstring(&tmppath) ;

   if (test_query())                      goto ONABORT ;
   if (test_initfree(tempdir, tmpstr))    goto ONABORT ;
   if (test_fileoffset(tempdir))          goto ONABORT ;
   if (test_seek(tempdir))                goto ONABORT ;

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
   return EINVAL ;
}

#endif
