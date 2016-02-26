/* title: Directory Linux
   Implements <Directory>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/filesystem/directory.h
    Header file <Directory>.

   file: C-kern/platform/Linux/io/directory.c
    Linux specific implementation <Directory Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/string/cstring.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include <glob.h>
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#endif


/* struct: directory_t
 * Alias for POSIX specific DIR structure. */
struct directory_t;

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_directory_errtimer
 * Simulates an error in <path_directory>. */
static test_errortimer_t   s_directory_errtimer = test_errortimer_FREE;
#endif

// group: helper

static inline DIR * cast2DIR_directory(const directory_t * dir)
{
   return (DIR*) CONST_CAST(directory_t, dir);
}

// group: query

int trypath_directory(const directory_t * dir, const char * const file_path)
{
   int err;
   int statatfd = AT_FDCWD;
   struct stat stat_result;

   if (dir) {
      statatfd = io_directory(dir);
   }

   err = fstatat(statatfd, file_path, &stat_result, 0);

   return err ? errno : 0;
}

int filesize_directory(const directory_t * dir, const char * file_path, /*out*/off_t * file_size)
{
   int err;
   int statatfd = AT_FDCWD;
   struct stat stat_result;

   if (dir) {
      statatfd = io_directory(dir);
   }

   err = fstatat(statatfd, file_path, &stat_result, 0 );
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("fstatat", err);
      goto ONERR;
   }

   *file_size = stat_result.st_size;
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int path_directory(const directory_t * dir, /*ret*/struct wbuffer_t * path)
{
   int err;
   int         dfd    = io_directory(dir);
   int         dfd2   = -1;
   DIR *       sysdir = 0;
   struct stat filestat;
   size_t      rpath_offset = sys_path_MAXSIZE-1;
   uint8_t     rpath[sys_path_MAXSIZE];

   rpath[sys_path_MAXSIZE-1] = 0;

   if (PROCESS_testerrortimer(&s_directory_errtimer, &err)) {
      errno = err;
   } else {
      err = stat("/", &filestat);
   }
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("stat('/')", err);
      goto ONERR;
   }

   const ino_t rootinode = filestat.st_ino;

   for (;;) {
      if (PROCESS_testerrortimer(&s_directory_errtimer, &err)) {
         errno = err;
      } else {
         err = fstat(dfd, &filestat);
      }
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("fstat", err);
         goto ONERR;
      }
      if (rootinode == filestat.st_ino) {
         break;  // root directory ==> DONE
      }
      if (PROCESS_testerrortimer(&s_directory_errtimer, &err)) {
         errno = err;
         dfd2 = -1;
      } else {
         dfd2 = openat(dfd, "..", O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC);
      }
      if (-1 == dfd2) {
         err = errno;
         TRACESYSCALL_ERRLOG("openat('..')", err);
         goto ONERR;
      }
      if (sysdir && closedir(sysdir)/*closes dfd*/) {
         close(dfd);
         sysdir = 0;   // (sysdir == 0) ==> dfd2 will be closed
         err = errno;
         TRACESYSCALL_ERRLOG("closedir", err);
         goto ONERR;
      }
      if (PROCESS_testerrortimer(&s_directory_errtimer, &err)) {
         errno = err;
         sysdir = 0;
      } else {
         sysdir = fdopendir(dfd2);
      }
      if (!sysdir) {
         // (sysdir == 0) ==> dfd2 will be closed
         err = errno;
         TRACESYSCALL_ERRLOG("fdopendir", err);
         goto ONERR;
      }
      dfd = dfd2;
      for (;;) {
         struct dirent * direntry;
         if (PROCESS_testerrortimer(&s_directory_errtimer, &err)) {
            errno = err;
            direntry = 0;
         } else {
            errno = 0;
            direntry = readdir(sysdir);
         }
         if (!direntry) {
            err = errno ? errno : ENOENT/*read last entry of directory*/;
            TRACESYSCALL_ERRLOG("readdir", err);
            goto ONERR;
         }
         if (filestat.st_ino == direntry->d_ino) {
            size_t namesize = strlen(direntry->d_name);
            if (rpath_offset < namesize + 1) {
               err = ENAMETOOLONG;
               goto ONERR;
            }
            rpath_offset -= namesize;
            memcpy(rpath + rpath_offset, direntry->d_name, namesize);
            rpath[--rpath_offset] = '/';
            break;
         }
      }
   }

   // copy rpath into path
   size_t path_size = sys_path_MAXSIZE - rpath_offset;

   if (PROCESS_testerrortimer(&s_directory_errtimer, &err)) goto ONERR;

   if (path_size <= 1) {
      err = appendcopy_wbuffer(path, 2, (const uint8_t*)"/");
      if (err) goto ONERR;
   } else {
      err = appendcopy_wbuffer(path, path_size, rpath + rpath_offset);
      if (err) goto ONERR;
   }

   if (sysdir) (void) closedir(sysdir);

   return 0;
