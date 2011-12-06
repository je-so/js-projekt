/* title: Main-LogWriter impl
   See <Main-LogWriter>

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

   file: C-kern/api/writer/main_logwriter.h
    Header file of <Main-LogWriter>.

   file: C-kern/writer/main_logwriter.c
    Implementation file of <Main-LogWriter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/writer/main_logwriter.h"
#include "C-kern/api/writer/logwriter.h"
#include "C-kern/api/writer/logwritermt.h"
#include "C-kern/api/err.h"
#include "C-kern/api/aspect/interface/log_it.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/os/sync/signal.h"
#endif

// group: types

/* typedef: logwritermt_it
 * Define interface <logwritermt_it>, see also <ilog_it>.
 * The interface is generated with the macro <log_it_DECLARE>. */
log_it_DECLARE(1, logwritermt_it, logwritermt_t)

/* typedef: logwriter_it
 * Define interface <logwriter_it>, see also <ilog_it>.
 * The interface is generated with the macro <log_it_DECLARE>. */
log_it_DECLARE(1, logwriter_it, logwriter_t)

// adapt to different thread model
#define THREAD 1
#if ((KONFIG_SUBSYS)&THREAD)
#define EMITTHREADCODE  EMITCODE_1
#else
#define EMITTHREADCODE  EMITCODE_0
#define  logwritermt_t              logwriter_t
#define  logwritermt_it             logwriter_it
#define  free_logwritermt           free_logwriter
#define  printf_logwritermt         printf_logwriter
#define  flushbuffer_logwritermt    flushbuffer_logwriter
#define  clearbuffer_logwritermt    clearbuffer_logwriter
#define  getbuffer_logwritermt      getbuffer_logwriter
#endif
#undef THREAD

// group: variables

/* variable: s_logbuffer
 * Buffer <g_main_logwriter> uses internally to store one log entry. */
static uint8_t  s_logbuffer[1 + log_PRINTF_MAXSIZE] ;

/* variable: g_main_logwriter
 * Is used to support a safe standard log configuration.
 * Used to write log output before any other init function was called. */

logwritermt_t     g_main_logwriter = {
EMITTHREADCODE (     .logwriter = { )
                                    .buffer = { .addr = s_logbuffer, .size = sizeof(s_logbuffer) }, .logsize = 0
EMITTHREADCODE (                }, )
EMITTHREADCODE (     .lock      = sys_mutex_INIT_DEFAULT, )
                  } ;

/* variable: g_main_logwriter_interface
 * Contains single instance of interface <logwritermt_it>. */
logwritermt_it    g_main_logwriter_interface = {
                     &printf_logwritermt,
                     &flushbuffer_logwritermt,
                     &clearbuffer_logwritermt,
                     &getbuffer_logwritermt,
                  } ;


// group: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION, ABBRUCH)

static char * s_thrdarg_buffer = 0 ;
static size_t s_thrdarg_size   = 0 ;

static int thread_getbuffer(logwritermt_t * log)
{
   assert(0 == send_rtsignal(0)) ;
   getbuffer_logwritermt(log, &s_thrdarg_buffer, &s_thrdarg_size) ;
   return 0 ;
}

static int thread_clearbuffer(logwritermt_t * log)
{
   assert(0 == send_rtsignal(0)) ;
   clearbuffer_logwritermt(log) ;
   return 0 ;
}

static int thread_flushbuffer(logwritermt_t * log)
{
   assert(0 == send_rtsignal(0)) ;
   flushbuffer_logwritermt(log) ;
   return 0 ;
}

static int thread_printf(logwritermt_t * log)
{
   assert(0 == send_rtsignal(0)) ;
   printf_logwritermt(log, "1%c%s%d", '2', "3", 4) ;
   return 0 ;
}

