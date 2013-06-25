/* title: LogWriter impl
   Implements logging of error messages to standard error channel.
   See <LogWriter>.

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

   file: C-kern/api/io/writer/log/logwriter.h
    Header file of <LogWriter>.

   file: C-kern/io/writer/log/logwriter.c
    Implementation file <LogWriter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/writer/log/logwriter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: logwriter_t

// group: types

/* typedef: logwriter_it
 * Defines interface for <logwriter_t> - see <log_it_DECLARE>. */
log_it_DECLARE(logwriter_it, logwriter_t)

// group: variables

/* variable: s_logwriter_interface
 * Contains single instance of interface <logwriter_it>. */
logwriter_it      s_logwriter_interface = {
                        &printf_logwriter,
                        &flushbuffer_logwriter,
                        &clearbuffer_logwriter,
                        &getbuffer_logwriter
                  } ;

// group: initthread

struct log_it * interface_logwriter(void)
{
   return genericcast_logit(&s_logwriter_interface, logwriter_t) ;
}

// group: helper

/* function: allocatebuffer_logwriter
 * Reserves some memory pages for internal buffer. */
static int allocatebuffer_logwriter(/*out*/memblock_t * buffer)
{
   static_assert(16384 <= INT_MAX, "size_t of buffer will be used in vnsprintf and returned as int") ;
   static_assert(16384 >  2*log_config_MAXSIZE, "buffer can hold at least 2 entries") ;
   return ALLOC_PAGECACHE(pagesize_16384, buffer) ;
}

/* function: freebuffer_logwriter
 * Frees internal buffer. */
static int freebuffer_logwriter(memblock_t * buffer)
{
   return RELEASE_PAGECACHE(buffer) ;
}

// group: lifetime

