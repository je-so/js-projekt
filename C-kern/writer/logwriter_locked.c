/* title: LogWriter-Locked impl
   Implements thread safe logging of error messages.
   See <LogWriter-Locked>.

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

   file: C-kern/api/writer/logwriter_locked.h
    Header file of <LogWriter-Locked>.

   file: C-kern/writer/logwriter_locked.c
    Implementation file <LogWriter-Locked impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/writer/logwriter_locked.h"
#include "C-kern/api/err.h"
#include "C-kern/api/os/sync/mutex.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/os/filesystem/mmfile.h"
#include "C-kern/api/os/sync/signal.h"
#endif


// section: global variables

/* variable: s_logbuffer
 * Buffer <g_main_logwriterlocked> uses internally to store a new log entry. */
static uint8_t       s_logbuffer[1 + log_PRINTF_MAXSIZE] ;

/* variable: g_main_logwriterlocked
 * Is used to support a safe standard log configuration.
 * Used to write log output before any other init function was called. */
logwriter_locked_t   g_main_logwriterlocked = {
         .logwriter = { .buffer = { .addr = s_logbuffer, .size = sizeof(s_logbuffer) }, .logsize = 0 },
         .lock      = sys_mutex_INIT_DEFAULT
} ;


// section: logwriter_locked_t

// group: initumgebung

int initumgebung_logwriterlocked(logwriter_locked_t ** log)
{
   int err ;
   const size_t         objsize = sizeof(logwriter_locked_t) ;
   logwriter_locked_t * log2    = (logwriter_locked_t*) malloc(objsize) ;

   if (!log2) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(objsize) ;
      goto ABBRUCH ;
   }

   if (  *log
      && *log != &g_main_logwriterlocked) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   err = init_logwriterlocked( log2 ) ;
   if (err) goto ABBRUCH ;

   *log = log2 ;

   return 0 ;
