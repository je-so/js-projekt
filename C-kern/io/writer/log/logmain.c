/* title: LogMain impl
   Implements <LogMain>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
#endif


/* struct: logmain_t
 * No object necessary.
 * All calls are delegated to a temporary <logwriter_t> object. */
struct logmain_t ;

// forward

static void printf_logmain(void * logmain, uint8_t channel, uint8_t flags, const log_header_t * logheader, const char * format, ... ) __attribute__ ((__format__ (__printf__, 5, 6))) ;
static void printtext_logmain(void * logmain, uint8_t channel, uint8_t flags, const log_header_t * logheader, log_text_f textf, ... ) ;
static void flushbuffer_logmain(void * logmain, uint8_t channel) ;
static void truncatebuffer_logmain(void * logmain, uint8_t channel, size_t size) ;
static void getbuffer_logmain(const void * logmain, uint8_t channel, /*out*/uint8_t ** buffer, /*out*/size_t * size) ;
static uint8_t getstate_logmain(const void * logmain, uint8_t channel) ;
static int compare_logmain(const void * logmain, uint8_t channel, size_t logsize, const uint8_t logbuffer[logsize]);
static void setstate_logmain(void * logmain, uint8_t channel, uint8_t state) ;

// group: global variables

/* variable: g_logmain_interface
 * Contains single instance of interface <log_it>. */
