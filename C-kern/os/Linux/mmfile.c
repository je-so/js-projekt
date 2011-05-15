/*
   Implements memory mapped files.

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
   (C) 2010 Jörg Seebohn

   file: C-kern/os/Linux/mmfile.c
    Implementation file of <Memory-Mapped File>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/filesystem/mmfile.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/malloctest.h"
#endif


size_t pagesize_mmfile()
{
   return pagesize_vm() ;
}

int init_mmfile( /*out*/mmfile_t * mfile,  const char * file_path, off_t file_offset, size_t size, const struct directory_stream_t * path_relative_to /*0 => current working directory*/, mmfile_openmode_e mode)
{
   int err ;
   int                    fd = -1 ;
   const size_t     pagesize = pagesize_vm() ;
   void          * mem_start = MAP_FAILED ;

   if (     (path_relative_to && !path_relative_to->sys_dir)
            || (unsigned)mode > mmfile_openmode_RDWR_PRIVATE
            || file_offset < 0
            || (file_offset % pagesize) ) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (     mode == mmfile_openmode_CREATE
         && (     file_offset
               || ! size )) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   const mode_t permission = S_IRUSR|S_IWUSR ;
   const int        pathfd = path_relative_to ? dirfd(path_relative_to->sys_dir) : AT_FDCWD ;

   int open_flags ;
   if (mode == mmfile_openmode_RDONLY) {
      open_flags = O_RDONLY ;
   } else if (mode == mmfile_openmode_CREATE) {
      open_flags = O_RDWR|O_EXCL|O_CREAT ;
   } else {
      open_flags = O_RDWR ;
   }
   fd = openat(pathfd, file_path, open_flags|O_CLOEXEC, permission ) ;
   if (fd < 0) {
      err = errno ;
      LOG_SYSERR("openat", err) ;
      LOG_STRING(file_path) ;
      goto ABBRUCH ;
   }

   if (mode == mmfile_openmode_CREATE) {
      err = ftruncate(fd, size) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("ftruncate", err) ;
         LOG_SIZE(size) ;
         goto ABBRUCH ;
      }
   } else {
      struct stat file_info ;
      err = fstat(fd, &file_info) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("fstat", err) ;
         goto ABBRUCH ;
      }

      if (file_info.st_size <= file_offset) {
         err = ENODATA ;
         goto ABBRUCH ;
      }

      off_t size_minus_offset = file_info.st_size - file_offset ;
      if ( !size ) {
         if ( size_minus_offset >= (size_t)-1 ) {
            err = ENOMEM ;
            goto ABBRUCH ;
         }
         size = (size_t) size_minus_offset ;
      } else if (size > size_minus_offset) {
         size = (size_t) size_minus_offset ;
      }
   }

   const size_t aligned_size = (size + pagesize - 1) - ((size + pagesize - 1) % pagesize) ;

   if (aligned_size < size) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(size) ;
      goto ABBRUCH ;
   }

#define protection_flags   ((mode == mmfile_openmode_RDONLY) ? PROT_READ : PROT_READ|PROT_WRITE)
#define shared_flags       ((mode == mmfile_openmode_RDWR_PRIVATE) ? MAP_PRIVATE : MAP_SHARED)
   mem_start = mmap(0, size, protection_flags, shared_flags, fd, 0) ;
#undef  shared_flags
#undef  protection_flags
   if (MAP_FAILED == mem_start) {
      err = errno ;
      LOG_SYSERR("mmap", err) ;
      goto ABBRUCH ;
   }

   err = madvise(mem_start, aligned_size, MADV_SEQUENTIAL) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("madvise", err) ;
      goto ABBRUCH ;
   }

   mfile->sys_file         = fd ;
   mfile->addr             = mem_start ;
   mfile->size_pagealigned = aligned_size ;
   mfile->file_offset      = file_offset ;
   mfile->size             = size ;
   return 0 ;
