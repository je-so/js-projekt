/* title: Filesystem-Path impl

   Implements <Filesystem-Path>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/filesystem/filepath.h
    Header file <Filesystem-Path>.

   file: C-kern/platform/Linux/io/filepath.c
    Implementation file <Filesystem-Path impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filesystem/filepath.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: filepath_static_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_filepathstatic_errtimer
 * Simulates an error in <init_filepathstatic>. */
static test_errortimer_t   s_filepathstatic_errtimer = test_errortimer_FREE ;
#endif

// group: lifetime

void init_filepathstatic(/*out*/filepath_static_t * fpath, const struct directory_t * workdir, const char * filename)
{
   int err ;

   if (  workdir
         && (  !filename
               || filename[0] != '/')) {
      wbuffer_t path = wbuffer_INIT_STATIC(sizeof(fpath->workdir)-2, (uint8_t*)fpath->workdir) ;
      err = path_directory(workdir, &path) ;
      SETONERROR_testerrortimer(&s_filepathstatic_errtimer, &err) ;
      if (err) {
         memcpy(fpath->workdir, "???ERR/", sizeof("???ERR/")/*include trailing \0*/) ;
      } else {
         size_t size = size_wbuffer(&path);
         if (fpath->workdir[size-1] != '/') {
            fpath->workdir[size] = '/';
            ++size;
         }
         fpath->workdir[size] = 0;
      }
   } else {
      fpath->workdir[0] = 0;
   }

   fpath->filename = filename ? filename : "";
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_filepathstatic(void)
{
   filepath_static_t    fpath ;
   const char *         F ;
   cstring_t            workpath   = cstring_INIT ;
   wbuffer_t            workpathwb = wbuffer_INIT_CSTRING(&workpath) ;
   directory_t *        workdir    = 0 ;
   char                 fullpath[sys_path_MAXSIZE] ;

   // prepare
   TEST(0 == new_directory(&workdir, "", 0)) ;
   TEST(0 == path_directory(workdir, &workpathwb)) ;

   // TEST init_filepathstatic: workdir == 0 && filename == 0
   memset(&fpath, 255, sizeof(fpath)) ;
   init_filepathstatic(&fpath, 0, 0) ;
   TEST(0 == fpath.workdir[0]) ;
   TEST(0 != fpath.filename) ;
   TEST(0 == fpath.filename[0]) ;

   // TEST init_filepathstatic: workdir == 0
   memset(&fpath, 255, sizeof(fpath)) ;
   F = "test-filename" ;
   init_filepathstatic(&fpath, 0, F) ;
   TEST(0 == fpath.workdir[0]) ;
   TEST(F == fpath.filename) ;

   // TEST init_filepathstatic: filename == 0
   memset(&fpath, 255, sizeof(fpath)) ;
   init_filepathstatic(&fpath, workdir, 0) ;
   TEST('/' == fpath.workdir[size_cstring(&workpath)]) ;
   TEST(0 == fpath.workdir[size_cstring(&workpath)+1]) ;
   TEST(0 == memcmp(fpath.workdir, str_cstring(&workpath), size_cstring(&workpath))) ;
   TEST(0 != fpath.filename) ;
   TEST(0 == fpath.filename[0]) ;

   // TEST init_filepathstatic: filename[0] != '/'
   memset(&fpath, 255, sizeof(fpath)) ;
   F = "test-filename" ;
   init_filepathstatic(&fpath, workdir, F) ;
   TEST('/' == fpath.workdir[size_cstring(&workpath)]) ;
   TEST(0 == fpath.workdir[size_cstring(&workpath)+1]) ;
   TEST(0 == memcmp(fpath.workdir, str_cstring(&workpath), size_cstring(&workpath))) ;
   TEST(F == fpath.filename) ;

   // TEST init_filepathstatic: absolute filename (filename[0] == '/')
   memset(&fpath, 255, sizeof(fpath)) ;
   F = "/tmp/test-filename" ;
   init_filepathstatic(&fpath, workdir, F) ;
   TEST(0 == fpath.workdir[0]) ;
   TEST(F == fpath.filename) ;

   // TEST init_filepathstatic: ERROR
   init_testerrortimer(&s_filepathstatic_errtimer, 1, ENOENT) ;
   F = "test-filename" ;
   init_filepathstatic(&fpath, workdir, F) ;
   TEST(0 == strcmp("???ERR/", fpath.workdir)) ;
   TEST(F == fpath.filename) ;

   // TEST STRPARAM_filepathstatic
   F = "test-filename" ;
   init_filepathstatic(&fpath, workdir, F) ;
   memset(fullpath, 255, sizeof(fullpath)) ;
   snprintf(fullpath, sizeof(fullpath), "%s%s", STRPARAM_filepathstatic(&fpath)) ;
   TEST(0 == strncmp(fullpath, str_cstring(&workpath), size_cstring(&workpath))) ;
   TEST('/' == fullpath[size_cstring(&workpath)]) ;
   TEST(0 == strcmp(fullpath + size_cstring(&workpath) + 1, F)) ;

   // unprepare
   TEST(0 == free_cstring(&workpath)) ;
   TEST(0 == delete_directory(&workdir)) ;

   return 0 ;
ONERR:
   free_cstring(&workpath) ;
   delete_directory(&workdir) ;
   return EINVAL ;
}

int unittest_io_filepath()
{
   if (test_filepathstatic())    goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