int init_logwriter(/*out*/logwriter_t * lgwrt)
{
   int err ;
   memblock_t  buffer ;

   err = allocatebuffer_logwriter(&buffer) ;
   if (err) goto ONABORT ;

   *genericcast_memblock(&lgwrt->buffer,) = buffer ;
   lgwrt->logsize = 0 ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_logwriter(logwriter_t * lgwrt)
{
   int err ;

   if (lgwrt->logsize) {
      flushbuffer_logwriter(lgwrt) ;
   }

   err = freebuffer_logwriter(genericcast_memblock(&lgwrt->buffer,)) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

void getbuffer_logwriter(logwriter_t * lgwrt, /*out*/char ** buffer, /*out*/size_t * size)
{
   *buffer = (char*) lgwrt->buffer.addr ;
   *size   = lgwrt->logsize ;
}

// group: change

void clearbuffer_logwriter(logwriter_t * lgwrt)
{
   lgwrt->logsize        = 0 ;
   // NULL terminated string
   lgwrt->buffer.addr[0] = 0 ;
}

static void write_logwriter(int fd, size_t buffer_size, uint8_t buffer[buffer_size])
{
   size_t bytes_written = 0 ;

   // TODO: add syncthread which does logging in background !
   //       first add global io queue where I/Os could be registered
   //       create new buffer and transfer full buffer to I/O queue

   while (bytes_written < buffer_size) {
      ssize_t bytes ;
      do {
         bytes = write(fd, buffer + bytes_written, buffer_size - bytes_written) ;
      } while (bytes < 0 && errno == EINTR) ;

      if (bytes == 0) {
         // TODO: add code which indicates log channel has been closed
         break ;  // ignore closed
      } else if (bytes < 0) {
         if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // TODO: add code that indicates error state in logging
            break ;  // ignore error
         }
         /* should be blocking i/o */
      } else {
         bytes_written += (size_t) bytes;
      }
   }
}

void flushbuffer_logwriter(logwriter_t * lgwrt)
{
   write_logwriter(STDERR_FILENO, lgwrt->logsize, lgwrt->buffer.addr) ;
   clearbuffer_logwriter(lgwrt) ;
}

void vprintf_logwriter(logwriter_t * lgwrt, uint8_t channel, const char * format, va_list args)
{
   size_t buffer_size = lgwrt->buffer.size - lgwrt->logsize ;

   if (buffer_size <= log_config_MAXSIZE) {
      // ensure log_config_MAXSIZE bytes for message + \0 byte
      flushbuffer_logwriter(lgwrt) ;
      // flushbuffer_logwriter calls clearbuffer_logwriter => lgwrt->logsize == 0
      buffer_size = lgwrt->buffer.size ;
   }

   uint8_t * buffer = lgwrt->buffer.addr + lgwrt->logsize ;

   int bytes = vsnprintf((char*)buffer, buffer_size, format, args) ;

   if (bytes > 0) {
      size_t append_size = (size_t)bytes ;

      if (append_size >= buffer_size) {
         // data has been truncated => mark it with " ..."
         append_size = buffer_size-1 ;
         static_assert(log_config_MAXSIZE > 4, "") ;
         memcpy(buffer + append_size-4, " ...", 4) ;
      }

      if (log_channel_CONSOLE == channel) {
         write_logwriter(STDOUT_FILENO, append_size, buffer) ;
         buffer[0] = 0 ;

      } else {
         // keep in buffer
         lgwrt->logsize += append_size ;
      }
   }
}

void printf_logwriter(logwriter_t * lgwrt, uint8_t channel, const char * format, ...)
{
   va_list args ;
   va_start(args, format) ;
   vprintf_logwriter(lgwrt, channel, format, args) ;
   va_end(args) ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   logwriter_t lgwrt = logwriter_INIT_FREEABLE ;

   // TEST logwriter_INIT_FREEABLE
   TEST(0 == lgwrt.buffer.addr) ;
   TEST(0 == lgwrt.buffer.size) ;
   TEST(0 == lgwrt.logsize) ;

   // TEST logwriter_INIT_TEMP
   logwriter_t lgwrt2 = logwriter_INIT_TEMP(100, (uint8_t*)29) ;
   TEST(lgwrt2.buffer.addr == (uint8_t*)29) ;
   TEST(lgwrt2.buffer.size == 100) ;
   TEST(lgwrt2.logsize     == 0) ;

   // TEST init, double free
   lgwrt.logsize = 1 ;
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(lgwrt.buffer.addr != 0 ) ;
   TEST(lgwrt.buffer.size == 16384) ;
   TEST(lgwrt.logsize     == 0 ) ;
   TEST(0 == free_logwriter(&lgwrt)) ;
   TEST(0 == lgwrt.buffer.addr) ;
   TEST(0 == lgwrt.buffer.size) ;
   TEST(0 == lgwrt.logsize) ;
   TEST(0 == free_logwriter(&lgwrt)) ;
   TEST(0 == lgwrt.buffer.addr) ;
   TEST(0 == lgwrt.buffer.size) ;
   TEST(0 == lgwrt.logsize) ;

   return 0 ;
ONABORT:
   free_logwriter(&lgwrt) ;
   return EINVAL ;
}

static int test_query(void)
{
   logwriter_t lgwrt = logwriter_INIT_FREEABLE ;

   // prepare
   TEST(0 == init_logwriter(&lgwrt)) ;

   // TEST getbuffer_logwriter
   TEST(5 == snprintf((char*)lgwrt.buffer.addr, lgwrt.buffer.size, "12345")) ;
   lgwrt.logsize = 5 ;
   char  *  logstr  = 0 ;
   size_t   logsize = 0 ;
   getbuffer_logwriter(&lgwrt, &logstr, &logsize) ;
   TEST(logstr  == (char*)lgwrt.buffer.addr) ;
   TEST(logsize == 5) ;
   lgwrt.logsize = 0 ;
   printf_logwriter(&lgwrt, log_channel_ERR, "%s", "abcdef") ;
   getbuffer_logwriter(&lgwrt, &logstr, &logsize) ;
   TEST(logstr  == (char*)lgwrt.buffer.addr) ;
   TEST(logsize == 6) ;
   lgwrt.logsize = 0 ;

   // unprepare
   TEST(0 == free_logwriter(&lgwrt)) ;

   return 0 ;
ONABORT:
   lgwrt.logsize = 0 ;
   free_logwriter(&lgwrt) ;
   return EINVAL ;
}

static int test_flushbuffer(void)
{
   logwriter_t    lgwrt      = logwriter_INIT_FREEABLE ;
   int            tempfd     = -1 ;
   int            oldstderr  = -1 ;
   mmfile_t       logcontent = mmfile_INIT_FREEABLE ;
   cstring_t      tmppath    = cstring_INIT ;
   directory_t  * tempdir    = 0 ;

   // prepare logfile
   TEST(0 == newtemp_directory(&tempdir, "tempdir", &tmppath)) ;
   TEST(0 == makefile_directory(tempdir, "testlog", 0)) ;
   tempfd = openat(io_directory(tempdir), "testlog", O_RDWR|O_CLOEXEC, 0600) ;
   TEST(0 < tempfd) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 < oldstderr) ;
   TEST(STDERR_FILENO == dup2(tempfd, STDERR_FILENO)) ;

   // TEST flush
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(lgwrt.logsize     == 0) ;
   TEST(lgwrt.buffer.size != 0) ;
   for(unsigned i = 0; i < lgwrt.buffer.size; ++i) {
      lgwrt.buffer.addr[i] = (uint8_t) (1+i) ;
   }
   lgwrt.logsize = lgwrt.buffer.size ;
   TEST(lgwrt.buffer.addr[0] == 1) ;
   flushbuffer_logwriter(&lgwrt) ;
   TEST(lgwrt.logsize        == 0) ;
   TEST(lgwrt.buffer.addr[0] == 0) ; // NULL byte
   TEST(0 == free_logwriter(&lgwrt)) ;

   // Test flushed content
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(lgwrt.logsize     == 0) ;
   TEST(lgwrt.buffer.size != 0) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, accessmode_READ, tempdir)) ;
   TEST(lgwrt.buffer.size == size_mmfile(&logcontent)) ;
   for(unsigned i = 0; i < lgwrt.buffer.size; ++i) {
      TEST(addr_mmfile(&logcontent)[i] == (uint8_t) (1+i)) ;
   }
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST no automatic flush
   TEST(0 == init_logwriter(&lgwrt)) ;
   lgwrt.logsize = lgwrt.buffer.size - 1 - log_config_MAXSIZE ;
   lgwrt.buffer.addr[lgwrt.buffer.size - 1 - log_config_MAXSIZE] = 0 ;
   lgwrt.buffer.addr[lgwrt.buffer.size - log_config_MAXSIZE] = 1 ;
   printf_logwriter(&lgwrt, log_channel_ERR, "x") ;
   TEST('x' == (char) (lgwrt.buffer.addr[lgwrt.buffer.size - 1 - log_config_MAXSIZE])) ;
   TEST(0 == lgwrt.buffer.addr[lgwrt.buffer.size - log_config_MAXSIZE]) ;
   TEST(lgwrt.logsize == lgwrt.buffer.size - log_config_MAXSIZE) ;
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST automatic flush
   TEST(0 == ftruncate(tempfd, 0)) ;
   TEST(0 == lseek(STDERR_FILENO, 0, SEEK_SET)) ;
   TEST(0 == init_logwriter(&lgwrt)) ;
   for(unsigned i = 0; i < lgwrt.buffer.size; ++i) {
      lgwrt.buffer.addr[i] = (uint8_t) (2+i) ;
   }
   lgwrt.logsize = lgwrt.buffer.size - log_config_MAXSIZE ;
   printf_logwriter(&lgwrt, log_channel_ERR, "Y") ;
   TEST(lgwrt.logsize == 1) ;
   TEST('Y' == (char) lgwrt.buffer.addr[0]) ;
   TEST(0 == lgwrt.buffer.addr[1]) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, accessmode_READ, tempdir)) ;
   TEST((lgwrt.buffer.size - log_config_MAXSIZE) == size_mmfile(&logcontent)) ;
   for(unsigned i = 0; i < lgwrt.buffer.size - log_config_MAXSIZE; ++i) {
      TEST(addr_mmfile(&logcontent)[i] == (uint8_t) (2+i)) ;
   }
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST free calls flush
   TEST(0 == ftruncate(tempfd, 0)) ;
   TEST(0 == lseek(STDERR_FILENO, 0, SEEK_SET)) ;
   TEST(0 == init_logwriter(&lgwrt)) ;
   for(unsigned i = 0; i < lgwrt.buffer.size; ++i) {
      lgwrt.buffer.addr[i] = (uint8_t) (3+i) ;
   }
   lgwrt.logsize = lgwrt.buffer.size ;
   TEST(0 == free_logwriter(&lgwrt)) ; // call flush
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, accessmode_READ, tempdir)) ;
   TEST(lgwrt.buffer.size == size_mmfile(&logcontent)) ;
   for(unsigned i = 0; i < lgwrt.buffer.size; ++i) {
      TEST(addr_mmfile(&logcontent)[i] == (uint8_t) (3+i)) ;
   }
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == free_logwriter(&lgwrt)) ;

   // unprepare/free logfile
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&oldstderr)) ;
   TEST(0 == free_iochannel(&tempfd)) ;
   TEST(0 == removefile_directory(tempdir, "testlog")) ;
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

   return 0 ;
