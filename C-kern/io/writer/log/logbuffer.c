/* title: LogBuffer impl

   Implements <LogBuffer>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/writer/log/logbuffer.h
    Header file <LogBuffer>.

   file: C-kern/io/writer/log/logbuffer.c
    Implementation file <LogBuffer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: logbuffer_t

// group: lifetime

int init_logbuffer(/*out*/logbuffer_t * logbuf, size_t buffer_size, uint8_t buffer_addr[buffer_size], sys_iochannel_t io)
{
   int err;

   VALIDATE_INPARAM_TEST(buffer_size > log_config_MINSIZE, ONERR,);

   logbuf->addr = buffer_addr;
   logbuf->size = buffer_size;
   logbuf->logsize = 0;
   logbuf->io   = io;
   logbuf->addr[0] = 0;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_logbuffer(logbuffer_t * logbuf)
{
   int err;

   logbuf->addr = 0;
   logbuf->size = 0;
   logbuf->logsize = 0;

   if (  logbuf->io == iochannel_STDOUT
         || logbuf->io == iochannel_STDERR) {
      logbuf->io = iochannel_FREE;
   } else {
      err = free_iochannel(&logbuf->io);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

int compare_logbuffer(const logbuffer_t * logbuf, size_t logsize, const uint8_t logbuffer[logsize])
{
   if (logsize != logbuf->logsize) return EINVAL;

   for (size_t i = 0; i < logsize; ++i) {
      if (  logbuf->addr[i] != logbuffer[i]) {
         return EINVAL;
      }
      if (  logbuf->addr[i] == '['
            && (i == 0 || logbuf->addr[i-1] == '\n')) {
         size_t len = 0;
         while (i+len < logsize && logbuf->addr[i+len] != ' ') {
            ++len;
         }
         ++ len;
         if (  (i+len) >= logsize
               || 0 != memcmp(logbuf->addr+i, logbuffer+i, len)) {
            return EINVAL;
         }
         i += len;
         while (i < logsize && logbuf->addr[i] != ']' && logbuffer[i] != ']') {
            ++i;
         }
         if (  i >= logsize || logbuf->addr[i] != logbuffer[i]) {
            return EINVAL;
         }
      }
   }

   return 0;
}

// group: update

int write_logbuffer(logbuffer_t * logbuf)
{
   size_t bytes_written = 0;

   // TODO: add syncthread which does logging in background !
   //       first add global io queue where I/Os could be registered
   //       create new buffer and transfer full buffer to I/O queue

   while (bytes_written < logbuf->logsize) {
      ssize_t bytes;
      do {
         bytes = write(logbuf->io, logbuf->addr + bytes_written, logbuf->logsize - bytes_written);
      } while (bytes < 0 && errno == EINTR);

      if (bytes < 0) {
         if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // TODO: add code that indicates error state in logging
            return errno;
         }
         struct pollfd pfd = { .fd = logbuf->io, .events = POLLOUT };
         while (poll(&pfd, 1, -1/*no timeout*/) < 0) {
            if (errno != EINTR) return errno;
         }
      } else {
         bytes_written += (size_t) bytes;
      }
   }

   return 0;
}

void vprintf_logbuffer(logbuffer_t * logbuf, const char * format, va_list args)
{
   if (!format) return;

   size_t   buffer_size = sizefree_logbuffer(logbuf);
   uint8_t* buffer      = logbuf->addr + logbuf->logsize;

   int bytes = vsnprintf((char*)buffer, buffer_size, format, args);

   if (bytes > 0) {
      size_t size = (size_t)bytes;

      if (size >= buffer_size) {
         // data has been truncated => mark it with " ..."
         size = buffer_size ? buffer_size-1/*last byte \0 byte*/ : 0;
         if (logbuf->size >= 5) {
            memcpy(logbuf->addr + logbuf->size - 5, " ...", 4);
         }
      }

      logbuf->logsize += size;
   }
}

void printf_logbuffer(logbuffer_t * logbuf, const char * format, ...)
{
   va_list args;
   va_start(args, format);
   vprintf_logbuffer(logbuf, format, args);
   va_end(args);
}

void printheader_logbuffer(logbuffer_t * logbuf, const struct log_header_t * header)
{
   struct timeval tv;
   if (-1 == gettimeofday(&tv, 0)) {
      tv.tv_sec  = 0;
      tv.tv_usec = 0;
   }
   static_assert(sizeof(tv.tv_sec) <= sizeof(uint64_t), "conversion works");
   // !not needed! static_assert(sizeof(tv.tv_usec) <= sizeof(uint32_t), "conversion works");
   // cause only values are expected in range 0..999999 < UINT32_MAX

#define CALL(TEXTID, ...) \
   {                                         \
      struct p_ ## TEXTID ## _ERRLOG         \
         params = { __VA_ARGS__ };           \
      TEXTID ## _ERRLOG (logbuf, &params);   \
   }
   CALL(LOGENTRY_HEADER, threadid_maincontext(), (uint64_t)tv.tv_sec, (uint32_t)tv.tv_usec, header->funcname, header->filename, header->linenr);
#undef CALL
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   logbuffer_t logbuf = logbuffer_FREE;
   uint8_t     buffer[10];

   // TEST logbuffer_FREE
   TEST(0 == logbuf.addr);
   TEST(0 == logbuf.size);
   TEST(0 == logbuf.logsize);
   TEST(sys_iochannel_FREE == logbuf.io);

   // TEST logbuffer_INIT
   buffer[2] = 1;
   logbuf = (logbuffer_t) logbuffer_INIT(4, &buffer[2], 6);
   TEST(logbuf.addr    == &buffer[2]);
   TEST(logbuf.size    == 4);
   TEST(logbuf.logsize == 0);
   TEST(logbuf.io      == 6);
   TEST(buffer[2]      == 0);

   // TEST init_logbuffer
   buffer[0] = 1;
   TEST(0 == init_logbuffer(&logbuf, log_config_MINSIZE+1, buffer, iochannel_STDOUT));
   TEST(logbuf.addr    == buffer);
   TEST(logbuf.size    == log_config_MINSIZE+1);
   TEST(logbuf.logsize == 0);
   TEST(logbuf.io      == iochannel_STDOUT);
   TEST(buffer[0]      == 0);

   // TEST free_logbuffer
   logbuf.logsize = 1;
   TEST(0 == free_logbuffer(&logbuf));
   TEST(0 == logbuf.addr);
   TEST(0 == logbuf.size);
   TEST(0 == logbuf.logsize);
   TEST(sys_iochannel_FREE == logbuf.io);
   TEST(1 == isvalid_iochannel(iochannel_STDOUT));
   TEST(0 == free_logbuffer(&logbuf));
   TEST(0 == logbuf.addr);
   TEST(0 == logbuf.size);
   TEST(0 == logbuf.logsize);
   TEST(sys_iochannel_FREE == logbuf.io);

   // TEST free_logbuffer: close descriptor
   int pfd[2];
   TEST(0 == pipe2(pfd, O_CLOEXEC));
   for (unsigned i = 0; i < 2; ++i) {
      logbuf = (logbuffer_t) logbuffer_INIT(1, buffer, pfd[i]);
      TEST(1 == isvalid_iochannel(pfd[i]));
      logbuf.logsize = 1;
      TEST(0 == free_logbuffer(&logbuf));
      TEST(0 == logbuf.addr);
      TEST(0 == logbuf.size);
      TEST(0 == logbuf.logsize);
      TEST(sys_iochannel_FREE == logbuf.io);
      TEST(0 == isvalid_iochannel(pfd[i]));
   }

   // TEST init_logbuffer: EINVAL
   TEST(EINVAL == init_logbuffer(&logbuf, log_config_MINSIZE/*too small*/, buffer, iochannel_STDOUT));
   TEST(0 == logbuf.addr);
   TEST(0 == logbuf.size);
   TEST(0 == logbuf.logsize);
   TEST(sys_iochannel_FREE == logbuf.io);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   logbuffer_t logbuf = logbuffer_FREE;

   // TEST io_logbuffer
   for (int i = -1; i < 100; ++i) {
      logbuf.io = i;
      TEST(i == io_logbuffer(&logbuf));
   }

   // TEST getbuffer_logbuffer
   for (unsigned i = 0; i <= 100; ++i) {
      logbuf.addr    = (uint8_t*) (uintptr_t) (i*33);
      logbuf.logsize = 10000 * i;
      uint8_t * addr = 0;
      size_t    size = 0;
      getbuffer_logbuffer(&logbuf, &addr, &size);
      TEST(addr == (uint8_t*) (uintptr_t) (i*33));
      TEST(size == 10000 * i);
   }

   // TEST sizefree_logbuffer
   logbuf.size    = 100000;
   logbuf.logsize = 0;
   for (unsigned i = 0; i <= 100; ++i) {
      logbuf.logsize = i;
      TEST(100000-i == sizefree_logbuffer(&logbuf));
   }
   logbuf.logsize = 100000;
   TEST(0 == sizefree_logbuffer(&logbuf));
   logbuf.size += 100000;
   TEST(100000 == sizefree_logbuffer(&logbuf));

   // TEST compare_logbuffer: compare equal (no timestamp)
   uint8_t buffer[256];
   logbuf.addr = buffer;
   logbuf.size = sizeof(buffer);
   logbuf.logsize = 10;
   memcpy(buffer, "xyz123,;._", 10);
   TEST(0 == compare_logbuffer(&logbuf, 10, (const uint8_t*)"xyz123,;._"));

   // TEST compare_logbuffer: compare unequal (no timestamp)
   TEST(EINVAL == compare_logbuffer(&logbuf,  9, (const uint8_t*)"xyz123,;._"));
   TEST(EINVAL == compare_logbuffer(&logbuf, 10, (const uint8_t*)"xyz123,;.?"));

   // TEST compare_logbuffer: skip timestamp
   const char * testlog1[] = { "[NOT-IGNORED: 99IGNORED99]\ntext", "same text\n[1: 99IGNORED99]\ntext" };
   const char * testlog2[] = { "[NOT-IGNORED: XXXXXXXXXXX]\ntext", "same text\n[1: XXXXXXXXXXX]\ntext" };
   for (unsigned i = 0; i < lengthof(testlog1);  ++i) {
      logbuf.logsize = strlen(testlog1[i]);
      memcpy(buffer, testlog1[i], logbuf.logsize);
      TEST(0 == compare_logbuffer(&logbuf, strlen(testlog2[i]), (const uint8_t*)testlog2[i]));
   }

   // TEST compare_logbuffer: compare unequal (with timestamp)
   /* 1: no space
    * 2: not equal until space
    * 3: ignored part no same length
    * 4: no closing ']'
    * 5: [ does not start at beginning of line */
   const char * testlog3[] = { "[...]\n", "[... ]\n", "[1: IGN]\n", "[1: IGN\n", "x[1: X]\n" };
   const char * testlog4[] = { "[...]\n", "[..? ]\n", "[1: XX] \n", "[1: IGN\n", "x[1: Y]\n" };
   for (unsigned i = 0; i < lengthof(testlog3);  ++i) {
      logbuf.logsize = strlen(testlog3[i]);
      memcpy(buffer, testlog3[i], logbuf.logsize);
      TEST(EINVAL == compare_logbuffer(&logbuf, strlen(testlog4[i]), (const uint8_t*)testlog4[i]));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int compare_header(size_t buffer_size, uint8_t buffer_addr[buffer_size], const char * funcname, const char * filename, int linenr)
{
   char     buffer[200];
   int      nr1;
   uint64_t nr2;
   uint32_t nr3;
   TEST(3 == sscanf((char*)buffer_addr, "[%d: %"SCNu64".%"SCNu32, &nr1, &nr2, &nr3));
   TEST((unsigned)nr1 == threadid_maincontext());
   struct timeval tv;
   TEST(0 == gettimeofday(&tv, 0));
   TEST((uint64_t)tv.tv_sec >= nr2);
   TEST((uint64_t)tv.tv_sec <= nr2 + 1);
   TEST(nr3 < 1000000);

   snprintf(buffer, sizeof(buffer), "[%d: %"PRIu64".%06"PRIu32"s]\n%s() %s:%d\n", nr1, nr2, nr3, funcname, filename, linenr);
   TEST(strlen(buffer) == buffer_size);
   TEST(0 == memcmp(buffer, buffer_addr, strlen(buffer)));

   return 0;
ONERR:
   return EINVAL;
}

static int thread_printheader(logbuffer_t * logbuf)
{
   logbuf->logsize = 0;
   log_header_t header = log_header_INIT("thread_printheader", __FILE__, 100);
   printheader_logbuffer(logbuf, &header);
   TEST(0 == compare_header(logbuf->logsize, logbuf->addr, "thread_printheader", __FILE__, 100));

   return 0;
ONERR:
   CLEARBUFFER_ERRLOG();
   return EINVAL;
}

static int test_update(void)
{
   logbuffer_t logbuf = logbuffer_FREE;
   thread_t *  thread = 0;
   pipe_t      pipe   = pipe_FREE;
   uint8_t     buffer[1024];
   uint8_t     readbuffer[1024+1];

   // prepare
   TEST(0 == init_pipe(&pipe));
   logbuf = (logbuffer_t) logbuffer_INIT(sizeof(buffer), buffer, pipe.write);

   // TEST truncate_logbuffer
   for (unsigned i = 0; i < 32; ++i) {
      logbuf.logsize = 32;
      logbuf.addr[i] = 'a';
      truncate_logbuffer(&logbuf, i);
      TEST(logbuf.addr == buffer);
      TEST(logbuf.size == sizeof(buffer));
      TEST(logbuf.io   == pipe.write);
      TEST(logbuf.addr[i] == 0);
      TEST(logbuf.logsize == i);
   }

   // TEST truncate_logbuffer: parameter with bigger or equal size are ignored
   for (unsigned i = 0; i < 32; ++i) {
      logbuf.logsize = i;
      logbuf.addr[i]   = 'a';
      logbuf.addr[i+1] = 'a';
      truncate_logbuffer(&logbuf, i+1);
      truncate_logbuffer(&logbuf, i);
      TEST(logbuf.addr == buffer);
      TEST(logbuf.size == sizeof(buffer));
      TEST(logbuf.io   == pipe.write);
      TEST(logbuf.addr[i]   == 'a');
      TEST(logbuf.addr[i+1] == 'a');
      TEST(logbuf.logsize   == i);
   }

   // TEST write_logbuffer
   memset(readbuffer, 0, sizeof(readbuffer));
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t)i;
   }
   logbuf.logsize = logbuf.size;
   // test
   TEST( 0 == write_logbuffer(&logbuf));
   // check logbuf
   TEST( logbuf.addr == buffer);
   TEST( logbuf.size == sizeof(buffer));
   TEST( logbuf.logsize == sizeof(buffer));
   TEST( logbuf.io   == pipe.write);
   // check content of pipe
   static_assert(sizeof(readbuffer) > sizeof(buffer), "check that only sizeof(buffer) are written");
   TEST(sizeof(buffer) == read(pipe.read, readbuffer, sizeof(readbuffer)));
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      TEST(buffer[i] == readbuffer[i]);
   }

   // TEST printheader_logbuffer
   logbuf.logsize = 0;
   log_header_t header = log_header_INIT("test_update", "file", 123456);
   printheader_logbuffer(&logbuf, &header);
   TEST(0 == compare_header(logbuf.logsize, logbuf.addr, "test_update", "file", 123456));
   for (size_t len = logbuf.logsize, i = 1; i < 10; ++i) {
      printheader_logbuffer(&logbuf, &header);
      TEST((i+1)*len == logbuf.logsize);
      TEST(0 == compare_header(len, logbuf.addr + i*len, "test_update", "file", 123456));
   }

   // TEST printheader_logbuffer: other thread
   TEST(0 == newgeneric_thread(&thread, &thread_printheader, &logbuf));
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));

   // TEST printheader_logbuffer: adds " ..." at end in case of truncated message
   logbuf.logsize = logbuf.size - 10;
   logbuf.addr[logbuf.logsize] = 0;
   printheader_logbuffer(&logbuf, &header);
   TEST(logbuf.logsize == logbuf.size - 1)
   TEST(0 == memcmp(logbuf.addr + logbuf.size - 10, "[", 1));
   TEST(0 == memcmp(logbuf.addr + logbuf.size - 5, " ...", 5));

   // TEST vprintf_logbuffer: append on already stored content
   for (unsigned i = 0; i < sizeof(buffer)-100; ++i) {
      memset(buffer, 0, sizeof(buffer));
      memset(readbuffer, 0, sizeof(readbuffer));
      logbuf.logsize = i;
      printf_logbuffer(&logbuf, "%d : %s : %c;;", i, "OK!", '0');
      snprintf((char*)readbuffer + i, 100, "%d : %s : %c;;", i, "OK!", '0');
      TEST(0 == memcmp(buffer, readbuffer, sizeof(buffer)));
   }

   // TEST vprintf_logbuffer: different formats
   logbuf.logsize = 0;
   printf_logbuffer(&logbuf, "%%%s%%", "str" );
   printf_logbuffer(&logbuf, "%"PRIi8";", (int8_t)-1);
   printf_logbuffer(&logbuf, "%"PRIu8";", (uint8_t)1);
   printf_logbuffer(&logbuf, "%"PRIi16";", (int16_t)-256);
   printf_logbuffer(&logbuf, "%"PRIu16";", (uint16_t)256);
   printf_logbuffer(&logbuf, "%"PRIi32";", (int32_t)-65536);
   printf_logbuffer(&logbuf, "%"PRIu32";", (uint32_t)65536);
   printf_logbuffer(&logbuf, "%zd;", (ssize_t)-65536);
   printf_logbuffer(&logbuf, "%zu;", (size_t)65536);
   printf_logbuffer(&logbuf, "%g;", 2e100);
   printf_logbuffer(&logbuf, "%.0f;", (double)1234567);
   const char * result = "%str%-1;1;-256;256;-65536;65536;-65536;65536;2e+100;1234567;";
   TEST(strlen(result) == logbuf.logsize);
   TEST(0 == memcmp(logbuf.addr, result, logbuf.logsize));

   // TEST vprintf_logwriter: adds " ..." at end in case of truncated message
   char strtoobig[100];
   memset(strtoobig, '1', sizeof(strtoobig));
   logbuf.logsize = logbuf.size - sizeof(strtoobig);
   logbuf.addr[logbuf.logsize] = 0;
   printf_logbuffer(&logbuf, "%.100s", strtoobig);
   TEST(logbuf.logsize == logbuf.size - 1)
   TEST(0 == memcmp(logbuf.addr + logbuf.size - sizeof(strtoobig), strtoobig, sizeof(strtoobig)-5));
   TEST(0 == memcmp(logbuf.addr + logbuf.size - 5, " ...", 5));

   // TEST vprintf_logbuffer: format == 0
   logbuf.logsize = 0;
   printf_logbuffer(&logbuf, 0);
   // nothing printed
   TEST(0 == logbuf.logsize);

   // TEST vprintf_logbuffer: sizefree_logbuffer() == 0
   logbuf.logsize = logbuf.size;
   memset(logbuf.addr, 255, logbuf.size);
   printf_logbuffer(&logbuf, "%d", 12345);
   // check logbuf not changed
   TEST(buffer == logbuf.addr);
   TEST(sizeof(buffer) == logbuf.size);
   TEST(sizeof(buffer) == logbuf.logsize);
   TEST(pipe.write     == logbuf.io);
   // check content of logbuf not changed except for " ..."
   for (size_t i = 0; i < logbuf.logsize - 5; ++i) {
      TEST(255 == logbuf.addr[i]);
   }
   TEST(0 == memcmp(logbuf.addr + logbuf.logsize - 5, " ...", 4));
   TEST(255 == logbuf.addr[logbuf.logsize-1]);

   // TEST vprintf_logbuffer: logbuffer_t.size <= 5
   for (size_t s = 5; s <= 5; --s) {
      TEST(sizeof(buffer) == logbuf.size);
      logbuf.size = s;
      logbuf.logsize = 0;
      memset(logbuf.addr, 255, s);
      printf_logbuffer(&logbuf, "%d", 12345);
      // check logbuf
      TEST(buffer == logbuf.addr);
      TEST(s      == logbuf.size);
      TEST((s?s-1:0)  == logbuf.logsize);
      TEST(pipe.write == logbuf.io);
      // check content of logbuf
      if (s == 5) {
         // " ..."
         TEST(0 == memcmp(logbuf.addr, " ...", 5));
      } else {
         // truncated 12345
         for (size_t i = 0; i < logbuf.logsize; ++i) {
            TEST(i+'1' == logbuf.addr[i]);
         }
         TEST(0 == (logbuf.size ? logbuf.addr[logbuf.size-1] : 0));
      }
      // reset
      logbuf.size = sizeof(buffer);
   }

   // unprepare
   TEST(-1 == read(pipe.read, readbuffer, sizeof(readbuffer)));
   TEST(0 == free_pipe(&pipe));

   return 0;
ONERR:
   delete_thread(&thread);
   free_pipe(&pipe);
   return EINVAL;
}

int unittest_io_writer_log_logbuffer()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