ABBRUCH:
   if (fd >= 0) {
      if (mode == mmfile_openmode_CREATE) {
         (void) unlinkat(pathfd, file_path, 0) ;
      }
      close(fd) ;
   }
   if (MAP_FAILED != mem_start) {
      (void) munmap(mem_start, size) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int free_mmfile(mmfile_t * mfile)
{
   int err ;
   if (mfile->sys_file >= 0) {
      err = close(mfile->sys_file) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("close", err) ;
         goto ABBRUCH ;
      }
      mfile->sys_file = -1 ;

      err = munmap( mfile->addr, mfile->size_pagealigned ) ;
      if (err) {
         err = errno ;
         LOG_SYSERR("munmap", err) ;
         LOG_PTR(mfile->addr) ;
         LOG_SIZE(mfile->size_pagealigned) ;
         goto ABBRUCH ;
      }
      mfile->addr             = 0 ;
      mfile->size_pagealigned = 0 ;
      mfile->size             = 0 ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_memorymappedfile,ABBRUCH)

static int test_creat_mmfile(directory_stream_t * tempdir)
{
   size_t   pagesize = pagesize_vm() ;
   mmfile_t    mfile = mmfile_INIT_FREEABLE ;
   int            fd = -1 ;

   // TEST init, double free
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 1, tempdir, mmfile_openmode_CREATE)) ;
   TEST(mfile.sys_file         >  0) ;
   TEST(mfile.addr             != 0) ;
   TEST(mfile.size_pagealigned == pagesize) ;
   TEST(mfile.file_offset      == 0) ;
   TEST(mfile.size             == 1) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(mfile.sys_file         == -1) ;
   TEST(mfile.addr             ==  0) ;
   TEST(mfile.size_pagealigned ==  0) ;
   TEST(mfile.file_offset      ==  0) ;
   TEST(mfile.size             ==  0) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == removefile_directorystream(tempdir, "mmfile")) ;

   // TEST write
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 111, tempdir, mmfile_openmode_CREATE)) ;
   TEST(mfile.sys_file         >  0) ;
   TEST(mfile.addr             != 0) ;
   TEST(mfile.addr             == addr_mmfile(&mfile)) ;
   TEST(mfile.size_pagealigned == pagesize) ;
   TEST(mfile.file_offset      == 0) ;
   TEST(mfile.file_offset      == fileoffset_mmfile(&mfile)) ;
   TEST(mfile.size             == 111) ;
   TEST(mfile.size             == size_mmfile(&mfile)) ;
   uint8_t * memory = addr_mmfile(&mfile) ;
   for(uint8_t i = 0; i < 111; ++i) {
      memory[i] = i ;
   }
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(mfile.sys_file         == -1) ;
   TEST(mfile.addr             ==  0) ;
   TEST(mfile.size_pagealigned ==  0) ;
   TEST(mfile.file_offset      ==  0) ;
   TEST(mfile.size             ==  0) ;
   // read written content
   {
      fd = openat(dirfd(tempdir->sys_dir), "mmfile", O_RDONLY|O_CLOEXEC) ;
      TEST(fd > 0) ;
      uint8_t buffer[111] = {0} ;
      TEST(111 == read(fd, buffer, 111)) ;
      for(uint8_t i = 0; i < 111; ++i) {
         TEST(i == buffer[i]) ;
      }
      TEST(0 == close(fd)) ;
      fd = -1 ;
   }
   TEST(0 == removefile_directorystream(tempdir, "mmfile")) ;

   // TEST mapping is shared
   {
      char pathname[ strlen("mmfile") + tempdir->path_len + 1 ] ;
      strncpy( pathname, tempdir->path, tempdir->path_len ) ;
      strcpy( pathname + tempdir->path_len, "mmfile" ) ;
      TEST(0 == init_mmfile(&mfile, pathname, 0, 111, 0, mmfile_openmode_CREATE)) ;
   }
   TEST(mfile.sys_file         >  0) ;
   TEST(mfile.addr             != 0) ;
   TEST(mfile.addr             == addr_mmfile(&mfile)) ;
   TEST(mfile.size_pagealigned == pagesize) ;
   TEST(mfile.file_offset      == 0) ;
   TEST(mfile.file_offset      == fileoffset_mmfile(&mfile)) ;
   TEST(mfile.size             == 111) ;
   TEST(mfile.size             == size_mmfile(&mfile)) ;
   memory = addr_mmfile(&mfile) ;
   for(uint8_t i = 0; i < 111; ++i) {
      memory[i] = '2' ;
   }
   for(uint8_t i = 0; i < 111; ++i) {
      TEST('2' == memory[i]) ;
   }
   // write different content
   {
      fd = openat(dirfd(tempdir->sys_dir), "mmfile", O_WRONLY|O_CLOEXEC) ;
      TEST(fd > 0) ;
      uint8_t buffer[111] ;
      memset( buffer, '3', 111 ) ;
      TEST(111 == write(fd, buffer, 111)) ;
      TEST(0 == close(fd)) ;
      fd = -1 ;
   }
   for(uint8_t i = 0; i < 111; ++i) {
      TEST('3' == memory[i]) ;
   }
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(0 == removefile_directorystream(tempdir, "mmfile")) ;

   // TEST EINVAL
   {
      directory_stream_t dummy  = directory_stream_INIT_FREEABLE ;
      // path (dummy) invalid
      TEST(EINVAL == init_mmfile(&mfile, "mmfile", 0, 1, &dummy, mmfile_openmode_CREATE)) ;
      // offset != 0
      TEST(EINVAL == init_mmfile(&mfile, "mmfile", 1, 1, tempdir, mmfile_openmode_CREATE)) ;
      // size == 0
      TEST(EINVAL == init_mmfile(&mfile, "mmfile", 0, 0, tempdir, mmfile_openmode_CREATE)) ;
   }

   // TEST EEXIST
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 1, tempdir, mmfile_openmode_CREATE)) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(EEXIST == init_mmfile(&mfile, "mmfile", 0, 1, tempdir, mmfile_openmode_CREATE)) ;
   TEST(0 == removefile_directorystream(tempdir, "mmfile")) ;

   return 0 ;
