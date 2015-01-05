/* title: Run-Performance-Test impl

   Implements <Run-Performance-Test>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/test/run/run_perftest.h
    Header file <Run-Performance-Test>.

   file: C-kern/test/run/run_perftest.c
    Implementation file <Run-Performance-Test impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/run/run_perftest.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/perftest.h"
#include "C-kern/api/io/iochannel.h"


// section: perftest_t

// group: helper

// TODO: refactor unittest log into own testlog module
//       move helper functions also into testlog

static void logrun_testlog(const char * testname)
{
   struct iovec   iov[3] = {
       { (void*) (uintptr_t) "RUN ", 4 }
      ,{ (void*) (uintptr_t) testname, strlen(testname) }
      ,{ (void*) (uintptr_t) ": ", 2 }
   };

   ssize_t written = writev(iochannel_STDOUT, iov, lengthof(iov));
   (void) written;
}

static void logf_testlog(const char * format, ...)
{
   char     buffer[256];
   unsigned bytes;
   {
      va_list args;
      va_start(args, format);
      bytes = (unsigned) vsnprintf(buffer, sizeof(buffer), format, args);
      va_end(args);
      if (bytes >= sizeof(buffer)) {
         bytes = sizeof(buffer)-1;
      }
   }
   ssize_t written = write(iochannel_STDOUT, buffer, bytes);
   (void) written;
}

static void run_singletest(const char * testname, perftest_f test_f)
{
   int err;
   perftest_info_t info;
   uint64_t nrops;
   uint64_t usec;

   logrun_testlog(testname);

   err = test_f(&info);
   if (err) {
      logf_testlog("FAILED\n");

   } else {
      logf_testlog("ops == \"%s\"\n", info.ops_description);

      for (unsigned t = 1; t <= 8; t *= 2, t*=1u+(t==2)) {
         logf_testlog("%d-thread: ", t);
         err = exec_perftest(&info.iimpl, info.shared_addr, info.shared_size, 1, (uint16_t)t, &nrops, &usec);
         if (err) {
            logf_testlog("FAILED\n");
         } else {
            if (usec == 0) usec = 1;
            logf_testlog("%6" PRIu64 " ops/msec (operations per millisecond)\n", 1000*nrops/usec);
         }
      }
   }

}

#define RUN(FCT) \
         extern int FCT (struct perftest_info_t*); \
         run_singletest(#FCT, &FCT)

// group: execute

int run_perftest(maincontext_t* maincontext)
{
   (void) maincontext;

   RUN(perftest_task_syncrunner);
   RUN(perftest_task_syncrunner_raw);

   return 0;
}

