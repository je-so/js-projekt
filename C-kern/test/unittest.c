/* title: Unit-Test impl

   Implements functions defined in <Unit-Test>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/task/process.h"
#include "C-kern/api/test/resourceusage.h"


/* struct: unittest_t
 * Holds state of executed unit test. */
typedef struct unittest_t {
   // group: private
   const char *   log_files_directory;
   size_t         okcount;
   size_t         errcount;
   /* variables: isResult
    * If set indicates that running test is running at least the 2nd time. */
   uint8_t        isRepeat;
   /* variables: isResult
    * If set indicates that running test has called <logresult_unittest>.
    * Is reset with <logrun_unittest> which should be called before a test is run. */
   atomicflag_t   isResult;
} unittest_t;

// group: static variables

/* variable: s_unittest_singleton
 * Holds the context for all running unit tests. */

static unittest_t    s_unittest_singleton;

// group: lifetime

void initsingleton_unittest(const char * log_files_directory)
{
   s_unittest_singleton.log_files_directory = log_files_directory;
   s_unittest_singleton.okcount             = 0;
   s_unittest_singleton.errcount            = 0;
   s_unittest_singleton.isRepeat            = 0;
   set_atomicflag(&s_unittest_singleton.isResult);
}

void freesingleton_unittest(void)
{
   s_unittest_singleton.log_files_directory = 0;
   s_unittest_singleton.okcount             = 0;
   s_unittest_singleton.errcount            = 0;
   s_unittest_singleton.isRepeat            = 0;
   clear_atomicflag(&s_unittest_singleton.isResult);
}

// group: log

int isrepeat_unittest(void)
{
   return s_unittest_singleton.isRepeat;
}

// group: log

void logf_unittest(const char * format, ...)
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

static void vlogfailedf_unittest(const char * filename, unsigned line_number, const char * format, va_list args)
{
   struct iovec iov[3];
   char  number[11];
   char  buffer[256];
   ssize_t written;

   logresult_unittest(true);

   iov[0].iov_base = (void*) (uintptr_t) filename;
   iov[0].iov_len  = strlen(filename);
   iov[1].iov_base = number;
   iov[1].iov_len  = (unsigned) snprintf(number, sizeof(number), ":%u: ", line_number);
   iov[2].iov_base = (void*) (uintptr_t) "TEST FAILED\n";
   iov[2].iov_len  = 12;
   written = writev(iochannel_STDOUT, iov, lengthof(iov));
   (void) written;

   if (format) {
      iov[2].iov_base = buffer;
      iov[2].iov_len  = (unsigned) vsnprintf(buffer, sizeof(buffer), format, args);
      if (iov[2].iov_len >= sizeof(buffer)) {
         iov[2].iov_len = sizeof(buffer)-1;
      }
      buffer[iov[2].iov_len ++] = '\n';
      written = writev(iochannel_STDOUT, iov, lengthof(iov));
      (void) written;
   }
}

void logfailed_unittest(const char * filename, unsigned line_number)
{
   va_list args;
   memset(&args, 0, sizeof(args));
   vlogfailedf_unittest(filename, line_number, 0, args);
}

void logfailedf_unittest(const char * filename, unsigned line_number, const char * format, ...)
{
   va_list args;
   va_start(args, format);
   vlogfailedf_unittest(filename, line_number, format, args);
   va_end(args);
}

void logwarning_unittest(const char * reason)
{
   struct iovec iov[3];
   ssize_t written;

   iov[0].iov_base = CONST_CAST(char, "** ");
   iov[0].iov_len  = 3;
   iov[1].iov_base = CONST_CAST(char, reason);
   iov[1].iov_len  = strlen(reason);
   iov[2].iov_base = CONST_CAST(char, " ** ");
   iov[2].iov_len  = 4;

   written = writev(iochannel_STDOUT, iov, lengthof(iov));
   (void) written;
}

// group: report

