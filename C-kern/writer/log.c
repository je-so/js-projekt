/* title: LogWriter impl
   Implements logging of error messages.
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

   file: C-kern/api/writer/log.h
    Header file of <LogWriter>.

   file: C-kern/writer/log.c
    Implementation file <LogWriter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/writer/log.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/os/virtmemory.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/os/filesystem/mmfile.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

// forward

static void write_logbuffer(log_buffer_t * log) ;
static void printf_logbuffer( log_config_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;
static void printf_logstderr( log_config_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;
static void printf_logignore( log_config_t * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;

/* struct: log_buffer_t
 * Stores the memory address and size of cached output.
 * If the buffer is nearly full it is written to the
 * configured log channel. Currently only standard error is supported. */
struct log_buffer_t
{
   vm_block_t  buffer ;
   size_t      buffered_logsize ;
} ;

// group: lifetime

/* define: log_buffer_INIT_FREEABLE
 * */
#define log_buffer_INIT_FREEABLE    { .buffer = vm_block_INIT_FREEABLE, .buffered_logsize = 0 }

/* function: free_logbuffer
 * Frees allocates memory. If the buffer is filled
 * its content is written out before any resource is freed. */
static int free_logbuffer( log_buffer_t * log )
{
   int err ;

   if (log->buffered_logsize) {
      (void) write_logbuffer(log) ;
   }

   err = free_vmblock(&log->buffer) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: init_logbuffer
 * Reserves (virtual) memory for logbuffer. */
static int init_logbuffer( /*out*/log_buffer_t * log )
{
   int err ;
   size_t  pgsize = pagesize_vm() ;
   size_t nrpages = (pgsize < 8192) ? (8191 + pgsize) / pgsize : 1 ;

   err = init_vmblock(&log->buffer, nrpages) ;
   if (err) goto ABBRUCH ;
   log->buffered_logsize = 0 ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: clear_logbuffer
 * Clears content of buffer. */
static inline void clear_logbuffer(log_buffer_t * log)
{
   log->buffered_logsize = 0 ;
   log->buffer.addr[0]   = 0 ;   // NULL terminated string
}

/* function: getlogbuffer_log
 * Returns start and length of buffered log string. */
static inline void getlogbuffer_logbuffer(log_buffer_t * log, /*out*/char ** buffer, /*out*/size_t * size)
{
   *buffer = (char*) log->buffer.addr ;
   *size   = log->buffered_logsize ;
}

/* function: write_logbuffer
 * Writes content of buffer to log channel.
 * Currently only standard error is supported. */
static void write_logbuffer(log_buffer_t * log)
{
   size_t bytes_written = 0 ;

   do {
      ssize_t bytes ;
      do {
         bytes = write( STDERR_FILENO, log->buffer.addr + bytes_written, log->buffered_logsize - bytes_written) ;
      } while( bytes < 0 && errno == EINTR ) ;
      if (bytes <= 0) {
         // TODO: add some special log code that always works and which indicates error state in logging
         assert(errno != EAGAIN /*should be blocking i/o*/) ;
         break ;
      }
      bytes_written += (size_t) bytes ;
   } while (bytes_written < log->buffered_logsize) ;

   clear_logbuffer(log) ;
}

static void printf_logbuffer( log_config_t * logconfig, const char * format, ... )
{
   log_buffer_t * log = logconfig->log_buffer ;
   size_t buffer_size = log->buffer.size - log->buffered_logsize ;

   for(;;) {

      if (buffer_size < 512 ) {
         write_logbuffer(log) ;
         buffer_size = log->buffer.size ;
      }

      va_list args ;
      va_start(args, format) ;
      int append_size = vsnprintf( log->buffered_logsize + (char*)log->buffer.addr, buffer_size, format, args) ;
      va_end(args) ;

      if ( (unsigned)append_size < buffer_size ) {
         // no truncate
         log->buffered_logsize += (unsigned)append_size ;
         break ;
      }

      if ( buffer_size == log->buffer.size ) {
         // => s_logbuffered.buffered_logsize == 0
         // ignore truncate
         log->buffered_logsize = buffer_size - 1/*ignore 0 byte at end*/ ;
         write_logbuffer(log) ;
         break ;
      }

      // force flush && ignore append_size bytes
      buffer_size = 0 ;
   }
}

// section: logstderr

/* function: printf_logstderr
 * */
static void printf_logstderr( log_config_t * log, const char * format, ... )
{
   (void) log ;
   va_list args ;
   va_start(args, format) ;
   vdprintf(STDERR_FILENO, format, args) ;
   va_end(args) ;
}

// section: logignore

/* function: printf_logignore
 * */
static void printf_logignore( log_config_t * log, const char * format, ... )
{
   // generate no output
   (void) log ;
   (void) format ;
   return ;
}


// section: logconfig

// group: initumgebung

int initumgebung_log(log_config_t ** log)
{
   int err ;

   err = new_logconfig( log ) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_log(log_config_t ** log)
{
   int err ;
   log_config_t * log2 = *log ;

   *log = &g_main_logservice ;

   err = delete_logconfig( &log2 ) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: lifetime

int new_logconfig(/*out*/log_config_t ** log)
{
   int err ;
   log_config_t * logobj ;
   size_t        objsize = sizeof(log_config_t) + sizeof(log_buffer_t) ;

   if (  *log
      && *log != &g_main_logservice) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   logobj = (log_config_t*) malloc(objsize) ;
   if (!logobj) {
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   logobj->printf        = &printf_logstderr ;
   logobj->isOn          = true ;
   logobj->isBuffered    = false ;
   logobj->isConstConfig = false ;
   logobj->log_buffer    = (log_buffer_t*) &logobj[1] ;
   *logobj->log_buffer   = (log_buffer_t) log_buffer_INIT_FREEABLE ;

   *log = logobj ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int delete_logconfig(log_config_t ** log)
{
   int err ;
   log_config_t * logobj = *log ;

   if (     logobj
         && logobj != &g_main_logservice) {
      *log = 0 ;
      err  = free_logbuffer(logobj->log_buffer) ;
      free(logobj) ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: other

/* function: switch_printf_logconfig
 * Used to switch between different implementation.
 * Only the variable <log_config_t.printf> is switched between
 *
 * 1. <printf_logbuffer>
 * 2. <printf_logstderr>
 * 3. <printf_logignore> */
static void switch_printf_logconfig(log_config_t * log)
{
   if (log->isOn) {
      if (log->isBuffered) {
         log->printf = &printf_logbuffer ;
      } else {
         log->printf = &printf_logstderr ;
      }
   } else {
      log->printf = &printf_logignore ;
   }
}

int setonoff_logconfig(log_config_t * log, bool onoff)
{
   int err ;

   if (log->isConstConfig) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (log->isOn != onoff) {

      log->isOn = onoff ;

      switch_printf_logconfig(log) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: setbuffermode_logconfig implementation
 * See <setbuffermode_logconfig> for a description of its interface.
 * The internal variable <log_config_t.log_buffer> is initialized
 * or freed depending on whether buffermode is switched on or off.
 * If the initialization (or free) returns an error nothing is
 * changed. */
int setbuffermode_logconfig(log_config_t * log, bool mode)
{
   int err ;

   if (log->isConstConfig) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (log->isBuffered != mode) {

      if ( mode ) {
         err = init_logbuffer(log->log_buffer) ;
         if (err) goto ABBRUCH ;
      } else {
         err = free_logbuffer(log->log_buffer) ;
         if (err) goto ABBRUCH ;
      }

      log->isBuffered = mode ;
      switch_printf_logconfig(log) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

void clearbuffer_logconfig(log_config_t * log)
{
   if (log->isBuffered) {
      clear_logbuffer(log->log_buffer) ;
   }
}

void writebuffer_logconfig(log_config_t * log)
{
   if (log->isBuffered) {
      write_logbuffer(log->log_buffer) ;
   }
}

void getlogbuffer_logconfig(log_config_t * log, /*out*/char ** buffer, /*out*/size_t * size)
{
   if (log->isBuffered) {
      getlogbuffer_logbuffer( log->log_buffer, buffer, size ) ;
   } else {
      *buffer = 0 ;
      *size   = 0 ;
   }
}

// section: global variables

/* variable: g_main_logservice
 * Is used to support a safe standard log configuration.
 * Used to write log output before any other init function was called. */
log_config_t  g_main_logservice = {
         .printf          = &printf_logstderr,
         .isOn            = true,
         .isBuffered      = false,
         .isConstConfig   = true,
         .log_buffer      = 0
} ;


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_writer_log,ABBRUCH)

static int test_defaultvalues(log_config_t * logconf, bool isOn, typeof(((log_config_t*)0)->printf) printf_callback )
{
   TEST( logconf ) ;
   TEST( ! logconf->isBuffered ) ;
   TEST( ! logconf->isConstConfig ) ;
   TEST( logconf->isOn       == isOn ) ;
   TEST( logconf->log_buffer ) ;
   TEST( logconf->log_buffer == (log_buffer_t*)(&logconf[1])) ;
   TEST( logconf->printf     == printf_callback) ;
   return 0 ;
ABBRUCH:
   return 1 ;
}

static int test_log_default(void)
{
   log_config_t     * logconf = 0 ;
   int                 tempfd = - 1;
   int              oldstderr = -1 ;
   mmfile_t        logcontent = mmfile_INIT_FREEABLE ;
   directory_stream_t tempdir = directory_stream_INIT_FREEABLE ;

   // TEST init, double free
   TEST(0 == new_logconfig(&logconf)) ;
   TEST(0 == test_defaultvalues(logconf, true, &printf_logstderr)) ;
   TEST(0 == delete_logconfig(&logconf)) ;
   TEST( ! logconf ) ;
   TEST(0 == delete_logconfig(&logconf)) ;
   TEST( ! logconf ) ;

   // TEST set_onoff
   TEST(0 == new_logconfig(&logconf)) ;
   TEST(0 == setonoff_logconfig(logconf, false)) ;
   TEST(0 == test_defaultvalues(logconf, false, &printf_logignore)) ;
   TEST(0 == setonoff_logconfig(logconf, true)) ;
   TEST(0 == test_defaultvalues(logconf, true, &printf_logstderr)) ;
   TEST(0 == delete_logconfig(&logconf)) ;

   // init (TEST write)
   TEST(0 == inittemp_directorystream(&tempdir, "tempdir")) ;
   TEST(0 == makefile_directorystream(&tempdir, "testlog")) ;
   tempfd = openat(dirfd(tempdir.sys_dir), "testlog", O_RDWR|O_CLOEXEC, 0600) ;
   TEST(0 <  tempfd) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 <  oldstderr) ;
   TEST(STDERR_FILENO == dup2(tempfd, STDERR_FILENO)) ;

   // TEST write printf_logstderr
   TEST(0 == new_logconfig(&logconf)) ;
   TEST(0 == test_defaultvalues(logconf, true, &printf_logstderr)) ;
   logconf->printf( logconf, "TEST1: %d: %s: END-TEST\n", -123, "123test" ) ;
   logconf->printf( logconf, "TEST2: %d: %c: END-TEST\n", 1, 'X' ) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
#define LOG_CONTENT "TEST1: -123: 123test: END-TEST\nTEST2: 1: X: END-TEST\n"
   size_t logsize = sizeof(LOG_CONTENT) - 1 ;
   TEST(logsize == size_mmfile(&logcontent)) ;
   TEST(0 == memcmp(addr_mmfile(&logcontent), LOG_CONTENT, logsize)) ;
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == delete_logconfig(&logconf)) ;

   // TEST write printf_logignore
   TEST(0 == new_logconfig(&logconf)) ;
   TEST(0 == test_defaultvalues(logconf, true, &printf_logstderr)) ;
   TEST(0 == setonoff_logconfig(logconf, false)) ;
   TEST(0 == test_defaultvalues(logconf, false, &printf_logignore)) ;
   logconf->printf( logconf, "NOTHING IS WRITTEN: %d: %s: END-NOTHING\n", 4, "5" ) ;
   // test logcontent has not changed
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
   TEST(logsize == size_mmfile(&logcontent)) ;
   TEST(0 == memcmp(addr_mmfile(&logcontent), LOG_CONTENT, logsize)) ;
#undef LOG_CONTENT
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == delete_logconfig(&logconf)) ;

   // free (TEST write)
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
   delete_logconfig(&logconf) ;
   return 1 ;
}

static int test_log_safe(void)
{
   log_config_t * logconf = &g_main_logservice ;

   // TEST g_safe_logservice static definition
   TEST( logconf ) ;
   TEST( ! logconf->isBuffered ) ;
   TEST( logconf->isConstConfig ) ;
   TEST( logconf->isOn ) ;
   TEST( ! logconf->log_buffer ) ;
   TEST( logconf->printf == &printf_logstderr ) ;

   // TEST config
   TEST(EINVAL == setonoff_logconfig(logconf, false)) ;
   TEST(EINVAL == setbuffermode_logconfig(logconf, true)) ;

   // TEST write
   // ! already DONE in test_log_default for printf_logstderr !
   TEST( logconf->printf == &printf_logstderr ) ;

   // TEST g_safe_logservice static definition has not changed
   TEST( logconf ) ;
   TEST( ! logconf->isBuffered ) ;
   TEST( logconf->isConstConfig ) ;
   TEST( logconf->isOn ) ;
   TEST( ! logconf->log_buffer ) ;
   TEST( logconf->printf == &printf_logstderr ) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

static int test_bufferedvalues(log_config_t * logconf, bool isOn, typeof(((log_config_t*)0)->printf) printf_callback )
{
   TEST( logconf ) ;
   TEST( logconf->isBuffered ) ;
   TEST( ! logconf->isConstConfig ) ;
   TEST( logconf->isOn       == isOn ) ;
   TEST( logconf->log_buffer ) ;
   TEST( logconf->log_buffer == (log_buffer_t*)(&logconf[1])) ;
   TEST( logconf->printf     == printf_callback) ;
   return 0 ;
ABBRUCH:
   return 1 ;
}

static int test_log_buffered(void)
{
   log_config_t     * logconf = 0 ;
   int                 tempfd = - 1;
   int              oldstderr = -1 ;
   mmfile_t        logcontent = mmfile_INIT_FREEABLE ;
   directory_stream_t tempdir = directory_stream_INIT_FREEABLE ;
   size_t         buffer_size = 0 ;
   off_t            file_size ;

   while( buffer_size < 8192 ) {
      buffer_size += pagesize_vm() ;
   }

   // TEST init, double free
   TEST(0 == new_logconfig(&logconf)) ;
   TEST(0 == test_defaultvalues(logconf, true, &printf_logstderr)) ;
   TEST(0 == setbuffermode_logconfig(logconf, true)) ;
   TEST(0 == test_bufferedvalues(logconf, true, &printf_logbuffer)) ;
   TEST(0 != logconf->log_buffer->buffer.addr) ;
   TEST(buffer_size == logconf->log_buffer->buffer.size) ;
   TEST(0 == logconf->log_buffer->buffered_logsize) ;
   TEST(0 == delete_logconfig(&logconf)) ;
   TEST( ! logconf ) ;
   TEST(0 == delete_logconfig(&logconf)) ;
   TEST( ! logconf ) ;

   // TEST set_onoff
   TEST(0 == new_logconfig(&logconf)) ;
   TEST(0 == setbuffermode_logconfig(logconf, true)) ;
   TEST(0 == setonoff_logconfig(logconf, false)) ;
   TEST(0 == test_bufferedvalues(logconf, false, &printf_logignore)) ;
   TEST(0 == setonoff_logconfig(logconf, true)) ;
   TEST(0 == test_bufferedvalues(logconf, true, &printf_logbuffer)) ;
   TEST(0 == delete_logconfig(&logconf)) ;

   // init (TEST write)
   TEST(0 == inittemp_directorystream(&tempdir, "tempdir")) ;
   TEST(0 == makefile_directorystream(&tempdir, "testlog")) ;
   tempfd = openat(dirfd(tempdir.sys_dir), "testlog", O_RDWR|O_CLOEXEC, 0600) ;
   TEST(0 <  tempfd) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 <  oldstderr) ;
   TEST(STDERR_FILENO == dup2(tempfd, STDERR_FILENO)) ;

   // TEST write printf_logbuffer
   TEST(0 == new_logconfig(&logconf)) ;
   TEST(0 == setbuffermode_logconfig(logconf, true)) ;
   TEST(0 == test_bufferedvalues(logconf, true, &printf_logbuffer)) ;
   logconf->printf( logconf, "%s", "TESTSTRT\n" ) ;
   logconf->printf( logconf, "%s", "TESTENDE\n" ) ;
   TEST(18 == logconf->log_buffer->buffered_logsize) ;
   TEST(ENODATA == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
   writebuffer_logconfig(logconf) ;
   TEST(0 == logconf->log_buffer->buffered_logsize) ;
#define LOG_CONTENT "TESTSTRT\nTESTENDE\n"
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
   size_t logsize = sizeof(LOG_CONTENT) - 1 ;
   TEST(logsize == size_mmfile(&logcontent)) ;
   TEST(0 == memcmp(addr_mmfile(&logcontent), LOG_CONTENT, logsize)) ;
   TEST(0 == free_mmfile(&logcontent)) ;
   for(size_t i = 0; i < buffer_size-510; ++i) {
      TEST(i   == logconf->log_buffer->buffered_logsize) ;
      logconf->printf( logconf, "%c", 'F' ) ;
   }
   TEST(1 == logconf->log_buffer->buffered_logsize) ;
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
   logsize = sizeof(LOG_CONTENT) - 1 + buffer_size - 511 ;
   TEST(logsize == size_mmfile(&logcontent)) ;
   TEST(0 == memcmp(addr_mmfile(&logcontent), LOG_CONTENT, sizeof(LOG_CONTENT)-1)) ;
   for(size_t i = 0; i < buffer_size-511; ++i) {
      TEST('F' == (addr_mmfile(&logcontent)[sizeof(LOG_CONTENT)-1+i]) ) ;
   }
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == delete_logconfig(&logconf)) ;   // writes content of buffer !
   TEST(0 == filesize_directory("testlog", &tempdir, &file_size)) ;
   ++ logsize ;
   TEST(logsize == file_size) ;

   // TEST write printf_logignore
   TEST(0 == new_logconfig(&logconf)) ;
   TEST(0 == setbuffermode_logconfig(logconf, true)) ;
   TEST(0 == setonoff_logconfig(logconf, false)) ;
   TEST(0 == test_bufferedvalues(logconf, false, &printf_logignore)) ;
   logconf->printf( logconf, "NOTHING IS WRITTEN\n" ) ;
   TEST(0 == logconf->log_buffer->buffered_logsize) ;
   writebuffer_logconfig(logconf) ;
   // test logcontent has not changed
   TEST(0 == init_mmfile(&logcontent, "testlog", 0, 0, &tempdir, mmfile_openmode_RDONLY)) ;
   TEST(logsize == size_mmfile(&logcontent)) ;
   TEST(0 == memcmp(addr_mmfile(&logcontent), LOG_CONTENT, sizeof(LOG_CONTENT)-1)) ;
   for(size_t i = 0; i < buffer_size-510; ++i) {
      TEST('F' == (addr_mmfile(&logcontent)[sizeof(LOG_CONTENT)-1+i]) ) ;
   }
#undef LOG_CONTENT
   TEST(0 == free_mmfile(&logcontent)) ;
   TEST(0 == delete_logconfig(&logconf)) ;

   // free (TEST write)
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
   delete_logconfig(&logconf) ;
   return EINVAL ;
}

static int test_initumgebung(void)
{
   log_config_t * log = 0 ;

   // TEST EINVAL initumgebung
   log = (log_config_t*) 1 ;
   TEST(EINVAL == initumgebung_log(&log)) ;

   // TEST initumgebung, double freeumgebung
   log = 0 ;
   TEST(0 == initumgebung_log(&log)) ;
   TEST(log) ;
   TEST(0 == freeumgebung_log(&log)) ;
   TEST(&g_main_logservice == log) ;
   TEST(0 == freeumgebung_log(&log)) ;
   TEST(&g_main_logservice == log) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_writer_log()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_log_default())    goto ABBRUCH ;
   if (test_log_safe())       goto ABBRUCH ;
   if (test_log_buffered())   goto ABBRUCH ;
   if (test_initumgebung())   goto ABBRUCH ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;}

#endif
