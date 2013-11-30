/* title: Unit-Test impl

   Implements functions defined in <Unit-Test>.

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

   file: C-kern/api/test/unittest.h
    Header file of <Unit-Test>.

   file: C-kern/test/unittest.c
    Implementation file <Unit-Test impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/sync/mutex.h"

typedef struct unittest_t unittest_t;

// TODO: add test of resource usage before and after test is run (remove include resourceusage.h)

/* struct: unittest_t
 * Holds <mutex_t> to make it thread-safe. */
struct unittest_t {
   mutex_t     mutex;
   const unittest_adapter_t
              *adapter;
   const char *log_files_directory;
   size_t      okcount;
   size_t      errcount;
   bool        isResult;
};

// group: variables

/* variable: s_unittest_singleton
 * Holds the context for all running unit tests. */

static unittest_t    s_unittest_singleton;

// group: lifetime

int initsingleton_unittest(const unittest_adapter_t * adapter, const char * log_files_directory)
{
   int err;

   err = init_mutex(&s_unittest_singleton.mutex);
   if (err) goto ONABORT;
   s_unittest_singleton.adapter             = adapter;
   s_unittest_singleton.log_files_directory = log_files_directory;
   s_unittest_singleton.okcount             = 0;
   s_unittest_singleton.errcount            = 0;
   s_unittest_singleton.isResult            = false;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

int freesingleton_unittest(void)
{
   int err;

   err = free_mutex(&s_unittest_singleton.mutex);
   if (err) goto ONABORT;
   s_unittest_singleton.adapter             = 0;
   s_unittest_singleton.log_files_directory = 0;
   s_unittest_singleton.okcount             = 0;
   s_unittest_singleton.errcount            = 0;
   s_unittest_singleton.isResult            = false;

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}

// group: report

void logf_unittest(const char * format, ...)
{
   char     buffer[256];
   unsigned bytes;
   {
      va_list args;
      va_start(args, format);
      bytes = (unsigned) vsnprintf(buffer, sizeof(buffer), format, args);
      va_end(args);
   }
   ssize_t written = write(iochannel_STDOUT, buffer, bytes >= sizeof(buffer) ? sizeof(buffer)-1 : bytes);
   (void) written;
}

void logresult_unittest(bool isFailed)
{
   slock_mutex(&s_unittest_singleton.mutex);
   if (! s_unittest_singleton.isResult) {
      s_unittest_singleton.okcount  += ! isFailed;
      s_unittest_singleton.errcount += isFailed;
      s_unittest_singleton.isResult  = true;

      ssize_t written;
      if (isFailed) {
         written = write(iochannel_STDOUT, "FAILED\n", 7);
      } else {
         written = write(iochannel_STDOUT, "OK\n", 3);
      }
      (void) written;
   }
   sunlock_mutex(&s_unittest_singleton.mutex);
}

void logfailed_unittest(const char * filename, unsigned line_number)
{
   char           number[11];
   struct iovec   iov[4] = {
      { (void*) (uintptr_t) "TEST failed at ", 15 },
      { (void*) (uintptr_t) filename, strlen(filename) },
      { (void*) (uintptr_t) ":", 1 },
      { number, (unsigned) snprintf(number, sizeof(number), "%u\n", line_number) },
   };

   logresult_unittest(true);

   ssize_t written = writev(iochannel_STDOUT, iov, lengthof(iov));
   (void) written;
}

void logrun_unittest(const char * testname)
{
   struct iovec   iov[3] = {
       { (void*) (uintptr_t) "RUN ", 4 }
      ,{ (void*) (uintptr_t) testname, strlen(testname) }
      ,{ (void*) (uintptr_t) ": ", 2 }
   };

   ssize_t written = writev(iochannel_STDOUT, iov, lengthof(iov));
   (void) written;

   s_unittest_singleton.isResult = false;
}

void logsummary_unittest(void)
{
   logf_unittest("%s", "\nTEST SUMMARY:\n-------------\n");
   logf_unittest("FAILED TESTs: %zu\n", s_unittest_singleton.errcount);
   logf_unittest("PASSED TESTs: %zu\n", s_unittest_singleton.okcount);
}

// group: logfile

/* function: writelogfile_unittest
 * Writes errlog to s_unittest_singleton.log_files_directory + "/" + testname. */
static int writelogfile_unittest(const char * testname)
{
   int err;
   directory_t * dir = 0;

   err = new_directory(&dir, s_unittest_singleton.log_files_directory, 0);
   if (err) goto ONABORT;

   if (ENOENT == trypath_directory(dir, testname)) {
      uint8_t *logbuffer;
      size_t   size;
      GETBUFFER_ERRLOG(&logbuffer, &size);

      err = save_file(testname, size, logbuffer, dir);
      if (err) goto ONABORT;
   }

   err = delete_directory(&dir);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   logresult_unittest(true);
   logf_unittest("FAILED to write '%s/%s'\n", s_unittest_singleton.log_files_directory, testname);
   delete_directory(&dir);
   return err;
}

/* function: comparelogfile_unittest
 * Compares stored errlog file with content of current error log. */
static int comparelogfile_unittest(const char * testname)
{
   int err;
   directory_t  * dir             = 0;
   memblock_t     logfile_content = memblock_INIT_FREEABLE;
   wbuffer_t      wbuffer         = wbuffer_INIT_MEMBLOCK(&logfile_content);

   err = new_directory(&dir, s_unittest_singleton.log_files_directory, 0);
   if (err) goto ONABORT;

   err = load_file(testname, &wbuffer, dir);
   if (err) goto ONABORT;

   uint8_t *logbuffer;
   size_t   logsize;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);

   err = s_unittest_singleton.adapter->comparelog(logsize, logbuffer,
            size_wbuffer(&wbuffer), logfile_content.addr);
   if (err) goto ONABORT;

   err = FREE_MM(&logfile_content);
   if (err) goto ONABORT;

   err = delete_directory(&dir);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   logresult_unittest(true);
   logf_unittest("FAILED to compare '%s/%s'\n", s_unittest_singleton.log_files_directory, testname);
   delete_directory(&dir);
   FREE_MM(&logfile_content);
   return err;
}