ONERR:
   if (sysdir) {
      closedir(sysdir);
   } else if (-1 != dfd2) {
      close(dfd2);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: lifetime

int new_directory(/*out*/directory_t ** dir, const char * dir_path, const directory_t * relative_to)
{
   int err;
   int         fdd      = -1;
   int         openatfd = AT_FDCWD;
   DIR*        sysdir   = 0;
   const bool  is_absolute = ('/' == dir_path[0]);
   const char* path        = dir_path[0] ? dir_path : ".";

   if (  relative_to
         && !is_absolute) {
      openatfd = io_directory(relative_to);
   }

   fdd = openat(openatfd, path, O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC);
   if (-1 == fdd) {
      err = errno;
      TRACESYSCALL_ERRLOG("openat", err);
      PRINTCSTR_ERRLOG(path);
      goto ONERR;
   }

   sysdir = fdopendir(fdd);
   if (!sysdir) {
      err = errno;
      TRACESYSCALL_ERRLOG("fdopendir", err);
      PRINTCSTR_ERRLOG(path);
      goto ONERR;
   }
   // fdd is now used by sysdir

   *(DIR**)dir = sysdir;

   return 0;
ONERR:
   free_iochannel(&fdd);
   if (sysdir) {
      (void) closedir(sysdir);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int newtemp_directory(/*out*/directory_t ** dir, const char * name_prefix, /*ret*/struct wbuffer_t* dirpath/*could be set to 0*/)
{
   int err;
   char*        mkdpath  = 0;
   const size_t dir_len  = sizeof(P_tmpdir) - 1;
   size_t       name_len = name_prefix ? strlen(name_prefix) : 0;
   size_t       tmpsize  = dir_len + name_len + 1 + 8;
   uint8_t      strbuf[dir_len + sizeof(((struct dirent*)0)->d_name) + 1];
   wbuffer_t*   tmppath  = dirpath ? dirpath : &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(strbuf), strbuf);
   const size_t oldsize  = size_wbuffer(tmppath);
   uint8_t*     bytes;

   if (! PROCESS_testerrortimer(&s_directory_errtimer, &err)) {
      err = appendbytes_wbuffer(tmppath, tmpsize, &bytes);
   }
   if (err) goto ONERR;
   memcpy(bytes, P_tmpdir, dir_len);
   bytes[dir_len] = '/';
   memcpy((bytes+dir_len+1), name_prefix, name_len);
   memcpy((bytes+dir_len+1)+name_len, ".XXXXXX", 8);

   if (PROCESS_testerrortimer(&s_directory_errtimer, &err)) {
      errno = err;
   } else {
      mkdpath = mkdtemp((char*)bytes);
   }
   if (!mkdpath) {
      err = errno;
      TRACESYSCALL_ERRLOG("mkdtemp",err);
      goto ONERR;
   }

   if (! PROCESS_testerrortimer(&s_directory_errtimer, &err)) {
      err = new_directory(dir, mkdpath, NULL);
   }
   if (err) goto ONERR;

   return 0;
ONERR:
   if (mkdpath) {
      rmdir(mkdpath);
   }
   if (dirpath) {
      shrink_wbuffer(dirpath, oldsize);
   } else if (tmpsize > sizeof(strbuf)) {
      err = ENAMETOOLONG;
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int delete_directory(directory_t** dir)
{
   int err;
   directory_t* delobj = *dir;

   if (delobj) {
      *dir = 0;

      err = closedir(cast2DIR_directory(delobj));
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("closedir", err);
      }

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: read

int next_directory(directory_t* dir, /*out*/const char** name, /*out*/filetype_e* ftype)
{
   int err;
   struct dirent* result = 0;

   errno = 0;
   result = readdir(cast2DIR_directory(dir));
   if (!result && errno) {
      err = errno;
      TRACESYSCALL_ERRLOG("readdir",err);
      goto ONERR;
   }

   if (ftype) {
      *ftype = ftUnknown;
      if (result) {
         struct stat statbuf;
         switch(result->d_type) {
         case DT_BLK:   *ftype = ftBlockDevice;      break;
         case DT_CHR:   *ftype = ftCharacterDevice;  break;
         case DT_DIR:   *ftype = ftDirectory;        break;
         case DT_FIFO:  *ftype = ftNamedPipe;        break;
         case DT_REG:   *ftype = ftRegularFile;      break;
         case DT_SOCK:  *ftype = ftSocket;           break;
         case DT_LNK:   *ftype = ftSymbolicLink;     break;
         case DT_UNKNOWN:
         default:
                       if (0 == fstatat(io_directory(dir), result->d_name, &statbuf, AT_SYMLINK_NOFOLLOW)) {
                           if (S_ISBLK(statbuf.st_mode))       { *ftype = ftBlockDevice; }
                           else if (S_ISCHR(statbuf.st_mode))  { *ftype = ftCharacterDevice; }
                           else if (S_ISDIR(statbuf.st_mode))  { *ftype = ftDirectory; }
                           else if (S_ISFIFO(statbuf.st_mode)) { *ftype = ftNamedPipe; }
                           else if (S_ISLNK(statbuf.st_mode))  { *ftype = ftSymbolicLink; }
                           else if (S_ISREG(statbuf.st_mode))  { *ftype = ftRegularFile; }
                           else if (S_ISSOCK(statbuf.st_mode)) { *ftype = ftSocket; }
                        }
                        break;
         }
      }
   }

   *name = (result ? result->d_name : 0);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int gofirst_directory(directory_t* dir)
{
   int err;

   VALIDATE_INPARAM_TEST(dir, ONERR, );

   rewinddir(cast2DIR_directory(dir));
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: write

int makedirectory_directory(directory_t* dir, const char* directory_path)
{
   int err;
   int mkdiratfd = AT_FDCWD;

   if (dir) {
      mkdiratfd = io_directory(dir);
   }

   err = mkdirat(mkdiratfd, directory_path, 0700);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("mkdirat(mkdiratfd, directory_path, 0700)",err);
      PRINTINT_ERRLOG(mkdiratfd);
      PRINTCSTR_ERRLOG(directory_path);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int makefile_directory(const directory_t* dir, const char* file_path, off_t file_length)
{
   int err;
   int fd       = -1;
   int openatfd = AT_FDCWD;

   if (dir) {
      openatfd = io_directory(dir);
   }

   fd = openat(openatfd, file_path, O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC, S_IRUSR|S_IWUSR);
   if (-1 == fd) {
      err = errno;
      TRACESYSCALL_ERRLOG("openat(openatfd, file_path)",err);
      PRINTINT_ERRLOG(openatfd);
      PRINTCSTR_ERRLOG(file_path);
      goto ONERR;
   }

   err = ftruncate(fd, file_length);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("ftruncate(file_path, file_length)",err);
      PRINTCSTR_ERRLOG(file_path);
      PRINTUINT64_ERRLOG(file_length);
      goto ONERR;
   }

   free_iochannel(&fd);

   return 0;
ONERR:
   if (! isfree_iochannel(fd)) {
      free_iochannel(&fd);
      unlinkat(openatfd, file_path, 0);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int removedirectory_directory(const directory_t* dir, const char* directory_path)
{
   int err;
   int unlinkatfd = AT_FDCWD;

   if (dir) {
      unlinkatfd = io_directory(dir);
   }

   err = unlinkat(unlinkatfd, directory_path, AT_REMOVEDIR);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("unlinkat(unlinkatfd, directory_path)",err);
      PRINTINT_ERRLOG(unlinkatfd);
      PRINTCSTR_ERRLOG(directory_path);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int removefile_directory(const directory_t* dir, const char* file_path)
{
   int err;
   int unlinkatfd = AT_FDCWD;

   if (dir) {
      unlinkatfd = io_directory(dir);
   }

   err = unlinkat(unlinkatfd, file_path, 0);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("unlinkat(unlinkatfd, file_path)",err);
      PRINTINT_ERRLOG(unlinkatfd);
      PRINTCSTR_ERRLOG(file_path);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: test

#ifdef KONFIG_UNITTEST

static int test_checkpath(void)
{
   directory_t* basedir = 0;

   for (int i = 0; i < 2; ++i) {

      if (i == 1) {
         TEST(0 == new_directory(&basedir, ".", 0));
      }

      // TEST trypath_directory: files exist in working dir
      TEST(0 == trypath_directory(basedir, "." ));
      TEST(0 == trypath_directory(basedir, ".." ));
      TEST(0 == trypath_directory(basedir, "bin/" ));
      TEST(0 == trypath_directory(basedir, "LICENSE" ));
      TEST(0 == trypath_directory(basedir, "README" ));

      // TEST trypath_directory: absolute files exist
      TEST(0 == trypath_directory(basedir, "/" ));
      TEST(0 == trypath_directory(basedir, "/home" ));
      TEST(0 == trypath_directory(basedir, "/usr" ));
      TEST(0 == trypath_directory(basedir, "/../../"));

      // TEST trypath_directory: ENOENT
      TEST(ENOENT == trypath_directory(basedir, "123456.XXX" ));

      // TEST trypath_directory: EFAULT
      TEST(EFAULT == trypath_directory(basedir, 0));
   }

   TEST(0 == delete_directory(&basedir));

   return 0;
ONERR:
   return EINVAL;
}

static int test_directory_stream__nextdir(directory_t* dir, int test_flags_value)
{
   const char* read_name;
   filetype_e  read_type;
   int read_flag[100] = { 0 };
   int isDir  = 0 != (test_flags_value&2);
   int isFile = 0 != (test_flags_value&1);

   for (int i = 0; i < 100 * (isDir + isFile); ++i) {
      unsigned read_number = 0;
      do {
         TEST(0 == next_directory(dir, &read_name, &read_type));
      } while (!strcmp(read_name, ".") || !strcmp(read_name, ".."));
      if (0 == strncmp(read_name, "file_", 5)) {
         sscanf( read_name+5, "%u", &read_number);
         TEST(read_type == ftRegularFile);
         TEST(0 == (read_flag[read_number] & 1));
         TEST(read_number < 100);
         read_flag[read_number] |= 1;
      } else {
         TEST(0 == strncmp(read_name, "dir_", 4));
         sscanf( read_name+4, "%u", &read_number);
         TEST(read_type == ftDirectory);
         TEST(0 == (read_flag[read_number] & 2));
         TEST(read_number < 100);
         read_flag[read_number] |= 2;
      }
   }
   // end of read returns 0 in read_name
   do {
      read_name = (const char*) 1;
      read_type = ftDirectory;
      TEST(0 == next_directory(dir, &read_name, &read_type));
   } while (read_name && (!strcmp(read_name, ".") || !strcmp(read_name, "..")));
   TEST(read_name == 0);
   TEST(read_type == ftUnknown);
   // all files found
   for (int i = 0; i < 100; ++i) {
      TEST(test_flags_value == read_flag[i]);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   directory_t* temp_dir  = 0;
   directory_t* dir       = 0;
   cstring_t    tmppath   = cstring_INIT;
   wbuffer_t    tmppathwb = wbuffer_INIT_CSTRING(&tmppath);
   const char*  fname;
   size_t       nr_files;
   glob_t       fndfiles;
   int          fd_oldwd = -1;

   fd_oldwd = open( ".", O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC);
   TEST(-1 != fd_oldwd);

   // TEST new_directory: current working dir "."
   TEST(0 == new_directory(&dir, ".", NULL));
   TEST(0 != dir);
   TEST(0 == glob("*", GLOB_PERIOD|GLOB_NOSORT, 0, &fndfiles));
   for (nr_files = 0; 0 == next_directory(dir, &fname, 0); ++nr_files) {
      if (!fname) {
         TEST(nr_files == fndfiles.gl_pathc);
         break;
      }
      TEST(0 == strcmp(fname, fndfiles.gl_pathv[nr_files]));
   }
   globfree(&fndfiles);

   // TEST delete_directory
   TEST(0 == delete_directory(&dir));
   TEST(!dir);
   TEST(0 == delete_directory(&dir));
   TEST(!dir);

   // TEST new_directory: current working dir ""
   TEST(0 == new_directory(&dir, "", NULL));
   TEST(0 != dir);
   TEST(0 == glob("*", GLOB_PERIOD|GLOB_NOSORT, 0, &fndfiles));
   for (nr_files = 0; 0 == next_directory(dir, &fname, 0); ++nr_files) {
      if (!fname) {
         TEST(nr_files == fndfiles.gl_pathc);
         break;
      }
      TEST(0 == strcmp(fname, fndfiles.gl_pathv[nr_files]));
   }
   globfree(&fndfiles);

   // TEST delete_directory
   TEST(0 == delete_directory(&dir));
   TEST(!dir);
   TEST(0 == delete_directory(&dir));
   TEST(!dir);

   // TEST newtemp_directory
   TEST(0 == newtemp_directory(&temp_dir, "test1", &tmppathwb));
   // check
   TEST(0 != temp_dir);
   TEST(18 == size_wbuffer(&tmppathwb));
   TEST(0 == strncmp(str_cstring(&tmppath), "/tmp/test1.", 11));
   TEST(0 == glob(str_cstring(&tmppath), GLOB_NOSORT, 0, &fndfiles));
   TEST(1 == fndfiles.gl_pathc);
   TEST(0 == strcmp(fndfiles.gl_pathv[0], str_cstring(&tmppath)));
   // reset
   globfree(&fndfiles);
   clear_wbuffer(&tmppathwb);
   TEST(0 == delete_directory(&temp_dir));
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath)));

   // TEST newtemp_directory: name_prefix == 0 && dirpath == 0
   TEST(0 == newtemp_directory(&temp_dir, 0, 0));
   // check
   TEST(0 != temp_dir);
   TEST(0 == path_directory(temp_dir, &tmppathwb));
   TEST(13 == size_wbuffer(&tmppathwb));
   TEST(0 == strncmp(str_cstring(&tmppath), "/tmp/.", 6));
   TEST(0 == glob(str_cstring(&tmppath), GLOB_NOSORT, 0, &fndfiles));
   TEST(1 == fndfiles.gl_pathc);
   TEST(0 == strcmp(fndfiles.gl_pathv[0], str_cstring(&tmppath)));
   // reset
   globfree(&fndfiles);
   clear_wbuffer(&tmppathwb);
   TEST(0 == delete_directory(&temp_dir));
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath)));

   // TEST newtemp_directory: ENAMETOOLONG (prefix too long)
   for (int i = 0; i < 2; ++i) {
      char long_prefix[sizeof(((struct dirent*)0)->d_name) - 6];
      memset(long_prefix, 'a', sizeof(long_prefix)-1);
      long_prefix[sizeof(long_prefix)-1] = 0;
      TEST(ENAMETOOLONG == newtemp_directory(&temp_dir, long_prefix, i ? 0 : &tmppathwb));
      // check
      TEST(0 == temp_dir);
      TEST(0 == size_wbuffer(&tmppathwb));
      TEST(GLOB_NOMATCH == glob("/tmp/aaaaaaaaaaaaaaaa*", GLOB_NOSORT, 0, &fndfiles));
   }

   // TEST newtemp_directory: simulated ERROR
   for (int i = 2;; ++i) {
      init_testerrortimer(&s_directory_errtimer, (unsigned)i/2, i);
      int err = newtemp_directory(&temp_dir, "xyzgamma", (i&1) ? &tmppathwb : 0);
      if (!err) {
         TEST(8 == i);
         // reset
         TEST(0 == delete_directory(&temp_dir));
         TEST(0 == glob("/tmp/xyzgamma.*", GLOB_NOSORT, 0, &fndfiles));
         TEST(1 == fndfiles.gl_pathc);
         TEST(0 == rmdir(fndfiles.gl_pathv[0]));
         globfree(&fndfiles);
         free_testerrortimer(&s_directory_errtimer);
         break;
      }
      TEST(i == err);
      // check
      TEST(0 == temp_dir);
      TEST(0 == size_wbuffer(&tmppathwb));
      TEST(GLOB_NOMATCH == glob("/tmp/xyzgamma.*", GLOB_NOSORT, 0, &fndfiles));
   }

   // TEST new_directory: relative path
   TEST(0 == newtemp_directory(&temp_dir, "test1", &tmppathwb));
   TEST(0 == mkdirat(io_directory(temp_dir), "reldir.123", 0777));
   TEST(0 == new_directory(&dir, "reldir.123", temp_dir));
   TEST(0 != dir);

   // TEST delete_directory
   TEST(0 == delete_directory(&dir));
   TEST(!dir);
   TEST(0 == delete_directory(&dir));
   TEST(!dir);
   TEST(0 == unlinkat(io_directory(temp_dir), "reldir.123", AT_REMOVEDIR));

   // TEST new_directory: current working dir + relative_to
   TEST(0 == new_directory(&dir, "", temp_dir));
   TEST(0 == fchdir(io_directory(temp_dir)));
   TEST(0 == glob("*", GLOB_PERIOD|GLOB_NOSORT, 0, &fndfiles));
   TEST(0 == fchdir(fd_oldwd));
   for (nr_files = 0; 0 == next_directory(dir, &fname, 0); ++nr_files) {
      if (!fname) {
         TEST(nr_files == fndfiles.gl_pathc);
         break;
      }
      TEST(0 == strcmp(fname, fndfiles.gl_pathv[nr_files]));
   }
   globfree(&fndfiles);
   TEST(0 == delete_directory(&dir));
   TEST(0 == new_directory(&dir, ".", temp_dir));
   TEST(0 == delete_directory(&dir));

   // TEST new_directory: absolute pathname + relative_to
   TEST(0 == new_directory(&dir, "/", temp_dir));
   TEST(0 == glob("/*", GLOB_PERIOD|GLOB_NOSORT, 0, &fndfiles));
   for (nr_files = 0; 0 == next_directory(dir, &fname, 0); ++nr_files) {
      if (!fname) {
         TEST(nr_files == fndfiles.gl_pathc);
         break;
      }
      TEST(0 == strcmp(fname, 1+fndfiles.gl_pathv[nr_files]));
   }
   globfree(&fndfiles);
   TEST(0 == delete_directory(&dir));

   // TEST makefile_directory, makedirectory_directory
   for (int i = 0; i < 100; ++i) {
      char filename[100];
      sprintf( filename, "file_%06d", i);
      TEST(0 == makefile_directory(temp_dir, filename, 0));
      sprintf( filename, "dir_%06d", i);
      TEST(0 == makedirectory_directory(temp_dir, filename));
   }

   // TEST next_directory
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), NULL));
   TEST(0 == test_directory_stream__nextdir(dir, 3));
   TEST(0 == delete_directory(&dir));
   TEST(0 == test_directory_stream__nextdir(temp_dir, 3));

   // TEST makefile_directory: EINVAL
   TEST(EINVAL == makefile_directory(temp_dir, "123", -1));
   TEST(ENOENT == trypath_directory(temp_dir, "123"));

   // TEST makefile_directory: EFBIG
   TEST(EFBIG == makefile_directory(temp_dir, "123", 0x7fffffffffffffff));
   TEST(ENOENT == trypath_directory(temp_dir, "123"));

   // TEST new_directory: ENOENT
   TEST(ENOENT == new_directory(&dir, ".....", NULL));

   // TEST new_directory: ENOTDIR
   char buffer[1000];
   assert(sizeof(buffer) > size_cstring(&tmppath)+13);
   strcpy( buffer, str_cstring(&tmppath) );
   strcat( buffer, "/file_000000" );
   TEST(ENOTDIR == new_directory(&dir, buffer, NULL));

   // TEST makedirectory_directory, makefile_directory: EACCES
   TEST(0 == new_directory(&dir, "/", NULL));
   TEST(EACCES == makedirectory_directory(dir, "XXXXXXXX.test" ));
   TEST(EACCES == makefile_directory(dir, "XXXXXXXX.test", 0 ));
   TEST(0 == delete_directory(&dir));

   // TEST removedirectory_directory: ENOTEMPTY
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), NULL));
   TEST(ENOTEMPTY == removedirectory_directory(dir, ".."));
   TEST(0 == delete_directory(&dir));

   // TEST removedirectory_directory
   for (int i = 0; i < 100; ++i) {
      char filename[100];
      sprintf( filename, "dir_%06d", i);
      TEST(0 == removedirectory_directory(temp_dir, filename));
   }
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), NULL));
   TEST(0 == test_directory_stream__nextdir(dir, 1));
   TEST(0 == delete_directory(&dir));
   TEST(0 == gofirst_directory(temp_dir));
   TEST(0 == test_directory_stream__nextdir(temp_dir, 1));

   // TEST removefile
   for (int i = 0; i < 100; ++i) {
      char filename[100];
      sprintf( filename, "file_%06d", i);
      TEST(0 == removefile_directory(temp_dir, filename));
   }
   TEST(0 == new_directory(&dir, str_cstring(&tmppath), NULL));
   TEST(0 == test_directory_stream__nextdir(dir, 0));
   TEST(0 == delete_directory(&dir));
   TEST(0 == gofirst_directory(temp_dir));
   TEST(0 == test_directory_stream__nextdir(temp_dir, 0));

   // adapt LOG
   uint8_t *logbuffer;
   size_t   logsize;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   if (logsize) {
      char * found = (char*)logbuffer;
      while ( (found = strstr(found, str_cstring(&tmppath))) ) {
         if (!strchr(found, '.')) break;
         memcpy(strchr(found, '.')+1, "123456", 6);
         found += size_cstring(&tmppath);
      }
   }

   // reset
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath)));
   TEST(0 == delete_directory(&temp_dir));
   TEST(0 == free_cstring(&tmppath));
   TEST(0 == free_iochannel(&fd_oldwd));

   return 0;