static int setresult_unittest(int testerr)
{
   if (0 != set_atomicflag(&s_unittest_singleton.isResult)) goto ONERR;

   s_unittest_singleton.okcount  += (0 == testerr);
   s_unittest_singleton.errcount += (0 != testerr);
   return 0;

ONERR:
   return EALREADY;
}

void logresult_unittest(int testerr)
{
   if (0 == setresult_unittest(testerr)) {
      ssize_t written;
      if (testerr) {
         written = write(iochannel_STDOUT, "FAILED\n", 7);
      } else {
         written = write(iochannel_STDOUT, "OK\n", 3);
      }
      (void) written;
   }
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

   s_unittest_singleton.isRepeat = 0;
   clear_atomicflag(&s_unittest_singleton.isResult);
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
   if (err) goto ONERR;

   if (ENOENT == trypath_directory(dir, testname)) {
      uint8_t *logbuffer;
      size_t   size;
      GETBUFFER_ERRLOG(&logbuffer, &size);

      err = save_file(testname, size, logbuffer, dir);
      if (err) goto ONERR;
   }

   err = delete_directory(&dir);
   if (err) goto ONERR;

   return 0;
ONERR:
   logfailedf_unittest(__FILE__, __LINE__, "Can not write file '%s/%s'", s_unittest_singleton.log_files_directory, testname);
   delete_directory(&dir);
   return err;
}

/* function: comparelogfile_unittest
 * Compares stored errlog file with content of current error log. */
static int comparelogfile_unittest(const char * testname)
{
   int err;
   directory_t   *dir             = 0;
   memblock_t     logfile_content = memblock_FREE;
   wbuffer_t      wbuffer         = wbuffer_INIT_MEMBLOCK(&logfile_content);

   err = new_directory(&dir, s_unittest_singleton.log_files_directory, 0);
   if (err) goto ONERR;

   err = load_file(testname, &wbuffer, dir);
   if (err) goto ONERR;

   err = COMPARE_ERRLOG(size_wbuffer(&wbuffer), logfile_content.addr);
   if (err) goto ONERR;

   err = FREE_MM(&logfile_content);
   if (err) goto ONERR;

   err = delete_directory(&dir);
   if (err) goto ONERR;

   return 0;
ONERR:
   logfailedf_unittest(__FILE__, __LINE__, "Errlog differs from file '%s/%s'", s_unittest_singleton.log_files_directory, testname);
   delete_directory(&dir);
   FREE_MM(&logfile_content);
   return err;
}

// group: execute

int execsingle_unittest(const char * testname, int (*test_f)(void))
{
   int err = 0;
   int resourceLeak = 0;
   resourceusage_t usage = resourceusage_FREE;

   logrun_unittest(testname);

   // repeat several times in case of a resource leak
   for (unsigned testrepeat = 3; testrepeat; --testrepeat) {
      err = init_resourceusage(&usage);
      if (err) break;

      CLEARBUFFER_ERRLOG();
      err = test_f();

      if (!err) err = writelogfile_unittest(testname);
      if (!err) err = comparelogfile_unittest(testname);
      resourceLeak = (!err && 0 != same_resourceusage(&usage));

      int err2 = free_resourceusage(&usage);
      if (err2) err = err2;

      if (!resourceLeak) break;
      s_unittest_singleton.isRepeat = 1;
   }

   logresult_unittest(resourceLeak ? ELEAK : err);

   if (resourceLeak) {
      err = ELEAK;
      logfailedf_unittest(__FILE__, __LINE__, "FAILED to free all resources");
   }

   return err;
}


typedef struct childprocess_t {
   int   pipefd;
   int (*test_f)(void);
} childprocess_t;

static int childprocess_unittest(childprocess_t * param)
{
   int err;
   uint8_t *buffer;
   size_t   size = 0;
   size_t   written;

   // transfer flushed error messages
   TEST(STDERR_FILENO == dup2(param->pipefd,STDERR_FILENO));

   CLEARBUFFER_ERRLOG();
   err = param->test_f();

   // transfer cached error messages
   GETBUFFER_ERRLOG(&buffer, &size);
   TEST(0 == write_iochannel(param->pipefd, size, buffer, &written));
   TEST(written == size);

   return err;
ONERR:
   return EIO;
}

