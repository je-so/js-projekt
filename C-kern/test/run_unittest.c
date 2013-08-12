/* title: Unittest impl

   Implements <run_unittest>.

   Calls all unittest. The calling sequence and list of unittests
   must be managed manually.

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
#include "C-kern/api/context/errorcontext.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
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
   int   err = EINVAL ;
   int   fd  = sys_iochannel_INIT_FREEABLE ;
   char  resource_path[sizeof(GENERATED_LOGRESOURCE_DIR) + strlen(test_name)] ;

   strcpy(resource_path, GENERATED_LOGRESOURCE_DIR) ;
   strcpy(resource_path + sizeof(GENERATED_LOGRESOURCE_DIR)-1, test_name) ;

   fd = open(resource_path, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, S_IRUSR|S_IWUSR) ;
   if (fd < 0) {
      err = errno ;
      goto ONABORT ;
   }

   char   * logbuffer ;
   size_t logbuffer_size ;
   GETBUFFER_ERRLOG(&logbuffer, &logbuffer_size) ;

   if (logbuffer_size) {
      int logsize = write(fd, logbuffer, logbuffer_size) ;
      if (logbuffer_size != (unsigned)logsize) {
         logformat_test("logbuffer_size = %d, logsize = %d\n", logbuffer_size, logsize) ;
         goto ONABORT ;
      }
   }

   free_iochannel(&fd) ;
   return ;
ONABORT:
   if (err != EEXIST) {
      logformat_test("%s: %s:\n", __FILE__, __FUNCTION__) ;
      logformat_test("ERROR(%d:%s): '" GENERATED_LOGRESOURCE_DIR "%s'\n", err, str_errorcontext(error_maincontext(),err), test_name) ;
   }
   free_iochannel(&fd) ;
   return ;
}

/* function: check_logresource
 * Compares saved log with content of log buffer.
 * The log *test_name* is read from directory "C-kern/resource/unittest.log/"
 * and compared with the content of the log buffer.
 * The content of the log buffer is queried with <GETBUFFER_ERRLOG>. */
static int check_logresource(const char * test_name)
{
   int   err ;
   mmfile_t logfile = mmfile_INIT_FREEABLE ;
   off_t    logfile_size ;
   char     resource_path[sizeof(GENERATED_LOGRESOURCE_DIR) + strlen(test_name)] ;

   strcpy(resource_path, GENERATED_LOGRESOURCE_DIR) ;
   strcpy(resource_path + sizeof(GENERATED_LOGRESOURCE_DIR)-1, test_name) ;

   err = filesize_directory(0, resource_path, &logfile_size) ;
   if (err) goto ONABORT ;

   if (logfile_size != 0) {
      err = init_mmfile(&logfile, resource_path, 0, 0, accessmode_READ, 0) ;
      if (err) goto ONABORT ;
      logfile_size = size_mmfile(&logfile) ;
   }

   char * logbuffer ;
   size_t logbuffer_size ;
   GETBUFFER_ERRLOG(&logbuffer, &logbuffer_size) ;

   char * logfile_buffer = (char*) addr_mmfile(&logfile) ;

   for (size_t i1 = 0, i2 = 0; i1 < logbuffer_size || i2 < logfile_size; ++i1, ++i2) {
      if (  i1 >= logbuffer_size
            || i2 >= logfile_size
            || logbuffer[i1] != logfile_buffer[i2]) {
         int logfile2 = open("/tmp/logbuffer", O_RDWR|O_TRUNC|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR) ;
         if (logfile2 >= 0) {
            ssize_t byteswritten = write(logfile2, logbuffer, logbuffer_size) ;
            (void) byteswritten ;
            free_iochannel(&logfile2) ;
         }
         logformat_test("Content of logbuffer differs:\nWritten to '/tmp/logbuffer'\n") ;
         err = EINVAL ;
         goto ONABORT ;
      }
      if (logbuffer[i1] == '[') {
         size_t i3 = i1 ;
         while (i3 < logbuffer_size && logbuffer[i3] != ' ') {
            ++i3 ;
         }
         size_t len = i3-i1+1 ;
         if (  i3 < logbuffer_size
               && len <= (logfile_size-i2)
               && 0 == memcmp(logbuffer+i1, logfile_buffer+i2, len)) {
            i3 = i1 ;
            while (i3 < logbuffer_size && logbuffer[i3] != '\n') {
               ++i3 ;
            }
            if (i3 < logbuffer_size) i1 = i3 ;
            i3 = i2 ;
            while (i3 < logfile_size && logfile_buffer[i3] != '\n') {
               ++i3 ;
            }
            if (i3 < logfile_size) i2 = i3 ;
         }
      }
   }

   err = free_mmfile(&logfile) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   logformat_test("%s: %s:\n", __FILE__, __FUNCTION__) ;
   logformat_test("ERROR(%d:%s): '" GENERATED_LOGRESOURCE_DIR "%s'\n", err, str_errorcontext(error_maincontext(),err), test_name) ;
   free_mmfile(&logfile) ;
   return err ;
}