ONERR:
   assert(0 == fchdir(fd_oldwd));
   (void) delete_directory(&temp_dir);
   (void) delete_directory(&dir);
   free_cstring(&tmppath);
   free_iochannel(&fd_oldwd);
   return EINVAL;
}

static int test_workingdir(void)
{
   directory_t* local1 = 0;
   directory_t* local2 = 0;

   // TEST "" & "." are the same
   TEST(0 == new_directory(&local1, "", 0));
   TEST(0 == new_directory(&local2, ".", 0));

   for (;;) {
      const char  * name1, * name2;
      filetype_e  ft1,     ft2;
      TEST(0 == next_directory(local1, &name1, &ft1));
      TEST(0 == next_directory(local2, &name2, &ft2));
      if (!name1 || !name2) {
         TEST(name1 == name2);
         break;
      }
      TEST(0 == strcmp(name1, name2));
   }

   TEST(0 == delete_directory(&local1));
   TEST(0 == delete_directory(&local2));

   return 0;
ONERR:
   delete_directory(&local1);
   delete_directory(&local2);
   return EINVAL;
}

static int test_filesize(void)
{
   directory_t* workdir   = 0;
   directory_t* tempdir   = 0;
   uint8_t      tmppath[256];

   // prepare
   TEST(0 == new_directory(&workdir, "", 0));
   TEST(0 == newtemp_directory(&tempdir, "tempdir", &(wbuffer_t)wbuffer_INIT_STATIC(sizeof(tmppath), tmppath)));
   for (int i = 0; i < 100; ++i) {
      char filename[100];
      sprintf( filename, "file_%06d", i);
      TEST(0 == makefile_directory(tempdir, filename, 0));
      int fd = openat(io_directory(tempdir), filename, O_RDWR|O_CLOEXEC);
      TEST(fd > 0);
      int written = (int) write(fd, filename, (size_t) i);
      free_iochannel(&fd);
      TEST(i == written);
   }

   // TEST filesize workdir != 0
   for (int i = 0; i < 100; ++i) {
      char filename[100];
      sprintf( filename, "file_%06d", i);
      off_t file_size = -1;
      TEST(0 == filesize_directory( tempdir, filename, &file_size));
      TEST(i == file_size);
   }

   // TEST filesize workdir == 0
   TEST(0 == fchdir(io_directory(tempdir)));
   for (int i = 0; i < 100; ++i) {
      char filename[100];
      sprintf( filename, "file_%06d", i);
      off_t file_size = -1;
      TEST(0 == filesize_directory( 0, filename, &file_size));
      TEST(i == file_size);
   }
   TEST(0 == fchdir(io_directory(workdir)));

   // unprepare
   for (int i = 0; i < 100; ++i) {
      char filename[100];
      sprintf( filename, "file_%06d", i);
      TEST(0 == removefile_directory(tempdir, filename));
   }
   TEST(0 == removedirectory_directory(0, (const char*)tmppath));
   TEST(0 == delete_directory(&tempdir));
   TEST(0 == delete_directory(&workdir));

   return 0;
ONERR:
   if (workdir) {
      assert(0 == fchdir(io_directory(workdir)));
   }
   delete_directory(&workdir);
   delete_directory(&tempdir);
   return EINVAL;
}