log_it         g_logmain_interface  = {
                     &printf_logmain,
                     &printtext_logmain,
                     &flushbuffer_logmain,
                     &truncatebuffer_logmain,
                     &getbuffer_logmain,
                     &getstate_logmain,
                     &compare_logmain,
                     &setstate_logmain
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

static void printtext_logmain(void * logmain, uint8_t channel, uint8_t flags, const log_header_t * logheader, log_text_f textf, ... )
{
   (void) logmain ;
   (void) channel ;
   (void) flags ;
   uint8_t buffer[log_config_MINSIZE+1] = { 0 } ;
   va_list args ;
   va_start(args, textf) ;
   logbuffer_t temp = logbuffer_INIT(
                        sizeof(buffer), buffer,
                        iochannel_STDERR
                      ) ;
   if (logheader) {
      printheader_logbuffer(&temp, logheader) ;
   }
   textf(&temp, args) ;
   write_logbuffer(&temp) ;
   va_end(args) ;
}

static void flushbuffer_logmain(void * logmain, uint8_t channel)
{
   (void) logmain;
   (void) channel;
}

static void truncatebuffer_logmain(void * logmain, uint8_t channel, size_t size)
{
   (void) logmain;
   (void) channel;
   (void) size;
}

static void getbuffer_logmain(const void * logmain, uint8_t channel, /*out*/uint8_t ** buffer, /*out*/size_t * size)
{
   (void) logmain ;
   (void) channel ;
   *buffer = 0 ;
   *size   = 0 ;
}

static uint8_t getstate_logmain(const void * logmain, uint8_t channel)
{
   (void) logmain ;
   (void) channel ;
   return log_state_IMMEDIATE ;
}

static int compare_logmain(const void * logmain, uint8_t channel, size_t logsize, const uint8_t logbuffer[logsize])
{
   (void) logmain ;
   (void) channel ;
   (void) logbuffer ;
   return logsize ? EINVAL : 0;
}

static void setstate_logmain(void * logmain, uint8_t channel, uint8_t state)
{
   (void) logmain ;
   (void) channel ;
   (void) state ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_globalvar(void)
{
   // TEST g_logmain_interface
   TEST(g_logmain_interface.printf         == &printf_logmain);
   TEST(g_logmain_interface.printtext      == &printtext_logmain);
   TEST(g_logmain_interface.flushbuffer    == &flushbuffer_logmain);
   TEST(g_logmain_interface.truncatebuffer == &truncatebuffer_logmain);
   TEST(g_logmain_interface.getbuffer      == &getbuffer_logmain);
   TEST(g_logmain_interface.getstate       == &getstate_logmain);
   TEST(g_logmain_interface.compare        == &compare_logmain);
   TEST(g_logmain_interface.setstate       == &setstate_logmain);

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_query(void)
{
   // TEST getbuffer_logmain
   uint8_t *logbuffer = (uint8_t*)1;
   size_t   logsize   = 1;
   getbuffer_logmain(0, log_channel_ERR, &logbuffer, &logsize);
   TEST(0 == logbuffer);
   TEST(0 == logsize);
   getbuffer_logmain(0, log_channel__NROF/*not used*/, &logbuffer, &logsize);
   TEST(0 == logbuffer);
   TEST(0 == logsize);

   // TEST getstate_logmain
   TEST(log_state_IMMEDIATE == getstate_logmain(0, 0/*not used*/));
   TEST(log_state_IMMEDIATE == getstate_logmain(0, log_channel__NROF/*not used*/));

   // TEST compare_logmain
   TEST(0 == compare_logmain(0, 0, 0/*logsize == 0*/, 0));
   TEST(EINVAL == compare_logmain(0, 0, 1/*logsize > 0*/, 0));

   return 0;
ONERR:
   return EINVAL;
}

static void text_resource_test(logbuffer_t * logbuf, va_list vargs)
{
   vprintf_logbuffer(logbuf, "2%c%s%d", vargs);
}

static int test_update(void)
{
   int         pipefd[2] = { -1, -1 } ;
   int         oldstderr = -1 ;
   char        readbuffer[log_config_MINSIZE+1];
   char        maxstring[log_config_MINSIZE+1];

   // prepare
   memset(maxstring, '$', sizeof(maxstring)-1) ;
   maxstring[sizeof(maxstring)-1] = 0 ;
   TEST(0 == pipe2(pipefd,O_CLOEXEC|O_NONBLOCK)) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 < oldstderr) ;
   TEST(STDERR_FILENO == dup2(pipefd[1], STDERR_FILENO)) ;

   // TEST truncatebuffer_logmain: does nothing
   truncatebuffer_logmain(0, log_channel_ERR, 0);
   truncatebuffer_logmain(0, log_channel_ERR, (size_t)-1);

   // TEST flushbuffer_logmain: does nothing
   flushbuffer_logmain(0, log_channel_ERR) ;
   {
      char buffer2[9] = { 0 } ;
      TEST(-1 == read(pipefd[0], buffer2, sizeof(buffer2))) ;
      TEST(EAGAIN == errno) ;
   }

   // TEST printf_logmain: all channels flushed immediately
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      printf_logmain(0, i, log_flags_NONE, 0, "1%c%s%d", '2', "3", 4) ;
      TEST(4 == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(0 == strncmp("1234", readbuffer, 4)) ;
      printf_logmain(0, i, log_flags_NONE, 0, "%s;%d", maxstring, 1) ;
      TEST(log_config_MINSIZE == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(0 == memcmp(readbuffer, maxstring, log_config_MINSIZE-4)) ;
      TEST(0 == memcmp(readbuffer+log_config_MINSIZE-4, " ...", 4)) ;
   }

   // TEST printf_logmain: header
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      log_header_t header = log_header_INIT("func", "file", 10);
      printf_logmain(0, i, log_flags_NONE, &header, "%s", "xxx");
      memset(readbuffer, 0, sizeof(readbuffer)) ;
      TEST(42 <= read(pipefd[0], readbuffer, sizeof(readbuffer)));
      TEST(0 == strncmp("[1: ", readbuffer, 4)) ;
      TEST(0 != strchr((char*)readbuffer, ']')) ;
      TEST(0 == strcmp("]\nfunc() file:10\nxxx", strchr((char*)readbuffer, ']'))) ;
   }

   // TEST printf_logmain: format == 0
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      printf_logmain(0, i, log_flags_NONE, 0, 0);
      // nothing written
      TEST(-1 == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(EAGAIN == errno);
   }

   // TEST printtext_logmain: all channels flushed immediately
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      // 2%c%s%d
      printtext_logmain(0, i, log_flags_NONE, 0, text_resource_test, '3', "45", 6) ;
      TEST(5 == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(0 == strncmp("23456", readbuffer, 5)) ;
      printtext_logmain(0, i, log_flags_NONE, 0, text_resource_test, '3', maxstring, 6) ;
      TEST(log_config_MINSIZE == read(pipefd[0], readbuffer, sizeof(readbuffer))) ;
      TEST(0 == memcmp(readbuffer, "23", 2)) ;
      TEST(0 == memcmp(readbuffer+2, maxstring, log_config_MINSIZE-6)) ;
      TEST(0 == memcmp(readbuffer+log_config_MINSIZE-4, " ...", 4)) ;
   }

   // TEST printtext_logmain: header
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      log_header_t header = log_header_INIT("func", "file", 1);
      printtext_logmain(0, i, log_flags_NONE, &header, text_resource_test, '4', maxstring, 6) ;
      TEST(log_config_MINSIZE == read(pipefd[0], readbuffer, sizeof(readbuffer)));
      TEST(0 == strncmp("[1: ", readbuffer, 4)) ;
      const char * off = strchr((char*)readbuffer, ']') ;
      TEST(0 != off) ;
      const char * result = "]\nfunc() file:1\n24$";
      TEST(0 == strncmp(result, off, strlen(result)));
      off += strlen(result);
      TEST(0 == memcmp(off, maxstring, log_config_MINSIZE-4-(size_t)(off-readbuffer))) ;
      TEST(0 == memcmp(readbuffer+log_config_MINSIZE-4, " ...", 4)) ;
   }

   // TEST setstate_logmain
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      setstate_logmain(0, i, log_state_BUFFERED) ;   // ignored
      TEST(log_state_IMMEDIATE == getstate_logmain(0, i)) ;
   }

   // unprepare
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&pipefd[0])) ;
   TEST(0 == free_iochannel(&pipefd[1])) ;
   TEST(0 == free_iochannel(&oldstderr)) ;

   return 0 ;
ONERR:
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
   if (test_globalvar())   goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
