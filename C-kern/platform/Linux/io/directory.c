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

   file: C-kern/platform/Linux/io/directory.c
    Linux specific implementation <Directory Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/string/cstring.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include <glob.h>
#include "C-kern/api/test.h"
#endif


/* struct: directory_t
 * Alias for POSIX specific DIR structure. */
struct directory_t ;

// group: helper

static inline DIR * asDIR_directory(const directory_t * dir)
{
   return (DIR*) CONST_CAST(directory_t, dir) ;
}

// group: implementation

int checkpath_directory(const directory_t * dir, const char * const file_path)
{
   int err ;
   struct stat sbuf ;

   VALIDATE_INPARAM_TEST(0 != file_path, ONABORT, ) ;

   if (dir) {
      err = fstatat(fd_directory(dir), file_path, &sbuf, 0) ;
   } else {
      err = stat(file_path, &sbuf) ;
   }

   if (err) return errno ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int filesize_directory(const directory_t * relative_to, const char * file_path, /*out*/off_t * file_size)
{
   int err ;
   int statatfd = AT_FDCWD;
   struct stat stat_result ;

   if (relative_to)
   {
      statatfd = fd_directory(relative_to) ;
   }

   err = fstatat( statatfd, file_path, &stat_result, 0 ) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("fstatat", err) ;
      goto ONABORT ;
   }

   *file_size = stat_result.st_size ;
   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int new_directory(/*out*/directory_t ** dir, const char * dir_path, const directory_t * relative_to)
{
   int err ;
   int             fdd    = -1 ;
   int           openatfd = AT_FDCWD ;
   DIR           * sysdir = 0 ;
   char          * cwd    = 0 ;
   const bool      is_absolute  = ('/' == dir_path[0]) ;
   const char    * path         = dir_path[0] ? dir_path : "." ;

   if (     relative_to
         && !is_absolute) {
      openatfd = fd_directory(relative_to) ;
   }

   fdd = openat( openatfd, path, O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC) ;
   if (-1 == fdd) {
      err = errno ;
      TRACESYSERR_LOG("openat", err) ;
      PRINTCSTR_LOG(path) ;
      goto ONABORT ;
   }

   sysdir = fdopendir(fdd) ;
   if (!sysdir) {
      err = errno ;
      TRACESYSERR_LOG("fdopendir", err) ;
      PRINTCSTR_LOG(path) ;
      goto ONABORT ;
   }
   fdd = -1 ; // is used internally

   *(DIR**)dir = sysdir ;

   return 0 ;
ONABORT:
   free_file(&fdd) ;
   if (sysdir) {
      (void) closedir(sysdir) ;
   }
   free(cwd) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int newtemp_directory(/*out*/directory_t ** dir, const char * name_prefix, cstring_t * dir_path)
{
   int err ;
   directory_t * newdir = 0 ;
   char *      mkdpath  = 0 ;
   size_t      dir_len  = strlen(P_tmpdir) ;
   size_t      name_len = name_prefix ? strlen(name_prefix) : 0 ;
   size_t      tmpsize  = dir_len + name_len + 1 + 7 + 1 ;
   cstring_t   buffer   = cstring_INIT ;
   cstring_t * tmppath  = dir_path ? dir_path : &buffer ;

   err = allocate_cstring(tmppath, tmpsize) ;
   if (err) goto ONABORT ;

   clear_cstring(tmppath) ;

   err = append_cstring(tmppath, dir_len, P_tmpdir) ;
   if (err) goto ONABORT ;
   err = append_cstring(tmppath, 1, "/") ;
   if (err) goto ONABORT ;
   err = append_cstring(tmppath, name_len, name_prefix) ;
   if (err) goto ONABORT ;
   err = append_cstring(tmppath, 7, ".XXXXXX") ;
   if (err) goto ONABORT ;

   mkdpath = mkdtemp(str_cstring(tmppath)) ;
   if (!mkdpath) {
      err = errno ;
      TRACESYSERR_LOG("mkdtemp",err) ;
      goto ONABORT ;
   }

   err = new_directory(&newdir, mkdpath, NULL) ;
   if (err) goto ONABORT ;

   err = free_cstring(&buffer) ;
   if (err) goto ONABORT ;

   *dir = newdir ;

   return 0 ;
ONABORT:
   if (mkdpath) {
      rmdir(mkdpath) ;
   }
   clear_cstring(tmppath) ;
   free_cstring(&buffer) ;
   delete_directory(&newdir) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int delete_directory(directory_t ** dir)
{
   int err ;
   directory_t * delobj = *dir ;

   if (delobj) {
      *dir = 0 ;

      err = closedir(asDIR_directory(delobj)) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("closedir", err) ;
      }

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int next_directory(directory_t * dir, /*out*/const char ** name, /*out*/filetype_e * ftype)
{
   int err ;
   struct dirent  * result           = 0 ;

   errno = 0 ;
   result = readdir(asDIR_directory(dir)) ;
   if (!result && errno) {
      err = errno ;
      TRACESYSERR_LOG("readdir",err) ;
      goto ONABORT ;
   }

   if (ftype) {
      *ftype = ftUnknown ;
      if (result) {
         struct stat statbuf ;
         switch(result->d_type) {
         case DT_BLK:   *ftype = ftBlockDevice ;      break ;
         case DT_CHR:   *ftype = ftCharacterDevice ;  break ;
         case DT_DIR:   *ftype = ftDirectory ;        break ;
         case DT_FIFO:  *ftype = ftNamedPipe ;        break ;
         case DT_REG:   *ftype = ftRegularFile ;      break ;
         case DT_SOCK:  *ftype = ftSocket ;           break ;
         case DT_LNK:   *ftype = ftSymbolicLink ;     break ;
         case DT_UNKNOWN:
         default:
                       if (0 == fstatat( fd_directory(dir), result->d_name, &statbuf, AT_SYMLINK_NOFOLLOW)) {
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
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int gofirst_directory(directory_t * dir)
{
   int err ;

   VALIDATE_INPARAM_TEST(dir, ONABORT, ) ;

   rewinddir(asDIR_directory(dir)) ;
   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


int makedirectory_directory(directory_t * dir, const char * directory_path)
{
   int err ;
   int mkdiratfd = AT_FDCWD ;

   if (dir) {
      mkdiratfd = fd_directory(dir) ;
   }

   err = mkdirat(mkdiratfd, directory_path, 0700) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("mkdirat(mkdiratfd, directory_path, 0700)",err) ;
      PRINTINT_LOG(mkdiratfd) ;
      PRINTCSTR_LOG(directory_path) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int makefile_directory(directory_t * dir, const char * file_path, off_t file_length)
{
   int err ;
   int fd       = -1 ;
   int openatfd = AT_FDCWD ;

   if (dir) {
      openatfd = fd_directory(dir) ;
   }

   fd = openat(openatfd, file_path, O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC, S_IRUSR|S_IWUSR) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSERR_LOG("openat(openatfd, file_path)",err) ;
      PRINTINT_LOG(openatfd) ;
      PRINTCSTR_LOG(file_path) ;
      goto ONABORT ;
   }

   err = ftruncate(fd, file_length) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("ftruncate(file_path, file_length)",err) ;
      PRINTCSTR_LOG(file_path) ;
      PRINTUINT64_LOG(file_length) ;
      goto ONABORT ;
   }

   free_file(&fd) ;

   return 0 ;
ONABORT:
   if (-1 != fd) {
      free_file(&fd) ;
      unlinkat(openatfd, file_path, 0) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int removedirectory_directory(directory_t * dir, const char * directory_path)
{
   int err ;
   int unlinkatfd = AT_FDCWD ;

   if (dir) {
      unlinkatfd = fd_directory(dir) ;
   }

   err = unlinkat(unlinkatfd, directory_path, AT_REMOVEDIR) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("unlinkat(unlinkatfd, directory_path)",err) ;
      PRINTINT_LOG(unlinkatfd) ;
      PRINTCSTR_LOG(directory_path) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removefile_directory(directory_t * dir, const char * file_path)
{
   int err ;
   int unlinkatfd = AT_FDCWD ;

   if (dir) {
      unlinkatfd = fd_directory(dir) ;
   }

   err = unlinkat(unlinkatfd, file_path, 0) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("unlinkat(unlinkatfd, file_path)",err) ;
      PRINTINT_LOG(unlinkatfd) ;
      PRINTCSTR_LOG(file_path) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: test

#ifdef KONFIG_UNITTEST

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
ONABORT:
   return EINVAL ;
}

static int test_directory_stream__nextdir(directory_t * dir, int test_flags_value)
{
   const char * read_name ;
   filetype_e   read_type ;
   int read_flag[100] = { 0 } ;

   for (int i = 0; i < 100 * ((0!=(test_flags_value&2)) + (0!=(test_flags_value&1))); ++i) {
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
   for (int i = 0; i < 100; ++i) {
      TEST(test_flags_value == read_flag[i]) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   directory_t  * temp_dir = 0 ;
   directory_t  * dir      = 0 ;
   cstring_t      tmppath  = cstring_INIT ;
   const char *   fname ;
   size_t         nr_files ;
   glob_t         fndfiles ;
   int            fd_oldwd = -1 ;

   fd_oldwd = open( ".", O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC) ;
   TEST(-1 != fd_oldwd) ;

   // TEST new, double free (current working dir ".")
   TEST(0 == new_directory(&dir, ".", NULL)) ;
   TEST(0 != dir) ;
   TEST(0 == glob("*", GLOB_PERIOD|GLOB_NOSORT, 0, &fndfiles)) ;
   for (nr_files = 0; 0 == next_directory(dir, &fname, 0); ++nr_files) {
      if (!fname) {
         TEST(nr_files == fndfiles.gl_pathc) ;
         break ;
      }
      TEST(0 == strcmp(fname, fndfiles.gl_pathv[nr_files])) ;
   }
   globfree(&fndfiles) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;

   // TEST new, double free (current working dir "")
   TEST(0 == new_directory(&dir, "", NULL)) ;
   TEST(0 != dir) ;
   TEST(0 == glob("*", GLOB_PERIOD|GLOB_NOSORT, 0, &fndfiles)) ;
   for (nr_files = 0; 0 == next_directory(dir, &fname, 0); ++nr_files) {
      if (!fname) {
         TEST(nr_files == fndfiles.gl_pathc) ;
         break ;
      }
      TEST(0 == strcmp(fname, fndfiles.gl_pathv[nr_files])) ;
   }
   globfree(&fndfiles) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;

   // TEST newtemp
   TEST(0 == newtemp_directory(&temp_dir, "test1", &tmppath)) ;
   TEST(0 != temp_dir) ;
   TEST(17 == size_cstring(&tmppath)) ;
   TEST(0 == strncmp(str_cstring(&tmppath), "/tmp/test1.", 11)) ;
   TEST(0 == glob(str_cstring(&tmppath), GLOB_NOSORT, 0, &fndfiles)) ;
   TEST(1 == fndfiles.gl_pathc) ;
   TEST(0 == strcmp(fndfiles.gl_pathv[0], str_cstring(&tmppath))) ;
   globfree(&fndfiles) ;

   // TEST init relative, double free
   TEST(0 == mkdirat(fd_directory(temp_dir), "reldir.123", 0777)) ;
   TEST(0 == new_directory(&dir, "reldir.123", temp_dir)) ;
   TEST(0 != dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(!dir) ;
   TEST(0 == unlinkat(fd_directory(temp_dir), "reldir.123", AT_REMOVEDIR)) ;

   // TEST init current working dir + relative_to
   TEST(0 == new_directory(&dir, "", temp_dir)) ;
   TEST(0 == fchdir(fd_directory(temp_dir))) ;
   TEST(0 == glob("*", GLOB_PERIOD|GLOB_NOSORT, 0, &fndfiles)) ;
   TEST(0 == fchdir(fd_oldwd)) ;
   for (nr_files = 0; 0 == next_directory(dir, &fname, 0); ++nr_files) {
      if (!fname) {
         TEST(nr_files == fndfiles.gl_pathc) ;
         break ;
      }
      TEST(0 == strcmp(fname, fndfiles.gl_pathv[nr_files])) ;
   }
   globfree(&fndfiles) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(0 == new_directory(&dir, ".", temp_dir)) ;
   TEST(0 == delete_directory(&dir)) ;

   // TEST init absolute pathname + relative_to
   TEST(0 == new_directory(&dir, "/", temp_dir)) ;
   TEST(0 == glob("/*", GLOB_PERIOD|GLOB_NOSORT, 0, &fndfiles)) ;
   for (nr_files = 0; 0 == next_directory(dir, &fname, 0); ++nr_files) {
      if (!fname) {
         TEST(nr_files == fndfiles.gl_pathc) ;
         break ;
      }
      TEST(0 == strcmp(fname, 1+fndfiles.gl_pathv[nr_files])) ;
   }
   globfree(&fndfiles) ;
   TEST(0 == delete_directory(&dir)) ;

   // TEST makedirecory & makefile
   for (int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == makefile_directory(temp_dir, filename, 0)) ;
      sprintf( filename, "dir_%06d", i) ;
      TEST(0 == makedirectory_directory(temp_dir, filename)) ;
   }

   // TEST nextdirentry
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), NULL)) ;
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
   assert(sizeof(buffer) > size_cstring(&tmppath)+13) ;
   strcpy( buffer, str_cstring(&tmppath) ) ;
   strcat( buffer, "/file_000000" ) ;
   TEST(ENOTDIR == new_directory(&dir, buffer, NULL)) ;

   // TEST EACCES
   TEST(0 == new_directory(&dir, "/", NULL)) ;
   TEST(EACCES == makedirectory_directory(dir, "XXXXXXXX.test" )) ;
   TEST(EACCES == makefile_directory(dir, "XXXXXXXX.test", 0 )) ;
   TEST(0 == delete_directory(&dir)) ;

   // TEST ENOTEMPTY
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), NULL)) ;
   TEST(ENOTEMPTY == removedirectory_directory(dir, "..")) ;
   TEST(0 == delete_directory(&dir)) ;

   // TEST removedirectory
   for (int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "dir_%06d", i) ;
      TEST(0 == removedirectory_directory(temp_dir, filename)) ;
   }
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), NULL)) ;
   TEST(0 == test_directory_stream__nextdir(dir, 1)) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(0 == gofirst_directory(temp_dir)) ;
   TEST(0 == test_directory_stream__nextdir(temp_dir, 1)) ;

   // TEST removefile
   for (int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == removefile_directory(temp_dir, filename)) ;
   }
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), NULL)) ;
   TEST(0 == test_directory_stream__nextdir(dir, 0)) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(0 == gofirst_directory(temp_dir)) ;
   TEST(0 == test_directory_stream__nextdir(temp_dir, 0)) ;

   // TEST remove temp directory (now empty)
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), 0)) ;
   TEST(0 == delete_directory(&dir)) ;
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
      // check it does no more exist
   TEST(ENOENT == new_directory(&dir, str_cstring(&tmppath), 0)) ;

   // adapt LOG
   char * logbuffer ;
   size_t logbuffer_size ;
   GETBUFFER_LOG( &logbuffer, &logbuffer_size ) ;
   if (logbuffer_size) {
      char * found = logbuffer ;
      while ( (found=strstr( found, str_cstring(&tmppath) )) ) {
         if (!strchr(found, '.')) break ;
         memcpy( strchr(found, '.')+1, "123456", 6 ) ;
         found += size_cstring(&tmppath) ;
      }
   }

   TEST(0 == delete_directory(&temp_dir)) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == free_file(&fd_oldwd)) ;

   return 0 ;