static int test_query_path(void)
{
   directory_t * tempdir = 0;
   directory_t * dir     = 0;
   char          tmppath[256];
   char          buffer[sys_path_MAXSIZE];
   wbuffer_t     pathwb  = wbuffer_INIT_STATIC(sizeof(buffer), (uint8_t*)buffer);
   cstring_t     parent  = cstring_INIT;
   cstring_t     cstrbuf = cstring_INIT;
   wbuffer_t     pathcstr = wbuffer_INIT_CSTRING(&cstrbuf);

   // prepare
   TEST(0 == newtemp_directory(&tempdir, "qp", &(wbuffer_t)wbuffer_INIT_STATIC(sizeof(tmppath), (uint8_t*)tmppath)));
   TEST(0 == strncmp(tmppath, "/tmp/qp.", 8));

   // TEST path_directory: /
   TEST(0 == new_directory(&dir, "/", 0));
   TEST(0 == path_directory(dir, &pathwb));
   TEST(2 == size_wbuffer(&pathwb));
   TEST(0 == strcmp(buffer, "/"));

   // TEST path_directory: appends path to wbuffer_t
   for (unsigned i = 2; i <= 4; ++i) {
      buffer[i] = '.';
      TEST(i == size_wbuffer(&pathwb));
      TEST(0 == shrink_wbuffer(&pathwb, i-1)); // remove trailing \0 byte
      TEST(0 == path_directory(dir, &pathwb));
      TEST(i+1 == size_wbuffer(&pathwb));
      TEST(0 == strncmp(buffer, "////", i));
      TEST(0 == buffer[i]); // trailing \0 byte
   }
   TEST(0 == delete_directory(&dir));

   // TEST path_directory: temp directory
   clear_wbuffer(&pathwb);
   TEST(0 == path_directory(tempdir, &pathwb));
   TEST(strlen(tmppath)+1 == size_wbuffer(&pathwb));
   TEST(0 == strcmp(buffer, tmppath));
   TEST(0 == new_directory(&dir, "..", tempdir));
   clear_wbuffer(&pathwb);
   TEST(0 == path_directory(dir, &pathwb));
   TEST(5 == size_wbuffer(&pathwb));
   TEST(0 == strcmp(buffer, "/tmp"));
   TEST(0 == delete_directory(&dir));

   // TEST path_directory: follow chain
   const char * testname[] = {
      "__test__", "abc---^^__.ddd.d", "query", "gnavsoq5789vmdkwdzenv", "asvuhwenwef8236235nj",
      "1", "2", "3", "4", "9", "_usr", "lib", "opt", "simple", ".vvv", "-", "--", ".d.d",
      "xx_____________________________XX%%$''"
   };
   char testpath[1024] = { 0 };
   TEST(0 == path_directory(tempdir, &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(testpath), (uint8_t*)testpath)));
   for (unsigned i = 0; i < lengthof(testname); ++i) {
      size_t len = strlen(testpath) + 1 + strlen(testname[i]);
      TEST(len < sizeof(testpath));
      strcat(testpath, "/");
      strcat(testpath, testname[i]);
      TEST(0 == makedirectory_directory(0, testpath));
      TEST(0 == new_directory(&dir, testpath, 0));
      clear_wbuffer(&pathwb);
      TEST(0 == path_directory(dir, &pathwb));
      TEST(len+1 == size_wbuffer(&pathwb));
      TEST(0 == memcmp(buffer, testpath, len+1));
      TEST(0 == delete_directory(&dir));
   }

   // TEST path_directory: on empty cstrbuf
   TEST(0 == new_directory(&dir, testpath, 0));
   TEST(0 == path_directory(dir, &pathcstr));
   TEST(0 == size_cstring(&cstrbuf));
   TEST(strlen(testpath)+1 == capacity_cstring(&cstrbuf));
   TEST(0 == memcmp(str_cstring(&cstrbuf), testpath, capacity_cstring(&cstrbuf)));

   // TEST path_directory: on allocated cstrbuf
   for (int i = '1'; i <= '5'; ++i) {
      appendbyte_wbuffer(&pathcstr, (uint8_t)i); // double string in size and remove \0 byte
   }
   size_t C = capacity_cstring(&cstrbuf);
   TEST(C > strlen(testpath)+1);
   clear_wbuffer(&pathcstr);
   appendbyte_wbuffer(&pathcstr, 'S');
   TEST(0 == path_directory(dir, &pathcstr));
   // check cstrbuf
   TEST(0 == size_cstring(&cstrbuf));
   TEST(C == capacity_cstring(&cstrbuf));
   // check appended to 'S' + \0 byte
   TEST('S' == str_cstring(&cstrbuf)[0]);
   TEST(0 == memcmp(str_cstring(&cstrbuf)+1, testpath, strlen(testpath)+1));
   TEST(0 == memcmp(str_cstring(&cstrbuf)+2+strlen(testpath), "2345", 4));

   // reset
   TEST(0 == delete_directory(&dir));
   for ( size_t len = strlen(testpath), i = lengthof(testname);
         i > 0;
         --i, len -= strlen(testname[i])+1, testpath[len] = 0) {
      TEST(0 == removedirectory_directory(0, (const char*)testpath));
   }

   // TEST path_directory: ERROR
   TEST(0 == makedirectory_directory(tempdir, "1"));
   TEST(0 == new_directory(&dir, "1", tempdir));
   clear_wbuffer(&pathwb);
   for (int i = 1;; ++i) {
      size_t logsize, logsize2;
      uint8_t* logbuffer;
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      init_testerrortimer(&s_directory_errtimer, (unsigned) i, i);
      int err = path_directory(dir, &pathwb);
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      if (!err) {
         TEST(logsize == logsize2);
         TESTP(40 < i, "i=%d", i);
         // reset
         free_testerrortimer(&s_directory_errtimer);
         TEST(0 == removedirectory_directory(tempdir, "1"));
         TEST(0 == delete_directory(&dir));
         break;
      }
      TEST(logsize < logsize2); // log written
      TEST(i == err);
      TEST(0 == size_wbuffer(&pathwb));
      // reset
      TRUNCATEBUFFER_ERRLOG(logsize); // too much log
   }

   // reset
   TEST(0 == path_directory(tempdir, &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(testpath), (uint8_t*)testpath)));
   TEST(0 == delete_directory(&tempdir));
   TEST(0 == free_cstring(&parent));
   TEST(0 == free_cstring(&cstrbuf));
   TEST(0 == removedirectory_directory(0, testpath));

   return 0;