static int test_globalvar(void)
{
   osthread_t      * thread    = 0 ;
   logwritermt_t   * log       = &g_main_logwriter ;
   int               pipefd[2] = { -1, -1 } ;
   int               oldstderr = -1 ;

   // TEST free does nothing
   TEST(log) ;
   TEST(0 == free_logwritermt(log)) ;

   // TEST interface
   TEST(g_main_logwriter_interface.printf      == &printf_logwritermt) ;
   TEST(g_main_logwriter_interface.flushbuffer == &flushbuffer_logwritermt) ;
   TEST(g_main_logwriter_interface.clearbuffer == &clearbuffer_logwritermt) ;
   TEST(g_main_logwriter_interface.getbuffer   == &getbuffer_logwritermt) ;

   // TEST buffer
   TEST(log-> EMITTHREADCODE(logwriter.) buffer.addr == s_logbuffer) ;
   TEST(log-> EMITTHREADCODE(logwriter.) buffer.size == sizeof(s_logbuffer)) ;
   log->EMITTHREADCODE(logwriter.)logsize = 0 ;
   printf_logwritermt(log, "123%s", "4") ;
   TEST(4 == log-> EMITTHREADCODE(logwriter.) logsize) ;
   TEST(0 == strcmp((char*)s_logbuffer, "1234")) ;
   clearbuffer_logwritermt(log) ;
   TEST(0 == log-> EMITTHREADCODE(logwriter.) logsize) ;

#define THREAD 1
#if ((KONFIG_SUBSYS)&THREAD)
   // TEST mutex
   TEST(0 == lock_mutex(&log->lock)) ;
   TEST(0 == unlock_mutex(&log->lock)) ;

   // TEST mutex getbuffer
   TEST(0 == lock_mutex(&log->lock)) ; // mutex is locked
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   s_thrdarg_buffer  = 0 ;
   s_thrdarg_size    = 0 ;
   log->logwriter.logsize = 16 ;
   TEST(0 == new_osthread(&thread, &thread_getbuffer, log)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;       // thread started
   sleepms_osthread(1) ;
   TEST(0 == s_thrdarg_buffer) ;
   TEST(0 == s_thrdarg_size) ;
   TEST(0 == unlock_mutex(&log->lock)) ;  // mutex is unlocked
   TEST(0 == delete_osthread(&thread)) ;
   TEST(s_thrdarg_buffer == (char*)log->logwriter.buffer.addr) ;
   TEST(s_thrdarg_size   == 16) ;

   // TEST mutex clearbuffer
   TEST(0 == lock_mutex(&log->lock)) ; // mutex is locked
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   log->logwriter.logsize = 1 ;
   TEST(0 == new_osthread(&thread, &thread_clearbuffer, log)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;       // thread s va_list aptarted
   sleepms_osthread(1) ;
   TEST(1 == log->logwriter.logsize) ;
   TEST(0 == unlock_mutex(&log->lock)) ;  // mutex is unlocked
   TEST(0 == delete_osthread(&thread)) ;
   TEST(0 == log->logwriter.logsize) ;

   // TEST mutex flushbuffer
   TEST(0 == pipe2(pipefd, O_CLOEXEC)) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 < oldstderr) ;
   TEST(STDERR_FILENO == dup2(pipefd[1], STDERR_FILENO)) ;
   TEST(0 == lock_mutex(&log->lock)) ; // mutex is locked
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   strcpy((char*)log->logwriter.buffer.addr, "_1_2_3_4") ;
   log->logwriter.logsize = 8 ;
   TEST(0 == new_osthread(&thread, &thread_flushbuffer, log)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;       // thread started
   sleepms_osthread(1) ;
   TEST(8 == log->logwriter.logsize) ;
   TEST(0 == unlock_mutex(&log->lock)) ;  // mutex is unlocked
   TEST(0 == delete_osthread(&thread)) ;
   TEST(0 == log->logwriter.logsize) ;
   {
      char buffer[9] = { 0 } ;
      TEST(8 == read(pipefd[0], buffer, 9)) ;
      TEST(0 == strcmp(buffer, "_1_2_3_4")) ;
   }
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == close(oldstderr)) ;
   TEST(0 == close(pipefd[0])) ;
   TEST(0 == close(pipefd[1])) ;
   oldstderr = pipefd[0] = pipefd[1] = -1 ;

   // TEST mutex printf
   TEST(0 == lock_mutex(&log->lock)) ; // mutex is locked
   TEST(EAGAIN == trywait_rtsignal(0)) ;
   log->logwriter.logsize = 0 ;
   TEST(0 == new_osthread(&thread, &thread_printf, log)) ;
   TEST(0 == wait_rtsignal(0, 1)) ;       // thread started
   sleepms_osthread(1) ;
   TEST(0 == log->logwriter.logsize) ;
   TEST(0 == unlock_mutex(&log->lock)) ;  // mutex is unlocked
   TEST(0 == delete_osthread(&thread)) ;
   TEST(4 == log->logwriter.logsize) ;
   TEST(0 == strcmp((char*)log->logwriter.buffer.addr, "1234")) ;
   clearbuffer_logwritermt(log) ;
   TEST(0 == log->logwriter.logsize) ;
   TEST(0 == strcmp((char*)log->logwriter.buffer.addr, "")) ;
#endif
#undef THREAD

   return 0 ;
ABBRUCH:
   if (-1 != oldstderr) {
      dup2(oldstderr, STDERR_FILENO) ;
   }
   close(oldstderr) ;
   close(pipefd[0]) ;
   close(pipefd[1]) ;
   delete_osthread(&thread) ;
   return EINVAL ;
}

int unittest_writer_mainlogwriter()
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
   return EINVAL ;}

#endif
