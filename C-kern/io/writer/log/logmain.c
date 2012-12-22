/* title: LogMain impl
   Implements <LogMain>.

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

   file: C-kern/api/io/writer/log/logmain.h
    Header file of <LogMain>.

   file: C-kern/io/writer/log/logmain.c
    Implementation file of <LogMain impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/io/writer/log/log_it.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* struct: logmain_t
 * Object to describe data of main log writer.
 * This object is currently not used.
 * The implementation of <g_logmain_interface> uses C-Library functions
 * to write the log to standard error channel.
 */
struct logmain_t {
   int dummy ;
} ;

// group: forward

static void printf_logmain(void * log, log_channel_e channel, const char * format, ... ) __attribute__ ((__format__ (__printf__, 3, 4))) ;
static void flushbuffer_logmain(void * log) ;
static void clearbuffer_logmain(void * log) ;
static void getbuffer_logmain(void * log, /*out*/char ** buffer, /*out*/size_t * size) ;

// group: variables

/* variable: g_logmain
 * Is used to support a safe standard log configuration.
 * Used to write log output before any other init function was called. */

logmain_t      g_logmain            = { 0 } ;

/* variable: g_logmain_interface
 * Contains single instance of interface <logwritermt_it>. */
log_it         g_logmain_interface  = {
                     &printf_logmain,
                     &flushbuffer_logmain,
                     &clearbuffer_logmain,
                     &getbuffer_logmain,
               } ;

// group: interface-implementation

static void printf_logmain(void * lgwrt, log_channel_e channel, const char * format, ... )
{
   uint8_t  buffer[log_PRINTF_MAXSIZE+1] = { 0 } ;
   va_list  args ;
   (void) lgwrt ;
   va_start(args, format) ;
   int bytes = vsnprintf((char*)buffer, sizeof(buffer), format, args) ;
   if (bytes > 0) {
      unsigned ubytes = ((unsigned)bytes >= sizeof(buffer)) ? sizeof(buffer) -1 : (unsigned) bytes ;
      if (log_channel_TEST == channel) {
         write_file(file_STDOUT, ubytes, buffer, 0) ;
      } else {
         write_file(file_STDERR, ubytes, buffer, 0) ;
      }
   }
   va_end(args) ;
}

static void flushbuffer_logmain(void * lgwrt)
{
   (void) lgwrt ;
}

static void clearbuffer_logmain(void * lgwrt)
{
   (void) lgwrt ;
}

static void getbuffer_logmain(void * lgwrt, /*out*/char ** buffer, /*out*/size_t * size)
{
   (void) lgwrt ;
   *buffer = 0 ;
   *size   = 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_globalvar(void)
{
   logmain_t   * lgwrt   = &g_logmain ;
   char        * buffer  = 0 ;
   size_t      size      = 0 ;
   int         pipefd[2] = { -1, -1 } ;
   int         oldstderr = -1 ;
   int         oldstdout = -1 ;
   char        readbuffer[log_PRINTF_MAXSIZE+1] ;
   char        maxstring[log_PRINTF_MAXSIZE] ;

   // prepare
   memset(maxstring, '$', sizeof(maxstring)) ;
   TEST(0 == pipe2(pipefd,O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 < (oldstderr = dup(STDERR_FILENO))) ;
   TEST(0 < (oldstdout = dup(STDOUT_FILENO))) ;

   // TEST interface
   TEST(g_logmain_interface.printf      == &printf_logmain) ;
   TEST(g_logmain_interface.flushbuffer == &flushbuffer_logmain) ;
   TEST(g_logmain_interface.clearbuffer == &clearbuffer_logmain) ;
   TEST(g_logmain_interface.getbuffer   == &getbuffer_logmain) ;

   // TEST getbuffer_logmain
   buffer = (char*) 1;
   size   = 1 ;
   getbuffer_logmain(lgwrt, &buffer, &size) ;
   TEST(0 == buffer) ;
   TEST(0 == size) ;

   // TEST clearbuffer_logmain does nothing
   TEST(0 == lgwrt->dummy) ;
   clearbuffer_logmain(lgwrt) ;
   TEST(0 == lgwrt->dummy) ;

   // TEST flushbuffer_logmain does nothing
   TEST(STDERR_FILENO == dup2(pipefd[1], STDERR_FILENO)) ;
   flushbuffer_logmain(lgwrt) ;
   {
      char buffer2[9] = { 0 } ;
      TEST(-1 == read(pipefd[0], buffer2, sizeof(buffer2))) ;
      TEST(EAGAIN == errno) ;
   }

   // TEST printf_logmain (log_channel_ERR)
   printf_logmain(lgwrt, log_channel_ERR, "1%c%s%d", '2', "3", 4) ;
   TEST(4 == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
   TEST(0 == strncmp("1234", readbuffer, 4)) ;
   printf_logmain(lgwrt, log_channel_ERR, "%s;%d", maxstring, 1) ;
   TEST(sizeof(maxstring) == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
   TEST(0 == memcmp(readbuffer, maxstring, sizeof(maxstring))) ;
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;

   // TEST printf_logmain (log_channel_TEST)
   TEST(STDOUT_FILENO == dup2(pipefd[1], STDOUT_FILENO)) ;
   printf_logmain(lgwrt, log_channel_TEST, "1%c%s%d", '2', "3", 4) ;
   TEST(4 == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
   TEST(0 == strncmp("1234", readbuffer, 4)) ;
   printf_logmain(lgwrt, log_channel_TEST, "%s;%d", maxstring, 1) ;
   TEST(sizeof(maxstring) == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
   TEST(0 == memcmp(readbuffer, maxstring, sizeof(maxstring))) ;
   TEST(STDOUT_FILENO == dup2(oldstdout, STDOUT_FILENO)) ;

   // unprepare
   TEST(0 == free_file(&pipefd[0])) ;
   TEST(0 == free_file(&pipefd[1])) ;
   TEST(0 == free_file(&oldstderr)) ;
   TEST(0 == free_file(&oldstdout)) ;

   return 0 ;
ONABORT:
   if (-1 != oldstderr) {
      dup2(oldstderr, STDERR_FILENO) ;
   }
   if (-1 != oldstdout) {
      dup2(oldstdout, STDOUT_FILENO) ;
   }
   free_file(&oldstderr) ;
   free_file(&oldstdout) ;
   free_file(&pipefd[0]) ;
   free_file(&pipefd[1]) ;
   return EINVAL ;
}

int unittest_io_writer_log_logmain()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_globalvar())      goto ONABORT ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