// group: execute

int execsingle_unittest(const char * testname, int (*test_f)(void))
{
   int err;

   CLEARBUFFER_ERRLOG();

   logrun_unittest(testname);
   {
      err = test_f();
      if (!err) err = writelogfile_unittest(testname);
      if (!err) err = comparelogfile_unittest(testname);
   }
   logresult_unittest(err != 0);

   return err;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   unittest_t           old;
   unittest_adapter_t   adapter;

   // prepare
   memcpy(&old, &s_unittest_singleton, sizeof(old));
   TEST(0 == freesingleton_unittest());

   // TEST initsingleton_unittest
   s_unittest_singleton.log_files_directory = 0;
   s_unittest_singleton.okcount  = 1;
   s_unittest_singleton.errcount = 1;
   s_unittest_singleton.isResult = true;
   const char * dirname = "-test-/";
   TEST(0 == initsingleton_unittest(&adapter, dirname));
   TEST(&adapter== s_unittest_singleton.adapter);
   TEST(dirname == s_unittest_singleton.log_files_directory);
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isResult);

   // TEST freesingleton_unittest
   s_unittest_singleton.okcount  = 1;
   s_unittest_singleton.errcount = 1;
   s_unittest_singleton.isResult = true;
   TEST(0 == freesingleton_unittest());
   TEST(0 == s_unittest_singleton.adapter);
   TEST(0 == s_unittest_singleton.log_files_directory);
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isResult);
   TEST(0 == freesingleton_unittest());

   // unprepare
   memcpy(&s_unittest_singleton, &old, sizeof(old));

   return 0;
ONABORT:
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   return EINVAL;
}