ONERR:
   free_cstring(&parent);
   free_cstring(&cstrbuf);
   delete_directory(&tempdir);
   delete_directory(&dir);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t   usage      = resourceusage_FREE;
   unsigned          open_count = 0;
   iochannel_t       dummyfile[8];

   if (test_initfree())          goto ONERR;
   CLEARBUFFER_ERRLOG();

   TEST(0 == init_resourceusage(&usage));

   // increment open files to 8 to make logged fd number always the same (support debug && X11 which opens files)
   {
      size_t nrfdopen;
      TEST(0 == nropen_iochannel(&nrfdopen));
      for (;nrfdopen < 8; ++ nrfdopen, ++ open_count) {
         TEST(0 == initcopy_iochannel(&dummyfile[open_count], STDOUT_FILENO));
      }
   }

   if (test_checkpath())         goto ONERR;
   if (test_initfree())          goto ONERR;
   if (test_workingdir())        goto ONERR;
   if (test_filesize())          goto ONERR;
   if (test_query_path())        goto ONERR;

   while (open_count) {
      TEST(0 == free_iochannel(&dummyfile[--open_count]));
   }

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   while (open_count) {
      free_iochannel(&dummyfile[--open_count]);
   }
   (void) free_resourceusage(&usage);
   return EINVAL;
}

int unittest_io_directory()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
