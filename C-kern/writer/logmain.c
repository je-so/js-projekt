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

   file: C-kern/api/writer/logmain.h
    Header file of <LogMain>.

   file: C-kern/writer/logmain.c
    Implementation file of <LogMain impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/writer/logmain.h"
#include "C-kern/api/writer/log_it.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filedescr.h"
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

static void printf_logmain(void * log, const char * format, ... ) __attribute__ ((__format__ (__printf__, 2, 3))) ;
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

static void printf_logmain(void * lgwrt, const char * format, ... )
{
   char     buffer[log_PRINTF_MAXSIZE+1] = { 0 } ;
   va_list  args ;
   (void) lgwrt ;
   va_start(args, format) ;
   vsnprintf(buffer, sizeof(buffer), format, args) ;
   write_filedescr(filedescr_STDERR, strlen(buffer), buffer, 0) ;
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
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 < oldstderr) ;
   TEST(0 == pipe2(pipefd,O_CLOEXEC|O_NONBLOCK)) ;
   TEST(STDERR_FILENO == dup2(pipefd[1], STDERR_FILENO)) ;
   flushbuffer_logmain(lgwrt) ;
   {
      char buffer2[9] = { 0 } ;
      TEST(-1 == read(pipefd[0], buffer2, sizeof(buffer2))) ;
      TEST(EAGAIN == errno) ;
   }

   // TEST printf_logmain
   printf_logmain(lgwrt, "1%c%s%d", '2', "3", 4) ;
   {
      char buffer2[5] = { 0 } ;
      TEST(4 == read(pipefd[0], buffer2, sizeof(buffer2))) ;
      TEST(0 == strncmp("1234", buffer2, 4)) ;
   }
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == free_filedescr(&oldstderr)) ;
   TEST(0 == free_filedescr(&pipefd[0])) ;
   TEST(0 == free_filedescr(&pipefd[1])) ;

   return 0 ;
ABBRUCH:
   if (-1 != oldstderr) {
      dup2(oldstderr, STDERR_FILENO) ;
   }
   free_filedescr(&oldstderr) ;
   free_filedescr(&pipefd[0]) ;
   free_filedescr(&pipefd[1]) ;
   return EINVAL ;
}

int unittest_writer_logmain()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_globalvar())      goto ABBRUCH ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