static int test_report(void)
{
   int         fd[2]       = { iochannel_INIT_FREEABLE, iochannel_INIT_FREEABLE };
   int         oldstdout   = iochannel_INIT_FREEABLE;
   unittest_t  old;
   uint8_t     buffer[100];
   size_t      bytes_read;

   // prepare
   memcpy(&old, &s_unittest_singleton, sizeof(old));
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK));
   oldstdout = dup(iochannel_STDOUT);
   TEST(0 < oldstdout);
   TEST(iochannel_STDOUT == dup2(fd[1], iochannel_STDOUT));

   // TEST logf_unittest
   logf_unittest("Hello %d,%d\n", 1, 2);
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(10 == bytes_read);
   TEST(0 == strncmp((const char*)buffer, "Hello 1,2\n", bytes_read));

   // TEST logresult_unittest: logresult_unittest(0)
   s_unittest_singleton.okcount  = 2;
   s_unittest_singleton.errcount = 2;
   s_unittest_singleton.isResult = false;
   logresult_unittest(0);
   TEST(3 == s_unittest_singleton.okcount);
   TEST(2 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isResult);
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(3 == bytes_read);
   TEST(0 == strncmp((const char*)buffer, "OK\n", bytes_read));

   // TEST logresult_unittest: logresult_unittest(true)
   s_unittest_singleton.okcount  = 2;
   s_unittest_singleton.errcount = 2;
   s_unittest_singleton.isResult = false;
   logresult_unittest(true);
   TEST(2 == s_unittest_singleton.okcount);
   TEST(3 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isResult);
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(7 == bytes_read);
   TEST(0 == strncmp((const char*)buffer, "FAILED\n", bytes_read));

   // TEST logresult_unittest: s_unittest_singleton.isResult == true ==> nothing is written or counted
   s_unittest_singleton.okcount  = 0;
   s_unittest_singleton.errcount = 0;
   s_unittest_singleton.isResult = true;
   logresult_unittest(0);
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isResult);
   TEST(EAGAIN == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));

   // TEST logfailed_unittest
   logfailed_unittest("file", 45);
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(23 == bytes_read);
   TEST(0  == strncmp((const char*)buffer, "TEST failed at file:45\n", bytes_read));

   // TEST logrun_unittest
   s_unittest_singleton.isResult = true;
   logrun_unittest("test-name");
   TEST(0  == s_unittest_singleton.isResult);
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(15 == bytes_read);
   TEST(0  == strncmp((const char*)buffer, "RUN test-name: ", bytes_read));

   // TEST logsummary_unittest
   s_unittest_singleton.errcount = 3;
   s_unittest_singleton.okcount  = 4;
   logsummary_unittest();
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(61 == bytes_read);
   TEST(0  == strncmp((const char*)buffer, "\nTEST SUMMARY:\n-------------\nFAILED TESTs: 3\nPASSED TESTs: 4\n", bytes_read));

   // unprepare
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   TEST(iochannel_STDOUT == dup2(oldstdout, iochannel_STDOUT));
   TEST(0 == free_iochannel(&oldstdout));
   TEST(0 == free_iochannel(&fd[0]));
   TEST(0 == free_iochannel(&fd[1]));

   return 0;
ONABORT:
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   if (!isfree_iochannel(oldstdout)) {
      dup2(oldstdout, iochannel_STDOUT);
   }
   memset(buffer, 0, sizeof(buffer));
   read_iochannel(fd[0], sizeof(buffer)-1, buffer, 0);
   write_iochannel(iochannel_STDOUT, strlen((char*)buffer), buffer, 0);
   free_iochannel(&oldstdout);
   free_iochannel(&fd[0]);
   free_iochannel(&fd[1]);
   return EINVAL;
}

