/* title: Directory Linux
   Implements <Directory>.

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

   file: C-kern/api/io/filesystem/directory.h
    Header file <Directory>.

   file: C-kern/os/Linux/io/directory.c
    Linux specific implementation <Directory Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: directory_t

int checkpath_directory(const directory_t * dir, const char * const file_path)
{
   int err ;
   struct stat sbuf ;

   PRECONDITION_INPUT(0 != file_path, ABBRUCH, ) ;

   if (dir) {
      err = fstatat(dirfd(dir->sys_dir), file_path, &sbuf, 0) ;
   } else {
      err = stat(file_path, &sbuf) ;
   }

   if (err) return errno ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

filedescr_t fd_directory(const directory_t * dir)
{
   int err ;

   PRECONDITION_INPUT(0 != dir, ABBRUCH, ) ;

   return dirfd(dir->sys_dir) ;
ABBRUCH:
   LOG_ABORT(err) ;
   return filedescr_INIT_FREEABLE ;
}

int filesize_directory(const directory_t * relative_to, const char * file_path, /*out*/off_t * file_size)
{
   int err ;
   int statatfd = AT_FDCWD;
   struct stat stat_result ;

   if (relative_to)
   {
      if (!relative_to->sys_dir) {
         err = EINVAL ;
         goto ABBRUCH ;
      }
      statatfd = dirfd(relative_to->sys_dir) ;
   }

   err = fstatat( statatfd, file_path, &stat_result, 0 ) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("fstatat", err) ;
      goto ABBRUCH ;
   }

   *file_size = stat_result.st_size ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int path_directory(const directory_t * dir, /*out*/size_t * path_len, /*out*/const char ** path)
{
   int err ;
   PRECONDITION_INPUT(0 != dir, ABBRUCH, ) ;

   if (path_len) {
      *path_len = dir->path_len ;
   }

   if (path) {
      *path = dir->path ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int new_directory(/*out*/directory_t ** dir, const char * dir_path, const directory_t * relative_to)
{
   int err ;
   int             fdd    = -1 ;
   directory_t   * newobj = 0 ;
   int           openatfd = AT_FDCWD ;
   DIR           * sysdir = 0 ;
   char          * cwd    = 0 ;
   const bool      is_absolute  = ('/' == dir_path[0]) ;
   const bool      is_currentwd = ((0 == dir_path[0]) || (0 == strcmp(dir_path, ".")) || (0 == strcmp(dir_path, "./"))) ;
   const char    * path         = is_currentwd ? "." : dir_path ;
   size_t          path_len     = is_currentwd ? 0   : strlen(dir_path) ;
   size_t          relative_len = 0 ;

   PRECONDITION_INPUT( (0 == relative_to) || (relative_to->sys_dir), ABBRUCH, ) ;

   if (     relative_to
         && !is_absolute) {
      openatfd     = dirfd(relative_to->sys_dir) ;
      relative_len = relative_to->path_len ;
      if (is_currentwd) path_len = 0 ;
   } else if (    !relative_to
               && is_currentwd) {
      cwd = get_current_dir_name() ;
      if (!cwd) {
         err = errno ;
         goto ABBRUCH ;
      }
      path     = cwd ;
      path_len = strlen(cwd) ;
   }

   fdd = openat( openatfd, path, O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC) ;
   if (-1 == fdd) {
      err = errno ;
      LOG_SYSERR("openat", err) ;
      LOG_STRING(path) ;
      goto ABBRUCH ;
   }

   sysdir = fdopendir(fdd) ;
   if (!sysdir) {
      err = errno ;
      LOG_SYSERR("fdopendir", err) ;
      LOG_STRING(path) ;
      goto ABBRUCH ;
   }
   fdd = -1 ; // is used internally

   size_t filename_maxsize = 1/*0 byte*/ + (size_t)fpathconf(dirfd(sysdir), _PC_NAME_MAX) ;
   if (filename_maxsize < sizeof(((struct dirent *)0)->d_name)) {
      filename_maxsize = sizeof(((struct dirent *)0)->d_name) ;
   }

   if (path_len && path[path_len-1] != '/')  ++path_len ;
   path_len = path_len + relative_len ;
   size_t path_size   = 1 + path_len ;
   size_t object_size = path_size + sizeof(directory_t) ;
   if (     path_size < path_len
         || path_len  < relative_len
         || object_size < sizeof(directory_t)) {
      LOG_OUTOFMEMORY(-1) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   newobj = (directory_t*) malloc( object_size ) ;
   if (!newobj) {
      LOG_OUTOFMEMORY(object_size) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   newobj->sys_dir   = sysdir ;
   newobj->path_len  = path_len ;
   if (relative_len) {
      strncpy( newobj->path, relative_to->path, relative_len) ;
   }
   strncpy( newobj->path + relative_len, path, path_len-relative_len) ;
   if (path_len) newobj->path[path_len-1] = '/' ;
   newobj->path[path_len] = 0 ;

   free(cwd) ;
   cwd = 0 ;

   *dir = newobj ;

   return 0 ;
ABBRUCH:
   free_filedescr(&fdd) ;
   if (sysdir) {
      (void) closedir(sysdir) ;
   }
   free(cwd) ;
   LOG_ABORT(err) ;
   return err ;
}

int newtemp_directory(/*out*/directory_t ** dir, const char * name_prefix)
{
   int err ;
   size_t tmp_path_len = strlen(P_tmpdir) ;
   size_t   prefix_len = strlen(name_prefix?name_prefix:"") ;
   size_t    path_size = tmp_path_len + prefix_len + 8 + 1 ;
   char *     dir_path = malloc(path_size) ;

   if (!dir_path) {
      LOG_OUTOFMEMORY(path_size) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   char * next_char = dir_path ;
   strcpy(next_char, P_tmpdir) ;
   next_char   += tmp_path_len  ;
   next_char[0] = '/' ;
   ++ next_char ;
   if (prefix_len) {
      strncpy( next_char, name_prefix, prefix_len ) ;
      next_char += prefix_len ;
   }
   strcpy( next_char, ".XXXXXX" ) ;

   if (!mkdtemp( dir_path )) {
      err = errno ;
      LOG_SYSERR("mkdtemp",err) ;
      LOG_STRING(dir_path) ;
      goto ABBRUCH ;
   }

   err = new_directory(dir, dir_path, NULL) ;
   if (err) goto ABBRUCH ;

   free(dir_path) ;
   return 0 ;
ABBRUCH:
   free(dir_path) ;
   LOG_ABORT(err) ;
   return err ;
}

int delete_directory(directory_t ** dir)
{
   int err = 0 ;
   directory_t * delobj = *dir ;

   if (delobj) {
      *dir = 0 ;

      delobj->path_len = 0 ;
      if (closedir(delobj->sys_dir)) {
         err = errno ;
         LOG_SYSERR("closedir", err) ;
      }
      delobj->sys_dir = 0 ;

      free(delobj) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int next_directory(directory_t * dir, /*out*/const char ** name, /*out*/filetype_e * ftype)
{
   int err ;
   struct dirent        * result = 0 ;
   const bool followSymbolicLink = true ;
   int             fstatat_flags = followSymbolicLink ? 0 : AT_SYMLINK_NOFOLLOW ;
   errno = 0 ;
   result = readdir( dir->sys_dir ) ;
   if (!result && errno) {
      err = errno ;
      LOG_SYSERR("readdir",err) ;
      goto ABBRUCH ;
   }

   if (ftype) {
      *ftype = ftUnknown ;
      if (result) {
         struct stat statbuf ;
         switch(result->d_type) {
         case DT_BLK:   *ftype = ftBlockDevice ; break ;
         case DT_CHR:   *ftype = ftCharacterDevice ; break ;
         case DT_DIR:   *ftype = ftDirectory ; break ;
         case DT_FIFO:  *ftype = ftNamedPipe ; break ;
         case DT_REG:   *ftype = ftRegularFile ; break ;
         case DT_SOCK:  *ftype = ftSocket ;  break ;
         case DT_LNK:
                        *ftype = ftSymbolicLink ;
                        if (!followSymbolicLink) break ;
                        // fall through
         case DT_UNKNOWN:
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

int gofirst_directory(directory_t * dir)
{
   int err ;

   PRECONDITION_INPUT(dir && dir->sys_dir, ABBRUCH, ) ;

   rewinddir(dir->sys_dir) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


int makedirectory_directory(directory_t * dir, const char * directory_path)
{
   int err ;
   int mkdiratfd = AT_FDCWD ;

   if (dir) {
      mkdiratfd = dirfd(dir->sys_dir) ;
   }

   err = mkdirat(mkdiratfd, directory_path, 0700) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("mkdirat(dir->path, directory_path, 0700)",err) ;
      if (dir) {
         LOG_STRING(dir->path) ;
      } else {
         LOG_PTR(dir) ;
      }
      LOG_STRING(directory_path) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int makefile_directory(directory_t * dir, const char * file_path, off_t file_length)
{
   int err ;
   int fd       = -1 ;
   int openatfd = AT_FDCWD ;

   if (dir) {
      openatfd = dirfd(dir->sys_dir) ;
   }

   fd = openat(openatfd, file_path, O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC, S_IRUSR|S_IWUSR) ;
   if (-1 == fd) {
      err = errno ;
      LOG_SYSERR("openat(openatfd, file_path)",err) ;
      LOG_INT(openatfd) ;
      LOG_STRING(file_path) ;
      goto ABBRUCH ;
   }

   err = ftruncate(fd, file_length) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("ftruncate(file_path, file_length)",err) ;
      LOG_STRING(file_path) ;
      LOG_UINT64(file_length) ;
      goto ABBRUCH ;
   }

   free_filedescr(&fd) ;

   return 0 ;
ABBRUCH:
   if (-1 != fd) {
      free_filedescr(&fd) ;
      unlinkat(openatfd, file_path, 0) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int remove_directory(directory_t * dir)
{
   int err ;

   PRECONDITION_INPUT(dir && dir->sys_dir, ABBRUCH, ) ;

   err = rmdir(dir->path) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("rmdir",err) ;
      LOG_STRING(dir->path) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int removedirectory_directory(directory_t * dir, const char * directory_path)
{
   int err ;
   int unlinkatfd = AT_FDCWD ;

   if (dir) {
      unlinkatfd = dirfd(dir->sys_dir) ;
   }

   err = unlinkat(unlinkatfd, directory_path, AT_REMOVEDIR) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("unlinkat(unlinkatfd, directory_path)",err) ;
      LOG_INT(unlinkatfd) ;
      LOG_STRING(directory_path) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int removefile_directory(directory_t * dir, const char * file_path)
{
   int err ;
   int unlinkatfd = AT_FDCWD ;

   if (dir) {
      unlinkatfd = dirfd(dir->sys_dir) ;
   }

   err = unlinkat(unlinkatfd, file_path, 0) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("unlinkat(unlinkatfd, file_path)",err) ;
      LOG_INT(unlinkatfd) ;
      LOG_STRING(file_path) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_checkpath(void)
{
   directory_t * basedir = 0 ;

   // TEST files exist in working dir
   do {
      TEST(0 == checkpath_directory(basedir, "." )) ;
      TEST(0 == checkpath_directory(basedir, ".." )) ;
      TEST(0 == checkpath_directory(basedir, "bin/" )) ;
      TEST(0 == checkpath_directory(basedir, "LICENSE" )) ;
      TEST(0 == checkpath_directory(basedir, "README" )) ;
      if (basedir) {
         TEST(0 == delete_directory(&basedir)) ;
         TEST(0 == basedir) ;
      } else {
         TEST(0 == new_directory(&basedir, ".", 0)) ;
         TEST(0 != basedir) ;
      }
   } while( basedir ) ;

   // TEST absolute files exist
   do {
      TEST(0 == checkpath_directory(basedir, "/" )) ;
      TEST(0 == checkpath_directory(basedir, "/home" )) ;
      TEST(0 == checkpath_directory(basedir, "/usr" )) ;
      TEST(0 == checkpath_directory(basedir, "/../../")) ;
      if (basedir) {
         TEST(0 == delete_directory(&basedir)) ;
         TEST(0 == basedir) ;
      } else {
         TEST(0 == new_directory(&basedir, ".", 0)) ;
         TEST(0 != basedir) ;
      }
   } while( basedir ) ;

   // TEST error
   do {
      TEST(ENOENT == checkpath_directory(basedir, "123456" )) ;
      TEST(ENOENT == checkpath_directory(basedir, "home.XXX" )) ;
      TEST(ENOENT == checkpath_directory(basedir, "usr.XXX" )) ;
      TEST(ENOENT == checkpath_directory(basedir, "./123456.XXX")) ;
      if (basedir) {
         TEST(0 == delete_directory(&basedir)) ;
         TEST(0 == basedir) ;
      } else {
         TEST(0 == new_directory(&basedir, "/", 0)) ;
         TEST(0 != basedir) ;
      }
   } while( basedir ) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_directory_stream__nextdir(directory_t * dir, int test_flags_value)
{
   const char * read_name ;
   filetype_e   read_type ;
   int read_flag[100] = { 0 } ;

   for(int i = 0; i < 100 * ((0!=(test_flags_value&2)) + (0!=(test_flags_value&1))); ++i) {
      unsigned read_number = 0 ;
      do {
         TEST(0 == next_directory(dir, &read_name, &read_type)) ;
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
   do {
      read_name = (const char*) 1 ;
      read_type = ftDirectory ;
      TEST(0 == next_directory(dir, &read_name, &read_type)) ;
   } while (read_name && (!strcmp(read_name, ".") || !strcmp(read_name, ".."))) ;
   TEST(read_name == 0) ;
   TEST(read_type == ftUnknown) ;
   // all files found
   for(int i = 0; i < 100; ++i) {
      TEST(test_flags_value == read_flag[i]) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_initfree(void)
{
   directory_t  * temp_dir = 0 ;
   directory_t  * dir      = 0 ;
   const char   * path     = 0 ;
   size_t         path_len ;
   char           cwd[4096]= { 0 } ;

   TEST(cwd == getcwd(cwd, sizeof(cwd)));
   TEST(sizeof(cwd) > 1+strlen(cwd)) ;
   strcat(cwd, "/") ;

   // TEST new, double free (current working dir ".")
   TEST(0 == new_directory(&dir, ".", NULL)) ;
   TEST(0 == path_directory(dir, &path_len, &path)) ;
   TEST(strlen(cwd) == path_len) ;
   TEST(0 != path) ;
   TEST(0 == strcmp(path, cwd)) ;
   TEST(0 != dir->sys_dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;

   // TEST new, double free (current working dir "./")
   TEST(0 == new_directory(&dir, "./", NULL)) ;
   TEST(0 == path_directory(dir, &path_len, &path)) ;
   TEST(strlen(cwd) == path_len) ;
   TEST(0 != path) ;
   TEST(0 == strcmp(path, cwd)) ;
   TEST(0 != dir->sys_dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;

   // TEST new, double free (current working dir "")
   TEST(0 == new_directory(&dir, "", NULL)) ;
   TEST(0 == path_directory(dir, &path_len, &path)) ;
   TEST(strlen(cwd) == path_len) ;
   TEST(0 != path) ;
   TEST(0 == strcmp(path, cwd)) ;
   TEST(0 != dir->sys_dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;

   // TEST newtemp
   TEST(0 == newtemp_directory(&temp_dir, "test1")) ;
   TEST(0 == path_directory(temp_dir, &path_len, &path)) ;
   TEST(18 == path_len) ;
   TEST(18 == strlen(path)) ;
   TEST(0 == strncmp(path, "/tmp/test1.", 11)) ;
   TEST('/' == path[17]) ;
   TEST(0 != temp_dir->sys_dir) ;

   // TEST init relative, double free
   TEST(0 == mkdirat(dirfd(temp_dir->sys_dir), "reldir.123", 0777)) ;
   TEST(0 == new_directory(&dir, "reldir.123", temp_dir)) ;
   TEST(0 == path_directory(dir, &path_len, &path)) ;
   TEST(path_len == strlen(temp_dir->path) + strlen("reldir.123/")) ;
   TEST(0 == strncmp(path, temp_dir->path, temp_dir->path_len)) ;
   TEST(0 == strcmp(path + temp_dir->path_len, "reldir.123/")) ;
   TEST(0 != dir->sys_dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == unlinkat(dirfd(temp_dir->sys_dir), "reldir.123", AT_REMOVEDIR)) ;

   // TEST init current working dir + relative_to
   TEST(0 == new_directory(&dir, "", temp_dir)) ;
   TEST(dir->path_len == temp_dir->path_len) ;
   TEST(0 == strcmp(dir->path, temp_dir->path)) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(0 == new_directory(&dir, ".", temp_dir)) ;
   TEST(dir->path_len == temp_dir->path_len) ;
   TEST(0 == strcmp(dir->path, temp_dir->path)) ;
   TEST(0 == delete_directory(&dir)) ;

   // TEST init absolute pathname + relative_to
   TEST(0 == new_directory(&dir, "/", temp_dir)) ;
   TEST(dir->path_len == 1) ;
   TEST(0 == strcmp(dir->path, "/")) ;
   TEST(0 == delete_directory(&dir)) ;

   // TEST init current working dir after change to temp dir
   {
      directory_t * oldwd = 0 ;
      TEST(0 == new_directory(&oldwd, "", 0)) ;
      TEST(0 == fchdir(dirfd(temp_dir->sys_dir))) ;
      TEST(0 == new_directory(&dir, "", 0)) ;
      TEST(dir->path_len == temp_dir->path_len) ;
      TEST(0 == strcmp(dir->path, temp_dir->path)) ;
      TEST(0 == delete_directory(&dir)) ;
      TEST(0 == fchdir(dirfd(oldwd->sys_dir))) ;
      TEST(0 == delete_directory(&oldwd)) ;
   }

   // TEST makedirecory & makefile
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == makefile_directory(temp_dir, filename, 0)) ;
      sprintf( filename, "dir_%06d", i) ;
      TEST(0 == makedirectory_directory(temp_dir, filename)) ;
   }

   // TEST nextdirentry
   TEST(0 == new_directory(&dir, temp_dir->path, NULL)) ;
   TEST(0 == test_directory_stream__nextdir(dir, 3)) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(0 == test_directory_stream__nextdir(temp_dir, 3)) ;

   // TEST EINVAL
   TEST(EINVAL == makefile_directory(temp_dir, "123", -1)) ;
   TEST(ENOENT == checkpath_directory(temp_dir, "123")) ;

   // TEST EFBIG
   TEST(EFBIG == makefile_directory(temp_dir, "123", 0x7fffffffffffffff)) ;
   TEST(ENOENT == checkpath_directory(temp_dir, "123")) ;

   // TEST ENOENT
   TEST(ENOENT == new_directory(&dir, ".....", NULL)) ;

   // TEST ENOTDIR
   char buffer[1000] ;
   assert(sizeof(buffer) > strlen(temp_dir->path)+12) ;
   strcpy( buffer, temp_dir->path ) ;
   strcat( buffer, "file_000000" ) ;
   TEST(ENOTDIR == new_directory(&dir, buffer, NULL)) ;

   // TEST EACCES
   TEST(0 == new_directory(&dir, "/", NULL)) ;
   TEST(EACCES == makedirectory_directory(dir, "XXXXXXXX.test" )) ;
   TEST(EACCES == makefile_directory(dir, "XXXXXXXX.test", 0 )) ;
   TEST(0 == delete_directory(&dir)) ;

   // TEST ENOTEMPTY
   TEST(0 == new_directory(&dir, temp_dir->path, NULL)) ;
   TEST(ENOTEMPTY == removedirectory_directory(dir, "..")) ;
   TEST(0 == delete_directory(&dir)) ;

   // TEST removedirectory
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "dir_%06d", i) ;
      TEST(0 == removedirectory_directory(temp_dir, filename)) ;
   }
   TEST(0 == new_directory(&dir, temp_dir->path, NULL)) ;
   TEST(0 == test_directory_stream__nextdir(dir, 1)) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(0 == gofirst_directory(temp_dir)) ;
   TEST(0 == test_directory_stream__nextdir(temp_dir, 1)) ;

   // TEST removefile
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == removefile_directory(temp_dir, filename)) ;
   }
   TEST(0 == new_directory(&dir, temp_dir->path, NULL)) ;
   TEST(0 == test_directory_stream__nextdir(dir, 0)) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(0 == gofirst_directory(temp_dir)) ;
   TEST(0 == test_directory_stream__nextdir(temp_dir, 0)) ;

   // TEST remove temp directory (now empty)
   TEST(0 == new_directory(&dir, "", temp_dir)) ;
   TEST(0 == remove_directory(dir)) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(ENOENT == new_directory(&dir, temp_dir->path, NULL)) ; // check it does no more exist

   // adapt LOG
   char * logbuffer ;
   size_t logbuffer_size ;
   LOG_GETBUFFER( &logbuffer, &logbuffer_size ) ;
   if (logbuffer_size) {
      char * found = logbuffer ;
      while ( (found=strstr( found, temp_dir->path )) ) {
         if (!strchr(found, '.')) break ;
         memcpy( strchr(found, '.')+1, "123456", 6 ) ;
         found += strlen(temp_dir->path) ;
      }
   }

   TEST(0 == delete_directory(&temp_dir)) ;

   return 0 ;
ABBRUCH:
   (void) delete_directory(&temp_dir) ;
   (void) delete_directory(&dir) ;
   return EINVAL ;
}

static int test_workingdir(void)
{
   directory_t * local1 = 0 ;
   directory_t * local2 = 0 ;

   // TEST "" & "." are the same
   TEST(0 == new_directory(&local1, "", 0)) ;
   TEST(0 == new_directory(&local2, ".", 0)) ;

   for(;;) {
      const char  * name1, * name2 ;
      filetype_e  ft1,     ft2 ;
      TEST(0 == next_directory(local1, &name1, &ft1)) ;
      TEST(0 == next_directory(local2, &name2, &ft2)) ;
      if (!name1 || !name2) {
         TEST(name1 == name2) ;
         break ;
      }
      TEST(0 == strcmp(name1, name2)) ;
   }

   TEST(0 == delete_directory(&local1)) ;
   TEST(0 == delete_directory(&local2)) ;

   return 0 ;
ABBRUCH:
   delete_directory(&local1) ;
   delete_directory(&local2) ;
   return EINVAL ;
}

static int test_filesize(void)
{
   directory_t * workdir = 0 ;
   directory_t * tempdir = 0 ;

   TEST(0 == new_directory(&workdir, "", 0)) ;
   TEST(0 == newtemp_directory(&tempdir, "tempdir")) ;

   // prepare
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == makefile_directory(tempdir, filename, 0)) ;
      int fd = openat(dirfd(tempdir->sys_dir), filename, O_RDWR|O_CLOEXEC) ;
      TEST(fd > 0) ;
      int written = (int) write(fd, filename, (size_t) i) ;
      free_filedescr(&fd) ;
      TEST(i == written) ;
   }

   // TEST filesize workdir != 0
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      off_t file_size = -1 ;
      TEST(0 == filesize_directory( tempdir, filename, &file_size)) ;
      TEST(i == file_size) ;
   }

   // TEST filesize workdir == 0
   TEST(0 == fchdir(dirfd(tempdir->sys_dir))) ;
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      off_t file_size = -1 ;
      TEST(0 == filesize_directory( 0, filename, &file_size)) ;
      TEST(i == file_size) ;
   }
   TEST(0 == fchdir(dirfd(workdir->sys_dir))) ;

   // unprepare
   for(int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == removefile_directory(tempdir, filename)) ;
   }
   TEST(0 == remove_directory(tempdir)) ;

   TEST(0 == delete_directory(&tempdir)) ;
   TEST(0 == delete_directory(&workdir)) ;

   return 0 ;
ABBRUCH:
   assert(0 == fchdir(dirfd(workdir->sys_dir))) ;
   delete_directory(&workdir) ;
   delete_directory(&tempdir) ;
   return EINVAL ;
}

int unittest_io_directory()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_checkpath())         goto ABBRUCH ;
   if (test_initfree())          goto ABBRUCH ;
   if (test_workingdir())        goto ABBRUCH ;
   if (test_filesize())          goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