ONABORT:
   free_iochannel(&tempfd) ;
   if (oldstderr >= 0) {
      dup2(oldstderr, STDERR_FILENO) ;
      free_iochannel(&oldstderr) ;
   }
   free_mmfile(&logcontent) ;
   delete_directory(&tempdir) ;
   free_cstring(&tmppath) ;
   free_logwriter(&lgwrt) ;
   return EINVAL ;
}

static int test_printf(void)
{
   logwriter_t    lgwrt     = logwriter_INIT_FREEABLE ;
   int            pipefd[2] = { -1, -1 } ;
   int            oldstdout = -1 ;

   // prepare
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK)) ;
   oldstdout = dup(STDOUT_FILENO) ;
   TEST(0 < oldstdout) ;
   TEST(STDOUT_FILENO == dup2(pipefd[1], STDOUT_FILENO)) ;

   // TEST printf_logwriter (log_channel_ERR)
   printf_logwriter(&lgwrt, log_channel_ERR, "%s", "TESTSTRT\n" ) ;
   printf_logwriter(&lgwrt, log_channel_ERR, "%s", "TESTENDE\n" ) ;
   TEST(18 == lgwrt.logsize) ;
   TEST(0 == strcmp((char*)lgwrt.buffer.addr, "TESTSTRT\nTESTENDE\n")) ;
   for(size_t i = 0; i < 510; ++i) {
      TEST(18 + i == lgwrt.logsize) ;
      printf_logwriter(&lgwrt, log_channel_ERR, "%c", (char)(i&127) ) ;
      TEST(19 + i == lgwrt.logsize) ;
   }
   TEST(0 == strncmp((char*)lgwrt.buffer.addr, "TESTSTRT\nTESTENDE\n", 18)) ;
   for(size_t i = 0; i < 510; ++i) {
      TEST((i&127) == lgwrt.buffer.addr[18+i]) ;
   }
   clearbuffer_logwriter(&lgwrt) ;

   // TEST printf_logwriter: log_channel_CONSOLE
   char testbuffer[log_config_MAXSIZE+2] ;
   printf_logwriter(&lgwrt, log_channel_CONSOLE, "%s", "SIMPLESTRING1\n" ) ;
   printf_logwriter(&lgwrt, log_channel_CONSOLE, "%s", "SIMPLESTRING2\n" ) ;
   TEST(0 == lgwrt.logsize) ;
   TEST(0 == lgwrt.buffer.addr[0]) ;
   TEST(28 == read(pipefd[0], testbuffer, sizeof(testbuffer))) ;
   TEST(0 == strncmp(testbuffer, "SIMPLESTRING1\nSIMPLESTRING2\n", 28)) ;
   for(size_t i = 0; i < 510; ++i) {
      printf_logwriter(&lgwrt, log_channel_CONSOLE, "%c", (char)(i&127)) ;
      TEST(1 == read(pipefd[0], testbuffer, sizeof(testbuffer))) ;
      TEST((i&127) == (uint8_t)testbuffer[0]) ;
   }

   // TEST printf_logwriter: different format types
   for(log_channel_e channel = 0; channel < log_channel_NROFCHANNEL; ++channel) {
      printf_logwriter(&lgwrt, channel, "%%%s%%", "str" ) ;
      printf_logwriter(&lgwrt, channel, "%"PRIi8";", (int8_t)-1) ;
      printf_logwriter(&lgwrt, channel, "%"PRIu8";", (uint8_t)1) ;
      printf_logwriter(&lgwrt, channel, "%"PRIi16";", (int16_t)-256) ;
      printf_logwriter(&lgwrt, channel, "%"PRIu16";", (uint16_t)256) ;
      printf_logwriter(&lgwrt, channel, "%"PRIi32";", (int32_t)-65536) ;
      printf_logwriter(&lgwrt, channel, "%"PRIu32";", (uint32_t)65536) ;
      printf_logwriter(&lgwrt, channel, "%zd;", (ssize_t)-65536) ;
      printf_logwriter(&lgwrt, channel, "%zu;", (size_t)65536) ;
      printf_logwriter(&lgwrt, channel, "%g;", 2e100) ;
      printf_logwriter(&lgwrt, channel, "%.0f;", (double)1234567) ;
      const char * result = "%str%-1;1;-256;256;-65536;65536;-65536;65536;2e+100;1234567;" ;
      if (channel == log_channel_CONSOLE) {
         TEST(strlen(result)   == (unsigned)read(pipefd[0], testbuffer, sizeof(testbuffer))) ;
         TEST(0 == strncmp(testbuffer, result, strlen(result))) ;
      } else {
         TEST(strlen(result) == lgwrt.logsize) ;
         TEST(0 == strncmp((char*)lgwrt.buffer.addr, result, strlen(result))) ;
         clearbuffer_logwriter(&lgwrt) ;
      }
   }

   // TEST printf_logwriter: buffer overflow ; adds " ..." at end
   for(log_channel_e channel = 0; channel < log_channel_NROFCHANNEL; ++channel) {
      char strtoobig[log_config_MAXSIZE+2] ;
      memset(strtoobig, '1', sizeof(strtoobig)-1) ;
      strtoobig[sizeof(strtoobig)-1] = 0 ;
      lgwrt.logsize = lgwrt.buffer.size - log_config_MAXSIZE -1 ;
      lgwrt.buffer.addr[lgwrt.logsize] = 0 ;
      printf_logwriter(&lgwrt, channel, "%s", strtoobig) ;

      if (channel == log_channel_CONSOLE) {
         TEST(lgwrt.logsize == lgwrt.buffer.size - log_config_MAXSIZE -1) ;
         TEST(lgwrt.buffer.addr[lgwrt.logsize] == 0) ;
         TEST(log_config_MAXSIZE == (unsigned)read(pipefd[0], testbuffer, sizeof(testbuffer))) ;
         TEST(0 == memcmp(testbuffer, strtoobig, log_config_MAXSIZE-4)) ;
         TEST(0 == memcmp(testbuffer + log_config_MAXSIZE-4, " ...", 4)) ;

      } else {    // log_channel_TEST ... log_channel_ERR
         TEST(lgwrt.logsize == lgwrt.buffer.size - 1) ;
         TEST(0 == memcmp(lgwrt.buffer.addr + lgwrt.buffer.size - log_config_MAXSIZE -1, strtoobig, log_config_MAXSIZE-4)) ;
         TEST(0 == memcmp(lgwrt.buffer.addr + lgwrt.buffer.size - 5, " ...", 4)) ;
         TEST(0 == lgwrt.buffer.addr[lgwrt.buffer.size-1]) ;
      }
   }

   // unprepare
   clearbuffer_logwriter(&lgwrt) ;
   TEST(0 == free_logwriter(&lgwrt)) ;
   dup2(oldstdout, STDOUT_FILENO) ;
   free_iochannel(&oldstdout) ;
   free_iochannel(&pipefd[0]) ;
   free_iochannel(&pipefd[1]) ;

   return 0 ;
ONABORT:
   if (-1 != oldstdout) dup2(oldstdout, STDOUT_FILENO) ;
   free_iochannel(&oldstdout) ;
   free_iochannel(&pipefd[0]) ;
   free_iochannel(&pipefd[1]) ;
   free_logwriter(&lgwrt) ;
   return EINVAL ;
}

static int test_initthread(void)
{
   // TEST genericcast_logit
   TEST(genericcast_logit(&s_logwriter_interface, logwriter_t) == (log_it*)&s_logwriter_interface) ;

   // TEST s_logwriter_interface
   TEST(s_logwriter_interface.printf      == &printf_logwriter)
   TEST(s_logwriter_interface.flushbuffer == &flushbuffer_logwriter) ;
   TEST(s_logwriter_interface.clearbuffer == &clearbuffer_logwriter) ;
   TEST(s_logwriter_interface.getbuffer   == &getbuffer_logwriter) ;

   // TEST interface_logwriter
   TEST(interface_logwriter() == genericcast_logit(&s_logwriter_interface, logwriter_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_io_writer_log_logwriter()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_flushbuffer())    goto ONABORT ;
   if (test_printf())         goto ONABORT ;
   if (test_initthread())     goto ONABORT ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
