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
#include "C-kern/api/err.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/io/writer/log/logwriter.h"
#include "C-kern/api/platform/virtmemory.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: logwriter_t

// group: types

/* typedef: struct logwriter_it
 * Export <logwriter_it>, see also <log_it>. */
typedef struct logwriter_it            logwriter_it ;

/* struct: logwriter_it
 * Generates interface with macro <log_it_DECLARE>. */
log_it_DECLARE(logwriter_it, logwriter_t)

// group: variables

/* variable: s_logwriter_interface
 * Contains single instance of interface <logwriter_it>. */
logwriter_it      s_logwriter_interface = {
                        &printf_logwriter,
                        &flushbuffer_logwriter,
                        &clearbuffer_logwriter,
                        &getbuffer_logwriter,
                  } ;

// group: init

int initthread_logwriter(/*out*/log_t * logobj)
{
   int err ;
   const size_t   objsize  = sizeof(logwriter_t) ;
   logwriter_t  * newlgwrt = (logwriter_t*) malloc(objsize) ;

   if (!newlgwrt) {
      err = ENOMEM ;
      TRACEOUTOFMEM_LOG(objsize) ;
      goto ONABORT ;
   }

   if (  logobj->object
      && logobj->object != &g_logmain) {
      err = EINVAL ;
      goto ONABORT ;
   }

   err = init_logwriter( newlgwrt ) ;
   if (err) goto ONABORT ;

   logobj->object = newlgwrt ;
   logobj->iimpl  = (log_it*) &s_logwriter_interface ;

   return 0 ;
ONABORT:
   free(newlgwrt) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int freethread_logwriter(log_t * logobj)
{
   int err ;
   logwriter_t * newlgwrt = (logwriter_t*) logobj->object ;

   if (  newlgwrt
      && newlgwrt != (logwriter_t*) &g_logmain ) {

      assert((log_it*)&s_logwriter_interface == logobj->iimpl) ;

      logobj->object = &g_logmain ;
      logobj->iimpl  = &g_logmain_interface ;

      err = free_logwriter(newlgwrt) ;

      free(newlgwrt) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: helper

/* function: allocatebuffer_logwriter
 * Reserves virtual memory for internal buffer. */
static int allocatebuffer_logwriter(/*out*/vm_block_t * buffer)
{
   int err ;
   size_t  pgsize = pagesize_vm() ;
   size_t nrpages = (8191 + pgsize) / pgsize ;

   err = init_vmblock(buffer, nrpages) ;

   return err ;
}

/* function: freebuffer_logwriter
 * Frees internal buffer. */
static int freebuffer_logwriter(vm_block_t * buffer)
{
   int err ;

   err = free_vmblock(buffer) ;

   return err ;
}

// group: lifetime

int init_logwriter(/*out*/logwriter_t * lgwrt)
{
   int err ;
   vm_block_t  buffer = vm_block_INIT_FREEABLE ;

   err = allocatebuffer_logwriter(&buffer) ;
   if (err) goto ONABORT ;

   lgwrt->buffer        = buffer ;
   lgwrt->logsize       = 0 ;

   return 0 ;
ONABORT:
   (void) freebuffer_logwriter(&buffer) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_logwriter(logwriter_t * lgwrt)
{
   int err ;

   if (lgwrt->logsize) {
      flushbuffer_logwriter(lgwrt) ;
   }

   err = freebuffer_logwriter(&lgwrt->buffer) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
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

void flushbuffer_logwriter(logwriter_t * lgwrt)
{
   size_t bytes_written = 0 ;

   do {
      ssize_t bytes ;
      do {
         bytes = write( STDERR_FILENO, lgwrt->buffer.addr + bytes_written, lgwrt->logsize - bytes_written) ;
      } while( bytes < 0 && errno == EINTR ) ;
      if (bytes <= 0) {
         // TODO: add some special log code that always works and which indicates error state in logging
         assert(errno != EAGAIN /*should be blocking i/o*/) ;
         break ;
      }
      bytes_written += (size_t) bytes ;
   } while (bytes_written < lgwrt->logsize) ;

   clearbuffer_logwriter(lgwrt) ;
}

void vprintf_logwriter(logwriter_t * lgwrt, const char * format, va_list args)
{
   size_t buffer_size = lgwrt->buffer.size - lgwrt->logsize ;

   if (buffer_size < 1 + log_PRINTF_MAXSIZE ) {
      flushbuffer_logwriter(lgwrt) ;
      buffer_size = lgwrt->buffer.size ;
   }

   int append_size = vsnprintf( (char*) (lgwrt->logsize + lgwrt->buffer.addr), buffer_size, format, args) ;

   if ( (unsigned)append_size < buffer_size ) {
      // no truncate
      lgwrt->logsize += (unsigned)append_size ;
   } else {
      lgwrt->logsize += buffer_size ;
      TRACEERR_LOG(LOG_ENTRY_TRUNCATED, append_size, (int)buffer_size-1) ;
   }
}

void printf_logwriter(logwriter_t * lgwrt, enum log_channel_e channel, const char * format, ... )
{
   va_list args ;
   va_start(args, format) ;
   if (log_channel_TEST == channel) {
      uint8_t buffer[log_PRINTF_MAXSIZE+1] ;
      int bytes = vsnprintf((char*)buffer, sizeof(buffer), format, args) ;
      if (bytes > 0) {
         unsigned ubytes = ((unsigned)bytes >= sizeof(buffer)) ? sizeof(buffer) -1 : (unsigned) bytes ;
         write_file(file_STDOUT, ubytes, buffer, 0) ;
      }
   } else {
      vprintf_logwriter(lgwrt, format, args) ;
   }
   va_end(args) ;
}


// section: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   logwriter_t lgwrt = logwriter_INIT_FREEABLE ;

   // TEST static init
   TEST(! lgwrt.buffer.addr ) ;
   TEST(! lgwrt.buffer.size ) ;
   TEST(! lgwrt.logsize ) ;

   // TEST init, double free
   lgwrt.logsize = 1 ;
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(lgwrt.buffer.addr != 0 ) ;
   TEST(lgwrt.buffer.size == 8192) ;
   TEST(lgwrt.logsize     == 0 ) ;
   TEST(0 == free_logwriter(&lgwrt)) ;
   TEST(! lgwrt.buffer.addr ) ;
   TEST(! lgwrt.buffer.size ) ;
   TEST(! lgwrt.logsize ) ;
   TEST(0 == free_logwriter(&lgwrt)) ;
   TEST(! lgwrt.buffer.addr ) ;
   TEST(! lgwrt.buffer.size ) ;
   TEST(! lgwrt.logsize ) ;

   return 0 ;
ONABORT:
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
   tempfd = openat(fd_directory(tempdir), "testlog", O_RDWR|O_CLOEXEC, 0600) ;
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
   lgwrt.logsize = lgwrt.buffer.size - 1 - log_PRINTF_MAXSIZE ;
   lgwrt.buffer.addr[lgwrt.buffer.size - 1 - log_PRINTF_MAXSIZE] = 0 ;
   lgwrt.buffer.addr[lgwrt.buffer.size - log_PRINTF_MAXSIZE] = 1 ;
   printf_logwriter(&lgwrt, log_channel_ERR, "x") ;
   TEST('x' == (char) (lgwrt.buffer.addr[lgwrt.buffer.size - 1 - log_PRINTF_MAXSIZE])) ;
   TEST(0 == lgwrt.buffer.addr[lgwrt.buffer.size - log_PRINTF_MAXSIZE]) ;
   TEST(lgwrt.logsize == lgwrt.buffer.size - log_PRINTF_MAXSIZE) ;
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST automatic flush
   TEST(0 == ftruncate(tempfd, 0)) ;
   TEST(0 == lseek(STDERR_FILENO, 0, SEEK_SET)) ;
   TEST(0 == init_logwriter(&lgwrt)) ;
   for(unsigned i = 0; i < lgwrt.buffer.size; ++i) {
      lgwrt.buffer.addr[i] = (uint8_t) (2+i) ;
   }
   lgwrt.logsize = lgwrt.buffer.size - log_PRINTF_MAXSIZE ;
   printf_logwriter(&lgwrt, log_channel_ERR, "Y") ;
   TEST(lgwrt.logsize == 1) ;
   TEST('Y' == (char) lgwrt.buffer.addr[0]) ;
   TEST(0 == lgwrt.buffer.addr[1]) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, accessmode_READ, tempdir)) ;
   TEST((lgwrt.buffer.size - log_PRINTF_MAXSIZE) == size_mmfile(&logcontent)) ;
   for(unsigned i = 0; i < lgwrt.buffer.size - log_PRINTF_MAXSIZE; ++i) {
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
   TEST(0 == free_file(&oldstderr)) ;
   TEST(0 == free_file(&tempfd)) ;
   TEST(0 == removefile_directory(tempdir, "testlog")) ;
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

   return 0 ;
ONABORT:
   free_file(&tempfd) ;
   if (oldstderr >= 0) {
      dup2(oldstderr, STDERR_FILENO) ;
      free_file(&oldstderr) ;
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

   // prepare pipe
   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK)) ;
   oldstdout = dup(STDOUT_FILENO) ;
   TEST(0 < oldstdout) ;
   TEST(STDOUT_FILENO == dup2(pipefd[1], STDOUT_FILENO)) ;
   // prepare logwriter
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(lgwrt.buffer.addr != 0 ) ;
   TEST(lgwrt.buffer.size == 8192) ;
   TEST(lgwrt.logsize     == 0 ) ;

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

   // TEST printf_logwriter (log_channel_TEST)
   char testbuffer[256] ;
   printf_logwriter(&lgwrt, log_channel_TEST, "%s", "SIMPLESTRING1\n" ) ;
   printf_logwriter(&lgwrt, log_channel_TEST, "%s", "SIMPLESTRING2\n" ) ;
   TEST(28 == read(pipefd[0], testbuffer, sizeof(testbuffer))) ;
   TEST(0 == strncmp(testbuffer, "SIMPLESTRING1\nSIMPLESTRING2\n", 28)) ;
   for(size_t i = 0; i < 510; ++i) {
      printf_logwriter(&lgwrt, log_channel_TEST, "%c", (char)(i&127)) ;
      TEST(1 == read(pipefd[0], testbuffer, sizeof(testbuffer))) ;
      TEST((i&127) == (uint8_t)testbuffer[0]) ;
   }

   // TEST printf_logwriter types
   for(log_channel_e channel = log_channel_ERR; channel <= log_channel_TEST; ++channel) {
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
   }
   const char * result = "%str%-1;1;-256;256;-65536;65536;-65536;65536;2e+100;1234567;" ;
   TEST(strlen(result) == lgwrt.logsize) ;
   TEST(strlen(result) == (unsigned)read(pipefd[0], testbuffer, sizeof(testbuffer))) ;
   TEST(0 == strncmp((char*)lgwrt.buffer.addr, result, strlen(result))) ;
   TEST(0 == strncmp(testbuffer, result, strlen(result))) ;
   clearbuffer_logwriter(&lgwrt) ;

   // unprepare
   TEST(0 == free_logwriter(&lgwrt)) ;
   dup2(oldstdout, STDOUT_FILENO) ;
   free_file(&oldstdout) ;
   free_file(&pipefd[0]) ;
   free_file(&pipefd[1]) ;

   return 0 ;
ONABORT:
   if (-1 != oldstdout) dup2(oldstdout, STDOUT_FILENO) ;
   free_file(&oldstdout) ;
   free_file(&pipefd[0]) ;
   free_file(&pipefd[1]) ;
   free_logwriter(&lgwrt) ;
   return EINVAL ;
}

static int test_initthread(void)
{
   log_t        logobj  = log_INIT_FREEABLE ;
   logwriter_t  * lgwrt = 0 ;

   // TEST static init
   TEST(0 == logobj.object) ;
   TEST(0 == logobj.iimpl) ;

   // TEST exported interface
   TEST(s_logwriter_interface.printf      == &printf_logwriter)
   TEST(s_logwriter_interface.flushbuffer == &flushbuffer_logwriter) ;
   TEST(s_logwriter_interface.clearbuffer == &clearbuffer_logwriter) ;
   TEST(s_logwriter_interface.getbuffer   == &getbuffer_logwriter) ;

   // TEST init, double free (logobj.object = 0)
   TEST(0 == initthread_logwriter(&logobj)) ;
   TEST(logobj.object) ;
   TEST(logobj.object != &g_logmain) ;
   TEST(logobj.iimpl  == (log_it*) &s_logwriter_interface) ;
   lgwrt = (logwriter_t*) logobj.object ;
   TEST(lgwrt->buffer.addr) ;
   TEST(lgwrt->buffer.size) ;
   TEST(0 == freethread_logwriter(&logobj)) ;
   TEST(logobj.object == &g_logmain) ;
   TEST(logobj.iimpl  == &g_logmain_interface) ;
   TEST(0 == freethread_logwriter(&logobj)) ;
   TEST(logobj.object == &g_logmain) ;
   TEST(logobj.iimpl  == &g_logmain_interface) ;
   lgwrt = 0 ;

   // TEST init, double free (logobj.object = &g_logmain)
   logobj.object = &g_logmain ;
   TEST(0 == initthread_logwriter(&logobj)) ;
   TEST(logobj.object) ;
   TEST(logobj.object != &g_logmain) ;
   TEST(logobj.iimpl  == (log_it*) &s_logwriter_interface) ;
   lgwrt = (logwriter_t*) logobj.object ;
   TEST(lgwrt->buffer.addr) ;
   TEST(lgwrt->buffer.size) ;
   TEST(0 == freethread_logwriter(&logobj)) ;
   TEST(logobj.object == &g_logmain) ;
   TEST(logobj.iimpl  == &g_logmain_interface) ;
   TEST(0 == freethread_logwriter(&logobj)) ;
   TEST(logobj.object == &g_logmain) ;
   TEST(logobj.iimpl  == &g_logmain_interface) ;
   lgwrt = 0 ;

   // TEST free (logobj.object = 0)
   logobj.object = 0 ;
   TEST(0 == freethread_logwriter(&logobj)) ;
   TEST(0 == logobj.object) ;

   // TEST EINVAL
   logobj.object = (logwriter_t*) 1 ;
   TEST(EINVAL == initthread_logwriter(&logobj)) ;
   TEST(logobj.object == (logwriter_t*) 1) ;

   return 0 ;
ONABORT:
   freethread_logwriter(&logobj) ;
   return EINVAL ;
}

int unittest_io_writer_log_logwriter()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_flushbuffer())    goto ONABORT ;
   if (test_printf())         goto ONABORT ;
   if (test_initthread())     goto ONABORT ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;}

#endif