ABBRUCH:
   free(log2) ;
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_logwriterlocked(logwriter_locked_t ** log)
{
   int err ;
   logwriter_locked_t * log2 = *log ;

   if (  log2
      && log2 != &g_main_logwriterlocked ) {

      *log = &g_main_logwriterlocked ;

      err = free_logwriterlocked( log2 ) ;

      free(log2) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: lifetime

int init_logwriterlocked(/*out*/logwriter_locked_t * log)
{
   int err ;
   logwriter_locked_t log2 = logwriter_locked_INIT_FREEABLE ;

   err = init_logwriter(&log2.logwriter) ;
   if (err) goto ABBRUCH ;

   err = init_mutex(&log2.lock) ;
   if (err) goto ABBRUCH ;

   *log = log2 ;

   return 0 ;
ABBRUCH:
   (void) free_logwriterlocked(&log2) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_logwriterlocked(logwriter_locked_t * log)
{
   int err ;
   int err2 ;

   if (  log
      && log != &g_main_logwriterlocked) {

      err = free_mutex(&log->lock) ;

      err2 = free_logwriter(&log->logwriter) ;
      if (err2) err = err2 ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: query

void getbuffer_logwriterlocked(logwriter_locked_t * log, /*out*/char ** buffer, /*out*/size_t * size)
{
   slock_mutex(&log->lock) ;
   getbuffer_logwriter(&log->logwriter, buffer, size) ;
   sunlock_mutex(&log->lock) ;
}

// group: change

void clearbuffer_logwriterlocked(logwriter_locked_t * log)
{
   slock_mutex(&log->lock) ;
   clearbuffer_logwriter(&log->logwriter) ;
   sunlock_mutex(&log->lock) ;
}

void flushbuffer_logwriterlocked(logwriter_locked_t * log)
{
   slock_mutex(&log->lock) ;
   flushbuffer_logwriter(&log->logwriter) ;
   sunlock_mutex(&log->lock) ;
}

void printf_logwriterlocked(logwriter_locked_t * log, const char * format, ... )
{
   slock_mutex(&log->lock) ;
   va_list args ;
   va_start(args, format) ;
   vprintf_logwriter(&log->logwriter, format, args) ;
   va_end(args) ;
   sunlock_mutex(&log->lock) ;
}


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_writer_logwriterlocked,ABBRUCH)

static int test_initumgebung(void)
{
   logwriter_locked_t * log = 0 ;

   // TEST init, double free (log = 0)
   log = 0 ;
   TEST(0 == initumgebung_logwriterlocked(&log)) ;
   TEST(log) ;
   TEST(log != &g_main_logwriterlocked) ;
   TEST(log->logwriter.buffer.addr) ;
   TEST(log->logwriter.buffer.size) ;
   TEST(0 == lock_mutex(&log->lock)) ;
   TEST(0 == unlock_mutex(&log->lock)) ;
   TEST(0 == freeumgebung_logwriterlocked(&log)) ;
   TEST(&g_main_logwriterlocked == log) ;
   TEST(0 == freeumgebung_logwriterlocked(&log)) ;
   TEST(&g_main_logwriterlocked == log) ;

   // TEST init, double free (log = &g_main_logwriterlocked)
   log = &g_main_logwriterlocked ;
   TEST(0 == initumgebung_logwriterlocked(&log)) ;
   TEST(log) ;
   TEST(log != &g_main_logwriterlocked) ;
   TEST(log->logwriter.buffer.addr) ;
   TEST(log->logwriter.buffer.size) ;
   TEST(0 == lock_mutex(&log->lock)) ;
   TEST(0 == unlock_mutex(&log->lock)) ;
   TEST(0 == freeumgebung_logwriterlocked(&log)) ;
   TEST(&g_main_logwriterlocked == log) ;
   TEST(0 == freeumgebung_logwriterlocked(&log)) ;
   TEST(&g_main_logwriterlocked == log) ;

   // TEST free (log = 0)
   log = 0 ;
   TEST(0 == freeumgebung_logwriterlocked(&log)) ;
   TEST(0 == log) ;

   // TEST EINVAL
   log = (logwriter_locked_t*) 1 ;
   TEST(EINVAL == initumgebung_logwriterlocked(&log)) ;
   TEST(log == (logwriter_locked_t*) 1) ;

   return 0 ;
ABBRUCH:
   TEST(0 == freeumgebung_logwriterlocked(&log)) ;
   return EINVAL ;
}

static char * s_thrdarg_buffer = 0 ;
static size_t s_thrdarg_size   = 0 ;

static int thread_getbuffer(logwriter_locked_t * log)
{
   assert(0 == send_rtsignal(0)) ;
   getbuffer_logwriterlocked(log, &s_thrdarg_buffer, &s_thrdarg_size) ;
   return 0 ;
}

static int thread_clearbuffer(logwriter_locked_t * log)
{
   assert(0 == send_rtsignal(0)) ;
   clearbuffer_logwriterlocked(log) ;
   return 0 ;
}

static int thread_flushbuffer(logwriter_locked_t * log)
{
   assert(0 == send_rtsignal(0)) ;
   flushbuffer_logwriterlocked(log) ;
   return 0 ;
}

static int thread_printf(logwriter_locked_t * log)
{
   assert(0 == send_rtsignal(0)) ;
   printf_logwriterlocked(log, "1%c%s%d", '2', "3", 4) ;
   return 0 ;
}

static int test_globalvar(void)
{
   osthread_t         * thread    = 0 ;
   logwriter_locked_t * log       = &g_main_logwriterlocked ;
   int                  pipefd[2] = { -1, -1 } ;
   int                  oldstderr = -1 ;

   // TEST free does nothing
   TEST(log) ;
   TEST(0 == free_logwriterlocked(log)) ;

   // TEST buffer
   TEST(log->logwriter.buffer.addr == s_logbuffer) ;
   TEST(log->logwriter.buffer.size == sizeof(s_logbuffer)) ;
   log->logwriter.logsize = 0 ;
   printf_logwriterlocked(log, "123%s", "4") ;
   TEST(4 == log->logwriter.logsize) ;
   TEST(0 == strcmp((char*)s_logbuffer, "1234")) ;
   clearbuffer_logwriterlocked(log) ;
   TEST(0 == log->logwriter.logsize) ;

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
   usleep(1000) ;
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
   usleep(1000) ;
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
   usleep(1000) ;
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
   usleep(1000) ;
   TEST(0 == log->logwriter.logsize) ;
   TEST(0 == unlock_mutex(&log->lock)) ;  // mutex is unlocked
   TEST(0 == delete_osthread(&thread)) ;
   TEST(4 == log->logwriter.logsize) ;
   TEST(0 == strcmp((char*)log->logwriter.buffer.addr, "1234")) ;
   clearbuffer_logwriterlocked(log) ;
   TEST(0 == log->logwriter.logsize) ;
   TEST(0 == strcmp((char*)log->logwriter.buffer.addr, "")) ;


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

int unittest_writer_logwriterlocked()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initumgebung())   goto ABBRUCH ;
   if (test_globalvar())      goto ABBRUCH ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;}

#endif
