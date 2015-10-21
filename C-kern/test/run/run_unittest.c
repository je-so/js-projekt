/* title: Run-Unit-Test impl

   Implements <unittest_t.run_unittest>.

   Calls all unit test. The calling sequence and list of unit tests
   must be managed manually - but the static test
   C-kern/test/static/call_all_unittest.sh checks that all unittest_UNIT
   are called.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/test/run/run_unittest.h
    Header file of <Run-Unit-Test>.

   file: C-kern/test/run/run_unittest.c
    Implementation file <Run-Unit-Test impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/run/run_unittest.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/math/fpu.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/mm/testmm.h"


// section: unittest_t

// group: helper

#define GENERATED_LOGRESOURCE_DIR "C-kern/resource/unittest.log"

static void prepare_test(void)
{
   // check for fpu errors
   enable_fpuexcept(fpu_except_MASK_ERR);

   if (type_maincontext() != maincontext_DEFAULT) {
      // this makes created threads compatible with expected behaviour
      log_t * ilog = cast_log(&log_maincontext());
      ilog->iimpl->setstate(ilog->object, log_channel_USERERR, log_state_IGNORED);
      ilog->iimpl->setstate(ilog->object, log_channel_ERR,     log_state_BUFFERED);
      g_maincontext.type = maincontext_DEFAULT;
   }

   // preallocate some memory
   // TODO: remove line if own memory subsystem instead of malloc
   resourceusage_t   usage[200]  = { resourceusage_FREE };
   for (unsigned i = 0; i < lengthof(usage); ++i) {
      (void) init_resourceusage(&usage[i]);
   }
   for (unsigned i = 0; i < lengthof(usage); ++i) {
      (void) free_resourceusage(&usage[i]);
   }
}

// group: execute

static void run_singletest(const char * testname, int (*test_f) (void))
{
   int err;

   err = switchon_testmm();
   if (err) {
      logf_unittest("\n%s:%d: ", __FILE__, __LINE__);
      logf_unittest("switchon_testmm FAILED\n");

   } else {

      err = execsingle_unittest(testname, test_f);

      // in case of error write errlog into error.log
      // and overwrite any existing error.log
      if (err) {
         if (0 == trypath_directory(0, "error.log")) {
            (void) removefile_directory(0, "error.log");
         }
         uint8_t *logbuffer;
         size_t   logsize;
         GETBUFFER_ERRLOG(&logbuffer, &logsize);
         (void) save_file("error.log", logsize, logbuffer, 0);
      }
   }

   if (switchoff_testmm()) {
      logf_unittest("\n%s:%d: ", __FILE__, __LINE__);
      logf_unittest("switchoff_testmm FAILED\n");
   }

   resetthreadid_threadcontext();
}

#define RUN(FCT)                 \
         extern int FCT (void);  \
         run_singletest(#FCT, &FCT)

static void run_all_test(unsigned run_idx/*0..*/, bool isLastRun)
{
      if (run_idx == 0) {
         initsingleton_unittest(GENERATED_LOGRESOURCE_DIR);
      }

      prepare_test();

//{ err
      RUN(unittest_err_errorcontext);
      RUN(unittest_err_errornr);
//}

//{ std types
      RUN(unittest_stdtypes_iobj);
//}

//{ task context
      RUN(unittest_task_module);
      RUN(unittest_task_processcontext);
      RUN(unittest_task_threadcontext);
//}

//{ main context
      RUN(unittest_main_maincontext);
//}

//{ cache unittest
      RUN(unittest_cache_objectcacheimpl);
      RUN(unittest_cache_valuecache);
//}

//{ data structure unittest
      RUN(unittest_ds_link);
      // in memory data structures
      RUN(unittest_ds_inmem_arraysf);
      RUN(unittest_ds_inmem_arraystf);
      RUN(unittest_ds_inmem_binarystack);
      RUN(unittest_ds_inmem_blockarray);
      RUN(unittest_ds_inmem_dlist);
      RUN(unittest_ds_inmem_dlist2);
      RUN(unittest_ds_inmem_exthash);
      RUN(unittest_ds_inmem_heap);
      RUN(unittest_ds_inmem_patriciatrie);
      RUN(unittest_ds_inmem_queue);
      RUN(unittest_ds_inmem_redblacktree);
      RUN(unittest_ds_inmem_slist);
      RUN(unittest_ds_inmem_splaytree);
      RUN(unittest_ds_inmem_suffixtree);
      RUN(unittest_ds_inmem_trie);
      // sort algorithms
      RUN(unittest_ds_sort_mergesort);
      // type adapter
      RUN(unittest_ds_typeadapt);
      RUN(unittest_ds_typeadapt_comparator);
      RUN(unittest_ds_typeadapt_getkey);
      RUN(unittest_ds_typeadapt_gethash);
      RUN(unittest_ds_typeadapt_lifetime);
      RUN(unittest_ds_typeadapt_typeadaptimpl);
      RUN(unittest_ds_typeadapt_nodeoffset);
//}

//{ math unittest
      RUN(unittest_math_fpu);
      RUN(unittest_math_float_decimal);
      RUN(unittest_math_hash_crc32);
      RUN(unittest_math_hash_sha1);
      RUN(unittest_math_int_abs);
      RUN(unittest_math_int_biginteger);
      RUN(unittest_math_int_bitorder);
      RUN(unittest_math_int_byteorder);
      RUN(unittest_math_int_factorise);
      RUN(unittest_math_int_log10);
      RUN(unittest_math_int_log2);
      RUN(unittest_math_int_power2);
      RUN(unittest_math_int_sign);
      RUN(unittest_math_int_sqroot);
//}

//{ memory unittest
      RUN(unittest_memory_atomic);
      RUN(unittest_memory_hwcache);
      RUN(unittest_memory_memblock);
      RUN(unittest_memory_memstream);
      RUN(unittest_memory_memvec);
      RUN(unittest_memory_pagecache);
      RUN(unittest_memory_pagecacheimpl);
      RUN(unittest_memory_pagecache_macros);
      RUN(unittest_memory_ptr);
      RUN(unittest_memory_wbuffer);
      RUN(unittest_memory_mm_mm);
      RUN(unittest_memory_mm_mmimpl);
//}

//{ string unittest
      RUN(unittest_string);
      RUN(unittest_string_convertwchar);
      RUN(unittest_string_cstring);
      RUN(unittest_string_base64encode);
      RUN(unittest_string_strsearch);
      RUN(unittest_string_textpos);
      RUN(unittest_string_urlencode);
      RUN(unittest_string_utf8);
//}

//{ task unittest
      RUN(unittest_task_synccond);
      RUN(unittest_task_syncfunc);
      RUN(unittest_task_syncrunner);
      RUN(unittest_task_itc_itccounter);
//}

//{ test unittest
      RUN(unittest_test_errortimer);
      RUN(unittest_test_perftest);
      RUN(unittest_test_resourceusage);
      RUN(unittest_test_unittest);
      RUN(unittest_test_mm_mm_test);
      RUN(unittest_test_mm_testmm);
//}

//{ time unittest
      RUN(unittest_time_sysclock);
      RUN(unittest_time_systimer);
//}

//{ io unittest
      // filesystem
      RUN(unittest_io_subsys_iobuffer);
      RUN(unittest_io_subsys_iolist);
      RUN(unittest_io_subsys_iothread);
      RUN(unittest_io_directory);
      RUN(unittest_io_file);
      RUN(unittest_io_filepath);
      RUN(unittest_io_fileutil);
      RUN(unittest_io_mmfile);
      // IP
      RUN(unittest_io_ip_ipaddr);
      RUN(unittest_io_ip_ipsocket);
      // generic
      RUN(unittest_io_iochannel);
      RUN(unittest_io_iopoll);
      RUN(unittest_io_pipe);
      RUN(unittest_io_url);
      // reader
      RUN(unittest_io_reader_csvfilereader);
      RUN(unittest_io_reader_filereader);
      RUN(unittest_io_reader_utf8reader);
      // writer
      RUN(unittest_io_writer_log_logbuffer);
      RUN(unittest_io_writer_log_logwriter);
      RUN(unittest_io_writer_log_logmain);
      // Terminal
      RUN(unittest_io_terminal_pseudoterm);
      RUN(unittest_io_terminal_termadapt);
      RUN(unittest_io_terminal_terminal);
//}

//{ platform unittest
      // sync unittest
      RUN(unittest_platform_sync_mutex);
      RUN(unittest_platform_sync_rwlock);
      RUN(unittest_platform_sync_semaphore);
      RUN(unittest_platform_sync_signal);
      RUN(unittest_platform_sync_thrmutex);
      RUN(unittest_platform_sync_waitlist);
      // task unittest
      RUN(unittest_platform_task_process);
      RUN(unittest_platform_task_thread);
      RUN(unittest_platform_task_thread_localstore);
      // other
      RUN(unittest_platform_locale);
      RUN(unittest_platform_malloc);
      RUN(unittest_platform_init);
      RUN(unittest_platform_syslogin);
      RUN(unittest_platform_vm);
      // user interface subsystem
#if defined(KONFIG_USERINTERFACE_X11)
      RUN(unittest_platform_X11);
      RUN(unittest_platform_X11_x11display);
      RUN(unittest_platform_X11_x11screen);
      RUN(unittest_platform_X11_x11drawable);
      // RUN(unittest_platform_X11_x11window);      // TODO: remove comment
      // RUN(unittest_platform_X11_x11dblbuffer);   // TODO: remove comment
      // RUN(unittest_platform_X11_x11videomode);   // TODO: remove comment
#endif
#if defined(KONFIG_USERINTERFACE_EGL)
      RUN(unittest_platform_opengl_egl_egl);
      RUN(unittest_platform_opengl_egl_eglconfig);
      RUN(unittest_platform_opengl_egl_eglcontext);
      RUN(unittest_platform_opengl_egl_egldisplay);
      RUN(unittest_platform_opengl_egl_eglpbuffer);
      RUN(unittest_platform_opengl_egl_eglwindow);
#endif
//}

#if !defined(KONFIG_USERINTERFACE_NONE)
//{ graphic unittest
      RUN(unittest_graphic_display);
      RUN(unittest_graphic_gconfig);
      RUN(unittest_graphic_gcontext);
      RUN(unittest_graphic_pixelbuffer);
      RUN(unittest_graphic_surface);
      RUN(unittest_graphic_windowconfig);
      RUN(unittest_graphic_window);
//}
#endif

   CLEARBUFFER_ERRLOG();

   if (isLastRun) {

      logsummary_unittest();

      freesingleton_unittest();

   }

}

int run_unittest(void * argv)
{
   const maincontext_e test_context_type[2] = {
      maincontext_DEFAULT,
      maincontext_CONSOLE
   };

   for (unsigned type_nr = 0; type_nr < lengthof(test_context_type); ++type_nr) {

      if (init_maincontext(test_context_type[type_nr], 0, argv)) {
         logf_unittest("\n%s:%d: ", __FILE__, __LINE__);
         logf_unittest("init_maincontext FAILED\n");
         goto ONERR;
      }

      run_all_test(type_nr, type_nr == lengthof(test_context_type)-1);

      if (free_maincontext()) {
         logf_unittest("\n%s:%d: ", __FILE__, __LINE__);
         logf_unittest("free_maincontext FAILED\n");
         goto ONERR;
      }
   }

ONERR:

   return 0;
}
