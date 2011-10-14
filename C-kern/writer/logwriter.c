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

   file: C-kern/api/writer/logwriter.h
    Header file of <LogWriter>.

   file: C-kern/writer/logwriter.c
    Implementation file <LogWriter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/writer/logwriter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/os/virtmemory.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/os/filesystem/mmfile.h"
#endif

// forward

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

int init_logwriter(/*out*/logwriter_t * log)
{
   int err ;
   vm_block_t  buffer = vm_block_INIT_FREEABLE ;

   err = allocatebuffer_logwriter(&buffer) ;
   if (err) goto ABBRUCH ;

   log->buffer        = buffer ;
   log->logsize       = 0 ;

   return 0 ;
ABBRUCH:
   (void) freebuffer_logwriter(&buffer) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_logwriter(logwriter_t * log)
{
   int err ;

   if (log->logsize) {
      flushbuffer_logwriter(log) ;
   }

   err = freebuffer_logwriter(&log->buffer) ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: query

void getbuffer_logwriter(logwriter_t * log, /*out*/char ** buffer, /*out*/size_t * size)
{
   *buffer = (char*) log->buffer.addr ;
   *size   = log->logsize ;
}

// group: change

void clearbuffer_logwriter(logwriter_t * log)
{
   log->logsize        = 0 ;
   // NULL terminated string
   log->buffer.addr[0] = 0 ;
}

void flushbuffer_logwriter(logwriter_t * log)
{
   size_t bytes_written = 0 ;

   do {
      ssize_t bytes ;
      do {
         bytes = write( STDERR_FILENO, log->buffer.addr + bytes_written, log->logsize - bytes_written) ;
      } while( bytes < 0 && errno == EINTR ) ;
      if (bytes <= 0) {
         // TODO: add some special log code that always works and which indicates error state in logging
         assert(errno != EAGAIN /*should be blocking i/o*/) ;
         break ;
      }
      bytes_written += (size_t) bytes ;
   } while (bytes_written < log->logsize) ;

   clearbuffer_logwriter(log) ;
}

void vprintf_logwriter(logwriter_t * log, const char * format, va_list args)
{
   size_t buffer_size = log->buffer.size - log->logsize ;

   if (buffer_size < 1 + log_PRINTF_MAXSIZE ) {
      flushbuffer_logwriter(log) ;
      buffer_size = log->buffer.size ;
   }

   int append_size = vsnprintf( (char*) (log->logsize + log->buffer.addr), buffer_size, format, args) ;

   if ( (unsigned)append_size < buffer_size ) {
      // no truncate
      log->logsize += (unsigned)append_size ;
   } else {
      log->logsize += buffer_size ;
      LOG_ERRTEXT(LOG_ENTRY_TRUNCATED(append_size, buffer_size-1)) ;
   }
}

void printf_logwriter(logwriter_t * log, const char * format, ... )
{
   va_list args ;
   va_start(args, format) ;
   vprintf_logwriter(log, format, args) ;
   va_end(args) ;
}


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_writer_logwriter,ABBRUCH)

static int test_initfree(void)
{
   logwriter_t log = logwriter_INIT_FREEABLE ;

   // TEST static init
   TEST(! log.buffer.addr ) ;
   TEST(! log.buffer.size ) ;
   TEST(! log.logsize ) ;

   // TEST init, double free
   log.logsize = 1 ;
   TEST(0 == init_logwriter(&log)) ;
   TEST(log.buffer.addr != 0 ) ;
   TEST(log.buffer.size == 8192) ;
   TEST(log.logsize     == 0 ) ;
   TEST(0 == free_logwriter(&log)) ;
   TEST(! log.buffer.addr ) ;
   TEST(! log.buffer.size ) ;
   TEST(! log.logsize ) ;
   TEST(0 == free_logwriter(&log)) ;
   TEST(! log.buffer.addr ) ;
   TEST(! log.buffer.size ) ;
   TEST(! log.logsize ) ;

   return 0 ;
ABBRUCH:
   free_logwriter(&log) ;
   return EINVAL ;
}

static int test_flushbuffer(void)
{
   logwriter_t          log        = logwriter_INIT_FREEABLE ;
   int                  tempfd     = -1 ;
   int                  oldstderr  = -1 ;
   mmfile_t             logcontent = mmfile_INIT_FREEABLE ;
   directory_stream_t   tempdir    = directory_stream_INIT_FREEABLE ;

   // prepare logfile
   TEST(0 == inittemp_directorystream(&tempdir, "tempdir")) ;
   TEST(0 == makefile_directorystream(&tempdir, "testlog")) ;
   tempfd = openat(dirfd(tempdir.sys_dir), "testlog", O_RDWR|O_CLOEXEC, 0600) ;
   TEST(0 < tempfd) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 < oldstderr) ;
   TEST(STDERR_FILENO == dup2(tempfd, STDERR_FILENO)) ;

   // TEST flush
   TEST(0 == init_logwriter(&log)) ;
   TEST(log.logsize     == 0) ;
   TEST(log.buffer.size != 0) ;
   for(unsigned i = 0; i < log.buffer.size; ++i) {
      log.buffer.addr[i] = (uint8_t) (1+i) ;
   }
   log.logsize = log.buffer.size ;
   TEST(log.buffer.addr[0] = 1) ;
   flushbuffer_logwriter(&log) ;
   TEST(log.logsize        == 0) ;
   TEST(log.buffer.addr[0] == 0) ; // NULL byte
   TEST(0 == free_logwriter(&log)) ;

   // Test flushed content
   TEST(0 == init_logwriter(&log)) ;
   TEST(log.logsize     == 0) ;
   TEST(log.buffer.size != 0) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
   TEST(log.buffer.size == size_mmfile(&logcontent)) ;
   for(unsigned i = 0; i < log.buffer.size; ++i) {
      TEST(addr_mmfile(&logcontent)[i] == (uint8_t) (1+i)) ;
   }
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == free_logwriter(&log)) ;

   // TEST no automatic flush
   TEST(0 == init_logwriter(&log)) ;
   log.logsize = log.buffer.size - 1 - log_PRINTF_MAXSIZE ;
   log.buffer.addr[log.buffer.size - 1 - log_PRINTF_MAXSIZE] = 0 ;
   log.buffer.addr[log.buffer.size - log_PRINTF_MAXSIZE] = 1 ;
   printf_logwriter(&log, "x") ;
   TEST('x' == (char) (log.buffer.addr[log.buffer.size - 1 - log_PRINTF_MAXSIZE])) ;
   TEST(0 == log.buffer.addr[log.buffer.size - log_PRINTF_MAXSIZE]) ;
   TEST(log.logsize == log.buffer.size - log_PRINTF_MAXSIZE) ;
   TEST(0 == free_logwriter(&log)) ;

   // TEST automatic flush
   TEST(0 == ftruncate(tempfd, 0)) ;
   TEST(0 == lseek(STDERR_FILENO, 0, SEEK_SET)) ;
   TEST(0 == init_logwriter(&log)) ;
   for(unsigned i = 0; i < log.buffer.size; ++i) {
      log.buffer.addr[i] = (uint8_t) (2+i) ;
   }
   log.logsize = log.buffer.size - log_PRINTF_MAXSIZE ;
   printf_logwriter(&log, "Y") ;
   TEST(log.logsize == 1) ;
   TEST('Y' == (char) log.buffer.addr[0]) ;
   TEST(0 == log.buffer.addr[1]) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
   TEST((log.buffer.size - log_PRINTF_MAXSIZE) == size_mmfile(&logcontent)) ;
   for(unsigned i = 0; i < log.buffer.size - log_PRINTF_MAXSIZE; ++i) {
      TEST(addr_mmfile(&logcontent)[i] == (uint8_t) (2+i)) ;
   }
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == free_logwriter(&log)) ;

   // TEST free calls flush
   TEST(0 == ftruncate(tempfd, 0)) ;
   TEST(0 == lseek(STDERR_FILENO, 0, SEEK_SET)) ;
   TEST(0 == init_logwriter(&log)) ;
   for(unsigned i = 0; i < log.buffer.size; ++i) {
      log.buffer.addr[i] = (uint8_t) (3+i) ;
   }
   log.logsize = log.buffer.size ;
   TEST(0 == free_logwriter(&log)) ; // call flush
   TEST(0 == init_logwriter(&log)) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
   TEST(log.buffer.size == size_mmfile(&logcontent)) ;
   for(unsigned i = 0; i < log.buffer.size; ++i) {
      TEST(addr_mmfile(&logcontent)[i] == (uint8_t) (3+i)) ;
   }
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == free_logwriter(&log)) ;

   // unprepare/free logfile
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == close(oldstderr)) ;
   oldstderr = -1 ;
   TEST(0 == close(tempfd)) ;
   tempfd = -1 ;
   TEST(0 == removefile_directorystream(&tempdir, "testlog")) ;
   TEST(0 == remove_directorystream(&tempdir)) ;
   TEST(0 == free_directorystream(&tempdir)) ;

   return 0 ;