static void prepare_test(void)
{
   // check for fpu errors
   enable_fpuexcept(fpu_except_MASK_ERR) ;

   // preallocate some memory
   // TODO: remove line if own memory subsystem instead of malloc
   resourceusage_t   usage[200]  = { resourceusage_INIT_FREEABLE } ;
   for (unsigned i = 0; i < lengthof(usage); ++i) {
      (void) init_resourceusage(&usage[i]) ;
   }
   for (unsigned i = 0; i < lengthof(usage); ++i) {
      (void) free_resourceusage(&usage[i]) ;
   }
}

static void run_singletest(const char * test_name, int (*unittest) (void), unsigned * total_count, unsigned * err_count)
{
   int err ;

   logrun_test(test_name) ;
   CLEARBUFFER_ERRLOG() ;

   err = switchon_testmm() ;
   if (err) {
      logformat_test("\n%s:%d: %s: ", __FILE__, __LINE__, __FUNCTION__) ;
      logformat_test("switchon_testmm FAILED\n") ;
   } else {
      err = unittest() ;
      if (err) {
         file_t error_log ;
         if (0 == initappend_file(&error_log, "error.log", 0)) {
            char     * buffer ;
            size_t   size ;
            GETBUFFER_ERRLOG(&buffer, &size) ;
            write_file(error_log, size, buffer, 0) ;
            free_file(&error_log) ;
         }
      } else {
         generate_logresource(test_name) ;
         err = check_logresource(test_name) ;
      }
   }

   if (switchoff_testmm()) {
      logformat_test("\n%s:%d: %s: ", __FILE__, __LINE__, __FUNCTION__) ;
      logformat_test("switchoff_testmm FAILED\n") ;
   }

   resetthreadid_threadcontext() ;

   if (err)
      ++ (*err_count) ;
   else
      logworking_test() ;

   ++ (*total_count) ;
}


