/* title: Unittest impl
   Implements <run_unittest>.

   Ruft nacheinander alle Unittests auf.

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

   file: C-kern/api/test/run/unittest.h
    Header file of <Unittest>.

   file: C-kern/test/run_unittest.c
    Implementation file <Unittest impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/run/unittest.h"
#include "C-kern/api/err.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/math/int/signum.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/os/filesystem/mmfile.h"
#include "C-kern/api/os/index/redblacktree.h"
#include "C-kern/api/os/index/splaytree.h"
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/os/sync/semaphore.h"
#include "C-kern/api/os/sync/signal.h"
#include "C-kern/api/os/sync/waitlist.h"
#include "C-kern/api/os/task/exoscheduler.h"
#include "C-kern/api/os/task/exothread.h"
#include "C-kern/api/os/file.h"
#include "C-kern/api/os/locale.h"
#include "C-kern/api/os/malloc.h"
#include "C-kern/api/os/process.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/string/converter.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/umg/umgtype_default.h"
#include "C-kern/api/umg/umgtype_test.h"



typedef char RESULT_STRING[20] ;


/* Ruft einen Unittest auf und notiert das Ergebnis. */
#define RUN(FCT) \
   LOG_CLEARBUFFER() ;  \
   err = FCT() ;        \
   if (!err) generate_logresource(#FCT) ;    \
   if (!err) err = check_logresource(#FCT) ; \
   ++total_count ; if (err) ++err_count ;    \
   print_result( err, &progress, &progress_count) ;

#define GENERATED_LOGRESOURCE_DIR   "C-kern/resource/unittest.log/"


/* function: generate_logresource
 * Writes log buffer to "C-kern/resource/unittest.log/" + test_name. */
static void generate_logresource(const char * test_name)
{
   int     err = EINVAL ;
   int     fd = -1 ;
   char   resource_path[sizeof(GENERATED_LOGRESOURCE_DIR) + strlen(test_name)] ;

   strcpy( resource_path, GENERATED_LOGRESOURCE_DIR ) ;
   strcpy( resource_path + sizeof(GENERATED_LOGRESOURCE_DIR)-1, test_name ) ;

   fd = open( resource_path, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   if (fd < 0) {
      err = errno ;
      goto ABBRUCH ;
   }

   char   * logbuffer ;
   size_t logbuffer_size ;
   LOG_GETBUFFER( &logbuffer, &logbuffer_size ) ;

   if (logbuffer_size) {
      int logsize = write( fd, logbuffer, logbuffer_size ) ;
      if (logbuffer_size != (unsigned)logsize) {
         LOGC_PRINTF(TEST, "logbuffer_size = %d, logsize = %d\n", logbuffer_size, logsize) ;
         goto ABBRUCH ;
      }
   }

   close(fd) ;
   return ;
ABBRUCH:
   if (err != EEXIST) {
      LOGC_PRINTF(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      LOGC_PRINTF(TEST, "ERROR(%d:%s): '" GENERATED_LOGRESOURCE_DIR "%s'\n", err, strerror(err), test_name ) ;
   }
   if (fd >= 0) close(fd) ;
   return ;
}

/* function: check_logresource
 * Compares saved log with content of log buffer.
 * The log *test_name* is read from directory "C-kern/resource/unittest.log/"
 * and compared with the content of the log buffer.
 * The content of the log buffer is queried with <LOG_GETBUFFER>. */
static int check_logresource(const char * test_name)
{
   int              err ;
   mmfile_t     logfile = mmfile_INIT_FREEABLE ;
   off_t   logfile_size ;
   char   resource_path[sizeof(GENERATED_LOGRESOURCE_DIR) + strlen(test_name)] ;

   strcpy( resource_path, GENERATED_LOGRESOURCE_DIR ) ;
   strcpy( resource_path + sizeof(GENERATED_LOGRESOURCE_DIR)-1, test_name ) ;

   err = filesize_directory(resource_path, 0, &logfile_size ) ;
   if (err) goto ABBRUCH ;

   if (logfile_size != 0) {
      err = init_mmfile( &logfile, resource_path, 0, 0, 0, mmfile_openmode_RDONLY ) ;
      if (err) goto ABBRUCH ;
      logfile_size = size_mmfile(&logfile) ;
   }

   char * logbuffer ;
   size_t logbuffer_size ;
   LOG_GETBUFFER( &logbuffer, &logbuffer_size ) ;

   if (logbuffer_size != logfile_size) {
      LOGC_PRINTF(TEST, "logbuffer_size = %d, logfile_size = %d\n", (int)logbuffer_size, (int)logfile_size) ;
      err = EINVAL ;
      goto ABBRUCH ;
   }

   char * stored_content = (char*) addr_mmfile(&logfile) ;
   if (logbuffer_size && memcmp(stored_content, logbuffer, logbuffer_size )) {
      mmfile_t logfile2 = mmfile_INIT_FREEABLE ;
      if (0 == initcreate_mmfile(&logfile2, "/tmp/logbuffer", logbuffer_size, 0)) {
         memcpy( addr_mmfile(&logfile2), logbuffer, logbuffer_size ) ;
      }
      free_mmfile(&logfile2) ;
      LOGC_PRINTF(TEST, "Content of logbuffer differs:\nWritten to '/tmp/logbuffer'\n") ;
      err = EINVAL ;
      goto ABBRUCH ;
   }

   err = free_mmfile(&logfile) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOGC_PRINTF(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
   LOGC_PRINTF(TEST, "ERROR(%d:%s): '" GENERATED_LOGRESOURCE_DIR "%s'\n", err, strerror(err), test_name ) ;
   free_mmfile(&logfile) ;
   return err ;
}

/* function: print_result
 * Stores result of current test and prints progress.
 * If err is 0 (No error) then a '.' is printed else an '!'.
 * The result of previous executed are also printed until
 * the number all executed tests reaches sizeof(RESULT_STRING)==20.
 * Then the number of the previously executed tests (parameter progress_count)
 * is reset to 0 and the whol process starts from the beginning. */
static void print_result(int err, RESULT_STRING * progress, unsigned * progress_count)
{
   assert(*progress_count < sizeof(*progress)) ;
   if (err) {
      (*progress)[*progress_count] = '!' ;
   } else {
      (*progress)[*progress_count] = '.' ;
   }
   ++ (*progress_count) ;
   LOGC_PRINTF(TEST, "UNITTEST: %.*s\n", (*progress_count), (*progress)) ;
   if (*progress_count == sizeof(*progress)) {
      (*progress_count) = 0 ;
   }
}

static void set_testconfig(void)
{
   LOG_TURNON() ;
   LOG_CONFIG_BUFFERED(true) ;
   // make printed system error messages language (English) neutral
   resetmsg_locale() ;
}

static void preallocate(void)
{
   // preallocate some memory
   // TODO: remove line if own memory subsystem intead of malloc
   resourceusage_t   usage[2000]  = { resourceusage_INIT_FREEABLE } ;
   for(int i = 0; i < 2000; ++i) {
      (void) init_resourceusage(&usage[i]) ;
   }
   for(int i = 0; i < 2000; ++i) {
      (void) free_resourceusage(&usage[i]) ;
   }
}

int run_unittest(void)
{
   int err ;
   unsigned      err_count = 0 ;
   unsigned    total_count = 0 ;
   unsigned progress_count = 0 ;
   RESULT_STRING           progress ;
   const umgebung_type_e test_umgebung_type[2] = {
       umgebung_type_TEST
      ,umgebung_type_DEFAULT
   } ;

   // before init
   ++ total_count ;
   if (unittest_umgebung()) {
      ++ err_count ;
      LOGC_PRINTF(TEST, "unittest_umgebung FAILED\n") ;
      goto ABBRUCH ;
   }

for(unsigned type_nr = 0; type_nr < nrelementsof(test_umgebung_type); ++type_nr) {

   // init
   if (initprocess_umgebung(test_umgebung_type[type_nr])) {
      LOGC_PRINTF(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      LOGC_PRINTF(TEST, "%s\n", "Abort reason: initprocess_umgebung failed" ) ;
      goto ABBRUCH ;
   }

   preallocate() ;

   set_testconfig() ;

   // current development
   // assert(0) ;

//{ umgebung unittest
   RUN(unittest_umgebung) ;
   RUN(unittest_umgebung_typedefault) ;
   RUN(unittest_umgebung_typetest) ;
//}

//{ cache unittest
   RUN(unittest_cache_objectcache) ;
   RUN(unittest_cache_valuecache) ;
//}

//{ data structure unittest
   RUN(unittest_ds_inmem_slist) ;
//}

//{ math unittest
   RUN(unittest_math_int_signum) ;
   RUN(unittest_math_int_power2) ;
//}

//{ string unittest
   RUN(unittest_string_converter) ;
//}

//{ test unittest
   RUN(unittest_test_resourceusage) ;
//}

//{ writer unittest
   RUN(unittest_writer_log) ;
//}

//{ compiler unittest
//}

//{ os unittest
   // filesystem
   RUN(unittest_os_directory) ;
   RUN(unittest_os_memorymappedfile) ;
   // sync unittest
   RUN(unittest_os_sync_mutex) ;
   RUN(unittest_os_sync_semaphore) ;
   RUN(unittest_os_sync_signal) ;
   RUN(unittest_os_sync_waitlist) ;
   // task unittest
   RUN(unittest_os_task_exothread) ;
   RUN(unittest_os_task_exoscheduler) ;
   // other
   RUN(unittest_os_file) ;
   RUN(unittest_os_locale) ;
   RUN(unittest_os_malloc) ;
   RUN(unittest_os_process) ;
   RUN(unittest_os_thread) ;
   RUN(unittest_os_virtualmemory) ;
   // graphik subsystem
#define X11 1
#if (KONFIG_GRAPHIK==X11)
   RUN(unittest_os_X11) ;
   RUN(unittest_os_X11_display) ;
   RUN(unittest_os_X11_glxwindow) ;
#endif
#undef X11
   // database subsystem
   RUN(unittest_os_index_redblacktree) ;
   RUN(unittest_os_index_splaytree) ;
//}


   LOG_CLEARBUFFER() ;

   if (freeprocess_umgebung()) {
      LOGC_PRINTF(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      LOGC_PRINTF(TEST, "%s\n", "Abort reason: freeprocess_umgebung failed" ) ;
      goto ABBRUCH ;
   }

}

ABBRUCH:

   if (!err_count) {
      LOGC_PRINTF(TEST, "\nALL UNITTEST(%d): OK\n", total_count) ;
   } else if (err_count == total_count) {
      LOGC_PRINTF(TEST, "\nALL UNITTEST(%d): FAILED\n", total_count) ;
   } else {
      LOGC_PRINTF(TEST, "\n%d UNITTEST: OK\n%d UNITTEST: FAILED\n", total_count-err_count, err_count) ;
   }

   return err_count > 0 ;
}
