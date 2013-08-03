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
#include "C-kern/api/io/writer/log/log.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/io/writer/log/logbuffer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* struct: logmain_t
 * No object necessary.
 * All calls are delegated to a temporary <logwriter_t> object. */
struct logmain_t ;

// forward

static void printf_logmain(void * logmain, uint8_t channel, uint8_t flags, const log_header_t * logheader, const char * format, ... ) __attribute__ ((__format__ (__printf__, 5, 6))) ;
static void flushbuffer_logmain(void * logmain, uint8_t channel) ;
static void clearbuffer_logmain(void * logmain, uint8_t channel) ;
static void getbuffer_logmain(void * logmain, uint8_t channel, /*out*/char ** buffer, /*out*/size_t * size) ;

// group: variables

/* variable: g_logmain_interface
 * Contains single instance of interface <log_it>. */
log_it         g_logmain_interface  = {
                     &printf_logmain,
                     &flushbuffer_logmain,
                     &clearbuffer_logmain,
                     &getbuffer_logmain,
               } ;

// group: interface-implementation

static void printf_logmain(void * logmain, uint8_t channel, uint8_t flags, const log_header_t * logheader, const char * format, ... )
{
   (void) logmain ;
   (void) channel ;
   (void) flags ;
   uint8_t buffer[log_config_MINSIZE+1] = { 0 } ;
   va_list args ;
   va_start(args, format) ;
   logbuffer_t temp = logbuffer_INIT(
                        sizeof(buffer), buffer,
                        iochannel_STDERR
                      ) ;
   if (logheader) {
      printheader_logbuffer(&temp, logheader) ;
   }
   vprintf_logbuffer(&temp, format, args) ;
   write_logbuffer(&temp) ;
   va_end(args) ;
}

static void flushbuffer_logmain(void * logmain, uint8_t channel)
{
   (void) logmain ;
   (void) channel ;
}

static void clearbuffer_logmain(void * logmain, uint8_t channel)
{
   (void) logmain ;
   (void) channel ;
}

static void getbuffer_logmain(void * logmain, uint8_t channel, /*out*/char ** buffer, /*out*/size_t * size)
{
   (void) logmain ;
   (void) channel ;
   *buffer = 0 ;
   *size   = 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_globalvar(void)
{
   // TEST g_logmain_interface
   TEST(g_logmain_interface.printf      == &printf_logmain) ;
   TEST(g_logmain_interface.flushbuffer == &flushbuffer_logmain) ;
   TEST(g_logmain_interface.clearbuffer == &clearbuffer_logmain) ;
   TEST(g_logmain_interface.getbuffer   == &getbuffer_logmain) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   // TEST getbuffer_logmain
   char *   buffer  = (char*)1 ;
   size_t   size    = 1 ;
   getbuffer_logmain(0, log_channel_ERR, &buffer, &size) ;
   TEST(0 == buffer) ;
   TEST(0 == size) ;
   getbuffer_logmain(0, log_channel_NROFCHANNEL/*not used*/, &buffer, &size) ;
   TEST(0 == buffer) ;
   TEST(0 == size) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_update(void)
{
   int         pipefd[2] = { -1, -1 } ;
   int         oldstderr = -1 ;
   char        readbuffer[log_config_MINSIZE+1] ;
   char        maxstring[log_config_MINSIZE+1] ;

   // prepare
   memset(maxstring, '$', sizeof(maxstring)-1) ;
   maxstring[sizeof(maxstring)-1] = 0 ;
   TEST(0 == pipe2(pipefd,O_CLOEXEC|O_NONBLOCK)) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 < oldstderr) ;
   TEST(STDERR_FILENO == dup2(pipefd[1], STDERR_FILENO)) ;

   // TEST clearbuffer_logmain: does nothing
   clearbuffer_logmain(0, log_channel_ERR) ;

   // TEST flushbuffer_logmain: does nothing
   flushbuffer_logmain(0, log_channel_ERR) ;
   {
      char buffer2[9] = { 0 } ;
      TEST(-1 == read(pipefd[0], buffer2, sizeof(buffer2))) ;
      TEST(EAGAIN == errno) ;
   }

   // TEST printf_logmain: all channels flushed immediately
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      printf_logmain(0, i, log_flags_NONE, 0, "1%c%s%d", '2', "3", 4) ;
      TEST(4 == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(0 == strncmp("1234", readbuffer, 4)) ;
      printf_logmain(0, i, log_flags_NONE, 0, "%s;%d", maxstring, 1) ;
      TEST(log_config_MINSIZE == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(0 == memcmp(readbuffer, maxstring, log_config_MINSIZE-4)) ;
      TEST(0 == memcmp(readbuffer+log_config_MINSIZE-4, " ...", 4)) ;
   }

   // TEST printf_logmain: header
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      log_header_t header = log_header_INIT("func", "file", 10, EPERM) ;
      printf_logmain(0, i, log_flags_NONE, &header, "%s", "xxx") ;
      memset(readbuffer, 0, sizeof(readbuffer)) ;
      TEST(76 <= read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(0 == strncmp("[1: ", readbuffer, 4)) ;
      TEST(0 != strchr((char*)readbuffer, ']')) ;
      TEST(0 == strcmp("]\nfunc() file:10\nError 1 - Operation not permitted\nxxx", strchr((char*)readbuffer, ']'))) ;
   }

   // unprepare
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&pipefd[0])) ;
   TEST(0 == free_iochannel(&pipefd[1])) ;
   TEST(0 == free_iochannel(&oldstderr)) ;

   return 0 ;
ONABORT:
   if (-1 != oldstderr) {
      dup2(oldstderr, STDERR_FILENO) ;
   }
   free_iochannel(&oldstderr) ;
   free_iochannel(&pipefd[0]) ;
   free_iochannel(&pipefd[1]) ;
   return EINVAL ;
}

int unittest_io_writer_log_logmain()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_query())       goto ONABORT ;
   if (test_update())      goto ONABORT ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_globalvar())   goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_update())      goto ONABORT ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