int execasprocess_unittest(int (*test_f)(void), /*out*/int * retcode)
{
   int err;
   process_t         child = process_FREE;
   process_result_t  result;
   childprocess_t    param;
   int               fd[2] = { -1, -1 };

   if (pipe2(fd, O_NONBLOCK|O_CLOEXEC)) {
      err = errno;
      TRACESYSCALL_ERRLOG("pipe2", err);
      goto ONERR;
   }

   param.pipefd = fd[1];
   param.test_f = test_f;

   err = init_process(&child, (int(*)(void*))&childprocess_unittest, &param, &(process_stdio_t)process_stdio_INIT_INHERIT);
   if (err) goto ONERR;
   err = wait_process(&child, &result);
   if (err) goto ONERR;
   err = free_process(&child);
   if (err) goto ONERR;

   if (process_state_TERMINATED != result.state) {
      logfailedf_unittest(__FILE__, __LINE__, "Test process aborted (signalnr:%02d)", result.returncode);
      result.returncode = EINTR;
   }

   for (;;) {
      uint8_t  buffer[256];
      size_t   size;
      err = read_iochannel(fd[0], sizeof(buffer)-1, buffer, &size);
      if (err) {
         if (err == EAGAIN) break;
         goto ONERR;
      }
      buffer[size] = 0;
      PRINTF_ERRLOG("%s", (char*)buffer);
   }

   err = free_iochannel(&fd[0]);
   if (err) goto ONERR;
   err = free_iochannel(&fd[1]);
   if (err) goto ONERR;

   if (retcode) {
      *retcode = result.returncode;
   }

   return 0;
ONERR:
   (void) free_iochannel(&fd[0]);
   (void) free_iochannel(&fd[1]);
   (void) free_process(&child);
   return err;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   unittest_t old;

   // prepare
   memcpy(&old, &s_unittest_singleton, sizeof(old));
   freesingleton_unittest();

   // TEST initsingleton_unittest
   s_unittest_singleton.log_files_directory = 0;
   s_unittest_singleton.okcount  = 1;
   s_unittest_singleton.errcount = 1;
   s_unittest_singleton.isRepeat = 1;
   s_unittest_singleton.isResult = 0;
   const char * dirname = "-test-/";
   initsingleton_unittest(dirname);
   TEST(dirname == s_unittest_singleton.log_files_directory);
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isRepeat);
   TEST(0 != s_unittest_singleton.isResult);

   // TEST freesingleton_unittest
   s_unittest_singleton.okcount  = 1;
   s_unittest_singleton.errcount = 1;
   s_unittest_singleton.isRepeat = true;
   s_unittest_singleton.isResult = true;
   for (int t = 0; t < 2; ++t) {
      freesingleton_unittest();
      TEST(0 == s_unittest_singleton.log_files_directory);
      TEST(0 == s_unittest_singleton.okcount);
      TEST(0 == s_unittest_singleton.errcount);
      TEST(0 == s_unittest_singleton.isRepeat);
      TEST(0 == s_unittest_singleton.isResult);
   }

   // unprepare
   memcpy(&s_unittest_singleton, &old, sizeof(old));

   return 0;
ONERR:
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   return EINVAL;
}

static int test_query(void)
{
   uint8_t old = s_unittest_singleton.isRepeat;

   // TEST isrepeat_unittest: false
   s_unittest_singleton.isRepeat = 0;
   TEST(0 == isrepeat_unittest());

   // TEST isrepeat_unittest: false
   s_unittest_singleton.isRepeat = 1;
   TEST(1 == isrepeat_unittest());

   // unprepare
   s_unittest_singleton.isRepeat = old;

   return 0;
ONERR:
   s_unittest_singleton.isRepeat = old;
   return EINVAL;
}

