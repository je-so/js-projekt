/*
   Directory read, write and file name handling

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
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// directory

int isvalid_directory( const char * const checked_path, const char * const basedir )
{
   assert( checked_path && basedir ) ;
   assert( !basedir[0] || (!isvalid_directory(basedir, "") && basedir[0]=='/') ) ;

   size_t bdi  = strlen(basedir) ;
   // is basedir of form ".../" ? => adapt
   if (bdi && basedir[bdi-1] == '/') --bdi ;

   const char * cp = checked_path ;

   while (!strncmp(cp,"../",3)) {
      if (!bdi) goto ERROR_TOO_MANY_LEADING_DOTS ;
      while( bdi && basedir[--bdi] != '/') { } // go basedir up
      cp += 3 ;
   }

   if (!strcmp(cp,"..")) {
      if (!bdi) goto ERROR_TOO_MANY_LEADING_DOTS ;
      while( bdi && basedir[--bdi] != '/') { } // go basedir up
      cp += 2 ;
   }

   size_t clen = strlen(cp) ;

   if (  !strcmp(cp, "." )
         || !strncmp(cp, "./", 2)
         || (clen >= 2 && !strcmp(cp+clen-2, "/."))
         || strstr(cp, "/./")
         || strstr((cp>checked_path)?cp-1:cp, "//")
         || strstr(cp, "/../")
         || (clen >= 3 && !strcmp(cp+clen-3, "/.."))
      ) {
      goto ERROR_CONTAINS_BAD_DOTS ;
   }

   return 0 ;

ERROR_TOO_MANY_LEADING_DOTS:
   LOG_TEXT(DIRECTORY_ERROR_TOO_MANY_LEADING_DOTS(checked_path, basedir)) ;
   return 1 ;

ERROR_CONTAINS_BAD_DOTS:
   LOG_TEXT(DIRECTORY_ERROR_CONTAINS_BAD_DOTS(checked_path)) ;
   return 2 ;
}

int filesize_directory( const char * file_path, const directory_stream_t * working_dir, /*out*/ off_t * file_size )
{
   int err ;
   int statatfd = AT_FDCWD;
   struct stat stat_result ;

   if (working_dir)
   {
      if (!working_dir->sys_dir) {
         err = EINVAL ;
         goto ABBRUCH ;
      }
      statatfd = dirfd(working_dir->sys_dir) ;
   }

   err = fstatat( statatfd, file_path, &stat_result, 0 ) ;
   if (err) {
      err = errno ;
      LOG_SYSERRNO("fstatat") ;
      goto ABBRUCH ;
   }

   *file_size = stat_result.st_size ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// directorystream

int init_directorystream(/*out*/directory_stream_t * dir, const char * dir_path, const directory_stream_t * working_dir)
{
   int err ;
   DIR           * sysdir = 0 ;
   int           openatfd = AT_FDCWD ;
   size_t workdir_pathlen = 0 ;

   if (working_dir)
   {
      if (!working_dir->sys_dir) {
         err = EINVAL ;
         goto ABBRUCH ;
      }
      openatfd        = dirfd(working_dir->sys_dir) ;
      workdir_pathlen = working_dir->path_len ;
   }

   int fdd = openat( openatfd, dir_path[0] ? dir_path : "." , O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC) ;
   if (-1 == fdd) {
      err = errno ;
      LOG_SYSERRNO("openat") ;
      LOG_STRING(dir_path) ;
      goto ABBRUCH ;
   }
   sysdir = fdopendir(fdd) ;
   if (!sysdir) {
      err = errno ;
      LOG_SYSERRNO("fdopendir") ;
      LOG_STRING(dir_path) ;
      goto ABBRUCH ;
   }
   struct dirent * direntry ;
   size_t pathoffset = sizeof(struct dirent) ;
   size_t path_len   = strlen(dir_path) ;
   if (path_len && dir_path[path_len-1] != '/') ++ path_len ;
   path_len += workdir_pathlen ;
   size_t path_size  = path_len + sizeof(direntry->d_name) ;
   size_t path_and_direntry_size = sizeof(struct dirent) + path_size ;
   if (     path_len < workdir_pathlen
         || path_size < path_len
         || path_and_direntry_size < path_size ) {
      err = EINVAL ;
      goto ABBRUCH ;
   }
   {
      long name_max = pathconf(dir_path, _PC_NAME_MAX) ;
      if (name_max >= (long)sizeof(direntry->d_name)) {
         size_t correction = ((size_t)name_max + (size_t)1) - sizeof(direntry->d_name) ;
         path_and_direntry_size += 2 * correction ;
         pathoffset += correction ;
         path_size  += correction ;
      }
   }
   direntry = (struct dirent *) malloc( path_and_direntry_size ) ;
   if (!direntry) {
      LOG_OUTOFMEMORY(path_and_direntry_size) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   dir->sys_dir   = sysdir ;
   dir->sysentry  = direntry ;
   dir->path_len  = path_len ;
   dir->path_size = path_size ;
   dir->path      = pathoffset + (char*)direntry ;
   if (workdir_pathlen) {
      strncpy( dir->path, working_dir->path, workdir_pathlen) ;
   }
   strcpy( dir->path + workdir_pathlen, dir_path ) ;
   if (path_len) dir->path[path_len-1] = '/' ;
   static_assert_void(sizeof(direntry->d_name) >= 1) ;
   dir->path[path_len]   = 0 ; // 0 is stored in direntry->d_name
   return 0 ;
ABBRUCH:
   if (sysdir) {
      closedir(sysdir) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int inittemp_directorystream( /*out*/directory_stream_t * dir, const char * name_prefix )
{
   int err ;
   size_t tmp_path_len = strlen(P_tmpdir) + 1 ;
   size_t   prefix_len = strlen(name_prefix?name_prefix:"") ;
   size_t    path_size = tmp_path_len + prefix_len + 7 + 1 ;
   char *     dir_path = malloc(path_size) ;

   if (!dir_path) {
      LOG_OUTOFMEMORY(path_size) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   strncpy( dir_path, P_tmpdir, tmp_path_len ) ;
   dir_path[tmp_path_len - 1] = '/' ;
   if (prefix_len) {
      strncpy( dir_path + tmp_path_len, name_prefix, prefix_len ) ;
   }
   char * id_str = dir_path + tmp_path_len + prefix_len ;
   strcpy( id_str, ".XXXXXX" ) ;
   ++ id_str ;

   // if (!mkdtemp( dir_path )) {
   //    err = errno ;
   //    LOG_SYSERRNO("mkdtemp") ;
   //    LOG_STRING(dir_path) ;
   //    goto ABBRUCH ;
   // }

   // TODO: replace with mkdtemp
   uint32_t id ;
   {
      struct timespec tspec ;
      if (0 == clock_gettime( CLOCK_REALTIME, &tspec)) {
         id = (uint32_t)tspec.tv_sec + (uint32_t)tspec.tv_nsec ;
      } else {
         id = (uint32_t) time(NULL) ;
      }
   }
   for( uint32_t i = 1000; true ; --i, id += 333 ) {
      id = (uint32_t) (id % 1000000) ;
      sprintf( id_str, "%06" PRIu32, id) ;
      if (0 == mkdir( dir_path, 0700 )) break ;
      err = errno ;
      if (err != EEXIST || 0 == i) {
         LOG_SYSERRNO("mkdir") ;
         LOG_STRING(dir_path) ;
         goto ABBRUCH ;
      }
   }

   err = init_directorystream(dir, dir_path, NULL) ;
   if (err) goto ABBRUCH ;

   free(dir_path) ;
   return 0 ;
ABBRUCH:
   free(dir_path) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_directorystream(directory_stream_t * dir)
{
   int err ;
   if (dir && dir->sys_dir) {
      free(dir->sysentry) ;
      dir->sysentry = 0 ;
      dir->path = 0 ;
      dir->path_len = 0 ;
      dir->path_size = 0 ;
      if (closedir(dir->sys_dir)) {
         err = errno ;
         LOG_SYSERRNO("closedir") ;
         goto ABBRUCH ;
      }
      dir->sys_dir = 0 ;
   }
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int readnext_directorystream(directory_stream_t * dir, /*out*/const char ** name, /*out*/filetype_e * ftype)
{
   int err ;
   struct dirent        * result = 0 ;
   const bool followSymbolicLink = true ;
   int             fstatat_flags = followSymbolicLink ? 0 : AT_SYMLINK_NOFOLLOW ;
   err = readdir_r( dir->sys_dir, dir->sysentry, &result ) ;
   if (err) {
      LOG_SYSERROR("readdir_r",err) ;
      goto ABBRUCH ;
   }

   if (ftype) {
      *ftype = ftUnknown ;
      if (result) {
         struct stat statbuf ;
         switch(dir->sysentry->d_type) {
         case DT_BLK:   *ftype = ftBlockDevice ; break ;
         case DT_CHR:   *ftype = ftCharacterDevice ; break ;
         case DT_DIR:   *ftype = ftDirectory ; break ;
         case DT_FIFO:  *ftype = ftNamedPipe ; break ;
         case DT_REG:   *ftype = ftRegularFile ; break ;
         case DT_SOCK:  *ftype = ftSocket ;  break ;
         case DT_UNKNOWN:
         case DT_LNK:
                        *ftype = ftSymbolicLink ;
                        if (!followSymbolicLink) break ;
                        // fall through
         default:
                        if (0 == fstatat( dirfd(dir->sys_dir), result->d_name, &statbuf, fstatat_flags)) {
                           if (S_ISBLK(statbuf.st_mode))       { *ftype = ftBlockDevice ; }
                           else if (S_ISCHR(statbuf.st_mode))  { *ftype = ftCharacterDevice ; }
                           else if (S_ISDIR(statbuf.st_mode))  { *ftype = ftDirectory ; }
                           else if (S_ISFIFO(statbuf.st_mode)) { *ftype = ftNamedPipe ; }
                           else if (S_ISLNK(statbuf.st_mode))  { *ftype = ftSymbolicLink ; }
                           else if (S_ISREG(statbuf.st_mode))  { *ftype = ftRegularFile ; }
                           else if (S_ISSOCK(statbuf.st_mode)) { *ftype = ftSocket ; }
                        }
                        break ;
         }
      }
   }

   *name = (result ? result->d_name : 0) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int returntobegin_directorystream(directory_stream_t * dir)
{
   if (!dir->sys_dir) {
      goto ABBRUCH ;
   }
   rewinddir(dir->sys_dir) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(EINVAL) ;
   return EINVAL ;
}


int makedirectory_directorystream(directory_stream_t * dir, const char * directory_name)
{
   int err ;
   size_t name_len = strlen(directory_name) ;
   if (name_len >= dir->path_size - dir->path_len) {
      err = ENAMETOOLONG ;
      goto ABBRUCH ;
   }
   strcpy( dir->path + dir->path_len, directory_name) ;
   // TODO: replace with mkdirat
   err = mkdir(dir->path, 0700) ;
   if (err) {
      err = errno ;
      LOG_SYSERROR("mkdir(dir->path, 0700)",err) ;
      LOG_STRING(dir->path) ;
      dir->path[dir->path_len] = 0 ;
      goto ABBRUCH ;
   }
   dir->path[dir->path_len] = 0 ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int makefile_directorystream(directory_stream_t * dir, const char * file_name)
{
   int err ;
   size_t name_len = strlen(file_name) ;
   if (name_len >= dir->path_size - dir->path_len) {
      err = ENAMETOOLONG ;
      goto ABBRUCH ;
   }
   strcpy( dir->path + dir->path_len, file_name) ;
   // TODO: replace with openat
   int fd = open(dir->path, O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC, S_IRUSR|S_IWUSR) ;
   if (-1 == fd) {
      err = errno ;
      LOG_SYSERROR("open(dir->path, O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC)",err) ;
      LOG_STRING(dir->path) ;
      dir->path[dir->path_len] = 0 ;
      goto ABBRUCH ;
   }
   dir->path[dir->path_len] = 0 ;
   close(fd) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int remove_directorystream(directory_stream_t * dir)
{
   int err ;
   err = rmdir(dir->path) ;
   if (err) {
      err = errno ;
      LOG_SYSERROR("rmdir",err) ;
      LOG_STRING(dir->path) ;
      goto ABBRUCH ;
   }
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int removedirectory_directorystream(directory_stream_t * dir, const char * directory_name)
{
   int err ;
   size_t name_len = strlen(directory_name) ;
   if (name_len >= dir->path_size - dir->path_len) {
      err = ENAMETOOLONG ;
      goto ABBRUCH ;
   }
   strcpy( dir->path + dir->path_len, directory_name) ;
   // TODO: replace with rmdirat
   err = rmdir(dir->path) ;
   if (err) {
      err = errno ;
      LOG_SYSERROR("rmdir",err) ;
      LOG_STRING(dir->path) ;
      dir->path[dir->path_len] = 0 ;
      goto ABBRUCH ;
   }
   dir->path[dir->path_len] = 0 ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int removefile_directorystream(directory_stream_t * dir, const char * file_name)
{
   int err ;
   size_t name_len = strlen(file_name) ;
   if (name_len >= dir->path_size - dir->path_len) {
      err = ENAMETOOLONG ;
      goto ABBRUCH ;
   }
   strcpy( dir->path + dir->path_len, file_name) ;
   // TODO: replace with unlinkat
   err = unlink(dir->path) ;
   if (err) {
      err = errno ;
      LOG_SYSERROR("unlink(dir->path)",err) ;
      LOG_STRING(dir->path) ;
      dir->path[dir->path_len] = 0 ;
      goto ABBRUCH ;
   }
   dir->path[dir->path_len] = 0 ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_directory,ABBRUCH)

static int test_isvalid(void)
{
   int err = 1 ;
   const int OK  = 0 ;
   const int LEADDOT = 1 ;
   const int BADDOT  = 2 ;

   TEST( BADDOT == isvalid_directory( ".", "/d1") ) ;
   TEST( BADDOT == isvalid_directory( "./", "") ) ;
   TEST( BADDOT == isvalid_directory( "/.", "") ) ;
   TEST( BADDOT == isvalid_directory( "/./", "") ) ;
   TEST( BADDOT == isvalid_directory( "./x/y/z", "") ) ;
   TEST( BADDOT == isvalid_directory( "/./x/y", "") ) ;
   TEST( BADDOT == isvalid_directory( "x/y/./z", "") ) ;
   TEST( BADDOT == isvalid_directory( "/x/y/.", "") ) ;
   TEST( BADDOT == isvalid_directory( "x/y/z/./", "") ) ;
   TEST( BADDOT == isvalid_directory( "//", "/d1") ) ;
   TEST( BADDOT == isvalid_directory( "//x/y/z", "/d1/") ) ;
   TEST( BADDOT == isvalid_directory( "x//y/z", "/d1/") ) ;
   TEST( BADDOT == isvalid_directory( "x/y//z", "/d1/") ) ;
   TEST( BADDOT == isvalid_directory( "x/y/z//", "/d1/") ) ;
   TEST( BADDOT == isvalid_directory( "/../x/y/z", "/d1/") ) ;
   TEST( BADDOT == isvalid_directory( "x/../y/z", "/d1/") ) ;
   TEST( BADDOT == isvalid_directory( "x/y/z/..", "/d1/") ) ;

   TEST( LEADDOT == isvalid_directory( "..", "/") ) ;
   TEST( LEADDOT == isvalid_directory( "..", "") ) ;
   TEST( LEADDOT == isvalid_directory( "../", "/") ) ;
   TEST( LEADDOT == isvalid_directory( "../", "") ) ;
   TEST( LEADDOT == isvalid_directory( "../x", "/") ) ;
   TEST( LEADDOT == isvalid_directory( "../x/", "") ) ;
   TEST( LEADDOT == isvalid_directory( "../..", "/d1") ) ;
   TEST( LEADDOT == isvalid_directory( "../../", "/d1/") ) ;
   TEST( LEADDOT == isvalid_directory( "../../x", "/d1/") ) ;
   TEST( LEADDOT == isvalid_directory( "../../..", "/d1/d2") ) ;
   TEST( LEADDOT == isvalid_directory( "../../../x/y/z", "/d1/d2") ) ;
   TEST( LEADDOT == isvalid_directory( "../../../../../../x", "/1/2/3/4ddd/5ddd") ) ;
   TEST( LEADDOT == isvalid_directory( "../../../../../../", "/1/2/3/4xx/5dxd") ) ;

   TEST( BADDOT == isvalid_directory( "../.", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "../.././x/y/z", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "../../x/./y/z", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "../x/y/./z", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "../../../x/.", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "..//", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "../../x//y/z", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "../../x/y/z//", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "../x/../y/z", "/d1/d2/d3/") ) ;
   TEST( BADDOT == isvalid_directory( "../x/y/z/..", "/d1/d2/d3/") ) ;

   TEST( OK == isvalid_directory( "1/2/3/4", "") ) ;
   TEST( OK == isvalid_directory( "1/2/3/4/", "") ) ;
   TEST( OK == isvalid_directory( "/1/2/3/4", "") ) ;
   TEST( OK == isvalid_directory( "/1/2/3/4/", "") ) ;
   TEST( OK == isvalid_directory( "..1/2../3/..4../x..", "") ) ;
   TEST( OK == isvalid_directory( "..1/2../3/..4../..x", "") ) ;
   TEST( OK == isvalid_directory( "1../2../3/..4../x..", "") ) ;
   TEST( OK == isvalid_directory( "1../2../3/..4../..x", "") ) ;
   TEST( OK == isvalid_directory( ".1/2./3/.4./x.", "") ) ;
   TEST( OK == isvalid_directory( ".1/2./3/.4./.x", "") ) ;
   TEST( OK == isvalid_directory( "1./2./3/.4./x.", "") ) ;
   TEST( OK == isvalid_directory( "1./2./3/.4./.x", "") ) ;
   TEST( OK == isvalid_directory( "/..1/2../3/..4../x..", "") ) ;
   TEST( OK == isvalid_directory( "/..1/2../3/..4../..x", "") ) ;
   TEST( OK == isvalid_directory( "/1../2../3/..4../x..", "") ) ;
   TEST( OK == isvalid_directory( "/1../2../3/..4../..x", "") ) ;
   TEST( OK == isvalid_directory( "/.1/2./3/.4./x.", "") ) ;
   TEST( OK == isvalid_directory( "/.1/2./3/.4./.x", "") ) ;
   TEST( OK == isvalid_directory( "/1./2./3/.4./x.", "") ) ;
   TEST( OK == isvalid_directory( "/1./2./3/.4./.x", "") ) ;

   TEST( OK == isvalid_directory( "../", "/d1") ) ;
   TEST( OK == isvalid_directory( "..", "/d1/") ) ;
   TEST( OK == isvalid_directory( "../..", "/d1/d2") ) ;
   TEST( OK == isvalid_directory( "../..", "/d1/d2/") ) ;
   TEST( OK == isvalid_directory( "../../x", "/d1/d2") ) ;
   TEST( OK == isvalid_directory( "../../x/", "/d1/d2") ) ;
   TEST( OK == isvalid_directory( "../../x", "/d1/d2/") ) ;
   TEST( OK == isvalid_directory( "../../x/", "/d1/d2/") ) ;
   TEST( OK == isvalid_directory( "../../../../../../../../x/y/z", "/1/2/3/4/5/6/7ddd/8ddd") ) ;
   TEST( OK == isvalid_directory( "../../../../../../../../", "/1/2/3/4/5/6/7ddd/8ddd") ) ;
   TEST( OK == isvalid_directory( "../../../../../../../..", "/1/2/3/4/5/6/7ddd/8ddd/") ) ;
   TEST( OK == isvalid_directory( "../../../../../../../../x/y../.z./.x..", "/1/2/3/4/5/6/7ddd/8ddd/") ) ;

   err = 0 ;
ABBRUCH:
   return err ;
}

static int test_directory_stream__nextdir(directory_stream_t * dir, int test_flags_value)
{
   int err = 1 ;
   const char * read_name ;
   filetype_e   read_type ;
   int read_flag[100] = { 0 } ;

   if (!test_flags_value) {
      TEST(0 == readnext_directorystream(dir, &read_name, &read_type)) ;  // skip .
      TEST(0 == readnext_directorystream(dir, &read_name, &read_type)) ;  // skip ..
   }
   for(int i = 0; i < 100 * ((0!=(test_flags_value&2)) + (0!=(test_flags_value&1))); ++i) {
      unsigned read_number = 0 ;
      do {
         TEST(0 == readnext_directorystream(dir, &read_name, &read_type)) ;
      } while (!strcmp(read_name, ".") || !strcmp(read_name, "..")) ;
      if (0 == strncmp(read_name, "file_", 5)) {
         sscanf( read_name+5, "%u", &read_number) ;
         TEST(read_type == ftRegularFile) ;
         TEST(0 == (read_flag[read_number] & 1)) ;
         TEST(read_number < 100) ;
         read_flag[read_number] |= 1 ;
      } else {
         TEST(0 == strncmp(read_name, "dir_", 4)) ;
         sscanf( read_name+4, "%u", &read_number) ;
         TEST(read_type == ftDirectory) ;
         TEST(0 == (read_flag[read_number] & 2)) ;
         TEST(read_number < 100) ;
         read_flag[read_number] |= 2 ;
      }
   }
   // end of read returns 0 in read_name
   read_name = (const char*) 1 ;
   read_type = ftDirectory ;
   TEST(0 == readnext_directorystream(dir, &read_name, &read_type)) ;
   TEST(read_name == 0) ;
   TEST(read_type == ftUnknown) ;
   // all files found
   for(int i = 0; i < 100; ++i) {
      TEST(test_flags_value == read_flag[i]) ;
   }

   err = 0 ;
ABBRUCH:
   return err ;
}

static int test_directory_stream(void)
{
   directory_stream_t temp_dir = directory_stream_INIT_FREEABLE ;
   directory_stream_t      dir = directory_stream_INIT_FREEABLE ;

   TEST(0 == inittemp_directorystream(&temp_dir, "test1")) ;

   // TEST init,double free
   TEST(0 == init_directorystream(&dir, temp_dir.path, NULL)) ;
   TEST(0 == strcmp(dir.path, temp_dir.path)) ;
   TEST(dir.path_len == strlen(temp_dir.path)) ;
   TEST(dir.path[dir.path_len-1] == '/') ;
   TEST(dir.path_size >= dir.path_len + sizeof(dir.sysentry->d_name)) ;
   TEST(0 != dir.sys_dir) ;
   TEST(0 != dir.sysentry) ;
   TEST(0 == free_directorystream(&dir)) ;
   TEST(!dir.sys_dir) ;
   TEST(!dir.sysentry) ;
   TEST(!dir.path) ;
   TEST(!dir.path_size) ;
   TEST(!dir.path_len) ;
   TEST(0 == free_directorystream(&dir)) ;

   // TEST makedirecory & makefile
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == makefile_directorystream(&temp_dir, filename)) ;
      sprintf( filename, "dir_%06d", i) ;
      TEST(0 == makedirectory_directorystream(&temp_dir, filename)) ;
   }

   // TEST working_dir
   TEST(0 == init_directorystream(&dir, "dir_000001", &temp_dir)) ;
   TEST(0 == strncmp(dir.path, temp_dir.path, temp_dir.path_len)) ;
   TEST(0 == strcmp(&dir.path[temp_dir.path_len-1], "/dir_000001/")) ;
   TEST(0 == free_directorystream(&dir)) ;

   // TEST nextdirentry
   TEST(0 == init_directorystream(&dir, temp_dir.path, NULL)) ;
   TEST(0 == test_directory_stream__nextdir(&dir, 3)) ;
   TEST(0 == free_directorystream(&dir)) ;
   TEST(0 == test_directory_stream__nextdir(&temp_dir, 3)) ;

   // TEST error conditions
   TEST(ENOENT == init_directorystream(&dir, ".....", NULL)) ;
   char buffer[1000] ;
   assert(sizeof(buffer) > strlen(temp_dir.path)+12) ;
   strcpy( buffer, temp_dir.path ) ;
   strcat( buffer, "file_000000" ) ;
   TEST(ENOTDIR == init_directorystream(&dir, buffer, NULL)) ;

   TEST(0 == init_directorystream(&dir, "/", NULL)) ;
   TEST(EACCES == makedirectory_directorystream(&dir, "XXXXXXXX.test" )) ;
   TEST(EACCES == makefile_directorystream(&dir, "XXXXXXXX.test" )) ;
   TEST(0 == free_directorystream(&dir)) ;

   TEST(0 == init_directorystream(&dir, temp_dir.path, NULL)) ;
   TEST(ENOTEMPTY == removedirectory_directorystream(&dir, "..")) ;
   TEST(0 == free_directorystream(&dir)) ;


   // TEST removedirectory
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "dir_%06d", i) ;
      TEST(0 == removedirectory_directorystream(&temp_dir, filename)) ;
   }
   TEST(0 == init_directorystream(&dir, temp_dir.path, NULL)) ;
   TEST(0 == test_directory_stream__nextdir(&dir, 1)) ;
   TEST(0 == free_directorystream(&dir)) ;
   TEST(0 == returntobegin_directorystream(&temp_dir)) ;
   TEST(0 == test_directory_stream__nextdir(&temp_dir, 1)) ;

   // TEST removefile
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == removefile_directorystream(&temp_dir, filename)) ;
   }
   TEST(0 == init_directorystream(&dir, temp_dir.path, NULL)) ;
   TEST(0 == test_directory_stream__nextdir(&dir, 0)) ;
   TEST(0 == free_directorystream(&dir)) ;
   TEST(0 == returntobegin_directorystream(&temp_dir)) ;
   TEST(0 == test_directory_stream__nextdir(&temp_dir, 0)) ;

   // TEST remove temp directory (now empty)
   TEST(0 == init_directorystream(&dir, temp_dir.path, NULL)) ;
   TEST(0 == remove_directorystream(&dir)) ;
   TEST(0 == free_directorystream(&dir)) ;
   TEST(ENOENT == init_directorystream(&dir, temp_dir.path, NULL)) ; // check it does no more exist

   // adapt LOG
   char * logbuffer ;
   size_t logbuffer_size ;
   LOG_GETBUFFER( &logbuffer, &logbuffer_size ) ;
   if (logbuffer_size) {
      char * found = logbuffer ;
      while ( (found=strstr( found, temp_dir.path )) ) {
         if (!strchr(found, '.')) break ;
         memcpy( strchr(found, '.')+1, "123456", 6 ) ;
         found += strlen(temp_dir.path) ;
      }
   }

   TEST(0 == free_directorystream(&temp_dir)) ;

   return 0 ;
ABBRUCH:
   (void) free_directorystream(&temp_dir) ;
   (void) free_directorystream(&dir) ;
   return 1 ;
}

static int test_directory_local(void)
{
   directory_stream_t local1 = directory_stream_INIT_FREEABLE ;
   directory_stream_t local2 = directory_stream_INIT_FREEABLE ;

   TEST(0 == init_directorystream(&local1, "", 0)) ;
   TEST(0 == init_directorystream(&local2, ".", 0)) ;

   for(;;) {
      const char  * name1, * name2 ;
      filetype_e  ft1,     ft2 ;
      TEST(0 == readnext_directorystream(&local1, &name1, &ft1)) ;
      TEST(0 == readnext_directorystream(&local2, &name2, &ft2)) ;
      if (!name1 || !name2) {
         TEST(name1 == name2) ;
         break ;
      }
      TEST(0 == strcmp(name1, name2)) ;
   }

   TEST(0 == free_directorystream(&local1)) ;
   TEST(0 == free_directorystream(&local2)) ;

   return 0 ;
ABBRUCH:
   free_directorystream(&local1) ;
   free_directorystream(&local2) ;
   return 1 ;
}

static int test_directory_filesize(void)
{
   directory_stream_t workdir = directory_stream_INIT_FREEABLE ;
   directory_stream_t tempdir = directory_stream_INIT_FREEABLE ;

   TEST(0 == init_directorystream(&workdir, "", 0)) ;
   TEST(0 == inittemp_directorystream(&tempdir, "tempdir")) ;

   // prepare
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == makefile_directorystream(&tempdir, filename)) ;
      int fd = openat(dirfd(tempdir.sys_dir), filename, O_RDWR|O_CLOEXEC) ;
      TEST(fd > 0) ;
      int written = (int) write(fd, filename, (size_t) i) ;
      close(fd) ;
      TEST(i == written) ;
   }

   // TEST filesize workdir != 0
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      off_t file_size = -1 ;
      filesize_directory( filename, &tempdir, &file_size) ;
      TEST(i == file_size) ;
   }

   // TEST filesize workdir == 0
   TEST(0 == fchdir(dirfd(tempdir.sys_dir))) ;
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      off_t file_size = -1 ;
      filesize_directory( filename, 0, &file_size) ;
      TEST(i == file_size) ;
   }
   TEST(0 == fchdir(dirfd(workdir.sys_dir))) ;

   // unprepare
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == removefile_directorystream(&tempdir, filename)) ;
   }
   TEST(0 == remove_directorystream(&tempdir)) ;
   TEST(0 == free_directorystream(&tempdir)) ;

   return 0 ;
ABBRUCH:
   assert(0 == fchdir(dirfd(workdir.sys_dir))) ;
   free_directorystream(&workdir) ;
   free_directorystream(&tempdir) ;
   return 1 ;
}

int unittest_directory()
{

   if (test_isvalid())              goto ABBRUCH ;
   if (test_directory_stream())     goto ABBRUCH ;
   if (test_directory_local())      goto ABBRUCH ;
   if (test_directory_filesize())   goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return 1 ;
}
#endif