ONABORT:
   assert(0 == fchdir(fd_oldwd)) ;
   (void) delete_directory(&temp_dir) ;
   (void) delete_directory(&dir) ;
   free_cstring(&tmppath) ;
   free_file(&fd_oldwd) ;
   return EINVAL ;
}

static int test_workingdir(void)
{
   directory_t * local1 = 0 ;
   directory_t * local2 = 0 ;

   // TEST "" & "." are the same
   TEST(0 == new_directory(&local1, "", 0)) ;
   TEST(0 == new_directory(&local2, ".", 0)) ;

   for (;;) {
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
ONABORT:
   delete_directory(&local1) ;
   delete_directory(&local2) ;
   return EINVAL ;
}

static int test_filesize(void)
{
   directory_t * workdir = 0 ;
   directory_t * tempdir = 0 ;
   cstring_t     tmppath = cstring_INIT ;

   TEST(0 == new_directory(&workdir, "", 0)) ;
   TEST(0 == newtemp_directory(&tempdir, "tempdir", &tmppath)) ;

   // prepare
   for (int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == makefile_directory(tempdir, filename, 0)) ;
      int fd = openat(fd_directory(tempdir), filename, O_RDWR|O_CLOEXEC) ;
      TEST(fd > 0) ;
      int written = (int) write(fd, filename, (size_t) i) ;
      free_file(&fd) ;
      TEST(i == written) ;
   }

   // TEST filesize workdir != 0
   for (int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      off_t file_size = -1 ;
      TEST(0 == filesize_directory( tempdir, filename, &file_size)) ;
      TEST(i == file_size) ;
   }

   // TEST filesize workdir == 0
   TEST(0 == fchdir(fd_directory(tempdir))) ;
   for (int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      off_t file_size = -1 ;
      TEST(0 == filesize_directory( 0, filename, &file_size)) ;
      TEST(i == file_size) ;
   }
   TEST(0 == fchdir(fd_directory(workdir))) ;

   // unprepare
   for (int i = 0; i < 100; ++i) {
      char filename[100] ;
      sprintf( filename, "file_%06d", i) ;
      TEST(0 == removefile_directory(tempdir, filename)) ;
   }
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;

   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;
   TEST(0 == delete_directory(&workdir)) ;

   return 0 ;
ONABORT:
   if (workdir) {
      assert(0 == fchdir(fd_directory(workdir))) ;
   }
   free_cstring(&tmppath) ;
   delete_directory(&workdir) ;
   delete_directory(&tempdir) ;
   return EINVAL ;
}

int unittest_io_directory()
{
   resourceusage_t   usage      = resourceusage_INIT_FREEABLE ;
   unsigned          open_count = 0 ;
   directory_t     * dummydir[8] ;

   if (test_initfree())          goto ONABORT ;
   CLEARBUFFER_LOG() ;

   TEST(0 == init_resourceusage(&usage)) ;

   // increment open files to 8 to make logged fd number always the same (support debug && X11 GLX which opens files)
   {
      size_t nrfdopen ;
      TEST(0 == nropen_file(&nrfdopen)) ;
      while (nrfdopen < 8) {
         TEST(0 == new_directory(&dummydir[open_count], "", 0)) ;
         ++ open_count ;
         ++ nrfdopen ;
      }
   }

   if (test_checkpath())         goto ONABORT ;
   if (test_initfree())          goto ONABORT ;
   if (test_workingdir())        goto ONABORT ;
   if (test_filesize())          goto ONABORT ;

   while (open_count) {
      TEST(0 == delete_directory(&dummydir[--open_count])) ;
   }

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   while (open_count) {
      TEST(0 == delete_directory(&dummydir[--open_count])) ;
   }
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}
#endif