static int test_report(void)
{
   int         fd[2]       = { iochannel_FREE, iochannel_FREE };
   int         oldstdout   = iochannel_FREE;
   unittest_t  old;
   uint8_t     buffer[512];
   size_t      bytes_read;

   // prepare
   memcpy(&old, &s_unittest_singleton, sizeof(old));
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK));
   oldstdout = dup(iochannel_STDOUT);
   TEST(0 < oldstdout);
   TEST(iochannel_STDOUT == dup2(fd[1], iochannel_STDOUT));

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
   for (int t = 0; t < 2; ++t) {
      logresult_unittest(t != 0);
      TEST(0 == s_unittest_singleton.okcount);
      TEST(0 == s_unittest_singleton.errcount);
      TEST(1 == s_unittest_singleton.isResult);
      TEST(EAGAIN == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   }

   // TEST logrun_unittest
   s_unittest_singleton.isRepeat = true;
   s_unittest_singleton.isResult = true;
   logrun_unittest("test-name");
   TEST(0  == s_unittest_singleton.isRepeat);
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
ONERR:
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

static int test_log(void)
{
   int         fd[2]       = { iochannel_FREE, iochannel_FREE };
   int         oldstdout   = iochannel_FREE;
   unittest_t  old;
   uint8_t     buffer[512];
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

   // TEST logf_unittest: only 255 characters are print at max
   uint8_t teststr[257];
   memset(teststr, 'A', sizeof(teststr)-1);
   teststr[sizeof(teststr)-1] = 0;
   logf_unittest("%s", teststr);
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(255 == bytes_read);
   TEST(0 == memcmp(buffer, teststr, bytes_read));

   // TEST logfailed_unittest: sets isResult
   TEST(0 == s_unittest_singleton.isResult);
   logfailed_unittest("file", 45);
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(28 == bytes_read);
   TEST(0  == strncmp((const char*)buffer, "FAILED\nfile:45: TEST FAILED\n", bytes_read));

   // TEST logfailed_unittest: isResult already set
   logfailed_unittest("file", 45);
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(21 == bytes_read);
   TEST(0  == strncmp((const char*)buffer, "file:45: TEST FAILED\n", bytes_read));

   // TEST logfailedf_unittest
   logfailedf_unittest("File", 35, "%" PRIu32, (uint32_t)-1);
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(41 == bytes_read);
   TEST(0  == memcmp(buffer, "File:35: TEST FAILED\nFile:35: 4294967295\n", bytes_read));

   // TEST logfailedf_unittest: only 255 chars are printed at max as value
   memset(teststr, 'A', sizeof(teststr)-1);
   teststr[sizeof(teststr)-1] = 0;
   logfailedf_unittest("File", 35, "%s", teststr);
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(30+255+1 == bytes_read);
   TEST(0  == memcmp(buffer, "File:35: TEST FAILED\nFile:35: ", 30));
   TEST(0  == memcmp(buffer+30, teststr, 255));
   TEST(0  == memcmp(buffer+30+255, "\n", 1));

   // TEST logwarning_unittest
   logwarning_unittest("12 Warning 34");
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(3+4+13 == bytes_read);
   TEST(0  == memcmp(buffer, "** 12 Warning 34 ** ", bytes_read));

   // unprepare
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   TEST(iochannel_STDOUT == dup2(oldstdout, iochannel_STDOUT));
   TEST(0 == free_iochannel(&oldstdout));
   TEST(0 == free_iochannel(&fd[0]));
   TEST(0 == free_iochannel(&fd[1]));

   return 0;
ONERR:
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
   int         fd[2]     = { iochannel_FREE, iochannel_FREE };
   int         oldstdout = iochannel_FREE;
   memblock_t  memblock  = memblock_FREE;
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
ONERR:
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

static int s_dummy_unittest_leak[10];
static int s_dummy_unittest_leak_isrepeat[10] = { 0 };

static int dummy_unittest_leak(void)
{
   for (unsigned i = 0; i < lengthof(s_dummy_unittest_leak); ++i) {
      if (s_dummy_unittest_leak[i] == iochannel_FREE) {
         s_dummy_unittest_leak[i] = dup(STDERR_FILENO);
         s_dummy_unittest_leak_isrepeat[i] = isrepeat_unittest();
         break;
      }
   }
   return 0;
}

static int dummy_unittest_abort(void)
{
   abort();
   return 0;
}

static int test_exec(void)
{
   int         fd[2]     = { iochannel_FREE, iochannel_FREE };
   int         oldstdout = iochannel_FREE;
   unittest_t  old;
   uint8_t     buffer[200];
   wbuffer_t   wbuffer   = wbuffer_INIT_STATIC(sizeof(buffer), buffer);
   size_t      bytes_read;

   // prepare
   for (unsigned i = 0; i < lengthof(s_dummy_unittest_leak); ++i) {
      s_dummy_unittest_leak[i] = iochannel_FREE;
      s_dummy_unittest_leak_isrepeat[i] = 0;
   }
   memcpy(&old, &s_unittest_singleton, sizeof(old));
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK));
   oldstdout = dup(iochannel_STDOUT);
   TEST(0 < oldstdout);
   TEST(iochannel_STDOUT == dup2(fd[1], iochannel_STDOUT));

   // TEST execsingle_unittest: test return OK (second run compares with created log file)
   for (int t = 0; t < 2; ++t) {
      s_unittest_singleton.log_files_directory = ".";
      s_unittest_singleton.okcount  = 2;
      s_unittest_singleton.errcount = 2;
      s_unittest_singleton.isRepeat = 1;
      s_unittest_singleton.isResult = 0;
      TEST(0 == execsingle_unittest("dummy_unittest_ok", &dummy_unittest_ok));
      TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
      TEST(26 == bytes_read);
      TEST(0 == strncmp("RUN dummy_unittest_ok: OK\n", (const char*)buffer, bytes_read));
      TEST(3 == s_unittest_singleton.okcount);
      TEST(2 == s_unittest_singleton.errcount);
      TEST(0 == s_unittest_singleton.isRepeat);
      TEST(0 != s_unittest_singleton.isResult);
      TEST(0 == trypath_directory(0, "dummy_unittest_ok"));
      TEST(0 == load_file("dummy_unittest_ok", &wbuffer, 0));
      TEST(6 == size_wbuffer(&wbuffer));
      TEST(0 == memcmp(buffer, "ERRLOG", 6));
      clear_wbuffer(&wbuffer);
   }

   // TEST execsingle_unittest: test OK but compare_log returns error
   TEST(0 == removefile_directory(0, "dummy_unittest_ok"));
   TEST(0 == save_file("dummy_unittest_ok", 6, "ERRLOX", 0));
   s_unittest_singleton.log_files_directory = ".";
   s_unittest_singleton.isRepeat = 1;
   s_unittest_singleton.isResult = 0;
   TEST(EINVAL == execsingle_unittest("dummy_unittest_ok", &dummy_unittest_ok));
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(145 == bytes_read);
   TEST(0 == strncmp("RUN dummy_unittest_ok: FAILED\nC-kern/test/unittest.c:268: TEST FAILED\nC-kern/test/unittest.c:268: Errlog differs from file './dummy_unittest_ok'\n", (const char*)buffer, bytes_read));
   TEST(3 == s_unittest_singleton.okcount);
   TEST(3 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isRepeat);
   TEST(0 != s_unittest_singleton.isResult);
   TEST(0 == trypath_directory(0, "dummy_unittest_ok"));
   TEST(0 == removefile_directory(0, "dummy_unittest_ok"));

   // TEST execsingle_unittest: test returns ERROR (ENOMEM)
   s_unittest_singleton.isRepeat = 1;
   s_unittest_singleton.isResult = 0;
   TEST(ENOMEM == execsingle_unittest("dummy_unittest_fail", &dummy_unittest_fail1));
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(32 == bytes_read);
   TEST(0 == strncmp("RUN dummy_unittest_fail: FAILED\n", (const char*)buffer, bytes_read));
   TEST(3 == s_unittest_singleton.okcount);
   TEST(4 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isRepeat);
   TEST(0 != s_unittest_singleton.isResult);
   TEST(ENOENT == trypath_directory(0, "dummy_unittest_fail"));

   // TEST execsingle_unittest: test returns ERROR (EINVAL) and calls logfailed_unittest
   s_unittest_singleton.isRepeat = 1;
   s_unittest_singleton.isResult = 0;
   TEST(EINVAL == execsingle_unittest("dummy_unittest_fail", &dummy_unittest_fail2));
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(57 == bytes_read);
   TEST(0 == strncmp("RUN dummy_unittest_fail: FAILED\n_file_:1234: TEST FAILED\n", (const char*)buffer, bytes_read));
   TEST(3 == s_unittest_singleton.okcount);
   TEST(5 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isRepeat);
   TEST(0 != s_unittest_singleton.isResult);
   TEST(ENOENT == trypath_directory(0, "dummy_unittest_fail"));

   // TEST execsingle_unittest: ELEAK (leaks file) repeated 3 times
   s_unittest_singleton.isRepeat = 1;
   s_unittest_singleton.isResult = 0;
   TEST(ELEAK == execsingle_unittest("dummy_unittest_leak", &dummy_unittest_leak));
   for (unsigned i = 0; i < 3; ++i) {
      TEST(0 == isfree_iochannel(s_dummy_unittest_leak[i]));
      TEST((i != 0) == s_dummy_unittest_leak_isrepeat[i]);
      TEST(0 == free_iochannel(&s_dummy_unittest_leak[i]));
   }
   for (unsigned i = 3; i < lengthof(s_dummy_unittest_leak); ++i) {
      TEST(0 != isfree_iochannel(s_dummy_unittest_leak[i]));
      TEST(0 == s_dummy_unittest_leak_isrepeat[i]);
   }
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(129 == bytes_read);
   TEST(0 == strncmp("RUN dummy_unittest_leak: FAILED\nC-kern/test/unittest.c:307: TEST FAILED\nC-kern/test/unittest.c:307: FAILED to free all resources\n", (const char*)buffer, bytes_read));
   TEST(3 == s_unittest_singleton.okcount);
   TEST(6 == s_unittest_singleton.errcount);
   TEST(0 != s_unittest_singleton.isResult);
   TEST(ENOENT != trypath_directory(0, "dummy_unittest_leak"));
   TEST(0 == removefile_directory(0, "dummy_unittest_leak"));

   // TEST execasprocess_unittest: return code 0
   s_unittest_singleton.okcount  = 0;
   s_unittest_singleton.errcount = 0;
   s_unittest_singleton.isRepeat = 1;
   s_unittest_singleton.isResult = 0;
   int      retcode = 1;
   uint8_t *logbuffer;
   size_t   logsize;
   CLEARBUFFER_ERRLOG();
   TEST(0 == execasprocess_unittest(&dummy_unittest_ok, &retcode));
   TEST(0 == retcode);
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isRepeat);
   TEST(0 == s_unittest_singleton.isResult);
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(6 == logsize);
   TEST(0 == memcmp(logbuffer, "ERRLOG", 6));

   // TEST execasprocess_unittest: null address instead of &retcode
   CLEARBUFFER_ERRLOG();
   TEST(0 == execasprocess_unittest(&dummy_unittest_ok, 0/*null*/));
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isRepeat);
   TEST(0 == s_unittest_singleton.isResult);
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(6 == logsize);
   TEST(0 == memcmp(logbuffer, "ERRLOG", 6));

   // TEST execasprocess_unittest: ERRORLOG is cleared in child process
   s_unittest_singleton.isRepeat = 0;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(6 == logsize); // is not doubled
   TEST(0 == execasprocess_unittest(&dummy_unittest_ok, &retcode));
   TEST(0 == retcode);
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isRepeat);
   TEST(0 == s_unittest_singleton.isResult);
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(12 == logsize); // would be 18 if logsize is not cleared
   TEST(0  == memcmp(logbuffer, "ERRLOGERRLOG", 12));

   // TEST execasprocess_unittest: return code ENOMEM
   CLEARBUFFER_ERRLOG();
   TEST(0 == execasprocess_unittest(&dummy_unittest_fail1, &retcode));
   TEST(ENOMEM == retcode);
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isRepeat);
   TEST(0 == s_unittest_singleton.isResult);
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(6 == logsize);
   TEST(0 == memcmp(logbuffer, "ERRLOG", 6));

   // TEST execasprocess_unittest: ignore called logfailed_unittest (std-out is inherited)
   CLEARBUFFER_ERRLOG();
   TEST(0 == execasprocess_unittest(&dummy_unittest_fail2, &retcode));
   TEST(EINVAL == retcode);
   // check logfailed_unittest has not transfered result
   TEST(0 == s_unittest_singleton.okcount);
   TEST(0 == s_unittest_singleton.errcount);
   TEST(0 == s_unittest_singleton.isRepeat);
   TEST(0 == s_unittest_singleton.isResult);
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(6 == logsize);
   TEST(0 == memcmp(logbuffer, "ERRLOG", 6));
   PRINTF_ERRLOG("\n");
   // check std-out inherited
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(32 == bytes_read);
   TEST(0 == strncmp("FAILED\n_file_:1234: TEST FAILED\n", (const char*)buffer, bytes_read));

   // TEST execasprocess_unittest: EINTR
   s_unittest_singleton.errcount = 0;
   s_unittest_singleton.isRepeat = 1;
   s_unittest_singleton.isResult = 0;
   TEST(0 == execasprocess_unittest(&dummy_unittest_abort, &retcode));
   TEST(EINTR == retcode);
   TEST(0 == s_unittest_singleton.okcount);
   TEST(1 == s_unittest_singleton.errcount);
   TEST(1 == s_unittest_singleton.isRepeat);
   TEST(0 != s_unittest_singleton.isResult);
   // errlog not changed
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(7 == logsize);
   TEST(0 == memcmp(logbuffer, "ERRLOG\n", 7));
   // stdout contains info about process failure
   TEST(0 == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TESTP(110 == bytes_read, "bytes_read:%zd", bytes_read)
   TEST(0 == strncmp("FAILED\nC-kern/test/unittest.c:367: TEST FAILED\nC-kern/test/unittest.c:367: Test process aborted (signalnr:06)\n", (const char*)buffer, bytes_read));
   TEST(EAGAIN == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));

   // unprepare
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   TEST(iochannel_STDOUT == dup2(oldstdout, iochannel_STDOUT));
   TEST(0 == free_iochannel(&oldstdout));
   TEST(0 == free_iochannel(&fd[0]));
   TEST(0 == free_iochannel(&fd[1]));

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(s_dummy_unittest_leak); ++i) {
      free_iochannel(&s_dummy_unittest_leak[i]);
   }
   removefile_directory(0, "dummy_unittest_ok");
   removefile_directory(0, "dummy_unittest_leak");
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