static int test_logfile(void)
{
   int         fd[2]     = { iochannel_INIT_FREEABLE, iochannel_INIT_FREEABLE };
   int         oldstdout = iochannel_INIT_FREEABLE;
   memblock_t  memblock  = memblock_INIT_FREEABLE;
   wbuffer_t   wbuffer   = wbuffer_INIT_MEMBLOCK(&memblock);
   unittest_t  old;
   uint8_t     buffer[100];
   uint8_t    *logbuffer;
   size_t      logsize;

   // prepare
   memcpy(&old, &s_unittest_singleton, sizeof(old));
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK));
   oldstdout = dup(iochannel_STDOUT);
   TEST(0 < oldstdout);
   TEST(iochannel_STDOUT == dup2(fd[1], iochannel_STDOUT));

   // TEST writelogfile_unittest: empty errlog
   s_unittest_singleton.log_files_directory = "./";
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(0 == logsize);
   TEST(ENOENT == trypath_directory(0, "xxx.unittest"));
   TEST(0 == writelogfile_unittest("xxx.unittest"));
   TEST(0 == trypath_directory(0, "xxx.unittest"));
   TEST(0 == load_file("xxx.unittest", &wbuffer, 0));
   TEST(0 == size_wbuffer(&wbuffer));
   TEST(0 == removefile_directory(0, "xxx.unittest"));

   // TEST writelogfile_unittest: errlog contains data
   PRINTF_ERRLOG("1234567\n");
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(8 == logsize);
   TEST(ENOENT == trypath_directory(0, "xxx.unittest"));
   TEST(0 == writelogfile_unittest("xxx.unittest"));
   TEST(0 == trypath_directory(0, "xxx.unittest"));
   TEST(0 == load_file("xxx.unittest", &wbuffer, 0));
   TEST(8 == size_wbuffer(&wbuffer));
   TEST(0 == strncmp((char*)memblock.addr, "1234567\n", 8));

   // TEST writelogfile_unittest: if file exists nothing is done
   TEST(0 == writelogfile_unittest("xxx.unittest"));

   // TEST comparelogfile_unittest
   TEST(0 == comparelogfile_unittest("xxx.unittest"));

   // TEST comparelogfile_unittest: EINVAL
   CLEARBUFFER_ERRLOG();
   TEST(EINVAL == comparelogfile_unittest("xxx.unittest"));

   // TEST comparelogfile_unittest
   TEST(0 == removefile_directory(0, "xxx.unittest"));
   TEST(ENOENT == comparelogfile_unittest("xxx.unittest"));

   // unprepare
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   TEST(iochannel_STDOUT == dup2(oldstdout, iochannel_STDOUT));
   TEST(0 == free_iochannel(&oldstdout));
   TEST(0 == free_iochannel(&fd[0]));
   TEST(0 == free_iochannel(&fd[1]));
   TEST(0 == FREE_MM(&memblock));

   return 0;
ONABORT:
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   if (!isfree_iochannel(oldstdout)) {
      dup2(oldstdout, iochannel_STDOUT);
   }
   memset(buffer, 0, sizeof(buffer));
   read_iochannel(fd[0], sizeof(buffer)-1, buffer, 0);
   write_iochannel(iochannel_STDOUT, strlen((char*)buffer), buffer, 0);
   free_iochannel(&oldstdout);
   free_iochannel(&fd[0]);
   free_iochannel(&fd[1]);
   FREE_MM(&memblock);
   return EINVAL;
}

static int dummy_unittest_ok(void)
{
   PRINTF_ERRLOG("ERRLOG");
   return 0;
}

static int dummy_unittest_fail1(void)
{
   PRINTF_ERRLOG("ERRLOG");
   return ENOMEM;
}

static int dummy_unittest_fail2(void)
{
   PRINTF_ERRLOG("ERRLOG");
   logfailed_unittest("_file_", 1234);
   return EINVAL;
}

static int compare_log_error(size_t logsize1, const uint8_t logbuffer1[logsize1],
                             size_t logsize2, const uint8_t logbuffer2[logsize2])
{
   (void)logsize1;
   (void)logbuffer1;
   (void)logsize2;
   (void)logbuffer2;
   return ENOBUFS;
}