ABBRUCH:
   if (tempfd >= 0) {
      close(tempfd) ;
      removefile_directorystream(&tempdir, "testlog") ;
      remove_directorystream(&tempdir) ;
   }
   if (oldstderr >= 0) {
      dup2(oldstderr, STDERR_FILENO) ;
      close(oldstderr) ;
   }
   free_mmfile(&logcontent) ;
   free_directorystream(&tempdir) ;
   free_logwriter(&log) ;
   return EINVAL ;
}

static int test_printf(void)
{
   logwriter_t    log = logwriter_INIT_FREEABLE ;

   // TEST init
   TEST(0 == init_logwriter(&log)) ;
   TEST(log.buffer.addr != 0 ) ;
   TEST(log.buffer.size == 8192) ;
   TEST(log.logsize     == 0 ) ;

   // TEST printf_logwriter
   printf_logwriter(&log, "%s", "TESTSTRT\n" ) ;
   printf_logwriter(&log, "%s", "TESTENDE\n" ) ;
   TEST(18 == log.logsize) ;
   TEST(0 == strcmp((char*)log.buffer.addr, "TESTSTRT\nTESTENDE\n")) ;
   for(size_t i = 0; i < 510; ++i) {
      TEST(18 + i == log.logsize) ;
      printf_logwriter(&log, "%c", 'F' ) ;
      TEST(19 + i == log.logsize) ;
   }
   TEST(0 == strncmp((char*)log.buffer.addr, "TESTSTRT\nTESTENDE\n", 18)) ;
   for(size_t i = 0; i < 510; ++i) {
      TEST('F' == (char)log.buffer.addr[18+i]) ;
   }

   clearbuffer_logwriter(&log) ;
   TEST(0 == free_logwriter(&log)) ;

   return 0 ;
ABBRUCH:
   free_logwriter(&log) ;
   return EINVAL ;
}

int unittest_writer_logwriter()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_flushbuffer())    goto ABBRUCH ;
   if (test_printf())         goto ABBRUCH ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;}

#endif
