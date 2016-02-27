/* title: File-Util impl

   Implements <File-Util>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/filesystem/fileutil.h
    Header file <File-Util>.

   file: C-kern/io/filesystem/fileutil.c
    Implementation file <File-Util impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/memory/wbuffer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/cstring.h"
#endif

// section: Functions

// group: util

int load_file(const char* filepath, /*ret*/struct wbuffer_t* result, struct directory_t* relative_to)
{
   int err;
   off_t  loadsize;
   size_t readsize;
   size_t oldsize = size_wbuffer(result);
   file_t file    = file_FREE;

   err = init_file(&file, filepath, accessmode_READ, relative_to);
   if (err) goto ONERR;

   err = size_file(file, &loadsize);
   if (err) goto ONERR;

   if (loadsize < 0 || (OFF_MAX > SIZE_MAX && loadsize >= SIZE_MAX)) {
      err = ENOMEM;
      goto ONERR;
   }

   uint8_t* buffer;
   err = appendbytes_wbuffer(result, (size_t)loadsize, &buffer) ;
   if (err) goto ONERR;

   err = read_file(file, (size_t)loadsize, buffer, &readsize) ;
   if (err) goto ONERR;

   if (readsize != (size_t)loadsize) {
      err = EIO;
      goto ONERR;
   }

   err = free_file(&file);
   if (err) goto ONERR;

   return 0;
ONERR:
   shrink_wbuffer(result, oldsize);
   (void) free_file(&file);
   TRACEEXIT_ERRLOG(err);
   return err;
}


int save_file(const char * filepath, size_t file_size, const void * file_content/*[filer_size]*/, struct directory_t * relative_to)
{
   int err ;
   bool   is_created = false ;
   file_t file ;

   err = initcreate_file(&file, filepath, relative_to) ;
   if (err) goto ONERR;
   is_created = true ;

   size_t bytes_written ;
   err = write_file(file, file_size, file_content, &bytes_written) ;
   if (err) goto ONERR;

   if (bytes_written != file_size) {
      err = EIO ;
      goto ONERR;
   }

   err = free_file(&file) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   if (is_created) {
      (void) remove_file(filepath, relative_to) ;
      (void) free_file(&file) ;
   }
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_loadsave(directory_t * tempdir)
{
   file_t      file = file_FREE;
   cstring_t   cstr = cstring_INIT;
   wbuffer_t   wbuf = wbuffer_FREE;
   const char* testcontent[] = { "", "12345", "afigaihoingaspgmsagpj---}n\n", "\u0fffäöäüö" };
   off_t       filesize;
   memblock_t  datablock = memblock_FREE;

   // TEST save_file, load_file: small files
   for (unsigned ti = 0; ti < lengthof(testcontent); ++ti) {
      // save_file
      const size_t datasize = strlen(testcontent[ti]) ;
      TEST(0 == save_file("save", datasize, testcontent[ti], tempdir)) ;
      TEST(0 == trypath_directory(tempdir, "save")) ;
      TEST(0 == filesize_directory(tempdir, "save", &filesize)) ;
      TEST(filesize == datasize) ;
      // load_file
      uint8_t buffer[1+datasize] ;
      memset(buffer, 0, datasize) ;
      wbuf = (wbuffer_t) wbuffer_INIT_STATIC(datasize, buffer) ;
      TEST(0 == load_file("save", &wbuf, tempdir)) ;
      TEST(0 == strncmp(testcontent[ti], (char*)buffer, datasize)) ;
      // remove_file
      TEST(0 == remove_file("save", tempdir)) ;
   }

   // TEST save_file: 1MB file size
   TEST(0 == RESIZE_MM(1024*1024, &datablock));
   for (size_t i = 0; i < size_memblock(&datablock); ++i) {
      addr_memblock(&datablock)[i] = (uint8_t)(11*i);
   }
   TEST(0 == save_file("save", size_memblock(&datablock), addr_memblock(&datablock), tempdir));

   // TEST load_file: 1MB file size
   wbuf = (wbuffer_t) wbuffer_INIT_CSTRING(&cstr);
   TEST(0 == load_file("save", &wbuf, tempdir));
   TEST(capacity_cstring(&cstr) == size_memblock(&datablock));
   for (size_t i = 0; i < size_memblock(&datablock); ++i) {
      TEST(addr_memblock(&datablock)[i] == (uint8_t) str_cstring(&cstr)[i]);
   }
   TEST(0 == FREE_MM(&datablock));

   // TEST save_file: EEXIST
   TEST(EEXIST == save_file("save", 1, "", tempdir));
   TEST(0 == remove_file("save", tempdir));

   // TEST load_file: ENOENT
   size_t oldsize = capacity_cstring(&cstr);
   TEST(oldsize == size_wbuffer(&wbuf));
   TEST(0 < oldsize);
   TEST(ENOENT == load_file("save", &wbuf, tempdir));
   TEST(oldsize == capacity_cstring(&cstr));
   TEST(oldsize == size_wbuffer(&wbuf));

   // unprepare
   TEST(0 == free_cstring(&cstr));

   return 0;
ONERR:
   free_cstring(&cstr);
   free_file(&file);
   FREE_MM(&datablock);
   remove_file("save", tempdir);
   return EINVAL;
}

int unittest_io_fileutil()
{
   directory_t* tempdir = 0;
   char         tmppath[128];

   // prepare
   TEST(0 == newtemp_directory(&tempdir, "iofiletest", &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(tmppath), (uint8_t*)tmppath)));

   if (test_loadsave(tempdir))   goto ONERR;

   /* adapt log */
   uint8_t* logbuffer;
   size_t   logsize;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   while (strstr((char*)logbuffer, "/iofiletest.")) {
      logbuffer = (uint8_t*)strstr((char*)logbuffer, "/iofiletest.")+12;
      memcpy(logbuffer, "XXXXXX", 6);
   }

   // unprepare
   TEST(0 == removedirectory_directory(0, tmppath));
   TEST(0 == delete_directory(&tempdir));

   return 0;
ONERR:
   (void) delete_directory(&tempdir);
   return EINVAL;
}

#endif
