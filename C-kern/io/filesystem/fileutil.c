/* title: File-Util impl

   Implements <File-Util>.

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

int load_file(const char * filepath, /*ret*/struct wbuffer_t * result, struct directory_t * relative_to)
{
   int err ;
   off_t    loadsize ;
   size_t   readsize ;
   uint8_t* buffer = 0 ;
   file_t   file   = file_INIT_FREEABLE ;

   err = init_file(&file, filepath, accessmode_READ, relative_to) ;
   if (err) goto ONABORT ;

   err = size_file(file, &loadsize) ;
   if (err) goto ONABORT ;

   if (loadsize < 0 || (sizeof(size_t) < sizeof(off_t) && loadsize >= (off_t)SIZE_MAX)) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   err = appendbytes_wbuffer(result, (size_t)loadsize, &buffer) ;
   if (err) goto ONABORT ;

   err = read_file(file, (size_t)loadsize, buffer, &readsize) ;
   if (err) goto ONABORT ;

   if (readsize != (size_t)loadsize) {
      err = EIO ;
      goto ONABORT ;
   }

   err = free_file(&file) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   if (buffer) clear_wbuffer(result) ;
   (void) free_file(&file) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


int save_file(const char * filepath, size_t file_size, const void * file_content/*[filer_size]*/, struct directory_t * relative_to)
{
   int err ;
   bool   is_created = false ;
   file_t file ;

   err = initcreate_file(&file, filepath, relative_to) ;
   if (err) goto ONABORT ;
   is_created = true ;

   size_t bytes_written ;
   err = write_file(file, file_size, file_content, &bytes_written) ;
   if (err) goto ONABORT ;

   if (bytes_written != file_size) {
      err = EIO ;
      goto ONABORT ;
   }

   err = free_file(&file) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   if (is_created) {
      (void) remove_file(filepath, relative_to) ;
      (void) free_file(&file) ;
   }
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_loadsave(directory_t * tempdir)
{
   file_t      file = file_INIT_FREEABLE ;
   cstring_t   cstr = cstring_INIT ;
   wbuffer_t   wbuf = wbuffer_INIT_FREEABLE ;
   const char  * testcontent[] = { "", "12345", "afigaihoingaspgmsagpj---}n\n", "\u0fffäöäüö" } ;
   off_t       filesize ;
   memblock_t  datablock = memblock_INIT_FREEABLE ;

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

   // TEST save_file, load_file: bigger file
   TEST(0 == RESIZE_MM(1024*1024, &datablock)) ;
   for (size_t i = 0; i < size_memblock(&datablock); ++i) {
      addr_memblock(&datablock)[i] = (uint8_t)(11*i) ;
   }
   TEST(0 == save_file("save", size_memblock(&datablock), addr_memblock(&datablock), tempdir)) ;
   wbuf = (wbuffer_t) wbuffer_INIT_CSTRING(&cstr) ;
   TEST(0 == load_file("save", &wbuf, tempdir)) ;
   TEST(size_cstring(&cstr) == size_memblock(&datablock)) ;
   for (size_t i = 0; i < size_memblock(&datablock); ++i) {
      TEST(addr_memblock(&datablock)[i] == (uint8_t) str_cstring(&cstr)[i]) ;
   }
   TEST(0 == FREE_MM(&datablock)) ;
   TEST(0 == free_cstring(&cstr)) ;

   // TEST save_file: EEXIST
   TEST(EEXIST == save_file("save", 1, "", tempdir)) ;
   TEST(0 == remove_file("save", tempdir)) ;

   // TEST load_file: ENOENT
   TEST(ENOENT == load_file("save", 0, tempdir)) ;

   return 0 ;
ONABORT:
   free_cstring(&cstr) ;
   free_file(&file) ;
   FREE_MM(&datablock) ;
   remove_file("save", tempdir) ;
   return EINVAL ;
}

int unittest_io_fileutil()
{
   resourceusage_t   usage   = resourceusage_INIT_FREEABLE ;
   cstring_t         tmppath = cstring_INIT ;
   directory_t     * tempdir = 0 ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == newtemp_directory(&tempdir, "iofiletest")) ;
   TEST(0 == path_directory(tempdir, &(wbuffer_t)wbuffer_INIT_CSTRING(&tmppath))) ;

   if (test_loadsave(tempdir))   goto ONABORT ;

   /* adapt log */
   uint8_t *logbuffer ;
   size_t   logsize ;
   GETBUFFER_ERRLOG(&logbuffer, &logsize) ;
   while (strstr((char*)logbuffer, "/iofiletest.")) {
      logbuffer = (uint8_t*)strstr((char*)logbuffer, "/iofiletest.")+12;
      memcpy(logbuffer, "XXXXXX", 6) ;
   }

   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

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
