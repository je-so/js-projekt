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
#include "C-kern/api/platform/virtmemory.h"
#include "C-kern/api/writer/main_logwriter.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/string/cstring.h"
#endif

// section: logwriter_t

// group: types

/* typedef: logwriter_it
 * Define interface <logwriter_it>, see also <ilog_it>.
 * The interface is generated with the macro <log_it_DECLARE>. */
log_it_DECLARE(1, logwriter_it, logwriter_t)

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

int initumgebung_logwriter(/*out*/log_oit * ilog)
{
   int err ;
   const size_t   objsize = sizeof(logwriter_t) ;
   logwriter_t  * log2    = (logwriter_t*) malloc(objsize) ;

   if (!log2) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(objsize) ;
      goto ABBRUCH ;
   }

   if (  ilog->object
      && ilog->object != &g_main_logwriter) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   err = init_logwriter( log2 ) ;
   if (err) goto ABBRUCH ;

   ilog->object    = log2 ;
   ilog->functable = (log_it*) &s_logwriter_interface ;

   return 0 ;
ABBRUCH:
   free(log2) ;
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_logwriter(log_oit * ilog)
{
   int err ;
   logwriter_t * log2 = (logwriter_t*) ilog->object ;

   if (  log2
      && log2 != (logwriter_t*) &g_main_logwriter ) {

      assert((void*)log2 != (void*)&g_main_logwriter) ;
      assert((log_it*)&s_logwriter_interface == ilog->functable) ;

      ilog->object    = &g_main_logwriter ;
      ilog->functable = (log_it*) &g_main_logwriter_interface ;

      err = free_logwriter( log2 ) ;

      free(log2) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
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

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION, ABBRUCH)

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
   logwriter_t    log        = logwriter_INIT_FREEABLE ;
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
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, mmfile_openmode_RDONLY, tempdir)) ;
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
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, mmfile_openmode_RDONLY, tempdir)) ;
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
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, mmfile_openmode_RDONLY, tempdir)) ;
   TEST(log.buffer.size == size_mmfile(&logcontent)) ;
   for(unsigned i = 0; i < log.buffer.size; ++i) {
      TEST(addr_mmfile(&logcontent)[i] == (uint8_t) (3+i)) ;
   }
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == free_logwriter(&log)) ;

   // unprepare/free logfile
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == free_filedescr(&oldstderr)) ;
   TEST(0 == free_filedescr(&tempfd)) ;
   TEST(0 == removefile_directory(tempdir, "testlog")) ;
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == delete_directory(&tempdir)) ;

   return 0 ;
ABBRUCH:
   free_filedescr(&tempfd) ;
   if (oldstderr >= 0) {
      dup2(oldstderr, STDERR_FILENO) ;
      free_filedescr(&oldstderr) ;
   }
   free_mmfile(&logcontent) ;
   delete_directory(&tempdir) ;
   free_cstring(&tmppath) ;
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

static int test_initumgebung(void)
{
   log_oit         ilog = log_oit_INIT_FREEABLE ;
   logwriter_t   * log  = 0 ;

   // TEST static init
   TEST(0 == ilog.object) ;
   TEST(0 == ilog.functable) ;

   // TEST exported interface
   TEST(s_logwriter_interface.printf      == &printf_logwriter)
   TEST(s_logwriter_interface.flushbuffer == &flushbuffer_logwriter) ;
   TEST(s_logwriter_interface.clearbuffer == &clearbuffer_logwriter) ;
   TEST(s_logwriter_interface.getbuffer   == &getbuffer_logwriter) ;

   // TEST init, double free (ilog.object = 0)
   TEST(0 == initumgebung_logwriter(&ilog)) ;
   TEST(ilog.object) ;
   TEST(ilog.object    != &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &s_logwriter_interface) ;
   log = (logwriter_t*) ilog.object ;
   TEST(log->buffer.addr) ;
   TEST(log->buffer.size) ;
   TEST(0 == freeumgebung_logwriter(&ilog)) ;
   TEST(ilog.object    == &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &g_main_logwriter_interface) ;
   TEST(0 == freeumgebung_logwriter(&ilog)) ;
   TEST(ilog.object    == &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &g_main_logwriter_interface) ;
   log = 0 ;

   // TEST init, double free (ilog.object = &g_main_logwriter)
   ilog.object = &g_main_logwriter ;
   TEST(0 == initumgebung_logwriter(&ilog)) ;
   TEST(ilog.object) ;
   TEST(ilog.object    != &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &s_logwriter_interface) ;
   log = (logwriter_t*) ilog.object ;
   TEST(log->buffer.addr) ;
   TEST(log->buffer.size) ;
   TEST(0 == freeumgebung_logwriter(&ilog)) ;
   TEST(ilog.object    == &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &g_main_logwriter_interface) ;
   TEST(0 == freeumgebung_logwriter(&ilog)) ;
   TEST(ilog.object    == &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &g_main_logwriter_interface) ;
   log = 0 ;

   // TEST free (ilog.object = 0)
   ilog.object = 0 ;
   TEST(0 == freeumgebung_logwriter(&ilog)) ;
   TEST(0 == ilog.object) ;

   // TEST EINVAL
   ilog.object = (logwriter_t*) 1 ;
   TEST(EINVAL == initumgebung_logwriter(&ilog)) ;
   TEST(ilog.object == (logwriter_t*) 1) ;

   return 0 ;
ABBRUCH:
   freeumgebung_logwriter(&ilog) ;
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
   if (test_initumgebung())   goto ABBRUCH ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;}

#endif
