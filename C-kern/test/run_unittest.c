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
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/os/locale.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/test/resourceusage.h"



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

   err = filesize_directory(0, resource_path, &logfile_size ) ;
   if (err) goto ABBRUCH ;

   if (logfile_size != 0) {
      err = init_mmfile( &logfile, resource_path, 0, 0, mmfile_openmode_RDONLY, 0) ;
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

static void set_testconfig(void)
{
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

static void run_singletest(const char * test_name, int (*test_fct) (void), unsigned * total_count, unsigned * err_count)
{
   int err ;

   printf("RUN %s: ", test_name) ;
   LOG_CLEARBUFFER() ;

   err = test_fct() ;
   if (!err) generate_logresource(test_name) ;
   if (!err) err = check_logresource(test_name) ;

   if (err)
      ++ (*err_count) ;
   else
      printf("OK\n") ;

   ++ (*total_count) ;
}


#define RUN(FCT)              \
   extern int FCT (void) ;    \
   run_singletest(#FCT, &FCT, &total_count, &err_count)

int run_unittest(void)
{
   unsigned    err_count   = 0 ;
   unsigned    total_count = 0 ;
   const umgebung_type_e test_umgebung_type[2] = {
       umgebung_type_SINGLETHREAD
      ,umgebung_type_MULTITHREAD
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
   if (initmain_umgebung(test_umgebung_type[type_nr])) {
      LOGC_PRINTF(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      LOGC_PRINTF(TEST, "%s\n", "Abort reason: initmain_umgebung failed" ) ;
      goto ABBRUCH ;
   }

   preallocate() ;

   set_testconfig() ;

   // current development
   // assert(0) ;

//{ umgebung unittest
   RUN(unittest_umgebung) ;
   RUN(unittest_umgebung_shared) ;
   RUN(unittest_umgebung_services_multithread) ;
   RUN(unittest_umgebung_services_singlethread) ;
//}

//{ cache unittest
   RUN(unittest_cache_objectcache) ;
   RUN(unittest_cache_objectcachemt) ;
   RUN(unittest_cache_valuecache) ;
//}

//{ data structure unittest
   RUN(unittest_ds_inmem_slist) ;
//}

//{ math unittest
   RUN(unittest_math_hash_sha1) ;
   RUN(unittest_math_int_signum) ;
   RUN(unittest_math_int_power2) ;
//}

//{ memory unittest
   RUN(unittest_memory_memblock) ;
   RUN(unittest_memory_wbuffer) ;
//}

//{ string unittest
   RUN(unittest_string) ;
   RUN(unittest_string_convertwchar) ;
   RUN(unittest_string_cstring) ;
   RUN(unittest_string_encode) ;
   RUN(unittest_string_utf8) ;
//}

//{ test unittest
   RUN(unittest_test_resourceusage) ;
//}

//{ writer unittest
   RUN(unittest_writer_logwriter) ;
   RUN(unittest_writer_logwritermt) ;
   RUN(unittest_writer_mainlogwriter) ;
//}

//{ compiler unittest
//}

//{ os unittest
   // io
   RUN(unittest_io_filedescr) ;
   RUN(unittest_io_url) ;
   // io/filesystem
   RUN(unittest_io_directory) ;
   RUN(unittest_io_file) ;
   RUN(unittest_io_memorymappedfile) ;
   // io/ip
   RUN(unittest_io_ipaddr) ;
   RUN(unittest_io_ipsocket) ;
   // sync unittest
   RUN(unittest_os_sync_mutex) ;
   RUN(unittest_os_sync_semaphore) ;
   RUN(unittest_os_sync_signal) ;
   RUN(unittest_os_sync_waitlist) ;
   // task unittest
   RUN(unittest_os_task_exothread) ;
   RUN(unittest_os_task_exoscheduler) ;
   RUN(unittest_os_task_threadpool) ;
   // other
   RUN(unittest_os_locale) ;
   RUN(unittest_os_malloc) ;
   RUN(unittest_os_process) ;
   RUN(unittest_os_thread) ;
   RUN(unittest_os_virtualmemory) ;
   // database subsystem
   RUN(unittest_os_index_redblacktree) ;
   RUN(unittest_os_index_splaytree) ;
//}

//{ user interface subsystem
#define HTML5  1
#define X11    2
#if ((KONFIG_USERINTERFACE)&HTML5)
#endif
#if ((KONFIG_USERINTERFACE)&X11)
   RUN(unittest_userinterface_X11) ;
   RUN(unittest_userinterface_X11_display) ;
   RUN(unittest_userinterface_X11_glxwindow) ;
#endif
#undef HTML5
#undef X11
//}


   LOG_CLEARBUFFER() ;

   if (freemain_umgebung()) {
      LOGC_PRINTF(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      LOGC_PRINTF(TEST, "%s\n", "Abort reason: freemain_umgebung failed" ) ;
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
