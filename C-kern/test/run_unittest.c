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
#include "C-kern/api/err.h"
#include "C-kern/api/test.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/math/fpu.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/testmm.h"
#include "C-kern/api/test/run/unittest.h"



#define GENERATED_LOGRESOURCE_DIR   "C-kern/resource/unittest.log/"

/* function: generate_logresource
 * Writes log buffer to "C-kern/resource/unittest.log/" + test_name. */
static void generate_logresource(const char * test_name)
{
   int     err = EINVAL ;
   int     fd  = sys_filedescr_INIT_FREEABLE ;
   char   resource_path[sizeof(GENERATED_LOGRESOURCE_DIR) + strlen(test_name)] ;

   strcpy( resource_path, GENERATED_LOGRESOURCE_DIR ) ;
   strcpy( resource_path + sizeof(GENERATED_LOGRESOURCE_DIR)-1, test_name ) ;

   fd = open( resource_path, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   if (fd < 0) {
      err = errno ;
      goto ONABORT ;
   }

   char   * logbuffer ;
   size_t logbuffer_size ;
   GETBUFFER_LOG( &logbuffer, &logbuffer_size ) ;

   if (logbuffer_size) {
      int logsize = write( fd, logbuffer, logbuffer_size ) ;
      if (logbuffer_size != (unsigned)logsize) {
         CPRINTF_LOG(TEST, "logbuffer_size = %d, logsize = %d\n", logbuffer_size, logsize) ;
         goto ONABORT ;
      }
   }

   free_filedescr(&fd) ;
   return ;
ONABORT:
   if (err != EEXIST) {
      CPRINTF_LOG(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      CPRINTF_LOG(TEST, "ERROR(%d:%s): '" GENERATED_LOGRESOURCE_DIR "%s'\n", err, strerror(err), test_name ) ;
   }
   free_filedescr(&fd) ;
   return ;
}

/* function: check_logresource
 * Compares saved log with content of log buffer.
 * The log *test_name* is read from directory "C-kern/resource/unittest.log/"
 * and compared with the content of the log buffer.
 * The content of the log buffer is queried with <GETBUFFER_LOG>. */
static int check_logresource(const char * test_name)
{
   int              err ;
   mmfile_t     logfile = mmfile_INIT_FREEABLE ;
   off_t   logfile_size ;
   char   resource_path[sizeof(GENERATED_LOGRESOURCE_DIR) + strlen(test_name)] ;

   strcpy( resource_path, GENERATED_LOGRESOURCE_DIR ) ;
   strcpy( resource_path + sizeof(GENERATED_LOGRESOURCE_DIR)-1, test_name ) ;

   err = filesize_directory(0, resource_path, &logfile_size ) ;
   if (err) goto ONABORT ;

   if (logfile_size != 0) {
      err = init_mmfile( &logfile, resource_path, 0, 0, mmfile_openmode_RDONLY, 0) ;
      if (err) goto ONABORT ;
      logfile_size = size_mmfile(&logfile) ;
   }

   char * logbuffer ;
   size_t logbuffer_size ;
   GETBUFFER_LOG( &logbuffer, &logbuffer_size ) ;

   if (logbuffer_size != logfile_size) {
      CPRINTF_LOG(TEST, "logbuffer_size = %d, logfile_size = %d\n", (int)logbuffer_size, (int)logfile_size) ;
      err = EINVAL ;
      goto ONABORT ;
   }

   char * stored_content = (char*) addr_mmfile(&logfile) ;
   if (logbuffer_size && memcmp(stored_content, logbuffer, logbuffer_size )) {
      mmfile_t logfile2 = mmfile_INIT_FREEABLE ;
      if (0 == initcreate_mmfile(&logfile2, "/tmp/logbuffer", logbuffer_size, 0)) {
         memcpy( addr_mmfile(&logfile2), logbuffer, logbuffer_size ) ;
      }
      free_mmfile(&logfile2) ;
      CPRINTF_LOG(TEST, "Content of logbuffer differs:\nWritten to '/tmp/logbuffer'\n") ;
      err = EINVAL ;
      goto ONABORT ;
   }

   err = free_mmfile(&logfile) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   CPRINTF_LOG(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
   CPRINTF_LOG(TEST, "ERROR(%d:%s): '" GENERATED_LOGRESOURCE_DIR "%s'\n", err, strerror(err), test_name ) ;
   free_mmfile(&logfile) ;
   return err ;
}

static void prepare_test(void)
{
   // make printed system error messages language (English) neutral
   resetmsg_locale() ;

   // check for fpu errors
   enable_fpuexcept(fpu_except_MASK_ERR) ;

   // preallocate some memory
   // TODO: remove line if own memory subsystem instead of malloc
   resourceusage_t   usage[2000]  = { resourceusage_INIT_FREEABLE } ;
   for (unsigned i = 0; i < nrelementsof(usage); ++i) {
      (void) init_resourceusage(&usage[i]) ;
   }
   for (unsigned i = 0; i < nrelementsof(usage); ++i) {
      (void) free_resourceusage(&usage[i]) ;
   }

}

static void run_singletest(const char * test_name, int (*unittest) (void), unsigned * total_count, unsigned * err_count)
{
   int err ;

   logrun_test(test_name) ;
   CLEARBUFFER_LOG() ;

   err = switchon_testmm() ;
   if (err) {
      CPRINTF_LOG(TEST, "\n%s:%d: %s: ", __FILE__, __LINE__, __FUNCTION__ ) ;
      CPRINTF_LOG(TEST, "switchon_testmm FAILED\n") ;
   } else {
      err = unittest() ;
      if (err) {
         file_t error_log ;
         if (0 == initappend_file(&error_log, "error.log", 0)) {
            char     * buffer ;
            size_t   size ;
            GETBUFFER_LOG(&buffer, &size) ;
            write_filedescr(error_log, size, (uint8_t*)buffer, 0) ;
            free_file(&error_log) ;
         }
      } else {
         generate_logresource(test_name) ;
         err = check_logresource(test_name) ;
      }
   }

   if (switchoff_testmm()) {
      CPRINTF_LOG(TEST, "\n%s:%d: %s: ", __FILE__, __LINE__, __FUNCTION__ ) ;
      CPRINTF_LOG(TEST, "switchoff_testmm FAILED\n") ;
   }

   if (err)
      ++ (*err_count) ;
   else
      logworking_test() ;

   ++ (*total_count) ;
}


#define RUN(FCT)              \
   extern int FCT (void) ;    \
   run_singletest(#FCT, &FCT, &total_count, &err_count)

int run_unittest(void)
{
   unsigned    err_count   = 0 ;
   unsigned    total_count = 0 ;
   const maincontext_e test_context_type[2] = {
      maincontext_DEFAULT
      ,maincontext_DEFAULT
   } ;

   if (0 == checkpath_directory(0, "error.log")) {
      (void) removefile_directory(0, "error.log") ;
   }

   // before init
   ++ total_count ;
   if (unittest_context_maincontext()) {
      ++ err_count ;
      CPRINTF_LOG(TEST, "unittest_context FAILED\n") ;
      goto ONABORT ;
   }

   for (unsigned type_nr = 0; type_nr < nrelementsof(test_context_type); ++type_nr) {

      // init
      if (init_maincontext(test_context_type[type_nr], 0, 0)) {
         CPRINTF_LOG(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
         CPRINTF_LOG(TEST, "%s\n", "Abort reason: initmain_context failed" ) ;
         goto ONABORT ;
      }

      prepare_test() ;

//{ context unittest
      RUN(unittest_context_maincontext) ;
      RUN(unittest_context_processcontext) ;
      RUN(unittest_context_threadcontext) ;
//}

//{ cache unittest
      RUN(unittest_cache_objectcacheimpl) ;
      RUN(unittest_cache_valuecache) ;
//}

//{ data structure unittest
      RUN(unittest_ds_inmem_arraysf) ;
      RUN(unittest_ds_inmem_arraystf) ;
      RUN(unittest_ds_inmem_binarystack) ;
      RUN(unittest_ds_inmem_dlist) ;
      RUN(unittest_ds_inmem_exthash) ;
      RUN(unittest_ds_inmem_patriciatrie) ;
      RUN(unittest_ds_inmem_redblacktree) ;
      RUN(unittest_ds_inmem_slist) ;
      RUN(unittest_ds_inmem_splaytree) ;
      RUN(unittest_ds_inmem_suffixtree) ;
      RUN(unittest_ds_typeadapt) ;
      RUN(unittest_ds_typeadapt_comparator) ;
      RUN(unittest_ds_typeadapt_getkey) ;
      RUN(unittest_ds_typeadapt_gethash) ;
      RUN(unittest_ds_typeadapt_lifetime) ;
      RUN(unittest_ds_typeadapt_typeadaptimpl) ;
      RUN(unittest_ds_typeadapt_typeinfo) ;
//}

//{ math unittest
      RUN(unittest_math_fpu) ;
      RUN(unittest_math_float_decimal) ;
      RUN(unittest_math_hash_sha1) ;
      RUN(unittest_math_int_abs) ;
      RUN(unittest_math_int_biginteger) ;
      RUN(unittest_math_int_log10) ;
      RUN(unittest_math_int_log2) ;
      RUN(unittest_math_int_power2) ;
      RUN(unittest_math_int_sign) ;
      RUN(unittest_math_int_sqroot) ;
//}

//{ memory unittest
      RUN(unittest_memory_hwcache) ;
      RUN(unittest_memory_memblock) ;
      RUN(unittest_memory_wbuffer) ;
      RUN(unittest_memory_manager_transient) ;
//}

//{ string unittest
      RUN(unittest_string) ;
      RUN(unittest_string_convertwchar) ;
      RUN(unittest_string_cstring) ;
      RUN(unittest_string_base64encode) ;
      RUN(unittest_string_urlencode) ;
      RUN(unittest_string_utf8) ;
//}

//{ test unittest
      RUN(unittest_test_errortimer) ;
      RUN(unittest_test_resourceusage) ;
      RUN(unittest_test_test) ;
      RUN(unittest_test_testmm) ;
//}

//{ time unittest
      RUN(unittest_time_sysclock) ;
      RUN(unittest_time_systimer) ;
//}

//{ io unittest
      RUN(unittest_io_iocallback) ;
      RUN(unittest_io_url) ;
      // reader
      RUN(unittest_io_reader_csvfilereader) ;
      RUN(unittest_io_reader_utf8reader) ;
      // writer
      RUN(unittest_io_writer_log_logwriter) ;
      RUN(unittest_io_writer_log_logmain) ;
//}

//{ platform unittest
      // io
      RUN(unittest_io_filedescr) ;
      RUN(unittest_io_iocontroler) ;
      // io/filesystem
      RUN(unittest_io_directory) ;
      RUN(unittest_io_file) ;
      RUN(unittest_io_mmfile) ;
      // io/ip
      RUN(unittest_io_ipaddr) ;
      RUN(unittest_io_ipsocket) ;
      // sync unittest
      RUN(unittest_platform_sync_mutex) ;
      RUN(unittest_platform_sync_semaphore) ;
      RUN(unittest_platform_sync_signal) ;
      RUN(unittest_platform_sync_waitlist) ;
      // task unittest
      RUN(unittest_platform_task_exothread) ;
      RUN(unittest_platform_task_exoscheduler) ;
      RUN(unittest_platform_task_threadpool) ;
      // other
      RUN(unittest_platform_locale) ;
      RUN(unittest_platform_malloc) ;
      RUN(unittest_platform_process) ;
      RUN(unittest_platform_sysuser) ;
      RUN(unittest_platform_thread) ;
      RUN(unittest_platform_virtualmemory) ;
      // user interface subsystem
#define HTML5  1
#define X11    2
#if ((KONFIG_USERINTERFACE)&HTML5)
#endif
#if ((KONFIG_USERINTERFACE)&X11)
      RUN(unittest_presentation_X11) ;
      RUN(unittest_presentation_X11_display) ;
      RUN(unittest_presentation_X11_glxwindow) ;
#endif
#undef HTML5
#undef X11
//}


      CLEARBUFFER_LOG() ;

      if (free_maincontext()) {
         CPRINTF_LOG(TEST, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
         CPRINTF_LOG(TEST, "%s\n", "Abort reason: freemain_context failed" ) ;
         goto ONABORT ;
      }

   }

ONABORT:

   if (!err_count) {
      CPRINTF_LOG(TEST, "\nALL UNITTEST(%d): OK\n", total_count) ;
   } else if (err_count == total_count) {
      CPRINTF_LOG(TEST, "\nALL UNITTEST(%d): FAILED\n", total_count) ;
   } else {
      CPRINTF_LOG(TEST, "\n%d UNITTEST: OK\n%d UNITTEST: FAILED\n", total_count-err_count, err_count) ;
   }

   return err_count > 0 ;
}