ABBRUCH:
   if (fd > 0) close(fd) ;
   (void) free_mmfile(&mfile) ;
   (void) removefile_directorystream(tempdir, "mmfile") ;
   return 1 ;
}

static ucontext_t s_usercontext ;

static void sigsegfault(int _signr)
{
   (void) _signr ;
   setcontext(&s_usercontext) ;
}

static int test_open_mmfile(directory_stream_t * tempdir)
{
   int err = 1 ;
   size_t        pagesize = pagesize_vm() ;
   mmfile_t         mfile = mmfile_INIT_FREEABLE ;
   uint8_t    buffer[256] = { 0 } ;
   int                 fd = -1 ;
   bool                isOldact = false ;
   struct sigaction    newact ;
   struct sigaction    oldact ;

   // creat content
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 256, tempdir, mmfile_openmode_CREATE)) ;
   for(unsigned i = 0; i < 256; ++i) {
      addr_mmfile(&mfile)[i] = (uint8_t)i ;
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST init, double free
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 0, tempdir, mmfile_openmode_RDONLY)) ;
   TEST(mfile.sys_file          >  0) ;
   TEST(mfile.addr             != 0) ;
   TEST(mfile.size_pagealigned == pagesize) ;
   TEST(mfile.file_offset      == 0) ;
   TEST(mfile.size             == 256) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(mfile.sys_file         == -1) ;
   TEST(mfile.addr             ==  0) ;
   TEST(mfile.size_pagealigned ==  0) ;
   TEST(mfile.file_offset      ==  0) ;
   TEST(mfile.size             ==  0) ;
   TEST(0 == free_mmfile(&mfile)) ;
   TEST(mfile.sys_file         == -1) ;
   TEST(mfile.addr             ==  0) ;
   TEST(mfile.size_pagealigned ==  0) ;
   TEST(mfile.file_offset      ==  0) ;
   TEST(mfile.size             ==  0) ;

   // TEST absolute pathname with no relative tempdir path
   {
      char pathname[ strlen("mmfile") + tempdir->path_len + 1 ] ;
      strncpy( pathname, tempdir->path, tempdir->path_len ) ;
      strcpy( pathname + tempdir->path_len, "mmfile" ) ;
      TEST(0 == init_mmfile(&mfile, pathname, 0, 0, 0, mmfile_openmode_RDONLY)) ;
      TEST(fileoffset_mmfile(&mfile) == 0 ) ;
      TEST(size_mmfile(&mfile)       == 256 ) ;
      TEST(mfile.size_pagealigned    == pagesize) ;
      for(unsigned i = 0; i < 256; ++i) {
         TEST(addr_mmfile(&mfile)[i] == (uint8_t)i) ;
      }
      TEST(0 == free_mmfile(&mfile)) ;
   }

   // TEST read only / shared
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 0, tempdir, mmfile_openmode_RDONLY)) ;
   TEST(fileoffset_mmfile(&mfile) == 0 ) ;
   TEST(size_mmfile(&mfile)       == 256 ) ;
   TEST(mfile.size_pagealigned    == pagesize) ;
   for(unsigned i = 0; i < 256; ++i) {
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
      is_exception = 2 ;
   }
   TEST(1 == is_exception) ;
   // write different content
   fd = openat(dirfd(tempdir->sys_dir), "mmfile", O_WRONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   memset( buffer, '3', 256 ) ;
   TEST(256 == write(fd, buffer, 256)) ;
   TEST(0   == close(fd)) ;
   fd = -1 ;
   // test change is shared
   for(unsigned i = 0; i < 256; ++i) {
      TEST(addr_mmfile(&mfile)[i] == '3') ;
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST RDWR / shared
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 512, tempdir, mmfile_openmode_RDWR_SHARED)) ;
   TEST(fileoffset_mmfile(&mfile) == 0 ) ;
   TEST(size_mmfile(&mfile)       == 256 ) ;
   TEST(mfile.size_pagealigned    == pagesize) ;
   for(unsigned i = 0; i < 256; ++i) {
      TEST(addr_mmfile(&mfile)[i] == '3') ;
   }
   // write different content
   for(unsigned i = 0; i < 256; ++i) {
      addr_mmfile(&mfile)[i] = (uint8_t)i ;
   }
   // test change is shared
   fd = openat(dirfd(tempdir->sys_dir), "mmfile", O_RDONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   TEST(256 == read(fd, buffer, 256)) ;
   TEST(0   == close(fd)) ;
   fd = -1 ;
   for(unsigned i = 0; i < 256; ++i) {
      TEST(buffer[i] == (uint8_t)i) ;
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST RDWR / private
   TEST(0 == init_mmfile(&mfile, "mmfile", 0, 1024, tempdir, mmfile_openmode_RDWR_PRIVATE)) ;
   TEST(fileoffset_mmfile(&mfile) == 0 ) ;
   TEST(size_mmfile(&mfile)       == 256 ) ;
   TEST(mfile.size_pagealigned    == pagesize) ;
   for(unsigned i = 0; i < 256; ++i) {
      TEST(buffer[i] == (uint8_t)i) ;
   }
   // write different content
   for(unsigned i = 0; i < 256; ++i) {
      addr_mmfile(&mfile)[i] = '1' ;
   }
   for(unsigned i = 0; i < 256; ++i) {
      TEST(addr_mmfile(&mfile)[i] == '1') ;
   }
   // test change is *not* shared
   fd = openat(dirfd(tempdir->sys_dir), "mmfile", O_RDONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   TEST(256 == read(fd, buffer, 256)) ;
   TEST(0   == close(fd)) ;
   fd = -1 ;
   for(unsigned i = 0; i < 256; ++i) {
      TEST(buffer[i] == (uint8_t)i) ; // old content
   }
   TEST(0 == free_mmfile(&mfile)) ;

   // TEST EINVAL
   // mmfile_openmode_e
   TEST(EINVAL == init_mmfile(&mfile, "mmfile", 0, 0, tempdir, mmfile_openmode_RDWR_PRIVATE+1)) ;
   // offset not page aligned
   TEST(EINVAL == init_mmfile(&mfile, "mmfile", pagesize-1, 0, tempdir, mmfile_openmode_RDWR_PRIVATE)) ;

   // TEST ENODATA
   // offset > filesize
   TEST(ENODATA== init_mmfile(&mfile, "mmfile", pagesize, 0, tempdir, mmfile_openmode_RDWR_PRIVATE)) ;
   fd = openat(dirfd(tempdir->sys_dir), "mmfile", O_WRONLY|O_CLOEXEC) ;
   TEST(fd > 0) ;
   TEST(0 == ftruncate(fd, 0)) ; // empty file
   TEST(0 == close(fd)) ;
   fd = -1 ;
   TEST(ENODATA == init_mmfile(&mfile, "mmfile", 0, 0, tempdir, mmfile_openmode_RDWR_SHARED)) ;

   // TEST ENOMEM
   if (sizeof(size_t) == sizeof(uint32_t)) {
      fd = openat(dirfd(tempdir->sys_dir), "mmfile", O_WRONLY|O_CLOEXEC) ;
      TEST(fd > 0) ;
      TEST(0 == ftruncate(fd, (size_t)-1)) ; // 4 GB
      TEST(0 == close(fd)) ;
      fd = -1 ;
      // mmap is never called
      TEST(ENOMEM == init_mmfile(&mfile, "mmfile", 0, 0, tempdir, mmfile_openmode_RDWR_PRIVATE)) ;
      // mmap returns ENOMEM
      TEST(ENOMEM == init_mmfile(&mfile, "mmfile", pagesize, 0, tempdir, mmfile_openmode_RDWR_PRIVATE)) ;
   }
   TEST(0 == removefile_directorystream(tempdir, "mmfile")) ;

   err = 0 ;
ABBRUCH:
   if (isOldact) sigaction(SIGSEGV, &oldact, 0) ;
   if (fd > 0) close(fd) ;
   (void) free_mmfile(&mfile) ;
   if (err) {
      (void) removefile_directorystream(tempdir, "mmfile") ;
   }
   return err ;
}

int unittest_os_memorymappedfile()
{
   int err ;
   directory_stream_t        tempdir = directory_stream_INIT_FREEABLE ;
   vm_mappedregions_t  mappedregions = vm_mappedregions_INIT_FREEABLE ;
   vm_mappedregions_t mappedregions2 = vm_mappedregions_INIT_FREEABLE ;
   const size_t       malloced_bytes = allocatedsize_malloctest() ;

   // store current mapping
   TEST(0 == init_vmmappedregions(&mappedregions)) ;

   TEST(0 == inittemp_directorystream(&tempdir, "mmfile" )) ;

   err = test_creat_mmfile(&tempdir) ;
   if (err) goto ABBRUCH ;

   err = test_open_mmfile(&tempdir) ;
   if (err) goto ABBRUCH ;

   TEST(0 == remove_directorystream(&tempdir)) ;
   TEST(0 == free_directorystream(&tempdir)) ;

   // TEST mapping has not changed
   trimmemory_malloctest() ;
   TEST(0 == init_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == free_vmmappedregions(&mappedregions2)) ;
   TEST(malloced_bytes == allocatedsize_malloctest()) ;

   return 0 ;
ABBRUCH:
   if (tempdir.sys_dir) {
      remove_directorystream(&tempdir) ;
   }
   free_directorystream(&tempdir) ;
   free_vmmappedregions(&mappedregions) ;
   free_vmmappedregions(&mappedregions2) ;
   return 1 ;
}
#endif
