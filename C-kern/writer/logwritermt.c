/* title: LogWriter-MT impl
   Implements thread safe logging of error messages.
   See <LogWriter-MT>.

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

   file: C-kern/api/writer/logwritermt.h
    Header file of <LogWriter-MT>.

   file: C-kern/writer/logwritermt.c
    Implementation file <LogWriter-MT impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/writer/logwritermt.h"
#include "C-kern/api/err.h"
#include "C-kern/api/aspect/interface/log_it.h"
#include "C-kern/api/os/sync/mutex.h"
#include "C-kern/api/writer/main_logwriter.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/os/filesystem/mmfile.h"
#include "C-kern/api/os/sync/signal.h"
#endif

// section: logwritermt_t

// group: types

/* typedef: logwritermt_it
 * Define interface <logwritermt_it>, see also <ilog_it>.
 * The interface is generated with the macro <log_it_DECLARE>. */
log_it_DECLARE(1, logwritermt_it, logwritermt_t)

// group: variables

/* variable: s_logwritermt_interface
 * Contains single instance of interface <logwritermt_it>. */
logwritermt_it  s_logwritermt_interface = {
                        &printf_logwritermt,
                        &flushbuffer_logwritermt,
                        &clearbuffer_logwritermt,
                        &getbuffer_logwritermt,
                     } ;

// group: init

int initumgebung_logwritermt(/*out*/log_oit * ilog)
{
   int err ;
   const size_t      objsize = sizeof(logwritermt_t) ;
   logwritermt_t   * log2    = (logwritermt_t*) malloc(objsize) ;

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

   err = init_logwritermt( log2 ) ;
   if (err) goto ABBRUCH ;

   ilog->object    = log2 ;
   ilog->functable = (log_it*) &s_logwritermt_interface ;

   return 0 ;
ABBRUCH:
   free(log2) ;
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_logwritermt(log_oit * ilog)
{
   int err ;
   logwritermt_t * log2 = (logwritermt_t*) ilog->object ;

   if (  log2
      && log2 != &g_main_logwriter ) {

      assert((log_it*)&s_logwritermt_interface == ilog->functable) ;

      ilog->object    = &g_main_logwriter ;
      ilog->functable = (log_it*) &g_main_logwriter_interface ;

      err = free_logwritermt( log2 ) ;

      free(log2) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: lifetime

int init_logwritermt(/*out*/logwritermt_t * log)
{
   int err ;
   logwritermt_t log2 = logwritermt_INIT_FREEABLE ;

   err = init_logwriter(&log2.logwriter) ;
   if (err) goto ABBRUCH ;

   err = init_mutex(&log2.lock) ;
   if (err) goto ABBRUCH ;

   *log = log2 ;

   return 0 ;
ABBRUCH:
   (void) free_logwritermt(&log2) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_logwritermt(logwritermt_t * log)
{
   int err ;
   int err2 ;

   if (  log
      && log != &g_main_logwriter) {

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

void getbuffer_logwritermt(logwritermt_t * log, /*out*/char ** buffer, /*out*/size_t * size)
{
   slock_mutex(&log->lock) ;
   getbuffer_logwriter(&log->logwriter, buffer, size) ;
   sunlock_mutex(&log->lock) ;
}

// group: change

void clearbuffer_logwritermt(logwritermt_t * log)
{
   slock_mutex(&log->lock) ;
   clearbuffer_logwriter(&log->logwriter) ;
   sunlock_mutex(&log->lock) ;
}

void flushbuffer_logwritermt(logwritermt_t * log)
{
   slock_mutex(&log->lock) ;
   flushbuffer_logwriter(&log->logwriter) ;
   sunlock_mutex(&log->lock) ;
}

void printf_logwritermt(logwritermt_t * log, const char * format, ... )
{
   slock_mutex(&log->lock) ;
   va_list args ;
   va_start(args, format) ;
   vprintf_logwriter(&log->logwriter, format, args) ;
   va_end(args) ;
   sunlock_mutex(&log->lock) ;
}


// group: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_writer_logwritermt,ABBRUCH)

static int test_initumgebung(void)
{
   log_oit           ilog = log_oit_INIT_FREEABLE ;
   logwritermt_t   * log  = 0 ;

   // TEST static init
   TEST(0 == ilog.object) ;
   TEST(0 == ilog.functable) ;

   // TEST exported interface
   TEST(s_logwritermt_interface.printf      == &printf_logwritermt)
   TEST(s_logwritermt_interface.flushbuffer == &flushbuffer_logwritermt) ;
   TEST(s_logwritermt_interface.clearbuffer == &clearbuffer_logwritermt) ;
   TEST(s_logwritermt_interface.getbuffer   == &getbuffer_logwritermt) ;

   // TEST init, double free (ilog.object = 0)
   TEST(0 == initumgebung_logwritermt(&ilog)) ;
   TEST(ilog.object) ;
   TEST(ilog.object    != &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &s_logwritermt_interface) ;
   log = (logwritermt_t*) ilog.object ;
   TEST(log->logwriter.buffer.addr) ;
   TEST(log->logwriter.buffer.size) ;
   TEST(0 == lock_mutex(&log->lock)) ;
   TEST(0 == unlock_mutex(&log->lock)) ;
   TEST(0 == freeumgebung_logwritermt(&ilog)) ;
   TEST(ilog.object    == &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &g_main_logwriter_interface) ;
   TEST(0 == freeumgebung_logwritermt(&ilog)) ;
   TEST(ilog.object    == &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &g_main_logwriter_interface) ;
   log = 0 ;

   // TEST init, double free (ilog.object = &g_main_logwriter)
   ilog.object = &g_main_logwriter ;
   TEST(0 == initumgebung_logwritermt(&ilog)) ;
   TEST(ilog.object) ;
   TEST(ilog.object    != &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &s_logwritermt_interface) ;
   log = (logwritermt_t*) ilog.object ;
   TEST(log->logwriter.buffer.addr) ;
   TEST(log->logwriter.buffer.size) ;
   TEST(0 == lock_mutex(&log->lock)) ;
   TEST(0 == unlock_mutex(&log->lock)) ;
   TEST(0 == freeumgebung_logwritermt(&ilog)) ;
   TEST(ilog.object    == &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &g_main_logwriter_interface) ;
   TEST(0 == freeumgebung_logwritermt(&ilog)) ;
   TEST(ilog.object    == &g_main_logwriter) ;
   TEST(ilog.functable == (log_it*) &g_main_logwriter_interface) ;
   log = 0 ;

   // TEST free (ilog.object = 0)
   ilog.object = 0 ;
   TEST(0 == freeumgebung_logwritermt(&ilog)) ;
   TEST(0 == ilog.object) ;

   // TEST EINVAL
   ilog.object = (logwritermt_t*) 1 ;
   TEST(EINVAL == initumgebung_logwritermt(&ilog)) ;
   TEST(ilog.object == (logwritermt_t*) 1) ;

   return 0 ;
ABBRUCH:
   freeumgebung_logwritermt(&ilog) ;
   return EINVAL ;
}

int unittest_writer_logwritermt()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initumgebung())   goto ABBRUCH ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;}

#endif