#define RUN(FCT)              \
   extern int FCT (void) ;    \
   run_singletest(#FCT, &FCT, &total_count, &err_count)

int run_unittest(int argc, const char ** argv)
{
   unsigned    err_count   = 0 ;
   unsigned    total_count = 0 ;
   const maincontext_e test_context_type[2] = {
      maincontext_DEFAULT,
      maincontext_DEFAULT
   } ;

   // before init
   ++ total_count ;
   if (unittest_context_maincontext()) {
      ++ err_count ;
      logformat_test("unittest_context FAILED\n") ;
      goto ONABORT ;
   }

   for (unsigned type_nr = 0; type_nr < lengthof(test_context_type); ++type_nr) {

      // init
      if (init_maincontext(test_context_type[type_nr], argc, argv)) {
         logformat_test("%s: %s:\n", __FILE__, __FUNCTION__) ;
         logformat_test("%s\n", "Abort reason: init_maincontext failed") ;
         goto ONABORT ;
      }

      if (0 == checkpath_directory(0, "error.log")) {
         (void) removefile_directory(0, "error.log") ;
      }

      prepare_test() ;

//{ context unittest
      RUN(unittest_context_errorcontext) ;
      RUN(unittest_context_iobj) ;
      RUN(unittest_context_maincontext) ;
      RUN(unittest_context_module) ;
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
      RUN(unittest_ds_inmem_blockarray) ;
      RUN(unittest_ds_inmem_patriciatrie) ;
      RUN(unittest_ds_inmem_queue) ;
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
      RUN(unittest_ds_typeadapt_nodeoffset) ;
//}

//{ lang(uage) unittest
      RUN(unittest_lang_utf8scanner) ;
      RUN(unittest_lang_transc_transCtoken) ;
      RUN(unittest_lang_transc_transCstringtable) ;
      RUN(unittest_lang_transc_transCparser) ;
//}

//{ math unittest
      RUN(unittest_math_fpu) ;
      RUN(unittest_math_float_decimal) ;
      RUN(unittest_math_hash_crc32) ;
      RUN(unittest_math_hash_sha1) ;
      RUN(unittest_math_int_abs) ;
      RUN(unittest_math_int_atomic) ;
      RUN(unittest_math_int_biginteger) ;
      RUN(unittest_math_int_bitorder) ;
      RUN(unittest_math_int_byteorder) ;
      RUN(unittest_math_int_log10) ;
      RUN(unittest_math_int_log2) ;
      RUN(unittest_math_int_power2) ;
      RUN(unittest_math_int_sign) ;
      RUN(unittest_math_int_sqroot) ;
//}

//{ memory unittest
      RUN(unittest_memory_hwcache) ;
      RUN(unittest_memory_memblock) ;
      RUN(unittest_memory_memstream) ;
      RUN(unittest_memory_pagecache) ;
      RUN(unittest_memory_pagecacheimpl) ;
      RUN(unittest_memory_pagecache_macros) ;
      RUN(unittest_memory_wbuffer) ;
      RUN(unittest_memory_mm_mm) ;
      RUN(unittest_memory_mm_mmimpl) ;
//}

//{ string unittest
      RUN(unittest_string) ;
      RUN(unittest_string_convertwchar) ;
      RUN(unittest_string_cstring) ;
      RUN(unittest_string_base64encode) ;
      RUN(unittest_string_splitstring) ;
      RUN(unittest_string_stringstream) ;
      RUN(unittest_string_textpos) ;
      RUN(unittest_string_urlencode) ;
      RUN(unittest_string_utf8) ;
//}

//{ task unittest
      RUN(unittest_task_syncthread) ;
      RUN(unittest_task_syncrun) ;
      RUN(unittest_task_syncqueue) ;
      RUN(unittest_task_syncwait) ;
      RUN(unittest_task_syncwlist) ;
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
      // filesystem
      RUN(unittest_io_directory) ;
      RUN(unittest_io_file) ;
      RUN(unittest_io_filepath) ;
      RUN(unittest_io_fileutil) ;
      RUN(unittest_io_mmfile) ;
      // IP
      RUN(unittest_io_ipaddr) ;
      RUN(unittest_io_ipsocket) ;
      // generic
      RUN(unittest_io_iochannel) ;
      RUN(unittest_io_iocallback) ;
      RUN(unittest_io_url) ;
      RUN(unittest_io_iopoll) ;
      // reader
      RUN(unittest_io_reader_csvfilereader) ;
      RUN(unittest_io_reader_filereader) ;
      RUN(unittest_io_reader_utf8reader) ;
      // writer
      RUN(unittest_io_writer_log_logbuffer) ;
      RUN(unittest_io_writer_log_logwriter) ;
      RUN(unittest_io_writer_log_logmain) ;
//}

//{ platform unittest
      // sync unittest
      RUN(unittest_platform_sync_mutex) ;
      RUN(unittest_platform_sync_rwlock) ;
      RUN(unittest_platform_sync_semaphore) ;
      RUN(unittest_platform_sync_signal) ;
      RUN(unittest_platform_sync_thrmutex) ;
      RUN(unittest_platform_sync_waitlist) ;
      // task unittest
      RUN(unittest_platform_task_process) ;
      RUN(unittest_platform_task_thread) ;
      RUN(unittest_platform_task_thread_tls) ;
      // other
      RUN(unittest_platform_locale) ;
      RUN(unittest_platform_malloc) ;
      RUN(unittest_platform_startup) ;
      RUN(unittest_platform_sysuser) ;
      RUN(unittest_platform_vm) ;
      // user interface subsystem
#define KONFIG_html5  1
#define KONFIG_x11    2
#if ((KONFIG_USERINTERFACE)&KONFIG_html5)
#endif
#if ((KONFIG_USERINTERFACE)&KONFIG_x11)
      RUN(unittest_platform_X11) ;
      RUN(unittest_platform_X11_x11attribute) ;
      RUN(unittest_platform_X11_x11display) ;
      RUN(unittest_platform_X11_x11screen) ;
      RUN(unittest_platform_X11_x11drawable) ;
      // RUN(unittest_platform_X11_x11window) ;      // TODO: remove comment
      // RUN(unittest_platform_X11_glxwindow) ;      // TODO: remove comment
      // RUN(unittest_platform_X11_x11videomode) ;   // TODO: remove comment
#endif
#undef KONFIG_html5
#undef KONFIG_x11
//}

      CLEARBUFFER_ERRLOG() ;

      if (free_maincontext()) {
         logformat_test("%s: %s:\n", __FILE__, __FUNCTION__) ;
         logformat_test("%s\n", "Abort reason: free_maincontext failed") ;
         goto ONABORT ;
      }

   }

ONABORT:

   if (!err_count) {
      logformat_test("\nALL UNITTEST(%d): OK\n", total_count) ;
   } else if (err_count == total_count) {
      logformat_test("\nALL UNITTEST(%d): FAILED\n", total_count) ;
   } else {
      logformat_test("\n%d UNITTEST: OK\n%d UNITTEST: FAILED\n", total_count-err_count, err_count) ;
   }

   return err_count > 0 ;
}