static int test_exec(void)
{
   int         fd[2]     = { iochannel_INIT_FREEABLE, iochannel_INIT_FREEABLE };
   int         oldstdout = iochannel_INIT_FREEABLE;
   unittest_t  old;
   uint8_t     buffer[100];
   wbuffer_t   wbuffer   = wbuffer_INIT_STATIC(sizeof(buffer), buffer);
   size_t      bytes_read;
   unittest_adapter_t erradapter = { &compare_log_error };

   // prepare
   memcpy(&old, &s_unittest_singleton, sizeof(old));
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK));
   oldstdout = dup(iochannel_STDOUT);
   TEST(0 < oldstdout);
   TEST(iochannel_STDOUT == dup2(fd[1], iochannel_STDOUT));

   // TEST execsingle_unittest: test return OK
   s_unittest_singleton.log_files_directory = ".";
   s_unittest_singleton.okcount  = 2;
   s_unittest_singleton.errcount = 2;
   s_unittest_singleton.isResult = 0;
   TEST(0 == execsingle_unittest("dummy_unittest_ok", &dummy_unittest_ok));
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(26 == bytes_read);
   TEST(0 == strncmp("RUN dummy_unittest_ok: OK\n", (const char*)buffer, bytes_read));
   TEST(3 == s_unittest_singleton.okcount);
   TEST(2 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isResult);
   TEST(0 == trypath_directory(0, "dummy_unittest_ok"));
   TEST(0 == load_file("dummy_unittest_ok", &wbuffer, 0));
   TEST(6 == size_wbuffer(&wbuffer));
   TEST(0 == memcmp(buffer, "ERRLOG", 6));
   TEST(0 == removefile_directory(0, "dummy_unittest_ok"));
   clear_wbuffer(&wbuffer);

   // TEST execsingle_unittest: test OK but compare_log returns error
   s_unittest_singleton.adapter  = &erradapter;
   s_unittest_singleton.log_files_directory = ".";
   s_unittest_singleton.isResult = 0;
   TEST(ENOBUFS == execsingle_unittest("dummy_unittest_ok", &dummy_unittest_ok));
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(70 == bytes_read);
   TEST(0 == strncmp("RUN dummy_unittest_ok: FAILED\nFAILED to compare './dummy_unittest_ok'\n", (const char*)buffer, bytes_read));
   TEST(3 == s_unittest_singleton.okcount);
   TEST(3 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isResult);
   TEST(0 == trypath_directory(0, "dummy_unittest_ok"));
   TEST(0 == load_file("dummy_unittest_ok", &wbuffer, 0));
   TEST(6 == size_wbuffer(&wbuffer));
   TEST(0 == memcmp(buffer, "ERRLOG", 6));
   TEST(0 == removefile_directory(0, "dummy_unittest_ok"));
   clear_wbuffer(&wbuffer);

   // TEST execsingle_unittest: test returns ERROR (ENOMEM)
   TEST(ENOMEM == execsingle_unittest("dummy_unittest_fail", &dummy_unittest_fail1));
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(32 == bytes_read);
   TEST(0 == strncmp("RUN dummy_unittest_fail: FAILED\n", (const char*)buffer, bytes_read));
   TEST(3 == s_unittest_singleton.okcount);
   TEST(4 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isResult);
   TEST(ENOENT == trypath_directory(0, "dummy_unittest_fail"));

   // TEST execsingle_unittest: test returns ERROR (EINVAL) and calls logfailed_unittest
   TEST(EINVAL == execsingle_unittest("dummy_unittest_fail", &dummy_unittest_fail2));
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(59 == bytes_read);
   TEST(0 == strncmp("RUN dummy_unittest_fail: FAILED\nTEST failed at _file_:1234\n", (const char*)buffer, bytes_read));
   TEST(3 == s_unittest_singleton.okcount);
   TEST(5 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isResult);
   TEST(ENOENT == trypath_directory(0, "dummy_unittest_fail"));

   // unprepare
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   TEST(iochannel_STDOUT == dup2(oldstdout, iochannel_STDOUT));
   TEST(0 == free_iochannel(&oldstdout));
   TEST(0 == free_iochannel(&fd[0]));
   TEST(0 == free_iochannel(&fd[1]));

   return 0;
ONABORT:
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   if (!isfree_iochannel(oldstdout)) {
      dup2(oldstdout, iochannel_STDOUT);
   }
   memset(buffer, 0, sizeof(buffer));
   read_iochannel(fd[0], sizeof(buffer)-1, buffer, 0);
   write_iochannel(iochannel_STDOUT, strlen((char*)buffer), buffer, 0);
   free_iochannel(&oldstdout);
   free_iochannel(&fd[0]);
   free_iochannel(&fd[1]);
   return EINVAL;
}

int unittest_test_unittest()
{
   resourceusage_t   usage       = resourceusage_INIT_FREEABLE;
   size_t            oldokcount  = s_unittest_singleton.okcount;
   size_t            olderrcount = s_unittest_singleton.errcount;

   TEST(0 == init_resourceusage(&usage));

   if (test_initfree())    goto ONABORT;
   if (test_report())      goto ONABORT;
   if (test_logfile())     goto ONABORT;
   if (test_exec())        goto ONABORT;

   TEST(oldokcount  == s_unittest_singleton.okcount);
   TEST(olderrcount == s_unittest_singleton.errcount);

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONABORT:
   (void) free_resourceusage(&usage);
   return EINVAL;
}

#endif