static void call_test_macro(void)
{
   ssize_t bytes;
   TEST(1);
   bytes = write(STDOUT_FILENO, "X", 1);
   (void) bytes;
   TEST(0);
   bytes = write(STDOUT_FILENO, "Y", 1);
   (void) bytes;
   return;
ONERR:
   bytes = write(STDOUT_FILENO, "Z", 1);
   (void) bytes;
   return;
}

static void call_testp_macro(void)
{
   ssize_t bytes;
   TESTP(1 == 1, "%d %d", 1, 2);
   bytes = write(STDOUT_FILENO, "X", 1);
   (void) bytes;
   TESTP(0 == 1, "%" PRIu64 ",%d", (uint64_t)1, 2);
   bytes = write(STDOUT_FILENO, "Y", 1);
   (void) bytes;
   return;
ONERR:
   bytes = write(STDOUT_FILENO, "Z", 1);
   (void) bytes;
   return;
}

static int test_macros(void)
{
   int         fd[2]     = { iochannel_FREE, iochannel_FREE };
   int         oldstdout = iochannel_FREE;
   unittest_t  old;
   uint8_t     buffer[200];
   size_t      bytes_read;

   // prepare
   memcpy(&old, &s_unittest_singleton, sizeof(old));
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK));
   oldstdout = dup(iochannel_STDOUT);
   TEST(0 < oldstdout);
   TEST(iochannel_STDOUT == dup2(fd[1], iochannel_STDOUT));

   // TEST TEST: s_unittest_singleton.isResult == 0
   s_unittest_singleton.okcount = 0;
   s_unittest_singleton.errcount = 0;
   s_unittest_singleton.isResult = 0;
   call_test_macro();
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(0 == s_unittest_singleton.okcount);
   TEST(1 == s_unittest_singleton.errcount);
   TEST(0 != s_unittest_singleton.isResult);
   TEST(50 == bytes_read);
   TEST(0  == memcmp(buffer, "XFAILED\nC-kern/test/unittest.c:1002: TEST FAILED\nZ", bytes_read));

   // TEST TEST: s_unittest_singleton.isResult != 0
   call_test_macro();
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(0 == s_unittest_singleton.okcount);
   TEST(1 == s_unittest_singleton.errcount);
   TEST(0 != s_unittest_singleton.isResult);
   TEST(43 == bytes_read);
   TEST(0  == memcmp(buffer, "XC-kern/test/unittest.c:1002: TEST FAILED\nZ", bytes_read));

   // TEST TESTP: s_unittest_singleton.isResult == 0
   s_unittest_singleton.okcount = 0;
   s_unittest_singleton.errcount = 0;
   s_unittest_singleton.isResult = 0;
   call_testp_macro();
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(0 == s_unittest_singleton.okcount);
   TEST(1 == s_unittest_singleton.errcount);
   TEST(0 != s_unittest_singleton.isResult);
   TEST(83 == bytes_read);
   TEST(0  == memcmp(buffer, "XFAILED\nC-kern/test/unittest.c:1018: TEST FAILED\nC-kern/test/unittest.c:1018: 1,2\nZ", bytes_read));

   // TEST TESTP: s_unittest_singleton.isResult != 0
   call_testp_macro();
   TEST(0  == read_iochannel(fd[0], sizeof(buffer), buffer, &bytes_read));
   TEST(0 == s_unittest_singleton.okcount);
   TEST(1 == s_unittest_singleton.errcount);
   TEST(0 != s_unittest_singleton.isResult);
   TEST(76 == bytes_read);
   TEST(0  == memcmp(buffer, "XC-kern/test/unittest.c:1018: TEST FAILED\nC-kern/test/unittest.c:1018: 1,2\nZ", bytes_read));

   // unprepare
   memcpy(&s_unittest_singleton, &old, sizeof(old));
   TEST(iochannel_STDOUT == dup2(oldstdout, iochannel_STDOUT));
   TEST(0 == free_iochannel(&oldstdout));
   TEST(0 == free_iochannel(&fd[0]));
   TEST(0 == free_iochannel(&fd[1]));

   return 0;
ONERR:
   removefile_directory(0, "dummy_unittest_ok");
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
   size_t   oldokcount  = s_unittest_singleton.okcount;
   size_t   olderrcount = s_unittest_singleton.errcount;

   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_report())      goto ONERR;
   if (test_log())         goto ONERR;
   if (test_logfile())     goto ONERR;
   if (test_exec())        goto ONERR;
   if (test_macros())      goto ONERR;

   TEST(oldokcount  == s_unittest_singleton.okcount);
   TEST(olderrcount == s_unittest_singleton.errcount);

   return 0;
ONERR:
   return EINVAL;
}

#endif
